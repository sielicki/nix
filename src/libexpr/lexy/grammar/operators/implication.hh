#pragma once

/**
 * @file
 * Right-associated implication, with logical OR as its operand level.
 */

#include "tags.hh"
#include "logical-or.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct implication_operation : dsl::infix_op_right
{
    static constexpr auto op =
        dsl::op<binary_tag<nix::lex::binop_kind::impl>>(spaced(LEXY_LIT("->")) >> dsl::p<operator_depth>);
    using operand = logical_or_operation<Options>;
};

} // namespace nix::lex::grammar
