# 小六壬 AI Passport 移植说明

本实现从原网页提取并保留了核心六宫顺序与凭数起卦算法：

`大安 → 留连 → 速喜 → 赤口 → 小吉 → 空亡`

## 硬件交互

- `OK`：开始、启动 3 秒麦克风采集、确认数字、查看解读。
- `UP/DOWN`：调整 1–99 的直觉数字。
- 结果页 `UP`：再问一事。
- 任意页面长按 `OK`：回到首页。
- 录音采用 320 samples 小块流式采集，只计算平均绝对振幅以判断是否收到有效声音；不联网、不转写、不保存原始音频。
- 音频初始化失败时应用仍可显示，但不会谎报问题已录入。

## 与网页版本的差异

设备没有触摸和键盘，也没有现成的联网 ASR。当前版本因此将问题作为“语音意念录入”，不会显示虚假的转写文本。原网页的分类识别、历史记录和详细传统材料暂未移植。时间起卦也暂未开放：基线固件没有可靠的已校时 RTC/NTP，直接使用系统时间会产生错误结果。

LVGL 默认 Montserrat 字体不含中文字形，所以屏幕使用拼音宫名和英文短解读；算法枚举与本文保留中文对照。若产品必须显示中文，应加入经过子集化的 CJK 字体资产，并重新核对 Flash、LVGL 内存池和字体缓存。

## 构建与测试

```sh
cc -std=c11 -Wall -Wextra -Werror -Imain tests/test_xlr_logic.c main/xlr_logic.c -o /tmp/test_xlr_logic
/tmp/test_xlr_logic

# ESP-IDF 5.5.3 环境中
idf.py set-target esp32c3
idf.py build
idf.py flash monitor
```

## 实机验收

1. 启动无重启、断言或看门狗复位，画面方向和颜色正确。
2. `UP/DOWN/OK` 均响应；长按 `OK` 在每个页面都回首页。
3. 安静录音显示 quiet；正常说话显示 audio received。
4. 连续录入 `1,1,1` 得到 `DA AN → DA AN → DA AN`。
5. 连续录入 `6,6,6` 得到 `KONG WANG → XIAO JI → CHI KOU`。
6. 连续完成 20 次录音和页面切换，无持续 heap 下降、任务泄漏或音频卡死。
