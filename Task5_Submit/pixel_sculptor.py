#!/usr/bin/env python3
"""
🎨 Task 5: The Pixel Sculptor - GOATED Edition
Rearrange reef pixels using Optimal Transport

Features:
- Non-blocking HTTP fetch with asyncio
- Proper MQTT subscription for source image
- Multiple OT algorithms (blockwise, histogram, hungarian)
- Side-by-side visualization
- Progress logging with timestamps
"""

import argparse
import asyncio
import base64
import io
import json
import logging
import sys
import threading
import time
from datetime import datetime
from pathlib import Path

import numpy as np
from PIL import Image
from skimage.metrics import structural_similarity as ssim
import paho.mqtt.client as mqtt

# Import our OT module
from optimal_transport import apply_transport_plan

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S'
)
logger = logging.getLogger(__name__)

# Constants
BROKER = "broker.hivemq.com"
PORT = 1883
SOURCE_TOPIC = "coralcrib/img"
DEFAULT_TEAM_ID = "aniruddhyembedded"


class MQTTImageClient:
    """Non-blocking MQTT client for image subscription"""
    
    def __init__(self, broker=BROKER, port=PORT):
        self.broker = broker
        self.port = port
        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.source_image = None
        self.image_received = threading.Event()
        
        # Set up callbacks
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self.client.on_disconnect = self._on_disconnect
    
    def _on_connect(self, client, userdata, flags, reason_code, properties):
        logger.info(f"📡 Connected to MQTT broker: {self.broker}")
        client.subscribe(SOURCE_TOPIC)
        logger.info(f"📥 Subscribed to: {SOURCE_TOPIC}")
    
    def _on_message(self, client, userdata, msg):
        logger.info(f"📨 Received message on {msg.topic} ({len(msg.payload)} bytes)")
        try:
            # Try JSON format first
            try:
                data = json.loads(msg.payload.decode())
                if isinstance(data, dict) and 'image' in data:
                    img_data = base64.b64decode(data['image'])
                elif isinstance(data, dict) and 'img' in data:
                    img_data = base64.b64decode(data['img'])
                else:
                    img_data = base64.b64decode(msg.payload)
            except (json.JSONDecodeError, UnicodeDecodeError):
                # Raw base64
                img_data = base64.b64decode(msg.payload)
            
            self.source_image = Image.open(io.BytesIO(img_data))
            logger.info(f"✅ Source image loaded: {self.source_image.size}")
            self.image_received.set()
        except Exception as e:
            logger.error(f"❌ Failed to decode image: {e}")
    
    def _on_disconnect(self, client, userdata, flags, reason_code, properties):
        logger.warning(f"⚠️ Disconnected from broker (reason: {reason_code})")
    
    def start(self):
        """Start MQTT client in background thread"""
        self.client.connect(self.broker, self.port, 60)
        self.client.loop_start()
    
    def stop(self):
        """Stop MQTT client"""
        self.client.loop_stop()
        self.client.disconnect()
    
    def wait_for_image(self, timeout=30):
        """Wait for source image with timeout"""
        logger.info(f"⏳ Waiting for source image (timeout: {timeout}s)...")
        if self.image_received.wait(timeout):
            return self.source_image
        logger.error("❌ Timeout waiting for source image")
        return None
    
    def publish_result(self, team_id, transformed_image):
        """Publish transformed image to team topic"""
        try:
            buffer = io.BytesIO()
            transformed_image.save(buffer, format="PNG")
            img_b64 = base64.b64encode(buffer.getvalue()).decode()
            
            msg = json.dumps({"transformed_image": img_b64})
            result = self.client.publish(team_id, msg)
            
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                logger.info(f"✅ Published to {team_id} ({len(msg)} bytes)")
                return True
            else:
                logger.error(f"❌ Publish failed: {result.rc}")
                return False
        except Exception as e:
            logger.error(f"❌ Publish error: {e}")
            return False


async def fetch_target_image(url, timeout=10, retries=3):
    """Non-blocking HTTP fetch for target image"""
    import aiohttp
    
    for attempt in range(retries):
        try:
            logger.info(f"🌐 Fetching target image (attempt {attempt + 1}/{retries})...")
            
            async with aiohttp.ClientSession() as session:
                async with session.get(url, timeout=aiohttp.ClientTimeout(total=timeout)) as response:
                    if response.status == 200:
                        data = await response.read()
                        image = Image.open(io.BytesIO(data))
                        logger.info(f"✅ Target image fetched: {image.size}")
                        return image
                    else:
                        logger.error(f"❌ HTTP {response.status}")
        except asyncio.TimeoutError:
            logger.warning(f"⏱️ Timeout on attempt {attempt + 1}")
        except Exception as e:
            logger.error(f"❌ Fetch error: {e}")
        
        if attempt < retries - 1:
            await asyncio.sleep(1)
    
    return None


