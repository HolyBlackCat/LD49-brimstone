#pragma once

struct SavedProgress
{
    static constexpr std::string_view path = "progress.txt";

    MEMBERS(
        DECL(int INIT=1) level
    )
};
