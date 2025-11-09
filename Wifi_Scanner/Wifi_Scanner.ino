#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ===== KONFIGURASI ACCESS POINT =====
// ESP akan jadi hotspot sendiri, jadi gak perlu koneksi WiFi rumah
const char* ap_ssid = "ESP8266-Scanner";
const char* ap_password = "12345678"; // Min 8 karakter

// ===== SERVER =====
ESP8266WebServer server(80);

// ===== DATA SCAN =====
struct WiFiNetwork {
  String ssid;
  int rssi;
  int channel;
  String encryption;
  bool isHidden;
};

WiFiNetwork networks[50]; // Max 50 network
int networkCount = 0;
unsigned long lastScan = 0;
bool isScanning = false;

// ===== FUNGSI SCAN WIFI =====
void scanWiFi() {
  isScanning = true;
  Serial.println("Memulai scan WiFi...");
  
  int n = WiFi.scanNetworks();
  networkCount = (n > 50) ? 50 : n; // Batasi max 50
  
  if (n == 0) {
    Serial.println("Tidak ada network ditemukan!");
  } else {
    Serial.printf("Ditemukan %d network\n", n);
    
    // Simpan data ke array
    for (int i = 0; i < networkCount; i++) {
      networks[i].ssid = WiFi.SSID(i);
      networks[i].rssi = WiFi.RSSI(i);
      networks[i].channel = WiFi.channel(i);
      networks[i].isHidden = (WiFi.isHidden(i) == 1);
      
      // Tipe enkripsi
      switch (WiFi.encryptionType(i)) {
        case ENC_TYPE_NONE:
          networks[i].encryption = "Open";
          break;
        case ENC_TYPE_WEP:
          networks[i].encryption = "WEP";
          break;
        case ENC_TYPE_TKIP:
          networks[i].encryption = "WPA";
          break;
        case ENC_TYPE_CCMP:
          networks[i].encryption = "WPA2";
          break;
        case ENC_TYPE_AUTO:
          networks[i].encryption = "WPA/WPA2";
          break;
        default:
          networks[i].encryption = "Unknown";
      }
      
      Serial.printf("%d: %s (%d dBm) Ch:%d %s\n", 
                    i+1, 
                    networks[i].ssid.c_str(), 
                    networks[i].rssi, 
                    networks[i].channel,
                    networks[i].encryption.c_str());
    }
  }
  
  lastScan = millis();
  isScanning = false;
  WiFi.scanDelete(); // Bersihkan memory
}

// ===== FUNGSI SIGNAL STRENGTH =====
String getSignalStrength(int rssi) {
  if (rssi >= -50) return "Excellent";
  else if (rssi >= -60) return "Good";
  else if (rssi >= -70) return "Fair";
  else return "Weak";
}

// ===== FUNGSI SIGNAL COLOR =====
String getSignalColor(int rssi) {
  if (rssi >= -50) return "#4CAF50";      // Hijau
  else if (rssi >= -60) return "#8BC34A"; // Hijau muda
  else if (rssi >= -70) return "#FFC107"; // Kuning
  else return "#F44336";                  // Merah
}

// ===== FUNGSI SIGNAL BARS =====
int getSignalBars(int rssi) {
  if (rssi >= -50) return 4;
  else if (rssi >= -60) return 3;
  else if (rssi >= -70) return 2;
  else return 1;
}

