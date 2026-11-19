// SPDX-License-Identifier: GPL-3.0-or-later
#include "book.hpp"
#include <xmllite.h>
#include <shlwapi.h>
#include <zlib.h>
#include <map>
#include <cstring>
#include <cwctype>

namespace leaf {
namespace {
constexpr size_t inputLimit = 128u << 20, textLimit = 64u << 20;
using Bytes = std::vector<uint8_t>;
Bytes readFile(const fs::path &p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f)
        throw Fail{Msg::OpenFailed};
    auto n = f.tellg();
    if (n < 0 || uint64_t(n) > inputLimit)
        throw Fail{Msg::SizeLimit};
    Bytes b(static_cast<size_t>(n));
    f.seekg(0);
    if (n && !f.read((char *)b.data(), n))
        throw Fail{Msg::IncompleteRead};
    return b;
}
Str multi(const uint8_t *p, size_t n, UINT cp) {
    if (!n)
        return {};
    int len = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, (LPCCH)p, (int)n, nullptr, 0);
    if (!len)
        return {};
    if (size_t(len) > textLimit)
        throw Fail{Msg::DecodeLimit};
    Str s(len, 0);
    MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, (LPCCH)p, (int)n, s.data(), len);
    return s;
}
Str decode(const Bytes &b) {
    if (b.empty())
        return {};
    if (b.size() >= 2 && ((b[0] == 255 && b[1] == 254) || (b[0] == 254 && b[1] == 255))) {
        if (b.size() % 2)
            throw Fail{Msg::Utf16Incomplete};
        Str s((b.size() - 2) / 2, 0);
        for (size_t i = 0; i < s.size(); ++i) {
            auto a = b[2 + 2 * i], c = b[3 + 2 * i];
            s[i] = b[0] == 255 ? a + (c << 8) : c + (a << 8);
        }
        return s;
    }
    size_t off = (b.size() >= 3 && b[0] == 239 && b[1] == 187 && b[2] == 191) ? 3 : 0;
    if (off == b.size())
        return {};
    auto s = multi(b.data() + off, b.size() - off, CP_UTF8);
    if (s.empty() && !off)
        s = multi(b.data(), b.size(), 54936);
    if (s.empty())
        throw Fail{Msg::Encoding};
    return s;
}
// Narrow ZIP reader: central directory only, never extracts paths to disk.
struct Entry {
    uint32_t offset, packed, size, crc;
    uint16_t method;
};
struct Zip {
    const Bytes &b;
    std::map<std::string, Entry> entries;
    size_t total = 0;
    void span(size_t p, size_t n) const {
        if (p > b.size() || n > b.size() - p)
            throw Fail{Msg::CorruptDirectory};
    }
    uint16_t u16(size_t p) const {
        span(p, 2);
        return b[p] | (b[p + 1] << 8);
    }
    uint32_t u32(size_t p) const {
        return u16(p) | (uint32_t(u16(p + 2)) << 16);
    }
    explicit Zip(const Bytes &bytes) : b(bytes) {
        if (b.size() < 22)
            throw Fail{Msg::InvalidArchive};
        size_t p = b.size() - 22;
        while (u32(p) != 0x06054b50 || p + 22 + u16(p + 20) != b.size()) {
            if (!p || b.size() - p > 65557)
                throw Fail{Msg::MissingDirectory};
            --p;
        }
        if (u16(p + 4) || u16(p + 6))
            throw Fail{Msg::SplitZip};
        int count = u16(p + 10);
        p = u32(p + 16);
        for (int i = 0; i < count; ++i) {
            if (u32(p) != 0x02014b50)
                throw Fail{Msg::InvalidEntry};
            size_t name = u16(p + 28), end = p + 46 + name + u16(p + 30) + u16(p + 32);
            span(p, end - p);
            if (u16(p + 8) & 1)
                throw Fail{Msg::Encrypted};
            entries.emplace(std::string((char *)b.data() + p + 46, name),
                            Entry{u32(p + 42), u32(p + 20), u32(p + 24), u32(p + 16), u16(p + 10)});
            p = end;
        }
    }
    Bytes get(const std::string &name) {
        auto it = entries.find(name);
        if (it == entries.end())
            throw Fail{Msg::MissingDocument};
        auto e = it->second;
        size_t p = e.offset;
        if (e.size > 32u << 20 || (total += e.size) > inputLimit)
            throw Fail{Msg::UnpackLimit};
        if (u32(p) != 0x04034b50)
            throw Fail{Msg::ZipHeader};
        p += 30 + u16(p + 26) + u16(p + 28);
        span(p, e.packed);
        Bytes out(std::max(1u, e.size));
        if (!e.method && e.packed == e.size)
            std::memcpy(out.data(), b.data() + p, e.size);
        else if (e.method == 8) {
            z_stream z{};
            z.next_in = (Bytef *)b.data() + p;
            z.avail_in = e.packed;
            z.next_out = out.data();
            z.avail_out = (uInt)out.size();
            if (inflateInit2(&z, -MAX_WBITS) != Z_OK)
                throw Fail{Msg::InitInflate};
            int rc = inflate(&z, Z_FINISH);
            inflateEnd(&z);
            if (rc != Z_STREAM_END || z.total_out != e.size)
                throw Fail{Msg::Inflate};
        } else
            throw Fail{Msg::ZipMethod};
        if (crc32(0, out.data(), e.size) != e.crc)
            throw Fail{Msg::Checksum};
        out.resize(e.size);
        return out;
    }
};
using Attr = std::map<Str, Str>;
template <class F> void xml(const Bytes &bytes, F emit) {
    ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(bytes.data(), (UINT)bytes.size()));
    if (!stream)
        throw Fail{Msg::EmptyXml};
    ComPtr<IXmlReader> r;
    check(CreateXmlReader(__uuidof(IXmlReader), (void **)r.GetAddressOf(), nullptr));
    check(r->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit));
    check(r->SetProperty(XmlReaderProperty_MaxElementDepth, 128));
    check(r->SetInput(stream.Get()));
    XmlNodeType t;
    HRESULT hr;
    const wchar_t *v;
    UINT n;
    Attr a;
    while ((hr = r->Read(&t)) == S_OK) {
        if (t == XmlNodeType_Element || t == XmlNodeType_EndElement) {
            check(r->GetLocalName(&v, &n));
            Str name(v, n);
            a.clear();
            if (t == XmlNodeType_Element) {
                bool empty = r->IsEmptyElement();
                for (auto rc = r->MoveToFirstAttribute(); rc == S_OK; rc = r->MoveToNextAttribute()) {
                    check(r->GetLocalName(&v, &n));
                    Str k(v, n);
                    check(r->GetValue(&v, &n));
                    a.emplace(std::move(k), Str(v, n));
                }
                r->MoveToElement();
                emit(0, name, a);
                if (empty)
                    emit(1, name, a);
            } else
                emit(1, name, a);
        } else if (t == XmlNodeType_Text || t == XmlNodeType_CDATA || t == XmlNodeType_Whitespace) {
            check(r->GetValue(&v, &n));
            emit(2, View(v, n), a);
        }
    }
    if (FAILED(hr))
        throw Fail{Msg::InvalidXml};
}
std::string utf8(View v) {
    return fs::path(Str(v)).u8string();
}
void extractText(const Bytes &data, Book &book, int mode) { // 0 XHTML, 1 WordprocessingML, 2 FB2
    int body = 0, hidden = 0, wordText = 0;
    Str &s = book.text;
    auto nl = [&] {
        if (!s.empty() && s.back() != L'\n')
            s += L'\n';
    };
    xml(data, [&](int event, View tag, const Attr &) {
        if (event != 2) {
            int d = event == 0 ? 1 : -1;
            if (tag == L"body")
                body += d;
            if (tag == L"style" || tag == L"script" || tag == L"binary")
                hidden += d;
            if (tag == L"t")
                wordText += d;
            if (body && !hidden &&
                (tag == L"p" || tag == L"br" || tag == L"div" || tag == L"title" || tag == L"li" ||
                 (tag.size() == 2 && tag[0] == L'h' && iswdigit(tag[1]))))
                nl();
        } else if (body && !hidden && (mode != 1 || wordText)) {
            if (mode != 1 && tag.find_first_not_of(L"\t\r\n ") == View::npos &&
                tag.find_first_of(L"\r\n") != View::npos)
                return;
            if (tag.size() > textLimit - s.size())
                throw Fail{Msg::TextLimit};
            s.append(tag);
        }
    });
    nl();
}
Str attr(const Attr &a, const wchar_t *k) {
    auto i = a.find(k);
    return i == a.end() ? Str() : i->second;
}
Str trim(View v) {
    auto a = v.find_first_not_of(L" \t　");
    if (a == View::npos)
        return {};
    return Str(v.substr(a, v.find_last_not_of(L" \t　") - a + 1));
}
bool heading(View v) {
    if (v.empty() || v.size() > 84)
        return false;
    if (v[0] == L'#')
        return v.find(L' ') <= 6;
    if (v.front() == L'【' && v.back() == L'】')
        return true;
    if (v.size() > 8 && !_wcsnicmp(v.data(), L"chapter ", 8))
        return iswdigit(v[8]);
    if (v[0] != L'第')
        return false;
    auto i = v.find_first_of(L"章卷回节部");
    return i > 1 && i < 15 &&
           v.substr(1, i - 1).find_first_not_of(
               L"〇零一二两三四五六七八九十百千万0123456789０１２３４５６７８９ \t") == View::npos;
}
} // namespace
void Book::index() {
    size_t w = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        auto c = text[i];
        if (c == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n')
                ++i;
            c = L'\n';
        }
        if (c == 0)
            throw Fail{Msg::Binary};
        text[w++] = c;
    }
    text.resize(w);
    if (text.find_first_not_of(L"\n\t　 ") == Str::npos)
        throw Fail{Msg::EmptyBook};
    blocks = {0};
    toc.clear();
    for (size_t p = 0; p < w;) {
        size_t q = text.find(L'\n', p);
        if (q == Str::npos)
            q = w;
        if (q - p <= 96) {
            Str name = trim(View(text).substr(p, q - p));
            if (toc.size() < 100000 && heading(name))
                toc.push_back({(Pos)p, std::move(name)});
        }
        size_t end = q + (q < w);
        if (end - blocks.back() > 2048 && p > blocks.back())
            blocks.push_back((Pos)p);
        while (end - blocks.back() > 2048) {
            size_t k = blocks.back() + 2048;
            if (low(text[k]) && high(text[k - 1]))
                --k;
            blocks.push_back((Pos)k);
        }
        p = end;
    }
    if (blocks.back() != w)
        blocks.push_back((Pos)w);
}
int Book::chapter(Pos pos) const {
    return int(std::upper_bound(toc.begin(), toc.end(), pos,
                                [](Pos p, const Heading &h) { return p < h.pos; }) -
               toc.begin()) -
           1;
}
Book openBook(const fs::path &file) {
    Book book;
    book.file = fs::absolute(file).lexically_normal();
    Str ext = file.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    auto b = readFile(file);
    if (ext == L".txt" || ext == L".md")
        book.text = decode(b);
    else if (ext == L".fb2")
        extractText(b, book, 2);
    else if (ext == L".docx") {
        Zip z(b);
        extractText(z.get("word/document.xml"), book, 1);
    } else if (ext == L".epub") {
        Zip z(b);
        Str opf;
        xml(z.get("META-INF/container.xml"), [&](int e, View k, const Attr &a) {
            if (!e && k == L"rootfile" && opf.empty())
                opf = attr(a, L"full-path");
        });
        std::map<Str, Str> items;
        std::vector<Str> spine;
        xml(z.get(utf8(opf)), [&](int e, View k, const Attr &a) {
            if (!e && k == L"item")
                items[attr(a, L"id")] = attr(a, L"href");
            else if (!e && k == L"itemref")
                spine.push_back(attr(a, L"idref"));
        });
        for (auto &id : spine) {
            auto path = (fs::path(opf).parent_path() / items[id]).lexically_normal().generic_u8string();
            std::string plain;
            for (size_t i = 0; i < path.size(); ++i) {
                if (path[i] == '%' && i + 2 < path.size() && isxdigit((unsigned char)path[i + 1]) &&
                    isxdigit((unsigned char)path[i + 2])) {
                    plain += char(std::stoi(path.substr(i + 1, 2), nullptr, 16));
                    i += 2;
                } else
                    plain += path[i];
            }
            extractText(z.get(plain), book, 0);
        }
    } else
        throw Fail{Msg::Format};
    book.index();
    return book;
}
} // namespace leaf