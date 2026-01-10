"""
Alternative LSB Extraction Script
Try different methods to extract hidden message from the image
"""

from PIL import Image
import os

# Load the received image
img_path = "received_image.png"
if not os.path.exists(img_path):
    print("Error: received_image.png not found")
    exit(1)

image = Image.open(img_path)
print(f"Image size: {image.size}")
print(f"Image mode: {image.mode}")

# Try to convert to RGB if needed
if image.mode != 'RGB':
    image = image.convert('RGB')

pixels = list(image.getdata())
print(f"Total pixels: {len(pixels)}")

# ============================================
# Method 1: Standard LSB (Red channel only)
# ============================================
print("\n=== Method 1: Red Channel LSB ===")
bits = []
for pixel in pixels[:10000]:  # First 10k pixels
    bits.append(pixel[0] & 1)

message = ""
for i in range(0, min(len(bits) - 7, 1000), 8):
    byte_val = 0
    for j in range(8):
        byte_val = (byte_val << 1) | bits[i + j]
    if 32 <= byte_val <= 126:
        message += chr(byte_val)
    elif byte_val == 0:
        break

print(f"Extracted: {message[:100]}...")

# ============================================
# Method 2: All RGB channels LSB
# ============================================
print("\n=== Method 2: RGB LSB ===")
bits = []
for pixel in pixels[:10000]:
    for c in range(3):
        bits.append(pixel[c] & 1)

message = ""
for i in range(0, min(len(bits) - 7, 1000), 8):
    byte_val = 0
    for j in range(8):
        byte_val = (byte_val << 1) | bits[i + j]
    if 32 <= byte_val <= 126:
        message += chr(byte_val)
    elif byte_val == 0:
        break

print(f"Extracted: {message[:100]}...")

# ============================================
# Method 3: Look for text patterns in LSB
# ============================================
print("\n=== Method 3: Full scan for readable text ===")
bits = []
for pixel in pixels:
    for c in range(3):
        bits.append(pixel[c] & 1)

# Try to find readable ASCII sequences
best_message = ""
for start in range(0, min(1000, len(bits)), 8):
    message = ""
    valid_chars = 0
    for i in range(start, min(start + 2000, len(bits) - 7), 8):
        byte_val = 0
        for j in range(8):
            byte_val = (byte_val << 1) | bits[i + j]
        if 32 <= byte_val <= 126:
            message += chr(byte_val)
            valid_chars += 1
        elif byte_val == 0:
            break
        else:
            break
    if valid_chars > len(best_message):
        best_message = message

print(f"Best readable: {best_message[:100]}...")

# ============================================
# Method 4: Try LSB from the END of image
# ============================================
print("\n=== Method 4: End of image ===")
bits = []
for pixel in pixels[-10000:]:
    for c in range(3):
        bits.append(pixel[c] & 1)

message = ""
for i in range(0, min(len(bits) - 7, 1000), 8):
    byte_val = 0
    for j in range(8):
        byte_val = (byte_val << 1) | bits[i + j]
    if 32 <= byte_val <= 126:
        message += chr(byte_val)
    elif byte_val == 0:
        break

print(f"Extracted: {message[:100]}...")

# ============================================
# Method 5: Check image metadata
# ============================================
print("\n=== Method 5: Image metadata ===")
info = image.info
print(f"Metadata keys: {list(info.keys())}")
for key, val in info.items():
    if isinstance(val, (str, bytes)):
        print(f"  {key}: {str(val)[:100]}")

# ============================================
# Method 6: Look at first 64x64 region
# ============================================
print("\n=== Method 6: First 64x64 pixels ===")
bits = []
for y in range(64):
    for x in range(64):
        pixel = image.getpixel((x, y))
        for c in range(3):
            bits.append(pixel[c] & 1)

message = ""
for i in range(0, min(len(bits) - 7, 500), 8):
    byte_val = 0
    for j in range(8):
        byte_val = (byte_val << 1) | bits[i + j]
    if 32 <= byte_val <= 126:
        message += chr(byte_val)
    elif byte_val == 0:
        break

print(f"Extracted: {message[:100]}...")

print("\n=== Done ===")
print("Check the image visually: eog received_image.png")
