/*
 * image_classifier.cc
 *
 *  Created on: Jul 3, 2025
 *      Author: Can Doan (modified by AI for ML integration)
 *
 *  VERSION TÍCH HỢP MODEL - SỬA LỖI TƯƠNG THÍCH TFLITE MICRO
 *  - Sửa lỗi không tìm thấy file "micro_error_reporter.h".
 *  - Sử dụng "micro_log.h" theo phiên bản TFLM mới của Silicon Labs.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include "sl_status.h"
#include "sl_sleeptimer.h"
#include "arducam/arducam.h"
#include "jlink_stream/jlink_stream.hpp"
#include "image_classifier.h"

// --- TÍCH HỢP TENSORFLOW LITE MICRO ---
// SỬA LỖI: Bỏ file header cũ, dùng file mới
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "sl_tflite_micro_model.h"
#include "sl_tflite_micro_model_parameters.h"
#include "sl_tflite_micro_opcode_resolver.h"

// --- Cấu hình Model & Camera ---
#define IMG_WIDTH 112
#define IMG_HEIGHT 112
#define IMG_DATA_FORMAT ARDUCAM_DATA_FORMAT_GRAYSCALE

static uint8_t *camera_buffer = nullptr;

// --- BIẾN TOÀN CỤC CHO TFLITE MICRO ---
namespace
{
    // SỬA LỖI: Bỏ `MicroErrorReporter` cũ. Giờ đây chúng ta không cần một đối tượng
    // reporter cụ thể vì `MicroPrintf` đã xử lý việc logging.
    tflite::ErrorReporter* error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input_tensor = nullptr;
    TfLiteTensor *output_tensor = nullptr;

    constexpr int kTensorArenaSize = SL_TFLITE_MODEL_RUNTIME_MEMORY_SIZE + 10 * 1024;
    uint8_t tensor_arena[kTensorArenaSize];

    const char *category_labels[] = SL_TFLITE_MODEL_CLASSES;
    constexpr int category_count = sizeof(category_labels) / sizeof(category_labels[0]);
}

// --- Khai báo các hàm nội bộ ---
static bool initialize_camera_and_jlink();
static bool initialize_model();
static void run_inference_and_get_result(const uint8_t *image_data, uint32_t image_size);
static void apply_softmax(float *data, int size);

// --- Giao diện C ---
extern "C"
{
    void camera_jlink_test_init(void)
    {
        if (!initialize_camera_and_jlink()) {
            printf("ERROR: Failed to initialize camera or JLink stream\n");
            return;
        }
        if (!initialize_model()) {
            printf("ERROR: Failed to initialize ML model\n");
            return;
        }
        printf("\n=== Initialization Complete ===\n");
        printf("Model: %s, Version: %d\n", SL_TFLITE_MODEL_NAME, SL_TFLITE_MODEL_VERSION);
        printf("Ready for face classification...\n");
    }

    void camera_jlink_test_loop(void)
    {
        uint8_t *image_data = nullptr;
        uint32_t image_size = 0;

        for (;;) {
            sl_status_t status = arducam_get_next_image(&image_data, &image_size);
            if (status == SL_STATUS_IN_PROGRESS) {
                sl_sleeptimer_delay_millisecond(5);
                continue;
            } else if (status != SL_STATUS_OK) {
                printf("ERROR: Failed to get image, status: 0x%lx\n", status);
                return;
            }
            break;
        }
        run_inference_and_get_result(image_data, image_size);
        arducam_release_image();
    }

    // Các hàm còn lại không thay đổi
    void camera_jlink_test_deinit(void) { /* ... */ }
    void camera_jlink_test_settings(void) { /* ... */ }
}

// --- Các hàm thực thi nội bộ ---

