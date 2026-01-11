# 🎨 TASK 5: THE PIXEL SCULPTOR
*"Rearrange the reef's colors to match its ancient blueprint"*

---

## Objective

You've discovered the target image—the reef's spatial blueprint. Now you must take a source image and rearrange its pixels to match the target's structure using Optimal Transport (OT), a mathematical framework for matching probability distributions. This advanced task bridges image processing and optimization, showing how the reef's pixels "flow" from source to target configuration.

---

## Problem Statement

### Phase 1: Fetch Target Image (Non-Blocking Network I/O)

The broker provided a target image URL in Task 4's response.

**Task:** Implement a non-blocking HTTP GET request to download the target image without blocking your MQTT subscriptions or other tasks.

- Use HTTP client library with timeout (e.g., 5 seconds)
- Handle network failures gracefully (retry or skip)
- Parse downloaded image (PNG/JPEG, e.g., 128×64 pixels)
- Store image in memory for processing

---

### Phase 2: Load Source Image

A default source image is available as a bytestream on:

- **Topic:** `coralcrib/img`

Subscribe and receive the base64-encoded source image. Decode and load it into memory.

---

### Phase 3: Compute Optimal Transport Plan

Optimal Transport (OT) finds the most efficient way to remap source pixels to match target spatial distribution.

```
Input:
  - Source image (S): 128×64 pixels with original pixel values
  - Target image (T): 128×64 pixels with spatial structure

Algorithm:
  - Extract pixel values and positions from source
  - Extract pixel values and positions from target
  - Compute transport plan: which source pixel goes to which target position
  - Minimize total "transport cost" (typically Euclidean distance or Wasserstein distance)

Output:
  - Transport plan (P): matrix or sparse representation
  - Maps source pixel positions to target pixel positions
```

**Complexity Note:** Full Optimal Transport computation is CPU-intensive. Brownie points if optimized to run on ESP32. Otherwise, implement a simplified version or approximate algorithm.

---

### Phase 4: Transform Source Image

Apply the computed transport plan to rearrange pixels:

**Result:** Transformed image with source pixel values in target spatial arrangement

> **Critical:** Only spatial position changes; pixel values remain intact. Color information is preserved, structure is transformed.

---

### Phase 5: Validate Transformation Quality

Use Structural Similarity Index (SSIM) or correlation metric to compare transformed image with target:

```
SSIM(Transformed, Target) ≥ 0.70 (minimum acceptable)
SSIM ≥ 0.75 (excellent quality)
```

Log the SSIM score for documentation.

---

### Phase 6: Transmit Final Image

- Convert transformed image to base64 encoding
- Publish to your team's unique topic:
  - **Topic:** `<TeamID>`
  - **Message:** `{"transformed_image": "<base64_string>"}`

---

## Hardware Requirements

**This task can run on:**
- Development machine (laptop with Python, MATLAB, etc.) - **Recommended**
- ESP32 - **Brownie points for optimization**

---

## Evaluation Criteria

- Target image fetched successfully via non-blocking HTTP
- Source image loaded correctly from `coralcrib/img`
- Optimal transport plan computed or approximated correctly
- Transformed image generated without memory overflow or crashes
- Transformed image published as valid base64 with correct formatting
- SSIM score ≥ 0.70 minimum (0.75 excellent)

---

## Success Condition

Broker confirms successful transformation and publishes your sequencer code for Task 6.

**Implementation Note:** The topic `<TeamID>` will no longer function as a challenge topic from Task 2. Instead, it will indefinitely publish the new sequencer topic address for Task 6.

---

## Video Submission Requirements

**Recording must show:**
- Can be on laptop (not required on ESP)
- Side-by-side images: Source → Transformed → Target
- SSIM score logged and visible
- HTTP fetch logs showing target image downloaded
- Base64 encoding of final image
- MQTT publish confirmation

**Brownie Points:** Full optimization to run on ESP32 with real-time performance. Show profiling data and memory usage optimization.

---

## Scoring (60 points)

| Criteria | Weight |
|----------|--------|
| Optimal transport computation | High |
| Transformation quality (SSIM ≥0.70) | High |
| MQTT publish | Medium |
| ESP32 optimization | Brownie points |

---

## Expected Repository Structure

```
Task5_PixelSculptor/
├── pixel_sculptor.py (or .ino)
├── optimal_transport.py
├── source_image.png
├── target_image.png
└── transformed_image.png
```
