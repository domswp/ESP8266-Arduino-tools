#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266Ping.h>
#include <ESP8266HTTPClient.h>

// ===== KONFIGURASI WIFI =====
const char* ssid = "Lantaib2";
const char* password = "Nasih51pria";

// ===== SERVER =====
ESP8266WebServer server(80);
WiFiClient client;

// ===== DATA DEVICE SCANNER =====
struct Device {
  IPAddress ip;
  String mac;
  String hostname;
  bool active;
  int pingTime;
};

Device devices[50];
int deviceCount = 0;

// ===== DATA SPEED TEST =====
struct SpeedTest {
  int pingGoogle;
  int pingCloudflare;
  int pingLocal;
  float downloadSpeed;
  bool internetOK;
};

SpeedTest speedTest;
unsigned long lastSpeedTest = 0;
unsigned long bootTime = 0;

// ===== FUNGSI PING TEST =====
void runPingTest() {
  Serial.println("\n=== Running Ping Test ===");
  
  // Ping Google DNS
  Serial.print("Pinging 8.8.8.8... ");
  if (Ping.ping("8.8.8.8", 3)) {
    speedTest.pingGoogle = Ping.averageTime();
    Serial.printf("OK (%d ms)\n", speedTest.pingGoogle);
    speedTest.internetOK = true;
  } else {
    speedTest.pingGoogle = -1;
    Serial.println("FAILED");
    speedTest.internetOK = false;
  }
  
  // Ping Cloudflare
  Serial.print("Pinging 1.1.1.1... ");
  if (Ping.ping("1.1.1.1", 3)) {
    speedTest.pingCloudflare = Ping.averageTime();
    Serial.printf("OK (%d ms)\n", speedTest.pingCloudflare);
  } else {
    speedTest.pingCloudflare = -1;
    Serial.println("FAILED");
  }
  
  // Ping Gateway (router)
  IPAddress gateway = WiFi.gatewayIP();
  Serial.print("Pinging Gateway... ");
  if (Ping.ping(gateway, 3)) {
    speedTest.pingLocal = Ping.averageTime();
    Serial.printf("OK (%d ms)\n", speedTest.pingLocal);
  } else {
    speedTest.pingLocal = -1;
    Serial.println("FAILED");
  }
  
  lastSpeedTest = millis();
}

// ===== FUNGSI SCAN NETWORK DEVICES =====
void scanDevices() {
  Serial.println("\n=== Scanning Network Devices ===");
  
  IPAddress localIP = WiFi.localIP();
  IPAddress subnet = WiFi.subnetMask();
  IPAddress gateway = WiFi.gatewayIP();
  
  // Hitung range IP
  uint32_t startIP = (uint32_t)localIP & (uint32_t)subnet;
  uint32_t endIP = startIP | ~((uint32_t)subnet);
  
  deviceCount = 0;
  
  // Scan IP range (skip network address dan broadcast)
  for (uint32_t ip = startIP + 1; ip < endIP && deviceCount < 50; ip++) {
    IPAddress testIP = IPAddress(ip);
    
    // Skip IP sendiri
    if (testIP == localIP) continue;
    
    // Ping dengan timeout singkat
    if (Ping.ping(testIP, 1)) {
      devices[deviceCount].ip = testIP;
      devices[deviceCount].active = true;
      devices[deviceCount].pingTime = Ping.averageTime();
      
      // Coba dapatkan MAC address (hanya bisa untuk device di subnet yang sama)
      // Note: ESP8266 tidak bisa langsung baca ARP table, jadi ini simplified
      devices[deviceCount].mac = "N/A";
      
      // Cek apakah ini gateway
      if (testIP == gateway) {
        devices[deviceCount].hostname = "Gateway/Router";
      } else {
        devices[deviceCount].hostname = "Device";
      }
      
      Serial.printf("Found: %s (Ping: %dms)\n", 
                    testIP.toString().c_str(), 
                    devices[deviceCount].pingTime);
      
      deviceCount++;
    }
    
    // Yield untuk menghindari watchdog reset
    yield();
  }
  
  Serial.printf("Total devices found: %d\n", deviceCount);
}

// ===== FUNGSI GET UPTIME =====
String getUptime() {
  unsigned long uptime = millis() / 1000;
  int days = uptime / 86400;
  int hours = (uptime % 86400) / 3600;
  int minutes = (uptime % 3600) / 60;
  int seconds = uptime % 60;
  
  String result = "";
  if (days > 0) result += String(days) + "d ";
  if (hours > 0) result += String(hours) + "h ";
  if (minutes > 0) result += String(minutes) + "m ";
  result += String(seconds) + "s";
  
  return result;
}

