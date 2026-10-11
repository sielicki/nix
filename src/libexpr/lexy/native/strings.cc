/**
 * Complete quoted strings from borrowed literal runs and native interpolations.
 * Indented strings delegate to the native stripIndentation algorithm after
 * reconstructing the escape-token boundaries expected by that algorithm.
 */
#include "context.hh"

namespace nix::lex {

nix::lex::expression
Builder::string(const char * position, bool indented, const std::pmr::vector<nix::lex::part> & parts)
{
    if (indented)
        return {indentedString(position, parts), position};
    // Return ExprString for a literal-only string. Interpolations need an
    // ExprConcatStrings node to retain string coercion during evaluation.
    if (parts.empty())
        return {make<ExprString>(alloc, std::string_view{}), position};
    if (parts.size() == 1 && !parts.front().expr)
        return {make<ExprString>(alloc, parts.front().text), position};

    auto & values = concatParts;
    values.clear();
    values.reserve(parts.size());
    for (auto & part : parts) {
        values.emplace_back(pos(part.position), part.expr ? part.expr : make<ExprString>(alloc, part.text));
    }
    return {make<ExprConcatStrings>(alloc, pos(position), true, values), position};
}

} // namespace nix::lex
