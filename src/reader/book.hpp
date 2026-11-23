// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "common.hpp"
namespace leaf {
struct Heading {
    Pos pos;
    Str name;
    int depth = 0;
};
struct Book {
    fs::path file;
    Str text;
    std::vector<Heading> toc;
    std::vector<Pos> blocks; // Layout boundaries plus EOF sentinel.
    void index();
    int chapter(Pos pos) const;
};
Book openBook(const fs::path &file);
} // namespace leaf