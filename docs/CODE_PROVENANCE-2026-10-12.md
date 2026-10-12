# EasyREAD 代码来源核对（2026-10-12）

## 结论和处理

在本次固定上游范围内，没有发现整份应用或大段阅读器实现直接复制的证据。
SD 适配器有短段相同/改名后的相似代码；增量分页流程与 freeink-reader 类似，
两者共用 FreeInk SDK。证据不足以还原每行创作来源，不能只凭相似度认定侵权。

保留本项目 MIT 版权；对相似的存储/增量缓存接入另行审慎保留
**Copyright (c) 2026 Justin Mitchell** 和 freeink-reader 完整 MIT 文本。
不能用 SDK 的 **Copyright (c) 2026 FreeInk** 代替。这不认定整个应用是上游
分支，也不把其他代码认证为绝对原创。

XTC 参考补留仓库 MIT 全文及文档指向的 CrazyCoder 来源链接，不复制文档正文/
表格；字体、GBK、库和安装工具仍分别遵循原许可。

## 固定基线及方法

| 对象 | 版本及范围 |
| --- | --- |
| EasyREAD | `69094f5950d9615a6c06fc1b54b846584d88f589`；16 个可达提交；44 个非生成代码/配置文件、4,894 行；74 个历史文件版本 |
| freeink-reader | `ca1c53d04e7db0dfcafe7e9dab1414445b33e990`；完整获取的 12 个可达提交，含初始 freeink-books 名称 |
| FreeInk SDK | `a1cf5b4be3e5259f856fe0497129e360e4e1346d`；固定检出，Git 跟踪文件无修改；本地单提交浅检出，不声称核查 SDK 全历史 |
| epub-to-xtc-converter | `a5719c6d4f64c52675b55889b18dae9877a9e9d6`；当前实现/格式文档，不含其全部历史 |

逐段阅读应用、存储、分页、字体缓存、TXT 导入、图片书模块；逐文件核对
脚本/安装程序的职责、标注与历史。另作去注释/预处理行的词元搜索，以及
统一标识符后的结构搜索，人工分类输出候选。

- 参考语料 380 个文件版本；精确阈值 20 词元，结构阈值 40 词元。
- 每个本地版本/模式最多输出 12 个去重候选；超过 1,000 个参考位置的高重复
  窗口跳过。生成数组及 SDK third_party 不参与作者归属匹配，另按来源/许可处理。
- 10 个历史版本有精确候选、8 个有结构候选。这不是抄袭文件数或相似率。
- [源码清单和候选](provenance/2026-10-12-candidates.json)保存 Git blob、SHA256、
  固定提交和行号；行号对应基线及候选具体上游 blob，不一定是上游 HEAD。

早期 STATE 记载已查看 SDK/reader，能证明参考关系，不能定位具体语句来源。
首个提交已含阅读功能，之前的逐次编辑过程未在 Git 保存，不补造创作记录。
“未发现匹配”均限本次语料/方法，不等于已证明原创。

## main.cpp 逐段记录

行号为检查基线，不含新增声明注释。

| 行号 | 内容、判断和处理 |
| --- | --- |
| 1–100 | SDK、帧缓冲、Arena、View、设备对象；共享 SDK，上游用 FreeInkApp/FreeInkUI；保留 SDK 许可 |
| 101–183 | pixel/text/fit/header/row 手工 UI；未发现整段匹配；坐标语义记录 SDK 来源 |
| 184–232 | 流式 FtFont、字体链、扇区缓存；上游用 TtfFont/PSRAM 缓冲；未发现大段复制；字体仍 OFL |
| 233–372 | 三语言字典、设置结构和 NVS 接入；未发现大段匹配；项目 MIT |
| 373–452 | 设置、位置、书架扫描、LayoutParams；通用扫描/API；本项目 NVS 与上游 SD 不同 |
| 453–600 | 等待页、章节缓存、后台排版；共同 reader/writer 水位、字符锚点、suspend 策略，实际包装/错误处理不同；审慎保留 reader 声明 |
| 601–693 | 指纹、TXT 派生 EPUB、XTC/BMP 打开及位置校验；项目接入，模块见下表 |
| 694–768 | 翻页、切章、保存、BLE 退出/深睡眠；SDK 接入；未发现大段匹配 |
| 769–1180 | 菜单、返回、按键、两次触控确认；手工 View，不使用上游 action 系统；未发现大段复制 |
| 1181–1436 | 缩放、阅读/目录/菜单渲染；标准 Bayer 矩阵与 SDK 相同；核心渲染属 SDK，不应宣称自写 EPUB/字体引擎 |
| 1437–1582 | SD/SPI、字体/触控初始化、循环、BLE 映射、休眠；共享 SDK；短触控声明候选属通用接入 |

## 逐文件记录

