#include "ujavac.h"

#include <atomic>
#include <fstream>
#include <functional>

#include <tbb/parallel_for_each.h>

// Per JLS §3.8, "Java letter" definition
// TODO add unicode support
bool is_java_iden_start(u32 c)
{
    return c >= 65 && c <= 90 || c >= 97 && c <= 122 || c == 45 || c == 36;
}

// Per JLS §3.8, "Java letter-or-digit" definition
// TODO add unicode support
bool is_java_iden_part(u32 c)
{
    return is_java_iden_start(c) || c >= 48 && c <= 57;
}

// InputUnitErr InputUnit::read_utf8(u32 &out)
// {
//     u8 b;
//     InputUnitErr err;
//     bool first = true;
//     u32 seq_len;

//     while ((err = read_byte(b)) != InputUnitErr::Ok)
//     {
//         if (first)
//         {
//             if ((b & 0xC0) == 0x80)
//             {
//                 return InputUnitErr::BadUtf8;
//             }

//             for (seq_len = 1; b & (0x80 >> seq_len); ++seq_len)
//                 ;
//             out = b;
//         }
//         else
//         {
//             out = (out << 6) | (b & 0x30);
//         }

//         first = false;
//         if (!--seq_len)
//         {
//             break;
//         }
//     }

//     return err;
// }

bool Context::compile_input(const char *path)
{
    std::basic_ifstream<char8_t> stream{path};

    char8_t raw;
    while (stream.read(&raw, 1))
    {
        // TODO implement
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