#include "ujavac.h"

#include <atomic>
#include <cstdio>
#include <functional>

#include <tbb/parallel_for_each.h>

InputUnit::~InputUnit()
{
    if (m_file)
    {
        std::fclose(m_file);
    }
}

InputUnitErr InputUnit::open()
{
    if (m_file)
    {
        return InputUnitErr::Ok;
    }

    return (m_file = std::fopen(m_path.data(), "r")) ? InputUnitErr::Ok : InputUnitErr::BadOpen;
}

InputUnitErr InputUnit::read(u8 &c)
{
    auto res = std::fread(&c, 1, 1, m_file);
    if (res != 1)
    {
        if (std::feof(m_file))
        {
            return InputUnitErr::EndOfFile;
        }

        return InputUnitErr::BadRead;
    }

    return InputUnitErr::Ok;
}

bool Context::compile_input(const std::string_view &item)
{
    InputUnit unit{item};
    if (unit.open() != InputUnitErr::Ok)
    {
        return false;
    }

    // TODO implement

    return true;
}

u8 Context::compile()
{
    std::atomic_bool status = true;
    tbb::parallel_for_each(m_inputs.begin(), m_inputs.end(), [&, this](const std::string_view &item) {
        bool expected = true;
        status.compare_exchange_strong(expected, compile_input(item));
    });

    return status.load();
}