/*
 * linux/include/asm-arm/arch-sa1100/graphicsmaster.h
 *
 * Created 2000/12/18 by Woojung Huh <whuh@applieddata.net>
 *
 * This file comes from graphicsclient.h of Nicolas Pitre <nico@cam.org>
 *
 * This file contains the hardware specific definitions for the
 * ADS GraphicsMaster
 */

#ifndef __ASM_ARCH_HARDWARE_H
#error "include <asm/hardware.h> instead"
#endif

#define ADS_CPLD_BASE		(0x10000000)
#define ADS_p2v( x )		((x) - ADS_CPLD_BASE + 0xf0000000)
#define ADS_v2p( x )		((x) - 0xf0000000 + ADS_CPLD_BASE)


#define _ADS_SW_SWITCHES	0x10060000	/* Software Switches */

/* Extra IRQ Controller */
#define _ADS_INT_ST1		0x10080000	/* IRQ Status #1 */
#define _ADS_INT_ST2		0x10080004	/* IRQ Status #2 */
#define _ADS_INT_EN1		0x10080008	/* IRQ Enable #1 */
#define _ADS_INT_EN2		0x1008000c	/* IRQ Enable #2 */
#define _ADS_DCR			0x10080018	/* Discrete Control Reg */

/* Discrete Controller (AVR:Atmel AT90LS8535) */
#define _ADS_AVR_REG		0x10080018

/* On-Board Ethernet */
#define _ADS_ETHERNET		0x100e0000	/* Ethernet */

/* On-Board Quad UART 16C554 */
#define	ADS_QUAD_UART1		0x10100000
#define	ADS_QUAD_UART2		0x10120000
#define	ADS_QUAD_UART3		0x10140000
#define	ADS_QUAD_UART4		0x10160000

/* LEDs */
#define ADS_LED0	GPIO_GPIO20		/* on-board Green */
#define ADS_LED1	GPIO_GPIO25		/* on-board Yellow */
#define ADS_LED2	GPIO_GPIO26		/* on-board Red */

/* DCR */
#define DCR_AVR_RESET		0x01
#define DCR_SA1111_RESET	0x02
#define	DCR_BACKLITE_ON		0x04

/* Virtual register addresses */

#ifndef __ASSEMBLY__
#define ADS_INT_ST1	(*((volatile u_char *) ADS_p2v(_ADS_INT_ST1)))
#define ADS_INT_ST2	(*((volatile u_char *) ADS_p2v(_ADS_INT_ST2)))
#define ADS_INT_EN1	(*((volatile u_char *) ADS_p2v(_ADS_INT_EN1)))
#define ADS_INT_EN2	(*((volatile u_char *) ADS_p2v(_ADS_INT_EN2)))
#define ADS_ETHERNET	((int) ADS_p2v(_ADS_ETHERNET))
#define ADS_AVR_REG	(*((volatile u_char *) ADS_p2v(_ADS_AVR_REG)))
#define ADS_DCR		(*((volatile u_char *) ADS_p2v(_ADS_DCR)))
#endif

#define ADS_SA1111_BASE		(0x18000000)
