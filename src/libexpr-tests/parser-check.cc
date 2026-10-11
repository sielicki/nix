#include "nix/expr/parser.hh"
#include "nix/expr/eval-gc.hh"
#include "nix/expr/eval-settings.hh"
#include "nix/store/globals.hh"
#include "nix/util/configuration.hh"
#include "nix/util/current-process.hh"
#include "nix/util/file-system.hh"

#include <iomanip>
#include <limits>
#include <bit>
#include <nlohmann/json.hpp>

namespace nix {

// Compare selected native AST fields, including forceString and exact floating
// values, and fold adjacent literal string fragments. Error text, operator/call
// diagnostic positions and legacy-or markers are excluded. Functional tests
// separately compare legacy-or warning output.
struct CanonicalTree
{
    using Json = nlohmann::json;
    const SymbolTable & symbols;
    const PosTable & positions;
    const DocCommentMap & comments;

    Json position(PosIdx p) const
    {
        if (!p)
            return nullptr;
        auto pos = positions[p];
        return {pos.line, pos.column};
    }

    Json document(DocComment comment) const
    {
        return comment ? Json{position(comment.begin), position(comment.end)} : Json{};
    }

    Json documentAt(PosIdx p) const
    {
        auto it = comments.find(p);
        return it == comments.end() ? Json{} : document(it->second);
    }

    std::string name(Symbol s) const
    {
        return s ? std::string(symbols[s]) : "";
    }

    Json attrPath(std::span<const AttrName> path) const
    {
        Json result = Json::array();
        for (auto & attr : path)
            result.push_back(attr.symbol ? Json{"static", name(attr.symbol)} : Json{"dynamic", tree(attr.expr)});
        return result;
    }

