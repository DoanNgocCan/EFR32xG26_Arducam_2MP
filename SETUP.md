# Quick Start Guide

## For Recruiters / Code Reviewers

This is a **real-time face classification system** running on embedded microcontroller without cloud connectivity.

### Complete Workflow

```
PART 1: MODEL TRAINING (Google Colab)
  Dataset (face/noface images)
       ↓
  train_model.ipynb (Jupyter notebook)
       ↓
  MobileNet v1 (α=0.25) training
       ↓
  INT8 Quantization (301.6 KB)
       ↓
  train_model.tflite (Quantized model)

PART 2: EMBEDDED INFERENCE (EFR32 Hardware)
  Arducam Camera (112×112 RGB)
       ↓
  EFR32MG26 Microcontroller
       ├─ RGB565 → Grayscale conversion
       ├─ TensorFlow Lite Micro inference (7.2 fps)
       └─ MVP accelerator (7.3M cycles)
       ↓
  IF confidence > 60% THEN:
       ↓
  USB VCOM Streaming (921600 baud)

PART 3: PC-SIDE PROCESSING (Python)
  script_python_for_test.py
       ├─ Receive RGB565 stream
       ├─ Save binary files
       └─ Convert to PNG (Pillow)
       ↓
  data_rgb565/image_*.rgb565
  image_rgb888/image_*.png (viewable)
```

### Model Training on Google Colab

1. **Open Training Notebook**
   ```bash
   # Upload face_classification_model/train_model.ipynb to Google Colab
   # Or run directly with Colab's GPU/TPU
   ```

2. **Training Configuration**
   - Model: MobileNet v1 (α=0.25) - efficient for embedded
   - Input: 112×112×1 grayscale
   - Output: 2 classes (face / no-face)
   - Quantization: INT8 (reduces model size 4×)
   - Epochs: 50, Batch size: 64
   - Data augmentation: rotation, shift, zoom, flip

3. **Output Model**
   - `train_model.tflite` (301.6 KB, INT8 quantized)
   - Accuracy: 92.64% overall, 85.80% face detection, 99.80% no-face
   - ROC AUC: 0.9845

4. **Download & Deploy**
   ```bash
   # Download: train_model.tflite
   # Replace in: aiml_2.1.0/src/
   # Rebuild firmware
   ```

## Project Setup

### 1. Hardware Requirements
- Silicon Labs EFR32MG26B510F3200 (or compatible EFR32xG26 board)
- Arducam mini 2MP camera module  
- USB cable (for VCOM/JLink connection)

### 2. Development Environment

**Option A: Using Simplicity Studio 5 (Recommended)**
```bash
# Download from: https://www.silabs.com/developers/simplicity-studio
# Install with EFR32 SDK support
# Open: camera_arducam_2.slcp
# Build → Build Project
# Run → Flash
```

**Option B: Using Command Line**
```bash
# Install ARM GCC toolchain
# Install J-Link tools
# Compile:
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb ... -o camera_arducam_2.elf
# Flash:
JLinkExe -device EFR32MG26B510F -speed 4000 -if SWD -autoconnect 1
```

### 3. Python Environment (PC-Side)

```bash
# Install Python 3.7+
# Create virtual environment
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate

# Install dependencies
pip install -r requirements.txt
```

## Running the System

### Step 1: Build & Flash Firmware
```bash
# In Simplicity Studio 5:
# 1. Right-click project → Simplicity → Generate
# 2. Build → Build Project
# 3. Run → Flash
# 4. Should see in terminal: "=== System Initialized (VCOM Image Streamer v7) ==="
```

### Step 2: Start Python Receiver
```bash
# Open terminal/command prompt
cd /path/to/camera_arducam_2
python script_python_for_test.py

# Output:
# Đã tạo thư mục: data_rgb565
# Đã tạo thư mục: image_rgb888
# [HH:MM:SS] Đang tìm kiếm gói tin...  (Searching for packets...)
```

