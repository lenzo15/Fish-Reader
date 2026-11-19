// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "platform/shortcuts.hpp"
#include <shlobj.h>

namespace leaf {
struct Record {
    Str path;
    Pos cursor = 0;
    std::vector<Pos> marks;
};
struct State {
    std::vector<Record> recent;
    static constexpr int linePercents[] = {120, 140, 155, 172, 190, 210, 240};
    static constexpr int defaultSpacing = 3;
    static constexpr DWORD noBackground = 0xffffffff;
    int font = 20, theme = 0, spacing = defaultSpacing;
    DWORD background = noBackground; // 0xRRGGBB; sentinel follows the preset.
    bool pin = false;
    Language language = Language::Chinese;
    Keymap keys = defaultKeys;
    fs::path file;
    void init() {
        PWSTR p = nullptr;
        check(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p));
        file = fs::path(p) / L"MoyuLeaf" / L"state.bin";
        CoTaskMemFree(p);
    }
    Record &touch(const Str &p) {
        auto it = std::find_if(recent.begin(), recent.end(),
                               [&](auto &r) { return !_wcsicmp(p.c_str(), r.path.c_str()); });
        if (it != recent.end())
            std::rotate(recent.begin(), it, it + 1);
        else
            recent.insert(recent.begin(), {p, 0, {}});
        if (recent.size() > 32)
            recent.resize(32);
        return recent.front();
    }
    void load() {
        std::ifstream f(file, std::ios::binary);
        if (!f)
            return;
        auto get = [&]() {
            uint32_t n = 0;
            if (!f.read((char *)&n, 4))
                throw 1;
            return n;
        };
        try {
            if (get() != 0x3146414c)
                return;
            int font2 = std::clamp<int>(get(), 14, 32), theme2 = get() % 3;
            bool pin2 = get() != 0;
            auto count = get();
            if (count > 32)
                return;
            std::vector<Record> records;
            while (count--) {
                auto n = get();
                if (!n || n > 32768)
                    return;
                Record r;
                r.path.resize(n);
                if (!f.read((char *)r.path.data(), 2 * n))
                    return;
                r.cursor = get();
                auto m = get();
                if (m > 256)
                    return;
                while (m--)
                    r.marks.push_back(get());
                records.push_back(std::move(r));
            }
            recent = std::move(records);
            theme = theme2;
            font = font2;
            pin = pin2;
            // Optional appearance trailer: keep all v1 books and bookmarks, even if a trailer is absent.
            language = Language::Chinese;
            keys = defaultKeys;
            spacing = defaultSpacing;
            background = noBackground;
            if (f.peek() != std::char_traits<char>::eof() && get() == 0x31505041) {
                auto s = get(), rgb = get();
                if (s < std::size(linePercents))
                    spacing = int(s);
                if (rgb <= 0xffffff)
                    background = rgb;
                // Optional UI-language trailer, absent in legacy profiles.
                if (f.peek() != std::char_traits<char>::eof() && get() == 0x31474e4c) {
                    if (get() == 1)
                        language = Language::English;
                    if (f.peek() != std::char_traits<char>::eof() && get() == 0x3159454b &&
                        get() == keys.size()) {
                        Keymap loaded{};
                        for (auto &k : loaded)
                            k = get();
                        if (validMap(loaded))
                            keys = loaded;
                    }
                }
            }
        } catch (int) {
        }
    }
    bool save() const {
        std::error_code ec;
        fs::create_directories(file.parent_path(), ec);
        if (ec)
            return false;
        auto tmp = file;
        tmp += L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f)
            return false;
        auto put = [&](uint32_t n) { f.write((const char *)&n, 4); };
        put(0x3146414c);
        put(font);
        put(theme);
        put(pin);
        put((uint32_t)recent.size());
        for (auto &r : recent) {
            put((uint32_t)r.path.size());
            f.write((const char *)r.path.data(), r.path.size() * 2);
            put(r.cursor);
            put((uint32_t)r.marks.size());
            for (auto m : r.marks)
                put(m);
        }
        put(0x31505041);
        put(spacing);
        put(background);
        put(0x31474e4c);
        put(static_cast<uint32_t>(language));
        put(0x3159454b);
        put(static_cast<uint32_t>(keys.size()));
        for (auto key : keys)
            put(key);
        f.close();
        if (!f ||
            !MoveFileExW(tmp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            fs::remove(tmp, ec);
            return false;
        }
        return true;
    }
};
} // namespace leaf