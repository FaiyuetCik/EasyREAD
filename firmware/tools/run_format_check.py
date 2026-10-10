# SPDX-License-Identifier: MIT
"""Run on WSL/Linux with g++; compiles production TXT/raster code with ASan/UBSan.

Run make_format_samples.py on Windows first (requires Pillow).
The host adapters replace only Arduino strings, allocation and SD I/O.
"""
from pathlib import Path
import subprocess
import json
import zipfile
import xml.etree.ElementTree as ET
import hashlib
import struct

BASE = Path(__file__).resolve().parents[1]
FIXTURES = BASE/'test-fixtures/formats'
OUT = BASE/'test-results/formats'
HOST = BASE/'tools/format-host'
EXE = OUT/'format-check'

def run(args, expected=0):
    result = subprocess.run([str(EXE),*map(str,args)], capture_output=True, text=True)
    if result.returncode != expected:
        raise AssertionError(f'{args}: exit={result.returncode}\n{result.stdout}\n{result.stderr}')
    if result.stderr: raise AssertionError(result.stderr)
    return result.stdout

def read_gray(path):
    # Source PNGs are read using stdlib-only BMP-like raw companions, generated below.
    data=path.read_bytes()
    if not data.startswith(b'P5\n480 800\n255\n'): raise AssertionError('PGM header')
    return data.split(b'\n',3)[3]

