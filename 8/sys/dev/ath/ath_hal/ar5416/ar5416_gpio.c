/*
 * Copyright (c) 2002-2009 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2008 Atheros Communications, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $FreeBSD$
 */
#include "opt_ah.h"

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"

#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416phy.h"

#define AR_GPIO_BIT(_gpio)	(1 << _gpio)

/*
 * Configure GPIO Output Mux control
 */
static void
cfgOutputMux(struct ath_hal *ah, uint32_t gpio, uint32_t type)
{
	int addr;
	uint32_t gpio_shift, reg;

	/* each MUX controls 6 GPIO pins */
	if (gpio > 11)
		addr = AR_GPIO_OUTPUT_MUX3;
	else if (gpio > 5)
		addr = AR_GPIO_OUTPUT_MUX2;
	else
		addr = AR_GPIO_OUTPUT_MUX1;

	/*
	 * 5 bits per GPIO pin. Bits 0..4 for 1st pin in that mux,
	 * bits 5..9 for 2nd pin, etc.
	 */
	gpio_shift = (gpio % 6) * 5;

	/*
	 * From Owl to Merlin 1.0, the value read from MUX1 bit 4 to bit
	 * 9 are wrong.  Here is hardware's coding:
	 * PRDATA[4:0] <= gpio_output_mux[0];
	 * PRDATA[9:4] <= gpio_output_mux[1];
	 *	<==== Bit 4 is used by both gpio_output_mux[0] [1].
	 * Currently the max value for gpio_output_mux[] is 6. So bit 4
	 * will never be used.  So it should be fine that bit 4 won't be
	 * able to recover.
	 */
	reg = OS_REG_READ(ah, addr);
	if (addr == AR_GPIO_OUTPUT_MUX1 && !AR_SREV_MERLIN_20_OR_LATER(ah))
		reg = ((reg & 0x1F0) << 1) | (reg & ~0x1F0);
	reg &= ~(0x1f << gpio_shift);
	reg |= type << gpio_shift;
	OS_REG_WRITE(ah, addr, reg);
}

/*
 * Configure GPIO Output lines
 */
HAL_BOOL
ar5416GpioCfgOutput(struct ath_hal *ah, uint32_t gpio, HAL_GPIO_MUX_TYPE type)
{
	uint32_t gpio_shift, reg;

	HALASSERT(gpio < AH_PRIVATE(ah)->ah_caps.halNumGpioPins);

	/* NB: type maps directly to hardware */
	cfgOutputMux(ah, gpio, type);
	gpio_shift = gpio << 1;			/* 2 bits per output mode */

	reg = OS_REG_READ(ah, AR_GPIO_OE_OUT);
	reg &= ~(AR_GPIO_OE_OUT_DRV << gpio_shift);
	reg |= AR_GPIO_OE_OUT_DRV_ALL << gpio_shift;
	OS_REG_WRITE(ah, AR_GPIO_OE_OUT, reg);

	return AH_TRUE;
}
 
/*
 * Configure GPIO Input lines
 */
HAL_BOOL
ar5416GpioCfgInput(struct ath_hal *ah, uint32_t gpio)
{
	uint32_t gpio_shift, reg;

	HALASSERT(gpio < AH_PRIVATE(ah)->ah_caps.halNumGpioPins);

	/* TODO: configure input mux for AR5416 */
	/* If configured as input, set output to tristate */
	gpio_shift = gpio << 1;

	reg = OS_REG_READ(ah, AR_GPIO_OE_OUT);
	reg &= ~(AR_GPIO_OE_OUT_DRV << gpio_shift);
	reg |= AR_GPIO_OE_OUT_DRV_ALL << gpio_shift;
	OS_REG_WRITE(ah, AR_GPIO_OE_OUT, reg);

	return AH_TRUE;
}

/*
 * Once configured for I/O - set output lines
 */
HAL_BOOL
ar5416GpioSet(struct ath_hal *ah, uint32_t gpio, uint32_t val)
{
	uint32_t reg;

	HALASSERT(gpio < AH_PRIVATE(ah)->ah_caps.halNumGpioPins);

	reg = OS_REG_READ(ah, AR_GPIO_IN_OUT);
	if (val & 1)
		reg |= AR_GPIO_BIT(gpio);
	else 
		reg &= ~AR_GPIO_BIT(gpio);
	OS_REG_WRITE(ah, AR_GPIO_IN_OUT, reg);	
	return AH_TRUE;
}

/*
 * Once configured for I/O - get input lines
 */
uint32_t
ar5416GpioGet(struct ath_hal *ah, uint32_t gpio)
{
	uint32_t bits;

	if (gpio >= AH_PRIVATE(ah)->ah_caps.halNumGpioPins)
		return 0xffffffff;
	/*
	 * Read output value for all gpio's, shift it,
	 * and verify whether the specific bit is set.
	 */
	if (AR_SREV_KITE_10_OR_LATER(ah))
		bits = MS(OS_REG_READ(ah, AR_GPIO_IN_OUT), AR9285_GPIO_IN_VAL);
	else if (AR_SREV_MERLIN_10_OR_LATER(ah))
		bits = MS(OS_REG_READ(ah, AR_GPIO_IN_OUT), AR928X_GPIO_IN_VAL);
	else
		bits = MS(OS_REG_READ(ah, AR_GPIO_IN_OUT), AR_GPIO_IN_VAL);
	return ((bits & AR_GPIO_BIT(gpio)) != 0);
}

