/*
 * FILE NAME
 *	include/asm-mips/vr41xx/vrc4173.h
 *
 * BRIEF MODULE DESCRIPTION
 *	Include file for NEC VRC4173.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2000 by Michael R. McDonald
 *
 * Copyright 2001-2003 Montavista Software Inc.
 * Author: Yoichi Yuasa
 *         yyuasa@mvista.com or source@mvista.com
 */
#ifndef __NEC_VRC4173_H 
#define __NEC_VRC4173_H 

#include <asm/io.h>

/*
 * Interrupt Number
 */
#define VRC4173_IRQ_BASE	72
#define VRC4173_IRQ(x)		(VRC4173_IRQ_BASE + (x))
#define VRC4173_USB_IRQ		VRC4173_IRQ(0)
#define VRC4173_PCMCIA2_IRQ	VRC4173_IRQ(1)
#define VRC4173_PCMCIA1_IRQ	VRC4173_IRQ(2)
#define VRC4173_PS2CH2_IRQ	VRC4173_IRQ(3)
#define VRC4173_PS2CH1_IRQ	VRC4173_IRQ(4)
#define VRC4173_PIU_IRQ		VRC4173_IRQ(5)
#define VRC4173_AIU_IRQ		VRC4173_IRQ(6)
#define VRC4173_KIU_IRQ		VRC4173_IRQ(7)
#define VRC4173_GIU_IRQ		VRC4173_IRQ(8)
#define VRC4173_AC97_IRQ	VRC4173_IRQ(9)
#define VRC4173_AC97INT1_IRQ	VRC4173_IRQ(10)
/* RFU */
#define VRC4173_DOZEPIU_IRQ	VRC4173_IRQ(13)
#define VRC4173_IRQ_LAST	VRC4173_DOZEPIU_IRQ

/*
 * PCI I/O accesses
 */
extern unsigned long vrc4173_io_offset;

#define set_vrc4173_io_offset(offset)	do { vrc4173_io_offset = (offset); } while (0)

#define vrc4173_outb(val,port)		outb((val), vrc4173_io_offset+(port))
#define vrc4173_outw(val,port)		outw((val), vrc4173_io_offset+(port))
#define vrc4173_outl(val,port)		outl((val), vrc4173_io_offset+(port))
#define vrc4173_outb_p(val,port)	outb_p((val), vrc4173_io_offset+(port))
#define vrc4173_outw_p(val,port)	outw_p((val), vrc4173_io_offset+(port))
#define vrc4173_outl_p(val,port)	outl_p((val), vrc4173_io_offset+(port))

#define vrc4173_inb(port)		inb(vrc4173_io_offset+(port))
#define vrc4173_inw(port)		inw(vrc4173_io_offset+(port))
#define vrc4173_inl(port)		inl(vrc4173_io_offset+(port))
#define vrc4173_inb_p(port)		inb_p(vrc4173_io_offset+(port))
#define vrc4173_inw_p(port)		inw_p(vrc4173_io_offset+(port))
#define vrc4173_inl_p(port)		inl_p(vrc4173_io_offset+(port))

#define vrc4173_outsb(port,addr,count)	outsb(vrc4173_io_offset+(port),(addr),(count))
#define vrc4173_outsw(port,addr,count)	outsw(vrc4173_io_offset+(port),(addr),(count))
#define vrc4173_outsl(port,addr,count)	outsl(vrc4173_io_offset+(port),(addr),(count))

#define vrc4173_insb(port,addr,count)	insb(vrc4173_io_offset+(port),(addr),(count))
#define vrc4173_insw(port,addr,count)	insw(vrc4173_io_offset+(port),(addr),(count))
#define vrc4173_insl(port,addr,count)	insl(vrc4173_io_offset+(port),(addr),(count))

/*
 * Clock Mask Unit
 */
#define VRC4173_PIU_CLOCK		0x0001
#define VRC4173_KIU_CLOCK		0x0002
#define VRC4173_AIU_CLOCK		0x0004
#define VRC4173_PS2CH1_CLOCK		0x0008
#define VRC4173_PS2CH2_CLOCK		0x0010
#define VRC4173_USBU_PCI_CLOCK		0x0020
#define VRC4173_CARDU1_PCI_CLOCK	0x0040
#define VRC4173_CARDU2_PCI_CLOCK	0x0080
#define VRC4173_AC97U_PCI_CLOCK		0x0100
#define VRC4173_USBU_48MHz_CLOCK	0x0400
#define VRC4173_EXT_48MHz_CLOCK		0x0800
#define VRC4173_48MHz_CLOCK		0x1000

extern void vrc4173_clock_supply(u16 mask);
extern void vrc4173_clock_mask(u16 mask);

/*
 * General-Purpose I/O Unit
 */
enum {
	PS2CH1_SELECT,
	PS2CH2_SELECT,
	TOUCHPANEL_SELECT,
	KIU8_SELECT,
	KIU10_SELECT,
	KIU12_SELECT,
	GPIO_SELECT
};

extern void vrc4173_select_function(int func);

#endif /* __NEC_VRC4173_H */
