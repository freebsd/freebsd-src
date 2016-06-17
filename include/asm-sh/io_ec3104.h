#ifndef _ASM_SH_IO_EC3104_H
#define _ASM_SH_IO_EC3104_H

#include <asm/io_generic.h>
#include <linux/types.h>

extern unsigned char ec3104_inb(unsigned long port);
extern unsigned short ec3104_inw(unsigned long port);
extern unsigned long ec3104_inl(unsigned long port);

extern void ec3104_outb(unsigned char value, unsigned long port);
extern void ec3104_outw(unsigned short value, unsigned long port);
extern void ec3104_outl(unsigned long value, unsigned long port);

extern int ec3104_irq_demux(int irq);

#ifdef __WANT_IO_DEF

# define __inb			ec3104_inb
# define __inw			ec3104_inw
# define __inl			ec3104_inl
# define __outb			ec3104_outb
# define __outw			ec3104_outw
# define __outl			ec3104_outl

# define __inb_p		ec3104_inb
# define __inw_p		ec3104_inw
# define __inl_p		ec3104_inl
# define __outb_p		ec3104_outb
# define __outw_p		ec3104_outw
# define __outl_p		ec3104_outl

# define __insb			generic_insb
# define __insw			generic_insw
# define __insl			generic_insl
# define __outsb		generic_outsb
# define __outsw		generic_outsw
# define __outsl		generic_outsl

# define __readb		generic_readb
# define __readw		generic_readw
# define __readl		generic_readl
# define __writeb		generic_writeb
# define __writew		generic_writew
# define __writel		generic_writel

# define __isa_port2addr	generic_isa_port2addr
# define __ioremap		generic_ioremap
# define __ioremap_nocache	generic_ioremap_nocache
# define __iounmap		generic_iounmap

#endif

#endif /* _ASM_SH_IO_EC3104_H */
