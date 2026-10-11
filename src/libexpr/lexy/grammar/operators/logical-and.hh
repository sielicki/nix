#pragma once

/**
 * @file
 * Left-associated logical AND, folded directly into native binary nodes.
 */

#include "tags.hh"
#include "equality.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct logical_and_operation : dsl::infix_op_left
{
    static constexpr auto op = dsl::op<binary_tag<nix::lex::binop_kind::and_>>(spaced(LEXY_LIT("&&")));
    using operand = equality_operation<Options>;
};

} // namespace nix::lex::grammar
