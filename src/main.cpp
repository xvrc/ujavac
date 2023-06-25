#include "ujavac.h"

#include <cstdio>
#include <cstring>
#include <format>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace
{
bool is_file_terminal(std::FILE *f)
{
#ifdef _WIN32
    return _isatty(_fileno(f));
#else
    return isatty(fileno(f));
#endif
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

void print_usage(std::FILE *f)
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

    std::vector<const char *> inputs;
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

        inputs.push_back(arg);

    outer:;
    }

    Context ctx{inputs};
    return ctx.compile();
}