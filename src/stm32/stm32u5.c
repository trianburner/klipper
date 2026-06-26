// Code to setup clocks and gpio on stm32f1
//
// Copyright (C) 2019-2022  Kevin O'Connor <kevin@koconnor.net>
// Copyright (C) 2026  Brian Turner <trian.burner@outlook.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "autoconf.h" // CONFIG_CLOCK_REF_FREQ
#include "board/armcm_boot.h" // VectorTable
#include "board/armcm_reset.h" // try_request_canboot
#include "board/irq.h" // irq_disable
#include "board/misc.h" // bootloader_request
#include "command.h" // DECL_CONSTANT_STR
#include "internal.h" // enable_pclock
#include "sched.h" // sched_main

#define FREQ_PERIPH CONFIG_CLOCK_FREQ
#define FREQ_USB 48000000

/****************************************************************
 * Clock setup
 ****************************************************************/

 // Map a peripheral address to its enable bits
struct cline
lookup_clock_line(uint32_t periph_base)
{
    if (periph_base == (uint32_t)LPUART1) {
        return (struct cline){.en=&RCC->APB3ENR, .rst=&RCC->APB3RSTR, .bit=RCC_APB3ENR_LPUART1EN};
    } else if (periph_base == (uint32_t)ADC1){
        return (struct cline){.en=&RCC->AHB2ENR1, .rst=&RCC->AHB2RSTR1, .bit=RCC_AHB2ENR1_ADC12EN};
    } else if (periph_base == (uint32_t)ADC4){
        return (struct cline){.en=&RCC->AHB3ENR, .rst=&RCC->AHB3RSTR, .bit=RCC_AHB3ENR_ADC4EN};
    } else if (periph_base >= AHB3PERIPH_BASE_NS) {
        uint32_t bit = 1 << ((periph_base - AHB3PERIPH_BASE_NS) / 0x400);
        return (struct cline){.en=&RCC->AHB3ENR, .rst=&RCC->AHB3RSTR, .bit=bit};
    } else if (periph_base >= APB3PERIPH_BASE_NS) {
        uint32_t bit = 1 << ((periph_base - APB3PERIPH_BASE_NS) / 0x400);
        return (struct cline){.en=&RCC->APB3ENR, .rst=&RCC->APB3RSTR, .bit=bit};
    } else if (periph_base >= AHB2PERIPH_BASE) {
        uint32_t bit = 1 << ((periph_base - AHB2PERIPH_BASE) / 0x400);
        return (struct cline){.en=&RCC->AHB2ENR1, .rst=&RCC->AHB2RSTR1, .bit=bit};
    } else if (periph_base >= AHB1PERIPH_BASE) {
        uint32_t bit = 1 << ((periph_base - AHB1PERIPH_BASE) / 0x400);
        return (struct cline){.en=&RCC->AHB1ENR, .rst=&RCC->AHB1RSTR, .bit=bit};
    } else if (periph_base >= APB2PERIPH_BASE) {
        uint32_t bit = 1 << ((periph_base - APB2PERIPH_BASE) / 0x400);
        return (struct cline){.en=&RCC->APB2ENR, .rst=&RCC->APB2RSTR, .bit=bit};
    } else {
        uint32_t bit = 1 << ((periph_base - APB1PERIPH_BASE) / 0x400);
        return (struct cline){.en=&RCC->APB1ENR1, .rst=&RCC->APB1RSTR1, .bit=bit};
    }
}

 // Return the frequency of the given peripheral clock
uint32_t
get_pclock_frequency(uint32_t periph_base)
{
    return FREQ_PERIPH;
}

// Enable a GPIO peripheral clock
void
gpio_clock_enable(GPIO_TypeDef *regs)
{
    uint32_t rcc_pos = ((uint32_t)regs - AHB2PERIPH_BASE) / 0x400;
    if (rcc_pos < 32) {
        RCC->AHB2ENR1 |= 1 << rcc_pos;
        RCC->AHB2ENR1;
    } else {
        rcc_pos -= 32;
        RCC->AHB2ENR2 |= 1 << rcc_pos;
        RCC->AHB2ENR2;
    }
}

#if !CONFIG_STM32_CLOCK_REF_INTERNAL
DECL_CONSTANT_STR("RESERVE_PINS_crystal", "PH0,PH1");
#endif
        
