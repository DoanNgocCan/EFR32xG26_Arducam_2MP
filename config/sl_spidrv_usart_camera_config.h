/***************************************************************************//**
 * @file
 * @brief SPIDRV_USART Config
 *******************************************************************************
 * # License
 * <b>Copyright 2019 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#ifndef SL_SPIDRV_USART_CAMERA_CONFIG_H
#define SL_SPIDRV_USART_CAMERA_CONFIG_H

#include "spidrv.h"

// <<< Use Configuration Wizard in Context Menu >>>
// <h> SPIDRV settings

// <o SL_SPIDRV_USART_CAMERA_BITRATE> SPI bitrate
// <i> Default: 1000000
#define SL_SPIDRV_USART_CAMERA_BITRATE           1000000

// <o SL_SPIDRV_USART_CAMERA_FRAME_LENGTH> SPI frame length <4-16>
// <i> Default: 8
#define SL_SPIDRV_USART_CAMERA_FRAME_LENGTH      8

// <o SL_SPIDRV_USART_CAMERA_TYPE> SPI mode
// <spidrvMaster=> Master
// <spidrvSlave=> Slave
#define SL_SPIDRV_USART_CAMERA_TYPE              spidrvMaster

// <o SL_SPIDRV_USART_CAMERA_BIT_ORDER> Bit order on the SPI bus
// <spidrvBitOrderLsbFirst=> LSB transmitted first
// <spidrvBitOrderMsbFirst=> MSB transmitted first
#define SL_SPIDRV_USART_CAMERA_BIT_ORDER         spidrvBitOrderMsbFirst

// <o SL_SPIDRV_USART_CAMERA_CLOCK_MODE> SPI clock mode
// <spidrvClockMode0=> SPI mode 0: CLKPOL=0, CLKPHA=0
// <spidrvClockMode1=> SPI mode 1: CLKPOL=0, CLKPHA=1
// <spidrvClockMode2=> SPI mode 2: CLKPOL=1, CLKPHA=0
// <spidrvClockMode3=> SPI mode 3: CLKPOL=1, CLKPHA=1
#define SL_SPIDRV_USART_CAMERA_CLOCK_MODE        spidrvClockMode0

// <o SL_SPIDRV_USART_CAMERA_CS_CONTROL> SPI master chip select (CS) control scheme.
// <spidrvCsControlAuto=> CS controlled by the SPI driver
// <spidrvCsControlApplication=> CS controlled by the application
#define SL_SPIDRV_USART_CAMERA_CS_CONTROL        spidrvCsControlAuto

// <o SL_SPIDRV_USART_CAMERA_SLAVE_START_MODE> SPI slave transfer start scheme
// <spidrvSlaveStartImmediate=> Transfer starts immediately
// <spidrvSlaveStartDelayed=> Transfer starts when the bus is idle (CS deasserted)
// <i> Only applies if instance type is spidrvSlave
#define SL_SPIDRV_USART_CAMERA_SLAVE_START_MODE  spidrvSlaveStartImmediate
// </h>
// <<< end of configuration section >>>

// <<< sl:start pin_tool >>>
// <usart signal=TX,RX,CLK,(CS)> SL_SPIDRV_USART_CAMERA
// $[USART_SL_SPIDRV_USART_CAMERA]
#ifndef SL_SPIDRV_USART_CAMERA_PERIPHERAL       
#define SL_SPIDRV_USART_CAMERA_PERIPHERAL        USART0
#endif
#ifndef SL_SPIDRV_USART_CAMERA_PERIPHERAL_NO    
#define SL_SPIDRV_USART_CAMERA_PERIPHERAL_NO     0
#endif

// USART0 TX on PC13
#ifndef SL_SPIDRV_USART_CAMERA_TX_PORT          
#define SL_SPIDRV_USART_CAMERA_TX_PORT           SL_GPIO_PORT_C
#endif
#ifndef SL_SPIDRV_USART_CAMERA_TX_PIN           
#define SL_SPIDRV_USART_CAMERA_TX_PIN            13
#endif

// USART0 RX on PC12
#ifndef SL_SPIDRV_USART_CAMERA_RX_PORT          
#define SL_SPIDRV_USART_CAMERA_RX_PORT           SL_GPIO_PORT_C
#endif
#ifndef SL_SPIDRV_USART_CAMERA_RX_PIN           
#define SL_SPIDRV_USART_CAMERA_RX_PIN            12
#endif

// USART0 CLK on PC11
#ifndef SL_SPIDRV_USART_CAMERA_CLK_PORT         
#define SL_SPIDRV_USART_CAMERA_CLK_PORT          SL_GPIO_PORT_C
#endif
#ifndef SL_SPIDRV_USART_CAMERA_CLK_PIN          
#define SL_SPIDRV_USART_CAMERA_CLK_PIN           11
#endif

// USART0 CS on PC10
#ifndef SL_SPIDRV_USART_CAMERA_CS_PORT          
#define SL_SPIDRV_USART_CAMERA_CS_PORT           SL_GPIO_PORT_C
#endif
#ifndef SL_SPIDRV_USART_CAMERA_CS_PIN           
#define SL_SPIDRV_USART_CAMERA_CS_PIN            10
#endif
// [USART_SL_SPIDRV_USART_CAMERA]$
// <<< sl:end pin_tool >>>

#endif // SL_SPIDRV_USART_CAMERA_CONFIG_H
