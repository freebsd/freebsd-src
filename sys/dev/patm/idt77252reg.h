/*-
 * Copyright (c) 2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 * Author: Hartmut Brandt <harti@freebsd.org>
 *
 * $FreeBSD$
 *
 * Register definitions for the IDT77252 chip.
 */

#define	PCI_VENDOR_IDT		0x111D
#define	PCI_DEVICE_IDT77252	3
#define	PCI_DEVICE_IDT77v252	4
#define	PCI_DEVICE_IDT77v222	5

#define	IDT_PCI_REG_MEMBASE	0x14

#define	IDT_NOR_D0	0x00	/* R/W Data register 0 */
#define	IDT_NOR_D1	0x04	/* R/W Data register 1 */
#define	IDT_NOR_D2	0x08	/* R/W Data register 2 */
#define	IDT_NOR_D3	0x0C	/* R/W Data register 3 */
#define	IDT_NOR_CMD	0x10	/* R/W Command */
#define	IDT_NOR_CFG	0x14	/* R/W Configuration */
#define	IDT_NOR_STAT	0x18	/* R/W Status */
#define	IDT_NOR_RSQB	0x1C	/* R/W Receive status queue base */
#define	IDT_NOR_RSQT	0x20	/* R   Receive status queue tail */
#define	IDT_NOR_RSQH	0x24	/* R/W Receive status queue tail */
#define	IDT_NOR_CDC	0x28	/* R/W Cell drop counter */
#define	IDT_NOR_VPEC	0x2C	/* R/W VPI/VCI Lookup error counter */
#define	IDT_NOR_ICC	0x30	/* R/W Invalid cell counter */
#define	IDT_NOR_RAWCT	0x34	/* R   Raw cell tail */
#define	IDT_NOR_TMR	0x38	/* R   Timer */
#define	IDT_NOR_TSTB	0x3C	/* R/W Transmit schedule table base */
#define	IDT_NOR_TSQB	0x40	/* R/W Transmit Status queue base */
#define	IDT_NOR_TSQT	0x44	/* R/W Transmit Status queue tail */
#define	IDT_NOR_TSQH	0x48	/* R/W Transmit Status queue head */
#define	IDT_NOR_GP	0x4C	/* R/W General purpose */
#define	IDT_NOR_VPM	0x50	/* R/W VPI/VCI mask */
#define	IDT_NOR_RXFD	0x54	/* R/W Receive FIFO descriptor */
#define	IDT_NOR_RXFT	0x58	/* R/W Receive FIFO tail */
#define	IDT_NOR_RXFH	0x5C	/* R/W Receive FIFO head */
#define	IDT_NOR_RAWHND	0x60	/* R/W Raw cell handle */
#define	IDT_NOR_RXSTAT	0x64	/* R   Receive connection state */
#define	IDT_NOR_ABRSTD	0x68	/* R/W ABR & VBR Schedule table descriptor */
#define	IDT_NOR_ABRRQ	0x6C	/* R/W ABR Ready queue pointer */
#define	IDT_NOR_VBRRQ	0x70	/* R/W VBR Ready queue pointer */
#define	IDT_NOR_RTBL	0x74	/* R/W Rate table descriptor */
#define	IDT_NOR_MXDFCT	0x78	/* R/W Maximum deficit counter */
#define	IDT_NOR_TXSTAT	0x7C	/* R/W Transmit connection state */
#define	IDT_NOR_TCMDQ	0x80	/*   W Transmit command queue */
#define	IDT_NOR_IRCP	0x84	/* R/W Inactive receive connection pointer */
#define	IDT_NOR_FBQP0	0x88	/* R/W Free buffer queue 0 pointer */
#define	IDT_NOR_FBQP1	0x8C	/* R/W Free buffer queue 1 pointer */
#define	IDT_NOR_FBQP2	0x90	/* R/W Free buffer queue 2 pointer */
#define	IDT_NOR_FBQP3	0x94	/* R/W Free buffer queue 3 pointer */
#define	IDT_NOR_FBQS0	0x98	/* R/W Free buffer queue 0 size */
#define	IDT_NOR_FBQS1	0x9C	/* R/W Free buffer queue 1 size */
#define	IDT_NOR_FBQS2	0xA0	/* R/W Free buffer queue 2 size */
#define	IDT_NOR_FBQS3	0xA4	/* R/W Free buffer queue 3 size */
#define	IDT_NOR_FBQWP0	0xA8	/* R/W Free buffer queue 0 write pointer */
#define	IDT_NOR_FBQWP1	0xAC	/* R/W Free buffer queue 1 write pointer */
#define	IDT_NOR_FBQWP2	0xB0	/* R/W Free buffer queue 2 write pointer */
#define	IDT_NOR_FBQWP3	0xB4	/* R/W Free buffer queue 3 write pointer */
#define	IDT_NOR_NOW	0xB8	/* R   Current transmit schedule table addr */
#define	IDT_NOR_DNOW	0xBC	/* R   Dynamic Now register */
#define	IDT_NOR_END	0xC0

