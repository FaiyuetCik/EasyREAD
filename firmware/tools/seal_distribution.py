"""Seal only after relink/build verification. No flashing or GitHub mutations."""
from pathlib import Path
import json,hashlib,zipfile
root=Path(__file__).resolve().parents[2];out=root/'distribution/content'
verification=json.loads((out/'relinked/verification.json').read_text())
assert verification['result']=='PASS' and verification['matches_original_elf']
assert (out/'dependency-sources/sources.json').exists(), 'Dependency source retrieval is incomplete'
source_check=json.loads((out/'verification/source-build-check.json').read_text())
assert source_check['portable_build_verified'] and source_check['bundled_arduino_selected']
app=(out/'binaries/firmware.bin').read_bytes()
assert (out/'binaries/full.bin').read_bytes()[0x10000:]==app, 'Merged image differs from application'
def included(p):
    relative=p.relative_to(out)
    return (p.is_file() and not p.name.endswith('.partial') and p.name not in ['manifest.json','.piopm'] and
            not any(x in relative.parts for x in ['.pio','__pycache__','test-results']) and
            (relative.parts[0]!='relinked' or p.name=='verification.json'))
files={p.relative_to(out).as_posix():{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in out.rglob('*') if included(p)}
manifest={'target':'Seeed reTerminal Sticky ESP32-S3 16MB/8MB OPI','hardware_verified':False,
          'relink_verified':True,'portable_build_verified':True,'original_elf_sha256':verification['original_elf_sha256'],'files':files}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
dest=root/'distribution/Faiyuet-Morgan-share-2026-10-08.zip'
with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
    for p in sorted(out.rglob('*')):
        if included(p) or p==out/'manifest.json':archive.write(p,p.relative_to(out).as_posix())
with zipfile.ZipFile(dest) as archive:
    assert archive.testzip() is None
    assert set(archive.namelist())==set(files)|{'manifest.json'}
    for name,expected in files.items():
        data=archive.read(name)
        assert len(data)==expected['bytes'] and hashlib.sha256(data).hexdigest()==expected['sha256'],name
report={'result':'PASS','files':len(files),'bytes':dest.stat().st_size,'sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'relink_verified':True,'portable_build_verified':True,'hardware_verified':False}
(root/'distribution/verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
