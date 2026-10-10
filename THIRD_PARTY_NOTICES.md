# Third-party notices

License review date: 2026-10-10. Target: Seeed Studio reTerminal Sticky.
The project-specific application uses FreeInk SDK; it must not be presented as
an entirely independently written display, EPUB, font or Bluetooth stack.

| Component / source | Version or provenance | License / retained notices |
| --- | --- | --- |
| [FreeInk SDK](https://github.com/Free-Ink/freeink-sdk) | `a1cf5b4be3e5259f856fe0497129e360e4e1346d` | MIT, Copyright 2026 FreeInk; `legal/FreeInkSDK/LICENSE` and `NOTICE` |
| OpenX4 display driver / waveforms and community ports | Attribution retained by SDK NOTICE | MIT; Copyright 2025 Open X4 E-Paper Contributors; credit CidVonHighwind |
| [Arduino-ESP32](https://github.com/espressif/arduino-esp32/tree/3.3.7) | 3.3.7 | LGPL-2.1-or-later core; `legal/Arduino-ESP32/LICENSE.md`; bundled components can have other licenses |
| [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino/tree/2.5.1) | 2.5.1 | Apache-2.0; upstream LICENSE and NOTICE, including Ryan Powell, Neil Kolban and bundled notices, retained in `legal/NimBLE-Arduino/` |
| [SdFat](https://github.com/greiman/SdFat/tree/2.3.1) | 2.3.1, SDK dependency | MIT, Bill Greiman; `legal/SdFat/` |
| FreeType | SDK vendored copy | Choose FreeType License (FTL), not the alternative GPL; `legal/FreeInkSDK/libs/font/FreeInkFont/third_party/freetype/` |
| stb_truetype | SDK vendored copy | Choose MIT alternative, Copyright 2017 Sean Barrett; `legal/stb_truetype/` |
| miniz | SDK vendored copies | MIT; both SDK copies' licenses retained |
| Expat | SDK vendored copy | MIT; upstream COPYING retained |
| pngle | SDK vendored copy | MIT, Copyright 2019 kikuchan; upstream LICENSE retained |
| TJpgDec | R0.03, SDK vendored copy | ChaN 2021 permissive custom terms; `legal/TJpgDec/` |
| libunibreak | SDK vendored copy | zlib-style terms, Wu Yongwei and Petr Filipsky; `legal/libunibreak/` |
| Unicode data | SDK line-break data | Unicode license text in `legal/Unicode/`; preserve source notices |
| Hyphenation patterns / DejaVu test font | Included in SDK, not supplied as application reading fonts | Their own upstream notices retained; inclusion here does not mean the runtime loads these assets |
| [Noto Sans SC / TC](https://github.com/notofonts/noto-cjk) | Variable TTF; SC-derived embedded UI subset | SIL Open Font License 1.1; `sd-card/Fonts/LICENSE.txt` and `legal/Noto-CJK/OFL.txt`; subsets remain font material under OFL |

Portions of this software are copyright © 1996–2024 The FreeType Project
(https://www.freetype.org). All rights reserved. FreeType is used under the FTL.
This software is based in part on the work of the FreeType Team.
Additional embedded FreeType/zlib/HarfBuzz/hash notices are retained in
`legal/FreeType-additional/`; source headers remain authoritative.

The SDK source and its notices are unchanged. Project-specific modifications
are in the application's own files. Full retained license texts, rather than
this summary table, govern third-party permissions and obligations.

The application's MIT grant is limited by `LICENSE_SCOPE.md`; it does not
replace any third-party license. Noto-derived `firmware/src/ui_font.h` and
`firmware/tools/ui-subset.otf` remain OFL font material, not MIT application code.
Windows Georgia font files used by the branding generator are not distributed.

The current 2026-10-10.1 EasyREAD sharing bundle is documented in `docs/SHARING.md`.
It includes the current application and MIT scope updates, together with the
actual Arduino library source, application sources, original link objects,
selected ESP-IDF archives, linker scripts and verified relinking instructions.
ESP-IDF source provenance is commit `87912cd291d68f4319f13695718af6754879a83f`;
its main source archive and supporting submodule archives accompany the bundle.
ESP-IDF's Apache-2.0 and embedded component notices are retained in
`legal/source-archives/` inside the bundle. Radio binary components' upstream
Apache-2.0 texts are retained in `legal/ESP-IDF-radio/`.
The managed esp_diagnostics 1.2.1 source ZIP is verified against the component
registry's individual file checksums (Apache-2.0). Toolchain runtime notices,
GCC Runtime Library Exception and Newlib notices accompany the firmware;
compiler executables are installed separately, not redistributed in this bundle.
The older software-preview ZIP lacks these complete relinking materials.
Use `EasyREAD-share-2026-10-10.1.zip` for this release; the 2026-10-08 and
2026-10-09 bundles remain historical. Future firmware releases need regenerated matching source,
notices, build/relink materials and verification; old results do not verify them.

## TXT support: GBK character mapping

`firmware/src/GbkTable.h` is generated from CPython 3.12.10's standard `gbk`
codec by the original MIT script `firmware/tools/build_gbk_table.py`.
The generated character mapping retains the Python Software Foundation
copyright and PSF license; the complete Python distribution license is retained
in `legal/Python-GBK/LICENSE.txt`. It is data embedded in the firmware;
the Python runtime and codec implementation are not embedded.
The new TXT importer and XTC/XTCH/BMP parser are original MIT application code.
XTC field definitions were consulted in the converter's public format documentation:
<https://github.com/bigbag/epub-to-xtc-converter/blob/main/docs/xtc-format-spec.md>.
No converter or other reader implementation is incorporated.
