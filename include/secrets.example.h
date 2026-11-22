#pragma once
// 复制到同目录 secrets.h，仅在本机填写；不要提交真实凭据。
// 填妥后删除下一行。普通构建不额外强制热点密码长度，遵循ESP32接口规则。
#error "Fill local include/secrets.h and remove this line; build-only uses synthetic configuration"
// 默认STA网络：SSID为空则不启动默认STA；设备已保存的NVS设置优先。
#define STA_SSID ""
#define STA_PASS ""
// 热点：填写自己的名称；开放热点或加密热点由操作者自行选择并评估风险。
#define AP_SSID ""
#define AP_PSW ""
