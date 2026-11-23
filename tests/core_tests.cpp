// SPDX-License-Identifier: GPL-3.0-or-later
// Headless reader tests: synthetic input, temporary files, no UI or user profile.
#include "reader/pages.hpp"
#include <zlib.h>
#include <iostream>

namespace {
using namespace leaf;
unsigned assertions = 0;
void expect(bool condition, const char *message) {
    ++assertions;
    if (!condition)
        throw std::runtime_error(message);
}
template <class F> void rejects(F action) {
    try {
        action();
    } catch (const Fail &) {
        ++assertions;
        return;
    }
    throw std::runtime_error("malformed input was accepted");
}
template <class F> void rejects(F action, Msg reason) {
    try {
        action();
    } catch (const Fail &failure) {
        expect(failure.id == reason, "unexpected input rejection reason");
        return;
    }
    throw std::runtime_error("invalid input was accepted");
}
struct Temp {
    fs::path path = fs::temp_directory_path() /
                    (L"FishReader-core-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                     std::to_wstring(GetTickCount64()));
    Temp() {
        expect(fs::create_directory(path), "create isolated temporary directory");
    }
    ~Temp() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};
void put(const fs::path &path, const std::string &data) {
    std::ofstream out(path, std::ios::binary);
    out.write(data.data(), (std::streamsize)data.size());
    expect(bool(out), "write synthetic input");
}
std::string utf16(View text, bool be) {
    std::string data = be ? "\xfe\xff" : "\xff\xfe";
    for (wchar_t ch : text) {
        data += char(be ? ch >> 8 : ch & 255);
        data += char(be ? ch & 255 : ch >> 8);
    }
    return data;
}
fs::path fixtures() {
    wchar_t executable[32768]{};
    DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    expect(length && length < 32768, "resolve fixtures independently of working directory");
    return fs::path(executable).parent_path().parent_path() / L"tests" / L"fixtures";
}
using Files = std::vector<std::pair<std::string, std::string>>;
void le(std::string &bytes, uint32_t value, int width) {
    for (int i = 0; i < width; ++i)
        bytes += char(value >> (8 * i));
}
std::string deflateRaw(const std::string &data) {
    z_stream stream{};
    expect(deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8,
                        Z_DEFAULT_STRATEGY) == Z_OK, "initialize fixture compressor");
    std::string packed(deflateBound(&stream, (uLong)data.size()), '\0');
    stream.next_in = (Bytef *)data.data();
    stream.avail_in = (uInt)data.size();
    stream.next_out = (Bytef *)packed.data();
    stream.avail_out = (uInt)packed.size();
    int result = deflate(&stream, Z_FINISH);
    size_t length = stream.total_out;
    deflateEnd(&stream);
    expect(result == Z_STREAM_END, "compress fixture data");
    packed.resize(length);
    return packed;
}
std::string zip(const Files &files, bool compressed = false) {
    std::string bytes, directory;
    for (const auto &file : files) {
        auto packed = compressed ? deflateRaw(file.second) : file.second;
        uint32_t offset = (uint32_t)bytes.size(), size = (uint32_t)file.second.size();
        uint32_t crc = crc32(0, (const Bytef *)file.second.data(), size);
        uint32_t packedSize = (uint32_t)packed.size();
        int method = compressed ? 8 : 0;
        le(bytes, 0x04034b50, 4);
        for (int n : {20, 0x800, method, 0, 0})
            le(bytes, n, 2);
        for (auto n : {crc, packedSize, size})
            le(bytes, n, 4);
        le(bytes, (uint32_t)file.first.size(), 2);
        le(bytes, 0, 2);
        bytes += file.first;
        bytes += packed;
        le(directory, 0x02014b50, 4);
        for (int n : {20, 20, 0x800, method, 0, 0})
            le(directory, n, 2);
        for (auto n : {crc, packedSize, size})
            le(directory, n, 4);
        le(directory, (uint32_t)file.first.size(), 2);
        for (int i = 0; i < 4; ++i)
            le(directory, 0, 2);
        le(directory, 0, 4);
        le(directory, offset, 4);
        directory += file.first;
    }
    uint32_t offset = (uint32_t)bytes.size();
    bytes += directory;
    le(bytes, 0x06054b50, 4);
    for (int n : {0, 0, int(files.size()), int(files.size())})
        le(bytes, n, 2);
    le(bytes, (uint32_t)directory.size(), 4);
    le(bytes, offset, 4);
    le(bytes, 0, 2);
    return bytes;
}
Files epub(const std::string &xhtml) {
    return {{"META-INF/container.xml", "<container><rootfile full-path='content.opf'/></container>"},
            {"content.opf", "<package><manifest><item id='body' href='body.xhtml'/></manifest>"
                            "<spine><itemref idref='body'/></spine></package>"},
            {"body.xhtml", xhtml}};
}
void textTests(const fs::path &temp) {
    auto path = temp / L"编码.TXT";
    for (const auto &data : {std::string(u8"第一章 春水\r\n正文😀\r末尾"),
                             std::string("\xef\xbb\xbf") + u8"第一章 春水\r\n正文😀\r末尾",
                             utf16(L"第一章 春水\r\n正文😀\r末尾", false),
                             utf16(L"第一章 春水\r\n正文😀\r末尾", true)}) {
        put(path, data);
        auto book = openBook(path);
        expect(book.text == L"第一章 春水\n正文😀\n末尾", "UTF-8/BOM/UTF-16 and newline normalization");
        expect(book.toc.size() == 1 && book.toc[0].name == L"第一章 春水", "text heading detection");
        expect(book.file == fs::absolute(path).lexically_normal(), "absolute source path");
    }
    put(path, std::string("\xd6\xd0\xce\xc4", 4));
    expect(openBook(path).text == L"中文", "GB18030 fallback");
    auto markdown = temp / L"book.md";
    put(markdown, u8"# 书名\r\n\r\n## 章节\r正文");
    auto book = openBook(markdown);
    expect(book.text == L"# 书名\n\n## 章节\n正文" && book.toc.size() == 2,
           "Markdown stays plain text with indexed headings");
    rejects([&] { openBook(temp / L"absent.txt"); }, Msg::OpenFailed);
    for (const auto &data : {std::string(), std::string(" \r\n\t")}) {
        put(path, data);
        rejects([&] { openBook(path); }, Msg::EmptyBook);
    }
    put(path, std::string("A\0B", 3));
    rejects([&] { openBook(path); }, Msg::Binary);
    put(path, std::string("\xff\xfe\x41", 3));
    rejects([&] { openBook(path); }, Msg::Utf16Incomplete);
    put(path, std::string("\xef\xbb\xbf\xff", 4));
    rejects([&] { openBook(path); }, Msg::Encoding);
    put(temp / L"unsupported.pdf", "not a supported format");
    rejects([&] { openBook(temp / L"unsupported.pdf"); }, Msg::Format);
}
void formatTests(const fs::path &temp) {
    for (auto file : {L"sample.epub", L"sample.docx", L"sample.fb2"}) {
        auto book = openBook(fixtures() / file);
        expect(!book.text.empty() && book.text.find(L"hidden head") == Str::npos,
               "EPUB/DOCX/FB2 import excludes metadata");
        expect(book.toc.size() == (View(file) == L"sample.epub" ? 2u : 1u), "fixture chapter count");
        if (View(file) == L"sample.epub")
            expect(book.toc[0].name == L"第一章 春水" && book.toc[1].name == L"第二章 山月",
                   "EPUB follows spine rather than manifest order");
    }
    auto path = temp / L"archive.epub";
    auto files = epub(u8"<html><body><h1>第一章 存档</h1><p>正文</p></body></html>");
    for (bool compressed : {false, true}) {
        put(path, zip(files, compressed));
        expect(openBook(path).text == L"第一章 存档\n正文\n", "stored and raw-DEFLATE ZIP entries");
    }
    auto original = zip(files);
    size_t central = original.find("PK\1\2");
    expect(central != std::string::npos, "fixture central directory");
    auto corrupt = original;
    corrupt[central + 16] ^= 1;
    put(path, corrupt);
    rejects([&] { openBook(path); }, Msg::Checksum);
    corrupt = original;
    corrupt[central + 8] |= 1;
    put(path, corrupt);
    rejects([&] { openBook(path); }, Msg::Encrypted);
    corrupt = original;
    corrupt[central + 10] = 99;
    put(path, corrupt);
    rejects([&] { openBook(path); }, Msg::ZipMethod);
    corrupt = original;
    std::string oversized;
    le(oversized, (32u << 20) + 1, 4);
    corrupt.replace(central + 24, 4, oversized);
    put(path, corrupt);
    rejects([&] { openBook(path); }, Msg::UnpackLimit);
    for (const auto &bad : {std::string("bad zip"), original.substr(0, original.size() - 7)}) {
        put(path, bad);
        rejects([&] { openBook(path); });
    }
    auto fb2 = temp / L"unsafe.fb2";
    put(fb2, "<!DOCTYPE body [<!ENTITY x 'unsafe'>]><body>&x;</body>");
    rejects([&] { openBook(fb2); });
}
void xhtmlTests(const fs::path &temp) {
    auto path = temp / L"compatibility.epub";
    auto read = [&](const std::string &xml) {
        put(path, zip(epub(xml)));
        return openBook(path);
    };
    auto book = read(u8"<?xml version='1.0' encoding='UTF-8'?><!DOCTYPE html>"
        "<html><head><title>hidden</title><style>hidden</style></head><body>"
        "<script>hidden</script><h1>第一章 春水</h1><p title='&nbsp;'>甲&nbsp;乙&amp;丙😀</p>"
        "<p>&amp;nbsp;</p></body></html>");
    expect(book.text == L"第一章 春水\n甲\u00a0乙&丙😀\n&nbsp;\n", "DOCTYPE, nbsp and XML entities");
    for (const char *declaration : {
            "<!DOCTYPE html PUBLIC '-//W3C//DTD XHTML 1.1//EN' 'https://example.invalid/xhtml.dtd'>",
            "<!DOCTYPE html SYSTEM 'https://example.invalid/dtd?x=[>'>"})
        expect(read(std::string(declaration) + "<html><body>A&nbsp;B</body></html>").text == L"A\u00a0B\n",
               "external HTML DOCTYPE ignored without loading a DTD");
    book = read("<!DOCTYPE html><!-- <!DOCTYPE html [<!ENTITY x 'bad'>]> &nbsp; -->"
                "<?note &nbsp;?><html><body><![CDATA[&nbsp; <!DOCTYPE html>]]></body></html>");
    expect(book.text == L"&nbsp; <!DOCTYPE html>\n", "comment, PI and CDATA literals preserved");
    std::string controls;
    for (char ch = 0; ch < 32; ++ch)
        if (ch != '\t' && ch != '\r' && ch != '\n')
            controls += ch;
    book = read("<html><body><p>A" + controls + "B\tC\r\nD\x7f</p></body></html>");
    expect(book.text == L"AB\tC\nD\x7f\n", "illegal C0 controls removed, valid whitespace preserved");
    for (bool be : {false, true}) {
        book = read(utf16(L"<?xml version='1.0' encoding='UTF-16'?><!DOCTYPE html>"
                          L"<html><body>中文&nbsp;文本😀\x1d</body></html>", be));
        expect(book.text == L"中文\u00a0文本😀\n", "UTF-16 LE/BE XHTML compatibility");
    }
    for (const char *bad : {
            "<!DOCTYPE html [<!ENTITY x 'expanded'>]><html><body>&x;</body></html>",
            "<!DOCTYPE html [<!ENTITY x SYSTEM 'file:///test-only'>]><html><body>&x;</body></html>",
            "<!DOCTYPE other><html><body>wrong root</body></html>",
            "<!DOCTYPE html><!DOCTYPE html><html><body>duplicate DTD</body></html>",
            "<html><body><!DOCTYPE html>misplaced DTD</body></html>",
            "<!DOCTYPE html SYSTEM 'unterminated><html><body>bad</body></html>",
            "<html><body>&unknown;</body></html>",
            "<html><body><p>broken</body></html>",
            "<html><body><![CDATA[unterminated</body></html>"})
        rejects([&] { read(bad); });
    std::string deep = "<html><body>";
    for (int i = 0; i < 129; ++i)
        deep += "<div>";
    deep += "too deep";
    for (int i = 0; i < 129; ++i)
        deep += "</div>";
    rejects([&] { read(deep + "</body></html>"); });
}
Book collection(const fs::path &path, const std::string &toc, bool nav = false, bool includeToc = true) {
    std::string item = nav ? "<item id='nav' href='Nav/toc.xhtml' properties='scripted&#10;nav'/>"
                          : "<item id='ncx' href='Nav/toc.ncx' media-type='application/x-dtbncx+xml'/>";
    Files files = {
        {"META-INF/container.xml", "<container><rootfile full-path='OPS/package.opf'/></container>"},
        {"OPS/package.opf", "<package><manifest><item id='second' href='Text/second.xhtml'/>"
            "<item id='first' href='Text/%E4%B8%8A%20%E5%8D%B7.xhtml'/>"
            "<item id='cover' href='Text/cover.xhtml'/>" + item + "</manifest><spine toc='ncx'>"
            "<itemref idref='cover'/><itemref idref='first'/><itemref idref='second'/></spine></package>"},
        {"OPS/Text/cover.xhtml", "<html><body><img src='cover.jpg'/></body></html>"},
        {u8"OPS/Text/上 卷.xhtml", u8"<html><body><h1 id='intro'>序言</h1><p>开篇\r\n文字</p>"
            "<h2 id='chapter one'>第一章 春水</h2><p>甲<span id='中'>乙</span>丙</p>"
            "<p><a name='legacy'/>旧式锚点</p></body></html>"},
        {"OPS/Text/second.xhtml", u8"<html><body><h1>第二章 山月</h1><p>后续</p></body></html>"}};
    if (includeToc)
        files.emplace_back(nav ? "OPS/Nav/toc.xhtml" : "OPS/Nav/toc.ncx", toc);
    put(path, zip(files, true));
    return openBook(path);
}
void ncxTests(const fs::path &temp) {
    auto path = temp / L"collection.epub";
    auto book = collection(path, u8"<!DOCTYPE ncx SYSTEM 'https://example.invalid/ncx.dtd'>"
        "<ncx><navMap><navPoint><navLabel><text>第一本书</text></navLabel>"
        "<content src='../Text/cover.xhtml'/><navPoint><navLabel><text>上卷</text></navLabel>"
        "<content src='../Text/上%20卷.xhtml'/><navPoint><navLabel><text>序言</text></navLabel>"
        "<content src='../Text/上%20卷.xhtml#intro'/></navPoint>"
        "<navPoint><navLabel><text>春水</text></navLabel>"
        "<content src='../Text/上%20卷.xhtml#chapter%20one'/></navPoint>"
        "<navPoint><navLabel><text>段内</text></navLabel>"
        "<content src='../Text/上%20卷.xhtml#%E4%B8%AD'/></navPoint>"
        "<navPoint><navLabel><text>旧式</text></navLabel>"
        "<content src='../Text/上%20卷.xhtml#legacy'/></navPoint>"
        "<navPoint><navLabel><text>无效</text></navLabel>"
        "<content src='../Text/上%20卷.xhtml#missing'/></navPoint></navPoint></navPoint>"
        "<navPoint><navLabel><text>第二本书</text></navLabel>"
        "<content src='../Text/./second.xhtml'/></navPoint>"
        "<navPoint><navLabel><text>外部</text></navLabel>"
        "<content src='https://example.invalid/book.xhtml'/></navPoint></navMap></ncx>");
    expect(book.toc.size() == 7, "NCX retains groups and omits unresolved/external targets");
    expect(book.toc[0].name == L"第一本书" && book.toc[0].depth == 0 && book.toc[0].pos == 0 &&
           book.toc[1].name == L"上卷" && book.toc[1].depth == 1 && book.toc[1].pos == 0 &&
           book.toc[2].name == L"序言" && book.toc[2].depth == 2 && book.toc[2].pos == 0,
           "book/volume/chapter preorder retained for image-only covers and equal targets");
    expect(book.toc[3].pos == book.text.find(L"第一章 春水") && book.toc[3].name == L"春水" &&
           book.toc[4].pos == book.text.find(L"乙丙") && book.toc[5].pos == book.text.find(L"旧式锚点") &&
           book.toc[6].pos == book.text.find(L"第二章 山月") && book.toc[6].depth == 0,
           "relative paths, percent-encoded UTF-8 filenames/fragments and exact anchor positions");
    expect(book.chapter(0) == 2 && book.chapter(book.toc[3].pos) == 3 &&
           book.chapter((Pos)book.text.size()) == 6, "binary chapter lookup with duplicate positions");
    book = collection(path, "<ncx><navMap><navPoint><navLabel><text>" + std::string(120, 'A') +
        u8"</text></navLabel><content src='../Text/second.xhtml'/></navPoint>"
        "<navPoint><navLabel><text>前面的书</text></navLabel>"
        "<content src='../Text/上%20卷.xhtml#intro'/></navPoint></navMap></ncx>");
    expect(book.toc.size() == 2 && book.toc[0].name == L"前面的书" && book.toc[1].name.size() == 120 &&
           book.chapter(book.toc[1].pos) == 1, "long native labels and spine-position sorting");
}
void navigationTests(const fs::path &temp) {
    auto path = temp / L"navigation.epub";
    auto book = collection(path, u8"<!DOCTYPE html><html xmlns:epub='http://www.idpf.org/2007/ops'>"
        "<body><nav epub:type='landmarks'><ol><li><a href='../Text/second.xhtml'>忽略</a></li></ol></nav>"
        "<nav epub:type='other toc'><h1>目录</h1><ol><li><span>第一本\n  书</span><ol>"
        "<li><a href='../Text/上%20卷.xhtml#intro'><span>上卷</span></a><ol>"
        "<li><a href='../Text/上%20卷.xhtml#chapter%20one'>第一章 <em>春水</em></a></li>"
        "</ol></li></ol></li><li><a href='../Text/second.xhtml'>第二本书</a></li></ol></nav>"
        "<nav epub:type='page-list'><ol><li><a href='../Text/second.xhtml'>忽略</a></li></ol></nav>"
        "</body></html>", true);
    expect(book.toc.size() == 4 && book.toc[0].name == L"第一本 书" && book.toc[0].pos == 0 &&
           book.toc[1].name == L"上卷" && book.toc[1].depth == 1 &&
           book.toc[2].name == L"第一章 春水" && book.toc[2].depth == 2 &&
           book.toc[2].pos == book.text.find(L"第一章 春水") && book.toc[3].depth == 0,
           "EPUB 3 toc-only scope, inline labels, nested levels and linkless group targets");
    for (const char *bad : {
            "<ncx><navMap/>",
            "<!DOCTYPE ncx [<!ENTITY x 'unsafe'>]><ncx>&x;</ncx>",
            "<ncx><navMap><navPoint><navLabel><text>missing</text></navLabel>"
            "<content src='../absent.xhtml'/></navPoint></navMap></ncx>"}) {
        book = collection(path, bad);
        expect(book.toc.size() == 2 && book.toc[0].name == L"第一章 春水" && book.toc[0].depth == 0,
               "malformed, unsafe and unresolved optional TOCs fall back to text headings");
    }
    book = collection(path, "", false, false);
    expect(book.toc.size() == 2, "missing optional NCX does not prevent reading");
}
void indexTests(const fs::path &) {
    Book book;
    book.text = L"序言\r\n第一章 春水\r正文\r\nChapter 2 Moon\n# 附录\n普通段落";
    book.index();
    expect(book.text.find(L'\r') == Str::npos && book.toc.size() == 3, "normalized text heading index");
    expect(book.toc[0].pos == book.text.find(L"第一章") &&
           book.toc[1].pos == book.text.find(L"Chapter 2") && book.toc[2].pos == book.text.find(L"# 附录"),
           "detected heading positions reference normalized text");
    expect(book.chapter(0) == -1 && book.chapter(book.toc[0].pos) == 0 &&
           book.chapter(book.toc[1].pos - 1) == 0 && book.chapter((Pos)book.text.size()) == 2,
           "chapter lookup before first heading, at boundaries and at EOF");
    Book native;
    native.text = L"A\r\nB\rC";
    native.toc = {{0, L"book", 0}, {3, L"volume", 1}, {5, L"chapter", 2}, {6, L"EOF", 0}};
    native.index();
    expect(native.text == L"A\nB\nC" && native.toc[1].pos == 2 && native.toc[2].pos == 4 &&
           native.toc[3].pos == 5 && native.toc[1].depth == 1, "native offsets survive CRLF/CR normalization");
    Book longLine;
    longLine.text = Str(2047, L'A') + L"😀" + Str(16000, L'B') + L"\n最后一行";
    longLine.index();
    expect(longLine.blocks.front() == 0 && longLine.blocks.back() == longLine.text.size(),
           "layout blocks cover the entire text with EOF sentinel");
    for (size_t i = 1; i < longLine.blocks.size(); ++i) {
        Pos start = longLine.blocks[i - 1], end = longLine.blocks[i];
        expect(end > start && end - start <= 2048, "bounded, strictly increasing layout blocks");
        expect(end == longLine.text.size() || !low(longLine.text[end]), "blocks never split surrogate pairs");
    }
    expect(longLine.toc.empty() && longLine.chapter(0) == -1, "plain prose has no false headings");
}
void paginationTests(const fs::path &) {
    ComPtr<IDWriteFactory> factory;
    check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                             (IUnknown **)factory.GetAddressOf()));
    Book book;
    for (int i = 0; i < 900; ++i)
        book.text += L"汉字 mixed words 👋🏽，标点。0123456789! 随着窗口宽度改变，正文仍应完整连续。\n";
    book.text += L"\n最后的段落\n";
    book.index();
    expect(book.blocks.size() > 13, "pagination fixture exceeds the bounded layout cache");
    Pages pages;
    for (float width : {160.f, 460.f, 720.f}) {
        for (float font : {14.f, 24.f}) {
            int spacing = font == 14 ? 120 : 210;
            pages.configure(&book, factory.Get(), width, font, spacing);
            auto *format = pages.format.Get();
            pages.configure(&book, factory.Get(), width + .5f, font, spacing);
            expect(pages.format.Get() == format && pages.width == width, "equivalent layout reuses format");
            expect(pages.line == std::ceil(font * spacing / 100.f), "configured line spacing");
            for (unsigned rows : {1u, 11u, 23u}) {
                Pos position = 0;
                size_t visits = 0;
                while (position < book.text.size()) {
                    expect(++visits <= book.text.size(), "pagination terminates");
                    expect(pages.align(position) == position && !low(book.text[position]),
                           "page starts are aligned and never split UTF-16 pairs");
                    unsigned drawn = 0;
                    Pos end = pages.visit(position, rows, [&](IDWriteTextLayout *layout, unsigned first,
                                                              unsigned count, unsigned y) {
                        expect(layout && count && count <= rows - drawn && y == drawn,
                               "layout callbacks cover consecutive visible rows");
                        UINT32 lines = 0;
                        layout->GetLineMetrics(nullptr, 0, &lines);
                        expect(first + count <= lines, "draw ranges stay inside shaped lines");
                        drawn += count;
                    });
                    expect(drawn && end > position && end <= book.text.size(), "page coverage without gaps or loops");
                    if (end < book.text.size())
                        expect(pages.previous(end, rows) == position, "exact forward/backward page roundtrip");
                    position = end;
                }
                expect(pages.previous(0, rows) == 0, "previous page clamps at book start");
            }
            Pos probe = (Pos)book.text.size() / 2;
            expect(pages.align(probe) <= probe && !low(book.text[pages.align(probe)]), "arbitrary anchor alignment");
        }
    }
    pages.configure(&book, factory.Get(), 1, 20);
    expect(pages.width == 20 && pages.visit(0, 2, [](auto...) {}) > 0, "minimum width remains usable");
    pages.reset();
    expect(!pages.book && !pages.format, "layout reset releases book and format");
    pages.configure(&book, factory.Get(), 320, 20);
    expect(pages.visit(0, 2, [](auto...) {}) > 0, "pagination resumes after cache reset");
}
} // namespace
int wmain() {
    const char *current = "initialization";
    try {
        ComScope com;
        check(com.hr);
        Temp temp;
        struct Test { const char *name; void (*run)(const fs::path &); };
        const Test tests[] = {
            {"text encodings, Markdown and rejected inputs", textTests},
            {"EPUB/DOCX/FB2 import, ZIP compression and corruption", formatTests},
            {"EPUB XHTML compatibility, Unicode and XML security", xhtmlTests},
            {"EPUB 2 NCX hierarchy and exact navigation targets", ncxTests},
            {"EPUB 3 navigation and optional-TOC fallback", navigationTests},
            {"heading indexes, newline offsets and bounded blocks", indexTests},
            {"headless DirectWrite pagination and cache resets", paginationTests}};
        for (const auto &test : tests) {
            current = test.name;
            test.run(temp.path);
            std::cout << "PASS: " << test.name << "\n";
        }
        std::cout << "PASS: " << std::size(tests) << " core test groups, " << assertions << " assertions\n";
        return 0;
    } catch (const Fail &failure) {
        std::cerr << "FAIL: " << current << "\n";
        std::wcerr << failure.message(Language::English) << L"\n";
    } catch (const std::exception &failure) {
        std::cerr << "FAIL: " << current << ": " << failure.what() << "\n";
    }
    return 1;
}
