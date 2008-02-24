/*-
 * Copyright (c) 2006 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD: src/sys/arm/xscale/ixp425/if_npereg.h,v 1.1 2006/11/19 23:55:23 sam Exp $
 */

/*
 * Copyright (c) 2001-2005, Intel Corporation.
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
 * 3. Neither the name of the Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef ARM_XSCALE_IF_NPEREG_H
#define	ARM_XSCALE_IF_NPEREG_H

/*
 * NPE/NPE tx/rx descriptor format.  This is just the area
 * shared with ucode running in the NPE; the driver-specific
 * state is defined in the driver.  The shared area must be
 * cacheline-aligned.  We allocate NPE_MAXSEG "descriptors"
 * per buffer; this allows us to do minimal s/g.  The number
 * of descriptors can be expanded but doing so uses memory
 * so should be done with care.
 *
 * The driver sets up buffers in uncached memory.
 */
#define	NPE_MAXSEG	3		/* empirically selected */

struct npehwbuf {
	struct {			/* NPE shared area, cacheline aligned */
		uint32_t next;		/* phys addr of next segment */
		uint32_t len;		/* buffer/segment length (bytes) */
		uint32_t data;		/* phys addr of data segment */
		uint32_t pad[5];	/* pad to cacheline */
	} ix_ne[NPE_MAXSEG];
};

/* NPE ID's */
#define	NPE_A		0
#define	NPE_B		1
#define	NPE_C		2
#define	NPE_MAX		(NPE_C+1)

#define NPE_PORTS_MAX		2	/* logical ports */
#define NPE_FRAME_SIZE_DEFAULT	1536
#define NPE_FRAME_SIZE_MAX	(65536-64)
#define NPE_FRAME_SIZE_MIN	64

/*
 * Queue Manager-related definitions.
 *
 * These define the layout of 32-bit Q entries passed
 * between the host cpu and the NPE's.
 */
#define	NPE_QM_Q_NPE(e)		(((e)>>0)&0x3)		/* NPE ID */
#define	NPE_QM_Q_PORT(e)	(((e)>>3)&0x1)		/* Port ID */
#define	NPE_QM_Q_PRIO(e)	(((e)>>0)&0x3)		/* 802.1d priority */
#define	NPE_QM_Q_ADDR(e)	((e)&0xfffffffe0)	/* phys address */

/*
 * Host->NPE requests written to the shared mailbox.
 * The NPE writes the same value back as an ACK.
 */
#define	NPE_GETSTATUS		0x00	/* get firmware revision */
#define	NPE_SETPORTADDRESS	0x01	/* set port id and mac address */
#define	NPE_GETMACADDRDB	0x02	/* upload filter database */
#define	NPE_SETMACADDRDB	0x03	/* download filter database */
#define	NPE_GETSTATS		0x04	/* get statistics */
#define	NPE_RESETSTATS		0x05	/* reset stats + return result */
#define	NPE_SETMAXFRAME		0x06	/* configure max tx/rx frame lengths */
#define	NPE_SETRXTAGMODE	0x07	/* configure VLAN rx operating mode */
#define	NPE_SETDEFRXVID		0x08	/* set def VLAN tag + traffic class */
#define	NPE_SETRXQOSENTRY	0x0b	/* map user pri -> QoS class+rx qid */
#define	NPE_SETFIREWALLMODE	0x0e	/* config firewall services */
#define	NPE_SETLOOPBACK		0x12	/* enable/disable loopback */
/* ... XXX more */

#define	NPE_MAC_MSGID_SHL	24
#define	NPE_MAC_PORTID_SHL	16

/*
 * MAC register definitions; see section
 * 15.2 of the Intel Developers Manual.
 */
