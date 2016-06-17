/*
 * linux/arch/arm/mach-sa1100/graphicsclient.c
 *
 * Author: Nicolas Pitre
 *
 * Pieces specific to the GraphicsClient board
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/list.h>
#include <linux/timer.h>

#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/irq.h>

#include <asm/mach/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>
#include <linux/serial_core.h>

#include <asm/arch/irq.h>

#include "generic.h"


/*
 * Handlers for GraphicsClient's external IRQ logic
 */

#define GRAPHICSCLIENT_N_IRQ     (IRQ_GRAPHICSCLIENT_END - IRQ_GRAPHICSCLIENT_START)
#define GRAPHICSCLIENT_FIRST_IRQ (IRQ_GRAPHICSCLIENT_CAN - IRQ_GRAPHICSCLIENT_START)

static void ADS_IRQ_demux( int irq, void *dev_id, struct pt_regs *regs )
{
	int i;

	while( (irq = ADS_INT_ST1 | (ADS_INT_ST2 << 8)) ){
		for( i = GRAPHICSCLIENT_FIRST_IRQ; i < GRAPHICSCLIENT_N_IRQ; i++ )
			if( irq & (1<<i) )
				do_IRQ( IRQ_GRAPHICSCLIENT_START + i, regs );
	}
}

static struct irqaction ADS_ext_irq = {
	.name		= "ADS_ext_IRQ",
	.handler	= ADS_IRQ_demux,
	.flags		= SA_INTERRUPT
};

static void ADS_mask_and_ack_irq0(unsigned int irq)
{
	int mask = (1 << (irq - IRQ_GRAPHICSCLIENT_START));
	ADS_INT_EN1 &= ~mask;
	ADS_INT_ST1 = mask;
}

static void ADS_mask_irq0(unsigned int irq)
{
	ADS_INT_ST1 = (1 << (irq - IRQ_GRAPHICSCLIENT_START));
}

static void ADS_unmask_irq0(unsigned int irq)
{
	ADS_INT_EN1 |= (1 << (irq - IRQ_GRAPHICSCLIENT_START));
}

static void ADS_mask_and_ack_irq1(unsigned int irq)
{
	int mask = (1 << (irq - (IRQ_GRAPHICSCLIENT_UCB1200)));
	ADS_INT_EN2 &= ~mask;
	ADS_INT_ST2 = mask;
}

static void ADS_mask_irq1(unsigned int irq)
{
	ADS_INT_ST2 = (1 << (irq - (IRQ_GRAPHICSCLIENT_UCB1200)));
}

static void ADS_unmask_irq1(unsigned int irq)
{
	ADS_INT_EN2 |= (1 << (irq - (IRQ_GRAPHICSCLIENT_UCB1200)));
}

static void __init graphicsclient_init_irq(void)
{
	int irq;

	/* First the standard SA1100 IRQs */
	sa1100_init_irq();

	/* disable all IRQs */
	ADS_INT_EN1 = 0;
	ADS_INT_EN2 = 0;
	/* clear all IRQs */
	ADS_INT_ST1 = 0xff;
	ADS_INT_ST2 = 0xff;

	// Set RTS low during sleep
	PGSR |= GPIO_GPIO15 | GPIO_GPIO17 | GPIO_GPIO19;

	for (irq = IRQ_GRAPHICSCLIENT_START; irq < IRQ_GRAPHICSCLIENT_UCB1200; irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= ADS_mask_and_ack_irq0;
		irq_desc[irq].mask	= ADS_mask_irq0;
		irq_desc[irq].unmask	= ADS_unmask_irq0;
	}
	for (irq = IRQ_GRAPHICSCLIENT_UCB1200; irq < IRQ_GRAPHICSCLIENT_END; irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= ADS_mask_and_ack_irq1;
		irq_desc[irq].mask	= ADS_mask_irq1;
		irq_desc[irq].unmask	= ADS_unmask_irq1;
	}
	set_GPIO_IRQ_edge(GPIO_GPIO0, GPIO_FALLING_EDGE);
	setup_arm_irq( IRQ_GPIO0, &ADS_ext_irq );
}

