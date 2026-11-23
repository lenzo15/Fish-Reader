// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/window.hpp"
#include "platform/single_instance.hpp"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show) {
    using namespace leaf;
    Str file;
    int n = 0;
    auto args = CommandLineToArgvW(GetCommandLineW(), &n);
    if (args && n > 1)
        file = fs::absolute(args[1]).lexically_normal().wstring();
    if (args)
        LocalFree(args);
    SingleInstance single;
    auto result = single.enter(file);
    if (result != SingleInstance::Result::Primary)
        return result == SingleInstance::Result::Forwarded ? 0 : 1;
    ComScope com;
    Window a;
    WNDCLASSW cls{};
    cls.lpfnWndProc = proc;
    cls.hInstance = instance;
    cls.lpszClassName = SingleInstance::windowClass;
    cls.style = CS_DBLCLKS;
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.hIcon = LoadIconW(instance, MAKEINTRESOURCE(2));
    RegisterClassW(&cls);
    auto dpi = GetDpiForSystem();
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int width = std::min<int>(work.right - work.left, MulDiv(768, dpi, 96)),
        height = std::min<int>(work.bottom - work.top, MulDiv(724, dpi, 96));
    HWND h = CreateWindowW(cls.lpszClassName, translate(Language::Chinese, Msg::EmptyTitle),
                           WS_OVERLAPPEDWINDOW, work.left + (work.right - work.left - width) / 2,
                           work.top + (work.bottom - work.top - height) / 2, width, height, nullptr,
                           nullptr, instance, &a);
    if (!h)
        return 1;
    ShowWindow(h, show);
    UpdateWindow(h);
    if (!file.empty())
        a.beginFile(file);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
