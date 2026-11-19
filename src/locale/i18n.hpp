// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <iterator>

namespace leaf {
enum class Language : uint32_t { Chinese = 0, English = 1 };
enum class Msg {
    Brand,
    EmptyTitle,
    Shelf,
    ShelfTitle,
    Toc,
    Marks,
    Chapters,
    MyMarks,
    Reading,
    Current,
    AddBook,
    MarkPage,
    ShelfEmpty,
    NoChapters,
    NoMarks,
    DropHint,
    ChapterHint,
    MarkHint,
    Resume,
    BackShelf,
    Offline,
    Type,
    Color,
    Window,
    Size,
    Spacing,
    Slate,
    Paper,
    Night,
    Custom,
    Reset,
    PinOn,
    Pin,
    GlassOn,
    Glass,
    TrayHide,
    Settings,
    Move,
    Focus,
    Appearance,
    Boss,
    KeyBusy,
    Background,
    Cancel,
    Apply,
    Loading,
    PickTitle,
    TrayTip,
    SaveFailed,
    HotConflict,
    Wait,
    ImportFailed,
    EndBook,
    MarkLimit,
    Marked,
    Unmarked,
    NoTray,
    Fatal,
    SysError,
    OpenFailed,
    SizeLimit,
    IncompleteRead,
    DecodeLimit,
    Utf16Incomplete,
    Encoding,
    CorruptDirectory,
    InvalidArchive,
    MissingDirectory,
    SplitZip,
    InvalidEntry,
    Encrypted,
    MissingDocument,
    UnpackLimit,
    ZipHeader,
    InitInflate,
    Inflate,
    ZipMethod,
    Checksum,
    EmptyXml,
    InvalidXml,
    TextLimit,
    Binary,
    EmptyBook,
    Format,
    LayoutFailed,
    Keys,
    KeyHelp,
    KeyListening,
    CaptureHint,
    KeySaved,
    NoBind,
    BossModifiers,
    InvalidBind,
    DupBind,
    OsBusy,
    KeyDefault,
    PrevPage,
    NextPage,
    FirstPage,
    LastPage,
    Count
};
struct Translation {
    const wchar_t *zh;
    const wchar_t *en;
};
inline constexpr Translation strings[] = {
    {L"鱼读", L"FishReader"},                                                                // Brand
    {L"鱼读  ·  FishReader", L"FishReader"},                                         // EmptyTitle
    {L"书架", L"Library"},                                                             // Shelf
    {L"我的书架", L"My library"},                                                      // ShelfTitle
    {L"目录", L"Index"},                                                               // Toc
    {L"书签", L"Marks"},                                                               // Marks
    {L"章节目录", L"Chapters"},                                                        // Chapters
    {L"我的书签", L"Bookmarks"},                                                       // MyMarks
    {L"阅读中", L"Reading"},                                                           // Reading
    {L"当前", L"Current"},                                                             // Current
    {L"＋  打开书", L"+ Open book"},                                                   // AddBook
    {L"标记 / 取消", L"Toggle mark"},                                                  // MarkPage
    {L"给自己留一页安静", L"A quiet place to read"},                                   // ShelfEmpty
    {L"未识别到章节标题", L"No chapters found"},                                       // NoChapters
    {L"留住喜欢的一页", L"Save a favorite page"},                                      // NoMarks
    {L"把一本书拖到这里，或点「打开书」。", L"Drop a book here or select Open book."}, // DropHint
    {L"仍可使用方向键和滚轮翻页。", L"Use the arrow keys or scroll to turn pages."},   // ChapterHint
    {L"在正文中使用书签快捷键，或点击「标记 / 取消」。",
     L"Use the bookmark shortcut or Toggle mark."},        // MarkHint
    {L"返回阅读", L"Resume"},                              // Resume
    {L"返回书架", L"Library"},                             // BackShelf
    {L"本地阅读  ·  无需联网", L"Offline reading"},        // Offline
    {L"排版", L"Type"},                                    // Type
    {L"配色", L"Color"},                                   // Color
    {L"窗口", L"Window"},                                  // Window
    {L"字号", L"Size"},                                    // Size
    {L"行距", L"Spacing"},                                 // Spacing
    {L"深蓝灰", L"Slate"},                                 // Slate
    {L"纸页", L"Paper"},                                   // Paper
    {L"夜读", L"Night"},                                   // Night
    {L"自定义背景", L"Custom color"},                      // Custom
    {L"恢复默认", L"Reset"},                               // Reset
    {L"置顶 · 开", L"Pinned"},                             // PinOn
    {L"窗口置顶", L"Pin window"},                          // Pin
    {L"半透明 · 开", L"Glass on"},                         // GlassOn
    {L"半透明", L"Glass"},                                 // Glass
    {L"隐藏到托盘", L"Hide to tray"},                      // TrayHide
    {L"设置", L"Settings"},                                // Settings
    {L"移动", L"Move"},                                    // Move
    {L"专注阅读", L"Focus"},                               // Focus
    {L"阅读外观", L"Appearance"},                          // Appearance
    {L"老板键（全局）", L"Hide / show (global)"},          // Boss
    {L"全局隐藏键已被占用", L"Hide shortcut unavailable"}, // KeyBusy
    {L"背景色", L"Page color"},                            // Background
    {L"取消", L"Cancel"},                                  // Cancel
    {L"应用", L"Apply"},                                   // Apply
    {L"正在整理正文 · ", L"Opening · "},                   // Loading
    {L"添加一本书", L"Open a book"},                       // PickTitle
    {L"鱼读 · 点击恢复阅读", L"FishReader · Click to resume"},   // TrayTip
    {L"进度未能保存，请检查目录权限。",
     L"Could not save progress. Check folder permissions."}, // SaveFailed
    {L"老板键无法注册，可到「Aa → 按键」更改。",
     L"Hide shortcut unavailable. Change it in Aa → Keys."},         // HotConflict
    {L"请稍候，正在打开书籍。", L"Please wait. A book is opening."}, // Wait
    {L"无法读取此书；请检查格式和可用内存。",
     L"Cannot read this book. Check its format and available memory."},                // ImportFailed
    {L"已读至最后一页", L"End of book"},                                               // EndBook
    {L"每本书最多保留 256 个书签", L"Limit: 256 bookmarks per book"},                  // MarkLimit
    {L"已添加书签", L"Bookmark added"},                                                // Marked
    {L"已取消书签", L"Bookmark removed"},                                              // Unmarked
    {L"系统托盘不可用，窗口未隐藏。", L"Tray unavailable. The window is still open."}, // NoTray
    {L"图形或系统资源不可用。请重新启动鱼读。",
     L"Graphics or system resources unavailable. Restart FishReader."},              // Fatal
    {L"Windows 无法完成此操作", L"Windows could not complete this operation"}, // SysError
    {L"未能打开文件，请确认文件仍在原位置。",
     L"Cannot open the file. Check that it has not moved."},                  // OpenFailed
    {L"文件超过单本 128 MiB 限制。", L"The file exceeds the 128 MiB limit."}, // SizeLimit
    {L"文件读取不完整。", L"Could not read the entire file."},                // IncompleteRead
    {L"解码后文本过大。", L"The decoded text is too large."},                 // DecodeLimit
    {L"UTF-16 文件不完整。", L"Incomplete UTF-16 file."},                     // Utf16Incomplete
    {L"编码无法识别，请另存为 UTF-8 或带 BOM 的 UTF-16。",
     L"Unknown encoding. Save as UTF-8 or UTF-16 with a BOM."},                        // Encoding
    {L"电子书压缩目录损坏。", L"The archive directory is corrupt."},                   // CorruptDirectory
    {L"不是有效的电子书压缩包。", L"This is not a valid book archive."},               // InvalidArchive
    {L"未找到 ZIP 目录。", L"ZIP directory not found."},                               // MissingDirectory
    {L"不支持分卷电子书。", L"Multi-part archives are not supported."},                // SplitZip
    {L"ZIP 目录项损坏。", L"The ZIP directory entry is corrupt."},                     // InvalidEntry
    {L"不支持加密电子书。", L"Encrypted books are not supported."},                    // Encrypted
    {L"电子书缺少所需文档。", L"A required document is missing from the book."},       // MissingDocument
    {L"电子书解压大小超过限制。", L"The unpacked book exceeds the size limit."},       // UnpackLimit
    {L"ZIP 文档头损坏。", L"The ZIP header is corrupt."},                              // ZipHeader
    {L"无法初始化解压器。", L"Could not initialize decompression."},                   // InitInflate
    {L"电子书解压失败。", L"Could not unpack the book."},                              // Inflate
    {L"不支持此 ZIP 压缩方式。", L"Unsupported ZIP compression method."},              // ZipMethod
    {L"电子书文档校验失败。", L"The document checksum does not match."},               // Checksum
    {L"XML 文档为空。", L"Empty XML document."},                                       // EmptyXml
    {L"XML 格式无效或包含未支持的 DTD。", L"Invalid XML or unsupported DTD."},         // InvalidXml
    {L"提取的正文过大。", L"The extracted text is too large."},                        // TextLimit
    {L"文件含有二进制数据或缺少 UTF-16 BOM。", L"Binary data or missing UTF-16 BOM."}, // Binary
    {L"书中没有可阅读的正文。", L"No readable text found."},                           // EmptyBook
    {L"支持 TXT、Markdown、EPUB、DOCX 和 FB2。",
     L"Supported formats: TXT, Markdown, EPUB, DOCX, FB2."},             // Format
    {L"无法计算文本行。", L"Could not lay out the text."},               // LayoutFailed
    {L"按键", L"Keys"},                                                  // Keys
    {L"点击一项，再按新快捷键", L"Click a row, then press a shortcut"},  // KeyHelp
    {L"请按键…", L"Press keys…"},                                        // KeyListening
    {L"Esc 取消 · Delete 清除", L"Esc: cancel · Delete: clear"},         // CaptureHint
    {L"快捷键已保存", L"Shortcut saved"},                                // KeySaved
    {L"未绑定", L"Unassigned"},                                          // NoBind
    {L"老板键需含 Ctrl 或 Alt", L"Global key needs Ctrl or Alt"},        // BossModifiers
    {L"此组合不可用，请换一组键", L"Unavailable; try another shortcut"}, // InvalidBind
    {L"已用于：", L"Already assigned to: "},                             // DupBind
    {L"Windows 已占用，请换一组键", L"Windows refused this shortcut"},   // OsBusy
    {L"本项默认", L"Use default"},                                       // KeyDefault
    {L"上一页", L"Previous page"},                                       // PrevPage
    {L"下一页", L"Next page"},                                           // NextPage
    {L"书首", L"Start of book"},                                         // FirstPage
    {L"书末", L"End of book"},                                           // LastPage
};
static_assert(std::size(strings) == static_cast<unsigned>(Msg::Count));
inline constexpr const wchar_t *translate(Language lang, Msg id) {
    const auto &entry = strings[static_cast<unsigned>(id)];
    return lang == Language::English ? entry.en : entry.zh;
}
inline constexpr const wchar_t *bookFilter(Language lang) {
    return lang == Language::English ? L"Supported books\0*.txt;*.md;*.epub;*.docx;*.fb2\0"
                                     : L"支持的书籍\0*.txt;*.md;*.epub;*.docx;*.fb2\0";
}
} // namespace leaf
