"""Run under WSL/Linux; compile the same fixed SDK used by firmware."""
from pathlib import Path
import subprocess, concurrent.futures, tempfile, shutil
base=Path(__file__).resolve().parents[1]
sdk=base/'freeink-sdk/libs'
book=sdk/'book/FreeInkBook';font=sdk/'font/FreeInkFont'
work=base/'test-results/host-build';work.mkdir(parents=True,exist_ok=True)
includes=[book/'include',font/'include',font/'third_party/stb',font/'third_party/freetype/include']+[book/'third_party'/x for x in ['expat','miniz','libunibreak','pngle','tjpgd','stb']]
flags=[f'-I{x}' for x in includes]
sources=list((book/'src/vendor').glob('*.c'))+list((font/'src/freetype').glob('*.c'))+[font/'src/FontAlloc.c']
sources+=list((book/'src').rglob('*.cpp'))+[font/'src'/x for x in ['TtfFont.cpp','FtFont.cpp','Gsub.cpp','Gpos.cpp']]+[base/'tools/host_check.cpp']
def compile(src):
    dest=work/(str(src.relative_to(base)).replace('/','_')+'.o')
    cmd=['g++' if src.suffix=='.cpp' else 'gcc','-std=c++17' if src.suffix=='.cpp' else '-std=c11','-O1','-g']+flags+['-c',str(src),'-o',str(dest)]
    if not dest.exists() or dest.stat().st_mtime < src.stat().st_mtime:
        result=subprocess.run(cmd,capture_output=True,text=True)
        if result.returncode: print(result.stderr);result.check_returncode()
    return str(dest)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:objects=list(pool.map(compile,sources))
exe=work/'host-check';subprocess.run(['g++',*objects,'-o',str(exe)],check=True)
root=base.parent
local=Path(tempfile.mkdtemp(prefix='faiyuet-host-'))
for name in ['Books/Faiyuet-Morgan-test.epub','Fonts/NotoSansSC-VF.ttf','Fonts/NotoSansTC-VF.ttf']:
    shutil.copy2(root/'sd-card'/name,local/Path(name).name)
for name in ['drm.epub','truncated.epub','garbage.epub']:
    shutil.copy2(base/'test-fixtures'/name,local/name)
result=subprocess.run([str(exe),str(local/'Faiyuet-Morgan-test.epub'),str(local/'NotoSansSC-VF.ttf'),str(local/'NotoSansTC-VF.ttf'),str(base/'test-results')],capture_output=True,text=True)
(base/'test-results/reading-check.txt').write_text(result.stdout+result.stderr)
print(result.stdout+result.stderr);result.check_returncode()
