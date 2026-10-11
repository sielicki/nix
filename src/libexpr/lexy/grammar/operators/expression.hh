#pragma once

/**
 * Parse operators by precedence without descending through every precedence
 * level for each operand. Recursive syntax consumes the grammar recursion
 * budget; transitions between precedence levels do not.
 */
#include "implication.hh"
#include "../application.hh"
#include <limits>

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct op_impl : lexy::expression_production
{
    // Nix permits a low-precedence prefix operator wherever an operand is
    // expected. Lexy's normal prefix lookup filters by the current binding
    // power, so reenter at NOT precedence when that lookup falls through.
    // The guard must reject !=, or this recursion would not consume input.
    static constexpr auto atom = dsl::peek(logical_not_operation<Options>::op) >> recurse<expr_logical_not, Options>
                                 | dsl::else_ >> dsl::p<expr_app<Options>>;
    using operation = implication_operation<Options>;

    // Lexy's limit counts completed operators as well as pending ones, even
    // within an infix_op_list's mixed-precedence operands. Bound actual pending
    // prefix/right-associative operators with operator_depth instead, so flat
    // chains remain unbounded without allowing unbounded C++ recursion.
    static constexpr auto max_operator_nesting = std::numeric_limits<unsigned>::max();

    static constexpr auto value = lexy::bind(
        lexy::callback<nix::lex::expression>(
            [](Builder & builder, nix::lex::expression e) { return e; },
            [](Builder & builder, nix::lex::expression lhs, nix::lex::binary_operator op, nix::lex::expression rhs) {
                return builder.binary(op, lhs, rhs);
            },
            [](Builder & builder,
               nix::lex::expression lhs,
               nix::lex::binary_operator op,
               Builder::OperatorDepth,
               nix::lex::expression rhs) { return builder.binary(op, lhs, rhs); },
            [](Builder & builder, nix::lex::unary_operator op, Builder::OperatorDepth, nix::lex::expression rhs) {
                return builder.unary(op.position, op.kind, rhs);
            },
            [](Builder & builder,
               nix::lex::expression target,
               has_attr_tag,
               std::pmr::vector<nix::lex::attr_name> path) { return builder.hasAttr(target.position, target, path); }),
        lexy::parse_state,
        lexy::values);
};

template<LanguageOptions Options>
struct expr_logical_not : lexy::subexpression_production<op_impl<Options>, logical_not_operation<Options>>
{};

} // namespace nix::lex::grammar
