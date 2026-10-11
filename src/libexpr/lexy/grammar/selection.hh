#pragma once

/**
 * @file
 * Attribute selection, defaults, and optional legacy `f or` arguments.
 * A dot commits to selection only if it does not start a relative path or float.
 * The default operand recurses at selection precedence.
 */

#include "common.hh"
#include "simple-expressions.hh"
#include "attributes.hh"

namespace nix::lex::grammar {

struct selection_or : lexy::token_production
{
    // Flex prefers the longer path/URI token in `f or/path` and `f or:uri`.
    // Only inspect those prefixes after recognizing "or"; ordinary selection
    // suffixes should not pay for path or URI lookahead.
    static constexpr auto raw_rule =
        dsl::peek(dsl::peek(kw_or::pattern) >> dsl::peek_not(dsl::p<nix_path_prefix> | dsl::p<uri_prefix>))
        >> kw_or::pattern;
    static constexpr auto rule = spaced(raw_rule);
    static constexpr auto value = lexy::noop;
};

struct cursed_or_suffix : lexy::token_production
{
    // Capture the keyword end before expression whitespace is skipped, so
    // deprecation snippets exclude any following spaces and comments.
    static constexpr auto rule = selection_or::raw_rule + dsl::position + dsl::p<ws_star>;
    static constexpr auto value =
        lexy::callback<const char *>([](auto end) { return reinterpret_cast<const char *>(end); });
};

template<LanguageOptions Options>
struct expr_select
{
    static constexpr auto dot_attr =
        attr_dot >> (dsl::p<attrpath<Options>> + dsl::opt(dsl::p<selection_or> >> recurse<expr_select, Options>));
    static constexpr auto suffix = [] {
        if constexpr (Options.cursedOr)
            return dsl::opt(dot_attr | dsl::p<cursed_or_suffix>);
        else
            return dsl::opt(dot_attr);
    }();
    static constexpr auto raw_rule = dsl::p<expr_simple<Options>> + suffix;
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder, const char *, nix::lex::expression target, lexy::nullopt) { return target; },
        [](Builder & builder, const char * p, nix::lex::expression target, const char * end) {
            return builder.cursedOr(p, target, end);
        },
        [](Builder & builder,
           const char * p,
           nix::lex::expression target,
           std::pmr::vector<nix::lex::attr_name> path,
           lexy::nullopt) { return builder.select(p, target, path, {}); },
        [](Builder & builder,
           const char * p,
           nix::lex::expression target,
           std::pmr::vector<nix::lex::attr_name> path,
           nix::lex::expression fallback) { return builder.select(p, target, path, fallback); });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

} // namespace nix::lex::grammar
