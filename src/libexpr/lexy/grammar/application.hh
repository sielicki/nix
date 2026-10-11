#pragma once

/**
 * @file
 * Function application: one selection followed by zero or more arguments.
 * The argument lookahead is deliberately recursion-free. A fold builds calls
 * in source order as arguments finish, without an intermediate argument vector.
 */

#include "common.hh"
#include "selection.hh"
#include "expression-start.hh"

namespace nix::lex::grammar {

struct application_state
{
    nix::lex::expression result;
    const char * position = nullptr;
};

struct application_sink
{
    auto sink(Builder & builder) const
    {
        return lexy::fold_inplace<application_state>(
                   application_state{},
                   [&builder](application_state & state, auto p, nix::lex::expression operand) {
                       if (!state.result) {
                           operand.expr->resetCursedOr();
                           state.result = operand;
                           // Parentheses retain the child's position until an actual call is built.
                           state.position = reinterpret_cast<const char *>(p);
                       } else
                           state.result = builder.apply(state.position, state.result, operand);
                   })
            .sink();
    }
};

template<LanguageOptions Options>
struct expr_app
{
    // The first selection is required. Later selections use a zero-width
    // separator to recognize an argument's first token without recursive
    // lookahead. Each item then consumes a complete selection.
    static constexpr auto rule =
        dsl::list(dsl::position(dsl::p<expr_select<Options>>), dsl::sep(dsl::peek(dsl::p<expr_select_start>)));
    static constexpr auto value = application_sink{}
                                  >> lexy::callback<nix::lex::expression>([](application_state state) {
                                        return state.result;
                                    });
};

} // namespace nix::lex::grammar
