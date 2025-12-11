#include "WiFiSetupPortal.h"
#include <Arduino.h>
#include <stdarg.h>

WiFiSetupPortal::WiFiSetupPortal() {
  _server = nullptr;
  _dnsServer = nullptr;
  _isConnected = false;
  _dashboardURL = "";
  _currentStatus = "BOOTING";
  _taskHandle = nullptr;
}

WiFiSetupPortal::~WiFiSetupPortal() {
  stop();
}

void WiFiSetupPortal::begin(const Config& config) {
  _config = config;
  _dashboardURL = _config.defaultDashboardURL;
  
  _safePrintln("\n[WiFi] Initializing setup portal...");
  
  // Initialize WiFi in AP+STA mode
  _initWiFiAP();
  
  // Setup web server
  _server = new WebServer(80);
  _dnsServer = new DNSServer();
  _dnsServer->start(53, "*", WiFi.softAPIP());
  
  // Setup routes
  _setupRoutes();
  
  _safePrintf("[WiFi] AP started: %s (IP: %s)\n", 
              _config.apName, WiFi.softAPIP().toString().c_str());
}

void WiFiSetupPortal::_initWiFiAP() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(_config.apName, _config.apPassword, _config.apChannel, _config.apHidden);
  
  if (_config.debugMode) {
    _safePrintf("[WiFi] AP MAC: %s\n", WiFi.softAPmacAddress().c_str());
  }
}

