[English](README.md) | 简体中文

# 小六壬 · 默念一问

把 FoloToy AI Passport 变成随身的问卦小册：默念一个问题，凭三个数字或当下时间起卦，在纸墨界面中翻读四页解析。

## 怎么玩

1. 首页按 `OK` 进入问事，按住默念，松开后选择起卦方式。
2. 按 `UP` 凭数起卦：用上下键选择 1–9，按 `OK` 确认，依次选出三个数字。
3. 按 `DOWN` 按时间起卦：首次在时间页按 `DOWN` 配网。**安卓和 iPhone 扫同一个码**，加入设备热点后，在弹出的页面选择家中 2.4GHz Wi-Fi、填写密码并保存。不弹页时打开 `http://192.168.4.1`；设备 `OK` 可显示备用网址二维码。
4. 校时成功后按 `OK` 起卦；上下键翻读解析，长按 `OK` 返回首页或在结果页再问。

凭数起卦无需联网。机器记住 Wi-Fi，重启自动重连；每次重启后按时间起卦需先成功校时，校时后离线可用最长 24 小时。时间采用北京时间与农历月、日、时辰，午夜换日、闰月沿用月数，支持 2024–2099 年。麦克风声音不保存、不上传。卦象只作思考参考。

## 下载安装

从 [GitHub Releases](https://github.com/hyt24/ai-passport-xiaoliuren/releases/latest) 下载 `xiaoliuren-v0.2.0.bin` 和 `SHA256SUMS`。完整固件**仅适用于 FoloToy AI Passport（ESP32-C3）**，从 `0x0` 烧录：

```sh
shasum -a 256 -c SHA256SUMS
esptool --chip esp32c3 --port <设备串口> --baud 460800 write_flash 0x0 xiaoliuren-v0.2.0.bin
```

安装会替换设备当前固件。完整镜像也会重置保存的 Wi-Fi，安装后重新扫码配网。使用 agent 安装可参考[安装指南](docs/AGENT_INSTALL_XIAOLIUREN.md)。

## 开发与验证

使用 ESP-IDF 5.5.3：

```sh
. /path/to/esp-idf-v5.5.3/export.sh
idf.py build
idf.py -p <设备串口> flash
```

主机回归：

```sh
python3 tests/test_xlr_network.py
python3 tests/test_xlr_portal.py
cc -std=c11 -Imain tests/test_xlr_logic.c main/xlr_logic.c -o /tmp/test_xlr_logic
/tmp/test_xlr_logic
cc -std=c11 -Imain tests/test_xlr_time.c main/xlr_calendar.c main/xlr_logic.c main/xlr_net_form.c main/lunar.c -lm -o /tmp/test_xlr_time
/tmp/test_xlr_time
```

覆盖计数、历法边界、配网表单、访问限制、校时和 DNS 异常报文；编译与主机测试不能代替屏幕、按键和手机配网实测。

## 代码导航

- `main/xlr_app.c`：页面与按键交互。
- `main/xlr_logic.c`、`main/xlr_calendar.c`：起卦和历法。
- `main/xlr_net.c`、`main/xlr_net_form.c`、`main/xlr_portal.c`：联网、校时与配网页。
- `components/bsp/`：硬件驱动、引脚定义。
- `tests/`：主机测试；`scripts/`：字体生成。
- [移植记录](PORTING_XIAOLIUREN.md)、[硬件开发指南](docs/AI_HARDWARE_DEVELOPMENT_GUIDE.md)、[协作规范](AGENTS.md)。

本仓库专注小六壬应用，已移除继承自开发模板的硬件演示页面和示例分支目录介绍。

## 致谢

交互与玩法参考了[小六壬网页项目](https://youvibe.run/s/zhgxVMPRMFb)，设备支持沿用 FoloToy AI Passport 开发基线。字体、素材与历法许可见[第三方声明](THIRD_PARTY_NOTICES.md)、[字体许可](FONT_LICENSE.md)和[历法许可](main/LUNAR_LICENSE.txt)。
