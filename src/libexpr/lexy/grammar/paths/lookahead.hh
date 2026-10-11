#pragma once

/**
 * @file
 * Recursion-free recognition of a path prefix.
 * Use this in expression dispatch instead of peeking nix_path, whose interpolation
 * would recurse inside a token matcher and violate lexy recursion constraints.
 */

#include "../common.hh"
#include "characters.hh"

namespace nix::lex::grammar {

struct nix_path_prefix : lexy::token_production
{
    static constexpr auto rule =
        (dsl::lit_c<'/'> >> dsl::peek(path_char_rule | LEXY_LIT("${"))) | LEXY_LIT("~/")
        | dsl::peek(dsl::p<path_segment> + dsl::lit_c<'/'> + dsl::peek(path_char_rule | LEXY_LIT("${")));
};

} // namespace nix::lex::grammar
