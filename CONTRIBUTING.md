# 贡献指南

改动请针对新版原生阅读器。报告问题时注明 Windows 版本、界面语言、显示缩放、操作步骤和预期/实际结果；格式导入问题请优先提供**最小的合成样本**，不要上传有版权的整本书或个人书架文件。

## 开发约定

- 使用 C++17，遵循根目录的 `.clang-format`。文件导入放在 `src/reader/`；绘制在 `src/ui/render.cpp`，Win32 消息与交互在 `src/ui/input.cpp`，窗口状态与异步导入在 `src/ui/window.cpp`。
- UI 字符串在 `src/locale/i18n.hpp` 中成对维护。不要翻译用户的书名、章节或正文。配置格式变更需兼容旧数据并补充迁移测试。
- 修改导入、排版或窗口行为时，在 `tests/tests.cpp` 增加相应测试。按照 [README](README.md#测试) 运行。CI 工作流只覆盖核心测试；托盘、DPI 和窗口行为还需要桌面环境。
- 正常编译不需要生成图标。若修改鱼形轮廓或配色，请安装 Python 3 和 Pillow 后运行 `python tools/make_fish_icon.py`，同时提交 `assets/fish.ico` 和 `src/ui/fish_mark.hpp`。
- 原创贡献按项目许可 **GPL-3.0-or-later** 提供。在新的源码/脚本文件中保留相应的 SPDX 标识。引入第三方素材前请确认授权和来源，并保留上游许可。不要给第三方原始许可正文附加项目许可头。

## 发布检查清单

本项目仅发布源码，不附安装包或预编译程序。构建脚本、资源、测试和许可声明都是源码分发所需的输入，不要删除。

1. 保持 [中文 README](README.md) 与 [English README](README.en.md) 内容同步，版本号与 Windows 资源和源码导出脚本一致。
2. 新增必要文件时更新 `SOURCE_MANIFEST.txt`。运行 `python tools/package_source.py`，生成本地归档 `dist/FishReader-1.0.0-source.zip`。只分发清单内的源码树，不提交归档本身、`dist/`、`bin/`、个人书库或配置。
3. 将归档解压到临时目录，确认文件清单正确，运行 `build.cmd test`。测试生成的文件不放回源码包。导出和测试不会上传或推送任何内容。
