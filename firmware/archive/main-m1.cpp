#include <Arduino.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <SDCardManager.h>
#include <esp_rom_sys.h>

#include <cstring>
#include <vector>

#include "splash.h"

namespace {

EInkDisplay display(13, 14, 15, 16, 17, 18);
SDCardManager sd;
uint8_t panelFrame[FM_SPLASH_BYTES];

// The SDK's Sticky framebuffer is 800x480 in native panel coordinates. The
// product artwork is portrait 480x800. This switch stays configurable until a
// real Sticky confirms which rotation is upright in its housing.
void drawSplash() {
  static_assert(FM_SPLASH_WIDTH == 480 && FM_SPLASH_HEIGHT == 800);
  static_assert(FM_SPLASH_BYTES == 48000);
  memset(panelFrame, 0xFF, sizeof(panelFrame));
  for (uint16_t y = 0; y < FM_SPLASH_HEIGHT; ++y) {
    for (uint16_t x = 0; x < FM_SPLASH_WIDTH; ++x) {
      const uint32_t src = static_cast<uint32_t>(y) * (FM_SPLASH_WIDTH / 8) + x / 8;
      if (fmSplash[src] & (0x80 >> (x & 7))) continue;
#if FM_SPLASH_ROTATE_CW
      const uint16_t panelX = FM_SPLASH_HEIGHT - 1 - y;
      const uint16_t panelY = x;
#else
      const uint16_t panelX = y;
      const uint16_t panelY = FM_SPLASH_WIDTH - 1 - x;
#endif
      const uint32_t dst = static_cast<uint32_t>(panelY) * (FM_SPLASH_HEIGHT / 8) + panelX / 8;
      panelFrame[dst] &= static_cast<uint8_t>(~(0x80 >> (panelX & 7)));
    }
  }
  display.setFramebuffer(panelFrame);
  display.displayBuffer(EInkDisplay::FULL_REFRESH);
}

bool isEpub(const String& filename) {
  String lower = filename;
  lower.toLowerCase();
  return lower.endsWith(".epub");
}

void scanBooks() {
  if (!sd.begin()) {
    esp_rom_printf("[FM] microSD mount failed; insert a FAT32 card and reboot\n");
    return;
  }

  const char* directories[] = {"/Books", "/"};
  size_t count = 0;
  for (const char* directory : directories) {
    if (!sd.exists(directory)) continue;
    const std::vector<String> names = sd.listFiles(directory, 200);
    for (const String& name : names) {
      if (!isEpub(name)) continue;
      esp_rom_printf("[FM] EPUB: %s/%s\n", directory, name.c_str());
      ++count;
    }
  }
  esp_rom_printf("[FM] %u EPUB files found\n", static_cast<unsigned>(count));
}

}  // namespace

void setup() {
  BoardConfig::holdPowerRails();
  Serial.begin(115200);
  esp_rom_printf("[FM] Faiyuet Morgan boot\n");
  display.begin();
  drawSplash();
  scanBooks();
}

void loop() {
  delay(100);
}
