/** Complete sets and control flow without revisiting their native children. */
#include "context.hh"

namespace nix::lex {

nix::lex::expression
Builder::attrset(const char * position, bool recursive, const std::pmr::vector<nix::lex::binding> & source)
{
    auto result = bindings(source, pos(position));
    result->recursive = recursive;
    return {result, position};
}

nix::lex::expression
Builder::let(const char * position, const std::pmr::vector<nix::lex::binding> & source, Expression body)
{
    auto attrs = bindings(source);
    if (!body) {
        attrs->recursive = true;
        attrs->pos = pos(position);
        return {make<ExprSelect>(alloc, pos(position), attrs, state.s.body), position};
    }
    if (!attrs->dynamicAttrs->empty())
        error(position, "dynamic attributes not allowed in let");
    return {make<ExprLet>(attrs, body.expr), position};
}

nix::lex::expression Builder::conditional(const char * position, Expression cond, Expression yes, Expression no)
{
    return {make<ExprIf>(pos(position), cond.expr, yes.expr, no.expr), position};
}

nix::lex::expression Builder::assertion(const char * position, Expression cond, Expression body)
{
    return {make<ExprAssert>(pos(position), cond.expr, body.expr), position};
}

nix::lex::expression Builder::with(const char * position, Expression env, Expression body)
{
    return {make<ExprWith>(pos(position), env.expr, body.expr), position};
}

} // namespace nix::lex
