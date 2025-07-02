/***************************************************************************/ /**
                                                                               * @file
                                                                               * @brief Simple ArduCam Driver Test for EFR32xG26
                                                                               *******************************************************************************
                                                                               * # License
                                                                               * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
                                                                               *******************************************************************************
                                                                               *
                                                                               * The licensor of this software is Silicon Laboratories Inc. Your use of this
                                                                               * software is governed by the terms of Silicon Labs Master Software License
                                                                               * Agreement (MSLA) available at
                                                                               * www.silabs.com/about-us/legal/master-software-license-agreement. This
                                                                               * software is distributed to you in Source Code format and is governed by the
                                                                               * sections of the MSLA applicable to Source Code.
                                                                               * *
                                                                               ******************************************************************************/

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "sl_iostream.h"
#include "sl_iostream_init_instances.h"
#include "sl_iostream_handles.h"
#include "sl_sleeptimer.h"
#include "arducam/arducam.h"

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

// Test image configuration
#define IMAGE_WIDTH 64
#define IMAGE_HEIGHT 64
#define BUFFER_COUNT 2 // Single buffer for testing
#define EXTRA_BUFFER_SIZE 1024
/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

static uint8_t image_buffer[BUFFER_COUNT * IMAGE_WIDTH * IMAGE_HEIGHT + EXTRA_BUFFER_SIZE]; // Grayscale
static bool camera_initialized = false;
static uint32_t test_counter = 0;

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************/ /**
                                                                               * Simple ArduCam driver initialization test
                                                                               ******************************************************************************/
void app_iostream_usart_init(void)
{
    /* Prevent buffering of output/input.*/
#if !defined(__CROSSWORKS_ARM) && defined(__GNUC__)
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);
#endif

    /* Setting default stream */
    sl_iostream_set_default(sl_iostream_vcom_handle);

    printf("\r\n");
    printf("=====================================\r\n");
    printf("  ArduCam Driver Test - EFR32xG26\r\n");
    printf("=====================================\r\n");
    printf("Testing camera driver connection...\r\n\r\n");

    // Configure ArduCam for testing
    arducam_config_t config = ARDUCAM_DEFAULT_CONFIG;
    config.data_format = ARDUCAM_DATA_FORMAT_GRAYSCALE;
    config.sensor_resolution = ARDUCAM_RESOLUTION_AUTO;
    config.image_resolution.width = IMAGE_WIDTH;
    config.image_resolution.height = IMAGE_HEIGHT;

    printf("1. Camera Configuration:\r\n");
    printf("   - Format: Grayscale\r\n");
    printf("   - Resolution: %dx%d pixels\r\n", IMAGE_WIDTH, IMAGE_HEIGHT);
    printf("   - Buffer size: %d bytes\r\n", sizeof(image_buffer));
    printf("   - Expected image size: %lu bytes\r\n\r\n",
           arducam_calculate_image_size(config.data_format, IMAGE_WIDTH, IMAGE_HEIGHT));

    printf("2. Initializing ArduCam driver...\r\n");
    sl_status_t status = arducam_init(&config, image_buffer, sizeof(image_buffer));

    if (status == SL_STATUS_OK)
    {
        printf("   ✓ SUCCESS: ArduCam driver initialized\r\n\r\n");
        camera_initialized = true;

        // Test camera settings (I2C communication)
        printf("3. Testing I2C communication (camera settings)...\r\n");
        status = arducam_set_setting(ARDUCAM_SETTING_BRIGHTNESS, 0);
        if (status == SL_STATUS_OK)
        {
            printf("   ✓ SUCCESS: I2C communication working\r\n");
            printf("   ✓ Camera settings configured\r\n\r\n");
        }
        else
        {
            printf("   ✗ WARNING: I2C communication issue (0x%lx)\r\n", status);
            printf("   - Check I2C SDA/SCL connections\r\n");
            printf("   - Check camera power supply\r\n\r\n");
        }

        // Test capture start (SPI communication)
        printf("4. Starting image capture...\r\n");
        status = arducam_start_capture();
        if (status == SL_STATUS_OK)
        {
            printf("   ✓ SUCCESS: Camera capture started\r\n");
            printf("   ✓ SPI communication ready\r\n\r\n");
        }
        else
        {
            printf("   ✗ FAILED: Cannot start capture (0x%lx)\r\n", status);
            printf("   - Check SPI connections (MOSI/MISO/SCK/CS)\r\n\r\n");
            camera_initialized = false;
        }
    }
    else
    {
        printf("   ✗ FAILED: ArduCam initialization failed (0x%lx)\r\n\r\n", status);

        // Detailed error analysis
        printf("3. Troubleshooting:\r\n");
        if (status == SL_STATUS_INVALID_PARAMETER)
        {
            printf("   - Buffer size too small\r\n");
            printf("   - Invalid configuration parameters\r\n");
        }
        else if (status == SL_STATUS_FAIL)
        {
            printf("   - Hardware connection issue\r\n");
            printf("   - Check I2C connections (SDA/SCL)\r\n");
            printf("   - Check SPI connections (MOSI/MISO/SCK/CS)\r\n");
            printf("   - Check camera power supply (3.3V)\r\n");
            printf("   - Check camera module is properly seated\r\n");
        }
        else
        {
            printf("   - Unknown error code: 0x%lx\r\n", status);
        }
        printf("\r\n");
        camera_initialized = false;
    }

    // Final status
    if (camera_initialized)
    {
        printf("=====================================\r\n");
        printf("   DRIVER TEST: PASSED ✓\r\n");
        printf("   Camera driver connected successfully\r\n");
        printf("   Now testing image capture...\r\n");
        printf("=====================================\r\n\r\n");
    }
    else
    {
        printf("=====================================\r\n");
        printf("   DRIVER TEST: FAILED ✗\r\n");
        printf("   Check hardware connections!\r\n");
        printf("=====================================\r\n\r\n");
    }
}

