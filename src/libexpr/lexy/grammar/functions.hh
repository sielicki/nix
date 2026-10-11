#pragma once

/**
 * @file
 * Dispatch the lowest-precedence expression forms.
 * URI and path prefixes take precedence over lambda lookahead. All other ordinary
 * expressions enter at pipe or implication precedence, according to the options.
 */

#include "common.hh"
#include "lambdas.hh"
#include "control-flow.hh"
#include "paths/lookahead.hh"

#include "operators/pipes.hh"

namespace nix::lex::grammar {

template<LanguageOptions Options>
struct expr_function
{
    static constexpr auto operators = [] {
        if constexpr (Options.pipes)
            return dsl::p<op_pipe<Options>>;
        else
            return dsl::p<op_impl<Options>>;
    }();
    static constexpr auto raw_rule = dsl::peek(dsl::p<uri_prefix> | dsl::p<nix_path_prefix>) >> operators
                                     | dsl::p<lambda_at_braced<Options>> | dsl::p<lambda_braced<Options>>
                                     | dsl::p<lambda_simple<Options>> | dsl::p<expr_assert<Options>>
                                     | dsl::p<expr_with<Options>> | dsl::p<expr_let_in<Options>>
                                     | dsl::p<expr_if<Options>> | dsl::else_ >> operators;
    static constexpr auto raw_value = lexy::forward<nix::lex::expression>;
    static constexpr auto rule = raw_rule;
    static constexpr auto value = raw_value;
};

} // namespace nix::lex::grammar
