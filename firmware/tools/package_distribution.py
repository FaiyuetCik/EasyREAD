"""Build a source-and-relink sharing bundle from verified local build inputs.
Run fetch_distribution_sources.py first, then PlatformIO build with export_relink.py.
"""
from pathlib import Path
import json, hashlib, shutil, re, zipfile, subprocess, tarfile
base=Path(__file__).resolve().parents[1];root=base.parent
build=Path('D:/fm-pio-build/sticky')
core=Path('D:/fm-pio-core/packages')
cache=root/'.distribution-cache'
out=root/'distribution/content';out.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def copy(p,dest):
    dest=out/dest;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest)
def tree(folder,prefix):
    for p in folder.rglob('*'):
        relative=p.relative_to(folder)
        if not p.is_file() or p.name=='.piopm' or p.name.endswith('.partial') or any(x in relative.parts for x in ['.git','.venv','.pio','__pycache__','test-results','artifacts','freeink-sdk']):continue
        if p.suffix.lower() in ['.exe','.dll','.so','.o','.pyc']:continue
        copy(p,Path(prefix)/relative)
for name in ['README.md','LICENSE.md','THIRD_PARTY_NOTICES.md','.gitignore']:copy(root/name,name)
for name in ['docs','legal','assets/branding']:tree(root/name,name)
for name in ['src','tools','test-fixtures']:tree(base/name,'firmware/'+name)
for name in ['platformio.ini','dependencies.lock.json','README.md']:copy(base/name,'firmware/'+name)
tree(base/'freeink-sdk','firmware/freeink-sdk')
tree(core/'framework-arduinoespressif32','firmware/vendor/Arduino-ESP32')
for name in ['NimBLE-Arduino','SdFat']:tree(Path('D:/fm-pio-libdeps/sticky')/name,'firmware/vendor/'+name)
copy(root/'legal/Arduino-ESP32/LICENSE.md','firmware/vendor/Arduino-ESP32/LICENSE.md')
tree(root/'sd-card/Fonts','sd-card/Fonts')
copy(root/'sd-card/Books/Faiyuet-Morgan-test.epub','sd-card/Books/Faiyuet-Morgan-test.epub')
for source,label in [(core/'toolchain-xtensa-esp-elf/share/licenses/gcc','GCC-runtime'),
                     (core/'toolchain-xtensa-esp-elf/share/licenses/newlib','newlib')]:
    for p in source.rglob('*'):
        if p.is_file() and (p.name.startswith(('COPYING','LICENSE'))):copy(p,'legal/'+label+'/'+p.relative_to(source).as_posix())
sources=json.loads((cache/'sources.json').read_text()) if (cache/'sources.json').exists() else []
for record in sources:
    if 'archive' in record:
        p=cache/record['archive'];assert sha(p)==record['sha256'];copy(p,'dependency-sources/'+p.name)
        # Make source archive license files immediately visible without extraction.
        with tarfile.open(p) as archive:
            for member in archive.getmembers():
                if member.isfile() and Path(member.name).name.upper().startswith(('LICENSE','COPYING','NOTICE')):
                    dest=out/'legal/source-archives'/p.name/Path(*Path(member.name).parts[1:])
                    dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(archive.extractfile(member).read())
if sources:copy(cache/'sources.json','dependency-sources/sources.json')
diagnostics=cache/'esp_diagnostics-1.2.1.zip'
checks=json.loads((cache/'esp_diagnostics-CHECKSUMS.json').read_text())
with zipfile.ZipFile(diagnostics) as archive:
    for entry in checks['files']:
        data=archive.read(entry['path'])
        assert len(data)==entry['size'] and hashlib.sha256(data).hexdigest()==entry['hash'],entry['path']
    license_path=out/'legal/esp_diagnostics/LICENSE';license_path.parent.mkdir(parents=True,exist_ok=True)
    license_path.write_bytes(archive.read('LICENSE'))
copy(diagnostics,'dependency-sources/'+diagnostics.name)
copy(cache/'esp_diagnostics-CHECKSUMS.json','dependency-sources/esp_diagnostics-CHECKSUMS.json')
portable=(base/'platformio.ini').read_text()
portable=portable.replace('build_dir = D:\\fm-pio-build','build_dir = .pio/build').replace('libdeps_dir = D:\\fm-pio-libdeps','libdeps_dir = .pio/libdeps')
portable=portable.replace('    h2zero/NimBLE-Arduino@2.5.1','    NimBLE-Arduino=symlink://vendor/NimBLE-Arduino\n    SdFat=symlink://vendor/SdFat')
portable+='\nplatform_packages =\n    framework-arduinoespressif32 @ symlink://vendor/Arduino-ESP32\n'
(out/'firmware/platformio-share.ini').write_text(portable,encoding='utf-8')
# Use precisely the archives whose members were processed by the original link;
# omit unused platform libraries, rather than redistribute unrelated components.
maptext=(build/'firmware.map').read_text()
archives={Path(x.replace('\\','/')).resolve() for x in re.findall(r'([^\s()]+\.a)\(',maptext)}
used_names={p.name[3:-2] for p in archives}
sdk=core/'framework-arduinoespressif32-libs/esp32s3'
for p in archives:
    if sdk in p.parents:copy(p,'relink/idf/'+p.relative_to(sdk).as_posix())
    elif build in p.parents:copy(p,'relink/build/'+p.relative_to(build).as_posix())
    elif (core/'toolchain-xtensa-esp-elf') not in p.parents:raise RuntimeError('Unknown link archive: '+str(p))
for folder in ['ld','qio_opi']:
    for p in (sdk/folder).rglob('*.ld'):copy(p,'relink/idf/'+p.relative_to(sdk).as_posix())
capture=json.loads((base/'test-results/relink-link.json').read_text())
for p in map(Path,capture['elf_sources']):
    assert p.exists(),p
    copy(p,'relink/build/'+p.relative_to(build).as_posix())
argv=[]
for arg in capture['argv']:
    if arg.startswith('-l') and arg[2:] not in used_names:continue
    arg=arg.replace('\\','/')
    arg=arg.replace(build.as_posix(),'${ROOT}/relink/build').replace(sdk.as_posix(),'${ROOT}/relink/idf')
    arg=arg.replace('${ROOT}/relink/build/firmware.elf','${ROOT}/relinked/firmware.elf').replace('${ROOT}/relink/build/firmware.map','${ROOT}/relinked/firmware.map')
    assert not re.search(r'[A-Z]:/',arg),arg
    argv.append(arg)
link={'argv':argv,'original_elf_sha256':sha(build/'firmware.elf'),
      'used_archives':[p.name for p in sorted(archives)],'toolchain':'xtensa-esp-elf 14.2.0+20251107',
      'method':'Original input objects and linker flags; unused -l entries omitted; matching toolchain runtimes are installed separately.'}
(out/'relink/link.json').parent.mkdir(exist_ok=True)
(out/'relink/link.json').write_text(json.dumps(link,indent=2),encoding='utf-8')
for name in ['firmware.bin','bootloader.bin','partitions.bin']:copy(build/name,'binaries/'+name)
copy(core/'framework-arduinoespressif32/tools/partitions/boot_app0.bin','binaries/boot_app0.bin')
copy(build/'firmware.map','verification/original-firmware.map')
for name in ['reading-check.txt','ble-check.txt','distribution-build.txt','reading-preview.png']:copy(base/'test-results'/name,'verification/'+name)
copy(root/'docs/SHARING.md','START_HERE.md')
print(json.dumps({'prepared':str(out),'objects':len(capture['elf_sources']),'archives':len(archives),'link_args':len(argv)},indent=2))