// ===== FUNGSI GET SIGNAL STRENGTH =====
String getSignalQuality() {
  int rssi = WiFi.RSSI();
  if (rssi >= -50) return "Excellent";
  else if (rssi >= -60) return "Good";
  else if (rssi >= -70) return "Fair";
  else return "Weak";
}

// ===== HALAMAN WEB =====
void handleRoot() {
  String html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>Network Tools - ESP8266</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 20px;
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
    }
    .header {
      background: white;
      border-radius: 15px;
      padding: 25px;
      margin-bottom: 20px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.2);
      text-align: center;
    }
    h1 {
      color: #333;
      font-size: 28px;
      margin-bottom: 10px;
    }
    .subtitle {
      color: #666;
      font-size: 14px;
    }
    .stats-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 15px;
      margin-bottom: 20px;
    }
    .stat-card {
      background: white;
      border-radius: 15px;
      padding: 20px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.2);
      text-align: center;
    }
    .stat-icon {
      font-size: 36px;
      margin-bottom: 10px;
    }
    .stat-label {
      font-size: 12px;
      color: #666;
      margin-bottom: 5px;
    }
    .stat-value {
      font-size: 24px;
      font-weight: bold;
      color: #333;
    }
    .stat-sub {
      font-size: 11px;
      color: #999;
      margin-top: 5px;
    }
    .section {
      background: white;
      border-radius: 15px;
      padding: 25px;
      margin-bottom: 20px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.2);
    }
    .section-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 20px;
      padding-bottom: 15px;
      border-bottom: 2px solid #f0f0f0;
    }
    .section-title {
      font-size: 20px;
      font-weight: bold;
      color: #333;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .btn {
      padding: 10px 20px;
      border: none;
      border-radius: 8px;
      font-size: 14px;
      cursor: pointer;
      transition: all 0.2s;
      font-weight: bold;
      color: white;
    }
    .btn-primary {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    }
    .btn-success {
      background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%);
    }
    .btn:hover {
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(0,0,0,0.2);
    }
    .btn:active {
      transform: translateY(0);
    }
    .btn:disabled {
      opacity: 0.5;
      cursor: not-allowed;
      transform: none;
    }
    .ping-results {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
      gap: 15px;
    }
    .ping-item {
      background: #f8f9fa;
      padding: 15px;
      border-radius: 10px;
      text-align: center;
    }
    .ping-label {
      font-size: 12px;
      color: #666;
      margin-bottom: 5px;
    }
    .ping-value {
      font-size: 32px;
      font-weight: bold;
      margin: 5px 0;
    }
    .ping-value.good { color: #4CAF50; }
    .ping-value.fair { color: #FFC107; }
    .ping-value.bad { color: #F44336; }
    .ping-unit {
      font-size: 12px;
      color: #999;
    }
    .device-table {
      width: 100%;
      border-collapse: collapse;
      margin-top: 15px;
    }
    .device-table th {
      background: #f8f9fa;
      padding: 12px;
      text-align: left;
      font-size: 12px;
      color: #666;
      font-weight: bold;
      border-bottom: 2px solid #e0e0e0;
    }
    .device-table td {
      padding: 15px 12px;
      border-bottom: 1px solid #f0f0f0;
    }
    .device-table tr:hover {
      background: #f8f9fa;
    }
    .device-ip {
      font-weight: bold;
      color: #333;
      font-size: 15px;
    }
    .device-name {
      color: #666;
      font-size: 13px;
    }
    .device-mac {
      font-family: 'Courier New', monospace;
      font-size: 12px;
      color: #999;
    }
    .status-badge {
      display: inline-block;
      padding: 4px 10px;
      border-radius: 20px;
      font-size: 11px;
      font-weight: bold;
    }
    .status-online {
      background: #e8f5e9;
      color: #2e7d32;
    }
    .status-offline {
      background: #ffebee;
      color: #c62828;
    }
    .loading {
      text-align: center;
      padding: 40px;
      color: #666;
    }
    .spinner {
      border: 4px solid #f3f3f3;
      border-top: 4px solid #667eea;
      border-radius: 50%;
      width: 40px;
      height: 40px;
      animation: spin 1s linear infinite;
      margin: 0 auto 15px;
    }
    @keyframes spin {
      0% { transform: rotate(0deg); }
      100% { transform: rotate(360deg); }
    }
    .empty {
      text-align: center;
      padding: 40px;
      color: #999;
    }
    .ping-chart {
      display: flex;
      gap: 10px;
      align-items: flex-end;
      height: 100px;
      margin-top: 15px;
      padding: 10px;
      background: #f8f9fa;
      border-radius: 8px;
    }
    .ping-bar {
      flex: 1;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      border-radius: 4px 4px 0 0;
      min-height: 5px;
      position: relative;
    }
    .ping-bar-label {
      position: absolute;
      top: -20px;
      left: 50%;
      transform: translateX(-50%);
      font-size: 10px;
      font-weight: bold;
      white-space: nowrap;
    }
    @media (max-width: 768px) {
      .stats-grid {
        grid-template-columns: repeat(2, 1fr);
      }
      .device-table {
        font-size: 12px;
      }
      .device-table th, .device-table td {
        padding: 8px;
      }
    }
  </style>
</head>
<body>
  <div class='container'>
    <div class='header'>
      <h1>🛠️ Network Tools</h1>
      <div class='subtitle'>ESP8266 Speed Tester & Device Finder</div>
    </div>
    
    <div class='stats-grid'>
      <div class='stat-card'>
        <div class='stat-icon'>📡</div>
        <div class='stat-label'>WiFi Signal</div>
        <div class='stat-value'>)";
  
  html += String(WiFi.RSSI());
  html += R"( dBm</div>
        <div class='stat-sub'>)";
  html += getSignalQuality();
  html += R"(</div>
      </div>
      
      <div class='stat-card'>
        <div class='stat-icon'>🌐</div>
        <div class='stat-label'>Internet Status</div>
        <div class='stat-value' style='color: )";
  html += speedTest.internetOK ? "#4CAF50" : "#F44336";
  html += "'>";
  html += speedTest.internetOK ? "Online" : "Offline";
  html += R"(</div>
        <div class='stat-sub'>)";
  html += WiFi.localIP().toString();
  html += R"(</div>
      </div>
      
      <div class='stat-card'>
        <div class='stat-icon'>⏱️</div>
        <div class='stat-label'>Uptime</div>
        <div class='stat-value' style='font-size: 18px;'>)";
  html += getUptime();
  html += R"(</div>
        <div class='stat-sub'>Since boot</div>
      </div>
      
      <div class='stat-card'>
        <div class='stat-icon'>🔍</div>
        <div class='stat-label'>Devices Found</div>
        <div class='stat-value'>)";
  html += String(deviceCount);
  html += R"(</div>
        <div class='stat-sub'>On network</div>
      </div>
    </div>
    
    <!-- SPEED TEST SECTION -->
    <div class='section'>
      <div class='section-header'>
        <div class='section-title'>
          <span>⚡</span>
          <span>Speed Test</span>
        </div>
        <button class='btn btn-primary' onclick='runSpeedTest()' id='speedBtn'>
          🔄 Run Test
        </button>
      </div>
      
      <div class='ping-results'>
        <div class='ping-item'>
          <div class='ping-label'>Google DNS</div>
          <div class='ping-value )";
  
  String googleClass = "good";
  if (speedTest.pingGoogle > 100) googleClass = "fair";
  if (speedTest.pingGoogle > 200 || speedTest.pingGoogle == -1) googleClass = "bad";
  html += googleClass;
  html += "'>";
  html += speedTest.pingGoogle == -1 ? "N/A" : String(speedTest.pingGoogle);
  html += R"(</div>
          <div class='ping-unit'>)";
  html += speedTest.pingGoogle == -1 ? "Failed" : "ms";
  html += R"(</div>
        </div>
        
        <div class='ping-item'>
          <div class='ping-label'>Cloudflare</div>
          <div class='ping-value )";
  
  String cfClass = "good";
  if (speedTest.pingCloudflare > 100) cfClass = "fair";
  if (speedTest.pingCloudflare > 200 || speedTest.pingCloudflare == -1) cfClass = "bad";
  html += cfClass;
  html += "'>";
  html += speedTest.pingCloudflare == -1 ? "N/A" : String(speedTest.pingCloudflare);
  html += R"(</div>
          <div class='ping-unit'>)";
  html += speedTest.pingCloudflare == -1 ? "Failed" : "ms";
  html += R"(</div>
        </div>
        
        <div class='ping-item'>
          <div class='ping-label'>Gateway</div>
          <div class='ping-value )";
  
  String localClass = "good";
  if (speedTest.pingLocal > 20) localClass = "fair";
  if (speedTest.pingLocal > 50 || speedTest.pingLocal == -1) localClass = "bad";
  html += localClass;
  html += "'>";
  html += speedTest.pingLocal == -1 ? "N/A" : String(speedTest.pingLocal);
  html += R"(</div>
          <div class='ping-unit'>)";
  html += speedTest.pingLocal == -1 ? "Failed" : "ms";
  html += R"(</div>
        </div>
      </div>
      
      <div class='ping-chart'>
        <div class='ping-bar' style='height: )";
  int googleHeight = speedTest.pingGoogle > 0 ? min(speedTest.pingGoogle / 2, 100) : 5;
  html += String(googleHeight);
  html += R"(%;'>
          <div class='ping-bar-label'>Google</div>
        </div>
        <div class='ping-bar' style='height: )";
  int cfHeight = speedTest.pingCloudflare > 0 ? min(speedTest.pingCloudflare / 2, 100) : 5;
  html += String(cfHeight);
  html += R"(%;'>
          <div class='ping-bar-label'>CF</div>
        </div>
        <div class='ping-bar' style='height: )";
  int localHeight = speedTest.pingLocal > 0 ? min(speedTest.pingLocal * 2, 100) : 5;
  html += String(localHeight);
  html += R"(%;'>
          <div class='ping-bar-label'>Gateway</div>
        </div>
      </div>
    </div>
    
    <!-- DEVICE FINDER SECTION -->
    <div class='section'>
      <div class='section-header'>
        <div class='section-title'>
          <span>📱</span>
          <span>Device Finder</span>
        </div>
        <button class='btn btn-success' onclick='scanDevices()' id='scanBtn'>
          🔍 Scan Network
        </button>
      </div>
      
      <div id='deviceList'>)";

  if (deviceCount == 0) {
    html += R"(
        <div class='empty'>
          <div style='font-size: 48px; margin-bottom: 10px;'>📱</div>
          <div>No devices scanned yet. Click "Scan Network" to start.</div>
        </div>
    )";
  } else {
    html += R"(
        <table class='device-table'>
          <thead>
            <tr>
              <th>Status</th>
              <th>IP Address</th>
              <th>Device Name</th>
              <th>Ping</th>
            </tr>
          </thead>
          <tbody>
    )";
    
    for (int i = 0; i < deviceCount; i++) {
      html += "<tr>";
      html += "<td><span class='status-badge status-online'>● Online</span></td>";
      html += "<td><div class='device-ip'>" + devices[i].ip.toString() + "</div></td>";
      html += "<td><div class='device-name'>" + devices[i].hostname + "</div></td>";
      html += "<td>" + String(devices[i].pingTime) + " ms</td>";
      html += "</tr>";
    }
    
    html += R"(
          </tbody>
        </table>
    )";
  }

  html += R"(
      </div>
    </div>
  </div>
  
  <script>
    function runSpeedTest() {
      const btn = document.getElementById('speedBtn');
      btn.disabled = true;
      btn.textContent = '⏳ Testing...';
      
      fetch('/speedtest')
        .then(response => response.text())
        .then(data => {
          setTimeout(() => location.reload(), 2000);
        })
        .catch(error => {
          alert('Error: ' + error);
          btn.disabled = false;
          btn.textContent = '🔄 Run Test';
        });
    }
    
    function scanDevices() {
      const btn = document.getElementById('scanBtn');
      const list = document.getElementById('deviceList');
      
      btn.disabled = true;
      btn.textContent = '⏳ Scanning...';
      
      list.innerHTML = `
        <div class='loading'>
          <div class='spinner'></div>
          <div>Scanning network... This may take 1-2 minutes</div>
        </div>
      `;
      
      fetch('/scan')
        .then(response => response.text())
        .then(data => {
          setTimeout(() => location.reload(), 3000);
        })
        .catch(error => {
          alert('Error: ' + error);
          btn.disabled = false;
          btn.textContent = '🔍 Scan Network';
        });
    }
    
    // Auto refresh every 5 minutes
    setTimeout(() => location.reload(), 300000);
  </script>
