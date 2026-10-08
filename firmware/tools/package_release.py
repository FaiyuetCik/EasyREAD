"""Package local software preview; never flashes a connected device."""
from pathlib import Path
import sys, json, hashlib, zipfile, shutil
base=Path(__file__).resolve().parents[1]
root=base.parent
artifact=base/'artifacts/software-preview-2026-10-08'
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
build=Path('D:/fm-pio-build/sticky')
app=(artifact/'faiyuet-morgan-sticky-app.bin').read_bytes()
full=(artifact/'faiyuet-morgan-sticky-full.bin').read_bytes()
assert full[0x10000:]==app, 'Merged image application must match verified app'
assert full[0x8000:0x8000+3072]==(build/'partitions.bin').read_bytes()
assert (build/'firmware.bin').read_bytes()==app
source_files=list((base/'src').glob('*'))+[base/'platformio.ini']
assert (build/'firmware.bin').stat().st_mtime>=max(p.stat().st_mtime for p in source_files),'Rebuild changed source/config'
files=[]
for p in artifact.glob('*.bin'):files.append((p,p.name))
for p in (root/'sd-card').rglob('*'):
    if p.is_file():files.append((p,'sd-card/'+str(p.relative_to(root/'sd-card')).replace('\\','/')))
for name in ['USER_GUIDE.md','VALIDATION.md','STATE.md']:files.append((root/'docs'/name,name))
files.append((root/'docs/GITHUB_PRIVATE.md','GITHUB_PRIVATE.md'))
for name in ['LICENSE.md','THIRD_PARTY_NOTICES.md']:
    files.append((root/name,name))
for p in (root/'legal').rglob('*'):
    if p.is_file():files.append((p,'legal/'+str(p.relative_to(root/'legal')).replace('\\','/')))
for name in ['reading-check.txt','ble-check.txt','build-final.txt','reading-preview.png']:
    files.append((base/'test-results'/name,'verification/'+name))
for label,folder in [('FreeInkSDK',base/'freeink-sdk'),('NimBLE-Arduino',Path('D:/fm-pio-libdeps/sticky/NimBLE-Arduino'))]:
    for p in folder.rglob('*'):
        if p.is_file() and (p.name.upper().startswith(('LICENSE','COPYING','NOTICE')) or p.name=='FTL.TXT'):
            files.append((p,'licenses/'+label+'/'+str(p.relative_to(folder)).replace('\\','/')))
manifest={'date':'2026-10-08','status':'software_preview_not_hardware_verified','chip':'ESP32-S3','flash':'16MB','psram':'8MB OPI','sdk':'a1cf5b4be3e5259f856fe0497129e360e4e1346d','nimble':'2.5.1','files':{name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p,name in files}}
manifest['source_files']={str(p.relative_to(base)).replace('\\','/'):{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in source_files}
assert len(files)==len({name for _,name in files}), 'Duplicate archive names'
(artifact/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
dest=out/'Faiyuet-Morgan-software-preview-2026-10-08.zip'
with zipfile.ZipFile(dest,'w',compression=zipfile.ZIP_DEFLATED) as z:
    for p,name in files:z.write(p,name)
    z.write(artifact/'manifest.json','manifest.json')
with zipfile.ZipFile(dest) as z:assert z.testzip() is None
shutil.copy2(root/'docs/USER_GUIDE.md',out/'使用说明.md')
shutil.copy2(root/'docs/STATE.md',out/'项目进度.md')
shutil.copy2(base/'test-results/reading-preview.png',out/'阅读预览.png')
print(json.dumps({'package':str(dest),'bytes':dest.stat().st_size,'sha256':hashlib.sha256(dest.read_bytes()).hexdigest()},indent=2))
