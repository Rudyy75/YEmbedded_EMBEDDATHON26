#ifndef CONFIG_H
#define CONFIG_H

// WiFi Credentials
#define WIFI_SSID "crn_net"
#define WIFI_PASSWORD "crn_l016"

// MQTT Broker
#define MQTT_BROKER "broker.mqttdashboard.com"
#define MQTT_PORT 1883

// Team Identifiers
#define TEAM_ID "aniruddhyembedded"
#define REEF_ID "aniruddhyelluriyembedded"

// Window Topic from Task 2 - UPDATE THIS WITH YOUR ACTUAL CODE
#define WINDOW_TOPIC "lkjhgf_window"

// Hardware Pins (Common Anode RGB LED)
#define LED_RED 18      // Red channel - no window indicator
#define LED_GREEN 19    // Green channel - button press indicator
#define LED_BLUE 23     // Blue channel - window open indicator
#define BUTTON_PIN 4    // Push button

// OLED Display (optional)
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDR 0x3C

#endif
