#pragma once

/**
 * @file
 * Non-associative ordering comparisons.
 * Native construction expresses these using the lessThan builtin and logical negation.
 */

#include "tags.hh"
#include "update.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct comparison_operation : dsl::infix_op_single
{
    static constexpr auto op =
        dsl::op<binary_tag<nix::lex::binop_kind::lt>>(
            spaced(dsl::not_followed_by(LEXY_LIT("<"), dsl::ascii::one_of<"|=">)))
        / dsl::op<binary_tag<nix::lex::binop_kind::lte>>(spaced(LEXY_LIT("<=")))
        / dsl::op<binary_tag<nix::lex::binop_kind::gt>>(spaced(dsl::not_followed_by(LEXY_LIT(">"), dsl::lit_c<'='>)))
        / dsl::op<binary_tag<nix::lex::binop_kind::gte>>(spaced(LEXY_LIT(">=")));
    using operand = update_operation<Options>;
};

} // namespace nix::lex::grammar
