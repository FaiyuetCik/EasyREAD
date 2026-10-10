# 固件开发

此目录是 EasyREAD 自有固件原型。`freeink-sdk/` 为 MIT 许可的第三方依赖，检出提交 `a1cf5b4be3e5259f856fe0497129e360e4e1346d`；应用入口是 `src/main.cpp`。

## 当前原型

应用已实现屏幕书架、递归扫描、EPUB 分页/缓存/文字与图片、目录与章内锚点、位置保存、排版设置、触控/实体键/BLE HID 操作、图片查看器和休眠唤醒流程。需从项目 `sd-card/` 复制 Fonts 和 Books 至 FAT32 卡根目录。完整操作说明见 `../docs/USER_GUIDE.md`。M1 原型源代码保留在 `archive/main-m1.cpp`。

固件的屏幕方向由 `platformio.ini` 中 `FM_SPLASH_ROTATE_CW` 控制，当前设为 1。2026-10-09 已统一 `ReaderGeometry` 的手绘映射与 SDK 文字渲染方向；触摸使用已实测翻页的独立校准。完整四角/所有菜单的硬件回归仍待验证。

## 构建

在 PowerShell 中从项目根目录运行：

```powershell
$env:PLATFORMIO_CORE_DIR = 'D:\fm-pio-core'
& '.\firmware\.venv\Scripts\python.exe' -m platformio run -d '.\firmware' -e sticky
```

`platformio.ini` 将构建目录和依赖缓存放在 D 盘短路径，使用 8 MB OPI PSRAM 和 3 MB app0 分区。NimBLE 固定为 2.5.1；SDK 文件未修改。2026-10-09 当前应用已烧入并通过 hash 校验，SD 与中文阅读通过硬件检查；切章偶发启动页仍待确认。`artifacts/software-preview-2026-10-08/` 是旧版产物：full.bin 从 0x0 烧录，app.bin 仅从 0x10000 烧录。优先使用 PlatformIO 完整上传流程，不要混用 M1 和新版本的分区/镜像。

修改启动画面 PNG 后，先运行：

```powershell
& '.\firmware\.venv\Scripts\python.exe' '.\firmware\tools\pack_splash.py'
```

新增或修改固定 UI 文案后，重建嵌入的中文/ASCII 字体（需要 fonttools）：

```powershell
& '.\firmware\.venv\Scripts\python.exe' '.\firmware\tools\build_ui_font.py'
& '.\firmware\.venv\Scripts\python.exe' '.\firmware\tools\embed_ui_font.py'
```

原始字体及 OFL 许可在 `../sd-card/Fonts/`，静态子集重命名为 FaiyuetUI。分段排版会话必须持有整个会话期间有效的参数、文件名和 arena；不要在仍然排版时重置这些内存。
