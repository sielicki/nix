/**
 * Decode one indented string at a time, preserving Flex's token boundaries.
 * Native stripIndentation distinguishes ordinary bytes from escaped bytes and
 * trims the last line of its final token. Joining adjacent escape tokens before
 * that operation can change the result, for example an escaped newline followed
 * by an escaped space. All views below borrow the source or static spellings;
 * stripIndentation copies them into native expressions before this list dies.
 */
#include "context.hh"
#include "../values/bytes.hh"
#include <cassert>

namespace nix::lex {

namespace {

/** Match the longest ordinary token from lexer.l's IND_STRING rule. */
size_t ordinaryLength(std::string_view text)
{
    size_t i = 0;
    while (i < text.size()) {
        auto c = text[i];
        if (c == '$' || c == '\'') {
            if (i + 1 == text.size())
                break;
            auto next = text[i + 1];
            if (next == '\'' || (c == '$' ? next == '{' : next == '$'))
                break;
            i += 2;
        } else
            ++i;
    }
    return i;
}

} // namespace

Expr * Builder::indentedString(const char * position, const std::pmr::vector<nix::lex::part> & parts)
{
    auto & values = indentedParts;
    values.clear();
    values.reserve(parts.size());
    for (auto & part : parts) {
        if (part.expr) {
            values.emplace_back(pos(part.position), part.expr);
            continue;
        }
        auto remaining = part.text;
        while (!remaining.empty()) {
            auto length = ordinaryLength(remaining);
            bool hasIndentation = length != 0;
            std::string_view decoded;
            if (hasIndentation)
                decoded = remaining.substr(0, length);
            else if (remaining.starts_with("'''")) {
                length = 3;
                decoded = "''";
            } else if (remaining.starts_with("''$")) {
                length = 3;
                decoded = "$";
            } else if (remaining.starts_with("''\\")) {
                // Input fragments must include a byte after the escape prefix.
                assert(remaining.size() >= 4);
                length = 4;
                decoded = nix::lex::unescape_dq(remaining.substr(3, 1));
            } else {
                // A lone dollar or quote is a non-indenting fallback token.
                length = 1;
                decoded = remaining.substr(0, 1);
            }
            values.emplace_back(pos(remaining.data()), StringToken{decoded.data(), decoded.size(), hasIndentation});
            remaining.remove_prefix(length);
        }
    }
    return state.stripIndentation(pos(position), values);
}

} // namespace nix::lex
