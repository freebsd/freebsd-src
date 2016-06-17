/*
 * include/asm-sh/io_keywest.h
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

#ifndef _ASM_SH_IO_KEYWEST_H
#define _ASM_SH_IO_KEYWEST_H

#include <linux/types.h>
#include <asm/io_generic.h>

extern unsigned char keywest_inb(unsigned long port);
extern unsigned short keywest_inw(unsigned long port);
extern unsigned int keywest_inl(unsigned long port);

extern void keywest_outb(unsigned char value, unsigned long port);
extern void keywest_outw(unsigned short value, unsigned long port);
extern void keywest_outl(unsigned int value, unsigned long port);

extern unsigned char keywest_inb_p(unsigned long port);
extern void keywest_outb_p(unsigned char value, unsigned long port);

extern void keywest_insb(unsigned long port, void *addr, unsigned long count);
extern void keywest_insw(unsigned long port, void *addr, unsigned long count);
extern void keywest_insl(unsigned long port, void *addr, unsigned long count);
extern void keywest_outsb(unsigned long port, const void *addr, unsigned long count);
extern void keywest_outsw(unsigned long port, const void *addr, unsigned long count);
extern void keywest_outsl(unsigned long port, const void *addr, unsigned long count);
extern unsigned long keywest_isa_port2addr(unsigned long offset);
extern int keywest_irq_demux(int irq);
extern void keywest_init_pci(void);
/* Provision for generic secondary demux step -- used by PCMCIA code */
extern void keywest_register_irq_demux(int irq,
		int (*demux)(int irq, void *dev), void *dev);
extern void keywest_unregister_irq_demux(int irq);
/* Set this variable to 1 to see port traffic */
extern int keywest_io_debug;
/* Map a range of ports to a range of kernel virtual memory. */
extern void keywest_port_map(u32 baseport, u32 nports, u32 addr, u8 shift);
extern void keywest_port_unmap(u32 baseport, u32 nports);
#define mach_port_map keywest_port_map
#define mach_port_unmap keywest_port_unmap

#endif /* _ASM_SH_IO_KEYWEST_H */

#ifdef __WANT_IO_DEF

# define __inb			keywest_inb
# define __inw			keywest_inw
# define __inl			keywest_inl
# define __outb			keywest_outb
# define __outw			keywest_outw
# define __outl			keywest_outl

# define __inb_p		keywest_inb_p
# define __inw_p		keywest_inw
# define __inl_p		keywest_inl
# define __outb_p		keywest_outb_p
# define __outw_p		keywest_outw
# define __outl_p		keywest_outl

# define __insb			keywest_insb
# define __insw			keywest_insw
# define __insl			keywest_insl
# define __outsb		keywest_outsb
# define __outsw		keywest_outsw
# define __outsl		keywest_outsl

# define __readb		generic_readb
# define __readw		generic_readw
# define __readl		generic_readl
# define __writeb		generic_writeb
# define __writew		generic_writew
# define __writel		generic_writel

# define __isa_port2addr	keywest_isa_port2addr
# define __ioremap		generic_ioremap
# define __iounmap		generic_iounmap

#endif
