/** Intern and validate parameters, preserving innermost documentation precedence. */
#include "context.hh"

namespace nix::lex {

nix::lex::expression Builder::simpleLambda(const char * position, std::string_view name, Expression body)
{
    auto p = pos(position);
    return {lambdaDoc(make<ExprLambda>(p, state.symbols.create(name), body.expr), p), position};
}

nix::lex::expression Builder::lambda(
    const char * position,
    const std::pmr::vector<nix::lex::formal> & source,
    bool ellipsis,
    std::string_view name,
    Expression body)
{
    auto p = pos(position);
    auto arg = name.empty() ? Symbol{} : state.symbols.create(name);
    FormalsBuilder formals{.ellipsis = ellipsis};
    formals.formals.reserve(source.size());
    for (auto & f : source)
        formals.formals.push_back({pos(f.position), state.symbols.create(f.name), f.default_});
    state.validateFormals(formals, p, arg);
    return {lambdaDoc(make<ExprLambda>(state.positions, alloc, p, arg, formals, body.expr), p), position};
}

} // namespace nix::lex
