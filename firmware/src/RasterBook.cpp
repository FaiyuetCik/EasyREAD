// SPDX-License-Identifier: MIT
#include "RasterBook.h"
#include <esp_heap_caps.h>
#include <algorithm>
#include <memory>

namespace fm {
namespace {
uint16_t u16(const uint8_t *p) { return p[0] | (uint16_t(p[1]) << 8); }
uint32_t u32(const uint8_t *p) { return u16(p) | (uint32_t(u16(p + 2)) << 16); }
uint64_t u64(const uint8_t *p) { return u32(p) | (uint64_t(u32(p + 4)) << 32); }
uint8_t gray(const uint8_t *bgr) { return (uint32_t(bgr[2]) * 77 + uint32_t(bgr[1]) * 150 + uint32_t(bgr[0]) * 29) >> 8; }
struct FreeBuffer { void operator()(uint8_t *p) const { heap_caps_free(p); } };
using Buffer = std::unique_ptr<uint8_t, FreeBuffer>;
Buffer buffer(size_t n) { return Buffer(static_cast<uint8_t *>(heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT))); }
}
void RasterBook::close() {
  source = nullptr; count = 0; title = ""; chapters.clear(); palette.clear(); lowPlaneFirst = false;
}
bool RasterBook::pageInfo(uint32_t page, PageInfo &info) {
  if (!source || page >= count) return false;
  if (bitmap) {
    info = {bmpOffset, bmpStride * bmpHeight, uint16_t(bmpWidth), uint16_t(bmpHeight)};
    return true;
  }
  uint8_t bytes[16];
  if (source->readAt(uint64_t(indexOffset) + uint64_t(page) * 16, bytes, sizeof(bytes)) != sizeof(bytes)) return false;
  uint64_t offset = u64(bytes);
  info = {uint32_t(offset), u32(bytes + 8), u16(bytes + 12), u16(bytes + 14)};
  return offset <= UINT32_MAX && info.size >= 22 && offset + info.size <= source->size() &&
         info.width && info.width <= 8192 && info.height && info.height <= 8192;
}
bool RasterBook::open(freeink::book::BookSource &input, bool bmp, String &error) {
  close(); source = &input; bitmap = bmp;
  uint8_t bytes[256]{};
  if (source->readAt(0, bytes, 54) != 54) { error = "Truncated image book"; return false; }
  if (bitmap) {
    uint32_t dib = u32(bytes + 14), compression = u32(bytes + 30);
    int32_t w = int32_t(u32(bytes + 18)), h = int32_t(u32(bytes + 22));
    bmpDepth = u16(bytes + 28); bmpOffset = u32(bytes + 10);
    if (bytes[0] != 'B' || bytes[1] != 'M' || dib < 40 || dib > 124 ||
        u16(bytes + 26) != 1 || compression != 0 || w <= 0 || w > 8192 ||
        h == 0 || h < -8192 || h > 8192 ||
        (bmpDepth != 1 && bmpDepth != 4 && bmpDepth != 8 && bmpDepth != 24 && bmpDepth != 32)) {
      error = "Unsupported BMP (use uncompressed 1/4/8/24/32-bit RGB)"; return false;
    }
    topDown = h < 0; bmpWidth = w; bmpHeight = topDown ? -h : h;
    bmpStride = ((bmpWidth * bmpDepth + 31) / 32) * 4;
    if (bmpOffset < 14 + dib || uint64_t(bmpOffset) + uint64_t(bmpStride) * bmpHeight > source->size()) {
      error = "Truncated BMP pixels"; return false;
    }
    if (bmpDepth <= 8) {
      uint32_t colors = u32(bytes + 46);
      if (!colors) colors = 1u << bmpDepth;
      if (colors > (1u << bmpDepth) || 14 + dib + colors * 4 > bmpOffset) { error = "Invalid BMP palette"; return false; }
      palette.resize(colors * 4);
      if (source->readAt(14 + dib, palette.data(), palette.size()) != int32_t(palette.size())) { error = "Cannot read BMP palette"; return false; }
    }
    count = 1;
    return true;
  }
  if (source->readAt(0, bytes, 56) != 56 || (u32(bytes) != 0x00435458 && u32(bytes) != 0x48435458)) {
    error = "Invalid XTC/XTCH header"; return false;
  }
  uint16_t version = u16(bytes + 4);
  count = u16(bytes + 6);
  uint64_t index = u64(bytes + 24), metadata = u64(bytes + 16), chapterOffset = u64(bytes + 48);
  bool hasMetadata = bytes[9] != 0, hasChapters = bytes[11] != 0;
  if ((version != 1 && version != 0x100) || !count || index > UINT32_MAX ||
      index < 56 || index + uint64_t(count) * 16 > source->size()) {
    error = "Unsupported or truncated XTC index"; return false;
  }
  indexOffset = uint32_t(index);
  if (hasMetadata) {
    if (metadata < 56 || metadata + 256 > source->size() || source->readAt(metadata, bytes, 256) != 256) {
      error = "Invalid XTC metadata"; return false;
    }
    char name[129]; memcpy(name, bytes, 128); name[128] = 0; title = name;
    unsigned chapterCount = u16(bytes + 246);
    // Some converters use an older metadata layout: timestamp at 192,
    // chapter count at 196, one-based chapter pages, and LSB plane first.
    // Recognize that layout from its fields rather than treating every v1
    // file alike (the public specification also uses version 1).
    bool oldMetadata = false;
    if (!chapterCount && version == 1) {
      unsigned oldCount = u16(bytes + 196);
      uint32_t oldTimestamp = u32(bytes + 192);
      bool unusedStandardFields = u32(bytes + 224) == 0 && u32(bytes + 240) == 0;
      bool oldChapterTable = hasChapters && oldCount && oldCount <= 2000 &&
          chapterOffset >= 56 && chapterOffset + uint64_t(oldCount) * 96 == index;
      oldMetadata = oldChapterTable || (!hasChapters && !oldCount && unusedStandardFields &&
          oldTimestamp >= 1000000000 && oldTimestamp <= 4102444800u);
      if (oldMetadata) chapterCount = oldCount;
    }
    lowPlaneFirst = oldMetadata;
    if (hasChapters && chapterCount) {
      if (chapterCount > 2000 || chapterOffset < 56 || chapterOffset + uint64_t(chapterCount) * 96 > source->size()) {
        error = "Invalid XTC contents"; return false;
      }
      for (unsigned i = 0; i < chapterCount; ++i) {
        if (source->readAt(chapterOffset + i * 96, bytes, 96) != 96) { error = "Cannot read XTC contents"; return false; }
        memcpy(name, bytes, 80); name[80] = 0;
        uint32_t page = u16(bytes + 80);
        if (oldMetadata) {
          if (!page) { error = "Invalid XTC chapter page"; return false; }
          --page;
        }
        if (page >= count) { error = "Invalid XTC chapter page"; return false; }
        chapters.push_back({String(name), page});
      }
    }
  }
  PageInfo info;
  if (!pageInfo(0, info)) { error = "Invalid XTC first page"; return false; }
  return true;
}
bool RasterBook::render(uint32_t page, bool enlarged, int zoom, int &panX, int &panY,
                        const std::function<void(int, int, bool)> &pixel, String &error) {
  PageInfo info;
  if (!pageInfo(page, info)) { error = "Invalid image page index"; return false; }
  Buffer data;
  bool xth = false;
  uint32_t plane = 0, stride = 0;
  if (bitmap) {
    data = buffer(bmpStride);
    if (!data) { error = "Not enough memory for BMP row"; return false; }
  } else {
    uint8_t h[22];
    if (source->readAt(info.offset, h, sizeof(h)) != sizeof(h)) { error = "Truncated XTC page"; return false; }
    xth = u32(h) == 0x00485458;
    uint32_t magic = u32(h), size = u32(h + 10);
    if ((magic != 0x00475458 && !xth) || h[9] != 0 || u16(h + 4) != info.width || u16(h + 6) != info.height) {
      error = "Unsupported XTC page encoding"; return false;
    }
    // XTG: horizontal rows. XTH: two vertical bit planes, columns right to left.
    stride = (info.width + 7) / 8;
    plane = uint32_t(info.width) * ((info.height + 7) / 8);
    uint32_t expected = xth ? plane * 2 : stride * info.height;
    if (size != expected || uint64_t(expected) + 22 > info.size || expected > 16 * 1024 * 1024) {
      error = "Invalid XTC bitmap size"; return false;
    }
    data = buffer(expected);
    if (!data || source->readAt(uint64_t(info.offset) + 22, data.get(), expected) != int32_t(expected)) {
      error = "Cannot read XTC bitmap"; return false;
    }
  }
  int top = enlarged ? 100 : 0, available = enlarged ? 650 : 748;
  float scale = std::min(480.0f / info.width, float(available) / info.height) * (enlarged ? zoom : 1);
  int w = std::max(1, int(info.width * scale)), h = std::max(1, int(info.height * scale));
  panX = std::clamp(panX, 0, std::max(0, w - 480)); panY = std::clamp(panY, 0, std::max(0, h - available));
  int left = std::max(0, (480 - w) / 2) - panX;
  int startY = top + std::max(0, (available - h) / 2) - panY;
  static const uint8_t bayer[4][4] = {{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
  int lastRow = -1;
  for (int y = std::max(top, startY); y < std::min(top + available, startY + h); ++y) {
    int sy = std::min(int(info.height) - 1, int(uint32_t(y - startY) * info.height / h));
    if (bitmap && lastRow != sy) {
      uint64_t at = uint64_t(bmpOffset) + uint64_t(topDown ? sy : bmpHeight - 1 - sy) * bmpStride;
      if (source->readAt(at, data.get(), bmpStride) != int32_t(bmpStride)) { error = "Cannot read BMP row"; return false; }
      lastRow = sy;
    }
    for (int x = std::max(0, left); x < std::min(480, left + w); ++x) {
      int sx = std::min(int(info.width) - 1, int(uint32_t(x - left) * info.width / w));
      uint8_t value;
      if (bitmap) {
        if (bmpDepth >= 24) value = gray(data.get() + sx * (bmpDepth / 8));
        else {
          unsigned idx = bmpDepth == 8 ? data.get()[sx] : bmpDepth == 4 ? (data.get()[sx / 2] >> ((sx & 1) ? 0 : 4)) & 15 : (data.get()[sx / 8] >> (7 - (sx & 7))) & 1;
          if (idx * 4 >= palette.size()) { error = "Invalid BMP palette index"; return false; }
          value = gray(palette.data() + idx * 4);
        }
      } else if (xth) {
        uint32_t at = uint32_t(info.width - 1 - sx) * ((info.height + 7) / 8) + sy / 8;
        uint8_t mask = 0x80 >> (sy & 7);
        unsigned level = ((data.get()[at] & mask) ? (lowPlaneFirst ? 1 : 2) : 0) |
                         ((data.get()[plane + at] & mask) ? (lowPlaneFirst ? 2 : 1) : 0);
        static const uint8_t levels[] = {255, 85, 170, 0}; value = levels[level];
      } else value = (data.get()[sy * stride + sx / 8] & (0x80 >> (sx & 7))) ? 255 : 0;
      pixel(x, y, value < bayer[y & 3][x & 3] * 16 + 8);
    }
    if ((y & 31) == 0) delay(1);
  }
  return true;
}
}
