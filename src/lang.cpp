#include "ujavac.h"

#include <atomic>
#include <cstdio>
#include <format>

#include <tbb/parallel_for.h>

constexpr bool is_dec_digit(u32 c)
{
    return c >= '0' && c <= '9';
}

constexpr bool is_identifier_ignorable(u32 c)
{
    return c <= 0x08 || c >= 0x0E && c <= 0x01B || c >= 0x7F && c <= 0x9F;
}

// JLS 3.8, "Java letter" definition
// TODO add Unicode support
constexpr bool is_identifier_start(u32 c)
{
    return c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' || c == '$' || c == '_';
}

// JLS 3.8, "Java letter-or-digit" definition
// TODO add Unicode support
constexpr bool is_identifier_part(u32 c)
{
    return is_identifier_start(c) || is_dec_digit(c) || is_identifier_ignorable(c);
}

// JLS 3.6
constexpr bool is_whitespace(u32 c)
{
    return c == ' ' || c == '\t' || c == '\f' || c == '\r' || c == '\n';
}

constexpr bool is_hex_digit(u32 c)
{
    return c >= 'A' && c <= 'F' || c >= 'a' && c <= 'f' || is_dec_digit(c);
}

constexpr bool is_ascii(u32 c)
{
    return c < 0x80;
}

constexpr bool is_utf16_high_surrogate(u16 c)
{
    return c >= 0xD800 && c <= 0xDBFF;
}

constexpr bool is_utf16_low_surrogate(u16 c)
{
    return c >= 0xDC00 && c <= 0xDFFF;
}

Compiler::Compiler(const char *input, const char *output) : m_input(input), m_output(output)
{
}

bool Compiler::ascii_token_contains(std::string_view token) const
{
    return std::string_view(m_ascii_tok_buf, m_ascii_tok_buf_len) == token;
}

void Compiler::push_diagnostic(std::string_view msg)
{
    println(stderr, "{}:{}:{}: {}", m_input, m_line_num, m_col_num, msg);
}

void Compiler::ascii_token_append(char c)
{
    if (!m_ascii_tok_buf_len)
    {
        m_ascii_tok_buf_line_num = m_line_num;
        m_ascii_tok_buf_col_num = m_col_num;
    }

    m_ascii_tok_buf[m_ascii_tok_buf_len++] = c;
}

