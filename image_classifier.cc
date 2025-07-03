/*
 * image_classifier.cc
 *
 *  Created on: Jul 3, 2025
 *      Author: ADMIN
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "sl_status.h"
#include "sl_sleeptimer.h"
#include "arducam/arducam.h"
#include "jlink_stream/jlink_stream.hpp"
#include "image_classifier.h" // Include the C header

// Test configuration
#define TEST_IMAGE_WIDTH 96
#define TEST_IMAGE_HEIGHT 96
#define TEST_DATA_FORMAT ARDUCAM_DATA_FORMAT_RGB565
#define TEST_CAPTURE_INTERVAL_MS 1000

// Camera buffer
static uint8_t *camera_buffer = nullptr;
static uint32_t camera_buffer_size = 0;

// Forward declarations for internal C++ functions
static bool initialize_camera_test();
static bool initialize_jlink_test();
static void capture_and_send_image();
static void print_camera_info();

// C interface functions (called from app.c)
extern "C"
{

    /*************************************************************************************************/
    void camera_jlink_test_init(void)
    {
        //printf("\n=== Camera + JLink Stream Test ===\n");
        printf("\nMLTK Image Classifier Example\n");

        // Initialize JLink stream first
        if (!initialize_jlink_test())
        {
            printf("ERROR: Failed to initialize JLink stream\n");
            return;
        }

        // Initialize camera
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
        /*
        static uint32_t last_capture_time = 0;
        uint32_t current_time = sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());

        // Capture image every TEST_CAPTURE_INTERVAL_MS
        if ((current_time - last_capture_time) >= TEST_CAPTURE_INTERVAL_MS)
        {
            printf("--- Capture cycle %lu ---\n", current_time);
            capture_and_send_image();
            last_capture_time = current_time;
        }
        */
       
        capture_and_send_image();
        /*
        // Print heartbeat every 5 seconds
        static uint32_t heartbeat_time = 0;
        if ((current_time - heartbeat_time) >= 5000)
        {
            printf("System running... uptime: %lu ms\n", current_time);
            heartbeat_time = current_time;
        }
        */
    }

    /*************************************************************************************************/
    void camera_jlink_test_deinit(void)
    {
        printf("Deinitializing camera and JLink test...\n");

        // Stop camera capture
        arducam_stop_capture();

        // Unregister JLink streams
        jlink_stream::unregister_stream("image");
        jlink_stream::unregister_stream("info");
        jlink_stream::unregister_stream("status");

        // Free camera buffer if allocated
        if (camera_buffer != nullptr)
        {
            free(camera_buffer);
            camera_buffer = nullptr;
            camera_buffer_size = 0;
        }

        printf("Cleanup complete\n");
    }

    /*************************************************************************************************/
    void camera_jlink_test_settings(void)
    {
        printf("Configuring camera settings...\n");

        // Example: Set brightness, contrast, etc.
        // arducam_set_setting(ARDUCAM_SETTING_BRIGHTNESS, 0);
        // arducam_set_setting(ARDUCAM_SETTING_CONTRAST, 2);

        printf("Camera settings configured\n");
    }

} // extern "C"

// Internal C++ implementation functions
static bool initialize_jlink_test()
{
    printf("Initializing JLink stream...\n");

    // Initialize JLink stream system
    if (!jlink_stream::initialize())
    {
        printf("Failed to initialize JLink stream system\n");
        return false;
    }

    // Register image stream for writing
    if (!jlink_stream::register_stream("image", jlink_stream::Write))
    {
        printf("Failed to register image stream\n");
        return false;
    }

    // Register info stream for writing debug info
    if (!jlink_stream::register_stream("info", jlink_stream::Write))
    {
        printf("Failed to register info stream\n");
        return false;
    }

    // Register status stream for connection status
    if (!jlink_stream::register_stream("status", jlink_stream::Write))
    {
        printf("Failed to register status stream\n");
        return false;
    }

    printf("JLink stream initialized successfully\n");
    return true;
}

