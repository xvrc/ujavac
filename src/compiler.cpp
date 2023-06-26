#include "ujavac.h"

#include <atomic>
#include <fstream>
#include <functional>

#include <tbb/parallel_for_each.h>

constexpr u32 UNICODE_UPPERCASE_A = 65;
constexpr u32 UNICODE_UPPERCASE_B = 66;
constexpr u32 UNICODE_UPPERCASE_C = 67;
constexpr u32 UNICODE_UPPERCASE_D = 68;
constexpr u32 UNICODE_UPPERCASE_E = 69;
constexpr u32 UNICODE_UPPERCASE_F = 70;
constexpr u32 UNICODE_UPPERCASE_Z = 90;

constexpr u32 UNICODE_LOWERCASE_A = 97;
constexpr u32 UNICODE_LOWERCASE_B = 98;
constexpr u32 UNICODE_LOWERCASE_C = 99;
constexpr u32 UNICODE_LOWERCASE_D = 100;
constexpr u32 UNICODE_LOWERCASE_E = 101;
constexpr u32 UNICODE_LOWERCASE_F = 102;
constexpr u32 UNICODE_LOWERCASE_U = 117;
constexpr u32 UNICODE_LOWERCASE_Z = 122;

constexpr u32 UNICODE_DOLLAR_SIGN = 36;
constexpr u32 UNICODE_UNDERSCORE = 95;

constexpr u32 UNICODE_DIGIT_ZERO = 48;
constexpr u32 UNICODE_DIGIT_ONE = 49;
constexpr u32 UNICODE_DIGIT_TWO = 50;
constexpr u32 UNICODE_DIGIT_THREE = 51;
constexpr u32 UNICODE_DIGIT_FOUR = 52;
constexpr u32 UNICODE_DIGIT_FIVE = 53;
constexpr u32 UNICODE_DIGIT_SIX = 54;
constexpr u32 UNICODE_DIGIT_SEVEN = 55;
constexpr u32 UNICODE_DIGIT_EIGHT = 56;
constexpr u32 UNICODE_DIGIT_NINE = 57;

constexpr u32 UNICODE_LINE_FEED = 10;
constexpr u32 UNICODE_CARRIAGE_RETURN = 13;
constexpr u32 UNICODE_SPACE = 32;
constexpr u32 UNICODE_HORIZONTAL_TAB = 9;
constexpr u32 UNICODE_FORM_FEED = 12;
constexpr u32 UNICODE_BACKSLASH = 92;

// JLS 3.8, "Java letter" definition
// TODO add Unicode support
bool is_java_iden_start(u32 c)
{
    return c >= UNICODE_UPPERCASE_A && c <= UNICODE_UPPERCASE_Z ||
           c >= UNICODE_LOWERCASE_A && c <= UNICODE_LOWERCASE_Z || c == UNICODE_DOLLAR_SIGN || c == UNICODE_UNDERSCORE;
}

// JLS 3.8, "Java letter-or-digit" definition
// TODO add Unicode support
bool is_java_iden_part(u32 c)
{
    return is_java_iden_start(c) || is_dec_digit(c);
}

// JLS 3.4
bool is_java_single_line_terminator(u32 c)
{
    return c == UNICODE_LINE_FEED || c == UNICODE_CARRIAGE_RETURN;
}

// JLS 3.6
bool is_java_whitespace(u32 c)
{
    return c == UNICODE_SPACE || c == UNICODE_HORIZONTAL_TAB || c == UNICODE_FORM_FEED ||
           is_java_single_line_terminator(c);
}

bool is_dec_digit(u32 c)
{
    return c >= UNICODE_DIGIT_ZERO && c <= UNICODE_DIGIT_NINE;
}

bool is_hex_digit(u32 c)
{
    return c >= UNICODE_UPPERCASE_A && c <= UNICODE_UPPERCASE_F ||
           c >= UNICODE_LOWERCASE_A && c <= UNICODE_LOWERCASE_F || is_dec_digit(c);
}

bool Context::compile_input(const char *path)
{
    std::basic_ifstream<char8_t> stream{path};

    // A single UTF-8 encoding character from the input
    // stream. All input starts from this representation.
    char8_t raw;

    // Reconstructed Unicode code point from UTF-8 input.
    u32 raw_unicode = 0;

    // State counter for Unicode code point reconstruction.
    u32 raw_unicode_remaining = 0;

    u16 esc_utf16_a = 0;
    u16 esc_utf16_b = 0;
    u32 esc_utf16_remaining = 0;
    bool last_input_esc = false;

    u32 unicode;

    u64 line_num = 1;
    u64 col_num = 1;
    u64 backslash_count = 0;

    while (stream.read(&raw, 1))
    {
        // TODO update line_num and col_num

        if (!raw_unicode_remaining)
        {
            for (raw_unicode_remaining = 1; raw & (0x80 >> raw_unicode_remaining); ++raw_unicode_remaining)
                ;
            raw_unicode = raw;
        }
        else
        {
            raw_unicode = (raw_unicode << 6) | (raw & 0x30);
        }

        if (--raw_unicode_remaining)
        {
            continue;
        }

        if (--esc_utf16_remaining)
        {
            if (esc_utf16_remaining == 5)
            {
                if (raw_unicode != UNICODE_LOWERCASE_U)
                {
                    // TODO fatal
                }
                unicode = 0;
            }
            else if (esc_utf16_remaining < 5)
            {
                u32 val;
                switch (raw_unicode)
                {
                case UNICODE_LOWERCASE_A:
                case UNICODE_LOWERCASE_B:
                case UNICODE_LOWERCASE_C:
                case UNICODE_LOWERCASE_D:
                case UNICODE_LOWERCASE_E:
                case UNICODE_LOWERCASE_F:
                    raw_unicode -= UNICODE_LOWERCASE_A - UNICODE_UPPERCASE_A;
                case UNICODE_UPPERCASE_A:
                case UNICODE_UPPERCASE_B:
                case UNICODE_UPPERCASE_C:
                case UNICODE_UPPERCASE_D:
                case UNICODE_UPPERCASE_E:
                case UNICODE_UPPERCASE_F:
                    val = raw_unicode - UNICODE_UPPERCASE_A;
                    break;
                case UNICODE_DIGIT_ZERO:
                case UNICODE_DIGIT_ONE:
                case UNICODE_DIGIT_TWO:
                case UNICODE_DIGIT_THREE:
                case UNICODE_DIGIT_FOUR:
                case UNICODE_DIGIT_FIVE:
                case UNICODE_DIGIT_SIX:
                case UNICODE_DIGIT_SEVEN:
                case UNICODE_DIGIT_EIGHT:
                case UNICODE_DIGIT_NINE:
                    val = raw_unicode - UNICODE_DIGIT_ZERO;
                    break;
                default:;
                    // TODO fatal
                }
                unicode |= val;
            }

            continue;
        }

        if (raw_unicode == UNICODE_BACKSLASH)
        {
            backslash_count++;

            if (last_input_esc)
            {
                last_input_esc = false;
                unicode = raw_unicode;
                goto lex;
            }
            else
            {
                esc_utf16_remaining = 6;
                continue;
            }
        }
    lex:;
    }

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