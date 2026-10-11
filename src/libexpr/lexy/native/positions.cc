/** Convert borrowed byte positions and associate captured documentation with native nodes. */
#include "context.hh"

namespace nix::lex {

void Builder::recordDocComment(const char * begin, const char * end, const char * nextToken)
{
    if (nextToken != text.data() + text.size())
        state.lexerState.positionToDocComment.emplace(pos(nextToken), DocComment{pos(begin), pos(end)});
}

ExprLambda * Builder::lambdaDoc(ExprLambda * lambda, PosIdx p)
{
    auto & comments = state.lexerState.positionToDocComment;
    if (auto it = comments.find(p); it != comments.end())
        lambda->setDocComment(it->second);
    return lambda;
}

[[noreturn]] void Builder::error(const char * p, const std::string & message) const
{
    throw ParseError({.msg = HintFmt(message), .pos = state.positions[pos(p)]});
}

} // namespace nix::lex
