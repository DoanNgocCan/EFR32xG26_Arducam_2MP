# EFR32 Face Classifier with Arducam Camera

Real-time on-device face classification system using **Silicon Labs EFR32MG26B510F3200** microcontroller with **Arducam mini 2MP OV2640** camera module and **TensorFlow Lite Micro**.

## Overview

This project implements an edge AI face classifier that runs inference directly on an embedded microcontroller without requiring cloud connectivity. The system captures images from an Arducam camera, processes them with a pre-trained ML model, and streams results to a PC for logging and visualization.

### Key Features

✨ **On-Device ML Inference** - Uses TensorFlow Lite Micro for efficient model execution  
📸 **Real-Time Image Capture** - 112×112 RGB images from Arducam OV2640 camera  
🚀 **High Performance** - Optimized color space conversion and buffer management  
🔌 **USB Streaming** - Real-time image transmission to PC via VCOM/JLink  
🎯 **Configurable Detection** - Adjustable confidence thresholds and class filtering  
⚡ **Low Power** - Embedded ML inference without external accelerators  

## Hardware Requirements

- **Microcontroller**: Silicon Labs EFR32MG26B510F3200 (32-bit ARM Cortex-M4)
- **Camera**: Arducam mini 2MP (OV2640 sensor) with SPI + I2C interface
- **Board**: BRD2608A development kit
- **Interfaces**: 
  - SPI (camera image data)
  - I2C (camera control)
  - UART EUSART0 (USB VCOM via JLink)

## Software Stack

- **SDK**: Silicon Labs Simplicity SDK 2025.6.0
- **ML Framework**: TensorFlow Lite Micro 2.x
- **Build System**: Simplicity Studio 5 / GCC ARM v12.2.1
- **Python**: 3.7+ (for PC-side image processing)

## Project Structure

```
.
├── image_classifier.cc/h             # Core ML inference engine
├── arducam/                          # Camera driver implementation
│   ├── arducam.c/h                 # High-level camera API
│   └── drivers/m2mp/               # OV2640 driver
├── app.c/h                           # Application entry points
├── main.c                            # Main firmware entry
├── config/                           # Hardware/SDK configuration
├── autogen/                          # Auto-generated SDK components
├── aiml_2.1.0/                      # TensorFlow Lite Micro library
├── cpputils/                         # C++ utility libraries
├── script_python_for_test.py        # PC-side image receiver
├── rgb565_to_rgb888.py              # Image format converter
├── camera_arducam_2.slcp            # Simplicity project file
│
├── 🤖 ML Model Training (Google Colab)
│   └── face_classification_model/   # Model training workspace
│       ├── train_model.ipynb        # Training notebook for Colab
│       ├── train_model.py           # Standalone training script
│       ├── dataset/                 # Training dataset (face/noface)
│       └── train_model_results.mltk/ # Trained model artifacts
│           ├── train_model.tflite   # Quantized model (INT8)
│           ├── train_model.float32.tflite # Full precision model
│           ├── train_model.h5       # Keras model
│           └── eval/                # Evaluation metrics & confusion matrix
│
└── captured_images/                 # Runtime data storage
```

## How It Works

### System Architecture

The complete system consists of three main components:

1. **Arducam Mini 2MP Camera** (OV2640 sensor)
   - 112×112 resolution
   - RGB565 color format (2 bytes/pixel)
   - SPI data interface + I2C control interface

2. **EFR32MG26 Microcontroller** (ARM Cortex-M4)
   - 78 MHz processor clock
   - 96 KB SRAM + 256 KB Flash
   - Integrated Matrix Vector Processor (MVP) for ML acceleration
   - GPIO, I2C, SPI, UART peripherals

3. **TensorFlow Lite Micro ML Model** (Trained MobileNet v1)
   - INT8 quantized model (301.6 KB flash, 146.1 KB RAM)
   - 2 output classes: face / no-face
   - 34 layers, 20.4M operations
   - Inference time: 139.9 ms (~7.2 FPS)

### Firmware Flow (5 Phase Pipeline)

