/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Putting things on the screen/serial line using YAMONs facilities.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/serial_reg.h>
#include <linux/spinlock.h>
#include <asm/io.h>

#ifdef CONFIG_MIPS_ATLAS

#include <asm/mips-boards/atlas.h>

/*
 * Atlas registers are memory mapped on 64-bit aligned boundaries and
 * only word access are allowed.
 * When reading the UART 8 bit registers only the LSB are valid.
 */
static inline unsigned int serial_in(int offset)
{
	return (*(volatile unsigned int *)(mips_io_port_base + ATLAS_UART_REGS_BASE + offset*8) & 0xff);
}

static inline void serial_out(int offset, int value)
{
	*(volatile unsigned int *)(mips_io_port_base + ATLAS_UART_REGS_BASE + offset*8) = value;
}

#elif defined(CONFIG_MIPS_SEAD)

#include <asm/mips-boards/sead.h>

/*
 * SEAD registers are just like Atlas registers.
 */
static inline unsigned int serial_in(int offset)
{
	return (*(volatile unsigned int *)(mips_io_port_base + SEAD_UART0_REGS_BASE + offset*8) & 0xff);
}

static inline void serial_out(int offset, int value)
{
	*(volatile unsigned int *)(mips_io_port_base + SEAD_UART0_REGS_BASE + offset*8) = value;
}

#else

static inline unsigned int serial_in(int offset)
{
	return inb(0x3f8 + offset);
}

static inline void serial_out(int offset, int value)
{
	outb(value, 0x3f8 + offset);
}
#endif

int putPromChar(char c)
{
	while ((serial_in(UART_LSR) & UART_LSR_THRE) == 0)
		;

	serial_out(UART_TX, c);

	return 1;
}

char getPromChar(void)
{
	while (!(serial_in(UART_LSR) & 1))
		;

	return serial_in(UART_RX);
}

static spinlock_t con_lock = SPIN_LOCK_UNLOCKED;

static char buf[1024];

void __init prom_printf(char *fmt, ...)
{
	va_list args;
	int l;
	char *p, *buf_end;
	long flags;

	int putPromChar(char);

	spin_lock_irqsave(con_lock, flags);
	va_start(args, fmt);
	l = vsprintf(buf, fmt, args); /* hopefully i < sizeof(buf) */
	va_end(args);

	buf_end = buf + l;

	for (p = buf; p < buf_end; p++) {
		/* Crude cr/nl handling is better than none */
		if (*p == '\n')
			putPromChar('\r');
		putPromChar(*p);
	}
	spin_unlock_irqrestore(con_lock, flags);
}
