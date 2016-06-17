/*
 * arch/ppc/kernel/gen550_dbg.c
 *
 * A library of polled 16550 serial routines.  These are intended to
 * be used to support progress messages, xmon, kgdb, etc. on a
 * variety of platforms.
 *
 * Adapted from lots of code ripped from the arch/ppc/boot/ polled
 * 16550 support.
 *
 * Matt Porter <mporter@mvista.com>
 *
 * Copyright 2002 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/config.h>
#include <linux/serialP.h>
#include <linux/serial_reg.h>
#include <asm/serial.h>
#include <asm/io.h>

#define SERIAL_BAUD	9600

extern struct serial_state rs_table[];

static void (*serial_outb)(unsigned long, unsigned char);
static unsigned long (*serial_inb)(unsigned long);

static int shift;

unsigned long direct_inb(unsigned long addr)
{
	return readb(addr);
}

void direct_outb(unsigned long addr, unsigned char val)
{
	writeb(val, addr);
}

unsigned long io_inb(unsigned long port)
{
	return inb(port);
}

void io_outb(unsigned long port, unsigned char val)
{
	outb(val, port);
}

unsigned long serial_init(int chan, void *ignored)
{
	unsigned long com_port;
	unsigned char lcr, dlm;

	/* We need to find out which type io we're expecting.  If it's
	 * 'SERIAL_IO_PORT', we get an offset from the isa_io_base.
	 * If it's 'SERIAL_IO_MEM', we can the exact location.  -- Tom */
	switch (rs_table[chan].io_type) {
		case SERIAL_IO_PORT:
			com_port = rs_table[chan].port;
			serial_outb = io_outb;
			serial_inb = io_inb;
			break;
		case SERIAL_IO_MEM:
			com_port = (unsigned long)rs_table[chan].iomem_base;
			serial_outb = direct_outb;
			serial_inb = direct_inb;
			break;
		default:
			/* We can't deal with it. */
			return -1;
	}

	/* How far apart the registers are. */
	shift = rs_table[chan].iomem_reg_shift;

	/* save the LCR */
	lcr = serial_inb(com_port + (UART_LCR << shift));

	/* Access baud rate */
	serial_outb(com_port + (UART_LCR << shift), UART_LCR_DLAB);
	dlm = serial_inb(com_port + (UART_DLM << shift));

	/*
	 * Test if serial port is unconfigured
	 * We assume that no-one uses less than 110 baud or
	 * less than 7 bits per character these days.
	 *  -- paulus.
	 */
	if ((dlm <= 4) && (lcr & 2)) {
		/* port is configured, put the old LCR back */
		serial_outb(com_port + (UART_LCR << shift), lcr);
	}
	else {
		/* Input clock. */
		serial_outb(com_port + (UART_DLL << shift),
			(rs_table[chan].baud_base / SERIAL_BAUD) & 0xFF);
		serial_outb(com_port + (UART_DLM << shift),
				(rs_table[chan].baud_base / SERIAL_BAUD) >> 8);
		/* 8 data, 1 stop, no parity */
		serial_outb(com_port + (UART_LCR << shift), 0x03);
		/* RTS/DTR */
		serial_outb(com_port + (UART_MCR << shift), 0x03);

		/* Clear & enable FIFOs */
		serial_outb(com_port + (UART_FCR << shift), 0x07);
	}

	return (com_port);
}

void
serial_putc(unsigned long com_port, unsigned char c)
{
	while ((serial_inb(com_port + (UART_LSR << shift)) & UART_LSR_THRE) == 0)
		;
	serial_outb(com_port, c);
}

unsigned char
serial_getc(unsigned long com_port)
{
	while ((serial_inb(com_port + (UART_LSR << shift)) & UART_LSR_DR) == 0)
		;
	return serial_inb(com_port);
}

int
serial_tstc(unsigned long com_port)
{
	return ((serial_inb(com_port + (UART_LSR << shift)) & UART_LSR_DR) != 0);
}

void
serial_close(unsigned long com_port)
{
}

void
gen550_init(int i, struct serial_struct *serial_req)
{
	rs_table[i].io_type = serial_req->io_type;
	rs_table[i].port = serial_req->port;
	rs_table[i].iomem_base = serial_req->iomem_base;
	rs_table[i].iomem_reg_shift = serial_req->iomem_reg_shift;
}

#ifdef CONFIG_SERIAL_TEXT_DEBUG
void
gen550_progress(char *s, unsigned short hex)
{
	volatile unsigned int progress_debugport;
	volatile char c;

	progress_debugport = serial_init(0, NULL);

	serial_putc(progress_debugport, '\r');

	while ((c = *s++) != 0)
		serial_putc(progress_debugport, c);

	serial_putc(progress_debugport, '\n');
	serial_putc(progress_debugport, '\r');
}
#endif /* CONFIG_SERIAL_TEXT_DEBUG */