### Step 3: Point Camera at Face
- The device continuously captures images
- When a face is detected with >60% confidence:
  - LED/indicator may turn on
  - Image is streamed to PC
  - Python script saves: `data_rgb565/image_<timestamp>.rgb565`
  - PNG preview saved: `image_rgb888/image_<timestamp>.png`

### Step 4: View Results
```bash
# Captured PNG images are in:
# image_rgb888/image_*.png

# View in any image viewer or:
# python rgb565_to_rgb888.py data_rgb565/image_1234567890.rgb565
```

## Key Code Files to Review

| File | What It Does |
|------|--------------|
| **image_classifier.cc** | ML model inference + color conversion (⭐ Core logic) |
| **arducam/arducam.c** | Camera driver high-level API |
| **script_python_for_test.py** | PC-side image receiver (handles serial protocol) |
| **config/pin_config.h** | GPIO pin definitions |

## Architecture Overview

### Firmware Flow (Embedded)
```
SETUP: Initialize GPIO, I2C, SPI, UART, TF Lite model
  ↓
LOOP: 
  1. Poll camera for new frame (RGB565, 112×112)
  2. Convert RGB565 → Grayscale (weighted formula)
  3. Run TF Lite inference
  4. Apply softmax activation
  5. If confidence[target_class] > 60%:
     - Send SOP marker (0xDEADBEEF)
     - Stream raw RGB565 to PC
  6. Repeat
```

### PC Flow (Python)
```
SETUP: Open COM port (921600 baud), create data directories
  ↓
LOOP:
  1. Wait for SOP marker from device
  2. Read image size header (4 bytes)
  3. Receive image data in 25KB chunks
  4. Send ACK after each chunk
  5. Save binary RGB565 file
  6. Convert RGB565 → PNG for viewing
  7. Repeat
```

## Configuration

### To Change Detection Threshold
Edit `image_classifier.cc` line ~40:
```cpp
#define CONFIDENCE_THRESHOLD 60.0f  // Change this value (0-100)
```
Then rebuild and flash.

### To Use Different ML Model
1. Replace model in `aiml_2.1.0/src/`
2. Update `sl_tflite_micro_model.cc/h` with new model
3. Adjust `REQUIRED_CLASS_INDEX` if needed
4. Update class label count if different
5. Rebuild and flash

## Troubleshooting

### Device not showing up as COM port
```bash
# Check device manager for "Silicon Labs" COM port
# If not found:
# 1. Install JLink drivers: https://www.segger.com/
# 2. Reseat USB connection
# 3. Try different USB port
```

### Python script says "No data received"
```bash
# Check:
# 1. Device is powered and flashed
# 2. Correct COM port in script_python_for_test.py
# 3. Device is actually detecting faces (check brightness/angle)
# 4. Confidence threshold is reachable
```

### Build errors in Simplicity Studio
```bash
# Try:
# 1. Right-click → Simplicity → Manage Components (regenerate)
# 2. Clean → Clean Build
# 3. Close and reopen project
```

## Performance Metrics

| Metric | Value |
|--------|-------|
| Capture Rate | 10-15 FPS |
| Inference Time | 50-100 ms |
| Streaming Speed | 25KB/chunk @ 921600 baud |
| Total Latency | ~150-200 ms |
| Model Size | Varies by model (~100-500 KB) |

## What This Demonstrates

✅ **Embedded ML** - On-device inference without GPU/cloud  
✅ **Real-time Systems** - <200ms latency end-to-end  
✅ **Hardware Integration** - I2C, SPI, UART protocols  
✅ **Optimization** - Efficient color conversion, buffer management  
✅ **Debugging** - Serial streaming, image logging  
✅ **Protocol Design** - Custom binary protocol with handshaking  

## For Production Use

To make this production-ready:
- Add power management / sleep modes
- Implement edge triggering (not continuous streaming)
- Add SD card logging for offline analysis
- Implement model versioning/OTA updates
- Add thermal management
- Implement redundancy/error correction

## Questions?

See `readme.md` for detailed documentation and references.

---

**Last Updated**: May 2025  
**Difficulty Level**: Intermediate-Advanced  
**Estimated Time to Run**: 30 minutes (with hardware ready)
