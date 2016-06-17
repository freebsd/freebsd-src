/*
 * m8xx_wdt.c - MPC8xx watchdog driver
 *
 * Copyright (C) 2002 Florian Schirmer <jolt@tuxbox.org>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/8xx_immap.h>

static int wdt_timeout;

void
m8xx_wdt_reset(void)
{
	volatile immap_t *imap = (volatile immap_t *) IMAP_ADDR;

	imap->im_siu_conf.sc_swsr = 0x556c;	/* write magic1 */
	imap->im_siu_conf.sc_swsr = 0xaa39;	/* write magic2 */
}

static void
m8xx_wdt_interrupt(int irq, void *dev, struct pt_regs *regs)
{
	volatile immap_t *imap = (volatile immap_t *) IMAP_ADDR;

	m8xx_wdt_reset();

	imap->im_sit.sit_piscr |= PISCR_PS;	/* clear irq */
}

void __init
m8xx_wdt_handler_install(bd_t * binfo)
{
	volatile immap_t *imap = (volatile immap_t *) IMAP_ADDR;
	u32 pitc;
	u32 sypcr;
	u32 pitrtclk;

	sypcr = imap->im_siu_conf.sc_sypcr;

	if (!(sypcr & 0x04)) {
		printk(KERN_NOTICE "m8xx_wdt: wdt disabled (SYPCR: 0x%08X)\n",
		       sypcr);
		return;
	}

	m8xx_wdt_reset();

	printk(KERN_NOTICE
	       "m8xx_wdt: active wdt found (SWTC: 0x%04X, SWP: 0x%01X)\n",
	       (sypcr >> 16), sypcr & 0x01);

	wdt_timeout = (sypcr >> 16) & 0xFFFF;

	if (!wdt_timeout)
		wdt_timeout = 0xFFFF;

	if (sypcr & 0x01)
		wdt_timeout *= 2048;

	/*
	 * Fire trigger if half of the wdt ticked down 
	 */

	if (imap->im_sit.sit_rtcsc & RTCSC_38K)
		pitrtclk = 9600;
	else
		pitrtclk = 8192;

	if ((wdt_timeout) > (UINT_MAX / pitrtclk))
		pitc = wdt_timeout / binfo->bi_intfreq * pitrtclk / 2;
	else
		pitc = pitrtclk * wdt_timeout / binfo->bi_intfreq / 2;

	imap->im_sit.sit_pitc = pitc << 16;
	imap->im_sit.sit_piscr =
	    (mk_int_int_mask(PIT_INTERRUPT) << 8) | PISCR_PIE | PISCR_PTE;

	if (request_irq(PIT_INTERRUPT, m8xx_wdt_interrupt, 0, "watchdog", NULL))
		panic("m8xx_wdt: could not allocate watchdog irq!");

	printk(KERN_NOTICE
	       "m8xx_wdt: keep-alive trigger installed (PITC: 0x%04X)\n", pitc);

	wdt_timeout /= binfo->bi_intfreq;
}

int
m8xx_wdt_get_timeout(void)
{
	return wdt_timeout;
}
