/*
 * include/asm-sh/io_bigsur.h
 *
 * By Dustin McIntire (dustin@sensoria.com) (c)2001
 * Derived from io_hd64465.h, which bore the message:
 * By Greg Banks <gbanks@pocketpenguins.com>
 * (c) 2000 PocketPenguins Inc. 
 * and from io_hd64461.h, which bore the message:
 * Copyright 2000 Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * IO functions for a Hitachi Big Sur Evaluation Board.
 */

#ifndef _ASM_SH_IO_BIGSUR_H
#define _ASM_SH_IO_BIGSUR_H

#include <linux/types.h>
#include <asm/io_generic.h>

extern unsigned char bigsur_inb(unsigned long port);
extern unsigned short bigsur_inw(unsigned long port);
extern unsigned int bigsur_inl(unsigned long port);

extern void bigsur_outb(unsigned char value, unsigned long port);
extern void bigsur_outw(unsigned short value, unsigned long port);
extern void bigsur_outl(unsigned int value, unsigned long port);

extern unsigned char bigsur_inb_p(unsigned long port);
extern void bigsur_outb_p(unsigned char value, unsigned long port);

extern void bigsur_insb(unsigned long port, void *addr, unsigned long count);
extern void bigsur_insw(unsigned long port, void *addr, unsigned long count);
extern void bigsur_insl(unsigned long port, void *addr, unsigned long count);
extern void bigsur_outsb(unsigned long port, const void *addr, unsigned long count);
extern void bigsur_outsw(unsigned long port, const void *addr, unsigned long count);
extern void bigsur_outsl(unsigned long port, const void *addr, unsigned long count);
extern unsigned long bigsur_isa_port2addr(unsigned long offset);
extern int bigsur_irq_demux(int irq);
extern void bigsur_init_pci(void);
/* Provision for generic secondary demux step -- used by PCMCIA code */
extern void bigsur_register_irq_demux(int irq,
		int (*demux)(int irq, void *dev), void *dev);
extern void bigsur_unregister_irq_demux(int irq);
/* Set this variable to 1 to see port traffic */
extern int bigsur_io_debug;
/* Map a range of ports to a range of kernel virtual memory. */
extern void bigsur_port_map(u32 baseport, u32 nports, u32 addr, u8 shift);
extern void bigsur_port_unmap(u32 baseport, u32 nports);

#endif /* _ASM_SH_IO_BIGSUR_H */

#ifdef __WANT_IO_DEF

# define __inb			bigsur_inb
# define __inw			bigsur_inw
# define __inl			bigsur_inl
# define __outb			bigsur_outb
# define __outw			bigsur_outw
# define __outl			bigsur_outl

# define __inb_p		bigsur_inb_p
# define __inw_p		bigsur_inw
# define __inl_p		bigsur_inl
# define __outb_p		bigsur_outb_p
# define __outw_p		bigsur_outw
# define __outl_p		bigsur_outl

# define __insb			bigsur_insb
# define __insw			bigsur_insw
# define __insl			bigsur_insl
# define __outsb		bigsur_outsb
# define __outsw		bigsur_outsw
# define __outsl		bigsur_outsl

# define __readb		generic_readb
# define __readw		generic_readw
# define __readl		generic_readl
# define __writeb		generic_writeb
# define __writew		generic_writew
# define __writel		generic_writel

# define __isa_port2addr	bigsur_isa_port2addr
# define __ioremap		generic_ioremap
# define __iounmap		generic_iounmap

#endif
