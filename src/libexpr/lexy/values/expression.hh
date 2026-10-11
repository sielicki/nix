#pragma once
/**
 * A completed native expression and its first source byte. Passed by value:
 * there is no temporary expression allocation or recursive intermediate tree.
 * Even native nodes without a PosIdx retain their source position here while
 * parents are assembled (concatenation and binding diagnostics need it).
 */
#include <cstddef>

namespace nix {
struct Expr;
}

namespace nix::lex {
struct expression
{
    nix::Expr * expr = nullptr;
    const char * position = nullptr;
    expression() = default;

    expression(std::nullptr_t) {}

    expression(nix::Expr * expr, const char * position)
        : expr(expr)
        , position(position)
    {
    }

    explicit operator bool() const
    {
        return expr != nullptr;
    }
};
} // namespace nix::lex
