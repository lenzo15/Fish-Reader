# FishReader · 鱼读

[简体中文](README.md) · **English** | **[Download the latest release (Windows x64)](https://github.com/lenzo15/Fish-Reader/releases/latest)** · [All releases](https://github.com/lenzo15/Fish-Reader/releases)

FishReader is a minimalist offline reader for Windows, built for sneaking in a few pages at work. The Windows executable is just over 1 MB.

Drop a book onto the window to start reading. Press `Ctrl+'` to switch to a compact, borderless window that shows only the text.

No build required: download `FishReader.exe` from **Assets** on the release page and double-click it to run. For the matching source, choose the source ZIP from the same release.

## Build from source

You need Windows 10 version 1703 or later (including Windows 11) and the **UCRT64** toolchain from [MSYS2](https://www.msys2.org/).

1. Update MSYS2 and install the dependencies in the **MSYS2 UCRT64** terminal. If the update asks you to restart the terminal, reopen it and run `pacman -Syu` again.

   ```sh
   pacman -Syu
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-binutils mingw-w64-ucrt-x86_64-zlib
   ```

2. Open a Windows **Command Prompt (cmd)** in the project root. Build and launch the reader:

   ```bat
   build.cmd
   bin\FishReader.exe
   ```

   The build script looks for the toolchain in `C:\msys64\ucrt64`. If you installed it elsewhere, set `UCRT64` in cmd to your `ucrt64` directory first. You can also give this README to an AI coding assistant and ask it to build the app for you.

## Reading

- **Open a book:** Drop a TXT, Markdown, EPUB, DOCX or FB2 file onto the window, or press `Ctrl+O`. Markdown is displayed as plain text.
- **Focus mode:** By default, `Ctrl+'` (apostrophe) toggles focus mode on or off.
- **Appearance:** Open **Aa** to adjust the font size, line spacing and background color. To switch to English, choose **Aa → 窗口 → English**.
- **Saves and backups:** FishReader does not copy or modify your books. Your library, reading positions, bookmarks and settings are stored in `%LOCALAPPDATA%\MoyuLeaf\state.bin`.

## Shortcuts

Open **Aa → Keys** to see all 11 bindings. Click a row and press a new key or key combination to save it without opening a dialog. While editing, press `Esc` to cancel, `Delete` to clear the binding, or click **Use default** to reset that row. If FishReader detects a conflict, it keeps the previous binding.

| Action | Default shortcut |
| --- | --- |
| Hide to tray / restore (global boss key) | `Ctrl+Q` |
| Toggle focus mode | `Ctrl+'` |
| Open a book / library / chapter list | `Ctrl+O` / `Ctrl+L` / `Ctrl+I` |
| Toggle a bookmark at the current position | `Ctrl+B` |
| Open settings | `Ctrl+,` |
| Previous / next page | `K` / `J` |
| Jump to the start / end of the book | `Home` / `End` |

The boss key also works when another app has focus. You can also turn pages with the arrow keys, Page Up, Page Down or the mouse wheel.

## Tests

From the project root in cmd, run `build.cmd test-core` to check book import and pagination. Run `build.cmd test` to also check shortcuts, restoring the window from the tray, the interface and saved reading positions. The full suite requires an interactive Windows desktop with the Explorer notification area. Tests use a temporary profile and do not read your library.

## License and credits

FishReader's original code, assets and documentation are licensed under [GPL-3.0-or-later](LICENSE). Third-party components retain their own licenses. The floating reading window was inspired by [moyu-novel-reader](https://github.com/JoChiho/moyu-novel-reader). FishReader is an independent Win32 implementation and does not include the Electron project. To contribute, see the [contributing guide](CONTRIBUTING.md).
