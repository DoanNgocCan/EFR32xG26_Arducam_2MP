#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

// --- CÁC FILE INCLUDE CẦN THIẾT ---
#include "sl_status.h"
#include "sl_sleeptimer.h"
#include "em_gpio.h"
#include "pin_config.h"

// <<< THÊM VÀO >>>: Include các thư viện IOStream để dùng printf
#include "sl_iostream.h"
#include "sl_iostream_init_instances.h"
#include "sl_iostream_handles.h"

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
#define CONFIDENCE_THRESHOLD    60.0f   // Ngưỡng tin cậy 60%
#define IMAGE_SIZE              (IMG_WIDTH * IMG_HEIGHT * 2)

#define SOP_BYTE_1 0xDE
#define SOP_BYTE_2 0xAD
#define SOP_BYTE_3 0xBE
#define SOP_BYTE_4 0xEF
#define VCOM_CHUNK_SIZE 1024 // Gửi 1KB mỗi lần
#define ACK_BYTE 0x06

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
static void initialize_iostream(void);
static bool initialize_system();
static bool initialize_model();
static void convert_rgb565_to_grayscale(const uint8_t *rgb_src, uint8_t *gray_dst, uint32_t width, uint32_t height);
static void process_image(const uint8_t *rgb565_image, uint32_t rgb_image_size);
static void apply_softmax(float *data, int size);


// --- Giao diện C ---
extern "C" {
    void camera_jlink_test_init(void) {
        if (!initialize_system()) {
            printf("ERROR: System initialization failed.\n");
            while(1);
        }
        printf("\n=== System Initialized (VCOM Image Streamer v7) ===\n");
        printf("Mode: Face Classifier -> Stream to PC via USB VCOM\n");
        printf("Will stream %d bytes if class '%s' has confidence > %.1f%%\n", IMAGE_SIZE, category_labels[REQUIRED_CLASS_INDEX], CONFIDENCE_THRESHOLD);
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

        // 2. Sao chép con trỏ và kích thước ảnh để xử lý
        const uint8_t* image_to_process = rgb565_image;
        const uint32_t size_to_process = rgb_image_size;

        // 3. GIẢI PHÓNG bus SPI của camera NGAY LẬP TỨC để nó sẵn sàng cho lần chụp tiếp theo.
        // Dữ liệu trong `image_to_process` vẫn an toàn.
        arducam_release_image();

        // 4. Xử lý ảnh và gửi đi qua UART (nếu cần)
        process_image(image_to_process, size_to_process);
    }
}


// --- Các hàm thực thi nội bộ ---

static void initialize_iostream(void)
{
  #if !defined(__CROSSWORKS_ARM) && defined(__GNUC__)
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);
  #endif
  // Hàm sl_iostream_init_instances() được gọi tự động bởi sl_main_init()
  // nên chúng ta không cần gọi lại. Việc này chỉ đảm bảo stdout được đặt đúng.
  sl_iostream_set_default(sl_iostream_vcom_handle);
}

static bool initialize_system() {
    initialize_iostream();
    printf("IOStream for printf initialized.\n");
    printf("Initializing System...\n");

    // 1. Khởi tạo Camera. Hàm này sẽ khởi tạo SPI (USART0) để giao tiếp với camera.
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

    // 2. Khởi tạo Model
    if (!initialize_model()) {
        printf("ERROR: Failed to initialize ML model\n");
        return false;
    }

    // 3. Bắt đầu chụp ảnh
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
        // Trích xuất các thành phần màu từ pixel RGB565
        uint16_t pixel_rgb565 = (rgb_src[1] << 8) | rgb_src[0];
        uint8_t r5 = (pixel_rgb565 >> 11) & 0x1F;
        uint8_t g6 = (pixel_rgb565 >> 5) & 0x3F;
        uint8_t b5 = pixel_rgb565 & 0x1F;

        // Chuyển đổi sang thang 8-bit
        uint8_t r8 = (r5 * 527 + 23) >> 6;
        uint8_t g8 = (g6 * 259 + 33) >> 6;
        uint8_t b8 = (b5 * 527 + 23) >> 6;

        // Chuyển đổi sang Grayscale bằng công thức trọng số (Luminosity)
        gray_dst[i] = (uint8_t)((r8 * 54 + g8 * 183 + b8 * 19) >> 8);
        rgb_src += 2; // Di chuyển con trỏ đến pixel tiếp theo
    }
}

