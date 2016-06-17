/*
 * linux/arch/arm/mach-sa1100/badge4.c
 *
 * BadgePAD 4 specific initialization
 *
 *   Tim Connors <connors@hpl.hp.com>
 *   Christopher Hoover <ch@hpl.hp.com>
 *
 * Copyright (C) 2002 Hewlett-Packard Company
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/tty.h>
#include <linux/errno.h>

#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/arch/irqs.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/hardware/sa1111.h>
#include <asm/mach/serial_sa1100.h>

#include "generic.h"
#include "sa1111.h"

static int __init badge4_sa1111_init(void)
{
	int ret;

	/*
	 * Ensure that the memory bus request/grant signals are setup,
	 * and the grant is held in its inactive state
	 */
	sa1110_mb_disable();

	/*
	 * Probe for SA1111.
	 */
	ret = sa1111_probe(BADGE4_SA1111_BASE);
	if (ret < 0)
		return ret;

	/*
	 * We found it.  Wake the chip up.
	 */
	sa1111_wake();

	/*
	 * The SDRAM configuration of the SA1110 and the SA1111 must
	 * match.  This is very important to ensure that SA1111 accesses
	 * don't corrupt the SDRAM.  Note that this ungates the SA1111's
	 * MBGNT signal, so we must have called sa1110_mb_disable()
	 * beforehand.
	 */
	sa1111_configure_smc(1,
			     FExtr(MDCNFG, MDCNFG_SA1110_DRAC0),
			     FExtr(MDCNFG, MDCNFG_SA1110_TDL0));

	/*
	 * We only need to turn on DCLK whenever we want to use the
	 * DMA.  It can otherwise be held firmly in the off position.
	 */
	SKPCR |= SKPCR_DCLKEN;

	/*
	 * Enable the SA1110 memory bus request and grant signals.
	 */
	sa1110_mb_enable();

	set_GPIO_IRQ_edge(BADGE4_GPIO_INT_1111, GPIO_RISING_EDGE);
	sa1111_init_irq(BADGE4_IRQ_GPIO_SA1111);

	return 0;
}

static int __init badge4_init(void)
{
	int ret;

	if (!machine_is_badge4())
		return -ENODEV;

	ret = badge4_sa1111_init();
	if (ret < 0)
		printk(KERN_ERR __FUNCTION__
		       ": SA-1111 initialization failed (%d)\n", ret);

	/* N.B, according to rmk this is the singular place that GPDR
           should be set */

	/* Video expansion */
	GPCR  = (BADGE4_GPIO_INT_VID | BADGE4_GPIO_LGP2 | BADGE4_GPIO_LGP3 |
		 BADGE4_GPIO_LGP4 | BADGE4_GPIO_LGP5 | BADGE4_GPIO_LGP6 |
		 BADGE4_GPIO_LGP7 | BADGE4_GPIO_LGP8 | BADGE4_GPIO_LGP9 |
		 BADGE4_GPIO_GPA_VID | BADGE4_GPIO_GPB_VID |
		 BADGE4_GPIO_GPC_VID);
	GPDR |= (BADGE4_GPIO_INT_VID | BADGE4_GPIO_LGP2 | BADGE4_GPIO_LGP3 |
		 BADGE4_GPIO_LGP4 | BADGE4_GPIO_LGP5 | BADGE4_GPIO_LGP6 |
		 BADGE4_GPIO_LGP7 | BADGE4_GPIO_LGP8 | BADGE4_GPIO_LGP9 |
		 BADGE4_GPIO_GPA_VID | BADGE4_GPIO_GPB_VID |
		 BADGE4_GPIO_GPC_VID);

	/* SDRAM SPD i2c */
	GPCR  = (BADGE4_GPIO_SDSDA | BADGE4_GPIO_SDSCL);
	GPDR |= (BADGE4_GPIO_SDSDA | BADGE4_GPIO_SDSCL);

	/* uart */
	GPCR  = (BADGE4_GPIO_UART_HS1 | BADGE4_GPIO_UART_HS2);
	GPDR |= (BADGE4_GPIO_UART_HS1 | BADGE4_GPIO_UART_HS2);

	/* drives CPLD muxsel0 input */
	GPCR  = BADGE4_GPIO_MUXSEL0;
	GPDR |= BADGE4_GPIO_MUXSEL0;

	/* test points */
	GPCR  = (BADGE4_GPIO_TESTPT_J7 | BADGE4_GPIO_TESTPT_J6 |
		 BADGE4_GPIO_TESTPT_J5);
	GPDR |= (BADGE4_GPIO_TESTPT_J7 | BADGE4_GPIO_TESTPT_J6 |
		 BADGE4_GPIO_TESTPT_J5);

	/* drives CPLD sdram type inputs; this shouldn't be needed;
           bootloader left it this way. */
	GPDR |= (BADGE4_GPIO_SDTYP0 | BADGE4_GPIO_SDTYP1);

  	/* 5V supply rail. */
  	GPCR  = BADGE4_GPIO_PCMEN5V;		/* initially off */
   	GPDR |= BADGE4_GPIO_PCMEN5V;

	/* drives SA1111 reset pin; this shouldn't be needed;
           bootloader left it this way. */
	GPSR  = BADGE4_GPIO_SA1111_NRST;
	GPDR |= BADGE4_GPIO_SA1111_NRST;

	return 0;
}

