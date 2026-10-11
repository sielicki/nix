#pragma once

/**
 * @file
 * Left-associated multiplication and division.
 * The slash guard leaves // to attribute-set update; expression dispatch
 * distinguishes literal paths before this operator level is reached.
 */

#include "tags.hh"
#include "concatenation.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct multiply_operation : dsl::infix_op_left
{
    static constexpr auto op =
        dsl::op<binary_tag<nix::lex::binop_kind::mul>>(spaced(LEXY_LIT("*")))
        / dsl::op<binary_tag<nix::lex::binop_kind::div>>(spaced(dsl::not_followed_by(LEXY_LIT("/"), dsl::lit_c<'/'>)));
    using operand = concat_operation<Options>;
};

} // namespace nix::lex::grammar
