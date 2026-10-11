#pragma once

/**
 * @file
 * Byte-preserving lexeme and escape helpers.
 * Nix strings are not restricted to UTF-8. Unknown escapes preserve the byte
 * after the backslash. Callers handle raw newline normalization separately.
 */

#include <string_view>

namespace nix::lex {

template<typename Lexeme>
inline std::string_view lexeme_view(const Lexeme & lex)
{
    return std::string_view(reinterpret_cast<const char *>(lex.data()), lex.size());
}

// Recognized escapes borrow static spellings; all other bytes borrow the input.
inline std::string_view unescape_dq(std::string_view text)
{
    switch (text.front()) {
    case 'n':
        return "\n";
    case 't':
        return "\t";
    case 'r':
        return "\r";
    default:
        return text;
    }
}

} // namespace nix::lex
