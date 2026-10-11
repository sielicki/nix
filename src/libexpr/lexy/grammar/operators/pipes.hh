#pragma once

/**
 * @file
 * Pipe operators: |> associates left and <| associates right.
 * Opposite directions cannot be mixed without parentheses. expr_function only
 * enters this production when the grammar's pipes option is enabled.
 */

#include "expression.hh"

namespace nix::lex::grammar {

struct pipe_into_operation : dsl::infix_op_left
{
    static constexpr auto op = dsl::op<binary_tag<nix::lex::binop_kind::pipe_r>>(spaced(LEXY_LIT("|>")));
    using operand = dsl::atom;
};

struct pipe_from_operation : dsl::infix_op_right
{
    static constexpr auto op =
        dsl::op<binary_tag<nix::lex::binop_kind::pipe_l>>(spaced(LEXY_LIT("<|")) >> dsl::p<operator_depth>);
    using operand = dsl::atom;
};

template<LanguageOptions Options>
struct op_pipe : lexy::expression_production
{
    // Keep the shared higher-precedence grammar in the atom: only pipe
    // directions belong to distinct groups. Parentheses start a new expression
    // and allow switching groups. Completed operations build calls directly,
    // without collecting an intermediate list of pipe operands.
    static constexpr auto atom = dsl::p<op_impl<Options>>;
    using operation = dsl::groups<pipe_into_operation, pipe_from_operation>;
    static constexpr auto max_operator_nesting = op_impl<Options>::max_operator_nesting;

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
               nix::lex::expression rhs) { return builder.binary(op, lhs, rhs); }),
        lexy::parse_state,
        lexy::values);
};

} // namespace nix::lex::grammar