static void print_camera_info()
{
    printf("=== Camera Information ===\n");
    printf("Resolution: %dx%d\n", TEST_IMAGE_WIDTH, TEST_IMAGE_HEIGHT);
    printf("Format: RGB888\n");
    printf("Buffer size: %lu bytes\n", camera_buffer_size);
    printf("========================\n");
}

static bool initialize_camera_test()
{
    printf("Initializing camera...\n");

    // Create camera configuration
    arducam_config_t cam_config = ARDUCAM_DEFAULT_CONFIG;
    cam_config.image_resolution.width = TEST_IMAGE_WIDTH;
    cam_config.image_resolution.height = TEST_IMAGE_HEIGHT;
    cam_config.data_format = TEST_DATA_FORMAT;

    // Calculate buffer size needed
    camera_buffer_size = arducam_calculate_image_buffer_length(
        TEST_DATA_FORMAT, TEST_IMAGE_WIDTH, TEST_IMAGE_HEIGHT);

    // Allocate camera buffer
    camera_buffer = (uint8_t *)malloc(camera_buffer_size);
    if (camera_buffer == nullptr)
    {
        printf("Failed to allocate camera buffer (%lu bytes)\n", camera_buffer_size);
        return false;
    }

    // Initialize camera with buffer
    sl_status_t status = arducam_init(&cam_config, camera_buffer, camera_buffer_size);
    if (status != SL_STATUS_OK)
    {
        printf("Failed to initialize camera (0x%lx)\n", status);
        free(camera_buffer);
        camera_buffer = nullptr;
        return false;
    }

    // Start capture
    status = arducam_start_capture();
    if (status != SL_STATUS_OK)
    {
        printf("Failed to start camera capture (0x%lx)\n", status);
        return false;
    }

    printf("Camera initialized successfully\n");

    // Print camera info để sử dụng hàm và tránh warning
    print_camera_info();

    return true;
}

static void capture_and_send_image()
{
    printf("=== Starting image capture ===\n");

    // Poll camera
    arducam_poll();

    // Get next image
    uint8_t *image_data = nullptr;
    uint32_t image_size = 0;

    sl_status_t status = arducam_get_next_image(&image_data, &image_size);
    if (status == SL_STATUS_OK && image_data != nullptr)
    {
        printf("✓ Image captured successfully!\n");
        printf("  - Size: %lu bytes\n", image_size);
        printf("  - Expected size: %d bytes (96x96x3)\n", 96 * 96 * 3);
        printf("  - Buffer address: 0x%p\n", image_data);

        // Print first few pixels as hex for verification - Fix warning về sign comparison
        printf("  - First 12 bytes (4 pixels RGB): ");
        for (uint32_t i = 0; i < 12 && i < image_size; i++)
        {
            printf("%02X ", image_data[i]);
        }
        printf("\n");

        // Check if JLink streams are connected
        bool image_connected = false, info_connected = false;
        jlink_stream::is_connected("image", &image_connected);
        jlink_stream::is_connected("info", &info_connected);

        printf("  - JLink streams status:\n");
        printf("    * image: %s\n", image_connected ? "CONNECTED" : "disconnected");
        printf("    * info: %s\n", info_connected ? "CONNECTED" : "disconnected");

        if (image_connected)
        {
            // Send image data via JLink
            bool send_success = jlink_stream::write_all("image", image_data, image_size);
            printf("  - JLink transmission: %s\n", send_success ? "SUCCESS" : "FAILED");

            // Send image info
            char info_msg[100];
            snprintf(info_msg, sizeof(info_msg), "IMG:%lu,96x96,RGB888", image_size);
            jlink_stream::write_all("info", info_msg, strlen(info_msg));
        }

        // Release image
        arducam_release_image();
        printf("✓ Image released\n");
    }
    else
    {
        printf("✗ No image available (status: 0x%lx)\n", status);

        // Bỏ phần debug camera state vì arducam_context không accessible từ đây
        // Thay vào đó, in thông tin từ các API công khai
        printf("  - Attempting to restart capture...\n");
        arducam_start_capture(); // Thử restart nếu capture bị lỗi
    }
    printf("=== Capture cycle complete ===\n\n");
}
