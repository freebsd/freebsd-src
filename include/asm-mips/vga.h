/*
 *	Access to VGA videoram
 *
 *	(c) 1998 Martin Mares <mj@ucw.cz>
 */

#ifndef _LINUX_ASM_VGA_H_
#define _LINUX_ASM_VGA_H_

/*
 *	On the PC, we can just recalculate addresses and then
 *	access the videoram directly without any black magic.
 */

#define VGA_MAP_MEM(x) ((unsigned long)0xb0000000 + (unsigned long)(x))

#define vga_readb(x) (*(x))
#define vga_writeb(x,y) (*(y) = (x))

#endif
