#pragma once

/**
 * @file
 * Identifier spelling and its first-character recognizer.
 * Keyword exclusion is separate so attribute and lambda lookahead can reuse these rules.
 */

#include "common.hh"

namespace nix::lex::grammar {

struct identifier : lexy::token_production
{
    static constexpr auto pattern =
        dsl::identifier(dsl::ascii::alpha / dsl::lit_c<'_'>, dsl::ascii::word / dsl::ascii::one_of<"'-">);
    static constexpr auto trail = pattern.trailing_pattern();
    static constexpr auto rule = pattern;
    static constexpr auto value = lexy::callback<std::string_view>([](auto lex) { return nix::lex::lexeme_view(lex); });
};

struct identifier_start : lexy::token_production
{
    static constexpr auto rule = identifier::pattern.leading_pattern();
};

} // namespace nix::lex::grammar
