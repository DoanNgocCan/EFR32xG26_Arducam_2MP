// File: uart_dma_streamer.h

#ifndef UART_DMA_STREAMER_H
#define UART_DMA_STREAMER_H

#include <stdint.h>
#include "sl_status.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @brief
 *   Initializes the EUSART1 and LDMA for streaming data over UART.
 *
 * @details
 *   - Configures EUSART1 for UART communication (e.g., 921600 baud, 8-N-1).
 *   - Configures GPIO pins PB05 (TX) and PB06 (RX).
 *   - Initializes the LDMA module and sets up the required interrupt.
 ******************************************************************************/
void uart_dma_streamer_init(void);

/***************************************************************************//**
 * @brief
 *   Streams a block of data (e.g., an image) over UART using DMA.
 *
 * @param[in] data_buffer
 *   Pointer to the data buffer to be transmitted.
 *
 * @param[in] data_size
 *   The number of bytes to transmit.
 *
 * @return
 *   SL_STATUS_OK if the transfer was successfully started.
 *   SL_STATUS_IN_PROGRESS if a transfer is already ongoing.
 *   SL_STATUS_FAIL for other errors.
 *
 * @note
 *   This function is non-blocking. It returns immediately after starting the
 *   DMA transfer. The CPU can enter a low-power mode while the transfer
 *   is in progress. Use uart_dma_streamer_is_busy() to check for completion.
 ******************************************************************************/
sl_status_t uart_dma_streamer_send(const uint8_t *data_buffer, uint32_t data_size);

/***************************************************************************//**
 * @brief
 *   Checks if the UART DMA transfer is currently in progress.
 *
 * @return
 *   true if the DMA is busy, false otherwise.
 ******************************************************************************/
bool uart_dma_streamer_is_busy(void);

#ifdef __cplusplus
}
#endif

#endif // UART_DMA_STREAMER_H