/*
 * Command (IDT_NOR_CMD)
 */
#define	IDT_CMD_NOP	0x00000000	/* No operation */
#define	IDT_CMD_OPCL	0x20000000	/* Open/Close connection */
#define	IDT_CMD_WSRAM	0x40000000	/* Write SRAM */
#define	IDT_CMD_RSRAM	0x50000000	/* Read SRAM */
#define	IDT_CMD_WFBQ	0x60000000	/* Write free buffer queue */
#define	IDT_CMD_RUTIL	0x80000000	/* Read utility bus */
#define	IDT_CMD_WUTIL	0x90000000	/* Write utility bus */

#define	IDT_MKCMD_OPEN(VC)	(IDT_CMD_OPCL | (1 << 19) | ((V) << 4))
#define	IDT_MKCMD_CLOSE(VC)	(IDT_CMD_OPCL | (0 << 19) | ((V) << 4))
#define	IDT_MKCMD_WSRAM(A, S)	(IDT_CMD_WSRAM | ((A) << 2) | (S))
#define	IDT_MKCMD_RSRAM(A)	(IDT_CMD_RSRAM | ((A) << 2))
#define	IDT_MKCMD_WFBQ(Q)	(IDT_CMD_WFBQ | (Q))
#define	IDT_MKCMD_RUTIL(S0, S1, A)	\
	    (IDT_CMD_RUTIL | ((S1) << 9) | ((S0) << 8) | (A))
#define	IDT_MKCMD_WUTIL(S0, S1, A)	\
	    (IDT_CMD_WUTIL | ((S1) << 9) | ((S0) << 8) | (A))

/*
 * Configuration register (CFG)
 */