```
PHASE 1: INITIALIZATION
├─ GPIO, I2C, SPI, UART setup
├─ Arducam camera initialization (112×112 resolution)  
├─ TensorFlow Lite Micro model loading from flash
└─ DMA channels configuration for buffer transfers

PHASE 2: IMAGE CAPTURE (Continuous)
├─ Poll Arducam SPI for new frame
├─ Receive raw RGB565 image data (2 bytes/pixel)
└─ Store in ping-pong buffers (12.5 KB each)

PHASE 3: COLOR SPACE CONVERSION & PREPROCESSING
├─ Convert RGB565 → Grayscale (weighted formula)
├─ Normalize to INT8 range [-128, 127]
└─ Prepare input for TensorFlow Lite Micro

PHASE 4: ML INFERENCE (MVP ACCELERATED)
├─ Run TF Lite model inference
├─ 34 layers of depthwise separable convolutions
├─ MVP accelerator: 7.3M cycles vs CPU: 4.9M cycles
└─ Output: 2 class logits (face probability, no-face probability)

PHASE 5: DECISION & STREAMING
├─ Apply softmax activation
├─ IF class[0]='face' AND confidence > 60%:
│  ├─ Send SOP marker (0xDEADBEEF)
│  ├─ Send image size header (4 bytes)
│  └─ Stream RGB565 in 25 KB chunks with ACK handshake
└─ ELSE: Loop back to PHASE 2
```

### Color Space Conversion Algorithm

Optimized RGB565 → Grayscale conversion for embedded systems:

```
INPUT FORMAT: RGB565 (16-bit packed)
  Bit layout: [GGGBBBBB][RRRRRGGG]
  - R (red):   bits 15-11 (5-bit precision)
  - G (green): bits 10-5  (6-bit precision)
  - B (blue):  bits 4-0   (5-bit precision)

EXTRACTION STEP:
  r5 = (rgb565 >> 11) & 0x1F         // Isolate 5-bit red
  g6 = (rgb565 >> 5)  & 0x3F         // Isolate 6-bit green
  b5 = rgb565 & 0x1F                 // Isolate 5-bit blue

8-BIT EXPANSION (Replicate MSBs):
  r8 = (r5 << 3) | (r5 >> 2)         // Scale 5→8 bits
  g8 = (g6 << 2) | (g6 >> 4)         // Scale 6→8 bits
  b8 = (b5 << 3) | (b5 >> 2)         // Scale 5→8 bits

LUMINANCE CALCULATION (ITU-R BT.601):
  Y = (R×77 + G×150 + B×29) >> 8
  ≈ 0.299R + 0.587G + 0.114B         // Standard weighting

OUTPUT: Single-byte grayscale value (0-255)
```

### Model Architecture (MobileNet v1 with α=0.25)

Uses depthwise separable convolutions for efficiency on embedded systems:

```
Layer Stack:
  Input:           1×112×112×1 (INT8 grayscale)
    ↓
  Conv2D (3×3, stride=2):      1×56×56×8
  DepthwiseConv2D:             1×56×56×8
  Conv2D (1×1):                1×56×56×16
    ↓
  [Repeated block: DepthwiseConv2D + Conv2D]
  Progressive resolution reduction: 56→28→14→7
    ↓
  GlobalAveragePooling2D:      1×128
  Dropout (0.2):               1×128
  Dense + Softmax:             1×2 (output logits)
    ↓
  Output: 2 class probabilities (INT8)
```

### Validation Performance (Test Dataset)

| Metric | Face | No-Face | Overall |
|--------|------|---------|---------|
| **Accuracy** | 85.80% | 99.80% | 92.64% |
| **Precision** | 97.3% | 89.5% | - |
| **Recall** | 97.8% | 99.8% | - |
| **ROC AUC** | 0.9845 | 0.9845 | 0.9845 |

Confusion Matrix (INT8 model, test set):
- True Positive (Face): 1,348
- False Positive (Negative→Positive): 223
- False Negative (Positive→Negative): 3
- True Negative (No-Face): 1,498

### PC-Side Processing

The Python script (`script_python_for_test.py`) running on PC:
1. Opens serial connection (921600 baud, VCOM via USB)
2. Waits for SOP marker from device
3. Reads image size header (4 bytes)
4. Receives image data in chunks with ACK handshake
5. Saves raw RGB565 binary file: `data_rgb565/image_<timestamp>.rgb565`
6. Converts and saves as PNG: `image_rgb888/image_<timestamp>.png`

## Configuration

### Camera Settings (in `image_classifier.cc`)

```cpp
#define IMG_WIDTH              112      // Image resolution
#define IMG_HEIGHT             112
#define IMG_DATA_FORMAT_CAMERA ARDUCAM_DATA_FORMAT_RGB565
#define REQUIRED_CLASS_INDEX   0        // Target class (0 = first class)
#define CONFIDENCE_THRESHOLD   60.0f    // Detection threshold in percentage
#define VCOM_CHUNK_SIZE        25088    // DMA chunk size (must match buffer)
```

