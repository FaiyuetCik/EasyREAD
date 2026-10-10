"""Build the offline Windows starter ZIP; original code MIT, 2026 FaiyuetCik."""
import argparse
import hashlib
import importlib.metadata as metadata
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import urllib.request
import zipfile

HERE = Path(__file__).resolve().parent

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
    version='2026-10-10.1'
    if sha(a.firmware_package) != '532f67974ba7e73f3e5e2a11c932890070e492190dfdeb72e68567f30d60af13':
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
        if file.is_file(): shutil.copy2(file,source/file.name)
    shutil.copy2(root/'LICENSE.md',out/'LICENSE.md')
    shutil.copy2(root/'LICENSE.md',source/'LICENSE.md')
    legal=out/'licenses'; legal.mkdir()
    records=[]; lock=[]
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
        (source/'dependencies').mkdir(exist_ok=True)
        shutil.copy2(cached,source/'dependencies'/cached.name)
    (source/'requirements-lock.txt').write_text('\n'.join(lock)+'\n',encoding='utf-8')
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
    (source/'sources.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
    notice='''# Windows starter licenses and sources

EasyREAD UI/build scripts: MIT, copyright 2026 FaiyuetCik (LICENSE.md).
The separate esptool executable and esptool_entry.py are GPL-2.0-or-later.
Its complete matching upstream source is included in installer-sources/dependencies.
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
Redistribute this complete starter ZIP with its sources and licenses intact.
'''
    (out/'THIRD_PARTY_NOTICES.md').write_text(notice,encoding='utf-8')
    guide='''EasyREAD — 三步开始 / Three steps to read
========================================
1. 完整解压此 ZIP，不要在压缩包内运行程序。
2. 将 SD-Card 里的 Fonts、Books 复制到 FAT32 microSD 卡根目录。
3. USB 数据线连接 reTerminal Sticky，打开 EasyREAD-Setup.exe，选择设备并安装。
   完成后插卡开机，即可阅读测试书；自己的电子书也放入 Books。

首次安装会重置内部进度和设置。保留设置升级先核对已有固件布局，不匹配则停止。
仅支持 Seeed Studio reTerminal Sticky；Windows 10/11 x64，无需 Python 或联网。
首次使用电脑如无串口，请检查数据线及设备 USB 驱动。不要在安装期间断开电源。
程序未签名；请从本项目 GitHub 下载并核对 SHA256，不要全局关闭系统安全功能。
许可证及源码已随包保留。此版为开发预览，完整硬件验收尚未完成。

1. Extract the entire ZIP before running the application.
2. Copy Fonts and Books inside SD-Card to the root of a FAT32 microSD card.
3. Connect reTerminal Sticky using a USB data cable, open EasyREAD-Setup.exe and install.
   Insert the prepared card after installation and start reading.

First install resets internal positions/settings. Keep-settings upgrade requires matching
existing firmware layout. Windows 10/11 x64 only. No Python or internet required;
a USB driver may still be needed. Keep power connected throughout installation.
The executable is unsigned. Download from this repository and verify SHA256.
Keep the included source and license materials when redistributing.
Development preview: complete hardware acceptance is still pending.
'''
    (out/'START-HERE.txt').write_text(guide,encoding='utf-8-sig')
    payload={'version':version,'target':'Seeed Studio reTerminal Sticky','files':{}}
    for file in sorted((out/'firmware').glob('*.bin')): payload['files'][file.relative_to(out).as_posix()]=sha(file)
    payload['files']['tools/esptool/esptool.exe']=sha(out/'tools/esptool/esptool.exe')
    (out/'payload.json').write_text(json.dumps(payload,indent=2),encoding='utf-8')
    manifest={p.relative_to(out).as_posix():{'sha256':sha(p),'size':p.stat().st_size} for p in sorted(out.rglob('*')) if p.is_file()}
    (out/'MANIFEST.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    destination=out.parent/f'EasyREAD-Windows-Starter-{version}.zip'
    if destination.exists(): raise FileExistsError(destination)
    with zipfile.ZipFile(destination,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
        for file in sorted(out.rglob('*')):
            if file.is_file(): archive.write(file,file.relative_to(out))
    record={'file':destination.name,'sha256':sha(destination),'size':destination.stat().st_size,
            'files':len(manifest),'firmware_changed':False,'hardware_install_verified':False,
            'python':pyversion,'tcl':tcl,'openssl':openssl}
    destination.with_suffix('.json').write_text(json.dumps(record,indent=2),encoding='utf-8')
    print(json.dumps(record),flush=True)

if __name__=='__main__': main()
