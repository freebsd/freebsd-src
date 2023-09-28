/*-
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#ifndef	__DWC1000_DMA_H__
#define	 __DWC1000_DMA_H__

/* TX descriptors - TDESC0 is almost unified */
#define	TDESC0_OWN		(1U << 31)
#define	TDESC0_IHE		(1U << 16)	/* IP Header Error */
#define	TDESC0_ES		(1U << 15)	/* Error Summary */
#define	TDESC0_JT		(1U << 14)	/* Jabber Timeout */
#define	TDESC0_FF		(1U << 13)	/* Frame Flushed */
#define	TDESC0_PCE		(1U << 12)	/* Payload Checksum Error */
#define	TDESC0_LOC		(1U << 11)	/* Loss of Carrier */
#define	TDESC0_NC		(1U << 10)	/* No Carrier */
#define	TDESC0_LC		(1U <<  9)	/* Late Collision */
#define	TDESC0_EC		(1U <<  8)	/* Excessive Collision */
#define	TDESC0_VF		(1U <<  7)	/* VLAN Frame */
#define	TDESC0_CC_MASK		0xf
#define	TDESC0_CC_SHIFT		3		/* Collision Count */
#define	TDESC0_ED		(1U <<  2)	/* Excessive Deferral */
#define	TDESC0_UF		(1U <<  1)	/* Underflow Error */
#define	TDESC0_DB		(1U <<  0)	/* Deferred Bit */
/* TX descriptors - TDESC0 extended format only */
#define	ETDESC0_IC		(1U << 30)	/* Interrupt on Completion */
#define	ETDESC0_LS		(1U << 29)	/* Last Segment */
#define	ETDESC0_FS		(1U << 28)	/* First Segment */
#define	ETDESC0_DC		(1U << 27)	/* Disable CRC */
#define	ETDESC0_DP		(1U << 26)	/* Disable Padding */
#define	ETDESC0_CIC_NONE	(0U << 22)	/* Checksum Insertion Control */
#define	ETDESC0_CIC_HDR		(1U << 22)
#define	ETDESC0_CIC_SEG 	(2U << 22)
#define	ETDESC0_CIC_FULL	(3U << 22)
#define	ETDESC0_TER		(1U << 21)	/* Transmit End of Ring */
#define	ETDESC0_TCH		(1U << 20)	/* Second Address Chained */

/* TX descriptors - TDESC1 normal format */
#define	NTDESC1_IC		(1U << 31)	/* Interrupt on Completion */
#define	NTDESC1_LS		(1U << 30)	/* Last Segment */
#define	NTDESC1_FS		(1U << 29)	/* First Segment */
#define	NTDESC1_CIC_NONE	(0U << 27)	/* Checksum Insertion Control */
#define	NTDESC1_CIC_HDR		(1U << 27)
#define	NTDESC1_CIC_SEG 	(2U << 27)
#define	NTDESC1_CIC_FULL	(3U << 27)
#define	NTDESC1_DC		(1U << 26)	/* Disable CRC */
#define	NTDESC1_TER		(1U << 25)	/* Transmit End of Ring */
#define	NTDESC1_TCH		(1U << 24)	/* Second Address Chained */
/* TX descriptors - TDESC1 extended format */
#define	ETDESC1_DP		(1U << 23)	/* Disable Padding */
#define	ETDESC1_TBS2_MASK	0x7ff
#define	ETDESC1_TBS2_SHIFT	11		/* Receive Buffer 2 Size */
#define	ETDESC1_TBS1_MASK	0x7ff
#define	ETDESC1_TBS1_SHIFT	0		/* Receive Buffer 1 Size */

