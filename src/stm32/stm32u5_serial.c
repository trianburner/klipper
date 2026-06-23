// STM32 serial
//
// Copyright (C) 2019  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "autoconf.h" // CONFIG_SERIAL_BAUD
#include "board/armcm_boot.h" // armcm_enable_irq
#include "board/serial_irq.h" // serial_rx_byte
#include "command.h" // DECL_CONSTANT_STR
#include "internal.h" // enable_pclock
#include "sched.h" // DECL_INIT

// Select the configured serial port
#if CONFIG_STM32_SERIAL_USART1
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PA10,PA9");
  #define GPIO_Rx GPIO('A', 10)
  #define GPIO_Tx GPIO('A', 9)
  #define GPIO_AF_MODE 7
  #define USARTx USART1
  #define USARTx_IRQn USART1_IRQn
#elif CONFIG_STM32_SERIAL_USART1_ALT_PB7_PB6
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PB7,PB6");
  #define GPIO_Rx GPIO('B', 7)
  #define GPIO_Tx GPIO('B', 6)
  #define GPIO_AF_MODE 7
  #define USARTx USART1
  #define USARTx_IRQn USART1_IRQn
#elif CONFIG_STM32_SERIAL_USART1_ALT_PG10_PG9
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PG10,PG9");
  #define GPIO_Rx GPIO('G', 10)
  #define GPIO_Tx GPIO('G', 9)
  #define GPIO_AF_MODE 7
  #define USARTx USART1
  #define USARTx_IRQn USART1_IRQn
#elif CONFIG_STM32_SERIAL_USART2
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PA3,PA2");
  #define GPIO_Rx GPIO('A', 3)
  #define GPIO_Tx GPIO('A', 2)
  #define GPIO_AF_MODE 7
  #define USARTx USART2
  #define USARTx_IRQn USART2_IRQn
#elif CONFIG_STM32_SERIAL_USART2_ALT_PD6_PD5
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PD6,PD5");
  #define GPIO_Rx GPIO('D', 6)
  #define GPIO_Tx GPIO('D', 5)
  #define GPIO_AF_MODE 7
  #define USARTx USART2
  #define USARTx_IRQn USART2_IRQn
#elif CONFIG_STM32_SERIAL_USART3
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PB11,PB10");
  #define GPIO_Rx GPIO('B', 11)
  #define GPIO_Tx GPIO('B', 10)
  #define GPIO_AF_MODE 7
  #define USARTx USART3
  #define USARTx_IRQn USART3_IRQn
#elif CONFIG_STM32_SERIAL_USART3_ALT_PA5_PA7
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PA5,PA7");
  #define GPIO_Rx GPIO('A', 5)
  #define GPIO_Tx GPIO('A', 7)
  #define GPIO_AF_MODE 7
  #define USARTx USART3
  #define USARTx_IRQn USART3_IRQn
#elif CONFIG_STM32_SERIAL_USART3_ALT_PC5_PC4
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PC5,PC4");
  #define GPIO_Rx GPIO('C', 5)
  #define GPIO_Tx GPIO('C', 4)
  #define GPIO_AF_MODE 7
  #define USARTx USART3
  #define USARTx_IRQn USART3_IRQn
#elif CONFIG_STM32_SERIAL_USART3_ALT_PC11_PC10
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PC11,PC10");
  #define GPIO_Rx GPIO('C', 11)
  #define GPIO_Tx GPIO('C', 10)
  #define GPIO_AF_MODE 7
  #define USARTx USART3
  #define USARTx_IRQn USART3_IRQn
#elif CONFIG_STM32_SERIAL_USART3_ALT_PD9_PD8
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PD9,PD8");
  #define GPIO_Rx GPIO('D', 9)
  #define GPIO_Tx GPIO('D', 8)
  #define GPIO_AF_MODE 7
  #define USARTx USART3
  #define USARTx_IRQn USART3_IRQn
#elif CONFIG_STM32_SERIAL_UART4
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PA1,PA0");
  #define GPIO_Rx GPIO('A', 1)
  #define GPIO_Tx GPIO('A', 0)
  #define GPIO_AF_MODE 8
  #define USARTx UART4
  #define USARTx_IRQn UART4_IRQn
