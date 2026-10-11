#pragma once

/**
 * @file
 * Static and dynamic attribute names and dotted attribute paths.
 * The dot recognizer must not consume the start of a relative path or a float.
 */

#include "common.hh"
#include "variables.hh"
#include "quoted-strings.hh"

namespace nix::lex::grammar {

/**
 * Reuse the quoted-string recognizer, but keep literal attribute names as byte
 * views. Constructing an ExprString here would copy its bytes into the native
 * arena only for Builder::attr to discard that node and intern the name again.
 * Decoded runs belong to the scratch arena and outlive the parts vector.
 */
template<LanguageOptions Options>
struct quoted_attr_name : dq_string<Options>
{
    static constexpr auto raw_value = lexy::callback<nix::lex::attr_name>(
        [](Builder & builder, const char * p, const std::pmr::vector<nix::lex::part> & parts) {
            nix::lex::attr_name result;
            if (parts.size() == 1 && !parts.front().expr)
                result.name = parts.front().text;
            else if (!parts.empty())
                result.dynamic = builder.string(p, false, parts).expr;
            return result;
        });
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

struct attr_or
{
    static constexpr auto rule = dsl::p<kw_or>;
    static constexpr auto value = lexy::callback<std::string_view>([]() { return std::string_view("or"); });
};

template<LanguageOptions Options>
struct attr_name
{
    static constexpr auto raw_rule = dsl::p<variable> | dsl::p<attr_or>
                                     | (dsl::peek(dsl::lit_c<'"'>) >> dsl::p<quoted_attr_name<Options>>)
                                     | dsl::peek(LEXY_LIT("${")) >> spaced(dsl::p<interpolation<Options>>);
    static constexpr auto raw_value = lexy::callback<nix::lex::attr_name>(
        [](std::string_view sv) {
            nix::lex::attr_name an;
            an.name = sv;
            return an;
        },
        [](nix::lex::attr_name name) { return name; },
        [](nix::lex::expression n) {
            nix::lex::attr_name an;
            an.dynamic = n.expr;
            return an;
        });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value = positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value};
};

// A dot followed by a block comment still separates attributes (`a./*...*/b`).
// Exclude the starts of paths and floats without rejecting that trivia boundary.
inline constexpr auto attr_dot =
    dsl::peek(
        dsl::lit_c<'.'>
        + dsl::peek_not(dsl::lit_c<'.'> | dsl::ascii::digit | (dsl::lit_c<'/'> >> dsl::peek_not(dsl::lit_c<'*'>))))
    >> spaced(dsl::lit_c<'.'>);

template<LanguageOptions Options>
struct attrpath
{
    // Only peek the separator, since attr_name can recurse via interpolation.
    static constexpr auto rule = dsl::list(dsl::p<attr_name<Options>>, dsl::sep(attr_dot));
    // collects every name, including the first
    static constexpr auto value = list_value<nix::lex::attr_name>;
};

} // namespace nix::lex::grammar
