#include "uart_dma_streamer.h"
#include "pin_config.h"

#include <stdbool.h>

#include "sl_hal_eusart.h"
#include "sl_gpio.h"
#include "sl_clock_manager.h"
#include "sl_device_peripheral.h"

// Bao gồm thư viện DMADRV
#include "dmadrv.h"

// --- Cấu hình ---
#define UART_INSTANCE           EUSART1
#define UART_BAUDRATE           115200

// --- Biến nội bộ ---
static unsigned int dma_channel;
static volatile bool dma_transfer_in_progress = false;

// --- Hàm Callback cho DMADRV ---
static void dma_transfer_complete_callback(unsigned int channel,
                                           unsigned int sequenceNo,
                                           void *userParam)
{
  (void)channel;
  (void)sequenceNo;
  (void)userParam;
  dma_transfer_in_progress = false;
}

void uart_dma_streamer_init(void)
{
  // 1. Khởi tạo DMADRV (an toàn để gọi nhiều lần)
  DMADRV_Init();

  // 2. Yêu cầu DMADRV cấp phát một kênh DMA
  // Dùng kênh 1 trở đi để an toàn, tránh kênh 0 mà SPIDRV có thể đang dùng
  for (unsigned int ch = 1; ch < 8; ++ch) {
      if (DMADRV_AllocateChannelById(ch, NULL) == ECODE_EMDRV_DMADRV_OK) {
          dma_channel = ch;
          break;
      }
  }

  // 3. Khởi tạo EUSART1
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_GPIO);
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_EUSART1);

  sl_hal_eusart_uart_config_t uart_config = SL_HAL_EUSART_UART_INIT_DEFAULT_HF;

  uint32_t clk_freq;
  sl_clock_branch_t clock_branch = sl_device_peripheral_get_clock_branch(SL_PERIPHERAL_EUSART1);
  sl_clock_manager_get_clock_branch_frequency(clock_branch, &clk_freq);
  uart_config.clock_div = sl_hal_eusart_uart_calculate_clock_div(clk_freq,
                                                                 UART_BAUDRATE,
                                                                 uart_config.oversampling);

  sl_gpio_set_pin_mode(&(sl_gpio_t){EUSART1_TX_PORT, EUSART1_TX_PIN}, SL_GPIO_MODE_PUSH_PULL, 1);
  sl_gpio_set_pin_mode(&(sl_gpio_t){EUSART1_RX_PORT, EUSART1_RX_PIN}, SL_GPIO_MODE_INPUT, 0);

  GPIO->EUSARTROUTE[EUSART_NUM(UART_INSTANCE)].TXROUTE =
      (EUSART1_TX_PORT << _GPIO_USART_TXROUTE_PORT_SHIFT) | (EUSART1_TX_PIN << _GPIO_USART_TXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[EUSART_NUM(UART_INSTANCE)].RXROUTE =
      (EUSART1_RX_PORT << _GPIO_USART_RXROUTE_PORT_SHIFT) | (EUSART1_RX_PIN << _GPIO_USART_RXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[EUSART_NUM(UART_INSTANCE)].ROUTEEN = GPIO_EUSART_ROUTEEN_TXPEN | GPIO_EUSART_ROUTEEN_RXPEN;

  sl_hal_eusart_init_uart_hf(UART_INSTANCE, &uart_config);
  sl_hal_eusart_enable(UART_INSTANCE);
}

sl_status_t uart_dma_streamer_send(const uint8_t *data_buffer, uint32_t data_size)
{
  if (dma_transfer_in_progress) {
    return SL_STATUS_IN_PROGRESS;
  }
  if (data_buffer == NULL || data_size == 0) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  dma_transfer_in_progress = true;

  // << SỬA LỖI >>: Gọi hàm DMADRV_MemoryPeripheral với đúng các tham số
  Ecode_t status = DMADRV_MemoryPeripheral(dma_channel,
                                           dmadrvPeripheralSignal_EUSART1_TXBL,
                                           (void *)&(UART_INSTANCE->TXDATA), // Đích (dst)
                                           (void *)data_buffer,               // Nguồn (src)
                                           true,                              // srcInc: CÓ tăng địa chỉ nguồn
                                           data_size,                         // Số lượng (len)
                                           dmadrvDataSize1,                   // Kích thước mỗi lần chuyển
                                           dma_transfer_complete_callback,    // Hàm callback khi xong
                                           NULL);                             // userParam

  if (status != ECODE_EMDRV_DMADRV_OK) {
    dma_transfer_in_progress = false;
    return SL_STATUS_FAIL;
  }

  return SL_STATUS_OK;
}

bool uart_dma_streamer_is_busy(void)
{
  return dma_transfer_in_progress;
}
