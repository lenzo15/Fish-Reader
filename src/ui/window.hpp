// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "reader/pages.hpp"
#include "platform/state.hpp"
#include "platform/tray.hpp"
#include "ui/color_picker.hpp"
#include <future>
#include <map>
#include <tuple>
#include <optional>
namespace leaf {
using Box = D2D1_RECT_F;
struct Colors {
    DWORD bg, card, ink, muted, accent, tint, rule, white;
};
inline constexpr Colors palettes[] = {
    {0x263341, 0x324353, 0xe8eff5, 0xbccbd8, 0xa9c5dc, 0x3b5063, 0x51677b, 0x182734},
    {0xeee8da, 0xfffaec, 0x392f23, 0x75624c, 0x825a33, 0xf2e2c7, 0xdfd1bb, 0xfffbef},
    {0x151e28, 0x202c39, 0xe0e8ef, 0xacbbc9, 0x9eb9d0, 0x2c3c4e, 0x3e5061, 0x17232f}};
enum Action {
    Nil,
    Home,
    Open,
    Read,
    Contents,
    Marks,
    Style,
    Focus,
    BubbleSettings,
    BubbleMove,
    Back,
    Next,
    AddMark,
    Smaller,
    Bigger,
    Tighter,
    Looser,
    TabType,
    TabColor,
    TabWindow,
    TabKeys,
    KeysEarlier,
    KeysLater,
    KeyDefault,
    ToggleLanguage,
    ChooseBg,
    ResetBg,
    Theme0,
    Theme1,
    Theme2,
    Pin,
    Translucent,
    Hide,
    Up,
    Down,
    PickField,
    PickHue,
    PickCancel,
    PickApply,
    PickSwatch,
    KeyRow = 70,
    Item = 100
};
static_assert(KeyRow + int(Slot::Count) <= Item);
enum Screen { Shelf, Reading, Chapters, Bookmarks, Settings };
struct Target {
    int id;
    Box box;
};
struct Window {
    HWND hwnd = nullptr;
    float scale = 1, w = 0, h = 0, margin = 28, bodyTop = 0, bodyBottom = 0;
    ComPtr<ID2D1Factory> d2;
    ComPtr<ID2D1PathGeometry> fishMark;
    ComPtr<IDWriteFactory> dw;
    ComPtr<ID2D1HwndRenderTarget> rt;
    ComPtr<ID2D1SolidColorBrush> brush;
    std::map<std::tuple<int, bool, bool>, ComPtr<IDWriteTextFormat>> fonts;
    Colors c = palettes[0];
    State state;
    bool dirty = false, focus = false, glass = false, hot = false, loading = false;
    bool keyboard = false, bubbles = false, options = false, dragging = false;
    int hotId = 1;
    decltype(&RegisterHotKey) registerKey = RegisterHotKey;
    decltype(&UnregisterHotKey) unregisterKey = UnregisterHotKey;
    int resizeEdge = HTNOWHERE, appearanceTab = 0;
    Slot recording = Slot::Count, conflictOwner = Slot::Count;
    Msg keyHint = Msg::KeyHelp;
    int keyFirst = 0, keyRows = 1;
    UINT capturedVK = 0;
    bool choosingColor = false;
    ColorDraft draft;
    PickerBrushes pickerBrushes;
    int picking = Nil;
    Box field{}, hueBar{};
    ReaderTray tray;
    bool hiddenToTray = false;
    UINT taskbarCreated = 0;
    std::vector<HWND> hiddenDialogs;
    POINT dragStart{};
    RECT dragRect{}, zoomRect{};
    bool enteredZoomed = false;
    LONG_PTR savedFrame = 0;
    COLORREF savedBorder = 0xffffffff;
    int selected = -1, hover = Nil, wheel = 0;
    std::vector<Target> hits;
    std::unique_ptr<Book> book;
    Pages pages;
    Pos pos = 0, end = 0;
    unsigned rows = 1;
    Screen screen = Shelf;
    int first = 0, listRows = 1;
    std::future<Book> job;
    std::optional<Fail> notice;
    Str loadName;
    const wchar_t *tr(Msg msg) const;
    void updateTitle();
    void invalid();
    void note(Msg id);
    void note(Fail s);
    void changed();
    void save();
    void stopDrag();
    void clearBubbles();
    SIZE minOuter(bool inFocus) const;
    void setFocusMode(bool on);
    void pageScreen(Screen s);
    void theme();
    void init();
    void beginFile(fs::path path);
    void complete();
    void picker();
    void ensure();
    void color(DWORD col, float alpha = 1);
    void panel(Box b, float rad, DWORD col);
    void border(Box b, float rad, DWORD col, float stroke = 1);
    void line(float x, float y, float x2, float y2, DWORD col, float stroke = 1);
    void label(View text, Box b, int pts, DWORD col, bool bold = false, bool center = false);
    void target(int id, Box b);
    void button(int id, Box b, View title, bool primary = false, bool active = false);
    void fishIcon(float x, float y, float s, DWORD col);
    bool compact() const;
    float textLeft() const;
    DWORD canvasColor() const;
    static double luminance(DWORD rgb);
    DWORD canvasInk(DWORD preferred) const;
    void setBackground(DWORD rgb);
    void pickBackground();
    void endColor(bool apply);
    void pickAt(int part, LPARAM xy);
    void colorCard();
    void header();
    void layout();
    void read();
    int count() const;
    void list();
    void appearanceControls(float l, float r, float y);
    void focusControls();
    void settings();
    void paint();
    void jump(Pos p);
    void action(int id);
    bool key(WPARAM k, UINT mods, bool repeat = false);
    bool keysVisible() const;
    void endRecording();
    void scrollKeys(int delta);
    void recordValue(Chord value);
    bool captureKey(UINT vk, UINT mods, bool repeat);
    void shortcutList(float left, float right, float y, float bottom);
    Msg applyShortcut(Slot slot, Chord value);
    Str bossStatus() const;
    void pointerEntered(bool nonClient);
    int hit(LPARAM p);
    POINT screenPoint(LPARAM lp) const;
    void moveDrag(POINT p);
    int edgeAt(POINT p) const;
    void resizeDrag(POINT p);
    LRESULT msg(UINT m, WPARAM wp, LPARAM lp);
};
LRESULT CALLBACK proc(HWND, UINT, WPARAM, LPARAM);
} // namespace leaf
