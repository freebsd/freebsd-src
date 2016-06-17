/*
 * include/asm-sh/io_hd64465.h
 *
 * By Greg Banks <gbanks@pocketpenguins.com>
 * (c) 2000 PocketPenguins Inc.
 *
 * Derived from io_hd64461.h, which bore the message:
 * Copyright 2000 Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * IO functions for an HD64465 "Windows CE Intelligent Peripheral Controller".
 */

#ifndef _ASM_SH_IO_HD64465_H
#define _ASM_SH_IO_HD64465_H

#include <asm/io_generic.h>

extern unsigned char hd64465_inb(unsigned long port);
extern unsigned short hd64465_inw(unsigned long port);
extern unsigned int hd64465_inl(unsigned long port);

extern void hd64465_outb(unsigned char value, unsigned long port);
extern void hd64465_outw(unsigned short value, unsigned long port);
extern void hd64465_outl(unsigned int value, unsigned long port);

extern unsigned char hd64465_inb_p(unsigned long port);
extern void hd64465_outb_p(unsigned char value, unsigned long port);

extern void hd64465_insb(unsigned long port, void *addr, unsigned long count);
extern void hd64465_insw(unsigned long port, void *addr, unsigned long count);
extern void hd64465_insl(unsigned long port, void *addr, unsigned long count);
extern void hd64465_outsb(unsigned long port, const void *addr, unsigned long count);
extern void hd64465_outsw(unsigned long port, const void *addr, unsigned long count);
extern void hd64465_outsl(unsigned long port, const void *addr, unsigned long count);
extern unsigned long hd64465_isa_port2addr(unsigned long offset);
extern int hd64465_irq_demux(int irq);
/* Provision for generic secondary demux step -- used by PCMCIA code */
extern void hd64465_register_irq_demux(int irq,
		int (*demux)(int irq, void *dev), void *dev);
extern void hd64465_unregister_irq_demux(int irq);
/* Set this variable to 1 to see port traffic */
extern int hd64465_io_debug;
/* Map a range of ports to a range of kernel virtual memory.
 */
extern void hd64465_port_map(unsigned short baseport, unsigned int nports,
			     unsigned long addr, unsigned char shift);
extern void hd64465_port_unmap(unsigned short baseport, unsigned int nports);


#ifdef __WANT_IO_DEF

# define __inb			hd64465_inb
# define __inw			hd64465_inw
# define __inl			hd64465_inl
# define __outb			hd64465_outb
# define __outw			hd64465_outw
# define __outl			hd64465_outl

# define __inb_p		hd64465_inb_p
# define __inw_p		hd64465_inw
# define __inl_p		hd64465_inl
# define __outb_p		hd64465_outb_p
# define __outw_p		hd64465_outw
# define __outl_p		hd64465_outl

# define __insb			hd64465_insb
# define __insw			hd64465_insw
# define __insl			hd64465_insl
# define __outsb		hd64465_outsb
# define __outsw		hd64465_outsw
# define __outsl		hd64465_outsl

# define __readb		generic_readb
# define __readw		generic_readw
# define __readl		generic_readl
# define __writeb		generic_writeb
# define __writew		generic_writew
# define __writel		generic_writel

# define __isa_port2addr	hd64465_isa_port2addr
# define __ioremap		generic_ioremap
# define __iounmap		generic_iounmap


#endif

#endif /* _ASM_SH_IO_HD64465_H */