路径相对项目根目录；“无匹配”意为未发现达到阈值的参考实现候选。

| 文件 | 来源判断及处理 |
| --- | --- |
| `firmware/src/main.cpp` | 上表；MIT + SDK 与审慎保留的 reader 声明 |
| `firmware/src/Storage.h` | readAt/size 与 reader 相似，部分同见 SDK host 示例；保留两组 MIT |
| `firmware/src/IncrementalChapter.h` | SDK session/writer/reader 包装；流程类似 ReaderSession，无阈值以上匹配；审慎保留参考许可 |
| `firmware/src/ReaderGeometry.h` | 变换遵循 SDK PageRenderer，触控设备校准；补 SDK 来源 |
| `firmware/src/LayoutMetricsCache.h` | 度量缓存；无匹配；MIT |
| `firmware/src/FontReadCache.h` | 512 字节缓存与 EOF；无匹配；MIT |
| `firmware/src/TextImport.cpp` | 多编码、ZIP、TXT→EPUB；上游直接 plain-text layout；无匹配；MIT，映射另为 PSF |
| `firmware/src/TextImport.h` | 导入接口；无匹配；MIT |
| `firmware/src/RasterBook.cpp` | XTC/XTCH/BMP；参考格式；精确候选仅标准 Bayer 矩阵；MIT + 参考记录 |
| `firmware/src/RasterBook.h` | 图片书接口；无匹配；MIT |
| `firmware/archive/main-m1.cpp` | 历史启动/SD 原型；无匹配；MIT |
| `firmware/platformio.ini` | 平台/USB/屏幕选项，22 词元短匹配；同板同 SDK；保留参考 |
| `assets/branding/generate.py` | 几何及 Georgia 固定文字；无匹配；MIT，不分发字体文件 |
| `firmware/tools/audit_distribution.py` | 包/白名单/重链接检查；无匹配；MIT |
| `firmware/tools/build_gbk_table.py` | CPython codec 映射；脚本 MIT、输出 PSF，版权/许可已保留 |
| `firmware/tools/build_ui_font.py` | fontTools 子集/实例化；脚本 MIT、输出 OFL |
| `firmware/tools/copy_format_samples.py` | 白名单及复制校验；无匹配；MIT |
| `firmware/tools/embed_ui_font.py` | 字节转换/OFL 头生成；脚本 MIT、输出 OFL |
| `firmware/tools/export_relink.py` | SCons 参数导出；无匹配；MIT |
| `firmware/tools/fetch_distribution_sources.py` | 固定归档/许可下载；无匹配；脚本 MIT，下载内容保留自身许可 |
| `firmware/tools/format-host/Arduino.h` | std::string 适配；无匹配；MIT |
| `firmware/tools/format-host/SDCardManager.h` | fstream 适配；无匹配；MIT |
| `firmware/tools/format-host/esp_heap_caps.h` | 分配替身；无匹配；MIT |
| `firmware/tools/format_host_check.cpp` | 格式调用/PGM 输出；无匹配；MIT |
| `firmware/tools/host_check.cpp` | SDK API 驱动检查，与 SDK 测试目标类似，无连续匹配；保留 SDK 许可 |
| `firmware/tools/incremental_check.cpp` | host 适配、缓存/分页检查；无匹配；MIT + SDK |
| `firmware/tools/make_format_samples.py` | 文本/图案/容器生成；常量遵循规范；无转换器实现匹配；MIT |
| `firmware/tools/make_sample.py` | 双语 EPUB、图案和损坏输入；无匹配；MIT |
| `firmware/tools/pack_splash.py` | 项目 PNG 打包；无匹配；MIT |
| `firmware/tools/package_distribution.py` | 来源/许可、链接输入封包；无匹配；MIT |
| `firmware/tools/package_release.py` | 历史预览封包；无匹配；MIT |
| `firmware/tools/prepare_distribution_verification.py` | 来源核对/刷新；无匹配；MIT；本次补递归声明复制 |
| `firmware/tools/relink_distribution.py` | 原参数链接 ELF；无匹配；MIT |
| `firmware/tools/run_ble_check.py` | 调用 SDK 自带测试，不将测试归为原创；启动脚本 MIT、测试按 SDK 许可 |
| `firmware/tools/run_format_check.py` | 格式/像素对照及标准 Bayer；无转换器匹配；MIT |
| `firmware/tools/run_host_check.py` | host 构建/运行；无匹配；MIT |
| `firmware/tools/run_incremental_check.py` | host 构建/临时数据；无匹配；MIT |
| `firmware/tools/seal_distribution.py` | 核对后的清单封包；无匹配；MIT |
| `firmware/tools/use_bundled_arduino.py` | 可编辑 Arduino 路径覆盖；无匹配；脚本 MIT、库 LGPL |
| `firmware/tools/verify_release.py` | 历史预览检查；无匹配；MIT |
| `installer/build_starter.py` | 冻结工具、来源/许可封包；无匹配；MIT，组件分别许可；本次补声明复制 |
| `installer/easyread_setup.py` | Tk UI/校验/独立 esptool 进程；无匹配；MIT |
| `installer/esptool_entry.py` | 六行 GPL 启动器；已有 GPL-2.0-or-later；不归入笼统 MIT |
| `installer/test_setup.py` | 流程模拟；无匹配；MIT；本次未运行 |

