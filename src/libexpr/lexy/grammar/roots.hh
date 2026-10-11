#pragma once

/**
 * @file
 * Complete-expression and REPL-binding entry productions.
 * Both consume the entire input, use the same comment-aware whitespace, and
 * share the recursion bound. Include grammar.hh before instantiating a parse.
 */

#include "common.hh"
#include "whitespace.hh"
#include "functions.hh"
#include "bindings.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct nix_expr
{
    // Bound actual grammar cycles; operator precedence itself does not consume
    // this budget. This remains an implementation limit, not a language choice.
    static constexpr auto max_recursion_depth = 10000;
    static constexpr auto raw_rule = dsl::p<ws_star> + dsl::p<expr_function<Options>> + dsl::eof;
    static constexpr auto raw_value = lexy::forward<nix::lex::expression>;
    static constexpr auto rule = raw_rule;
    static constexpr auto value = raw_value;
};

template<LanguageOptions Options>
struct repl_bindings
{
    static constexpr auto max_recursion_depth = nix_expr<Options>::max_recursion_depth;
    static constexpr auto rule = dsl::p<ws_star> + dsl::p<binds_items<Options>> + dsl::eof;
    static constexpr auto value = lexy::forward<std::pmr::vector<nix::lex::binding>>;
};

} // namespace nix::lex::grammar
