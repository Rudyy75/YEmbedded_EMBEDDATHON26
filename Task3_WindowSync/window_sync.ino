/*
 * Task 3: The Window Synchronizer
 * "Catch the moment between moments"
 * 
 * Objective: Press a physical button during the 500-1000ms MQTT window
 * with ±50ms tolerance. Achieve 3 successful syncs.
 * 
 * Hardware:
 *   - Common Anode RGB LED (LOW = ON, HIGH = OFF)
 *   - Blue LED (GPIO 23): Window indicator
 *   - Green LED (GPIO 19): Button press indicator
 *   - Red LED (GPIO 18): No window indicator (pulses)
 *   - Push Button (GPIO 4): With internal pull-up
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ============================================
// COMMON ANODE RGB LED - Logic is INVERTED!
// LOW = LED ON, HIGH = LED OFF
// ============================================

// Common Anode helper macros (inverted logic)
#define LED_ON  LOW
#define LED_OFF HIGH

// Timing constants
#define DEBOUNCE_MS 20           // Button debounce time
#define SYNC_TOLERANCE_MS 50     // ±50ms tolerance (per problem statement)
#define BUTTON_LED_FLASH_MS 100  // Green LED flash duration
#define WINDOW_TIMEOUT_MS 1200   // Max window duration + buffer (1000ms + 200ms)
#define RED_PULSE_INTERVAL_MS 500 // Red LED pulse interval

// Response topic
#define RESPONSE_TOPIC "cagedmonkey/listener"

WiFiClient espClient;
PubSubClient mqtt(espClient);

// Window state (volatile for ISR access)
volatile bool windowOpen = false;
volatile unsigned long windowOpenTime = 0;

// Button state (volatile for ISR access)
volatile bool buttonPressed = false;
volatile unsigned long buttonPressTime = 0;

// Sync tracking
int syncCount = 0;

// Non-blocking LED flash tracking
unsigned long buttonLedOffTime = 0;
bool buttonLedOn = false;

// Red LED pulse tracking
unsigned long lastPulseTime = 0;
bool pulseState = false;

// ============================================
// MQTT CALLBACK - Window Detection + Next Code
// ============================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Print raw message for debugging
    char msgBuffer[512];
    int copyLen = min((int)length, 511);
    memcpy(msgBuffer, payload, copyLen);
    msgBuffer[copyLen] = '\0';
    
    Serial.printf("[MQTT] Received on %s: %s\n", topic, msgBuffer);
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    
    if (error) {
        // Not JSON - print anyway, might be the code!
        Serial.println("\n##############################################");
        Serial.println("### NON-JSON MESSAGE RECEIVED! ###");
        Serial.printf("Topic: %s\n", topic);
        Serial.printf("Raw: %s\n", msgBuffer);
        Serial.println("##############################################\n");
        return;
    }
    
    const char* status = doc["status"];
    
    // Handle window open/close on window topic
    if (strcmp(topic, WINDOW_TOPIC) == 0) {
        if (status && strcmp(status, "open") == 0) {
            noInterrupts();
            windowOpen = true;
            windowOpenTime = millis();
            interrupts();
            
            digitalWrite(LED_BLUE, LED_ON);
            digitalWrite(LED_RED, LED_OFF);
            
            Serial.printf("[WINDOW] OPENED at %lu ms\n", windowOpenTime);
        } 
        else if (status && strcmp(status, "closed") == 0) {
            unsigned long closeTime = millis();
            unsigned long duration = closeTime - windowOpenTime;
            
            noInterrupts();
            windowOpen = false;
            interrupts();
            
            digitalWrite(LED_BLUE, LED_OFF);
            digitalWrite(LED_RED, LED_ON);
            
            Serial.printf("[WINDOW] CLOSED at %lu ms (was open for %lu ms)\n", closeTime, duration);
        }
        return;
    }
    
    // Ignore our own sync messages
    if (status && strcmp(status, "synced") == 0) {
        return;
    }
    
    // Ignore the old "HOT STREAK" message (that was Task 2 -> Task 3 code)
    if (strstr(msgBuffer, "lkjhgf_window") != NULL) {
        // Already have this code, skip
        return;
    }
    
    // ANY other message could be the next challenge code!
    Serial.println("\n##############################################");
    Serial.println("### POTENTIAL NEXT CHALLENGE CODE! ###");
    Serial.println("##############################################");
    Serial.printf("Topic: %s\n", topic);
    Serial.printf("Full message: %s\n", msgBuffer);
    
    // Try to extract any known field
    const char* code = doc["code"];
    const char* nextTask = doc["next_task"];
    const char* challenge = doc["challenge"];
    const char* task = doc["task"];
    const char* stego = doc["steganography"];
    const char* next = doc["next"];
    const char* hint = doc["hint"];
    
    if (code) Serial.printf(">>> CODE: %s\n", code);
    if (nextTask) Serial.printf(">>> NEXT_TASK: %s\n", nextTask);
    if (challenge) Serial.printf(">>> CHALLENGE: %s\n", challenge);
    if (task) Serial.printf(">>> TASK: %s\n", task);
    if (stego) Serial.printf(">>> STEGANOGRAPHY: %s\n", stego);
    if (next) Serial.printf(">>> NEXT: %s\n", next);
    if (hint) Serial.printf(">>> HINT: %s\n", hint);
    
    Serial.println("##############################################\n");
}

// ============================================
// BUTTON ISR - Hardware Interrupt with Debounce
// ============================================
void IRAM_ATTR buttonISR() {
    static unsigned long lastPress = 0;
    unsigned long now = millis();
    
    // Debounce: ignore presses within DEBOUNCE_MS of last press
    if (now - lastPress > DEBOUNCE_MS) {
        buttonPressTime = now;
        buttonPressed = true;
        lastPress = now;
    }
}

// ============================================
// CHECK SYNC - Main synchronization logic
// ============================================
void checkSync() {
    // Atomic read of button state
    noInterrupts();
    bool btnPressed = buttonPressed;
    unsigned long btnTime = buttonPressTime;
    bool winOpen = windowOpen;
    unsigned long winOpenTime = windowOpenTime;
    if (btnPressed) buttonPressed = false;  // Reset atomically
    interrupts();
    
    // Only proceed if button was pressed
    if (!btnPressed) return;
    
    // Flash green LED on any button press (non-blocking)
    digitalWrite(LED_GREEN, LED_ON);
    buttonLedOn = true;
    buttonLedOffTime = millis() + BUTTON_LED_FLASH_MS;
    
    // Check if button was pressed while window was open
    if (winOpen) {
        long timeSinceOpen = (long)(btnTime - winOpenTime);
        
        // Button pressed during the window
        if (timeSinceOpen >= 0) {
            // Calculate sync time
            unsigned long syncTime = btnTime;
            
            // Prepare ACK message
            StaticJsonDocument<128> doc;
            doc["status"] = "synced";
            doc["timestamp_ms"] = syncTime;
            
            char buffer[128];
            serializeJson(doc, buffer);
            
            // Publish sync confirmation
            if (mqtt.publish(RESPONSE_TOPIC, buffer)) {
                syncCount++;
                
                Serial.println("╔══════════════════════════════════════════╗");
                Serial.printf("║  [SYNC] SUCCESS #%d                        ║\n", syncCount);
                Serial.printf("║  Button pressed at: %lu ms               ║\n", btnTime);
                Serial.printf("║  Window opened at:  %lu ms               ║\n", winOpenTime);
                Serial.printf("║  Time since open:   %ld ms                ║\n", timeSinceOpen);
                Serial.println("╚══════════════════════════════════════════╝");
                Serial.printf("[MQTT] Published to %s\n", RESPONSE_TOPIC);
                
                if (syncCount >= 3) {
                    Serial.println("\n********************************************");
                    Serial.println("***  3 SUCCESSFUL SYNCS ACHIEVED!  ***");
                    Serial.println("***  Waiting for next challenge code...  ***");
                    Serial.println("********************************************\n");
                }
            } else {
                Serial.println("[ERROR] Failed to publish sync ACK");
            }
        }
    } else {
        // Button pressed but window was NOT open
        Serial.printf("[SYNC] MISSED - Button at %lu ms, window not open\n", btnTime);
    }
}

// ============================================
// WINDOW TIMEOUT - Auto-close if no "closed" message
// ============================================
void checkWindowTimeout() {
    if (windowOpen) {
        unsigned long elapsed = millis() - windowOpenTime;
        if (elapsed > WINDOW_TIMEOUT_MS) {
            noInterrupts();
            windowOpen = false;
            interrupts();
            
            digitalWrite(LED_BLUE, LED_OFF);
            digitalWrite(LED_RED, LED_ON);
            
            Serial.printf("[WINDOW] TIMEOUT - Auto-closed after %lu ms\n", elapsed);
        }
    }
}

// ============================================
// NON-BLOCKING LED HANDLERS
// ============================================
void handleButtonLed() {
    // Turn off green LED after flash duration
    if (buttonLedOn && millis() >= buttonLedOffTime) {
        digitalWrite(LED_GREEN, LED_OFF);
        buttonLedOn = false;
    }
}

void handleNoWindowPulse() {
    // Pulse red LED when no window is present
    if (!windowOpen) {
        if (millis() - lastPulseTime >= RED_PULSE_INTERVAL_MS) {
            pulseState = !pulseState;
            digitalWrite(LED_RED, pulseState ? LED_ON : LED_OFF);
            lastPulseTime = millis();
        }
    }
}

// ============================================
// MQTT RECONNECTION
// ============================================
void reconnect() {
    while (!mqtt.connected()) {
        Serial.print("[MQTT] Connecting to broker...");
        if (mqtt.connect(TEAM_ID)) {
            Serial.println(" connected!");
            
            // Subscribe to window topic
            mqtt.subscribe(WINDOW_TOPIC);
            Serial.printf("[MQTT] Subscribed to: %s\n", WINDOW_TOPIC);
            
            // Subscribe to response topic (for next code after 3 syncs)
            mqtt.subscribe(RESPONSE_TOPIC);
            Serial.printf("[MQTT] Subscribed to: %s\n", RESPONSE_TOPIC);
            
            // Subscribe to reef ID (might get next code here)
            mqtt.subscribe(REEF_ID);
            Serial.printf("[MQTT] Subscribed to: %s\n", REEF_ID);
            
            // Try cagedmonkey wildcard
            mqtt.subscribe("cagedmonkey/#");
            Serial.printf("[MQTT] Subscribed to: cagedmonkey/#\n");
            
        } else {
            Serial.printf(" failed, rc=%d. Retrying in 2s...\n", mqtt.state());
            delay(2000);
        }
    }
}

// ============================================
// SETUP
// ============================================
void setup() {
    Serial.begin(115200);
    delay(100);
    
    Serial.println("\n╔══════════════════════════════════════════╗");
    Serial.println("║     Task 3: The Window Synchronizer       ║");
    Serial.println("║     \"Catch the moment between moments\"    ║");
    Serial.println("╚══════════════════════════════════════════╝\n");
    
    // Configure RGB LED pins (Common Anode - OUTPUT)
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    
    // Configure button with internal pull-up
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    // Initial LED state: Red ON (no window)
    digitalWrite(LED_RED, LED_ON);
    digitalWrite(LED_GREEN, LED_OFF);
    digitalWrite(LED_BLUE, LED_OFF);
    
    // Attach button interrupt (trigger on falling edge - button press)
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
    
    // Connect to WiFi
    Serial.printf("[WIFI] Connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    
    // Setup MQTT
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    
    Serial.println("\n┌──────────────────────────────────────────┐");
    Serial.printf("│ Window Topic: %-26s │\n", WINDOW_TOPIC);
    Serial.printf("│ Response Topic: %-24s │\n", RESPONSE_TOPIC);
    Serial.println("├──────────────────────────────────────────┤");
    Serial.println("│ LED Indicators:                          │");
    Serial.println("│   RED pulses     = No window             │");
    Serial.println("│   BLUE ON        = Window OPEN           │");
    Serial.println("│   GREEN flash    = Button pressed        │");
    Serial.println("├──────────────────────────────────────────┤");
    Serial.println("│ Press button when BLUE LED is ON!        │");
    Serial.println("│ Need 3 successful syncs to complete.     │");
    Serial.println("└──────────────────────────────────────────┘\n");
}

// ============================================
// MAIN LOOP - Non-blocking superloop
// ============================================
void loop() {
    // Maintain MQTT connection
    if (!mqtt.connected()) {
        reconnect();
    }
    mqtt.loop();
    
    // Check for sync condition
    checkSync();
    
    // Check window timeout (in case broker doesn't send "closed")
    checkWindowTimeout();
    
    // Handle non-blocking LED operations
    handleButtonLed();
    handleNoWindowPulse();
}
