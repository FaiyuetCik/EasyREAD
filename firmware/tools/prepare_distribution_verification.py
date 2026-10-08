from pathlib import Path
import json,shutil,hashlib
root=Path(__file__).resolve().parents[2];out=root/'distribution/content'
for folder in ['docs','firmware/tools']:
    for p in (root/folder).glob('*'):
        if p.is_file():shutil.copy2(p,out/folder/p.name)
for name in ['README.md','LICENSE.md','THIRD_PARTY_NOTICES.md','.gitignore']:shutil.copy2(root/name,out/name)
shutil.copy2(root/'docs/SHARING.md',out/'START_HERE.md')
for name,dest in [('distribution-vendor-check.txt','vendor-source-selection.txt'),('distribution-relink.txt','relink-check.txt')]:
    shutil.copy2(root/'firmware/test-results'/name,out/'verification'/dest)
capture=json.loads((out/'firmware/test-results/relink-link.json').read_text())
assert Path(capture['arduino_source']).resolve()==(out/'firmware/vendor/Arduino-ESP32').resolve()
original=Path('D:/fm-pio-core/packages/framework-arduinoespressif32')
checked=0
for p in original.rglob('*'):
    if p.is_file() and p.suffix in ['.cpp','.c','.h','.hpp','.S']:
        bundled=out/'firmware/vendor/Arduino-ESP32'/p.relative_to(original)
        assert p.read_bytes()==bundled.read_bytes(),str(p)
        checked+=1
log=(out/'verification/portable-build.txt').read_text()
assert '[SUCCESS]' in log
result={'result':'PASS','portable_build_verified':True,'bundled_arduino_selected':True,
        'arduino_source_files_compared':checked,'main_application_sha256':hashlib.sha256((out/'binaries/firmware.bin').read_bytes()).hexdigest(),
        'hardware_verified':False}
(out/'verification/source-build-check.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))

