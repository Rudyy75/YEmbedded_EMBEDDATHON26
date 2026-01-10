"""
Task 4: The Silent Image - Steganography Decoder
"Not everything worth knowing announces itself."

This script:
1. Connects to MQTT broker
2. Subscribes to challenge_code topic (lkjhgf) to receive image
3. Publishes request to kelpsaute/steganography
4. Receives base64 encoded PNG image
5. Extracts hidden message using LSB steganography
6. Follows the direction from the hidden message

Usage:
    python steg_decoder.py
    
Requirements:
    pip install paho-mqtt Pillow
"""

import paho.mqtt.client as mqtt
import base64
import json
from PIL import Image
import io
import time
import sys

# ============================================
# CONFIGURATION
# ============================================
TEAM_ID = "aniruddhyembedded"
BROKER = "broker.mqttdashboard.com"
PORT = 1883

# Topics
CHALLENGE_CODE = "lkjhgf"  # From Task 2 (response will come here)
REQUEST_TOPIC = "kelpsaute/steganography"

# The "hidden message from Task 3"
HIDDEN_MESSAGE = "REEFING KRILLS :( CORALS BLOOM <3"

# Track state
image_received = False
hidden_message_extracted = ""
next_topic = ""

# ============================================
# LSB STEGANOGRAPHY EXTRACTION
# ============================================
def extract_lsb_message(image):
    """
    Extract hidden message from LSB of image pixels.
    
    LSB Steganography hides data in the least significant bit of each color channel.
    We read the LSB of each RGB channel and assemble them into bytes.
    """
    print("\n" + "="*50)
    print("PHASE 3: LSB EXTRACTION")
    print("="*50)
    
    pixels = list(image.getdata())
    print(f"[LSB] Total pixels: {len(pixels)}")
    print(f"[LSB] Image mode: {image.mode}")
    
    bits = []
    
    # Extract LSB from each RGB channel
    for i, pixel in enumerate(pixels):
        # Handle different image modes
        if isinstance(pixel, int):
            # Grayscale
            bits.append(pixel & 1)
        else:
            # RGB or RGBA
            for channel in pixel[:3]:  # Only RGB, ignore Alpha
                bits.append(channel & 1)
        
        # Show first few pixels for debugging
        if i < 5:
            if isinstance(pixel, int):
                print(f"[LSB] Pixel {i}: Gray={pixel}, LSB={pixel & 1}")
            else:
                lsbs = [c & 1 for c in pixel[:3]]
                print(f"[LSB] Pixel {i}: RGB={pixel[:3]}, LSBs={lsbs}")
    
    print(f"[LSB] Total bits extracted: {len(bits)}")
    
    # Convert bits to characters
    message = ""
    for i in range(0, len(bits) - 7, 8):
        byte_val = 0
        for j in range(8):
            byte_val = (byte_val << 1) | bits[i + j]
        
        # Stop at null terminator
        if byte_val == 0:
            print(f"[LSB] Null terminator found at bit position {i}")
            break
        
        # Only accept printable ASCII
        if 32 <= byte_val <= 126 or byte_val in (10, 13):
            message += chr(byte_val)
        else:
            # Non-printable character might signal end
            if len(message) > 5:  # We have some message already
                break
    
    print(f"\n[LSB] Extracted {len(message)} characters")
    print("="*50)
    print(f"HIDDEN MESSAGE: {message}")
    print("="*50)
    
    return message

# ============================================
# MQTT CALLBACKS
# ============================================
def on_connect(client, userdata, flags, reason_code, properties):
    """Called when connected to MQTT broker"""
    if reason_code == 0:
        print("\n" + "="*50)
        print("CONNECTED TO MQTT BROKER")
        print("="*50)
        
        # Subscribe to challenge code topic (where image response comes)
        client.subscribe(CHALLENGE_CODE)
        print(f"[MQTT] Subscribed to: {CHALLENGE_CODE}")
        
        # Subscribe to team topic (backup)
        client.subscribe(TEAM_ID)
        print(f"[MQTT] Subscribed to: {TEAM_ID}")
        
        # Subscribe to wildcard topics
        client.subscribe(f"{CHALLENGE_CODE}/#")
        print(f"[MQTT] Subscribed to: {CHALLENGE_CODE}/#")
        
        # Try kelpsaute wildcard (steganography namespace)
        client.subscribe("kelpsaute/#")
        print(f"[MQTT] Subscribed to: kelpsaute/#")
        
        # Try reef related topics
        client.subscribe("reef/#")
        print(f"[MQTT] Subscribed to: reef/#")
        
        # Try window topic without _window suffix
        client.subscribe("lkjhgf_window")
        print(f"[MQTT] Subscribed to: lkjhgf_window")
        
        # Wait a moment then send request
        time.sleep(1)
        
        # Phase 1: Signal the Reef
        print("\n" + "="*50)
        print("PHASE 1: SIGNALING THE REEF")
        print("="*50)
        
        request = {
            "request": HIDDEN_MESSAGE,
            "agent_id": TEAM_ID
        }
        
        print(f"[TX] Topic: {REQUEST_TOPIC}")
        print(f"[TX] Message: {json.dumps(request)}")
        
        client.publish(REQUEST_TOPIC, json.dumps(request))
        print("[TX] Request sent! Waiting for image response...")
        
    else:
        print(f"[ERROR] Connection failed with code: {reason_code}")

