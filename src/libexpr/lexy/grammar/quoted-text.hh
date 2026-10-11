#pragma once

/**
 * @file
 * Capture a maximal quoted-string literal between interpolation boundaries.
 * Escapes consume their following byte; $$ consumes a pair before checking ${.
 * The callback borrows unchanged text or decodes the captured run once into the
 * parse arena. This avoids a grammar callback and scratch fragment for each escape.
 */

#include "common.hh"
#include <algorithm>
#include <cstring>

namespace nix::lex::grammar {

struct quoted_text : dsl::token_base<quoted_text>
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
            while (reader.peek() != Reader::encoding::eof() && reader.peek() != '"') {
                auto before = reader.current();
                auto c = reader.peek();
                reader.bump();
                if (c == '\\') {
                    if (reader.peek() == Reader::encoding::eof()) {
                        end = reader.current();
                        return false;
                    }
                    reader.bump();
                } else if (c == '$') {
                    if (reader.peek() == '{') {
                        reader.reset(before);
                        break;
                    }
                    if (reader.peek() == '$')
                        reader.bump();
                }
            }
            end = reader.current();
            return reader.position() != begin;
        }

        template<typename Context>
        void report_error(Context & context, Reader)
        {
            auto error = lexy::error<Reader, lexy::expected_char_class>(end.position(), "string character");
            context.on(lexy::parse_events::error{}, error);
        }
    };
};

struct dq_text
{
    static constexpr auto raw_rule = dsl::capture(quoted_text{});
    static constexpr auto raw_value = lexy::callback<nix::lex::part>([](Builder & builder, auto lex) {
        auto text = nix::lex::lexeme_view(lex);
        nix::lex::part result;
        result.text = text;
        auto special = std::min(text.find('\\'), text.find('\r'));
        if (special == text.npos)
            return result;

        // Decoding never expands the input. A single upper-bound allocation
        // keeps the text stable even when the surrounding part vector grows.
        auto buffer = builder.scratchAllocator<char>().allocate(text.size());
        std::memcpy(buffer, text.data(), special);
        auto out = buffer + special;
        auto read = text.data() + special;
        auto end = text.data() + text.size();
        while (read != end) {
            auto c = *read++;
            if (c == '\\') {
                // quoted_text has already checked that an escaped byte exists.
                *out++ = nix::lex::unescape_dq(std::string_view(read++, 1)).front();
            } else if (c == '\r') {
                *out++ = '\n';
                if (read != end && *read == '\n')
                    ++read;
            } else
                *out++ = c;
        }
        result.text = std::string_view(buffer, out - buffer);
        return result;
    });
    static constexpr auto rule = dsl::position(raw_rule);
    static constexpr auto value =
        lexy::bind(positioned_value<std::remove_cv_t<decltype(raw_value)>>{raw_value}, lexy::parse_state, lexy::values);
};

} // namespace nix::lex::grammar
