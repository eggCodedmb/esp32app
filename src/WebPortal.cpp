#include "WebPortal.h"

#include <cstdio>

WebPortal::WebPortal(uint16_t port,
                     AuthService& authService,
                     WifiService& wifiService,
                     ConfigStore& configStore,
                     DeviceStateService& deviceState)
    : _server(port),
      _authService(authService),
      _wifiService(wifiService),
      _configStore(configStore),
      _deviceState(deviceState) {}

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
                          "ESPSESSION=" + token + "; Path=/; HttpOnly; SameSite=Lax");
      request->send(response);
      return;
    }

    request->send(401, "text/html; charset=utf-8", loginPage("Invalid username or password."));
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
    String body = "{";
    body += "\"computerName\":\"" + jsonEscape(config.name) + "\",";
    body += "\"computerIp\":\"" + jsonEscape(config.ip) + "\",";
    body += "\"computerPort\":" + String(config.port) + ",";
    body += "\"computerOwner\":\"" + jsonEscape(config.owner) + "\",";
    body += "\"powerOn\":" + String(_deviceState.isPoweredOn() ? "true" : "false") + ",";
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

    const std::vector<WifiNetworkInfo> networks = _wifiService.scanNetworks();
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

  _server.on("/api/power", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    const String body = String("{\"powerOn\":") +
                        String(_deviceState.isPoweredOn() ? "true" : "false") + "}";
    request->send(200, "application/json", body);
  });

  _server.on("/api/power", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!ensureAuthorized(request, true)) {
      return;
    }

    bool result = false;
    if (request->hasParam("powerOn", true)) {
      const bool desiredState = parseBoolValue(request->getParam("powerOn", true)->value(), false);
      result = _deviceState.setPowerState(desiredState);
    } else {
      result = _deviceState.togglePowerState();
    }

    if (!result) {
      request->send(500, "application/json", "{\"success\":false,\"error\":\"persist_failed\"}");
      return;
    }

    const String body = String("{\"success\":true,\"powerOn\":") +
                        String(_deviceState.isPoweredOn() ? "true" : "false") + "}";
    request->send(200, "application/json", body);
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
    request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
  } else {
    AsyncWebServerResponse* response = request->beginResponse(302);
    response->addHeader("Location", "/login");
    request->send(response);
  }
  return false;
}

