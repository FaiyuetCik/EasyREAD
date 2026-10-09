"""Embed the OFL FaiyuetUI Chinese/ASCII subset built by build_ui_font.py."""
from pathlib import Path
base = Path(__file__).resolve().parents[1]
data = (base / 'tools/ui-subset.otf').read_bytes()
lines = ['#pragma once', '#include <stdint.h>', 'static const uint8_t fmUiFont[] = {']
lines += [','.join(f'0x{x:02x}' for x in data[i:i+24]) + ',' for i in range(0, len(data), 24)]
lines += ['};']
(base / 'src/ui_font.h').write_text('\n'.join(lines), encoding='utf-8')
print(f'Embedded {len(data)} font bytes')
