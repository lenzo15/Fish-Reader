# 鱼读 · FishReader

**简体中文** · [English](README.en.md)

鱼读是一款极简的 Windows 离线阅读器，提供完整的上班摸鱼阅读体验，完善包体仅1M多。

把书拖进窗口即可开始阅读；按 `Ctrl+'`，可以把窗口缩成只显示正文的无边框浮窗。关闭后再从书架打开，鱼读会回到上次读到的位置。

**1.0.0 · Windows x64 · [GPL-3.0-or-later](LICENSE)**  
本项目仅发布源码。仓库包含 C++17 源码、界面资源、构建脚本和测试，不提供安装包、预编译程序或 `bin/`、`dist/` 等构建目录。

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

   `bin\FishReader.exe` 是在你电脑上生成的文件，不是下载项。脚本默认使用 `C:\msys64\ucrt64`；如安装在别处，请先在 cmd 中设置 `UCRT64` 指向对应的 `ucrt64` 目录。

## 阅读

- **打开书籍：** 拖入 TXT、Markdown、EPUB、DOCX 或 FB2 文件，或按 `Ctrl+O`。Markdown 按纯文本显示；EPUB 按书脊顺序读取正文，不还原插图和出版社排版，也不支持 DRM。
- **专注阅读：** 默认按 `Ctrl+'`（英文单引号）进入或退出。鼠标移入浮窗，「设置」和「移动」便会出现；按住「移动」拖动窗口，拖动边缘可缩放。
- **调整外观：** 从 **Aa** 修改字号、行距和背景色。要切换英文界面，选 **Aa → 窗口 → English**。
- **保存与备份：** 鱼读不复制或修改原书。书架、进度、书签和设置保存在 `%LOCALAPPDATA%\MoyuLeaf\state.bin`。备份前先退出鱼读；该文件含本地书籍路径，请勿提交到仓库或问题报告中。

## 快捷键

**Aa → 按键** 集中列出以下 11 项绑定。点一行，再按新键或组合键即可保存，不弹窗。录入时按 `Esc` 取消、`Delete` 解绑，或点「本项默认」恢复该项。若检测到冲突，原绑定不变。

| 操作 | 默认快捷键 |
| --- | --- |
| 隐藏到托盘 / 恢复（全局老板键） | `Ctrl+Q` |
| 切换专注模式 | `Ctrl+'` |
| 打开书籍 / 书架 / 章节目录 | `Ctrl+O` / `Ctrl+L` / `Ctrl+I` |
| 添加或取消当前位置的书签 | `Ctrl+B` |
| 打开设置 | `Ctrl+,` |
| 上一页 / 下一页 | `K` / `J` |
| 跳到书首 / 书末 | `Home` / `End` |

只有老板键在其他应用获得焦点时也生效，且组合必须包含 Ctrl 或 Alt。即使老板键不可用，仍可点击 Windows 通知区域的鱼形图标恢复已隐藏的窗口。方向键、Page Up / Page Down 和滚轮也可翻页，不受上述绑定影响。

## 测试

在源码根目录的 cmd 中运行 `build.cmd test-core` 验证导入与分页；运行 `build.cmd test` 还会检查快捷键、托盘恢复、界面和进度保存。完整测试需要带有 Explorer 通知区域的交互式 Windows 桌面。测试使用临时配置，不读取你的书架。

## 许可与致谢

鱼读的原创代码、资源和文档采用 [GPL-3.0-or-later](LICENSE)。第三方组件保留各自的许可。浮窗阅读的设计灵感来自 [moyu-novel-reader](https://github.com/JoChiho/moyu-novel-reader)；鱼读是独立的 Win32 实现，不包含原 Electron 项目。参与开发请参阅[贡献指南](CONTRIBUTING.md)。
