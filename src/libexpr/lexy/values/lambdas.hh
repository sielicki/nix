#pragma once

/**
 * @file
 * Unfinished formal parameters. Defaults are already native expressions; names
 * borrow the input until the lambda builder interns and validates them.
 */

#include <memory_resource>
#include <string_view>
#include <vector>
#include "expression.hh"

namespace nix::lex {

struct formal
{
    const char * position = nullptr;
    std::string_view name;
    nix::Expr * default_ = nullptr;
};

} // namespace nix::lex
