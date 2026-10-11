#pragma once

/**
 * Trivia is parsed explicitly at complete token boundaries. Recognition is pure;
 * only a committed production callback registers documentation in parse state.
 * No automatic whitespace rule may consume these spans before the callback.
 */

#include "common.hh"

namespace nix::lex::grammar {

struct doc_comment
{
    const char * begin = nullptr;
    const char * end = nullptr;
};

struct ws_star : lexy::token_production
{
    static constexpr auto item = dsl::while_one(dsl::ascii::blank / dsl::ascii::one_of<"\r\n">)
                                 | (LEXY_LIT("#") >> dsl::until(dsl::ascii::one_of<"\r\n">).or_eof())
                                 | dsl::capture(dsl::token(LEXY_LIT("/*") >> dsl::until(LEXY_LIT("*/"))));
    static constexpr auto rule = dsl::opt(dsl::list(item)) + dsl::position;
    static constexpr auto fold = lexy::fold_inplace<doc_comment>(
        doc_comment{},
        lexy::callback<void>(
            [](doc_comment &) {},
            [](doc_comment & last, auto lexeme) {
                auto text = nix::lex::lexeme_view(lexeme);
                // Match lexer.l: exclude /**/, /***/, and /***...*/.
                if (text.size() >= 6 && text[2] == '*' && text[3] != '*' && text[3] != '/')
                    last = {text.data(), text.data() + text.size()};
            }));
    static constexpr auto value = fold >> lexy::callback_with_state<void>(
                                      [](Builder &, lexy::nullopt, auto) {},
                                      [](Builder & builder, doc_comment comment, auto next) {
                                          if (comment.begin)
                                              builder.recordDocComment(
                                                  comment.begin, comment.end, reinterpret_cast<const char *>(next));
                                      });
};

/** Consume trailing trivia after a complete token, preserving branch behavior. */
template<typename Rule>
constexpr auto spaced(Rule rule)
{
    if constexpr (lexy::is_branch_rule<Rule>)
        return rule >> dsl::p<ws_star>;
    else
        return rule + dsl::p<ws_star>;
}

} // namespace nix::lex::grammar
