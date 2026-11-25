# 威哥 AI 智能打印助手 — P0 验证工程

独立源码副本，基于此前上传的小智工程提交 317677d。没有原 CW 的 .git 或远程配置。保留原 LCD/音频/配网。目标板禁用 CW 控制器，启动原生 USB Host 枚举诊断（WG_PRINT 串口日志）。未移植 LoveLH 打印代码，未实现 RAW 9100 或 Web 管理；不是可打印的固件。未编译验证。

在 D:\Android 解压，进入 WG-AI-PrintServer，建立独立 Git 仓库。ESP-IDF 5.5.2，执行 python scripts/release.py bread-compact-wifi-lcd --name bread-compact-wifi-lcd-240x240。或使用独立 GitHub Actions。先刷回退固件前备份现有打印配置；OTG 跳线短接后不要连接手机或电脑原生 USB，调试用 UART。

下一步需要 LoveLH 原版 v2.9.7 源码 ZIP，保留 LICENSE，移植 USB 打印及 TCP 服务，统一 WiFi，后续添加屏幕状态。
