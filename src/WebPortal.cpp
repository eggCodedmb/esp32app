#include "WebPortal.h"

#include <cctype>
#include <cstdio>

namespace {
bool normalizeMacAddress(const String& source, String* normalized) {
  if (normalized == nullptr) {
    return false;
  }

  String compact;
  compact.reserve(12);

  for (size_t i = 0; i < source.length(); ++i) {
    const char c = source.charAt(i);
    if (c == ':' || c == '-' || c == ' ') {
      continue;
    }
    if (!std::isxdigit(static_cast<unsigned char>(c))) {
      return false;
    }
    compact += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }

  if (compact.length() != 12) {
    return false;
  }

  normalized->clear();
  normalized->reserve(17);
  for (size_t i = 0; i < compact.length(); ++i) {
    if (i > 0 && (i % 2) == 0) {
      *normalized += ':';
    }
    *normalized += compact.charAt(i);
  }

  return true;
}
}  // namespace

WebPortal::WebPortal(uint16_t port,
                     AuthService& authService,
                     WifiService& wifiService,
                     ConfigStore& configStore,
                     PowerOnService& powerOnService)
    : _server(port),
      _authService(authService),
      _wifiService(wifiService),
      _configStore(configStore),
      _powerOnService(powerOnService) {}

void WebPortal::begin() {
  registerRoutes();
  _server.begin();
}

