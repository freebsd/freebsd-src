/*-
 * Copyright (c) 1998, 1999 Scott Mitchell
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: if_xereg.h,v 1.3 1999/02/22 14:00:53 root Exp $
 */

/*
 * Register definitions for Xircom CreditCard Ethernet adapters.  See if_xe.c
 * for details of supported hardware.  Adapted from Werner Koch's 'xirc2ps'
 * driver for Linux and the FreeBSD 'xl' driver (for the MII support).
 */

#include "xe.h"
#if NXE > 0


/*
 * Common registers
 */
#define XE_CR  0	/* Command register (write) */
#define XE_ESR 0	/* Ethernet status register (read) */
#define XE_PSR 1	/* Page select register */
#define XE_EDP 4	/* Ethernet data port */
#define XE_ISR 6	/* Interrupt status register */

/*
 * Command register values
 */
#define XE_CR_TX_PACKET     0x01
#define XE_CR_SOFT_RESET    0x02
#define XE_CR_ENABLE_INTR   0x04
#define XE_CR_FORCE_INTR    0x08
#define XE_CR_CLEAR_FIFO    0x10
#define XE_CR_CLEAR_OVERRUN 0x20
#define XE_CR_RESTART_TX    0x40

/*
 * Status register values
 */
#define XE_ESR_FULL_PKT_RX  0x01
#define XE_ESR_PKT_REJECT   0x04
#define XE_ESR_TX_PENDING   0x08
#define XE_ESR_BAD_POLARITY 0x10
#define XE_ESR_MEDIA_SELECT 0x20

/*
 * Interrupt register values
 */
#define XE_ISR_TX_OVERFLOW 0x01
#define XE_ISR_TX_PACKET   0x02
#define XE_ISR_MAC_INTR    0x04
#define XE_ISR_TX_RES      0x08
#define XE_ISR_RX_PACKET   0x20
#define XE_ISR_RX_REJECT   0x40
#define XE_ISR_FORCE_INTR  0x80


/*
 * Page 0 registers
 */
#define XE_TSO 8	/* Transmit space open */
#define XE_TRS 10	/* Transmit reservation size */
#define XE_DOR 12	/* Data offset register (write) */
#define XE_RSR 12	/* Receive status register (read) */
#define XE_PTR 13	/* Packets transmitted register (read) */
#define XE_RBC 14	/* Received byte count (read) */

/*
 * RSR values
 */
#define XE_RSR_PHYS_PKT  0x01
#define XE_RSR_BCAST_PKT 0x02
#define XE_RSR_LONG_PKT  0x04
#define XE_RSR_ALIGN_ERR 0x10
#define XE_RSR_CRC_ERR   0x20
#define XE_RSR_RX_OK     0x80


/*
 * Page 1 registers
 */
#define XE_IMR0 12	/* Interrupt mask register, part 1 */
#define XE_IMR1 13	/* Interrupt mask register, part 2 */
#define XE_ECR  14	/* Ethernet configuration register */

/*
 * ECR values
 */
#define XE_ECR_FULL_DUPLEX  0x04
#define XE_ECR_LONG_TPCABLE 0x08
#define XE_ECR_NO_POLCOL    0x10
#define XE_ECR_NO_LINKPULSE 0x20
#define XE_ECR_NO_AUTOTX    0x40


/*
 * Page 2 registers
 */
#define XE_RBS  8	/* Receive buffer start */
#define XE_LED  10	/* LED configuration register */
#define XE_MSR  12	/* Mohawk specfic register (Mohawk = CE3) */
#define XE_GPR2 13	/* General purpose register 2 */


/*
 * Page 4 registers
 */
#define XE_GPR0 8	/* General purpose register 0 */
#define XE_GPR1 9	/* General purpose register 1 */
#define XE_BOV  10	/* Bonding version register */
#define XE_LMA  12	/* Local memory address */
#define XE_LMD  14	/* Local memory data */


/*
 * Page 5 registers
 */
#define XE_RHS 10	/* Receive host start address */


/*
 * Page 0x40 registers
 */
#define XE_OCR  8	/* The Other command register */
#define XE_RXS0 9	/* Receive status 0 */
#define XE_TXS0 11	/* Transmit status 0 */
#define XE_TXS1 12	/* Transmit status 1 */
#define XE_RXM0 13	/* Receive mask register 0 */
#define XE_TXM0 14      /* Transmit mask register 0 */
#define XE_TXM1 15	/* Transmit mask register 1 */

/*
 * OCR values
 */
#define XE_OCR_TX         0x01
#define XE_OCR_RX_ENABLE  0x04
#define XE_OCR_RX_DISABLE 0x08
#define XE_OCR_ABORT      0x10
#define XE_OCR_ONLINE     0x20
#define XE_OCR_ACK_INTR   0x40
#define XE_OCR_OFFLINE    0x80


/*
 * Page 0x42 registers
 */
#define XE_SWC0 8	/* Software configuration register 0 */
#define XE_SWC1 9	/* Software configuration register 1 */
#define XE_BOC  10	/* Back-off configuration */


/*
 * Page 0x44 registers
 */
#define XE_TDR0 8	/* Time domain reflectometry register 0 */
#define XE_TDR1 9	/* Time domain reflectometry register 1 */
#define XE_RXC0 10	/* Receive byte count low */
#define XE_RXC1 11	/* Receive byte count high */


/*
 * Page 0x45 registers
 */
#define XE_REV  15	/* Revision (read) */


/*
 * Page 0x50 registers
 */
#define XE_IAR  8	/* Individual address register */


/*
 * Pages 0x43, 0x46-0x4f and 0x51-0x5e apparently don't exist.
 * The remainder of 0x0-0x8 and 0x40-0x5f exist, but I have no
 * idea what's on most of them.
 */



