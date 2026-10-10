// SPDX-License-Identifier: MIT
#include "TextImport.h"
#include "GbkTable.h"
#include <SDCardManager.h>
#include <algorithm>
#include <vector>

namespace fm {
namespace {
String escape(const String &s) {
  String result;
  for (unsigned i = 0; i < s.length(); ++i) {
    switch (s[i]) {
    case '&': result += "&amp;"; break;
    case '<': result += "&lt;"; break;
    case '>': result += "&gt;"; break;
    case '"': result += "&quot;"; break;
    default: result += s[i];
    }
  }
  return result;
}
void appendUtf8(String &s, uint32_t cp) {
  if (cp < 128) s += char(cp);
  else if (cp < 2048) { s += char(0xc0 | (cp >> 6)); s += char(0x80 | (cp & 63)); }
  else if (cp < 65536) {
    s += char(0xe0 | (cp >> 12)); s += char(0x80 | ((cp >> 6) & 63)); s += char(0x80 | (cp & 63));
  } else {
    s += char(0xf0 | (cp >> 18)); s += char(0x80 | ((cp >> 12) & 63));
    s += char(0x80 | ((cp >> 6) & 63)); s += char(0x80 | (cp & 63));
  }
}
class Decoder {
public:
  FsFile &file;
  uint32_t cursor = 0, length;
  enum Encoding { Utf8, Gbk, Utf16LE, Utf16BE } encoding = Utf8;
  bool failed = false;
  explicit Decoder(FsFile &f) : file(f), length(f.fileSize()) {}
  int byte(uint32_t at) {
    if (at >= length) return -1;
    if (at < start || at >= start + used) {
      start = at;
      if (!file.seekSet(at)) { failed = true; return -1; }
      int n = file.read(buffer.data(), buffer.size());
      if (n <= 0) { failed = true; return -1; }
      used = n;
    }
    return buffer[at - start];
  }
  uint32_t next() {
    int a = byte(cursor++);
    if (a < 0) { failed = true; return 0xfffd; }
    if (encoding == Utf16LE || encoding == Utf16BE) {
      int b = byte(cursor++);
      if (b < 0) { failed = true; return 0xfffd; }
      uint32_t cp = encoding == Utf16LE ? a | (b << 8) : (a << 8) | b;
      if (cp >= 0xd800 && cp <= 0xdbff) {
        a = byte(cursor++); b = byte(cursor++);
        if (a < 0 || b < 0) { failed = true; return 0xfffd; }
        uint32_t tail = encoding == Utf16LE ? a | (b << 8) : (a << 8) | b;
        if (tail < 0xdc00 || tail > 0xdfff) { failed = true; return 0xfffd; }
        return 0x10000 + ((cp - 0xd800) << 10) + tail - 0xdc00;
      }
      if (cp >= 0xdc00 && cp <= 0xdfff) { failed = true; return 0xfffd; }
      return cp;
    }
    if (a < 128) return a;
    if (encoding == Gbk) {
      if (a == 0x80) return 0x20ac;
      int b = byte(cursor++);
      if (a < 0x81 || a > 0xfe || b < 0x40 || b > 0xfe || b == 0x7f) {
        failed = true; return 0xfffd;
      }
      uint16_t cp = gbkTable[(a - 0x81) * 190 + b - 0x40 - (b > 0x7f ? 1 : 0)];
      if (!cp) failed = true;
      return cp ? cp : 0xfffd;
    }
    unsigned n = a >= 0xc2 && a <= 0xdf ? 1 : a >= 0xe0 && a <= 0xef ? 2 : a >= 0xf0 && a <= 0xf4 ? 3 : 0;
    if (!n) { failed = true; return 0xfffd; }
    uint32_t cp = a & (0x7f >> n);
    for (unsigned i = 0; i < n; ++i) {
      int b = byte(cursor++);
      if (b < 0x80 || b > 0xbf) { failed = true; return 0xfffd; }
      cp = (cp << 6) | (b & 63);
    }
    if ((n == 1 && cp < 128) || (n == 2 && cp < 2048) || (n == 3 && cp < 65536) ||
        cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) failed = true;
    return cp;
  }
private:
  std::vector<uint8_t> buffer = std::vector<uint8_t>(4096);
  uint32_t start = UINT32_MAX, used = 0;
};
class ZipWriter {
  struct Entry { String name; uint32_t offset, crc, size; };
  std::vector<Entry> entries;
  FsFile &file;
  uint32_t crcTable[256];
  std::vector<uint8_t> buffer = std::vector<uint8_t>(4096);
  unsigned used = 0;
  bool ok = true;
  void u16(uint16_t n) { uint8_t b[] = {uint8_t(n), uint8_t(n >> 8)}; raw(b, 2); }
  void u32(uint32_t n) { u16(n); u16(n >> 16); }
  void raw(const void *p, size_t n) { if (file.write(p, n) != n) ok = false; }
  void flush() { if (used) raw(buffer.data(), used); used = 0; }
public:
  explicit ZipWriter(FsFile &f) : file(f) {
    for (unsigned i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (unsigned j = 0; j < 8; ++j) c = (c >> 1) ^ ((c & 1) ? 0xedb88320 : 0);
      crcTable[i] = c;
    }
  }
  void begin(const String &name) {
    entries.push_back({name, uint32_t(file.curPosition()), 0xffffffff, 0});
    u32(0x04034b50); u16(20); u16(0); u16(0); u16(0); u16(0);
    u32(0); u32(0); u32(0); u16(name.length()); u16(0); raw(name.c_str(), name.length());
  }
  void write(const String &s) {
    auto &e = entries.back();
    for (unsigned i = 0; i < s.length(); ++i) {
      uint8_t b = s[i]; e.crc = crcTable[(e.crc ^ b) & 255] ^ (e.crc >> 8);
      buffer[used++] = b;
      if (used == buffer.size()) flush();
    }
    e.size += s.length();
  }
  void end() {
    flush(); auto &e = entries.back(); e.crc ^= 0xffffffff;
    uint32_t at = file.curPosition();
    if (!file.seekSet(e.offset + 14)) ok = false;
    u32(e.crc); u32(e.size); u32(e.size);
    if (!file.seekSet(at)) ok = false;
  }
  void entry(const String &name, const String &content) { begin(name); write(content); end(); }
  bool finish() {
    uint32_t at = file.curPosition();
    for (const auto &e : entries) {
      u32(0x02014b50); u16(20); u16(20); u16(0); u16(0); u16(0); u16(0);
      u32(e.crc); u32(e.size); u32(e.size); u16(e.name.length()); u16(0); u16(0);
      u16(0); u16(0); u32(0); u32(e.offset); raw(e.name.c_str(), e.name.length());
    }
    uint32_t size = uint32_t(file.curPosition()) - at;
    u32(0x06054b50); u16(0); u16(0); u16(entries.size()); u16(entries.size());
    u32(size); u32(at); u16(0);
    return ok && file.sync();
  }
};
bool heading(const String &s) {
  if (s.length() > 180) return false;
  if (s.startsWith("第"))
    return s.indexOf("章") > 0 || s.indexOf("回") > 0 || s.indexOf("卷") > 0 || s.indexOf("节") > 0;
  String lower = s; lower.toLowerCase();
  return lower.startsWith("chapter ") || lower.startsWith("part ") || s == "序章" || s == "前言" || s == "后记";
}
}
bool importText(const String &path, const String &destination, const String &title,
                const std::function<bool(unsigned)> &progress, String &error) {
  if (SdMan.exists(destination.c_str())) return true;
  auto input = SdMan.open(path.c_str());
  if (!input || !input.fileSize() || input.fileSize() > 128 * 1024 * 1024) {
    error = "TXT is empty or exceeds 128 MiB"; return false;
  }
  Decoder decoder(input);
  uint32_t bom = 0;
  if (decoder.byte(0) == 0xff && decoder.byte(1) == 0xfe) { decoder.encoding = Decoder::Utf16LE; bom = 2; }
  else if (decoder.byte(0) == 0xfe && decoder.byte(1) == 0xff) { decoder.encoding = Decoder::Utf16BE; bom = 2; }
  else if (decoder.byte(0) == 0xef && decoder.byte(1) == 0xbb && decoder.byte(2) == 0xbf) bom = 3;
  // Validate the whole input so a late GBK character cannot change decoding halfway through.
  if (decoder.encoding == Decoder::Utf8) {
    decoder.cursor = bom;
    uint32_t tick = millis();
    while (decoder.cursor < decoder.length && !decoder.failed) {
      decoder.next();
      if (millis() - tick > 20) {
        tick = millis(); delay(1);
        if (!progress(unsigned(uint64_t(decoder.cursor) * 20 / decoder.length))) { error = "Import cancelled"; return false; }
      }
    }
    if (decoder.failed) {
      if (bom) { error = "Invalid UTF-8 TXT"; return false; }
      decoder.encoding = Decoder::Gbk; decoder.failed = false;
    }
  }
  String temporary = destination + ".tmp";
  auto output = SdMan.open(temporary.c_str(), O_RDWR | O_CREAT | O_TRUNC);
  if (!output) { error = "Cannot create TXT cache"; return false; }
  // Scope writer so every failure closes and discards the incomplete cache.
  bool success = [&]() {
    ZipWriter zip(output);
    zip.entry("mimetype", "application/epub+zip");
    zip.entry("META-INF/container.xml", "<?xml version=\"1.0\"?><container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\"><rootfiles><rootfile full-path=\"book.opf\" media-type=\"application/oebps-package+xml\"/></rootfiles></container>");
    zip.entry("book.opf", "<?xml version=\"1.0\"?><package xmlns=\"http://www.idpf.org/2007/opf\" version=\"2.0\" unique-identifier=\"id\"><metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><dc:identifier id=\"id\">easyread-txt</dc:identifier><dc:title>" + escape(title) + "</dc:title><dc:language>zh</dc:language></metadata><manifest><item id=\"body\" href=\"text.xhtml\" media-type=\"application/xhtml+xml\"/><item id=\"ncx\" href=\"toc.ncx\" media-type=\"application/x-dtbncx+xml\"/></manifest><spine toc=\"ncx\"><itemref idref=\"body\"/></spine></package>");
    zip.begin("text.xhtml");
    zip.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?><html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>" + escape(title) + "</title></head><body>");
    std::vector<String> toc;
    String line;
    line.reserve(4096);
    bool continued = false, skipLF = false;
    auto flushLine = [&](bool complete) {
      String trimmed = line; trimmed.trim();
      if (!continued) {
        if (complete && heading(trimmed) && toc.size() < 1000) {
          String id = "c" + String(toc.size()); toc.push_back(trimmed);
          zip.write("<h2 id=\"" + id + "\">" + escape(trimmed) + "</h2>");
          line = ""; return;
        }
        zip.write("<p>");
      }
      zip.write(escape(line));
      if (complete) zip.write(trimmed.isEmpty() && !continued ? "<br/></p>" : "</p>");
      continued = !complete; line = "";
    };
    decoder.cursor = bom;
    uint32_t tick = millis();
    while (decoder.cursor < decoder.length) {
      uint32_t cp = decoder.next();
      if (decoder.failed) { error = "Invalid or unsupported TXT encoding"; return false; }
      if (cp == '\n' && skipLF) { skipLF = false; continue; }
      skipLF = cp == '\r';
      if (cp == '\n' || cp == '\r') flushLine(true);
      else if (cp == '\t') line += "    ";
      else if (cp >= 32 && cp != 0xfffe && cp != 0xffff) appendUtf8(line, cp);
      if (line.length() >= 4096) flushLine(false);
      if (millis() - tick > 20) {
        tick = millis(); delay(1);
        if (!progress(20 + unsigned(uint64_t(decoder.cursor) * 80 / decoder.length))) { error = "Import cancelled"; return false; }
      }
    }
    if (line.length() || continued) flushLine(true);
    zip.write("</body></html>"); zip.end();
    zip.begin("toc.ncx");
    zip.write("<?xml version=\"1.0\"?><ncx xmlns=\"http://www.daisy.org/z3986/2005/ncx/\" version=\"2005-1\"><head><meta name=\"dtb:uid\" content=\"easyread-txt\"/></head><docTitle><text>" + escape(title) + "</text></docTitle><navMap>");
    if (toc.empty()) zip.write("<navPoint id=\"start\" playOrder=\"1\"><navLabel><text>" + escape(title) + "</text></navLabel><content src=\"text.xhtml\"/></navPoint>");
    for (unsigned i = 0; i < toc.size(); ++i)
      zip.write("<navPoint id=\"n" + String(i) + "\" playOrder=\"" + String(i + 1) + "\"><navLabel><text>" + escape(toc[i]) + "</text></navLabel><content src=\"text.xhtml#c" + String(i) + "\"/></navPoint>");
    zip.write("</navMap></ncx>"); zip.end();
    if (!zip.finish()) { error = "TXT cache write failed"; return false; }
    return true;
  }();
  input.close(); output.close();
  if (success && SdMan.replaceFile(temporary.c_str(), destination.c_str())) return true;
  SdMan.remove(temporary.c_str());
  if (error.isEmpty()) error = "TXT cache rename failed";
  return false;
}
}
