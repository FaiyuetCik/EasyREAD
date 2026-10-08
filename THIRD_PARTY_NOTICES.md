# Third-party notices

License review date: 2026-10-08. Target: Seeed Studio reTerminal Sticky.
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
Additional embedded FreeType/zlib/HarfBuzz/hash notices are retained in
`legal/FreeType-additional/`; source headers remain authoritative.

The SDK source and its notices are unchanged. Project-specific modifications
are in the application's own files. Full retained license texts, rather than
this summary table, govern third-party permissions and obligations.

This inventory covers the inspected application dependencies and vendored SDK
materials. It is not a complete redistribution audit of every precompiled
ESP-IDF/toolchain component. Before sharing binaries with others, retain the
exact corresponding library sources and build/link inputs and review all
linked components. In particular, LGPL static-link distribution can require
materials allowing recipients to relink with a modified library. The present
software-preview ZIP is a personal-use package, not a cleared public release.
