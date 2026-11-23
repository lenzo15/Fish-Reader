# 鱼读 · FishReader

**简体中文** · [English](README.en.md) ｜ **[下载最新版（Windows x64）](https://github.com/lenzo15/Fish-Reader/releases/latest)** · [全部发行版本](https://github.com/lenzo15/Fish-Reader/releases)

鱼读是一款极简的 Windows 离线阅读器，提供完整的上班摸鱼阅读体验，包体仅1M多。

无需编译：在发行页的 **Assets** 中下载 `FishReader.exe`，双击即可运行。需要源码时，选择同一版本的源码 ZIP。

## 从源码运行

需要 Windows 10 1703 或更高版本（含 Windows 11）和 [MSYS2](https://www.msys2.org/) 的 **UCRT64** 工具链。

1. 在 **MSYS2 UCRT64** 终端更新工具链并安装依赖。如果更新要求重启终端，重开后再执行一次 `pacman -Syu`。

   ```sh
   pacman -Syu
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-binutils mingw-w64-ucrt-x86_64-zlib
   ```

2. 在源码根目录打开 Windows **命令提示符（cmd）**，编译后启动：

   ```bat
   build.cmd
   bin\FishReader.exe
   ```

   脚本默认使用 `C:\msys64\ucrt64`；如安装在别处，请先在 cmd 中设置 `UCRT64` 指向对应的 `ucrt64` 目录，或者直接将本文档交给 AI 进行构建。

## 阅读

- **打开书籍：** 拖入 TXT、Markdown、EPUB、DOCX 或 FB2 文件，或按 `Ctrl+O`。Markdown 按纯文本显示。
- **单实例：** 重复打开应用会显示已有窗口；已最小化或隐藏到托盘时会恢复显示。
- **专注阅读：** 默认按 `Ctrl+'`（英文单引号）进入或退出。
- **调整外观：** 点击右上角的**齿轮（设置）**图标，修改字号、行距和背景色。要切换英文界面，选 **设置 → 窗口 → English**。
- **保存与备份：** 鱼读不复制或修改原书。书架、进度、书签和设置保存在 `%LOCALAPPDATA%\MoyuLeaf\state.bin`。

## 快捷键

**设置 → 按键** 集中列出以下 11 项绑定。

| 操作                            | 默认快捷键                                           |
| ------------------------------- | ---------------------------------------------------- |
| 隐藏到托盘 / 恢复（全局老板键） | `Ctrl+Q`                                             |
| 切换专注模式                    | `Ctrl+'`                                             |
| 打开书籍 / 书架 / 章节目录      | `Ctrl+O` / `Ctrl+L` / `Ctrl+I`                       |
| 添加或取消当前位置的书签        | `Ctrl+B`                                             |
| 打开设置                        | `Ctrl+,`                                             |
| 上一页 / 下一页                 | `K` / `J` 、 方向键、`Page Up` / `Page Down` 、 滚轮 |
| 跳到书首 / 书末                 | `Home` / `End`                                       |

## 测试

在源码根目录的 cmd 中运行 `build.cmd test-core` 验证导入与分页；运行 `build.cmd test` 还会检查快捷键、托盘恢复、界面和进度保存。完整测试需要带有 Explorer 通知区域的交互式 Windows 桌面。测试使用临时配置，不读取你的书架。

## 许可与致谢

鱼读的原创代码、资源和文档采用 [GPL-3.0-or-later](LICENSE)。第三方组件保留各自的许可。浮窗阅读的设计灵感来自 [moyu-novel-reader](https://github.com/JoChiho/moyu-novel-reader)；鱼读是独立的 Win32 实现，不包含原 Electron 项目。参与开发请参阅[贡献指南](CONTRIBUTING.md)。
