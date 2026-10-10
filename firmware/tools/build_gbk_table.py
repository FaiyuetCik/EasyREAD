# SPDX-License-Identifier: MIT
"""Generate GBK byte-pair to Unicode data using Python's standard codec.

The generated data retains the Python Software Foundation notice in
legal/Python-GBK/LICENSE.txt. No runtime Python dependency.
Re-run with Python 3 to reproduce firmware/src/GbkTable.h.
"""
from pathlib import Path

values = []
for lead in range(0x81, 0xFF):
    for trail in range(0x40, 0xFF):
        if trail == 0x7F:
            continue
        try:
            values.append(ord(bytes([lead, trail]).decode("gbk")))
        except UnicodeDecodeError:
            values.append(0)
target = Path(__file__).resolve().parents[1] / "src/GbkTable.h"
target.write_text(
    "// Generated mapping data by tools/build_gbk_table.py; CPython GBK codec.\n"
    "// Copyright (c) 2001-2023 Python Software Foundation. All rights reserved.\n"
    "// Data license: PSF-2.0; see legal/Python-GBK/LICENSE.txt.\n"
    "#pragma once\n#include <stdint.h>\nnamespace fm {\n"
    "inline constexpr uint16_t gbkTable[] = {\n"
    + "\n".join(",".join(f"0x{v:04x}" for v in values[i:i+16]) + "," for i in range(0, len(values), 16))
    + "\n};\n}\n", encoding="utf-8"
)
print(f"Generated {len(values)} GBK mappings")