    Json tree(const Expr * e) const
    {
        if (!e)
            return nullptr;
        if (auto n = dynamic_cast<const ExprInt *>(e))
            return {"int", n->v.integer().value};
        if (auto n = dynamic_cast<const ExprFloat *>(e))
            return {"float", std::bit_cast<uint64_t>(n->v.fpoint())};
        if (auto n = dynamic_cast<const ExprString *>(e))
            return {"string", std::string(n->v.string_view())};
        if (auto n = dynamic_cast<const ExprPath *>(e))
            return {"path", std::string(n->v.pathStrView())};
        if (auto n = dynamic_cast<const ExprInheritFrom *>(e))
            return {"inherit-from", n->displ};
        if (auto n = dynamic_cast<const ExprVar *>(e))
            return {"var", name(n->name)};
        if (auto n = dynamic_cast<const ExprSelect *>(e))
            return {"select", tree(n->e), attrPath(n->getAttrPath()), tree(n->def)};
        if (auto n = dynamic_cast<const ExprOpHasAttr *>(e))
            return {"has-attr", tree(n->e), attrPath(n->attrPath)};
        if (auto n = dynamic_cast<const ExprAttrs *>(e)) {
            Json attrs = Json::object(), dynamic = Json::array(), from = Json::array();
            for (auto & [symbol, def] : *n->attrs)
                attrs[name(symbol)] = {static_cast<int>(def.kind), tree(def.e), position(def.pos), documentAt(def.pos)};
            for (auto & def : *n->dynamicAttrs)
                dynamic.push_back({tree(def.nameExpr), tree(def.valueExpr), position(def.pos), documentAt(def.pos)});
            if (n->inheritFromExprs)
                for (auto source : *n->inheritFromExprs)
                    from.push_back(tree(source));
            return {"attrs", n->recursive, attrs, dynamic, from};
        }
        if (auto n = dynamic_cast<const ExprList *>(e)) {
            Json elements = Json::array();
            for (auto child : n->elems)
                elements.push_back(tree(child));
            return {"list", elements};
        }
        if (auto n = dynamic_cast<const ExprLambda *>(e)) {
            Json formals = nullptr;
            if (auto f = n->getFormals()) {
                Json args = Json::object();
                for (auto & arg : f->formals)
                    args[name(arg.name)] = {tree(arg.def), position(arg.pos)};
                formals = {f->ellipsis, args};
            }
            Json doc = document(n->docComment);
            return {"lambda", name(n->arg), formals, tree(n->body), position(n->pos), doc};
        }
        if (auto n = dynamic_cast<const ExprCall *>(e)) {
            Json args = Json::array();
            for (auto child : *n->args)
                args.push_back(tree(child));
            return {"call", tree(n->fun), args};
        }
        if (auto n = dynamic_cast<const ExprLet *>(e))
            return {"let", tree(n->attrs), tree(n->body)};
        if (auto n = dynamic_cast<const ExprWith *>(e))
            return {"with", tree(n->attrs), tree(n->body)};
        if (auto n = dynamic_cast<const ExprIf *>(e))
            return {"if", tree(n->cond), tree(n->then), tree(n->else_)};
        if (auto n = dynamic_cast<const ExprAssert *>(e))
            return {"assert", tree(n->cond), tree(n->body)};
        if (auto n = dynamic_cast<const ExprOpNot *>(e))
            return {"not", tree(n->e)};
#define CHECK_BINARY(Type)                      \
    if (auto n = dynamic_cast<const Type *>(e)) \
        return {#Type, tree(n->e1), tree(n->e2)};
        CHECK_BINARY(ExprOpEq)
        CHECK_BINARY(ExprOpNEq)
        CHECK_BINARY(ExprOpAnd)
        CHECK_BINARY(ExprOpOr)
        CHECK_BINARY(ExprOpImpl)
        CHECK_BINARY(ExprOpConcatLists)
        CHECK_BINARY(ExprOpUpdate)
#undef CHECK_BINARY
        if (auto n = dynamic_cast<const ExprConcatStrings *>(e)) {
            Json parts = Json::array();
            for (auto & [pos, child] : n->es) {
                auto part = tree(child);
                if (!parts.empty() && part[0] == "string" && parts.back()[0] == "string")
                    parts.back()[1].get_ref<std::string &>() += part[1].get_ref<const std::string &>();
                else
                    parts.push_back(std::move(part));
            }
            if (parts.size() == 1 && parts[0][0] == "string")
                return parts[0];
            return {"concat", n->forceString, parts};
        }
        if (auto n = dynamic_cast<const ExprPos *>(e)) {
            auto pos = positions[n->pos];
            return {"position", pos.line, pos.column};
        }
        throw Error("unhandled expression type in corpus comparison");
    }
};

static nlohmann::json parse(const SourcePath & path, const std::string & source, const std::string & backend)
{
    bool readOnly = true;
    EvalSettings settings{readOnly};
    settings.parser = backend;
    Exprs expressions;
    SymbolTable symbols{StaticEvalSymbols::staticSymbolTable()};
    PosTable positions;
    DocCommentMap comments;
    auto buffer = source;
    buffer.append("\0\0", 2);
    Expr * expr;
    try {
        expr = parseExprFromBuf(
            buffer.data(),
            buffer.size(),
            path,
            path.parent(),
            expressions,
            symbols,
            settings,
            positions,
            comments,
            getFSSourceAccessor());
    } catch (UsageError &) {
        throw;
    } catch (Error & e) {
        return {{"accepted", false}, {"error", e.what()}};
    }
    return {{"accepted", true}, {"tree", CanonicalTree{symbols, positions, comments}.tree(expr)}};
}

} // namespace nix

int main(int argc, char ** argv)
{
    using namespace nix;
    initLibStore(false);
    initGC();
#ifndef _WIN32
    // Match EvalState's stack allowance. This checker deliberately uses the raw
    // parser without constructing an evaluator, so it must do this itself.
    ensureStackSizeAtLeast(60 * 1024 * 1024);
#endif
    experimentalFeatureSettings.set("experimental-features", "pipe-operators");
    if (argc < 2) {
        std::cerr << "usage: nix-parser-check FILE...\n";
        return 2;
    }
    bool failed = false;
    try {
        for (int i = 1; i < argc; ++i) {
            SourcePath path{getFSSourceAccessor(), CanonPath(absPath(argv[i]).string())};
            auto source = path.readFile();
            auto bison = parse(path, source, "bison");
            auto lexy = parse(path, source, "lexy");
            bool match = bison["accepted"] == lexy["accepted"]
                         && (!bison["accepted"].get<bool>() || bison["tree"] == lexy["tree"]);
            nlohmann::json result{{"file", argv[i]}, {"match", match}, {"accepted", bison["accepted"]}};
            if (!match || !bison["accepted"].get<bool>()) {
                result["bison"] = std::move(bison);
                result["lexy"] = std::move(lexy);
            }
            std::cout << result.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace) << '\n';
            failed |= !match;
        }
    } catch (Error & e) {
        std::cerr << e.what() << '\n';
        return 2;
    }
    return failed ? 1 : 0;
}
