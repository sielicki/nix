#pragma once

/**
 * @file
 * Shared position and optional-list callbacks.
 * Positions are byte pointers into the caller-owned input. Optional list callbacks
 * produce an arena-bound empty vector rather than using the default PMR resource.
 */

#include "../values.hh"
#include "../native/context.hh"
#include "forward.hh"
#include "../values/bytes.hh"
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <optional>
#include <type_traits>
#include <utility>

namespace nix::lex::grammar {

namespace dsl = lexy::dsl;
using nix::lex::Builder;

/**
 * Delay specialization across grammar cycles. Passing Production<Options>
 * directly to dsl::recurse lets argument-dependent operator lookup instantiate
 * that specialization while its parent is still incomplete. A template-template
 * argument keeps it opaque until lexy instantiates the actual recursive parser.
 * Delegating to dsl::recurse preserves its depth accounting and whitespace.
 */
template<template<LanguageOptions> typename Production, LanguageOptions Options>
struct recursive_rule : dsl::rule_base
{
    template<typename NextParser>
    using p = lexy::parser_for<decltype(dsl::recurse<Production<Options>>), NextParser>;
};

template<template<LanguageOptions> typename Production, LanguageOptions Options>
inline constexpr auto recurse = recursive_rule<Production, Options>{};

template<typename Callback>
struct positioned_value : Callback
{
    using return_type = typename Callback::return_type;

    template<typename Iterator, typename... Args>
    return_type operator()(Iterator position, Args &&... args) const
    {
        auto result = [&]() {
            // Native construction needs the start position before allocating a node.
            // Callbacks without a position parameter receive only their values.
            if constexpr (lexy::is_callback_for<Callback, const char *, Args...>)
                return Callback::operator()(reinterpret_cast<const char *>(position), std::forward<Args>(args)...);
            else
                return Callback::operator()(std::forward<Args>(args)...);
        }();
        if (!result.position)
            result.position = reinterpret_cast<const char *>(position);
        return result;
    }

    template<typename Iterator, typename... Args>
    return_type operator()(Builder & builder, Iterator position, Args &&... args) const
    {
        auto result = [&]() {
            if constexpr (lexy::is_callback_for<Callback, Builder &, const char *, Args...>)
                return Callback::operator()(
                    builder, reinterpret_cast<const char *>(position), std::forward<Args>(args)...);
            else
                return Callback::operator()(builder, std::forward<Args>(args)...);
        }();
        if (!result.position)
            result.position = reinterpret_cast<const char *>(position);
        return result;
    }
};

/** The same allocator is used for non-empty sinks and optional empty lists. */
template<typename T>
inline constexpr auto list_value =
    lexy::as_list<std::pmr::vector<T>>.allocator([](Builder & builder) { return builder.scratchAllocator<T>(); });

} // namespace nix::lex::grammar
