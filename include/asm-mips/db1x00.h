/*
 * AMD Alchemy DB1x00 Reference Boards
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * 
 */
#ifndef __ASM_DB1X00_H
#define __ASM_DB1X00_H


/*
 * Overlay data structure of the Db1x00 board registers.
 * Registers located at physical 1E0000xx, KSEG1 0xAE0000xx
 */
typedef volatile struct
{
	/*00*/	unsigned long whoami;
	/*04*/	unsigned long status;
	/*08*/	unsigned long switches;
	/*0C*/	unsigned long resets;
	/*10*/	unsigned long pcmcia;
	/*14*/	unsigned long specific;
	/*18*/	unsigned long leds;
	/*1C*/	unsigned long swreset;

} BCSR;


/*
 * Register/mask bit definitions for the BCSRs
 */
#define BCSR_WHOAMI_DCID		0x000F
#define BCSR_WHOAMI_CPLD		0x00F0
#define BCSR_WHOAMI_BOARD		0x0F00

#define BCSR_STATUS_PC0VS		0x0003
#define BCSR_STATUS_PC1VS		0x000C
#define BCSR_STATUS_PC0FI		0x0010
#define BCSR_STATUS_PC1FI		0x0020
#define BCSR_STATUS_FLASHBUSY		0x0100
#define BCSR_STATUS_ROMBUSY		0x0400
#define BCSR_STATUS_SWAPBOOT		0x2000
#define BCSR_STATUS_FLASHDEN		0xC000

#define BCSR_SWITCHES_DIP		0x00FF
#define BCSR_SWITCHES_DIP_1		0x0080
#define BCSR_SWITCHES_DIP_2		0x0040
#define BCSR_SWITCHES_DIP_3		0x0020
#define BCSR_SWITCHES_DIP_4		0x0010
#define BCSR_SWITCHES_DIP_5		0x0008
#define BCSR_SWITCHES_DIP_6		0x0004
#define BCSR_SWITCHES_DIP_7		0x0002
#define BCSR_SWITCHES_DIP_8		0x0001
#define BCSR_SWITCHES_ROTARY		0x0F00

#define BCSR_RESETS_PHY0		0x0001
#define BCSR_RESETS_PHY1		0x0002
#define BCSR_RESETS_DC			0x0004
#define BCSR_RESETS_FIR_SEL		0x2000
#define BCSR_RESETS_IRDA_MODE_MASK	0xC000
#define BCSR_RESETS_IRDA_MODE_FULL	0x0000
#define BCSR_RESETS_IRDA_MODE_OFF	0x4000
#define BCSR_RESETS_IRDA_MODE_2_3	0x8000
#define BCSR_RESETS_IRDA_MODE_1_3	0xC000

#define BCSR_PCMCIA_PC0VPP		0x0003
#define BCSR_PCMCIA_PC0VCC		0x000C
#define BCSR_PCMCIA_PC0DRVEN		0x0010
#define BCSR_PCMCIA_PC0RST		0x0080
#define BCSR_PCMCIA_PC1VPP		0x0300
#define BCSR_PCMCIA_PC1VCC		0x0C00
#define BCSR_PCMCIA_PC1DRVEN		0x1000
#define BCSR_PCMCIA_PC1RST		0x8000

#define BCSR_BOARD_PCIM66EN		0x0001
#define BCSR_BOARD_PCIM33		0x0100
#define BCSR_BOARD_GPIO200RST		0x0400
#define BCSR_BOARD_PCICFG		0x1000

#define BCSR_LEDS_DECIMALS		0x0003
#define BCSR_LEDS_LED0			0x0100
#define BCSR_LEDS_LED1			0x0200
#define BCSR_LEDS_LED2			0x0400
#define BCSR_LEDS_LED3			0x0800

#define BCSR_SWRESET_RESET		0x0080

/* PCMCIA Db1x00 specific defines */
#define PCMCIA_MAX_SOCK 1
#define PCMCIA_NUM_SOCKS (PCMCIA_MAX_SOCK+1)

/* VPP/VCC */
#define SET_VCC_VPP(VCC, VPP, SLOT)\
	((((VCC)<<2) | ((VPP)<<0)) << ((SLOT)*8))

/* MTD CONFIG OPTIONS */
#if defined(CONFIG_MTD_DB1X00_BOOT) && defined(CONFIG_MTD_DB1X00_USER)
#define DB1X00_BOTH_BANKS
#elif defined(CONFIG_MTD_DB1X00_BOOT) && !defined(CONFIG_MTD_DB1X00_USER)
#define DB1X00_BOOT_ONLY
#elif !defined(CONFIG_MTD_DB1X00_BOOT) && defined(CONFIG_MTD_DB1X00_USER)
#define DB1X00_USER_ONLY
#endif

#endif /* __ASM_DB1X00_H */
