/**
 * Parse-driver boundary: pass temporary parse state, report syntax errors,
 * construct native expressions in callbacks, then release scratch storage.
 * This is the only translation unit that instantiates the lexy grammar.
 */
#include "nix/expr/parser.hh"
#include "grammar.hh"
#include "native/context.hh"
#include "values/storage.hh"
#include "nix/util/experimental-features.hh"

#include <lexy/action/parse.hpp>
#include <lexy/input/string_input.hpp>

namespace nix::lex {

template<typename Production, typename Finish>
auto parseLexy(std::string_view text, ParserState & state, std::pmr::memory_resource * scratch, Finish finish)
{
    nix::lex::parse_arena arena(scratch);
    Builder builder{state, text, &arena};
    if (auto offset = text.find('\0'); offset != text.npos)
        builder.error(text.data() + offset, "NUL bytes are not allowed in Nix expressions");
    auto input = lexy::string_input<lexy::byte_encoding>(text.data(), text.size());
    auto onError = lexy::callback<void>([&](const auto &, const auto & error) {
        auto p = reinterpret_cast<const char *>(error.position());
        std::string message = "syntax error: ";
        if constexpr (requires {
                          error.string();
                          error.length();
                      }) {
            message += "expected '";
            message.append(reinterpret_cast<const char *>(error.string()), error.length());
            message += "'";
            if constexpr (requires { error.index(); })
                p += error.index();
        } else if constexpr (requires { error.name(); })
            message += std::string("expected ") + error.name();
        else
            message += error.message();
        bool incomplete = p == text.data() + text.size();
        // More input can close a delimiter, but cannot repair a recursion
        // bound reached at EOF. The REPL must report that error immediately.
        if constexpr (requires { error.is(lexy::max_recursion_depth_exceeded{}); })
            incomplete &= !error.is(lexy::max_recursion_depth_exceeded{});
        throw ParseError({.msg = HintFmt(message), .pos = state.positions[builder.pos(p)]}).setIncomplete(incomplete);
    });
    auto result = lexy::parse<Production>(input, builder, onError);
    if (!result.is_success())
        builder.error(text.data(), "syntax error in lexy parser");
    return finish(builder, result.value());
}

template<template<grammar::LanguageOptions> typename Production, typename Finish>
auto parseWithLanguageOptions(
    std::string_view text, ParserState & state, std::pmr::memory_resource * scratch, Finish finish)
{
    // Keep legacy `or` enabled for compatibility. Only pipes vary at runtime,
    // so the native grammar needs two language instantiations, not four.
    if (experimentalFeatureSettings.isEnabled(Xp::PipeOperators))
        return parseLexy<Production<grammar::LanguageOptions{.pipes = true, .cursedOr = true}>>(
            text, state, scratch, finish);
    return parseLexy<Production<grammar::LanguageOptions{.pipes = false, .cursedOr = true}>>(
        text, state, scratch, finish);
}

} // namespace nix::lex

namespace nix {

std::expected<ParsedAst<Expr>, ParseError>
LexyParserBackend::parseExpr(std::span<char> input, ParserState & state) const
try {
    auto text = std::string_view(input.data(), input.size() - 2);
    auto expression = lex::parseWithLanguageOptions<lex::grammar::nix_expr>(
        text, state, allocator.resource(), [](auto &, auto expression) { return expression.expr; });
    return ParsedAst(*expression, state.exprs);
} catch (ParseError & error) {
    return std::unexpected(std::move(error));
}

std::expected<ParsedAst<ExprAttrs>, ParseError>
LexyParserBackend::parseReplBindings(std::span<char> input, ParserState & state) const
try {
    auto text = std::string_view(input.data(), input.size() - 2);
    auto bindings = lex::parseWithLanguageOptions<lex::grammar::repl_bindings>(
        text, state, allocator.resource(), [](auto & builder, auto & bindings) { return builder.bindings(bindings); });
    return ParsedAst(*bindings, state.exprs);
} catch (ParseError & error) {
    return std::unexpected(std::move(error));
}

} // namespace nix
