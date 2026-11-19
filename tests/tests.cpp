// SPDX-License-Identifier: GPL-3.0-or-later
// Integration and release-mode timing harness. Never uses the production profile.
#include "ui/window.hpp"
#include <windowsx.h>
#include <chrono>
#include <iostream>
#include <iomanip>

namespace {
using namespace leaf;
using Clock = std::chrono::steady_clock;
double ms(Clock::time_point t) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}
void expect(bool b, const char *m) {
    if (!b)
        throw std::runtime_error(m);
}
void put(const fs::path &p, const std::string &v) {
    std::ofstream f(p, std::ios::binary);
    f << v;
    expect(bool(f), "fixture write");
}
template <class F> void reject(F f) {
    try {
        f();
    } catch (const Fail &) {
        return;
    }
    throw std::runtime_error("malformed input accepted");
}
struct Temp {
    fs::path p;
    Temp() {
        p = fs::temp_directory_path() / (L"leaf-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                                         std::to_wstring(GetTickCount64()));
        if (!fs::create_directory(p))
            throw std::runtime_error("temp exists");
    }
    ~Temp() {
        std::error_code ec;
        fs::remove_all(p, ec);
    }
};
void run(bool coreOnly) {
    ComScope com;
    Temp temp;
    auto t = temp.p;
    for (auto pair : std::vector<std::pair<const wchar_t *, size_t>>{
             {L"sample.epub", 2}, {L"sample.docx", 1}, {L"sample.fb2", 1}}) {
        auto b = openBook(fs::path(L"tests/fixtures") / pair.first);
        expect(b.toc.size() == pair.second, "XML chapter count");
        expect(b.text.find(L"hidden head") == Str::npos, "excluded metadata");
        if (pair.second == 2)
            expect(b.toc[0].name == L"第一章 春水" && b.toc[1].name == L"第二章 山月", "EPUB spine order");
    }
    put(t / L"sample.md", u8"# 书名\r\n\r\n## 章节\r正文");
    auto md = openBook(t / L"sample.md");
    expect(md.toc.size() == 2 && md.text == L"# 书名\n\n## 章节\n正文", "MD index/newlines");
    put(t / L"gb.txt", std::string("\xd6\xd0\xce\xc4", 4));
    expect(openBook(t / L"gb.txt").text == L"中文", "GB18030");
    put(t / L"utf16.txt", std::string("\xfe\xff\x5c\x0f\x8b\xf4", 6));
    expect(openBook(t / L"utf16.txt").text == L"小说", "UTF16-BE");
    put(t / L"utf16.txt", std::string("\xff\xfe\x0f\x5c\xf4\x8b", 6));
    expect(openBook(t / L"utf16.txt").text == L"小说", "UTF16-LE");
    put(t / L"empty.txt", "");
    reject([&] { openBook(t / L"empty.txt"); });
    put(t / L"bad.epub", "bad zip");
    reject([&] { openBook(t / L"bad.epub"); });
    put(t / L"bad.fb2", "<!DOCTYPE body [<!ENTITY x 'abc'>]><body>&x;</body>");
    reject([&] { openBook(t / L"bad.fb2"); });
    std::cout << "PASS: five formats, Chinese encodings, EPUB spine, malformed/empty input\n";
    ComPtr<IDWriteFactory> dw;
    check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                              (IUnknown **)dw.GetAddressOf()));
    Book b;
    for (int i = 0; i < 430; ++i)
        b.text += L"汉字 mixed words 👋🏽，标点。0123456789! ";
    b.text += L"\n\n最后的段落\n";
    b.index();
    for (size_t i = 1; i < b.blocks.size(); ++i)
        expect(!low(b.text[b.blocks[i - 1]]) && b.blocks[i] > b.blocks[i - 1], "surrogate split");
    Pages pg;
    auto noop = [](auto...) {};
    for (float width : {160.f, 460.f, 720.f}) {
        pg.configure(&b, dw.Get(), width, 20);
        Pos p = 0;
        while (p < b.text.size()) {
            auto e = pg.visit(p, 10, noop);
            expect(e > p && e <= b.text.size(), "page gap or loop");
            if (e < b.text.size())
                expect(pg.previous(e, 10) == p, "backward roundtrip");
            p = e;
        }
    }
    std::cout << "PASS: bounded blocks, page coverage and exact back/forward across 3 widths\n";
    if (coreOnly)
        return;
    // Generate Chinese prose with a deterministic heading every 100 paragraphs.
    auto big = t / L"large.txt";
    std::string paragraph = u8"窗外微雨落在石阶上，河面漾起一层细纹。她合上信，听了一会儿远处的钟声。";
    std::ofstream out(big, std::ios::binary);
    for (int i = 0; i < 180000; ++i) {
        if (i % 100 == 0)
            out << u8"第" << i / 100 + 1 << u8"章 山月\n";
        out << paragraph << "\n";
    }
    out.close();
    Window a;
    a.state.file = t / L"profile" / L"state.bin";
    WNDCLASSW wc{};
    wc.lpfnWndProc = proc;
    wc.lpszClassName = L"Leaf.Test";
    wc.hInstance = GetModuleHandleW(nullptr);
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName, L"鱼读 · 测试",
                                WS_OVERLAPPEDWINDOW, 0, 0, 800, 640, nullptr, nullptr, wc.hInstance, &a);
    expect(hwnd != nullptr, "window creation");
    a.invalid();
    UpdateWindow(hwnd); // Never takes focus or registers a global hotkey.
    auto t0 = Clock::now();
    a.beginFile(big);
    expect(a.loading, "asynchronous open");
    while (a.loading && ms(t0) < 30000) {
        a.complete();
        Sleep(1);
    }
    double importMs = ms(t0);
    expect(a.book && a.book->toc.size() == 1800 && a.screen == Reading, "imported large book in window");
    // Hidden surfaces may be skipped by the compositor. Time CPU-side page shaping
    // separately from WM_PAINT and do not interpret it as on-screen frame latency.
    a.w = 768;
    a.h = 640;
    a.margin = 32;
    a.layout();
    auto step = pg.visit(0, 10, noop);
    expect(step > 0, "engine initialized");
    auto &large = *a.book;
    Pages speed;
    speed.configure(&large, dw.Get(), 660, 20);
    std::vector<double> samples;
    Pos p = 0;
    for (int i = 0; i < 1000; ++i) {
        auto now = Clock::now();
        auto q = speed.visit(p, 14, noop);
        samples.push_back(ms(now));
        expect(q > p, "sequential turn");
        p = q;
    }
    std::sort(samples.begin(), samples.end());
    a.action(Next);
    auto saved = a.pos;
    expect(saved > 0, "UI page turn");
    a.action(Back);
    expect(a.pos == 0, "UI back");
    a.action(Next);
    expect(a.pos == saved, "UI next");
    a.action(AddMark);
    expect(a.state.recent.front().marks.size() == 1, "bookmark");
    a.action(Contents);
    a.action(Item + 900);
    expect(a.pos == large.toc[900].pos, "chapter jump");
    a.action(Marks);
    a.action(Item);
    expect(a.pos == saved, "bookmark jump");
    a.action(Style);
    a.action(Theme2);
    a.action(Bigger);
    auto styleBefore = GetWindowLongPtrW(hwnd, GWL_STYLE);
    RECT initial{};
    GetWindowRect(hwnd, &initial);
    a.key('Q', MOD_CONTROL);
    expect(!a.focus, "Ctrl+Q is not the focus shortcut");
    a.key(VK_OEM_7, MOD_CONTROL);
    a.key(VK_OEM_7, MOD_CONTROL, true);
    expect(a.focus && a.screen == Reading && !a.bubbles &&
               !(GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_OVERLAPPEDWINDOW),
           "Ctrl+apostrophe enters borderless focus once per press");
    auto draw = [&] {
        InvalidateRect(hwnd, nullptr, FALSE);
        SendMessageW(hwnd, WM_PAINT, 0, 0);
    };
    auto center = [&](int id) -> LPARAM {
        for (auto t : a.hits)
            if (t.id == id)
                return MAKELPARAM(int((t.box.left + t.box.right) * .5f * a.scale),
                                  int((t.box.top + t.box.bottom) * .5f * a.scale));
        throw std::runtime_error("missing focus control");
    };
    draw();
    expect(a.hits.empty(), "clean reading surface");
    SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(20, 50));
    expect(a.bubbles && !a.options, "first move reveals bubbles without dwell or timer");
    draw();
    expect(a.hits.size() == 2, "exactly two entry bubbles");
    SendMessageW(hwnd, WM_LBUTTONUP, 0, center(BubbleSettings));
    draw();
    expect(a.options && a.focus && a.screen == Reading, "settings popover stays in focus");
    int oldFont = a.state.font;
    SendMessageW(hwnd, WM_LBUTTONUP, 0, center(Smaller));
    draw();
    expect(a.state.font == oldFont - 2 && a.options, "inline setting changed");
    SendMessageW(hwnd, WM_LBUTTONUP, 0, center(Bigger));
    draw();
    SendMessageW(hwnd, WM_LBUTTONUP, 0, center(BubbleSettings));
    draw();
    LPARAM grip = center(BubbleMove);
    SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, grip);
    expect(a.dragging && GetCapture() == hwnd, "move bubble captures drag");
    SendMessageW(hwnd, WM_MOUSELEAVE, 0, 0);
    expect(a.bubbles && a.dragging, "drag survives pointer leave");
    SendMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON,
                 MAKELPARAM(GET_X_LPARAM(grip) + 48, GET_Y_LPARAM(grip) + 25));
    RECT moved{};
    GetWindowRect(hwnd, &moved);
    expect(moved.left == initial.left + 48 && moved.top == initial.top + 25, "actual window moved");
    SendMessageW(hwnd, WM_LBUTTONUP, 0, grip);
    SendMessageW(hwnd, WM_MOUSELEAVE, 0, 0);
    expect(!a.dragging && !a.bubbles && !a.options && a.hits.empty(), "popover and hit targets cleared");
    // Exercise global-hotkey delivery without claiming the real user's shortcut.
    a.hot = true;
    ShowWindow(hwnd, SW_SHOWNA);
    SendMessageW(hwnd, WM_HOTKEY, 1, MAKELPARAM(MOD_CONTROL, 'Q'));
    expect(!IsWindowVisible(hwnd) && a.focus && a.pos == saved, "boss hides a focused reader");
    SendMessageW(hwnd, WM_HOTKEY, 1, MAKELPARAM(MOD_CONTROL, 'Q'));
    expect(IsWindowVisible(hwnd) && a.focus && a.pos == saved, "boss restores same mode and anchor");
    expect(!a.hiddenToTray && !a.tray.installed, "restoring removes the tray icon");
    a.hot = false; // Simulate Ctrl+Q being owned by another application.
    a.action(Hide);
    expect(a.hiddenToTray && a.tray.installed && !IsWindowVisible(hwnd),
           "manual hide adds an actual shell icon without a global hotkey");
    auto n = ReaderTray::identity(hwnd);
    expect(Shell_NotifyIconW(NIM_DELETE, &n) != FALSE, "shell confirms icon existed");
    SendMessageW(hwnd, a.taskbarCreated, 0, 0);
    expect(a.hiddenToTray && a.tray.installed, "hidden icon recreated after shell reset");
    SendMessageW(hwnd, ReaderTray::message, ReaderTray::id,
                 a.tray.version4 ? MAKELPARAM(NIN_SELECT, ReaderTray::id) : WM_LBUTTONUP);
    expect(IsWindowVisible(hwnd) && a.focus && a.pos == saved && !a.tray.installed,
           "tray click restores reader without shortcut");
    ShowWindow(hwnd, SW_HIDE);
    a.key(VK_OEM_7, MOD_CONTROL);
    RECT restoredRect{};
    GetWindowRect(hwnd, &restoredRect);
    expect(!a.focus && GetWindowLongPtrW(hwnd, GWL_STYLE) == styleBefore && a.pos == saved &&
               EqualRect(&restoredRect, &moved),
           "Ctrl+apostrophe restores original chrome at moved position");
    a.key(VK_OEM_7, MOD_CONTROL);
    a.key(VK_ESCAPE, 0);
    expect(!a.focus && GetWindowLongPtrW(hwnd, GWL_STYLE) == styleBefore, "Esc restores frame");
    a.key(VK_OEM_7, MOD_CONTROL);
    // Drag the frameless lower-right corner far beyond its minimum.
    GetWindowRect(hwnd, &initial);
    auto corner = MAKELPARAM(initial.right - 1, initial.bottom - 1);
    expect(SendMessageW(hwnd, WM_NCHITTEST, 0, corner) == HTBOTTOMRIGHT, "borderless edge hit");
    SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTBOTTOMRIGHT, corner);
    SendMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(90, 55));
    SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(90, 55));
    RECT tiny{};
    GetClientRect(hwnd, &tiny);
    expect(tiny.right == a.minOuter(true).cx && tiny.bottom == a.minOuter(true).cy,
           "focus resize clamped at reduced minimum");
    a.clearBubbles();
    SendMessageW(hwnd, WM_NCMOUSEMOVE, HTLEFT, MAKELPARAM(initial.left + 1, initial.top + 30));
    expect(a.bubbles, "entry through a resize edge shows bubbles");
    draw();
    SendMessageW(hwnd, WM_LBUTTONUP, 0, center(BubbleSettings));
    draw();
    int prevSpacing = a.state.spacing;
    float prevLine = a.pages.line;
    SendMessageW(hwnd, WM_LBUTTONUP, 0, center(Looser));
    draw();
    expect(a.state.spacing == prevSpacing + 1 && a.pages.line > prevLine && a.pos == saved,
           "line leading updates without losing anchor");
    auto visibleTargets = [&] {
        for (auto &t : a.hits)
            expect(t.box.left >= 0 && t.box.top >= 0 && t.box.right <= a.w && t.box.bottom <= a.h,
                   "clipped small-window button");
    };
    visibleTargets();
    auto click = [&](int id) {
        SendMessageW(hwnd, WM_LBUTTONUP, 0, center(id));
        draw();
    };
    click(TabKeys);
    auto keyRow = [](Slot slot) { return KeyRow + int(slot); };
    // Even the minimum borderless reader can reach every binding without turning a book page.
    for (int i = 0; i < int(Slot::Count); i++) {
        while (i >= a.keyFirst + a.keyRows)
            click(KeysLater);
        click(keyRow(Slot(i)));
        visibleTargets();
        expect(a.recording == Slot(i) && a.focus && a.options && a.pos == saved,
               "inline row arms in tiny focus window");
        SendMessageW(hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
        draw();
        expect(a.recording == Slot::Count && a.focus && a.state.keys == defaultKeys,
               "Escape cancels only capture");
    }
    SendMessageW(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), 0);
    draw();
    expect(a.keyFirst < int(Slot::Count) - a.keyRows && a.pos == saved, "wheel scrolls key list only");
    click(keyRow(Slot(a.keyFirst)));
    click(TabColor);
    expect(a.recording == Slot::Count, "switching tab cancels capture");
    draw();
    visibleTargets();
    SendMessageW(hwnd, WM_LBUTTONUP, 0, center(ChooseBg));
    draw();
    visibleTargets();
    expect(a.choosingColor && a.focus, "in-window picker in smallest reader");
    auto original = a.state.background;
    SendMessageW(hwnd, WM_LBUTTONUP, 0, center(PickSwatch + 1));
    draw();
    expect(a.canvasColor() == paperSwatches[1] && a.state.background == original,
           "live preview without committing");
    a.key(VK_RIGHT, 0);
    expect(a.pos == saved, "picker does not turn page");
    a.key(VK_ESCAPE, 0);
    expect(!a.choosingColor && a.state.background == original && a.focus && a.options,
           "Esc cancels only picker");
    draw();
    SendMessageW(hwnd, WM_LBUTTONUP, 0, center(ChooseBg));
    draw();
    SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, center(PickHue));
    SendMessageW(hwnd, WM_LBUTTONUP, 0, center(PickHue));
    expect(a.picking == Nil && GetCapture() != hwnd, "hue drag released");
    SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, center(PickField));
    auto whitePoint = MAKELPARAM(int((a.field.left - 8) * a.scale), int((a.field.top - 8) * a.scale));
    SendMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON, whitePoint);
    SendMessageW(hwnd, WM_LBUTTONUP, 0, whitePoint);
    expect(a.draft.rgb() == 0xffffff && a.state.background == original, "clamped SV drag");
    SendMessageW(hwnd, WM_LBUTTONUP, 0, center(PickApply));
    draw();
    expect(!a.choosingColor && a.state.background == 0xffffff, "applied custom color");
    std::cout << "PASS: immediate bubbles, in-window swatches, HSV drag, cancel/apply at minimum size\n";
    a.setBackground(0x000000);
    expect(a.canvasColor() == 0 && a.canvasInk(0x050505) == 0xffffff, "dark background contrast");
    a.setBackground(0xffffff);
    expect(a.canvasInk(0xfefefe) == 0, "light background contrast");
    a.action(ResetBg);
    expect(a.canvasColor() == a.c.bg, "default background");
    a.setBackground(0x345678);
    a.action(TabWindow);
    draw();
    visibleTargets();
    auto checkTitle = [&](const wchar_t *brand) {
        wchar_t title[256]{};
        GetWindowTextW(hwnd, title, 256);
        expect(Str(title) == large.file.stem().wstring() + L"  ·  " + brand,
               "localized window title preserves book name");
    };
    const auto heading = large.toc.front().name;
    expect(a.state.language == Language::Chinese, "default language");
    auto switchLang = [&] {
        SendMessageW(hwnd, WM_LBUTTONUP, 0, center(ToggleLanguage));
        draw();
        visibleTargets();
    };
    switchLang();
    checkTitle(L"FishReader");
    expect(View(a.tr(Msg::TrayHide)) == L"Hide to tray" &&
               View(bookFilter(a.state.language)) == L"Supported books",
           "English UI and dialog text");
    expect(a.focus && a.options && a.pos == saved && large.toc.front().name == heading,
           "language does not change reader or text");
    switchLang();
    checkTitle(L"鱼读");
    expect(View(a.tr(Msg::TrayHide)) == L"隐藏到托盘", "switch back to Chinese");
    a.action(Contents);
    GetClientRect(hwnd, &tiny);
    draw();
    visibleTargets();
    expect(tiny.right >= int(256 * a.scale) - 1 && tiny.bottom >= int(216 * a.scale) - 1,
           "small regular client restored");
    a.action(Style);
    draw();
    visibleTargets();
    switchLang();
    checkTitle(L"FishReader"); // Regular settings use the same saved preference.
    a.action(Contents);
    expect(!a.focus && a.screen == Chapters && GetWindowLongPtrW(hwnd, GWL_STYLE) == styleBefore,
           "navigation restores frame");
    std::cout
        << "PASS: Ctrl+apostrophe focus, Ctrl+Q boss key, shell tray add/recreate/restore, capture drag "
           "and restore\n";
    a.action(Home);
    expect(a.screen == Shelf, "library");
    a.action(Item);
    expect(a.screen == Reading, "resume same book");
    a.save();
    State restored;
    restored.file = a.state.file;
    restored.load();
    expect(restored.recent.size() == 1 && restored.recent[0].cursor == saved &&
               restored.recent[0].marks.size() == 1 && restored.font == 22 && restored.theme == 2 &&
               restored.background == 0x345678 && restored.spacing == 4,
           "profile roundtrip");
    expect(restored.language == Language::English, "saved language roundtrip");
    expect(validMap(defaultKeys) && !allowed(Slot::Boss, 'Q') &&
               !allowed(Slot::Open, chord(MOD_ALT, VK_F4)),
           "reserved/unsafe combinations");
    expect(a.applyShortcut(Slot::Settings, defaultKeys[index(Slot::Boss)]) == Msg::DupBind,
           "duplicate binding rejected");
    // Deterministic Windows registration double; never takes over the user's global keys.
    static std::map<int, leaf::Chord> global;
    const leaf::Chord blocked = chord(MOD_ALT | MOD_SHIFT, 'Z');
    a.registerKey = [](HWND, int id, UINT mods, UINT vk) -> BOOL {
        auto v = chord(mods & ~MOD_NOREPEAT, vk);
        if (v == chord(MOD_ALT | MOD_SHIFT, 'Z') || global.count(id))
            return FALSE;
        for (auto x : global)
            if (x.second == v)
                return FALSE;
        global[id] = v;
        return TRUE;
    };
    a.unregisterKey = [](HWND, int id) -> BOOL { return global.erase(id) != 0; };
    a.hot = true;
    global[a.hotId] = defaultKeys[0];
    SetWindowPos(hwnd, nullptr, 0, 0, int(768 * a.scale), int(724 * a.scale),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    a.action(Style);
    draw();
    click(TabKeys);
    expect(a.keyRows == int(Slot::Count), "single list shows all 11 actions at standard size");
    for (int i = 0; i < int(Slot::Count); i++)
        (void)center(KeyRow + i);
    click(keyRow(Slot::Focus));
    SendMessageW(hwnd, WM_KEYDOWN, VK_CONTROL, 0);
    expect(a.recording == Slot::Focus && a.state.keys == defaultKeys, "modifier alone waits for main key");
    SendMessageW(hwnd, WM_HOTKEY, a.hotId, MAKELPARAM(MOD_CONTROL, 'Q'));
    expect(a.recording == Slot::Focus && a.conflictOwner == Slot::Boss && !a.hiddenToTray &&
               a.state.keys == defaultKeys,
           "existing global binding gives inline conflict, not hide");
    a.key(VK_RIGHT, 0);
    expect(a.keyHint == Msg::InvalidBind && a.recording == Slot::Focus && a.pos == saved,
           "navigation rejected without dispatch");
    a.key('Z', MOD_ALT | MOD_SHIFT);
    expect(a.keyHint == Msg::OsBusy && a.state.keys == defaultKeys, "OS conflict is retryable inline");
    SendMessageW(hwnd, WM_KEYDOWN, 'X', 0);
    draw();
    expect(a.state.keys[index(Slot::Focus)] == 'X' && a.recording == Slot::Count && a.screen == Settings,
           "one press commits without activating binding");
    State inlineState;
    inlineState.file = a.state.file;
    inlineState.load();
    expect(inlineState.keys == a.state.keys, "inline change saved immediately");
    SendMessageW(hwnd, WM_KEYDOWN, 'X', 1L << 30);
    expect(!a.focus, "held just-recorded key does not run action");
    SendMessageW(hwnd, WM_KEYUP, 'X', 0);
    click(keyRow(Slot::Focus));
    SendMessageW(hwnd, WM_KEYDOWN, VK_DELETE, 0);
    draw();
    expect(!a.state.keys[index(Slot::Focus)], "Delete clears only selected row");
    click(keyRow(Slot::Focus));
    click(KeyDefault);
    expect(a.state.keys == defaultKeys, "per-row restore is immediate");
    click(keyRow(Slot::Focus));
    SendMessageW(hwnd, WM_ACTIVATE, WA_INACTIVE, 0);
    expect(a.recording == Slot::Count, "deactivation cancels capture");
    click(keyRow(Slot::Boss));
    a.key('H', 0);
    expect(a.keyHint == Msg::BossModifiers && a.state.keys == defaultKeys, "global binding needs modifier");
    a.key('H', MOD_ALT);
    draw();
    expect(a.state.keys[0] == chord(MOD_ALT, 'H') && global[a.hotId] == a.state.keys[0] && !a.hiddenToTray,
           "global binding also edits inline");
    click(keyRow(Slot::Boss));
    click(KeyDefault);
    a.action(Read);
    draw();
    std::cout << "PASS: inline shortcut list in both layouts, click/press, conflict retry, cancel, reset "
                 "and immediate persistence\n";
    expect(a.applyShortcut(Slot::Boss, blocked) == Msg::OsBusy && global.size() == 1 &&
               global[a.hotId] == defaultKeys[0],
           "OS conflict keeps existing registration");
    auto profile = a.state.file;
    a.state.file = t; // Cannot replace an existing directory with a config file.
    expect(a.applyShortcut(Slot::Boss, chord(MOD_CONTROL | MOD_SHIFT, 'Y')) == Msg::SaveFailed &&
               global.size() == 1 && a.state.keys[0] == defaultKeys[0],
           "failed save rolls back tentative registration");
    a.state.file = profile;
    auto oldId = a.hotId;
    expect(a.applyShortcut(Slot::Boss, chord(MOD_CONTROL | MOD_SHIFT, 'Y')) == Msg::Count &&
               global.size() == 1 && a.hotId != oldId,
           "replace global hotkey atomically");
    expect(a.applyShortcut(Slot::Focus, chord(MOD_CONTROL | MOD_SHIFT, 'E')) == Msg::Count,
           "remap local focus");
    a.key(VK_OEM_7, MOD_CONTROL);
    a.key('E', MOD_CONTROL);
    expect(!a.focus, "old binding and partial modifiers ignored");
    a.key('E', MOD_CONTROL | MOD_SHIFT);
    expect(a.focus, "new local binding dispatch");
    a.key(VK_ESCAPE, 0);
    expect(a.applyShortcut(Slot::Boss, 0) == Msg::Count && !a.hot && global.empty(),
           "boss key can be disabled");
    State custom;
    custom.file = profile;
    custom.load();
    expect(custom.keys == a.state.keys &&
               custom.keys[index(Slot::Focus)] == chord(MOD_CONTROL | MOD_SHIFT, 'E'),
           "keymap survives restart");
    expect(a.applyShortcut(Slot::Focus, defaultKeys[index(Slot::Focus)]) == Msg::Count,
           "restore one default binding");
    std::cout << "PASS: exact shortcut dispatch, duplicate/system conflicts, rollback, disable, restore, "
                 "persistence\n";
    fs::copy_file(restored.file, t / L"pre-keys.bin");
    fs::resize_file(t / L"pre-keys.bin", fs::file_size(t / L"pre-keys.bin") - 4 * (2 + defaultKeys.size()));
    State beforeKeys;
    beforeKeys.file = t / L"pre-keys.bin";
    beforeKeys.load();
    expect(beforeKeys.keys == defaultKeys && beforeKeys.language == Language::English &&
               beforeKeys.recent.front().cursor == saved,
           "pre-keymap profile loads defaults");
    fs::copy_file(beforeKeys.file, t / L"previous.bin");
    fs::resize_file(t / L"previous.bin", fs::file_size(t / L"previous.bin") - 8);
    State previous;
    previous.file = t / L"previous.bin";
    previous.load();
    expect(previous.language == Language::Chinese && previous.background == 0x345678 &&
               previous.spacing == 4 && previous.recent.front().cursor == saved,
           "pre-language profile with appearance preserved");
    fs::copy_file(previous.file, t / L"v1.bin");
    fs::resize_file(t / L"v1.bin", fs::file_size(t / L"v1.bin") - 12);
    State legacy;
    legacy.file = t / L"v1.bin";
    legacy.load();
    expect(legacy.recent.size() == 1 && legacy.recent[0].cursor == saved && legacy.spacing == 3 &&
               legacy.background == State::noBackground,
           "legacy profile preserved");
    std::cout << "PASS: mini window resize, appearance controls, contrast, spacing, v1 migration and boss "
                 "visibility\n";
    a.beginFile(t / L"bad.epub");
    while (a.loading) {
        a.complete();
        Sleep(1);
    }
    expect(a.book.get() == &large && a.pos == saved, "failed import preserves book");
    expect(a.notice && a.notice->message(a.state.language) == L"This is not a valid book archive.",
           "translated background import failure");
    std::cout << "PASS: live Chinese/English in both layouts, localized failure and persisted language\n";
    std::cout << "PASS: real HWND, async import, book/chapter/mark navigation, themes, font, restore, "
                 "failed import\n";
    std::cout << std::fixed << std::setprecision(3)
              << "BENCH file_MiB=" << double(fs::file_size(big)) / (1 << 20)
              << " chars_UTF16=" << large.text.size() << " chapters=" << large.toc.size()
              << " async_import_ms=" << importMs << " turns=1000 page_layout_ms p50=" << samples[500]
              << " p95=" << samples[950] << " p99=" << samples[990] << " max=" << samples.back() << "\n";
    DestroyWindow(hwnd);
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    try {
        run(argc == 2 && std::wstring_view(argv[1]) == L"--core");
        return 0;
    } catch (const leaf::Fail &e) {
        std::wcerr << L"FAIL: " << e.message(leaf::Language::English) << L"\n";
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << "\n";
    }
    return 1;
}
