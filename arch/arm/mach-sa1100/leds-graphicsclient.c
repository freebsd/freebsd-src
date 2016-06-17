/*
 *  linux/arch/arm/mach-sa1100/leds-graphicsclient.c
 *
 * GraphicsClient Plus LEDs support
 * Woojung Huh, Feb 13, 2001
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

#define LED_TIMER       ADS_LED0  /* green heartbeat */
#define LED_USER        ADS_LED1  /* amber, boots to on */
#define LED_IDLE        ADS_LED2  /* red has the idle led, if any */

#define LED_MASK	(ADS_LED0|ADS_LED1|ADS_LED2)

void graphicsclient_leds_event(led_event_t evt)
{
	unsigned long flags;

	local_irq_save(flags);

	switch (evt) {
	case led_start:
		hw_led_state = 0;        /* gc leds are positive logic */
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
			hw_led_state ^= LED_TIMER;
		break;
#endif

#ifdef CONFIG_LEDS_CPU
	case led_idle_start:
		if (!(led_state & LED_STATE_CLAIMED))
			hw_led_state &= ~LED_IDLE;
		break;

	case led_idle_end:
		if (!(led_state & LED_STATE_CLAIMED))
			hw_led_state |= LED_IDLE;
		break;
#endif

	case led_green_on:
		break;

	case led_green_off:
		break;

	case led_amber_on:
		hw_led_state |= LED_USER;
		break;

	case led_amber_off:
		hw_led_state &= ~LED_USER;
		break;

	case led_red_on:
		break;

	case led_red_off:
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
