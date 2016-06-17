/*
 *  linux/include/asm-arm/arch-anakin/uncompress.h
 *
 *  Copyright (C) 2001 Aleph One Ltd. for Acunia N.V.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   10-Apr-2001 TTC	Created
 */

#ifndef __ASM_ARCH_UNCOMPRESS_H
#define __ASM_ARCH_UNCOMPRESS_H

#include <linux/config.h>
#include <asm/io.h>
#include <asm/arch/serial_reg.h>

#ifndef CONFIG_ANAKIN_DEFAULT_BAUDRATE
#define CONFIG_ANAKIN_DEFAULT_BAUDRATE	9600
#endif

static inline void
putc(int c)
{
	while (!(__raw_readl(IO_START + UART0 + 0x10) & TXEMPTY))
		barrier();

	__raw_writel(c, IO_START + UART0 + 0x14);
	__raw_writel(__raw_readl(IO_START + UART0 + 0x18)
			| SENDREQUEST, IO_START + UART0 + 0x18);
}

static void
puts(const char *s)
{
	int c;

	while ((c = *s++)) {
		putc(c);
		if (c == '\n') putc('\r');
	}
}

static void
arch_decomp_setup(void)
{
	__raw_writel(__raw_readl(IO_START + UART0 + 0x10) & ~PRESCALER
			| SETBAUD(CONFIG_ANAKIN_DEFAULT_BAUDRATE),
			IO_START + UART0 + 0x10);
	__raw_writel(__raw_readl(IO_START + UART0 + 0x18) & ~(IRQENABLE
			| RTS | DTR | BLOCKRX | PARITY),
			IO_START + UART0 + 0x18);
}

#define arch_decomp_wdog()

#endif
