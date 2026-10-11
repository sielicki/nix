#pragma once

/**
 * @file
 * Formal parameters, optional defaults, ellipsis, and trailing commas.
 * The separator lookahead distinguishes another formal from the ellipsis or close
 * brace. Defaults recurse into expressions; duplicate checking stays in libexpr.
 */

#include "common.hh"
#include "variables.hh"
#include "whitespace.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct formal
{
    static constexpr auto raw_rule =
        dsl::p<variable> + dsl::opt(spaced(dsl::lit_c<'?'>) >> recurse<expr_function, Options>);
    static constexpr auto raw_value = lexy::callback<nix::lex::formal>(
        [](std::string_view name, lexy::nullopt) {
            nix::lex::formal f;
            f.name = name;
            return f;
        },
        [](std::string_view name, nix::lex::expression def) {
            nix::lex::formal f;
            f.name = name;
            f.default_ = def.expr;
            return f;
        });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value = positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value};
};

struct ellipsis_flag
{
    static constexpr auto rule = dsl::p<kw_ellipsis>;
    static constexpr auto value = lexy::callback<bool>([] { return true; });
};

template<LanguageOptions Options>
struct formal_list
{
    // One formal; a leading `,` starts another formal only when a variable
    // follows (not `...` or `}`). Peek is recursion-free: variable is a
    // token_production without dsl::recurse.
    static constexpr auto comma = dsl::peek(spaced(dsl::lit_c<','>) + dsl::p<variable_start>)
                                  >> spaced(dsl::lit_c<','>);
    static constexpr auto rule = dsl::list(dsl::p<formal<Options>>, dsl::sep(comma));
    static constexpr auto value = list_value<nix::lex::formal>;
};

template<LanguageOptions Options>
struct formal_set
{
    // Tail after the formals: an optional comma, then an optional `...`.
    // Values: (nullopt) when the tail is absent or a bare comma,
    // (bool) when `...` is present.
    static constexpr auto tail = spaced(dsl::lit_c<','>) >> dsl::opt(dsl::p<ellipsis_flag>);
    static constexpr auto rule =
        spaced(dsl::lit_c<'{'>)
        + dsl::opt(
            dsl::peek(dsl::p<variable_start>) >> (dsl::p<formal_list<Options>> + dsl::opt(tail))
            | dsl::p<ellipsis_flag>)
        + spaced(dsl::lit_c<'}'>);

    struct formal_set_value
    {
        std::pmr::vector<nix::lex::formal> formals;
        bool ellipsis;
    };

    static constexpr auto value = lexy::bind(
        lexy::callback<formal_set_value>(
            [](Builder & builder, lexy::nullopt) { // `{}`
                return formal_set_value{
                    std::pmr::vector<nix::lex::formal>(builder.scratchAllocator<nix::lex::formal>()), false};
            },
            [](Builder & builder, bool e) { // `{...}`
                return formal_set_value{
                    std::pmr::vector<nix::lex::formal>(builder.scratchAllocator<nix::lex::formal>()), e};
            },
            [](Builder & builder, std::pmr::vector<nix::lex::formal> fs, lexy::nullopt) { // `{a}`, `{a,}`
                return formal_set_value{std::move(fs), false};
            },
            [](Builder & builder, std::pmr::vector<nix::lex::formal> fs, bool e) { // `{a, ...}`, `{a, b, ...}`
                return formal_set_value{std::move(fs), e};
            }),
        lexy::parse_state,
        lexy::values);
};

} // namespace nix::lex::grammar