### 生成数据/外部依赖

- ui_font.h、ui-subset.otf、Noto：OFL；Adobe/Source 保留名、FaiyuetUI 修改名及许可保留。
- GbkTable.h：CPython codec 数据，PSF 版权/Python 全文保留，不是 MIT 数据。
- splash.h：项目 PNG 生成；未发现纳入上游 reader 图标；不授权 Georgia 字体分发。
- SDK third_party、Arduino、NimBLE、SdFat、ESP-IDF、工具链/运行库未逐行判定
  原创，沿用各自许可和既有匹配源码/重新链接材料；不替代之前分发审计。
- 项目样书有生成脚本/白名单；目录中用户书籍继续忽略，不纳入声明补充包。

## 候选人工分类

1. Storage 当前 readAt/size 有约 55 个统一标识符词元与 reader 相似，历史版约
   60 个；精确候选 20–23 词元，部分同见 SDK 示例。不是独占来源证据，审慎保留 reader 声明。
2. main/RasterBook 标准 Bayer 矩阵与 SDK FreeInkUIDisplayTarget 一致，49 精确
   词元；不能由此认定复制了显示引擎。
3. main 的 charForAnchor/hashPath、闭合括号/触控声明 20–21 词元候选是通用接入；
   跨函数括号匹配不当作大段借用。
4. PlatformIO 22 词元选项属于共享硬件/SDK 配置。
5. IncrementalChapter 无阈值以上连续匹配，人工仍看到相似策略，一并保留参考。
6. 其余文件无达到阈值候选，不作原创认证。未覆盖互联网全部代码、商标、专利或书籍授权。

## 交付、复核及发布状态

- 本地补两份 MIT 全文/来源、源码注释、中英文 README、范围/第三方声明与 STATE。
- 新工具 audit_code_provenance.py 在基线之后新增，不计入 44 个输入；只读 Git。
- 固件封包原已递归复制 legal，刷新工具补递归 docs/legal；starter 另带声明目录，
  并复制到对应构建源码。打包修改尚未构建新固件/安装 EXE。
- 本地 `EasyREAD-attribution-supplement-2026-10-12.zip` 含声明、许可、审计及清单；
  随原匹配源码/重新链接包分发，不替代 LGPL 材料；外部记录为
  `distribution/attribution-supplement-2026-10-12.json`。
- 发布补充：声明 ZIP 和独立 SHA256 已附于 `share-2026-10-10.2` Release；
  原固件/安装包及历史校验值保持原状。源码同步状态见根目录 STATE。
- 补充包内报告及 `2026-10-12-verification.json` 是首次本地审计快照，
  其中“尚未上传”/`github_uploaded: false` 记录当时状态；当前发布状态以
  Release、根目录 STATE 和 `distribution/attribution-supplement-2026-10-12.json` 为准。
- 复核：许可字节一致、引用/文件存在、差异/注释范围、补充包清单/SHA256；
  未重新构建固件、刷机或运行功能测试。

复现时将本项目和上游分别检出至上表版本（路径自行替换）：

```powershell
python firmware/tools/audit_code_provenance.py --project . --reader D:/references/freeink-reader --sdk firmware/freeink-sdk --converter D:/references/epub-to-xtc-converter --output candidates.json
```

工具搜索执行时可达历史；后来新增提交会改变范围。SHA256 确定对象，不证明权属。
这是限定范围工程来源记录，不是法律裁决。

## English summary

Reviewed 44 tracked non-generated code/configuration files (4,894 lines), 74
historical EasyREAD versions, all 12 fetched freeink-reader commits, the pinned
SDK and converter reference. Small SD adapter matches, common SDK calls and a
standard Bayer matrix were identified; no wholesale application copying was
identified in this scope. This does not prove originality or every line's origin.

Justin Mitchell's separate MIT notice is conservatively retained for similar
storage/incremental-cache integration alongside FreeInk's SDK notice and
FaiyuetCik's copyright. The converter repository MIT text is retained; its external
reference is credited, not redistributed or assumed relicensed. Existing archives
predate these additions; the local supplement accompanies the original matching
source/relink package. The supplement and checksum are now attached to
`share-2026-10-10.2`; embedded audit records are pre-publication snapshots.
No new firmware/installer build occurred.
This is a technical provenance review, not a legal determination.
