#pragma once

/**
 * @file
 * Borrow maximal literal path spans between interpolations.
 * A slash is consumed only when more path text or an interpolation follows it;
 * keeping separators in the capture avoids per-segment scratch parts and copies.
 * Positive first-token checks stop the list before its terminator without committing
 * to an item that cannot be parsed.
 */

#include "../common.hh"
#include "characters.hh"
#include "../interpolation.hh"

namespace nix::lex::grammar {

struct path_text : lexy::token_production
{
    // Flex permits repeated separators in interpolated paths. Builder checks
    // the native PATH/STR boundary before resolving the literal prefix.
    static constexpr auto character =
        path_char_rule | (dsl::lit_c<'/'> >> dsl::peek(path_char_rule | dsl::lit_c<'/'> | LEXY_LIT("${")));
    static constexpr auto raw_rule = dsl::capture(dsl::token(character >> dsl::loop(character | dsl::break_)));
    static constexpr auto raw_value = lexy::callback<nix::lex::part>([](auto lex) {
        nix::lex::part p;
        p.text = nix::lex::lexeme_view(lex);
        return p;
    });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value = positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value};
};

template<LanguageOptions Options>
struct path_parts
{
    struct content
    {
        // Positive first-token guard per alternative: the item branch must fail
        // *before committing* when no segment char or ${ remains, otherwise the
        // list would try another iteration at the terminator and hard-fail.
        static constexpr auto rule = dsl::list(
            dsl::peek(LEXY_LIT("${")) >> dsl::p<interp_part<Options>>
            | dsl::peek(path_char_rule | dsl::lit_c<'/'>) >> dsl::p<path_text>);
        static constexpr auto value = list_value<nix::lex::part>;
    };

    // The prefix slash belongs to nix_path; the body cannot start with another.
    static constexpr auto rule = dsl::peek(path_char_rule | LEXY_LIT("${")) >> dsl::p<content>;
    static constexpr auto value = lexy::forward<std::pmr::vector<nix::lex::part>>;
};

} // namespace nix::lex::grammar
