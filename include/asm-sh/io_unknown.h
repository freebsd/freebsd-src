/*
 * include/asm-sh/io_unknown.h
 *
 * Copyright 2000 Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * IO functions for use when we don't know what machine we are on
 */

#ifndef _ASM_SH_IO_UNKNOWN_H
#define _ASM_SH_IO_UNKNOWN_H

extern unsigned char unknown_inb(unsigned long port);
extern unsigned short unknown_inw(unsigned long port);
extern unsigned int unknown_inl(unsigned long port);

extern void unknown_outb(unsigned char value, unsigned long port);
extern void unknown_outw(unsigned short value, unsigned long port);
extern void unknown_outl(unsigned int value, unsigned long port);

extern unsigned char unknown_inb_p(unsigned long port);
extern unsigned short unknown_inw_p(unsigned long port);
extern unsigned int unknown_inl_p(unsigned long port);
extern void unknown_outb_p(unsigned char value, unsigned long port);
extern void unknown_outw_p(unsigned short value, unsigned long port);
extern void unknown_outl_p(unsigned int value, unsigned long port);

extern void unknown_insb(unsigned long port, void *addr, unsigned long count);
extern void unknown_insw(unsigned long port, void *addr, unsigned long count);
extern void unknown_insl(unsigned long port, void *addr, unsigned long count);
extern void unknown_outsb(unsigned long port, const void *addr, unsigned long count);
extern void unknown_outsw(unsigned long port, const void *addr, unsigned long count);
extern void unknown_outsl(unsigned long port, const void *addr, unsigned long count);

extern unsigned char unknown_readb(unsigned long addr);
extern unsigned short unknown_readw(unsigned long addr);
extern unsigned int unknown_readl(unsigned long addr);
extern void unknown_writeb(unsigned char b, unsigned long addr);
extern void unknown_writew(unsigned short b, unsigned long addr);
extern void unknown_writel(unsigned int b, unsigned long addr);

extern unsigned long unknown_isa_port2addr(unsigned long offset);
extern void *unknown_ioremap(unsigned long offset, unsigned long size);
extern void unknown_iounmap(void *addr);

#ifdef __WANT_IO_DEF

# define __inb			unknown_inb
# define __inw			unknown_inw
# define __inl			unknown_inl
# define __outb			unknown_outb
# define __outw			unknown_outw
# define __outl			unknown_outl

# define __inb_p		unknown_inb_p
# define __inw_p		unknown_inw_p
# define __inl_p		unknown_inl_p
# define __outb_p		unknown_outb_p
# define __outw_p		unknown_outw_p
# define __outl_p		unknown_outl_p

# define __insb			unknown_insb
# define __insw			unknown_insw
# define __insl			unknown_insl
# define __outsb		unknown_outsb
# define __outsw		unknown_outsw
# define __outsl		unknown_outsl

# define __readb		unknown_readb
# define __readw		unknown_readw
# define __readl		unknown_readl
# define __writeb		unknown_writeb
# define __writew		unknown_writew
# define __writel		unknown_writel

# define __isa_port2addr	unknown_isa_port2addr
# define __ioremap		unknown_ioremap
# define __iounmap		unknown_iounmap

#endif

#endif /* _ASM_SH_IO_UNKNOWN_H */
