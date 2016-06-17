/*
 * linux/include/asm-arm/arch-nexuspci/uncompress.h
 *  from linux/include/asm-arm/arch-ebsa110/uncompress.h
 *
 * Copyright (C) 1996,1997,1998 Russell King
 * Copyright (C) 1998, 1999 Phil Blundell
 */

#include <asm/io.h>

#define UARTBASE 0x00400000

/*
 * This does not append a newline
 */
static void puts(const char *s)
{
  while (*s)
  {
    char c = *(s++);
    while (!(__raw_readb(UARTBASE + 0x14) & 0x20))
      barrier();
    __raw_writeb(c, UARTBASE);
    if (c == 10) {
      while (!(__raw_readb(UARTBASE + 0x14) & 0x20))
        barrier();
      __raw_writeb(13, UARTBASE);
    }
  }
}

/*
 * nothing to do
 */
#define arch_decomp_setup()

/*
 * Stroke the watchdog so we don't get reset during decompression.
 */
#define arch_decomp_wdog()				\
	do {						\
	__raw_writel(1, 0xa00000);			\
	__raw_writel(0, 0xa00000);			\
	} while (0)
