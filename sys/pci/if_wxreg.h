/*
 * Copyright (c) 1999, Traakan Software
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * $FreeBSD: src/sys/pci/if_wxreg.h,v 1.3.2.1 2000/06/16 22:58:03 mjacob Exp $
 */

#define WX_VENDOR_INTEL		0x8086
#define WX_PRODUCT_82452	0x1000
#define	WX_MMBA			0x10
#define	MWI			0x10	/* Memory Write Invalidate */
#define	WX_CACHELINE_SIZE	0x20

/*
 * Information about this chipset gathered from a released Intel Linux driver,
 * which was clearly a port of an NT driver. 
 */

/*
 * Various Descriptor Structures.
 * These are all in little endian format (for now).
 */

typedef struct {
	u_int32_t	lowpart;
	u_int32_t	highpart;
} wxpa_t, wxrp_t;

/*
 * Receive Descriptor.
 * The base address of a receive descriptor ring must be on a 4KB boundary,
 * and they must be allocated in multiples of 8.
 */
typedef struct {
	wxpa_t		address;	/* physical address of buffer */
	u_int16_t	length;
	u_int16_t	csum;
	u_int8_t	status;
	u_int8_t	errors;
	u_int16_t	special;
} wxrd_t;

#define	RDSTAT_DD	0x1		/* descriptor done */
#define RDSTAT_EOP	0x2		/* end of packet */
#define	RDSTAT_RSVD	0x74		/* reserved bits */

#define	RDERR_CRC	0x1		/* CRC Error */
#define	RDERR_SE	0x2		/* Symbol Error */
#define	RDERR_SEQ	0x4		/* Sequence Error */

/*
 * Transmit Descriptor
 * The base address of a transmit descriptor ring must be on a 4KB boundary,
 * and they must be allocated in multiples of 8.
 */
typedef struct {
	wxpa_t		address;
	u_int16_t	length;
	u_int8_t	cso;		/* checksum offset */
	u_int8_t	cmd;		/* cmd */
	u_int8_t	status;		/* status */
	u_int8_t	css;		/* checksum start */
	u_int16_t	special;
} wxtd_t;

#define	TXCMD_EOP	0x1		/* last packet */
#define	TXCMD_IFCS	0x2		/* insert FCS */
#define	TXCMD_IC	0x4		/* insert checksum */
#define	TXCMD_RS	0x8		/* report status */
#define	TXCMD_RPS	0x10		/* report packet sent */
#define	TXCMD_SM	0x20		/* symbol mode */
#define	TXCMD_IDE	0x80		/* interrupt delay enable */

#define	TXSTS_DD	0x1		/* descriptor done */
#define	TXSTS_EC	0x2		/* excess collisions */
#define	TXSTS_LC	0x4		/* late collision */

/*
 * This device can only be accessed via memory space.
 */
/*
 * Register access via offsets.
 */

#define	WXREG_DCR		0x00000000
#define	WXREG_DSR		0x00000008
#define	WXREG_EECDR		0x00000010
#define	WXREG_FCAL		0x00000028
#define	WXREG_FCAH		0x0000002C
#define	WXREG_FCT		0x00000030
#define	WXREG_VET		0x00000038
#define	WXREG_RAL_BASE		0x00000040
#define	WXREG_RAL_LO(x)		(WXREG_RAL_BASE + ((x) << 3))
#define	WXREG_RAL_HI(x)		(WXREG_RAL_LO(x) + 4)
#define	WXREG_ICR		0x000000c0
#define	WXREG_ICS		0x000000c8
#define	WXREG_IMASK		0x000000d0
#define	WXREG_IMCLR		0x000000d8
#define	WXREG_RCTL		0x00000100
#define	WXREG_RDTR0		0x00000108
#define	WXREG_RDBA0_LO		0x00000110
#define	WXREG_RDBA0_HI		0x00000114
#define	WXREG_RDLEN0		0x00000118
#define	WXREG_RDH0		0x00000120
#define	WXREG_RDT0		0x00000128
#define	WXREG_RDTR1		0x00000130
#define	WXREG_RDBA1_LO		0x00000138
#define	WXREG_RDBA1_HI		0x0000013C
#define	WXREG_RDLEN1		0x00000140
#define	WXREG_RDH1		0x00000148
#define	WXREG_RDT1		0x00000150
#define	WXREG_FLOW_RCV_HI	0x00000160
#define	WXREG_FLOW_RCV_LO	0x00000168
#define	WXREG_FLOW_XTIMER	0x00000170
#define	WXREG_XMIT_CFGW		0x00000178
#define	WXREG_RECV_CFGW		0x00000180
#define	WXREG_MTA		0x00000200
#define	WXREG_TCTL		0x00000400
#define	WXREG_TQSA_LO		0x00000408
#define	WXREG_TQSA_HI		0x0000040C
#define	WXREG_TIPG		0x00000410
#define	WXREG_TQC		0x00000418
#define	WXREG_TDBA_LO		0x00000420
#define	WXREG_TDBA_HI		0x00000424
#define	WXREG_TDLEN		0x00000428
#define	WXREG_TDH		0x00000430
#define	WXREG_TDT		0x00000438
#define	WXREG_TIDV		0x00000440
#define	WXREG_VFTA		0x00000600

