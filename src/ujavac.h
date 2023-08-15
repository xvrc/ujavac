#ifndef UJAVAC_H_
#define UJAVAC_H_

#include <cstdio>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using s8 = signed char;
using u8 = unsigned char;
using s16 = signed short;
using u16 = unsigned short;
using s32 = signed int;
using u32 = unsigned int;
using s64 = signed long long;
using u64 = unsigned long long;

template <class... Args> u32 print(std::FILE *f, std::format_string<Args...> fmt, Args &&...args)
{
    int ret =
        std::fprintf(f, "%s", std::vformat(fmt.get(), std::make_format_args(std::forward<Args>(args)...)).c_str());
    return ret < 0 ? 0 : ret;
}

template <class... Args> u32 print(std::format_string<Args...> fmt, Args &&...args)
{
    return print(stdout, fmt, std::forward<Args>(args)...);
}

template <class... Args> u32 println(std::FILE *f, std::format_string<Args...> fmt, Args &&...args)
{
    int ret =
        std::fprintf(f, "%s\n", std::vformat(fmt.get(), std::make_format_args(std::forward<Args>(args)...)).c_str());
    return ret < 0 ? 0 : ret;
}

template <class... Args> u32 println(std::format_string<Args...> fmt, Args &&...args)
{
    return println(stdout, fmt, std::forward<Args>(args)...);
}

enum class LexerItem
{
    WhiteSpace,
    TraditionalComment,
    EndOfLineComment,
    // Indeterminate form that can morph into one
    // of: indentifier, reserved keyword, or literal
    IdentifierChars,
    Identifier,
};

class Compiler
{
  public:
    Compiler(const char *input, const char *output);
    bool compile();

  private:
    void push_diagnostic(std::string_view msg);

    void ascii_token_append(char c);
    bool ascii_token_contains(std::string_view token) const;

    const char *m_input;
    const char *m_output;

    // Reconstructed Unicode code point from UTF-8 input.
    u32 m_raw_unicode;
    // State counter for Unicode code point reconstruction.
    u32 m_raw_unicode_remaining;

    // Source code line segmentation state.
    u64 m_line_num;
    u64 m_col_num;
    bool prev_raw_cr;

    // State variables responsible for keeping track of
    // Unicode escape sequences (i.e. \uXXXX).
    u16 m_esc_utf16[2];
    u32 m_esc_utf16_len;
    u32 m_esc_utf16_remaining;
    bool m_prev_from_esc;

    // Backslashes need special care during parsing because
    // they're used in escape sequences at various stages.
    bool m_prev_backslash;
    u64 m_raw_backslash_count;

    char m_ascii_tok_buf[sizeof("synchronized")];
    u32 m_ascii_tok_buf_len;
    // Position of the current token's first character in the input stream.
    u64 m_ascii_tok_buf_line_num;
    u64 m_ascii_tok_buf_col_num;

    LexerItem m_lexer_item;
    bool m_prev_trad_comment_end_star;
};

class CompilerManager
{
  public:
    explicit CompilerManager(std::span<const char *> inputs);
    u8 run() const;

  private:
    bool compile_unit(u32 i) const;

    const std::span<const char *> m_inputs;
    std::vector<std::string> m_outputs;
};

#endif
