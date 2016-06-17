/*
 * arch/ppc/platforms/ibm440gx.h
 *
 * PPC440GX definitions
 *
 * Matt Porter <mporter@mvista.com>
 *
 * Copyright 2002 Roland Dreier
 * Copyright 2003 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 * 
 */

#ifdef __KERNEL__
#ifndef __PPC_PLATFORMS_IBM440GX_H
#define __PPC_PLATFORMS_IBM440GX_H

#include <linux/config.h>

#include <asm/ibm44x.h>

/* UART */
#define PPC440GX_UART0_ADDR	0x0000000140000200
#define PPC440GX_UART1_ADDR	0x0000000140000300
#define UART0_INT		0
#define UART1_INT		1

/* EMAC */
#define PPC440GX_EMAC0_ADDR	0x0000000140000800
#define PPC440GX_EMAC1_ADDR	0x0000000140000900
#define PPC440GX_EMAC2_ADDR	0x0000000140000C00
#define PPC440GX_EMAC3_ADDR	0x0000000140000E00
#define PPC440GX_EMAC_SIZE	0xFC
#define EMAC_NUMS               2
#define BL_MAC_WOL	61	/* WOL */
#define BL_MAC_WOL1	63	/* WOL */
#define BL_MAC_WOL2	65	/* WOL */
#define BL_MAC_WOL3	67	/* WOL */
#define BL_MAL_SERR	32	/* MAL SERR */
#define BL_MAL_TXDE	33	/* MAL TXDE */
#define BL_MAL_RXDE	34	/* MAL RXDE */
#define BL_MAL_TXEOB	10	/* MAL TX EOB */
#define BL_MAL_RXEOB	11	/* MAL RX EOB */
#define BL_MAC_ETH0	60	/* MAC */
#define BL_MAC_ETH1	62	/* MAC */
#define BL_MAC_ETH2	64	/* MAC */
#define BL_MAC_ETH3	66	/* MAC */
#define BL_TAH0		68	/* TAH 0 */
#define BL_TAH1		69	/* TAH 1 */

/* TAH */
#define PPC440GX_TAH0_ADDR	0x0000000140000B00
#define PPC440GX_TAH1_ADDR	0x0000000140000D00
#define PPC440GX_TAH_SIZE	0xFC

/* ZMII */
#define PPC440GX_ZMII_ADDR	0x0000000140000780
#define PPC440GX_ZMII_SIZE	0x0c

/* RGMII */
#define PPC440GX_RGMII_ADDR	0x0000000140000790
#define PPC440GX_RGMII_SIZE	0x0c

/* IIC  */
#define PPC440GX_IIC0_ADDR	0x40000400
#define PPC440GX_IIC1_ADDR	0x40000500
#define IIC0_IRQ		2
#define IIC1_IRQ		3

/* GPIO */
#define PPC440GX_GPIO0_ADDR	0x0000000140000700

/* Clock and Power Management */
#define IBM_CPM_IIC0		0x80000000	/* IIC interface */
#define IBM_CPM_IIC1		0x40000000	/* IIC interface */
#define IBM_CPM_PCI		0x20000000	/* PCI bridge */
#define IBM_CPM_RGMII		0x10000000	/* RGMII */
#define IBM_CPM_TAHOE0		0x08000000	/* TAHOE 0 */
#define IBM_CPM_TAHOE1		0x04000000	/* TAHOE 1 */
#define IBM_CPM_CPU		    0x02000000	/* processor core */
#define IBM_CPM_DMA		    0x01000000	/* DMA controller */
#define IBM_CPM_BGO		    0x00800000	/* PLB to OPB bus arbiter */
#define IBM_CPM_BGI		    0x00400000	/* OPB to PLB bridge */
#define IBM_CPM_EBC		    0x00200000	/* External Bux Controller */
#define IBM_CPM_EBM		    0x00100000	/* Ext Bus Master Interface */
#define IBM_CPM_DMC		    0x00080000	/* SDRAM peripheral controller */
#define IBM_CPM_PLB		    0x00040000	/* PLB bus arbiter */
#define IBM_CPM_SRAM		0x00020000	/* SRAM memory controller */
#define IBM_CPM_PPM		    0x00002000	/* PLB Performance Monitor */
#define IBM_CPM_UIC1		0x00001000	/* Universal Interrupt Controller */
#define IBM_CPM_GPIO0		0x00000800	/* General Purpose IO (??) */
#define IBM_CPM_GPT		    0x00000400	/* General Purpose Timers  */
#define IBM_CPM_UART0		0x00000200	/* serial port 0 */
#define IBM_CPM_UART1		0x00000100	/* serial port 1 */
#define IBM_CPM_UIC0		0x00000080	/* Universal Interrupt Controller */
#define IBM_CPM_TMRCLK		0x00000040	/* CPU timers */
#define IBM_CPM_EMAC0  		0x00000020	/* EMAC 0     */
#define IBM_CPM_EMAC1  		0x00000010	/* EMAC 1     */
#define IBM_CPM_EMAC2  		0x00000008	/* EMAC 2     */
#define IBM_CPM_EMAC3  		0x00000004	/* EMAC 3     */

#define DFLT_IBM4xx_PM		~(IBM_CPM_UIC | IBM_CPM_UIC1 | IBM_CPM_CPU \
				| IBM_CPM_EBC | IBM_CPM_SRAM | IBM_CPM_BGO \
				| IBM_CPM_EBM | IBM_CPM_PLB | IBM_CPM_OPB \
				| IBM_CPM_TMRCLK | IBM_CPM_DMA | IBM_CPM_PCI \
				| IBM_CPM_TAHOE0 | IBM_CPM_TAHOE1 \
				| IBM_CPM_EMAC0 | IBM_CPM_EMAC1 \
			  	| IBM_CPM_EMAC2 | IBM_CPM_EMAC3 )	

/* OPB */
#define PPC440GX_OPB_BASE_START	0x0000000140000000

/*
 * Serial port defines
 */
#define RS_TABLE_SIZE	2

#endif /* __PPC_PLATFORMS_IBM440GX_H */
#endif /* __KERNEL__ */
