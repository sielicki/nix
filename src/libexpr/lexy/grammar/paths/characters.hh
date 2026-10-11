#pragma once

/**
 * @file
 * Recursion-free path characters and segments.
 * These rules can be used in lookahead; expression interpolation is defined separately.
 */

#include "../common.hh"

namespace nix::lex::grammar {

inline constexpr auto path_char_rule = dsl::ascii::alnum / dsl::ascii::one_of<"._+-">;

struct path_segment : lexy::token_production
{
    static constexpr auto rule = dsl::while_one(path_char_rule);
    static constexpr auto value = lexy::noop;
};

} // namespace nix::lex::grammar
