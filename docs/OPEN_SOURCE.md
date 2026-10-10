# 开源与发布检查

> 最新整体检查、Windows r2 许可补充及未完成事项见 [2026-10-10 审查报告](AUDIT-2026-10-10.md)。下文保留此前各次检查的历史范围与结论。

检查日期：2026-10-09。范围：项目原创源码、依赖版本记录、上游许可与 NOTICE、
字体和启动图来源、打包脚本、当前 Git 可达历史及本地历史分享 ZIP。
这是工程许可检查记录，不是对全部依赖、权利归属或法律合规的保证。

## 许可选择与文件分工

项目所有者选择允许商用、闭源修改和再分发，原创部分采用标准 MIT。
版权名按现有 Git 作者名写为 `Copyright (c) 2026 FaiyuetCik`。
若有雇佣、委托或共同作者的权利安排，须先确认自己有权许可相应内容；
Git 作者名本身不能证明版权归属。不要把别人的版权改成自己的名字。

- `LICENSE.md`：标准 MIT 全文，适用于本项目有权许可的原创部分。
- `LICENSE_SCOPE.md`：原创范围，以及第三方代码、字体、数据等例外。
- `THIRD_PARTY_NOTICES.md`：上游项目、固定版本、署名和许可选择。
- `legal/`：保留完整第三方许可与 NOTICE；摘要表不能替代这些正文。
- `sd-card/Fonts/LICENSE.txt`：阅读字体和 UI 字体衍生数据的 OFL 正文及版权。

