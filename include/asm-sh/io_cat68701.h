/*
 * include/asm-sh/io_cat68701.h
 *
 * Copyright 2000 Stuart Menefy (stuart.menefy@st.com)
 *           2001 Yutarou Ebihar (ebihara@si-linux.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * IO functions for an AONE Corp. CAT-68701 SH7708 Borad
 */

#ifndef _ASM_SH_IO_CAT68701_H
#define _ASM_SH_IO_CAT68701_H

#include <asm/io_generic.h>

extern unsigned char cat68701_inb(unsigned long port);
extern unsigned short cat68701_inw(unsigned long port);
extern unsigned int cat68701_inl(unsigned long port);

extern void cat68701_outb(unsigned char value, unsigned long port);
extern void cat68701_outw(unsigned short value, unsigned long port);
extern void cat68701_outl(unsigned int value, unsigned long port);

extern unsigned char cat68701_inb_p(unsigned long port);
extern void cat68701_outb_p(unsigned char value, unsigned long port);

extern void cat68701_insb(unsigned long port, void *addr, unsigned long count);
extern void cat68701_insw(unsigned long port, void *addr, unsigned long count);
extern void cat68701_insl(unsigned long port, void *addr, unsigned long count);
extern void cat68701_outsb(unsigned long port, const void *addr, unsigned long count);
extern void cat68701_outsw(unsigned long port, const void *addr, unsigned long count);
extern void cat68701_outsl(unsigned long port, const void *addr, unsigned long count);

extern unsigned char cat68701_readb(unsigned long addr);
extern unsigned short cat68701_readw(unsigned long addr);
extern unsigned int cat68701_readl(unsigned long addr);
extern void cat68701_writeb(unsigned char b, unsigned long addr);
extern void cat68701_writew(unsigned short b, unsigned long addr);
extern void cat68701_writel(unsigned int b, unsigned long addr);

extern void * cat68701_ioremap(unsigned long offset, unsigned long size);
extern void cat68701_iounmap(void *addr);

extern unsigned long cat68701_isa_port2addr(unsigned long offset);
extern int cat68701_irq_demux(int irq);

extern void setup_cat68701(void);
extern void init_cat68701_IRQ(void);
extern void heartbeat_cat68701(void);

#ifdef __WANT_IO_DEF

# define __inb			cat68701_inb
# define __inw			cat68701_inw
# define __inl			cat68701_inl
# define __outb			cat68701_outb
# define __outw			cat68701_outw
# define __outl			cat68701_outl

# define __inb_p		cat68701_inb_p
# define __inw_p		cat68701_inw
# define __inl_p		cat68701_inl
# define __outb_p		cat68701_outb_p
# define __outw_p		cat68701_outw
# define __outl_p		cat68701_outl

# define __insb			cat68701_insb
# define __insw			cat68701_insw
# define __insl			cat68701_insl
# define __outsb		cat68701_outsb
# define __outsw		cat68701_outsw
# define __outsl		cat68701_outsl

# define __readb		cat68701_readb
# define __readw		cat68701_readw
# define __readl		cat68701_readl
# define __writeb		cat68701_writeb
# define __writew		cat68701_writew
# define __writel		cat68701_writel

# define __isa_port2addr	cat68701_isa_port2addr
# define __ioremap		generic_ioremap
# define __iounmap		generic_iounmap

#endif

#endif /* _ASM_SH_IO_CAT68701_H */
