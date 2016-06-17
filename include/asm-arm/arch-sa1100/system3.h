/*
 * linux/include/asm-arm/arch-sa1100/system3.h
 *
 * Copyright (C) 2001 Stefan Eletzhofer <stefan.eletzhofer@eletztrick.de>
 *
 * $Id: system3.h,v 1.2.4.2 2001/12/04 14:58:50 seletz Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * $Log: system3.h,v $
 * Revision 1.2.4.2  2001/12/04 14:58:50  seletz
 * - removed neponset hack
 * - removed irq definitions (now in irqs.h)
 *
 * Revision 1.2.4.1  2001/12/04 12:51:18  seletz
 * - re-added from linux_2_4_8_ac12_rmk1_np1_pt1
 *
 * Revision 1.2.2.2  2001/11/16 13:58:43  seletz
 * - simplified cpld register access
 *
 * Revision 1.2.2.1  2001/10/15 16:17:20  seletz
 * - first revision
 *
 *
 */

#ifndef __ASM_ARCH_HARDWARE_H
#error "include <asm/hardware.h> instead"
#endif

/* System 3 LCD */
#define SYS3LCD			SKPEN0
#define SYS3LCDBACKL	SKPEN1
#define SYS3LCDBRIGHT	SKPWM0
#define SYS3LCDCONTR	SKPWM1

#define PT_CPLD_BASE		(0x10000000)
#define PT_SMC_BASE			(0x18000000)
#define PT_SA1111_BASE		(0x40000000)

#define Ptcpld_p2v( x )	((x) - PT_CPLD_BASE + 0xf3000000)
#define Ptcpld_v2p( x )	((x) - 0xf3000000 + PT_CPLD_BASE)

#define _PT_SYSID	( PT_CPLD_BASE + 0x00 )
#define _PT_IRQSR	( PT_CPLD_BASE + 0x24 )
#define _PT_CTRL0	( PT_CPLD_BASE + 0x90 )
#define _PT_CTRL1	( PT_CPLD_BASE + 0xA0 )
#define _PT_CTRL2	( PT_CPLD_BASE + 0xB0 )

#define PT_SYSID	(*((volatile u_char *)Ptcpld_p2v( _PT_SYSID )))
#define PT_IRQSR	(*((volatile u_char *)Ptcpld_p2v( _PT_IRQSR )))
#define PT_CTRL0	(*((volatile u_char *)Ptcpld_p2v( _PT_CTRL0 )))
#define PT_CTRL1	(*((volatile u_char *)Ptcpld_p2v( _PT_CTRL1 )))
#define PT_CTRL2	(*((volatile u_char *)Ptcpld_p2v( _PT_CTRL2 )))

#define PTCTRL0_set( x )	PT_CTRL0 |= (x)
#define PTCTRL1_set( x )	PT_CTRL1 |= (x)
#define PTCTRL2_set( x )	PT_CTRL2 |= (x)
#define PTCTRL0_clear( x )	PT_CTRL0 &= ~(x)
#define PTCTRL1_clear( x )	PT_CTRL1 &= ~(x)
#define PTCTRL2_clear( x )	PT_CTRL2 &= ~(x)

/* System ID register */

/* IRQ Source Register */
#define PT_IRQ_LAN		( 1<<0 )
#define PT_IRQ_X		( 1<<1 )
#define PT_IRQ_SA1111	( 1<<2 )
#define PT_IRQ_RS1		( 1<<3 )
#define PT_IRQ_RS1_RING	( 1<<4 )
#define PT_IRQ_RS1_DCD	( 1<<5 )
#define PT_IRQ_RS1_DSR	( 1<<6 )
#define PT_IRQ_RS2		( 1<<7 )

/* FIXME */
#define PT_IRQ_USAR		( 1<<1 )

/* CTRL 0 */
#define PT_CTRL0_USBSLAVE	( 1<<0 )
#define PT_CTRL0_USBHOST	( 1<<1 )
#define PT_CTRL0_LCD_BL		( 1<<2 )
#define PT_CTRL0_LAN_EN		( 1<<3 )	/* active low */
#define PT_CTRL0_IRDA_M(x)	( (((u_char)x)&0x03)<<4 )
#define PT_CTRL0_IRDA_M0	( 1<<4 )
#define PT_CTRL0_IRDA_M1	( 1<<5 )
#define PT_CTRL0_IRDA_FSEL	( 1<<6 )
#define PT_CTRL0_LCD_EN		( 1<<7 )

#define PT_CTRL0_INIT	( PT_CTRL0_USBSLAVE | PT_CTRL0_USBHOST | \
						PT_CTRL0_LCD_BL | PT_CTRL0_LAN_EN | PT_CTRL0_LCD_EN )

/* CTRL 1 */
#define PT_CTRL1_RS3_MUX(x) ( (((u_char)x)&0x03)<<0 )
#define PT_CTRL1_RS3_MUX0	( 1<<0 )
#define PT_CTRL1_RS3_MUX1	( 1<<1 )
#define PT_CTRL1_RS3_RST	( 1<<2 )
#define PT_CTRL1_RS3_RS485_TERM	( 1<<4 )
#define PT_CTRL1_X			( 1<<4 )
#define PT_CTRL1_PCMCIA_A0VPP	( 1<<6 )
#define PT_CTRL1_PCMCIA_A1VPP	( 1<<7 )

#define PT_RS3_MUX_ALIRS	( 0 )
#define PT_RS3_MUX_IDATA	( 1 )
#define PT_RS3_MUX_RADIO	( 2 )
#define PT_RS3_MUX_RS485	( 3 )

/* CTRL 2 */
#define PT_CTRL2_RS1_RTS	( 1<<0 )
#define PT_CTRL2_RS1_DTR	( 1<<1 )
