/*
 * Task 4: The Silent Image - ESP32 Steganography Decoder
 * "Not everything worth knowing announces itself."
 * 
 * This is a minimal ESP32 version that:
 * 1. Sends the request to the reef
 * 2. Receives the base64 image data
 * 3. Prints it to Serial for Python processing
 * 
 * RECOMMENDED: Use steg_decoder.py for full processing
 * ESP32 has limited RAM for image processing.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

WiFiClient espClient;
PubSubClient mqtt(espClient);

// Store received base64 data (limited by ESP32 RAM)
String receivedData = "";
bool dataReceived = false;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.println("\n##############################################");
    Serial.printf("RECEIVED on %s\n", topic);
    Serial.println("##############################################");
    
    // Convert to string
    String message = "";
    for (int i = 0; i < length && i < 50000; i++) {
        message += (char)payload[i];
    }
    
    Serial.printf("Payload length: %d bytes\n", length);
    
    // Try to parse JSON
    DynamicJsonDocument doc(60000);  // Large buffer for base64 data
    DeserializationError error = deserializeJson(doc, payload, length);
    
    if (error) {
        Serial.printf("JSON parse error: %s\n", error.c_str());
        Serial.println("Raw message:");
        Serial.println(message.substring(0, 500));
        return;
    }
    
    // Check for image data
    if (doc.containsKey("data")) {
        Serial.println("\n*** IMAGE DATA RECEIVED! ***\n");
        
        const char* imgType = doc["type"] | "unknown";
        int width = doc["width"] | 0;
        int height = doc["height"] | 0;
        
        Serial.printf("Type: %s\n", imgType);
        Serial.printf("Dimensions: %dx%d\n", width, height);
        
        const char* base64Data = doc["data"];
        int dataLen = strlen(base64Data);
        Serial.printf("Base64 data length: %d chars\n", dataLen);
        
        // Print marker for Python script to capture
        Serial.println("\n--- BASE64_START ---");
        Serial.println(base64Data);
        Serial.println("--- BASE64_END ---");
        
        Serial.println("\n*** Copy base64 data between markers ***");
        Serial.println("*** Use Python script to decode and extract LSB ***");
        
        dataReceived = true;
    }
    
    // Check for target URL
    if (doc.containsKey("target_image_url")) {
        Serial.println("\n*** TARGET IMAGE URL RECEIVED! ***");
        const char* url = doc["target_image_url"];
        Serial.println(url);
        Serial.println("*** Save this for Task 5! ***\n");
    }
}

void reconnect() {
    while (!mqtt.connected()) {
        Serial.print("[MQTT] Connecting...");
        if (mqtt.connect(TEAM_ID)) {
            Serial.println(" connected!");
            
            // Subscribe to challenge code topic
            mqtt.subscribe(CHALLENGE_CODE);
            Serial.printf("[MQTT] Subscribed to: %s\n", CHALLENGE_CODE);
            
            // Subscribe to team topic
            mqtt.subscribe(TEAM_ID);
            Serial.printf("[MQTT] Subscribed to: %s\n", TEAM_ID);
            
        } else {
            Serial.printf(" failed, rc=%d\n", mqtt.state());
            delay(2000);
        }
    }
}

void sendRequest() {
    Serial.println("\n##############################################");
    Serial.println("PHASE 1: SIGNALING THE REEF");
    Serial.println("##############################################");
    
    StaticJsonDocument<256> doc;
    doc["request"] = HIDDEN_MESSAGE;
    doc["agent_id"] = TEAM_ID;
    
    char buffer[256];
    serializeJson(doc, buffer);
    
    Serial.printf("[TX] Topic: %s\n", REQUEST_TOPIC);
    Serial.printf("[TX] Message: %s\n", buffer);
    
    if (mqtt.publish(REQUEST_TOPIC, buffer)) {
        Serial.println("[TX] Request sent successfully!");
    } else {
        Serial.println("[TX] Failed to send request!");
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);
    
    Serial.println("\n##############################################");
    Serial.println("Task 4: The Silent Image");
    Serial.println("ESP32 Steganography Helper");
    Serial.println("##############################################\n");
    
    // Connect WiFi
    Serial.printf("[WIFI] Connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    
    // Setup MQTT with large buffer for image data
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    mqtt.setBufferSize(60000);  // 60KB buffer for base64 data
    
    Serial.printf("\n[CONFIG] Challenge Code: %s\n", CHALLENGE_CODE);
    Serial.printf("[CONFIG] Request Topic: %s\n", REQUEST_TOPIC);
    Serial.printf("[CONFIG] Hidden Message: %s\n", HIDDEN_MESSAGE);
    
    Serial.println("\n[INFO] Type 's' to send request");
    Serial.println("[INFO] Waiting for commands...\n");
}

bool requestSent = false;

void loop() {
    if (!mqtt.connected()) {
        reconnect();
    }
    mqtt.loop();
    
    // Auto-send request once after connection
    if (mqtt.connected() && !requestSent) {
        delay(1000);  // Wait for subscriptions
        sendRequest();
        requestSent = true;
    }
    
    // Manual command input
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 's' || c == 'S') {
            sendRequest();
        }
    }
}
