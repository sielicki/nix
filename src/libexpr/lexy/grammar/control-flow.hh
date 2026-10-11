#pragma once

/**
 * @file
 * assert, with, let-in, and if expressions.
 * The let-in branch excludes let { ... }, which is the legacy set form handled
 * in simple-expressions.hh. Bodies cross the expr_function recursion boundary.
 */

#include "common.hh"
#include "bindings.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct expr_assert
{
    static constexpr auto raw_rule =
        dsl::p<
            kw_assert> >> (recurse<expr_function, Options> + spaced(dsl::lit_c<';'>) + recurse<expr_function, Options>);
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder, const char * p, nix::lex::expression cond, nix::lex::expression body) {
            return builder.assertion(p, cond, body);
        });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

template<LanguageOptions Options>
struct expr_with
{
    static constexpr auto raw_rule =
        dsl::p<
            kw_with> >> (recurse<expr_function, Options> + spaced(dsl::lit_c<';'>) + recurse<expr_function, Options>);
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder, const char * p, nix::lex::expression env, nix::lex::expression body) {
            return builder.with(p, env, body);
        });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

template<LanguageOptions Options>
struct expr_let_in
{
    // `let {` is the legacy let form (handled in expr_simple); only commit
    // to let-in when `{` does not follow.
    static constexpr auto raw_rule =
        dsl::peek(dsl::p<kw_let> + dsl::peek_not(dsl::lit_c<'{'>))
        >> dsl::p<kw_let> >> (dsl::p<binds<Options>> + dsl::p<kw_in> + recurse<expr_function, Options>);
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder, const char * p, std::pmr::vector<nix::lex::binding> binds, nix::lex::expression body) {
            return builder.let(p, binds, body);
        });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

template<LanguageOptions Options>
struct expr_if
{
    static constexpr auto raw_rule =
        dsl::p<
            kw_if> >> (recurse<expr_function, Options> + dsl::p<kw_then> + recurse<expr_function, Options> + dsl::p<kw_else> + recurse<expr_function, Options>);
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder,
           const char * p,
           nix::lex::expression cond,
           nix::lex::expression yes,
           nix::lex::expression no) { return builder.conditional(p, cond, yes, no); });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

} // namespace nix::lex::grammar