// ===== HALAMAN WEB =====
void handleRoot() {
  String html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>ESP8266 WiFi Scanner</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 20px;
    }
    .container {
      max-width: 800px;
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
    .stats {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
      gap: 15px;
      margin-top: 20px;
    }
    .stat-box {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 15px;
      border-radius: 10px;
      text-align: center;
    }
    .stat-value {
      font-size: 32px;
      font-weight: bold;
    }
    .stat-label {
      font-size: 12px;
      opacity: 0.9;
      margin-top: 5px;
    }
    .controls {
      display: flex;
      gap: 10px;
      margin-bottom: 20px;
    }
    .button {
      flex: 1;
      padding: 15px;
      border: none;
      border-radius: 10px;
      font-size: 16px;
      cursor: pointer;
      transition: transform 0.2s;
      color: white;
      font-weight: bold;
    }
    .btn-scan {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    }
    .btn-auto {
      background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%);
    }
    .button:hover {
      transform: translateY(-2px);
    }
    .button:active {
      transform: translateY(0);
    }
    .button:disabled {
      opacity: 0.5;
      cursor: not-allowed;
    }
    .network-list {
      background: white;
      border-radius: 15px;
      overflow: hidden;
      box-shadow: 0 10px 30px rgba(0,0,0,0.2);
    }
    .network-item {
      padding: 20px;
      border-bottom: 1px solid #eee;
      display: grid;
      grid-template-columns: 1fr auto;
      gap: 15px;
      align-items: center;
      transition: background 0.2s;
    }
    .network-item:hover {
      background: #f8f9fa;
    }
    .network-item:last-child {
      border-bottom: none;
    }
    .network-info {
      display: flex;
      flex-direction: column;
      gap: 5px;
    }
    .network-name {
      font-size: 18px;
      font-weight: bold;
      color: #333;
    }
    .network-details {
      font-size: 12px;
      color: #666;
      display: flex;
      gap: 15px;
      flex-wrap: wrap;
    }
    .detail-item {
      display: flex;
      align-items: center;
      gap: 5px;
    }
    .network-signal {
      text-align: right;
    }
    .signal-strength {
      font-size: 24px;
      font-weight: bold;
      margin-bottom: 5px;
    }
    .signal-label {
      font-size: 11px;
      color: #666;
    }
    .signal-bars {
      display: flex;
      gap: 3px;
      justify-content: flex-end;
      margin-top: 5px;
    }
    .bar {
      width: 8px;
      height: 20px;
      background: #ddd;
      border-radius: 2px;
    }
    .bar.active {
      background: currentColor;
    }
    .badge {
      display: inline-block;
      padding: 3px 8px;
      border-radius: 5px;
      font-size: 11px;
      font-weight: bold;
      background: #e0e0e0;
      color: #666;
    }
    .badge.open { background: #ffebee; color: #c62828; }
    .badge.wpa { background: #e8f5e9; color: #2e7d32; }
    .badge.hidden { background: #fff3e0; color: #e65100; }
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
    @media (max-width: 600px) {
      .network-item {
        grid-template-columns: 1fr;
      }
      .network-signal {
        text-align: left;
      }
      .signal-bars {
        justify-content: flex-start;
      }
    }
  </style>
</head>
<body>
  <div class='container'>
    <div class='header'>
      <h1>📡 WiFi Scanner</h1>
      <div class='stats'>
        <div class='stat-box'>
          <div class='stat-value' id='count'>)";
  
  html += String(networkCount);
  html += R"(</div>
          <div class='stat-label'>Networks Found</div>
        </div>
        <div class='stat-box'>
          <div class='stat-value' id='channels'>)";
  
  // Hitung channel yang digunakan
  int channelUsed[14] = {0};
  for (int i = 0; i < networkCount; i++) {
    if (networks[i].channel > 0 && networks[i].channel < 14) {
      channelUsed[networks[i].channel] = 1;
    }
  }
  int totalChannels = 0;
  for (int i = 0; i < 14; i++) totalChannels += channelUsed[i];
  
  html += String(totalChannels);
  html += R"(</div>
          <div class='stat-label'>Channels Used</div>
        </div>
        <div class='stat-box'>
          <div class='stat-value' id='time'>)";
  
  html += String((millis() - lastScan) / 1000);
  html += R"(s</div>
          <div class='stat-label'>Last Scan</div>
        </div>
      </div>
    </div>
    
    <div class='controls'>
      <button class='button btn-scan' onclick='scanNow()' id='scanBtn'>
        🔍 Scan Now
      </button>
      <button class='button btn-auto' onclick='toggleAuto()' id='autoBtn'>
        🔄 Auto Scan: OFF
      </button>
    </div>
    
    <div class='network-list' id='networkList'>)";

  if (isScanning) {
    html += R"(
      <div class='loading'>
        <div class='spinner'></div>
        <div>Scanning WiFi networks...</div>
      </div>
    )";
  } else if (networkCount == 0) {
    html += R"(
      <div class='empty'>
        <div style='font-size: 48px; margin-bottom: 10px;'>📡</div>
        <div>No networks found. Click "Scan Now" to start.</div>
      </div>
    )";
  } else {
    // Tampilkan networks
    for (int i = 0; i < networkCount; i++) {
      String color = getSignalColor(networks[i].rssi);
      int bars = getSignalBars(networks[i].rssi);
      
      html += "<div class='network-item'>";
      html += "<div class='network-info'>";
      html += "<div class='network-name'>";
      
      if (networks[i].isHidden) {
        html += "(Hidden Network)";
      } else {
        html += networks[i].ssid;
      }
      
      html += "</div>";
      html += "<div class='network-details'>";
      html += "<div class='detail-item'>📶 Channel " + String(networks[i].channel) + "</div>";
      html += "<div class='detail-item'>🔒 " + networks[i].encryption + "</div>";
      html += "<div class='detail-item'>" + String(networks[i].rssi) + " dBm</div>";
      html += "</div></div>";
      
      html += "<div class='network-signal' style='color: " + color + "'>";
      html += "<div class='signal-strength'>" + String(networks[i].rssi) + " dBm</div>";
      html += "<div class='signal-label'>" + getSignalStrength(networks[i].rssi) + "</div>";
      html += "<div class='signal-bars'>";
      
      for (int b = 1; b <= 4; b++) {
        html += "<div class='bar";
        if (b <= bars) html += " active";
        html += "'></div>";
      }
      
      html += "</div></div></div>";
    }
  }

  html += R"(
    </div>
  </div>
  
  <script>
    let autoScan = false;
    let autoInterval;
    
    function scanNow() {
      document.getElementById('scanBtn').disabled = true;
      document.getElementById('scanBtn').textContent = '⏳ Scanning...';
      
      fetch('/scan')
        .then(() => {
          setTimeout(() => location.reload(), 2000);
        })
        .catch(err => {
          alert('Error: ' + err);
          document.getElementById('scanBtn').disabled = false;
          document.getElementById('scanBtn').textContent = '🔍 Scan Now';
        });
    }
    
    function toggleAuto() {
      autoScan = !autoScan;
      const btn = document.getElementById('autoBtn');
      
      if (autoScan) {
        btn.textContent = '🔄 Auto Scan: ON';
        btn.style.background = 'linear-gradient(135deg, #f093fb 0%, #f5576c 100%)';
        autoInterval = setInterval(() => location.reload(), 30000); // 30 detik
      } else {
        btn.textContent = '🔄 Auto Scan: OFF';
        btn.style.background = 'linear-gradient(135deg, #11998e 0%, #38ef7d 100%)';
        clearInterval(autoInterval);
      }
    }
  </script>
</body>
</html>
  )";
  
  server.send(200, "text/html", html);
}

// ===== ENDPOINT SCAN =====
void handleScan() {
  scanWiFi();
  server.send(200, "text/plain", "Scan complete!");
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n==============================");
  Serial.println("ESP8266 WiFi Scanner");
  Serial.println("==============================");
  
  // Set mode WiFi
  WiFi.mode(WIFI_AP_STA);
  
  // Buat Access Point
  Serial.println("\nMemulai Access Point...");
  WiFi.softAP(ap_ssid, ap_password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  Serial.println("\n--- CARA PAKAI ---");
  Serial.println("1. Hubungkan ke WiFi: " + String(ap_ssid));
  Serial.println("2. Password: " + String(ap_password));
  Serial.println("3. Buka browser: http://" + IP.toString());
  Serial.println("==================\n");
  
  // Setup web server
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.begin();
  
  Serial.println("Web server started!");
  
  // Scan pertama kali
  delay(1000);
  scanWiFi();
}

// ===== LOOP =====
void loop() {
  server.handleClient();
}
