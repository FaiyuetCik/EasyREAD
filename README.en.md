# EasyREAD

[简体中文](README.md) | **English**

An offline reader for **Seeed Studio reTerminal Sticky**. Copy books to a microSD card and read using the touchscreen or physical buttons. No Wi-Fi or account is required.

![EasyREAD](assets/branding/easyread-logo-512.png)

## Features

- Portrait reading at 480 × 800; DRM-free EPUB files.
- TXT, XTC, XTCH and BMP support; 74 automated checks and TXT pagination/resume regression passed. Full device acceptance remains incomplete.
- Simplified Chinese, Traditional Chinese and English interface languages, with mixed Chinese/English book content.
- Bookshelf, table of contents, chapter navigation, saved reading position and resume on startup.
- Per-book font size, line spacing, margins and regional Chinese glyph preference. Device-wide language, sleep timer and Bluetooth settings.
- PNG/JPEG images and covers with zoom and panning.
- Touch and physical button controls. BLE HID page turner support is implemented; hardware compatibility still needs full verification.

## Quick start

**Windows users: three steps to start reading.**

1. **Download and extract** the [Windows starter ZIP](https://github.com/FaiyuetCik/EasyREAD/releases/download/share-2026-10-10.1/EasyREAD-Windows-Starter-2026-10-10.1-r3.zip) completely.
2. **Prepare the card**: copy **Fonts and Books** inside `SD-Card` to the root of a **FAT32** microSD card. Sample books are included.
3. **Click Install**: connect Sticky with a USB data cable, open **EasyREAD-Setup.exe**, confirm the device and click **Install EasyREAD**. When finished, insert the card and start reading.

Windows 10/11 x64; the runtime is included. No Python installation or commands required. The setup UI supports Chinese / English. Put your own books in `Books` on the card.

**Install EasyREAD** resets device settings and reading positions; books on the SD card are unchanged. For future updates to an existing installation, open **Advanced options** and select **Keep settings**; setup checks the existing layout and stops on a mismatch. Keep USB connected during installation. If no device appears, check the data cable and USB driver. The executable is unsigned; download from this project and verify the Release checksum.

For other systems or manual flashing, see [advanced installation and building](docs/SHARING.md) (Chinese). The starter includes corresponding sources, licenses and relink materials. Firmware images are unchanged.

## Controls

| Screen | Touchscreen | Physical buttons |
| --- | --- | --- |
| Bookshelf and menus | Tap once to move the black selection bar; tap the same item again to execute. Swipe vertically to scroll long lists | Up/down to select; short press AI to confirm |
| Reading | Tap left/right to turn pages. Tap the center for the reader menu; tapping a central image opens the image viewer | Up: previous page; down: next page; AI: reader menu |
| Image viewer | Tap the center for the image menu. Tap an item to select, then tap it again to execute. Swipe to pan | AI: image menu; direction buttons: pan |
| Back | Select the menu item naming the return destination, then tap again | Select the return item and press AI |

A fast double-tap is not required. The footer only displays hints; tapping it does not go back. Hold the AI/power button to sleep; press it to wake.

See the [user guide](docs/USER_GUIDE.md) (Chinese) for detailed instructions.

## Current version and limitations

**2026-10-10.1 development preview**: TXT, XTC, XTCH and BMP support, eight original sample books, and corrected Bluetooth/language/sleep adjustment titles. Includes three interface languages, per-book settings and two-tap menu interaction.

The current application has been built and flashed to the development board. SD mounting, Chinese text, basic page turns and selected menu transitions have hardware records. Complete language and touch coverage, BLE pairing/reconnection, sleep/wake, power consumption and long-term stability still need verification. An occasional startup screen during chapter changes has been reported; its cause remains unresolved.

This version supports DRM-free EPUB and the following additional formats:

| Format | Scope |
| --- | --- |
| TXT | UTF-8, GBK and BOM-marked UTF-16 LE/BE; common chapter heading detection, text layout settings and saved position; up to 128 MiB per file |
| XTC / XTCH | Uncompressed XTG monochrome and XTH dual-plane grayscale pages; proportional display, page turns, embedded contents, saved position and zoom; grayscale is dithered to monochrome |
| BMP | Uncompressed RGB, 1/4/8/24/32-bit; proportional display and zoom; each image is one bookshelf entry |

The first TXT open creates a disposable reading cache in `/FM`, requiring free space and conversion time. Originals are unchanged. XTC/XTCH/BMP contain fixed pixels, so text size and line spacing cannot be changed. Image dimensions are limited to 8192 pixels per side; XTC page decoding also depends on available PSRAM. Compressed XTC pages and RLE/bitfields BMP are unsupported.

SVG, GIF, WebP, interlaced PNG, complex tables, mathematical layout and full CSS support are outside the supported scope. There is no Wi-Fi book transfer or landscape mode.

[Project status](STATE.md) · [Validation scope](docs/VALIDATION.md) · [Release package guide](docs/SHARING.md) (Chinese)

See the [format test report](docs/FORMAT_TESTS-2026-10-10.md) (Chinese). Eight original samples are included in `sd-card/Books/EasyREAD-Tests`. The older `2026-10-10` package does not contain the new formats.

## Build and modify

The complete sharing package includes pinned dependency sources and a portable configuration:

```powershell
python -m pip install platformio==6.2.0
cd firmware
python -m platformio run -c platformio-share.ini -e sticky
cd ..
```

The first toolchain/platform installation needs internet access. Use a short extraction path on Windows. To build from the Git repository, restore the pinned FreeInk SDK first; see [dependency setup](docs/GITHUB_PRIVATE.md) (Chinese).

## Licenses and attribution

- Original application code, scripts, documentation and original artwork are [MIT licensed](LICENSE.md), attributed to FaiyuetCik. See [LICENSE_SCOPE.md](LICENSE_SCOPE.md) for scope and exceptions.
- EasyREAD is based on **FreeInk SDK**. Third-party copyrights, licenses and NOTICE files are retained in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and [legal/](legal/).
- The Arduino-ESP32 core remains **LGPL-2.1-or-later**. The complete package includes the corresponding library source, actual link inputs and relinking instructions.
- Noto fonts and the derived FaiyuetUI font remain **SIL OFL-1.1**, separate from the application's MIT license. Upstream copyright and full license texts accompany them.
- Windows Georgia font files are not distributed. Branding uses fixed text rendered with a locally installed font.

When distributing firmware, provide its matching source, notices and build/relink materials. Historical `Faiyuet-Morgan` packages correspond only to their historical versions. Checksums accompany the release assets.

The repository is now named EasyREAD. The local directory and historical assets retain their previous names. The repository remains private; access requires authorization.
