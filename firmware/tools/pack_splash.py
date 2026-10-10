"""Pack the portrait 1-bit splash into an ESP32 C++ header (white bit = 1)."""

from pathlib import Path
from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / "assets" / "branding" / "easyread-sticky-splash-480x800.png"
OUTPUT = Path(__file__).resolve().parents[1] / "src" / "splash.h"


def main() -> None:
    image = Image.open(ART).convert("1")
    if image.size != (480, 800):
        raise ValueError(f"Expected 480x800, got {image.size}")
    pixels = image.load()
    packed = bytearray()
    for y in range(image.height):
        for x in range(0, image.width, 8):
            packed.append(sum((1 << (7 - bit)) if pixels[x + bit, y] else 0
                              for bit in range(8)))
    if len(packed) != 48000:
        raise AssertionError(len(packed))
    lines = ["#pragma once", "#include <stdint.h>", "",
             "static constexpr uint16_t FM_SPLASH_WIDTH = 480;",
             "static constexpr uint16_t FM_SPLASH_HEIGHT = 800;",
             "static constexpr uint32_t FM_SPLASH_BYTES = 48000;",
             "static const uint8_t fmSplash[FM_SPLASH_BYTES] PROGMEM = {"]
    for offset in range(0, len(packed), 24):
        lines.append("  " + ", ".join(f"0x{b:02X}" for b in packed[offset:offset + 24]) + ",")
    lines.append("};")
    OUTPUT.write_text("\n".join(lines) + "\n", encoding="ascii")
    print(f"Wrote {OUTPUT} ({len(packed)} bytes packed)")


if __name__ == "__main__":
    main()