static struct map_desc graphicsclient_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x08000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash bank 1 */
  { 0xf0000000, 0x10000000, 0x00400000, DOMAIN_IO, 0, 1, 0, 0 }, /* CPLD */
  { 0xf1000000, 0x18000000, 0x00400000, DOMAIN_IO, 0, 1, 0, 0 }, /* CAN */
  LAST_DESC
};

static int graphicsclient_uart_open(struct uart_port *port, struct uart_info *info)
{
	int	ret = 0;

	if (port->mapbase == _Ser1UTCR0) {
		Ser1SDCR0 |= SDCR0_UART;
	}
	else if (port->mapbase == _Ser2UTCR0) {
		Ser2UTCR4 = Ser2HSCR0 = 0;
	}
	return ret;
}

static u_int graphicsclient_get_mctrl(struct uart_port *port)
{
	u_int result = TIOCM_CD | TIOCM_DSR;

	if (port->mapbase == _Ser1UTCR0) {
		if (!(GPLR & GPIO_GPIO14))
			result |= TIOCM_CTS;
	} else if (port->mapbase == _Ser2UTCR0) {
		if (!(GPLR & GPIO_GPIO16))
			result |= TIOCM_CTS;
	} else if (port->mapbase == _Ser3UTCR0) {
		if (!(GPLR & GPIO_GPIO18))
			result |= TIOCM_CTS;
	} else {
		result = TIOCM_CTS;
	}

	return result;
}

static void graphicsclient_set_mctrl(struct uart_port *port, u_int mctrl)
{
	if (port->mapbase == _Ser1UTCR0) {
		if (mctrl & TIOCM_RTS)
			GPCR = GPIO_GPIO15;
		else
			GPSR = GPIO_GPIO15;
	} else if (port->mapbase == _Ser2UTCR0) {
		if (mctrl & TIOCM_RTS)
			GPCR = GPIO_GPIO17;
		else
			GPSR = GPIO_GPIO17;
	} else if (port->mapbase == _Ser3UTCR0) {
		if (mctrl & TIOCM_RTS)
			GPCR = GPIO_GPIO19;
		else
			GPSR = GPIO_GPIO19;
	}
}

static void
graphicsclient_uart_pm(struct uart_port *port, u_int state, u_int oldstate)
{
	// state has ACPI D0-D3
	// ACPI D0 	  : resume from suspend
	// ACPI D1-D3 : enter to a suspend state
	if (port->mapbase == _Ser1UTCR0) {
		if (state) {
                        // disable uart
                        Ser1UTCR3 = 0;
		}
	}
	else if (port->mapbase == _Ser2UTCR0) {
		if (state) {
                        // disable uart
                        Ser2UTCR3 = 0;
                        Ser2HSCR0 = 0;
		}
	}
	else if (port->mapbase == _Ser3UTCR0) {
		if (state) {
                        // disable uart
                        Ser3UTCR3 = 0;
		}
	}
}

static struct sa1100_port_fns graphicsclient_port_fns __initdata = {
	.open		= graphicsclient_uart_open,
	.get_mctrl	= graphicsclient_get_mctrl,
	.set_mctrl	= graphicsclient_set_mctrl,
	.pm		= graphicsclient_uart_pm,
};

static void __init graphicsclient_map_io(void)
{
	sa1100_map_io();
	iotable_init(graphicsclient_io_desc);

	sa1100_register_uart_fns(&graphicsclient_port_fns);
	sa1100_register_uart(0, 3);
	sa1100_register_uart(1, 1);

	// don't register if you want to use IRDA
#ifndef CONFIG_SA1100_FIR
	sa1100_register_uart(2, 2);
#endif

	/* set GPDR now */
	GPDR |= GPIO_GPIO15 | GPIO_GPIO17 | GPIO_GPIO19;
       	GPDR &= ~(GPIO_GPIO14 | GPIO_GPIO16 | GPIO_GPIO18);
}

MACHINE_START(GRAPHICSCLIENT, "ADS GraphicsClient")
        BOOT_PARAMS(0xc000003c)
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	MAPIO(graphicsclient_map_io)
	INITIRQ(graphicsclient_init_irq)
MACHINE_END
