#pragma once

#ifndef IMAGE_CLASSIFIER_H
#define IMAGE_CLASSIFIER_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize camera and JLink stream test
     *
     * This function initializes both the camera system and JLink streaming
     * capabilities for image capture and transmission.
     */
    void camera_jlink_test_init(void);

    /**
     * @brief Main processing loop for camera and JLink test
     *
     * This function should be called repeatedly in the main application loop.
     * It handles image capture, processing, and streaming via JLink.
     */
    void camera_jlink_test_loop(void);

    /**
     * @brief Deinitialize camera and JLink stream test (optional)
     *
     * Clean up resources when test is complete.
     */
    void camera_jlink_test_deinit(void);

    /**
     * @brief Configure camera settings (optional)
     *
     * Adjust camera parameters like brightness, contrast, etc.
     */
    void camera_jlink_test_settings(void);

#ifdef __cplusplus
}
#endif

#endif /* IMAGE_CLASSIFIER_H */