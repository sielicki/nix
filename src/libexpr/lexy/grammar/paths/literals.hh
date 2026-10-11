#pragma once

/**
 * @file
 * Assemble complete path syntax while retaining its prefix kind.
 * Resolution against the source accessor and base path belongs to native/paths.cc.
 */

#include "../common.hh"
#include "segments.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options, nix::lex::path_kind Kind>
struct path_body
{
    static constexpr auto raw_rule = dsl::p<path_parts<Options>>;
    static constexpr auto raw_value = lexy::callback<nix::lex::path_parts>(
        [](std::pmr::vector<nix::lex::part> parts) { return nix::lex::path_parts{Kind, std::move(parts)}; });
    static constexpr auto rule = raw_rule;
    static constexpr auto value = raw_value;
};

template<LanguageOptions Options>
struct nix_path : lexy::token_production
{
    static constexpr auto raw_rule = (dsl::lit_c<'/'> >> dsl::p<path_body<Options, nix::lex::path_kind::abs>>)
                                     | (LEXY_LIT("~/") >> dsl::p<path_body<Options, nix::lex::path_kind::home>>)
                                     | (dsl::else_ >> dsl::p<path_body<Options, nix::lex::path_kind::rel>>);
    static constexpr auto raw_value =
        lexy::callback<nix::lex::expression>([](Builder & builder, const char * p, nix::lex::path_parts parts) {
            return builder.path(p, std::move(parts));
        });
    static constexpr auto rule = spaced(dsl::position(raw_rule));
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

} // namespace nix::lex::grammar
