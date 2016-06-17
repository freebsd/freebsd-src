/*
 * include/asm-sh/io_od.h
 *
 * Copyright 2000 Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * IO functions for an STMicroelectronics Overdrive
 */

#ifndef _ASM_SH_IO_OD_H
#define _ASM_SH_IO_OD_H

#include <asm/io_generic.h>

extern unsigned char od_inb(unsigned long port);
extern unsigned short od_inw(unsigned long port);
extern unsigned int od_inl(unsigned long port);

extern void od_outb(unsigned char value, unsigned long port);
extern void od_outw(unsigned short value, unsigned long port);
extern void od_outl(unsigned int value, unsigned long port);

extern unsigned char od_inb_p(unsigned long port);
extern unsigned short od_inw_p(unsigned long port);
extern unsigned int od_inl_p(unsigned long port);
extern void od_outb_p(unsigned char value, unsigned long port);
extern void od_outw_p(unsigned short value, unsigned long port);
extern void od_outl_p(unsigned int value, unsigned long port);

extern void od_insb(unsigned long port, void *addr, unsigned long count);
extern void od_insw(unsigned long port, void *addr, unsigned long count);
extern void od_insl(unsigned long port, void *addr, unsigned long count);
extern void od_outsb(unsigned long port, const void *addr, unsigned long count);
extern void od_outsw(unsigned long port, const void *addr, unsigned long count);
extern void od_outsl(unsigned long port, const void *addr, unsigned long count);

extern unsigned long od_isa_port2addr(unsigned long offset);

#ifdef __WANT_IO_DEF

# define __inb			od_inb
# define __inw			od_inw
# define __inl			od_inl
# define __outb			od_outb
# define __outw			od_outw
# define __outl			od_outl

# define __inb_p		od_inb_p
# define __inw_p		od_inw_p
# define __inl_p		od_inl_p
# define __outb_p		od_outb_p
# define __outw_p		od_outw_p
# define __outl_p		od_outl_p

# define __insb			od_insb
# define __insw			od_insw
# define __insl			od_insl
# define __outsb		od_outsb
# define __outsw		od_outsw
# define __outsl		od_outsl

# define __readb		generic_readb
# define __readw		generic_readw
# define __readl		generic_readl
# define __writeb		generic_writeb
# define __writew		generic_writew
# define __writel		generic_writel

# define __isa_port2addr	od_isa_port2addr
# define __ioremap		generic_ioremap
# define __iounmap		generic_iounmap

#endif

#endif /* _ASM_SH_IO_OD_H */