#define	NPE_MAC_TX_CNTRL1	0x000
#define	NPE_MAC_TX_CNTRL2	0x004
#define	NPE_MAC_RX_CNTRL1	0x010
#define	NPE_MAC_RX_CNTRL2	0x014
#define	NPE_MAC_RANDOM_SEED	0x020
#define	NPE_MAC_THRESH_P_EMPTY	0x030
#define	NPE_MAC_THRESH_P_FULL	0x038
#define	NPE_MAC_BUF_SIZE_TX	0x040
#define	NPE_MAC_TX_DEFER	0x050
#define	NPE_MAC_RX_DEFER	0x054
#define	NPE_MAC_TX_TWO_DEFER_1	0x060
#define	NPE_MAC_TX_TWO_DEFER_2	0x064
#define	NPE_MAC_SLOT_TIME	0x070
#define	NPE_MAC_MDIO_CMD_1	0x080
#define	NPE_MAC_MDIO_CMD_2	0x084
#define	NPE_MAC_MDIO_CMD_3	0x088
#define	NPE_MAC_MDIO_CMD_4	0x08c
#define	NPE_MAC_MDIO_STS_1	0x090
#define	NPE_MAC_MDIO_STS_2	0x094
#define	NPE_MAC_MDIO_STS_3	0x098
#define	NPE_MAC_MDIO_STS_4	0x09c
#define	NPE_MAC_ADDR_MASK_1	0x0A0
#define	NPE_MAC_ADDR_MASK_2	0x0A4
#define	NPE_MAC_ADDR_MASK_3	0x0A8
#define	NPE_MAC_ADDR_MASK_4	0x0AC
#define	NPE_MAC_ADDR_MASK_5	0x0B0
#define	NPE_MAC_ADDR_MASK_6	0x0B4
#define	NPE_MAC_ADDR_1		0x0C0
#define	NPE_MAC_ADDR_2		0x0C4
#define	NPE_MAC_ADDR_3		0x0C8
#define	NPE_MAC_ADDR_4		0x0CC
#define	NPE_MAC_ADDR_5		0x0D0
#define	NPE_MAC_ADDR_6		0x0D4
#define	NPE_MAC_INT_CLK_THRESH	0x0E0
#define	NPE_MAC_UNI_ADDR_1	0x0F0
#define	NPE_MAC_UNI_ADDR_2	0x0F4
#define	NPE_MAC_UNI_ADDR_3	0x0F8
#define	NPE_MAC_UNI_ADDR_4	0x0FC
#define	NPE_MAC_UNI_ADDR_5	0x100
#define	NPE_MAC_UNI_ADDR_6	0x104
#define	NPE_MAC_CORE_CNTRL	0x1FC

#define	NPE_MAC_ADDR_MASK(i)    (NPE_MAC_ADDR_MASK_1 + ((i)<<2))
#define	NPE_MAC_ADDR(i)     	(NPE_MAC_ADDR_1 + ((i)<<2))
#define	NPE_MAC_UNI_ADDR(i)    	(NPE_MAC_UNI_ADDR_1 + ((i)<<2))

/*
 * Bit definitions
 */

/* TX Control Register 1*/
#define	NPE_TX_CNTRL1_TX_EN		0x01	/* enable TX engine */
#define	NPE_TX_CNTRL1_DUPLEX		0x02	/* select half duplex */
#define	NPE_TX_CNTRL1_RETRY		0x04	/* auto-retry on collision */
#define	NPE_TX_CNTRL1_PAD_EN		0x08	/* pad frames <64 bytes */
#define	NPE_TX_CNTRL1_FCS_EN		0x10	/* append FCS */
#define	NPE_TX_CNTRL1_2DEFER		0x20	/* select 2-part deferral */
#define	NPE_TX_CNTRL1_RMII		0x40

/* TX Control Register 2 */
#define	NPE_TX_CNTRL2_RETRIES_MASK	0xf	/* max retry count */

/* RX Control Register 1 */
#define	NPE_RX_CNTRL1_RX_EN		0x01	/* enable RX engine */
#define	NPE_RX_CNTRL1_PADSTRIP_EN	0x02	/* strip frame padding */
#define	NPE_RX_CNTRL1_CRC_EN		0x04	/* include CRC in RX frame */
#define	NPE_RX_CNTRL1_PAUSE_EN		0x08	/* detect Pause frames */
#define	NPE_RX_CNTRL1_LOOP_EN		0x10	/* loopback tx/rx */
#define	NPE_RX_CNTRL1_ADDR_FLTR_EN	0x20	/* enable address filtering */
#define	NPE_RX_CNTRL1_RX_RUNT_EN	0x40	/* enable RX of runt frames */
#define	NPE_RX_CNTRL1_BCAST_DIS		0x80	/* discard broadcast frames */

