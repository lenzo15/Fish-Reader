// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/window.hpp"
#include "ui/fish_mark.hpp"
#include <windowsx.h>

namespace leaf {
using namespace D2D1;

void Window::ensure() {
    RECT r;
    GetClientRect(hwnd, &r);
    w = (r.right - r.left) / scale;
    h = (r.bottom - r.top) / scale;
    if (!rt) {
        auto props = RenderTargetProperties();
        props.dpiX = props.dpiY = scale * 96.f;
        auto wp = HwndRenderTargetProperties(hwnd, SizeU(r.right, r.bottom));
        check(d2->CreateHwndRenderTarget(props, wp, rt.GetAddressOf()));
        check(rt->CreateSolidColorBrush(ColorF(0u), brush.GetAddressOf()));
    }
}

void Window::color(DWORD col, float alpha) {
    brush->SetColor(ColorF(col, alpha));
}

void Window::panel(Box b, float rad, DWORD col) {
    color(col);
    rt->FillRoundedRectangle(RoundedRect(b, rad, rad), brush.Get());
}

void Window::border(Box b, float rad, DWORD col, float stroke) {
    color(col);
    rt->DrawRoundedRectangle(RoundedRect(b, rad, rad), brush.Get(), stroke);
}

void Window::line(float x, float y, float x2, float y2, DWORD col, float stroke) {
    color(col);
    rt->DrawLine(Point2F(x, y), Point2F(x2, y2), brush.Get(), stroke);
}

void Window::label(View text, Box b, int pts, DWORD col, bool bold, bool center) {
    if (b.right <= b.left || b.bottom <= b.top)
        return;
    auto &f = fonts[{pts, bold, center}];
    if (!f) {
        check(dw->CreateTextFormat(
            L"Microsoft YaHei UI", nullptr, bold ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, (float)pts, L"zh-CN", f.GetAddressOf()));
        f->SetTextAlignment(center ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_LEADING);
        f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
        ComPtr<IDWriteInlineObject> dots;
        if (SUCCEEDED(dw->CreateEllipsisTrimmingSign(f.Get(), dots.GetAddressOf())))
            f->SetTrimming(&trim, dots.Get());
    }
    color(col);
    rt->DrawText(text.data(), (UINT32)text.size(), f.Get(), b, brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void Window::target(int id, Box b) {
    hits.push_back({id, b});
}

void Window::button(int id, Box b, View title, bool primary, bool active) {
    DWORD fill = primary ? c.accent : (active || hover == id ? c.tint : c.card);
    panel(b, (b.bottom - b.top) / 2, fill);
    if (!primary && !active)
        border(b, (b.bottom - b.top) / 2, hover == id ? c.accent : c.rule);
    label(title, b, 14, primary ? c.white : (active ? c.accent : c.ink), true, true);
    target(id, b);
}

void Window::fishIcon(float x, float y, float s, DWORD col) {
    if (!fishMark) {
        check(d2->CreatePathGeometry(fishMark.GetAddressOf()));
        ComPtr<ID2D1GeometrySink> sink;
        check(fishMark->Open(sink.GetAddressOf()));
        fishContours(sink.Get());
        check(sink->Close());
    }
    D2D1_MATRIX_3X2_F old;
    rt->GetTransform(&old);
    rt->SetTransform(Matrix3x2F::Scale(s / 100.f, s / 100.f) * Matrix3x2F::Translation(x, y) * old);
    color(col);
    rt->FillGeometry(fishMark.Get(), brush.Get());
    rt->SetTransform(old);
}

void Window::settingsButton(Box b) {
    bool active = screen == Settings;
    button(Style, b, L"", false, active);
    if (!gearMark) {
        check(d2->CreatePathGeometry(gearMark.GetAddressOf()));
        ComPtr<ID2D1GeometrySink> sink;
        check(gearMark->Open(sink.GetAddressOf()));
        // Eight teeth, in device-independent units; no icon-font dependency.
        constexpr float pi = 3.14159265f;
        constexpr float angles[] = {-17.f, -13.f, 13.f, 17.f};
        for (int i = 0; i < 32; ++i) {
            int corner = i % 4;
            float angle = (i / 4 * 45.f + angles[corner]) * pi / 180.f;
            float radius = corner == 0 || corner == 3 ? 7.f : 9.f;
            auto p = Point2F(std::cos(angle) * radius, std::sin(angle) * radius);
            if (i == 0)
                sink->BeginFigure(p, D2D1_FIGURE_BEGIN_HOLLOW);
            else
                sink->AddLine(p);
        }
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        check(sink->Close());
    }
    D2D1_MATRIX_3X2_F old;
    rt->GetTransform(&old);
    rt->SetTransform(Matrix3x2F::Translation((b.left + b.right) / 2, (b.top + b.bottom) / 2) * old);
    color(active ? c.accent : c.ink);
    rt->DrawGeometry(gearMark.Get(), brush.Get(), 1.5f);
    rt->DrawEllipse(Ellipse(Point2F(0, 0), 3.f, 3.f), brush.Get(), 1.5f);
    rt->SetTransform(old);
}

bool Window::compact() const {
    return w < 398 || h < 448;
}

float Window::textLeft() const {
    return compact() ? 12.f : std::max(margin + 22, (w - 730) / 2);
}

DWORD Window::canvasColor() const {
    if (screen == Reading && choosingColor)
        return draft.rgb();
    return screen == Reading && state.background != State::noBackground ? state.background : c.bg;
}

double Window::luminance(DWORD rgb) {
    auto ch = [](DWORD v) {
        double s = (v & 255) / 255.;
        return s <= .04045 ? s / 12.92 : std::pow((s + .055) / 1.055, 2.4);
    };
    return .2126 * ch(rgb >> 16) + .7152 * ch(rgb >> 8) + .0722 * ch(rgb);
}

DWORD Window::canvasInk(DWORD preferred) const {
    if (screen != Reading || (!choosingColor && state.background == State::noBackground))
        return preferred;
    double bg = luminance(canvasColor()), fg = luminance(preferred);
    if ((std::max(bg, fg) + .05) / (std::min(bg, fg) + .05) >= 4.5)
        return preferred;
    return bg > .179 ? 0x000000 : 0xffffff;
}

void Window::setBackground(DWORD rgb) {
    state.background = rgb;
    changed();
    invalid();
}

void Window::pickBackground() {
    draft = {};
    draft.fromRgb(state.background == State::noBackground ? c.bg : state.background);
    choosingColor = true;
    hits.clear();
    hover = Nil;
    selected = -1;
    keyboard = false;
    invalid();
}

void Window::endColor(bool apply) {
    picking = Nil;
    if (GetCapture() == hwnd)
        ReleaseCapture();
    choosingColor = false;
    if (apply)
        setBackground(draft.rgb());
    if (focus)
        bubbles = options = true;
    hits.clear();
    hover = Nil;
    selected = -1;
    keyboard = false;
    invalid();
}

void Window::pickAt(int part, LPARAM xy) {
    float x = GET_X_LPARAM(xy) / scale, y = GET_Y_LPARAM(xy) / scale;
    if (part == PickHue)
        draft.hue = std::clamp((y - hueBar.top) / (hueBar.bottom - hueBar.top), 0.f, 1.f);
    else if (part == PickField) {
        draft.sat = std::clamp((x - field.left) / (field.right - field.left), 0.f, 1.f);
        draft.value = 1 - std::clamp((y - field.top) / (field.bottom - field.top), 0.f, 1.f);
    }
    invalid();
}

void Window::colorCard() {
    hits.clear(); // Underlying page controls must not receive picker input.
    color(0x101923, .27f);
    rt->FillRectangle({0, 0, w, h}, brush.Get());
    float cw = std::min(312.f, w - 12), ch = std::min(228.f, h - 12);
    float x = (w - cw) / 2, y = (h - ch) / 2, r = x + cw, b = y + ch, l = x + 12, right = r - 12;
    panel({x, y, r, b}, 16, c.card);
    border({x, y, r, b}, 16, c.rule);
    label(tr(Msg::Background), {l, y + 3, right - 87, y + 32}, 15, c.ink, true);
    DWORD rgb = draft.rgb();
    wchar_t hex[12];
    swprintf(hex, 12, L"#%06X", unsigned(rgb));
    Box preview = {right - 78, y + 5, right, y + 29};
    panel(preview, 10, rgb);
    border(preview, 10, c.rule);
    label(hex, preview, 12, luminance(rgb) > .179 ? 0 : 0xffffff, true, true);
    field = {l, y + 36, right - 34, b - 68};
    hueBar = {right - 23, field.top, right, field.bottom};
    pickerBrushes.prepare(rt.Get());
    auto gradient = [&](Box bounds, ID2D1LinearGradientBrush *g, bool horizontal, float radius) {
        g->SetStartPoint(Point2F(bounds.left, bounds.top));
        g->SetEndPoint(horizontal ? Point2F(bounds.right, bounds.top)
                                  : Point2F(bounds.left, bounds.bottom));
        rt->FillRoundedRectangle(RoundedRect(bounds, radius, radius), g);
    };
    panel(field, 8, ColorDraft::rgb(draft.hue, 1, 1));
    gradient(field, pickerBrushes.white.Get(), true, 8);
    gradient(field, pickerBrushes.black.Get(), false, 8);
    gradient(hueBar, pickerBrushes.spectrum.Get(), false, 8);
    target(PickField, field);
    target(PickHue, hueBar);
    auto dot = Point2F(field.left + draft.sat * (field.right - field.left),
                       field.bottom - draft.value * (field.bottom - field.top));
    color(0x222222);
    rt->DrawEllipse(Ellipse(dot, 4, 4), brush.Get(), 3);
    color(0xffffff);
    rt->DrawEllipse(Ellipse(dot, 4, 4), brush.Get(), 1.5f);
    float hy = hueBar.top + draft.hue * (hueBar.bottom - hueBar.top);
    line(hueBar.left - 1, hy, hueBar.right + 1, hy, 0x222222, 4);
    line(hueBar.left - 1, hy, hueBar.right + 1, hy, 0xffffff, 2);
    float slot = (right - l) / std::size(paperSwatches);
    for (int i = 0; i < int(std::size(paperSwatches)); i++) {
        float mid = l + (i + .5f) * slot, cy = b - 51;
        Box chip = {mid - 10, cy - 10, mid + 10, cy + 10};
        panel(chip, 10, paperSwatches[i]);
        border(chip, 10, c.rule);
        if (paperSwatches[i] == rgb)
            border({mid - 12, cy - 12, mid + 12, cy + 12}, 12, c.accent, 2);
        target(PickSwatch + i, {mid - slot / 2, cy - 14, mid + slot / 2, cy + 14});
    }
    float mid = (l + right) / 2;
    button(PickCancel, {l, b - 33, mid - 3, b - 6}, tr(Msg::Cancel));
    button(PickApply, {mid + 3, b - 33, right, b - 6}, tr(Msg::Apply), true);
}

void Window::header() {
    if (compact()) {
        label(tr(Msg::Brand), {12, 5, w - 156, 38}, 18, canvasInk(c.ink), true);
        button(Home, {w - 143, 7, w - 77, 37}, tr(Msg::Shelf), false, screen == Shelf);
        settingsButton({w - 71, 7, w - 12, 37});
        return;
    }
    panel({margin, 18, margin + 36, 54}, 11, 0x34495d);
    fishIcon(margin + 4, 22, 28, 0xe8eff5);
    label(tr(Msg::Brand), {margin + 48, 12, w - 185, 44}, 22, canvasInk(c.ink), true);
    label(L"FishReader", {margin + 49, 41, w - 185, 58}, 10, canvasInk(c.muted), true);
    button(Home, {w - margin - 138, 21, w - margin - 74, 53}, tr(Msg::Shelf), false, screen == Shelf);
    settingsButton({w - margin - 66, 21, w - margin, 53});
}

void Window::layout() {
    if (!book)
        return;
    float left = textLeft();
    bodyTop = focus ? (compact() ? 10 : margin) : (compact() ? 45 : 151);
    bodyBottom = h - (focus ? (compact() ? 10 : 28) : (compact() ? 48 : 85));
    pages.configure(book.get(), dw.Get(), w - left * 2, (float)state.font,
                    State::linePercents[state.spacing]);
    rows = std::max(1, (int)std::floor((bodyBottom - bodyTop) / pages.line));
}

void Window::read() {
    layout();
    float x = textLeft();
    if (!focus && !compact()) {
        Str chap = tr(Msg::Reading);
        int ch = book->chapter(pos);
        if (ch >= 0)
            chap = book->toc[ch].name;
        label(chap, {x, 72, w - x, 98}, 13, canvasInk(c.accent), true);
        label(book->file.stem().wstring(), {x, 97, w - x, 134}, 26, canvasInk(c.ink), true);
        line(x, 141, w - x, 141, c.rule);
    }
    end = pages.visit(pos, rows, [&](IDWriteTextLayout *l, unsigned skip, unsigned n, unsigned y) {
        float top = bodyTop + y * pages.line;
        rt->PushAxisAlignedClip({x, top, x + pages.width, top + n * pages.line},
                                D2D1_ANTIALIAS_MODE_ALIASED);
        color(canvasInk(c.ink));
        rt->DrawTextLayout(Point2F(x, top - skip * pages.line), l, brush.Get());
        rt->PopAxisAlignedClip();
    });
    if (focus)
        return;
    float y = h - (compact() ? 36 : 60);
    double fraction = end == book->text.size() ? 1. : double(pos) / book->text.size();
    panel({x, y - 12, w - x, y - 9}, 1.5, c.rule);
    if (fraction > 0)
        panel({x, y - 12, x + float(pages.width * fraction), y - 9}, 1.5, c.accent);
    button(Contents, {x, y, x + 58, y + 32}, tr(Msg::Toc));
    button(Marks, {x + 65, y, x + 123, y + 32}, tr(Msg::Marks));
    button(Back, {w - x - 101, y, w - x - 55, y + 32}, L"‹");
    button(Next, {w - x - 48, y, w - x, y + 32}, L"›", true);
    if (w > 450)
        label(std::to_wstring(int(fraction * 100)) + L"%", {x + 130, y, w - x - 109, y + 32}, 13,
              canvasInk(c.muted), false, true);
}

int Window::count() const {
    return screen == Shelf
               ? (int)state.recent.size()
               : (screen == Chapters ? (int)book->toc.size() : (int)state.recent.front().marks.size());
}

void Window::list() {
    bool shelf = screen == Shelf;
    bool small = compact();
    float left = small ? 12 : margin + 4, right = w - left, top = small ? 86 : 145;
    if (!small)
        label(shelf                ? L"LIBRARY   /   01"
              : screen == Chapters ? L"INDEX   /   02"
                                   : L"NOTES   /   03",
              {left, 75, right, 94}, 11, c.accent, true);
    label(shelf                ? tr(Msg::ShelfTitle)
          : screen == Chapters ? tr(Msg::Chapters)
                               : tr(Msg::MyMarks),
          {left, small ? 43.f : 92.f, right - 110, small ? 81.f : 138.f}, small ? 17 : 30, c.ink, true);
    if (shelf)
        button(Open, {right - 108, top - 43, right, top - 10}, tr(Msg::AddBook), true);
    else if (screen == Bookmarks)
        button(AddMark, {right - 110, top - 43, right, top - 10}, tr(Msg::MarkPage), true);
    int total = count();
    listRows = std::max(1, int((h - top - 64) / 54));
    first = std::clamp(first, 0, std::max(0, total - listRows));
    bool scroll = screen == Chapters && total > listRows;
    float contentRight = right - (scroll ? 18 : 0);
    if (!total) {
        float bottom = h - 62, mid = (top + bottom) * .5f;
        panel({left, top, right, bottom}, 20, c.card);
        if (bottom - top >= 130)
            fishIcon((left + right) / 2 - 21, mid - 64, 42, c.accent);
        label(shelf                ? tr(Msg::ShelfEmpty)
              : screen == Chapters ? tr(Msg::NoChapters)
                                   : tr(Msg::NoMarks),
              {left + 10, mid - (small ? 21 : 11), right - 10, mid + (small ? 14 : 30)}, small ? 16 : 22,
              c.ink, true, true);
        if (bottom - top >= 110)
            label(shelf                ? tr(Msg::DropHint)
                  : screen == Chapters ? tr(Msg::ChapterHint)
                                       : tr(Msg::MarkHint),
                  {left + 10, mid + 30, right - 10, mid + 64}, 14, c.muted, false, true);
    } else {
        for (int i = first; i < std::min(total, first + listRows); ++i) {
            float y = top + (i - first) * 54;
            Box b = {left, y, contentRight, y + 48};
            panel(b, 12, hover == Item + i ? c.tint : c.card);
            Str name, detail;
            if (shelf) {
                auto p = fs::path(state.recent[i].path);
                name = p.stem().wstring();
                detail = p.extension().wstring();
            } else if (screen == Chapters) {
                name = book->toc[i].name;
                detail = i == book->chapter(pos) ? tr(Msg::Current) : std::to_wstring(i + 1);
            } else {
                auto p = state.recent.front().marks[i];
                name = book->text.substr(p, 55);
                std::replace(name.begin(), name.end(), L'\n', L' ');
                detail = std::to_wstring(int(100. * p / book->text.size())) + L"%";
            }
            float inset = 0;
            bool group = false;
            if (screen == Chapters) {
                inset = std::min(12.f * book->toc[i].depth,
                                 std::max(0.f, (contentRight - left - 130) * .5f));
                group = i + 1 < total && book->toc[i + 1].depth > book->toc[i].depth;
            }
            label(name, {left + 16 + inset, y, contentRight - 85, y + 48}, 16, c.ink, group);
            label(detail, {contentRight - 80, y, contentRight - 8, y + 48}, 12, c.accent, true, true);
            target(Item + i, b);
        }
    }
    if (scroll) {
        chapterTrack = {right - 12, top, right, top + listRows * 54 - 6};
        float height = chapterTrack.bottom - chapterTrack.top;
        float thumbHeight = std::min(height, std::max(28.f, height * listRows / total));
        float thumbTop = top + (height - thumbHeight) * first / (total - listRows);
        chapterThumb = {chapterTrack.left, thumbTop, chapterTrack.right, thumbTop + thumbHeight};
        panel({chapterTrack.left + 4, top, chapterTrack.right - 4, chapterTrack.bottom}, 2, c.rule);
        panel({chapterThumb.left + 3, thumbTop, chapterThumb.right - 3, chapterThumb.bottom}, 3,
              scrollingChapters || hover == ChapterScroll ? c.accent : c.muted);
    }
    float y = h - 49;
    if (book)
        button(Read, {left, y, left + 96, y + 30}, tr(Msg::Resume));
    if (total > listRows) {
        button(Up, {right - 99, y, right - 54, y + 30}, L"↑");
        button(Down, {right - 46, y, right, y + 30}, L"↓");
    }
}

void Window::appearanceControls(float l, float r, float y) {
    const float bottom =
        appearanceTab == 3 ? std::min(y + 84 + int(Slot::Count) * 32, focus ? h - 3 : h - 51) : y + 112;
    panel({l, y, r, bottom}, 14, c.card);
    float a = l + 8, b = r - 8, col = (b - a - 8) / 3, tabWidth = (b - a - 9) / 4;
    const wchar_t *tabs[] = {tr(Msg::Type), tr(Msg::Color), tr(Msg::Window), tr(Msg::Keys)};
    for (int i = 0; i < 4; ++i) {
        float x = a + i * (tabWidth + 3);
        button(TabType + i, {x, y + 5, x + tabWidth, y + 33}, tabs[i], false, appearanceTab == i);
    }
    auto row = [&](float top, View title, const Str &value, int minus, int plus) {
        label(title, {a + 3, top, b - 119, top + 32}, 13, c.ink, true);
        button(minus, {b - 115, top, b - 86, top + 30}, L"−");
        label(value, {b - 85, top, b - 30, top + 30}, 13, c.ink, false, true);
        button(plus, {b - 29, top, b, top + 30}, L"＋");
    };
    if (appearanceTab == 0) {
        row(y + 40, tr(Msg::Size), std::to_wstring(state.font), Smaller, Bigger);
        wchar_t buf[20];
        swprintf(buf, 20, L"%.2f×", State::linePercents[state.spacing] / 100.);
        row(y + 75, tr(Msg::Spacing), buf, Tighter, Looser);
    } else if (appearanceTab == 1) {
        const wchar_t *names[] = {tr(Msg::Slate), tr(Msg::Paper), tr(Msg::Night)};
        for (int i = 0; i < 3; ++i) {
            float x = a + i * (col + 4);
            button(Theme0 + i, {x, y + 40, x + col, y + 69}, names[i], false,
                   state.theme == i && state.background == State::noBackground);
        }
        float mid = (a + b) / 2;
        button(ChooseBg, {a, y + 75, mid + 8, y + 105}, tr(Msg::Custom), false,
               state.background != State::noBackground);
        button(ResetBg, {mid + 12, y + 75, b, y + 105}, tr(Msg::Reset));
    } else if (appearanceTab == 3) {
        shortcutList(a, b, y, bottom);
    } else {
        float divider = (a + b) / 2;
        button(ToggleLanguage, {divider + 2, y + 40, b, y + 69},
               state.language == Language::Chinese ? L"English" : L"中文");
        button(Pin, {a, y + 40, divider - 2, y + 69}, state.pin ? tr(Msg::PinOn) : tr(Msg::Pin), false,
               state.pin);
        float mid = (a + b) / 2;
        button(Translucent, {a, y + 75, mid - 2, y + 105}, glass ? tr(Msg::GlassOn) : tr(Msg::Glass), false,
               glass);
        button(Hide, {mid + 2, y + 75, b, y + 105}, tr(Msg::TrayHide));
    }
}

void Window::focusControls() {
    float r = w - 12;
    button(BubbleSettings, {r - 138, 12, r - 72, 46}, tr(Msg::Settings), false, options);
    button(BubbleMove, {r - 66, 12, r, 46}, tr(Msg::Move), false, dragging);
    if (options) {
        float l = std::max(12.f, r - 304);
        appearanceControls(l, r, 53);
        if (appearanceTab != 3)
            border({l, 53, r, 165}, 14, c.rule);
    }
}

void Window::settings() {
    if (compact()) {
        appearanceControls(10, w - 10, 45);
        button(book ? Read : Home, {10, h - 43, 105, h - 13}, book ? tr(Msg::Resume) : tr(Msg::BackShelf));
        if (book)
            button(Focus, {w - 110, h - 43, w - 10, h - 13}, tr(Msg::Focus), true);
        return;
    }
    float l = margin + 4, r = w - l, y = 96;
    label(L"DISPLAY  /  04", {l, 73, r, 94}, 11, c.accent, true);
    label(tr(Msg::Appearance), {l, y, r, y + 42}, 28, c.ink, true);
    y += 49;
    appearanceControls(l, r, y);
    y += 125;
    if (appearanceTab != 3) {
        if (book)
            button(Focus, {l, y, l + 108, y + 34}, tr(Msg::Focus), true);
        label(bossStatus(), {book ? l + 122 : l, y, r, y + 34}, 13, c.muted);
    } else if (book)
        button(Focus, {r - 108, h - 49, r, h - 19}, tr(Msg::Focus), true);
    button(book ? Read : Home, {l, h - 49, l + 95, h - 19}, book ? tr(Msg::Resume) : tr(Msg::BackShelf));
}

void Window::paint() {
    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);
    try {
        ensure();
        margin = w < 490 ? 17.f : 32.f;
        hits.clear();
        chapterTrack = chapterThumb = {};
        rt->BeginDraw();
        rt->Clear(ColorF(canvasColor()));
        if (!(focus && screen == Reading))
            header();
        if (screen == Reading && book)
            read();
        else if (screen == Settings)
            settings();
        else
            list();
        if (focus && screen == Reading && bubbles && !choosingColor)
            focusControls();
        if (choosingColor)
            colorCard();
        if (keyboard && selected >= 0 && selected < (int)hits.size()) {
            auto b = hits[selected].box;
            b.left -= 2;
            b.top -= 2;
            b.right += 2;
            b.bottom += 2;
            border(b, 12, c.accent, 2);
        }
        if (!choosingColor && (loading || notice.has_value())) {
            float l = margin, r = w - margin;
            Box b = {l, h - 91, r, h - 55};
            panel(b, 14, c.ink);
            label(loading ? tr(Msg::Loading) + loadName : notice->message(state.language),
                  {l + 10, b.top, r - 10, b.bottom}, 14, c.bg, true, true);
        }
        HRESULT rc = rt->EndDraw();
        if (rc == D2DERR_RECREATE_TARGET) {
            pickerBrushes.reset();
            brush.Reset();
            rt.Reset();
            invalid();
        } else
            check(rc);
    } catch (...) {
        EndPaint(hwnd, &ps);
        throw;
    }
    EndPaint(hwnd, &ps);
}
} // namespace leaf
