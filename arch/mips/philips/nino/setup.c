/*
 *  arch/mips/philips/nino/setup.c
 *
 *  Copyright (C) 2001 Steven J. Hill (sjhill@realitydiluted.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Interrupt and exception initialization for Philips Nino
 */
#include <linux/config.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <asm/addrspace.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/reboot.h>
#include <asm/time.h>
#include <asm/traps.h>
#include <asm/tx3912.h>

static void nino_machine_restart(char *command)
{
	static void (*back_to_prom)(void) = (void (*)(void)) 0xbfc00000;

	/* Reboot */
	back_to_prom();
}

static void nino_machine_halt(void)
{
	printk("Nino halted.\n");
	while(1);
}

static void nino_machine_power_off(void)
{
	printk("Nino halted. Please turn off power.\n");
	while(1);
}

static void __init nino_board_init()
{
	/*
	 * Set up the master clock module. The value set in
	 * the Clock Control Register by WindowsCE is 0x00432ba.
	 * We set a few values here and let the device drivers
	 * handle the rest.
	 *
	 * NOTE: The UART clocks must be enabled here to provide
	 *       enough time for them to settle.
	 */
	outl(0x00000000, TX3912_CLK_CTRL);
	outl((TX3912_CLK_CTRL_SIBMCLKDIR | TX3912_CLK_CTRL_SIBMCLKDIV_2 |
		TX3912_CLK_CTRL_ENSIBMCLK | TX3912_CLK_CTRL_CSERSEL |
		TX3912_CLK_CTRL_CSERDIV_3 | TX3912_CLK_CTRL_ENCSERCLK |
		TX3912_CLK_CTRL_ENUARTACLK | TX3912_CLK_CTRL_ENUARTBCLK),
		TX3912_CLK_CTRL);
}

static __init void nino_time_init(void)
{
	/* Load the counter and enable the timer */
	outl(TX3912_SYS_TIMER_VALUE, TX3912_TIMER_PERIOD);
	outl(TX3912_TIMER_CTRL_ENPERTIMER, TX3912_TIMER_CTRL);

	/* Enable the master timer clock */
	outl(inl(TX3912_CLK_CTRL) | TX3912_CLK_CTRL_ENTIMERCLK,
		TX3912_CLK_CTRL);

	/* Enable the interrupt */
	outl(inl(TX3912_INT5_ENABLE) | TX3912_INT5_PERINT,
		TX3912_INT5_ENABLE);
}

static __init void nino_timer_setup(struct irqaction *irq)
{
	irq->dev_id = (void *) irq;
	setup_irq(0, irq);
}


void __init nino_setup(void)
{
	extern void nino_irq_setup(void);
	extern void nino_wait(void);

	irq_setup = nino_irq_setup;
	set_io_port_base(KSEG1ADDR(0x10c00000));

	_machine_restart = nino_machine_restart;
	_machine_halt = nino_machine_halt;
	_machine_power_off = nino_machine_power_off;

	board_time_init = nino_time_init;
	board_timer_setup = nino_timer_setup;

	cpu_wait = nino_wait;

#ifdef CONFIG_FB
	conswitchp = &dummy_con;
#endif

	nino_board_init();
}
