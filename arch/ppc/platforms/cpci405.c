/*
 * arch/ppc/platforms/cpci405.c
 *
 * Board setup routines for the esd CPCI-405 cPCI Board.
 *
 * Author: Stefan Roese
 *         stefan.roese@esd-electronics.com
 *
 * Copyright 2001-2003 esd electronic system design - hannover germany
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 *	History: 11/09/2001 - armin
 *       added board_init to add in additional instuctions needed during platfrom_init
 *
 * 		: 03/26/03 - stefan
 *		Added cpci405_early_serial_map (cloned from evb405ep) to generate
 *		BASE_BAUD dynamically from the UDIV settings (configured by U-Boot).
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <asm/system.h>
#include <asm/pci-bridge.h>
#include <asm/machdep.h>
#include <asm/todc.h>
#include <linux/serial.h>

void *cpci405_nvram;

/*
 * Some IRQs unique to CPCI-405.
 */
int __init
ppc405_map_irq(struct pci_dev *dev, unsigned char idsel, unsigned char pin)
{
	static char pci_irq_table[][4] =
	/*
	 *      PCI IDSEL/INTPIN->INTLINE
	 *      A       B       C       D
	 */
	{
		{28,	28,	28,	28},	/* IDSEL 15 - cPCI slot 8 */
		{29,	29,	29,	29},	/* IDSEL 16 - cPCI slot 7 */
		{30,	30,	30,	30},	/* IDSEL 17 - cPCI slot 6 */
		{27,	27,	27,	27},	/* IDSEL 18 - cPCI slot 5 */
		{28,	28,	28,	28},	/* IDSEL 19 - cPCI slot 4 */
		{29,	29,	29,	29},	/* IDSEL 20 - cPCI slot 3 */
		{30,	30,	30,	30},	/* IDSEL 21 - cPCI slot 2 */
	};
	const long min_idsel = 15, max_idsel = 21, irqs_per_slot = 4;
	return PCI_IRQ_TABLE_LOOKUP;
};

/* The serial clock for the chip is an internal clock determined by
 * different clock speeds/dividers.
 * Calculate the proper input baud rate and setup the serial driver.
 */
static void __init
cpci405_early_serial_map(void)
{
	u32 uart_div;
	int serial_baud_405;
	bd_t *bip = (bd_t *) __res;
	struct serial_struct serialreq = {0};

	/* Calculate the serial clock input frequency
	 *
	 * The base baud is the PLL OUTA (provided in the board info
	 * structure) divided by the external UART Divisor, divided
	 * by 16.
	 */
	uart_div = ((mfdcr(DCRN_CHCR_BASE) & CHR0_UDIV) >> 1) + 1;
	serial_baud_405 = bip->bi_procfreq / uart_div / 16;

	/* Update the serial port attributes */
	serialreq.baud_base = serial_baud_405;
	serialreq.flags = (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST);
	serialreq.io_type = SERIAL_IO_MEM;

	serialreq.line = 0;
	serialreq.port = 0;
	serialreq.irq = ACTING_UART0_INT;
	serialreq.iomem_base = (void*)ACTING_UART0_IO_BASE;

#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
	/* Configure debug serial access */
	gen550_init(0, &serialreq);
#endif

	if (early_serial_setup(&serialreq) != 0) {
		printk("Early serial init of port 0 failed\n");
	}

	serialreq.line = 1;
	serialreq.port = 1;
	serialreq.irq = ACTING_UART1_INT;
	serialreq.iomem_base = (void*)ACTING_UART1_IO_BASE;

#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
	/* Configure debug serial access */
	gen550_init(1, &serialreq);
#endif

	if (early_serial_setup(&serialreq) != 0) {
		printk("Early serial init of port 1 failed\n");
	}
}

void __init
board_setup_arch(void)
{
	cpci405_early_serial_map();
	TODC_INIT(TODC_TYPE_MK48T35, cpci405_nvram, cpci405_nvram, cpci405_nvram, 8);
}

void __init
board_io_mapping(void)
{
	cpci405_nvram = ioremap(CPCI405_NVRAM_PADDR, CPCI405_NVRAM_SIZE);
}

void __init
board_setup_irq(void)
{
}

void __init
board_init(void)
{
	ppc_md.time_init = todc_time_init;
	ppc_md.set_rtc_time = todc_set_rtc_time;
	ppc_md.get_rtc_time = todc_get_rtc_time;
	ppc_md.nvram_read_val = todc_direct_read_val;
	ppc_md.nvram_write_val = todc_direct_write_val;
}
