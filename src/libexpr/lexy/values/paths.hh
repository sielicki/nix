#pragma once
/** A path prefix and unfinished fragments, consumed when the path closes. */
#include "parts.hh"
#include "kinds.hh"

namespace nix::lex {
struct path_parts
{
    path_kind kind;
    std::pmr::vector<part> parts;
};
} // namespace nix::lex
