// ADC functions on STM32
//
// Copyright (C) 2019-2020  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "board/irq.h" // irq_save
#include "board/misc.h" // timer_from_us
#include "command.h" // shutdown
#include "compiler.h" // ARRAY_SIZE
#include "generic/armcm_timer.h" // udelay
#include "gpio.h" // gpio_adc_setup
#include "internal.h" // GPIO
#include "sched.h" // sched_shutdown

DECL_CONSTANT("ADC_MAX", 4095);

#define ADC_TEMPERATURE_PIN 0xfe
//DECL_ENUMERATION("pin", "ADC_TEMPERATURE", ADC_TEMPERATURE_PIN);

static const uint8_t adc1_pins[] = {
    GPIO('C',  0), GPIO('C',  1), GPIO('C',  2), GPIO('C',  3),
    GPIO('A',  0), GPIO('A',  1), GPIO('A',  2), GPIO('A',  3),
    GPIO('A',  4), GPIO('A',  5), GPIO('A',  6), GPIO('A',  7),
    GPIO('C',  4), GPIO('C',  5), GPIO('B',  0), GPIO('B',  1),
    GPIO('B',  2),          0x00, //ADC_TEMPERATURE_PIN
};

static const uint8_t adc4_pins[] = {
    GPIO('C',  0), GPIO('C',  1), GPIO('C',  2), GPIO('C',  3),
    GPIO('F', 14), GPIO('F', 15), GPIO('G',  0), GPIO('G',  1),
    GPIO('A',  4), GPIO('A',  5), GPIO('A',  6),          0x00,
             0x00,          0x00, GPIO('D', 11), GPIO('D', 12),
    GPIO('D', 13), GPIO('B',  0), GPIO('B',  1), GPIO('A',  7),
             0x00, GPIO('C',  4), GPIO('C',  5)
};

// Setup and calibrate ADC1
static void
adc1_setup(ADC_TypeDef *adc)
{
    // Calibrate ADC
    // Disable deep-power-down
    adc->CR &= ~ADC_CR_DEEPPWD;
    // Enable the voltage regulator and wait for ready
    adc->CR |= ADC_CR_ADVREGEN;
    // Wait until LDO is ready
    while (!(adc->ISR & ADC_ISR_LDORDY))
        ;
    // Make sure ADC is disabled
    adc->CR &= ~ADC_CR_ADEN;
    // Enable linearity calibration
    // adc->CR |= ADC_CR_ADCALLIN;
    // Start calibration and wait until completed
    adc->CR |= ADC_CR_ADCAL;
    while (adc->CR & ADC_CR_ADCAL)
        ;

    // Set sampling time of ADC channels
    uint32_t aticks = 4; // 0b100 (84 cycles): ~5us sample time @ 16MHz
    // ADC1 supports per-channel sample time configuration
    adc->SMPR1 = ((aticks <<  0) | (aticks <<  3) | (aticks <<  6) | (aticks <<  9)
                | (aticks << 12) | (aticks << 15) | (aticks << 18) | (aticks << 21)
                | (aticks << 24) | (aticks << 27));
    adc->SMPR2 = ((aticks <<  0) | (aticks <<  3) | (aticks <<  6) | (aticks <<  9)
                | (aticks << 12) | (aticks << 15) | (aticks << 18) | (aticks << 21)
                | (aticks << 24) | (aticks << 27));
}

// Setup and calibrate ADC4
static void
adc4_setup(ADC_TypeDef *adc)
{
    // Calibrate ADC
    // Disable ADC
    adc->CR &= ~ADC_CR_ADEN;
    // Enable the voltage regulator
    adc->CR = ADC_CR_ADVREGEN;
    // Disable auto-off and deep-power-down
    adc->PWRR &= ~(ADC4_PWRR_AUTOFF | ADC4_PWRR_DPD);
    // Disable DMA transfer
    adc->CFGR1 &= ~ADC4_CFGR1_DMAEN;
    // Wait until LDO is ready
    while (!(adc->ISR & ADC_ISR_LDORDY))
        ;
    // Start calibration and wait until completed
    adc->CR |= ADC_CR_ADCAL;
    while (adc->CR & ADC_CR_ADCAL)
        ;

    // Set sampling time of ADC channels
    uint32_t aticks;
    aticks = 6; // 0b110 (79.5 cycles): ~5us sample time @ 16MHz
    // ADC4 supports choosing 1 of 2 configured sample times for each channel
    adc->SMPR1 = (aticks | ADC4_SMPR_SMPSEL_Msk);
}