bool Compiler::compile()
{
    std::FILE *src_file = std::fopen(m_input, "rb");
    std::FILE *dst_file = std::fopen(m_output, "wb");
    int src_file_ch = 0;
    bool compilation_successful = false;

    if (!src_file || !dst_file)
    {
        goto finish;
    }

    m_raw_unicode = 0;
    m_raw_unicode_remaining = 0;
    m_line_num = 1;
    m_col_num = 1;
    prev_raw_cr = false;
    m_esc_utf16_len = 0;
    m_esc_utf16_remaining = 0;
    m_prev_from_esc = false;
    m_prev_backslash = false;
    m_raw_backslash_count = 0;
    m_ascii_tok_buf_len = 0;
    m_lexer_item = LexerItem::WhiteSpace;
    m_prev_trad_comment_end_star = false;
    m_ascii_tok_buf_line_num = 0;
    m_ascii_tok_buf_col_num = 0;

    while (src_file_ch != EOF)
    {
        src_file_ch = std::fgetc(src_file);

        if (src_file_ch == EOF && std::ferror(src_file))
        {
            push_diagnostic("error reading input file");
            goto finish;
        }

        // A single UTF-8 encoding character from the input
        // stream. All input starts from this representation.
        // TODO handle EOF condition correctly (JLS 3.5)
        u8 raw_utf8 = src_file_ch == EOF ? 0 : src_file_ch;

        if (!m_raw_unicode_remaining)
        {
            // FIXME error handling must be proper
            if (raw_utf8 == 0x80)
            {
                push_diagnostic(std::format("invalid UTF-8 byte: {:#x}", raw_utf8));
                goto finish;
            }

            for (m_raw_unicode_remaining = 1; raw_utf8 & (0x80 >> m_raw_unicode_remaining); ++m_raw_unicode_remaining)
                ;
            m_raw_unicode = raw_utf8;
        }
        else
        {
            m_raw_unicode = (m_raw_unicode << 6) | (raw_utf8 & 0x30);
        }

        if (--m_raw_unicode_remaining)
        {
            continue;
        }

        // JLS 3.4
        if (m_raw_unicode == '\r' || m_raw_unicode == '\n' && !prev_raw_cr)
        {
            m_line_num++;
            m_col_num = 1;

            prev_raw_cr = m_raw_unicode == '\r';
        }
        else
        {
            m_col_num++;
        }

        // The input character after reconstruction
        // and Unicode escape sequence processing.
        // Lexing happens from this representation.
        u32 unicode = 0;

        if (m_esc_utf16_remaining)
        {
            // The start of a Unicode escape sequence can contain more than one "u"
            if (m_esc_utf16_remaining == 4 && m_raw_unicode == 'u')
            {
                continue;
            }

            --m_esc_utf16_remaining;

            u16 val;
            switch (m_raw_unicode)
            {
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
                m_raw_unicode -= 'a' - 'A';
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
                val = m_raw_unicode - 'A';
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                val = m_raw_unicode - '0';
                break;
            default:
                push_diagnostic(std::format("unexpected character in Unicode escape: {}", m_raw_unicode));
                goto finish;
            }

            m_esc_utf16[m_esc_utf16_len] |= val << (4 * m_esc_utf16_remaining);

            if (!m_esc_utf16_remaining)
            {
                m_prev_from_esc = true;
                m_esc_utf16_len++;

                if (m_esc_utf16_len == 1)
                {
                    if (is_utf16_high_surrogate(m_esc_utf16[0]))
                    {
                        // We expect a low surrogate to follow; parse it now
                        continue;
                    }

                    if (is_utf16_low_surrogate(m_esc_utf16[0]))
                    {
                        push_diagnostic("expected BMP Unicode escape");
                        goto finish;
                    }

                    unicode = m_esc_utf16[0];
                }
                else if (m_esc_utf16_len == 2)
                {
                    if (!is_utf16_high_surrogate(m_esc_utf16[0]) || !is_utf16_low_surrogate(m_esc_utf16[1]))
                    {
                        push_diagnostic("invalid Unicode escape sequence");
                        goto finish;
                    }

                    unicode = 0x10000 + (u32(m_esc_utf16[0] & 0x3FF) << 10) | (m_esc_utf16[1] & 0x3FF);
                }

                m_esc_utf16_len = 0;
            }
        }
        // JLS 3.3
        else if (m_prev_backslash && m_raw_unicode == 'u' && (m_raw_backslash_count % 2 == 0 || m_prev_from_esc))
        {
            m_esc_utf16_remaining = 4;
            m_prev_backslash = false;
            continue;
        }
        else if (m_raw_unicode == '\\')
        {
            m_raw_backslash_count++;
            m_prev_backslash = true;
            continue;
        }

        if (m_esc_utf16_len)
        {
            // If we get here, then the UTF-16 decoder was
            // waiting on a low surrogate which never came
            push_diagnostic("unexpected end of Unicode escape sequence");
            goto finish;
        }

        m_prev_from_esc = false;

        if (m_lexer_item == LexerItem::EndOfLineComment && (unicode == '\r' || unicode == '\n'))
        {
            m_lexer_item = LexerItem::WhiteSpace;
            continue;
        }

        if (m_lexer_item == LexerItem::TraditionalComment)
        {
            if (m_prev_trad_comment_end_star && unicode == '/')
            {
                m_lexer_item = LexerItem::WhiteSpace;
                m_prev_trad_comment_end_star = false;
            }
            else if (unicode == '*')
            {
                m_prev_trad_comment_end_star = true;
            }

            continue;
        }

        if (m_lexer_item == LexerItem::EndOfLineComment)
        {
            continue;
        }

        if (ascii_token_contains("/") && (unicode == '/' || unicode == '*'))
        {
            m_lexer_item = unicode == '/' ? LexerItem::EndOfLineComment : LexerItem::TraditionalComment;
            m_ascii_tok_buf_len = 0;
            continue;
        }

        if (m_lexer_item == LexerItem::WhiteSpace && is_identifier_start(unicode))
        {
            m_lexer_item = LexerItem::IdentifierChars;

            if (is_ascii(unicode))
            {
                ascii_token_append(unicode);
            }
            else
            {
                // Only identifiers can contain non-ASCII characters
                m_lexer_item = LexerItem::Identifier;

                // TODO implement
            }
        }
        else if (m_lexer_item == LexerItem::IdentifierChars && is_identifier_part(unicode))
        {
            if (is_ascii(unicode))
            {
                ascii_token_append(unicode);
            }
            else
            {
                // Only identifiers can contain non-ASCII characters
                m_lexer_item = LexerItem::Identifier;

                // TODO implement
            }
        }
        else if (m_ascii_tok_buf_len == 0 && unicode == '/')
        {
            ascii_token_append(unicode);
        }
        else if(m_lexer_item == LexerItem::IdentifierChars && is_whitespace(unicode))
        {
            // Perform token disambugation
            if (ascii_token_contains("const"))
            {
                push_diagnostic("unexpected const");
                goto finish;
            }
            else if (ascii_token_contains("goto"))
            {
                push_diagnostic("unexpected goto");
                goto finish;
            }
        }
    }

    compilation_successful = true;

finish:
    if (src_file)
    {
        std::fclose(src_file);
    }

    if (dst_file)
    {
        std::fclose(dst_file);
    }

    return compilation_successful;
}

CompilerManager::CompilerManager(std::span<const char *> inputs) : m_inputs(inputs)
{
    m_outputs.reserve(inputs.size());
    for (const auto &input : inputs)
    {
        std::string s = input;
        if (s.ends_with(".java"))
        {
            s.resize(s.size() - 5);
        }

        s.append(".class");
        m_outputs.push_back(s);
    }
}

u8 CompilerManager::run() const
{
    std::atomic_bool status = true;
    tbb::parallel_for(u32(0), u32(m_inputs.size()), [&, this](u32 i) {
        bool expected = true;
        status.compare_exchange_strong(expected, compile_unit(i));
    });

    // Invert the status when returning to match traditional
    // OS process error code conventions, where 0 means success
    return !status.load();
}

bool CompilerManager::compile_unit(u32 i) const
{
    Compiler compiler{m_inputs[i], m_outputs[i].c_str()};
    return compiler.compile();
}
