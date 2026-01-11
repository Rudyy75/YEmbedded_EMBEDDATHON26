
# Embedded Systems & Steganography Tasks: Solutions

---

# Task 1: LED Timing Control via MQTT

## Slide 1: MQTT LED Timing Setup
- Connect ESP32 to **WiFi** using `WIFI_SSID` & `WIFI_PASSWORD`
- Setup MQTT client (`PubSubClient`) and connect to broker
- Subscribe to topic: `shrimphub/led/timing/set`
- Receive JSON arrays for **Red, Green, Blue LED timings**
- Mutex (`timingMutex`) protects shared timing arrays from concurrency issues

## Slide 2: LED Task Execution
- Create **three FreeRTOS tasks**: `RedTask`, `GreenTask`, `BlueTask`
- Each task toggles LED using timing array received via MQTT
- Tasks run independently and read shared data safely using mutex
- Use `vTaskDelayUntil()` for precise LED ON/OFF timing
- If no data, LED stays OFF, task waits 100ms before checking again
- MQTT callback updates timing arrays and signals new pattern arrival

---

# Task 2: Priority Guardian – ESP32 FreeRTOS

## Slide 1: Task Overview
- Three tasks with **priorities**:
  1. **Background (Low)** – Rolling average of sensor data
  2. **MQTT Dispatcher (Medium)** – Maintains MQTT connection
  3. **Distress Handler (High)** – Responds to urgent messages
- Use **queues** and **semaphores** for synchronization
- Onboard LED indicates distress signal

## Slide 2: Task Mechanics
- Background task updates rolling average from queue
- MQTT task reconnects automatically and subscribes to topics
- Distress task reacts immediately:
  - LED ON
  - Publishes ACK
  - Measures response time
- FreeRTOS ensures **priority-based scheduling**  

---

# Task 3: MQTT Data & Rolling Average

## Slide 1: Data Reception
- Subscribe to topic: `krillparadise/data/stream`
- Store incoming float values in **queue**
- Update **rolling average** of last 10 values
- Mutex protects serial prints for clean debugging output

## Slide 2: Processing & Output
- Background task prints: `[BG] Value: X → Rolling Avg: Y`
- Helps monitor data trends in real-time
- Prioritized tasks prevent slow MQTT or serial output from blocking urgent operations

---

# Task 4: Steganography Decoder – “The Silent Image”

## Slide 1: Image Reception
- Connect to MQTT broker
- Subscribe to `challenge_code` topic for image
- Publish request to `kelpsaute/steganography`
- Receive **Base64 PNG image** from server

## Slide 2: LSB Message Extraction
- Extract **hidden message** from least significant bits (LSB) of image pixels
- Methods:
  1. Red channel only
  2. All RGB channels
  3. Full scan for readable ASCII
  4. End of image scan
  5. Metadata check
  6. First 64x64 pixel region
- Save extracted message to `extracted_message.txt`
- Follow direction indicated in hidden message via MQTT

---

# Task 5: LSB Extraction Methods

## Slide 1: Overview
- **Method 1:** Red channel only LSB
- **Method 2:** All RGB channels LSB
- **Method 3:** Scan for readable ASCII sequences
- **Method 4:** Scan from end of image
- **Method 5:** Image metadata check
- **Method 6:** First 64x64 pixels

## Slide 2: Extraction Highlights
- Convert bits to bytes, then characters
- Stop at null terminator or non-printable chars
- Compare methods to get **best readable message**
- Supports **steganography challenges** in IoT environments

---

# Task 6: End-to-End System Integration

## Slide 1: System Architecture
- ESP32 handles:
  - **LED timing (Task 1)**
  - **Priority-based MQTT handling (Task 2 & 3)**
  - **Steganography decoding (Task 4 & 5)**
- Uses:
  - FreeRTOS tasks with priorities
  - Queues and semaphores for synchronization
  - MQTT for remote communication
  - JSON for structured data

## Slide 2: Workflow Summary
1. MQTT receives data/images
2. Tasks process according to priority
3. Background averages & logs data
4. Distress handler reacts instantly
5. LED patterns updated dynamically
6. Hidden messages decoded and followed
- Ensures **robust, responsive, and synchronized embedded system**

---

# Notes:
- All code examples are **thread-safe**
- Tasks use **FreeRTOS scheduling** for concurrency
- MQTT topics must match configuration in `config.h`
- Images are saved locally for **offline LSB analysis**