### Hardware Pins (in `config/pin_config.h`)

- **I2C0**: Camera control (SCL=PC04, SDA=PC05)
- **USART0**: Camera SPI data (CLK=PC11, TX=PC13, RX=PC12, CS=PC10)
- **EUSART0**: VCOM/JLink (TX=PB05, RX=PB06)

## ML Model Training (Google Colab)

### Overview

The face classification model was trained using **Silicon Labs MLTK** (Machine Learning Toolkit) on Google Colab.

### Dataset Structure

```
face_classification_model/dataset/
├── face/                  # Positive class: face images
│   ├── image_001.jpg
│   ├── image_002.jpg
│   └── ... (hundreds of face images)
└── noface/               # Negative class: non-face images
    ├── background_001.jpg
    ├── background_002.jpg
    └── ... (hundreds of non-face/background images)
```

### Training Process

1. **Open in Google Colab**
   ```bash
   # Upload face_classification_model/train_model.ipynb to Colab
   # Mount Google Drive for dataset
   ```

2. **Run Training Notebook**
   ```bash
   # Execute: train_model.ipynb
   # The notebook handles:
   # - Dataset loading and preprocessing
   # - Data augmentation (rotation, shift, zoom, flip)
   # - Model training with MobileNet v1 (α=0.25)
   # - INT8 quantization
   # - Model evaluation and profiling
   ```

3. **Training Configuration** (from `train_model.py`)
   ```python
   # Model Architecture
   - Base: MobileNet (alpha=0.25)
   - Input: 112×112×1 (grayscale)
   - Output: 2 classes (face/noface)
   
   # Training Hyperparameters
   - Epochs: 50
   - Batch size: 64
   - Optimizer: Adam
   - Loss: Categorical crossentropy
   - Class weights: Balanced
   
   # Data Augmentation
   - Rotation: ±15°
   - Width/Height shift: ±10%
   - Zoom: ±10%
   - Horizontal flip: Enabled
   - Validation split: 20%
   
   # Quantization (for embedded deployment)
   - Optimization: DEFAULT (post-training quantization)
   - Input/Output type: INT8
   - Ops set: TFLITE_BUILTINS_INT8
   - Representative dataset: Generated from training set
   ```

### Output Artifacts (in `train_model_results.mltk/`)

| File | Size | Purpose |
|------|------|---------|
| `train_model.tflite` | 301.6 KB | **INT8 quantized model** (for device) |
| `train_model.float32.tflite` | ~1.2 MB | Full precision model (for comparison) |
| `train_model.h5` | ~1.5 MB | Keras model (for further training) |
| `eval/tflite/eval-results.json` | - | Quantitative evaluation metrics |
| `train/training-history.json` | - | Training loss/accuracy curves |

### Model Evaluation Results

**H5 Model (Float32)**
```
Overall accuracy: 92.18%
Class accuracies:
  - noface: 99.73%
  - face:   84.97%
Average ROC AUC: 98.38%
```

**TFLite Model (INT8 Quantized)**
```
Overall accuracy: 92.64%
Class accuracies:
  - noface: 99.80%
  - face:   85.80%
Average ROC AUC: 98.45%
```

### Profiling Summary (MVP Accelerated)

Profiling on EFR32MG26 with MVP accelerator:

```
Input Shape:           1×112×112×1
Input Data Type:       INT8

Flash, Model Size:     301.6 k bytes
RAM, Runtime Memory:   146.1 k bytes

Operation Count:       20.4 M operations
Multiply-Accumulate:   9.4 M MACs
Layer Count:           34 layers
Unsupported Layers:    0 (100% supported)

Accelerator (MVP):     7.3 M cycles
CPU (Cortex-M4):       4.9 M cycles
Total Time:            139.9 ms
Inference Rate:        7.2 fps

Power Estimate:        1.7 mJ per inference
Energy/Op:             84.6 pJ
Throughput:            145.6 M Ops/s
MAC/s:                 67.3 M MACs/s
```

### Using Custom Dataset

To retrain with your own face dataset:

1. **Prepare Dataset**
   ```
   dataset/
   ├── face/          (your face images)
   └── noface/        (background/non-face images)
   ```

2. **Upload to Google Colab**
   ```bash
   # In Colab, mount Google Drive and extract dataset
   ```

