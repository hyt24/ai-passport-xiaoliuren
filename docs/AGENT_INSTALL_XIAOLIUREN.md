# Agent 安装小六壬固件指南

本文给 AI agent 使用，用于协助用户把公开 Release 中的小六壬固件安装到 **FoloToy AI Passport（ESP32-C3）**。当前稳定版本为 [`v0.1.0`](https://github.com/hyt24/ai-passport-xiaoliuren/releases/tag/v0.1.0)。

## 安全边界

- 仅适用于已确认是 FoloToy AI Passport 的 ESP32-C3 设备；型号不明时不要烧录。
- 烧录会覆盖设备上的现有应用。必须在实际执行写入命令前，向用户说明此影响并取得明确同意。
- 不要使用 `erase_flash`，也不要猜测串口或替用户选择多个候选设备。
- 不上传、记录或展示用户的 Wi-Fi 密码、令牌或其他私密数据。
- 若设备正在供电不足、频繁断连或端口消失，停止操作并请用户处理连接问题。

## 安装流程

### 1. 确认目标和端口

先确认用户要覆盖设备当前固件，并确认设备已用 USB 连接。macOS 可列出候选串口：

```sh
ls /dev/cu.usbmodem*
```

只有一个候选端口时，仍应向用户说明将写入该端口；有多个候选端口时，请用户指定。常见端口形如 `/dev/cu.usbmodem101`。

### 2. 下载并校验固件

下载完整镜像。该文件已经包含 bootloader、分区表和应用，烧录地址固定为 `0x0`。

```sh
curl -L \
  https://github.com/hyt24/ai-passport-xiaoliuren/releases/download/v0.1.0/xiaoliuren-v0.1.0.bin \
  -o /private/tmp/xiaoliuren-v0.1.0.bin
shasum -a 256 /private/tmp/xiaoliuren-v0.1.0.bin
```

`v0.1.0` 的 SHA-256 必须为：

```text
35a1bdc2a527e3abf549f1ca7b6232b39545058072fad7bebf7fe0d2f72d7db5
```

校验失败时，删除该下载文件后重新下载；不要烧录校验不匹配的文件。

### 3. 最终确认后烧录

确认用户同意覆盖现有固件，并将 `<设备串口>` 替换为已确认的单一端口：

```sh
esptool --chip esp32c3 --port <设备串口> --baud 460800 \
  write_flash 0x0 /private/tmp/xiaoliuren-v0.1.0.bin
```

成功输出应包含 `Hash of data verified` 与 `Hard resetting via RTS pin`。烧录后设备会自动重启。

### 4. 最小验收

请用户确认以下项目：

1. 显示屏进入“小六壬”首页，未出现乱码或重启循环；
2. `UP`、`DOWN`、`OK` 三键可以响应；
3. 按 `DOWN` 能进入简介，按 `UP` 能打开键位说明；
4. 长按 `OK` 能进入默念提问，松开后可进入数字取卦。

## 常见失败处理

| 现象 | Agent 应做什么 |
| --- | --- |
| 找不到串口 | 请用户重新插拔数据线，确认不是仅充电线；不要猜测端口。 |
| 连接超时 | 请用户确认设备已连接、端口未被其他程序占用；必要时重新插拔后重试一次。 |
| 哈希不一致 | 重新下载并再次校验，禁止烧录。 |
| 烧录成功但屏幕异常 | 停止进一步写入，记录现象、设备型号和串口日志，再检查硬件版本兼容性。 |

## 对用户的完成说明

完成时应明确报告：Release 版本、固件 SHA-256、实际烧录端口、工具输出是否包含哈希验证，以及仍需用户手动确认的屏幕和按键验收项。
