// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "book.hpp"
#include <dwrite.h>
#include <list>
#include <cmath>

namespace leaf {
// DirectWrite shapes a few small text runs, not the entire book. Width/font
// changes discard only this bounded cache; character offsets stay stable.
class Pages {
    struct Run {
        size_t id;
        ComPtr<IDWriteTextLayout> layout;
        std::vector<Pos> starts;
    };
    std::list<Run> cache;
    Run &run(size_t id) {
        for (auto it = cache.begin(); it != cache.end(); ++it)
            if (it->id == id) {
                cache.splice(cache.begin(), cache, it);
                return cache.front();
            }
        Pos a = book->blocks[id], n = book->blocks[id + 1] - a;
        Run r{id, {}, {}};
        check(dw->CreateTextLayout(book->text.data() + a, n, format.Get(), width, 1e6f,
                                   r.layout.GetAddressOf()));
        UINT32 count = 0;
        r.layout->GetLineMetrics(nullptr, 0, &count);
        std::vector<DWRITE_LINE_METRICS> lines(count);
        check(r.layout->GetLineMetrics(lines.data(), count, &count));
        Pos cur = a;
        for (auto l : lines) {
            if (l.length) {
                r.starts.push_back(cur);
                cur += l.length;
            }
        }
        if (r.starts.empty() || cur != a + n)
            throw Fail{Msg::LayoutFailed};
        cache.push_front(std::move(r));
        if (cache.size() > 12)
            cache.pop_back();
        return cache.front();
    }
    std::pair<size_t, size_t> locate(Pos p) {
        p = std::min(p, (Pos)book->text.size() - 1);
        size_t b = std::upper_bound(book->blocks.begin(), book->blocks.end(), p) - book->blocks.begin() - 1;
        auto &lines = run(b).starts;
        return {b, size_t(std::upper_bound(lines.begin(), lines.end(), p) - lines.begin() - 1)};
    }

  public:
    const Book *book = nullptr;
    IDWriteFactory *dw = nullptr;
    ComPtr<IDWriteTextFormat> format;
    float width = 0, line = 0, size = 0;
    void reset() {
        cache.clear();
        book = nullptr;
        format.Reset();
    }
    void configure(const Book *b, IDWriteFactory *d, float w, float font, int linePercent = 172) {
        w = std::max(20.f, std::floor(w));
        float leading = std::ceil(font * linePercent / 100.f);
        if (book == b && format && width == w && size == font && line == leading)
            return;
        reset();
        book = b;
        dw = d;
        width = w;
        size = font;
        line = leading;
        check(dw->CreateTextFormat(L"Microsoft YaHei UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                   DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, font, L"zh-CN",
                                   format.GetAddressOf()));
        check(format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, line,
                                     std::ceil((line + font * .64f) / 2)));
    }
    Pos align(Pos p) {
        auto [b, i] = locate(p);
        return run(b).starts[i];
    }
    Pos previous(Pos p, unsigned rows) {
        auto [b, i] = locate(p);
        while (rows > i) {
            if (b == 0)
                return 0;
            rows -= unsigned(i) + 1;
            --b;
            i = run(b).starts.size() - 1;
        }
        return run(b).starts[i - rows];
    }
    template <class Draw> Pos visit(Pos p, unsigned rows, Draw draw) {
        auto [b, i] = locate(p);
        unsigned y = 0;
        while (rows && b + 1 < book->blocks.size()) {
            auto &r = run(b);
            unsigned n = std::min(rows, (unsigned)(r.starts.size() - i));
            draw(r.layout.Get(), unsigned(i), n, y);
            y += n;
            rows -= n;
            i += n;
            if (i < r.starts.size())
                return r.starts[i];
            ++b;
            i = 0;
        }
        return book->blocks[b];
    }
};
} // namespace leaf