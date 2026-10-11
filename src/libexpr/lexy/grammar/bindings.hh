#pragma once

/**
 * @file
 * Assignments and inherit clauses used by sets, let, and the REPL.
 * Recursive right-hand sides use forward declarations; duplicate and dynamic-name
 * validation is shared with Bison through native ParserState during construction.
 */

#include "common.hh"
#include "attributes.hh"
#include "whitespace.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct inherit_names_items
{
    static constexpr auto rule = dsl::list(
        dsl::peek(dsl::p<variable_start> | dsl::p<kw_or> | dsl::lit_c<'"'> | LEXY_LIT("${"))
        >> dsl::p<attr_name<Options>>);
    static constexpr auto value = list_value<nix::lex::attr_name>;
};

template<LanguageOptions Options>
struct inherit_names
{
    static constexpr auto rule = dsl::opt(dsl::p<inherit_names_items<Options>>);
    static constexpr auto value = list_value<nix::lex::attr_name>;
};

template<LanguageOptions Options>
struct inherit_binding
{
    static constexpr auto raw_rule = dsl::p<kw_inherit> >> dsl::opt(
                                         dsl::peek(spaced(dsl::lit_c<'('>))
                                         >> (spaced(dsl::lit_c<'('>) + recurse<expr_function, Options>
                                             + spaced(dsl::lit_c<')'>)))
                                     >> dsl::p<inherit_names<Options>> >> spaced(dsl::lit_c<';'>);
    static constexpr auto raw_value = lexy::callback<nix::lex::binding>(
        [](std::optional<nix::lex::expression> from, std::pmr::vector<nix::lex::attr_name> names) {
            return nix::lex::binding{.inherit = true, .from = from ? *from : nullptr, .names = std::move(names)};
        });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value = positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value};
};

template<LanguageOptions Options>
struct assign_binding
{
    // First tokens of the attrpath alternative, recursion-free: variable,
    // `or`, `"`, `${`. (attrpath itself can't be peeked: it contains
    // dsl::recurse via interpolation, and dsl::peek disables recurse.)
    // `inherit` is a reserved keyword, so it can never start an attrpath.
    static constexpr auto attr_start = dsl::p<variable_start> | dsl::p<kw_or> | dsl::lit_c<'"'> | LEXY_LIT("${");
    static constexpr auto raw_rule = dsl::peek(attr_start)
                                     >> (dsl::p<attrpath<Options>> + spaced(dsl::lit_c<'='>)
                                         + recurse<expr_function, Options> + spaced(dsl::lit_c<';'>));
    static constexpr auto raw_value =
        lexy::callback<nix::lex::binding>([](std::pmr::vector<nix::lex::attr_name> names, nix::lex::expression value) {
            return nix::lex::binding{.names = std::move(names), .value = value};
        });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value = positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value};
};

template<LanguageOptions Options>
struct binding
{
    static constexpr auto raw_rule = dsl::p<inherit_binding<Options>> | dsl::p<assign_binding<Options>>;
    static constexpr auto raw_value = lexy::forward<nix::lex::binding>;
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value = positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value};
};

template<LanguageOptions Options>
struct binds_items
{
    // dsl::p<binding<Options>> is a branch (binding's rule is a choice of peek-guarded
    // branches), so the loop tries it directly.
    static constexpr auto rule = dsl::list(dsl::p<binding<Options>>);
    static constexpr auto value = list_value<nix::lex::binding>;
};

template<LanguageOptions Options>
struct binds
{
    // dsl::list is non-empty; the opt allows `{}`.
    static constexpr auto rule = dsl::opt(dsl::p<binds_items<Options>>);
    static constexpr auto value = list_value<nix::lex::binding>;
};

} // namespace nix::lex::grammar
