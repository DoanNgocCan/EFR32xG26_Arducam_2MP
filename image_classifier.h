#pragma once

#ifndef IMAGE_CLASSIFIER_H
#define IMAGE_CLASSIFIER_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize camera, ML model, and JLink stream test
     */
    void camera_jlink_test_init(void);

    /**
     * @brief Main processing loop for image classification
     */
    void camera_jlink_test_loop(void);

    /**
     * @brief Deinitialize camera and JLink stream test (optional)
     */
    void camera_jlink_test_deinit(void);

    /**
     * @brief Configure camera settings (optional)
     */
    void camera_jlink_test_settings(void);

#ifdef __cplusplus
}
#endif

#endif /* IMAGE_CLASSIFIER_H */
