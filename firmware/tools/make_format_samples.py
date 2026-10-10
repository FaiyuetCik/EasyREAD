# SPDX-License-Identifier: MIT
"""Original multi-format reading fixtures; no third-party books are included."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import struct
import json
import hashlib

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / 'firmware/test-fixtures/formats'
RESULTS = ROOT / 'firmware/test-results/formats'

def text_book():
    lines = ['EasyREAD 原创格式测试书', 'This book is original test content.']
    for chapter in range(1, 5):
        lines += ['', f'第{chapter}章 测试章节', f'Marker: CHAPTER-{chapter}']
        for paragraph in range(1, 19):
            lines.append(f'本章第{paragraph}段：清晨，小舟驶过安静的河面。读者翻到下一页，仍能找到刚才的位置。'
                         'English 0123456789 & < > " XML symbols. 中文标点：，。！？（）。')
        lines += [f'CHAPTER-{chapter}-END']
    lines += ['', '全书结束。END-OF-BOOK']
    return '\r\n'.join(lines) + '\r\n'

def image_page(number, gray=False, size=(480, 800)):
    image = Image.new('L', size, 255)
    d = ImageDraw.Draw(image)
    font = ImageFont.load_default(size=28)
    d.rectangle((4, 4, size[0]-5, size[1]-5), outline=0, width=4)
    d.polygon([(12, 12), (85, 12), (12, 85)], fill=0)
    d.text((100, 22), f'TOP / PAGE {number}', fill=0, font=font)
    d.text((28, 110), 'EasyREAD original test', fill=0, font=font)
    d.text((28, 160), f'PAGE-{number}-MARKER', fill=0, font=font)
    for i, value in enumerate((255, 170, 85, 0) if gray else (255, 0, 255, 0)):
        d.rectangle((28+i*106, 240, 28+(i+1)*106-8, 430), fill=value, outline=0)
    for y in range(470, min(size[1]-110, 670), 20):
        for x in range(30, min(size[0]-30, 450), 20):
            if (x//20+y//20+number) % 2: d.rectangle((x, y, x+11, y+11), fill=0)
    d.text((28, size[1]-80), f'BOTTOM / PAGE {number}', fill=0, font=font)
    return image.point(lambda v: round(v/85)*85 if gray else (255 if v >= 128 else 0))

def encode_page(image, hq, low_first=False):
    w, h = image.size
    pixels = image.load()
    if hq:
        stride = (h+7)//8
        planes = [bytearray(stride*w), bytearray(stride*w)]
        for x in range(w):
            for y in range(h):
                level = {255:0, 170:2, 85:1, 0:3}[pixels[x,y]]
                at, mask = (w-1-x)*stride+y//8, 0x80>>(y%8)
                if level & 2: planes[0][at] |= mask
                if level & 1: planes[1][at] |= mask
        payload = b''.join(reversed(planes) if low_first else planes)
        magic = b'XTH\0'
    else:
        stride = (w+7)//8
        payload = bytearray(stride*h)
        for y in range(h):
            for x in range(w):
                if pixels[x,y] >= 128: payload[y*stride+x//8] |= 0x80>>(x%8)
        magic = b'XTG\0'
    return struct.pack('<4sHHBBIQ', magic, w, h, 0, 0, len(payload), 0) + payload

def container(images, hq=False, old=False):
    pages = [encode_page(image, hq, old) for image in images]
    chapter_count = len(pages)
    chapter_offset, index_offset = 312, 312+chapter_count*96
    data_offset = index_offset + len(pages)*16
    header = struct.pack('<4sHH4BI5Q', b'XTCH' if hq else b'XTC\0', 1 if old else 0x100,
                         len(pages), 0, 1, 0, 1, 1, 56, index_offset, data_offset, 0, chapter_offset)
    metadata = bytearray(256)
    title = b'EasyREAD original image book'
    metadata[:len(title)] = title
    if old:
        struct.pack_into('<IH', metadata, 192, 1791590400, chapter_count)
    else:
        metadata[224:226] = b'en'
        struct.pack_into('<IHH', metadata, 240, 1791590400, 0, chapter_count)
    chapters = bytearray(chapter_count*96)
    for i in range(chapter_count):
        name = f'Original page {i+1}'.encode()
        chapters[i*96:i*96+len(name)] = name
        struct.pack_into('<HH', chapters, i*96+80, i+int(old), i+int(old))
    index, at = bytearray(), data_offset
    for image, page in zip(images, pages):
        index += struct.pack('<QIHH', at, len(page), *image.size)
        at += len(page)
    return header + metadata + chapters + index + b''.join(pages)

def bitmap(image, depth, topdown=False):
    w, h = image.size
    palette = b''
    if depth <= 8:
        colors = 1 << depth
        palette = b''.join(bytes([round(i*255/(colors-1))]*3+[0]) for i in range(colors))
    stride = ((w*depth+31)//32)*4
    rows = bytearray()
    for y in range(h) if topdown else range(h-1, -1, -1):
        row = bytearray(stride)
        for x in range(w):
            value = image.getpixel((x,y))
            if depth >= 24: row[x*(depth//8):x*(depth//8)+depth//8] = bytes([value]*3+([0] if depth==32 else []))
            else:
                idx = round(value*((1<<depth)-1)/255)
                if depth == 8: row[x] = idx
                elif depth == 4: row[x//2] |= idx << (0 if x%2 else 4)
                else: row[x//8] |= idx << (7-x%8)
        rows += row
    offset = 54 + len(palette)
    return struct.pack('<2sIHHI', b'BM', offset+len(rows), 0, 0, offset) + struct.pack(
        '<IiiHHIIiiII', 40,w,-h if topdown else h,1,depth,0,len(rows),2835,2835,len(palette)//4,0) + palette + rows

def main():
    FIXTURES.mkdir(parents=True, exist_ok=True)
    RESULTS.mkdir(parents=True, exist_ok=True)
    content = text_book()
    for name, encoding in [('01-TXT-UTF8.txt','utf-8'),('02-TXT-GBK.txt','gbk'),
                           ('03-TXT-UTF16LE.txt','utf-16'),('04-TXT-UTF16BE.txt','utf-16-be')]:
        data = content.encode(encoding)
        if encoding == 'utf-16-be': data = b'\xfe\xff' + data
        (FIXTURES/name).write_bytes(data)
    mono = [image_page(i) for i in (1,2)]
    gray = [image_page(i, True) for i in (1,2)]
    (FIXTURES/'05-XTC-two-pages.xtc').write_bytes(container(mono))
    (FIXTURES/'06-XTCH-two-pages.xtch').write_bytes(container(gray, True))
    (FIXTURES/'07-BMP-mono.bmp').write_bytes(bitmap(mono[0], 1))
    (FIXTURES/'08-XTCH-legacy.xtch').write_bytes(container(gray, True, True))
    for i, image in enumerate(mono):
        image.save(RESULTS/f'mono-{i}.png'); (RESULTS/f'mono-{i}.gray').write_bytes(image.tobytes())
    for i, image in enumerate(gray):
        image.save(RESULTS/f'gray-{i}.png'); (RESULTS/f'gray-{i}.gray').write_bytes(image.tobytes())
    odd = Image.new('L',(17,13)); odd.putdata([255 if (x+y)%3 else 0 for y in range(13) for x in range(17)])
    odd.save(RESULTS/'odd.png')
    (RESULTS/'odd.gray').write_bytes(odd.tobytes())
    for depth in [1,4,8,24,32]:
        for topdown in [False,True]:
            (FIXTURES/f'host-bmp-{depth}-{int(topdown)}.bmp').write_bytes(bitmap(odd,depth,topdown))
    (FIXTURES/'host-xtc-odd.xtc').write_bytes(container([odd]))
    (FIXTURES/'host-xth-odd.xtch').write_bytes(container([odd], True))
    (FIXTURES/'host-long-line.txt').write_text('第1章 长行\n'+ '中英<&>ABC'*2000+'\nEND-LONG-LINE\n', encoding='utf-8')
    (FIXTURES/'host-utf8-bom.txt').write_bytes(content.encode('utf-8-sig'))
    for name, data in [('bad-empty.txt',b''),('bad-utf8.txt',b'\xef\xbb\xbf\xff'),
                       ('bad-utf16.txt',b'\xff\xfe\x00\xd8'),('bad-gbk.txt',b'\x81\x30'),
                       ('bad-utf16-low.txt',b'\xff\xfe\x00\xdc'),
                       ('bad-truncated.xtc',container(mono)[:-200]),('bad-header.xtch',b'not a book'*10)]:
        (FIXTURES/name).write_bytes(data)
    invalid = bytearray(bitmap(odd,24)); struct.pack_into('<I',invalid,30,1)
    (FIXTURES/'bad-compressed.bmp').write_bytes(invalid)
    invalid = bytearray(container(mono)); page_at = struct.unpack_from('<Q',invalid,24)[0]
    page_at = struct.unpack_from('<Q',invalid,page_at)[0]
    invalid[page_at+9] = 1
    (FIXTURES/'bad-compressed.xtc').write_bytes(invalid)
    invalid = bytearray(container(mono)); struct.pack_into('<Q',invalid,24,len(invalid)+1)
    (FIXTURES/'bad-index.xtc').write_bytes(invalid)
    manifest = {p.name: {'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
                for p in sorted(FIXTURES.iterdir()) if p.is_file() and p.suffix != '.json'}
    (FIXTURES/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print(f'Generated {len(manifest)} original fixtures; SD sample subset: {sum(v["bytes"] for n,v in manifest.items() if n[:2].isdigit())} bytes')

if __name__ == '__main__': main()
