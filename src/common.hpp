// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A000006
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include "locale/i18n.hpp"
#include <windows.h>
#include <wrl/client.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <stdexcept>
namespace leaf {
using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;
using Str = std::wstring;
using View = std::wstring_view;
using Pos = uint32_t;
struct Fail {
    Msg id;
    HRESULT code = S_OK;
    Str message(Language lang) const {
        Str result = translate(lang, id);
        if (FAILED(code))
            result += L" (" + std::to_wstring((unsigned)code) + L")";
        return result;
    }
};
inline void check(HRESULT h) {
    if (FAILED(h))
        throw Fail{Msg::SysError, h};
}
inline bool high(wchar_t c) {
    return c >= 0xd800 && c <= 0xdbff;
}
inline bool low(wchar_t c) {
    return c >= 0xdc00 && c <= 0xdfff;
}
struct ComScope {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ~ComScope() {
        if (SUCCEEDED(hr))
            CoUninitialize();
    }
};
} // namespace leaf