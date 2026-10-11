#pragma once

/**
 * @file
 * Left-associated logical OR, folded directly into native binary nodes.
 */

#include "tags.hh"
#include "logical-and.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct logical_or_operation : dsl::infix_op_left
{
    static constexpr auto op = dsl::op<binary_tag<nix::lex::binop_kind::or_>>(spaced(LEXY_LIT("||")));
    using operand = logical_and_operation<Options>;
};

} // namespace nix::lex::grammar