#define	IDT_CFG_SWRST	0x80000000	/* software reset */
#define	IDT_CFG_LOOP	0x40000000	/* internal loopback enable */
#define	IDT_CFG_RXPTH	0x20000000	/* receive path enable */
#define	IDT_CFG_IDLECLP	0x10000000	/* set CLP in null cells */
#define	IDT_CFG_TXFIFO9	0x00000000	/* Tx FIFO 9 cells */
#define	IDT_CFG_TXFIFO1	0x04000000	/* Tx FIFO 1 cells */
#define	IDT_CFG_TXFIFO2	0x08000000	/* Tx FIFO 2 cells */
#define	IDT_CFG_TXFIFO4	0x0C000000	/* Tx FIFO 4 cells */
#define	IDT_CFG_NOIDLE	0x02000000	/* don't send idle cells */
#define	IDT_CFG_RXQ128	0x00000000	/* Rx Status Queue 128 entries */
#define	IDT_CFG_RXQ256	0x00400000	/* Rx Status Queue 256 entries */
#define	IDT_CFG_RXQ512	0x00800000	/* Rx Status Queue 512 entries */
#define	IDT_CFG_ICAPT	0x00200000	/* Invalid cell accept */
#define	IDT_CFG_IGGFC	0x00100000	/* Ignore GFC field */
#define	IDT_CFG_VP0	0x00000000	/* 0 VPI bits */
#define	IDT_CFG_VP1	0x00040000	/* 1 VPI bit */
#define	IDT_CFG_VP2	0x00080000	/* 2 VPI bits */
#define	IDT_CFG_VP8	0x000C0000	/* 8 VPI bits */
#define	IDT_CFG_CTS1K	0x00000000	/* Rx Connection table 1024 entries */
#define	IDT_CFG_CTS4K	0x00010000	/* Rx Connection table 4096 entries */
#define	IDT_CFG_CTS16K	0x00020000	/* Rx Connection table 16384 entries */
#define	IDT_CFG_CTS512	0x00030000	/* Rx Connection table 512 entries */
#define	IDT_CFG_VPECA	0x00008000	/* VPI/VCI error cell accept */
#define	IDT_CFG_RXINONE	0x00000000	/* No interrupt on receive */
#define	IDT_CFG_RXIIMM	0x00001000	/* immediate interrupt */
#define	IDT_CFG_RXI28	0x00002000	/* every 0x2800 clocks */
#define	IDT_CFG_RXI4F	0x00003000	/* every 0x4F00 clocks */
#define	IDT_CFG_RXI74	0x00004000	/* every 0x7400 clocks */
#define	IDT_CFG_RAWIE	0x00000800	/* raw cell queue interrupt enable */
#define	IDT_CFG_RQFIE	0x00000400	/* Rx status queue almost full IE */
#define	IDT_CFG_CACHE	0x00000100	/* begin DMA on cache line */
#define	IDT_CFG_TIMOIE	0x00000080	/* timer roll over interrupt enable */
#define	IDT_CFG_FBIE	0x00000040	/* free buffer queue interrupt enable */
#define	IDT_CFG_TXENB	0x00000020	/* Tx enable */
#define	IDT_CFG_TXINT	0x00000010	/* Tx status interrupt enable */
#define	IDT_CFG_TXUIE	0x00000008	/* Tx underrun interrupt enable */
#define	IDT_CFG_UMODE	0x00000004	/* utopia byte mode */
#define	IDT_CFG_TXSFI	0x00000002	/* Tx status full interrupt enable */
#define	IDT_CFG_PHYIE	0x00000001	/* PHY interrupt enable */

/*
 * Status register (STAT)
 */
#define	IDT_STAT_FRAC3(S)	(((S) >> 28) & 0xf)	/* FBQ3 valid */
#define	IDT_STAT_FRAC2(S)	(((S) >> 24) & 0xf)	/* FBQ2 valid */
#define	IDT_STAT_FRAC1(S)	(((S) >> 20) & 0xf)	/* FBQ1 valid */
#define	IDT_STAT_FRAC0(S)	(((S) >> 16) & 0xf)	/* FBQ0 valid */
#define	IDT_STAT_TSIF	0x00008000	/* Tx status indicator flag */
#define	IDT_STAT_TXICP	0x00004000	/* Tx incomplete PDU */
#define	IDT_STAT_TSQF	0x00001000	/* Tx status queue full */
#define	IDT_STAT_TMROF	0x00000800	/* Timer overflow */
#define	IDT_STAT_PHYI	0x00000400	/* PHY interrupt */
#define	IDT_STAT_CMDBZ	0x00000200	/* command busy */
#define	IDT_STAT_FBQ3A	0x00000100	/* FBQ 3 attention flag */
#define	IDT_STAT_FBQ2A	0x00000080	/* FBQ 2 attention flag */
#define	IDT_STAT_RSQF	0x00000040	/* Rx status queue full */
#define	IDT_STAT_EPDU	0x00000020	/* end of CS-PDU */
#define	IDT_STAT_RAWCF	0x00000010	/* raw cell flag */
#define	IDT_STAT_FBQ1A	0x00000008	/* FBQ 1 attention flag */
#define	IDT_STAT_FBQ0A	0x00000004	/* FBQ 0 attention flag */
#define	IDT_STAT_RSQAF	0x00000002	/* Rx status queue almost full */

