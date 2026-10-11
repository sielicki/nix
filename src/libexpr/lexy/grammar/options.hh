#pragma once

namespace nix::lex::grammar {

/** Syntax choices are fixed throughout a grammar, including nested expressions. */
struct LanguageOptions
{
    bool pipes;
    bool cursedOr;
};

} // namespace nix::lex::grammar