void WebPortal::registerRoutes() {
  _server.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, false)) {
      return;
    }
    request->send(200, "text/html; charset=utf-8", dashboardPage());
  });

  _server.on("/login", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (_authService.isAuthorized(request)) {
      AsyncWebServerResponse* response = request->beginResponse(302);
      response->addHeader("Location", "/");
      request->send(response);
      return;
    }
    request->send(200, "text/html; charset=utf-8", loginPage());
  });

  _server.on("/login", HTTP_POST, [this](AsyncWebServerRequest* request) {
    const String username =
        request->hasParam("username", true) ? request->getParam("username", true)->value() : "";
    const String password =
        request->hasParam("password", true) ? request->getParam("password", true)->value() : "";

    if (_authService.validateCredentials(username, password)) {
      const String token = _authService.issueSessionToken();
      AsyncWebServerResponse* response = request->beginResponse(302);
      response->addHeader("Location", "/");
      response->addHeader("Set-Cookie",
                          "ESPSESSION=" + token +
                              "; Path=/; HttpOnly; SameSite=Lax; Max-Age=" +
                              String(_authService.sessionTtlSeconds()));
      request->send(response);
      return;
    }

    request->send(401, "text/html; charset=utf-8", loginPage("用户名或密码错误"));
  });

  _server.on("/logout", HTTP_GET, [this](AsyncWebServerRequest* request) {
    _authService.clearSession();
    AsyncWebServerResponse* response = request->beginResponse(302);
    response->addHeader("Location", "/login");
    response->addHeader("Set-Cookie", "ESPSESSION=deleted; Path=/; Max-Age=0");
    request->send(response);
  });

  _server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    const ComputerConfig config = _configStore.loadComputerConfig();
    const PowerOnStatus power = _powerOnService.getStatus();

    String body = "{";
    body += "\"computerName\":\"" + jsonEscape(config.name) + "\",";
    body += "\"computerIp\":\"" + jsonEscape(config.ip) + "\",";
    body += "\"computerMac\":\"" + jsonEscape(config.mac) + "\",";
    body += "\"computerPort\":" + String(config.port) + ",";
    body += "\"computerOwner\":\"" + jsonEscape(config.owner) + "\",";
    body += "\"powerState\":\"" + jsonEscape(power.stateText) + "\",";
    body += "\"powerMessage\":\"" + jsonEscape(power.message) + "\",";
    body += "\"powerBusy\":" + String(power.busy ? "true" : "false") + ",";
    body += "\"wifiConnected\":" + String(_wifiService.isConnected() ? "true" : "false") + ",";
    body += "\"wifiSsid\":\"" + jsonEscape(_wifiService.currentSsid()) + "\",";
    body += "\"wifiIp\":\"" + jsonEscape(_wifiService.ipAddress()) + "\"";
    body += "}";

    request->send(200, "application/json", body);
  });

  _server.on("/api/config", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    ComputerConfig config = _configStore.loadComputerConfig();
    if (request->hasParam("computerName", true)) {
      config.name = request->getParam("computerName", true)->value();
    }
    if (request->hasParam("computerIp", true)) {
      config.ip = request->getParam("computerIp", true)->value();
    }
    if (request->hasParam("computerOwner", true)) {
      config.owner = request->getParam("computerOwner", true)->value();
    }
    if (request->hasParam("computerPort", true)) {
      const int parsedPort = request->getParam("computerPort", true)->value().toInt();
      if (parsedPort > 0 && parsedPort <= 65535) {
        config.port = static_cast<uint16_t>(parsedPort);
      }
    }
    if (request->hasParam("computerMac", true)) {
      String normalizedMac;
      if (!normalizeMacAddress(request->getParam("computerMac", true)->value(), &normalizedMac)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"invalid_mac\"}");
        return;
      }
      config.mac = normalizedMac;
    }

    const bool saved = _configStore.saveComputerConfig(config);
    if (!saved) {
      request->send(500, "application/json", "{\"success\":false,\"error\":\"save_failed\"}");
      return;
    }

    request->send(200, "application/json", "{\"success\":true}");
  });

  _server.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    const WifiScanResult scanResult = _wifiService.scanNetworks();
    const std::vector<WifiNetworkInfo>& networks = scanResult.networks;
    String body = "{\"networks\":[";
    for (size_t i = 0; i < networks.size(); ++i) {
      body += "{";
      body += "\"ssid\":\"" + jsonEscape(networks[i].ssid) + "\",";
      body += "\"rssi\":" + String(networks[i].rssi) + ",";
      body += "\"secured\":" + String(networks[i].secured ? "true" : "false");
      body += "}";
      if (i + 1 < networks.size()) {
        body += ",";
      }
    }
    body += "],";
    body += "\"fromCache\":" + String(scanResult.fromCache ? "true" : "false") + ",";
    body += "\"scanInProgress\":" + String(scanResult.scanInProgress ? "true" : "false") + ",";
    body += "\"ageMs\":" + String(scanResult.ageMs) + ",";
    body += "\"connected\":" + String(_wifiService.isConnected() ? "true" : "false") + ",";
    body += "\"currentSsid\":\"" + jsonEscape(_wifiService.currentSsid()) + "\",";
    body += "\"ip\":\"" + jsonEscape(_wifiService.ipAddress()) + "\"";
    body += "}";

    request->send(200, "application/json", body);
  });

  _server.on("/api/wifi/connect", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    const String ssid =
        request->hasParam("ssid", true) ? request->getParam("ssid", true)->value() : "";
    const String password =
        request->hasParam("password", true) ? request->getParam("password", true)->value() : "";

    if (ssid.isEmpty()) {
      request->send(400, "application/json", "{\"success\":false,\"error\":\"ssid_required\"}");
      return;
    }

    const bool connected = _wifiService.connectTo(ssid, password);
    String body = "{";
    body += "\"success\":" + String(connected ? "true" : "false") + ",";
    body += "\"message\":\"" + jsonEscape(_wifiService.lastMessage()) + "\",";
    body += "\"ip\":\"" + jsonEscape(_wifiService.ipAddress()) + "\"";
    body += "}";

    request->send(connected ? 200 : 500, "application/json", body);
  });

  _server.on("/api/power/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    const PowerOnStatus status = _powerOnService.getStatus();
    String body = "{";
    body += "\"state\":\"" + jsonEscape(status.stateText) + "\",";
    body += "\"message\":\"" + jsonEscape(status.message) + "\",";
    body += "\"error\":\"" + jsonEscape(status.errorCode) + "\",";
    body += "\"busy\":" + String(status.busy ? "true" : "false");
    body += "}";
    request->send(200, "application/json", body);
  });

  _server.on("/api/power/on", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    const ComputerConfig config = _configStore.loadComputerConfig();
    String errorCode;
    const bool accepted = _powerOnService.requestPowerOn(config, _wifiService.isConnected(), &errorCode);
    _powerOnService.tick(_wifiService.isConnected());

    const PowerOnStatus status = _powerOnService.getStatus();
    const String responseError = errorCode.isEmpty() ? status.errorCode : errorCode;
    String body = "{";
    body += "\"success\":" + String(accepted ? "true" : "false") + ",";
    body += "\"state\":\"" + jsonEscape(status.stateText) + "\",";
    body += "\"message\":\"" + jsonEscape(status.message) + "\",";
    body += "\"error\":\"" + jsonEscape(responseError) + "\",";
    body += "\"busy\":" + String(status.busy ? "true" : "false");
    body += "}";

    int statusCode = 200;
    if (!accepted) {
      if (responseError == "wol_send_failed" || responseError == "udp_begin_failed") {
        statusCode = 500;
      } else {
        statusCode = 400;
      }
    }
    request->send(statusCode, "application/json", body);
  });

  _server.on("/api/auth/password", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    const String currentPassword = request->hasParam("currentPassword", true)
                                       ? request->getParam("currentPassword", true)->value()
                                       : "";
    const String newPassword =
        request->hasParam("newPassword", true) ? request->getParam("newPassword", true)->value()
                                               : "";
    const String confirmPassword = request->hasParam("confirmPassword", true)
                                       ? request->getParam("confirmPassword", true)->value()
                                       : "";

    if (currentPassword.isEmpty() || newPassword.isEmpty() || confirmPassword.isEmpty()) {
      request->send(400, "application/json", "{\"success\":false,\"error\":\"missing_fields\"}");
      return;
    }

    if (newPassword != confirmPassword) {
      request->send(400, "application/json", "{\"success\":false,\"error\":\"confirm_not_match\"}");
      return;
    }

    String errorCode;
    const bool changed = _authService.updatePassword(currentPassword, newPassword, &errorCode);
    if (!changed) {
      const int statusCode = errorCode == "persist_failed" ? 500 : 400;
      request->send(statusCode,
                    "application/json",
                    "{\"success\":false,\"error\":\"" + jsonEscape(errorCode) + "\"}");
      return;
    }

    request->send(200, "application/json", "{\"success\":true,\"relogin\":true}");
  });

  _server.onNotFound([this](AsyncWebServerRequest* request) {
    if (request->url().startsWith("/api/")) {
      request->send(404, "application/json", "{\"error\":\"not_found\"}");
      return;
    }
    request->send(404, "text/plain", "Not Found");
  });
}

