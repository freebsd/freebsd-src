/*
 * linux/include/asm-arm/arch-nexuspci/uncompress.h
 *
 * Copyright (C) 1998, 1999, 2000 Philip Blundell
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <asm/hardware.h>
#include <asm/io.h>

/*
 * Write a character to the UART
 */
void _ll_write_char(char c)
{
	while (!(__raw_readb(DUART_START + 0x4) & 0x4))
		barrier();
	__raw_writeb(c, DUART_START + 0xc);
}

/*
 * This does not append a newline
 */
static void puts(const char *s)
{
	while (*s) {
		if (*s == '\n')
			_ll_write_char('\r');
		_ll_write_char(*(s++));
	}
}

/*
 * Set up for decompression
 */
static void arch_decomp_setup(void)
{
	/* LED off */
	__raw_writel(INTCONT_LED, INTCONT_START);

	/* Set up SCC */
	__raw_writeb(42, DUART_START + 8);
	__raw_writeb(48, DUART_START + 8);
	__raw_writeb(16, DUART_START + 8);
	__raw_writeb(0x93, DUART_START);
	__raw_writeb(0x17, DUART_START);
	__raw_writeb(0xbb, DUART_START + 4);
	__raw_writeb(0x78, DUART_START + 16);
	__raw_writeb(0xa0, DUART_START + 8);
	__raw_writeb(5, DUART_START + 8);
}

/*
 * Stroke the watchdog so we don't get reset during decompression.
 */
static inline void arch_decomp_wdog(void)
{
	__raw_writel(INTCONT_WATCHDOG, INTCONT_START);
	__raw_writel(INTCONT_WATCHDOG | 1, INTCONT_START);
}
