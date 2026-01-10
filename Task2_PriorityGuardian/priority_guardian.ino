#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ESP32 onboard LED (GPIO 2) - Active HIGH
#define DISTRESS_LED 2

WiFiClient espClient;
PubSubClient mqtt(espClient);

// Rolling average data
float values[10] = {0};
int valueIndex = 0;
int valueCount = 0;

// Queues and Semaphores
QueueHandle_t bgDataQueue;
QueueHandle_t distressQueue;
SemaphoreHandle_t serialMutex;

// Task handles
TaskHandle_t mqttTask, bgTask, distressTask;

// Calculate rolling average
float calcRollingAvg() {
  float sum = 0;
  int count = min(valueCount, 10);
  for (int i = 0; i < count; i++) sum += values[i];
  return count > 0 ? sum / count : 0;
}

// Thread-safe serial print
void safePrint(const char* msg) {
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    Serial.print(msg);
    xSemaphoreGive(serialMutex);
  }
}

void safePrintf(const char* format, ...) {
  char buffer[200];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  safePrint(buffer);
}

// MQTT callback - runs in MQTT task context
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  
  // Check for Task 3 confirmation/code (look for window topic info)
  if (msg.indexOf("window") >= 0 || msg.indexOf("task3") >= 0 || 
      msg.indexOf("code") >= 0 || msg.indexOf("next") >= 0 ||
      msg.indexOf("topic") >= 0) {
    // This could be the Task 3 confirmation!
    safePrintf("\n*** BROKER MESSAGE on %s ***\n", topic);
    safePrintf(">>> %s <<<\n\n", msg.c_str());
  }
  
  if (topicStr == TEAM_ID) {
    // HIGH PRIORITY: Distress signal
    if (msg.indexOf("CHALLENGE") >= 0) {
      unsigned long timestamp = millis();
      digitalWrite(DISTRESS_LED, HIGH);  // LED ON (active HIGH for GPIO 2)
      xQueueSend(distressQueue, &timestamp, 0);
    } else {
      // Any other message on TeamID - could be Task 3 code!
      safePrintf("[TEAM_ID MSG] %s\n", msg.c_str());
    }
  }
  else if (topicStr == REEF_ID) {
    // Messages on ReefID - likely Task 3 confirmation!
    safePrintf("[REEF_ID MSG] %s\n", msg.c_str());
  }
  else if (topicStr == "krillparadise/data/stream") {
    // LOW PRIORITY: Background data
    float val = msg.toFloat();
    xQueueSend(bgDataQueue, &val, 0);
  }
  else {
    // Unknown topic - print it
    safePrintf("[UNKNOWN TOPIC: %s] %s\n", topic, msg.c_str());
  }
}

// Priority 3 (HIGHEST): Distress handler task
void DistressTask(void* param) {
  unsigned long distressTime;
  
  for (;;) {
    if (xQueueReceive(distressQueue, &distressTime, portMAX_DELAY) == pdTRUE) {
      unsigned long ackTime = millis();
      
      // Build ACK response
      StaticJsonDocument<128> doc;
      doc["status"] = "ACK";
      doc["timestamp_ms"] = ackTime;
      
      char buffer[128];
      serializeJson(doc, buffer);
      
      // Publish ACK to ReefID
      mqtt.publish(REEF_ID, buffer);
      
      // LED OFF after ACK sent
      digitalWrite(DISTRESS_LED, LOW);
      
      unsigned long responseTime = ackTime - distressTime;
      safePrintf("[DISTRESS] Challenge at %lu → ACK at %lu (Response: %lu ms)\n", 
                 distressTime, ackTime, responseTime);
      
      if (responseTime > 150) {
        safePrint("[WARNING] Response time > 150ms!\n");
      }
    }
  }
}

// Priority 2 (MEDIUM): MQTT event dispatcher task
void MqttTask(void* param) {
  for (;;) {
    if (!mqtt.connected()) {
      safePrint("Connecting MQTT...\n");
      if (mqtt.connect(TEAM_ID)) {
        safePrint("MQTT connected!\n");
        mqtt.subscribe("krillparadise/data/stream");
        mqtt.subscribe(TEAM_ID);
        mqtt.subscribe(REEF_ID);  // Subscribe to get Task 3 confirmation!
        safePrintf("Subscribed to: krillparadise/data/stream, %s, %s\n", TEAM_ID, REEF_ID);
      } else {
        safePrintf("MQTT failed, rc=%d\n", mqtt.state());
        vTaskDelay(pdMS_TO_TICKS(2000));
        continue;
      }
    }
    mqtt.loop();
    vTaskDelay(pdMS_TO_TICKS(10));  // Small delay to prevent starvation
  }
}

// Priority 1 (LOWEST): Background chorus task - rolling average
void BackgroundTask(void* param) {
  float newVal;
  
  for (;;) {
    if (xQueueReceive(bgDataQueue, &newVal, portMAX_DELAY) == pdTRUE) {
      // Update rolling average
      values[valueIndex] = newVal;
      valueIndex = (valueIndex + 1) % 10;
      if (valueCount < 10) valueCount++;
      
      float avg = calcRollingAvg();
      safePrintf("[BG] Value: %.2f → Rolling Avg: %.2f\n", newVal, avg);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Task 2: Priority Guardian ===");
  
  // Setup onboard LED
  pinMode(DISTRESS_LED, OUTPUT);
  digitalWrite(DISTRESS_LED, LOW);
  
  // Create synchronization primitives
  serialMutex = xSemaphoreCreateMutex();
  bgDataQueue = xQueueCreate(20, sizeof(float));
  distressQueue = xQueueCreate(5, sizeof(unsigned long));
  
  if (!serialMutex || !bgDataQueue || !distressQueue) {
    Serial.println("Failed to create sync primitives!");
    while(1);
  }
  
  // Connect WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
  
  // Setup MQTT
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  
  // Create tasks with PROPER priorities as per problem statement
  // Priority 1 (lowest): Background chorus
  // Priority 2 (medium): MQTT dispatcher
  // Priority 3 (highest): Distress handler
  xTaskCreatePinnedToCore(BackgroundTask, "Background", 4096, NULL, 1, &bgTask, 1);
  xTaskCreatePinnedToCore(MqttTask, "MQTT", 8192, NULL, 2, &mqttTask, 0);
  xTaskCreatePinnedToCore(DistressTask, "Distress", 4096, NULL, 3, &distressTask, 1);
  
  Serial.println("\nTasks created:");
  Serial.println("  Priority 1: Background (rolling average)");
  Serial.println("  Priority 2: MQTT Dispatcher");
  Serial.println("  Priority 3: Distress Handler");
  Serial.println("\nWaiting for MQTT messages...\n");
}

void loop() {
  // Empty - all work done in FreeRTOS tasks
  vTaskDelay(portMAX_DELAY);
}