struct gpio_adc
gpio_adc_setup(uint32_t pin)
{
    // Match pin to ADC and channel
    int chan;
    ADC_TypeDef *adc;
    ADC_Common_TypeDef *adc_common;
    // Find pin in adc1_pins table
    for (chan = 0; chan < ARRAY_SIZE(adc1_pins); chan++) {
        if (adc1_pins[chan] == pin) {
            adc = ADC1;
            adc_common = ADC12_COMMON;
            break;
        }
    }
    // If pin is not an ADC1 pin, check if ADC4
    if (adc != ADC1) {
        // Find pin in adc4_pins table
        for (chan = 0; chan < ARRAY_SIZE(adc4_pins); chan++) {
            if (chan >= ARRAY_SIZE(adc4_pins))
                shutdown("Not a valid ADC pin");
            if (adc4_pins[chan] == pin) {
                adc = ADC4;
                adc_common = ADC4_COMMON;
                break;
            }
        }
    }

    // adc = ADC1;
    // adc_base = ADC1_BASE;
    // chan = 0;


    // Enable the ADC clock if not already
    if (!is_enabled_pclock((uint32_t)adc)) {
        enable_pclock((uint32_t)adc);
        (adc == ADC1) ? adc1_setup(adc) : adc4_setup(adc);

        // Enable ADC
        // Clear the ready bit
        adc->ISR = ADC_ISR_ADRDY;
        // Enable and wait for ready
        adc->CR |= ADC_CR_ADEN;
        while (!(adc->ISR & ADC_ISR_ADRDY))
            ;
    }

    if (pin == ADC_TEMPERATURE_PIN)
        ADC12_COMMON->CCR |= ADC_CCR_VSENSEEN;
    else
        gpio_peripheral(pin, GPIO_ANALOG, 0);

    return (struct gpio_adc){ .adc = adc, .chan = chan };
}

// Try to sample a value. Returns zero if sample ready, otherwise
// returns the number of clock ticks the caller should wait before
// retrying this function.
uint32_t
gpio_adc_sample(struct gpio_adc g)
{
    ADC_TypeDef *adc = g.adc;
    if (adc->CR & ADC_CR_ADSTART)
        goto need_delay;
    if (adc->ISR & ADC_ISR_EOC) {
        if (adc->SQR1 == (g.chan << ADC_SQR1_SQ1_Pos))
            return 0;
        goto need_delay;
    }

    // Start sample
    adc->SQR1 = (g.chan << ADC_SQR1_SQ1_Pos);
    adc->CR |= ADC_CR_ADSTART;

    need_delay:
        return timer_from_us(20);
}

// Read a value; use only after gpio_adc_sample() returns zero
uint16_t
gpio_adc_read(struct gpio_adc g)
{
    ADC_TypeDef *adc = g.adc;
    return adc->DR;
}

// Cancel a sample that may have been started with gpio_adc_sample()
void
gpio_adc_cancel_sample(struct gpio_adc g)
{
    ADC_TypeDef *adc = g.adc;
    irqstatus_t flag = irq_save();
    if (adc->SQR1 == (g.chan << ADC_SQR1_SQ1_Pos)) {
        uint32_t cr = adc->CR;
        if (cr & ADC_CR_ADSTART)
            adc->CR = (cr & ~ADC_CR_ADSTART) | ADC_CR_ADSTP;
        if (adc->ISR & ADC_ISR_EOC)
            gpio_adc_read(g);
    }
    irq_restore(flag);
}
