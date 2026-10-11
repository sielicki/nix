/** Translate syntax operators into native expression nodes and builtin calls. */
#include "context.hh"

namespace nix::lex {

nix::lex::expression Builder::binary(nix::lex::binary_operator op, nix::lex::expression lhs, nix::lex::expression rhs)
{
    auto p = pos(op.position);
    auto left = lhs.expr;
    auto right = rhs.expr;
    using enum nix::lex::binop_kind;
    auto native = [&]() -> Expr * {
        switch (op.kind) {
        case pipe_r:
        case pipe_l:
            return op.kind == pipe_r ? call(p, right, left) : call(p, left, right);
        case impl:
            return make<ExprOpImpl>(p, left, right);
        case or_:
            return make<ExprOpOr>(p, left, right);
        case and_:
            return make<ExprOpAnd>(p, left, right);
        case eq:
            return make<ExprOpEq>(p, left, right);
        case neq:
            return make<ExprOpNEq>(p, left, right);
        case lt:
            return builtin(p, state.s.lessThan, left, right);
        case lte:
            return make<ExprOpNot>(builtin(p, state.s.lessThan, right, left));
        case gt:
            return builtin(p, state.s.lessThan, right, left);
        case gte:
            return make<ExprOpNot>(builtin(p, state.s.lessThan, left, right));
        case update:
            return make<ExprOpUpdate>(p, left, right);
        case add: {
            auto & parts = concatParts;
            parts.clear();
            parts.emplace_back(pos(lhs.position), left);
            parts.emplace_back(pos(rhs.position), right);
            return make<ExprConcatStrings>(alloc, p, false, parts);
        }
        case sub:
            return builtin(p, state.s.sub, left, right);
        case mul:
            return builtin(p, state.s.mul, left, right);
        case div:
            return builtin(p, state.s.div, left, right);
        case concat:
            return make<ExprOpConcatLists>(p, left, right);
        }
        unreachable();
    };
    return {native(), lhs.position};
}

nix::lex::expression Builder::unary(const char * position, nix::lex::unop_kind op, Expression operand)
{
    auto e = op == nix::lex::unop_kind::not_ ? static_cast<Expr *>(make<ExprOpNot>(operand.expr))
                                             : builtin(pos(position), state.s.sub, make<ExprInt>(0), operand.expr);
    return {e, position};
}

} // namespace nix::lex
