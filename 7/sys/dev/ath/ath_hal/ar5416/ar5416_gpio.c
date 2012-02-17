/*
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
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
 * $Id: ar5416_gpio.c,v 1.3 2008/11/10 04:08:04 sam Exp $
 */
#include "opt_ah.h"

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"
#ifdef AH_DEBUG
#include "ah_desc.h"			/* NB: for HAL_PHYERR* */
#endif

#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416phy.h"

#define	AR_NUM_GPIO	6		/* 6 GPIO pins */
#define AR_GPIO_BIT(_gpio)	(1 << _gpio)

/*
 * Configure GPIO Output lines
 */
HAL_BOOL
ar5416GpioCfgOutput(struct ath_hal *ah, uint32_t gpio)
{
	HALASSERT(gpio < AR_NUM_GPIO);
	OS_REG_CLR_BIT(ah, AR_GPIO_INTR_OUT, AR_GPIO_BIT(gpio));
	return AH_TRUE;
}

/*
 * Configure GPIO Input lines
 */
HAL_BOOL
ar5416GpioCfgInput(struct ath_hal *ah, uint32_t gpio)
{
	HALASSERT(gpio < AR_NUM_GPIO);
	OS_REG_SET_BIT(ah, AR_GPIO_INTR_OUT, AR_GPIO_BIT(gpio));
	return AH_TRUE;
}

/*
 * Once configured for I/O - set output lines
 */
HAL_BOOL
ar5416GpioSet(struct ath_hal *ah, uint32_t gpio, uint32_t val)
{
	uint32_t reg;

	HALASSERT(gpio < AR_NUM_GPIO);
	reg = MS(OS_REG_READ(ah, AR_GPIO_INTR_OUT), AR_GPIO_OUT_VAL);
	if (val & 1)
		reg |= AR_GPIO_BIT(gpio);
	else 
		reg &= ~AR_GPIO_BIT(gpio);
		
	OS_REG_RMW_FIELD(ah, AR_GPIO_INTR_OUT, AR_GPIO_OUT_VAL, reg);	
	return AH_TRUE;
}

/*
 * Once configured for I/O - get input lines
 */
uint32_t
ar5416GpioGet(struct ath_hal *ah, uint32_t gpio)
{
	if (gpio >= AR_NUM_GPIO)
		return 0xffffffff;
	return ((OS_REG_READ(ah, AR_GPIO_IN) & AR_GPIO_BIT(gpio)) >> gpio);
}

/*
 * Set the GPIO Interrupt
 */
void
ar5416GpioSetIntr(struct ath_hal *ah, u_int gpio, uint32_t ilevel)
{
	uint32_t val;
	
	HALASSERT(gpio < AR_NUM_GPIO);
	/* XXX bounds check gpio */
	val = MS(OS_REG_READ(ah, AR_GPIO_INTR_OUT), AR_GPIO_INTR_CTRL);
	if (ilevel)		/* 0 == interrupt on pin high */
		val &= ~AR_GPIO_BIT(gpio);
	else			/* 1 == interrupt on pin low */
		val |= AR_GPIO_BIT(gpio);
	OS_REG_RMW_FIELD(ah, AR_GPIO_INTR_OUT, AR_GPIO_INTR_CTRL, val);

	/* Change the interrupt mask. */
	val = MS(OS_REG_READ(ah, AR_INTR_ASYNC_ENABLE), AR_INTR_GPIO);
	val |= AR_GPIO_BIT(gpio);
	OS_REG_RMW_FIELD(ah, AR_INTR_ASYNC_ENABLE, AR_INTR_GPIO, val);

	val = MS(OS_REG_READ(ah, AR_INTR_ASYNC_MASK), AR_INTR_GPIO);
	val |= AR_GPIO_BIT(gpio);
	OS_REG_RMW_FIELD(ah, AR_INTR_ASYNC_MASK, AR_INTR_GPIO, val);	
}
