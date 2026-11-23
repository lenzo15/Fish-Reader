// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/window.hpp"
#include "platform/single_instance.hpp"
#include <windowsx.h>

namespace leaf {
using namespace D2D1;

void Window::jump(Pos p) {
    pos = std::min(p, (Pos)book->text.size() - 1);
    state.recent.front().cursor = pos;
    changed();
    pageScreen(Reading);
}

void Window::action(int id) {
    if (choosingColor && id != Hide) {
        if (id >= PickSwatch && id < PickSwatch + int(std::size(paperSwatches))) {
            draft.fromRgb(paperSwatches[id - PickSwatch]);
            invalid();
        } else if (id == PickApply || id == PickCancel)
            endColor(id == PickApply);
        return;
    }
    if (keysVisible() && id >= KeyRow && id < KeyRow + int(Slot::Count)) {
        recording = Slot(id - KeyRow);
        conflictOwner = Slot::Count;
        keyHint = Msg::CaptureHint;
        invalid();
        return;
    }
    if (keysVisible() && id == KeyDefault && recording != Slot::Count) {
        recordValue(defaultKeys[index(recording)]);
        return;
    }
    if (recording != Slot::Count) {
        endRecording();
        invalid();
    }
    if (id >= Item) {
        int i = id - Item;
        if (screen == Shelf && i < (int)state.recent.size())
            beginFile(state.recent[i].path);
        else if (screen == Chapters && i < (int)book->toc.size())
            jump(book->toc[i].pos);
        else if (screen == Bookmarks && i < (int)state.recent.front().marks.size())
            jump(state.recent.front().marks[i]);
        return;
    }
    switch (id) {
    case Open:
        picker();
        break;
    case Home:
        pageScreen(Shelf);
        break;
    case Style:
        if (focus) {
            bubbles = true;
            action(BubbleSettings);
        } else
            pageScreen(Settings);
        break;
    case BubbleSettings:
        if (focus && bubbles) {
            options = !options;
            hits.clear();
            selected = -1;
            invalid();
        }
        break;
    case Read:
        if (book)
            pageScreen(Reading);
        break;
    case Contents:
        if (book) {
            pageScreen(Chapters);
        }
        break;
    case Marks:
        if (book) {
            pageScreen(Bookmarks);
        }
        break;
    case Focus:
        if (book) {
            setFocusMode(!focus);
            pageScreen(Reading);
        }
        break;
    case Back:
    case Next:
        if (book && screen == Reading) {
            layout();
            auto q = pages.visit(pos, rows, [](auto...) {});
            if (id == Back)
                jump(pages.previous(pos, rows));
            else if (q < book->text.size())
                jump(q);
            else
                note(Msg::EndBook);
        }
        break;
    case AddMark:
        if (book) {
            layout();
            auto &marks = state.recent.front().marks;
            auto aligned = pages.align(pos);
            auto it =
                std::find_if(marks.begin(), marks.end(), [&](auto v) { return pages.align(v) == aligned; });
            if (it == marks.end()) {
                if (marks.size() >= 256) {
                    note(Msg::MarkLimit);
                    break;
                }
                marks.push_back(aligned);
                std::sort(marks.begin(), marks.end());
                note(Msg::Marked);
            } else {
                marks.erase(it);
                note(Msg::Unmarked);
            }
            changed();
        }
        break;
    case Bigger:
    case Smaller:
        state.font = std::clamp(state.font + (id == Bigger ? 2 : -2), 14, 32);
        changed();
        invalid();
        break;
    case TabType:
    case TabColor:
    case TabWindow:
    case TabKeys:
        endRecording();
        keyFirst = 0;
        wheel = 0;
        appearanceTab = id - TabType;
        hits.clear();
        selected = -1;
        invalid();
        break;
    case KeysEarlier:
    case KeysLater:
        if (keysVisible())
            scrollKeys(id == KeysLater ? keyRows : -keyRows);
        break;
    case ToggleLanguage:
        state.language = state.language == Language::Chinese ? Language::English : Language::Chinese;
        updateTitle();
        changed();
        save();
        invalid();
        break;
    case ChooseBg:
        pickBackground();
        break;
    case ResetBg:
        setBackground(State::noBackground);
        break;
    case Tighter:
    case Looser:
        state.spacing =
            std::clamp(state.spacing + (id == Looser ? 1 : -1), 0, int(std::size(State::linePercents)) - 1);
        changed();
        invalid();
        break;
    case Theme0:
    case Theme1:
    case Theme2:
        state.theme = id - Theme0;
        state.background = State::noBackground;
        theme();
        changed();
        invalid();
        break;
    case Pin:
        state.pin = !state.pin;
        SetWindowPos(hwnd, state.pin ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        changed();
        invalid();
        break;
    case Translucent:
        glass = !glass;
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE,
                          glass ? GetWindowLongPtrW(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED
                                : GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
        if (glass)
            SetLayeredWindowAttributes(hwnd, 0, 205, LWA_ALPHA);
        invalid();
        break;
    case Up:
    case Down:
        scrollList(id == Down ? listRows : -listRows);
        break;
    case Hide:
        if (!hiddenToTray) {
            if (!IsWindowVisible(hwnd))
                break;
            if (!tray.show(hwnd, state.language)) {
                note(Msg::NoTray);
                break;
            }
            hiddenToTray = true;
            hiddenDialogs.clear();
            EnumThreadWindows(
                GetCurrentThreadId(),
                [](HWND win, LPARAM ctx) -> BOOL {
                    auto &a = *reinterpret_cast<Window *>(ctx);
                    if (win != a.hwnd && GetAncestor(win, GA_ROOTOWNER) == a.hwnd && IsWindowVisible(win))
                        a.hiddenDialogs.push_back(win);
                    return TRUE;
                },
                reinterpret_cast<LPARAM>(this));
            for (HWND dialog : hiddenDialogs)
                ShowWindow(dialog, SW_HIDE);
            ShowWindow(hwnd, SW_HIDE);
        } else {
            hiddenToTray = false;
            bool noActivate = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE) != 0;
            ShowWindow(hwnd, IsIconic(hwnd) ? SW_RESTORE : (noActivate ? SW_SHOWNA : SW_SHOW));
            HWND activate = hwnd;
            for (auto it = hiddenDialogs.rbegin(); it != hiddenDialogs.rend(); ++it)
                if (IsWindow(*it) && GetAncestor(*it, GA_ROOTOWNER) == hwnd) {
                    ShowWindow(*it, SW_SHOWNA);
                    activate = *it;
                }
            hiddenDialogs.clear();
            if (!noActivate)
                SetForegroundWindow(activate);
            tray.remove(hwnd);
        }
        break;
    }
}

void Window::scrollList(int delta) {
    if (screen != Shelf && screen != Chapters && screen != Bookmarks)
        return;
    int next = std::clamp(first + delta, 0, std::max(0, count() - listRows));
    if (next == first)
        return;
    first = next;
    hits.clear(); // Rows must not retain stale click/focus targets after scrolling.
    selected = -1;
    hover = Nil;
    keyboard = false;
    invalid();
}

void Window::dragChapterScroll(LPARAM xy) {
    if (!scrollingChapters || screen != Chapters || !book)
        return;
    float travel = (chapterTrack.bottom - chapterTrack.top) - (chapterThumb.bottom - chapterThumb.top);
    if (travel <= 0)
        return;
    float y = GET_Y_LPARAM(xy) / scale;
    float fraction = std::clamp((y - chapterGrab - chapterTrack.top) / travel, 0.f, 1.f);
    int next = int(std::lround(fraction * std::max(0, count() - listRows)));
    scrollList(next - first);
}

bool Window::key(WPARAM k, UINT mods, bool repeat) {
    if (captureKey(UINT(k), mods, repeat))
        return true;
    if (keysVisible() && !mods && (k == VK_PRIOR || k == VK_NEXT || k == VK_UP || k == VK_DOWN)) {
        scrollKeys(k == VK_PRIOR ? -keyRows : k == VK_NEXT ? keyRows : k == VK_UP ? -1 : 1);
        return true;
    }
    if (k == VK_TAB && !(mods & ~MOD_SHIFT) && !hits.empty()) {
        keyboard = true;
        int n = int(hits.size());
        bool back = mods & MOD_SHIFT;
        selected = selected < 0 ? (back ? n - 1 : 0) : (selected + (back ? n - 1 : 1)) % n;
        invalid();
        return true;
    }
    if (k == VK_RETURN && !mods && keyboard && selected >= 0 && selected < int(hits.size())) {
        action(hits[selected].id);
        return true;
    }
    if (choosingColor) {
        if (mods)
            return false;
        if (k == VK_ESCAPE)
            endColor(false);
        else if (k == VK_RETURN)
            endColor(true);
        else if (keyboard && selected >= 0 && selected < int(hits.size())) {
            float dx = k == VK_RIGHT  ? .01f
                       : k == VK_LEFT ? -.01f
                                      : 0,
                  dy = k == VK_UP     ? .01f
                       : k == VK_DOWN ? -.01f
                                      : 0;
            if (hits[selected].id == PickField) {
                draft.sat = std::clamp(draft.sat + dx, 0.f, 1.f);
                draft.value = std::clamp(draft.value + dy, 0.f, 1.f);
            }
            if (hits[selected].id == PickHue)
                draft.hue = std::clamp(draft.hue + dx - dy, 0.f, 1.f);
            invalid();
        }
        return true;
    }
    if (!mods && k == VK_ESCAPE) {
        setFocusMode(false);
        pageScreen(book ? Reading : Shelf);
        return true;
    }
    if (!mods && (k == VK_RIGHT || k == VK_DOWN || k == VK_NEXT || k == VK_SPACE)) {
        action(screen == Reading ? Next : Down);
        return true;
    }
    if (!mods && (k == VK_LEFT || k == VK_UP || k == VK_PRIOR)) {
        action(screen == Reading ? Back : Up);
        return true;
    }
    Slot slot = owner(state.keys, chord(mods, UINT(k)));
    if (slot == Slot::Count)
        return false;
    if (slot == Slot::Boss)
        return true; // Global shortcut is dispatched only by WM_HOTKEY.
    if (repeat && slot != Slot::Previous && slot != Slot::Next)
        return true;
    switch (slot) {
    case Slot::Focus:
        action(Focus);
        break;
    case Slot::Open:
        action(Open);
        break;
    case Slot::Library:
        action(Home);
        break;
    case Slot::Contents:
        action(Contents);
        break;
    case Slot::Bookmark:
        action(AddMark);
        break;
    case Slot::Settings:
        action(Style);
        break;
    case Slot::Previous:
        action(screen == Reading ? Back : Up);
        break;
    case Slot::Next:
        action(screen == Reading ? Next : Down);
        break;
    case Slot::First:
    case Slot::Last:
        if (screen == Reading && book) {
            layout();
            Pos last = pages.align(Pos(book->text.size()) - 1);
            jump(slot == Slot::First ? 0 : pages.previous(last, rows - 1));
        }
        break;
    default:
        break;
    }
    return true;
}

void Window::pointerEntered(bool nonClient) {
    TRACKMOUSEEVENT watch{sizeof(watch), TME_LEAVE | (nonClient ? DWORD(TME_NONCLIENT) : 0u), hwnd, 0};
    TrackMouseEvent(&watch);
    if (focus && screen == Reading && !choosingColor && !bubbles) {
        bubbles = true;
        invalid();
        UpdateWindow(hwnd); // Draw and install hit targets on the entry event.
    }
}

int Window::hit(LPARAM p) {
    float x = GET_X_LPARAM(p) / scale, y = GET_Y_LPARAM(p) / scale;
    // Like a native scrollbar, this is mouse-only; keyboard users retain the
    // existing arrow/page controls without an extra Tab stop for the track.
    if (!choosingColor && screen == Chapters && chapterTrack.bottom > chapterTrack.top &&
        x >= chapterTrack.left && x <= chapterTrack.right &&
        y >= chapterTrack.top && y <= chapterTrack.bottom)
        return ChapterScroll;
    for (auto t : hits) {
        auto b = t.box;
        if (x >= b.left && x <= b.right && y >= b.top && y <= b.bottom)
            return t.id;
    }
    return Nil;
}

POINT Window::screenPoint(LPARAM lp) const {
    POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    ClientToScreen(hwnd, &p);
    return p;
}

void Window::moveDrag(POINT p) {
    LONG dx = p.x - dragStart.x, dy = p.y - dragStart.y;
    if (!dx && !dy)
        return;
    if (IsZoomed(hwnd)) {
        // Keep the same horizontal grab point when pulling a maximized reader free.
        double grab = double(dragStart.x - dragRect.left) / std::max(1L, dragRect.right - dragRect.left);
        LONG topOffset = dragStart.y - dragRect.top;
        ShowWindow(hwnd, SW_RESTORE);
        enteredZoomed = false;
        GetWindowRect(hwnd, &dragRect);
        dragRect.left = dragStart.x - LONG(grab * (dragRect.right - dragRect.left));
        dragRect.top = dragStart.y - topOffset;
    }
    SetWindowPos(hwnd, nullptr, dragRect.left + dx, dragRect.top + dy, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

int Window::edgeAt(POINT p) const {
    if (!focus || IsZoomed(hwnd))
        return HTCLIENT;
    RECT r;
    GetWindowRect(hwnd, &r);
    if (!PtInRect(&r, p))
        return HTNOWHERE;
    LONG zone = LONG(std::ceil(5 * scale));
    bool l = p.x < r.left + zone, right = p.x >= r.right - zone, t = p.y < r.top + zone,
         b = p.y >= r.bottom - zone;
    if (t)
        return l ? HTTOPLEFT : right ? HTTOPRIGHT : HTTOP;
    if (b)
        return l ? HTBOTTOMLEFT : right ? HTBOTTOMRIGHT : HTBOTTOM;
    return l ? HTLEFT : right ? HTRIGHT : HTCLIENT;
}

void Window::resizeDrag(POINT p) {
    RECT next = dragRect;
    LONG dx = p.x - dragStart.x, dy = p.y - dragStart.y;
    SIZE min = minOuter(true);
    if (resizeEdge == HTLEFT || resizeEdge == HTTOPLEFT || resizeEdge == HTBOTTOMLEFT)
        next.left = std::min(next.right - min.cx, next.left + dx);
    if (resizeEdge == HTRIGHT || resizeEdge == HTTOPRIGHT || resizeEdge == HTBOTTOMRIGHT)
        next.right = std::max(next.left + min.cx, next.right + dx);
    if (resizeEdge == HTTOP || resizeEdge == HTTOPLEFT || resizeEdge == HTTOPRIGHT)
        next.top = std::min(next.bottom - min.cy, next.top + dy);
    if (resizeEdge == HTBOTTOM || resizeEdge == HTBOTTOMLEFT || resizeEdge == HTBOTTOMRIGHT)
        next.bottom = std::max(next.top + min.cy, next.bottom + dy);
    SetWindowPos(hwnd, nullptr, next.left, next.top, next.right - next.left, next.bottom - next.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT Window::msg(UINT m, WPARAM wp, LPARAM lp) {
    if (taskbarCreated && m == taskbarCreated && hiddenToTray) {
        // Explorer lost its icon table. If re-adding fails, restore the reader rather than strand it.
        tray.installed = false;
        if (!tray.show(hwnd, state.language))
            action(Hide);
        return 0;
    }
    switch (m) {
    case WM_COPYDATA: {
        Str file;
        if (!SingleInstance::request(reinterpret_cast<const COPYDATASTRUCT *>(lp), file))
            return 0;
        activate();
        if (!file.empty())
            beginFile(file);
        return 1;
    }
    case ReaderTray::message:
        if (hiddenToTray && tray.activation(wp, lp))
            action(Hide);
        return 0;
    case WM_CREATE:
        init();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        paint();
        return 0;
    case WM_GETMINMAXINFO: {
        auto *mm = (MINMAXINFO *)lp;
        auto min = minOuter(focus);
        mm->ptMinTrackSize = {min.cx, min.cy};
        return 0;
    }
    case WM_DPICHANGED: {
        if (scrollingChapters)
            stopDrag();
        chapterTrack = chapterThumb = {};
        scale = HIWORD(wp) / 96.f;
        auto r = (RECT *)lp;
        if (rt)
            rt->SetDpi(scale * 96, scale * 96);
        SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        invalid();
        return 0;
    }
    case WM_SIZE:
        if (scrollingChapters)
            stopDrag();
        chapterTrack = chapterThumb = {};
        if (rt && wp != SIZE_MINIMIZED) {
            rt->Resize(SizeU(LOWORD(lp), HIWORD(lp)));
        }
        invalid();
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        UINT mods = (GetKeyState(VK_CONTROL) < 0 ? MOD_CONTROL : 0) |
                    (GetKeyState(VK_SHIFT) < 0 ? MOD_SHIFT : 0) | (GetKeyState(VK_MENU) < 0 ? MOD_ALT : 0) |
                    ((GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0) ? MOD_WIN : 0);
        if (key(wp, mods, (lp & (1L << 30)) != 0))
            return 0;
        break;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (capturedVK == wp)
            capturedVK = 0;
        if (recording != Slot::Count)
            return 0;
        break;
    case WM_SYSCHAR:
        if (recording != Slot::Count || capturedVK)
            return 0;
        break;
    case WM_HOTKEY:
        if (hot && wp == WPARAM(hotId) && chord(LOWORD(lp), HIWORD(lp)) == state.keys[index(Slot::Boss)]) {
            // Windows delivers the existing global binding here instead of WM_KEYDOWN.
            if (recording != Slot::Count)
                captureKey(HIWORD(lp), LOWORD(lp), false);
            else
                action(Hide);
        }
        return 0;
    case WM_NCHITTEST:
        if (focus)
            return edgeAt({GET_X_LPARAM(lp), GET_Y_LPARAM(lp)});
        break;
    case WM_NCLBUTTONDOWN:
        if (focus && !IsZoomed(hwnd) && wp >= HTLEFT && wp <= HTBOTTOMRIGHT) {
            GetWindowRect(hwnd, &dragRect);
            dragStart = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            SetCapture(hwnd);
            if (GetCapture() == hwnd)
                resizeEdge = int(wp);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        if (choosingColor) {
            int id = hit(lp);
            if (id == PickField || id == PickHue) {
                SetCapture(hwnd);
                picking = id;
                pickAt(id, lp);
            }
            return 0;
        }
        if (hit(lp) == ChapterScroll) {
            float y = GET_Y_LPARAM(lp) / scale;
            chapterGrab = y >= chapterThumb.top && y <= chapterThumb.bottom
                              ? y - chapterThumb.top : (chapterThumb.bottom - chapterThumb.top) / 2;
            SetCapture(hwnd);
            scrollingChapters = GetCapture() == hwnd;
            dragChapterScroll(lp);
            invalid();
            return 0;
        }
        if (focus && bubbles && hit(lp) == BubbleMove) {
            dragStart = screenPoint(lp);
            GetWindowRect(hwnd, &dragRect);
            SetCapture(hwnd);
            dragging = GetCapture() == hwnd;
            invalid();
        }
        return 0;
    case WM_LBUTTONUP: {
        if (scrollingChapters) {
            dragChapterScroll(lp);
            stopDrag();
            hover = Nil;
            invalid();
            return 0;
        }
        if (resizeEdge != HTNOWHERE) {
            resizeDrag(screenPoint(lp));
            stopDrag();
            invalid();
            return 0;
        }
        if (dragging) {
            moveDrag(screenPoint(lp));
            stopDrag();
            RECT client{};
            GetClientRect(hwnd, &client);
            POINT pointer{};
            GetCursorPos(&pointer);
            ScreenToClient(hwnd, &pointer);
            if (!PtInRect(&client, pointer))
                clearBubbles();
            else
                invalid();
            return 0;
        }
        int id = hit(lp);
        if (choosingColor) {
            if (picking != Nil) {
                pickAt(picking, lp);
                picking = Nil;
                if (GetCapture() == hwnd)
                    ReleaseCapture();
            } else
                action(id);
            return 0;
        }
        if (!id && recording != Slot::Count) {
            endRecording();
            invalid();
        }
        if (id)
            action(id);
        else if (book && screen == Reading && !focus) {
            float x = GET_X_LPARAM(lp) / scale, y = GET_Y_LPARAM(lp) / scale, left = textLeft(),
                  rail = h - (compact() ? 36 : 60);
            if (y >= rail - 20 && y <= rail - 3 && x >= left && x <= w - left) {
                layout();
                double fraction = std::clamp(double(x - left) / pages.width, 0., 1.);
                jump(pages.align(Pos(fraction * (book->text.size() - 1))));
            }
        }
        return 0;
    }
    case WM_LBUTTONDBLCLK:
        if (!choosingColor && !hit(lp) && !options && book && screen == Reading)
            action(Focus);
        return 0;
    case WM_NCMOUSEMOVE:
        if (focus)
            pointerEntered(true);
        break;
    case WM_MOUSEMOVE: {
        if (scrollingChapters) {
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
            dragChapterScroll(lp);
            return 0;
        }
        if (picking != Nil) {
            pickAt(picking, lp);
            return 0;
        }
        if (resizeEdge != HTNOWHERE) {
            resizeDrag(screenPoint(lp));
            return 0;
        }
        if (dragging) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
            moveDrag(screenPoint(lp));
            return 0;
        }
        pointerEntered(false);
        int id = hit(lp);
        SetCursor(LoadCursorW(nullptr, id == BubbleMove ? IDC_SIZEALL
                                        : id && id != ChapterScroll ? IDC_HAND : IDC_ARROW));
        if (id != hover) {
            hover = id;
            invalid();
        }
        return 0;
    }
    case WM_NCMOUSELEAVE:
    case WM_MOUSELEAVE: {
        // A client/non-client boundary crossing is not a departure from the window.
        POINT p{};
        if (focus && GetCursorPos(&p) && WindowFromPoint(p) == hwnd)
            return 0;
        if (focus && !dragging && resizeEdge == HTNOWHERE && !choosingColor)
            clearBubbles();
        else {
            hover = Nil;
            invalid();
        }
        return 0;
    }
    case WM_CANCELMODE:
    case WM_CAPTURECHANGED:
        if (picking != Nil) {
            picking = Nil;
            if (GetCapture() == hwnd)
                ReleaseCapture();
        }
        if (scrollingChapters || dragging || resizeEdge != HTNOWHERE) {
            stopDrag();
            clearBubbles();
        }
        break;
    case WM_KILLFOCUS:
        if (scrollingChapters) {
            stopDrag();
            invalid();
        }
        capturedVK = 0;
        if (recording != Slot::Count) {
            endRecording();
            invalid();
        }
        break;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) {
            endRecording();
            capturedVK = 0;
            invalid();
        }
        if (LOWORD(wp) == WA_INACTIVE && focus && !choosingColor) {
            stopDrag();
            clearBubbles();
        }
        break;
    case WM_SHOWWINDOW:
        if (!wp) {
            if (scrollingChapters)
                stopDrag();
            endRecording();
            capturedVK = 0;
        }
        if (!wp && picking != Nil) {
            picking = Nil;
            if (GetCapture() == hwnd)
                ReleaseCapture();
        }
        if (!wp && focus && !choosingColor) {
            stopDrag();
            clearBubbles();
        }
        break;
    case WM_MOUSEWHEEL:
        if (choosingColor)
            return 0;
        wheel += GET_WHEEL_DELTA_WPARAM(wp);
        if (abs(wheel) >= WHEEL_DELTA) {
            int dir = wheel > 0 ? -1 : 1;
            wheel = 0;
            if (keysVisible())
                scrollKeys(dir * std::min(3, keyRows));
            else if (screen == Reading)
                action(dir == 1 ? Next : Back);
            else
                scrollList(dir * 3);
        }
        return 0;
    case WM_DROPFILES: {
        HDROP d = (HDROP)wp;
        UINT len = DragQueryFileW(d, 0, nullptr, 0);
        Str p(len + 1, 0);
        DragQueryFileW(d, 0, p.data(), len + 1);
        DragFinish(d);
        p.resize(len);
        beginFile(p);
        return 0;
    }
    case WM_TIMER:
        if (wp == 1)
            complete();
        else if (wp == 2) {
            KillTimer(hwnd, 2);
            save();
        } else if (wp == 3) {
            KillTimer(hwnd, 3);
            notice.reset();
            invalid();
        }
        return 0;
    case WM_DESTROY:
        tray.remove(hwnd);
        if (hot)
            unregisterKey(hwnd, hotId);
        save();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, m, wp, lp);
}

LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto a = reinterpret_cast<Window *>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (m == WM_NCCREATE) {
        a = static_cast<Window *>(((CREATESTRUCTW *)l)->lpCreateParams);
        a->hwnd = h;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)a);
    }
    if (!a)
        return DefWindowProcW(h, m, w, l);
    try {
        return a->msg(m, w, l);
    } catch (...) {
        MessageBoxW(h, a->tr(Msg::Fatal), a->tr(Msg::Brand), MB_ICONERROR);
        PostQuitMessage(1);
        return 0;
    }
}

} // namespace leaf
