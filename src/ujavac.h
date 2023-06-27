#ifndef UJAVAC_H_
#define UJAVAC_H_

#include <cstdio>
#include <format>
#include <span>
#include <string>
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

class Compiler
{
  public:
    explicit Compiler(std::span<const char *> inputs) : m_inputs(inputs)
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

    u8 compile() const;

  private:
    bool compile_input(u32 i) const;

    const std::span<const char *> m_inputs;
    std::vector<std::string> m_outputs;
};

#endif