bool WebPortal::ensureAuthorized(AsyncWebServerRequest* request, bool apiRequest) const {
  if (_authService.isAuthorized(request)) {
    return true;
  }

  if (apiRequest) {
    AsyncWebServerResponse* response =
        request->beginResponse(401, "application/json", "{\"error\":\"unauthorized\"}");
    response->addHeader("Set-Cookie", "ESPSESSION=deleted; Path=/; Max-Age=0");
    request->send(response);
  } else {
    AsyncWebServerResponse* response = request->beginResponse(302);
    response->addHeader("Location", "/login");
    response->addHeader("Set-Cookie", "ESPSESSION=deleted; Path=/; Max-Age=0");
    request->send(response);
  }
  return false;
}

String WebPortal::loginPage(const String& errorMessage) const {
  String page = R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 登录</title>
  <style>
    body { font-family: "Microsoft YaHei", Arial, sans-serif; background: #f3f4f6; margin: 0; }
    main { max-width: 360px; margin: 9vh auto; background: #fff; padding: 24px; border-radius: 12px; box-shadow: 0 8px 24px rgba(0,0,0,0.08); }
    h1 { margin-top: 0; font-size: 22px; }
    label { display: block; margin-top: 12px; font-size: 14px; }
    input { width: 100%; box-sizing: border-box; padding: 10px; margin-top: 6px; border: 1px solid #d1d5db; border-radius: 8px; }
    button { margin-top: 16px; width: 100%; padding: 10px; border: 0; border-radius: 8px; background: #2563eb; color: #fff; cursor: pointer; }
    .error { color: #b91c1c; margin: 8px 0 0; font-size: 13px; }
    .hint { margin-top: 14px; color: #6b7280; font-size: 12px; }
  </style>
</head>
<body>
  <main>
    <h1>ESP32 控制台登录</h1>
    __ERROR_BLOCK__
    <form method="post" action="/login">
      <label for="username">用户名</label>
      <input id="username" name="username" required>
      <label for="password">密码</label>
      <input id="password" name="password" type="password" required>
      <button type="submit">登录</button>
    </form>
    <p class="hint">默认账号：admin / admin123，建议首次登录后修改密码</p>
  </main>
</body>
</html>
)HTML";

  const String errorBlock = errorMessage.isEmpty() ? "" : "<p class=\"error\">" + errorMessage + "</p>";
  page.replace("__ERROR_BLOCK__", errorBlock);
  return page;
}

String WebPortal::dashboardPage() const {
  return R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 管理页面</title>
  <style>
    :root {
      --bg: #f4f6f8;
      --card: #ffffff;
      --line: #d1d5db;
      --text: #111827;
      --accent: #0f766e;
      --danger: #b91c1c;
    }
    * { box-sizing: border-box; }
    body { margin: 0; font-family: "Microsoft YaHei", Arial, sans-serif; color: var(--text); background: linear-gradient(160deg, #ecfeff 0%, #f8fafc 45%, #fff 100%); }
    main { max-width: 900px; margin: 24px auto; padding: 0 16px; }
    header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 14px; }
    h1 { margin: 0; font-size: 24px; }
    .logout { color: #fff; text-decoration: none; padding: 8px 12px; background: #111827; border-radius: 8px; }
    section { background: var(--card); border: 1px solid var(--line); border-radius: 12px; padding: 16px; margin-bottom: 14px; box-shadow: 0 8px 24px rgba(0,0,0,0.04); }
    h2 { margin-top: 0; font-size: 18px; }
    .row { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    label { display: block; margin-top: 8px; font-size: 14px; }
    input, select { width: 100%; padding: 10px; border: 1px solid var(--line); border-radius: 8px; margin-top: 6px; }
    button { margin-top: 12px; padding: 10px 12px; border: none; border-radius: 8px; cursor: pointer; background: var(--accent); color: #fff; }
    button:disabled { opacity: 0.7; cursor: not-allowed; }
    .muted { color: #4b5563; font-size: 13px; }
    .status { min-height: 20px; margin-top: 6px; font-size: 14px; }
    @media (max-width: 720px) {
      .row { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <main>
    <header>
      <h1>ESP32 控制面板</h1>
      <a class="logout" href="/logout">退出登录</a>
    </header>

    <section>
      <h2>远程开机（Wake-on-LAN）</h2>
      <p class="muted">开机状态：<strong id="powerState">待机</strong></p>
      <button id="powerButton" type="button">执行开机</button>
      <p id="powerStatus" class="status"></p>
    </section>

    <section>
      <h2>WiFi 连接</h2>
      <div class="row">
        <div>
          <label for="wifiSelect">附近网络</label>
          <select id="wifiSelect"></select>
          <button id="scanButton" type="button">扫描 WiFi</button>
        </div>
        <div>
          <label for="wifiSsid">WiFi 名称（SSID）</label>
          <input id="wifiSsid" autocomplete="off">
          <label for="wifiPassword">WiFi 密码</label>
          <input id="wifiPassword" type="password" autocomplete="off">
          <button id="connectButton" type="button">连接</button>
        </div>
      </div>
      <p id="wifiStatus" class="status"></p>
    </section>

    <section>
      <h2>计算机配置</h2>
      <form id="configForm">
        <div class="row">
          <div>
            <label for="computerName">计算机名称</label>
            <input id="computerName" name="computerName" autocomplete="off">
          </div>
          <div>
            <label for="computerIp">计算机 IP</label>
            <input id="computerIp" name="computerIp" autocomplete="off">
          </div>
          <div>
            <label for="computerMac">计算机 MAC 地址</label>
            <input id="computerMac" name="computerMac" placeholder="AA:BB:CC:DD:EE:FF" autocomplete="off">
          </div>
          <div>
            <label for="computerPort">探测端口</label>
            <input id="computerPort" name="computerPort" type="number" min="1" max="65535">
          </div>
          <div>
            <label for="computerOwner">使用者</label>
            <input id="computerOwner" name="computerOwner" autocomplete="off">
          </div>
        </div>
        <button type="submit">保存配置</button>
      </form>
      <p id="configStatus" class="status"></p>
    </section>

    <section>
      <h2>修改登录密码</h2>
      <form id="passwordForm">
        <div class="row">
          <div>
            <label for="currentPassword">当前密码</label>
            <input id="currentPassword" name="currentPassword" type="password" required>
          </div>
          <div>
            <label for="newPassword">新密码（至少 6 位）</label>
            <input id="newPassword" name="newPassword" type="password" required>
          </div>
          <div>
            <label for="confirmPassword">确认新密码</label>
            <input id="confirmPassword" name="confirmPassword" type="password" required>
          </div>
        </div>
        <button type="submit">更新密码</button>
      </form>
      <p id="passwordStatus" class="status"></p>
    </section>
  </main>

  <script>
    function setText(id, text) {
      document.getElementById(id).textContent = text || "";
    }

    async function api(url, options) {
      const response = await fetch(url, Object.assign({ credentials: "same-origin" }, options || {}));
      const text = await response.text();
      let payload = {};
      if (text) {
        try { payload = JSON.parse(text); } catch (_) { payload = { message: text }; }
      }
      if (!response.ok) {
        throw new Error(payload.error || payload.message || ("HTTP " + response.status));
      }
      return payload;
    }

    function toMessage(code) {
      if (code === "invalid_mac") return "MAC 地址格式错误，请使用 AA:BB:CC:DD:EE:FF。";
      if (code === "missing_fields") return "请完整填写密码字段。";
      if (code === "confirm_not_match") return "两次输入的新密码不一致。";
      if (code === "current_password_incorrect") return "当前密码不正确。";
      if (code === "password_too_short") return "新密码长度不能少于 6 位。";
      if (code === "password_not_changed") return "新密码不能与当前密码相同。";
      if (code === "persist_failed") return "保存失败，请稍后重试。";
      if (code === "unauthorized") return "登录已失效，请重新登录。";
      if (code === "ssid_required") return "SSID 不能为空。";
      if (code === "wifi_not_connected") return "WiFi 未连接，无法远程开机。";
      if (code === "config_mac_required") return "请先配置目标电脑 MAC 地址。";
      if (code === "config_ip_invalid") return "请先配置正确的目标电脑 IP 地址。";
      if (code === "wol_send_failed") return "发送开机包失败，请检查网络环境。";
      if (code === "udp_begin_failed") return "UDP 通道初始化失败。";
      if (code === "boot_timeout") return "等待主机上线超时。";
      if (code === "already_booting") return "开机流程进行中，请稍候。";
      if (code === "already_on") return "主机已经在线。";
      return code || "请求失败";
    }

    function stateLabel(state) {
      if (state === "BOOTING") return "开机中";
      if (state === "ON") return "已开机";
      if (state === "FAILED") return "失败";
      return "待机";
    }

    function updatePowerStatus(data) {
      const state = data.state || data.powerState || "IDLE";
      const message = data.message || data.powerMessage || "";
      const busy = !!data.busy || !!data.powerBusy;

      setText("powerState", stateLabel(state));
      setText("powerStatus", message);

      const button = document.getElementById("powerButton");
      button.disabled = busy;
      button.textContent = busy ? "开机中..." : "执行开机";
    }

    let wifiScanRequesting = false;
    let wifiScanInProgress = false;
    let wifiScanTimer = 0;

    function updateScanButtonState() {
      const button = document.getElementById("scanButton");
      button.disabled = wifiScanRequesting || wifiScanInProgress;
      if (wifiScanRequesting) {
        button.textContent = "请求中...";
      } else if (wifiScanInProgress) {
        button.textContent = "扫描中...";
      } else {
        button.textContent = "扫描 WiFi";
      }
    }

    function scheduleWifiScan(delayMs) {
      if (wifiScanTimer) {
        clearTimeout(wifiScanTimer);
      }
      wifiScanTimer = setTimeout(function () {
        wifiScanTimer = 0;
        scanWifi();
      }, delayMs);
    }

    function renderWifiOptions(networks) {
      const select = document.getElementById("wifiSelect");
      const ssidInput = document.getElementById("wifiSsid");
      const previousSelection = ssidInput.value || select.value || "";
      select.innerHTML = "";

      if (networks.length === 0) {
        const emptyOption = document.createElement("option");
        emptyOption.value = "";
        emptyOption.textContent = "未扫描到网络";
        select.appendChild(emptyOption);
        return 0;
      }

      networks.forEach(function (network) {
        const option = document.createElement("option");
        option.value = network.ssid;
        option.textContent =
            network.ssid + " (" + network.rssi + " dBm" + (network.secured ? "，加密" : "，开放") + ")";
        select.appendChild(option);
      });

      if (previousSelection) {
        const options = Array.from(select.options);
        const matched = options.find(function (item) { return item.value === previousSelection; });
        if (matched) {
          select.value = previousSelection;
        }
      }

      ssidInput.value = select.value || previousSelection;
      return networks.length;
    }

    async function loadConfig() {
      const data = await api("/api/config");
      document.getElementById("computerName").value = data.computerName || "";
      document.getElementById("computerIp").value = data.computerIp || "";
      document.getElementById("computerMac").value = data.computerMac || "";
      document.getElementById("computerPort").value = data.computerPort || "";
      document.getElementById("computerOwner").value = data.computerOwner || "";
      updatePowerStatus(data);

      if (data.wifiConnected) {
        setText("wifiStatus", "已连接：" + (data.wifiSsid || "") + "，IP：" + (data.wifiIp || ""));
      } else {
        setText("wifiStatus", "未连接 WiFi");
      }
    }

    async function refreshPowerStatus() {
      try {
        const data = await api("/api/power/status");
        updatePowerStatus(data);
      } catch (error) {
        setText("powerStatus", toMessage(error.message));
      }
    }

    async function powerOn() {
      try {
        const data = await api("/api/power/on", { method: "POST" });
        updatePowerStatus(data);
        if (data.error) {
          setText("powerStatus", toMessage(data.error));
        }
      } catch (error) {
        setText("powerStatus", toMessage(error.message));
      }
    }

    async function scanWifi() {
      if (wifiScanRequesting) {
        return;
      }

      if (wifiScanTimer) {
        clearTimeout(wifiScanTimer);
        wifiScanTimer = 0;
      }

      wifiScanRequesting = true;
      updateScanButtonState();
      try {
        const data = await api("/api/wifi/scan");
        const networks = Array.isArray(data.networks) ? data.networks : [];
        const networkCount = renderWifiOptions(networks);
        const ageMs = Number(data.ageMs) || 0;
        const ageSeconds = Math.floor(ageMs / 1000);

        wifiScanInProgress = !!data.scanInProgress;
        updateScanButtonState();

        if (wifiScanInProgress) {
          if (networkCount > 0 && data.fromCache) {
            setText("wifiStatus", "正在刷新附近 WiFi，先显示缓存结果（" + ageSeconds + " 秒前）。");
          } else {
            setText("wifiStatus", "正在扫描附近 WiFi，请稍候...");
          }
          scheduleWifiScan(1200);
        } else if (networkCount > 0) {
          if (data.fromCache && ageSeconds > 0) {
            setText("wifiStatus", "已加载最近扫描结果（" + ageSeconds + " 秒前），共 " + networkCount + " 个网络。");
          } else {
            setText("wifiStatus", "扫描完成，共找到 " + networkCount + " 个网络。");
          }
        } else {
          setText("wifiStatus", "未扫描到附近 WiFi。");
        }
      } catch (error) {
        wifiScanInProgress = false;
        setText("wifiStatus", toMessage(error.message));
      } finally {
        wifiScanRequesting = false;
        updateScanButtonState();
      }
    }

    async function connectWifi() {
      const params = new URLSearchParams();
      params.set("ssid", (document.getElementById("wifiSsid").value || "").trim());
      params.set("password", document.getElementById("wifiPassword").value || "");
      try {
        const data = await api("/api/wifi/connect", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: params.toString()
        });
        setText("wifiStatus", (data.message || "连接成功") + (data.ip ? ("，IP：" + data.ip) : ""));
      } catch (error) {
        setText("wifiStatus", toMessage(error.message));
      }
    }

    async function saveConfig(event) {
      event.preventDefault();
      const params = new URLSearchParams(new FormData(event.target));
      try {
        await api("/api/config", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: params.toString()
        });
        setText("configStatus", "计算机配置已保存。");
      } catch (error) {
        setText("configStatus", toMessage(error.message));
      }
    }

    async function savePassword(event) {
      event.preventDefault();
      const params = new URLSearchParams(new FormData(event.target));
      try {
        const data = await api("/api/auth/password", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: params.toString()
        });
        setText("passwordStatus", "密码已更新，正在跳转到登录页...");
        event.target.reset();
        if (data.relogin) {
          setTimeout(function () {
            window.location.href = "/login";
          }, 1000);
        }
      } catch (error) {
        setText("passwordStatus", toMessage(error.message));
      }
    }

    document.getElementById("wifiSelect").addEventListener("change", function () {
      document.getElementById("wifiSsid").value = this.value;
    });
    document.getElementById("scanButton").addEventListener("click", scanWifi);
    document.getElementById("connectButton").addEventListener("click", connectWifi);
    document.getElementById("powerButton").addEventListener("click", powerOn);
    document.getElementById("configForm").addEventListener("submit", saveConfig);
    document.getElementById("passwordForm").addEventListener("submit", savePassword);

    window.addEventListener("DOMContentLoaded", async function () {
      updateScanButtonState();
      try {
        await loadConfig();
      } catch (error) {
        setText("configStatus", toMessage(error.message));
      }
      await scanWifi();
      setInterval(refreshPowerStatus, 2000);
    });
  </script>
</body>
</html>
)HTML";
}

String WebPortal::jsonEscape(const String& value) {
  String escaped;
  escaped.reserve(value.length() + 8);

  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value.charAt(i);
    switch (c) {
      case '\"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default: {
        if (static_cast<unsigned char>(c) < 0x20) {
          char buffer[7];
          std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(c));
          escaped += buffer;
        } else {
          escaped += c;
        }
        break;
      }
    }
  }

  return escaped;
}

bool WebPortal::parseBoolValue(const String& value, bool defaultValue) {
  String normalized = value;
  normalized.trim();
  normalized.toLowerCase();

  if (normalized == "1" || normalized == "true" || normalized == "on" || normalized == "yes") {
    return true;
  }
  if (normalized == "0" || normalized == "false" || normalized == "off" || normalized == "no") {
    return false;
  }
  return defaultValue;
}
