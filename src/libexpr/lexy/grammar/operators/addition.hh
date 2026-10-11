#pragma once

/**
 * @file
 * Left-associated addition and subtraction.
 * Builder preserves overloaded addition through native ExprConcatStrings.
 */

#include "tags.hh"
#include "multiplication.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct add_operation : dsl::infix_op_left
{
    static constexpr auto op =
        dsl::op<binary_tag<nix::lex::binop_kind::add>>(spaced(dsl::not_followed_by(LEXY_LIT("+"), dsl::lit_c<'+'>)))
        / dsl::op<binary_tag<nix::lex::binop_kind::sub>>(spaced(dsl::not_followed_by(LEXY_LIT("-"), dsl::lit_c<'>'>)));
    using operand = multiply_operation<Options>;
};

} // namespace nix::lex::grammar
