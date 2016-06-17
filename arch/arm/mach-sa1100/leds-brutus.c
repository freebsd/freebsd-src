/*
 * linux/arch/arm/mach-sa1100/leds-brutus.c
 *
 * Copyright (C) 2000 Nicolas Pitre
 *
 * Brutus uses the LEDs as follows:
 *   - D3 (Green, GPIO9) - toggles state every 50 timer interrupts
 *   - D17 (Red, GPIO20) - on if system is not idle
 *   - D4 (Green, GPIO8) - misc function
 */
#include <linux/config.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/leds.h>
#include <asm/system.h>

#include "leds.h"


#define LED_STATE_ENABLED	1
#define LED_STATE_CLAIMED	2

static unsigned int led_state;
static unsigned int hw_led_state;

#define LED_D3		GPIO_GPIO(9)
#define LED_D4		GPIO_GPIO(8)
#define LED_D17		GPIO_GPIO(20)
#define LED_MASK	(LED_D3|LED_D4|LED_D17)

void brutus_leds_event(led_event_t evt)
{
	unsigned long flags;

	local_irq_save(flags);

	switch (evt) {
	case led_start:
		hw_led_state = LED_MASK;
		led_state = LED_STATE_ENABLED;
		break;

	case led_stop:
		led_state &= ~LED_STATE_ENABLED;
		break;

	case led_claim:
		led_state |= LED_STATE_CLAIMED;
		hw_led_state = LED_MASK;
		break;

	case led_release:
		led_state &= ~LED_STATE_CLAIMED;
		hw_led_state = LED_MASK;
		break;

#ifdef CONFIG_LEDS_TIMER
	case led_timer:
		if (!(led_state & LED_STATE_CLAIMED))
			hw_led_state ^= LED_D3;
		break;
#endif

#ifdef CONFIG_LEDS_CPU
	case led_idle_start:
		if (!(led_state & LED_STATE_CLAIMED))
			hw_led_state |= LED_D17;
		break;

	case led_idle_end:
		if (!(led_state & LED_STATE_CLAIMED))
			hw_led_state &= ~LED_D17;
		break;
#endif

	case led_green_on:
		hw_led_state &= ~LED_D4;
		break;

	case led_green_off:
		hw_led_state |= LED_D4;
		break;

	case led_amber_on:
		break;

	case led_amber_off:
		break;

	case led_red_on:
		if (led_state & LED_STATE_CLAIMED)
			hw_led_state &= ~LED_D17;
		break;

	case led_red_off:
		if (led_state & LED_STATE_CLAIMED)
			hw_led_state |= LED_D17;
		break;

	default:
		break;
	}

	if  (led_state & LED_STATE_ENABLED) {
		GPSR = hw_led_state;
		GPCR = hw_led_state ^ LED_MASK;
	}

	local_irq_restore(flags);
}
