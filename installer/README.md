# Windows 安装助手 / Windows setup

面向 Windows 10/11 x64，目标设备 Seeed Studio reTerminal Sticky。
界面原创代码采用项目 MIT 许可；独立子进程 esptool 遵循 GPL-2.0-or-later。

普通用户下载 Release 的 `EasyREAD-Windows-Starter-2026-10-10.1.zip`，完整解压后打开
`EasyREAD-Setup.exe`，无需安装 Python、PlatformIO 或联网。
设备尚无 USB 驱动时仍需按 Seeed 官方设备说明安装对应驱动。

- 首次安装：固定 ESP32-S3，校验完整镜像后写入 0x0；明确提示重置 NVS。
- 保留设置升级：先只读核对 bootloader、分区表、boot_app0；不匹配时停止，不自动降级成首次安装。
- 每次写入前核对 SHA256，esptool 校验烧录数据。设备型号由用户确认；USB VID 只用于优先选择候选端口。
- 不修改或格式化 SD 卡，不静默烧录；日志位于 `%LOCALAPPDATA%/EasyREAD/logs`。
- 安装时保持 USB 连接；失败查看日志并重新连接，按官方设备说明进入下载模式。

The bilingual setup UI detects serial ports, verifies payload hashes and invokes a separate
esptool process. First install resets internal settings. Keep-settings upgrades require exact
bootloader/partition/boot_app0 matches before any write. No automatic destructive fallback.
The SD card is prepared separately; drivers may still be needed on a new Windows computer.

## Rebuild

Use Python 3.12 x64 on Windows. Build with the pinned environment in the distributed
`installer-sources/requirements-lock.txt`; `build_starter.py` packages the GUI and independent
esptool executable, collects dependency sources/licenses and includes the verified firmware
source/relink ZIP. See the script's `--help` for required inputs. Never overwrite an old package.

`EasyREAD-Setup.exe --self-check` verifies installation files and enumerates ports without
opening a device or writing flash, and saves `self-check.json` beside the executable.
Desktop UI and physical installation acceptance are recorded separately in STATE.md.