/*
 * Cell drop count (CDC)
 */
#define	IDT_CDC_RMID	0x00400000	/* RM cell ID error */
#define	IDT_CDC_CTE	0x00200000	/* Rx connection table error */
#define	IDT_CDC_NFB	0x00100000	/* No free buffers */
#define	IDT_CDC_OAMCRC	0x00080000	/* bad OAM CRC */
#define	IDT_CDC_RMCRC	0x00040000	/* bad RM CRC */
#define	IDT_CDC_RMFIFO	0x00020000	/* RM FIFO full */
#define	IDT_CDC_RXFIFO	0x00010000	/* Rx FIFO full */
#define	IDT_CDC(S)	((S) & 0xffff)	/* cell drop counter */

/*
 * VPI/VCI lookup error count (VPEC)
 */
#define	IDT_VPEC(S)	((S) & 0xffff)

/*
 * Invalid cell count (ICC)
 */
#define	IDT_ICC(S)	((S) & 0xffff)

/*
 * General purpose register
 */
#define	IDT_GP_TXNCC(S)	(((S) >> 24) & 0xff)	/* Tx negative cell count */
#define	IDT_GP_EEDI	0x00010000	/* EEPROM data in */
#define	IDT_GP_BIGE	0x00008000	/* big endian enable */
#define	IDT_GP_RM	0x00000000	/* process RM cells */
#define	IDT_GP_RM_TEE	0x00002000	/* process RM cells and put in RawQ */
#define	IDT_GP_RM_RAW	0x00006000	/* put RM cells in RawQ */
#define	IDT_GP_DLOOP	0x00001000	/* double loopback */
#define	IDT_GP_PCIPAR	0x00000010	/* force PCI parity error */
#define	IDT_GP_PCIPERR	0x00000020	/* force PERR */
#define	IDT_GP_PCISERR	0x00000040	/* force SERR */
#define	IDT_GP_PHY_RST	0x00000008	/* PHY reset */
#define	IDT_GP_EESCLK	0x00000004	/* EEPROM clock */
#define	IDT_GP_EECS	0x00000002	/* EEPROM chip select */
#define	IDT_GP_EEDO	0x00000001	/* EEPROM data out */

/*
 * Receive FIFO descriptor register (RXFD)
 */
#define	IDT_RXFD(A, S)	(((S) << 24) | ((A) << 2))
#define	IDT_RXFDS(V)	(((V) >> 24) & 0xf)
#define	IDT_RXFDA(V)	(((V) & 0x1ffffc) >> 2)

/*
 * ABR & VBR schedule table descriptor register
 */
#define	IDT_ABRSTD(A, S)	(((S) << 24) | ((A) << 2))
#define	IDT_ABRSTDS(V)		(((V) >> 24) & 0x7)
#define	IDT_ABRSTDA(V)		(((V) & 0x1ffffc) >> 2)

/*
 * ABR/VBR ready queue register
 */
#define	IDT_ABRRQH(V)	(((V) >> 16) & 0x3fff)
#define	IDT_ABRRQT(V)	(((V) >>  0) & 0x3fff)
#define	IDT_VBRRQH(V)	(((V) >> 16) & 0x3fff)
#define	IDT_VBRRQT(V)	(((V) >>  0) & 0x3fff)

/*
 * Maximum deficit limit register
 */
#define	IDT_MDFCT_LCI	0x00020000	/* local congestion indicator enable */
#define	IDT_MDFCT_LNI	0x00010000	/* local no incread enable */

/*
 * Transmit command queue register
 */
