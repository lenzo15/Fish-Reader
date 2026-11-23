# 贡献指南

改动请针对新版原生阅读器。报告问题时注明 Windows 版本、界面语言、显示缩放、操作步骤和预期/实际结果；格式导入问题请优先提供**最小的合成样本**，不要上传有版权的整本书或个人书架文件。

## 开发约定

- 使用 C++17，遵循根目录的 `.clang-format`。文件导入放在 `src/reader/`；绘制在 `src/ui/render.cpp`，Win32 消息与交互在 `src/ui/input.cpp`，窗口状态与异步导入在 `src/ui/window.cpp`。
- UI 字符串在 `src/locale/i18n.hpp` 中成对维护。不要翻译用户的书名、章节或正文。配置格式变更需兼容旧数据并补充迁移测试。
- 修改导入、目录或分页时，请在 `tests/core_tests.cpp` 添加相应回归测试，并运行 `build.cmd test-core`。CI 检查应用构建并运行无窗口核心测试；窗口交互请提供可复现步骤，托盘、DPI 和窗口行为仍需按照 [README](README.md#测试) 在桌面环境中手工验证。
- 正常编译不需要生成图标。若修改鱼形轮廓或配色，请安装 Python 3 和 Pillow 后运行 `python tools/make_fish_icon.py`，同时提交 `assets/fish.ico` 和 `src/ui/fish_mark.hpp`。
- 原创贡献按项目许可 **GPL-3.0-or-later** 提供。在新的源码/脚本文件中保留相应的 SPDX 标识。引入第三方素材前请确认授权和来源，并保留上游许可。不要给第三方原始许可正文附加项目许可头。

## 发布检查清单

本项目提供 Windows x64 可执行文件和对应版本的源码包。发布二进制时请同时提供对应源码、GPL 许可和第三方许可声明。构建脚本、资源和许可声明都是源码分发所需的输入，不要删除。

1. 保持 [中文 README](README.md) 与 [English README](README.en.md) 内容同步，版本号与 Windows 资源和源码导出脚本一致。
2. 新增必要文件时更新 `SOURCE_MANIFEST.txt`。运行 `python tools/package_source.py`，生成本地归档 `dist/FishReader-1.0.1-source.zip`。只分发清单内的源码树，不提交归档本身、`dist/`、`bin/`、个人书库或配置。
3. 将源码归档解压到临时目录，确认文件清单正确，运行 `build.cmd` 和 `build.cmd test-core`，使用这一干净构建的 EXE 制作 Windows 发行包；不要把编译产物放回源码包。按 README 完成窗口与托盘的手工验证，并为发行文件生成 SHA-256 校验清单。导出、构建和核心测试不会上传或推送任何内容。
