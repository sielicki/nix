#pragma once

/**
 * Attribute-existence tests are postfix operations with an attribute-path value.
 * Nix accepts repeated tests: `x ? a ? b` means `(x ? a) ? b`.
 * Bison's `%nonassoc '?'` does not forbid this: after `attrpath` it reduces
 * `expr_op '?' attrpath` without a shift/reduce conflict on the next `?`.
 */
#include "../attributes.hh"
#include "negation.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct has_attr_operation : dsl::postfix_op
{
    static constexpr auto op = dsl::op<has_attr_tag>(spaced(LEXY_LIT("?")) >> dsl::p<attrpath<Options>>);
    using operand = negation_operation;
};

} // namespace nix::lex::grammar
