// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "common.hpp"
#include <array>
namespace leaf {
// Stable on-disk order. Fixed navigation and dialog keys are deliberately not in the editable key set.
enum class Slot : unsigned {
    Boss,
    Focus,
    Open,
    Library,
    Contents,
    Bookmark,
    Settings,
    Previous,
    Next,
    First,
    Last,
    Count
};
constexpr unsigned index(Slot s) {
    return static_cast<unsigned>(s);
}
using Chord = uint32_t;
constexpr Chord chord(UINT mods, UINT vk) {
    return (mods << 16) | vk;
}
using Keymap = std::array<Chord, index(Slot::Count)>;
inline constexpr Keymap defaultKeys{chord(MOD_CONTROL, 'Q'),
                                    chord(MOD_CONTROL, VK_OEM_7),
                                    chord(MOD_CONTROL, 'O'),
                                    chord(MOD_CONTROL, 'L'),
                                    chord(MOD_CONTROL, 'I'),
                                    chord(MOD_CONTROL, 'B'),
                                    chord(MOD_CONTROL, VK_OEM_COMMA),
                                    'K',
                                    'J',
                                    VK_HOME,
                                    VK_END};
inline constexpr Msg keyNames[] = {Msg::Boss,     Msg::Focus,     Msg::PickTitle, Msg::Shelf,
                                   Msg::Chapters, Msg::MarkPage,  Msg::Settings,  Msg::PrevPage,
                                   Msg::NextPage, Msg::FirstPage, Msg::LastPage};
static_assert(std::size(keyNames) == defaultKeys.size());
inline bool mainKey(UINT k) {
    return (k >= 'A' && k <= 'Z') || (k >= '0' && k <= '9') || (k >= VK_F1 && k <= VK_F11) ||
           k == VK_HOME || k == VK_END || k == VK_OEM_7 || k == VK_OEM_COMMA || k == VK_OEM_PERIOD ||
           k == VK_OEM_3;
}
inline bool allowed(Slot s, Chord v) {
    if (!v)
        return true;
    auto mods = HIWORD(v);
    auto vk = LOWORD(v);
    return !(mods & ~(MOD_CONTROL | MOD_ALT | MOD_SHIFT)) && mainKey(vk) &&
           (s != Slot::Boss || (mods & (MOD_CONTROL | MOD_ALT))) && !(vk == VK_F4 && (mods & MOD_ALT));
}
inline Slot owner(const Keymap &map, Chord v) {
    if (v)
        for (unsigned i = 0; i < map.size(); i++)
            if (map[i] == v)
                return Slot(i);
    return Slot::Count;
}
inline bool validMap(const Keymap &map) {
    for (unsigned i = 0; i < map.size(); i++)
        if (!allowed(Slot(i), map[i]) || (map[i] && owner(map, map[i]) != Slot(i)))
            return false;
    return true;
}
inline Str keyName(UINT k) {
    if ((k >= 'A' && k <= 'Z') || (k >= '0' && k <= '9'))
        return Str(1, wchar_t(k));
    if (k >= VK_F1 && k <= VK_F11)
        return L"F" + std::to_wstring(k - VK_F1 + 1);
    if (k == VK_HOME)
        return L"Home";
    if (k == VK_END)
        return L"End";
    wchar_t name[64]{};
    if (GetKeyNameTextW(MapVirtualKeyW(k, MAPVK_VK_TO_VSC) << 16, name, 64))
        return name;
    return L"VK " + std::to_wstring(k);
}
inline Str modName(UINT m) {
    Str s;
    if (m & MOD_CONTROL)
        s = L"Ctrl";
    if (m & MOD_ALT)
        s += s.empty() ? L"Alt" : L"+Alt";
    if (m & MOD_SHIFT)
        s += s.empty() ? L"Shift" : L"+Shift";
    return s;
}
inline Str chordName(Chord v, Language language) {
    if (!v)
        return translate(language, Msg::NoBind);
    auto s = modName(HIWORD(v));
    return (s.empty() ? s : s + L"+") + keyName(LOWORD(v));
}
} // namespace leaf
