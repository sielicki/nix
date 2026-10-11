#pragma once

/**
 * @file
 * List expressions and their selection-level elements.
 * Ordinary application needs parentheses to form one element; the legacy
 * `f or` form is itself a selection. Looking for the closing bracket avoids
 * peeking a recursive element production.
 */

#include "common.hh"
#include "selection.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct nix_list_item
{
    static constexpr auto rule = dsl::p<expr_select<Options>>;
    static constexpr auto value = lexy::bind(
        lexy::callback<nix::lex::expression>(
            [](Builder & builder, nix::lex::expression element) { return builder.listElement(element); }),
        lexy::parse_state,
        lexy::values);
};

template<LanguageOptions Options>
struct nix_list
{
    static constexpr auto raw_rule =
        dsl::brackets(spaced(dsl::lit_c<'['>), spaced(dsl::lit_c<']'>)).opt_list(dsl::p<nix_list_item<Options>>);
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder, const char * p, lexy::nullopt) { return builder.list(p, {}); },
        [](Builder & builder, const char * p, std::pmr::vector<nix::lex::expression> elems) {
            return builder.list(p, elems);
        });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value = list_value<nix::lex::expression> >> lexy::bind(
                                      positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value},
                                      lexy::parse_state,
                                      lexy::values);
};

} // namespace nix::lex::grammar
