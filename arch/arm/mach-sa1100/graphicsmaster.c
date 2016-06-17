/*
 * linux/arch/arm/mach-sa1100/graphicsmaster.c
 *
 * Pieces specific to the GraphicsMaster board
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/serial_core.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/timer.h>

#include <asm/hardware.h>
#include <asm/hardware/sa1111.h>
#include <asm/setup.h>
#include <asm/irq.h>

#include <asm/mach/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>

#include <asm/arch/irq.h>

#include "generic.h"
#include "sa1111.h"

static int __init graphicsmaster_init(void)
{
	int ret;

	if (!machine_is_graphicsmaster())
		return -ENODEV;

	/*
	 * Ensure that the memory bus request/grant signals are setup,
	 * and the grant is held in its inactive state
	 */
	sa1110_mb_disable();

	/* GraphicsMaster uses GPIO pins for SPI interface to AVR
	 */

	/* use the alternate SSP pins */
	PPAR |= PPAR_SSPGPIO;

	// Set RTS low during sleep
	PGSR |= GPIO_GPIO15 | GPIO_GPIO17 | GPIO_GPIO19;

	/*
	 * Probe for SA1111.
	 */
	ret = sa1111_probe(ADS_SA1111_BASE);
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
	 * Enable PWM control for LCD
	 */
	SKPCR |= SKPCR_PWMCLKEN;
	SACR1 &= ~SACR1_L3EN;
	ADS_DCR |= DCR_BACKLITE_ON;
	SKPWM0 = 0x01;				// Backlight
	SKPEN0 = 1;
	SKPWM1 = 0x7F;				// VEE
	SKPEN1 = 1;

	/*
	 * We only need to turn on DCLK whenever we want to use the
	 * DMA.  It can otherwise be held firmly in the off position.
	 */
	SKPCR |= SKPCR_DCLKEN;

	/*
	 * Enable the SA1110 memory bus request and grant signals.
	 */
	sa1110_mb_enable();

	sa1111_init_irq(IRQ_GRAPHICSMASTER_SA1111);

	return 0;
}

__initcall(graphicsmaster_init);

/*
 * Handlers for GraphicsMaster's external IRQ logic
 */

#define GRAPHICSMASTER_N_IRQ (IRQ_GRAPHICSMASTER_END - IRQ_GRAPHICSMASTER_START)

static void ADS_IRQ_demux( int irq, void *dev_id, struct pt_regs *regs )
{
	int i;

	while( (irq = ADS_INT_ST1 | (ADS_INT_ST2 << 8)) ){
		for( i = 0; i < GRAPHICSMASTER_N_IRQ; i++ )
			if( irq & (1<<i) ) {
				do_IRQ( IRQ_GRAPHICSMASTER_START + i, regs );
			}
	}
}

static struct irqaction ADS_ext_irq = {
	.name		= "ADS_ext_IRQ",
	.handler	= ADS_IRQ_demux,
	.flags		= SA_INTERRUPT
};

static void ADS_mask_and_ack_irq0(unsigned int irq)
{
	int mask = (1 << (irq - IRQ_GRAPHICSMASTER_START));
	ADS_INT_EN1 &= ~mask;
	ADS_INT_ST1 = mask;
}

static void ADS_mask_irq0(unsigned int irq)
{
	ADS_INT_ST1 = (1 << (irq - IRQ_GRAPHICSMASTER_START));
}

static void ADS_unmask_irq0(unsigned int irq)
{
	ADS_INT_EN1 |= (1 << (irq - IRQ_GRAPHICSMASTER_START));
}

static void ADS_mask_and_ack_irq1(unsigned int irq)
{
	int mask = (1 << (irq - (IRQ_GRAPHICSMASTER_UCB1200)));
	ADS_INT_EN2 &= ~mask;
	ADS_INT_ST2 = mask;
}

static void ADS_mask_irq1(unsigned int irq)
{
	ADS_INT_ST2 = (1 << (irq - (IRQ_GRAPHICSMASTER_UCB1200)));
}

static void ADS_unmask_irq1(unsigned int irq)
{
	ADS_INT_EN2 |= (1 << (irq - (IRQ_GRAPHICSMASTER_UCB1200)));
}

static void __init graphicsmaster_init_irq(void)
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

	for (irq = IRQ_GRAPHICSMASTER_START; irq < IRQ_GRAPHICSMASTER_UCB1200; irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= ADS_mask_and_ack_irq0;
		irq_desc[irq].mask	= ADS_mask_irq0;
		irq_desc[irq].unmask	= ADS_unmask_irq0;
	}
	for (irq = IRQ_GRAPHICSMASTER_UCB1200; irq < IRQ_GRAPHICSMASTER_END; irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= ADS_mask_and_ack_irq1;
		irq_desc[irq].mask	= ADS_mask_irq1;
		irq_desc[irq].unmask	= ADS_unmask_irq1;
	}

	GPDR &= ~GPIO_GPIO0;
	set_GPIO_IRQ_edge(GPIO_GPIO0, GPIO_FALLING_EDGE);
	setup_arm_irq( IRQ_GPIO0, &ADS_ext_irq );
}


static struct map_desc graphicsmaster_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x08000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash bank 1 */
  { 0xf0000000, 0x10000000, 0x00400000, DOMAIN_IO, 0, 1, 0, 0 }, /* CPLD */
  { 0xf1000000, 0x40000000, 0x00400000, DOMAIN_IO, 0, 1, 0, 0 }, /* CAN */
  { 0xf4000000, 0x18000000, 0x00800000, DOMAIN_IO, 0, 1, 0, 0 }, /* SA-1111 */
  LAST_DESC
};

static int graphicsmaster_uart_open(struct uart_port *port, struct uart_info *info)
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

static u_int graphicsmaster_get_mctrl(struct uart_port *port)
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

static void graphicsmaster_set_mctrl(struct uart_port *port, u_int mctrl)
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
graphicsmaster_uart_pm(struct uart_port *port, u_int state, u_int oldstate)
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

static struct sa1100_port_fns graphicsmaster_port_fns __initdata = {
	.open		= graphicsmaster_uart_open,
	.get_mctrl	= graphicsmaster_get_mctrl,
	.set_mctrl	= graphicsmaster_set_mctrl,
	.pm		= graphicsmaster_uart_pm,
};

static void __init graphicsmaster_map_io(void)
{
	sa1100_map_io();
	iotable_init(graphicsmaster_io_desc);

	sa1100_register_uart_fns(&graphicsmaster_port_fns);
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

MACHINE_START(GRAPHICSMASTER, "ADS GraphicsMaster")
	BOOT_PARAMS(0xc000003c)
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	MAPIO(graphicsmaster_map_io)
	INITIRQ(graphicsmaster_init_irq)
MACHINE_END
