#pragma once

/**
 * @file
 * A literal fragment or an interpolated expression in a string or path.
 * Supply raw literal spans for paths and indented strings, and decoded spans
 * for quoted strings. Keep borrowed text alive until native construction has
 * copied it into expression storage.
 */

#include <memory_resource>
#include <string_view>
#include <vector>
#include "expression.hh"

namespace nix::lex {

struct part
{
    const char * position = nullptr;
    std::string_view text; // empty when expr != nullptr
    nix::Expr * expr = nullptr;
};

} // namespace nix::lex
