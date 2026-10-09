#pragma once
#include <algorithm>
#include <cstdint>
namespace fm {
struct PixelAddress {
  uint32_t byte;
  uint8_t mask;
};
inline PixelAddress address(int x, int y, bool clockwise) {
  // Match PageRenderer's Portrait / PortraitInverted transforms exactly.
  int px = clockwise ? y : 799 - y, py = clockwise ? 479 - x : x;
  return {uint32_t(py * 100 + px / 8), uint8_t(0x80 >> (px & 7))};
}
inline void touchToPortrait(float nx, float ny, bool clockwise, int &x,
                            int &y) {
  x = std::clamp(int((clockwise ? ny : 1 - ny) * 480), 0, 479);
  y = std::clamp(int((clockwise ? 1 - nx : nx) * 800), 0, 799);
}
} // namespace fm
