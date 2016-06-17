/*
 * linux/arch/arm/mach-sa1100/system3.c
 *
 * Copyright (C) 2001 Stefan Eletzhofer <stefan.eletzhofer@eletztrick.de>
 *
 * $Id: system3.c,v 1.1.6.1 2001/12/04 17:28:06 seletz Exp $
 *
 * This file contains all PT Sytsem 3 tweaks. Based on original work from
 * Nicolas Pitre's assabet fixes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * $Log: system3.c,v $
 * Revision 1.1.6.1  2001/12/04 17:28:06  seletz
 * - merged from previous branch
 *
 * Revision 1.1.4.3  2001/12/04 15:16:31  seletz
 * - merged from linux_2_4_13_ac5_rmk2
 *
 * Revision 1.1.4.2  2001/11/19 17:18:57  seletz
 * - more code cleanups
 *
 * Revision 1.1.4.1  2001/11/16 13:52:05  seletz
 * - PT Digital Board Support Code
 *
 * Revision 1.1.2.2  2001/11/05 16:46:18  seletz
 * - cleanups
 *
 * Revision 1.1.2.1  2001/10/15 16:00:43  seletz
 * - first revision working with new board
 *
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
#include "sa1111.h"

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
static void __init fixup_system3(struct machine_desc *desc,
		struct param_struct *params, char **cmdline, struct meminfo *mi);
static void __init get_system3_scr(void);
static int __init system3_init(void);
static void __init system3_init_irq(void);
static void __init system3_map_io(void);

static void system3_IRQ_demux( int irq, void *dev_id, struct pt_regs *regs );
static int system3_get_mctrl(struct uart_port *port);
static void system3_set_mctrl(struct uart_port *port, u_int mctrl);
static void system3_uart_pm(struct uart_port *port, u_int state, u_int oldstate);
static int sdram_notifier(struct notifier_block *nb, unsigned long event, void *data);

extern void convert_to_tag_list(struct param_struct *params, int mem_init);


/**********************************************************************
 *  global data
 */

/**********************************************************************
 *  static data
 */

