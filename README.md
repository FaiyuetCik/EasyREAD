# Faiyuet Morgan

为 Seeed Studio reTerminal Sticky 开发的离线 EPUB 阅读固件，并适配现有的 Page Turner 蓝牙翻页器。

## 已确定的产品范围

- 设备：Sticky，默认竖屏 480 × 800。
- 书籍：用户自行复制到 microSD 卡；只支持不带 DRM 的 EPUB。
- 语言：简体中文、繁体中文、英文和中英混排。
- 内容：文字、封面、图表、插画；图像按比例显示，图表可放大查看。
- 阅读：书架、目录、阅读进度、断点续读、字体与排版设置。
- 操作：触控、Sticky 实体按键、Page Turner 蓝牙翻页器。左/右键翻页；Sticky 实体 AI/电源键唤醒。
- 离线运行；不做 Wi-Fi 传书；不做横屏。

## 当前状态

2026-10-09：已在 Sticky 烧录并确认 SD 挂载、中文正文与翻页。新增分段分页、字宽/字体扇区缓存、中文菜单、黑底选中样式和明确的设置按钮；手绘元素与文字的方向已统一。93 页阅读回归、768,000 个坐标检查及真实书籍两种字号共 778 页分页一致性检查通过。用户曾报告切章时偶发启动页，尚未完成定位；不能据此声称整机稳定性已验收。进度详见 `docs/STATE.md`，操作说明见 `docs/USER_GUIDE.md`，验证边界见 `docs/VALIDATION.md`。

把 `sd-card/Fonts` 和 `sd-card/Books` 复制到 FAT32 microSD 根目录。`firmware/artifacts/software-preview-2026-10-08/` 中的镜像是历史软件预览；今天的版本请从当前源码构建，M1 旧产物保留。私人 EPUB 不上传仓库。

## 设计文件

- `assets/branding/faiyuet-morgan-logo.svg`：可编辑的矢量标识。
- `assets/branding/faiyuet-morgan-logo-512.png`：黑白图标预览。
- `assets/branding/faiyuet-morgan-sticky-splash-480x800.png`：Sticky 竖屏启动画面草稿。
- `assets/branding/generate.py`：重新生成 PNG 和 SVG 的脚本。



## 许可证与第三方来源

原创应用代码、脚本、文档、原创设计素材和测试样例采用 [MIT](LICENSE.md)，
适用范围及例外见 [LICENSE_SCOPE.md](LICENSE_SCOPE.md)。
本项目基于 FreeInk SDK；第三方来源与署名见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)，
完整许可保存在 `legal/`。Arduino 核心仍采用 LGPL，Noto 字体及嵌入式子集仍采用 OFL。
发布前检查见 [docs/OPEN_SOURCE.md](docs/OPEN_SOURCE.md)，依赖恢复见 `docs/GITHUB_PRIVATE.md`。
仓库目前保持 private；许可证选择不会自动改变仓库可见性。

## 完整分享包

当前完整包为 `distribution/Faiyuet-Morgan-share-2026-10-09.zip`，附带匹配的源码、
依赖、MIT 与第三方许可、实际链接对象及构建/重新链接材料，详见 `docs/SHARING.md`。
2026-10-08 包保留为历史版本，不能代替当前固件对应材料。
