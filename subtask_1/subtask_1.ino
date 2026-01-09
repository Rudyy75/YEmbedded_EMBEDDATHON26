#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// RTOS Handles
QueueHandle_t commandQueue;
SemaphoreHandle_t delayMutex;
TaskHandle_t t_Input;
TaskHandle_t t_Display;
TaskHandle_t t_Blink;

// Shared variable
volatile int currentDelay = 1000;

// Queue data structure
struct Command {
  char text[20];
  int blinkRate;
};

// TASK 1: THE HEART (Blink LED)
void HeartTask(void *pvParameters) {
  pinMode(2, OUTPUT);
  
  for (;;) {
    int localDelay;
    xSemaphoreTake(delayMutex, portMAX_DELAY);
    localDelay = currentDelay;
    xSemaphoreGive(delayMutex);
    
    digitalWrite(2, HIGH);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    digitalWrite(2, LOW);
    vTaskDelay(localDelay / portTICK_PERIOD_MS);
  }
}

// TASK 2: THE EAR (Serial Input)
void InputTask(void *pvParameters) {
  Serial.begin(115200);
  
  for (;;) {
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n');
      input.trim();

      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, input);

      if (!error) {
        Command cmd;
        strlcpy(cmd.text, doc["msg"], sizeof(cmd.text));
        cmd.blinkRate = doc["delay"];
        xQueueSend(commandQueue, &cmd, portMAX_DELAY);
        Serial.println("Command sent to queue!");
      } else {
        Serial.println("JSON Error");
      }
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// TASK 3: THE FACE (OLED Display)
void DisplayTask(void *pvParameters) {
  Command receivedCmd;
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println("Waiting...");
  display.display();

  for (;;) {
    if (xQueueReceive(commandQueue, &receivedCmd, portMAX_DELAY) == pdTRUE) {
      xSemaphoreTake(delayMutex, portMAX_DELAY);
      currentDelay = receivedCmd.blinkRate;
      xSemaphoreGive(delayMutex);

      display.clearDisplay();
      display.setCursor(0, 20);
      display.println(receivedCmd.text);
      display.display();
      Serial.println("Screen Updated.");
    }
  }
}

void setup() {
  delayMutex = xSemaphoreCreateMutex();
  commandQueue = xQueueCreate(5, sizeof(Command));

  xTaskCreate(HeartTask, "Heart", 1024, NULL, 1, &t_Blink);
  xTaskCreate(InputTask, "Input", 4096, NULL, 1, &t_Input);
  xTaskCreate(DisplayTask, "Display", 4096, NULL, 1, &t_Display);
}

void loop() {}