#elif CONFIG_STM32_SERIAL_UART4_ALT_PC11_PC10
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PC11,PC10");
  #define GPIO_Rx GPIO('C', 11)
  #define GPIO_Tx GPIO('C', 10)
  #define GPIO_AF_MODE 8
  #define USARTx UART4
  #define USARTx_IRQn UART4_IRQn
#elif CONFIG_STM32_SERIAL_UART5
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PD2,PC12");
  #define GPIO_Rx GPIO('D', 2)
  #define GPIO_Tx GPIO('C', 12)
  #define GPIO_AF_MODE 8
  #define USARTx UART5
  #define USARTx_IRQn UART5_IRQn
#elif CONFIG_STM32_SERIAL_LPUART1
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PA3,PA2");
  #define GPIO_Rx GPIO('A', 3)
  #define GPIO_Tx GPIO('A', 2)
  #define GPIO_AF_MODE 8
  #define USARTx LPUART1
  #define USARTx_IRQn LPUART1_IRQn
  #define LPUARTx
#elif CONFIG_STM32_SERIAL_LPUART1_ALT_PB10_PB11
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PB10,PB11");
  #define GPIO_Rx GPIO('B', 10)
  #define GPIO_Tx GPIO('B', 11)
  #define GPIO_AF_MODE 8
  #define USARTx LPUART1
  #define USARTx_IRQn LPUART1_IRQn
  #define LPUARTx
#elif CONFIG_STM32_SERIAL_LPUART1_ALT_PC0_PC1
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PC0,PC1");
  #define GPIO_Rx GPIO('C', 0)
  #define GPIO_Tx GPIO('C', 1)
  #define GPIO_AF_MODE 8
  #define USARTx LPUART1
  #define USARTx_IRQn LPUART1_IRQn
  #define LPUARTx
#elif CONFIG_STM32_SERIAL_LPUART1_ALT_PG8_PG7
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PG8,PG7");
  #define GPIO_Rx GPIO('G', 8)
  #define GPIO_Tx GPIO('G', 7)
  #define GPIO_AF_MODE 8
  #define USARTx LPUART1
  #define USARTx_IRQn LPUART1_IRQn
  #define LPUARTx
#endif

#define CR1_FLAGS (USART_CR1_UE | USART_CR1_RE | USART_CR1_TE   \
                   | USART_CR1_RXNEIE)

// Handle serial interrupts and read or transmit data
void
USARTx_IRQHandler(void)
{
    // Read recieved serial bytes if read data register is not empty
    uint32_t sr = USARTx->ISR;
    if (sr & (USART_ISR_RXNE | USART_ISR_ORE)) {
        serial_rx_byte(USARTx->RDR);
    }

    // 
    if (sr & USART_ISR_TXE && USARTx->CR1 & USART_CR1_TXEIE) {
        uint8_t data;
        int ret = serial_get_tx_byte(&data);
        if (ret)
            USARTx->CR1 = CR1_FLAGS;
        else
            USARTx->TDR = data;
    }
}

void
serial_enable_tx_irq(void)
{
    USARTx->CR1 = CR1_FLAGS | USART_CR1_TXEIE;
}

void
serial_init(void)
{
    // Enable peripheral's bus clock
    enable_pclock((uint32_t)USARTx);

    // Calculate and set baud rate register
    uint32_t pclk = get_pclock_frequency((uint32_t)USARTx);
    #ifdef LPUARTx
    uint32_t div = DIV_ROUND_CLOSEST(((uint64_t)256 * pclk), CONFIG_SERIAL_BAUD);
    #else
    uint32_t div = DIV_ROUND_CLOSEST(pclk, CONFIG_SERIAL_BAUD);
    #endif
    USARTx->BRR = div;
    //USARTx->CR3 = USART_CR3_OVRDIS;

    USARTx->CR1 = (USART_CR1_RE | USART_CR1_TE | USART_CR1_RXNEIE);

    USARTx->CR1 |= USART_CR1_UE;

    //USARTx->CR1 = CR1_FLAGS;
    armcm_enable_irq(USARTx_IRQHandler, USARTx_IRQn, 0);    

    gpio_peripheral(GPIO_Rx, GPIO_FUNCTION(GPIO_AF_MODE), 1);
    gpio_peripheral(GPIO_Tx, GPIO_FUNCTION(GPIO_AF_MODE), 0);
}
DECL_INIT(serial_init);
