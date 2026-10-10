# EasyREAD 原创格式测试书

由 `firmware/tools/make_format_samples.py` 生成，正文、图案、封装均为项目原创。
采用项目 MIT 许可；不包含用户书籍或第三方电子书。

## 放入 SD 卡的正常样书

| 文件 | 检查内容 |
| --- | --- |
| 01-TXT-UTF8.txt | UTF-8，中英混排，XML 特殊符号，四个章节 |
| 02-TXT-GBK.txt | 同一正文的 GBK 编码，应与 01 显示一致 |
| 03-TXT-UTF16LE.txt | 同一正文的 UTF-16 LE，带 BOM |
| 04-TXT-UTF16BE.txt | 同一正文的 UTF-16 BE，带 BOM |
| 05-XTC-two-pages.xtc | 黑白图片书，两页、两条目录，页码与方向标记 |
| 06-XTCH-two-pages.xtch | 标准灰度双平面，两页、四级灰度与方向标记 |
| 07-BMP-mono.bmp | 单张黑白 BMP，检查方向、缩放和移动 |
| 08-XTCH-legacy.xtch | 旧式转换器元数据、从 1 开始的章节页码、位平面顺序变体 |

TXT 四本内容相同：应识别四个章节，末尾出现 `END-OF-BOOK`。
图片的三角标记在左上，顶部 `TOP`，底部 `BOTTOM`。
目录第二项应进入 PAGE 2；06 和 08 显示的灰度效果应相同。

## 设备检查

1. 将本目录中以 01–08 开头的八个文件放入 SD 卡 `Books/EasyREAD-Tests`，重启扫描。
2. 各文件打开、前后翻页；所有菜单继续先点选、再次点同一项执行。
3. TXT 目录检查四个章节，跳到最后一章；修改字号后检查阅读位置。
4. 图片书从“查看图片”进入，放大、缩小、拖动并返回阅读。
5. 停在第二页或 TXT 中间位置，重启检查续读。

TXT 首次打开会在 `/FM` 生成缓存，原文件不变，需要额外空闲空间。
电脑自动测试通过不等于已完成上述设备验收。

## 电脑回归

`host-*` 是奇数尺寸、BMP 不同位深/行顺序、长行及 BOM 的额外样例；
`bad-*` 是预期拒绝的损坏/不支持输入，不放到日常书架中。

```powershell
& '.\firmware\.venv\Scripts\python.exe' firmware/tools/make_format_samples.py
wsl.exe --exec bash -c 'cd "/mnt/c/Users/seeed/Desktop/Faiyuet Morgan" && python3 firmware/tools/run_format_check.py'
```

生产解析器和 TXT 导入器原样编译，仅 Arduino 字符串、SD I/O 与 PSRAM 分配
替换为电脑适配器。开启 AddressSanitizer 与 UndefinedBehaviorSanitizer；
结果位于忽略的 `firmware/test-results/formats/format-check.json`。
随后运行 `run_incremental_check.py`，以导入的 EPUB 检查真实 SDK 分页一致性、
部分缓存恢复和文字位置。
