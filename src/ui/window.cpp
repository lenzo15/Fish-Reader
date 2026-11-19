// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/window.hpp"
#include <commdlg.h>
#include <dwmapi.h>

namespace leaf {
using namespace D2D1;

const wchar_t *Window::tr(Msg msg) const {
    return translate(state.language, msg);
}

void Window::updateTitle() {
    Str title = book ? book->file.stem().wstring() + L"  ·  " + tr(Msg::Brand) : tr(Msg::EmptyTitle);
    SetWindowTextW(hwnd, title.c_str());
}

void Window::invalid() {
    InvalidateRect(hwnd, nullptr, FALSE);
}

void Window::note(Msg id) {
    note(Fail{id});
}

void Window::note(Fail s) {
    notice = std::move(s);
    SetTimer(hwnd, 3, 3600, nullptr);
    invalid();
}

void Window::changed() {
    dirty = true;
    SetTimer(hwnd, 2, 700, nullptr);
}

void Window::save() {
    if (dirty) {
        if (state.save())
            dirty = false;
        else
            note(Msg::SaveFailed);
    }
}

void Window::stopDrag() {
    dragging = false;
    resizeEdge = HTNOWHERE;
    if (GetCapture() == hwnd)
        ReleaseCapture();
}

void Window::clearBubbles() {
    endRecording();
    bubbles = options = false;
    hover = Nil;
    selected = -1;
    keyboard = false;
    hits.clear();
    invalid();
}

SIZE Window::minOuter(bool inFocus) const {
    RECT rc{0, 0, LONG(std::ceil((inFocus ? 224 : 256) * scale)),
            LONG(std::ceil((inFocus ? 168 : 216) * scale))};
    if (!inFocus)
        AdjustWindowRectExForDpi(&rc, WS_OVERLAPPEDWINDOW, FALSE,
                                 (DWORD)GetWindowLongPtrW(hwnd, GWL_EXSTYLE), UINT(std::round(scale * 96)));
    return {rc.right - rc.left, rc.bottom - rc.top};
}

void Window::setFocusMode(bool on) {
    if (focus == on)
        return;
    stopDrag();
    clearBubbles();
    TRACKMOUSEEVENT cancel{sizeof(cancel), TME_CANCEL | TME_LEAVE, hwnd, 0};
    TrackMouseEvent(&cancel);
    auto style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    constexpr LONG_PTR frameMask = LONG_PTR(WS_OVERLAPPEDWINDOW) | LONG_PTR(WS_POPUP);
    RECT desired{};
    bool sizeWindow = false;
    if (on) {
        savedFrame = style & frameMask;
        savedBorder = 0xffffffff; // DWMWA_COLOR_DEFAULT on Windows 11.
        DwmGetWindowAttribute(hwnd, 34, &savedBorder, sizeof(savedBorder));
        enteredZoomed = IsZoomed(hwnd);
        if (enteredZoomed && GetWindowRect(hwnd, &zoomRect)) {
            MONITORINFO mi{};
            mi.cbSize = sizeof(mi);
            if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
                desired = mi.rcWork;
                sizeWindow = true;
            }
        }
        notice.reset();
    } else if (enteredZoomed && IsZoomed(hwnd)) {
        desired = zoomRect;
        sizeWindow = true;
    }
    if (!on && !sizeWindow && !IsZoomed(hwnd)) {
        GetWindowRect(hwnd, &desired);
        SIZE min = minOuter(false);
        if (desired.right - desired.left < min.cx || desired.bottom - desired.top < min.cy) {
            desired.right = std::max(desired.right, desired.left + min.cx);
            desired.bottom = std::max(desired.bottom, desired.top + min.cy);
            sizeWindow = true;
        }
    }
    focus = on;
    SetWindowLongPtrW(hwnd, GWL_STYLE, (style & ~frameMask) | (on ? LONG_PTR(WS_POPUP) : savedFrame));
    COLORREF borderColor = on ? 0xfffffffe : savedBorder; // NONE / original
    DwmSetWindowAttribute(hwnd, 34, &borderColor, sizeof(borderColor));
    SetWindowPos(hwnd, nullptr, desired.left, desired.top, desired.right - desired.left,
                 desired.bottom - desired.top,
                 SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE |
                     (sizeWindow ? 0 : SWP_NOMOVE | SWP_NOSIZE));
}

