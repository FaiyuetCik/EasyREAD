# Faiyuet Morgan 固件分享包

适配 **Seeed Studio reTerminal Sticky，ESP32-S3，16 MB Flash / 8 MB OPI PSRAM，480×800 竖屏**。
这是已构建并经过桌面验证的软件预览版。尚未烧录或完成显示、读卡、触控、真实蓝牙、休眠与续航的真机验收。

## 发给别人什么

发送完整的 `Faiyuet-Morgan-share-2026-10-08.zip`。接收者无需访问你的私有 GitHub 仓库。
不要只发送 `.bin`：完整包一起提供源码、许可、构建说明和重新链接材料。
`manifest.json` 记录逐个文件的大小和 SHA-256，`distribution/verification.json` 记录整体压缩包校验。

## 内容

- `binaries/`：应用固件、bootloader、分区表、boot_app0 和合并完整镜像。
- `sd-card/`：简体/繁体 Noto 字体、字体许可和原创样例 EPUB；不含你的个人书籍。
- `firmware/src/`：对应应用源码，包括字体子集和启动图。
- `firmware/freeink-sdk/`：实际使用的固定 SDK 源码；未使用的 lucide Git 子模块未展开。
- `firmware/vendor/`：实际构建使用的 Arduino-ESP32、NimBLE-Arduino、SdFat 源码。
- `dependency-sources/`：ESP-IDF `87912cd291` 对应源代码主树和有关子模块归档、来源及哈希。
- `legal/`：各组件许可与署名，包含 LGPL、GCC runtime exception、Newlib 和 Espressif 无线电二进制库条款。
- `relink/`：实际应用对象文件、Arduino 库、实际使用的 ESP-IDF 静态库、链接脚本和完整链接参数。
- `verification/`：构建记录、阅读/BLE 模拟检查及链接映射。

原创部分允许由项目所有者提供副本的接收者使用、复制、为自己使用而修改，
以及为调试修改而逆向工程和重新链接。第三方权利按其各自许可。
详见 LICENSE.md；没有把整个原创项目改为 MIT 或公开仓库。

## 使用和烧录

先阅读 `docs/USER_GUIDE.md`。microSD 使用 FAT32，把 `sd-card/Books` 和 `sd-card/Fonts` 放到卡根目录。
Python 环境安装烧录工具后，选择实际串口：

```powershell
python -m pip install esptool==5.1.2
python -m esptool --chip esp32s3 --port COM实际端口 write-flash 0x0 binaries/full.bin
```

完整镜像从 0x0 烧录，应用镜像 `binaries/firmware.bin` 从 0x10000 烧录；两者不要混用。
脚本不自动烧录，也不自动擦除用户 NVS。

## 从源码构建或修改 Arduino 库

完整对应源码随包提供。Python 3.12、PlatformIO 6.2.0 可使用以下配置：

```powershell
python -m pip install platformio==6.2.0
python -m platformio run -d "$PWD/firmware" -c "$PWD/firmware/platformio-share.ini" -e sticky
```

`platformio-share.ini` 使用当前目录的 `.pio/`，不要求接收者有 D: 盘。
源码依赖来自包内 `firmware/vendor` 与 `firmware/freeink-sdk`。
首次安装平台、编译器及 ESP-IDF 预编译支持包需要联网，由 PlatformIO 下载固定版本。
包没有重新分发整套编译器可执行文件；所用版本为 pioarduino 55.03.37、
Arduino-ESP32 3.3.7、ESP-IDF `87912cd291`、Xtensa GCC 14.2.0 / 20251107。

修改库时，编辑 `firmware/vendor/Arduino-ESP32` 的对应文件并重新构建。
如果再次分享修改后的库或固件，保留许可并注明修改内容和日期，同时提供对应材料。

## 使用对象文件重新链接

安装上述构建环境后，可直接使用包内对象文件重新链接，不必重新编译原创应用：

```powershell
python firmware/tools/relink_distribution.py --verify-original
```

如果 PlatformIO 目录不在默认位置，可设置 PLATFORMIO_CORE_DIR，或用
`--compiler "完整路径/xtensa-esp32s3-elf-g++.exe"` 指定同版本编译器。
输出在 `relinked/firmware.elf`。未经修改的输入应与包记录的原始 ELF SHA-256 完全一致。
修改库后，把新构建的 `libFrameworkArduino.a` 替换到 `relink/build/`，然后重新链接；
此时不加 `--verify-original`，因为修改后的固件应当发生变化。
如果修改了头文件接口，推荐直接从全部源码重建。

```powershell
python -m esptool --chip esp32s3 elf2image --flash-mode dio --flash-freq 80m --flash-size 16MB --elf-sha256-offset 0xb0 -o relinked/firmware.bin relinked/firmware.elf
```

部分无线电组件由上游以二进制形式提供，没有可随包提供的实现源码；
本包保留其上游 Apache-2.0 许可。字体子集继续遵守 OFL。
分享包提供当前固件的对应应用/Arduino 源码及重新链接途径；
软件验收不代表真机产品验收，也不代表未来修改或加入其他依赖后的分发许可自动通过。
