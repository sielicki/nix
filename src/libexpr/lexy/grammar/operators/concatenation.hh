#pragma once

/**
 * @file
 * Right-associated list concatenation (++).
 */

#include "has-attribute.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct concat_operation : dsl::infix_op_right
{
    static constexpr auto op =
        dsl::op<binary_tag<nix::lex::binop_kind::concat>>(spaced(LEXY_LIT("++")) >> dsl::p<operator_depth>);
    using operand = has_attr_operation<Options>;
};

} // namespace nix::lex::grammar
