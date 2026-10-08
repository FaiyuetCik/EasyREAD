#pragma once
#include <algorithm>
#include <cstdint>
namespace fm {
struct PixelAddress {
  uint32_t byte;
  uint8_t mask;
};
inline PixelAddress address(int x, int y, bool clockwise) {
  int px = clockwise ? 799 - y : y, py = clockwise ? x : 479 - x;
  return {uint32_t(py * 100 + px / 8), uint8_t(0x80 >> (px & 7))};
}
inline void touchToPortrait(float nx, float ny, bool clockwise, int &x,
                            int &y) {
  x = std::clamp(int((clockwise ? ny : 1 - ny) * 480), 0, 479);
  y = std::clamp(int((clockwise ? 1 - nx : nx) * 800), 0, 799);
}
} // namespace fm
