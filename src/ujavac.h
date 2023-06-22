#ifndef UJAVAC_H_
#define UJAVAC_H_

#include <cstdio>
#include <format>
#include <span>
#include <string_view>

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

class Context
{
  private:
    const std::span<std::string_view> m_inputs;

  public:
    explicit Context(std::span<std::string_view> inputs) : m_inputs(inputs)
    {
    }
    u8 compile();
};

#endif
