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

2026-10-08：书架、递归 EPUB 扫描、简繁中英阅读、PNG/JPEG、图片放大、目录、断点续读、排版设置、触控/实体键/BLE HID 翻页和深度休眠流程已写入，软件版已编译。93 页桌面阅读检查、265 项 BLE 模拟检查与竖屏坐标检查通过；尚未烧录或真机验证。进度详见 `docs/STATE.md`，操作说明见 `docs/USER_GUIDE.md`，验证边界见 `docs/VALIDATION.md`。

把 `sd-card/Fonts` 和 `sd-card/Books` 复制到 FAT32 microSD 根目录。软件版完整镜像和应用镜像位于 `firmware/artifacts/software-preview-2026-10-08/`，M1 旧产物保留。

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