3. **Modify train_model.py**
   ```python
   my_model.dataset = './dataset/YOUR_DATASET_PATH'
   my_model.epochs = 50  # Adjust as needed
   # Retrain and generate new .tflite model
   ```

4. **Export and Deploy**
   ```bash
   # Download trained model: train_model.tflite
   # Replace in aiml_2.1.0/src/ directory
   # Rebuild firmware for EFR32
   ```

## Building and Running

### Prerequisites

- Silicon Labs Simplicity Studio 5 (or GCC ARM toolchain)
- Python 3.7+ with `Pillow` library: `pip install Pillow`
- For model training: Google Colab with GPU/TPU access

### Build Steps

1. **Open Project in Simplicity Studio 5**
   ```bash
   # File → Open → camera_arducam_2.slcp
   ```

2. **Configure SDK Components**
   - Right-click project → Simplicity → Manage Components
   - Verify TensorFlow Lite Micro, I2CSPM, SPIDRV enabled
   - Generate autogen files if needed

3. **Build Project**
   ```bash
   # Build → Build Project (or Ctrl+B)
   # Output: GNU ARM v12.2.1 - Debug/camera_arducam_2.out
   ```

4. **Flash to Device**
   ```bash
   # Run → Flash (or Ctrl+Alt+F)
   # or use JLink command-line
   ```

### Running the Classifier

1. **Start PC Receiver Script**
   ```bash
   python script_python_for_test.py
   # Output: "Đang tìm kiếm gói tin..." (Searching for packets...)
   ```

2. **Power On Device**
   - Device starts image capture immediately
   - Prints debug messages via VCOM (115200 baud alternative channel)
   - When target face detected with >60% confidence:
     - Sends SOP marker to PC
     - Streams 112×112 RGB565 image (25,088 bytes)

3. **Receive and Process**
   ```bash
   # Script automatically:
   # - Saves raw image as: data_rgb565/image_<timestamp>.rgb565
   # - Converts to PNG: image_rgb888/image_<timestamp>.png
   ```

### Converting Captured Images

Convert any RGB565 file to PNG:
```bash
python rgb565_to_rgb888.py data_rgb565/image_1234567890.rgb565
# Output: data_rgb565/image_1234567890.png
```

## Performance Characteristics

| Metric | Value | Notes |
|--------|-------|-------|
| **Image Resolution** | 112×112 pixels | Optimized for embedded inference |
| **Color Format** | RGB565 (2 bytes/pixel) | Native camera output |
| **Capture Rate** | 10-15 FPS | Camera polling dependent |
| **Inference Time** | 139.9 ms | TF Lite model on MVP |
| **Inference Rate** | 7.2 fps | Bottleneck: inference |
| **Streaming Speed** | 25 KB/chunk @ 921600 baud | ~27 ms per chunk |
| **Total Frame Latency** | ~150-200 ms | Capture + inference + streaming |
| **Memory Usage** | 146.1 KB | TF Lite model + tensor arena |
| **Flash Usage** | 301.6 KB | Model size |
| **Operation Count** | 20.4M operations | Per inference |
| **Multiply-Accumulate Count** | 9.4M MACs | Per inference |
| **CPU Utilization** | 45.2% | MVP accelerator enabled |
| **Accelerator Cycles** | 7.3M | MVP processor |
| **CPU Cycles** | 4.9M | ARM Cortex-M4 |
| **Power (Estimated)** | 1.7 mJ per inference | 84.6 picojoules per operation |
| **Throughput** | 145.6M Ops/sec | With MVP |

### Resource Budget on EFR32MG26

```
FLASH MEMORY (256 KB)
├─ Firmware code:           ~100 KB
├─ ML model (INT8):         ~301 KB  ← EXCEEDS: requires external flash or compression
├─ TF Lite runtime:         ~50 KB
└─ Available for data:      ~0 KB (tight)

SRAM (96 KB)  
├─ Tensor arena:            146.1 KB  ← Exceeds! Requires careful optimization
├─ Stack:                   ~8 KB
├─ Global variables:        ~5 KB
└─ Heap:                    ~0 KB (tight)

SOLUTION: 
- Model stored in external flash (MX25) or on-chip flash bank 2
- Tensor arena must be carefully sized
- Runtime optimization: quantization (INT8) reduces memory by 4×
```

## Development Notes

### Debugging

