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
 *	$Id: if_xereg.h,v 1.2 1999/01/24 22:15:30 root Exp $
 */

/*
 * Register definitions for Xircom CreditCard Ethernet adapters.  See if_xe.c
 * for details of supported hardware.  Adapted from Werner Koch's 'xirc2ps'
 * driver for Linux.
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
 * MII/PHY defines adapted from the xl driver.  These need cleaning up a
 * little if we end up using them.
 */
#define XE_MII_CLK	0x01
#define XE_MII_DIR	0x08
#define XE_MII_WRD	0x02
#define XE_MII_RDD	0x20
#define XE_MII_STARTDELIM	0x01
#define XE_MII_READOP		0x02
#define XE_MII_WRITEOP		0x01
#define XE_MII_TURNAROUND	0x02

#define XE_MII_SET(x)	XE_OUTB(XE_GPR2, (XE_INB(XE_GPR2) | 0x04) | (x))
#define XE_MII_CLR(x)	XE_OUTB(XE_GPR2, (XE_INB(XE_GPR2) | 0x04) & ~(x))

#define XL_PHY_GENCTL		0x00
#define XL_PHY_GENSTS		0x01
#define XL_PHY_VENID		0x02
#define XL_PHY_DEVID		0x03
#define XL_PHY_ANAR		0x04
#define XL_PHY_LPAR		0x05
#define XL_PHY_ANER		0x06

#define PHY_ANAR_NEXTPAGE	0x8000
#define PHY_ANAR_RSVD0		0x4000
#define PHY_ANAR_TLRFLT		0x2000
#define PHY_ANAR_RSVD1		0x1000
#define PHY_ANAR_RSVD2		0x0800
#define PHY_ANAR_RSVD3		0x0400
#define PHY_ANAR_100BT4		0x0200
#define PHY_ANAR_100BTXFULL	0x0100
#define PHY_ANAR_100BTXHALF	0x0080
#define PHY_ANAR_10BTFULL	0x0040
#define PHY_ANAR_10BTHALF	0x0020
#define PHY_ANAR_PROTO4		0x0010
#define PHY_ANAR_PROTO3		0x0008
#define PHY_ANAR_PROTO2		0x0004
#define PHY_ANAR_PROTO1		0x0002
#define PHY_ANAR_PROTO0		0x0001

/*
 * PHY BMCR Basic Mode Control Register
 */
#define PHY_BMCR			0x00
#define PHY_BMCR_RESET			0x8000
#define PHY_BMCR_LOOPBK			0x4000
#define PHY_BMCR_SPEEDSEL		0x2000
#define PHY_BMCR_AUTONEGENBL		0x1000
#define PHY_BMCR_RSVD0			0x0800	/* write as zero */
#define PHY_BMCR_ISOLATE		0x0400
#define PHY_BMCR_AUTONEGRSTR		0x0200
#define PHY_BMCR_DUPLEX			0x0100
#define PHY_BMCR_COLLTEST		0x0080
#define PHY_BMCR_RSVD1			0x0040	/* write as zero, don't care */
#define PHY_BMCR_RSVD2			0x0020	/* write as zero, don't care */
#define PHY_BMCR_RSVD3			0x0010	/* write as zero, don't care */
#define PHY_BMCR_RSVD4			0x0008	/* write as zero, don't care */
#define PHY_BMCR_RSVD5			0x0004	/* write as zero, don't care */
#define PHY_BMCR_RSVD6			0x0002	/* write as zero, don't care */
#define PHY_BMCR_RSVD7			0x0001	/* write as zero, don't care */

/* 
 * PHY, BMSR Basic Mode Status Register 
 */   
#define PHY_BMSR			0x01
#define PHY_BMSR_100BT4			0x8000
#define PHY_BMSR_100BTXFULL		0x4000
#define PHY_BMSR_100BTXHALF		0x2000
#define PHY_BMSR_10BTFULL		0x1000
#define PHY_BMSR_10BTHALF		0x0800
#define PHY_BMSR_RSVD1			0x0400	/* write as zero, don't care */
#define PHY_BMSR_RSVD2			0x0200	/* write as zero, don't care */
#define PHY_BMSR_RSVD3			0x0100	/* write as zero, don't care */
#define PHY_BMSR_RSVD4			0x0080	/* write as zero, don't care */
#define PHY_BMSR_MFPRESUP		0x0040
#define PHY_BMSR_AUTONEGCOMP		0x0020
#define PHY_BMSR_REMFAULT		0x0010
#define PHY_BMSR_CANAUTONEG		0x0008
#define PHY_BMSR_LINKSTAT		0x0004
#define PHY_BMSR_JABBER			0x0002
#define PHY_BMSR_EXTENDED		0x0001


#endif /* NXE > 0 */