def fetch_target_image_sync(url, timeout=10, retries=3):
    """Synchronous fallback for HTTP fetch"""
    import requests
    
    for attempt in range(retries):
        try:
            logger.info(f"🌐 Fetching target image (attempt {attempt + 1}/{retries})...")
            response = requests.get(url, timeout=timeout)
            if response.status_code == 200:
                image = Image.open(io.BytesIO(response.content))
                logger.info(f"✅ Target image fetched: {image.size}")
                return image
            else:
                logger.error(f"❌ HTTP {response.status_code}")
        except Exception as e:
            logger.error(f"❌ Fetch error: {e}")
        
        if attempt < retries - 1:
            time.sleep(1)
    
    return None


def compute_ssim(img1, img2):
    """Compute SSIM between two images"""
    # Convert to grayscale for SSIM
    arr1 = np.array(img1.convert('L'))
    arr2 = np.array(img2.convert('L'))
    
    # Resize if needed
    if arr1.shape != arr2.shape:
        arr2_resized = np.array(Image.fromarray(arr2).resize((arr1.shape[1], arr1.shape[0])))
        arr2 = arr2_resized
    
    score = ssim(arr1, arr2)
    return score


def create_comparison_image(source, transformed, target):
    """Create side-by-side comparison image"""
    # Ensure all same size
    h, w = target.size[1], target.size[0]
    
    source_resized = source.resize((w, h))
    transformed_resized = transformed.resize((w, h)) if transformed.size != (w, h) else transformed
    
    # Create canvas
    comparison = Image.new('RGB', (w * 3 + 20, h + 40), color=(30, 30, 30))
    
    # Add images
    comparison.paste(source_resized, (0, 30))
    comparison.paste(transformed_resized, (w + 10, 30))
    comparison.paste(target, (w * 2 + 20, 30))
    
    return comparison


def load_local_image(path):
    """Load image from local file"""
    try:
        img = Image.open(path)
        logger.info(f"✅ Loaded local image: {path} ({img.size})")
        return img
    except Exception as e:
        logger.error(f"❌ Failed to load {path}: {e}")
        return None