#define	IDT_TCMDQ_NOP()		((0x0 << 24))		/* no operation */
#define	IDT_TCMDQ_START(C)	((0x1 << 24) | (C))	/* start connection */
#define	IDT_TCMDQ_ULACR(C, L)	((0x2 << 24) | (C) | ((L) << 16))
						/* update LACR */
#define	IDT_TCMDQ_SLACR(C, L)	((0x3 << 24) | (C) | ((L) << 16))
						/* start and update LACR */
#define	IDT_TCMDQ_UIER(C, L)	((0x4 << 24) | (C) | ((L) << 16))
						/* update Int ER */
#define	IDT_TCMDQ_HALT(C)	((0x5 << 24) | (C))	/* halt connection */

/*
 * Free buffer queue size registers
 */
#define	IDT_FBQS(T, N, C, S)	(((T) << 28) | ((N) << 24) | ((C) << 20) | (S))

/*
 * Receive status queue
 */
struct idt_rsqe {
	uint32_t	cid;		/* VPI/VCI */
	uint32_t	handle;		/* buffer handle */
	uint32_t	crc;		/* AAL-5 CRC */
	uint32_t	stat;		/* div. flags */
};
#define	IDT_RSQE_SIZE		16		/* bytes */
#define	IDT_RSQE_VPI(CID)	(((CID) >> 16) & 0xff)
#define	IDT_RSQE_VCI(CID)	((CID) & 0xffff)
#define	IDT_RSQE_TYPE(S)	(((S) >> 30) & 0x3)
#define	IDT_RSQE_DATA		0x2
#define	IDT_RSQE_IDLE		0x3
#define	IDT_RSQE_VALID		0x80000000
#define	IDT_RSQE_POOL(S)	(((S) >> 16) & 0x3)
#define	IDT_RSQE_BUF		0x8000
#define	IDT_RSQE_NZGFC		0x4000
#define	IDT_RSQE_EPDU		0x2000
#define	IDT_RSQE_CBUF		0x1000
#define	IDT_RSQE_EFCIE		0x0800
#define	IDT_RSQE_CLP		0x0400
#define	IDT_RSQE_CRC		0x0200
#define	IDT_RSQE_CNT(S)		((S) & 0x1ff)

#define	IDT_RSQH(R)		(((R) & 0x1ffc) >> 2)
#define	IDT_RSQT(R)		(((R) & 0x1ffc) >> 2)

/*
 * Transmit status queue
 */
#define	IDT_TSQ_SIZE	1024	/* no. of entries */
#define	IDT_TSQE_SIZE	8	/* bytes */
#define	IDT_TSQE_SHIFT	3
struct idt_tsqe {
	uint32_t	stat;
	uint32_t	stamp;
};
#define	IDT_TSQE_EMPTY		0x80000000
#define	IDT_TSQE_TYPE(E)	(((E) >> 29) & 0x3)
#define	IDT_TSQE_TIMER		0x0
#define	IDT_TSQE_TSR		0x1
#define	IDT_TSQE_IDLE		0x2
#define	IDT_TSQE_TBD		0x3
#define	IDT_TSQE_TAG(E)		(((E) >> 24) & 0x1f)
#define	IDT_TSQE_HALTED		0x10
#define	IDT_TSQE_STAMP(E)	((E) & 0xffffff)
#define	IDT_TSQE_TAG_SPACE	32

/*
 * Raw cell handle
 */
struct idt_rawhnd {
	uint32_t	tail;
	uint32_t	handle;
};
#define	IDT_RAWHND_SIZE	8

/*
 * TST
 */
#define	IDT_TST_NULL	(0 << 29)	/* transmit NULL cell */
#define	IDT_TST_CBR	(1 << 29)	/* transmit CBR cell */
#define	IDT_TST_VBR	(2 << 29)	/* transmit [AVU]BR cell */
#define	IDT_TST_BR	(3 << 29)	/* branch */
#define	IDT_TST_MASK	0x7ffff

/*
 * Free buffer queue
 */
#define	IDT_FBQ_SIZE	512		/* entries */

/*
 * Receive connection table
 */
