/*
 * linux/arch/arm/mach-sa1100/flexanet.c
 *
 * Author: Jordi Colomer <jco@ict.es>
 *
 * This file contains all FlexaNet-specific tweaks.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/delay.h>

#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/page.h>
#include <asm/pgtable.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>
#include <linux/serial_core.h>
#include <asm/arch/flexanet.h>

#include "generic.h"


unsigned long flexanet_BCR = FHH_BCR_POWERUP;

EXPORT_SYMBOL(flexanet_BCR);

/* physical addresses */
#define _RCNR        0x90010004
#define _GPLR        0x90040000
#define _Ser4SSCR0   0x80070060

/*
 * Get the modem-control register of the UARTs
 *
 */
static int flexanet_get_mctrl(struct uart_port *port)
{
	int            stat = 0;
	unsigned long  bsr;

	/* only DSR and CTS are implemented in UART1 & 3 */
	if (port->membase == (void *)&Ser1UTCR0)
	{
		bsr = FHH_BSR;

		if ((bsr & FHH_BSR_DSR1) != 0)
			stat |= TIOCM_DSR;
		if ((bsr & FHH_BSR_CTS1) != 0)
			stat |= TIOCM_CTS;
	}
	else if (port->membase == (void *)&Ser3UTCR0)
	{
		bsr = FHH_BSR;

		if ((bsr & FHH_BSR_DSR3) != 0)
			stat |= TIOCM_DSR;
		if ((bsr & FHH_BSR_CTS3) != 0)
			stat |= TIOCM_CTS;
	}

	return stat;
}

/*
 * Set the modem-control register of the UARTs
 *
 */
static void flexanet_set_mctrl(struct uart_port *port, u_int mctrl)
{
	unsigned long	flags;

	/* only the RTS signal is implemented in UART1 & 3 */
	if (port->membase == (void *)&Ser1UTCR0)
	{
		local_irq_save(flags);

		if (mctrl & TIOCM_RTS)
			flexanet_BCR |= FHH_BCR_RTS1;
		else
			flexanet_BCR &= ~FHH_BCR_RTS1;

		FHH_BCR = flexanet_BCR;
		local_irq_restore(flags);
	}
	else if (port->membase == (void *)&Ser3UTCR0)
	{
		local_irq_save(flags);

		if (mctrl & TIOCM_RTS)
			flexanet_BCR |= FHH_BCR_RTS3;
		else
			flexanet_BCR &= ~FHH_BCR_RTS3;

		FHH_BCR = flexanet_BCR;
		local_irq_restore(flags);
	}
}

/*
 * machine-specific serial port functions
 *
 * get_mctrl : set state of modem control lines
 * set_mctrl : set the modem control lines
 * pm        : power-management. Turn device on/off.
 *
 */
static struct sa1100_port_fns	flexanet_port_fns __initdata =
{
	set_mctrl : flexanet_set_mctrl,
	get_mctrl : flexanet_get_mctrl,
	pm        : NULL,
};


/*
 * Initialization and serial port mapping
 *
 */

static int flexanet_serial_init(void)
{
	/* register low-level functions */
	sa1100_register_uart_fns(&flexanet_port_fns);

	/* UART port number mapping */
	sa1100_register_uart(0, 1); /* RS232 */
	sa1100_register_uart(1, 3); /* Radio */

	/* Select UART function in Serial port 1 */
	Ser1SDCR0 |= SDCR0_UART;

	return 0;
}


static int __init flexanet_init(void)
{
	/* Set IRQ edges */
	set_GPIO_IRQ_edge(GPIO_GUI_IRQ, GPIO_RISING_EDGE);

	/* deassert the GUI reset */
	FLEXANET_BCR_set(FHH_BCR_GUI_NRST);
	return 0;
}

__initcall(flexanet_init);


static void __init
fixup_flexanet(struct machine_desc *desc, struct param_struct *params,
	      char **cmdline, struct meminfo *mi)
{
	/* fixed RAM size, by now (64MB) */
	SET_BANK( 0, 0xc0000000, 64*1024*1024 );
	mi->nr_banks = 1;

	/* setup ramdisk */
	ROOT_DEV = MKDEV(RAMDISK_MAJOR,0);
	setup_ramdisk( 1, 0, 0, 8192 );
	setup_initrd( 0xc0800000, 3*1024*1024 );
}


static struct map_desc flexanet_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x00000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash bank 0 */
  { 0xf0000000, 0x10000000, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 }, /* Board Control Register */
  { 0xf1000000, 0x18000000, 0x01000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Ethernet controller */
  { 0xD0000000, 0x40000000, 0x04000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Instrument boards */
  { 0xD8000000, 0x48000000, 0x01000000, DOMAIN_IO, 0, 1, 0, 0 }, /* External peripherals */
  LAST_DESC
};

static void __init flexanet_map_io(void)
{
	sa1100_map_io();
	iotable_init(flexanet_io_desc);
	flexanet_serial_init();

	/* wakeup source is GPIO-0 only */
	PWER = PWER_GPIO0;

	/* GPIOs set to zero during sleep */
	PGSR = 0;

	/*
	 * stop the 3.68 MHz oscillator and float control busses
	 * during sleep, since peripherals are powered off.
	 */
	PCFR = PCFR_OPDE | PCFR_FP | PCFR_FS;

}


MACHINE_START(FLEXANET, "FlexaNet")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	BOOT_PARAMS(0xc0000100)
	FIXUP(fixup_flexanet)
	MAPIO(flexanet_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END