/* RX descriptor - RDESC0 is unified */
#define	RDESC0_OWN		(1U << 31)
#define	RDESC0_AFM		(1U << 30)	/* Dest. Address Filter Fail */
#define	RDESC0_FL_MASK		0x3fff
#define	RDESC0_FL_SHIFT		16		/* Frame Length */
#define	RDESC0_ES		(1U << 15)	/* Error Summary */
#define	RDESC0_DE		(1U << 14)	/* Descriptor Error */
#define	RDESC0_SAF		(1U << 13)	/* Source Address Filter Fail */
#define	RDESC0_LE		(1U << 12)	/* Length Error */
#define	RDESC0_OE		(1U << 11)	/* Overflow Error */
#define	RDESC0_VLAN		(1U << 10)	/* VLAN Tag */
#define	RDESC0_FS		(1U <<  9)	/* First Descriptor */
#define	RDESC0_LS		(1U <<  8)	/* Last Descriptor */
#define	RDESC0_ICE		(1U <<  7)	/* IPC Checksum Error */
#define	RDESC0_LC		(1U <<  6)	/* Late Collision */
#define	RDESC0_FT		(1U <<  5)	/* Frame Type */
#define	RDESC0_RWT		(1U <<  4)	/* Receive Watchdog Timeout */
#define	RDESC0_RE		(1U <<  3)	/* Receive Error */
#define	RDESC0_DBE		(1U <<  2)	/* Dribble Bit Error */
#define	RDESC0_CE		(1U <<  1)	/* CRC Error */
#define	RDESC0_PCE		(1U <<  0)	/* Payload Checksum Error */
#define	RDESC0_RXMA		(1U <<  0)	/* Rx MAC Address */

/* RX descriptors - RDESC1 normal format */
#define	NRDESC1_DIC		(1U << 31)	/* Disable Intr on Completion */
#define	NRDESC1_RER		(1U << 25)	/* Receive End of Ring */
#define	NRDESC1_RCH		(1U << 24)	/* Second Address Chained */
#define	NRDESC1_RBS2_MASK	0x7ff
#define	NRDESC1_RBS2_SHIFT	11		/* Receive Buffer 2 Size */
#define	NRDESC1_RBS1_MASK	0x7ff
#define	NRDESC1_RBS1_SHIFT	0		/* Receive Buffer 1 Size */

/* RX descriptors - RDESC1 enhanced format */
#define	ERDESC1_DIC		(1U << 31)	/* Disable Intr on Completion */
#define	ERDESC1_RBS2_MASK	0x7ffff
#define	ERDESC1_RBS2_SHIFT	16		/* Receive Buffer 2 Size */
#define	ERDESC1_RER		(1U << 15)	/* Receive End of Ring */
#define	ERDESC1_RCH		(1U << 14)	/* Second Address Chained */
#define	ERDESC1_RBS1_MASK	0x7ffff
#define	ERDESC1_RBS1_SHIFT	0		/* Receive Buffer 1 Size */

/*
 * A hardware buffer descriptor.  Rx and Tx buffers have the same descriptor
 * layout, but the bits in the fields have different meanings.
 */
struct dwc_hwdesc
{
	uint32_t desc0;
	uint32_t desc1;
	uint32_t addr1;		/* ptr to first buffer data */
	uint32_t addr2;		/* ptr to next descriptor / second buffer data*/
};

/*
 * The hardware imposes alignment restrictions on various objects involved in
 * DMA transfers.  These values are expressed in bytes (not bits).
 */
#define	DWC_DESC_RING_ALIGN	2048

int dma1000_init(struct dwc_softc *sc);
void dma1000_free(struct dwc_softc *sc);
void dma1000_start(struct dwc_softc *sc);
void dma1000_stop(struct dwc_softc *sc);
int dma1000_setup_txbuf(struct dwc_softc *sc, int idx, struct mbuf **mp);
void dma1000_txfinish_locked(struct dwc_softc *sc);
void dma1000_rxfinish_locked(struct dwc_softc *sc);
void dma1000_txstart(struct dwc_softc *sc);

#endif	/* __DWC1000_DMA_H__ */
