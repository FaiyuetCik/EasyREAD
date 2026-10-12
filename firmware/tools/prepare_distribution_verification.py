from pathlib import Path
import argparse,json,shutil,hashlib
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser()
parser.add_argument('--package-root',type=Path,default=root/'distribution/content')
out=parser.parse_args().package_root.resolve()
for folder in ['docs','legal','firmware/tools']:
    for p in (root/folder).rglob('*'):
        if p.is_file() and '__pycache__' not in p.parts and not p.name.endswith(('.pyc','.partial')):
            destination=out/folder/p.relative_to(root/folder)
            destination.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(p,destination)
for name in ['README.md','README.en.md','STATE.md','LICENSE.md','LICENSE_SCOPE.md','THIRD_PARTY_NOTICES.md','.gitignore']:shutil.copy2(root/name,out/name)
(out/'START_HERE.md').write_text((root/'docs/SHARING.md').read_text(encoding='utf-8').replace('(../README.en.md)', '(README.en.md)'), encoding='utf-8')
capture=json.loads((out/'firmware/test-results/relink-link.json').read_text(encoding='utf-8'))
assert Path(capture['arduino_source']).resolve()==(out/'firmware/vendor/Arduino-ESP32').resolve()
original=Path('D:/fm-pio-core/packages/framework-arduinoespressif32')
checked=0
for p in original.rglob('*'):
    if p.is_file() and p.suffix in ['.cpp','.c','.h','.hpp','.S']:
        bundled=out/'firmware/vendor/Arduino-ESP32'/p.relative_to(original)
        assert p.read_bytes()==bundled.read_bytes(),str(p)
        checked+=1
log=(out/'verification/portable-build.txt').read_text(encoding='utf-8')
assert '[SUCCESS]' in log
for folder in ['src','tools','test-fixtures']:
    for p in (root/'firmware'/folder).rglob('*'):
        if p.is_file() and '__pycache__' not in p.parts and p.suffix != '.pyc':
            assert p.read_bytes()==(out/'firmware'/p.relative_to(root/'firmware')).read_bytes(),str(p)
result={'result':'PASS','portable_build_verified':True,'bundled_arduino_selected':True,
        'arduino_source_files_compared':checked,'main_application_sha256':hashlib.sha256((out/'binaries/firmware.bin').read_bytes()).hexdigest(),
        'portable_application_sha256':hashlib.sha256((out/'firmware/.pio/build/sticky/firmware.bin').read_bytes()).hexdigest(),
        'hardware_verified':False}
(out/'verification/source-build-check.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))

