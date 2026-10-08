# 软件验证记录

日期：2026-10-08。

## 已运行

- PlatformIO sticky 环境固件编译、链接与镜像生成成功；使用 pioarduino 55.03.37、Arduino 3.3.7、NimBLE-Arduino 2.5.1。
- 原创测试 EPUB：两章、简体/繁体/英文混排、粗体/斜体、章内目录锚点、PNG 和 JPEG 图表。
- 实际 FreeInkBook/FreeInkFont 桌面流水线：26 和 34 像素两种字号，总计 93 页全部分页、写缓存、读缓存、渲染成功；无缺字。
- 章内 `middle` 锚点在重新分页后保持相同文字偏移，并恢复到覆盖该文字偏移的页面。
- 简体和繁体 TTF 的流式初始化及字形覆盖通过。
- DRM 返回 Encrypted；截断文件和垃圾文件返回 NotZip。
- 应用共用 ReaderGeometry 映射：两种竖屏方向共 768,000 个像素地址与触控逆变换检查通过，全部落在帧缓冲范围内且一一对应。
- BLE SDK：265 checks, 0 failed；使用假的 NimBLE/FreeRTOS 后端，验证协议解析和生命周期，没有连接实体翻页器。
- 阅读预览经过目视检查，中英混排、两种字号、粗体/斜体和图片可见。

典型测试峰值：布局 arena 112,391 字节；解析 arena 45,824 字节。固件实际预留更大空间以应对书籍差异。

## 复现

在 firmware/.venv 中安装 fonttools、Pillow；构建用 PlatformIO。样书生成：

```powershell
& '.\firmware\.venv\Scripts\python.exe' '.\firmware\tools\make_sample.py'
```

桌面检查在 WSL 的 g++/gcc/Python 3 下运行：

```powershell
wsl --exec python3 '/mnt/c/Users/seeed/Desktop/Faiyuet Morgan/firmware/tools/run_host_check.py'
wsl --exec python3 '/mnt/c/Users/seeed/Desktop/Faiyuet Morgan/firmware/tools/run_ble_check.py'
```

结果：`firmware/test-results/reading-check.txt`、`ble-check.txt`、`build-final.txt`；PBM 页面与 PNG 预览在同一目录。

## 尚未证明

没有烧录、硬件输入/刷新/共享 SPI、真实 BLE 配对、深度休眠电流或续航记录。桌面检查覆盖引擎和共用坐标逻辑，不代表所有应用菜单/NVS/SD/电源路径均已经在设备上执行过。复杂 EPUB 与最大规模性能仍需样书和硬件测试。

## 依赖与排障记录

SDK 固定为 a1cf5b4be3e5259f856fe0497129e360e4e1346d，未修改 SDK 文件。其 BLE 回调不兼容 NimBLE 2.3.8，升级到 2.5.1 后编译通过。其 FreeType 配置不包含 CFF，采用 Noto 简繁中文 TrueType 字体。

本机 PlatformIO 的 verbose 镜像生成输出曾触发 SCons `_Null` TypeError；普通构建已成功。构建路径和库缓存继续放在 D 盘短路径，未改 Windows 系统设置。

## 再次检验与修订

2026-10-08：重新运行阅读检查（93 页）、坐标检查（768,000 个映射）与 BLE 模拟（265 项），全部通过。代码审查发现并修正切换书籍后首次分页失败的保存问题：新书位置在尝试分页前初始化，成功加载章节前禁止写入新书进度。分页完成后重置自动休眠计时。此异常分支经过代码核查和固件编译，尚未在硬件上注入 SD 写失败来验证。

修订版应用 1,172,480 字节；静态 RAM 89,296 字节、程序 Flash 1,171,975 字节。修订构建记录见 recheck-fixed-build.txt（并同步到 build-final.txt）。固件镜像及分区核查、逐文件包校验脚本为 tools/verify_release.py；结果保存在 test-results/recheck-package.json。manifest 同时记录自身应用源码与 platformio.ini 的 SHA-256，以检查源码/配置与交付版本是否一致。
