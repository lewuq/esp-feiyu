# 已编译固件

使用 ESP-IDF 5.5.2 构建，目标 ESP32-S3，未在实板验证。链接时在隔离构建目录中对 objdump 输出做完整性校验和重试；这不是构建环境的永久修复。

在 ESP-IDF 终端进入此目录，将 COM端口 改为设备端口后执行：

```powershell
python -m esptool --chip esp32s3 -p COM端口 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 2MB --flash_freq 80m 0x0 bootloader.bin 0x8000 partition-table.bin 0x10000 esp_project.bin
```

该命令会写入开发板的引导程序、分区表和应用；实际屏幕方向、颜色、亮度与动效需要上板检查。
