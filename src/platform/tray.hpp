// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "common.hpp"
#include <shellapi.h>

namespace leaf {
// One shell icon per hidden window. Explorer decides pinning vs. overflow.
struct ReaderTray {
    static constexpr UINT message = WM_APP + 23;
    static constexpr UINT id = 1;
    bool installed = false, version4 = false;

    static NOTIFYICONDATAW identity(HWND hwnd) {
        NOTIFYICONDATAW n{};
        n.cbSize = sizeof(n);
        n.hWnd = hwnd;
        n.uID = id;
        return n;
    }
    bool show(HWND hwnd, Language lang) {
        if (installed)
            return true;
        auto n = identity(hwnd);
        n.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
        n.uCallbackMessage = message;
        n.hIcon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(2), IMAGE_ICON,
                                                GetSystemMetrics(SM_CXSMICON),
                                                GetSystemMetrics(SM_CYSMICON), LR_SHARED));
        if (!n.hIcon)
            n.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wcscpy_s(n.szTip, translate(lang, Msg::TrayTip));
        if (!Shell_NotifyIconW(NIM_ADD, &n) && !Shell_NotifyIconW(NIM_MODIFY, &n))
            return false;
        installed = true;
        n.uVersion = NOTIFYICON_VERSION_4;
        version4 = Shell_NotifyIconW(NIM_SETVERSION, &n) != FALSE;
        return true;
    }
    bool activation(WPARAM w, LPARAM l) const {
        if (!installed || (version4 ? HIWORD(l) != id : w != id))
            return false;
        const auto event = LOWORD(l);
        return event == NIN_SELECT || event == NIN_KEYSELECT || event == WM_LBUTTONUP;
    }
    void remove(HWND hwnd) {
        if (installed) {
            auto n = identity(hwnd);
            Shell_NotifyIconW(NIM_DELETE, &n);
        }
        installed = version4 = false;
    }
};
} // namespace leaf
