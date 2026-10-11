#pragma once

/** Arithmetic negation binds more tightly than attribute tests, below application. */
#include "tags.hh"

namespace nix::lex::grammar {

struct negation_operation : dsl::prefix_op
{
    static constexpr auto op = dsl::op<unary_tag<nix::lex::unop_kind::negate>>(
        spaced(dsl::not_followed_by(LEXY_LIT("-"), dsl::lit_c<'>'>)) >> dsl::p<operator_depth>);
    using operand = dsl::atom;
};

} // namespace nix::lex::grammar
