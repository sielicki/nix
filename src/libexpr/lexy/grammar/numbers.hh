#pragma once

/**
 * @file
 * Integer and float tokens and checked numeric conversion.
 * Float rules require a decimal point and restrict leading zeros. The native
 * builder converts with std::from_chars and reports failures at the token position.
 */

#include "common.hh"
#include "whitespace.hh"

namespace nix::lex::grammar {

struct number_start : lexy::token_production
{
    // Dispatch only; the full spelling and conversion are checked by number.
    static constexpr auto rule = dsl::ascii::digit | (dsl::lit_c<'.'> >> dsl::ascii::digit);
};

struct digits : lexy::token_production
{
    static constexpr auto rule = dsl::digits<>;
    static constexpr auto value = lexy::noop;
};

struct integer_lit : lexy::token_production
{
    static constexpr auto raw_rule = dsl::capture(dsl::p<digits>);
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder, const char * p, auto lex) { return builder.integer(p, nix::lex::lexeme_view(lex)); });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

struct float_lit : lexy::token_production
{
    // (([1-9][0-9]*\.[0-9]*)|(0?\.[0-9]+))([eE][+-]?[0-9]+)?
    // A decimal point is required, and a leading zero requires digits after it.
    struct exponent : lexy::token_production
    {
        // Commit only a complete exponent. Otherwise, e/E and an optional
        // sign remain available to the following identifier or operator.
        static constexpr auto rule = dsl::token(
            (dsl::lit_c<'e'> | dsl::lit_c<'E'>) +dsl::opt(dsl::lit_c<'+'> | dsl::lit_c<'-'>) + dsl::p<digits>);
    };

    static constexpr auto raw_rule = dsl::capture(
        dsl::token(
            ((dsl::peek(dsl::ascii::digit - dsl::lit_c<'0'>)
              >> (dsl::p<digits> + dsl::lit_c<'.'> + dsl::while_(dsl::ascii::digit)))
             | (dsl::else_ >> (dsl::opt(dsl::lit_c<'0'>) + dsl::lit_c<'.'> + dsl::p<digits>) ))
            + dsl::opt(dsl::p<exponent>)));
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder, const char * p, auto lex) { return builder.floating(p, nix::lex::lexeme_view(lex)); });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

struct number
{
    // try float first (peek), else integer
    static constexpr auto raw_rule =
        (dsl::peek(dsl::p<float_lit>) >> dsl::p<float_lit> | dsl::else_ >> dsl::p<integer_lit>) +dsl::p<ws_star>;
    static constexpr auto raw_value = lexy::forward<nix::lex::expression>;
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value = positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value};
};

} // namespace nix::lex::grammar