#define	WX_RAL_TAB_SIZE		16
#define	WX_RAL_AV		0x80000000

#define	WX_MC_TAB_SIZE		128
#define	WX_VLAN_TAB_SIZE	128

/*
 * Device Control Register Defines
 */
#define	WXDCR_FD	0x1		/* full duplex */
#define	WXDCR_BEM	0x2		/* big endian mode */
#define	WXDCR_FAIR	0x4		/* 1->Fairness, 0->Receive Priority */
#define	WXDCR_LRST	0x8		/* Link Reset */
#define	WXDCR_SLU	0x40		/* Set Link Up */
#define	WXDCR_ILOS	0x80		/* Invert Loss-of-Signal */

/*
 * General purpose I/O pins
 *
 * Pin 0 is for the LED.
 *
 * Pin 1 is to detect loss of signal (LOS)- if it is set, we've lost signal.
 */
#define	WXDCR_SWDPINS_SHIFT	18
#define	WXDCR_SWDPINS_MASK	0xf
#define		WXDCR_SWDPIN0	(1 << 18)
#define		WXDCR_SWDPIN1	(1 << 19)
#define		WXDCR_SWDPIN2	(1 << 20)
#define		WXDCR_SWDPIN3	(1 << 21)
#define	WXDCR_SWDPIO_SHIFT	22
#define	WXDCR_SWDPIO_MASK	0xf
#define		WXDCR_SWDPIO0	(1 << 22)
#define		WXDCR_SWDPIO1	(1 << 23)
#define		WXDCR_SWDPIO2	(1 << 24)
#define		WXDCR_SWDPIO3	(1 << 25)


#define	WXDCR_RST	0x04000000	/* Device Reset (self clearing) */
#define	WXDCR_RFCE	0x08000000	/* Receive Flow Control Enable */
#define	WXDCR_TFCE	0x10000000	/* Transmit Flow Control Enable */
#define	WXDCR_RTE	0x20000000	/* Routing Tag Enable */
#define	WXDCR_VME	0x40000000	/* VLAN Mode Enable */

/*
 * Device Status Register Defines
 */
#define	WXDSR_FD	0x1		/* full duplex */
#define	WXDSR_LU	0x2		/* link up */
#define	WXDSR_TXCLK	0x4		/* transmit clock running */
#define	WXDSR_RBCLK	0x8		/* receive clock running */
#define	WXDSR_TXOFF	0x10		/* transmit paused */

/*
 * EEPROM Register Defines
 */
#define	WXEECD_SK	0x1		/* enable clock */
#define	WXEECD_CS	0x2		/* chip select */
#define	WXEECD_DI	0x4		/* data input */
#define	WXEECD_DO	0x8		/* data output */

#define	EEPROM_READ_OPCODE	0x6

/*
 * Constant Flow Control Frame MAC Address and Type values.
 */
#define	FC_FRM_CONST_LO	0x00C28001
#define	FC_FRM_CONST_HI	0x0100
#define	FC_TYP_CONST	0x8808

/*
 * Bits pertinent for the Receive Address register pairs. The low address
 * is the low 32 bits of a 48 bit MAC address. The high address contains
 * bits 32-47 of the 48 bit MAC address. The top bit in the high address
 * is a 'valid' bit.
 */
#define	WXRAH_RDR1	0x40000000	/* second receive descriptor ring */
#define	WXRAH_VALID	0x80000000

/*
 * Interrupt Cause Bits
 */
#define	WXISR_TXDW	0x1		/* transmit descriptor written back */
#define	WXISR_TXQE	0x2		/* transmit queue empty */
#define	WXISR_LSC	0x4		/* link status change */
#define	WXISR_RXSEQ	0x8		/* receive sequence error */
#define	WXISR_RXDMT0	0x10		/* receiver ring 0 getting empty */
#define	WXISR_RXO	0x40		/* receiver overrun */
#define	WXISR_RXT0	0x80		/* ring 0 receiver timer interrupt */
#define	WXISR_PCIE	0x200		/* ?? Probably PCI interface error... */

