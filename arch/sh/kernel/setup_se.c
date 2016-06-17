/* $Id: setup_se.c,v 1.1.1.1.2.2 2002/05/10 17:58:54 jzs Exp $
 *
 * linux/arch/sh/kernel/setup_se.c
 *
 * Copyright (C) 2000  Kazumoto Kojima
 *
 * Hitachi SolutionEngine Support.
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>

#include <linux/hdreg.h>
#include <linux/ide.h>
#include <asm/io.h>
#include <asm/hitachi_se.h>
#include <asm/smc37c93x.h>

#ifdef CONFIG_SH_KGDB
#include <asm/kgdb.h>
#endif

/*
 * Configure the Super I/O chip
 */
static void __init smsc_config(int index, int data)
{
	outb_p(index, INDEX_PORT);
	outb_p(data, DATA_PORT);
}

static void __init init_smsc(void)
{
	outb_p(CONFIG_ENTER, CONFIG_PORT);
	outb_p(CONFIG_ENTER, CONFIG_PORT);

	/* FDC */
	smsc_config(CURRENT_LDN_INDEX, LDN_FDC);
	smsc_config(ACTIVATE_INDEX, 0x01);
	smsc_config(IRQ_SELECT_INDEX, 6); /* IRQ6 */

	/* IDE1 */
	smsc_config(CURRENT_LDN_INDEX, LDN_IDE1);
	smsc_config(ACTIVATE_INDEX, 0x01);
	smsc_config(IRQ_SELECT_INDEX, 14); /* IRQ14 */

	/* AUXIO (GPIO): to use IDE1 */
	smsc_config(CURRENT_LDN_INDEX, LDN_AUXIO);
	smsc_config(GPIO46_INDEX, 0x00); /* nIOROP */
	smsc_config(GPIO47_INDEX, 0x00); /* nIOWOP */

	/* COM1 */
	smsc_config(CURRENT_LDN_INDEX, LDN_COM1);
	smsc_config(ACTIVATE_INDEX, 0x01);
	smsc_config(IO_BASE_HI_INDEX, 0x03);
	smsc_config(IO_BASE_LO_INDEX, 0xf8);
	smsc_config(IRQ_SELECT_INDEX, 4); /* IRQ4 */

	/* COM2 */
	smsc_config(CURRENT_LDN_INDEX, LDN_COM2);
	smsc_config(ACTIVATE_INDEX, 0x01);
	smsc_config(IO_BASE_HI_INDEX, 0x02);
	smsc_config(IO_BASE_LO_INDEX, 0xf8);
	smsc_config(IRQ_SELECT_INDEX, 3); /* IRQ3 */

	/* RTC */
	smsc_config(CURRENT_LDN_INDEX, LDN_RTC);
	smsc_config(ACTIVATE_INDEX, 0x01);
	smsc_config(IRQ_SELECT_INDEX, 8); /* IRQ8 */

	/* XXX: PARPORT, KBD, and MOUSE will come here... */
	outb_p(CONFIG_EXIT, CONFIG_PORT);
}

/*
 * Initialize IRQ setting
 */
void __init init_se_IRQ(void)
{
	/*
	 * Super I/O (Just mimic PC):
	 *  1: keyboard
	 *  3: serial 0
	 *  4: serial 1
	 *  5: printer
	 *  6: floppy
	 *  8: rtc
	 * 12: mouse
	 * 14: ide0
	 */
	make_ipr_irq(14, BCR_ILCRA, 2, 0x0f-14);
	make_ipr_irq(12, BCR_ILCRA, 1, 0x0f-12); 
	make_ipr_irq( 8, BCR_ILCRB, 1, 0x0f- 8); 
	make_ipr_irq( 6, BCR_ILCRC, 3, 0x0f- 6);
	make_ipr_irq( 5, BCR_ILCRC, 2, 0x0f- 5);
	make_ipr_irq( 4, BCR_ILCRC, 1, 0x0f- 4);
	make_ipr_irq( 3, BCR_ILCRC, 0, 0x0f- 3);
	make_ipr_irq( 1, BCR_ILCRD, 3, 0x0f- 1);

	make_ipr_irq(10, BCR_ILCRD, 1, 0x0f-10); /* LAN */

	make_ipr_irq( 0, BCR_ILCRE, 3, 0x0f- 0); /* PCIRQ3 */
	make_ipr_irq(11, BCR_ILCRE, 2, 0x0f-11); /* PCIRQ2 */
	make_ipr_irq( 9, BCR_ILCRE, 1, 0x0f- 9); /* PCIRQ1 */
	make_ipr_irq( 7, BCR_ILCRE, 0, 0x0f- 7); /* PCIRQ0 */

	/* #2, #13 are allocated for SLOT IRQ #1 and #2 (for now) */
	/* NOTE: #2 and #13 are not used on PC */
	make_ipr_irq(13, BCR_ILCRG, 1, 0x0f-13); /* SLOTIRQ2 */
	make_ipr_irq( 2, BCR_ILCRG, 0, 0x0f- 2); /* SLOTIRQ1 */
}

