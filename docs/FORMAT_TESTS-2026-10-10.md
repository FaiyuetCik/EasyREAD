# EasyREAD 新增格式测试 · 2026-10-10

## 已通过

- 74 项主机自动检查；直接编译生产 `TextImport.cpp` 与 `RasterBook.cpp`，
  仅用主机适配器替换 Arduino 字符串、SD 文件操作和 PSRAM 分配。
- AddressSanitizer 与 UndefinedBehaviorSanitizer 无错误。
- 21,888,000 个输出像素与独立源图采样结果逐像素一致。
- TXT 的 UTF-8、GBK、UTF-16 LE/BE、UTF-8 BOM；解码后全文 Unicode 一致，
  四章目录锚点一致，特殊符号与超长行不丢失；完成的缓存可复用。
- 取消导入、编码错误、空文件、损坏/截断文件、不支持的压缩方式正确拒绝；
  未发布半成品 TXT 缓存，测试输入 hash 保持不变。
- BMP 的 1/4/8/24/32 位、上下行顺序、奇数宽度与填充字节。
- XTC/XTCH 多页、目录页码、奇数尺寸、灰度、旧式元数据/位平面；
  普通阅读、2 倍与 4 倍缩放、平移边界均比对。
- 真实 SDK 分页：导入的 TXT 在 26/34 像素字号分别生成 24/38 页，
  62 页的同步/增量缓存完全一致；部分缓存恢复、取消与文字位置通过。
- 原有 EPUB 回归：93 页通过，覆盖正文、字体、图片、目录、重排续读和损坏输入。

## 已复制到 E 盘

`E:\Books\EasyREAD-Tests`：八本原创正常样书和 README，书籍共 581,364 字节。
复制后读回 SHA256 一致；原有 47 个文件 hash 均未改变；卡剩余 1,407,488 字节。
无用户电子书、第三方电子书或录音被复制到源码仓库或测试包中。

## 尚未验证

新样书在设备上的实际打开、触控翻页、目录选择、字体调整、休眠/重启续读和
实屏灰度效果。主机适配器不能模拟 SD 的共享 SPI 时序、ESP32 内存压力、
屏幕刷新和 NVS 断电持久化。需要将卡安全弹出、放回设备并重启后继续验收。

## 复现

先运行 `firmware/tools/make_format_samples.py`，然后在 WSL 中运行
`firmware/tools/run_format_check.py`。
再运行 `firmware/tools/run_incremental_check.py`，参数为
`firmware/test-results/formats/01-TXT-UTF8.epub`。

详细主机报告、渲染页和日志位于忽略的 `firmware/test-results/`。
对应发布版本：2026-10-10.1；发布后的状态以 GitHub Release 为准。
