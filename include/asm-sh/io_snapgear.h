/*
 * include/asm-sh/io_snapgear.h
 *
 * Modified version of io_se.h for the snapgear-specific functions.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * IO functions for a SnapGear
 */

#ifndef _ASM_SH_IO_SNAPGEAR_H
#define _ASM_SH_IO_SNAPGEAR_H

#include <asm/irq.h>

#if defined(__SH4__)
/*
 * The external interrupt lines, these take up ints 0 - 15 inclusive
 * depending on the priority for the interrupt.  In fact the priority
 * is the interrupt :-)
 */

#define IRL0_IRQ		2
#define IRL0_IPR_ADDR	INTC_IPRD
#define IRL0_IPR_POS	3
#define IRL0_PRIORITY	13

#define IRL1_IRQ		5
#define IRL1_IPR_ADDR	INTC_IPRD
#define IRL1_IPR_POS	2
#define IRL1_PRIORITY	10

#define IRL2_IRQ		8
#define IRL2_IPR_ADDR	INTC_IPRD
#define IRL2_IPR_POS	1
#define IRL2_PRIORITY	7

#define IRL3_IRQ		11
#define IRL3_IPR_ADDR	INTC_IPRD
#define IRL3_IPR_POS	0
#define IRL3_PRIORITY	4
#endif

#include <asm/io_generic.h>

extern unsigned char snapgear_inb(unsigned long port);
extern unsigned short snapgear_inw(unsigned long port);
extern unsigned int snapgear_inl(unsigned long port);

extern void snapgear_outb(unsigned char value, unsigned long port);
extern void snapgear_outw(unsigned short value, unsigned long port);
extern void snapgear_outl(unsigned int value, unsigned long port);

extern unsigned char snapgear_inb_p(unsigned long port);
extern void snapgear_outb_p(unsigned char value, unsigned long port);

extern void snapgear_insb(unsigned long port, void *addr, unsigned long count);
extern void snapgear_insw(unsigned long port, void *addr, unsigned long count);
extern void snapgear_insl(unsigned long port, void *addr, unsigned long count);
extern void snapgear_outsb(unsigned long port, const void *addr, unsigned long count);
extern void snapgear_outsw(unsigned long port, const void *addr, unsigned long count);
extern void snapgear_outsl(unsigned long port, const void *addr, unsigned long count);

extern unsigned char snapgear_readb(unsigned long addr);
extern unsigned short snapgear_readw(unsigned long addr);
extern unsigned int snapgear_readl(unsigned long addr);
extern void snapgear_writeb(unsigned char b, unsigned long addr);
extern void snapgear_writew(unsigned short b, unsigned long addr);
extern void snapgear_writel(unsigned int b, unsigned long addr);

extern unsigned long snapgear_isa_port2addr(unsigned long offset);

#ifdef __WANT_IO_DEF

# define __inb			snapgear_inb
# define __inw			snapgear_inw
# define __inl			snapgear_inl
# define __outb			snapgear_outb
# define __outw			snapgear_outw
# define __outl			snapgear_outl

# define __inb_p		snapgear_inb_p
# define __inw_p		snapgear_inw
# define __inl_p		snapgear_inl
# define __outb_p		snapgear_outb_p
# define __outw_p		snapgear_outw
# define __outl_p		snapgear_outl

# define __insb			snapgear_insb
# define __insw			snapgear_insw
# define __insl			snapgear_insl
# define __outsb		snapgear_outsb
# define __outsw		snapgear_outsw
# define __outsl		snapgear_outsl

# define __readb		snapgear_readb
# define __readw		snapgear_readw
# define __readl		snapgear_readl
# define __writeb		snapgear_writeb
# define __writew		snapgear_writew
# define __writel		snapgear_writel

# define __isa_port2addr	snapgear_isa_port2addr
# define __ioremap		generic_ioremap
# define __iounmap		generic_iounmap

#endif

#ifdef CONFIG_SH_SECUREEDGE5410
/*
 * We need to remember what was written to the ioport as some bits
 * are shared with other functions and you cannot read back what was
 * written :-|
 *
 * Bit        Read                   Write
 * -----------------------------------------------
 * D0         DCD on ttySC1          power
 * D1         Reset Switch           heatbeat
 * D2         ttySC0 CTS (7100)      LAN
 * D3         -                      WAN
 * D4         ttySC0 DCD (7100)      CONSOLE
 * D5         -                      ONLINE
 * D6         -                      VPN
 * D7         -                      DTR on ttySC1
 * D8         -                      ttySC0 RTS (7100)
 * D9         -                      ttySC0 DTR (7100)
 * D10        -                      RTC SCLK
 * D11        RTC DATA               RTC DATA
 * D12        -                      RTS RESET
 */

 #define SECUREEDGE_IOPORT_ADDR ((volatile short *) 0xb0000000)
 extern unsigned short secureedge5410_ioport;

 #define SECUREEDGE_WRITE_IOPORT(val, mask) (*SECUREEDGE_IOPORT_ADDR = \
		 (secureedge5410_ioport = \
		 		((secureedge5410_ioport & ~(mask)) | ((val) & (mask)))))
 #define SECUREEDGE_READ_IOPORT() \
 		 ((*SECUREEDGE_IOPORT_ADDR&0x0817) | (secureedge5410_ioport&~0x0817))
#endif

#endif /* _ASM_SH_IO_SNAPGEAR_H */
