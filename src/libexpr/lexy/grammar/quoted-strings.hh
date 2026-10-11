#pragma once

/**
 * @file
 * Double-quoted strings, escapes, and interpolated fragments.
 * Accept non-UTF-8 bytes and normalize raw CR/CRLF to LF as the Flex lexer does.
 * Only interpolation parses expression trivia inside the string.
 */

#include "quoted-text.hh"
#include "interpolation.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct dq_string : lexy::token_production
{
    struct content
    {
        static constexpr auto item =
            dsl::peek(LEXY_LIT("${")) >> dsl::p<interp_part<Options>> | dsl::else_ >> dsl::p<dq_text>;
        static constexpr auto rule = dsl::brackets(dsl::lit_c<'"'>, dsl::lit_c<'"'>).opt_list(item);
        static constexpr auto value = list_value<nix::lex::part>;
    };

    static constexpr auto raw_rule = dsl::p<content>;
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder, const char * p, std::pmr::vector<nix::lex::part> parts) {
            return builder.string(p, false, parts);
        });
    static constexpr auto rule = spaced(dsl::position(raw_rule));
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

} // namespace nix::lex::grammar
