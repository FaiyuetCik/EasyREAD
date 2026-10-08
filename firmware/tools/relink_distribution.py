"""Run from an extracted sharing package. Requires the matching ESP32-S3 toolchain.
Produces output only; never uploads/flashes. Paths come from bundled link.json.
"""
from pathlib import Path
import argparse, json, subprocess, shutil, hashlib, os
parser=argparse.ArgumentParser()
parser.add_argument('--compiler', help='Path to xtensa-esp32s3-elf-g++ executable')
parser.add_argument('--package-root', type=Path)
parser.add_argument('--verify-original',action='store_true')
options=parser.parse_args()
root=(options.package_root or Path(__file__).resolve().parents[2]).resolve()
compiler=options.compiler or shutil.which('xtensa-esp32s3-elf-g++')
if not compiler:
    core=Path(os.environ.get('PLATFORMIO_CORE_DIR',Path.home()/'.platformio'))
    candidate=core/'packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-g++.exe'
    if candidate.exists():compiler=str(candidate)
if not compiler:parser.error('Install the pinned PlatformIO environment or supply --compiler.')
data=json.loads((root/'relink/link.json').read_text(encoding='utf-8'))
output=root/'relinked';output.mkdir(exist_ok=True)
args=[x.replace('"','').replace('${ROOT}',root.as_posix()) for x in data['argv'][1:]]
args=['"'+x.replace('"','\\"')+'"' for x in args]
response=output/'link.rsp';response.write_text('\n'.join(args),encoding='utf-8')
subprocess.run([compiler,'@'+str(response)],cwd=root,check=True)
elf=output/'firmware.elf'
result={'result':'PASS','elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),
        'original_elf_sha256':data['original_elf_sha256']}
result['matches_original_elf']=result['elf_sha256']==result['original_elf_sha256']
if options.verify_original and not result['matches_original_elf']:
    raise RuntimeError('Relinked ELF differs from original; do not claim byte-identical verification.')
(output/'verification.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
