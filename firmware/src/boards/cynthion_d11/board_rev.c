/*
 * Board revision detection for Cynthion.
 *
 * This file is part of Apollo.
 *
 * Copyright (c) 2024 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>

#include "apollo_board.h"
#include "board_rev.h"

#include <hpl_pm_config.h>
#include <hpl/pm/hpl_pm_base.h>
#include <hpl/gclk/hpl_gclk_base.h>


#if (_BOARD_REVISION_DETECT_ == 1)

static uint16_t revision = CYNTHION_REV_UNKNOWN;
static bool gsg_production = false;

/**
 * Detect hardware revision using Cynthion pin straps.
 */
void detect_hardware_revision(void)
{
    _pm_enable_bus_clock(PM_BUS_APBC, ADC);
    _gclk_enable_channel(ADC_GCLK_ID, CONF_GCLK_ADC_SRC);

    // Initialize ADC device registers.
    uint16_t calib_reg = ADC_CALIB_BIAS_CAL((*(uint32_t *)ADC_FUSES_BIASCAL_ADDR >> ADC_FUSES_BIASCAL_Pos))
                | ADC_CALIB_LINEARITY_CAL((*(uint64_t *)ADC_FUSES_LINEARITY_0_ADDR >> ADC_FUSES_LINEARITY_0_Pos));

    hri_adc_wait_for_sync(ADC);
    hri_adc_write_CTRLA_reg(ADC, ADC_CTRLA_SWRST);
    hri_adc_wait_for_sync(ADC);

    hri_adc_write_CALIB_reg(ADC, calib_reg);
    hri_adc_write_REFCTRL_reg(ADC, ADC_REFCTRL_REFCOMP | ADC_REFCTRL_REFSEL_INTVCC1);
    hri_adc_write_CTRLB_reg(ADC, ADC_CTRLB_PRESCALER_DIV512 | ADC_CTRLB_RESSEL_12BIT);
    hri_adc_write_INPUTCTRL_reg(ADC, ADC_INPUTCTRL_GAIN_DIV2 | ADC_INPUTCTRL_MUXPOS_PIN5 | ADC_INPUTCTRL_MUXNEG_GND);
    hri_adc_write_CTRLA_reg(ADC, ADC_CTRLA_ENABLE);

    // Configure relevant GPIO to function as an ADC input.
    gpio_set_pin_function(PIN_PA07, PINMUX_PA07B_ADC_AIN5);

    // Retrieve a single ADC reading.
    hri_adc_set_SWTRIG_START_bit(ADC);
	while (!hri_adc_get_interrupt_RESRDY_bit(ADC));
	uint16_t reading = hri_adc_read_RESULT_reg(ADC);

    // Convert ADC measurement to per mille of the reference voltage.
    uint32_t permille = (((uint32_t)reading * 1000) + 20480) >> 12;
    if (permille > 510) {
        permille = 1000 - permille;
        gsg_production = true;
    }

    /*
    hardware version  |  percent of +3V3
    ___________________________________________
    0.6               |  0-1
    future versions   |  2-19
    1.4               |  21-22
    1.3               |  23-24
    1.2               |  25-26
    1.0               |  27-28
    1.1               |  29-31
    reserved          |  32-48
    0.7               |  49-51
    reserved          |  52-68
    1.1-production    |  69-71
    future-production |  72-100
    */

    // Identify the board revision by comparing against expected thresholds.
    struct {
        uint16_t version;
        uint16_t threshold;
    } revisions[] = {
        { CYNTHION_REV_0_6,         10 },
        { CYNTHION_REV_UNKNOWN,    195 },
        { CYNTHION_REV_1_4,        220 },
        { CYNTHION_REV_1_3,        240 },
        { CYNTHION_REV_1_2,        260 },
        { CYNTHION_REV_1_0,        280 },
        { CYNTHION_REV_1_1,        310 },
        { CYNTHION_REV_UNKNOWN,    480 },
        { CYNTHION_REV_0_7,        510 },
    };

    int i = 0;
    while (permille > revisions[i].threshold) { ++i; }
    revision = revisions[i].version;
}

/**
 * Returns the board revision in bcdDevice format.
 */
uint16_t get_board_revision(void)
{
    return revision;
}

/**
 * Return the manufacturer string.
 */
const char *get_manufacturer_string(void)
{
        return (gsg_production) ? "Great Scott Gadgets" : "Apollo Project";
}

/**
 * Return the product string.
 */
const char *get_product_string(void)
{
        return (gsg_production) ? "Cynthion Apollo Debugger" : "Apollo Debugger";
}

#endif
