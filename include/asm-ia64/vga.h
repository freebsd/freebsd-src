#ifndef _ASM_IA64_VGA_H
#define _ASM_IA64_VGA_H

/*
 *	Access to VGA videoram
 *
 *	(c) 1998 Martin Mares <mj@ucw.cz>
 *	(c) 1999 Asit Mallick <asit.k.mallick@intel.com>
 *	(c) 1999 Don Dugger <don.dugger@intel.com>
 *	Copyright (C) 2002 Hewlett-Packard Co
 */
/*
 * 2002/07/19	davidm@hpl.hp.com	Access frame-buffer memory via readX/writeX.
 */

#include <asm/io.h>

#define VT_BUF_HAVE_RW

#define VGA_MAP_MEM(x)	((unsigned long) ioremap((x), 0))

#define vga_readb	__raw_readb
#define vga_writeb	__raw_writeb

extern inline void
scr_writew (u16 val, volatile u16 *addr)
{
	/* Note: ADDR may point to normal memory.  That's OK on ia64.  */
	__raw_writew(val, (unsigned long) addr);
}

extern inline u16
scr_readw (volatile const u16 *addr)
{
	/* Note: ADDR may point to normal memory.  That's OK on ia64.  */
	return __raw_readw((unsigned long) addr);
}

#endif /* _ASM_IA64_VGA_H */