MIT 允许商用和闭源修改，不要求每个使用者公开其应用源码。
分发副本或实质部分时仍须保留版权和许可声明。不要另加“禁止商用”或
“禁止再分发”等条款后仍称其为标准 MIT。
依据：[MIT 正文](https://opensource.org/license/mit)。

新增的原创 C/C++ 文件可使用以下头注释；它不能贴到第三方文件上来替换上游许可：

```cpp
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 FaiyuetCik
```

嵌入字体数据 `ui_font.h` 使用 `SPDX-License-Identifier: OFL-1.1`，
同时保留 Adobe 版权、衍生子集说明和完整许可位置。生成脚本本身为原创 MIT，
输出的字体数据仍为 OFL。当前子集改名为 FaiyuetUI；不要恢复受保护的保留字体名。
依据：[OFL 正文](https://openfontlicense.org/open-font-license-official-text/)。

## 第三方依赖的具体处理

| 组件 | 本项目应保留或提供的内容 |
| --- | --- |
| FreeInk SDK / OpenX4 驱动来源 | MIT 版权与全文、SDK NOTICE 中的社区来源署名；嵌入组件分别按自身许可处理 |
| Arduino-ESP32 3.3.7 核心 | LGPL-2.1-or-later 全文、使用提示、匹配版本的库源码和可修改库后重新构建/链接的材料 |
| NimBLE-Arduino 2.5.1 | Apache-2.0 全文、NOTICE 和嵌入组件署名；若修改上游文件，标明修改 |
| SdFat 2.3.1 | 保留 Bill Greiman 版权及 MIT 正文 |
| Noto SC / TC、FaiyuetUI 字体子集 | OFL 全文及原版权，字体衍生数据继续 OFL，不套用应用 MIT |
| FreeType | 当前选择 FTL；保留全文、版权和“基于 FreeType Team 工作”的二进制分发文档致谢 |
| Expat、miniz、pngle、TJpgDec、stb_truetype、libunibreak、Unicode 等 | 保留 `legal/` 中各自文本及源码内通知；不能只保留 SDK 的顶层 MIT |
| ESP-IDF、无线电二进制组件、工具链运行库 | 按实际链接内容保留对应许可、来源和运行库例外，新增依赖重新检查 |

依据：[SDK 固定版本 LICENSE](https://github.com/Free-Ink/freeink-sdk/blob/a1cf5b4be3e5259f856fe0497129e360e4e1346d/LICENSE)、
[SDK NOTICE](https://github.com/Free-Ink/freeink-sdk/blob/a1cf5b4be3e5259f856fe0497129e360e4e1346d/NOTICE)、
[Arduino 3.3.7 LICENSE](https://github.com/espressif/arduino-esp32/blob/3.3.7/LICENSE.md)、
[Apache-2.0 正文](https://www.apache.org/licenses/LICENSE-2.0)。

## 发布源码与发布固件的区别

原创应用可以采用 MIT，Arduino 核心继续采用 LGPL；不要求仅因链接 Arduino
就把原创应用改为 LGPL。分发包含该库的固件时，需要满足 LGPL 对对应库源码、
通知和重新链接的要求，并允许为个人修改而调试所需的逆向工程。
按当前静态链接方案，应提供实际版本的库源码，以及完整可构建的应用源码
和/或必要对象文件、构建脚本与链接材料，使接收者能修改库后生成新固件。
仅附一个固件 `.bin` 或笼统指向上游首页不够。

2026-10-09 重新构建了当前源码，并重新生成匹配的源码、对象、许可与构建材料，
记录在 `docs/SHARING.md` 和版本化的 `distribution/verification-2026-10-09.json`。
已有 2026-10-08 分享包保留为历史版本，不能代替本版对应材料。
未来新固件仍须重新核验 hash、源码版本、构建和重新链接，不能复用旧报告。

## 书籍、字体和设计素材

- 当前 Git 跟踪及 3 个可达提交中的 EPUB 文件名仅有原创测试书和合成测试夹具，
  未发现《悉达多》进入这些提交。测试夹具 `drm.epub` 是脚本生成的加密标记测试文件。
- 本地 `Faiyuet-Morgan-share-2026-10-08.zip` 中也仅发现上述 4 个 EPUB。
  这项检查核对了文件名和样例生成来源，不是所有压缩附件内容的版权鉴定。
- 旧 `package_release.py` 曾遍历整个 `sd-card/`，可能收录以后放入的私人书籍；
  本次已限制为字体目录与指定原创样例书，并忽略 `.partial` 下载残留。
  `.gitignore` 不控制 ZIP 打包；后续每次发布都要检查归档清单。
- `assets/branding/generate.py` 依赖 Windows Georgia 字体。Windows 字体生成的
  固定文字图片和 logo，在微软 FAQ 所述条件下可以随应用分发，不能据此分发
  Georgia 字体文件或制成可复用的字形库。目前没有把 Georgia 字体文件打进项目。
  为了跨平台重生成，建议以后改用许可清楚的 OFL 字体；本次没有改变启动图外观。
  依据：[微软字体 FAQ](https://learn.microsoft.com/en-us/typography/fonts/font-faq)。
- 品牌脚本绘制的几何图标未发现外部图标库引用。将来引入图标、插画或截图，
  需重新记录来源和许可；硬件厂商品牌不能被描述成对本项目的官方背书。

## 当前验证与公开前剩余事项

本次对当前可达的 3 个 Git 提交扫描了 104 个不同 blob；
对文本做私钥、常见 GitHub / AWS / OpenAI 密钥格式检查，未命中。
9 个二进制 blob 未做文本密钥扫描。该检查不覆盖所有凭据格式、不可达历史、
远端独有引用、外部附件或二进制内部秘密；不能据此宣称“无任何泄密风险”。
Git 提交作者邮箱也会随历史公开，公开前应确认接受。

本次更新了许可、范围、README、第三方声明与相关指南，并让打包工具附带范围说明。
Python 工具语法检查通过；`ui_font.h` 的字体字节未改变。
随后项目所有者确认原创部分由本人创作，并授权重新生成对应材料及推送 GitHub。
本次没有再次烧录设备；仓库继续保持 Private。构建与封包结果见版本化报告，
代码提交和 Release 上传结果以 GitHub 实际记录为准。

公开前还需：

1. 项目所有者已确认原创部分由本人创作；以后引入贡献或委托素材时重新核对授权。
2. 当前私有仓库已有历史 Release，旧附件保留其版本与许可状态；公开可见性前再核对。
3. 当前版本重新生成对应材料；以后修改固件后仍须重新生成并验证。
4. 本版便携配置使用包内源码，从新目录构建；不要求源码包使用者有 D: 盘。
   这只验证了本机 Windows 环境，不等于跨操作系统构建已经验证。
5. 保留已知问题和验证边界：切章偶发启动页尚未完成定位，不标成整机稳定性验收完成。

完成这些检查后，再单独执行仓库公开和 Release 发布。

## 2026-10-10 EasyREAD 源码推送检查补充

- 应用更名及系统语言、按书设置、菜单返回与两次点按交互已更新。原创部分仍按 MIT，原作者 FaiyuetCik 署名不随产品名变更。
- 生成的 FaiyuetUI 字体保留 OFL-1.1 头注释、Adobe 版权和完整许可路径；字体子集继续使用独立的衍生字体名。新增 EasyREAD 标识由原脚本生成，没有附带 Windows Georgia 字体文件。
- `legal/`、原始字体许可及第三方 NOTICE 保留；SDK 固定提交与依赖版本未变。
- 本次推送应用源码与生成素材，不上传新的固件二进制或发布 Release。历史发布包及其重构建、重新链接验证记录仅适用于历史版本；分发新版固件前须生成并验证匹配材料。

## 2026-10-10 新固件发布准备

- GitHub 仓库更名为 EasyREAD，保持 Private；README.md 与 README.en.md 提供中英文切换。
- 新包使用本次应用、固定依赖源码和实际链接输入，不复用旧固件包或旧测试报告来验证新版。
- 保留 MIT 全文与 FaiyuetCik 版权、FreeInk SDK/嵌入组件署名、Arduino LGPL、NimBLE Apache NOTICE、Noto OFL 与 Adobe 版权、工具链运行库许可。
- Noto 字体和衍生子集随完整 OFL 正文分发；子集名 FaiyuetUI 不使用保留字体名。Georgia 仅用于固定文字图像，不分发字体文件。
- 许可依据：[Arduino 3.3.7 LGPL 全文](https://raw.githubusercontent.com/espressif/arduino-esp32/3.3.7/LICENSE.md)、[OFL 正文](https://openfontlicense.org/open-font-license-official-text/)、[微软字体 FAQ](https://learn.microsoft.com/en-us/typography/fonts/font-faq)。
- 发布附件包括完整固件、匹配源码/依赖/许可/重新链接材料的 ZIP 和 SHA-256；验证结果以本版本报告为准，整机硬件验收仍未完成。