#define	WXIENABLE_DEFAULT	\
	 (WXISR_RXO|WXISR_RXT0|WXISR_RXDMT0|WXISR_RXSEQ|WXISR_LSC|WXISR_PCIE)

#define	WXDISABLE	0xffffffff

/*
 * Receive Control Register bits.
 */

#define	WXRCTL_RST	0x1		/* receiver reset */
#define	WXRCTL_EN	0x2		/* receiver enable */
#define	WXRCTL_SBP	0x4		/* store bad packets */
#define	WXRCTL_UPE	0x8		/* unicast promiscuos mode */
#define	WXRCTL_MPE	0x10		/* multicast promiscuous mode */
#define	WXRCTL_LPE	0x20		/* large packet enable */
#define	WXRCTL_BAM	0x8000		/* broadcast accept mode */

#define	WXRCTL_2KRBUF	(0 << 16)	/* 2-Kbyte Receive Buffers */
#define	WXRCTL_1KRBUF	(1 << 16)	/* 1-Kbyte Receive Buffers */
#define	WXRCTL_512BRBUF	(2 << 16)	/* 512 Byte Receive Buffers */
#define	WXRCTL_256BRBUF	(3 << 16)	/* 256 Byte Receive Buffers */


/*
 * Receive Delay Timer Register bits.
 */
#define	WXRDTR_FPD	0x80000000	/* flush partial descriptor */

/*
 * Transmit Configuration Word defines
 */
#define	WXTXCW_FD	0x00000020	/* Full Duplex */
#define	WXTXCW_PMASK	0x00000180	/* pause mask */
#define	WXTXCW_ANE	0x80000000	/* AutoNegotiate */
#define	WXTXCW_DEFAULT	0x800001A0

/*
 * Transmit Control Register defines.
 */
#define	WXTCTL_RST	0x1		/* transmitter reset */
#define	WXTCTL_EN	0x2		/* transmitter enable */
#define	WXTCTL_PSP	0x8		/* pad short packets */
#define	WXTCTL_CT(x)	(((x) & 0xff) << 4)	/* 4:11 - Collision Threshold */
#define	WXTCTL_COLD(x)	(((x) & 0x3ff) << 12)	/* 12:21 - Collision Distance */
#define	WXTCTL_SWXOFF	(1 << 22)	/* Software XOFF */

#define	WX_COLLISION_THRESHOLD	15
#define	WX_FDX_COLLISION_DX	64
#define	WX_HDX_COLLISION_DX	512

/*
 * Receive Configuration Word defines
 */

#define	WXRXCW_CWMASK	0x0000ffff
#define	WXRXCW_NC	0x04000000
#define	WXRXCW_IV	0x08000000
#define	WXRXCW_CC	0x10000000
#define	WXRXCW_C	0x20000000
#define	WXRXCW_SYNCH	0x40000000
#define	WXRXCW_ANC	0x80000000

/*
 * Receive Configuration Word defines
 */

#define	WXRXCW_CWMASK	0x0000ffff
#define	WXRXCW_NC	0x04000000
#define	WXRXCW_IV	0x08000000
#define	WXRXCW_CC	0x10000000
#define	WXRXCW_C	0x20000000
#define	WXRXCW_SYNCH	0x40000000
#define	WXRXCW_ANC	0x80000000

/*
 * Miscellaneous
 */
#define	WX_EEPROM_MAC_OFF	0

/*
 * Offset for Initialization Control Word #1
 */
#define	WX_EEPROM_CTLR1_OFF	0xA
#define	WX_EEPROM_CTLR1_FD	(1 << 10)
#define	WX_EEPROM_CTLR1_SWDPIO_SHIFT	5
#define	WX_EEPROM_CTLR1_ILOS	(1 << 4)


#define	WX_XTIMER_DFLT		0x100
#define	WX_RCV_FLOW_HI_DFLT	0x8000
#define	WX_RCV_FLOW_LO_DFLT	0x4000

#define	WX_TIPG_DFLT		(10 | (2 << 10) | (10 << 20))

#define	WX_CRC_LENGTH		4


/*
 * Hardware cannot transmit less than 16 bytes. It also cannot
 * successfully receive less than 60 bytes.
 */
#define	WX_MIN_XPKT_SIZE	16
#define	WX_MIN_RPKT_SIZE	60
#define	WX_MAX_PKT_SIZE		1514
#define	WX_MAX_PKT_SIZE_JUMBO	9014
