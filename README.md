# DS DAFEIYU — XIAO 1.14 英寸 ESP-IDF 动效演示

适用于 Seeed XIAO 1.14'' IPS Display (ESP32-S3 Plus)，ESP-IDF 5.5.2，135×240 竖屏。上电后显示参考图风格的黑底蓝色笔触动画，文字分为 `DS`、`DAFEI`、`YU` 三行，按估算的笔画路径依次显现，然后进入角色画面。

- USR1 / USR2：上一个 / 下一个角色，循环切换。
- 侧面 USR3：熄灭或点亮背光；再次亮屏时恢复配置亮度。熄屏时暂停刷新，芯片不进入深度睡眠。
- 8 个角色姿态搭配 4 种武器，持续进行浮动动画。

## 配置

在 `main/config/demo_config.h` 中调整：

```c
#define INTRO_SPEED_PERCENT 150       // 150 表示 1.5 倍速
#define DISPLAY_BRIGHTNESS_PERCENT 50 // 背光 PWM 占空比 50%
```

开机书写进度与时间成线性关系，基础书写时长 6600 ms、完成后停留 900 ms；150% 时分别为 4400 ms 和 600 ms，总计约 5 秒。参数必须为正整数。亮度允许 0–100，50% 表示 50% PWM 占空比，实际观感亮度取决于背光电路和屏幕。

## 代码目录

```text
main/
  app_main.c                 负责应用状态、事件和刷新调度
  config/demo_config.h       速度和亮度等可调参数
  config/board_config.h      GPIO、屏幕尺寸和偏移
  display/display.c          ST7789、SPI DMA、双缓冲和 PWM 背光
  input/buttons.c            三个按键的独立任务、去抖和事件队列
  ui/ui.c                    开机及角色画面的绘制和动画
  ui/graphics.c              像素、图形及透明图片混合
  resources/image_assets.c   图片资源及角色/武器组合索引
  assets/                    已转换并嵌入固件的二进制图像
assets/actors/               大肥鱼原始姿态图片（WebP）
assets/weapons/              原始武器图片（WebP）
references/brush_reference.png 用户提供的开机文字参考图
tools/convert_splash.py       根据参考图生成墨迹和书写顺序数据
tools/convert_assets.py       将角色和武器 WebP 转换为嵌入式数据
```

应用通过非阻塞状态更新完成动画；SPI DMA 传输和另一帧合成可以并行，按键扫描运行在独立任务中。图片资源由构建系统嵌入，不需要运行时文件系统。

## 编译与烧录

打开 ESP-IDF 终端，进入本项目，然后执行：

```powershell
idf.py build
idf.py -p COM端口 flash monitor
```

将 `COM端口` 换成设备的实际端口；退出监视器按 `Ctrl+]`。`sdkconfig` 使用 USB Serial/JTAG 输出日志，避免默认 UART0 占用 USR1/USR2 的 GPIO43/44。若原 `build` 缓存来自不同 ESP-IDF 路径，可以执行 `idf.py fullclean` 后重新构建。

## 硬件配置

| 信号 | GPIO | XIAO 引脚 |
|---|---:|---|
| LCD CS / DC | 3 / 4 | D2 / D3 |
| LCD SCK / MOSI | 7 / 9 | D8 / D10 |
| LCD RST / BL | 13 / 12 | D17 / D18 |
| USR1 / USR2 / USR3 | 43 / 44 / 11 | D6 / D7 / D19 |

ST7789 使用反色、RGB 顺序、RAM 偏移 `(52, 40)`。如画面偏移、上下颠倒或红蓝交换，请在实板上核对并调整 `main/config/board_config.h` 与 `main/display/display.c`。

## 素材生成

原始大肥鱼和武器图片分别保存在 `assets/actors`、`assets/weapons`；已转换的显示素材包含在 `main/assets` 中，不需要 Python 就能编译。需重新转换时，安装 Python Pillow 后运行：

```powershell
python tools/convert_splash.py
python tools/convert_assets.py
```

开机图使用 8 位墨迹强度和 8 位笔画顺序，字符轨迹是根据参考图估算的书写路径，并非真实字体笔画数据。角色和武器存为 RGB565 大端序加 8 位透明度。角色素材来源及许可见 `ASSET-LICENSE.txt`。

## 已编译固件

`firmware_brush_demo/` 包含 ESP32-S3 固件及烧录说明，构建验证使用 ESP-IDF 5.5.2。