async def main_async(args):
    """Main async workflow"""
    output_dir = Path(args.output_dir)
    output_dir.mkdir(exist_ok=True)
    
    mqtt_client = None
    
    try:
        # Phase 1: Fetch target image
        logger.info("=" * 50)
        logger.info("🎨 PHASE 1: Fetching Target Image")
        logger.info("=" * 50)
        
        if args.target_url:
            try:
                target = await fetch_target_image(args.target_url)
            except Exception:
                # Fallback to sync if aiohttp not available
                target = fetch_target_image_sync(args.target_url)
        elif args.target_path:
            target = load_local_image(args.target_path)
        else:
            logger.error("❌ No target specified! Use --target-url or --target-path")
            return
        
        if target is None:
            logger.error("❌ Failed to load target image")
            return
        
        target.save(output_dir / "target_image_original.png")
        
        # Enforce 128x64 resolution as per requirements
        target = target.resize((128, 64), Image.LANCZOS)
        logger.info(f"✅ Target image resized to enforce requirements: {target.size}")
        
        target.save(output_dir / "target_image.png")
        
        # Phase 2: Load source image
        logger.info("=" * 50)
        logger.info("🎨 PHASE 2: Loading Source Image")
        logger.info("=" * 50)
        
        if args.source_mqtt:
            mqtt_client = MQTTImageClient()
            mqtt_client.start()
            source = mqtt_client.wait_for_image(timeout=args.mqtt_timeout)
            if source is None:
                logger.error("❌ Failed to receive source from MQTT")
                return
        elif args.source_path:
            source = load_local_image(args.source_path)
        else:
            logger.error("❌ No source specified! Use --source-mqtt or --source-path")
            return
        
        if source is None:
            logger.error("❌ Failed to load source image")
            return
        
        source.save(output_dir / "source_image.png")
        
        # Phase 3 & 4: Compute and apply optimal transport
        logger.info("=" * 50)
        logger.info(f"🎨 PHASE 3 & 4: Optimal Transport ({args.method})")
        logger.info("=" * 50)
        
        start_time = time.time()
        transformed_arr = apply_transport_plan(source, target, method=args.method)
        transformed = Image.fromarray(transformed_arr.astype(np.uint8))
        elapsed = time.time() - start_time
        
        logger.info(f"⏱️ Transport computed in {elapsed:.2f}s")
        transformed.save(output_dir / "transformed_image.png")
        
        # Phase 5: Validate with SSIM
        logger.info("=" * 50)
        logger.info("🎨 PHASE 5: SSIM Validation")
        logger.info("=" * 50)
        
        score = compute_ssim(transformed, target)
        
        if score >= 0.75:
            logger.info(f"🌟 SSIM Score: {score:.4f} (EXCELLENT!)")
        elif score >= 0.70:
            logger.info(f"✅ SSIM Score: {score:.4f} (Acceptable)")
        else:
            logger.warning(f"⚠️ SSIM Score: {score:.4f} (Below threshold, try different method)")
        
        # Create comparison image
        comparison = create_comparison_image(source, transformed, target)
        comparison.save(output_dir / "comparison.png")
        logger.info(f"📸 Saved comparison to {output_dir / 'comparison.png'}")
        
        # Phase 6: Publish result
        if score >= 0.70 and args.publish:
            logger.info("=" * 50)
            logger.info("🎨 PHASE 6: Publishing Result")
            logger.info("=" * 50)
            
            if mqtt_client is None:
                mqtt_client = MQTTImageClient()
                mqtt_client.start()
                time.sleep(1)  # Wait for connection
            
            if mqtt_client.publish_result(args.team_id, transformed):
                logger.info("🎉 SUCCESS! Awaiting sequencer code from broker...")
            else:
                logger.error("❌ Failed to publish result")
        elif score < 0.70:
            logger.warning("⚠️ SSIM too low, not publishing. Try --method histogram or blockwise")
        
        # Summary
        logger.info("=" * 50)
        logger.info("📊 SUMMARY")
        logger.info("=" * 50)
        logger.info(f"   Source: {source.size}")
        logger.info(f"   Target: {target.size}")
        logger.info(f"   Method: {args.method}")
        logger.info(f"   SSIM:   {score:.4f}")
        logger.info(f"   Time:   {elapsed:.2f}s")
        logger.info(f"   Output: {output_dir.absolute()}")
        logger.info("=" * 50)
        
    finally:
        if mqtt_client:
            mqtt_client.stop()


def main():
    parser = argparse.ArgumentParser(
        description="🎨 Pixel Sculptor - Optimal Transport Image Transformation",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Using local images
  python pixel_sculptor.py --target-path target.png --source-path source.png
  
  # Using URL for target
  python pixel_sculptor.py --target-url "http://example.com/target.png" --source-path source.png
  
  # Fetch source from MQTT
  python pixel_sculptor.py --target-path target.png --source-mqtt
  
  # Full run with publish
  python pixel_sculptor.py --target-url "URL" --source-mqtt --publish --team-id myteam
        """
    )
    
    # Target options
    target_group = parser.add_mutually_exclusive_group(required=True)
    target_group.add_argument('--target-url', help='URL to fetch target image from')
    target_group.add_argument('--target-path', help='Local path to target image')
    
    # Source options
    source_group = parser.add_mutually_exclusive_group(required=True)
    source_group.add_argument('--source-mqtt', action='store_true', 
                              help=f'Subscribe to {SOURCE_TOPIC} for source image')
    source_group.add_argument('--source-path', help='Local path to source image')
    
    # OT options
    parser.add_argument('--method', choices=['blockwise', 'histogram', 'hungarian'],
                        default='blockwise', help='Optimal transport method (default: blockwise)')
    
    # MQTT options
    parser.add_argument('--team-id', default=DEFAULT_TEAM_ID, help='Team ID for publishing')
    parser.add_argument('--publish', action='store_true', help='Publish result to MQTT')
    parser.add_argument('--mqtt-timeout', type=int, default=30, 
                        help='Timeout for MQTT source image (seconds)')
    
    # Output options
    parser.add_argument('--output-dir', default='.', help='Output directory for images')
    
    args = parser.parse_args()
    
    logger.info("🦐 Pixel Sculptor - Task 5 GOATED Edition")
    logger.info(f"   Team ID: {args.team_id}")
    logger.info(f"   Method:  {args.method}")
    
    try:
        asyncio.run(main_async(args))
    except KeyboardInterrupt:
        logger.info("⚠️ Interrupted by user")
    except Exception as e:
        logger.error(f"❌ Fatal error: {e}")
        raise


if __name__ == "__main__":
    main()
