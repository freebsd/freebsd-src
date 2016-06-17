
/*
 * linux/arch/arm/mach-sa1100/leds-frodo.c
 *
 * Author: Abraham van der Merwe <abraham@2d3d.co.za>
 *
 * This file contains the 2d3D, Inc. SA-1110 Development Board LED
 * event handler.
 *
 * Frodo LEDs
 *
 *   - led6 - toggles state every 50 timer interrupts (Heartbeat)
 *   - led7 - on if system is not idle (CPU load)
 */

#include <linux/config.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/leds.h>
#include <asm/system.h>

#include "leds.h"

#define led6_on() frodo_cpld_set (FRODO_CPLD_GENERAL,FRODO_LED2)
#define led6_off() frodo_cpld_clear (FRODO_CPLD_GENERAL,FRODO_LED2)
#define led6_invert() do {											\
		if ((frodo_cpld_read (FRODO_CPLD_GENERAL) & FRODO_LED2))	\
			led6_off ();											\
		else														\
			led6_on ();												\
	} while (0)

#define led7_on() frodo_cpld_set (FRODO_CPLD_GENERAL,FRODO_LED1)
#define led7_off() frodo_cpld_clear (FRODO_CPLD_GENERAL,FRODO_LED1)
#define led7_invert() do {						\
		if ((frodo_cpld_read (FRODO_CPLD_GENERAL) & FRODO_LED1))	\
			led7_off ();											\
		else														\
			led7_on ();												\
	} while (0)

static int claimed;

void frodo_leds_event (led_event_t evt)
{
   unsigned long flags;

   local_irq_save (flags);

   switch (evt)
	 {
#ifdef CONFIG_LEDS_CPU
		/* turn off CPU load LED */
	  case led_idle_start:
		if (!claimed) led7_off ();
		break;

		/* turn on CPU load LED */
	  case led_idle_end:
		if (!claimed) led7_on ();
		break;
#endif

#ifdef CONFIG_LEDS_TIMER
		/* toggle heartbeat LED */
	  case led_timer:
		if (!claimed) led6_invert ();
		break;
#endif

		/* start: turn on LEDs and set claimed to 0 */
	  case led_start:
		led6_on ();
		led7_on ();
		claimed = 0;
		break;

		/* stop: turn off LEDs */
	  case led_stop:
		led6_off ();
		led7_off ();
		break;

		/* override CPU load & timer LEDs */
	  case led_claim:
		claimed = 1;
		break;

		/* restore CPU load & timer LEDs */
	  case led_release:
		claimed = 0;
		break;

		/* direct LED access (must be previously claimed) */

		/* led7 */
	  case led_green_on:
		if (claimed) led7_on ();
		break;

	  case led_green_off:
		if (claimed) led7_off ();
		break;

		/* led6 -- at the moment this is actually also green */
	  case led_red_on:
		if (claimed) led6_on ();
		break;

	  case led_red_off:
		if (claimed) led6_off ();
		break;

	  default:
		break;
	}

   local_irq_restore (flags);
}