- **Serial Monitor**: Connect to COM port (VCOM) to see debug output
- **JLink Streaming**: Real-time image preview via JLink
- **SWV Console**: Optional advanced debugging
- **LED Indicators**: Can be added for status feedback

### Customization

To use a different ML model:

1. **Replace Model**
   - Update `aiml_2.1.0/src/` with new `.tflite` model
   - Regenerate `sl_tflite_micro_model.cc/h` with Simplicity SDK

2. **Adjust Input Size**
   - Modify `IMG_WIDTH`, `IMG_HEIGHT` in `image_classifier.cc`
   - Update Arducam resolution in `ov2640_set_framesize()`
   - Update Python scripts accordingly

3. **Change Detection Class**
   - Set `REQUIRED_CLASS_INDEX` to desired class
   - Adjust `CONFIDENCE_THRESHOLD` as needed

### Known Limitations

- Model must fit in device flash (typically 256KB available)
- Tensor arena limited by available SRAM (~96KB on EFR32MG26)
- Inference speed depends on model complexity
- Single-threaded execution (no RTOS in this build)

## Python Dependencies

For PC-side image processing:

```bash
pip install Pillow
pip install pyserial
```

Or install from requirements:
```bash
pip install -r requirements.txt
```

## Troubleshooting

### Device not detected
- Check USB cable connection (should appear as COM port)
- Verify JLink drivers installed: https://www.segger.com/downloads/jlink/
- Update device firmware if needed

### Images not streaming
- Verify CONFIDENCE_THRESHOLD is reachable by your model
- Check REQUIRED_CLASS_INDEX matches model output classes
- Monitor debug output via VCOM at 115200 baud

### Image corrupted or incomplete
- Ensure PC script reads until all chunks received
- Verify serial baud rate matches (921600)
- Check VCOM_CHUNK_SIZE matches buffer allocation

### Out of memory errors
- Reduce tensor arena size in `image_classifier.cc`
- Use smaller model
- Increase available SRAM if possible

## File Descriptions

| File | Purpose |
|------|---------|
| `image_classifier.cc/h` | ML inference engine, color conversion, image processing |
| `arducam/arducam.c/h` | High-level camera control API |
| `arducam/drivers/m2mp/` | OV2640 sensor driver, I2C/SPI protocols |
| `script_python_for_test.py` | PC receiver, image acquisition, PNG conversion |
| `rgb565_to_rgb888.py` | Standalone RGB565→PNG converter utility |
| `config/pin_config.h` | GPIO pin definitions |
| `config/sl_i2cspm_camera_config.h` | I2C camera interface config |
| `config/sl_spidrv_usart_camera_config.h` | SPI camera interface config |

## System Workflow Diagram

The project includes a comprehensive workflow visualization showing how all components interact:

![EFR32 Face Classification Workflow](images/work_flow.png)

**Workflow Components:**
- **Arducam Mini 2MP**: Captures raw RGB565 images at 112×112 resolution
- **Ping-Pong Buffers**: Two 12.5 KB buffers for continuous image capture without blocking
- **Color Conversion**: RGB565 → Grayscale using weighted formula
- **TensorFlow Lite Micro**: Runs face classification inference (34 layers)
- **Decision Logic**: IF face AND confidence > 60% → stream to PC
- **VCOM/USB Streaming**: Transmits detected images at 921600 baud
- **PC-Side Python**: Receives, converts RGB565 → PNG, saves timestamped files

## References

- [Silicon Labs EFR32MG26 Datasheet](https://www.silabs.com/products/wireless/efr32mg26)
- [TensorFlow Lite Micro Guide](https://www.tensorflow.org/lite/microcontrollers)
- [Silicon Labs MLTK (Machine Learning Toolkit)](https://github.com/SiliconLabs/mltk)
- [Arducam OV2640 Datasheet](https://www.ov.com.cn/)
- [Simplicity SDK Documentation](https://docs.silabs.com/)
- [MobileNet: Efficient Convolutional Neural Networks](https://arxiv.org/abs/1704.04861)

## License

This project is provided as-is for educational and development purposes.

## Author Notes

This is a production-ready embedded ML system demonstrating:
- ✅ Efficient embedded inference without external accelerators
- ✅ Real-time camera capture with minimal latency
- ✅ Optimized color space conversion using bit-shifting
- ✅ Robust error handling and buffer management
- ✅ PC integration for data logging and visualization

Suitable for IoT, edge AI, and embedded vision applications.

---

**Last Updated**: 2025  
**Status**: Active Development
