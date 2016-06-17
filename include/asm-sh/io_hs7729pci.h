/*
 * include/asm-sh/io_hs7729pci.h
 *
 * Copyright 2000 Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * IO functions for an Hitachi Semiconductor and Devices HS7729PCI
 */

#ifndef _ASM_SH_IO_HS7729PCI_H
#define _ASM_SH_IO_HS7729PCI_H

#include <asm/io_generic.h>

extern unsigned char hs7729pci_inb(unsigned long port);
extern unsigned short hs7729pci_inw(unsigned long port);
extern unsigned int hs7729pci_inl(unsigned long port);

extern void hs7729pci_outb(unsigned char value, unsigned long port);
extern void hs7729pci_outw(unsigned short value, unsigned long port);
extern void hs7729pci_outl(unsigned int value, unsigned long port);

extern unsigned char hs7729pci_inb_p(unsigned long port);
extern void hs7729pci_outb_p(unsigned char value, unsigned long port);

extern void hs7729pci_insb(unsigned long port, void *addr, unsigned long count);
extern void hs7729pci_insw(unsigned long port, void *addr, unsigned long count);
extern void hs7729pci_insl(unsigned long port, void *addr, unsigned long count);
extern void hs7729pci_outsb(unsigned long port, const void *addr, unsigned long count);
extern void hs7729pci_outsw(unsigned long port, const void *addr, unsigned long count);
extern void hs7729pci_outsl(unsigned long port, const void *addr, unsigned long count);

extern unsigned char hs7729pci_readb(unsigned long addr);
extern unsigned short hs7729pci_readw(unsigned long addr);
extern unsigned long hs7729pci_readl(unsigned long addr);
extern void hs7729pci_writeb(unsigned char b, unsigned long addr);
extern void hs7729pci_writew(unsigned short b, unsigned long addr);
extern void hs7729pci_writel(unsigned int b, unsigned long addr);

extern unsigned long hs7729pci_isa_port2addr(unsigned long offset);

extern void *hs7729pci_ioremap(unsigned long, unsigned long);
extern void hs7729pci_iounmap(void *addr);

#ifdef __WANT_IO_DEF

# define __inb			hs7729pci_inb
# define __inw			hs7729pci_inw
# define __inl			hs7729pci_inl
# define __outb			hs7729pci_outb
# define __outw			hs7729pci_outw
# define __outl			hs7729pci_outl

# define __inb_p		hs7729pci_inb_p
# define __inw_p		hs7729pci_inw
# define __inl_p		hs7729pci_inl
# define __outb_p		hs7729pci_outb_p
# define __outw_p		hs7729pci_outw
# define __outl_p		hs7729pci_outl

# define __insb			hs7729pci_insb
# define __insw			hs7729pci_insw
# define __insl			hs7729pci_insl
# define __outsb		hs7729pci_outsb
# define __outsw		hs7729pci_outsw
# define __outsl		hs7729pci_outsl

#if 0
# define __readb		hs7729pci_readb
# define __readw		hs7729pci_readw
# define __readl		hs7729pci_readl
# define __writeb		hs7729pci_writeb
# define __writew		hs7729pci_writew
# define __writel		hs7729pci_writel
#else
# define __readb		generic_readb
# define __readw		generic_readw
# define __readl		generic_readl
# define __writeb		generic_writeb
# define __writew		generic_writew
# define __writel		generic_writel
#endif

# define __isa_port2addr	hs7729pci_isa_port2addr
# define __ioremap		hs7729pci_ioremap
# define __ioremap_nocache	hs7729pci_ioremap
# define __iounmap		hs7729pci_iounmap

#endif

#endif /* _ASM_SH_IO_HS7729PCI_H */
