/*
 * linux/arch/arm/mach-sa1100/leds-flexanet.c
 *
 * by Jordi Colomer <jco@ict.es>
 *
 * Flexanet LEDs
 *
 *   - Red   - toggles state every 50 timer interrupts (Heartbeat)
 *   - Green - on if system is not idle (CPU load)
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
static unsigned int hw_led_bcr;
static unsigned int hw_led_gpio;


void flexanet_leds_event(led_event_t evt)
{
	unsigned long flags;

	local_irq_save(flags);

	switch (evt) {
	case led_start:
		/* start using LEDs and enable its hardware */
		hw_led_bcr = FHH_BCR_LED_GREEN;
		hw_led_gpio = GPIO_LED_RED;
		led_state = LED_STATE_ENABLED;
		break;

	case led_stop:
		/* disable LED h/w */
		led_state &= ~LED_STATE_ENABLED;
		break;

	case led_claim:
		/* select LEDs for direct access */
		led_state |= LED_STATE_CLAIMED;
		hw_led_bcr = 0;
		hw_led_gpio = 0;
		break;

	case led_release:
		/* release LEDs from direct access */
		led_state &= ~LED_STATE_CLAIMED;
		hw_led_bcr = 0;
		hw_led_gpio = 0;
		break;

#ifdef CONFIG_LEDS_TIMER
	case led_timer:
		/* toggle heartbeat LED */
		if (!(led_state & LED_STATE_CLAIMED))
			hw_led_gpio ^= GPIO_LED_RED;
		break;
#endif

#ifdef CONFIG_LEDS_CPU
	case led_idle_start:
		/* turn off CPU load LED */
		if (!(led_state & LED_STATE_CLAIMED))
			hw_led_bcr &= ~FHH_BCR_LED_GREEN;
		break;

	case led_idle_end:
		/* turn on CPU load LED */
		if (!(led_state & LED_STATE_CLAIMED))
			hw_led_bcr |= FHH_BCR_LED_GREEN;
		break;
#endif

	case led_halted:
		break;


	/* direct LED access (must be previously claimed) */
	case led_green_on:
		if (led_state & LED_STATE_CLAIMED)
			hw_led_bcr |= FHH_BCR_LED_GREEN;
		break;

	case led_green_off:
		if (led_state & LED_STATE_CLAIMED)
			hw_led_bcr &= ~FHH_BCR_LED_GREEN;
		break;

	case led_amber_on:
		break;

	case led_amber_off:
		break;

	case led_red_on:
		if (led_state & LED_STATE_CLAIMED)
			hw_led_gpio |= GPIO_LED_RED;
		break;

	case led_red_off:
		if (led_state & LED_STATE_CLAIMED)
			hw_led_gpio &= ~GPIO_LED_RED;
		break;

	default:
		break;
	}

	if  (led_state & LED_STATE_ENABLED)
	{
		/* update LEDs */
		FHH_BCR = flexanet_BCR = (flexanet_BCR & ~FHH_BCR_LED_GREEN) | hw_led_bcr;
		GPSR = hw_led_gpio;
		GPCR = hw_led_gpio ^ GPIO_LED_RED;
	}

	local_irq_restore(flags);
}

