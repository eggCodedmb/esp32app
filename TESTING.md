# ESP32App 测试文档

## 1. 测试范围
本项目提供以下模块的单元测试：
- `AuthService`
- `ConfigStore`
- `WakeOnLanService`
- `HostProbeService`
- `PowerOnService`
- `WifiService`
- `WebPortal`

## 2. 测试环境
- PlatformIO
- 测试环境：`esp32dev_test`
- 框架：Arduino
- 测试框架：Unity

`platformio.ini` 中已新增专用测试环境，关键配置：
- `test_build_src = true`
- `build_src_filter = +<*> -<main.cpp>`

用途：测试时构建业务源码并排除运行入口 `main.cpp`，避免与 Unity 测试入口冲突。

## 3. 测试文件
- `test/test_auth_service/test_main.cpp`
- `test/test_config_store/test_main.cpp`
- `test/test_host_probe_service/test_main.cpp`
- `test/test_power_on_service/test_main.cpp`
- `test/test_wake_on_lan_service/test_main.cpp`
- `test/test_wifi_service/test_main.cpp`
- `test/test_web_portal/test_main.cpp`

## 4. 各模块测试项

### 4.1 AuthService
- 默认账号密码校验是否正确
- 修改密码异常分支校验：
  - 当前密码错误
  - 新密码过短
  - 新旧密码相同
- 修改密码成功后是否可被新实例读取（持久化校验）
- Session Token 长度与会话 TTL 基础校验

### 4.2 ConfigStore
- 默认配置读取校验
- 配置保存后读取一致性校验（含 MAC 字段）

### 4.3 WifiService
- 空 SSID 输入拒绝校验
- 扫描接口返回容器有效性校验
- 连接状态访问器安全性校验

### 4.4 WakeOnLanService
- 非法 MAC 地址拒绝校验
- 合法 MAC 格式路径校验

### 4.5 HostProbeService
- 非法 IP 探测失败校验
- 端口为 0 探测失败校验

### 4.6 PowerOnService
- 未连接 WiFi 拒绝开机校验
- MAC 未配置拒绝开机校验
- IP 非法拒绝开机校验
- 请求开机后进入 `BOOTING` 状态校验
- 开机中 WiFi 断开进入 `FAILED` 状态校验

### 4.7 WebPortal
- 布尔解析辅助函数校验
- JSON 转义辅助函数校验
- 登录页关键元素校验
- 控制页关键元素校验（WOL 开机接口、MAC 配置与改密区域）

## 5. 执行命令

仅构建测试（不上传、不执行）：

```powershell
C:\Users\25547\.platformio\penv\Scripts\platformio.exe test -e esp32dev_test --without-uploading --without-testing
```

在连接板卡后执行全部测试：

```powershell
C:\Users\25547\.platformio\penv\Scripts\platformio.exe test -e esp32dev_test
```

执行单个模块测试：

```powershell
C:\Users\25547\.platformio\penv\Scripts\platformio.exe test -e esp32dev_test -f test_auth_service
```

## 6. 当前验证状态
- 已完成本地测试构建验证（Build-only）：
  - `test_auth_service` 构建通过
  - `test_config_store` 构建通过
  - `test_host_probe_service` 构建通过
  - `test_power_on_service` 构建通过
  - `test_wake_on_lan_service` 构建通过
  - `test_web_portal` 构建通过
  - `test_wifi_service` 构建通过

说明：Build-only 只验证编译与链接，运行时断言结果需在已连接 ESP32 的情况下执行测试命令确认。
