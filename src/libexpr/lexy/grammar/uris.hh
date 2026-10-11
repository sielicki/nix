#pragma once

/**
 * @file
 * Legacy URI literals and the token recognizers used to distinguish them
 * from simple lambdas. Deprecation diagnostics are emitted during native construction.
 */

#include "common.hh"
#include "whitespace.hh"

namespace nix::lex::grammar {

struct uri_scheme : lexy::token_production
{
    static constexpr auto rule = dsl::ascii::alpha >> dsl::while_(dsl::ascii::alnum / dsl::ascii::one_of<"+.-">);
};

struct uri_char
{
    static constexpr auto rule = dsl::ascii::alnum / dsl::ascii::one_of<"%/?:@&=+$,-_.!~*'">;
};

struct uri_prefix : lexy::token_production
{
    // A scheme, colon, and one URI byte suffice to distinguish this from a lambda.
    // Lookahead need not traverse the entire URI before actual token capture.
    static constexpr auto rule = dsl::token(dsl::p<uri_scheme> + dsl::lit_c<':'> + dsl::p<uri_char>);
};

struct uri : lexy::token_production
{
    static constexpr auto raw_rule = dsl::capture(dsl::token(dsl::p<uri_prefix> + dsl::while_(uri_char::rule)));
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder, const char * p, auto lex) { return builder.uri(p, nix::lex::lexeme_view(lex)); });
    static constexpr auto rule = spaced(dsl::position(raw_rule));
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

} // namespace nix::lex::grammar
