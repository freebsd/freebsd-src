/*
 * linux/arch/arm/mach-sa1100/adsbitsy.c
 *
 * Author: Woojung Huh
 *
 * Pieces specific to the ADS Bitsy
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/delay.h>
#include <linux/serial_core.h>
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


static int __init adsbitsy_init(void)
{
	int ret;

	if (!machine_is_adsbitsy())
		return -ENODEV;

	/*
	 * Ensure that the memory bus request/grant signals are setup,
	 * and the grant is held in its inactive state
	 */
	sa1110_mb_disable();

	/* Bitsy uses GPIO pins for SPI interface to AVR
	 * Bitsy Plus uses the standard pins instead.
	 * it also needs to reset the AVR when booting
	 */

	PPAR |= PPAR_SSPGPIO;

	/*
	 * Reset SA1111
	 */
	GPCR |= GPIO_GPIO26;
	udelay(1000);
	GPSR |= GPIO_GPIO26;


#ifndef CONFIG_LEDS_TIMER
	// Set Serial port 1 RTS and DTR Low during sleep
	PGSR |= GPIO_GPIO15 | GPIO_GPIO20;
#else
	// only RTS (because DTR is also the LED
	// which should be off during sleep);
	PGSR |= GPIO_GPIO15;
#endif

	// Set Serial port 3RTS Low during sleep
	PGSR |= GPIO_GPIO19;

	/*
	 * Probe for SA1111.
	 */
	ret = sa1111_probe(ADSBITSY_SA1111_BASE);
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

	set_GPIO_IRQ_edge(GPIO_GPIO0, GPIO_RISING_EDGE);
	sa1111_init_irq(IRQ_GPIO0);

	return 0;
}

__initcall(adsbitsy_init);

static void __init adsbitsy_init_irq(void)
{
	/* First the standard SA1100 IRQs */
	sa1100_init_irq();
}


static struct map_desc adsbitsy_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x08000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash bank 1 */
  { 0xf0000000, 0x3C000000, 0x00004000, DOMAIN_IO, 0, 1, 0, 0 }, /* 91C1111 */
  { 0xf4000000, 0x18000000, 0x00800000, DOMAIN_IO, 0, 1, 0, 0 }, /* SA1111 */
  LAST_DESC
};

/* Use this to see when all uarts are shutdown.  Or all are closed.
 * We can only turn off RS232 chip if either of these are true.
 */

static int uart_wake_count[3] = {1, 1, 1};

enum {UART_SHUTDOWN, UART_WAKEUP};

static void update_uart_counts(int line, int state)
{
	switch (state) {
	case UART_WAKEUP:
		uart_wake_count[line]++;
		break;
	case UART_SHUTDOWN:
		uart_wake_count[line]--;
		break;
	}
}

static int adsbitsy_uart_open(struct uart_port *port, struct uart_info *info)
{
	if (port->mapbase == _Ser1UTCR0) {
		Ser1SDCR0 |= SDCR0_UART;
	} else if (port->mapbase == _Ser2UTCR0) {
		Ser2UTCR4 = Ser2HSCR0 = 0;
	}
	return 0;
}

void adsbitsy_uart_pm(struct uart_port *port, u_int state, u_int oldstate)
{
	// state has ACPI D0-D3
	// ACPI D0 	  : resume from suspend
	// ACPI D1-D3 : enter to a suspend state
	if (port->mapbase == _Ser1UTCR0) {
		if (state) {
			update_uart_counts(1, UART_SHUTDOWN);
			// disable uart
			Ser1UTCR3 = 0;
		}
		else {
			update_uart_counts(1, UART_WAKEUP);
		}
	}
	else if (port->mapbase == _Ser2UTCR0) {
		if (state) {
			update_uart_counts(2, UART_SHUTDOWN);
			// disable uart
			Ser2UTCR3 = 0;
			Ser2HSCR0 = 0;
		}
		else {
			update_uart_counts(2, UART_WAKEUP);
		}
	}
	else if (port->mapbase == _Ser3UTCR0) {
		if (state) {
			update_uart_counts(0, UART_SHUTDOWN);
			// disable uart
			Ser3UTCR3 = 0;
		}
		else {
			update_uart_counts(0, UART_WAKEUP);
		}
	}
}

static void adsbitsy_set_mctrl(struct uart_port *port, u_int mctrl)
{
	// note: only ports 1 and 3 have modem control
	if (port->mapbase == _Ser1UTCR0) {
		if (mctrl & TIOCM_RTS)
			// Set RTS High
			GPCR = GPIO_GPIO15;
		else
			// Set RTS LOW
			GPSR = GPIO_GPIO15;
		if (mctrl & TIOCM_DTR)
			// Set DTR High
			GPCR = GPIO_GPIO20;
		else
			// Set DTR Low
			GPSR = GPIO_GPIO20;
	} else if (port->mapbase == _Ser3UTCR0) {
		if (mctrl & TIOCM_RTS)
			// Set RTS High
			GPCR = GPIO_GPIO19;
		else
			// Set RTS LOW
			GPSR = GPIO_GPIO19;
	}
}

static u_int adsbitsy_get_mctrl(struct uart_port *port)
{
	u_int ret = 0;

	// note: only ports 1 and 3 have modem control
	if (port->mapbase == _Ser1UTCR0) {
		if (!(GPLR & GPIO_GPIO14))
			ret |= TIOCM_CTS;
		if (!(GPLR & GPIO_GPIO24))
			ret |= TIOCM_DSR;
		if (!(GPLR & GPIO_GPIO16))
			ret |= TIOCM_RI;
		if (!(GPLR & GPIO_GPIO17))
			ret |= TIOCM_CD;
	} else if (port->mapbase == _Ser3UTCR0) {
		if (!(GPLR & GPIO_GPIO18))
			ret |= TIOCM_CTS;
	}

	return ret;
}

static struct sa1100_port_fns adsbitsy_port_fns __initdata = {
	.set_mctrl =    adsbitsy_set_mctrl,
	.get_mctrl =    adsbitsy_get_mctrl,
	.open =	        adsbitsy_uart_open,
	.pm =           adsbitsy_uart_pm,
};

static void __init adsbitsy_map_io(void)
{
	sa1100_map_io();
	iotable_init(adsbitsy_io_desc);

	sa1100_register_uart_fns(&adsbitsy_port_fns);
	sa1100_register_uart(0, 3);
	sa1100_register_uart(1, 1);

	// don't register if you want to use IRDA
#ifndef CONFIG_SA1100_FIR
	sa1100_register_uart(2, 2);
#endif

	// COM1 Set RTS and DTR Output
	GPDR |= GPIO_GPIO15 | GPIO_GPIO20;
	// Set CTS, DSR, RI and CD Input
	GPDR &= ~(GPIO_GPIO14 | GPIO_GPIO24 | GPIO_GPIO16 | GPIO_GPIO17);

	// COM3 Set RTS Output
	GPDR |= GPIO_GPIO19;
	// Set CTS Input
	GPDR &= ~GPIO_GPIO18;
}


MACHINE_START(ADSBITSY, "ADS Bitsy")
	BOOT_PARAMS(0xc000003c)
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	MAPIO(adsbitsy_map_io)
	INITIRQ(adsbitsy_init_irq)
MACHINE_END
