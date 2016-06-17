/*
 * linux/arch/arm/mach-sa1100/hackkit.c
 *
 * Copyright (C) 2002 Stefan Eletzhofer <stefan.eletzhofer@eletztrick.de>
 *
 * This file contains all HackKit tweaks. Based on original work from
 * Nicolas Pitre's assabet fixes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/cpufreq.h>
#include <linux/list.h>
#include <linux/timer.h>

#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>
#include <asm/mach/serial_sa1100.h>

#include <asm/arch/irq.h>

#include <linux/serial_core.h>

#include "generic.h"

#define DEBUG 1

#ifdef DEBUG
#	define DPRINTK( x, args... )	printk( "%s: line %d: "x, __FUNCTION__, __LINE__, ## args  );
#else
#	define DPRINTK( x, args... )	/* nix */
#endif

/**********************************************************************
 *  prototypes
 */

/* init funcs */
static void __init fixup_hackkit(struct machine_desc *desc,
		struct param_struct *params, char **cmdline, struct meminfo *mi);
static void __init get_hackkit_scr(void);
static int __init hackkit_init(void);
static void __init hackkit_init_irq(void);
static void __init hackkit_map_io(void);

static int hackkit_get_mctrl(struct uart_port *port);
static void hackkit_set_mctrl(struct uart_port *port, u_int mctrl);
static void hackkit_uart_pm(struct uart_port *port, u_int state, u_int oldstate);

extern void convert_to_tag_list(struct param_struct *params, int mem_init);


/**********************************************************************
 *  global data
 */

/**********************************************************************
 *  static data
 */

static struct map_desc hackkit_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x00000000, 0x01000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash bank 0 */
  LAST_DESC
};

static struct sa1100_port_fns hackkit_port_fns __initdata = {
	.set_mctrl	= hackkit_set_mctrl,
	.get_mctrl	= hackkit_get_mctrl,
	.pm		= hackkit_uart_pm,
};

/**********************************************************************
 *  Static functions
 */

static void __init hackkit_map_io(void)
{
	DPRINTK( "%s\n", "START" );
	sa1100_map_io();
	iotable_init(hackkit_io_desc);

	sa1100_register_uart_fns(&hackkit_port_fns);
	sa1100_register_uart(0, 1);	/* com port */
	sa1100_register_uart(1, 2);
	sa1100_register_uart(2, 3);	/* radio module */

	Ser1SDCR0 |= SDCR0_SUS;
}

static void __init hackkit_init_irq(void)
{
	/* none used yet */
}

/**
 *	fixup_hackkit - fixup function for system 3 board
 *	@desc:		machine description
 *	@param:		kernel params
 *	@cmdline:	kernel cmdline
 *	@mi:		memory info struct
 *
 */
static void __init fixup_hackkit(struct machine_desc *desc,
		struct param_struct *params, char **cmdline, struct meminfo *mi)
{
	DPRINTK( "%s\n", "START" );

	ROOT_DEV = MKDEV(RAMDISK_MAJOR,0);
	setup_ramdisk( 1, 0, 0, 8192 );
	setup_initrd( 0xc0800000, 8*1024*1024 );
}


/**
 *	hackkit_uart_pm - powermgmt callback function for system 3 UART
 *	@port: uart port structure
 *	@state: pm state
 *	@oldstate: old pm state
 *
 */
static void hackkit_uart_pm(struct uart_port *port, u_int state, u_int oldstate)
{
	/* TODO: switch on/off uart in powersave mode */
}

/*
 * Note! this can be called from IRQ context.
 * FIXME: No modem ctrl lines yet.
 */
static void hackkit_set_mctrl(struct uart_port *port, u_int mctrl)
{
#if 0
	if (port->mapbase == _Ser1UTCR0) {
		u_int set = 0, clear = 0;

		if (mctrl & TIOCM_RTS)
			set |= PT_CTRL2_RS1_RTS;
		else
			clear |= PT_CTRL2_RS1_RTS;

		if (mctrl & TIOCM_DTR)
			set |= PT_CTRL2_RS1_DTR;
		else
			clear |= PT_CTRL2_RS1_DTR;

		PTCTRL2_clear(clear);
		PTCTRL2_set(set);
	}
#endif
}

static int hackkit_get_mctrl(struct uart_port *port)
{
	u_int ret = 0;
#if 0
	u_int irqsr = PT_IRQSR;

	/* need 2 reads to read current value */
	irqsr = PT_IRQSR;

	/* TODO: check IRQ source register for modem/com
	 status lines and set them correctly. */
#endif

	ret = TIOCM_CD | TIOCM_CTS | TIOCM_DSR;

	return ret;
}

static int __init hackkit_init(void)
{
	int ret = 0;
	DPRINTK( "%s\n", "START" );

	if ( !machine_is_hackkit() ) {
		ret = -EINVAL;
		goto DONE;
	}

	hackkit_init_irq();

	ret = 0;
DONE:
	DPRINTK( "ret=%d\n", ret );
	return ret;
}

/**********************************************************************
 *  Exported Functions
 */

/**********************************************************************
 *  kernel magic macros
 */
__initcall(hackkit_init);

MACHINE_START(HACKKIT, "HackKit Cpu Board")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	BOOT_PARAMS(0xc0000100)
	FIXUP(fixup_hackkit)
	MAPIO(hackkit_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
