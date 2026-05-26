# Project Structure & File Guide

## Directory Overview

```
camera_arducam_2/
│
├── 📄 Core Application Files
│   ├── main.c                      # Firmware entry point (minimal - calls app_init)
│   ├── app.c/h                     # Application initialization & main loop
│   ├── image_classifier.cc/h       # ⭐ ML inference engine (MAIN LOGIC)
│   └── camera_arducam_2.slcp       # Silicon Labs project configuration
│
├── 📦 Camera Driver (Arducam OV2640)
│   └── arducam/
│       ├── arducam.c/h             # High-level camera API
│       ├── arducam.h               # Camera interface header
│       ├── arducam_types.h         # Type definitions
│       └── drivers/
│           └── m2mp/
│               ├── arducam_m_2mp_driver.c/h  # Low-level SPI/I2C driver
│               ├── ov2640.c/h                # OV2640 sensor control
│               ├── ov2640_regs.h            # Sensor register definitions
│               └── arducam_config.h         # Hardware pin configuration
│
├── ⚙️ Hardware Configuration
│   └── config/
│       ├── pin_config.h                    # GPIO pin definitions
│       ├── sl_board_control_config.h       # Board control settings
│       ├── sl_core_config.h                # Core MCU config
│       ├── sl_device_init_dcdc_config.h    # Power management
│       ├── sl_clock_manager_*.h            # Clock tree config
│       ├── sl_i2cspm_camera_config.h       # I2C camera interface
│       ├── sl_iostream_eusart_vcom_config.h # USB VCOM config
│       ├── sl_spidrv_usart_camera_config.h # SPI camera interface
│       ├── sl_tflite_micro_config.h        # TF Lite memory config
│       ├── sl_memory_manager_region_config.h # Memory layout
│       ├── dmadrv_config.h                 # DMA configuration
│       ├── spidrv_config.h                 # SPI driver config
│       └── tflite/                         # Additional ML configs
│
├── 🤖 ML Framework
│   └── aiml_2.1.0/                 # TensorFlow Lite Micro library
│       ├── inc/                    # Header files
│       ├── lib/                    # Pre-built libraries
│       ├── src/                    # TensorFlow source code
│       ├── flatbuffers/            # Model format library
│       ├── gemmlowp/               # Matrix math library
│       ├── ruy/                    # ML acceleration library
│       └── third_party/            # Dependencies
│
├── 🧠 ML Model Training (For Google Colab)
│   └── face_classification_model/  # ⭐ Model training workspace
│       ├── train_model.ipynb       # Training notebook for Colab
│       ├── train_model.py          # Standalone training script (MLTK)
│       ├── dataset/
│       │   ├── face/               # Positive class: face images
│       │   └── noface/             # Negative class: non-face/background
│       └── train_model_results.mltk/
│           ├── train_model.tflite           # INT8 quantized (301.6 KB) ← Deploy this
│           ├── train_model.float32.tflite   # Full precision (~1.2 MB)
│           ├── train_model.h5               # Keras model (~1.5 MB)
│           ├── train_model.tflite-profiling-results.txt
│           ├── eval/
│           │   ├── tflite/
│           │   │   ├── eval-results.json    # Quantized model metrics
│           │   │   └── summary.txt
│           │   └── h5/
│           │       ├── eval-results.json    # Float32 model metrics
│           │       └── summary.txt
│           └── train/
│               ├── log.txt
│               └── training-history.json
│
├── 🔧 Auto-Generated Files
│   └── autogen/
│       ├── gen.properties          # Generator metadata
│       ├── linkerfile.ld           # Memory layout definition
│       ├── RTE_Components.h        # Component manifest
│       ├── sl_component_catalog.h  # Available components
│       ├── sl_board_default_init.c # Board initialization
│       ├── sl_event_handler.c/h    # Event routing
│       ├── sl_iostream_handles.c/h # IO stream setup
│       ├── sl_iostream_init_*.c/h  # IO initialization
│       ├── sl_spidrv_init.c/h      # SPI setup
│       ├── sl_i2cspm_init.c/h      # I2C setup
│       ├── sl_tflite_micro_model.c/h       # ML model embedding
│       ├── sl_tflite_micro_opcode_resolver.h # ML op support
│       └── sbom/                   # Software bill of materials
│
├── 🐍 Python Scripts (PC-Side)
│   ├── script_python_for_test.py   # ⭐ Main image receiver
│   │   └── Features:
│   │       - Opens COM port (921600 baud)
│   │       - Receives RGB565 image stream
│   │       - Converts to PNG via Pillow
│   │       - Saves timestamped files
│   │
│   └── rgb565_to_rgb888.py         # Standalone format converter
│       └── Features:
│           - Command-line image converter
│           - RGB565 → PNG
│           - Independent utility
│
├── 📊 Data & Output Directories
│   ├── data_rgb565/                # Raw RGB565 images from device
│   ├── image_rgb888/               # Converted PNG images (viewable)
│   ├── captured_images/            # Additional capture storage
│   ├── converted_png/              # PNG conversion output
│   ├── images/                     # System architecture diagrams
│   │   └── work_flow.png           # Complete workflow visualization
│   ├── logging/                    # Debug logs
│   ├── jlink_stream/               # JLink streaming data
│   └── test_fps/                   # Performance test data
│
├── 📚 C++ Utilities
│   └── cpputils/                   # Helper library
│       ├── buffer.cc/hpp           # Dynamic buffer management
│       ├── string.cc/hpp           # String utilities
│       ├── list.cc/hpp             # Linked list implementation
│       ├── dict.cc/hpp             # Dictionary/map
│       ├── heap.cc/hpp             # Memory heap utilities
│       ├── prng.cc/hpp             # Random number generation
│       ├── semver.cc/hpp           # Semantic versioning
│       └── (and more...)           # Other utility functions
│
├── 🔨 Build Output
│   └── GNU ARM v12.2.1 - Debug/    # Compiled object files, executables
│       ├── camera_arducam_2.out    # Final firmware image
│       ├── *.o                     # Object files
│       ├── dependency files        # Auto-generated dependencies
│       └── (other build artifacts)
│
├── 📝 Documentation & Config
│   ├── readme.md                   # ⭐ Comprehensive project guide
│   ├── SETUP.md                    # Quick start for recruiters
│   ├── PROJECT_STRUCTURE.md        # This file (detailed guide)
│   ├── .gitignore                  # Git exclusion rules
│   ├── requirements.txt            # Python dependencies
│   ├── camera_arducam_2.pintool    # Pin tool configuration (XML)
│   └── camera_arducam_2.slps       # Simplicity Studio project snapshot
│
└── 🐙 Version Control
    └── .git/                       # Git repository metadata
        ├── objects/                # Compressed objects
        ├── refs/                   # Branch/tag references
        └── HEAD                    # Current branch pointer
```