#define	IDT_RCT_FBP2	0x00400000	/* use FBQ 2 */
#define	IDT_RCT_OPEN	0x00080000	/* connection open */
#define	IDT_RCT_AAL0	0x00000000	/* AAL 0 */
#define	IDT_RCT_AAL34	0x00010000	/* AAL 3/4 */
#define	IDT_RCT_AAL5	0x00020000	/* AAL 5 */
#define	IDT_RCT_AALRAW	0x00030000	/* raw cells */
#define	IDT_RCT_AALOAM	0x00040000	/* OAM cells */
#define	IDT_RCT_RCI	0x00008000	/* raw cell interrupt enable */
#define	IDT_RCT_IACT_CNT_MASK	0x1c000000
#define	IDT_RCT_IACT_CNT_SHIFT	26
#define	IDT_RCT_ENTRY_SIZE	4	/* words */

/*
 * Transmit connection table
 */
#define	IDT_TCT_CBR	0x00000000
#define	IDT_TCT_VBR	0x40000000
#define	IDT_TCT_ABR	0x80000000
#define	IDT_TCT_UBR	0x00000000
#define	IDT_TCT_UBR_FLG	0x80000000	/* word8 flag */
#define	IDT_TCT_HALT	0x80000000	/* connection halted */
#define	IDT_TCT_IDLE	0x40000000	/* connection idle */
#define	IDT_TCT_TSIF	0x00004000
#define	IDT_TCT_MAXIDLE	0x7f000000
#define	IDT_TCT_MBS_SHIFT	16
#define	IDT_TCT_CRM_SHIFT	29
#define	IDT_TCT_NAGE_SHIFT	21
#define	IDT_TCT_LMCR_SHIFT	24
#define	IDT_TCT_CDF_SHIFT	20
#define	IDT_TCT_RDF_SHIFT	14
#define	IDT_TCT_AIR_SHIFT	8
#define	IDT_TCT_ACRI_SHIFT	16

/*
 * Segmentation channel queue
 */
#define	IDT_SCQ_SIZE	64		/* number of entries */
struct idt_tbd {
	uint32_t	flags;
	uint32_t	addr;
	uint32_t	aal5;
	uint32_t	hdr;
};
#define	IDT_TBD_SIZE	16		/* bytes */
#define	IDT_TBD_SHIFT	4
#define	IDT_TBD_TSR	0x80000000	/* TSR entry */
#define	IDT_TBD_EPDU	0x40000000	/* end of AAL PDU */
#define	IDT_TBD_TSIF	0x20000000	/* generate status */
#define	IDT_TBD_AAL0	0x00000000	/* AAL0 */
#define	IDT_TBD_AAL34	0x04000000	/* AAL3/4 */
#define	IDT_TBD_AAL5	0x08000000	/* AAL5 */
#define	IDT_TBD_AALOAM	0x10000000	/* OAM cells */
#define	IDT_TBD_GTSI	0x02000000	/* generate transmit status entry */
#define	IDT_TBD_TAG_SHIFT	20
#define	IDT_TBD_HDR(VPI, VCI, PTI, CLP) \
	    (((VPI) << 20) | ((VCI) << 4) | ((PTI) << 1) | (CLP))
#define	IDT_TBD_VPI(H)	(((H) >> 20) & 0xff)
#define	IDT_TBD_VCI(H)	(((H) >> 4) & 0xffff)

/*
 * Segmentation channel descriptor
 */
#define	IDT_SCD_SIZE	12		/* words */