void Window::pageScreen(Screen s) {
    endRecording();
    if (choosingColor)
        endColor(false);
    if (s != Reading)
        setFocusMode(false);
    screen = s;
    hits.clear();
    first = 0;
    hover = Nil;
    selected = -1;
    keyboard = false;
    if (s == Chapters && book)
        first = std::max(0, book->chapter(pos) - 1);
    invalid();
}

void Window::theme() {
    c = palettes[state.theme];
    BOOL dark = state.theme != 1;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof dark);
    COLORREF bg = RGB(c.bg >> 16, (c.bg >> 8) & 255, c.bg & 255);
    DwmSetWindowAttribute(hwnd, 35, &bg, sizeof bg);
    COLORREF fg = RGB(c.ink >> 16, (c.ink >> 8) & 255, c.ink & 255);
    DwmSetWindowAttribute(hwnd, 36, &fg, sizeof fg);
}

void Window::init() {
    check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2.GetAddressOf()));
    check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                              (IUnknown **)dw.GetAddressOf()));
    const bool normalProfile = state.file.empty();
    if (normalProfile)
        state.init();
    state.load();
    updateTitle();
    scale = GetDpiForWindow(hwnd) / 96.f;
    theme();
    int rounded = 2;
    DwmSetWindowAttribute(hwnd, 33, &rounded, sizeof rounded);
    if (state.pin)
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
    taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    Chord boss = state.keys[index(Slot::Boss)];
    hot = normalProfile && boss && registerKey(hwnd, hotId, HIWORD(boss) | MOD_NOREPEAT, LOWORD(boss));
    if (normalProfile && boss && !hot)
        note(Msg::HotConflict);
    DragAcceptFiles(hwnd, TRUE);
}

void Window::beginFile(fs::path path) {
    if (loading) {
        note(Msg::Wait);
        return;
    }
    if (book && !_wcsicmp(book->file.c_str(), path.c_str())) {
        pageScreen(Reading);
        return;
    }
    loadName = path.filename().wstring();
    job = std::async(std::launch::async, [path] {
        ComScope com;
        return openBook(path);
    });
    loading = true;
    SetTimer(hwnd, 1, 25, nullptr);
    invalid();
}

void Window::complete() {
    if (!loading || job.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;
    loading = false;
    KillTimer(hwnd, 1);
    try {
        auto fresh = std::make_unique<Book>(job.get());
        pages.reset();
        book = std::move(fresh);
        auto &rec = state.touch(book->file.wstring());
        pos = std::min(rec.cursor, (Pos)book->text.size() - 1);
        rec.marks.erase(std::remove_if(rec.marks.begin(), rec.marks.end(),
                                       [&](Pos p) { return p >= book->text.size(); }),
                        rec.marks.end());
        setFocusMode(false);
        notice.reset();
        pageScreen(Reading);
        changed();
        updateTitle();
    } catch (const Fail &e) {
        note(e);
    } catch (...) {
        note(Msg::ImportFailed);
    }
}

void Window::picker() {
    wchar_t name[32768] = {};
    OPENFILENAMEW o{};
    o.lStructSize = sizeof o;
    o.hwndOwner = hwnd;
    o.lpstrFile = name;
    o.nMaxFile = 32768;
    o.lpstrFilter = bookFilter(state.language);
    o.lpstrTitle = tr(Msg::PickTitle);
    o.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&o))
        beginFile(name);
}
} // namespace leaf
