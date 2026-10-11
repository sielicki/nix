#pragma once

/**
 * @file
 * Attribute paths, assignments, inheritance, and attribute sets.
 * Binding positions belong to the left-hand side; value positions belong to the
 * expression. Names borrow the input until native symbol interning. Native duplicate checking and documentation
 * attachment happen in native construction.
 */

#include <memory_resource>
#include <string_view>
#include <vector>
#include "expression.hh"

namespace nix::lex {

struct attr_name
{
    const char * position = nullptr;
    std::string_view name; // set when dynamic == nullptr
    nix::Expr * dynamic = nullptr;
};

struct binding
{
    const char * position = nullptr;
    bool inherit = false;
    expression from;                   // inherit (e)
    std::pmr::vector<attr_name> names; // inherit names, or attrpath
    expression value;                  // rhs for attrpath = v
};

} // namespace nix::lex
