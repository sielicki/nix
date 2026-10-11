/** Construct native variables and numeric, URI and search-path literals. */
#include "context.hh"
#include "nix/expr/diagnose.hh"
#include "nix/expr/eval-settings.hh"
#include <charconv>

namespace nix::lex {

nix::lex::expression Builder::variable(const char * position, std::string_view name)
{
    auto p = pos(position);
    return {
        name == "__curPos" ? static_cast<Expr *>(make<ExprPos>(p)) : make<ExprVar>(p, state.symbols.create(name)),
        position};
}

nix::lex::expression Builder::integer(const char * position, std::string_view literal)
{
    int64_t value = 0;
    auto [end, ec] = std::from_chars(literal.data(), literal.data() + literal.size(), value);
    if (ec != std::errc{} || end != literal.data() + literal.size())
        error(position, fmt("invalid integer '%s'", literal));
    return {make<ExprInt>(value), position};
}

nix::lex::expression Builder::floating(const char * position, std::string_view literal)
{
    double value = 0;
    auto [end, ec] = std::from_chars(literal.data(), literal.data() + literal.size(), value);
    if (ec != std::errc{} || end != literal.data() + literal.size())
        error(position, fmt("invalid float '%s'", literal));
    return {make<ExprFloat>(value), position};
}

nix::lex::expression Builder::uri(const char * position, std::string_view value)
{
    diagnose(state.settings.lintUrlLiterals, [&](bool fatal) -> std::optional<ParseError> {
        return ParseError(
            {.msg = HintFmt(
                 "URL literals are %s. Consider using a string literal \"%s\" instead",
                 fatal ? "disallowed" : "discouraged",
                 value),
             .pos = state.positions[pos(position)]});
    });
    return {make<ExprString>(alloc, value), position};
}

nix::lex::expression Builder::searchPath(const char * position, std::string_view value)
{
    return {
        builtin(pos(position), state.s.findFile, make<ExprVar>(state.s.nixPath), make<ExprString>(alloc, value)),
        position};
}

} // namespace nix::lex
