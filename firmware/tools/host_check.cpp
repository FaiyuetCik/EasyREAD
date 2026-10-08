#include "../src/ReaderGeometry.h"
#include <FreeInkBook.h>
#include <FtFont.h>
#include <cache/PageCache.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <render/ImageRenderer.h>
#include <render/PageRenderer.h>
#include <stdexcept>
#include <vector>
using namespace freeink::book;
namespace fs = std::filesystem;
void check(bool ok, const char *why) {
  if (!ok)
    throw std::runtime_error(why);
}
class Source : public BookSource {
public:
  mutable std::ifstream f;
  uint64_t length;
  explicit Source(const char *p) : f(p, std::ios::binary) {
    check(bool(f), "source open");
    f.seekg(0, std::ios::end);
    length = f.tellg();
  }
  int32_t readAt(uint64_t at, void *dst, uint32_t n) override {
    f.clear();
    f.seekg(at);
    f.read(static_cast<char *>(dst), n);
    return f.gcount();
  }
  uint64_t size() const override { return length; }
};
unsigned long fontRead(void *p, unsigned long at, unsigned char *out,
                       unsigned long n) {
  return static_cast<Source *>(p)->readAt(at, out, n);
}
class Cache : public CacheStorage {
public:
  fs::path root;
  std::ofstream out;
  fs::path dest, tmp;
  explicit Cache(const char *p) : root(p) { fs::create_directories(root); }
  bool exists(const char *n) override { return fs::exists(root / n); }
  bool remove(const char *n) override {
    return !exists(n) || fs::remove(root / n);
  }
  int64_t fileSize(const char *n) override {
    return exists(n) ? int64_t(fs::file_size(root / n)) : -1;
  }
  int32_t readAt(const char *n, uint32_t at, void *dst, uint32_t len) override {
    std::ifstream f(root / n, std::ios::binary);
    if (!f)
      return -1;
    f.seekg(at);
    f.read(static_cast<char *>(dst), len);
    return f.gcount();
  }
  bool beginWrite(const char *n) override {
    dest = root / n;
    tmp = dest.string() + ".tmp";
    out.open(tmp, std::ios::binary | std::ios::trunc);
    return bool(out);
  }
  bool write(const void *p, uint32_t n) override {
    out.write(static_cast<const char *>(p), n);
    return bool(out);
  }
  bool endWrite() override {
    out.close();
    if (fs::exists(dest))
      fs::remove(dest);
    fs::rename(tmp, dest);
    return true;
  }
};
struct Memory {
  std::vector<uint8_t> bytes;
  Arena a;
  explicit Memory(size_t n) : bytes(n), a(bytes.data(), bytes.size()) {};
};
struct ZoomCheck {
  unsigned rows = 0;
};
bool imageRow(void *p, uint16_t, const uint8_t *, uint16_t width) {
  check(width > 0, "image width");
  ++static_cast<ZoomCheck *>(p)->rows;
  return true;
}
int main(int argc, char **argv) {
  try {
    check(argc == 5, "arguments: epub sc-font tc-font results");
    Source source(argv[1]), sc(argv[2]), tc(argv[3]);
    Cache cache(argv[4]);
    freeink::font::FtFont regular, bold, italic, traditional;
    check(regular.initStream(fontRead, &sc, sc.size(), 26),
          "SC streamed font init");
    check(traditional.initStream(fontRead, &tc, tc.size(), 26),
          "TC streamed font init");
    check(bold.initStream(fontRead, &sc, sc.size(), 26, 700), "bold font init");
    check(italic.initStream(fontRead, &sc, sc.size(), 26, 400, true),
          "italic font init");
    FontChain fonts;
    fonts.add(&regular);
    fonts.add(&traditional);
    fonts.add(&bold, 1);
    fonts.add(&italic, 2);
    for (uint32_t cp :
         {0x4e66u, 0x66f8u, 0x9605u, 0x95b1u, 0x8b80u, 0x6d4bu, 0x8a66u})
      check(fonts.covers(cp), "CJK coverage");
    Memory bookMem(768 * 1024), layout(512 * 1024), parse(256 * 1024),
        index(256 * 1024), page(256 * 1024), image(512 * 1024);
    Book book;
    check(book.open(source, bookMem.a, parse.a) == BookStatus::Ok, "EPUB open");
    check(book.spineCount() == 2, "spine");
    check(book.tocCount() == 3, "TOC");
    LayoutParams p;
    p.pageWidth = 480;
    p.pageHeight = 800;
    p.marginBottom = 66;
    p.baseSizePx = 26;
    p.lineSpacingPct = 125;
    p.font = &fonts;
    p.language = "zh";
    uint32_t savedChar = 0;
    bool sawImage = false, sawPng = false, sawJpeg = false;
    unsigned rendered = 0;
    for (unsigned size : {26u, 34u}) {
      p.baseSizePx = size;
      uint32_t hash = layoutGenerationHash(p, 123);
      for (uint16_t spine = 0; spine < 2; ++spine) {
        char name[64];
        pageCacheName(spine, hash, name, sizeof(name));
        layout.a.reset();
        parse.a.reset();
        index.a.reset();
        PageCacheWriter writer;
        check(writer.begin(cache, name, hash, index.a), "cache begin");
        auto *item = book.spineItem(spine);
        uint32_t chars = 0;
        auto status = ChapterLayout::layout(
            source, book.zip(), *book.zip().find(item->href), item->href, p,
            layout.a, writer, nullptr, &chars, &parse.a);
        check(status == BookStatus::Ok, "chapter layout");
        writer.setTotalChars(chars);
        check(writer.finish(), "cache finish");
        index.a.reset();
        PageCacheReader r;
        check(r.open(cache, name, hash, index.a) == BookStatus::Ok,
              "cache reopen");
        check(r.pageCount() > 0, "page count");
        if (spine == 0) {
          uint32_t anchor = 0;
          check(r.charForAnchor(ZipCatalog::hashPath("middle"), &anchor),
                "TOC fragment anchor");
          if (size == 26)
            savedChar = anchor;
          check(anchor == savedChar, "character anchor stable after relayout");
          uint32_t chosen = r.pageForChar(savedChar);
          check(r.charStart(chosen) <= savedChar, "resume before anchor");
          if (chosen + 1 < r.pageCount())
            check(r.charStart(chosen + 1) > savedChar, "resume correct page");
        }
        for (uint32_t i = 0; i < r.pageCount(); ++i) {
          page.a.reset();
          image.a.reset();
          Page pg{};
          check(r.readPage(i, page.a, &pg) == BookStatus::Ok, "page read");
          std::vector<uint8_t> buffer(48000, 255);
          FrameTarget target{buffer.data(), 480, 800, 60};
          check(PageRenderer::renderText(pg, fonts, target) == 0,
                "no missing glyphs");
          check(PageRenderer::renderImages(pg, source, book.zip(), image.a,
                                           target) == BookStatus::Ok,
                "image render");
          for (unsigned j = 0; j < pg.imageCount; ++j) {
            sawImage = true;
            sawPng |= std::string(pg.images[j].href).find(".png") !=
                      std::string::npos;
            sawJpeg |= std::string(pg.images[j].href).find(".jpg") !=
                       std::string::npos;
            ZoomCheck z;
            check(ImageRenderer::render(source, book.zip(), pg.images[j],
                                        image.a, imageRow,
                                        &z) == BookStatus::Ok &&
                      z.rows > 0,
                  "image streaming");
          }
          if (i == 0 || (spine == 0 && i == r.pageForChar(savedChar))) {
            std::ofstream out(fs::path(argv[4]) /
                                  ("page-" + std::to_string(size) + "-" +
                                   std::to_string(spine) + "-" +
                                   std::to_string(i) + ".pbm"),
                              std::ios::binary);
            out << "P4\n480 800\n";
            for (auto b : buffer)
              out.put(char(~b));
          }
          ++rendered;
        }
        std::cout << "PASS size=" << size << " spine=" << spine
                  << " pages=" << r.pageCount() << " chars=" << chars
                  << " layoutPeak=" << layout.a.highWater()
                  << " parsePeak=" << parse.a.highWater() << "\n";
      }
    }
    check(sawImage, "sample contains rendered images");
    check(sawPng && sawJpeg, "PNG and JPEG are both rendered");
    for (bool cw : {false, true}) {
      std::vector<uint8_t> visited(48000, 0);
      for (int y = 0; y < 800; ++y)
        for (int x = 0; x < 480; ++x) {
          auto a = fm::address(x, y, cw);
          check(a.byte < visited.size(), "rotated framebuffer bounds");
          check(!(visited[a.byte] & a.mask), "rotation is one-to-one");
          visited[a.byte] |= a.mask;
          // The digitizer supplies normalized panel coordinates; at pixel
          // centers its inverse must recover the logical pixel exactly.
          int px = cw ? 799 - y : y, py = cw ? x : 479 - x, tx, ty;
          fm::touchToPortrait((px + .5f) / 800, (py + .5f) / 480, cw, tx, ty);
          check(tx == x && ty == y, "touch and rendered coordinates agree");
        }
      for (auto byte : visited)
        check(byte == 255, "rotation covers entire framebuffer");
    }
    std::cout
        << "PASS both portrait rotations: 768000 pixel and touch mappings\n";
    for (const char *name : {"drm.epub", "truncated.epub", "garbage.epub"}) {
      auto file = fs::path(argv[1]).parent_path() / name;
      Source invalid(file.c_str());
      bookMem.a.reset();
      parse.a.reset();
      Book b;
      auto status = b.open(invalid, bookMem.a, parse.a);
      check(status != BookStatus::Ok, "invalid EPUB must be rejected");
      if (std::string(name) == "drm.epub")
        check(status == BookStatus::Encrypted, "DRM rejected explicitly");
      std::cout << "PASS rejects " << name << ": " << bookStatusName(status)
                << "\n";
    }
    std::cout << "PASS " << rendered
              << " pages; mixed Chinese/English, fonts, PNG, TOC, cache and "
                 "relayout resume\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << "\n";
    return 1;
  }
}
