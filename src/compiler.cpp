#include "ujavac.h"

#include <atomic>
#include <cstdio>
#include <cstring>

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

enum class LexerItem
{
    None,
    TraditionalComment,
    EndOfLineComment,
};

bool Compiler::compile_input(u32 i) const
{
    std::FILE *src_file = std::fopen(m_inputs[i], "rb");
    std::FILE *dst_file = std::fopen(m_outputs[i].c_str(), "wb");

    // State that tracks the special end-of-file condition outlined in JLS 3.5
    int src_file_ch = 0;
    bool prev_raw_sub = false;

    // Reconstructed Unicode code point from UTF-8 input.
    u32 raw_unicode = 0;

    // State counter for Unicode code point reconstruction.
    u32 raw_unicode_remaining = 0;

    // Source code line segmentation state.
    u64 line_num = 1;
    u64 col_num = 1;
    bool prev_raw_cr = false;

    // State variables responsible for keeping track of
    // Unicode escape sequences (i.e. \uXXXX).
    u16 esc_utf16[2] = {};
    u32 esc_utf16_len = 0;
    u32 esc_utf16_remaining = 0;
    bool prev_from_esc = false;

    // Backslashes need special care during parsing because
    // they're used in escape sequences at various stages.
    bool prev_backslash = false;
    u64 raw_backslash_count = 0;

    char ascii_tok_buf[sizeof("synchronized")];
    u32 ascii_tok_buf_len = 0;

    LexerItem lexer_item = LexerItem::None;
    bool prev_trad_comment_end_star = false;

    if (!src_file || !dst_file)
    {
        goto cleanup;
    }

#define ascii_tok_buf_eq(other) (!std::strncmp(ascii_tok_buf, other, ascii_tok_buf_len))

    while (src_file_ch != EOF)
    {
        src_file_ch = std::fgetc(src_file);

        // A single UTF-8 encoding character from the input
        // stream. All input starts from this representation.
        // TODO handle EOF condition correctly
        u8 raw_utf8 = src_file_ch == EOF ? 0 : src_file_ch;

        if (!raw_unicode_remaining)
        {
            if (raw_utf8 == 0x80)
            {
                // TODO fatal
                goto cleanup;
            }

            for (raw_unicode_remaining = 1; raw_utf8 & (0x80 >> raw_unicode_remaining); ++raw_unicode_remaining)
                ;
            raw_unicode = raw_utf8;
        }
        else
        {
            raw_unicode = (raw_unicode << 6) | (raw_utf8 & 0x30);
        }

        if (--raw_unicode_remaining)
        {
            continue;
        }

        if (raw_unicode == '\x1A')
        {
            prev_raw_sub = true;
        }

        // JLS 3.4
        if (raw_unicode == '\r' || raw_unicode == '\n' && !prev_raw_cr)
        {
            line_num++;
            col_num = 1;

            if (lexer_item == LexerItem::EndOfLineComment)
            {
                lexer_item = LexerItem::None;
            }

            prev_raw_cr = raw_unicode == '\r';
        }
        else
        {
            col_num++;
        }

        // The input character after reconstruction
        // and Unicode escape sequence processing.
        // Lexing happens from this representation.
        u32 unicode = 0;

        if (esc_utf16_remaining)
        {
            // The start of a Unicode escape sequence can contain more than one "u"
            if (esc_utf16_remaining == 4 && raw_unicode == 'u')
            {
                continue;
            }

            --esc_utf16_remaining;

            u16 val;
            switch (raw_unicode)
            {
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
                raw_unicode -= 'a' - 'A';
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
                val = raw_unicode - 'A';
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
                val = raw_unicode - '0';
                break;
            default:;
                // TODO fatal
                goto cleanup;
            }

            esc_utf16[esc_utf16_len] |= val << (4 * esc_utf16_remaining);

            if (!esc_utf16_remaining)
            {
                prev_from_esc = true;
                esc_utf16_len++;

                if (esc_utf16_len == 1 && is_utf16_low_surrogate(esc_utf16[0]))
                {
                    // TODO fatal
                    goto cleanup;
                }

                // TODO convert esc_utf16 to unicode
            }

            if (esc_utf16_len != 2)
            {
                continue;
            }

            esc_utf16_len = 0;
        }
        // JLS 3.3
        else if (prev_backslash && raw_unicode == 'u' && (raw_backslash_count % 2 == 0 || prev_from_esc))
        {
            esc_utf16_remaining = 4;
            prev_backslash = false;
            continue;
        }
        else if (raw_unicode == '\\')
        {
            raw_backslash_count++;
            prev_backslash = true;
            continue;
        }

        prev_from_esc = false;

        if (esc_utf16_len && is_utf16_high_surrogate(esc_utf16[0]))
        {
            // TODO fatal
            goto cleanup;
        }

        if (lexer_item == LexerItem::TraditionalComment)
        {
            if (prev_trad_comment_end_star && unicode == '/')
            {
                lexer_item = LexerItem::None;
                prev_trad_comment_end_star = false;
            }
            else if (unicode == '*')
            {
                prev_trad_comment_end_star = true;
            }

            continue;
        }

        if (lexer_item == LexerItem::EndOfLineComment)
        {
            continue;
        }

        if (is_ascii(unicode))
        {
            ascii_tok_buf[ascii_tok_buf_len++] = unicode;

            if (ascii_tok_buf_eq("//"))
            {
                lexer_item = LexerItem::EndOfLineComment;
                ascii_tok_buf_len = 0;
            }
            else if (ascii_tok_buf_eq("/*"))
            {
                lexer_item = LexerItem::TraditionalComment;
                ascii_tok_buf_len = 0;
            }
            // FIXME too preemptive, doesn't account for e.g. "constz", which is valid
            else if (ascii_tok_buf_eq("const") || ascii_tok_buf_eq("goto"))
            {
                // TODO fatal
                goto cleanup;
            }
        }
    }

#undef ascii_tok_buf_eq

cleanup:
    bool good = false;
    if (src_file)
    {
        good = !std::ferror(src_file);
        std::fclose(src_file);
    }

    if (dst_file)
    {
        std::fclose(dst_file);
    }

    return good;
}

u8 Compiler::compile() const
{
    std::atomic_bool status = true;
    tbb::parallel_for(u32(0), u32(m_inputs.size()), [&, this](u32 i) {
        bool expected = true;
        status.compare_exchange_strong(expected, compile_input(i));
    });

    // Invert the status when returning to match traditional
    // OS process error code conventions, where 0 means success
    return !status.load();
}