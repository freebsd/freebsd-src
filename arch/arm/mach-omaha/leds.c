/*
 *  linux/arch/arm/mach-omaha/leds.c
 *
 *  Omaha LED control routines
 *
 *  Copyright (C) 1999-2002 ARM Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/leds.h>
#include <asm/system.h>
#include <asm/mach-types.h>

static int saved_leds;

static void omaha_leds_event(led_event_t ledevt)
{
	unsigned long flags;			      
	unsigned int ctrl = IO_ADDRESS(PLAT_DBG_LEDS);
	unsigned int leds;
	
	// yup, change the LEDs
	local_irq_save(flags);

	switch(ledevt) {
	case led_idle_start:
		leds = __raw_readl(ctrl);
		leds |= GREEN_LED;
		__raw_writel(leds,ctrl);
		break;

	case led_idle_end:
		leds = __raw_readl(ctrl);
		leds &= ~GREEN_LED;
		__raw_writel(leds,ctrl);
		break;

	case led_timer:
		leds = __raw_readl(ctrl);
		leds ^= YELLOW_LED;
		__raw_writel(leds,ctrl);
		break;

	case led_red_on:
		leds = __raw_readl(ctrl);
		leds |= RED_LED;
		__raw_writel(leds,ctrl);
		break;

	case led_red_off:
		leds = __raw_readl(ctrl);
		leds &= ~RED_LED;
		__raw_writel(leds,ctrl);
		break;

	default:
		break;
	}

	local_irq_restore(flags);
}

static int __init leds_init(void)
{
	if (machine_is_omaha())
		leds_event = omaha_leds_event;

	return 0;
}

__initcall(leds_init);
