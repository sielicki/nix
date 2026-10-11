/** Construct native calls, flattening successive applications without changing argument order. */
#include "context.hh"

namespace nix::lex {

Expr * Builder::call(PosIdx p, Expr * fn, Expr * arg)
{
    if (auto existing = dynamic_cast<ExprCall *>(fn)) {
        existing->args->push_back(arg);
        return existing;
    }
    return make<ExprCall>(p, fn, std::initializer_list<Expr *>{arg});
}

Expr * Builder::builtin(PosIdx p, Symbol name, Expr * a, Expr * b)
{
    return make<ExprCall>(p, make<ExprVar>(name), std::initializer_list<Expr *>{a, b});
}

} // namespace nix::lex
