# 大肥鱼语音桌宠：XIAO 1.14 英寸 ESP32-S3 Plus

本工程基于已有 1.14 英寸大肥鱼动画工程，保留屏幕动画，增加板载 PDM 麦克风、Wi-Fi、网关请求与 I2S 播放。语音供应商尚未选定，网关实现的是可替换的 OpenAI 兼容 ASR / TTS 接口。没有 CAN 或触摸屏功能。

## 操作

- USR1：切换角色。
- USR2：录制固定 3 秒，发送一次语音请求。录音期间请说话。
- USR3：打开或关闭屏幕背光。
- 右上角圆点：蓝色空闲、红色录音、黄色等待、绿色播放、紫色错误。配置为空或联网失败时不能发起语音。

交互是按键触发、固定时长、按键式对话；网关为每台设备保留最近 6 轮上下文。当前不支持唤醒词、连续录音或字幕。

## 硬件

XIAO 1.14 英寸 ESP32-S3 Plus 显示板自带麦克风，扬声器需要外接 I2S 数字功放（例如 MAX98357A）。按 Seeed 官方接法：D11/GPIO38 -> DIN，D12/GPIO39 -> BCLK，D13/GPIO40 -> LRC；接通电源、地和功放的扬声器端。PDM 麦克风使用 D0/GPIO1（CLK）、D1/GPIO2（DATA）。屏幕和麦克风、扬声器尚未实板测试。

## 配置设备

ESP-IDF 5.5.2，目标 ESP32-S3，16 MB Flash、8 MB Octal PSRAM。使用 `idf.py menuconfig` 在 `Dafeiyu voice` 下设置 Wi-Fi SSID、密码、网关 URL 和可选网关令牌，然后 `idf.py build`、`idf.py -p COMx flash monitor`。SSID 和密码会进入本地 sdkconfig 和固件，切勿将包含真实凭据的文件提交或发布。首次生成默认配置时这些字段为空，不会连接网络。

网关 URL 必须是开发板实际可访问的地址，例如 `http://192.168.x.x:8080/voice`；127.0.0.1 对开发板指向它自己。推荐使用可信证书的 HTTPS 反向代理。明文 HTTP 会暴露语音内容与设备令牌，只适合可信隔离局域网调试。

## 网关：图形化填写 URL 和 Key

1. 双击项目根目录下的 `start_gateway.bat`。网关会启动，并自动打开本机设置页 `http://127.0.0.1:8081/`。保持启动窗口运行。
2. 在页面分别填写 ASR、DeepSeek 和 TTS 的 **URL、模型和 API Key**；TTS 还需填写供应商要求的音色。DeepSeek 的默认地址和模型已填好。默认推荐硅基流动的 TeleSpeechASR 和 CosyVoice2，ASR 与 TTS 可填写同一个硅基流动 API Key；DeepSeek 使用自己的 API Key。
3. 可以填写完整接口 URL；填写以 `/v1` 结尾的标准基础地址时，程序会自动补上相应的 `/audio/transcriptions` 或 `/audio/speech`。DeepSeek 基础地址自动补上 `/chat/completions`。
4. 在“开发板连接”中选择“局域网设备可访问”、设置设备令牌并保存，然后关闭网关窗口并重新启动。开发板设置中的网关 URL 填 `http://电脑的局域网IP:8080/voice`，令牌与页面设置相同。

密钥保存在 `gateway/config.local.json` 中，明文保存在本机，不在页面回显。文件已加入 `.gitignore`，不要将它分享、提交或上传。输入框留空并保存会保留原密钥，点“清除”并保存可删除；服务 URL 和密钥更改会用于下一次对话。监听地址和端口的更改需要重启。原来的环境变量仍支持，且优先级高于页面保存的配置；页面会提示哪些项被环境变量覆盖。

网关需要 Python 3.11；启动脚本优先使用本机现有 ESP-IDF Python 3.11，其他电脑需自行安装 Python 3.11。也可在项目根目录执行 `python gateway/server.py --open`。

ASR 需要兼容 OpenAI multipart 音频转写接口；TTS 需要兼容 OpenAI speech 接口并能返回 16 位 PCM WAV。供应商接口若采用不同的协议、鉴权或格式，仍需修改 `gateway/server.py` 中对应适配器。设备与网关之间的明文 HTTP 仅适合可信局域网，公网应使用 HTTPS 反向代理；开发板应配置可信证书。

网关 `/voice` 接收 16 kHz 单声道 16 位小端 PCM（最长 6 秒），返回同格式 PCM；设备发送 3 秒。语音和回复不写入磁盘；上下文仅保存在网关内存，重启后清空。

## 测试状态

网关单元测试：`python -m unittest discover -s gateway -p 'test_*.py'`，使用模拟服务，不会访问云端。真实 ASR、DeepSeek、TTS、Wi-Fi、麦克风、放音和显示方向需要配置真实服务及实板验证。

原动效版说明见 [README_ANIMATION.md](README_ANIMATION.md)，素材来源和许可见 [ASSET-LICENSE.txt](ASSET-LICENSE.txt)。`firmware_brush_demo/` 是旧动效版固件，不包含语音功能；语音版请按本说明重新配置并构建。
