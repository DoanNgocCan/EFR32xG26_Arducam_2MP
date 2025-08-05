/*
 * image_classifier.cc
 *
 *  Created on: Jul 3, 2025
 *      Author: Can Doan (modified by AI for ML integration)
 *
 *  VERSION CUỐI CÙNG v4 - Dùng SPIDRV một cách an toàn
 *  - Dựa trên datasheet của LDMA, xác nhận SPIDRV là cách tiếp cận đúng để tránh xung đột.
 *  - Sửa lại logic khởi tạo để tránh bị treo: Giả định arducam đã khởi tạo SPIDRV,
 *    code của chúng ta chỉ "mượn" handle đã có để sử dụng.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

// --- CÁC FILE INCLUDE CẦN THIẾT ---
#include "sl_status.h"
#include "sl_sleeptimer.h"
#include "em_gpio.h"
#include "pin_config.h"

// <<< QUAN TRỌNG >>>: Chỉ dùng SPIDRV, không dùng DMADRV trực tiếp
#include "spidrv.h"
#include "sl_spidrv_instances.h" // Chứa định nghĩa sl_spidrv_usart_camera_handle

#include "arducam/arducam.h"
#include "image_classifier.h"

// --- TÍCH HỢP TENSORFLOW LITE MICRO ---
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "sl_tflite_micro_model.h"
#include "sl_tflite_micro_model_parameters.h"
#include "sl_tflite_micro_opcode_resolver.h"


// --- Cấu hình ---
#define IMG_WIDTH 112
#define IMG_HEIGHT 112
#define IMG_DATA_FORMAT_CAMERA ARDUCAM_DATA_FORMAT_RGB565
#define REQUIRED_CLASS_INDEX    0
#define CONFIDENCE_THRESHOLD    60.0f

// <<< SỬA ĐỔI >>>: Các biến cho SPIDRV
static volatile bool pi_spi_transfer_complete;
#define PI_SPI_HANDLE sl_spidrv_usart_camera_handle // Sử dụng handle đã được autogen

// Buffers
static uint8_t *camera_pingpong_buffer = nullptr;
static uint8_t *grayscale_buffer = nullptr;

// --- BIẾN TOÀN CỤC CHO TFLITE MICRO ---
namespace {
    tflite::ErrorReporter* error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input_tensor = nullptr;
    TfLiteTensor *output_tensor = nullptr;

    constexpr int kTensorArenaSize = SL_TFLITE_MODEL_RUNTIME_MEMORY_SIZE + 16 * 1024;
    uint8_t tensor_arena[kTensorArenaSize];

    const char *category_labels[] = SL_TFLITE_MODEL_CLASSES;
    constexpr int category_count = sizeof(category_labels) / sizeof(category_labels[0]);
}


// --- Khai báo các hàm nội bộ ---
static bool initialize_system();
static bool initialize_model();
static void select_pi();
static void deselect_pi();
static void convert_rgb565_to_grayscale(const uint8_t *rgb_src, uint8_t *gray_dst, uint32_t width, uint32_t height);
static void process_image(const uint8_t *rgb565_image, uint32_t rgb_image_size);
static void stream_image_to_pi_spidrv(const uint8_t *rgb565_image, uint32_t image_size);
static void pi_spidrv_callback(SPIDRV_HandleData_t *handle, Ecode_t transferStatus, int itemsTransferred);
static void apply_softmax(float *data, int size);


// --- Giao diện C ---
extern "C" {
    void camera_jlink_test_init(void) {
        if (!initialize_system()) {
            printf("ERROR: System initialization failed.\n");
            while(1);
        }
        printf("\n=== System Initialized Successfully (SPIDRV v4) ===\n");
        printf("Mode: Face Classifier -> Stream to Pi via SPI/DMA\n");
        printf("Will stream image if class '%s' has confidence > %.1f%%\n",
               category_labels[REQUIRED_CLASS_INDEX], CONFIDENCE_THRESHOLD);
    }

    void camera_jlink_test_loop(void) {
            uint8_t *rgb565_image = nullptr;
            uint32_t rgb_image_size = 0;

            // 1. Lấy ảnh từ camera
            for (;;) {
                sl_status_t status = arducam_get_next_image(&rgb565_image, &rgb_image_size);
                if (status == SL_STATUS_IN_PROGRESS) {
                    sl_sleeptimer_delay_millisecond(10);
                    continue;
                } else if (status != SL_STATUS_OK) {
                    MicroPrintf("ERROR: Failed to get image, status: 0x%lx\n", status);
                    return;
                }
                break;
            }

            // <<< SỬA LỖI QUAN TRỌNG: Thay đổi thứ tự gọi hàm >>>

            // 2. Xử lý ảnh và ra quyết định
            // Chúng ta cần sao chép con trỏ ảnh lại vì arducam_release_image() có thể thay đổi nó
            const uint8_t* image_to_process = rgb565_image;
            const uint32_t size_to_process = rgb_image_size;

            // 3. GIẢI PHÓNG bus SPI NGAY LẬP TỨC
            // Hàm này sẽ "dọn dẹp" giao dịch của camera và đưa SPIDRV về trạng thái IDLE.
            arducam_release_image();

            // 4. BÂY GIỜ MỚI xử lý và gửi đi
            // Dữ liệu trong buffer vẫn an toàn vì chúng ta đã có con trỏ `image_to_process`.
            // Hàm `release` chỉ giải phóng quyền sử dụng buffer cho lần chụp tiếp theo,
            // chứ không xóa dữ liệu ngay lập tức.
            process_image(image_to_process, size_to_process);
        }
}


// --- Các hàm thực thi nội bộ ---

static void select_pi() { GPIO_PinOutClear(CS_2_PORT, CS_2_PIN); }
static void deselect_pi() { GPIO_PinOutSet(CS_2_PORT, CS_2_PIN); }

static sl_status_t wait_for_spi_bus_idle(void) {
    Ecode_t ecode;
    int items_transferred, items_remaining;
    const uint32_t timeout_ms = 100;
    uint32_t start_tick = sl_sleeptimer_get_tick_count();

    do {
        // Gọi hàm theo đúng chữ ký trong spidrv.h
        ecode = SPIDRV_GetTransferStatus(PI_SPI_HANDLE, &items_transferred, &items_remaining);

        // Nếu hàm trả về mã lỗi IDLE, có nghĩa là bus đã rảnh
        if (ecode == ECODE_EMDRV_SPIDRV_IDLE) {
            return SL_STATUS_OK;
        }
        // Nếu hàm trả về OK, có nghĩa là nó vẫn đang bận
        else if (ecode != ECODE_OK) {
            // Xử lý các mã lỗi khác nếu cần
            printf("ERROR: SPIDRV_GetTransferStatus returned unexpected error: 0x%lx\n", ecode);
            return SL_STATUS_FAIL;
        }

        // Kiểm tra timeout
        if (sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count() - start_tick) > timeout_ms) {
            printf("ERROR: Timeout waiting for SPI bus to become idle.\n");
            return SL_STATUS_TIMEOUT;
        }

    } while (true);
}

// <<< SỬA ĐỔI QUAN TRỌNG >>>: Đơn giản hóa hàm khởi tạo
static bool initialize_system() {
    printf("Initializing System (SPIDRV version)...\n");

    // 1. Khởi tạo chân CS cho Pi. Đây là một GPIO độc lập và luôn an toàn.
    GPIO_PinModeSet(CS_2_PORT, CS_2_PIN, gpioModePushPull, 1);

    // 2. Khởi tạo Camera. HÀM NÀY SẼ KHỞI TẠO USART0 VÀ SPIDRV.
    arducam_config_t cam_config = ARDUCAM_DEFAULT_CONFIG;
    cam_config.image_resolution.width = IMG_WIDTH;
    cam_config.image_resolution.height = IMG_HEIGHT;
    cam_config.data_format = IMG_DATA_FORMAT_CAMERA;
    const uint32_t rgb_length_per_image = arducam_calculate_image_buffer_length(
        cam_config.data_format, cam_config.image_resolution.width, cam_config.image_resolution.height);
    camera_pingpong_buffer = (uint8_t *)malloc(rgb_length_per_image * 2);
    if (!camera_pingpong_buffer) { printf("Failed to allocate camera buffer\n"); return false; }
    grayscale_buffer = (uint8_t *)malloc(IMG_WIDTH * IMG_HEIGHT);
    if (!grayscale_buffer) { printf("Failed to allocate grayscale buffer\n"); free(camera_pingpong_buffer); return false; }

    sl_status_t status = arducam_init(&cam_config, camera_pingpong_buffer, rgb_length_per_image * 2);
    if (status != SL_STATUS_OK) {
        printf("Failed to initialize camera (0x%lx)\n", status);
        free(camera_pingpong_buffer);
        free(grayscale_buffer);
        return false;
    }

    // 3. KHÔNG khởi tạo DMADRV hay SPIDRV ở đây nữa. Cứ để arducam_init() làm việc đó.

    // 4. Khởi tạo Model
    if (!initialize_model()) {
        printf("ERROR: Failed to initialize ML model\n");
        return false;
    }

    // 5. Bắt đầu chụp ảnh
    status = arducam_start_capture();
    if (status != SL_STATUS_OK) {
        printf("Failed to start camera capture (0x%lx)\n", status);
        return false;
    }

    return true;
}

static bool initialize_model() {
    error_reporter = nullptr;
    model = tflite::GetModel(sl_tflite_model_array);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        MicroPrintf("Model schema version mismatch.");
        return false;
    }
    SL_TFLITE_MICRO_OPCODE_RESOLVER(op_resolver);
    static tflite::MicroInterpreter static_interpreter(model, op_resolver,
                                                       tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        MicroPrintf("AllocateTensors() failed.");
        return false;
    }
    input_tensor = interpreter->input(0);
    output_tensor = interpreter->output(0);
    return true;
}

static void convert_rgb565_to_grayscale(const uint8_t *rgb_src, uint8_t *gray_dst, uint32_t width, uint32_t height) {
    const uint32_t pixel_count = width * height;
    for (uint32_t i = 0; i < pixel_count; ++i) {
        uint16_t pixel_rgb565 = (rgb_src[1] << 8) | rgb_src[0];
        uint8_t r5 = (pixel_rgb565 >> 11) & 0x1F;
        uint8_t g6 = (pixel_rgb565 >> 5) & 0x3F;
        uint8_t b5 = pixel_rgb565 & 0x1F;
        uint8_t r8 = (r5 * 527 + 23) >> 6;
        uint8_t g8 = (g6 * 259 + 33) >> 6;
        uint8_t b8 = (b5 * 527 + 23) >> 6;
        gray_dst[i] = (uint8_t)((r8 * 54 + g8 * 183 + b8 * 19) >> 8);
        rgb_src += 2;
    }
}

static void process_image(const uint8_t *rgb565_image, uint32_t rgb_image_size) {
    if (!interpreter || !input_tensor || !output_tensor) return;
    convert_rgb565_to_grayscale(rgb565_image, grayscale_buffer, IMG_WIDTH, IMG_HEIGHT);
    const float scale = input_tensor->params.scale;
    const int32_t zero_point = input_tensor->params.zero_point;
    for (uint32_t i = 0; i < (IMG_WIDTH * IMG_HEIGHT); ++i) {
        float normalized_value = (static_cast<float>(grayscale_buffer[i]) / 127.5f) - 1.0f;
        input_tensor->data.int8[i] = static_cast<int8_t>((normalized_value / scale) + zero_point);
    }
    if (interpreter->Invoke() != kTfLiteOk) { MicroPrintf("Invoke failed."); return; }
    const float output_scale = output_tensor->params.scale;
    const int32_t output_zero_point = output_tensor->params.zero_point;
    float scores[category_count];
    for (int i = 0; i < category_count; ++i) {
        scores[i] = (static_cast<float>(output_tensor->data.int8[i]) - output_zero_point) * output_scale;
    }
    apply_softmax(scores, category_count);
    int highest_score_index = 0;
    float max_score = scores[0];
    for (int i = 1; i < category_count; ++i) { if (scores[i] > max_score) { max_score = scores[i]; highest_score_index = i; } }
    const char *result_label = category_labels[highest_score_index];
    float confidence = max_score * 100.0f;
    printf("Result: %s, Confidence: %.1f%% -> ", result_label, confidence);
    if (highest_score_index == REQUIRED_CLASS_INDEX && confidence > CONFIDENCE_THRESHOLD) {
        printf("DETECTED! Streaming image to Raspberry Pi...\n");
        fflush(stdout);
        stream_image_to_pi_spidrv(rgb565_image, rgb_image_size);
    } else {
        printf("NOT detected or low confidence. Skipping.\n");
        fflush(stdout);
    }
}

static void apply_softmax(float *data, int size) {
    float sum = 0.0f;
    float max_val = data[0];
    for (int i = 1; i < size; ++i) { if (data[i] > max_val) max_val = data[i]; }
    for (int i = 0; i < size; ++i) {
        data[i] = expf(data[i] - max_val);
        sum += data[i];
    }
    for (int i = 0; i < size; ++i) { data[i] /= sum; }
}

// <<< HÀM MỚI CHO SPIDRV >>>
static void pi_spidrv_callback(SPIDRV_HandleData_t *handle,
                               Ecode_t transferStatus,
                               int itemsTransferred)
{
    (void)handle;
    (void)itemsTransferred;

    deselect_pi(); // Kéo CS lên cao để kết thúc

    if (transferStatus == ECODE_EMDRV_SPIDRV_OK) {
        pi_spi_transfer_complete = true;
    } else {
        printf("ERROR: SPIDRV transfer to Pi failed with status: 0x%lx\n", transferStatus);
        pi_spi_transfer_complete = true; // Vẫn đặt cờ để thoát
    }
}

// <<< HÀM MỚI CHO SPIDRV >>>
static void stream_image_to_pi_spidrv(const uint8_t *rgb565_image, uint32_t image_size) {
    if (wait_for_spi_bus_idle() != SL_STATUS_OK) { return; }

    pi_spi_transfer_complete = false;
    select_pi();

    Ecode_t status = SPIDRV_MTransmit(
        PI_SPI_HANDLE,
        rgb565_image,
        image_size,
        pi_spidrv_callback
    );

    if (status != ECODE_OK) {
        printf("ERROR: Failed to start SPIDRV_MTransmit: 0x%lx\n", status);
        deselect_pi();
        return;
    }

    while (!pi_spi_transfer_complete) {
        // sl_power_manager_sleep();
    }
    printf("SPI transfer to Pi complete.\n");
    fflush(stdout);
}
