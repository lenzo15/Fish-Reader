// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "common.hpp"

namespace leaf {
// Session-local, independent of the executable's location. Hold ownership until
// shutdown; an abandoned mutex can be claimed after a crash, even if another
// launcher still has a handle to it.
struct SingleInstance {
    static constexpr const wchar_t *windowClass = L"FishReaderNative";
    static constexpr const wchar_t *mutexName = L"Local\\FishReaderNative.SingleInstance.v1";
    static constexpr ULONG_PTR activation = 0x46523131;
    enum class Result { Primary, Forwarded, Failed };
    HANDLE mutex = nullptr;
    bool owned = false;

    explicit SingleInstance(const wchar_t *name = mutexName) {
        mutex = CreateMutexW(nullptr, FALSE, name);
    }
    SingleInstance(const SingleInstance &) = delete;
    SingleInstance &operator=(const SingleInstance &) = delete;
    ~SingleInstance() {
        if (owned)
            ReleaseMutex(mutex);
        if (mutex)
            CloseHandle(mutex);
    }
    Result enter(const Str &file, const wchar_t *className = windowClass, DWORD timeout = 5000) {
        if (!mutex || file.size() >= 32768)
            return Result::Failed;
        ULONGLONG deadline = GetTickCount64() + timeout;
        for (;;) {
            DWORD wait = WaitForSingleObject(mutex, 0);
            if (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED) {
                owned = true;
                return Result::Primary;
            }
            if (wait != WAIT_TIMEOUT || GetTickCount64() >= deadline)
                return Result::Failed;
            if (HWND existing = FindWindowW(className, nullptr)) {
                DWORD pid = 0;
                GetWindowThreadProcessId(existing, &pid);
                if (pid)
                    AllowSetForegroundWindow(pid);
                COPYDATASTRUCT data{activation, DWORD((file.size() + 1) * sizeof(wchar_t)),
                                    const_cast<wchar_t *>(file.c_str())};
                DWORD_PTR accepted = 0;
                ULONGLONG now = GetTickCount64();
                if (now >= deadline)
                    return Result::Failed;
                DWORD remaining = DWORD(deadline - now);
                if (SendMessageTimeoutW(existing, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data),
                                        SMTO_ABORTIFHUNG | SMTO_BLOCK | SMTO_ERRORONEXIT,
                                        std::min<DWORD>(1000, remaining), &accepted) && accepted == 1)
                    return Result::Forwarded;
            }
            // The owner may still be creating its window or shutting down. Do
            // not launch a second reader while waiting for its endpoint.
            Sleep(25);
        }
    }
    static bool request(const COPYDATASTRUCT *data, Str &file) {
        if (!data || data->dwData != activation || !data->lpData ||
            data->cbData < sizeof(wchar_t) || data->cbData > 32768 * sizeof(wchar_t) ||
            data->cbData % sizeof(wchar_t))
            return false;
        const auto *text = static_cast<const wchar_t *>(data->lpData);
        size_t length = data->cbData / sizeof(wchar_t);
        if (text[length - 1] != L'\0')
            return false;
        file.assign(text, length - 1);
        return file.find(L'\0') == Str::npos;
    }
};
} // namespace leaf
