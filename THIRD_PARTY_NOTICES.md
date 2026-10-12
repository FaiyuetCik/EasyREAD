# Third-party notices

License review date: 2026-10-12. Target: Seeed Studio reTerminal Sticky.
The project-specific application uses FreeInk SDK; it must not be presented as
an entirely independently written display, EPUB, font or Bluetooth stack.

Windows starter tooling is distributed separately from the firmware: the MIT
setup UI invokes an independent esptool 5.1.0 executable. Its upstream source is
GPL-2.0-or-later; the r2 frozen executable elects GPL-3.0-or-later to accommodate
Apache-2.0 components, with GPLv3 text included. Complete registry Rust dependency
source archives and notices are collected from cryptography/tibs Cargo.lock files.
Python 3.12.10, Tcl/Tk 8.6.15, PyInstaller 6.20.0 (bootloader exception),
pyserial and other frozen dependencies retain their own licenses. Exact package
versions and corresponding source archives are included under `installer-sources/`
in the starter ZIP; `licenses/` contains full notices, including Python native
libraries and Microsoft runtime terms. The included firmware-materials ZIP
retains all firmware source/relink materials. See [installer README](installer/README.md).

| Component / source | Version or provenance | License / retained notices |
| --- | --- | --- |
| [FreeInk SDK](https://github.com/Free-Ink/freeink-sdk) | `a1cf5b4be3e5259f856fe0497129e360e4e1346d` | MIT, Copyright 2026 FreeInk; `legal/FreeInkSDK/LICENSE` and `NOTICE` |
| [freeink-reader](https://github.com/Free-Ink/freeink-reader) | Consulted reference; reviewed history through `ca1c53d04e7db0dfcafe7e9dab1414445b33e990` | MIT, Copyright (c) 2026 Justin Mitchell; full text in `legal/freeink-reader/LICENSE`; retained conservatively for similar storage/incremental-cache integration, separately from the SDK |
| [epub-to-xtc-converter](https://github.com/bigbag/epub-to-xtc-converter) format reference | Reviewed `docs/xtc-format-spec.md` at `a5719c6d4f64c52675b55889b18dae9877a9e9d6` | Upstream MIT, Copyright (c) 2025 Dave Allie; `legal/epub-to-xtc-converter/LICENSE`; reference-document credit, no converter binary linked |
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

The current 2026-10-10.2 EasyREAD sharing bundle is documented in `docs/SHARING.md`.
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
Use `EasyREAD-share-2026-10-10.2.zip` for this release; the 2026-10-08 and
2026-10-09 bundles remain historical. Future firmware releases need regenerated matching source,
notices, build/relink materials and verification; old results do not verify them.

## TXT support: GBK character mapping

`firmware/src/GbkTable.h` is generated from CPython 3.12.10's standard `gbk`
codec by the original MIT script `firmware/tools/build_gbk_table.py`.
The generated character mapping retains the Python Software Foundation
copyright and PSF license; the complete Python distribution license is retained
in `legal/Python-GBK/LICENSE.txt`. It is data embedded in the firmware;
the Python runtime and codec implementation are not embedded.
The TXT importer and XTC/XTCH/BMP parser are project application code under MIT,
except for separately licensed data and retained upstream notices. The 2026-10-12
review found no matching converter implementation blocks in the inspected scope;
this is not proof of independent authorship of every line.
XTC field definitions were consulted in the converter's public format documentation:
<https://github.com/bigbag/epub-to-xtc-converter/blob/main/docs/xtc-format-spec.md>.
The format document itself cites CrazyCoder's gist:
<https://gist.github.com/CrazyCoder/b125f26d6987c0620058249f59f1327d>.
That citation is retained as reference provenance; the upstream repository's MIT
license does not establish the rights to every independently hosted document.
EasyREAD does not redistribute that gist or the specification's prose/tables.

## Application provenance review / 应用代码来源核对

See `docs/CODE_PROVENANCE-2026-10-12.md` and the pinned source inventory and
candidate records under `docs/provenance/`. The review covers tracked application
sources and their reachable Git history against identified references. Small
storage-adapter matches and common SDK integration patterns do not establish
the entire application as a copy, nor does an absent match prove originality.
The freeink-reader copyright and MIT text are retained as a precaution for the
similar storage and incremental-cache integration. Do not replace these with
only the FreeInk SDK notice or only EasyREAD's own copyright.

The 2026-10-12 attribution supplement is attached to release `share-2026-10-10.2`.
Original firmware/starter archives predate it and retain their original hashes.
Redistributions should carry the attribution supplement alongside the original
matching source/relink package; future packages must include the updated notices.
This supplement does not replace corresponding sources or grant rights to books,
fonts, trademarks or unreviewed third-party material.

本项目基于 FreeInk SDK，开发中参考过 freeink-reader。对相似的存储与
增量缓存接入另行保留 Justin Mitchell 的 MIT 声明；这不是把整个项目认定为
上游改名版，也不是“所有其他代码均已证明原创”的声明。发布时同时携带本文件、
`LICENSE_SCOPE.md`、`legal/` 和匹配源码/重新链接材料。
