# 分享包

本地完整包：`Faiyuet-Morgan-share-2026-10-08.zip`。
它包含固件、SD 卡内容、对应源码、第三方许可和通过验证的重新链接材料。
完整用法见 `../docs/SHARING.md`。请发送整个 ZIP，保留全部配套材料。

同一份包保存在个人私有仓库的 [分享版 Release](https://github.com/FaiyuetCik/Faiyuet-Morgan/releases/tag/share-2026-10-08)
中。Release 附件遵循仓库私有设置；接收者可以直接使用你发送的本地 ZIP，
无需访问私有仓库。大文件作为 Release 附件保存，Git 提交保留工具、说明和校验记录。

维护者重新打包顺序：

1. `firmware/tools/fetch_distribution_sources.py` 获取固定版本的源代码和许可。
2. 使用主项目 PlatformIO 配置构建，`export_relink.py` 自动保存实际链接参数。
3. `package_distribution.py` 整理分享包内容。
4. 使用 `relink_distribution.py --verify-original` 核对原始对象能够恢复相同 ELF，
   并按 SHARING.md 检验接收者配置；生成完整合并镜像。
5. `prepare_distribution_verification.py` 刷新材料、检查库源码一致性及构建记录；
   `seal_distribution.py` 生成 ZIP、逐文件 manifest 和 `verification.json`。
6. 上传新包和校验记录。修改源码/依赖后重新执行，不沿用旧包的验证结论。

当前仍是未经真机验收的软件预览，不是完成硬件测试的正式发布。
