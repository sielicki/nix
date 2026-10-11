#pragma once

/**
 * @file
 * Tags for operators and path prefixes.
 * These describe parsed syntax; native libexpr node selection belongs to native construction.
 */

namespace nix::lex {

enum class binop_kind {
    pipe_r, // |>
    pipe_l, // <|
    impl,   // ->
    or_,    // ||
    and_,   // &&
    eq,     // ==
    neq,    // !=
    lt,     // <
    lte,    // <=
    gt,     // >
    gte,    // >=
    update, // //
    add,    // +
    sub,    // -
    mul,    // *
    div,    // /
    concat, // ++
};

/** Keep the operator token's position for native diagnostics. */
struct binary_operator
{
    binop_kind kind;
    const char * position;
};

enum class unop_kind {
    not_,   // !
    negate, // -
};

struct unary_operator
{
    unop_kind kind;
    const char * position;
};

enum class path_kind {
    abs,    // /a/b
    home,   // ~/a/b
    dotdot, // ../a/b
    dot,    // ./a/b
    rel,    // a/b
};

} // namespace nix::lex
