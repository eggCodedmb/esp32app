#include "WebPortal.h"

String WebPortal::loginPage(const String& errorMessage) const {
  String page = R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 登录</title>
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bulma@1.0.4/css/bulma.min.css">
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/@fortawesome/fontawesome-free@7.2.0/css/all.min.css">
  <style>
    :root {
      --shell-bg: radial-gradient(circle at top, #dbeafe 0%, #f8fafc 52%, #ffffff 100%);
      --card-bg: rgba(255, 255, 255, 0.9);
      --line-color: #d9e1ec;
      --title-color: #0f172a;
      --text-color: #1e293b;
      --hint-color: #64748b;
      --button-bg: #2563eb;
      --button-hover: #1d4ed8;
      --input-bg: #ffffff;
      --input-border: #cbd5e1;
      --theme-toggle-bg: rgba(37, 99, 235, 0.08);
      --theme-toggle-border: rgba(37, 99, 235, 0.32);
      --theme-toggle-color: #1d4ed8;
      --theme-toggle-hover-bg: rgba(37, 99, 235, 0.14);
    }
    html[data-theme="dark"] {
      --shell-bg: radial-gradient(circle at top, #0f172a 0%, #111827 52%, #020617 100%);
      --card-bg: rgba(15, 23, 42, 0.92);
      --line-color: rgba(148, 163, 184, 0.35);
      --title-color: #f8fafc;
      --text-color: #e2e8f0;
      --hint-color: #94a3b8;
      --button-bg: #3b82f6;
      --button-hover: #60a5fa;
      --input-bg: rgba(15, 23, 42, 0.96);
      --input-border: rgba(148, 163, 184, 0.45);
      --theme-toggle-bg: rgba(59, 130, 246, 0.14);
      --theme-toggle-border: rgba(96, 165, 250, 0.52);
      --theme-toggle-color: #93c5fd;
      --theme-toggle-hover-bg: rgba(96, 165, 250, 0.22);
    }
    html, body {
      width: 100%;
      height: 100%;
    }
    body {
      margin: 0;
      min-height: 100vh;
      min-height: 100dvh;
      display: grid;
      place-items: center;
      padding: 20px;
      font-family: "Noto Sans SC", "Microsoft YaHei", "PingFang SC", sans-serif;
      background: var(--shell-bg);
      color: var(--text-color);
    }
    main.login-card {
      width: min(100%, 390px);
      background: var(--card-bg);
      border: 1px solid var(--line-color);
      border-radius: 16px;
      padding: 24px;
      box-shadow: 0 16px 34px rgba(2, 6, 23, 0.12);
      backdrop-filter: blur(6px);
    }
    .title {
      color: var(--title-color);
    }
    #themeToggle {
      margin-top: 0;
      background: var(--theme-toggle-bg) !important;
      border-color: var(--theme-toggle-border) !important;
      color: var(--theme-toggle-color) !important;
    }
    #themeToggle:hover {
      background: var(--theme-toggle-hover-bg) !important;
      border-color: var(--theme-toggle-border) !important;
      color: var(--theme-toggle-color) !important;
    }
    #themeToggle .icon,
    #themeToggle .icon i {
      color: var(--theme-toggle-color) !important;
    }
    .error {
      color: #dc2626;
      margin: 0 0 10px;
      font-size: 13px;
    }
    .info {
      color: #2563eb;
      margin: 0 0 10px;
      font-size: 13px;
    }
    .hint {
      margin-top: 14px;
      color: var(--hint-color);
      font-size: 12px;
    }
  </style>
</head>
<body>
  <main class="login-card">
    <div class="is-flex is-justify-content-space-between is-align-items-center mb-3">
      <h1 class="title is-4 mb-0">ESP32 控制台登录</h1>
      <button id="themeToggle" class="button is-small is-light is-outlined" type="button" title="主题模式">
        <span class="icon is-small"><i id="themeToggleIcon" class="fa-solid fa-circle-half-stroke"></i></span>
      </button>
    </div>
    __ERROR_BLOCK__
    <form method="post" action="/login">
      <div class="field">
        <label class="label" for="username">用户名</label>
        <div class="control has-icons-left">
          <input class="input" id="username" name="username" required>
          <span class="icon is-small is-left">
             <i class="fas fa-user"></i>
          </span>
        </div>
      </div>
      <div class="field">
        <label class="label" for="password">密码</label>
        <div class="control has-icons-left">
          <input class="input" id="password" name="password" type="password" required>
          <span class="icon is-small is-left">
            <i class="fas fa-lock"></i>
          </span>
        </div>
      </div>
      <button class="button is-primary is-fullwidth mt-4" type="submit">登录</button>
    </form>
    <p class="hint">默认账号：admin / admin123，建议首次登录后修改密码</p>
  </main>
  <script>
    (function () {
      const themeStorageKey = "esp32_theme_mode";
      const themeModes = ["system", "light", "dark"];
      const themeIcons = {
        system: "fa-circle-half-stroke",
        light: "fa-sun",
        dark: "fa-moon"
      };
      const themeTitles = {
        system: "跟随系统",
        light: "亮色",
        dark: "暗色"
      };
      const darkSchemeMedia = window.matchMedia ? window.matchMedia("(prefers-color-scheme: dark)") : null;
      let currentThemeMode = "system";

      function resolveTheme(mode) {
        if (mode === "light" || mode === "dark") {
          return mode;
        }
        return darkSchemeMedia && darkSchemeMedia.matches ? "dark" : "light";
      }

      function updateToggle() {
        const icon = document.getElementById("themeToggleIcon");
        const button = document.getElementById("themeToggle");
        if (!icon || !button) {
          return;
        }

        icon.className = "fa-solid " + themeIcons[currentThemeMode];
        button.title = "主题模式：" + themeTitles[currentThemeMode];
      }

      function applyThemeMode(mode, persist) {
        currentThemeMode = themeModes.indexOf(mode) >= 0 ? mode : "system";
        document.documentElement.setAttribute("data-theme", resolveTheme(currentThemeMode));
        updateToggle();
        if (persist) {
          try {
            localStorage.setItem(themeStorageKey, currentThemeMode);
          } catch (_) {}
        }
      }

      function cycleThemeMode() {
        const currentIndex = themeModes.indexOf(currentThemeMode);
        const nextIndex = (currentIndex + 1) % themeModes.length;
        applyThemeMode(themeModes[nextIndex], true);
      }

      function bindSystemThemeListener() {
        if (!darkSchemeMedia) {
          return;
        }
        const handler = function () {
          if (currentThemeMode === "system") {
            applyThemeMode("system", false);
          }
        };
        if (typeof darkSchemeMedia.addEventListener === "function") {
          darkSchemeMedia.addEventListener("change", handler);
        } else if (typeof darkSchemeMedia.addListener === "function") {
          darkSchemeMedia.addListener(handler);
        }
      }

      const savedMode = (function () {
        try {
          return localStorage.getItem(themeStorageKey);
        } catch (_) {
          return "";
        }
      })();
      if (themeModes.indexOf(savedMode) >= 0) {
        currentThemeMode = savedMode;
      }
      applyThemeMode(currentThemeMode, false);
      bindSystemThemeListener();

      const toggleButton = document.getElementById("themeToggle");
      if (toggleButton) {
        toggleButton.addEventListener("click", cycleThemeMode);
      }
    })();
  </script>
</body>
</html>
)HTML";

  String cssClass = "error";
  if (errorMessage.indexOf("成功") != -1) {
    cssClass = "info";
  }
  const String errorBlock = errorMessage.isEmpty() ? "" : "<p class=\"" + cssClass + "\">" + errorMessage + "</p>";
  page.replace("__ERROR_BLOCK__", errorBlock);
  return page;
}