def reference(source, w, h, enlarged=False, zoom=1, px=0, py=0):
    # Integer sampling of independently supplied source pixels, not the decoded file.
    # Match the firmware's float32 geometry, then assert every output pixel.
    f32=lambda x:struct.unpack('f',struct.pack('f',x))[0]
    top, available=(100,650) if enlarged else (0,748)
    scale=f32(min(f32(480/w),f32(available/h))* (zoom if enlarged else 1))
    dw,dh=max(1,int(f32(w*scale))),max(1,int(f32(h*scale)))
    px,py=max(0,min(px,max(0,dw-480))),max(0,min(py,max(0,dh-available)))
    left,first=max(0,(480-dw)//2)-px,top+max(0,(available-dh)//2)-py
    bayer=((0,8,2,10),(12,4,14,6),(3,11,1,9),(15,7,13,5))
    frame=bytearray(b'\xff'*(480*800))
    for y in range(max(top,first),min(top+available,first+dh)):
        sy=(y-first)*h//dh
        for x in range(max(0,left),min(480,left+dw)):
            sx=(x-left)*w//dw
            if source[sy*w+sx] < bayer[y%4][x%4]*16+8: frame[y*480+x]=0
    return frame

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    includes=[HOST,BASE/'src',BASE/'freeink-sdk/libs/book/FreeInkBook/include']
    subprocess.run(['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                    '-no-pie',*[f'-I{p}' for p in includes],str(BASE/'src/TextImport.cpp'),
                    str(BASE/'src/RasterBook.cpp'),str(BASE/'tools/format_host_check.cpp'),'-o',str(EXE)],check=True)
    results=[]
    before={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in FIXTURES.iterdir() if p.is_file()}
    for path in sorted(FIXTURES.glob('*.txt')):
        dest=OUT/(path.stem+'.epub')
        if dest.exists(): dest.unlink()
        bad=path.name.startswith('bad-')
        output=run(['import',path,dest],2 if bad else 0)
        if bad:
            assert not dest.exists() and not Path(str(dest)+'.tmp').exists()
        else:
            with zipfile.ZipFile(dest) as z:
                assert z.testzip() is None
                assert z.read('mimetype') == b'application/epub+zip'
                body=ET.fromstring(z.read('text.xhtml'))
                toc=ET.fromstring(z.read('toc.ncx'))
                text=''.join(body.itertext())
                if 'UTF16BE' in path.name: original = path.read_bytes()[2:].decode('utf-16-be')
                elif 'UTF16LE' in path.name: original = path.read_bytes().decode('utf-16')
                elif 'GBK' in path.name: original = path.read_bytes().decode('gbk')
                else: original = path.read_bytes().decode('utf-8-sig')
                assert ''.join(body.find('{*}body').itertext()) == ''.join(original.splitlines()), 'exact Unicode content parity'
                if path.name.startswith('host-long'):
                    assert text.count('中英<&>ABC') == 2000 and 'END-LONG-LINE' in text
                else:
                    assert text.count('English 0123456789 & < > " XML symbols.') == 72
                    assert 'END-OF-BOOK' in text
                    assert len(toc.findall('.//{*}navPoint')) == 4
                    assert [e.attrib['id'] for e in body.findall('.//{*}h2')] == ['c0','c1','c2','c3']
            assert 'PASS import' in run(['import',path,dest]) # reusable completed cache
        results.append({'name':path.name,'result':'PASS','detail':output.strip()})
    # Force cancellation during a sufficiently long import and verify atomic cleanup.
    cancel=OUT/'cancel-input.txt'; cancel.write_bytes(('取消测试 paragraph.\n'*300000).encode())
    dest=OUT/'cancel.epub'
    if dest.exists(): dest.unlink()
    output=run(['cancel',cancel,dest],2)
    assert 'Import cancelled' in output and not dest.exists() and not Path(str(dest)+'.tmp').exists()
    results.append({'name':'cancel-and-cleanup','result':'PASS'})
    sources={name:(OUT/(name+'.gray')).read_bytes() for name in ['mono-0','mono-1','gray-0','gray-1','odd']}
    for path in sorted(FIXTURES.iterdir()):
        if path.suffix not in ['.xtc','.xtch','.bmp']: continue
        mode='bmp' if path.suffix=='.bmp' else 'xtc'
        if path.name.startswith('bad-'):
            # The truncated container still has a valid first page; test the missing second page.
            page=1 if 'truncated' in path.name else 0
            output=run([mode,path,OUT/'rejected.pgm',page,0,1,0,0],2)
            results.append({'name':path.name,'result':'PASS','detail':output.strip()}); continue
        odd='odd' in path.name or path.name.startswith('host-bmp')
        names=['odd'] if odd else ['mono-0'] if mode=='bmp' else ['gray-0','gray-1'] if path.suffix=='.xtch' else ['mono-0','mono-1']
        w,h=(17,13) if odd else (480,800)
        for page,name in enumerate(names):
            for enlarged,zoom,px,py in [(0,1,0,0),(1,2,100,160),(1,4,99999,99999)]:
                rendered=OUT/f'{path.stem}-p{page}-z{zoom}.pgm'
                output=run([mode,path,rendered,page,enlarged,zoom,px,py])
                expected=reference(sources[name],w,h,bool(enlarged),zoom,px,py)
                assert read_gray(rendered)==expected, f'pixel mismatch {path.name} page {page} zoom {zoom}'
                if mode=='xtc':
                    assert f'chapters={len(names)}' in output
                    for i in range(len(names)): assert f'TOC {i} Original page {i+1}' in output
                results.append({'name':path.name,'page':page,'zoom':zoom,'result':'PASS','pixels':480*800})
            assert 'REJECT' in run([mode,path,OUT/'rejected.pgm',len(names),0,1,0,0],2)
        # Truncate at multiple structural boundaries: must reject without a sanitizer error.
        original=path.read_bytes()
        for length in [0,4,21,53,55,len(original)//2]:
            damaged=OUT/('truncated-'+path.name); damaged.write_bytes(original[:length])
            run([mode,damaged,OUT/'rejected.pgm',len(names)-1,0,1,0,0],2)
    after={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in FIXTURES.iterdir() if p.is_file()}
    assert before==after,'original fixture modified'
    report={'result':'PASS','checks':len(results),'compares_pixels':sum(r.get('pixels',0) for r in results),
            'sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],'originals_unchanged':True,
            'hardware_verified':False,'results':results}
    (OUT/'format-check.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='results'},indent=2))

if __name__=='__main__': main()
