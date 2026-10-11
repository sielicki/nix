#pragma once

/** Operator tags retain the original token position for native diagnostics. */
#include "../common.hh"
#include "../whitespace.hh"

namespace nix::lex::grammar {

/** Keep an operator counted until its operand has completed, including nested expressions. */
struct operator_depth
{
    static constexpr auto rule = dsl::position;
    static constexpr auto value = lexy::bind(
        lexy::callback<Builder::OperatorDepth>([](Builder & builder, auto position) {
            return Builder::OperatorDepth(builder, reinterpret_cast<const char *>(position));
        }),
        lexy::parse_state,
        lexy::values);
};

template<nix::lex::binop_kind Kind>
struct binary_tag : nix::lex::binary_operator
{
    template<typename Iterator>
    explicit binary_tag(Iterator position)
        : nix::lex::binary_operator{Kind, reinterpret_cast<const char *>(position)}
    {
    }
};

template<nix::lex::unop_kind Kind>
struct unary_tag : nix::lex::unary_operator
{
    template<typename Iterator>
    explicit unary_tag(Iterator position)
        : nix::lex::unary_operator{Kind, reinterpret_cast<const char *>(position)}
    {
    }
};

struct has_attr_tag
{};

} // namespace nix::lex::grammar