/* RX Control Register 2 */
#define	NPE_RX_CNTRL2_DEFER_EN	0x01

/* Core Control Register */
#define	NPE_CORE_RESET			0x01	/* MAC reset state */
#define	NPE_CORE_RX_FIFO_FLUSH		0x02	/* flush RX FIFO */
#define	NPE_CORE_TX_FIFO_FLUSH		0x04	/* flush TX FIFO */
#define	NPE_CORE_SEND_JAM		0x08	/* send JAM on packet RX */
#define	NPE_CORE_MDC_EN			0x10	/* IXP42X drives MDC clock */

/*
 * Stat block returned by NPE with NPE_GETSTATS msg.
 */
struct npestats {
	uint32_t dot3StatsAlignmentErrors;
	uint32_t dot3StatsFCSErrors;
	uint32_t dot3StatsInternalMacReceiveErrors;
	uint32_t RxOverrunDiscards;
	uint32_t RxLearnedEntryDiscards;
	uint32_t RxLargeFramesDiscards;
	uint32_t RxSTPBlockedDiscards;
	uint32_t RxVLANTypeFilterDiscards;
	uint32_t RxVLANIdFilterDiscards;
	uint32_t RxInvalidSourceDiscards;
	uint32_t RxBlackListDiscards;
	uint32_t RxWhiteListDiscards;
	uint32_t RxUnderflowEntryDiscards;
	uint32_t dot3StatsSingleCollisionFrames;
	uint32_t dot3StatsMultipleCollisionFrames;
	uint32_t dot3StatsDeferredTransmissions;
	uint32_t dot3StatsLateCollisions;
	uint32_t dot3StatsExcessiveCollisions;
	uint32_t dot3StatsInternalMacTransmitErrors;
	uint32_t dot3StatsCarrierSenseErrors;
	uint32_t TxLargeFrameDiscards;
	uint32_t TxVLANIdFilterDiscards;
};

/*
 * Default values
 */
#define NPE_MAC_INT_CLK_THRESH_DEFAULT  0x1

#define NPE_MAC_RESET_DELAY    1

/* This value applies to RMII */
#define NPE_MAC_SLOT_TIME_RMII_DEFAULT  0xFF

/*
 * MII definitions - these have been verified against the LXT971 and LXT972 PHYs
 */
#define	NPE_MII_REG_SHL		16
#define	NPE_MII_ADDR_SHL	21

/* NB: shorthands for mii bus mdio routines */
#define	NPE_MAC_MDIO_CMD	NPE_MAC_MDIO_CMD_1
#define	NPE_MAC_MDIO_STS	NPE_MAC_MDIO_STS_1
 
#define NPE_MII_GO                  (1<<31)
#define NPE_MII_WRITE               (1<<26)
#define NPE_MII_TIMEOUT_10TH_SECS        5    
#define NPE_MII_10TH_SEC_IN_MILLIS     100
#define NPE_MII_READ_FAIL           (1<<31)
 
#define NPE_MII_PHY_DEF_DELAY	300	/* max delay before link up, etc. */
#define NPE_MII_PHY_NO_DELAY	0x0	/* do not delay */
#define NPE_MII_PHY_NULL	0xff	/* PHY is not present */
#define NPE_MII_PHY_DEF_ADDR	0x0	/* default PHY's logical address */

/* Register definition */  
#define NPE_MII_CTRL_REG	0x0	/* Control Register */
#define NPE_MII_STAT_REG	0x1	/* Status Register */
#define NPE_MII_PHY_ID1_REG	0x2	/* PHY identifier 1 Register */
#define NPE_MII_PHY_ID2_REG	0x3	/* PHY identifier 2 Register */
#define NPE_MII_AN_ADS_REG	0x4	/* Auto-Negotiation 	  */
					/* Advertisement Register */
#define NPE_MII_AN_PRTN_REG	0x5	/* Auto-Negotiation 	    */
					/* partner ability Register */
#define NPE_MII_AN_EXP_REG	0x6	/* Auto-Negotiation   */
					/* Expansion Register */
#define NPE_MII_AN_NEXT_REG	0x7	/* Auto-Negotiation 	       */
					/* next-page transmit Register */
#endif /* ARM_XSCALE_IF_NPEREG_H */
