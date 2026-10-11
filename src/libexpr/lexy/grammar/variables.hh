#pragma once

/**
 * @file
 * Identifiers that are not reserved words.
 * Test keyword exclusion before consuming the first identifier byte. The combined
 * lookahead prevents a failed identifier from committing a surrounding choice.
 */

#include "common.hh"
#include "whitespace.hh"
#include "keywords.hh"

namespace nix::lex::grammar {

struct variable_start : lexy::token_production
{
    // Check the first byte before keyword dispatch, without scanning the name.
    static constexpr auto rule = dsl::peek(dsl::peek(dsl::p<identifier_start>) >> dsl::peek_not(dsl::p<any_keyword>));
    static constexpr auto value = lexy::noop;
};

struct variable : lexy::token_production
{
    static constexpr auto rule = spaced(identifier::pattern.reserve(any_keyword::literals));
    static constexpr auto value = identifier::value;
};

} // namespace nix::lex::grammar
