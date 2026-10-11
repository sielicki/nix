#pragma once

/**
 * @file
 * Indented-string delimiters, escapes, and interpolation.
 * Retain raw literal spans and parse interpolations recursively. Escape-token
 * boundaries and indentation flags are reconstructed one string at a time by
 * native construction, so scratch parts stay proportional to interpolations.
 */

#include "indented-text.hh"
#include "interpolation.hh"

namespace nix::lex::grammar {

struct ind_open : lexy::token_production
{
    static constexpr auto rule = LEXY_LIT("''") >> dsl::opt(
                                     dsl::peek(dsl::while_(dsl::lit_c<' '>) + dsl::lit_c<'\n'>)
                                     >> (dsl::while_(dsl::lit_c<' '>) + dsl::lit_c<'\n'>) );
    static constexpr auto value = lexy::noop;
};

template<LanguageOptions Options>
struct ind_string : lexy::token_production
{
    struct content
    {
        static constexpr auto item =
            dsl::peek(LEXY_LIT("${")) >> dsl::p<interp_part<Options>> | dsl::else_ >> dsl::p<ind_text>;
        static constexpr auto rule =
            dsl::brackets(
                dsl::p<ind_open>,
                dsl::peek(LEXY_LIT("''") + dsl::peek_not(dsl::ascii::one_of<"'$\\">)) >> LEXY_LIT("''"))
                .opt_list(item);
        static constexpr auto value = list_value<nix::lex::part>;
    };

    static constexpr auto raw_rule = dsl::p<content>;
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder, const char * p, std::pmr::vector<nix::lex::part> parts) {
            return builder.string(p, true, parts);
        });
    static constexpr auto rule = spaced(dsl::position(raw_rule));
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

} // namespace nix::lex::grammar
