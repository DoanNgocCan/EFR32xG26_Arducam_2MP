/*
 * image_classifier.cc
 *
 *  Created on: Jul 3, 2025
 *      Author: ADMIN
 *
 *  VERSI-ON TỐI ƯU HÓA:
 *  - Sử dụng bộ đệm Ping-Pong (2 buffer) để streaming video mượt mà.
 *  - Tái cấu trúc luồng xử lý để tối ưu hiệu suất (lấy ảnh -> xử lý -> giải phóng).
 *  - Thêm cơ chế đồng bộ "Magic Bytes" để tránh xé hình/lệch dữ liệu.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "sl_status.h"
#include "sl_sleeptimer.h"
#include "arducam/arducam.h"
#include "jlink_stream/jlink_stream.hpp"
#include "image_classifier.h" // Include the C header

// --- Cấu hình Test ---
#define TEST_IMAGE_WIDTH 96
#define TEST_IMAGE_HEIGHT 96
// Sử dụng RGB565 và để Python xử lý chuyển đổi màu.
// Điều này giúp gỡ lỗi dễ dàng hơn.
#define TEST_DATA_FORMAT ARDUCAM_DATA_FORMAT_RGB565

// Camera buffer - sẽ được cấp phát động trong hàm init
static uint8_t *camera_buffer = nullptr;

// --- CẢI TIẾN QUAN TRỌNG: Cơ chế đồng bộ Magic Bytes ---
// Một chuỗi byte đặc biệt để báo hiệu bắt đầu một khung hình mới.
// Giúp phía Python không bao giờ đọc phải dữ liệu bị lệch.
const uint8_t magic_bytes[] = {0xDE, 0xAD, 0xBE, 0xEF};

// --- Tái cấu trúc: Khai báo các hàm nội bộ mới ---
static bool initialize_camera_test();
static bool initialize_jlink_test();
// Các hàm mới để thay thế cho capture_and_send_image() cũ
static void send_image_via_jlink(const uint8_t *image_data, uint32_t image_length);

// --- Giao diện C (gọi từ app.c) ---
extern "C"
{

    /*************************************************************************************************/
    void camera_jlink_test_init(void)
    {
        // Gửi chuỗi ký tự mà kịch bản Python gốc của MLTK đang chờ
        printf("\nMLTK Image Classifier Example\n");

        // Khởi tạo JLink stream trước
        if (!initialize_jlink_test())
        {
            printf("ERROR: Failed to initialize JLink stream\n");
            return;
        }

        // Khởi tạo camera
        if (!initialize_camera_test())
        {
            printf("ERROR: Failed to initialize camera\n");
            return;
        }

        printf("=== Initialization Complete ===\n");
        printf("Ready to capture and stream images...\n");
    }

    /*************************************************************************************************/
    void camera_jlink_test_loop(void)
    {
        uint8_t *image_data = nullptr;
        uint32_t image_size = 0;

        // --- CẢI TIẾN QUAN TRỌNG: Vòng lặp chờ ảnh chuyên dụng (Polling Loop) ---
        // Vòng lặp này đảm bảo chúng ta chỉ tiếp tục khi đã có ảnh sẵn sàng.
        for (;;)
        {
            sl_status_t status = arducam_get_next_image(&image_data, &image_size);

            if (status == SL_STATUS_IN_PROGRESS)
            {
                // Nếu camera đang bận, chờ 5ms rồi thử lại.
                // Điều này giúp tránh CPU chạy 100% chỉ để hỏi "có ảnh chưa?".
                sl_sleeptimer_delay_millisecond(5);
                continue; // Quay lại đầu vòng lặp for
            }
            else if (status != SL_STATUS_OK)
            {
                printf("ERROR: Failed to get image, status: 0x%lx\n", status);
                // Tùy chọn: Có thể thêm logic khởi động lại camera ở đây nếu cần
                return; // Thoát khỏi vòng lặp xử lý nếu có lỗi
            }

            // Nếu có ảnh (status == SL_STATUS_OK), thoát khỏi vòng lặp chờ
            break;
        }

        // --- Luồng xử lý tối ưu ---
        // Giờ chúng ta đã chắc chắn có một ảnh hợp lệ trong image_data.

        // 1. Gửi ảnh qua J-Link (đã bao gồm magic bytes)
        send_image_via_jlink(image_data, image_size);

        // 2. GIẢI PHÓNG BUFFER NGAY LẬP TỨC - RẤT QUAN TRỌNG!
        // Việc này cho phép camera sử dụng buffer vừa rồi để chụp ảnh tiếp theo
        // ngay lập tức, tối đa hóa hiệu suất của kiến trúc ping-pong.
        arducam_release_image();
    }

    /*************************************************************************************************/
    void camera_jlink_test_deinit(void)
    {
        printf("Deinitializing camera and JLink test...\n");
        arducam_stop_capture();
        jlink_stream::unregister_stream("image");
        if (camera_buffer != nullptr)
        {
            free(camera_buffer);
            camera_buffer = nullptr;
        }
        printf("Cleanup complete\n");
    }

    /*************************************************************************************************/
    void camera_jlink_test_settings(void)
    {
        // Không thay đổi
        printf("Configuring camera settings...\n");
        printf("Camera settings configured\n");
    }

} // extern "C"