static struct map_desc system3_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x00000000, 0x01000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash bank 0 */
  { 0xf3000000, 0x10000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* System Registers */
  { 0xf4000000, 0x40000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* SA-1111 */
  LAST_DESC
};

static struct sa1100_port_fns system3_port_fns __initdata = {
	.set_mctrl	= system3_set_mctrl,
	.get_mctrl	= system3_get_mctrl,
	.pm		= system3_uart_pm,
};

static struct irqaction system3_irq = {
	.name		= "PT Digital Board SA1111 IRQ",
	.handler	= system3_IRQ_demux,
	.flags		= SA_INTERRUPT
};

static struct notifier_block system3_clkchg_block = {
	.notifier_call	= sdram_notifier,
};

/**********************************************************************
 *  Static functions
 */

static void __init system3_map_io(void)
{
	DPRINTK( "%s\n", "START" );
	sa1100_map_io();
	iotable_init(system3_io_desc);

	sa1100_register_uart_fns(&system3_port_fns);
	sa1100_register_uart(0, 1);	/* com port */
	sa1100_register_uart(1, 2);
	sa1100_register_uart(2, 3);	/* radio module */

	Ser1SDCR0 |= SDCR0_SUS;
}


/*********************************************************************
 * Install IRQ handler
 */
static void system3_IRQ_demux( int irq, void *dev_id, struct pt_regs *regs )
{
	u_char irr;

	for(;;){
		//irr = PTCPLD_REG_IRQSR & (PT_IRQ_LAN | PT_IRQ_USAR | PT_IRQ_SA1111);
		irr = PT_IRQSR & (PT_IRQ_LAN | PT_IRQ_SA1111);

		irr ^= (PT_IRQ_LAN);
		if (!irr) break;

		if( irr & PT_IRQ_LAN )
			do_IRQ(IRQ_SYSTEM3_SMC9196, regs);

#if 0
		/* Highspeed Serial Bus not yet used */
		if( irr & PT_IRQ_USAR )
			do_IRQ(PT_USAR_IRQ, regs);
#endif

		if( irr & PT_IRQ_SA1111 )
			sa1111_IRQ_demux(irq, dev_id, regs);
	}
}


static void __init system3_init_irq(void)
{
	int irq;

	DPRINTK( "%s\n", "START" );

	/* SA1111 IRQ not routed to a GPIO. */
	sa1111_init_irq(-1);

	/* setup extra IRQs */
	irq = IRQ_SYSTEM3_SMC9196;
	irq_desc[irq].valid	= 1;
	irq_desc[irq].probe_ok	= 1;

#if 0
	/* Highspeed Serial Bus not yet used */
	irq = PT_USAR_IRQ;
	irq_desc[irq].valid	= 1;
	irq_desc[irq].probe_ok	= 1;
#endif

	/* IRQ by CPLD */
	set_GPIO_IRQ_edge( GPIO_GPIO(25), GPIO_RISING_EDGE );
	setup_arm_irq( IRQ_GPIO25, &system3_irq );
}

/**********************************************************************
 * On system 3 limit cpu frequency to 206 Mhz
 */
static int sdram_notifier(struct notifier_block *nb, unsigned long event,
		void *data)
{
	switch (event) {
		case CPUFREQ_MINMAX:
			cpufreq_updateminmax(data, 147500, 206000);
			break;

	}
	return 0;
}

/**
 *	fixup_system3 - fixup function for system 3 board
 *	@desc:		machine description
 *	@param:		kernel params
 *	@cmdline:	kernel cmdline
 *	@mi:		memory info struct
 *
 */
static void __init fixup_system3(struct machine_desc *desc,
		struct param_struct *params, char **cmdline, struct meminfo *mi)
{
	DPRINTK( "%s\n", "START" );

	ROOT_DEV = MKDEV(RAMDISK_MAJOR,0);
	setup_ramdisk( 1, 0, 0, 8192 );
	setup_initrd( 0xc0800000, 8*1024*1024 );
}


/**
 *	system3_uart_pm - powermgmt callback function for system 3 UART
 *	@port: uart port structure
 *	@state: pm state
 *	@oldstate: old pm state
 *
 */
static void system3_uart_pm(struct uart_port *port, u_int state, u_int oldstate)
{
	/* TODO: switch on/off uart in powersave mode */
}

/*
 * Note! this can be called from IRQ context.
 * FIXME: Handle PT Digital Board CTRL regs irq-safe.
 *
 * NB: system3 uses COM_RTS and COM_DTR for both UART1 (com port)
 * and UART3 (radio module).  We only handle them for UART1 here.
 */
static void system3_set_mctrl(struct uart_port *port, u_int mctrl)
{
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
}

static int system3_get_mctrl(struct uart_port *port)
{
	u_int ret = 0;
	u_int irqsr = PT_IRQSR;

	/* need 2 reads to read current value */
	irqsr = PT_IRQSR;

	/* TODO: check IRQ source register for modem/com
	 status lines and set them correctly. */

	ret = TIOCM_CD | TIOCM_CTS | TIOCM_DSR;

	return ret;
}

static int __init system3_init(void)
{
	int ret = 0;
	DPRINTK( "%s\n", "START" );

	if ( !machine_is_pt_system3() ) {
		ret = -EINVAL;
		goto DONE;
	}

	/* init control register */
	PT_CTRL0 = PT_CTRL0_INIT;
	PT_CTRL1 = 0x02;
	PT_CTRL2 = 0x00;
	DPRINTK( "CTRL[0]=0x%02x\n", PT_CTRL0 );
	DPRINTK( "CTRL[1]=0x%02x\n", PT_CTRL1 );
	DPRINTK( "CTRL[2]=0x%02x\n", PT_CTRL2 );

	/*
	 * Ensure that the memory bus request/grant signals are setup,
	 * and the grant is held in its inactive state.
	 */
	sa1110_mb_disable();

	/*
	 * Probe for a SA1111.
	 */
	ret = sa1111_probe(PT_SA1111_BASE);
	if (ret < 0) {
		printk( KERN_WARNING"PT Digital Board: no SA1111 found!\n" );
		goto DONE;
	}

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

	system3_init_irq();

#if defined( CONFIG_CPU_FREQ )
	ret = cpufreq_register_notifier(&system3_clkchg_block);
	if ( ret != 0 ) {
		printk( KERN_WARNING"PT Digital Board: could not register clock scale callback\n" );
		goto DONE;
	}
#endif

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
__initcall(system3_init);

MACHINE_START(PT_SYSTEM3, "PT System 3")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	BOOT_PARAMS(0xc0000100)
	FIXUP(fixup_system3)
	MAPIO(system3_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
