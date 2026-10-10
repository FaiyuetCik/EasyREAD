# SPDX-License-Identifier: MIT
"""Copy only the original 01–08 samples; preserve existing SD files."""
from pathlib import Path
import hashlib
import shutil
import json
import sys

base=Path(__file__).resolve().parents[1]
root=Path(sys.argv[1]).resolve()
assert (root/'Books').is_dir() and (root/'Fonts').is_dir(), 'Not the expected reading card'
source=base/'test-fixtures/formats'
samples=sorted(p for p in source.iterdir() if p.is_file() and p.name[:2].isdigit())
assert len(samples)==8
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
originals={str(p.relative_to(root)):digest(p) for p in root.rglob('*')
           if p.is_file() and 'System Volume Information' not in p.parts}
dest=root/'Books/EasyREAD-Tests'
files=samples+[source/'README.md']
assert shutil.disk_usage(root).free > sum(p.stat().st_size for p in files)+256*1024, 'Insufficient free space'
dest.mkdir(exist_ok=True)
for p in files:
    target=dest/p.name
    if target.exists():
        assert digest(target)==digest(p), f'Refusing to replace existing file: {target}'
    else: shutil.copy2(p,target)
    assert digest(p)==digest(target), f'Copy verification failed: {target}'
for relative,sha in originals.items(): assert digest(root/relative)==sha,f'Existing file changed: {relative}'
report={'destination':str(dest),'sample_books':8,'books_bytes':sum(p.stat().st_size for p in samples),
        'copied_sha256_verified':True,'existing_files_unchanged':len(originals),
        'free_bytes_after':shutil.disk_usage(root).free,'hardware_reading_verified':False}
out=base/'test-results/formats/sd-copy.json'
out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
