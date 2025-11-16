#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ==================== PIN CONFIGURATION ====================
#define SS_PIN 5
#define RST_PIN 4

// ==================== OBJECTS ====================
MFRC522 rfid(SS_PIN, RST_PIN);
#define CAM_SERIAL Serial2  // Serial connection to ESP32-CAM

// ==================== NETWORK CONFIG ====================
const char* ssid     = "realme C33";
const char* password = "itsnafinabi";
String serverURL = "http://your_local_ip:8000/api/uid_scan/";  // CHANGE TO YOUR PC IP!

// ==================== VARIABLES ====================
unsigned long lastScanTime = 0;
const unsigned long SCAN_DELAY = 3000; // 3 seconds between scans
bool wifiConnected = false;

// ==================== FUNCTION PROTOTYPES ====================
bool connectWiFi();
bool verifyUIDWithServer(String uid);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("    RFID Attendance System - RFID Board");
  Serial.println("========================================\n");

  // -------- Initialize Serial to ESP32-CAM --------
  CAM_SERIAL.begin(115200, SERIAL_8N1, 16, 17); // RX=16, TX=17
  Serial.println("✓ Serial2 initialized (ESP32-CAM communication)");

  // -------- Initialize SPI & RFID --------
  SPI.begin();
  rfid.PCD_Init();
  delay(100);
  
  // Check RFID module
  byte version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print("✓ RFID Module initialized. Firmware: 0x");
  Serial.println(version, HEX);
  
  if (version == 0x00 || version == 0xFF) {
    Serial.println("✗ WARNING: RFID module not detected! Check wiring:");
    Serial.println("  SDA/SS  -> GPIO 5");
    Serial.println("  SCK     -> GPIO 18");
    Serial.println("  MOSI    -> GPIO 23");
    Serial.println("  MISO    -> GPIO 19");
    Serial.println("  RST     -> GPIO 4");
    Serial.println("  3.3V    -> 3.3V");
    Serial.println("  GND     -> GND");
  }

  // -------- Connect to WiFi --------
  wifiConnected = connectWiFi();

  Serial.println("\n========================================");
  if (wifiConnected) {
    Serial.println("✓ System Ready! Waiting for RFID card...");
  } else {
    Serial.println("⚠ System Ready (WiFi not connected)");
    Serial.println("  Will retry WiFi connection in background");
  }
  Serial.println("========================================\n");
}

void loop() {
  // -------- Check WiFi Connection (every 10 seconds if not connected) --------
  static unsigned long lastWiFiCheck = 0;
  if (!wifiConnected && (millis() - lastWiFiCheck > 10000)) {
    lastWiFiCheck = millis();
    Serial.println("⏳ Attempting WiFi reconnection...");
    wifiConnected = connectWiFi();
  }

  // -------- Prevent too frequent scans --------
  if (millis() - lastScanTime < SCAN_DELAY) {
    return;
  }

  // -------- Check for new RFID card --------
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  // -------- Read UID --------
  String uidString = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) {
      uidString += "0";  // Add leading zero for single digit hex
    }
    uidString += String(rfid.uid.uidByte[i], HEX);
  }
  uidString.toUpperCase();

  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║         RFID CARD DETECTED!            ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.print("  UID: ");
  Serial.println(uidString);
  Serial.print("  UID Size: ");
  Serial.print(rfid.uid.size);
  Serial.println(" bytes");

  // -------- Send UID to ESP32-CAM via Serial --------
  Serial.println("\n→ Sending UID to ESP32-CAM via Serial...");
  CAM_SERIAL.println(uidString);
  Serial.println("  ✓ Sent to ESP32-CAM");

  // -------- Verify UID with Django Server (if WiFi connected) --------
  if (wifiConnected) {
    Serial.println("\n→ Verifying UID with Django server...");
    if (verifyUIDWithServer(uidString)) {
      Serial.println("  ✓ UID verified on server - User exists!");
      Serial.println("  ℹ ESP32-CAM will now capture image...");
    } else {
      Serial.println("  ✗ UID not found on server!");
      Serial.println("  ⚠ ESP32-CAM may still try to capture, but will fail at checkin.");
    }
  } else {
    Serial.println("\n⚠ WiFi not connected - skipping server verification");
    Serial.println("  ESP32-CAM will still receive UID and attempt capture");
  }

  // -------- Halt RFID card --------
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  
  lastScanTime = millis();
  
  Serial.println("\n========================================");
  Serial.println("Waiting for next card...");
  Serial.println("========================================\n");
}

// ==================== FUNCTION: Connect to WiFi ====================
bool connectWiFi() {
  Serial.println("\n⏳ Connecting to WiFi...");
  Serial.print("  SSID: ");
  Serial.println(ssid);
  Serial.print("  Password: ");
  Serial.println(password);
  
  // CRITICAL FIX: Completely disconnect and reset WiFi
  WiFi.disconnect(true);  // Disconnect and turn off WiFi
  delay(500);
  
  WiFi.mode(WIFI_OFF);    // Turn off WiFi completely
  delay(500);
  
  WiFi.mode(WIFI_STA);    // Set to station mode
  delay(500);
  
  // Now begin connection
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    // Print status every 10 attempts
    if (attempts % 10 == 0) {
      Serial.print("\n  Status: ");
      Serial.print(WiFi.status());
      Serial.print(" | Attempt: ");
      Serial.print(attempts);
      Serial.print("/40 ");
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi Connected!");
    Serial.print("  IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("  Signal Strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.print("  Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("  Server URL: ");
    Serial.println(serverURL);
    return true;
  } else {
    Serial.println("\n✗ WiFi Connection Failed!");
    Serial.print("  Final Status Code: ");
    Serial.println(WiFi.status());
    Serial.println("\n  Status Codes:");
    Serial.println("    0 = WL_IDLE_STATUS");
    Serial.println("    1 = WL_NO_SSID_AVAIL (Network not found)");
    Serial.println("    3 = WL_CONNECTED");
    Serial.println("    4 = WL_CONNECT_FAILED (Wrong password)");
    Serial.println("    6 = WL_DISCONNECTED");
    Serial.println("\n  Troubleshooting:");
    Serial.println("    - Check SSID spelling (case-sensitive)");
    Serial.println("    - Check password (no extra spaces)");
    Serial.println("    - Move ESP32 closer to router");
    Serial.println("    - Check if WiFi is 2.4GHz (not 5GHz)");
    return false;
  }
}

// ==================== FUNCTION: Verify UID with Server ====================
bool verifyUIDWithServer(String uid) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("  ✗ WiFi not connected");
    wifiConnected = false;
    return false;
  }

  HTTPClient http;
  WiFiClient client;
  
  http.begin(client, serverURL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);  // 5 second timeout
  
  String jsonPayload = "{\"rfid_uid\":\"" + uid + "\"}";
  Serial.print("  Payload: ");
  Serial.println(jsonPayload);
  
  int httpCode = http.POST(jsonPayload);
  
  Serial.print("  Response Code: ");
  Serial.println(httpCode);
  
  if (httpCode > 0) {
    String response = http.getString();
    Serial.print("  Response: ");
    Serial.println(response);
    
    http.end();
    
    // Check if response contains "ok" status
    if (response.indexOf("\"status\":\"ok\"") > 0 || 
        response.indexOf("\"status\": \"ok\"") > 0) {
      return true;
    }
  } else {
    Serial.print("  ✗ HTTP Error: ");
    Serial.println(http.errorToString(httpCode));
    http.end();
  }
  
  return false;
}