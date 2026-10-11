#pragma once

/**
 * @file
 * Recursive expression boundaries used by dsl::recurse.
 * Only recurse may cross an incomplete production; dsl::p dependencies must be
 * provided by the owning header. Do not include full recursive expressions in peeks.
 */

#include "options.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct expr_function;
template<LanguageOptions Options>
struct expr_select;
template<LanguageOptions Options>
struct expr_logical_not;
template<LanguageOptions Options>
struct nix_list;

} // namespace nix::lex::grammar
