# 📺 TASK 6: THE SEQUENCE RENDERER
*"Display the reef's final song—frame by frame, moment by moment"*

---

## Objective

The ultimate test: display the reef's ancient video on a physical OLED display with perfect frame synchronization. The broker streams video frames one at a time, each with a display duration. You must render each frame, acknowledge its display, and maintain smooth playback over multiple loops. This is real-time hardware control at its finest.

---

## Problem Statement

### Phase 1: Subscribe to Sequencer Topic

- **Topic:** `<sequencer_code>` (received from Task 5 completion)
- **Initial Message:**
  ```json
  {"frame_no": 0, "data": "0000"}
  ```
  - `frame_no`: 0 indicates the sequence START
  - `data`: "0000" is a 4-byte start marker (not actual image data)

When you receive frame 0, initialize your OLED display and prepare for the frame stream.

---

### Phase 2: Receive Video Frame Stream

The broker streams frames in sequence, one message per frame:

```json
Message 1: {
  "frame_no": 1,
  "data": "<base64_encoded_128x128_image>",
  "delay_ms": 100
}

Message 2: {
  "frame_no": 2,
  "data": "<base64_encoded_128x128_image>",
  "delay_ms": 100
}

... (frames 3 through N-1) ...

Final Message (Terminator): {
  "frame_no": 255,
  "data": "0000",
  "delay": "10"
}
```

**Stream Behavior:**
- Frames arrive one at a time via MQTT
- Each frame must be displayed for the specified `delay_ms` (e.g., 100 ms per frame)
- Frame order: 1 → 2 → 3 → ... → N → terminator (0000)
- After terminator, sequence loops indefinitely: Jump back to frame 1 (skip frame 0)

---

### Phase 3: Display on OLED

- **Timing Accuracy:** Display each frame for `delay_ms` ±20 ms tolerance
- **Smooth Playback:** Frames should transition smoothly without stuttering or black flashes

---

### Phase 4: Acknowledge Each Frame

After displaying each frame for its full duration, immediately send acknowledgment:

- **Topic:** `<ReefID>`
- **Message:**
  ```json
  {
    "frame_no": <current_frame>,
    "timestamp_ms": <millis_at_ack>,
    "image_base64": "<base64_of_frame_just_displayed>",
    "ack_id": "<agent_id>"
  }
  ```

**Timing:** Acknowledgment should be sent within 10 ms of frame display completion.

---

## Hardware Requirements

| Component | Connection | Purpose |
|-----------|------------|---------|
| OLED Display (128x128 or 128x64) | I2C (SDA, SCL) | Frame display |
| ESP32 | - | Main controller |

**I2C Pins (typical):**
- SDA: GPIO 21
- SCL: GPIO 22

---

## Evaluation Criteria

- OLED display initializes correctly (I2C communication verified)
- Frames received from broker without data corruption
- Each frame displays for the specified duration (±20 ms tolerance)
- Video playback is smooth with no visible stuttering or frame drops
- Acknowledgment sent for every frame (including loops)
- Acknowledgments include correct frame numbers in sequence
- Multiple complete loops executed successfully (at least 3 full sequences)
- No crashes, memory leaks, or watchdog resets during 5+ minutes of operation
- Serial logging shows frame timings and acknowledgment sequence

---

## Success Condition

After all frames are acknowledged and video looped 3+ times successfully, the broker publishes a final congratulation message on the acknowledgment topic, confirming you've decoded the reef's ancient secret.

---

## Video Submission Requirements

Record a video demonstrating:
- **OLED Display:** Clear animation of frames playing in sequence
- **Stopwatch:** Visible in background showing elapsed time (proves real-time synchronization)
- **Serial Monitor:** Shows frame numbers being received, display durations, and acknowledgments being sent
- **Duration:** At least 3 complete loops of the video sequence (minimum 1.5–2 minutes depending on frame count and delays)
- **Quality:** High resolution recording so frames are visible and readable

---

## Logging Example

```
[Task 6] Sequencer initialized, waiting for frame 0...
[Task 6] Frame 0 received (START marker)
[Task 6] Frame 1 received, delay: 100 ms
[OLED] Frame 1 displayed for 100 ms
[MQTT] ACK sent for frame 1 at timestamp 12345
[Task 6] Frame 2 received, delay: 100 ms
[OLED] Frame 2 displayed for 100 ms
[MQTT] ACK sent for frame 2 at timestamp 12445
...
[Task 6] Terminator received, looping back to frame 1...
```

---

## Scoring (60 points)

| Criteria | Weight |
|----------|--------|
| OLED initialization | Medium |
| Frame decoding | Medium |
| Timing accuracy (±20ms) | High |
| Acknowledgments | High |
| Smooth playback | High |

---

## Expected Repository Structure

```
Task6_SequenceRenderer/
├── sequence_renderer.ino
├── oled_driver.h
└── frame_logs.txt
```
