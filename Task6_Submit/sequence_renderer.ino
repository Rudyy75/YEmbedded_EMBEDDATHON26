/*
 * TASK 6: THE SEQUENCE RENDERER (GOATED EDITION 🐐)
 * - Perfect Synchronization
 * - Smooth OLED Rendering
 * - Robust MQTT Handling
 * 
 * Topics:
 * - Subscribe: TeamID_ReefID (frames)
 * - Publish: ReefID_TeamID (ACKs)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============ CONFIGURATION ============
#define WIFI_SSID        "vivo V30 pro"
#define WIFI_PASSWORD    "micromax"
#define MQTT_BROKER      "broker.mqttdashboard.com"
#define MQTT_PORT        1883

#define SEQUENCER_TOPIC  "aniruddhyembedded_aniruddhyelluriyembedded" // Receive Frames
#define ACK_TOPIC        "aniruddhyelluriyembedded_aniruddhyembedded" // Send ACKs
#define AGENT_ID         "aniruddhyembedded"

// OLED Settings
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDR      0x3C
#define I2C_SDA        21
#define I2C_SCL        22

// ============ GLOBALS ============
WiFiClient espClient;
PubSubClient mqtt(espClient);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// State Variables
int currentFrame = -1;
int loopCount = 0;
bool sequenceStarted = false;
String lastFrameBase64 = "";

// ============ DISPLAY FUNCTIONS ============
void setupDisplay() {
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("[OLED] Alloc Failed"));
    for(;;);
  }
  display.clearDisplay();
  
  // Custom Splash Screen
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println(F("TASK 6: SEQUENCE"));
  display.setCursor(35, 35);
  display.println(F("RENDERER"));
  display.drawRect(5, 15, 118, 35, SSD1306_WHITE);
  display.display();
}

void renderFrameUI(int frameNo, int delayMs) {
  display.clearDisplay();

  // Frame Number (Large & Centered)
  display.setTextSize(4);
  display.setTextColor(SSD1306_WHITE);
  
  // Center alignment for 1 or 2 or 3 digits
  int xPos = (frameNo < 10) ? 55 : (frameNo < 100) ? 42 : 30;
  display.setCursor(xPos, 10);
  display.printf("%d", frameNo);

  // Info Bar at Bottom
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.printf("L:%d", loopCount);
  
  display.setCursor(80, 56);
  display.printf("%dms", delayMs);

  // Dynamic Progress Bar (Top)
  int barWidth = map(frameNo % 60, 0, 60, 0, SCREEN_WIDTH);
  display.fillRect(0, 0, barWidth, 4, SSD1306_WHITE);

  display.display();
}

void showStatus(String msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 25);
  display.println(msg);
  display.display();
}

// ============ NETWORK FUNCTIONS ============
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  
  Serial.print(F("WiFi Connecting"));
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(F(" OK"));
}

void connectMQTT() {
  if (mqtt.connected()) return;
  
  Serial.print(F("MQTT Connecting..."));
  // Unique Client ID to prevent broker kicks
  String clientId = String(AGENT_ID) + "-T6-" + String(random(0xFFFF), HEX);
  
  if (mqtt.connect(clientId.c_str())) {
    Serial.println(F("OK"));
    mqtt.subscribe(SEQUENCER_TOPIC);
    Serial.printf("Subscribed: %s\n", SEQUENCER_TOPIC);
  } else {
    Serial.printf("Fail rc=%d\n", mqtt.state());
    delay(1000);
  }
}

// ============ LOGIC ============
void sendAck(int frameNo) {
  // Efficient JSON construction
  // We allocate just enough memory
  DynamicJsonDocument doc(1024 + lastFrameBase64.length()); 
  
  doc["frame_no"] = frameNo;
  doc["timestamp_ms"] = millis();
  doc["image_base64"] = lastFrameBase64; // Required by spec
  doc["ack_id"] = AGENT_ID;
  
  String output;
  serializeJson(doc, output);
  
  if (mqtt.publish(ACK_TOPIC, output.c_str())) {
    Serial.printf(">> ACK Frame %d Sent\n", frameNo);
  } else {
    Serial.printf("!! ACK Frame %d Failed (Size: %d)\n", frameNo, output.length());
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Using a larger buffer for frame data
  DynamicJsonDocument doc(40000); 
  DeserializationError err = deserializeJson(doc, payload, length);

  if (err) {
    Serial.print(F("JSON Error: "));
    Serial.println(err.c_str());
    return;
  }

  int frameNo = doc["frame_no"];
  const char* dataRaw = doc["data"];
  int delayMs = doc["delay_ms"] | 100; // Default 100ms
  
  // Handle START
  if (frameNo == 0) {
    Serial.println(F("--- SEQUENCE START ---"));
    sequenceStarted = true;
    currentFrame = 0;
    showStatus("SEQUENCE START");
    return;
  }

  // Handle TERMINATOR (Loop)
  if (frameNo == 255) {
    loopCount++;
    Serial.printf("--- LOOP COMPLETED (%d) ---\n", loopCount);
    currentFrame = 0;
    return;
  }

  // Regular Frame
  currentFrame = frameNo;
  lastFrameBase64 = String(dataRaw);

  // 1. Render
  renderFrameUI(frameNo, delayMs);
  
  // 2. Wait (Blocking with Keep-Alive)
  // We must wait correct duration BEFORE processing next frame/ACK
  unsigned long startWait = millis();
  while(millis() - startWait < delayMs) {
    if ((millis() - startWait) % 10 == 0) mqtt.loop(); // Keep MQTT alive
  }

  // 3. Acknowledge
  sendAck(frameNo);
}

void setup() {
  Serial.begin(115200);
  
  // High speed I2C
  Wire.setClock(400000);

  setupDisplay();
  connectWiFi();
  
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  // Important: Increase buffer size for large base64 payloads
  mqtt.setBufferSize(40960); 

  showStatus("Waiting for Sync...");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqtt.connected()) connectMQTT();
  
  mqtt.loop();
  
  // Status heartbeat
  static unsigned long lastLog = 0;
  if (millis() - lastLog > 5000) {
    Serial.printf("[STATUS] Fr:%d Loop:%d WiFi:%d\n", currentFrame, loopCount, WiFi.RSSI());
    lastLog = millis();
  }
}