String WebPortal::loginPage(const String& errorMessage) const {
  String page = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 Login</title>
  <style>
    body { font-family: Arial, sans-serif; background: #f3f4f6; margin: 0; }
    main { max-width: 360px; margin: 9vh auto; background: #fff; padding: 24px; border-radius: 12px; box-shadow: 0 8px 24px rgba(0,0,0,0.08); }
    h1 { margin-top: 0; font-size: 20px; }
    label { display: block; margin-top: 12px; font-size: 14px; }
    input { width: 100%; box-sizing: border-box; padding: 10px; margin-top: 6px; border: 1px solid #d1d5db; border-radius: 8px; }
    button { margin-top: 16px; width: 100%; padding: 10px; border: 0; border-radius: 8px; background: #2563eb; color: #fff; cursor: pointer; }
    .error { color: #b91c1c; margin: 8px 0 0; font-size: 13px; }
    .hint { margin-top: 14px; color: #6b7280; font-size: 12px; }
  </style>
</head>
<body>
  <main>
    <h1>ESP32 Console Login</h1>
    __ERROR_BLOCK__
    <form method="post" action="/login">
      <label for="username">Username</label>
      <input id="username" name="username" required>
      <label for="password">Password</label>
      <input id="password" name="password" type="password" required>
      <button type="submit">Login</button>
    </form>
    <p class="hint">Default account: admin / admin123</p>
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
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 Control Panel</title>
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
    body { margin: 0; font-family: Arial, sans-serif; color: var(--text); background: linear-gradient(160deg, #ecfeff 0%, #f8fafc 45%, #fff 100%); }
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
    .muted { color: #4b5563; font-size: 13px; }
    .status { min-height: 20px; margin-top: 6px; font-size: 14px; }
    .error { color: var(--danger); }
    @media (max-width: 720px) {
      .row { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <main>
    <header>
      <h1>ESP32 Control Panel</h1>
      <a class="logout" href="/logout">Logout</a>
    </header>

    <section>
      <h2>Power Control</h2>
      <p class="muted">Current state: <strong id="powerState">Unknown</strong></p>
      <button id="powerButton" type="button">Toggle</button>
      <p id="powerStatus" class="status"></p>
    </section>

    <section>
      <h2>WiFi Connection</h2>
      <div class="row">
        <div>
          <label for="wifiSelect">Nearby networks</label>
          <select id="wifiSelect"></select>
          <button id="scanButton" type="button">Scan WiFi</button>
        </div>
        <div>
          <label for="wifiSsid">SSID</label>
          <input id="wifiSsid" autocomplete="off">
          <label for="wifiPassword">Password</label>
          <input id="wifiPassword" type="password" autocomplete="off">
          <button id="connectButton" type="button">Connect</button>
        </div>
      </div>
      <p id="wifiStatus" class="status"></p>
    </section>

    <section>
      <h2>Computer Configuration</h2>
      <form id="configForm">
        <div class="row">
          <div>
            <label for="computerName">Computer Name</label>
            <input id="computerName" name="computerName" autocomplete="off">
          </div>
          <div>
            <label for="computerIp">Computer IP</label>
            <input id="computerIp" name="computerIp" autocomplete="off">
          </div>
          <div>
            <label for="computerPort">Computer Port</label>
            <input id="computerPort" name="computerPort" type="number" min="1" max="65535">
          </div>
          <div>
            <label for="computerOwner">Computer Owner</label>
            <input id="computerOwner" name="computerOwner" autocomplete="off">
          </div>
        </div>
        <button type="submit">Save Configuration</button>
      </form>
      <p id="configStatus" class="status"></p>
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

    function updatePower(powerOn) {
      setText("powerState", powerOn ? "ON" : "OFF");
      document.getElementById("powerButton").textContent = powerOn ? "Turn OFF" : "Turn ON";
    }

    async function loadConfig() {
      const data = await api("/api/config");
      document.getElementById("computerName").value = data.computerName || "";
      document.getElementById("computerIp").value = data.computerIp || "";
      document.getElementById("computerPort").value = data.computerPort || "";
      document.getElementById("computerOwner").value = data.computerOwner || "";
      updatePower(!!data.powerOn);
      if (data.wifiConnected) {
        setText("wifiStatus", "Connected: " + (data.wifiSsid || "") + "  IP: " + (data.wifiIp || ""));
      } else {
        setText("wifiStatus", "Not connected");
      }
    }

    async function scanWifi() {
      try {
        const data = await api("/api/wifi/scan");
        const select = document.getElementById("wifiSelect");
        select.innerHTML = "";
        if (Array.isArray(data.networks) && data.networks.length > 0) {
          data.networks.forEach(function (network) {
            const option = document.createElement("option");
            option.value = network.ssid;
            option.textContent = network.ssid + " (" + network.rssi + " dBm" + (network.secured ? ", secured" : ", open") + ")";
            select.appendChild(option);
          });
          document.getElementById("wifiSsid").value = select.value;
        } else {
          const option = document.createElement("option");
          option.value = "";
          option.textContent = "No networks found";
          select.appendChild(option);
        }
      } catch (error) {
        setText("wifiStatus", error.message);
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
        setText("wifiStatus", data.message + (data.ip ? (" IP: " + data.ip) : ""));
      } catch (error) {
        setText("wifiStatus", error.message);
      }
    }

    async function togglePower() {
      try {
        const data = await api("/api/power", { method: "POST" });
        updatePower(!!data.powerOn);
        setText("powerStatus", "Power state updated.");
      } catch (error) {
        setText("powerStatus", error.message);
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
        setText("configStatus", "Configuration saved.");
      } catch (error) {
        setText("configStatus", error.message);
      }
    }

    document.getElementById("wifiSelect").addEventListener("change", function () {
      document.getElementById("wifiSsid").value = this.value;
    });
    document.getElementById("scanButton").addEventListener("click", scanWifi);
    document.getElementById("connectButton").addEventListener("click", connectWifi);
    document.getElementById("powerButton").addEventListener("click", togglePower);
    document.getElementById("configForm").addEventListener("submit", saveConfig);

    window.addEventListener("DOMContentLoaded", async function () {
      try {
        await loadConfig();
      } catch (error) {
        setText("configStatus", error.message);
      }
      await scanWifi();
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