/***************************************************************************/ /**
                                                                               * Camera driver polling and testing function
                                                                               ******************************************************************************/
void app_iostream_usart_process_action(void)
{
    static uint32_t last_test_time = 0;
    static uint32_t successful_captures = 0;
    static uint32_t failed_attempts = 0;

    if (!camera_initialized)
    {
        return;
    }

    uint32_t current_time = sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());

    // Test every 3 seconds
    if ((current_time - last_test_time) >= 3000)
    {
        test_counter++;
        printf("=== Test #%lu ===\r\n", test_counter);

        // Step 1: Poll camera
        printf("1. Polling camera... ");
        sl_status_t poll_status = arducam_poll();
        if (poll_status == SL_STATUS_OK)
        {
            printf("OK\r\n");
        }
        else
        {
            printf("FAILED (0x%lx)\r\n", poll_status);
        }

        // Step 2: Try to get image
        printf("2. Getting image... ");
        uint8_t *image_data = NULL;
        uint32_t image_size = 0;
        sl_status_t get_status = arducam_get_next_image(&image_data, &image_size);

        if (get_status == SL_STATUS_OK && image_data != NULL)
        {
            successful_captures++;
            printf("SUCCESS!\r\n");
            printf("   - Image #%lu captured\r\n", successful_captures);
            printf("   - Size: %lu bytes (expected: %d)\r\n", image_size, IMAGE_WIDTH * IMAGE_HEIGHT);

            // Show first 12 bytes as hex for debugging
            printf("   - First 12 bytes: ");
            for (uint32_t i = 0; i < 12 && i < image_size; i++)
            {
                printf("%02X ", image_data[i]);
            }
            printf("\r\n");

            // Simple pixel analysis
            if (image_size >= 100)
            {
                uint32_t sum = 0;
                for (uint32_t i = 0; i < 100; i++)
                { // Sample first 100 pixels
                    sum += image_data[i];
                }
                uint32_t avg = sum / 100;
                printf("   - Average pixel value (first 100): %lu\r\n", avg);
            }

            // Release the image
            sl_status_t release_status = arducam_release_image();
            printf("3. Release image... ");
            if (release_status == SL_STATUS_OK)
            {
                printf("OK\r\n");
            }
            else
            {
                printf("FAILED (0x%lx)\r\n", release_status);
            }
        }
        else if (get_status == SL_STATUS_IN_PROGRESS)
        {
            printf("No image ready yet\r\n");
        }
        else
        {
            failed_attempts++;
            printf("FAILED (0x%lx)\r\n", get_status);
        }

        // Show statistics
        printf("4. Statistics:\r\n");
        printf("   - Successful captures: %lu\r\n", successful_captures);
        printf("   - Failed attempts: %lu\r\n", failed_attempts);
        if (test_counter > 0)
        {
            printf("   - Success rate: %lu%%\r\n", (successful_captures * 100) / test_counter);
        }

        printf("\r\n");
        last_test_time = current_time;
    }
}
