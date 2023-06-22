#include <cstdio>
#include <cstring>
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

enum class prog_opt
{
    help,
    system,
    verbose,
    version,
    werror
};

struct prog_opt_desc
{
    prog_opt id;
    const char *keys[3];
    const char *help = nullptr;
    const char *param = nullptr;
    bool extra = false;
};

constexpr prog_opt_desc prog_opt_descs[] = {
    {prog_opt::help, {"--help", "-help", "-?"}, "Show this help message"},
    {prog_opt::system, {"--system"}, "Override location of system modules", "<jdk>|none"},
    {
        prog_opt::verbose,
        {"-verbose"},
        "Output mesages about what the compiler is doing",
    },
    {prog_opt::version, {"--version", "-version"}, "Version information"},
    {prog_opt::werror, {"-Werror"}, "Terminate compilation if warnings occur"}};

u8 compile()
{
    // TODO implement
    return 0;
}

void print_usage(std::FILE* f)
{
    println(f, "Usage: ujavac <options> <source files>");
}

void print_usage()
{
    print_usage(stdout);
}

void print_help()
{
    print_usage();
    println("where possible options include:");

    for (auto &desc : prog_opt_descs)
    {
        u32 line_len = 0;

        for (u32 i = 0; i < std::size(desc.keys); i++)
        {
            auto key = desc.keys[i];
            if (!key)
            {
                break;
            }

            if (i == 0)
            {
                line_len += print("  ");
            }
            else
            {
                line_len += print(", ");
            }

            if (desc.param)
            {
                line_len += print("{} {}", key, desc.param);
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

        println("{}", desc.help);
    }
}

void print_version()
{
    println("ujavac 1.0.0");
}
} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        if (is_file_terminal(stdout))
        {
            print_help();
        }

        return 2;
    }

    for (u32 i = 1; i < argc; i++)
    {
        auto arg = argv[i];
        for (auto &desc : prog_opt_descs)
        {
            for (auto &key : desc.keys)
            {
                if (key && !std::strcmp(key, arg))
                {
                    if (i == argc - 1 && desc.param)
                    {
                        println(stderr, "error: {} requires an argument", arg);
                        print_usage(stderr);
                        println("use --help for a list of possible options");
                        return 1;
                    }

                    switch (desc.id)
                    {
                    case prog_opt::help:
                        print_help();
                        return 0;
                    case prog_opt::version:
                        print_version();
                        return 0;
                    }

                    goto outer;
                }
            }
        }

    outer:;
    }

    return compile();
}