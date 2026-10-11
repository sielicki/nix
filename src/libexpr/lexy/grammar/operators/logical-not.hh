#pragma once

/**
 * @file
 * Logical negation at its Nix precedence level, below addition.
 */

#include "tags.hh"
#include "addition.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct logical_not_operation : dsl::prefix_op
{
    static constexpr auto op = dsl::op<unary_tag<nix::lex::unop_kind::not_>>(
        spaced(dsl::not_followed_by(LEXY_LIT("!"), dsl::lit_c<'='>)) >> dsl::p<operator_depth>);
    using operand = add_operation<Options>;
};

} // namespace nix::lex::grammar
