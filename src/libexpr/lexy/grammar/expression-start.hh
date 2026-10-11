#pragma once

/**
 * @file
 * Recursion-free first-token set for application arguments.
 * Do not replace this approximation with a peek of expr_select: recursive
 * expressions cannot be parsed inside the token matcher used by peek.
 */

#include "common.hh"
#include "uris.hh"
#include "paths/lookahead.hh"
#include "paths/search.hh"
#include "variables.hh"
#include "whitespace.hh"

namespace nix::lex::grammar {

struct expr_select_start : lexy::token_production
{
    static constexpr auto rule =
        dsl::peek(dsl::p<uri_prefix> | dsl::p<nix_path_prefix>) | dsl::lit_c<'"'> | LEXY_LIT("''") | dsl::ascii::digit
        | dsl::lit_c<'('> | dsl::lit_c<'['> | dsl::lit_c<'{'>
        | dsl::peek(dsl::p<spath>) // <path>, not `x < y`
        // /path: `/` not followed by `/` (that's `//`) and followed by path char (not `a / b`)
        // all checks inside one peek so no partial commit
        | dsl::peek(dsl::lit_c<'/'> >> dsl::peek_not(dsl::lit_c<'/'>) >> (path_char_rule | LEXY_LIT("${")))
        | dsl::lit_c<'~'>
        // `rec {}` and legacy `let {}` are expr_simples (valid app arguments);
        // plain `let` (let-in) is not, so it stays excluded
        | dsl::p<kw_rec> | dsl::peek(dsl::p<kw_let> + dsl::lit_c<'{'>)
        | dsl::p<variable_start> | (dsl::lit_c<'.'> >> (dsl::ascii::digit | dsl::lit_c<'/'> | dsl::lit_c<'.'>) );
};

} // namespace nix::lex::grammar
