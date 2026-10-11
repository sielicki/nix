#pragma once

/**
 * @file
 * Atomic expression dispatch, parenthesized expressions, and legacy set forms.
 * Paths are tried before numbers and identifiers. Lists recurse through their
 * forward declaration because list elements in turn contain simple expressions.
 */

#include "common.hh"
#include "numbers.hh"
#include "quoted-strings.hh"
#include "indented-strings.hh"
#include "paths/literals.hh"
#include "paths/search.hh"
#include "attribute-sets.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct paren_expr
{
    static constexpr auto raw_rule =
        spaced(dsl::lit_c<'('>) + recurse<expr_function, Options> + spaced(dsl::lit_c<')'>);
    static constexpr auto raw_value = lexy::forward<nix::lex::expression>;
    static constexpr auto rule = raw_rule;
    static constexpr auto value = raw_value;
};

template<LanguageOptions Options>
struct legacy_let
{
    static constexpr auto raw_rule =
        dsl::p<kw_let> + spaced(dsl::lit_c<'{'>) + dsl::p<binds<Options>> + spaced(dsl::lit_c<'}'>);
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder, const char * p, std::pmr::vector<nix::lex::binding> binds) {
            return builder.let(p, binds, {});
        });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

template<LanguageOptions Options>
struct rec_set
{
    static constexpr auto raw_rule =
        dsl::p<kw_rec> + spaced(dsl::lit_c<'{'>) + dsl::p<binds<Options>> + spaced(dsl::lit_c<'}'>);
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder, const char * p, std::pmr::vector<nix::lex::binding> binds) {
            return builder.attrset(p, true, binds);
        });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

struct var_expr
{
    static constexpr auto raw_rule = dsl::p<variable>;
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder, const char * p, std::string_view name) { return builder.variable(p, name); });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

template<LanguageOptions Options>
struct expr_simple
{
    static constexpr auto raw_rule = dsl::peek(dsl::p<nix_path_prefix>) >> dsl::p<nix_path<Options>>
                                     | dsl::peek(dsl::lit_c<'"'>) >> dsl::p<dq_string<Options>>
                                     | dsl::peek(LEXY_LIT("''")) >> dsl::p<ind_string<Options>>
                                     | dsl::peek(dsl::p<number_start>) >> dsl::p<number>
                                     | dsl::peek(dsl::lit_c<'('>) >> dsl::p<paren_expr<Options>>
                                     | dsl::peek(dsl::lit_c<'['>) >> recurse<nix_list, Options>
                                     | dsl::peek(dsl::p<kw_let> + dsl::lit_c<'{'>) >> dsl::p<legacy_let<Options>>
                                     | dsl::peek(dsl::p<kw_rec>) >> dsl::p<rec_set<Options>> | dsl::p<attrset<Options>>
                                     | dsl::p<spath> | dsl::peek(dsl::p<uri_prefix>) >> dsl::p<uri>
                                     | dsl::else_ >> dsl::p<var_expr>;
    static constexpr auto raw_value = lexy::forward<nix::lex::expression>;
    static constexpr auto rule = raw_rule;
    static constexpr auto value = raw_value;
};

} // namespace nix::lex::grammar
