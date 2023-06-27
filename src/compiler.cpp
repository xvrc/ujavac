#include "ujavac.h"

#include <atomic>
#include <fstream>
#include <functional>

#include <tbb/parallel_for_each.h>

static bool is_dec_digit(u32 c)
{
    return c >= '0' && c <= '9';
}

// JLS 3.8, "Java letter" definition
// TODO add Unicode support
static bool is_java_iden_start(u32 c)
{
    return c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' || c == '$' || c == '_';
}

// JLS 3.8, "Java letter-or-digit" definition
// TODO add Unicode support
static bool is_java_iden_part(u32 c)
{
    return is_java_iden_start(c) || is_dec_digit(c);
}

// JLS 3.4
static bool is_java_single_line_terminator(u32 c)
{
    return c == '\n' || c == '\r';
}

// JLS 3.6
static bool is_java_whitespace(u32 c)
{
    return c == ' ' || c == '\t' || c == '\f' || is_java_single_line_terminator(c);
}

static bool is_hex_digit(u32 c)
{
    return c >= 'A' && c <= 'F' || c >= 'a' && c <= 'f' || is_dec_digit(c);
}

static bool is_utf16_high_surrogate(u16 c)
{
    return c >= 0xD800 && c <= 0xDBFF;
}

static bool is_utf16_low_surrogate(u16 c)
{
    return c >= 0xDC00 && c <= 0xDFFF;
}

static bool is_utf16_surrogate(u16 c)
{
    return is_utf16_high_surrogate(c) || is_utf16_low_surrogate(c);
}

bool Context::compile_input(const char *path)
{
    std::basic_ifstream<char8_t> stream{path};

    // A single UTF-8 encoding character from the input
    // stream. All input starts from this representation.
    char8_t raw_utf8 = 0;

    // Reconstructed Unicode code point from UTF-8 input.
    u32 raw_unicode = 0;

    // State counter for Unicode code point reconstruction.
    u32 raw_unicode_remaining = 0;

    // State variables responsible for keeping track of
    // Unicode escape sequences (i.e. \uXXXX).
    u16 esc_utf16[2] = {};
    u32 esc_utf16_len = 0;
    u32 esc_utf16_remaining = 0;
    bool prev_esc = false;

    // Backslashes need special care during parsing because
    // they're used in escape sequences at various stages.
    bool prev_backslash = false;

    char token_buf[sizeof("synchronized")] = {};
    u32 token_buf_len = 0;

    u64 line_num = 1;
    u64 col_num = 1;
    u64 raw_backslash_count = 0;

    while (stream.read(&raw_utf8, 1))
    {
        // The input character after reconstruction
        // and Unicode escape sequence processing.
        // Lexing happens from this representation.
        u32 unicode = 0;

        if (!raw_unicode_remaining)
        {
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

        // FIXME does not handle \r\n as a single line
        if (is_java_single_line_terminator(raw_unicode))
        {
            line_num++;
            col_num = 1;
        }
        else
        {
            col_num++;
        }

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
            }

            esc_utf16[esc_utf16_len] |= val << (4 * esc_utf16_remaining);

            if (!esc_utf16_remaining)
            {
                prev_esc = true;
                esc_utf16_len++;

                if (esc_utf16_len == 2)
                {
                    // TODO convert esc_utf16 to unicode
                }
                else if (esc_utf16_len == 1)
                {
                    // TODO check if convertible esc_utf16 to unicode
                }
            }

            if (esc_utf16_len != 2)
            {
                continue;
            }

            esc_utf16_len = 0;
        }
        // JLS 3.3
        else if (prev_backslash && raw_unicode == 'u' && (raw_backslash_count % 2 == 0 || prev_esc))
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

        prev_esc = false;

        if (esc_utf16_len && is_utf16_surrogate(esc_utf16[0]))
        {
            // TODO fatal
        }

        // print("{} ", unicode);
    }

    // println("");

    return stream.good();
}

u8 Context::compile()
{
    std::atomic_bool status = true;
    tbb::parallel_for_each(m_inputs.begin(), m_inputs.end(), [&, this](const char *&item) {
        bool expected = true;
        status.compare_exchange_strong(expected, compile_input(item));
    });

    // Invert the status when returning to match traditional
    // OS process error code conventions, where 0 means success
    return !status.load();
}