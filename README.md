# EasyREAD

**简体中文** | [English](README.en.md)

面向 **Seeed Studio reTerminal Sticky** 的离线阅读固件。把电子书放入 microSD 卡，即可通过触屏或实体按键阅读，无需 Wi-Fi 或账户。

![EasyREAD](assets/branding/easyread-logo-512.png)

## 功能

- 480 × 800 竖屏阅读，支持不带 DRM 的 EPUB。
- 支持 TXT、XTC、XTCH 和 BMP；新增格式通过 74 项自动检查及 TXT 分页/续读回归，完整设备验收仍待完成。
- 简体中文、繁体中文、英文系统界面；支持中英混排正文。
- 书架、目录、章节跳转、阅读进度保存及断点续读。
- 字号、行距、边距、简繁地域字形按书保存；语言、自动休眠和蓝牙开关在系统设置中管理。
- PNG/JPEG 图片与封面，支持放大、缩小和拖动查看。
- 触屏与实体按键操作；提供 BLE HID 翻页器支持，实物兼容性仍待完整验证。

## 快速开始

**Windows 用户，三步开始阅读：**

1. **下载解压**：[Windows 入门包](https://github.com/FaiyuetCik/EasyREAD/releases/download/share-2026-10-10.1/EasyREAD-Windows-Starter-2026-10-10.1-r2.zip)，完整解压到电脑。
2. **准备书卡**：把包中 `SD-Card` 内的 **Fonts、Books** 复制到 **FAT32** microSD 卡根目录，已附测试书。
3. **点击安装**：USB 数据线连接 Sticky，打开 **EasyREAD-Setup.exe**，确认设备并点击安装；完成后插卡开机阅读。

适用于 Windows 10/11 x64，内置运行环境，无需安装 Python 或输入命令。安装界面可切换中文 / English。自己的电子书放入卡里的 `Books`。

首次安装会重置设备内部进度和设置；已有 EasyREAD 可选“保留设置升级”，工具先核对已有布局，不匹配则停止。安装时保持 USB 连接。找不到设备时检查数据线及 USB 驱动。程序尚未签名，请从本项目下载并核对 Release 校验值。

其他系统或手动烧录请看 [高级安装与构建](docs/SHARING.md)。入门包附完整源码、许可证和重新链接材料；现有固件镜像未改动。

## 操作

| 场景 | 触屏 | 实体按键 |
| --- | --- | --- |
| 书架与菜单 | 第一次点选中，黑色条移动到该项；再次点同一项执行。上下滑动滚动长列表 | 上下键选择，短按 AI 确认 |
| 正文 | 左侧上一页、右侧下一页；中间打开阅读菜单，点中间的图片可查看图片 | 上键上一页，下键下一页；AI 打开菜单 |
| 图片 | 点中间打开图片菜单；先点选项选中，再点同一项执行；滑动移动视区 | AI 打开图片菜单，方向键移动视区 |
| 返回 | 在菜单中选择写明目标的“返回”选项，再点一次确认 | 选中返回项后按 AI |

不需要快速双击。屏幕底部为提示区域，点击不返回。长按 AI/电源键休眠；按 AI/电源键唤醒。

完整操作说明见 [用户指南](docs/USER_GUIDE.md)。

## 当前版本与已知限制

**2026-10-10.1 开发预览**：新增 TXT、XTC、XTCH、BMP，附八本原创测试书；修复系统设置中蓝牙、语言和休眠编辑页标题错位。保留三种系统语言、按书设置及两次点按交互。

当前应用已编译并烧录到开发设备；SD 挂载、中文正文、基本翻页及部分菜单跳转已有硬件记录。以下仍需验证：全部语言与触摸边界、BLE 实物配对/重连、完整睡眠唤醒、功耗及长期稳定性。用户曾报告切章时偶发出现启动图，原因尚未定位。

本版支持无 DRM EPUB，以及以下新增格式：

| 格式 | 支持范围 |
| --- | --- |
| TXT | UTF-8、GBK、带 BOM 的 UTF-16 LE/BE；自动识别常见章节标题，复用排版和进度保存；单文件最多 128 MiB |
| XTC / XTCH | 未压缩 XTG 黑白页、XTH 双平面灰度页；按比例显示、翻页、内置目录、进度保存与缩放；灰度转换成黑白抖动 |
| BMP | 未压缩 RGB，1/4/8/24/32 位，按比例显示与缩放；单张图片作为一个书架条目 |

TXT 首次打开在 `/FM` 创建阅读缓存，原文件保持不变；首次转换需要空闲空间和等待时间。XTC/XTCH/BMP 的文字已固定为像素，不能调整字号和行距。图片边长最多 8192 像素，XTC 页解码还受可用 PSRAM 限制；不支持压缩 XTC 页、RLE 或 bitfields BMP。

SVG、GIF、WebP、交错 PNG、复杂表格、数学排版及全部 CSS 效果不在完整支持范围内。不提供 Wi-Fi 传书或横屏模式。

[项目进度](STATE.md) · [验证边界](docs/VALIDATION.md) · [发布包说明](docs/SHARING.md)

新增格式的自动测试与限制见 [测试报告](docs/FORMAT_TESTS-2026-10-10.md)。八本原创样书位于包内 `sd-card/Books/EasyREAD-Tests`；旧 `2026-10-10` 包不包含此次新增格式。

## 构建与修改

完整分享包附带固定依赖源码与便携构建配置：

```powershell
python -m pip install platformio==6.2.0
cd firmware
python -m platformio run -c platformio-share.ini -e sticky
cd ..
```

首次安装编译工具和平台支持包需要联网；Windows 建议解压到较短路径。源码仓库构建前还需恢复固定版本 FreeInk SDK，见 [依赖恢复](docs/GITHUB_PRIVATE.md)。

## 许可证与来源

- 原创应用代码、脚本、文档和原创设计素材采用 [MIT](LICENSE.md)，版权署名为 FaiyuetCik；具体范围见 [LICENSE_SCOPE.md](LICENSE_SCOPE.md)。
- 本项目基于 **FreeInk SDK**。第三方组件保留原版权、许可证和 NOTICE，详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) 与 [legal/](legal/)。
- Arduino-ESP32 核心继续采用 **LGPL-2.1-or-later**；完整分享包附带对应库源码、实际链接对象与重新链接说明。
- Noto 字体与 FaiyuetUI 衍生字体采用 **SIL OFL-1.1**，不归入应用 MIT 许可；原始版权和完整许可随包保留。
- Windows Georgia 字体文件不随项目分发；品牌素材中的固定文字由本地字体渲染。

发布或转发固件时请同时提供对应的源码、许可与构建/重新链接材料。历史 `Faiyuet-Morgan` 包仅对应历史版本。完整镜像与发布包校验值见 Release 附件。

仓库更名为 EasyREAD；旧本地目录名和历史附件名保留。当前仓库仍为 Private，只有获授权的用户可以访问。