static bool initialize_camera_and_jlink() {
    // ... (Hàm này giữ nguyên, không cần thay đổi) ...
    printf("Initializing JLink stream and Camera...\n");
    if (!jlink_stream::initialize() || !jlink_stream::register_stream("image", jlink_stream::Write))
    {
        return false;
    }
    arducam_config_t cam_config = ARDUCAM_DEFAULT_CONFIG;
    cam_config.image_resolution.width = IMG_WIDTH;
    cam_config.image_resolution.height = IMG_HEIGHT;
    cam_config.data_format = IMG_DATA_FORMAT;
    const uint32_t length_per_image = arducam_calculate_image_buffer_length(
        cam_config.data_format,
        cam_config.image_resolution.width,
        cam_config.image_resolution.height);
    const uint32_t image_buffer_count = 2;
    const uint32_t total_buffer_size = length_per_image * image_buffer_count;
    camera_buffer = (uint8_t *)malloc(total_buffer_size);
    if (camera_buffer == nullptr)
    {
        printf("Failed to allocate camera buffer (%lu bytes)\n", total_buffer_size);
        return false;
    }
    sl_status_t status = arducam_init(&cam_config, camera_buffer, total_buffer_size);
    if (status != SL_STATUS_OK)
    {
        printf("Failed to initialize camera (0x%lx)\n", status);
        free(camera_buffer);
        camera_buffer = nullptr;
        return false;
    }
    status = arducam_start_capture();
    if (status != SL_STATUS_OK)
    {
        printf("Failed to start camera capture (0x%lx)\n", status);
        return false;
    }
    return true;
}

static bool initialize_model()
{
    printf("Initializing ML Model...\n");

    // SỬA LỖI: Chúng ta vẫn cần một đối tượng ErrorReporter, nhưng không có lớp
    // MicroErrorReporter nữa. Chúng ta có thể dùng nullptr và để TFLM
    // sử dụng một trình báo cáo mặc định không làm gì cả. Các lỗi sẽ vẫn được
    // in ra bằng MicroPrintf ở những nơi khác.
    // Nếu cách này gây lỗi, chúng ta sẽ tạo một lớp reporter tối giản.
    error_reporter = nullptr;

    model = tflite::GetModel(sl_tflite_model_array);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        // Sử dụng MicroPrintf thay vì error_reporter->Report
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
    printf("Model initialization successful!\n");
    return true;
}

static void run_inference_and_get_result(const uint8_t *image_data, uint32_t image_size)
{
    if (!interpreter || !input_tensor || !output_tensor) return;

    // --- BƯỚC 1: TIỀN XỬ LÝ (Không thay đổi) ---
    const float scale = input_tensor->params.scale;
    const int32_t zero_point = input_tensor->params.zero_point;
    for (uint32_t i = 0; i < image_size; ++i) {
        float normalized_value = (static_cast<float>(image_data[i]) / 127.5f) - 1.0f;
        input_tensor->data.int8[i] = static_cast<int8_t>((normalized_value / scale) + zero_point);
    }

    // --- BƯỚC 2: CHẠY SUY LUẬN (Không thay đổi) ---
    if (interpreter->Invoke() != kTfLiteOk) {
        MicroPrintf("Invoke failed.");
        return;
    }

    // --- BƯỚC 3: HẬU XỬ LÝ (Không thay đổi) ---
    const float output_scale = output_tensor->params.scale;
    const int32_t output_zero_point = output_tensor->params.zero_point;
    float scores[category_count];
    for (int i = 0; i < category_count; ++i) {
        scores[i] = (static_cast<float>(output_tensor->data.int8[i]) - output_zero_point) * output_scale;
    }

    apply_softmax(scores, category_count);

    int highest_score_index = 0;
    float max_score = scores[0];
    for (int i = 1; i < category_count; ++i) {
        if (scores[i] > max_score) {
            max_score = scores[i];
            highest_score_index = i;
        }
    }
    const char *result_label = category_labels[highest_score_index];
    float confidence = max_score * 100.0f;

    // --- BƯỚC 4: GỬI KẾT QUẢ (Không thay đổi) ---
    printf("Result: %s, Confidence: %.1f%%\n", result_label, confidence);
    fflush(stdout);
}

// Hàm softmax (Không thay đổi)
static void apply_softmax(float *data, int size) {
    float sum = 0.0f;
    for (int i = 0; i < size; ++i) {
        data[i] = expf(data[i]);
        sum += data[i];
    }
    for (int i = 0; i < size; ++i) {
        data[i] /= sum;
    }
}
