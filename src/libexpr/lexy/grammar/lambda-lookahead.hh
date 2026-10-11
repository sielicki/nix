#pragma once

/**
 * @file
 * Distinguish a formal set from an attribute set without recursive parsing.
 * Only enough initial tokens are inspected to commit to the formal-set grammar;
 * its full validation happens in formals.hh and native construction.
 */

#include "common.hh"
#include "identifiers.hh"
#include "whitespace.hh"

namespace nix::lex::grammar {

struct braced_lambda_lookahead : lexy::token_production
{
    static constexpr auto open = dsl::lit_c<'{'> + dsl::p<ws_star>;
    static constexpr auto empty_formals = dsl::lit_c<'}'> >> (dsl::p<ws_star> + (dsl::lit_c<':'> | dsl::lit_c<'@'>) );
    static constexpr auto id_formals =
        dsl::p<
            identifier> >> (dsl::p<ws_star> + (dsl::lit_c<','> | dsl::lit_c<'?'> | (dsl::lit_c<'}'> >> (dsl::p<ws_star> + (dsl::lit_c<':'> | dsl::lit_c<'@'>) ))));
    static constexpr auto rule = open + (empty_formals | LEXY_LIT("...") | id_formals);
};

} // namespace nix::lex::grammar