/*
 * Definitions for the Micro Linear ML6692 100Base-TX PHY, which handles the
 * 100Mbit functionality of CE3 type cards, including media autonegotiation.
 * It appears to be mostly compatible with the National Semiconductor
 * DP83840A, but with a much smaller register set.  Please refer to the data
 * sheets for these devices for the definitive word on what all this stuff
 * means :)
 *
 * Note that the ML6692 has no 10Mbit capability -- that is handled by another 
 * chip that we don't know anything about.
 *
 * Most of these definitions were adapted from the xl driver.
 */

/*
 * Masks for the MII-related bits in GPR2.  For some reason read and write
 * data are on separate bits.
 */
#define XE_MII_CLK	0x01
#define XE_MII_DIR	0x08
#define XE_MII_WRD	0x02
#define XE_MII_RDD	0x20

/*
 * MII command (etc) bit strings.
 */
#define XE_MII_STARTDELIM	0x01
#define XE_MII_READOP		0x02
#define XE_MII_WRITEOP		0x01
#define XE_MII_TURNAROUND	0x02

/*
 * PHY registers.
 */
#define PHY_BMCR		0x00	/* Basic Mode Control Register */
#define PHY_BMSR		0x01	/* Basic Mode Status Register */
#define PHY_ANAR		0x04	/* Auto-Negotiation Advertisment Register */
#define PHY_LPAR		0x05	/* Auto-Negotiation Link Partner Ability Register */
#define PHY_ANER		0x06	/* Auto-Negotiation Expansion Register */

#define PHY_BMCR_RESET		0x8000	/* Soft reset PHY.  Self-clearing */
#define PHY_BMCR_LOOPBK		0x4000	/* Enable loopback */
#define PHY_BMCR_SPEEDSEL	0x2000	/* 1=100Mbps, 0=10Mbps */
#define PHY_BMCR_AUTONEGENBL	0x1000	/* Auto-negotiation enabled */
#define PHY_BMCR_ISOLATE	0x0400	/* Isolate ML6692 from MII */
#define PHY_BMCR_AUTONEGRSTR	0x0200	/* Restart auto-negotiation.  Self-clearing */
#define PHY_BMCR_DUPLEX		0x0100	/* Full duplex operation */
#define PHY_BMCR_COLLTEST	0x0080	/* Enable collision test */

#define PHY_BMSR_100BT4		0x8000	/* 100Base-T4 capable */
#define PHY_BMSR_100BTXFULL	0x4000	/* 100Base-TX full duplex capable */
#define PHY_BMSR_100BTXHALF	0x2000	/* 100Base-TX half duplex capable */
#define PHY_BMSR_10BTFULL	0x1000	/* 10Base-T full duplex capable */
#define PHY_BMSR_10BTHALF	0x0800	/* 10Base-T half duplex capable */
#define PHY_BMSR_AUTONEGCOMP	0x0020	/* Auto-negotiation complete */
#define PHY_BMSR_CANAUTONEG	0x0008	/* Auto-negotiation supported */
#define PHY_BMSR_LINKSTAT	0x0004	/* Link is up */
#define PHY_BMSR_EXTENDED	0x0001	/* Extended register capabilities */

#define PHY_ANAR_NEXTPAGE	0x8000	/* Additional link code word pages */
#define PHY_ANAR_TLRFLT		0x2000	/* Remote wire fault detected */
#define PHY_ANAR_100BT4		0x0200	/* 100Base-T4 capable */
#define PHY_ANAR_100BTXFULL	0x0100	/* 100Base-TX full duplex capable */
#define PHY_ANAR_100BTXHALF	0x0080	/* 100Base-TX half duplex capable */
#define PHY_ANAR_10BTFULL	0x0040	/* 10Base-T full duplex capable */
#define PHY_ANAR_10BTHALF	0x0020	/* 10Base-T half duplex capable */
#define PHY_ANAR_PROTO4		0x0010	/* Protocol selection (00001 = 802.3) */
#define PHY_ANAR_PROTO3		0x0008
#define PHY_ANAR_PROTO2		0x0004
#define PHY_ANAR_PROTO1		0x0002
#define PHY_ANAR_PROTO0		0x0001

#define PHY_LPAR_NEXTPAGE	0x8000	/* Additional link code word pages */
#define PHY_LPAR_LPACK		0x4000	/* Link partner acknowledged receipt */
#define PHY_LPAR_TLRFLT		0x2000	/* Remote wire fault detected */
#define PHY_LPAR_100BT4		0x0200	/* 100Base-T4 capable */
#define PHY_LPAR_100BTXFULL	0x0100	/* 100Base-TX full duplex capable */
#define PHY_LPAR_100BTXHALF	0x0080	/* 100Base-TX half duplex capable */
#define PHY_LPAR_10BTFULL	0x0040	/* 10Base-T full duplex capable */
#define PHY_LPAR_10BTHALF	0x0020	/* 10Base-T half duplex capable */
#define PHY_LPAR_PROTO4		0x0010	/* Protocol selection (00001 = 802.3) */
#define PHY_LPAR_PROTO3		0x0008
#define PHY_LPAR_PROTO2		0x0004
#define PHY_LPAR_PROTO1		0x0002
#define PHY_LPAR_PROTO0		0x0001

#define PHY_ANER_MLFAULT	0x0010	/* More than one link is up! */
#define PHY_ANER_LPNPABLE	0x0008	/* Link partner supports next page */
#define PHY_ANER_NPABLE		0x0004	/* Local port supports next page */
#define PHY_ANER_PAGERX		0x0002	/* Page received */
#define PHY_ANER_LPAUTONEG	0x0001	/* Link partner can auto-negotiate */


#endif /* NXE > 0 */
