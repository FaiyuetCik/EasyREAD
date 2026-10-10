# Windows 安装助手 / Windows setup

面向 Windows 10/11 x64，目标设备 Seeed Studio reTerminal Sticky。
界面原创代码采用项目 MIT 许可；esptool 上游源码为 GPL-2.0-or-later，r2 冻结程序选用 GPL-3.0-or-later。

普通用户下载 Release 的 `EasyREAD-Windows-Starter-2026-10-10.2.zip`，完整解压后打开
`EasyREAD-Setup.exe`，无需安装 Python、PlatformIO 或联网。
设备尚无 USB 驱动时仍需按 Seeed 官方设备说明安装对应驱动。

- 默认主操作“安装 EasyREAD”，无需选择安装方式。高级选项默认收起，收起时恢复普通安装。
- 安装 EasyREAD：固定 ESP32-S3，校验完整镜像后写入 0x0；明确提示重置 NVS。
- 高级选项 → 保留设置升级：先只读核对 bootloader、分区表、boot_app0；不匹配时停止，不自动降级成首次安装。
- 每次写入前核对 SHA256，esptool 校验烧录数据。设备型号由用户确认；USB VID 只用于优先选择候选端口。
- 不修改或格式化 SD 卡，不静默烧录；日志位于 `%LOCALAPPDATA%/EasyREAD/logs`。
- 安装时保持 USB 连接；失败查看日志并重新连接，按官方设备说明进入下载模式。

The bilingual setup UI detects serial ports, verifies payload hashes and invokes a separate
esptool process. The default action is Install EasyREAD; it resets internal settings and positions, but leaves SD card books unchanged. Keep-settings upgrades are under collapsed Advanced options and require exact
bootloader/partition/boot_app0 matches before any write. No automatic destructive fallback.
The SD card is prepared separately; drivers may still be needed on a new Windows computer.

固件 2026-10-10.2 全新安装默认英文；升级保留已有语言选择，SD 卡内容不变。
Firmware 2026-10-10.2 defaults to English on a fresh install; upgrades preserve saved language choices. SD card content is unchanged.

## Rebuild

Use Python 3.12 x64 on Windows. Build with the pinned environment in the distributed
`installer-sources/requirements-lock.txt`; `build_starter.py` packages the GUI and independent
esptool executable, collects dependency sources/licenses and includes the verified firmware
source/relink ZIP. See the script's `--help` for required inputs. Never overwrite an old package.

`EasyREAD-Setup.exe --self-check` verifies installation files and enumerates ports without
opening a device or writing flash, and saves `self-check.json` beside the executable.
Desktop UI and physical installation acceptance are recorded separately in STATE.md.

## r2 检查修订 / Audit revision

- 修正 esptool 5 的进度解析；校验清单必须包含所有必要镜像，运行库和工具 DLL 也纳入 SHA256。
- 日志磁盘写入失败时继续处理完成事件，不让界面永久停在安装中。
- 原 esptool 源码仍 GPL-2.0-or-later；冻结的 esptool 可执行程序选用 GPL-3.0-or-later，兼容所含 Apache-2.0 组件，包内提供 GPLv3 全文。
- 保留 cryptography/tibs 锁文件中全部 registry Rust crates 的源码、校验和许可；不改变 UI 的 MIT 或固件许可。
- 26 项安装助手自动检查通过，设备调用全部模拟；未执行助手实际烧录，不能代替真机安装验收。
- 保存微软运行库实际许可文档及可读全文，并保留 Python Windows 分发条件；安装前提供查看/接受入口，仅适用于微软组件，不改变其他开源许可。

Reproduce the read-only checks: `python -m unittest discover -s installer -p test_setup.py -v`.
To rebuild cryptography/tibs from source, use their included Cargo.lock files and the matching
archives in `rust-crates/` (checksums in sources.json); standard Rust/C build toolchains are
separate prerequisites. No installed dependencies are silently relicensed.