__initcall(badge4_init);


static unsigned badge4_5V_bitmap = 0;

void badge4_set_5V(unsigned subsystem, int on)
{
	unsigned long flags;
	unsigned old_5V_bitmap;

	local_irq_save(flags);

	old_5V_bitmap = badge4_5V_bitmap;

	if (on) {
		badge4_5V_bitmap |= subsystem;
	} else {
		badge4_5V_bitmap &= ~subsystem;
	}

	/* detect on->off and off->on transitions */
	if ((!old_5V_bitmap) && (badge4_5V_bitmap)) {
		/* was off, now on */
		printk(KERN_INFO __FUNCTION__ ": enabling 5V supply rail\n");
		GPSR = BADGE4_GPIO_PCMEN5V;
	} else if ((old_5V_bitmap) && (!badge4_5V_bitmap)) {
		/* was on, now off */
		printk(KERN_INFO __FUNCTION__ ": disabling 5V supply rail\n");
		GPCR = BADGE4_GPIO_PCMEN5V;
	}

	local_irq_restore(flags);
}
EXPORT_SYMBOL(badge4_set_5V);


static void __init
fixup_badge4(struct machine_desc *desc, struct param_struct *params,
	    char **cmdline, struct meminfo *mi)
{
	/* nothing needed here */
}

static struct map_desc badge4_io_desc[] __initdata = {
  /*  virtual    physical    length      domain   r w c b		   */
  {0xe8000000, 0x00000000, 0x02000000, DOMAIN_IO, 0,1,0,0},/* Flash bank 0 */
  {0xf1000000, 0x08000000, 0x00100000, DOMAIN_IO, 0,1,0,0},/* SRAM  bank 1 */
  {0xf2000000, 0x10000000, 0x00100000, DOMAIN_IO, 0,1,0,0},/* SRAM  bank 2 */
  {0xf4000000, 0x48000000, 0x00100000, DOMAIN_IO, 0,1,0,0},/* SA-1111      */
  LAST_DESC
};

static void __init badge4_map_io(void)
{
	sa1100_map_io();
	iotable_init(badge4_io_desc);

	sa1100_register_uart(0, 3);
	sa1100_register_uart(1, 1);
}

MACHINE_START(BADGE4, "Hewlett-Packard Laboratories BadgePAD 4")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	BOOT_PARAMS(0xc0000100)
	FIXUP(fixup_badge4)
	MAPIO(badge4_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
