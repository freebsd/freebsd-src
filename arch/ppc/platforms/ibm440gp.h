/*
 * arch/ppc/platforms/ibm440gp.h
 *
 * PPC440GP definitions
 *
 * Roland Dreier <roland@digitalvampire.org>
 *
 * Copyright 2002 Roland Dreier
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This file contains code that was originally in the files ibm44x.h
 * and ebony.h, which were written by Matt Porter of MontaVista Software Inc.
 */

#ifdef __KERNEL__
#ifndef __PPC_PLATFORMS_IBM440GP_H
#define __PPC_PLATFORMS_IBM440GP_H

#include <linux/config.h>

#include <asm/ibm44x.h>

#define EMAC_NUMS		2
#define UART_NUMS		2
#define ZMII_NUMS		1
#define IIC_NUMS		2
#define IIC0_IRQ		2
#define IIC1_IRQ		3
#define GPIO_NUMS		1

/* UART location */
#define PPC440GP_UART0_ADDR	0x0000000140000200ULL
#define PPC440GP_UART1_ADDR	0x0000000140000300ULL
#define UART0_INT		0
#define UART1_INT		1

/* EMAC location */
#define PPC440GP_EMAC0_ADDR	0x0000000140000800ULL
#define PPC440GP_EMAC1_ADDR	0x0000000140000900ULL
#define PPC440GP_EMAC_SIZE	0x70

/* EMAC IRQ's */
#define BL_MAC_WOL	61	/* WOL */
#define BL_MAC_WOL1	63	/* WOL */
#define BL_MAL_SERR	32	/* MAL SERR */
#define BL_MAL_TXDE	33	/* MAL TXDE */
#define BL_MAL_RXDE	34	/* MAL RXDE */
#define BL_MAL_TXEOB	10	/* MAL TX EOB */
#define BL_MAL_RXEOB	11	/* MAL RX EOB */
#define BL_MAC_ETH0	60	/* MAC */
#define BL_MAC_ETH1	62	/* MAC */

/* ZMII location */
#define PPC440GP_ZMII_ADDR	0x0000000140000780ULL
#define PPC440GP_ZMII_SIZE	0x0c

/* I2C location */
#define PPC440GP_IIC0_ADDR	0x40000400
#define PPC440GP_IIC1_ADDR	0x40000500

/* GPIO location */
#define PPC440GP_GPIO0_ADDR	0x0000000140000700ULL

/* Clock and Power Management */
#define IBM_CPM_IIC0		0x80000000	/* IIC interface */
#define IBM_CPM_IIC1		0x40000000	/* IIC interface */
#define IBM_CPM_PCI		0x20000000	/* PCI bridge */
#define IBM_CPM_CPU		0x02000000	/* processor core */
#define IBM_CPM_DMA		0x01000000	/* DMA controller */
#define IBM_CPM_BGO		0x00800000	/* PLB to OPB bus arbiter */
#define IBM_CPM_BGI		0x00400000	/* OPB to PLB bridge */
#define IBM_CPM_EBC		0x00200000	/* External Bux Controller */
#define IBM_CPM_EBM		0x00100000	/* Ext Bus Master Interface */
#define IBM_CPM_DMC		0x00080000	/* SDRAM peripheral controller */
#define IBM_CPM_PLB		0x00040000	/* PLB bus arbiter */
#define IBM_CPM_SRAM		0x00020000	/* SRAM memory controller */
#define IBM_CPM_PPM		0x00002000	/* PLB Performance Monitor */
#define IBM_CPM_UIC1		0x00001000	/* Universal Interrupt Controller */
#define IBM_CPM_GPIO0		0x00000800	/* General Purpose IO (??) */
#define IBM_CPM_GPT		0x00000400	/* General Purpose Timers  */
#define IBM_CPM_UART0		0x00000200	/* serial port 0 */
#define IBM_CPM_UART1		0x00000100	/* serial port 1 */
#define IBM_CPM_UIC0		0x00000080	/* Universal Interrupt Controller */
#define IBM_CPM_TMRCLK		0x00000040	/* CPU timers */

#define DFLT_IBM4xx_PM		~(IBM_CPM_UIC | IBM_CPM_UIC1 | IBM_CPM_CPU \
				| IBM_CPM_EBC | IBM_CPM_SRAM | IBM_CPM_BGO \
				| IBM_CPM_EBM | IBM_CPM_PLB | IBM_CPM_OPB \
				| IBM_CPM_TMRCLK | IBM_CPM_DMA | IBM_CPM_PCI)

#define PPC440GP_OPB_BASE_START	0x0000000140000000ULL

/*
 * Serial port defines
 */
#define RS_TABLE_SIZE	2

#endif /* __PPC_PLATFORMS_IBM440GP_H */
#endif /* __KERNEL__ */
