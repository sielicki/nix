#pragma once

/**
 * @file
 * Borrow a complete indented-string literal span between interpolations.
 * This token recognizes escapes only to locate delimiters; it leaves decoding
 * and indentation flags to native/indented-strings.cc. Keeping the original
 * bytes avoids retaining a temporary scratch fragment for every escaped character.
 */

#include "common.hh"

namespace nix::lex::grammar {

struct indented_text : dsl::token_base<indented_text>
{
    template<typename Reader>
    struct tp
    {
        typename Reader::marker end;

        explicit tp(const Reader & reader)
            : end(reader.current())
        {
        }

        bool try_parse(Reader reader)
        {
            auto begin = reader.position();
            while (reader.peek() != Reader::encoding::eof()) {
                auto before = reader.current();
                auto c = reader.peek();
                reader.bump();
                if (c == '\'' && reader.peek() == '\'') {
                    reader.bump();
                    auto escaped = reader.peek();
                    if (escaped == '\'' || escaped == '$')
                        reader.bump();
                    else if (escaped == '\\') {
                        reader.bump();
                        if (reader.peek() == Reader::encoding::eof()) {
                            end = reader.current();
                            return false;
                        }
                        reader.bump();
                    } else {
                        reader.reset(before);
                        break;
                    }
                } else if (c == '$') {
                    if (reader.peek() == '{') {
                        reader.reset(before);
                        break;
                    }
                    // Flex consumes dollars in pairs with the following byte,
                    // except before a quote. In particular, $${ is literal.
                    if (reader.peek() != '\'' && reader.peek() != Reader::encoding::eof())
                        reader.bump();
                }
            }
            end = reader.current();
            return reader.position() != begin;
        }

        template<typename Context>
        void report_error(Context & context, Reader)
        {
            auto error = lexy::error<Reader, lexy::expected_char_class>(end.position(), "indented string character");
            context.on(lexy::parse_events::error{}, error);
        }
    };
};

struct ind_text
{
    static constexpr auto raw_rule = dsl::capture(indented_text{});
    static constexpr auto raw_value = lexy::callback<nix::lex::part>([](auto lex) {
        nix::lex::part result;
        result.text = nix::lex::lexeme_view(lex);
        return result;
    });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value = positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value};
};

} // namespace nix::lex::grammar
