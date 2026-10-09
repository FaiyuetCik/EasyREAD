"""Build the embedded static UI font from OFL Noto Sans SC.

Run before embed_ui_font.py after adding UI text. Requires fonttools.
An optional first argument selects the source font; the default is the
project's sd-card/Fonts/NotoSansSC-VF.ttf. Renamed subset: FaiyuetUI.
"""
from pathlib import Path
import re
import sys
from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

firmware = Path(__file__).resolve().parents[1]
source = Path(sys.argv[1]) if len(sys.argv) > 1 else firmware.parent / "sd-card/Fonts/NotoSansSC-VF.ttf"
text = (firmware / "src/main.cpp").read_text(encoding="utf-8")
strings = re.findall(r'"(?:[^"\\]|\\.)*"', text)
chars = set(range(32, 127)) | {ord(c) for s in strings for c in s if ord(c) > 127}
font = TTFont(source)
missing = chars - font.getBestCmap().keys()
if missing:
    raise ValueError(f"Missing UI characters: {sorted(missing)}")
options = subset.Options()
options.layout_features = []
builder = subset.Subsetter(options=options)
builder.populate(unicodes=chars)
builder.subset(font)
font = instantiateVariableFont(font, {"wght": 400}, inplace=True)
for record in font["name"].names:
    if record.nameID in [1, 2, 3, 4, 6, 16, 17]:
        name = "Regular" if record.nameID in [2, 17] else "FaiyuetUI"
        record.string = name.encode(record.getEncoding())
font.save(firmware / "tools/ui-subset.otf")
print(f"Built FaiyuetUI with {len(chars)} characters; license: sd-card/Fonts/LICENSE.txt")