</body>
</html>
  )";
  
  server.send(200, "text/html", html);
}

// ===== ENDPOINT SPEED TEST =====
void handleSpeedTest() {
  runPingTest();
  server.send(200, "text/plain", "Speed test complete!");
}

// ===== ENDPOINT DEVICE SCAN =====
void handleScan() {
  scanDevices();
  server.send(200, "text/plain", "Device scan complete!");
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n========================================");
  Serial.println("ESP8266 Network Tools");
  Serial.println("Speed Tester + Device Finder");
  Serial.println("========================================");
  
  // Koneksi WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.print("\nConnecting to WiFi");
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500);
    Serial.print(".");
    timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n\n✓ WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("Subnet: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("Signal: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    // Setup web server
    server.on("/", handleRoot);
    server.on("/speedtest", handleSpeedTest);
    server.on("/scan", handleScan);
    server.begin();
    
    Serial.println("\n✓ Web server started!");
    Serial.println("========================================");
    Serial.print("Open browser: http://");
    Serial.println(WiFi.localIP());
    Serial.println("========================================\n");
    
    bootTime = millis();
    
    // Run initial tests
    delay(2000);
    Serial.println("Running initial speed test...");
    runPingTest();
    
  } else {
    Serial.println("\n\n✗ WiFi connection failed!");
    Serial.println("Please check SSID and password");
  }
}

// ===== LOOP =====
void loop() {
  server.handleClient();
  
  // Auto speed test setiap 5 menit
  if (millis() - lastSpeedTest > 300000) {
    runPingTest();
  }
}
