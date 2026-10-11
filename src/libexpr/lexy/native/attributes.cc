/** Lower attribute names and bindings through the same validation helpers as Bison. */
#include "context.hh"

namespace nix::lex {

AttrName Builder::attr(const nix::lex::attr_name & a)
{
    if (!a.dynamic)
        return AttrName(state.symbols.create(a.name));
    auto e = a.dynamic;
    if (auto str = dynamic_cast<ExprString *>(e))
        return AttrName(state.symbols.create(str->v.string_view()));
    return AttrName(e);
}

AttrSelectionPath Builder::attrPath(const std::pmr::vector<nix::lex::attr_name> & path)
{
    AttrSelectionPath result;
    result.reserve(path.size());
    for (auto & a : path)
        result.push_back(attr(a));
    return result;
}

ExprAttrs * Builder::bindings(const std::pmr::vector<nix::lex::binding> & bindings, PosIdx p)
{
    auto result = make<ExprAttrs>(p);
    for (auto & b : bindings) {
        if (!b.inherit) {
            auto value = b.value.expr;
            state.addAttr(result, attrPath(b.names), location(b.position), value, location(b.value.position));
            continue;
        }
        Expr * from = nullptr;
        if (b.from) {
            if (!result->inheritFromExprs)
                result->inheritFromExprs = std::make_unique<std::pmr::vector<Expr *>>();
            result->inheritFromExprs->push_back(b.from.expr);
            from = make<ExprInheritFrom>(pos(b.from.position), result->inheritFromExprs->size() - 1);
        }
        for (auto & a : b.names) {
            auto name = attr(a);
            if (!name.symbol)
                error(a.position, "dynamic attributes not allowed in inherit");
            auto p = pos(a.position);
            if (auto it = result->attrs->find(name.symbol); it != result->attrs->end())
                state.dupAttr(name.symbol, p, it->second.pos);
            result->attrs->emplace(
                name.symbol,
                ExprAttrs::AttrDef(
                    from ? static_cast<Expr *>(make<ExprSelect>(alloc, p, from, name.symbol))
                         : make<ExprVar>(p, name.symbol),
                    p,
                    from ? ExprAttrs::AttrDef::Kind::InheritedFrom : ExprAttrs::AttrDef::Kind::Inherited));
        }
    }
    return result;
}

} // namespace nix::lex
