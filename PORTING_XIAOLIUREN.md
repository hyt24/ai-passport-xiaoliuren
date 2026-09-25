# 小六壬 AI Passport 移植说明

保留六宫顺序：大安 → 留连 → 速喜 → 赤口 → 小吉 → 空亡。凭数与时间两种方式共用 `xlr_cast_numbers()` 的逐宫计数算法。

## 交互

- 首页 `OK` 进入问事；按住默念，松开进入方式页。麦克风不可用时可直接按 `OK` 继续。
- 方式页 `UP`：凭数，依次选择三个 1–9 的数字；`DOWN`：时间起卦。
- 时间页 `DOWN` 直接进入统一扫码配网，安卓和 iPhone 均扫码加入设备热点，在弹出的网页选择家中 Wi-Fi 并输入密码。`OK` 显示备用网址二维码；`UP` 退出并释放配网资源。
- 结果页 `UP/DOWN` 翻阅四页解析；长按 `OK` 再问。其他页面长按 `OK` 返回首页，默念过程中除外。
- 音频仅进行本地音量检测，不联网转写、不保存原始音频。中文使用子集化文渊宋体。

## 校时与日期规则

复用 ESP-IDF `esp_netif_sntp`，配置 `ntp.aliyun.com` 和 `pool.ntp.org` 两台服务器，`CONFIG_LWIP_SNTP_MAX_SERVERS=2`。仅收到有效 SNTP 回调才认为本次启动已校时；不会因为系统年份看起来正常就放行。使用单调时间判断距上次校时是否超过 24 小时。连接或校时等待超过 60 秒时提供失败提示和重试，后台晚到的成功仍可恢复。

Wi-Fi 断开后最多自动重连五次，失败后可手动重试或重新配网。凭据以单一 NVS blob 保存，不因 NVS 初始化失败自动清空设备数据。配网页仅允许从设备 AP 地址访问；使用随机 WPA2 热点密码及表单令牌，先解码再校验 SSID/密码字节长度，支持中文 SSID。热点在校时成功或退出时间页时关闭。配网使用本地 HTTP，NVS 沿用当前固件默认存储配置。

时间按 UTC+8 换算，与系统时区设置无关。输入顺序为农历月份、农历日、时辰序号（子=1 … 亥=12）。23:00 进入子时，00:00 更换日期；闰月沿用其月份序号。确认时重新取时并冻结结果，浏览解析不再重算。支持 2024–2099 年，不实现其他换日流派或真太阳时。

