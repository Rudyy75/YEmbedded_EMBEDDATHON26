# 🔐 TASK 4: THE SILENT IMAGE
*"Not everything worth knowing announces itself."*

---

## Objective

A response awaits beneath the surface.

You will request an artifact from the reef—one that appears ordinary at first glance. Its purpose is not revealed through sight alone. To move forward, you must learn how to listen to what the image is quietly conveying and uncover a direction hidden within it.

This task marks the first moment where communication and perception intersect.

---

## Problem Statement

### Phase 1: Signal the Reef

To begin, announce your presence to the reef by publishing a message:

- **Topic:** `kelpsaute/steganography`
- **Message:** `{"request": "<hidden message from Task 3>", "agent_id": "<your_team_id>"}`

If your signal is accepted, the reef responds privately with a structured payload:

- **Topic:** `<challenge_code>` (from Task 2)
- **Message:** `{"data": "<image.png>", "type": "png", "width": 64, "height": 64}`

> ⚠️ **Note:** The response is precise. Any alteration or corruption will distort everything that follows.

---

### Phase 2: Reassemble What Was Sent

What you receive is not immediately viewable.

Your task is to:
- Restore the transmitted data into its intended form
- Confirm its shape and structure
- Ensure nothing was lost in translation

Only once it is whole can it be examined further.

---

### Phase 3: Look Beyond Appearance

At first, the artifact reveals nothing unusual.

Yet its purpose is not visual.

The information you seek is embedded in a manner that:
- Survives small variations
- Depends on relationships, not exact values
- Remains invisible unless examined methodically

Those who treat each element in isolation will find nothing.
Those who compare will begin to notice a pattern.

```
For each unit of color:
    observe how its components relate
    record what the relationship implies
```

Assemble the observations.
What emerges is not noise—but language.

> **Hint:** This is LSB (Least Significant Bit) steganography

---

### Phase 4: Follow What You Discover

The assembled message is not the destination, but a signpost.

- Interpret the recovered text
- Identify the direction it points to

If interpreted correctly, the reef responds once more:
```json
{"target_image_url": "https://example.com/target_image.png"}
```

> ⚠️ **Do not retrieve this resource yet.** Its purpose will become clear in the next task.

---

## Hardware Requirements

This task can be done on:
- ESP32 (challenging but possible)
- Development machine (laptop with Python recommended)

---

## Evaluation Criteria

- Correctly initiates communication with the reef
- Receives and restores the transmitted artifact without loss
- Confirms structural integrity of the reconstructed image
- Discovers and interprets the hidden pattern through analysis
- Recovers a meaningful, human-readable message
- Correctly identifies and follows the next direction
- Maintains clear logs demonstrating reasoning and verification

---

## Success Condition

A valid direction is uncovered and acted upon. The reef acknowledges your understanding by responding on the indicated path.

---

## Video Submission Requirements

**Recording must show:**
- Serial logs showing base64 data received
- Image reconstruction logs (dimensions, format verification)
- LSB extraction process with sample bits shown
- Final hidden message revealed in logs
- Next topic address parsed and confirmed

---

## Scoring (40 points)

| Criteria | Weight |
|----------|--------|
| Base64 decoding | Medium |
| Image reconstruction | Medium |
| LSB extraction | High |
| Message parsing | High |

---

## Documentation Required

Provide evidence of:
- Initial signal transmission
- Payload reception and restoration
- The reconstructed artifact
- Intermediate observations that led to discovery
- Final recovered message
- Confirmation of successful progression

---

## Expected Repository Structure

```
Task4_Steganography/
├── steg_decoder.ino
├── lsb_extraction.py (if used)
└── extracted_message.txt
```
