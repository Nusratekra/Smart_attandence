#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "base64.h"

#define RFID_SERIAL Serial2  // Serial connection for RFID UID

String lastRFID = "";

const char* ssid = "your wifi name";
const char* password = "wifi pass";
String serverURL = "http://your_local_ip:8000/api/checkin/";  // FIXED: Removed space!

// ---- Camera Pin Config ----
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void startCamera() {
  Serial.println("Initializing camera...");
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // IMPROVED: Better resolution for face recognition
  config.frame_size = FRAMESIZE_VGA;  // 640x480 (was QVGA 320x240)
  config.jpeg_quality = 12;  // Better quality (was 14)
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err == ESP_OK) {
    Serial.println("Camera OK!");
  } else {
    Serial.println("Camera FAILED!");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========================================");
  Serial.println("ESP32-CAM Attendance System");
  Serial.println("========================================\n");
  
  // Initialize Serial for RFID communication
  RFID_SERIAL.begin(115200, SERIAL_8N1, 13, 14); // RX=13, TX=14
  Serial.println("Serial2 initialized");

  // Connect to WiFi
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Server: ");
    Serial.println(serverURL);
  } else {
    Serial.println("\nWiFi FAILED!");
  }

  // Initialize camera
  startCamera();
  
  Serial.println("\n========================================");
  Serial.println("System Ready! Waiting for RFID UID...");
  Serial.println("========================================\n");
}

void loop() {
  // Check WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Reconnecting...");
    WiFi.begin(ssid, password);
    delay(5000);
    return;
  }

  // Read UID from RFID board
  if (RFID_SERIAL.available()) {
    lastRFID = RFID_SERIAL.readStringUntil('\n');
    lastRFID.trim();
    Serial.println("\n>>> Received UID: " + lastRFID);
  }

  if (lastRFID == "") return;  // No RFID yet

  // Wait 2 seconds for person to look at camera
  Serial.println("Preparing to capture in 2 seconds...");
  delay(2000);

  // Capture image
  Serial.println("Capturing image...");
  camera_fb_t* fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("Camera capture FAILED!");
    lastRFID = "";
    return;
  }

  Serial.print("Image captured: ");
  Serial.print(fb->width);
  Serial.print("x");
  Serial.print(fb->height);
  Serial.print(" (");
  Serial.print(fb->len);
  Serial.println(" bytes)");

  // Encode to Base64
  Serial.println("Encoding to Base64...");
  String imgB64 = base64::encode(fb->buf, fb->len);
  esp_camera_fb_return(fb);
  
  Serial.print("Base64 size: ");
  Serial.println(imgB64.length());

  // Prepare JSON
  String jsonData = "{\"rfid_uid\":\"" + lastRFID + "\",\"image_base64\":\"" + imgB64 + "\"}";

  // Send to Django
  Serial.println("Sending to Django...");
  Serial.println("This may take 10-20 seconds...");
  
  HTTPClient http;
  WiFiClient client;
  
  http.begin(client, serverURL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(30000);  // 30 second timeout

  int code = http.POST(jsonData);
  
  Serial.print("Response code: ");
  Serial.println(code);
  
  if (code > 0) {
    String response = http.getString();
    Serial.println("Django response:");
    Serial.println(response);
    
    // Check if successful
    if (response.indexOf("success") >= 0) {
      Serial.println(">>> SUCCESS! Attendance recorded!");
    } else {
      Serial.println(">>> FAILED! Check response above.");
    }
  } else {
    Serial.print("HTTP Error: ");
    Serial.println(http.errorToString(code));
  }

  http.end();
  
  lastRFID = "";
  
  Serial.println("\n========================================");
  Serial.println("Waiting for next RFID card...");
  Serial.println("========================================\n");
  
  delay(3000);
}