日期回归样例参考香港天文台：[2026 年公历农历对照表](https://www.hko.gov.hk/en/gts/time/calendar/pdf/files/2026e.pdf)、[2025 年年历](https://www.hko.gov.hk/en/gts/astron2025/files/HKO_almanac_2025.pdf)。

## 可重复验证

```sh
cc -std=c11 -Wall -Wextra -Werror -Imain tests/test_xlr_logic.c main/xlr_logic.c -o /tmp/test_xlr_logic
/tmp/test_xlr_logic
cc -std=c11 -D_DARWIN_C_SOURCE -Wall -Wextra -Imain tests/test_xlr_time.c main/xlr_calendar.c main/xlr_logic.c main/xlr_net_form.c main/lunar.c -lm -o /tmp/test_xlr_time
/tmp/test_xlr_time
python3 tests/test_xlr_network.py
cc -std=c11 -DLV_CONF_SKIP=1 -DLV_USE_QRCODE=1 -Imanaged_components/lvgl__lvgl/src/libs/qrcode tests/test_xlr_qr.c managed_components/lvgl__lvgl/src/libs/qrcode/qrcodegen.c -o /tmp/test_xlr_qr
/tmp/test_xlr_qr
# macOS：使用系统 Vision 独立解码测试二维码
swift -module-cache-path /tmp/xlr-swift-cache tests/decode_xlr_qr.swift

# ESP-IDF 5.5.3 环境
idf.py build
```

网络测试用主机桩运行生产事件处理逻辑，覆盖 NVS 失败、有限重试、连接/NTP 超时、有效回调、断网及 24 小时失效；它不模拟真实射频、DNS 或路由器。日期测试覆盖农历新年、闰六月、全部时辰以及 23:00/00:00/01:00 边界，表单测试覆盖中文和非法输入。

字体增量生成：`python3 scripts/build_font.py /path/WenYuanSerifSC-Regular.ttf /path/lv_font_conv.js`。保留已有字形，再加入界面所需字符。

## 仍需实机验收

1. 冷启动无 Wi-Fi 时凭数模式可用，时间模式不允许直接起卦。
2. 手机连接屏幕上显示的热点和密码，中文 SSID、特殊字符密码可成功配网；错误密码有失败提示，可重填恢复。
3. 仅连接路由器、阻断 UDP 123 时不会谎报校时成功；60 秒后可重试。恢复网络后可完成校时。
4. 校时后热点消失，屏幕北京时间与手机一致，时间确认结果按月/日/时辰推演。退出时间页时热点也应关闭。
5. 重启自动连接但必须再次校时；断网后已校时值可短期使用，超过 24 小时不可继续时间起卦。
6. 反复配网、重试、录音、切换页面 20 次，检查最低剩余 heap、按键响应、字体显示及重启情况。
7. 屏幕无缺字、溢出；配网密码、网址和底部操作提示完整可读。

已通过 `/dev/cu.usbmodem1101` 烧录到 ESP32-C3 revision v1.1 / 8MB Flash 设备；bootloader、分区表和应用均通过工具哈希验证，随后自动重启。以上屏幕、真实网络和稳定性验收仍待完成。已有发布版本 `v0.1.0` 未变更。

## 联网校时基础版验证结果（扫码功能加入前）

ESP-IDF 5.5.3 / ESP32-C3 构建通过。应用镜像 1,700,288 字节，应用分区剩余 46%；静态 D/IRAM 使用 155,324 字节（48.3%），余下 165,972 字节还需供运行时堆和任务使用，不等同于实机可用 heap。起卦、日期/表单、网络状态主机测试通过；日期/表单 AddressSanitizer 与 UndefinedBehaviorSanitizer 检查通过。中文界面字形覆盖检查通过。真实射频连接与运行时内存仍待实机验收。

烧录应用 SHA-256：`99306ed76fbef2e52a8e17350b526abd1fe530730470689a8b90fa9121d297e6`。本次未执行整片擦除，写入地址为 0x0、0x8000、0x10000。

配网页访问修复：ESP-IDF 双栈 HTTP 服务将 IPv4 地址返回为 IPv4-mapped IPv6。访问检查现同时接受热点的原生 IPv4 和映射地址；主机回归覆盖 GET 页面、POST 提交、非热点地址与截断地址拒绝。修复版已重新烧录并通过哈希校验；手机页面实际打开和联网校时仍需继续验收。

## 扫码配网开发版

复用 ESP-IDF DPP enrollee 和 LVGL 内置二维码，不引入新依赖。安卓支持时扫描 DPP 码，经系统确认传递已保存的网络配置；iPhone/不支持的安卓使用两张通用二维码，仍需填写家中密码。普通相机能否启动 Easy Connect 取决于手机厂商。详细操作见 README。

DPP 仅在用户选中时初始化，采用 SDK 生成的临时密钥，监听信道 6；收到凭据后共用 NVS/连接/校时路径。重试重新生成码，退出或校时成功释放 DPP 资源。主动配网期间忽略旧网络的断线重连及晚到的校时完成事件。等待超过 120 秒提示重试或使用通用方式。二维码仅编码 DPP 公钥引导信息或设备热点凭据，不编码家中 Wi-Fi 密码。

主机网络桩测试覆盖 DPP 码生成事件、凭据接收、保存后连接、超时/重试、晚到事件、退出与通用回退。二维码编码测试使用项目内的同一编码器，验证 160px 图案在白色底板中保留至少四模块留白。手机实际扫描、系统授权、DPP 交互成功率和运行时最低 heap 仍需实机验收。

三种测试二维码（热点、网址、DPP 引导码）已通过 macOS Vision 独立解码比对。码图为 160px，白色底板为 208px；仅使用虚构测试凭据。扫码新版构建期间设备曾断开；随后检测到设备重新接入，已通过 `/dev/cu.usbmodem1101` 烧录更新，三个镜像均通过哈希校验并自动重启。

扫码新版最终 ESP-IDF 5.5.3 构建通过，应用大小 1,789,536 字节，SHA-256 `d3bbbd246ccb6c6306003dc1826965792ff3f2584ad0e35681760a0da8143c3e`。网络/DPP 回归测试及三类二维码独立解码通过；已烧录并通过工具哈希校验；手机端 Easy Connect 扫码授权和实际联网仍待验收。

扫码新版静态 D/IRAM 使用 156,332 字节（48.7%），剩余 164,964 字节供运行时使用；不等同于实际剩余 heap。

### 配网 Wi-Fi 列表

手机加入设备热点并打开 `http://192.168.4.1` 后，设备扫描附近的 2.4GHz 网络，最多列出 20 条扫描记录中的不同 Wi-Fi 名称。选择家中网络、填写密码，再点击“保存并连接”。同名网络合并；可点击“重新扫描”，隐藏网络或扫描失败时可选“手动填写 / 隐藏网络”。不会自动选中陌生网络，也不会读取手机保存的 Wi-Fi 密码。

### 单次扫码配网

加入设备热点后，通过 AP 专用 DNS 和 HTTP 302 重定向引导系统打开配网页；保留手动网址与备用二维码，系统未触发弹窗时使用。DNS 仅绑定 AP 地址，退出或校时成功时关闭并释放任务和 socket。没有接管 HTTPS。手机弹窗行为仍需 iPhone 实机验收。

验证：`python3 tests/test_xlr_network.py` 覆盖 AP 访问限制、带正文的跳转和服务生命周期；`python3 tests/test_xlr_portal.py` 使用 ASan/UBSan 检查 DNS A/AAAA、截断、压缩名、多问题及随机异常报文。

统一入口更新：已移除应用中的手机类型选择页与 Android 专用入口；底层 DPP 实现保留但不由当前界面调用。前文扫码开发记录中的 Easy Connect 入口属于旧版。

## v0.2.0 发布整理

统一手机配网入口，删除未使用的硬件演示及像素 UI，将投币实验移出小六壬构建（本地文件保留）。中英文 README 已整理为应用说明。ESP-IDF 5.5.3 构建和起卦、历法、网络、DNS 主机检查通过；静态 D/IRAM 156348 字节（48.7%）。用户已确认清理前同功能固件配网可用，清理后的发布构建未再次做完整上板验收。

完整固件 `xiaoliuren-v0.2.0.bin` 从 `0x0` 烧录，SHA-256：`3441590599deee0baa147f8b6e27e4bb73204009173c80d7b761d3a98646fb34`。完整镜像不含私人凭据，NVS 区域为空填充，安装后重新配网。
