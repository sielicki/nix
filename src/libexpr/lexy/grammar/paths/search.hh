#pragma once

/**
 * @file
 * Angle-bracket search-path literals such as <nixpkgs/lib>.
 * This recognizer has no expression recursion and is also used in argument lookahead.
 */

#include "../common.hh"
#include "../whitespace.hh"
#include "characters.hh"

namespace nix::lex::grammar {

struct spath : lexy::token_production
{
    struct inner : lexy::token_production
    {
        static constexpr auto rule =
            dsl::capture(dsl::token(dsl::p<path_segment> + dsl::while_(dsl::lit_c<'/'> >> dsl::p<path_segment>)));
        static constexpr auto value =
            lexy::callback<std::string_view>([](auto lex) { return nix::lex::lexeme_view(lex); });
    };

    static constexpr auto raw_rule = dsl::lit_c<'<'> >> dsl::p<inner> >> dsl::lit_c<'>'>;
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder, const char * p, std::string_view value) { return builder.searchPath(p, value); });
    static constexpr auto rule = spaced(dsl::position(raw_rule));
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

} // namespace nix::lex::grammar
