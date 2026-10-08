# 固件开发

此目录是 Faiyuet Morgan 自有固件原型。`freeink-sdk/` 为 MIT 许可的第三方依赖，检出提交 `a1cf5b4be3e5259f856fe0497129e360e4e1346d`；应用入口是 `src/main.cpp`。

## 当前原型

应用已实现屏幕书架、递归扫描、EPUB 分页/缓存/文字与图片、目录与章内锚点、位置保存、排版设置、触控/实体键/BLE HID 操作、图片查看器和休眠唤醒流程。需从项目 `sd-card/` 复制 Fonts 和 Books 至 FAT32 卡根目录。完整操作说明见 `../docs/USER_GUIDE.md`。M1 原型源代码保留在 `archive/main-m1.cpp`。

固件的屏幕方向由 `platformio.ini` 中 `FM_SPLASH_ROTATE_CW` 控制，当前设为 1，需真机检查。

## 构建

在 PowerShell 中从项目根目录运行：

```powershell
$env:PLATFORMIO_CORE_DIR = 'D:\fm-pio-core'
& '.\firmware\.venv\Scripts\python.exe' -m platformio run -d '.\firmware' -e sticky
```

`platformio.ini` 将构建目录和依赖缓存放在 D 盘短路径，使用 8 MB OPI PSRAM 和 3 MB app0 分区。NimBLE 固定为 2.5.1；SDK 文件未修改。软件版产物在 `artifacts/software-preview-2026-10-08/`：full.bin 从 0x0 烧录，app.bin 仅从 0x10000 烧录。两者均尚未真机烧录。优先使用 PlatformIO 完整上传流程，不要混用 M1 和新版本的分区/镜像。

修改启动画面 PNG 后，先运行：

```powershell
& '.\firmware\.venv\Scripts\python.exe' '.\firmware\tools\pack_splash.py'
```
