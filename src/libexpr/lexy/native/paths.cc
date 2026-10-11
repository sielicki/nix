/** Resolve literal prefixes through the parser source accessor and retain interpolated suffixes. */
#include "context.hh"
#include "nix/expr/diagnose.hh"
#include "nix/expr/eval-settings.hh"
#include "nix/util/users.hh"

namespace nix::lex {

nix::lex::expression Builder::path(const char * position, nix::lex::path_parts n)
{
    std::string literal;
    switch (n.kind) {
    case nix::lex::path_kind::abs:
        literal = "/";
        break;
    case nix::lex::path_kind::home:
        literal = "~/";
        break;
    case nix::lex::path_kind::dotdot:
        literal = "../";
        break;
    case nix::lex::path_kind::dot:
        literal = "./";
        break;
    case nix::lex::path_kind::rel:
        break;
    }
    size_t i = 0;
    while (i < n.parts.size() && !n.parts[i].expr)
        literal += n.parts[i++].text;
    // Flex's initial PATH token stops after the first slash of a repeated pair.
    // Bison's parser.y permits one STR token between PATH and the first
    // interpolation, and arbitrary literal suffixes after that interpolation.
    // Preserve the PATH/STR split: only the PATH prefix is resolved here.
    std::string beforeInterpolation;
    if (auto repeated = literal.find("//"); repeated != literal.npos) {
        if (i == n.parts.size() || literal.find("//", repeated + 1) != literal.npos)
            error(position + repeated, "unexpected path fragment before interpolation");
        beforeInterpolation = literal.substr(repeated + 1);
        literal.resize(repeated + 1);
    }
    auto p = pos(position);
    auto accessor = state.basePath.accessor;
    std::string resolved;
    if (n.kind == nix::lex::path_kind::home) {
        if (state.settings.pureEval)
            throw Error("the path '%s' can not be resolved in pure mode", literal);
        diagnose(state.settings.lintAbsolutePathLiterals, [&](bool) -> std::optional<ParseError> {
            return ParseError(
                {.msg = HintFmt(
                     "home path literals are not portable. Consider replacing path literal '%s' by a string, relative path, or parameter",
                     literal),
                 .pos = state.positions[p]});
        });
        accessor = state.rootFS;
        resolved = getHome().string() + literal.substr(1);
    } else {
        if (n.kind == nix::lex::path_kind::abs) {
            diagnose(state.settings.lintAbsolutePathLiterals, [&](bool) -> std::optional<ParseError> {
                return ParseError(
                    {.msg = HintFmt(
                         "absolute path literals are not portable. Consider replacing path literal '%s' by a string, relative path, or parameter",
                         literal),
                     .pos = state.positions[p]});
            });
            accessor = state.rootFS;
            resolved = CanonPath(literal).abs();
        } else {
            diagnose(state.settings.lintShortPathLiterals, [&](bool) -> std::optional<ParseError> {
                if (literal.front() != '.')
                    return ParseError(
                        {.msg = HintFmt(
                             "relative path literal '%s' should be prefixed with '.' for clarity: './%s'",
                             literal,
                             literal),
                         .pos = state.positions[p]});
                return std::nullopt;
            });
            resolved = CanonPath(literal, state.basePath.path).abs();
        }
        if (literal.size() > 1 && literal.back() == '/')
            resolved += '/';
    }
    Expr * first = make<ExprPath>(alloc, accessor, resolved);
    if (i == n.parts.size())
        return {first, position};
    auto & values = concatParts;
    values.clear();
    values.reserve(1 + !beforeInterpolation.empty() + n.parts.size() - i);
    values.emplace_back(p, first);
    if (!beforeInterpolation.empty())
        values.emplace_back(pos(position + literal.size()), make<ExprString>(alloc, beforeInterpolation));
    for (; i < n.parts.size(); ++i) {
        auto & part = n.parts[i];
        values.emplace_back(pos(part.position), part.expr ? part.expr : make<ExprString>(alloc, part.text));
    }
    return {make<ExprConcatStrings>(alloc, p, false, values), position};
}

} // namespace nix::lex
