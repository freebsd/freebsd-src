/*
 * linux/include/asm-sh/io_microdev.h
 *
 * Copyright (C) 2003 Sean McGoogan (Sean.McGoogan@superh.com)
 *
 * IO functions for the SuperH SH4-202 MicroDev board.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */


#ifndef _ASM_SH_IO_MICRODEV_H
#define _ASM_SH_IO_MICRODEV_H

#include <asm/io_generic.h>

extern unsigned long microdev_isa_port2addr(unsigned long offset);

extern unsigned char microdev_inb(unsigned long port);
extern unsigned short microdev_inw(unsigned long port);
extern unsigned int microdev_inl(unsigned long port);

extern void microdev_outb(unsigned char value, unsigned long port);
extern void microdev_outw(unsigned short value, unsigned long port);
extern void microdev_outl(unsigned int value, unsigned long port);

extern unsigned char microdev_inb_p(unsigned long port);
extern unsigned short microdev_inw_p(unsigned long port);
extern unsigned int microdev_inl_p(unsigned long port);

extern void microdev_outb_p(unsigned char value, unsigned long port);
extern void microdev_outw_p(unsigned short value, unsigned long port);
extern void microdev_outl_p(unsigned int value, unsigned long port);

extern void microdev_insb(unsigned long port, void *addr, unsigned long count);
extern void microdev_insw(unsigned long port, void *addr, unsigned long count);
extern void microdev_insl(unsigned long port, void *addr, unsigned long count);

extern void microdev_outsb(unsigned long port, const void *addr, unsigned long count);
extern void microdev_outsw(unsigned long port, const void *addr, unsigned long count);
extern void microdev_outsl(unsigned long port, const void *addr, unsigned long count);

#ifdef __WANT_IO_DEF

# define __inb			microdev_inb
# define __inw			microdev_inw
# define __inl			microdev_inl
# define __outb			microdev_outb
# define __outw			microdev_outw
# define __outl			microdev_outl

# define __inb_p		microdev_inb_p
# define __inw_p		microdev_inw_p
# define __inl_p		microdev_inl_p
# define __outb_p		microdev_outb_p
# define __outw_p		microdev_outw_p
# define __outl_p		microdev_outl_p

# define __insb			microdev_insb
# define __insw			microdev_insw
# define __insl			microdev_insl
# define __outsb		microdev_outsb
# define __outsw		microdev_outsw
# define __outsl		microdev_outsl

# define __readb		generic_readb
# define __readw		generic_readw
# define __readl		generic_readl
# define __writeb		generic_writeb
# define __writew		generic_writew
# define __writel		generic_writel

# define __isa_port2addr	microdev_isa_port2addr
# define __ioremap		generic_ioremap
# define __iounmap		generic_iounmap

#endif

#endif /* _ASM_SH_IO_MICRODEV_H */

