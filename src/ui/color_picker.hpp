// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "common.hpp"
#include <cmath>
#include <d2d1.h>

namespace leaf {
// The picker edits a draft. HSV retains the hue while a black or grey shade is selected.
struct ColorDraft {
    float hue = 0, sat = 0, value = 1;
    void fromRgb(DWORD rgb) {
        float r = ((rgb >> 16) & 255) / 255.f, g = ((rgb >> 8) & 255) / 255.f, b = (rgb & 255) / 255.f;
        float hi = std::max({r, g, b}), d = hi - std::min({r, g, b});
        value = hi;
        sat = hi ? d / hi : 0;
        if (d == 0)
            return;
        float sector = hi == r ? (g - b) / d : hi == g ? 2 + (b - r) / d : 4 + (r - g) / d;
        hue = (sector < 0 ? sector + 6 : sector) / 6;
    }
    static DWORD rgb(float h, float s, float v) {
        float sector = h * 6, f = sector - std::floor(sector);
        float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
        float r, g, b;
        switch (int(sector) % 6) {
        case 0:
            r = v;
            g = t;
            b = p;
            break;
        case 1:
            r = q;
            g = v;
            b = p;
            break;
        case 2:
            r = p;
            g = v;
            b = t;
            break;
        case 3:
            r = p;
            g = q;
            b = v;
            break;
        case 4:
            r = t;
            g = p;
            b = v;
            break;
        default:
            r = v;
            g = p;
            b = q;
            break;
        }
        auto u = [](float x) { return DWORD(std::lround(std::clamp(x, 0.f, 1.f) * 255)); };
        return (u(r) << 16) | (u(g) << 8) | u(b);
    }
    DWORD rgb() const {
        return rgb(hue, sat, value);
    }
};
inline constexpr DWORD paperSwatches[] = {0xfff6dd, 0xdcebdc, 0xe1eaf6, 0xf4dfe3, 0xded9ed, 0x28332f};

// Only three fixed gradients; positions change on resize, brushes only on device loss.
struct PickerBrushes {
    ComPtr<ID2D1LinearGradientBrush> white, black, spectrum;
    void reset() {
        white.Reset();
        black.Reset();
        spectrum.Reset();
    }
    void prepare(ID2D1RenderTarget *rt) {
        using namespace D2D1;
        if (spectrum)
            return;
        auto make = [&](std::initializer_list<D2D1_GRADIENT_STOP> stops,
                        ComPtr<ID2D1LinearGradientBrush> &out) {
            ComPtr<ID2D1GradientStopCollection> colors;
            check(rt->CreateGradientStopCollection(stops.begin(), UINT32(stops.size()),
                                                   colors.GetAddressOf()));
            check(rt->CreateLinearGradientBrush(LinearGradientBrushProperties(Point2F(0, 0), Point2F(1, 0)),
                                                colors.Get(), out.ReleaseAndGetAddressOf()));
        };
        make({{0, ColorF(0xffffffu, 1)}, {1, ColorF(0xffffffu, 0)}}, white);
        make({{0, ColorF(0u, 0)}, {1, ColorF(0u, 1)}}, black);
        make({{0, ColorF(0xff0000u)},
              {1.f / 6, ColorF(0xffff00u)},
              {2.f / 6, ColorF(0x00ff00u)},
              {.5f, ColorF(0x00ffffu)},
              {4.f / 6, ColorF(0x0000ffu)},
              {5.f / 6, ColorF(0xff00ffu)},
              {1, ColorF(0xff0000u)}},
             spectrum);
    }
};
} // namespace leaf
