#pragma once

/**
 * @file
 * Non-recursive attribute sets.
 * Empty binding lists are allowed; rec sets and legacy let sets are dispatched
 * from simple-expressions.hh so application arguments retain their native grammar.
 */

#include "common.hh"
#include "bindings.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct attrset
{
    static constexpr auto raw_rule = spaced(dsl::lit_c<'{'>) >> dsl::p<binds<Options>> >> spaced(dsl::lit_c<'}'>);
    static constexpr auto raw_value = lexy::callback<nix::lex::expression>(
        [](Builder & builder, const char * p, std::pmr::vector<nix::lex::binding> binds) {
            return builder.attrset(p, false, binds);
        });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

} // namespace nix::lex::grammar
