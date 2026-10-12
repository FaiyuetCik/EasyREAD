"""Build the offline Windows starter ZIP; original code MIT, 2026 FaiyuetCik."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import importlib.metadata as metadata
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tomllib
import urllib.request
import zipfile
import xml.etree.ElementTree as ET

HERE = Path(__file__).resolve().parent
STARTER_REVISION = '2026-10-10.2'

def rust_sources(archives, cache, source, legal):
    """Retain exact transitive Rust source and notices from the sdist lock files."""
    crates = {}
    for archive in archives:
        with tarfile.open(archive) as tar:
            for member in tar.getmembers():
                if member.isfile() and member.name.endswith('/Cargo.lock'):
                    lock=tomllib.loads(tar.extractfile(member).read().decode('utf-8'))
                    for package in lock['package']:
                        origin=package.get('source','')
                        if origin.startswith('registry+'):
                            key=(package['name'],package['version'])
                            if key in crates and crates[key]['checksum'] != package['checksum']:
                                raise ValueError('Conflicting crate checksums')
                            crates[key]=package
                        elif origin:
                            raise ValueError('Uncollected non-registry Rust dependency: '+origin)
    def download(item):
        (name,version), package=item
        filename=f'{name}-{version}.crate'
        cached=cache/filename
        record=fetch(f'https://static.crates.io/crates/{name}/{filename}',cached,package['checksum'])
        return cached, record
    records=[]
    with ThreadPoolExecutor(max_workers=6) as pool:
        downloaded=list(pool.map(download,sorted(crates.items())))
    for cached,record in downloaded:
        filename=cached.name; records.append(record)
        dest=source/'rust-crates'/filename; dest.parent.mkdir(exist_ok=True); shutil.copy2(cached,dest)
        with tarfile.open(cached) as tar:
            for member in tar.getmembers():
                if member.isfile() and any(token in Path(member.name).name.lower() for token in ['license','copying','notice','copyright']):
                    dest=legal/'rust-crates'/filename/Path(member.name)
                    if not dest.resolve().is_relative_to(legal): raise ValueError('Invalid license path')
                    dest.parent.mkdir(parents=True,exist_ok=True); dest.write_bytes(tar.extractfile(member).read())
    return records

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024), b''): h.update(block)
    return h.hexdigest()

def fetch(url, dest, expected=None):
    if not dest.exists():
        print('Download', dest.name, flush=True)
        request = urllib.request.Request(url, headers={'User-Agent':'EasyREAD-release-builder'})
        with urllib.request.urlopen(request,timeout=90) as response, dest.with_suffix(dest.suffix+'.partial').open('wb') as f:
            shutil.copyfileobj(response,f)
        dest.with_suffix(dest.suffix+'.partial').rename(dest)
    if expected and sha(dest) != expected: raise ValueError('Source hash mismatch: '+str(dest))
    return {'file':dest.name,'url':url,'sha256':sha(dest),'size':dest.stat().st_size}

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True,help='New empty package directory')
    p.add_argument('--firmware-package',type=Path,required=True,help='Verified firmware/source/relink ZIP')
    p.add_argument('--cache',type=Path,required=True,help='Source archive download cache')
    a=p.parse_args(); out=a.output.resolve(); out.mkdir(parents=True,exist_ok=False)
    cache=a.cache.resolve(); cache.mkdir(parents=True,exist_ok=True)
    version='2026-10-10.2'
    if sha(a.firmware_package) != 'b3be94a6930e2383683525d9d88fe5c3667d43fcbe382724bdea20b5746fc263':
        raise ValueError('Wrong firmware sharing ZIP')
    root=HERE.parent
    # Build separate executables; the MIT UI does not import GPL esptool.
    for entry,name,console,extras in [
        ('easyread_setup.py','EasyREAD-Setup',False,[]),
        ('esptool_entry.py','esptool',True,['--collect-all','esptool','--copy-metadata','esptool'])]:
        subprocess.run([sys.executable,'-m','PyInstaller','--noconfirm','--clean','--onedir',
            '--console' if console else '--windowed','--name',name,
            '--distpath',str(out/'build-dist'),'--workpath',str(out/'build-work'),
            '--specpath',str(out/'build-spec'),*extras,str(HERE/entry)],check=True)
    shutil.copytree(out/'build-dist/EasyREAD-Setup',out,dirs_exist_ok=True)
    shutil.copytree(out/'build-dist/esptool',out/'tools/esptool')
    # These generated folders are verified descendants of the new output directory.
    for name in ['build-dist','build-work','build-spec']:
        target=(out/name).resolve()
        assert target.is_relative_to(out) and target != out
        shutil.rmtree(target)
    with zipfile.ZipFile(a.firmware_package) as archive:
        for item in archive.infolist():
            if item.filename.startswith('binaries/'):
                dest=out/'firmware'/Path(item.filename).name
            elif item.filename.startswith('sd-card/'):
                dest=out/'SD-Card'/Path(item.filename).relative_to('sd-card')
            else: continue
            if item.is_dir(): continue
            assert dest.resolve().is_relative_to(out)
            dest.parent.mkdir(parents=True,exist_ok=True)
            with archive.open(item) as src,dest.open('wb') as dst: shutil.copyfileobj(src,dst)
    (out/'firmware-materials').mkdir()
    shutil.copy2(a.firmware_package,out/'firmware-materials'/a.firmware_package.name)
    source=out/'installer-sources'; source.mkdir()
    for file in HERE.iterdir():
        if file.is_file() and file.name not in {'requirements-lock.txt','dependency-sources.json'}:
            shutil.copy2(file,source/file.name)
    shutil.copy2(root/'LICENSE.md',out/'LICENSE.md')
    shutil.copy2(root/'LICENSE.md',source/'LICENSE.md')
    legal=out/'licenses'; legal.mkdir()
    records=[]; lock=[]; python_archives=[]
    # Exact installed sources, metadata and license files accompany frozen executables.
    for dist in sorted(metadata.distributions(),key=lambda d:d.metadata['Name'].lower()):
        name=dist.metadata['Name']; version_inst=dist.version
        if name=='pip': continue
        lock.append(f'{name}=={version_inst}')
        folder=legal/name; folder.mkdir()
        (folder/'METADATA.txt').write_text(dist.read_text('METADATA') or '',encoding='utf-8')
        for path in dist.files or []:
            if any(token in path.name.lower() for token in ['license','copying','notice','copyright']):
                src=Path(dist.locate_file(path))
                if src.is_file():
                    dest=folder/str(path).replace('..','_up_').replace('\\','/')
                    dest.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(src,dest)
        url=f'https://pypi.org/pypi/{name}/{version_inst}/json'
        with urllib.request.urlopen(url,timeout=60) as response: info=json.load(response)
        candidates=[x for x in info['urls'] if x['packagetype']=='sdist']
        if not candidates: raise ValueError('No source distribution for '+name)
        item=candidates[0]; cached=cache/item['filename']
        records.append(fetch(item['url'],cached,item['digests']['sha256']))
        python_archives.append(cached)
        (source/'dependencies').mkdir(exist_ok=True)
        shutil.copy2(cached,source/'dependencies'/cached.name)
    (source/'requirements-lock.txt').write_text('\n'.join(lock)+'\n',encoding='utf-8')
    records.extend(rust_sources(python_archives,cache,source,legal))
    gpl=root/'legal/GPL-3.0.txt'
    if not gpl.is_file(): gpl=root/'licenses/GPL-3.0.txt'
    records.append({'file':'GPL-3.0.txt','url':'https://www.gnu.org/licenses/gpl-3.0.txt',
                    'sha256':sha(gpl),'size':gpl.stat().st_size})
    shutil.copy2(gpl,legal/'GPL-3.0.txt')
    # Python and Tcl/Tk are bundled by PyInstaller and carry separate notices.
    import tkinter
    interp=tkinter.Tcl(); tcl=interp.eval('info patchlevel')
    pyversion='.'.join(map(str,sys.version_info[:3]))
    runtime_urls=[(f'Python-{pyversion}.tar.xz',f'https://www.python.org/ftp/python/{pyversion}/Python-{pyversion}.tar.xz'),
        (f'tcl-{tcl}.tar.gz',f'https://codeload.github.com/tcltk/tcl/tar.gz/refs/tags/core-{tcl.replace(".","-")}'),
        (f'tk-{tcl}.tar.gz',f'https://codeload.github.com/tcltk/tk/tar.gz/refs/tags/core-{tcl.replace(".","-")}')]
    from cryptography.hazmat.backends.openssl.backend import backend
    openssl=backend.openssl_version_text().split()[1]
    runtime_urls.append((f'openssl-{openssl}.tar.gz',f'https://github.com/openssl/openssl/releases/download/openssl-{openssl}/openssl-{openssl}.tar.gz'))
    import ssl
    python_openssl=ssl.OPENSSL_VERSION.split()[1]
    if python_openssl != openssl:
        runtime_urls.append((f'openssl-{python_openssl}.tar.gz',f'https://github.com/openssl/openssl/releases/download/openssl-{python_openssl}/openssl-{python_openssl}.tar.gz'))
    # Versions pinned by CPython 3.12.10 PCbuild/python.props.
    if pyversion != '3.12.10': raise ValueError('Recheck native runtime dependencies for this Python version')
    runtime_urls.extend([
        ('libffi-3.4.4.tar.gz','https://github.com/libffi/libffi/releases/download/v3.4.4/libffi-3.4.4.tar.gz'),
        ('zlib-1.3.1.tar.gz','https://zlib.net/fossils/zlib-1.3.1.tar.gz'),
        ('bzip2-1.0.8.tar.gz','https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz'),
        ('xz-5.2.5.tar.gz','https://codeload.github.com/tukaani-project/xz/tar.gz/refs/tags/v5.2.5')])
    for filename,url in runtime_urls:
        cached=cache/filename; records.append(fetch(url,cached))
        shutil.copy2(cached,source/'dependencies'/filename)
        with tarfile.open(cached) as tar:
            for member in tar.getmembers():
                if member.isfile() and Path(member.name).name.lower() in ['license','license.txt','license.md','license.terms','copying','notice','license.txt']:
                    dest=legal/'runtime-sources'/filename/Path(member.name)
                    assert dest.resolve().is_relative_to(legal)
                    dest.parent.mkdir(parents=True,exist_ok=True); dest.write_bytes(tar.extractfile(member).read())
    shutil.copy2(Path(sys.base_prefix)/'LICENSE.txt',legal/'Python-runtime-LICENSE.txt')
    for name,url in [
        ('Microsoft-Visual-C-Runtime-terms.html','https://visualstudio.microsoft.com/license-terms/vs2022-cruntime/'),
        ('Microsoft-Visual-Studio-redistribution.html','https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution')]:
        cached=cache/name; records.append(fetch(url,cached)); shutil.copy2(cached,legal/name)
    # The Microsoft landing page embeds the actual terms as an external DOCX.
    # Retain that document and a readable full-text copy, not just the HTML shell.
    terms=cache/'Microsoft-Visual-C-Runtime-2015-2022-License.docx'
    records.append(fetch('https://visualstudio.microsoft.com/wp-content/uploads/2021/09/Visual-C-Runtime-2015-2022-License-1.docx',terms))
    shutil.copy2(terms,legal/terms.name)
    with zipfile.ZipFile(terms) as doc:
        document=ET.fromstring(doc.read('word/document.xml'))
        paragraphs=[''.join(p.itertext()) for p in document.findall('.//{http://schemas.openxmlformats.org/wordprocessingml/2006/main}p')]
    (legal/'Microsoft-Visual-C-Runtime-terms.txt').write_text('\n\n'.join(paragraphs),encoding='utf-8')
    (legal/'Microsoft-runtime-applicable-terms.txt').write_text(
        'Microsoft runtime components only. These terms do not relicense EasyREAD, Python, GPL esptool or other open-source components.\n'
        '微软运行库组件适用；不改变 EasyREAD、Python、esptool 等其他组件的开源许可。\n\n'
        + (legal/'Microsoft-Visual-C-Runtime-terms.txt').read_text(encoding='utf-8')
        + '\n\nPython Windows binary redistribution conditions and licenses:\n\n'
        + (legal/'Python-runtime-LICENSE.txt').read_text(encoding='utf-8'),encoding='utf-8')
    # Carry current firmware attribution even when repackaging an older,
    # unchanged firmware-materials ZIP. The nested matching sources stay intact.
    project_notices = legal/'firmware-attribution'
    if (root/'docs/CODE_PROVENANCE-2026-10-12.md').is_file():
        project_notices.mkdir()
        for name in ['LICENSE.md', 'LICENSE_SCOPE.md', 'THIRD_PARTY_NOTICES.md']:
            shutil.copy2(root/name, project_notices/name)
        for name in ['freeink-reader', 'epub-to-xtc-converter', 'FreeInkSDK']:
            shutil.copytree(root/'legal'/name, project_notices/'legal'/name)
        (project_notices/'docs').mkdir()
        shutil.copy2(root/'docs/CODE_PROVENANCE-2026-10-12.md', project_notices/'docs/CODE_PROVENANCE-2026-10-12.md')
        shutil.copytree(root/'docs/provenance', project_notices/'docs/provenance')
    else:
        # Rebuild from a previously extracted installer-sources directory.
        shutil.copytree(HERE/'firmware-attribution', project_notices)
    shutil.copytree(project_notices, source/'firmware-attribution')
    (source/'sources.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
    notice='''# Windows starter licenses and sources

EasyREAD UI/build scripts: MIT, copyright 2026 FaiyuetCik (LICENSE.md).
esptool upstream and esptool_entry.py are GPL-2.0-or-later. For the frozen esptool
executable, this distribution elects GPL-3.0-or-later as permitted by that grant,
to accommodate Apache-2.0 dependencies. GPL-3.0 text is in licenses/GPL-3.0.txt.
Upstream source grants remain unchanged. The UI is a separate MIT process.
Matching upstream source is in installer-sources/dependencies, with transitive
Rust source archives/checksums and notices in rust-crates folders.
PyInstaller retains GPL with its bootloader exception; see bundled licenses and source.
Python: PSF; Tcl/Tk: their BSD-style terms. Microsoft runtime DLLs retain Microsoft terms
(saved official terms and redistribution page in licenses; Windows only, not MIT/GPL).
Other dependencies retain their original terms,
including cryptography/OpenSSL, pyserial, bitarray, bitstring, tibs and rich.
Exact versions, original source URLs and SHA256 are recorded in installer-sources.
All packages in the isolated build environment are included as source, even if not frozen.
Rebuild using Python 3.12 x64 and requirements-lock.txt, then build_starter.py.
The nested firmware-materials ZIP contains the matching firmware source, LGPL relink
objects, dependency sources, font OFL notices and all firmware licenses.
Current supplemental firmware notices are also in licenses/firmware-attribution,
including the separately retained freeink-reader MIT notice (Justin Mitchell).
Redistribute this complete starter ZIP with its sources and licenses intact.
'''
    (out/'THIRD_PARTY_NOTICES.md').write_text(notice,encoding='utf-8')
    guide='''EasyREAD — 三步开始 / Three steps to read
========================================
1. 完整解压此 ZIP，不要在压缩包内运行程序。
2. 将 SD-Card 里的 Fonts、Books 复制到 FAT32 microSD 卡根目录。
3. USB 数据线连接 reTerminal Sticky，打开 EasyREAD-Setup.exe，选择设备并点击“安装 EasyREAD”。
   完成后插卡开机，即可阅读测试书；自己的电子书也放入 Books。

安装 EasyREAD 会重置设备设置和阅读进度，SD 卡上的书籍不受影响。
以后更新已有 EasyREAD 时，可在“高级选项”中选择“保留设置升级”；布局不匹配则停止。
固件全新安装默认英文；保留设置升级保留原语言。SD 卡内容与 2026-10-10.1 相同。
仅支持 Seeed Studio reTerminal Sticky；Windows 10/11 x64，无需 Python 或联网。
首次使用电脑如无串口，请检查数据线及设备 USB 驱动。不要在安装期间断开电源。
程序未签名；请从本项目 GitHub 下载并核对 SHA256，不要全局关闭系统安全功能。
许可证及源码已随包保留。此版为开发预览，完整硬件验收尚未完成。

1. Extract the entire ZIP before running the application.
2. Copy Fonts and Books inside SD-Card to the root of a FAT32 microSD card.
3. Connect reTerminal Sticky using a USB data cable, open EasyREAD-Setup.exe and click Install EasyREAD.
   Insert the prepared card after installation and start reading.

Fresh firmware installations default to English; upgrades preserve the saved language.
SD card content is unchanged from 2026-10-10.1.
Install EasyREAD resets device settings and reading positions; SD card books are unchanged.
For future updates, Keep-settings upgrade is under Advanced options and requires matching
existing firmware layout. Windows 10/11 x64 only. No Python or internet required;
a USB driver may still be needed. Keep power connected throughout installation.
The executable is unsigned. Download from this repository and verify SHA256.
Keep the included source and license materials when redistributing.
Development preview: complete hardware acceptance is still pending.
'''
    (out/'START-HERE.txt').write_text(guide,encoding='utf-8-sig')
    payload={'version':version,'target':'Seeed Studio reTerminal Sticky','files':{}}
    for file in sorted((out/'firmware').glob('*.bin')): payload['files'][file.relative_to(out).as_posix()]=sha(file)
    for folder in ['tools','_internal']:
        for file in sorted((out/folder).rglob('*')):
            if file.is_file(): payload['files'][file.relative_to(out).as_posix()]=sha(file)
    payload['files']['EasyREAD-Setup.exe']=sha(out/'EasyREAD-Setup.exe')
    for name in ['Microsoft-runtime-applicable-terms.txt','Python-runtime-LICENSE.txt','GPL-3.0.txt']:
        payload['files']['licenses/'+name]=sha(legal/name)
    (out/'payload.json').write_text(json.dumps(payload,indent=2),encoding='utf-8')
    manifest={p.relative_to(out).as_posix():{'sha256':sha(p),'size':p.stat().st_size} for p in sorted(out.rglob('*')) if p.is_file()}
    (out/'MANIFEST.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    destination=out.parent/f'EasyREAD-Windows-Starter-{STARTER_REVISION}.zip'
    if destination.exists(): raise FileExistsError(destination)
    with zipfile.ZipFile(destination,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
        for file in sorted(out.rglob('*')):
            if file.is_file(): archive.write(file,file.relative_to(out))
    record={'file':destination.name,'sha256':sha(destination),'size':destination.stat().st_size,
            'files':len(manifest),'firmware_changed':True,'hardware_install_verified':False,
            'python':pyversion,'tcl':tcl,'openssl':openssl,'starter_revision':STARTER_REVISION,
            'rust_crates':len([r for r in records if r['file'].endswith('.crate')])}
    destination.with_suffix('.json').write_text(json.dumps(record,indent=2),encoding='utf-8')
    print(json.dumps(record),flush=True)

if __name__=='__main__': main()
