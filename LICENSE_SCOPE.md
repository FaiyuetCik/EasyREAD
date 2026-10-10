# License scope / 许可范围

The MIT license in `LICENSE.md` applies to original EasyREAD application
code, scripts, documentation, original branding artwork and project-authored
test fixtures, except where a file or component carries another license.
Copyright attribution uses the project's existing Git author name, FaiyuetCik.

This grant does not relicense third-party material. Preserve its copyright,
license and attribution notices, including notices in source files. In particular:

- FreeInk SDK and its embedded dependencies retain their respective licenses;
  SDK-level MIT does not replace embedded components' licenses.
- Arduino-ESP32 core retains LGPL-2.1-or-later. Firmware containing it must meet
  the applicable distribution requirements, including a way to rebuild or relink
  with modified library code. The application's MIT license does not remove them.
- NimBLE-Arduino retains Apache-2.0 and its bundled notices; SdFat retains MIT.
- Noto font files in `sd-card/Fonts/`, the derived `firmware/tools/ui-subset.otf`
  and embedded font data in `firmware/src/ui_font.h` retain SIL OFL-1.1, including
  upstream copyright notices. They are not relicensed under the application MIT
  license. The original font-generation scripts are MIT; their font output is OFL.
- Other vendored code, Unicode data, test fonts, toolchain runtimes and binary
  components retain the terms recorded in `THIRD_PARTY_NOTICES.md` and `legal/`.
- The generated GBK mapping in `firmware/src/GbkTable.h` retains the Python
  Software Foundation copyright/license in `legal/Python-GBK/LICENSE.txt`;
  the original generator and text decoder are MIT application code.
- Windows setup UI and packaging scripts in `installer/` are MIT, except
  `esptool_entry.py`, which is GPL-2.0-or-later. The independently executed
  esptool program retains GPL-2.0-or-later. Frozen runtimes and dependencies
  retain their upstream terms; starter ZIPs include exact sources and licenses.
  Microsoft runtime binaries remain under Microsoft terms, for Windows only.

Windows Georgia font files are not included or licensed by this project.
`assets/branding/generate.py` uses an installed Windows font to render fixed logo
images; it does not grant permission to redistribute that font. SVG font-family
references do not embed the font. A compatible locally licensed font is required
to reproduce the artwork with that script.

Third-party books supplied by users are not covered by this license and must not
be included in releases without their rights holders' permission. The bundled
`Faiyuet-Morgan-test.epub` is project-authored test content.

This project grants no rights in third-party trademarks or endorsement by Seeed
Studio, FreeInk or other upstream authors. Full component license texts govern;
this scope document does not add restrictions to rights granted by MIT.

本项目的原创应用代码、脚本、文档、原创设计素材和原创测试样例采用 MIT，
另有许可标注的文件或组件除外。版权名使用现有 Git 作者名 FaiyuetCik。
第三方代码、字体、数据和二进制组件继续遵守其各自许可，保留原作者署名。
特别是 Noto 字体及其嵌入式子集仍为 OFL，Arduino 核心仍为 LGPL；
不能把包含这些内容的完整固件概括成“全部只有 MIT”。
发布固件的对应源码、构建与重新链接要求见 `docs/SHARING.md`，
公开前的检查事项见 `docs/OPEN_SOURCE.md`。
