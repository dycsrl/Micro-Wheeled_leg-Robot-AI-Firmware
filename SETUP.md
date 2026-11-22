# 初始化

按 README.md 的快速开始安装 requirements-build.txt、执行check_source和仅编译；普通构建先在本机填写include/secrets.h。首次PlatformIO构建需要联网下载锁定工具包。第三方源码已供应，不能用同版本未补丁库替换。

不包含：真实网络配置、设备NVS、checkpoint、IDE配置、构建缓存、日志和旧Git历史。模型参数与合同已经供应，无需读取旧工程。secrets.h须使用模板本地重建；checkpoint不是源码编译依赖。仅编译产物不适合直接烧录。运行前先读硬件/保护边界。