// Main clock setup called at chip startup
static void
clock_setup(void)
{
    // Enable write access to backup domain
    RCC->AHB3ENR |= RCC_AHB3ENR_PWREN;
    PWR->DBPR |= PWR_DBPR_DBP;
    // Enable LSE oscillator input and wait for ready
    RCC->BDCR |= RCC_BDCR_LSEON;
    while (!(RCC->BDCR & RCC_BDCR_LSERDY))
        ;
    // Enable LSE system clock and wait for ready
    RCC->BDCR |= RCC_BDCR_LSESYSEN;
    while (!(RCC->BDCR & RCC_BDCR_LSESYSRDY))
        ;
    // Set PLL mode to apply to MSIS
    RCC->CR |= RCC_CR_MSIPLLSEL;
    // Enable PLL-mode of MSI clock to use as SYSCLK source
    RCC->CR |= RCC_CR_MSIPLLEN;
    // Set MSIK to range 0 @ 48MHz for USB
    // MODIFY_REG(RCC->ICSCR1, RCC_ICSCR1_MSIKRANGE, RCC_ICSCR1_MSIKRANGE_0);
    // Need to set MSIRGSEL for this to take effect

    // Disable PLL1 and wait until unready
    RCC->CR &= ~RCC_CR_PLL1ON;
    while (RCC->CR & RCC_CR_PLL1RDY)
        ;

    // Set MSIS as clock source for PLL1
    MODIFY_REG(RCC->PLL1CFGR, RCC_PLL1CFGR_PLL1SRC, RCC_PLL1CFGR_PLL1SRC_0);
    // Set voltage regulator range to 1 (high performance)
    PWR->VOSR |= PWR_VOSR_VOS;
    // Enable EPOD booster
    PWR->VOSR |= PWR_VOSR_BOOSTEN;
    // Wait for voltage regulator and EPOD booster to be ready
    while (!(PWR->VOSR & PWR_VOSR_VOSRDY) || !(PWR->VOSR & PWR_VOSR_BOOSTRDY))
        ;
    // Adjust flash read latency to match voltage range and frequency, 4 WS (5 CPU cycles)
    MODIFY_REG(FLASH->ACR, FLASH_ACR_LATENCY, FLASH_ACR_LATENCY_4WS);

    // Configure PLL and enable
    // Set PLL1 pre-divider
    RCC->PLL1CFGR &= ~RCC_PLL1CFGR_PLL1M;
    // Set PLL1 input frequency range
    MODIFY_REG(RCC->PLL1CFGR, RCC_PLL1CFGR_PLL1RGE, RCC_PLL1CFGR_PLL1RGE_0);
    // Set PLL1 N multiplication factor (N = 80)
    MODIFY_REG(RCC->PLL1DIVR, RCC_PLL1DIVR_PLL1N, (80 - 1) << RCC_PLL1DIVR_PLL1N_Pos);
    // Enable PLL1 ouptuts P, Q and R
    RCC->PLL1CFGR |= RCC_PLL1CFGR_PLL1PEN | RCC_PLL1CFGR_PLL1QEN | RCC_PLL1CFGR_PLL1REN;
    // Enable PLL1 and wait for ready
    RCC->CR |= RCC_CR_PLL1ON;
    while (!(RCC->CR & RCC_CR_PLL1RDY))
        ;

    // Set PLL1 as SYSCLK source and wait for stabilization
    RCC->CFGR1 |= RCC_CFGR1_SW;
    while (!((RCC->CFGR1 & RCC_CFGR1_SWS) == RCC_CFGR1_SWS))
        ;
}

/****************************************************************
 * Bootloader
 ****************************************************************/

// Handle reboot requests
void
bootloader_request(void)
{
    try_request_canboot();
    dfu_reboot();
}


 /****************************************************************
 * Startup
 ****************************************************************/

// Main entry point - called from armcm_boot.c:ResetHandler()
void
armcm_main(void)
{
    // Run SystemInit() and then restore VTOR
    SystemInit();
    SCB->VTOR = (uint32_t)VectorTable;

    // Reset peripheral clocks (for some bootloaders that don't)
    RCC->AHB1ENR = 0xD0000100;
    RCC->AHB2ENR1 = 0xC0000000;
    RCC->AHB2ENR2 = 0x0;
    RCC->AHB3ENR = 0x80000000;
    RCC->APB1ENR1 = 0x0;
    RCC->APB1ENR2 = 0x0;
    RCC->APB2ENR = 0x0;
    RCC->APB3ENR = 0x0;

    // dfu_reboot_check();
    
    // Enable AHB3EN register for PWR peripheral
    RCC->AHB3ENR |= RCC_AHB3ENR_PWREN;
    // Enable VDDIO2 for port G[15:2] domain
    PWR->SVMCR |= PWR_SVMCR_IO2SV;
    // Enable VDDA for ADC analog domain
    PWR->SVMCR |= PWR_SVMCR_ASV;

    // Setup clocks
    clock_setup();

    // Go to main task scheduler
    sched_main();
}