static void process_image(const uint8_t *rgb565_image, uint32_t rgb_image_size) {
    if (!interpreter || !input_tensor || !output_tensor) return;

    // 1. Chuyển ảnh RGB sang Grayscale để đưa vào model
    convert_rgb565_to_grayscale(rgb565_image, grayscale_buffer, IMG_WIDTH, IMG_HEIGHT);

    // 2. Chuẩn bị dữ liệu đầu vào cho model (quantization)
    const float scale = input_tensor->params.scale;
    const int32_t zero_point = input_tensor->params.zero_point;
    for (uint32_t i = 0; i < (IMG_WIDTH * IMG_HEIGHT); ++i) {
        float normalized_value = (static_cast<float>(grayscale_buffer[i]) / 127.5f) - 1.0f;
        input_tensor->data.int8[i] = static_cast<int8_t>((normalized_value / scale) + zero_point);
    }

    // 3. Chạy suy luận
    if (interpreter->Invoke() != kTfLiteOk) { MicroPrintf("Invoke failed."); return; }

    // 4. Xử lý kết quả đầu ra
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

    printf("Result: %s, Confidence: %.1f%% -> ", result_label, confidence);
    fflush(stdout);

    // 5. Ra quyết định: Gửi ảnh nếu là 'face' và đủ tự tin
    if (highest_score_index == REQUIRED_CLASS_INDEX && confidence > CONFIDENCE_THRESHOLD) {
            printf("Result: %s, Confidence: %.1f%% -> DETECTED! Starting ACK transfer...\n",
                   category_labels[highest_score_index], confidence);
            fflush(stdout);
            sl_sleeptimer_delay_millisecond(50);

            // <<< BẮT ĐẦU VÙNG TRUYỀN DỮ LIỆU CÓ ĐỒNG BỘ >>>

            // 1. Gửi Header (SOP + Total Size)
            uint8_t header[8];
            header[0] = SOP_BYTE_1;
            header[1] = SOP_BYTE_2;
            header[2] = SOP_BYTE_3;
            header[3] = SOP_BYTE_4;
            memcpy(&header[4], &rgb_image_size, 4);
            sl_iostream_write(sl_iostream_vcom_handle, header, sizeof(header));

            // 2. Chờ ACK đầu tiên từ Python
            char ack_char = 0;
            size_t bytes_read = 0;
            sl_status_t status = sl_iostream_read(sl_iostream_vcom_handle, &ack_char, 1, &bytes_read);
            if (status != SL_STATUS_OK || ack_char != ACK_BYTE) {
                printf("Error: Did not receive initial ACK. Aborting.\n");
                fflush(stdout);
                return;
            }

            // 3. Gửi Payload theo từng Chặng và chờ ACK
            uint32_t bytes_sent = 0;
            const uint8_t *ptr = rgb565_image;
            while (bytes_sent < rgb_image_size) {
                uint32_t bytes_to_send = rgb_image_size - bytes_sent;
                if (bytes_to_send > VCOM_CHUNK_SIZE) {
                    bytes_to_send = VCOM_CHUNK_SIZE;
                }

                sl_iostream_write(sl_iostream_vcom_handle, ptr, bytes_to_send);

                ack_char = 0;
                bytes_read = 0;
                status = sl_iostream_read(sl_iostream_vcom_handle, &ack_char, 1, &bytes_read);
                if (status != SL_STATUS_OK || ack_char != ACK_BYTE) {
                     printf("Error: Did not receive chunk ACK. Aborting at %lu bytes.\n", bytes_sent);
                     fflush(stdout);
                     return;
                }

                ptr += bytes_to_send;
                bytes_sent += bytes_to_send;
            }

            printf("Transfer complete. Total: %lu bytes.\n", bytes_sent);
            fflush(stdout);

        } else {
            printf("Result: %s, Confidence: %.1f%% -> NOT detected. Skipping.\n",
                   category_labels[highest_score_index], confidence);
            fflush(stdout);
        }
}

static void apply_softmax(float *data, int size) {
    if (!data || size <= 0) return;
    float sum = 0.0f;
    float max_val = data[0];
    for (int i = 1; i < size; ++i) {
        if (data[i] > max_val) max_val = data[i];
    }
    for (int i = 0; i < size; ++i) {
        data[i] = expf(data[i] - max_val);
        sum += data[i];
    }
    for (int i = 0; i < size; ++i) {
        data[i] /= sum;
    }
}
