# FishReader · 鱼读

[简体中文](README.md) · **English**

FishReader is a minimalist offline reader for Windows with everything you need to sneak in a few pages at work. The whole app is just over 1 MB.

Drop a book onto the window to start reading. Press `Ctrl+'` to switch to a small, borderless window that shows just the text. The next time you open the book from your library, FishReader returns to where you left off.

**1.0.0 · Windows x64 · [GPL-3.0-or-later](LICENSE)**  
This is a source-only release. It includes the C++17 source, UI resources, build scripts and tests. Installers, prebuilt executables and output directories such as `bin/` and `dist/` are not distributed.

## Build and run

You need Windows 10 version 1703 or later (including Windows 11) and the **UCRT64** toolchain from [MSYS2](https://www.msys2.org/).

1. Update MSYS2 and install the dependencies in its **UCRT64** terminal. If the update asks you to restart the terminal, reopen it and run `pacman -Syu` again.

   ```sh
   pacman -Syu
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-binutils mingw-w64-ucrt-x86_64-zlib
   ```

2. Open a Windows **Command Prompt (cmd)** in the source directory, then build and launch the reader:

   ```bat
   build.cmd
   bin\FishReader.exe
   ```

   The build creates `bin\FishReader.exe` on your machine; it is not a download. If your toolchain is not in `C:\msys64\ucrt64`, set the `UCRT64` environment variable in cmd to the path of your `ucrt64` directory first.

## Reading

- **Open a book:** Drop a TXT, Markdown, EPUB, DOCX or FB2 file onto the reader, or press `Ctrl+O`. FishReader displays Markdown as plain text and reads EPUB text in spine order. It does not render EPUB illustrations or publisher layouts, or open DRM-protected books.
- **Read in focus mode:** Press `Ctrl+'` (apostrophe) to enter or leave focus mode. Move the pointer into the window to reveal **Settings** and **Move**. Drag **Move** to reposition it, or drag an edge to resize it.
- **Adjust the display:** Open **Aa** to change text size, line spacing or background color. The interface starts in Chinese; choose **Aa → 窗口 → English** to switch.
- **Back up your place:** FishReader does not copy or modify your books. It saves the library, reading positions, bookmarks and preferences in `%LOCALAPPDATA%\MoyuLeaf\state.bin`. Exit FishReader before backing up that file. It contains local file paths; do not include it in commits or bug reports.

## Shortcuts

Open **Aa → Keys** for all 11 bindings. Click a row and press the new key or combination to save it immediately, without opening a dialog. While editing, press `Esc` to cancel, `Delete` to unbind, or click **Use default** to reset that row. If FishReader detects a conflict, it keeps the previous binding.

| Action | Default shortcut |
| --- | --- |
| Hide to tray / restore (global) | `Ctrl+Q` |
| Toggle focus mode | `Ctrl+'` |
| Open a book / library / chapters | `Ctrl+O` / `Ctrl+L` / `Ctrl+I` |
| Toggle a bookmark at the current position | `Ctrl+B` |
| Open settings | `Ctrl+,` |
| Previous / next page | `K` / `J` |
| Jump to the start / end of the book | `Home` / `End` |

Only the hide/restore shortcut works outside FishReader, and it must include Ctrl or Alt. If that shortcut is unavailable, click the fish icon in the Windows notification area to restore a hidden window. Arrow keys, Page Up / Page Down and the mouse wheel still turn pages independently of these bindings.

## Tests

From the source directory in cmd, run `build.cmd test-core` to check import and pagination. Run `build.cmd test` to also check shortcuts, tray recovery, the UI and saved reading positions. The full suite needs an interactive Windows desktop with the Explorer notification area. Tests use a temporary profile, not your library.

## License and credits

FishReader's original code, assets and documentation are licensed under [GPL-3.0-or-later](LICENSE). Third-party components retain their own licenses. The floating reader was inspired by [moyu-novel-reader](https://github.com/JoChiho/moyu-novel-reader). FishReader is an independent Win32 implementation and does not include the Electron project. See the [contributing guide](CONTRIBUTING.md) to get involved.
