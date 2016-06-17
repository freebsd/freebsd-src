/*
 * include/asm-sh/io_shmse.h
 *
 * Copyright (C) 2003 Takashi Kusuda <kusuda-takashi@hitachi-ul.co.jp>
 * IO functions for SH-Mobile(SH7300) SolutionEngine
 */

#ifndef _ASM_SH_IO_SHMSE_H
#define _ASM_SH_IO_SHMSE_H

#include <asm/io_generic.h>

extern unsigned char shmse_inb(unsigned long port);
extern unsigned short shmse_inw(unsigned long port);
extern unsigned int shmse_inl(unsigned long port);

extern void shmse_outb(unsigned char value, unsigned long port);
extern void shmse_outw(unsigned short value, unsigned long port);
extern void shmse_outl(unsigned int value, unsigned long port);

extern unsigned char shmse_inb_p(unsigned long port);
extern void shmse_outb_p(unsigned char value, unsigned long port);

extern void shmse_insb(unsigned long port, void *addr, unsigned long count);
extern void shmse_insw(unsigned long port, void *addr, unsigned long count);
extern void shmse_insl(unsigned long port, void *addr, unsigned long count);
extern void shmse_outsb(unsigned long port, const void *addr, unsigned long count);
extern void shmse_outsw(unsigned long port, const void *addr, unsigned long count);
extern void shmse_outsl(unsigned long port, const void *addr, unsigned long count);

extern unsigned char shmse_readb(unsigned long addr);
extern unsigned short shmse_readw(unsigned long addr);
extern unsigned int shmse_readl(unsigned long addr);
extern void shmse_writeb(unsigned char b, unsigned long addr);
extern void shmse_writew(unsigned short b, unsigned long addr);
extern void shmse_writel(unsigned int b, unsigned long addr);

#ifdef __WANT_IO_DEF

# define __inb			shmse_inb
# define __inw			shmse_inw
# define __inl			shmse_inl
# define __outb			shmse_outb
# define __outw			shmse_outw
# define __outl			shmse_outl

# define __inb_p		shmse_inb_p
# define __inw_p		shmse_inw
# define __inl_p		shmse_inl
# define __outb_p		shmse_outb_p
# define __outw_p		shmse_outw
# define __outl_p		shmse_outl

# define __insb			shmse_insb
# define __insw			shmse_insw
# define __insl			shmse_insl
# define __outsb		shmse_outsb
# define __outsw		shmse_outsw
# define __outsl		shmse_outsl

# define __readb		shmse_readb
# define __readw		shmse_readw
# define __readl		shmse_readl
# define __writeb		shmse_writeb
# define __writew		shmse_writew
# define __writel		shmse_writel

# define __ioremap		generic_ioremap
# define __iounmap		generic_iounmap

# define __isa_port2addr

#endif

#endif /* _ASM_SH_IO_SHMSE_H */
