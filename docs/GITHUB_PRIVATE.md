# 个人 GitHub 私有仓库与许可证检查

检查日期：2026-10-08。项目目录：`C:\Users\seeed\Desktop\Faiyuet Morgan`。

## 检查结论

FreeInk SDK 已公开开源，采用 MIT；本项目固定到
`a1cf5b4be3e5259f856fe0497129e360e4e1346d`。上游也有 MIT 的
[FreeInk Reader](https://github.com/Free-Ink/freeink-reader)，但本项目直接使用的是 SDK。
SDK NOTICE 保留了 OpenX4 社区显示驱动及 CidVonHighwind 的来源署名。
MIT 允许在保留版权和许可声明的条件下用于私有应用，不要求把整个应用开源。

按个人保存、自己使用的用途，可以放到个人 GitHub private 仓库。
private 是访问设置，不会取消第三方许可义务。原创部分暂未选择开源许可，
根目录 LICENSE.md 只说明范围，没有擅自给你的原创代码授予 MIT。
第三方组件和字体按各自许可处理，详见 THIRD_PARTY_NOTICES.md 和 legal/。

Arduino-ESP32 核心使用 LGPL-2.1-or-later。如果以后向他人提供固件或设备，
需要进一步检查实际链接组件、对应库源码及可重新链接材料；不能把“仓库 private”
当作免除这些要求的理由。现在的包尚不是完成全部分发许可核验的公开发布包。
Noto 字体及嵌入式 UI 字体子集保留 OFL；OFL 不要求应用代码采用 OFL。

## 已补齐

- LICENSE.md：原创与第三方许可范围。
- THIRD_PARTY_NOTICES.md：开源来源、版本、署名和许可表。
- legal/：完整上游许可、NOTICE 及部分嵌入源码的许可段落。
- .gitignore：忽略虚拟环境、构建与测试产物、固件包、个人书籍、凭据及 SDK 本地克隆。
- firmware/dependencies.lock.json：SDK 固定提交和本次已使用的依赖版本。
- 发布包工具：附带上述许可说明和 legal/ 文本。

## 创建 private 仓库时

1. 在个人 GitHub 账户新建仓库，选择 **Private**；不选择 GitHub 自动生成的 MIT/GPL。
2. 上传本目录的源码、文档、legal/、必要字体和原创样例 EPUB。
   先检查将上传的文件；.gitignore 不会撤销已经被 Git 跟踪的文件，也不会拦截网页手工上传。
3. SDK 已是独立 Git 仓库，当前采用忽略本地克隆、记录固定依赖的方式，
   避免误把它当成一个没有 .gitmodules 的嵌套 gitlink。
   在新电脑从 firmware 目录恢复依赖：

```powershell
git clone https://github.com/Free-Ink/freeink-sdk.git freeink-sdk
git -C freeink-sdk checkout --detach a1cf5b4be3e5259f856fe0497129e360e4e1346d
```

4. 安装 PlatformIO 后按 firmware/platformio.ini 构建；当前配置的构建/依赖目录是
   D:\fm-pio-build 和 D:\fm-pio-libdeps，新电脑需有 D: 盘或调整到本机目录。
5. 上传后核对仓库标识为 Private。此次检查没有创建远程仓库、commit 或 push。

## 上游依据

- [FreeInk SDK LICENSE](https://github.com/Free-Ink/freeink-sdk/blob/a1cf5b4be3e5259f856fe0497129e360e4e1346d/LICENSE)
- [SDK NOTICE](https://github.com/Free-Ink/freeink-sdk/blob/a1cf5b4be3e5259f856fe0497129e360e4e1346d/NOTICE)
- [Arduino-ESP32 3.3.7 LICENSE](https://github.com/espressif/arduino-esp32/blob/3.3.7/LICENSE.md)
- [GNU 关于个人使用及 LGPL 静态链接的说明](https://www.gnu.org/licenses/gpl-faq.html)
- [Noto CJK OFL](https://github.com/notofonts/noto-cjk/blob/main/Sans/LICENSE)
- [GitHub 仓库许可说明](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository)