#ifdef CONFIG_SH_KGDB
static int kgdb_uart_setup(void);
static struct kgdb_sermap kgdb_uart_sermap = 
{ "ttyS", 0, kgdb_uart_setup, NULL };
#endif

/*
 * Initialize the board
 */
void __init setup_se(void)
{
	init_smsc();
	/* XXX: RTC setting comes here */
#ifdef CONFIG_SH_KGDB
	kgdb_register_sermap(&kgdb_uart_sermap);
#endif
}

/*********************************************************************
 * Currently a hack (e.g. does not interact well w/serial.c, lots of *
 * hardcoded stuff) but may be useful if SCI/F needs debugging.      *
 * Mostly copied from x86 code (see files asm-i386/kgdb_local.h and  *
 * arch/i386/lib/kgdb_serial.c).                                     *
 *********************************************************************/

#ifdef CONFIG_SH_KGDB
#include <linux/types.h>
#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/serial_reg.h>

#define COM1_PORT 0x3f8  /* Base I/O address */
#define COM1_IRQ  4      /* IRQ not used yet */
#define COM2_PORT 0x2f8  /* Base I/O address */
#define COM2_IRQ  3      /* IRQ not used yet */

#define SB_CLOCK 1843200 /* Serial baud clock */
#define SB_BASE (SB_CLOCK/16)
#define SB_MCR UART_MCR_OUT2 | UART_MCR_DTR | UART_MCR_RTS

struct uart_port {
	int base;
};
#define UART_NPORTS 2
struct uart_port uart_ports[] = {
	{ COM1_PORT },
	{ COM2_PORT },
};
struct uart_port *kgdb_uart_port;

#define UART_IN(reg)	inb_p(kgdb_uart_port->base + reg)
#define UART_OUT(reg,v)	outb_p((v), kgdb_uart_port->base + reg)

/* Basic read/write functions for the UART */
#define UART_LSR_RXCERR    (UART_LSR_BI | UART_LSR_FE | UART_LSR_PE)
static int kgdb_uart_getchar(void)
{
	int lsr;
	int c = -1;

	while (c == -1) {
		lsr = UART_IN(UART_LSR);
		if (lsr & UART_LSR_DR) 
			c = UART_IN(UART_RX);
		if ((lsr & UART_LSR_RXCERR))
			c = -1;
	}
	return c;
}

static void kgdb_uart_putchar(int c)
{
	while ((UART_IN(UART_LSR) & UART_LSR_THRE) == 0)
		;
	UART_OUT(UART_TX, c);
}

/*
 * Initialize UART to configured/requested values.
 * (But we don't interrupts yet, or interact w/serial.c)
 */
static int kgdb_uart_setup(void)
{
	int port;
	int lcr = 0;
	int bdiv = 0;

	if (kgdb_portnum >= UART_NPORTS) {
		KGDB_PRINTK("uart port %d invalid.\n", kgdb_portnum);
		return -1;
	}

	kgdb_uart_port = &uart_ports[kgdb_portnum];

	/* Init sequence from gdb_hook_interrupt */
	UART_IN(UART_RX);
	UART_OUT(UART_IER, 0);

	UART_IN(UART_RX);	/* Serial driver comments say */
	UART_IN(UART_IIR);	/* this clears interrupt regs */
	UART_IN(UART_MSR);

	/* Figure basic LCR values */
	switch (kgdb_bits) {
	case '7':
		lcr |= UART_LCR_WLEN7;
		break;
	default: case '8': 
		lcr |= UART_LCR_WLEN8;
		break;
	}
	switch (kgdb_parity) {
	case 'O':
		lcr |= UART_LCR_PARITY;
		break;
	case 'E':
		lcr |= (UART_LCR_PARITY | UART_LCR_EPAR);
		break;
	default: break;
	}

	/* Figure the baud rate divisor */
	bdiv = (SB_BASE/kgdb_baud);
	
	/* Set the baud rate and LCR values */
	UART_OUT(UART_LCR, (lcr | UART_LCR_DLAB));
	UART_OUT(UART_DLL, (bdiv & 0xff));
	UART_OUT(UART_DLM, ((bdiv >> 8) & 0xff));
	UART_OUT(UART_LCR, lcr);

	/* Set the MCR */
	UART_OUT(UART_MCR, SB_MCR);

	/* Turn off FIFOs for now */
	UART_OUT(UART_FCR, 0);

	/* Setup complete: initialize function pointers */
	kgdb_getchar = kgdb_uart_getchar;
	kgdb_putchar = kgdb_uart_putchar;

	return 0;
}
#endif /* CONFIG_SH_KGDB */