const char* WebPortal::dashboardPage() const {
  static const char kDashboardPage[] PROGMEM = R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 管理页面</title>
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bulma@1.0.4/css/bulma.min.css">
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/@fortawesome/fontawesome-free@7.2.0/css/all.min.css">
  <style>
    :root {
      --shell-bg: linear-gradient(155deg, #dff4ff 0%, #edf3ff 45%, #f8fafc 100%);
      --card-bg: rgba(255, 255, 255, 0.9);
      --line: rgba(148, 163, 184, 0.36);
      --text: #1e293b;
      --title: #0f172a;
      --muted: #475569;
      --metric-bg: rgba(248, 250, 252, 0.95);
      --button-bg: #2563eb;
      --button-hover: #1d4ed8;
      --button-secondary: #334155;
      --button-secondary-hover: #1f2937;
      --input-bg: #ffffff;
      --input-line: #cbd5e1;
      --input-text: #0f172a;
      --code: #334155;
      --shadow: 0 10px 28px rgba(15, 23, 42, 0.1);
      --theme-toggle-bg: rgba(37, 99, 235, 0.08);
      --theme-toggle-border: rgba(37, 99, 235, 0.32);
      --theme-toggle-color: #1d4ed8;
      --theme-toggle-hover-bg: rgba(37, 99, 235, 0.14);
    }
    html[data-theme="dark"] {
      --shell-bg: linear-gradient(165deg, #0b1220 0%, #111827 52%, #020617 100%);
      --card-bg: rgba(15, 23, 42, 0.9);
      --line: rgba(148, 163, 184, 0.3);
      --text: #e2e8f0;
      --title: #f8fafc;
      --muted: #94a3b8;
      --metric-bg: rgba(30, 41, 59, 0.85);
      --button-bg: #3b82f6;
      --button-hover: #60a5fa;
      --button-secondary: #1f2937;
      --button-secondary-hover: #111827;
      --input-bg: rgba(15, 23, 42, 0.92);
      --input-line: rgba(148, 163, 184, 0.45);
      --input-text: #e2e8f0;
      --code: #cbd5e1;
      --shadow: 0 12px 28px rgba(2, 6, 23, 0.35);
      --theme-toggle-bg: rgba(59, 130, 246, 0.14);
      --theme-toggle-border: rgba(96, 165, 250, 0.52);
      --theme-toggle-color: #93c5fd;
      --theme-toggle-hover-bg: rgba(96, 165, 250, 0.22);
    }
    * {
      box-sizing: border-box;
    }
    html, body {
      min-height: 100%;
    }
    body {
      margin: 0;
      font-family: "Noto Sans SC", "Microsoft YaHei", "PingFang SC", sans-serif;
      color: var(--text);
      background: var(--shell-bg);
      transition: background 0.2s ease, color 0.2s ease;
    }
    main {
      max-width: 980px;
      margin: 22px auto 30px;
      padding: 0 14px;
    }
    header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 10px;
      margin-bottom: 14px;
    }
    .header-actions {
      display: flex;
      align-items: center;
      gap: 10px;
    }
    h1 {
      margin: 0;
      font-size: 24px;
      color: var(--title);
    }
    section {
      background: var(--card-bg);
      border: 1px solid var(--line);
      border-radius: 14px;
      padding: 16px;
      margin-bottom: 14px;
      box-shadow: var(--shadow);
      backdrop-filter: blur(5px);
    }
    h2 {
      margin-top: 0;
      font-size: 18px;
      color: var(--title);
    }
    .row { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    label { display: block; margin-top: 8px; font-size: 14px; color: var(--muted); }
    .input, .select select {
      background: var(--input-bg);
      border-color: var(--input-line);
      color: var(--input-text);
    }
    .input:focus, .select select:focus {
      border-color: var(--button-bg);
      box-shadow: 0 0 0 0.125em rgba(37, 99, 235, 0.2);
    }
    .select, .select select {
      width: 100%;
    }
    button.button, a.button {
      margin-top: 12px;
    }
    #themeToggle, .logout {
      margin-top: 0;
    }
    #themeToggle {
      background: var(--theme-toggle-bg) !important;
      border-color: var(--theme-toggle-border) !important;
      color: var(--theme-toggle-color) !important;
    }
    #themeToggle:hover {
      background: var(--theme-toggle-hover-bg) !important;
      border-color: var(--theme-toggle-border) !important;
      color: var(--theme-toggle-color) !important;
    }
    #themeToggle .icon,
    #themeToggle .icon i,
    #themeToggle .mode-text {
      color: var(--theme-toggle-color) !important;
    }
    .muted { color: var(--muted); font-size: 13px; }
    .status { min-height: 20px; margin-top: 6px; font-size: 14px; color: var(--text); }
    .inline-actions { display: flex; align-items: center; gap: 10px; flex-wrap: wrap; }
    .metrics { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 10px; }
    .metric { border: 1px solid var(--line); border-radius: 10px; padding: 10px; background: var(--metric-bg); }
    .metric-title { display: block; color: var(--muted); font-size: 12px; }
    .metric-value { display: block; margin-top: 6px; font-size: 15px; font-weight: 600; word-break: break-word; }
    .inline-check { display: flex; align-items: center; gap: 10px; margin-top: 8px; }
    .inline-check input[type="checkbox"] { width: auto; margin: 0; }
    .code { font-family: Consolas, Monaco, monospace; font-size: 12px; color: var(--code); }
    .ddns-records { margin-top: 10px; display: flex; flex-direction: column; gap: 10px; }
    .ddns-record { border: 1px dashed var(--line); border-radius: 10px; background: var(--metric-bg); padding: 10px; }
    .ddns-record-header { display: flex; justify-content: space-between; align-items: center; gap: 8px; }
    .ddns-record-title { font-size: 14px; font-weight: 600; }
    .ddns-record.ddns-readonly { opacity: 0.68; }
    .ddns-record-summary { display: none; margin-top: 8px; }
    .ddns-record-details { margin-top: 8px; }
    .ddns-record.ddns-collapsed .ddns-record-summary { display: block; }
    .ddns-record.ddns-collapsed .ddns-record-details { display: none; }
    .power-config-grid { display: grid; grid-template-columns: minmax(0, 2fr) minmax(220px, 1fr); gap: 12px; }
    .power-config-card { border: 1px solid var(--line); border-radius: 10px; background: var(--metric-bg); padding: 12px; }
    .power-config-card h3 { margin: 0 0 10px; font-size: 15px; }
    .power-config-actions { display: flex; flex-direction: column; align-items: flex-start; gap: 8px; }
    .power-config-actions button { margin-top: 0; }
    @media (max-width: 720px) {
      .row { grid-template-columns: 1fr; }
      .metrics { grid-template-columns: 1fr 1fr; }
      .power-config-grid { grid-template-columns: 1fr; }
      #themeToggle .mode-text { display: none; }
    }
    @media (max-width: 460px) {
      .metrics { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <main>
    <header>
      <h1>ESP32 控制面板</h1>
      <div class="header-actions">
        <button id="themeToggle" class="button is-small is-light is-outlined" type="button" title="主题模式">
          <span class="icon is-small"><i id="themeToggleIcon" class="fa-solid fa-circle-half-stroke"></i></span>
          <span id="themeToggleText" class="mode-text">跟随系统</span>
        </button>
        <a class="button is-warning is-small is-dark is-outlined" href="/logout" style="margin: 0;">退出登录</a>
      </div>
    </header>

    <section>
      <h2>远程开机与计算机配置</h2>
      <div class="power-config-grid">
        <div class="power-config-card">
          <h3>目标主机参数</h3>
          <form id="configForm">
            <div class="row">
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
            </div>
            <button type="submit">保存计算机配置</button>
          </form>
          <p id="configStatus" class="status"></p>
        </div>
        <div class="power-config-card">
          <h3>远程开机控制</h3>
          <div class="power-config-actions">
            <p class="muted">开机状态：<strong id="powerState">待机</strong></p>
            <button id="powerButton" type="button"><span class="icon"><i class="fa-solid fa-power-off"></i></span></button>
          </div>
          <p id="powerStatus" class="status"></p>
        </div>
      </div>
    </section>

    <section>
      <h2>系统配置</h2>
      <form id="systemForm">
        <div class="row">
          <div>
            <label for="statusPollIntervalMinutes">状态轮询</label>
            <select id="statusPollIntervalMinutes" name="statusPollIntervalMinutes">
              <option value="manual">手动</option>
              <option value="1">1 分钟</option>
              <option value="3">3 分钟</option>
              <option value="10">10 分钟</option>
              <option value="30">30 分钟</option>
              <option value="60">60 分钟</option>
            </select>
          </div>
        </div>
        <div class="inline-actions">
          <button type="submit">保存系统配置</button>
          <button id="refreshAllButton" class="secondary" type="button">统一刷新状态</button>
        </div>
      </form>
      <p id="systemPollStatus" class="muted"></p>
      <p id="systemConfigStatus" class="status"></p>
    </section>

    <section>
      <h2>动态域名解析（DDNS）</h2>
      <form id="ddnsForm">
        <div class="inline-check">
          <input id="ddnsEnabled" name="ddnsEnabled" type="checkbox">
          <label for="ddnsEnabled" style="margin: 0;">启用 DDNS</label>
        </div>
        <div class="inline-actions">
          <button id="ddnsAddRecordButton" class="secondary" type="button"><span class="icon"><i class="fa-solid fa-plus"></i></span></button>
          <button id="ddnsToggleAllButton" class="secondary" type="button">Collapse All</button>
        </div>
      </form>
      <div id="ddnsRecords" class="ddns-records"></div>
      <p class="muted">状态：<strong id="ddnsState">-</strong>，活跃记录：<strong id="ddnsActiveCount">0</strong>，更新次数：<strong id="ddnsUpdateCount">0</strong></p>
      <p id="ddnsStatus" class="status"></p>
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
      <h2>ESP 设备信息</h2>
      <div class="metrics">
        <div class="metric"><span class="metric-title">运行时长</span><span id="espUptime" class="metric-value">-</span></div>
        <div class="metric"><span class="metric-title">堆内存 空闲/总量</span><span id="espHeap" class="metric-value">-</span></div>
        <div class="metric"><span class="metric-title">堆内存最小空闲</span><span id="espHeapMin" class="metric-value">-</span></div>
        <div class="metric"><span class="metric-title">最大可分配块</span><span id="espHeapMaxAlloc" class="metric-value">-</span></div>
        <div class="metric"><span class="metric-title">Flash 已用/总量</span><span id="espFlash" class="metric-value">-</span></div>
         <div class="metric"><span class="metric-title">Flash 剩余</span><span id="espFlashFree" class="metric-value">-</span></div>
         <div class="metric"><span class="metric-title">系统时间</span><span id="espSystemTime" class="metric-value">-</span></div>
       </div>
      <p id="espInfoStatus" class="status"></p>
    </section>

    <section>
      <h2>Bemfa Cloud</h2>
      <form id="bemfaForm">
        <div class="inline-check">
        
          <input id="bemfaEnabled" name="bemfaEnabled" type="checkbox">
          <label for="bemfaEnabled" style="margin: 0;">启用巴法云控制</label>
        </div>
        <div class="row">
          <div>
            <label for="bemfaTopic">主题 Topic</label>
            <input id="bemfaTopic" name="bemfaTopic" autocomplete="off" placeholder="esp32_switch">
          </div>
          <div>
            <label for="bemfaUid">私钥 UID</label>
            <input id="bemfaUid" name="bemfaUid" autocomplete="off">
          </div>
          <div>
            <label for="bemfaKey">Key（可选）</label>
            <input id="bemfaKey" name="bemfaKey" type="password" autocomplete="off">
          </div>
          <div>
            <label for="bemfaHost">主机</label>
            <input id="bemfaHost" name="bemfaHost" autocomplete="off" placeholder="bemfa.com">
          </div>
          <div>
            <label for="bemfaPort">端口</label>
            <input id="bemfaPort" name="bemfaPort" type="number" min="1" max="65535">
          </div>
        </div>
        <button type="submit">保存配置</button>
      </form>
      <p class="muted">状态：<strong id="bemfaState">-</strong>，连接：<strong id="bemfaConnected">-</strong></p>
      <p id="bemfaTopics" class="code">-</p>
      <p id="bemfaStatus" class="status"></p>
    </section>

    <section>
      <h2>固件 OTA 升级</h2>
      <p class="muted">状态：<strong id="otaState">-</strong></p>
      <p class="muted">当前版本：<span id="otaCurrentVersion" class="code">-</span></p>
      <p class="muted">目标版本：<span id="otaTargetVersion" class="code">-</span></p>
      <div class="inline-actions">
        <button id="otaCheckButton" class="secondary" type="button">检测新版本</button>
        <button id="otaUpgradeButton" type="button" style="display:none;">升级固件</button>
      </div>
      <p id="otaStatus" class="status"></p>
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
      const element = document.getElementById(id);
      if (!element) {
        return;
      }
      element.textContent = text || "";
    }

    const themeStorageKey = "esp32_theme_mode";
    const themeModes = ["system", "light", "dark"];
    const themeLabels = {
      system: "跟随系统",
      light: "亮色",
      dark: "暗色"
    };
    const themeIcons = {
      system: "fa-circle-half-stroke",
      light: "fa-sun",
      dark: "fa-moon"
    };
    const darkSchemeMedia = window.matchMedia ? window.matchMedia("(prefers-color-scheme: dark)") : null;
    let currentThemeMode = "system";

    function resolveThemeFromMode(mode) {
      if (mode === "light" || mode === "dark") {
        return mode;
      }
      return darkSchemeMedia && darkSchemeMedia.matches ? "dark" : "light";
    }

    function updateThemeToggleView() {
      const icon = document.getElementById("themeToggleIcon");
      const text = document.getElementById("themeToggleText");
      const button = document.getElementById("themeToggle");
      if (!icon || !text || !button) {
        return;
      }

      icon.className = "fa-solid " + themeIcons[currentThemeMode];
      text.textContent = themeLabels[currentThemeMode];
      button.title = "主题模式：" + themeLabels[currentThemeMode];
    }

    function applyThemeMode(mode, persist) {
      currentThemeMode = themeModes.indexOf(mode) >= 0 ? mode : "system";
      document.documentElement.setAttribute("data-theme", resolveThemeFromMode(currentThemeMode));
      updateThemeToggleView();
      if (persist) {
        try {
          localStorage.setItem(themeStorageKey, currentThemeMode);
        } catch (_) {}
      }
    }

    function cycleThemeMode() {
      const currentIndex = themeModes.indexOf(currentThemeMode);
      const nextIndex = (currentIndex + 1) % themeModes.length;
      applyThemeMode(themeModes[nextIndex], true);
    }

    function bindSystemThemeListener() {
      if (!darkSchemeMedia) {
        return;
      }
      const handler = function () {
        if (currentThemeMode === "system") {
          applyThemeMode("system", false);
        }
      };
      if (typeof darkSchemeMedia.addEventListener === "function") {
        darkSchemeMedia.addEventListener("change", handler);
      } else if (typeof darkSchemeMedia.addListener === "function") {
        darkSchemeMedia.addListener(handler);
      }
    }

    function initThemeMode() {
      const savedMode = (function () {
        try {
          return localStorage.getItem(themeStorageKey);
        } catch (_) {
          return "";
        }
      })();
      if (themeModes.indexOf(savedMode) >= 0) {
        currentThemeMode = savedMode;
      }
      applyThemeMode(currentThemeMode, false);
      bindSystemThemeListener();
    }

    function wrapSelectWithBulma(root) {
      const context = root || document;
      Array.from(context.querySelectorAll("select")).forEach(function (select) {
        const parent = select.parentElement;
        if (parent && parent.classList.contains("select")) {
          parent.classList.add("is-fullwidth");
          return;
        }

        const wrapper = document.createElement("div");
        wrapper.className = "select is-fullwidth";
        select.parentNode.insertBefore(wrapper, select);
        wrapper.appendChild(select);
      });
    }

    function applyBulmaClasses(root) {
      const context = root || document;

      Array.from(context.querySelectorAll("input")).forEach(function (input) {
        const type = String(input.type || "").toLowerCase();
        if (type === "checkbox" || type === "radio") {
          input.classList.remove("input");
          return;
        }
        input.classList.add("input");
      });

      Array.from(context.querySelectorAll("button")).forEach(function (button) {
        button.classList.add("button");
        if (button.id === "themeToggle") {
          button.classList.remove("is-primary");
          button.classList.add("is-light", "is-outlined", "is-small");
          return;
        }
        if (button.classList.contains("secondary")) {
          button.classList.remove("is-primary");
          button.classList.add("is-light");
        } else {
          button.classList.add("is-primary");
        }
      });

      wrapSelectWithBulma(context);
    }

    const defaultStatusPollIntervalMinutes = 3;
    const allowedStatusPollIntervals = [1, 3, 10, 30, 60];
    let statusPollTimer = 0;
    let otaActionPollTimer = 0;
    const ddnsProviders = ["aliyun"];
    const maxDdnsRecords = 5;
    let ddnsRecordsCache = [];
    
    // 时间实时刷新
    let lastUnixTime = 0;
    let lastClientEpochSeconds = 0;
    let timeUpdateTimer = 0;

    function normalizeStatusPollIntervalMinutes(value) {
      const minutes = Number(value);
      if (!Number.isFinite(minutes)) {
        return defaultStatusPollIntervalMinutes;
      }
      const normalized = Math.max(0, Math.floor(minutes));
      if (normalized === 0) {
        return 0;
      }
      return allowedStatusPollIntervals.indexOf(normalized) >= 0
          ? normalized
          : defaultStatusPollIntervalMinutes;
    }

    function statusPollIntervalLabel(minutes) {
      if (minutes === 0) {
        return "手动";
      }
      return "每 " + minutes + " 分钟自动刷新";
    }

    function statusPollIntervalSelectValue(minutes) {
      return minutes === 0 ? "manual" : String(minutes);
    }

    function stopStatusPollTimer() {
      if (statusPollTimer) {
        clearInterval(statusPollTimer);
        statusPollTimer = 0;
      }
    }

    function startOtaActionPolling() {
      if (otaActionPollTimer) {
        return;
      }
      otaActionPollTimer = setInterval(refreshOtaStatus, 2000);
    }

    function stopOtaActionPolling() {
      if (!otaActionPollTimer) {
        return;
      }
      clearInterval(otaActionPollTimer);
      otaActionPollTimer = 0;
    }

    function applyStatusPolling(minutes) {
      const normalizedMinutes = normalizeStatusPollIntervalMinutes(minutes);
      const select = document.getElementById("statusPollIntervalMinutes");
      const refreshButton = document.getElementById("refreshAllButton");

      select.value = statusPollIntervalSelectValue(normalizedMinutes);
      refreshButton.hidden = normalizedMinutes !== 0;

      stopStatusPollTimer();
      if (normalizedMinutes > 0) {
        statusPollTimer = setInterval(refreshAllStatus, normalizedMinutes * 60 * 1000);
      }

      setText("systemPollStatus", "状态轮询：" + statusPollIntervalLabel(normalizedMinutes));
    }

    function escapeHtml(value) {
      return String(value || "")
          .replace(/&/g, "&amp;")
          .replace(/</g, "&lt;")
          .replace(/>/g, "&gt;")
          .replace(/"/g, "&quot;")
          .replace(/'/g, "&#39;");
    }

    function normalizeDdnsProvider(value) {
      const normalized = String(value || "").trim().toLowerCase();
      if (ddnsProviders.indexOf(normalized) >= 0) {
        return normalized;
      }
      return "aliyun";
    }

    function normalizeDdnsIntervalSeconds(value) {
      const interval = Number(value);
      if (!Number.isFinite(interval)) {
        return 300;
      }
      const normalized = Math.floor(interval);
      if (normalized < 30 || normalized > 86400) {
        return 300;
      }
      return normalized;
    }

    function normalizeDdnsRecord(record) {
      const source = (record && typeof record === "object") ? record : {};
      const parsedDomain = splitDdnsDomain(source.domain || "");
      const rootDomain = String(source.rootDomain !== undefined ? source.rootDomain : parsedDomain.rootDomain).trim();
      const hostType = normalizeDdnsHostType(
          source.hostType !== undefined ? source.hostType : parsedDomain.hostType);
      const recordType = normalizeDdnsRecordType(
          source.recordType !== undefined ? source.recordType : "A");
      const ttl = normalizeDdnsIntervalSeconds(
          source.ttl !== undefined ? source.ttl : source.updateIntervalSeconds);
      const domain = buildDdnsFullDomain(rootDomain, hostType);
      return {
        enabled: source.enabled === undefined ? true : !!source.enabled,
        provider: normalizeDdnsProvider(source.provider),
        domain: domain,
        rootDomain: rootDomain,
        hostType: hostType,
        recordType: recordType,
        username: String(source.username || "").trim(),
        password: String(source.password || "").trim(),
        ttl: ttl,
        updateIntervalSeconds: ttl,
        useLocalIp: !!source.useLocalIp,
        value: String(source.value || "").trim(),
        recordId: String(source.recordId || "").trim(),
        editing: !!source.editing,
        isNew: !!source.isNew,
        collapsed: !!source.collapsed
      };
    }

    function normalizeDdnsHostType(value) {
      const normalized = String(value || "").trim();
      if (normalized === "@") {
        return "@";
      }
      return "www";
    }

    function normalizeDdnsRecordType(value) {
      // DDNS now supports IPv4 A record only.
      return "A";
    }

    function normalizeAliyunRecord(record) {
      const source = (record && typeof record === "object") ? record : {};
      const rr = normalizeAliyunRr(source.RR || source.rr || "@");
      const domainName = String(source.DomainName || source.domainName || "").trim();
      let fullDomain = String(source.fullDomain || source.domain || "").trim();
      if (!fullDomain && domainName) {
        fullDomain = rr !== "@" ? (rr + "." + domainName) : domainName;
      }
      return Object.assign({}, source, {
        recordId: String(source.RecordId || source.recordId || "").trim(),
        rr: rr,
        domainName: domainName,
        fullDomain: fullDomain,
        type: normalizeDdnsRecordType(source.Type || source.type || "A"),
        value: String(source.Value || source.value || "").trim(),
        status: String(source.Status || source.status || "").trim(),
        line: String(source.Line || source.line || "").trim(),
        ttl: source.TTL !== undefined ? String(source.TTL) : String(source.ttl || "")
      });
    }

    function createDefaultDdnsRecord() {
      return {
        enabled: true,
        provider: "aliyun",
        rootDomain: "",
        hostType: "www",
        recordType: "A",
        username: "",
        password: "",
        ttl: 300,
        updateIntervalSeconds: 300,
        useLocalIp: false,
        value: "",
        recordId: "",
        editing: true,
        isNew: true,
        collapsed: false
      };
    }

    function ddnsProviderOptions(selectedProvider) {
      return ddnsProviders
          .map(function (provider) {
            const selected = provider === selectedProvider ? " selected" : "";
            return "<option value=\"" + provider + "\"" + selected + ">" + provider + "</option>";
          })
          .join("");
    }

    function splitDdnsDomain(fullDomain) {
      const normalized = String(fullDomain || "").trim();
      if (!normalized) {
        return { rootDomain: "", hostType: "www", fullDomain: "" };
      }
      const firstDotPos = normalized.indexOf(".");
      const lastDotPos = normalized.lastIndexOf(".");
      if (firstDotPos < 0 || firstDotPos === lastDotPos) {
        return { rootDomain: normalized, hostType: "@", fullDomain: normalized };
      }
      const hostType = normalizeDdnsHostType(normalized.slice(0, firstDotPos));
      const rootDomain = String(normalized.slice(firstDotPos + 1) || "").trim();
      return { rootDomain: rootDomain, hostType: hostType, fullDomain: buildDdnsFullDomain(rootDomain, hostType) };
    }

    function normalizeAliyunRr(value) {
      const rr = String(value || "").trim();
      return rr || "@";
    }

    function buildDdnsFullDomain(rootDomain, hostType) {
      const root = String(rootDomain || "").trim();
      if (!root) {
        return "";
      }
      const normalizedHostType = normalizeDdnsHostType(hostType);
      return normalizedHostType === "@" ? root : (normalizedHostType + "." + root);
    }

    function ddnsHostTypeOptions(selectedHostType) {
      const normalized = normalizeDdnsHostType(selectedHostType);
      return "<option value=\"www\"" + (normalized === "www" ? " selected" : "") + ">www</option>" +
             "<option value=\"@\"" + (normalized === "@" ? " selected" : "") + ">@</option>";
    }

    function ddnsTypeOptions(selectedType) {
      const normalized = normalizeDdnsRecordType(selectedType);
      return "<option value=\"A\"" + (normalized === "A" ? " selected" : "") + ">A</option>";
    }

    function updateDdnsCardPreview(card) {
      if (!card) {
        return;
      }
      const rootDomainInput = card.querySelector(".ddns-root-domain");
      const hostTypeSelect = card.querySelector(".ddns-host-type");
      const rootDomain = rootDomainInput ? rootDomainInput.value : "";
      const hostType = hostTypeSelect ? hostTypeSelect.value : "www";
      const fullDomain = buildDdnsFullDomain(rootDomain, hostType);
      const labelElement = card.querySelector(".ddns-domain-label");
      const previewElement = card.querySelector(".ddns-domain-preview");
      if (labelElement) {
        labelElement.textContent = normalizeDdnsHostType(hostType);
      }
      if (previewElement) {
        previewElement.textContent = fullDomain || "-";
      }
    }

    function collectDdnsRecordsFromForm() {
      const cards = Array.from(document.querySelectorAll("#ddnsRecords .ddns-record"));
      return cards.map(function (card, index) {
        const enabledElement = card.querySelector(".ddns-enabled");
        const providerElement = card.querySelector(".ddns-provider");
        const rootDomainElement = card.querySelector(".ddns-root-domain");
        const hostTypeElement = card.querySelector(".ddns-host-type");
        const recordTypeElement = card.querySelector(".ddns-record-type");
        const usernameElement = card.querySelector(".ddns-username");
        const passwordElement = card.querySelector(".ddns-password");
        const ttlElement = card.querySelector(".ddns-ttl");
        const localIpElement = card.querySelector(".ddns-local-ip");
        const valueElement = card.querySelector(".ddns-value");
        const current = ddnsRecordsCache[index] && typeof ddnsRecordsCache[index] === "object"
            ? ddnsRecordsCache[index]
            : {};
        const normalized = normalizeDdnsRecord({
          enabled: enabledElement ? !!enabledElement.checked : true,
          provider: providerElement ? providerElement.value : "aliyun",
          rootDomain: rootDomainElement ? rootDomainElement.value : "",
          hostType: hostTypeElement ? hostTypeElement.value : "www",
          recordType: recordTypeElement ? recordTypeElement.value : "A",
          username: usernameElement ? usernameElement.value : "",
          password: passwordElement ? passwordElement.value : "",
          ttl: ttlElement ? ttlElement.value : 300,
          useLocalIp: localIpElement ? !!localIpElement.checked : false,
          value: valueElement ? valueElement.value : "",
          recordId: current.recordId,
          isNew: !!current.isNew,
          editing: card.classList.contains("ddns-editing"),
          collapsed: card.classList.contains("ddns-collapsed")
        });
        return Object.assign({}, current, normalized);
      });
    }

    function buildDdnsConfigParams(records) {
      const params = new URLSearchParams();
      params.set("ddnsEnabled", document.getElementById("ddnsEnabled").checked ? "1" : "0");
      params.set("ddnsRecordCount", String(records.length));
      records.forEach(function (record, index) {
        const normalized = normalizeDdnsRecord(record);
        params.set("ddns" + String(index) + "Enabled", normalized.enabled ? "1" : "0");
        params.set("ddns" + String(index) + "Provider", normalizeDdnsProvider(normalized.provider));
        params.set("ddns" + String(index) + "Domain", normalized.rootDomain || "");
        params.set("ddns" + String(index) + "RootDomain", normalized.rootDomain || "");
        params.set("ddns" + String(index) + "HostType", normalizeDdnsHostType(normalized.hostType));
        params.set("ddns" + String(index) + "RecordType", normalizeDdnsRecordType(normalized.recordType));
        params.set("ddns" + String(index) + "Username", normalized.username || "");
        params.set("ddns" + String(index) + "Password", normalized.password || "");
        params.set("ddns" + String(index) + "TTL", String(normalizeDdnsIntervalSeconds(normalized.ttl)));
        params.set("ddns" + String(index) + "UseLocalIp", normalized.useLocalIp ? "1" : "0");
      });
      return params;
    }

    async function persistDdnsConfigRecords(records) {
      const normalizedRecords =
          (Array.isArray(records) ? records : []).slice(0, maxDdnsRecords).map(normalizeDdnsRecord);
      const params = buildDdnsConfigParams(normalizedRecords);
      await api("/api/config", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: params.toString()
      });
      ddnsRecordsCache = normalizedRecords;
    }

    function findAliyunRecordMatch(records, hostType, recordType) {
      const normalizedHostType = normalizeAliyunRr(hostType);
      const normalizedRecordType = normalizeDdnsRecordType(recordType);
      return (Array.isArray(records) ? records : []).find(function (record) {
        return normalizeAliyunRr(record.rr) === normalizedHostType &&
               normalizeDdnsRecordType(record.type) === normalizedRecordType;
      }) || null;
    }

    async function fetchAliyunRecordsByConfigIndex(configIndex) {
      const data = await api("/api/ddns/aliyun/records?configIndex=" + encodeURIComponent(String(configIndex)));
      return Array.isArray(data.records) ? data.records.map(normalizeAliyunRecord) : [];
    }

    async function refreshDdnsRecordSnapshots() {
      const sourceRecords = ddnsRecordsCache.map(normalizeDdnsRecord);
      const enriched = await Promise.all(sourceRecords.map(async function (record, index) {
        if (!record.rootDomain || !record.username || !record.password) {
          return record;
        }
        try {
          const aliyunRecords = await fetchAliyunRecordsByConfigIndex(index);
          const matched = findAliyunRecordMatch(aliyunRecords, record.hostType, record.recordType);
          if (!matched) {
            return record;
          }
          return Object.assign({}, record, {
            value: String(matched.value || record.value || "").trim(),
            recordId: String(matched.recordId || record.recordId || "").trim()
          });
        } catch (_) {
          return record;
        }
      }));

      ddnsRecordsCache = enriched.map(function (record, index) {
        const source = sourceRecords[index] || {};
        return Object.assign({}, source, record, {
          editing: !!source.editing,
          isNew: !!source.isNew
        });
      });
      renderDdnsRecords(ddnsRecordsCache);
    }

    async function addDdnsRecordToAliyun(configIndex, record) {
      const params = new URLSearchParams();
      params.set("configIndex", String(configIndex));
      params.set("rr", normalizeAliyunRr(record.hostType));
      params.set("type", normalizeDdnsRecordType(record.recordType));
      params.set("value", String(record.value || "").trim());
      params.set("username", String(record.username || "").trim());
      params.set("password", String(record.password || "").trim());
      params.set("rootDomain", String(record.rootDomain || "").trim());
      params.set("useLocalIp", record.useLocalIp ? "1" : "0");
      const data = await api("/api/ddns/aliyun/add", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: params.toString()
      });
      return String(data.recordId || "").trim();
    }

    async function updateDdnsRecordOnAliyun(configIndex, recordId, record) {
      const params = new URLSearchParams();
      params.set("configIndex", String(configIndex));
      params.set("recordId", String(recordId || "").trim());
      params.set("rr", normalizeAliyunRr(record.hostType));
      params.set("type", normalizeDdnsRecordType(record.recordType));
      params.set("value", String(record.value || "").trim());
      params.set("username", String(record.username || "").trim());
      params.set("password", String(record.password || "").trim());
      params.set("rootDomain", String(record.rootDomain || "").trim());
      params.set("useLocalIp", record.useLocalIp ? "1" : "0");
      await api("/api/ddns/aliyun/update", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: params.toString()
      });
    }

    async function deleteDdnsRecordFromAliyun(configIndex, recordId) {
      const params = new URLSearchParams();
      params.set("configIndex", String(configIndex));
      params.set("recordId", String(recordId || "").trim());
      await api("/api/ddns/aliyun/delete", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: params.toString()
      });
    }

    function validateDdnsRecordForAliyun(record) {
      if (!record.rootDomain) {
        return "DDNS root domain is required.";
      }
      if (!record.username || !record.password) {
        return "Aliyun AccessKey is required.";
      }
      return "";
    }

    function updateDdnsToggleAllButton() {
      const button = document.getElementById("ddnsToggleAllButton");
      if (!button) {
        return;
      }

      if (ddnsRecordsCache.length === 0) {
        button.disabled = true;
        button.textContent = "全部折叠";
        return;
      }

      const allCollapsed = ddnsRecordsCache.every(function (record) {
        return !!record.collapsed;
      });
      button.disabled = false;
      button.textContent = allCollapsed ? "展开全部" : "全部折叠";
    }

    function setAllDdnsRecordsCollapsed(collapsed) {
      const records = collectDdnsRecordsFromForm().map(normalizeDdnsRecord);
      if (records.length === 0) {
        updateDdnsToggleAllButton();
        return;
      }

      renderDdnsRecords(records.map(function (record) {
        return Object.assign({}, record, { collapsed: !!collapsed });
      }));
    }

    function toggleAllDdnsRecords() {
      const records = collectDdnsRecordsFromForm().map(normalizeDdnsRecord);
      if (records.length === 0) {
        updateDdnsToggleAllButton();
        return;
      }

      const shouldCollapse = records.some(function (record) { return !record.collapsed; });
      setAllDdnsRecordsCollapsed(shouldCollapse);
    }

    function toggleDdnsRecordCollapsed(index) {
      const records = collectDdnsRecordsFromForm().map(normalizeDdnsRecord);
      if (index < 0 || index >= records.length) {
        return;
      }

      records[index] = Object.assign({}, records[index], { collapsed: !records[index].collapsed });
      renderDdnsRecords(records);
    }

    function renderDdnsRecords(records) {
      ddnsRecordsCache = (Array.isArray(records) ? records : []).slice(0, maxDdnsRecords).map(normalizeDdnsRecord);

      const container = document.getElementById("ddnsRecords");
      if (!container) {
        return;
      }

      if (ddnsRecordsCache.length === 0) {
        container.innerHTML = "<p class=\"muted\">暂无 DDNS 配置记录，点击“新增配置”开始。</p>";
        applyBulmaClasses(container);
        updateDdnsToggleAllButton();
        return;
      }

      container.innerHTML = ddnsRecordsCache
                                .map(function (record, index) {
                                  const fullDomain = buildDdnsFullDomain(record.rootDomain, record.hostType);
                                  const readonly = !record.editing;
                                  const collapsed = !!record.collapsed;
                                  const disabledAttr = readonly ? " disabled" : "";
                                  const actionIcon = record.isNew ? "fa-save" : "fa-pen-to-square";
                                  const collapseLabel = collapsed ? "展开" : "折叠";
                                  const cardClass = "ddns-record" +
                                      (readonly ? " ddns-readonly" : " ddns-editing") +
                                      (collapsed ? " ddns-collapsed" : "");
                                  return (
                                      "<div class=\"" + cardClass + "\" data-index=\"" + String(index) + "\">" +
                                      "<div class=\"ddns-record-header\">" +
                                      "<span class=\"ddns-record-title\">配置 #" + String(index + 1) + "</span>" +
                                      "<div class=\"inline-actions\">" +
                                      "<button class=\"button is-light ddns-collapse-toggle\" type=\"button\" data-index=\"" + String(index) + "\">" + collapseLabel + "</button>" +
                                      "<button class=\"button is-light ddns-action-button\" type=\"button\" data-index=\"" + String(index) + "\"><span class=\"icon\"><i class=\"fa-solid " + actionIcon + "\"></i></span></button>" +
                                      "<button class=\"button is-warning ddns-remove-button\" type=\"button\" data-index=\"" + String(index) + "\"><span class=\"icon\"><i class=\"fa-solid fa-trash-can\"></i></span></button>" +
                                      "</div>" +
                                      "</div>" +
                                      "<p class=\"muted ddns-record-summary\">域名： " + escapeHtml(fullDomain || "-") + ", IP: " + escapeHtml(record.value || "-") + ", 记录ID: " + escapeHtml(record.recordId || "-") + "</p>" +
                                      "<div class=\"ddns-record-details\">" +
                                      "<div class=\"inline-check\">" +
                                      "<input class=\"ddns-enabled\" type=\"checkbox\"" + (record.enabled ? " checked" : "") + disabledAttr + ">" +
                                      "<label style=\"margin:0;\">启用此 DDNS 任务</label>" +
                                      "</div>" +
                                      "<div class=\"row\">" +
                                      "<div><label>服务商</label><select class=\"ddns-provider\"" + disabledAttr + ">" + ddnsProviderOptions(record.provider) + "</select></div>" +
                                      "<div><label>主域名</label><input class=\"ddns-root-domain\" autocomplete=\"off\" value=\"" + escapeHtml(record.rootDomain) + "\" placeholder=\"example.com\"" + disabledAttr + "></div>" +
                                      "<div><label>主机类型</label><select class=\"ddns-host-type\"" + disabledAttr + ">" + ddnsHostTypeOptions(record.hostType) + "</select></div>" +
                                      "<div><label>记录类型</label><select class=\"ddns-record-type\"" + disabledAttr + ">" + ddnsTypeOptions(record.recordType) + "</select></div>" +
                                      "<div><label>AccessKey ID</label><input class=\"ddns-username\" autocomplete=\"off\" value=\"" + escapeHtml(record.username) + "\"" + disabledAttr + "></div>" +
                                      "<div><label>AccessKey Secret</label><input class=\"ddns-password\" type=\"password\" autocomplete=\"off\" value=\"" + escapeHtml(record.password) + "\"" + disabledAttr + "></div>" +
                                      "<div><label>TTL</label><input class=\"ddns-ttl\" type=\"number\" min=\"30\" max=\"86400\" value=\"" + String(record.ttl) + "\"" + disabledAttr + "></div>" +
                                      "<div><label>解析值(IP)</label><input class=\"ddns-value\" autocomplete=\"off\" value=\"" + escapeHtml(record.value) + "\"" + disabledAttr + "></div>" +
                                      "</div>" +
                                      "<div class=\"inline-check\">" +
                                      "<input class=\"ddns-local-ip\" type=\"checkbox\"" + (record.useLocalIp ? " checked" : "") + disabledAttr + ">" +
                                      "<label style=\"margin:0;\">使用局域网 IP（仅内网）</label>" +
                                      "</div>" +
                                      "<p class=\"muted\">主机类型： <span class=\"ddns-domain-label\">" + escapeHtml(record.hostType) + "</span>, 完整域名: <span class=\"ddns-domain-preview\">" + escapeHtml(fullDomain || "-") + "</span></p>" +
                                      "<p class=\"muted\">RecordId: " + escapeHtml(record.recordId || "-") + ", Runtime: <span class=\"ddns-record-state\">-</span>, Last IP: <span class=\"ddns-record-ip\">-</span></p>" +
                                      "</div>" +
                                      "</div>");
                                })
                                .join("");
      applyBulmaClasses(container);
      updateDdnsToggleAllButton();

      Array.from(container.querySelectorAll(".ddns-root-domain, .ddns-host-type")).forEach(function (input) {
        input.addEventListener("input", function () {
          const card = this.closest(".ddns-record");
          updateDdnsCardPreview(card);
        });
        input.addEventListener("change", function () {
          const card = this.closest(".ddns-record");
          updateDdnsCardPreview(card);
        });
      });

      Array.from(container.querySelectorAll(".ddns-collapse-toggle")).forEach(function (button) {
        button.addEventListener("click", function () {
          const index = Number(this.dataset.index);
          if (!Number.isFinite(index) || index < 0) {
            return;
          }
          toggleDdnsRecordCollapsed(index);
        });
      });

      Array.from(container.querySelectorAll(".ddns-action-button")).forEach(function (button) {
        button.addEventListener("click", async function () {
          const index = Number(this.dataset.index);
          if (!Number.isFinite(index) || index < 0) {
            return;
          }
          this.disabled = true;
          try {
            await handleDdnsRecordAction(index);
          } catch (error) {
            setText("ddnsStatus", toMessage(error.message));
          } finally {
            this.disabled = false;
          }
        });
      });

      Array.from(container.querySelectorAll(".ddns-remove-button")).forEach(function (button) {
        button.addEventListener("click", async function () {
          const index = Number(this.dataset.index);
          if (!Number.isFinite(index) || index < 0) {
            return;
          }
          this.disabled = true;
          try {
            await handleDdnsRecordDelete(index);
          } catch (error) {
            setText("ddnsStatus", toMessage(error.message));
          } finally {
            this.disabled = false;
          }
        });
      });
    }

    async function handleDdnsRecordAction(index) {
      const records = collectDdnsRecordsFromForm();
      if (index < 0 || index >= records.length) {
        return;
      }

      const currentRecord = normalizeDdnsRecord(records[index]);
      if (!currentRecord.editing && !currentRecord.isNew) {
        records[index] = Object.assign({}, currentRecord, { editing: true });
        renderDdnsRecords(records);
        setText("ddnsStatus", "DDNS record editing enabled.");
        return;
      }

      const validationError = validateDdnsRecordForAliyun(currentRecord);
      if (validationError) {
        setText("ddnsStatus", validationError);
        return;
      }

      let recordId = String(currentRecord.recordId || "").trim();
      if (currentRecord.isNew) {
        recordId = await addDdnsRecordToAliyun(index, currentRecord);
        setText("ddnsStatus", "Aliyun record added.");
      } else {
        if (!recordId) {
          const aliyunRecords = await fetchAliyunRecordsByConfigIndex(index);
          const matched = findAliyunRecordMatch(aliyunRecords, currentRecord.hostType, currentRecord.recordType);
          recordId = matched ? String(matched.recordId || "").trim() : "";
        }
        if (recordId) {
          await updateDdnsRecordOnAliyun(index, recordId, currentRecord);
          setText("ddnsStatus", "Aliyun record updated.");
        } else {
          recordId = await addDdnsRecordToAliyun(index, currentRecord);
          setText("ddnsStatus", "Aliyun record added.");
        }
      }

      const savedRecord = Object.assign({}, currentRecord, {
        recordId: recordId,
        isNew: false,
        editing: false
      });
      records[index] = savedRecord;

      try {
        await persistDdnsConfigRecords(records);
      } catch (error) {
        records[index] = Object.assign({}, savedRecord, { editing: true });
        ddnsRecordsCache = records.map(normalizeDdnsRecord);
        renderDdnsRecords(ddnsRecordsCache);
        setText("ddnsStatus", "Aliyun success, local save failed: " + toMessage(error.message));
        return;
      }

      ddnsRecordsCache = records.map(normalizeDdnsRecord);
      renderDdnsRecords(ddnsRecordsCache);
      await refreshDdnsStatus();
    }

    async function handleDdnsRecordDelete(index) {
      const records = collectDdnsRecordsFromForm();
      if (index < 0 || index >= records.length) {
        return;
      }

      const currentRecord = normalizeDdnsRecord(records[index]);
      const fullDomain = buildDdnsFullDomain(currentRecord.rootDomain, currentRecord.hostType);
      if (!window.confirm("确认删除 DDNS 配置 " + (fullDomain || ("#" + String(index + 1))) + " 吗？")) {
        return;
      }

      if (!currentRecord.isNew) {
        let recordId = String(currentRecord.recordId || "").trim();
        if (!recordId) {
          try {
            const aliyunRecords = await fetchAliyunRecordsByConfigIndex(index);
            const matched = findAliyunRecordMatch(aliyunRecords, currentRecord.hostType, currentRecord.recordType);
            recordId = matched ? String(matched.recordId || "").trim() : "";
          } catch (_) {}
        }
        if (recordId) {
          await deleteDdnsRecordFromAliyun(index, recordId);
        }
      }

      records.splice(index, 1);
      await persistDdnsConfigRecords(records);
      renderDdnsRecords(ddnsRecordsCache);
      await refreshDdnsStatus();
      setText("ddnsStatus", "DDNS record removed.");
    }

    function addDdnsRecord() {
      const currentRecords = collectDdnsRecordsFromForm();
      if (currentRecords.length >= maxDdnsRecords) {
        setText("ddnsStatus", "Max 5 DDNS records are supported.");
        return;
      }
      currentRecords.push(createDefaultDdnsRecord());
      renderDdnsRecords(currentRecords);
      setText("ddnsStatus", "");
    }

    async function saveDdnsEnabled() {
      try {
        await persistDdnsConfigRecords(collectDdnsRecordsFromForm());
        await refreshDdnsStatus();
      } catch (error) {
        setText("ddnsStatus", toMessage(error.message));
      }
    }
    function ddnsStateLabel(state) {
      if (state === "DISABLED") return "已禁用";
      if (state === "WAIT_CONFIG") return "待配置";
      if (state === "WAIT_WIFI") return "等待 WiFi";
      if (state === "READY") return "就绪";
      if (state === "RUNNING") return "运行中";
      if (state === "UPDATED") return "已更新";
      if (state === "ERROR") return "异常";
      return state || "-";
    }

    function updateDdnsStatus(data) {
      const state = data.state || data.ddnsState || "-";
      const message = data.message || data.ddnsMessage || "";
      const activeRecordCount = Number(
          data.activeRecordCount !== undefined ? data.activeRecordCount : data.ddnsActiveRecordCount);
      const totalUpdateCount = Number(
          data.totalUpdateCount !== undefined ? data.totalUpdateCount : data.ddnsTotalUpdateCount);

      setText("ddnsState", ddnsStateLabel(state));
      setText("ddnsActiveCount", Number.isFinite(activeRecordCount) ? String(activeRecordCount) : "0");
      setText("ddnsUpdateCount", Number.isFinite(totalUpdateCount) ? String(totalUpdateCount) : "0");
      if (message) {
        setText("ddnsStatus", message);
      }

      const recordStates = Array.isArray(data.records) ? data.records : [];
      const cards = Array.from(document.querySelectorAll("#ddnsRecords .ddns-record"));
      cards.forEach(function (card, index) {
        const record = recordStates[index];
        if (!record) {
          return;
        }

        const stateElement = card.querySelector(".ddns-record-state");
        const ipElement = card.querySelector(".ddns-record-ip");
        if (stateElement) {
          stateElement.textContent = ddnsStateLabel(record.state || "-");
        }
        if (ipElement) {
          const ip = record.lastNewIp || record.lastOldIp || "-";
          ipElement.textContent = ip;
        }
      });
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
      if (code === "config_index_required") return "缺少账号索引，请先选择账号。";
      if (code === "invalid_config_index") return "账号索引无效，请刷新页面后重试。";
      if (code === "ddns_record_incomplete") return "DDNS 配置不完整，请先补全域名和密钥并保存。";
      if (code === "record_id_required") return "缺少 RecordId。";
      if (code === "value_required") return "解析值不能为空。";
      if (code === "describe_failed") return "拉取阿里云记录失败，请检查网络和凭据。";
      if (code === "add_failed") return "新增阿里云记录失败。";
      if (code === "update_failed") return "修改阿里云记录失败。";
      if (code === "delete_failed") return "删除阿里云记录失败。";
      if (code === "ssid_required") return "SSID 不能为空。";
      if (code === "wifi_not_connected") return "WiFi 未连接，无法执行该操作。";
      if (code === "config_mac_required") return "请先配置目标电脑 MAC 地址。";
      if (code === "config_ip_invalid") return "请先配置正确的目标电脑 IP 地址。";
      if (code === "wol_send_failed") return "发送开机包失败，请检查网络环境。";
      if (code === "udp_begin_failed") return "UDP 通道初始化失败。";
      if (code === "boot_timeout") return "等待主机上线超时。";
      if (code === "already_booting") return "开机流程进行中，请稍候。";
      if (code === "already_on") return "主机已经在线。";
      if (code === "ota_busy") return "OTA 任务执行中，请稍后再试。";
      if (code === "ota_too_frequent") return "手动 OTA 操作触发过于频繁，请稍后重试。";
      if (code === "ota_check_too_frequent") return "手动检测触发过于频繁，请稍后重试。";
      if (code === "ota_upgrade_too_frequent") return "手动升级触发过于频繁，请稍后重试。";
      if (code === "ota_config_incomplete") return "OTA 配置不完整，请先填写巴法云 UID 和 Topic。";
      if (code === "ota_no_update") return "未发布可用新固件。";
      if (code === "ota_lookup_http_failed") return "查询 OTA 失败，请检查网络。";
      if (code === "ota_lookup_http_status") return "OTA 接口返回异常状态码。";
      if (code === "ota_lookup_parse_failed") return "解析 OTA 响应失败。";
      if (code === "ota_lookup_rejected") return "OTA 拒绝了请求。";
      if (code === "ota_version_invalid") return "OTA 返回的版本号无效。";
      if (code === "ota_url_missing") return "OTA 响应缺少固件地址。";
      if (code === "ota_update_failed") return "固件更新失败，请查看设备日志。";
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
      button.innerHTML =
          busy
              ? "<span class=\"icon\"><i class=\"fa-solid fa-spinner fa-spin\"></i></span>"
              : "<span class=\"icon\"><i class=\"fa-solid fa-power-off\"></i></span>";
    }

    function bemfaStateLabel(state) {
      if (state === "DISABLED") return "已禁用";
      if (state === "WAIT_CONFIG") return "待配置";
      if (state === "WAIT_WIFI") return "等待 WiFi";
      if (state === "READY") return "准备连接";
      if (state === "CONNECTING") return "连接中";
      if (state === "SUSPENDED") return "已暂停";
      if (state === "ONLINE") return "在线";
      if (state === "OFFLINE") return "离线";
      if (state === "ERROR") return "异常";
      return state || "-";
    }

    function bemfaMessageLabel(message) {
      if (!message) return "";
      if (message === "Disconnected on logout.") return "退出断开";
      if (message === "Bemfa suspended after logout.") return "已暂停";
      if (message === "Waiting to reconnect.") return "等待重连";
      return message;
    }

    function updateBemfaStatus(data) {
      const state = data.state || data.bemfaState || "-";
      const connected = data.connected !== undefined ? !!data.connected : !!data.bemfaConnected;
      const message = data.message || data.bemfaMessage || "";
      const subscribeTopic = data.subscribeTopic || ((data.bemfaTopic || "") ? ((data.bemfaTopic || "") + "/set") : "");
      const publishTopic = data.publishTopic || ((data.bemfaTopic || "") ? ((data.bemfaTopic || "") + "/up") : "");

      setText("bemfaState", bemfaStateLabel(state));
      setText("bemfaConnected", connected ? "已连接" : "未连接");
      setText("bemfaStatus", bemfaMessageLabel(message));
      if (subscribeTopic || publishTopic) {
        setText("bemfaTopics", "订阅: " + (subscribeTopic || "-") + " | 上报: " + (publishTopic || "-"));
      } else {
        setText("bemfaTopics", "-");
      }
    }

    function otaStateLabel(state) {
      if (state === "WAIT_CONFIG") return "配置不完整";
      if (state === "READY") return "可手动升级";
      if (state === "QUEUED") return "已排队";
      if (state === "CHECKING") return "检测中";
      if (state === "UPDATE_AVAILABLE") return "发现新版本";
      if (state === "DOWNLOADING") return "升级中";
      if (state === "UPDATED") return "升级成功";
      if (state === "NO_UPDATE") return "无新版本";
      if (state === "FAILED") return "失败";
      return state || "-";
    }
    function otaMessageLabel(message) {
      if (!message) return "";
      if (message === "OTA requires Bemfa UID and Topic.") return "OTA 配置不完整，请先填写巴法云 UID 和主题。";
      if (message === "Manual OTA is ready.") return "可手动升级。";
      if (message === "Auto OTA check is enabled.") return "已启用 OTA 自动检测。";
      if (message === "Auto OTA settings updated.") return "OTA 自动检测配置已更新。";
      if (message === "WiFi is disconnected.") return "WiFi 未连接。";
      if (message === "Checking firmware metadata from Bemfa.") return "正在查询巴法云固件信息。";
      if (message === "Downloading and applying firmware.") return "正在下载并更新固件。";
      if (message === "Auto OTA request queued.") return "已加入自动 OTA 升级检测队列。";
      if (message === "Manual OTA request queued.") return "已加入手动 OTA 升级检测队列。";
      if (message === "Auto OTA check request queued.") return "已加入自动 OTA 检测队列。";
      if (message === "Manual OTA check request queued.") return "已加入手动 OTA 检测队列。";
      if (message === "Auto OTA upgrade request queued.") return "已加入自动 OTA 升级队列。";
      if (message === "Manual OTA upgrade request queued.") return "已加入手动 OTA 升级队列。";
      if (message === "No OTA package is published on Bemfa.") return "巴法云未发布可用新固件。";
      if (message === "OTA package found on Bemfa.") return "检测到巴法云可用固件。";
      if (message === "Firmware update completed, rebooting.") return "固件升级完成，设备即将重启。";
      if (message === "No new firmware to apply.") return "没有可应用的新固件。";
      if (message === "Failed to parse Bemfa OTA response code.") return "解析巴法云 OTA 响应失败。";
      if (message === "Failed to open Bemfa OTA endpoint.") return "无法连接巴法云 OTA 接口。";
      if (message === "Bemfa OTA response does not contain firmware URL.") return "巴法云 OTA 响应缺少固件下载地址。";
      if (message.indexOf("New firmware available. Local=") === 0 ||
          message.indexOf("Firmware package available. Local=") === 0) {
        return "检测到新固件：" +
            message.replace("New firmware available. ", "")
                   .replace("Firmware package available. ", "")
                   .replace("Local=", "本地=")
                   .replace("remote=", "远端=");
      }
      if (message.indexOf("No newer firmware version. Local=") === 0 ||
          message.indexOf("No firmware version change. Local=") === 0) {
        return "当前已是最新版本：" +
            message.replace("No newer firmware version. ", "")
                   .replace("No firmware version change. ", "")
                   .replace("Local=", "本地=")
                   .replace("remote=", "远端=");
      }
      if (message.indexOf("Downloading firmware (") === 0) {
        const start = message.indexOf("(");
        const end = message.indexOf(")", start + 1);
        const percent = (start >= 0 && end > start) ? message.substring(start + 1, end) : "";
        return percent ? ("正在下载固件（" + percent + "）") : "正在下载固件...";
      }
      if (message.indexOf("Firmware update failed: ") === 0) {
        return "固件升级失败：" + message.replace("Firmware update failed: ", "");
      }
      if (message.indexOf("Bemfa OTA request failed, code=") === 0) {
        return "查询巴法云 OTA 失败，错误码：" + message.replace("Bemfa OTA request failed, code=", "");
      }
      if (message.indexOf("Bemfa OTA rejected request, code=") === 0) {
        return "巴法云 OTA 拒绝请求，错误码：" + message.replace("Bemfa OTA rejected request, code=", "");
      }
      if (message.indexOf("Bemfa OTA HTTP status is ") === 0) {
        return "巴法云 OTA 接口状态异常：" + message.replace("Bemfa OTA HTTP status is ", "");
      }
      return message;
    }
    function updateOtaStatus(data) {
      const state = data.state || "-";
      const message = data.message || "";
      const error = data.error || "";
      const targetVersion = data.targetVersion || "";
      const targetTag = data.targetTag || "";
      const progressPercent = Math.max(0, Math.min(100, Number(data.progressPercent) || 0));
      const progressBytes = Number(data.progressBytes) || 0;
      const progressTotalBytes = Number(data.progressTotalBytes) || 0;
      const busy = !!data.busy || !!data.pending;
      const updateAvailable = !!data.updateAvailable;

      setText("otaState", otaStateLabel(state));
      if (progressTotalBytes > 0) {
        setText("otaProgress", String(progressPercent) + "% (" + formatBytes(progressBytes) + " / " + formatBytes(progressTotalBytes) + ")");
      } else {
        setText("otaProgress", String(progressPercent) + "%");
      }
      setText(
          "otaTargetVersion",
          (targetVersion || targetTag) ? ((targetVersion || "-") + (targetTag ? (" (" + targetTag + ")") : "")) : "-");
      setText("otaStatus", error ? toMessage(error) : otaMessageLabel(message));

      if (busy) {
        startOtaActionPolling();
      } else {
        stopOtaActionPolling();
      }

      const checkButton = document.getElementById("otaCheckButton");
      const upgradeButton = document.getElementById("otaUpgradeButton");
      checkButton.disabled = busy;
      checkButton.textContent = busy ? "处理中..." : "手动检测新版本";

       upgradeButton.style.display = updateAvailable ? "" : "none";
       upgradeButton.disabled = busy;
       upgradeButton.textContent = busy ? "升级中..." : "升级固件";

       if (state === "UPDATED") {
         setTimeout(function() {
           window.location.href = "/login?message=upgrade_success";
         }, 1000);
       }
     }
    function formatBytes(value) {
      const bytes = Number(value);
      if (!Number.isFinite(bytes) || bytes < 0) {
        return "-";
      }
      if (bytes < 1024) {
        return Math.round(bytes) + " B";
      }

      const units = ["KB", "MB", "GB"];
      let size = bytes / 1024;
      let unitIndex = 0;
      while (size >= 1024 && unitIndex < (units.length - 1)) {
        size /= 1024;
        unitIndex += 1;
      }
      const precision = size >= 100 ? 0 : (size >= 10 ? 1 : 2);
      return size.toFixed(precision) + " " + units[unitIndex];
    }

    function formatUptime(secondsValue) {
      const seconds = Math.max(0, Number(secondsValue) || 0);
      const days = Math.floor(seconds / 86400);
      const hours = Math.floor((seconds % 86400) / 3600);
      const minutes = Math.floor((seconds % 3600) / 60);
      const secs = Math.floor(seconds % 60);
      const hhmmss =
          String(hours).padStart(2, "0") + ":" + String(minutes).padStart(2, "0") + ":" + String(secs).padStart(2, "0");
      if (days > 0) {
        return days + "天 " + hhmmss;
      }
      return hhmmss;
    }

    function updateSystemInfo(data) {
      setText("espUptime", formatUptime(data.uptimeSeconds));
      setText("espHeap", formatBytes(data.heapFree) + " / " + formatBytes(data.heapTotal));
      setText("espHeapMin", formatBytes(data.heapMinFree));
      setText("espHeapMaxAlloc", formatBytes(data.heapMaxAlloc));
      setText("espFlash", formatBytes(data.sketchUsed) + " / " + formatBytes(data.flashTotal));
      setText("espFlashFree", formatBytes(data.flashFree));

      // 保存时间戳用于实时更新
      lastUnixTime = Number(data.systemTimeUnix) || 0;
      lastClientEpochSeconds = Math.floor(Date.now() / 1000);
      
      // 立即更新时间显示
      updateRealTimeDisplay();
      
      // 如果定时器未启动，启动实时更新时间显示
      if (!timeUpdateTimer && lastUnixTime > 0) {
        timeUpdateTimer = setInterval(updateRealTimeDisplay, 1000);
      }

      const psramTotal = Number(data.psramTotal) || 0;
      if (psramTotal > 0) {
        setText("espPsram", formatBytes(data.psramFree) + " / " + formatBytes(psramTotal));
      } else {
        setText("espPsram", "不支持");
      }

      setText("espInfoStatus", "最后更新：" + new Date().toLocaleTimeString());
    }
    
    // 格式化Unix时间戳为本地时间字符串
    function formatUnixTime(unixTime) {
      if (!unixTime || unixTime <= 0) {
        return "-";
      }
      const date = new Date(unixTime * 1000);
      const year = date.getFullYear();
      const month = String(date.getMonth() + 1).padStart(2, '0');
      const day = String(date.getDate()).padStart(2, '0');
      const hours = String(date.getHours()).padStart(2, '0');
      const minutes = String(date.getMinutes()).padStart(2, '0');
      const seconds = String(date.getSeconds()).padStart(2, '0');
      return `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
    }
    
    // 实时更新时间显示
    function updateRealTimeDisplay() {
      if (lastUnixTime <= 0) {
        return;
      }
      
      // 计算当前时间：最后获取的Unix时间 + (当前运行时间 - 最后获取时的运行时间)
      const nowClientEpochSeconds = Math.floor(Date.now() / 1000);
      const elapsedSeconds = Math.max(0, nowClientEpochSeconds - lastClientEpochSeconds);
      const currentUnixTime = lastUnixTime + elapsedSeconds;
      
      setText("espSystemTime", formatUnixTime(currentUnixTime));
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
      document.getElementById("computerIp").value = data.computerIp || "";
      document.getElementById("computerMac").value = data.computerMac || "";
      document.getElementById("computerPort").value = data.computerPort || "";

      document.getElementById("bemfaEnabled").checked = !!data.bemfaEnabled;
      document.getElementById("bemfaTopic").value = data.bemfaTopic || "";
      document.getElementById("bemfaUid").value = data.bemfaUid || "";
      document.getElementById("bemfaKey").value = data.bemfaKey || "";
      document.getElementById("bemfaHost").value = data.bemfaHost || "bemfa.com";
      document.getElementById("bemfaPort").value = data.bemfaPort || 9501;
      document.getElementById("ddnsEnabled").checked = !!data.ddnsEnabled;
      const configRecords = Array.isArray(data.ddnsConfigRecords)
          ? data.ddnsConfigRecords
          : (Array.isArray(data.ddnsRecords) ? data.ddnsRecords : []);
      renderDdnsRecords(configRecords.map(function (record) {
        return Object.assign({}, record, { isNew: false, editing: false, collapsed: true });
      }));
      await refreshDdnsRecordSnapshots();
      setText("otaCurrentVersion", data.otaCurrentVersion || "-");

      updatePowerStatus(data);
      updateBemfaStatus(data);
      updateDdnsStatus(data);
      applyStatusPolling(data.statusPollIntervalMinutes);

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

    async function refreshSystemInfo() {
      try {
        const data = await api("/api/system/info");
        updateSystemInfo(data);
      } catch (error) {
        setText("espInfoStatus", toMessage(error.message));
      }
    }

    async function refreshBemfaStatus() {
      try {
        const data = await api("/api/bemfa/status");
        updateBemfaStatus(data);
      } catch (error) {
        setText("bemfaStatus", toMessage(error.message));
      }
    }

    async function refreshDdnsStatus() {
      try {
        const data = await api("/api/ddns/status");
        updateDdnsStatus(data);
      } catch (error) {
        setText("ddnsStatus", toMessage(error.message));
      }
    }

    async function refreshOtaStatus() {
      try {
        const data = await api("/api/ota/status");
        updateOtaStatus(data);
      } catch (error) {
        const upgradeButton = document.getElementById("otaUpgradeButton");
        if (upgradeButton) {
          upgradeButton.style.display = "none";
          upgradeButton.disabled = false;
          upgradeButton.textContent = "升级固件";
        }
        setText("otaStatus", toMessage(error.message));
      }
    }

    async function refreshAllStatus() {
      await Promise.all([refreshPowerStatus(),
                         refreshBemfaStatus(),
                         refreshDdnsStatus(),
                         refreshOtaStatus(),
                         refreshSystemInfo()]);
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

    async function triggerManualOtaCheck() {
      const button = document.getElementById("otaCheckButton");
      button.disabled = true;
      button.textContent = "提交中...";

      try {
        const data = await api("/api/ota/check", { method: "POST" });
        setText("otaStatus", otaMessageLabel(data.message) || "已提交 OTA 检测请求。");
        startOtaActionPolling();
        await refreshOtaStatus();
      } catch (error) {
        setText("otaStatus", toMessage(error.message));
        await refreshOtaStatus();
      }
    }

    async function triggerManualOtaUpgrade() {
      const button = document.getElementById("otaUpgradeButton");
      button.disabled = true;
      button.textContent = "提交中...";

      try {
        const data = await api("/api/ota/upgrade", { method: "POST" });
        setText("otaStatus", otaMessageLabel(data.message) || "已提交 OTA 升级请求。");
        startOtaActionPolling();
        await refreshOtaStatus();
      } catch (error) {
        setText("otaStatus", toMessage(error.message));
        await refreshOtaStatus();
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

    function waitMs(durationMs) {
      return new Promise(function (resolve) {
        setTimeout(resolve, durationMs);
      });
    }

    function formatWifiConnectMessage(data, fallbackMessage) {
      const message = (data && data.message) ? data.message : fallbackMessage;
      const ip = (data && data.ip) ? ("，IP：" + data.ip) : "";
      return message + ip;
    }

    async function waitForWifiConnectResult(timeoutMs) {
      const deadline = Date.now() + timeoutMs;
      while (Date.now() < deadline) {
        await waitMs(1000);
        const status = await api("/api/wifi/status");
        if (status.connected || !status.connecting) {
          return status;
        }
      }
      return { connected: false, connecting: false, message: "WiFi connection timed out." };
    }

    async function connectWifi() {
      const connectButton = document.getElementById("connectButton");
      connectButton.disabled = true;
      const params = new URLSearchParams();
      params.set("ssid", (document.getElementById("wifiSsid").value || "").trim());
      params.set("password", document.getElementById("wifiPassword").value || "");
      try {
        const data = await api("/api/wifi/connect", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: params.toString()
        });
        setText("wifiStatus", formatWifiConnectMessage(data, "Connecting WiFi..."));
        if (data.connecting && !data.connected) {
          const finalStatus = await waitForWifiConnectResult(18000);
          setText("wifiStatus", formatWifiConnectMessage(finalStatus, "WiFi connection failed."));
        }
      } catch (error) {
        setText("wifiStatus", toMessage(error.message));
      } finally {
        connectButton.disabled = false;
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

    async function saveBemfaConfig(event) {
      event.preventDefault();
      const params = new URLSearchParams();
      params.set("bemfaEnabled", document.getElementById("bemfaEnabled").checked ? "1" : "0");
      params.set("bemfaTopic", (document.getElementById("bemfaTopic").value || "").trim());
      params.set("bemfaUid", (document.getElementById("bemfaUid").value || "").trim());
      params.set("bemfaKey", (document.getElementById("bemfaKey").value || "").trim());
      params.set("bemfaHost", (document.getElementById("bemfaHost").value || "").trim());
      params.set("bemfaPort", (document.getElementById("bemfaPort").value || "").trim());

      try {
        await api("/api/config", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: params.toString()
        });
        setText("bemfaStatus", "巴法云配置已保存。");
        await refreshBemfaStatus();
      } catch (error) {
        setText("bemfaStatus", toMessage(error.message));
      }
    }
    async function saveSystemConfig(event) {
      event.preventDefault();
      const params = new URLSearchParams();
      params.set(
          "statusPollIntervalMinutes",
          document.getElementById("statusPollIntervalMinutes").value || String(defaultStatusPollIntervalMinutes));

      try {
        await api("/api/config", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: params.toString()
        });
        await loadConfig();
        setText("systemConfigStatus", "系统配置已保存。");
      } catch (error) {
        setText("systemConfigStatus", toMessage(error.message));
      }
    }

    async function refreshAllStatusByButton() {
      const button = document.getElementById("refreshAllButton");
      const originalLabel = button.textContent;
      button.disabled = true;
      button.textContent = "刷新中...";
      try {
        await refreshAllStatus();
        setText("systemConfigStatus", "状态已刷新。");
      } finally {
        button.disabled = false;
        button.textContent = originalLabel;
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
    document.getElementById("otaCheckButton").addEventListener("click", triggerManualOtaCheck);
    document.getElementById("otaUpgradeButton").addEventListener("click", triggerManualOtaUpgrade);
    document.getElementById("configForm").addEventListener("submit", saveConfig);
    document.getElementById("bemfaForm").addEventListener("submit", saveBemfaConfig);
    document.getElementById("ddnsForm").addEventListener("submit", function (event) { event.preventDefault(); });
    document.getElementById("ddnsAddRecordButton").addEventListener("click", addDdnsRecord);
    document.getElementById("ddnsToggleAllButton").addEventListener("click", toggleAllDdnsRecords);
    document.getElementById("ddnsEnabled").addEventListener("change", saveDdnsEnabled);
    document.getElementById("systemForm").addEventListener("submit", saveSystemConfig);
    document.getElementById("refreshAllButton").addEventListener("click", refreshAllStatusByButton);
    document.getElementById("passwordForm").addEventListener("submit", savePassword);
    document.getElementById("themeToggle").addEventListener("click", cycleThemeMode);

    window.addEventListener("DOMContentLoaded", async function () {
      initThemeMode();
      applyBulmaClasses();
      updateScanButtonState();
      renderDdnsRecords([]);
      const otaUpgradeButton = document.getElementById("otaUpgradeButton");
      if (otaUpgradeButton) {
        otaUpgradeButton.style.display = "none";
      }
      try {
        await loadConfig();
      } catch (error) {
        setText("configStatus", toMessage(error.message));
      }
      await refreshAllStatus();
      await scanWifi();
    });
  </script>
</body>
</html>
)HTML";
  return kDashboardPage;
}

