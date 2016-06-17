/*
 * linux/arch/arm/mach-sa1100/adsbitsyplus.c
 *
 * Author: Robert Whaley
 *
 * This file comes from adsbitsy.c of Woojung Huh <whuh@applieddata.net>
 *
 * Pieces specific to the ADS Bitsy Plus
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

/* unfortunately, we can't detect the difference between REV 2 and REV A
   connector boards.  So by convention, the registers.txt file is used
   to set a byte to either 0x02 or 0x0a to make this distinction.  The
   bootloader must detect this by default we assume rev A since most boards
   will be rev A */

static int adsbitsyplus_connector_board_rev_number = 0xA;

static int __init adsbitsyplus_connector_board_rev_setup(char *str)
{
	adsbitsyplus_connector_board_rev_number = simple_strtol(str,NULL,0);
	return 1;
}

int adsbitsyplus_connector_board_rev(void)
{
	static int only_once = 1;
	if (only_once) {
		printk(KERN_INFO "Bitsy Connector Board REV: %#x\n", adsbitsyplus_connector_board_rev_number);
		only_once = 0;
	}
	return adsbitsyplus_connector_board_rev_number;
}

static int __init adsbitsyplus_init(void)
{
	int ret;

	if (!machine_is_adsbitsyplus())
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

	PPAR &= ~PPAR_SSPGPIO;
	ADS_CPLD_SUPPC |= ADS_SUPPC_AVR_WKP;
	mdelay(100);
	ADS_CPLD_SUPPC &= ~ADS_SUPPC_AVR_WKP;

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
	ret = sa1111_probe(ADSBITSYPLUS_SA1111_BASE);
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

__initcall(adsbitsyplus_init);

static void __init adsbitsyplus_init_irq(void)
{
	/* First the standard SA1100 IRQs */
	sa1100_init_irq();
}

/*
 * Resume SA1111 when system wakes up
 */
void adsbitsyplus_sa1111_wake(unsigned long pa_dwr)
{
	// Turn ON SA1111
	GPCR |= GPIO_GPIO26;
	mdelay(1);
	GPSR |= GPIO_GPIO26;

	GAFR |= GPIO_32_768kHz;
	GPDR |= GPIO_32_768kHz;
	TUCR = TUCR_3_6864MHz;

	SBI_SKCR = SKCR_PLL_BYPASS | SKCR_RDYEN | SKCR_OE_EN;
	udelay(100);
	SBI_SKCR = SKCR_PLL_BYPASS | SKCR_RCLKEN | SKCR_RDYEN | SKCR_OE_EN;

	GAFR |= (GPIO_MBGNT | GPIO_MBREQ);
	GPDR |= GPIO_MBGNT;
	GPDR &= ~GPIO_MBREQ;
	TUCR |= TUCR_MR;

	sa1111_configure_smc(1,
			     FExtr(MDCNFG, MDCNFG_SA1110_DRAC0),
			     FExtr(MDCNFG, MDCNFG_SA1110_TDL0));
	SKPCR |= SKPCR_DCLKEN;

	// Reset PCMCIA
	PCCR = 0xFF;
	mdelay(100);
	PA_DDR = 0x00;
	// PA_DWR = GPIO_GPIO0 | GPIO_GPIO1 | GPIO_GPIO2 | GPIO_GPIO3;
	// PA_DWR = GPIO_GPIO0 | GPIO_GPIO2 | GPIO_GPIO3;
	PA_DWR = pa_dwr;
	PCCR = ~(PCCR_S0_RST | PCCR_S1_RST);

#ifdef CONFIG_USB_OHCI_SA1111
	// Turn ON clock
	SKPCR |= SKPCR_UCLKEN;
	udelay(100);

	// force a RESET
	USB_RESET = 0x01;
	USB_RESET |= 0x02;
	udelay(100);

	// Set Power Sense and Control Line
	USB_RESET = 0;
	USB_RESET = USB_RESET_PWRSENSELOW;
	USB_STATUS = 0;
	udelay(10);
#endif
}


static struct map_desc adsbitsyplus_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x08000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash bank 1 */
  { 0xf0000000, 0x3C000000, 0x00004000, DOMAIN_IO, 0, 1, 0, 0 }, /* 91C1111 */
  { 0xf4000000, 0x18000000, 0x00800000, DOMAIN_IO, 0, 1, 0, 0 }, /* SA1111 */
  { 0xf1000000, 0x10000000, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 }, /* CPLD Controller */
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

static int adsbitsyplus_uart_open(struct uart_port *port, struct uart_info *info)
{
	if (port->mapbase == _Ser1UTCR0) {
		Ser1SDCR0 |= SDCR0_UART;
	} else if (port->mapbase == _Ser2UTCR0) {
		Ser2UTCR4 = Ser2HSCR0 = 0;
	}
	return 0;
}

void adsbitsyplus_uart_pm(struct uart_port *port, u_int state, u_int oldstate)
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
        if (state == 0) {
		// Turn power on if uarts are awake
		if (uart_wake_count[0] + uart_wake_count[1] != 0) {
			// make sure RS-232 is turned on if 1 or 3 are open
			ADS_CPLD_PCON |= ADS_PCON_COM1_3_ON;
		}
		if (uart_wake_count[2] != 0) {
			if (adsbitsyplus_connector_board_rev() >= 0x0a)
				ADS_CPLD_PCON |= ADS_PCON_CONN_B_PE2;
		}
	}
	else {
		// Turn power off if uarts are asleep
		if (uart_wake_count[0] + uart_wake_count[1] == 0) {
			// save power if we are sleeping
			ADS_CPLD_PCON &= ~ADS_PCON_COM1_3_ON;
			GAFR &= ~(GPIO_GPIO15 | GPIO_GPIO19 | GPIO_GPIO20);
			GPDR |= GPIO_GPIO15 | GPIO_GPIO19 | GPIO_GPIO20;
		}
		if (uart_wake_count[2] == 0) {
			if (adsbitsyplus_connector_board_rev() >= 0x0a)
				ADS_CPLD_PCON &= ~ADS_PCON_CONN_B_PE2;
		}
	}
}

static void adsbitsyplus_set_mctrl(struct uart_port *port, u_int mctrl)
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

static u_int adsbitsyplus_get_mctrl(struct uart_port *port)
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

static struct sa1100_port_fns adsbitsyplus_port_fns __initdata = {
	.set_mctrl =    adsbitsyplus_set_mctrl,
	.get_mctrl =    adsbitsyplus_get_mctrl,
	.open =	        adsbitsyplus_uart_open,
	.pm =           adsbitsyplus_uart_pm,
};

static void __init adsbitsyplus_map_io(void)
{
	sa1100_map_io();
	iotable_init(adsbitsyplus_io_desc);

	sa1100_register_uart_fns(&adsbitsyplus_port_fns);
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

__setup("adsbitsyplus_conn_board_rev=", adsbitsyplus_connector_board_rev_setup);

MACHINE_START(ADSBITSYPLUS, "ADS Bitsy Plus")
        BOOT_PARAMS(0xc000003c)
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	MAPIO(adsbitsyplus_map_io)
	INITIRQ(adsbitsyplus_init_irq)
MACHINE_END
