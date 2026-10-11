#pragma once

/**
 * @file
 * Simple and formal-set lambdas, including both at-name spellings.
 * Lookahead resolves the lambda/attribute-set/URI ambiguities before a branch
 * commits. Separate builders preserve x: body versus { x }: body.
 */

#include "common.hh"
#include "formals.hh"
#include "lambda-lookahead.hh"
#include "uris.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct lambda_braced
{
    // `{ formals }:` or `{ formals }@name:`; the lookahead only confirms a
    // formal set (`,`/`?`/`...`/`{}`), the optional `@name` is parsed after `}`.
    static constexpr auto raw_rule =
        dsl::peek(dsl::p<braced_lambda_lookahead>)
        >> (dsl::p<formal_set<Options>> + dsl::opt(spaced(dsl::lit_c<'@'>) >> dsl::p<variable>)
            + spaced(dsl::lit_c<':'>) + recurse<expr_function, Options>);
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder,
           const char * p,
           typename formal_set<Options>::formal_set_value fs,
           lexy::nullopt,
           nix::lex::expression body) { return builder.lambda(p, fs.formals, fs.ellipsis, {}, body); },
        [](Builder & builder,
           const char * p,
           typename formal_set<Options>::formal_set_value fs,
           std::string_view at,
           nix::lex::expression body) { return builder.lambda(p, fs.formals, fs.ellipsis, at, body); });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

template<LanguageOptions Options>
struct lambda_braced_at
{
    static constexpr auto raw_rule = dsl::peek(dsl::p<braced_lambda_lookahead> + spaced(dsl::lit_c<'@'>))
                                     >> (dsl::p<formal_set<Options>> + spaced(dsl::lit_c<'@'>) + dsl::p<variable>
                                         + spaced(dsl::lit_c<':'>) + recurse<expr_function, Options>);
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder,
           const char * p,
           typename formal_set<Options>::formal_set_value fs,
           std::string_view at,
           nix::lex::expression body) { return builder.lambda(p, fs.formals, fs.ellipsis, at, body); });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

template<LanguageOptions Options>
struct lambda_at_braced
{
    static constexpr auto raw_rule = dsl::peek(dsl::p<variable> + spaced(dsl::lit_c<'@'>) + spaced(dsl::lit_c<'{'>))
                                     >> (dsl::p<variable> + spaced(dsl::lit_c<'@'>) + dsl::p<formal_set<Options>>
                                         + spaced(dsl::lit_c<':'>) + recurse<expr_function, Options>);
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder,
           const char * p,
           std::string_view at,
           typename formal_set<Options>::formal_set_value fs,
           nix::lex::expression body) { return builder.lambda(p, fs.formals, fs.ellipsis, at, body); });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

template<LanguageOptions Options>
struct lambda_simple
{
    static constexpr auto raw_rule =
        dsl::peek(dsl::peek_not(dsl::p<uri_prefix>) + dsl::p<variable> + spaced(dsl::lit_c<':'>))
        >> (dsl::p<variable> + spaced(dsl::lit_c<':'>) + recurse<expr_function, Options>);
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder, const char * p, std::string_view name, nix::lex::expression body) {
            return builder.simpleLambda(p, name, body);
        });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

} // namespace nix::lex::grammar
