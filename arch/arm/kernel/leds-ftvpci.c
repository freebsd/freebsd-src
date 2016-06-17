/*
 *  linux/arch/arm/kernel/leds-ftvpci.c
 *
 *  Copyright (C) 1999 FutureTV Labs Ltd
 */

#include <linux/module.h>

#include <asm/hardware.h>
#include <asm/leds.h>
#include <asm/system.h>
#include <asm/io.h>

static void ftvpci_leds_event(led_event_t ledevt)
{
	static int led_state = 0;

	switch(ledevt) {
	case led_timer:
		led_state ^= 1;
		raw_writeb(0x1a | led_state, INTCONT_BASE);
		break;

	default:
		break;
	}
}

void (*leds_event)(led_event_t) = ftvpci_leds_event;

EXPORT_SYMBOL(leds_event);
