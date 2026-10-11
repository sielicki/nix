#pragma once

/**
 * @file
 * Expression interpolation shared by strings, paths, and dynamic attributes.
 * The closing brace must not consume following string content as whitespace.
 * The recursive expression is completed when grammar.hh assembles the full grammar.
 */

#include "common.hh"
#include "whitespace.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct interpolation : lexy::token_production
{
    static constexpr auto raw_rule = spaced(LEXY_LIT("${")) + recurse<expr_function, Options> + dsl::lit_c<'}'>;
    static constexpr auto raw_value = lexy::forward<nix::lex::expression>;
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value = positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value};
};

template<LanguageOptions Options>
struct interp_part
{
    static constexpr auto raw_rule = dsl::p<interpolation<Options>>;
    static constexpr auto raw_value = lexy::callback<nix::lex::part>([](nix::lex::expression n) {
        nix::lex::part p;
        p.expr = n.expr;
        return p;
    });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value = positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value};
};

} // namespace nix::lex::grammar
