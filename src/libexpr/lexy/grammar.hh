#pragma once

/**
 * @file
 * Complete lexy grammar, assembled from the production families in grammar/.
 * LanguageOptions parameterizes the recursive production graph. The driver
 * selects pipes from the experimental-feature setting and keeps legacy `f or`
 * enabled, matching libexpr/parser.y and lexer.l.
 *
 * Include this header before instantiating lexy::parse. Individual production
 * headers include their direct dsl::p dependencies and use forward.hh for
 * dsl::recurse boundaries; the includes below complete those recursive edges.
 *
 * lexy choices commit to a branch rather than backtracking after a failure.
 * Peeks therefore inspect recursion-free tokens or first-token sets. A peek
 * creates a token matcher with recursion disabled: never move dsl::recurse
 * into its transitive dependencies. Whitespace handling is especially sensitive
 * at interpolation boundaries and around recursive operator operands.
 *
 * Precedence, lowest to highest: pipes, implication, OR, AND, equality,
 * comparison, update, logical NOT, addition, multiplication, list concatenation,
 * has-attribute, negation, application, selection, simple expressions.
 * Each operator header documents its associativity.
 */

#include "grammar/roots.hh"
#include "grammar/application.hh"
#include "grammar/lists.hh"