def on_message(client, userdata, msg):
    """Called when a message is received"""
    global image_received, hidden_message_extracted, next_topic
    
    topic = msg.topic
    payload = msg.payload.decode('utf-8', errors='replace')
    
    # Filter out spam (old Task 2 message)
    if "HOT STREAK" in payload or "lkjhgf_window" in payload:
        return  # Skip this spam
    
    print(f"\n[RX] Topic: {topic}")
    print(f"[RX] Payload length: {len(payload)} bytes")
    
    # Show first 200 chars of payload
    preview = payload[:200] + "..." if len(payload) > 200 else payload
    print(f"[RX] Preview: {preview}")
    
    try:
        data = json.loads(payload)
        
        # Check if this is the image response
        if "data" in data:
            print("\n" + "="*50)
            print("PHASE 2: IMAGE RECEIVED!")
            print("="*50)
            
            image_received = True
            
            # Get image info
            img_type = data.get("type", "unknown")
            width = data.get("width", "?")
            height = data.get("height", "?")
            
            print(f"[IMG] Type: {img_type}")
            print(f"[IMG] Dimensions: {width}x{height}")
            
            # Decode base64 image
            print("[IMG] Decoding base64 data...")
            img_data = base64.b64decode(data["data"])
            print(f"[IMG] Decoded size: {len(img_data)} bytes")
            
            # Open and save image
            image = Image.open(io.BytesIO(img_data))
            image.save("received_image.png")
            print(f"[IMG] Saved to: received_image.png")
            print(f"[IMG] Actual dimensions: {image.size}")
            print(f"[IMG] Mode: {image.mode}")
            
            # Phase 3: Extract hidden message
            hidden_message_extracted = extract_lsb_message(image)
            
            # Save extracted message
            with open("extracted_message.txt", "w") as f:
                f.write(hidden_message_extracted)
            print(f"[LSB] Saved to: extracted_message.txt")
            
            # Try to parse as topic/direction
            if hidden_message_extracted:
                print("\n" + "="*50)
                print("PHASE 4: FOLLOWING THE DIRECTION")
                print("="*50)
                print(f"[NEXT] The hidden message might be a topic to subscribe to")
                print(f"[NEXT] Message: {hidden_message_extracted}")
                
                # Subscribe to the hidden message as a topic
                next_topic = hidden_message_extracted.strip()
                client.subscribe(next_topic)
                print(f"[NEXT] Subscribed to: {next_topic}")
                
                # Try publishing to it to trigger response
                ack = {"status": "received", "message": hidden_message_extracted, "agent_id": TEAM_ID}
                client.publish(next_topic, json.dumps(ack))
                print(f"[NEXT] Published acknowledgment to: {next_topic}")
        
        # Check if this is the target_image_url response
        if "target_image_url" in data:
            print("\n" + "="*50)
            print("SUCCESS! TARGET IMAGE URL RECEIVED!")
            print("="*50)
            print(f"URL: {data['target_image_url']}")
            print("="*50)
            print("\nDO NOT RETRIEVE THIS YET - it's for Task 5!")
            
            # Save URL
            with open("target_image_url.txt", "w") as f:
                f.write(data['target_image_url'])
            print("[SAVED] target_image_url.txt")
            
    except json.JSONDecodeError:
        # Not JSON - might be plain text message
        print(f"[RX] Non-JSON message: {payload}")
    except Exception as e:
        print(f"[ERROR] {type(e).__name__}: {e}")

def on_disconnect(client, userdata, flags, reason_code, properties):
    """Called when disconnected"""
    print(f"[MQTT] Disconnected with code: {reason_code}")

# ============================================
# MAIN
# ============================================
def main():
    print("\n" + "="*60)
    print("  TASK 4: THE SILENT IMAGE - Steganography Decoder")
    print("  \"Not everything worth knowing announces itself.\"")
    print("="*60)
    
    print(f"\n[CONFIG] Team ID: {TEAM_ID}")
    print(f"[CONFIG] Broker: {BROKER}:{PORT}")
    print(f"[CONFIG] Challenge Code: {CHALLENGE_CODE}")
    print(f"[CONFIG] Request Topic: {REQUEST_TOPIC}")
    print(f"[CONFIG] Hidden Message: {HIDDEN_MESSAGE}")
    
    # Create MQTT client (v2 API)
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"{TEAM_ID}_steg_{int(time.time())}")
    
    # Set callbacks
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    
    # Connect
    print(f"\n[MQTT] Connecting to {BROKER}...")
    try:
        client.connect(BROKER, PORT, keepalive=60)
    except Exception as e:
        print(f"[ERROR] Failed to connect: {e}")
        sys.exit(1)
    
    # Run forever (Ctrl+C to stop)
    print("[MQTT] Starting message loop (Ctrl+C to stop)...")
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("\n[EXIT] Stopping...")
        client.disconnect()

if __name__ == "__main__":
    main()
