#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ============================================
// CONFIGURATION - Task 3: Window Synchronizer
// ============================================

// WiFi
#define WIFI_SSID "vivo V30 pro"
#define WIFI_PASSWORD "micromax"

// MQTT
#define MQTT_BROKER "broker.mqttdashboard.com"
#define MQTT_PORT 1883
#define TEAM_ID "aniruddhyembedded"

// Hardware Pins - YOUR CONFIG
#define LED_RED 18      // Red LED
#define LED_BLUE 19     // Blue LED - Window indicator
#define LED_GREEN 23    // Green LED - Sync success
#define BUTTON_PIN 4    // Push button

// Topics
#define WINDOW_TOPIC "lkjhgf_window"        // From Task 2
#define RESPONSE_TOPIC "cagedmonkey/listener"

// Common Anode = inverted logic (LOW=ON, HIGH=OFF)
// Change to false if using common cathode
#define COMMON_ANODE true

#if COMMON_ANODE
  #define LED_ON  LOW
  #define LED_OFF HIGH
#else
  #define LED_ON  HIGH
  #define LED_OFF LOW
#endif

// Timing
#define DEBOUNCE_MS 20

WiFiClient espClient;
PubSubClient mqtt(espClient);

// State
volatile bool windowOpen = false;
volatile unsigned long windowOpenTime = 0;
volatile bool buttonPressed = false;
volatile unsigned long buttonPressTime = 0;
int syncCount = 0;

// LED pulse
unsigned long lastPulse = 0;
bool pulseState = false;

// ============================================
// LED Control
// ============================================
void setRGB(bool r, bool g, bool b) {
  digitalWrite(LED_RED, r ? LED_ON : LED_OFF);
  digitalWrite(LED_GREEN, g ? LED_ON : LED_OFF);
  digitalWrite(LED_BLUE, b ? LED_ON : LED_OFF);
}

// ============================================
// Button ISR
// ============================================
void IRAM_ATTR buttonISR() {
  static unsigned long lastPress = 0;
  unsigned long now = millis();
  if (now - lastPress > DEBOUNCE_MS) {
    buttonPressTime = now;
    buttonPressed = true;
    lastPress = now;
  }
}

// ============================================
// MQTT Callback
// ============================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  
  Serial.printf("\n[RX] %s: %s\n", topic, msg.c_str());
  
  // Skip non-JSON messages (like the hidden message)
  if (msg.indexOf("{") < 0) {
    Serial.printf("[INFO] Non-JSON: %s\n", msg.c_str());
    return;
  }
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  
  if (error) {
    Serial.printf("[WARN] JSON error: %s\n", error.c_str());
    return;
  }
  
  // Check for window status
  if (doc.containsKey("status")) {
    const char* status = doc["status"];
    
    if (strcmp(status, "open") == 0) {
      windowOpen = true;
      windowOpenTime = millis();
      buttonPressed = false;
      
      setRGB(false, false, true);  // BLUE ON
      Serial.printf("[WINDOW] OPEN at %lu ms - PRESS BUTTON NOW!\n", windowOpenTime);
    }
    else if (strcmp(status, "closed") == 0) {
      windowOpen = false;
      setRGB(true, false, false);  // RED ON
      Serial.printf("[WINDOW] CLOSED\n");
    }
  }
  
  // Check for challenge code
  if (doc.containsKey("hidden_message") || doc.containsKey("challenge") || 
      doc.containsKey("target_image_url") || doc.containsKey("next")) {
    Serial.println("\n***** CHALLENGE CODE RECEIVED! *****");
    serializeJsonPretty(doc, Serial);
    Serial.println("\n************************************");
  }
}

// ============================================
// Check for Sync
// ============================================
void checkSync() {
  if (!buttonPressed) return;
  
  if (windowOpen) {
    // SUCCESS!
    long diff = (long)(buttonPressTime - windowOpenTime);
    
    StaticJsonDocument<128> doc;
    doc["status"] = "synced";
    doc["timestamp_ms"] = buttonPressTime;
    
    char buffer[128];
    serializeJson(doc, buffer);
    
    if (mqtt.publish(RESPONSE_TOPIC, buffer)) {
      syncCount++;
      
      // Flash GREEN
      setRGB(false, true, false);
      Serial.printf("\n[SYNC] SUCCESS #%d! Offset: %ld ms\n", syncCount, diff);
      Serial.printf("[TX] %s: %s\n", RESPONSE_TOPIC, buffer);
      delay(200);
      setRGB(false, false, true);  // Back to blue
      
      if (syncCount >= 3) {
        Serial.println("\n*** 3 SYNCS DONE! Waiting for Task 4 code... ***\n");
        // Keep green on
        setRGB(false, true, false);
      }
    }
  } else {
    Serial.printf("[MISS] Window not open!\n");
    // Flash red twice
    for (int i = 0; i < 2; i++) {
      setRGB(true, false, false);
      delay(100);
      setRGB(false, false, false);
      delay(100);
    }
    setRGB(true, false, false);
  }
  
  buttonPressed = false;
}

// ============================================
// Pulse Red LED when idle
// ============================================
void pulseIdle() {
  if (!windowOpen && millis() - lastPulse >= 500) {
    pulseState = !pulseState;
    digitalWrite(LED_RED, pulseState ? LED_ON : LED_OFF);
    lastPulse = millis();
  }
}

// ============================================
// Connect MQTT
// ============================================
void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("[MQTT] Connecting...");
    String clientId = String(TEAM_ID) + "_task3";
    
    if (mqtt.connect(clientId.c_str())) {
      Serial.println(" OK!");
      
      mqtt.subscribe(WINDOW_TOPIC);
      mqtt.subscribe(TEAM_ID);
      
      Serial.printf("[SUB] %s\n", WINDOW_TOPIC);
      Serial.printf("[SUB] %s\n", TEAM_ID);
    } else {
      Serial.printf(" FAIL (rc=%d)\n", mqtt.state());
      delay(2000);
    }
  }
}

// ============================================
// Setup
// ============================================
void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n========================================");
  Serial.println("     TASK 3: WINDOW SYNCHRONIZER");
  Serial.println("========================================");
  Serial.printf("Red: GPIO %d, Green: GPIO %d, Blue: GPIO %d\n", LED_RED, LED_GREEN, LED_BLUE);
  Serial.printf("Button: GPIO %d\n", BUTTON_PIN);
  Serial.println("========================================");
  
  // LED setup
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  setRGB(true, false, false);  // Red on initially
  
  // Button setup
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
  
  // WiFi
  Serial.printf("[WIFI] Connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  
  // Flash green for WiFi success
  setRGB(false, true, false);
  delay(300);
  setRGB(true, false, false);
  
  // MQTT
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(1024);
  
  Serial.println("\n[READY] Press button when BLUE LED is ON!");
  Serial.printf("[TOPIC] Window: %s\n", WINDOW_TOPIC);
  Serial.printf("[TOPIC] Response: %s\n", RESPONSE_TOPIC);
  Serial.println("========================================\n");
}

// ============================================
// Loop
// ============================================
void loop() {
  if (!mqtt.connected()) {
    connectMQTT();
  }
  mqtt.loop();
  
  checkSync();
  pulseIdle();
}
