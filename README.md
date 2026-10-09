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



## 许可证与个人私有仓库

原创部分暂未授予开源许可，范围见 `LICENSE.md`。FreeInk SDK 是 MIT 开源项目，
第三方来源和署名见 `THIRD_PARTY_NOTICES.md`，完整许可保存在 `legal/`。
个人 GitHub private 仓库准备与依赖恢复方法见 `docs/GITHUB_PRIVATE.md`。
私有保存不要求把原创应用公开；向他人分发固件前需另外核验 LGPL 等分发要求。

## 完整分享包

发送 `distribution/Faiyuet-Morgan-share-2026-10-08.zip`，其中附带对应源码、许可、
构建说明、实际对象文件及重新链接材料。具体用法见 `docs/SHARING.md`。
原创部分允许副本接收者为自己使用而修改、调试和重新构建，完整权限范围见 `LICENSE.md`。