## Key Files By Purpose

### 🔴 Must Read First (For Code Review)

1. **readme.md** - High-level overview and architecture
2. **image_classifier.cc** - Core ML inference + color conversion
3. **arducam/arducam.c** - Camera driver interface
4. **script_python_for_test.py** - PC communication protocol

### 🟠 Important Implementation Details

| File | Purpose | Lines of Code |
|------|---------|---------------|
| image_classifier.cc | ML inference, image processing, streaming | ~500+ |
| arducam/arducam.c | High-level camera control | ~400+ |
| arducam/drivers/m2mp/arducam_m_2mp_driver.c | Low-level SPI/I2C | ~400+ |
| arducam/drivers/m2mp/ov2640.c | Sensor register control | ~300+ |
| script_python_for_test.py | Serial protocol + image save | ~150+ |

### 🟡 Configuration Files (May Need Customization)

- **config/pin_config.h** - GPIO pins (modify if using different board)
- **config/sl_tflite_micro_config.h** - ML memory allocation
- **image_classifier.cc** (lines 35-45) - Inference thresholds
- **script_python_for_test.py** (line 1-10) - COM port selection

### 🟢 Auto-Generated (Don't Edit Manually)

- **autogen/** - All files here are auto-generated by Simplicity Studio
  - Delete and regenerate if components changed
  - Run: Right-click project → Simplicity → Generate

## Data Flow Diagram

```
┌─────────────────────────────────────────────────────────┐
│                  EMBEDDED SYSTEM (EFR32)                │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  Camera Driver                                          │
│  (arducam.c)                                            │
│       ↓                                                 │
│  Arducam OV2640 Camera                                  │
│  (112×112 RGB565 images)                                │
│       ↓                                                 │
│  Image Buffer (SPI/DMA)                                 │
│       ↓                                                 │
│  Color Space Conversion                                 │
│  RGB565 → Grayscale                                     │
│  (image_classifier.cc)                                  │
│       ↓                                                 │
│  TensorFlow Lite Micro                                  │
│  (aiml_2.1.0)                                           │
│       ↓                                                 │
│  ML Inference Engine                                    │
│  (image_classifier.cc)                                  │
│       ↓                                                 │
│  Decision Logic                                         │
│  IF confidence > 60% THEN:                              │
│       ├─ Send SOP (0xDEADBEEF)                          │
│       ├─ Send Header (image size)                       │
│       └─ Stream RGB565 data (25KB chunks)               │
│       ↓                                                 │
│  UART0 / USB VCOM                                       │
│  (921600 baud)                                          │
│                                                         │
└────────────────────────────┬────────────────────────────┘
                             │ Serial Protocol
                             │ (SOP + Header + Data)
                             ↓
┌─────────────────────────────────────────────────────────┐
│                     PC (PYTHON SIDE)                     │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  Serial Port Monitor                                    │
│  (script_python_for_test.py)                            │
│       ↓                                                 │
│  Receive RGB565 Stream                                  │
│       ↓                                                 │
│  Save Binary File                                       │
│  data_rgb565/image_<timestamp>.rgb565                   │
│       ↓                                                 │
│  Convert RGB565 → PNG                                   │
│  (Pillow library)                                       │
│       ↓                                                 │
│  Save Preview                                           │
│  image_rgb888/image_<timestamp>.png                     │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

## Build Process

```
SOURCE CODE
  ├── *.c / *.cc     (Application + Driver)
  ├── *.h            (Header files)
  └── config/        (Configuration)
       ↓
  GCC ARM Compiler
  (arm-none-eabi-gcc)
       ↓
  OBJECT FILES
  (*.o)
       ↓
  LINKER
  (linkerfile.ld)
       ↓
  EXECUTABLE
  (camera_arducam_2.out / .elf)
       ↓
  J-Link Programmer
       ↓
  DEVICE FLASH MEMORY
  (EFR32MG26)
```

## SDK Components Used

| Component | Purpose | Config File |
|-----------|---------|-------------|
| Device Init | MCU setup | autogen/ |
| Clock Manager | System clock tree | config/sl_clock_manager_* |
| GPIO | Digital I/O | config/pin_config.h |
| I2CSPM | Camera control (I2C) | config/sl_i2cspm_camera_config.h |
| SPIDRV + USART | Camera data (SPI) | config/sl_spidrv_usart_camera_config.h |
| DMADRV | Fast memory transfers | config/dmadrv_config.h |
| Sleep Timer | Delays/timing | - |
| IOStream EUSART | Serial VCOM | config/sl_iostream_eusart_vcom_config.h |
| TF Lite Micro | ML inference | aiml_2.1.0/ |

## Memory Layout (Typical)

```
0x00000000  ┌─────────────────────┐
            │   Flash Memory      │
            │   (256 KB)          │
            │                     │
            │   - Firmware        │
            │   - Model           │
            │   - Constants       │
            │                     │
0x00040000  ├─────────────────────┤
            │   Unused Flash      │
            │                     │
0x20000000  │ SRAM                │
            │ (96 KB)             │
            │                     │
            │ - Stack             │
            │ - Tensor Arena      │
            │ - Global buffers    │
            │ - Image buffers     │
            │                     │
0x20018000  └─────────────────────┘
```

## Development Workflow

```
1. EDIT CODE
   └─ Modify: image_classifier.cc, arducam/*, config/*
   
2. REBUILD
   └─ Simplicity Studio: Build → Build Project
   
3. REGENERATE (if config changed)
   └─ Right-click → Simplicity → Generate
   
4. FLASH
   └─ Run → Flash (or J-Link command-line)
   
5. TEST
   └─ Power on device + run script_python_for_test.py
   
6. DEBUG
   └─ Monitor VCOM serial output (115200 baud)
   └─ Check Python script console
   └─ Review captured PNG images
```

## Important Notes

⚠️ **Do NOT Edit Manually:**
- Any file in `autogen/` directory
- `linkerfile.ld` (regenerated automatically)
- `sl_tflite_micro_model.c/h` (replaced when model changes)

✅ **Safe to Modify:**
- `image_classifier.cc` - Add custom processing
- `config/pin_config.h` - Change GPIO assignments
- `script_python_for_test.py` - Customize PC-side behavior
- Any file in `config/` prefixed with `sl_` except for critical files

---

**Last Updated**: May 2025
