# GitHub 仓库与依赖恢复

更新日期：2026-10-09。

仓库：[FaiyuetCik/Faiyuet-Morgan](https://github.com/FaiyuetCik/Faiyuet-Morgan)。
目前可见性仍为 Private。按项目所有者选择，当前原创部分采用 MIT，见根目录
`LICENSE.md`；第三方许可边界见 `LICENSE_SCOPE.md`。
选择 MIT 不会自动改变仓库可见性，也不会替换第三方许可证。

## 恢复固定依赖

FreeInk SDK 固定到 `a1cf5b4be3e5259f856fe0497129e360e4e1346d`。
SDK 本地克隆不进入本仓库；在 `firmware` 目录运行：

```powershell
git clone https://github.com/Free-Ink/freeink-sdk.git freeink-sdk
git -C freeink-sdk checkout --detach a1cf5b4be3e5259f856fe0497129e360e4e1346d
```

安装 PlatformIO 后按 `firmware/platformio.ini` 构建。
当前配置使用 `D:/fm-pio-build` 和 `D:/fm-pio-libdeps`，
新电脑需要有 D: 盘或调整这两个目录。实际依赖版本记录在
`firmware/dependencies.lock.json`；该文件是版本来源记录，不是完整包管理锁文件。

## 公开之前

按 `docs/OPEN_SOURCE.md` 检查当前文件、提交历史和已有 Release 附件。
`.gitignore` 不会撤销已跟踪的文件，也不会拦截打包脚本读取本地文件。
历史分享包仍保存历史许可与历史固件；今天的 MIT 文档修改没有改写旧 ZIP。
发布新固件需要生成对应版本的源码、许可、构建和重新链接材料，见 `SHARING.md`。

仓库公开、提交推送、发布新版固件是分别执行的操作。项目所有者随后授权将
MIT 与第三方声明、匹配源码和构建材料推送，并上传 2026-10-09 新分享包。
本次保持 Private；上传结果以 GitHub 实际提交和 Release 为准。
