#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

// RGB LED Pins (from config.h)
#define LED_R LED_RED
#define LED_G LED_GREEN
#define LED_B LED_BLUE

// Common Anode: LOW = ON, HIGH = OFF
#define LED_ON  LOW
#define LED_OFF HIGH

WiFiClient espClient;
PubSubClient mqtt(espClient);

// Timing arrays for each color
int redTimings[50];
int greenTimings[50];
int blueTimings[50];
volatile int redCount = 0, greenCount = 0, blueCount = 0;

// Mutex for protecting shared timing data
SemaphoreHandle_t timingMutex;

// Task handles
TaskHandle_t redTask, greenTask, blueTask;

// MQTT callback - runs when timing data arrives
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  
  if (err) {
    Serial.println("JSON parse error");
    return;
  }
  
  // Take mutex before modifying shared data
  if (xSemaphoreTake(timingMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    
    // Parse red array
    JsonArray red = doc["red"];
    redCount = 0;
    for (int val : red) {
      if (redCount < 50) redTimings[redCount++] = val;
    }
    
    // Parse green array
    JsonArray green = doc["green"];
    greenCount = 0;
    for (int val : green) {
      if (greenCount < 50) greenTimings[greenCount++] = val;
    }
    
    // Parse blue array
    JsonArray blue = doc["blue"];
    blueCount = 0;
    for (int val : blue) {
      if (blueCount < 50) blueTimings[blueCount++] = val;
    }
    
    // Release mutex
    xSemaphoreGive(timingMutex);
    
    Serial.println("New timing pattern received!");
    Serial.printf("R: %d, G: %d, B: %d timings\n", redCount, greenCount, blueCount);
  }
}

// Red LED task
void RedTask(void* param) {
  pinMode(LED_R, OUTPUT);
  TickType_t xLastWakeTime = xTaskGetTickCount();
  int idx = 0;
  bool state = true;
  int localCount = 0;
  int localTimings[50];
  
  for (;;) {
    // Safely copy data with mutex
    if (xSemaphoreTake(timingMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      localCount = redCount;
      if (localCount > 0) {
        memcpy(localTimings, redTimings, localCount * sizeof(int));
        // Clamp index to prevent stale data access
        if (idx >= localCount) idx = 0;
      }
      xSemaphoreGive(timingMutex);
    }
    
    if (localCount > 0) {
      digitalWrite(LED_R, state ? LED_ON : LED_OFF);
      vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(localTimings[idx]));
      state = !state;
      idx = (idx + 1) % localCount;
    } else {
      digitalWrite(LED_R, LED_OFF);
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

// Green LED task
void GreenTask(void* param) {
  pinMode(LED_G, OUTPUT);
  TickType_t xLastWakeTime = xTaskGetTickCount();
  int idx = 0;
  bool state = true;
  int localCount = 0;
  int localTimings[50];
  
  for (;;) {
    if (xSemaphoreTake(timingMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      localCount = greenCount;
      if (localCount > 0) {
        memcpy(localTimings, greenTimings, localCount * sizeof(int));
        // Clamp index to prevent stale data access
        if (idx >= localCount) idx = 0;
      }
      xSemaphoreGive(timingMutex);
    }
    
    if (localCount > 0) {
      digitalWrite(LED_G, state ? LED_ON : LED_OFF);
      vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(localTimings[idx]));
      state = !state;
      idx = (idx + 1) % localCount;
    } else {
      digitalWrite(LED_G, LED_OFF);
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

// Blue LED task
void BlueTask(void* param) {
  pinMode(LED_B, OUTPUT);
  TickType_t xLastWakeTime = xTaskGetTickCount();
  int idx = 0;
  bool state = true;
  int localCount = 0;
  int localTimings[50];
  
  for (;;) {
    if (xSemaphoreTake(timingMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      localCount = blueCount;
      if (localCount > 0) {
        memcpy(localTimings, blueTimings, localCount * sizeof(int));
        // Clamp index to prevent stale data access
        if (idx >= localCount) idx = 0;
      }
      xSemaphoreGive(timingMutex);
    }
    
    if (localCount > 0) {
      digitalWrite(LED_B, state ? LED_ON : LED_OFF);
      vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(localTimings[idx]));
      state = !state;
      idx = (idx + 1) % localCount;
    } else {
      digitalWrite(LED_B, LED_OFF);
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

// MQTT reconnect
void reconnect() {
  while (!mqtt.connected()) {
    Serial.print("Connecting MQTT...");
    if (mqtt.connect(TEAM_ID)) {
      Serial.println("connected!");
      mqtt.subscribe("shrimphub/led/timing/set");
      Serial.println("Subscribed to: shrimphub/led/timing/set");
    } else {
      Serial.printf("failed, rc=%d\n", mqtt.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // Create mutex BEFORE tasks
  timingMutex = xSemaphoreCreateMutex();
  if (timingMutex == NULL) {
    Serial.println("Mutex creation failed!");
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
  
  // Create LED tasks
  xTaskCreate(RedTask, "Red", 2048, NULL, 1, &redTask);
  xTaskCreate(GreenTask, "Green", 2048, NULL, 1, &greenTask);
  xTaskCreate(BlueTask, "Blue", 2048, NULL, 1, &blueTask);
  
  Serial.println("Tasks created. Waiting for timing data...");
}

void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }
  mqtt.loop();
}