void WiFiSetupPortal::_setupRoutes() {
  _server->on("/", std::bind(&WiFiSetupPortal::_handleRoot, this));
  _server->on("/connect", HTTP_POST, std::bind(&WiFiSetupPortal::_handleConnect, this));
  _server->on("/scan", std::bind(&WiFiSetupPortal::_handleScan, this));
  _server->on("/status", std::bind(&WiFiSetupPortal::_handleStatus, this));
  _server->onNotFound(std::bind(&WiFiSetupPortal::_handleNotFound, this));
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Robot Setup</title>
  <style>
    :root {
      --bg-dark: #0f1724;
      --bg-card: #1e293b;
      --accent: #38bdf8;
      --success: #34d399;
      --text: #f1f5f9;
      --text-muted: #94a3b8;
      --border: #334155;
    }
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, 'Open Sans', 'Helvetica Neue', sans-serif;
      background: var(--bg-dark);
      color: var(--text);
      line-height: 1.5;
      padding: 16px;
      max-width: 500px;
      margin: 0 auto;
    }
    .container { margin: 16px 0; }
    h1 {
      font-size: 1.8rem;
      text-align: center;
      margin: 16px 0;
      color: var(--accent);
      font-weight: 700;
    }
    .status {
      background: rgba(56, 189, 248, 0.15);
      color: var(--text);
      padding: 8px 12px;
      border-radius: 8px;
      text-align: center;
      font-weight: 600;
      margin: 12px 0;
      border: 1px solid var(--accent);
    }
    .card {
      background: var(--bg-card);
      border-radius: 12px;
      padding: 16px;
      margin: 16px 0;
      border: 1px solid var(--border);
    }
    .card-title {
      font-weight: 600;
      margin-bottom: 12px;
      color: var(--accent);
      font-size: 1.1rem;
    }
    button {
      background: linear-gradient(to right, #1e40af, #1d4ed8);
      color: white;
      border: none;
      border-radius: 8px;
      padding: 12px;
      font-weight: 600;
      font-size: 1rem;
      cursor: pointer;
      margin: 8px 0;
      transition: opacity 0.2s;
    }
    button:disabled {
      opacity: 0.7;
      cursor: not-allowed;
    }
    button.scan { background: linear-gradient(to right, #0f766e, #115e59); }
    button.connect { background: linear-gradient(to right, #166534, #14532d); }
    button.dash { background: linear-gradient(to right, #7e22ce, #6b21a8); }
    button.pass-toggle {
      background: var(--bg-card);
      color: var(--text-muted);
      width: 50px;
      margin-left: 8px;
      padding: 8px;
    }
    input {
      width: 100%;
      padding: 12px;
      border-radius: 8px;
      border: 1px solid var(--border);
      background: rgba(30, 41, 59, 0.7);
      color: var(--text);
      font-size: 1rem;
      margin: 8px 0;
    }
    .input-group {
      display: flex;
      gap: 8px;
    }
    #networks {
      max-height: 200px;
      overflow-y: auto;
      margin-top: 12px;
      border-top: 1px solid var(--border);
      padding-top: 8px;
    }
    .net {
      padding: 10px;
      border-radius: 8px;
      margin: 6px 0;
      cursor: pointer;
      transition: background 0.2s;
      display: flex;
      justify-content: space-between;
    }
    .net:hover { background: rgba(56, 189, 248, 0.1); }
    .net.selected { 
      background: rgba(56, 189, 248, 0.2); 
      border: 1px solid var(--accent);
    }
    .footer {
      text-align: center;
      color: var(--text-muted);
      font-size: 0.85rem;
      margin-top: 20px;
      padding-top: 12px;
      border-top: 1px solid var(--border);
    }
    .hidden { display: none; }
  </style>
</head>
<body>
  <div class="container">
    <h1>🤖 Robot WiFi Setup</h1>
    <div class="status" id="status">BOOTING</div>
    
    <div id="config">
      <div class="card">
        <div class="card-title">Available Networks</div>
        <button class="scan" id="scan" onclick="scan()">↻ Scan Networks</button>
        <div id="networks"></div>
      </div>
      
      <div class="card">
        <div class="card-title">WiFi Credentials</div>
        <input type="text" id="ssid" placeholder="Select network or enter SSID" required>
        <div class="input-group">
          <input type="password" id="pass" placeholder="Password" required>
          <button type="button" class="pass-toggle" onclick="togglePass()">👁</button>
        </div>
        <button class="connect" onclick="connect()">Connect Robot</button>
      </div>
    </div>
    
    <div id="connected" class="card hidden">
      <div class="card-title">✅ Connected Successfully</div>
      <p style="color: var(--text-muted); margin: 12px 0;">Robot is online and ready for operation.</p>
      <button class="dash" onclick="openDash()">Open Dashboard</button>
    </div>
    
    <div class="footer">AP Name: <strong>%AP_SSID%</strong></div>
  </div>

  <script>
    let dashURL = "%DASHBOARD_URL%";
    
    function rssiBars(rssi) {
      if (rssi > -55) return '📶'.repeat(4);
      if (rssi > -65) return '📶'.repeat(3);
      if (rssi > -75) return '📶'.repeat(2);
      return '📶';
    }
    
    function scan() {
      const btn = document.getElementById('scan');
      btn.disabled = true;
      btn.innerHTML = 'Scanning...';
      fetch('/scan').then(r => r.json()).then(data => {
        const list = document.getElementById('networks');
        list.innerHTML = '';
        const networks = data.networks || [];
        if (networks.length === 0) {
          list.innerHTML = '<div style="color:var(--text-muted); text-align:center; padding:12px">No networks found</div>';
        } else {
          networks.forEach(net => {
            const div = document.createElement('div');
            div.className = 'net';
            div.innerHTML = `
              <span>${rssiBars(net.rssi)} ${net.secure ? '🔒' : ''} ${escapeHtml(net.ssid)}</span>
              <span style="color: var(--text-muted); font-size: 0.85rem">${net.rssi} dBm</span>
            `;
            div.onclick = () => select(net.ssid, div);
            list.appendChild(div);
          });
        }
      }).catch(err => {
        console.error('Scan failed:', err);
        document.getElementById('networks').innerHTML = 
          '<div style="color:#f87171; text-align:center; padding:12px">Scan failed. Try again.</div>';
      }).finally(() => {
        btn.disabled = false;
        btn.innerHTML = '↻ Scan Networks';
      });
    }
    
    function select(ssid, el) {
      document.querySelectorAll('.net').forEach(e => e.classList.remove('selected'));
      el.classList.add('selected');
      document.getElementById('ssid').value = ssid;
      document.getElementById('pass').focus();
    }
    
    function togglePass() {
      const p = document.getElementById('pass');
      p.type = p.type === 'password' ? 'text' : 'password';
    }
    
    function connect() {
      const ssid = document.getElementById('ssid').value.trim();
      const pass = document.getElementById('pass').value;
      if (!ssid) {
        alert('Please select or enter a WiFi network name');
        return;
      }
      document.getElementById('status').innerText = 'SENDING CREDENTIALS';
      fetch('/connect', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: `ssid=${encodeURIComponent(ssid)}&pass=${encodeURIComponent(pass)}`
      }).catch(console.error);
    }
    
    function openDash() {
      if (dashURL.includes('http')) {
        window.open(dashURL, '_blank');
      } else {
        alert('Dashboard URL not available yet');
      }
    }
    
    function escapeHtml(unsafe) {
      return unsafe.replace(/[&<>"']/g, m => 
        ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#039;' }[m]));
    }
    
    const evt = new EventSource('/status');
    evt.onmessage = e => {
      try {
        const d = JSON.parse(e.data);
        if (d.url) dashURL = d.url;
        document.getElementById('status').innerText = (d.status || 'UNKNOWN').replace(/_/g, ' ');
        document.getElementById('config').classList.toggle('hidden', d.connected);
        document.getElementById('connected').classList.toggle('hidden', !d.connected);
      } catch(err) {
        console.error('SSE parse error:', err);
      }
    };
    evt.onerror = () => console.log('SSE connection error');
  </script>
</body>
</html>
)rawliteral";

void WiFiSetupPortal::_handleRoot() {
  String html = index_html;
  html.replace("%DASHBOARD_URL%", _dashboardURL);
  html.replace("%AP_SSID%", _config.apName);
  _server->send(200, "text/html", html);
}

void WiFiSetupPortal::_handleConnect() {
  if (_server->hasArg("ssid") && _server->hasArg("pass")) {
    String ssid = _server->arg("ssid");
    String pass = _server->arg("pass");
    
    _safePrintf("[WiFi] Credentials received: SSID=%s\n", ssid.c_str());
    
    if (_credentialsCallback) {
      _credentialsCallback(ssid.c_str(), pass.c_str());
    } else {
      // Default behavior: print to serial for main app to handle
      if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        Serial.printf("SSID=%s;PASS=%s\n", ssid.c_str(), pass.c_str());
        xSemaphoreGive(serialMutex);
      }
    }
    
    _currentStatus = "CREDENTIALS_SENT";
    _isConnected = false;
    _server->send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    _server->send(400, "application/json", "{\"error\":\"Missing SSID or password\"}");
  }
}

void WiFiSetupPortal::_handleScan() {
  int n = WiFi.scanNetworks();
  
  StaticJsonDocument<1024> doc;
  JsonArray arr = doc.createNestedArray("networks");
  
  for (int i = 0; i < min(n, 10); i++) {
    JsonObject obj = arr.createNestedObject();
    obj["ssid"] = WiFi.SSID(i);
    obj["rssi"] = WiFi.RSSI(i);
    obj["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
  }
  
  String out;
  serializeJson(doc, out);
  _server->send(200, "application/json", out);
}

void WiFiSetupPortal::_handleStatus() {
  _server->setContentLength(CONTENT_LENGTH_UNKNOWN);
  _server->send(200, "text/event-stream");
  _server->sendContent("retry: 1000\n\n");
  
  StaticJsonDocument<128> doc;
  doc["status"] = _currentStatus;
  doc["connected"] = _isConnected;
  doc["url"] = _dashboardURL;
  
  String json;
  serializeJson(doc, json);
  _server->sendContent("data: " + json + "\n\n");
  _server->client().stop();
}

void WiFiSetupPortal::_handleNotFound() {
  String header = "HTTP/1.1 302 Found\r\nLocation: http://192.168.4.1\r\nCache-Control: no-cache\r\n\r\n";
  _server->sendContent(header);
  _server->client().stop();
}

void WiFiSetupPortal::_processSerialCommands() {
  while (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();
    if (msg.length() > 0) {
      if (_config.debugMode) {
        _safePrintf("[WiFi] Serial msg: %s\n", msg.c_str());
      }
      
      if (msg.startsWith("CONNECTED_OK") || msg.startsWith("CONNECTED_ASSOCIATED")) {
        _isConnected = true;
        _currentStatus = msg;
        int urlPos = msg.indexOf("http");
        if (urlPos != -1) {
          _dashboardURL = msg.substring(urlPos);
          if (_config.debugMode) {
            _safePrintf("[WiFi] Dashboard URL updated: %s\n", _dashboardURL.c_str());
          }
        }
      } 
      else if (msg.startsWith("CONNECTED_NO_INTERNET")) {
        _isConnected = true;
        _currentStatus = msg;
      }
      else {
        _isConnected = false;
        _currentStatus = msg;
      }
    }
  }
}

void WiFiSetupPortal::beginTask(int coreID, int stackSize, int priority) {
  begin(); // Initialize first
  
  if (_taskHandle == nullptr) {
    xTaskCreatePinnedToCore(
      _taskFunction,
      "WiFiSetupTask",
      stackSize,
      this,
      priority,
      &_taskHandle,
      coreID
    );
    
    if (_config.debugMode) {
      _safePrintf("[WiFi] Task started on Core %d\n", coreID);
    }
  }
}

void WiFiSetupPortal::_taskFunction(void* param) {
  WiFiSetupPortal* instance = static_cast<WiFiSetupPortal*>(param);
  instance->_runTask();
}

void WiFiSetupPortal::_runTask() {
  _safePrintln("[WiFi] 🌐 Setup portal task running");
  
  while (true) {
    if (_server) {
      _dnsServer->processNextRequest();
      _server->handleClient();
      _processSerialCommands();
    }
    
    vTaskDelay(pdMS_TO_TICKS(10)); // Yield to other tasks
  }
}

void WiFiSetupPortal::loop() {
  if (_server) {
    _dnsServer->processNextRequest();
    _server->handleClient();
    _processSerialCommands();
  }
}

void WiFiSetupPortal::stop() {
  if (_taskHandle != nullptr) {
    vTaskDelete(_taskHandle);
    _taskHandle = nullptr;
  }
  
  if (_server) {
    _server->stop();
    delete _server;
    _server = nullptr;
  }
  
  if (_dnsServer) {
    _dnsServer->stop();
    delete _dnsServer;
    _dnsServer = nullptr;
  }
  
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  
  _safePrintln("[WiFi] 🛑 Setup portal stopped");
}

bool WiFiSetupPortal::isConnected() {
  return _isConnected;
}

String WiFiSetupPortal::getDashboardURL() {
  return _dashboardURL;
}

String WiFiSetupPortal::getStatus() {
  return _currentStatus;
}

void WiFiSetupPortal::setCredentialsCallback(CredentialsCallback callback) {
  _credentialsCallback = callback;
}

void WiFiSetupPortal::_safePrint(const char* str) {
  if (serialMutex && xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    Serial.print(str);
    xSemaphoreGive(serialMutex);
  }
}

void WiFiSetupPortal::_safePrintln(const char* str) {
  if (serialMutex && xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    Serial.println(str);
    xSemaphoreGive(serialMutex);
  }
}

void WiFiSetupPortal::_safePrintf(const char* format, ...) {
  if (serialMutex && xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Serial.print(buffer);
    xSemaphoreGive(serialMutex);
  }
}
