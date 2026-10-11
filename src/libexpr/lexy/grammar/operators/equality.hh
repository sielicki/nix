#pragma once

/**
 * @file
 * Equality and inequality at a non-associative precedence level.
 */

#include "tags.hh"
#include "comparison.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct equality_operation : dsl::infix_op_single
{
    static constexpr auto op = dsl::op<binary_tag<nix::lex::binop_kind::eq>>(spaced(LEXY_LIT("==")))
                               / dsl::op<binary_tag<nix::lex::binop_kind::neq>>(spaced(LEXY_LIT("!=")));
    using operand = comparison_operation<Options>;
};

} // namespace nix::lex::grammar
