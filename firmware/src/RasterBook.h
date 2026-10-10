// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include <BookStorage.h>
#include <functional>
#include <vector>

namespace fm {
class RasterBook {
public:
  struct Chapter { String name; uint32_t page; };
  bool open(freeink::book::BookSource &source, bool bmp, String &error);
  void close();
  bool render(uint32_t page, bool enlarged, int zoom, int &panX, int &panY,
              const std::function<void(int, int, bool)> &pixel, String &error);
  uint32_t pageCount() const { return count; }
  String title;
  std::vector<Chapter> chapters;
private:
  struct PageInfo { uint32_t offset, size; uint16_t width, height; };
  freeink::book::BookSource *source = nullptr;
  uint32_t count = 0, indexOffset = 0, bmpOffset = 0, bmpStride = 0;
  uint16_t bmpDepth = 0;
  uint32_t bmpWidth = 0, bmpHeight = 0;
  bool bitmap = false, topDown = false, lowPlaneFirst = false;
  std::vector<uint8_t> palette;
  bool pageInfo(uint32_t page, PageInfo &info);
};
}
