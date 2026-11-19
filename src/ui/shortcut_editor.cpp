// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/window.hpp"
namespace leaf {
Str Window::bossStatus() const {
    auto v = state.keys[index(Slot::Boss)];
    return Str(tr(!hot && v ? Msg::KeyBusy : Msg::Boss)) + L" · " + chordName(v, state.language);
}

// Register the replacement under the inactive ID. On rejection or a failed save,
// keep both the old on-disk binding and its Windows registration.
Msg Window::applyShortcut(Slot slot, Chord value) {
    if (index(slot) >= state.keys.size() || !allowed(slot, value))
        return Msg::InvalidBind;
    auto used = owner(state.keys, value);
    if (used != Slot::Count && used != slot)
        return Msg::DupBind;
    const bool boss = slot == Slot::Boss;
    const Chord old = state.keys[index(slot)];
    if (old == value && (!boss || hot || !value))
        return Msg::Count;
    const int next = hotId == 1 ? 2 : 1;
    const bool reserve = boss && value;
    if (reserve && !registerKey(hwnd, next, HIWORD(value) | MOD_NOREPEAT, LOWORD(value)))
        return Msg::OsBusy;
    // A modified local binding must not already be owned globally at this moment.
    // Only the boss key stays registered once the edit is finished.
    if (!boss && (HIWORD(value) & (MOD_CONTROL | MOD_ALT))) {
        if (!registerKey(hwnd, 3, HIWORD(value) | MOD_NOREPEAT, LOWORD(value)))
            return Msg::OsBusy;
        unregisterKey(hwnd, 3);
    }
    state.keys[index(slot)] = value;
    if (!state.save()) {
        state.keys[index(slot)] = old;
        if (reserve)
            unregisterKey(hwnd, next);
        return Msg::SaveFailed;
    }
    if (boss) {
        if (hot)
            unregisterKey(hwnd, hotId);
        hot = reserve;
        if (hot)
            hotId = next;
    }
    dirty = false;
    invalid();
    return Msg::Count;
}

bool Window::keysVisible() const {
    return appearanceTab == 3 && (screen == Settings || (focus && screen == Reading && bubbles && options));
}

void Window::endRecording() {
    recording = conflictOwner = Slot::Count;
    keyHint = Msg::KeyHelp;
}

void Window::scrollKeys(int delta) {
    int next = std::clamp(keyFirst + delta, 0, int(Slot::Count) - keyRows);
    if (next == keyFirst)
        return;
    endRecording();
    keyFirst = next;
    selected = -1;
    hover = Nil;
    hits.clear();
    invalid();
}

void Window::recordValue(Chord value) {
    if (recording == Slot::Count)
        return;
    Msg result = applyShortcut(recording, value);
    if (result == Msg::Count) {
        endRecording();
        keyHint = Msg::KeySaved;
    } else {
        keyHint = (result == Msg::InvalidBind && recording == Slot::Boss &&
                   !(HIWORD(value) & (MOD_CONTROL | MOD_ALT)))
                      ? Msg::BossModifiers
                      : result;
        conflictOwner = result == Msg::DupBind ? owner(state.keys, value) : Slot::Count;
    }
    invalid();
}

bool Window::captureKey(UINT vk, UINT mods, bool repeat) {
    // Do not let a held newly assigned key immediately navigate or change a second binding.
    if (repeat && capturedVK == vk)
        return true;
    if (recording == Slot::Count)
        return false;
    if (repeat || vk == VK_CONTROL || vk == VK_MENU || vk == VK_SHIFT || vk == VK_LWIN || vk == VK_RWIN)
        return true;
    capturedVK = vk;
    if (vk == VK_ESCAPE && !mods) {
        endRecording();
        invalid();
    } else
        recordValue(vk == VK_DELETE && !mods ? 0 : chord(mods, vk));
    return true;
}

void Window::shortcutList(float l, float r, float y, float bottom) {
    const int total = int(Slot::Count);
    float top = y + 56, footer = bottom - 28;
    float step = std::min(32.f, footer - top);
    keyRows = std::clamp(int((footer - top) / std::max(step, 1.f)), 1, total);
    keyFirst = std::clamp(keyFirst, 0, total - keyRows);
    if (recording != Slot::Count)
        keyFirst = std::clamp(keyFirst, std::max(0, int(recording) - keyRows + 1), int(recording));
    Str hint = tr(keyHint);
    if (conflictOwner != Slot::Count)
        hint += tr(keyNames[index(conflictOwner)]);
    label(hint, {l + 3, y + 34, r, top}, 13, c.accent);
    for (int i = keyFirst; i < keyFirst + keyRows; ++i) {
        int id = KeyRow + i;
        bool active = recording == Slot(i);
        float row = top + (i - keyFirst) * step, mid = l + (r - l) * .51f;
        Box bounds = {l, row, r, row + step - 1};
        if (active || hover == id)
            panel(bounds, 6, c.tint);
        else
            line(l, row + step - 1, r, row + step - 1, c.rule);
        label(tr(keyNames[i]), {l + 4, row, mid - 2, bounds.bottom}, 13, c.ink, active);
        Str value = active ? tr(Msg::KeyListening) : chordName(state.keys[i], state.language);
        if (!active && i == int(Slot::Boss) && state.keys[i] && !hot)
            value = L"! " + value;
        label(value, {mid, row, r - 2, bounds.bottom}, 13, c.accent, true, true);
        target(id, bounds);
    }
    float f = footer + 2, end = bottom - 3;
    if (keyRows < total) {
        if (keyFirst)
            button(KeysEarlier, {l, f, l + 24, end}, L"‹");
        label(std::to_wstring(keyFirst + 1) + L"–" + std::to_wstring(keyFirst + keyRows) + L" / " +
                  std::to_wstring(total),
              {l + 24, f, l + 80, end}, 12, c.muted, false, true);
        if (keyFirst + keyRows < total)
            button(KeysLater, {l + 82, f, l + 106, end}, L"›");
    }
    if (recording != Slot::Count)
        button(KeyDefault, {r - 72, f, r, end}, tr(Msg::KeyDefault));
}
} // namespace leaf
