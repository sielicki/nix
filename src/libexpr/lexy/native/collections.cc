/** Assemble lists, calls and selections from completed native operands. */
#include "context.hh"

namespace nix::lex {

nix::lex::expression Builder::list(const char * position, const std::pmr::vector<Expression> & source)
{
    std::vector<Expr *> elems;
    elems.reserve(source.size());
    for (auto e : source)
        elems.push_back(e.expr);
    return {make<ExprList>(alloc, elems), position};
}

nix::lex::expression Builder::listElement(Expression element)
{
    // Call this when each element completes to preserve Bison's warning order,
    // before parsing any later elements that might emit their own warnings.
    element.expr->warnIfCursedOr(state.symbols, state.positions);
    return element;
}

nix::lex::expression Builder::apply(const char * position, Expression fn, Expression arg)
{
    auto result = call(pos(position), fn.expr, arg.expr);
    arg.expr->warnIfCursedOr(state.symbols, state.positions);
    return {result, position};
}

nix::lex::expression Builder::cursedOr(const char * position, Expression fn, const char * end)
{
    auto p = pos(position);
    // Give the legacy `f or` form its own call node and diagnostic span,
    // even when fn is itself a call from a parenthesized expression.
    auto result = make<ExprCall>(p, fn.expr, std::pmr::vector<Expr *>{make<ExprVar>(p, state.s.or_)}, pos(end));
    return {result, position};
}

nix::lex::expression Builder::select(
    const char * position, Expression target, const std::pmr::vector<nix::lex::attr_name> & path, Expression fallback)
{
    auto attrs = attrPath(path);
    auto result = make<ExprSelect>(alloc, pos(position), target.expr, attrs, fallback.expr);
    if (fallback)
        fallback.expr->warnIfCursedOr(state.symbols, state.positions);
    return {result, position};
}

nix::lex::expression
Builder::hasAttr(const char * position, Expression target, const std::pmr::vector<nix::lex::attr_name> & path)
{
    auto attrs = attrPath(path);
    return {make<ExprOpHasAttr>(alloc, target.expr, attrs), position};
}

} // namespace nix::lex
