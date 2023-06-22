#include <cstdio>
#include <format>
#include <utility>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace
{
using s8 = signed char;
using u8 = unsigned char;
using s16 = signed short;
using u16 = unsigned short;
using s32 = signed int;
using u32 = unsigned int;
using s64 = signed long long;
using u64 = unsigned long long;

bool is_file_terminal(std::FILE *f)
{
#ifdef _WIN32
    return _isatty(_fileno(f));
#else
    return isatty(fileno(f));
#endif
}

template <class... Args> u32 print(std::format_string<Args...> fmt, Args &&...args)
{
    int ret = std::printf("%s", std::vformat(fmt.get(), std::make_format_args(std::forward<Args>(args)...)).c_str());
    return ret < 0 ? 0 : ret;
}

template <class... Args> u32 println(std::format_string<Args...> fmt, Args &&...args)
{
    int ret = std::printf("%s\n", std::vformat(fmt.get(), std::make_format_args(std::forward<Args>(args)...)).c_str());
    return ret < 0 ? 0 : ret;
}

struct prog_opt_store
{
    const char *keys[3];
    const char *help = nullptr;
    const char *param = nullptr;
    bool extra = false;
};

constexpr prog_opt_store prog_opts[] = {{{"--system"}, "Override location of system modules", "<jdk>|none"},
                                        {
                                            {"-verbose"},
                                            "Output mesages about what the compiler is doing",
                                        },
                                        {{"--version", "-version"}, "Version information"},
                                        {{"-Werror"}, "Terminate compilation if warnings occur"}};

u8 compile()
{
    // TODO implement
    return 0;
}

void show_help()
{
    println("Usage: ujavac <options> <source files>");
    println("where possible options include:");

    for (u32 i = 0; i < std::size(prog_opts); i++)
    {
        u32 line_len = 0;
        const auto &opt = prog_opts[i];

        for (u32 j = 0; j < std::size(opt.keys); j++)
        {
            auto key = opt.keys[j];
            if (!key)
            {
                break;
            }

            if (j == 0)
            {
                line_len += print("  ");
            } else {
                line_len += print(", ");
            }

            if (opt.param)
            {
                line_len += print("{} {}", key, opt.param);
            }
            else
            {
                line_len += print("{}", key);
            }
        }

        constexpr u32 OPT_MAX_LEN = 40;
        if (line_len < OPT_MAX_LEN)
        {
            while (line_len <= OPT_MAX_LEN)
            {
                print(" ");
                line_len++;
            }
        }
        else
        {
            println("");
            print("      ");
        }

        println("{}", opt.help);
    }
}
} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        if (is_file_terminal(stdout))
        {
            show_help();
        }

        return 2;
    }

    bool opt_help = false;

    for (u32 i = 1; i < argc; i++)
    {

    }

    if (opt_help)
    {
        show_help();
        return 0;
    }

    return compile();
}