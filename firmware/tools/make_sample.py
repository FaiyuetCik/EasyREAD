"""Create an original bilingual EPUB fixture, with PNG/JPEG and TOC anchors."""
from pathlib import Path
import zipfile, zlib, struct, binascii, io
from PIL import Image
root = Path(__file__).resolve().parents[2]
def chunk(kind, data):
    return struct.pack('>I',len(data))+kind+data+struct.pack('>I',binascii.crc32(kind+data)&0xffffffff)
w,h=640,320
raw=b''.join(b'\0'+bytes(20 if (x//40+y//40)%2 else 235 for x in range(w)) for y in range(h))
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,0,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
container='<?xml version="1.0"?><container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>'
opf='''<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">fm-original-test-1</dc:identifier><dc:title>Faiyuet Morgan 中英閱讀測試</dc:title><dc:language>zh</dc:language><dc:creator>Faiyuet Morgan</dc:creator></metadata><manifest><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/><item id="ch1" href="ch1.xhtml" media-type="application/xhtml+xml"/><item id="ch2" href="ch2.xhtml" media-type="application/xhtml+xml"/><item id="cover" href="images/chart.png" media-type="image/png" properties="cover-image"/></manifest><spine><itemref idref="ch1"/><itemref idref="ch2"/></spine></package>'''
nav='''<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><head><title>目录</title></head><body><nav epub:type="toc"><ol><li><a href="ch1.xhtml">第一章：中文和 English</a></li><li><a href="ch1.xhtml#middle">章内跳转：中段</a></li><li><a href="ch2.xhtml">第二章：圖片與圖表</a></li></ol></nav></body></html>'''
paras=[]
for i in range(100):
    anchor=' id="middle"' if i==45 else ''
    paras.append(f'<p{anchor}>第 {i+1} 段。简体中文阅读测试，繁體中文閱讀測試。书架、目录、触控翻页、断点续读。English mixed text: the small reader keeps your place across font changes. <strong>粗体 Bold</strong> and <em>斜体 Italic</em>.</p>')
def html(body):return '<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Reading test</title></head><body>'+body+'</body></html>'
files={'META-INF/container.xml':container,'OEBPS/content.opf':opf,'OEBPS/nav.xhtml':nav,'OEBPS/ch1.xhtml':html('<h1>中文 English</h1>'+''.join(paras)), 'OEBPS/ch2.xhtml':html('<h1>圖片與圖表</h1><p>点击图片，放大并移动查看细节。Tap the picture to inspect it.</p><img src="images/chart.png" alt="Checkerboard chart"/><p>最後一頁。The end.</p>'), 'OEBPS/images/chart.png':png}
jpg=io.BytesIO();Image.open(io.BytesIO(png)).convert('RGB').save(jpg,format='JPEG',quality=90)
files['OEBPS/images/chart.jpg']=jpg.getvalue()
files['OEBPS/content.opf']=opf.replace('</manifest>','<item id="jpg" href="images/chart.jpg" media-type="image/jpeg"/></manifest>')
files['OEBPS/ch2.xhtml']=files['OEBPS/ch2.xhtml'].replace('<p>最後','<img src="images/chart.jpg" alt="JPEG chart"/><p>最後')
out=root/'sd-card/Books/Faiyuet-Morgan-test.epub'
out.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(out,'w') as z:
    z.writestr('mimetype','application/epub+zip',compress_type=zipfile.ZIP_STORED)
    for name,data in files.items():z.writestr(name,data,compress_type=zipfile.ZIP_DEFLATED)
print(out)
fixture=root/'firmware/test-fixtures';fixture.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(fixture/'drm.epub','w') as z:
    for name,data in files.items():z.writestr(name,data,compress_type=zipfile.ZIP_DEFLATED)
    z.writestr('META-INF/encryption.xml','<encryption xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><EncryptedData xmlns="http://www.w3.org/2001/04/xmlenc#"><EncryptionMethod Algorithm="http://www.w3.org/2001/04/xmlenc#aes256-cbc"/><CipherData><CipherReference URI="OEBPS/ch1.xhtml"/></CipherData></EncryptedData></encryption>')
(fixture/'truncated.epub').write_bytes(out.read_bytes()[:100])
(fixture/'garbage.epub').write_bytes(b'Not a ZIP or EPUB')