/*
 * Set the GPIO Interrupt Sync and Async interrupts are both set/cleared.
 * Async GPIO interrupts may not be raised when the chip is put to sleep.
 */
void
ar5416GpioSetIntr(struct ath_hal *ah, u_int gpio, uint32_t ilevel)
{
	uint32_t val, mask;

	HALASSERT(gpio < AH_PRIVATE(ah)->ah_caps.halNumGpioPins);

	if (ilevel == HAL_GPIO_INTR_DISABLE) {
		val = MS(OS_REG_READ(ah, AR_INTR_ASYNC_ENABLE),
			 AR_INTR_ASYNC_ENABLE_GPIO) &~ AR_GPIO_BIT(gpio);
		OS_REG_RMW_FIELD(ah, AR_INTR_ASYNC_ENABLE,
		    AR_INTR_ASYNC_ENABLE_GPIO, val);

		mask = MS(OS_REG_READ(ah, AR_INTR_ASYNC_MASK),
			  AR_INTR_ASYNC_MASK_GPIO) &~ AR_GPIO_BIT(gpio);
		OS_REG_RMW_FIELD(ah, AR_INTR_ASYNC_MASK,
		    AR_INTR_ASYNC_MASK_GPIO, mask);

		/* Clear synchronous GPIO interrupt registers and pending interrupt flag */
		val = MS(OS_REG_READ(ah, AR_INTR_SYNC_ENABLE),
			 AR_INTR_SYNC_ENABLE_GPIO) &~ AR_GPIO_BIT(gpio);
		OS_REG_RMW_FIELD(ah, AR_INTR_SYNC_ENABLE,
		    AR_INTR_SYNC_ENABLE_GPIO, val);

		mask = MS(OS_REG_READ(ah, AR_INTR_SYNC_MASK),
			  AR_INTR_SYNC_MASK_GPIO) &~ AR_GPIO_BIT(gpio);
		OS_REG_RMW_FIELD(ah, AR_INTR_SYNC_MASK,
		    AR_INTR_SYNC_MASK_GPIO, mask);

		val = MS(OS_REG_READ(ah, AR_INTR_SYNC_CAUSE),
			 AR_INTR_SYNC_ENABLE_GPIO) | AR_GPIO_BIT(gpio);
		OS_REG_RMW_FIELD(ah, AR_INTR_SYNC_CAUSE,
		    AR_INTR_SYNC_ENABLE_GPIO, val);
	} else {
		val = MS(OS_REG_READ(ah, AR_GPIO_INTR_POL),
			 AR_GPIO_INTR_POL_VAL);
		if (ilevel == HAL_GPIO_INTR_HIGH) {
			/* 0 == interrupt on pin high */
			val &= ~AR_GPIO_BIT(gpio);
		} else if (ilevel == HAL_GPIO_INTR_LOW) {
			/* 1 == interrupt on pin low */
			val |= AR_GPIO_BIT(gpio);
		}
		OS_REG_RMW_FIELD(ah, AR_GPIO_INTR_POL,
		    AR_GPIO_INTR_POL_VAL, val);

		/* Change the interrupt mask. */
		val = MS(OS_REG_READ(ah, AR_INTR_ASYNC_ENABLE),
			 AR_INTR_ASYNC_ENABLE_GPIO) | AR_GPIO_BIT(gpio);
		OS_REG_RMW_FIELD(ah, AR_INTR_ASYNC_ENABLE,
		    AR_INTR_ASYNC_ENABLE_GPIO, val);

		mask = MS(OS_REG_READ(ah, AR_INTR_ASYNC_MASK),
			  AR_INTR_ASYNC_MASK_GPIO) | AR_GPIO_BIT(gpio);
		OS_REG_RMW_FIELD(ah, AR_INTR_ASYNC_MASK,
		    AR_INTR_ASYNC_MASK_GPIO, mask);

		/* Set synchronous GPIO interrupt registers as well */
		val = MS(OS_REG_READ(ah, AR_INTR_SYNC_ENABLE),
			 AR_INTR_SYNC_ENABLE_GPIO) | AR_GPIO_BIT(gpio);
		OS_REG_RMW_FIELD(ah, AR_INTR_SYNC_ENABLE,
		    AR_INTR_SYNC_ENABLE_GPIO, val);

		mask = MS(OS_REG_READ(ah, AR_INTR_SYNC_MASK),
			  AR_INTR_SYNC_MASK_GPIO) | AR_GPIO_BIT(gpio);
		OS_REG_RMW_FIELD(ah, AR_INTR_SYNC_MASK,
		    AR_INTR_SYNC_MASK_GPIO, mask);
	}
	AH5416(ah)->ah_gpioMask = mask;		/* for ar5416SetInterrupts */
}
