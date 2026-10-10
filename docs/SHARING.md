# EasyREAD 固件分享包

最近一次历史包：`Faiyuet-Morgan-share-2026-10-09.zip`，适配 Seeed Studio reTerminal Sticky，
ESP32-S3、16 MB 配置 / 8 MB OPI PSRAM、480×800 竖屏。
该包仅对应 2026-10-09 应用及当日 MIT 和第三方许可范围声明。2026-10-10 EasyREAD 改动已更新源码并烧录，尚未封装新的对应分发材料；以下说明及验证结果仅属于历史包。
这是软件预览；当前新构建没有再次烧录。之前版本已验证 SD、中文正文与基本翻页，
切章偶发启动页尚未完成定位，BLE、休眠、功耗和长期稳定性不视为已验收。

## 包含什么

- `binaries/`：应用、bootloader、分区表、boot_app0 和完整合并镜像。
- `firmware/src/`、`tools/`、配置及测试夹具：匹配的原创应用源码和工具。
- `firmware/freeink-sdk/`、`vendor/`：固定 SDK、实际 Arduino、NimBLE、SdFat 源码。
- `dependency-sources/`：固定 ESP-IDF `87912cd291d68f4319f13695718af6754879a83f`
  及有关子模块源码归档、来源和 SHA-256；esp_diagnostics 1.2.1 源码和校验。
- `relink/`：294 个实际链接对象、使用的静态库、链接脚本与参数。
- `sd-card/`：Noto SC/TC 字体、OFL 全文和原创测试书，不含私人 EPUB。
- `LICENSE.md`、`LICENSE_SCOPE.md`、`THIRD_PARTY_NOTICES.md`、`legal/`：
  原创 MIT、第三方许可及完整版权/NOTICE。
- `manifest.json`：逐文件长度和 SHA-256；`verification/` 与
  `relinked/verification.json`：本版本的构建、源码选择与重新链接记录。

原创部分采用 MIT；Arduino 核心继续 LGPL，Noto 字体及字体衍生数据继续 OFL，
其余组件遵守各自许可。不要只转发 `.bin` 而遗漏其对应的源码与许可材料。

## 使用和烧录

阅读 `docs/USER_GUIDE.md`。microSD 使用 FAT32，将 `sd-card/Fonts` 和
`sd-card/Books` 复制到卡根目录。用户自己的无 DRM EPUB 自行放入 Books。

```powershell
python -m pip install esptool==5.1.2
python -m esptool --chip esp32s3 --port COM实际端口 write-flash 0x0 binaries/full.bin
```

完整镜像从 0x0 使用；单独应用镜像 `binaries/firmware.bin` 从 0x10000 使用。
脚本不自动烧录，不自动擦除用户 NVS。

## 从包内源码构建

使用 Python 3.12 与 PlatformIO 6.2.0，在解压后的包根目录运行：

```powershell
python -m pip install platformio==6.2.0
python -m platformio run -d firmware -c firmware/platformio-share.ini -e sticky
```

便携配置使用本目录 `.pio/`，不要求 D: 盘；SDK、Arduino、NimBLE 和 SdFat
来自包内源码。首次安装固定平台、编译器和 ESP-IDF 支持包需要联网。
版本为 pioarduino 55.03.37、Arduino-ESP32 3.3.7、Xtensa GCC 14.2.0 / 20251107。
修改 Arduino 时编辑 `firmware/vendor/Arduino-ESP32` 并重建；应用为 MIT，
库修改继续遵守 LGPL，包括上游版权及修改说明。
长路径敏感的 Windows 构建环境建议解压至较短目录，例如 `C:/fm-share`。

## 用附带对象重新链接

```powershell
python firmware/tools/relink_distribution.py --verify-original
```

需要相同编译器。可用 `PLATFORMIO_CORE_DIR` 或
`--compiler 完整路径/xtensa-esp32s3-elf-g++.exe` 指定工具链。
原始输入应生成与 `relink/link.json` 记录完全相同的 ELF SHA-256。
修改库后可替换 `relink/build/libFrameworkArduino.a` 再运行，不加
`--verify-original`；若修改头文件接口，建议直接从全部源码重新构建。

```powershell
python -m esptool --chip esp32s3 elf2image --flash-mode dio --flash-freq 80m --flash-size 16MB --elf-sha256-offset 0xb0 -o relinked/firmware.bin relinked/firmware.elf
```

重新构建的二进制可能因构建路径等元数据不同而与原镜像哈希不同；
附带原始对象的重新链接则要求字节一致。
无线电等部分由上游以二进制提供，随包保留其上游许可；不声称拥有这些实现的版权。

## 维护者重建分发包

在源码仓库安装固定依赖并完成正常构建后，选择不存在的空输出目录：

```powershell
python firmware/tools/package_distribution.py --output C:/fm-share-new
```

然后按上面的便携配置构建，日志保存为包内 `verification/portable-build.txt`；
执行重新链接校验；用同版本 esptool 将 bootloader/partitions/boot_app0/应用
分别在 0x0/0x8000/0xe000/0x10000 合并为 `binaries/full.bin`。

```powershell
python firmware/tools/prepare_distribution_verification.py --package-root C:/fm-share-new
python firmware/tools/seal_distribution.py --package-root C:/fm-share-new --version 新版本号
```

封包前会核验源码选择、合并应用及重新链接结果，封包后逐项核验 ZIP 内文件哈希。
不得复用旧版验证记录来声明新版已通过。输出目录和已有同版本 ZIP 都不自动覆盖。

## 历史包

`Faiyuet-Morgan-share-2026-10-08.zip` 及其旧 Release 保留，里面是旧源码、旧固件和
旧许可说明，不能用来代替本版对应材料。不要混用两版的固件与重新链接对象。
