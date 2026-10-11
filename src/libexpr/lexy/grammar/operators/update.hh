#pragma once

/**
 * @file
 * Right-associated attribute-set update (//).
 */

#include "tags.hh"
#include "logical-not.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct update_operation : dsl::infix_op_right
{
    static constexpr auto op =
        dsl::op<binary_tag<nix::lex::binop_kind::update>>(spaced(LEXY_LIT("//")) >> dsl::p<operator_depth>);
    using operand = logical_not_operation<Options>;
};

} // namespace nix::lex::grammar
