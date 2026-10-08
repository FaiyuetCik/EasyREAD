"""Check every packaged file against its hash and the project's current copy."""
from pathlib import Path, PurePosixPath
import sys, json, hashlib, zipfile
base=Path(__file__).resolve().parents[1];root=base.parent
artifact=base/'artifacts/software-preview-2026-10-08'
release=root/'releases/software-preview-2026-10-08'
package=release/'Faiyuet-Morgan-software-preview-2026-10-08.zip'
def sha(data):return hashlib.sha256(data).hexdigest()
def current(name):
    parts=PurePosixPath(name).parts
    assert '..' not in parts and not PurePosixPath(name).is_absolute()
    if name.endswith('.bin'):return artifact/name
    if parts[0]=='sd-card':return root/name
    if parts[0]=='legal' or name in ['LICENSE.md','THIRD_PARTY_NOTICES.md']:return root/name
    if parts[0]=='verification':return base/'test-results'/Path(*parts[1:])
    if parts[0]=='licenses':
        folder=base/'freeink-sdk' if parts[1]=='FreeInkSDK' else Path('D:/fm-pio-libdeps/sticky/NimBLE-Arduino')
        return folder/Path(*parts[2:])
    return root/'docs'/name
with zipfile.ZipFile(package) as z:
    assert z.testzip() is None,'ZIP CRC failed'
    manifest=json.loads(z.read('manifest.json'))
    assert set(z.namelist())==set(manifest['files'])|{'manifest.json'},'Unlisted/missing ZIP contents'
    for name,info in manifest['files'].items():
        data=z.read(name);assert len(data)==info['bytes'] and sha(data)==info['sha256'],name
        assert current(name).read_bytes()==data,'Project differs: '+name
    for name,info in manifest['source_files'].items():
        data=(base/name).read_bytes();assert len(data)==info['bytes'] and sha(data)==info['sha256'],'Source differs: '+name
    for name,origin in [('使用说明.md',root/'docs/USER_GUIDE.md'),('项目进度.md',root/'docs/STATE.md'),('阅读预览.png',base/'test-results/reading-preview.png')]:
        assert (release/name).read_bytes()==origin.read_bytes(),'Release copy differs: '+name
    app=z.read('faiyuet-morgan-sticky-app.bin');full=z.read('faiyuet-morgan-sticky-full.bin')
    assert full[0x10000:]==app,'Merged image mismatch'
    assert app==Path('D:/fm-pio-build/sticky/firmware.bin').read_bytes(),'Build artifact mismatch'
    for font in ['NotoSansSC-VF.ttf','NotoSansTC-VF.ttf']:
        assert z.read('sd-card/Fonts/'+font)[:4]==b'\x00\x01\x00\x00','Unsupported font format'
report={'result':'PASS','packaged_files_checked':len(manifest['files']),'source_files_checked':len(manifest['source_files']),'package_sha256':sha(package.read_bytes()),'application_sha256':sha(app),'full_image_sha256':sha(full),'hardware_verified':False}
(base/'test-results/recheck-package.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
