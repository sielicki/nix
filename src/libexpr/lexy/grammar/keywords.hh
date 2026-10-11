#pragma once

/**
 * @file
 * Reserved words with identifier-boundary checks.
 * A keyword prefix in a longer identifier must not commit the parser to a keyword.
 */

#include "common.hh"
#include "whitespace.hh"
#include "identifiers.hh"

namespace nix::lex::grammar {

#define NIX_KW(name, str)                                                       \
    struct kw_##name : lexy::token_production                                   \
    {                                                                           \
        static constexpr auto pattern = LEXY_KEYWORD(str, identifier::pattern); \
        static constexpr auto rule = spaced(pattern);                           \
        static constexpr auto value = lexy::noop;                               \
    }

NIX_KW(if, "if");
NIX_KW(then, "then");
NIX_KW(else, "else");
NIX_KW(assert, "assert");
NIX_KW(with, "with");
NIX_KW(let, "let");
NIX_KW(in, "in");
NIX_KW(rec, "rec");
NIX_KW(inherit, "inherit");
NIX_KW(or, "or");
#undef NIX_KW

struct kw_ellipsis : lexy::token_production
{
    static constexpr auto rule = spaced(LEXY_LIT("..."));
    static constexpr auto value = lexy::noop;
};

struct any_keyword : lexy::token_production
{
    // Keyword rules share a literal trie, including their identifier boundaries.
    static constexpr auto literals = dsl::literal_set(
        kw_if::pattern,
        kw_then::pattern,
        kw_else::pattern,
        kw_assert::pattern,
        kw_with::pattern,
        kw_let::pattern,
        kw_in::pattern,
        kw_rec::pattern,
        kw_inherit::pattern,
        kw_or::pattern);
    static constexpr auto rule = literals;
};

} // namespace nix::lex::grammar