// --- Các hàm thực thi nội bộ ---

static bool initialize_jlink_test()
{
    printf("Initializing JLink stream...\n");
    if (!jlink_stream::initialize())
    {
        return false;
    }
    // Chỉ cần đăng ký stream "image" cho mục đích test này
    if (!jlink_stream::register_stream("image", jlink_stream::Write))
    {
        return false;
    }
    printf("JLink stream initialized successfully\n");
    return true;
}

static bool initialize_camera_test()
{
    printf("Initializing camera...\n");

    // Cấu hình camera
    arducam_config_t cam_config = ARDUCAM_DEFAULT_CONFIG;
    cam_config.image_resolution.width = TEST_IMAGE_WIDTH;
    cam_config.image_resolution.height = TEST_IMAGE_HEIGHT;
    cam_config.data_format = TEST_DATA_FORMAT;

    // --- CẢI TIẾN QUAN TRỌNG: Bộ đệm Ping-Pong ---
    // Tính toán kích thước cho MỘT ảnh
    const uint32_t length_per_image = arducam_calculate_image_buffer_length(
        cam_config.data_format,
        cam_config.image_resolution.width,
        cam_config.image_resolution.height);

    // Cấp phát bộ đệm cho HAI ảnh (Ping-Pong Buffering)
    // Giúp camera có thể ghi vào buffer này trong khi CPU đang xử lý buffer kia.
    const uint32_t image_buffer_count = 2;
    const uint32_t total_buffer_size = length_per_image * image_buffer_count;

    camera_buffer = (uint8_t *)malloc(total_buffer_size);
    if (camera_buffer == nullptr)
    {
        printf("Failed to allocate camera buffer (%lu bytes)\n", total_buffer_size);
        return false;
    }

    // Khởi tạo camera. Driver arducam sẽ tự hiểu có 2 buffer dựa trên kích thước.
    sl_status_t status = arducam_init(&cam_config, camera_buffer, total_buffer_size);
    if (status != SL_STATUS_OK)
    {
        printf("Failed to initialize camera (0x%lx)\n", status);
        free(camera_buffer);
        camera_buffer = nullptr;
        return false;
    }

    // Bắt đầu chụp ảnh
    status = arducam_start_capture();
    if (status != SL_STATUS_OK)
    {
        printf("Failed to start camera capture (0x%lx)\n", status);
        return false;
    }

    printf("Camera initialized successfully with 2 buffers (ping-pong)\n");
    return true;
}

// Hàm mới để gửi ảnh, tích hợp magic bytes
static void send_image_via_jlink(const uint8_t *image_data, uint32_t image_length)
{
    bool connected = false;

    // Kiểm tra xem phía Python đã kết nối chưa để tránh lãng phí CPU
    jlink_stream::is_connected("image", &connected);
    if (connected)
    {
        // 1. Gửi MAGIC BYTES trước để đồng bộ
        jlink_stream::write_all("image", magic_bytes, sizeof(magic_bytes));

        // 2. Sau đó gửi dữ liệu ảnh thật
        jlink_stream::write_all("image", image_data, image_length);
    }
}