/*
 * Memory map for the different RAM sizes
 *
 *		16k		32k		128k		512k
 *
 * TxCT		0x00000/4k	0x00000/8x	0x00000/32k	0x00000/128k
 * RxCT		0x01000/2k	0x02000/4k	0x08000/16k	0x20000/64k
 * FBQ0		0x01800/1k	0x03000/1k	0x0c000/1k	0x30000/1k
 * FBQ1		0x01c00/1k	0x03400/1k	0x0c400/1k	0x30400/1k
 * FBQ2		0x02000/1k	0x03800/1k	0x0c800/1k	0x30800/1k
 * FBQ3		-		-		-		-
 * RT		0x02400/4.5k	0x03c00/4.5k	0x0cc00/4.5k	0x30c00/4.5k
 * SCD		0x03600/597	0x04e00/1621	0x0de00/9358	0x31e00/43036
 * TST		0x06000/2x2k	0x0c000/2x4k	0x37000/2x8k	0xef000/2x16k
 * ABR ST	0x07000/2x1k	0x0e000/2x2k	0x3b000/2x8k	0xf7000/2x16k
 * RxFIFO	0x07800/2k	0x0f000/4k	0x3f000/4k	0xff000/4k
 * End		0x08000		0x10000		0x40000		0x100000
 */
struct idt_mmap {
	u_int	sram;		/* K SRAM */
	u_int	max_conn;	/* connections */
	u_int	vcbits;		/* VPI + VCI bits */
	u_int	rxtab;		/* CFG word for CNTBL field */
	u_int	rct;		/* RCT base */
	u_int	rtables;	/* rate table address */
	u_int	scd_base;	/* SCD area base address */
	u_int	scd_num;	/* number of SCDs */
	u_int	tst1base;	/* base address of TST 1 */
	u_int	tst_size;	/* TST size in words */
	u_int	abrstd_addr;	/* schedule table address */
	u_int	abrstd_size;	/* schedule table size */
	u_int	abrstd_code;	/* schedule table size */
	u_int	rxfifo_addr;	/* address */
	u_int	rxfifo_size;	/* in words */
	u_int	rxfifo_code;	/* size */
};
#define	IDT_MMAP {							\
	{ /* 16k x 32, 512 connections */				\
	  16, 512, 9, IDT_CFG_CTS512,	/* RAM, connections, VC bits */	\
	  0x01000,			/* RCT base */			\
	  0x02400,			/* rate table address */	\
	  0x03600, 597,			/* SCD base and num */		\
	  0x06000, 2048,		/* TST/words, base */		\
	  0x07000, 2048, 0x1,		/* ABR schedule table */	\
	  0x07800, 2048, 0x2		/* RxFIFO size in words */	\
	},								\
	{ /* 32k x 32, 1024 connections */				\
	  32, 1024, 10, IDT_CFG_CTS1K,	/* RAM, connections, VC bits */	\
	  0x02000,			/* RCT base */			\
	  0x03c00,			/* rate table address */	\
	  0x04e00, 1621,		/* SCD base and num */		\
	  0x0c000, 4096,		/* TST/words, base */		\
	  0x0e000, 4096, 0x2, 		/* ABR schedule table */	\
	  0x0f000, 4096, 0x3		/* RxFIFO size in words */	\
	},								\
	{ /* 128k x 32, 4096 connections */				\
	  128, 4096, 12, IDT_CFG_CTS4K,	/* RAM, connections, VC bits */	\
	  0x08000,			/* RCT base */			\
	  0x0cc00,			/* rate table address */	\
	  0x0de00, 9358,		/* SCD base and num */		\
	  0x37000, 8192, 		/* TST/words, base */		\
	  0x3b000, 16384, 0x4,		/* ABR schedule table */	\
	  0x3f000, 4096, 0x3		/* RxFIFO size in words */	\
	},								\
	{ /* 512k x 32, 512 connections */				\
	  512, 16384, 14, IDT_CFG_CTS16K, /* RAM, connections, VC bits */\
	  0x20000,			/* RCT base */			\
	  0x30c00,			/* rate table address */	\
	  0x31e00, 43036,		/* SCD base and num */		\
	  0xef000, 16384,		/* TST/words, base */		\
	  0xf7000, 16384, 0x5,		/* ABR schedule table */	\
	  0xff000,  4096, 0x3		/* RxFIFO size in words */	\
	},								\
}
