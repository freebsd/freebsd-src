/*	$NetBSD: midwayreg.h,v 1.6 1997/03/20 21:34:47 chuck Exp $	*/

/*
 * m i d w a y r e g . h
 *
 * this file contains the description of the ENI ATM midway chip
 * data structures.   see midway.c for more details.
 *
 * $FreeBSD: src/sys/dev/en/midwayreg.h,v 1.5.30.1 2008/11/25 02:59:29 kensmith Exp $
 */

#define MID_SZTOB(X) 	((X) * 256 * 4) /* size to bytes */
#define MID_BTOSZ(X)	((X) / 256 / 4)	/* bytes to "size" */

#define MID_N_VC	1024		/* # of VCs we can use */
#define MID_VCI_BITS	10		/* number of bits */
#define MID_NTX_CH	8		/* 8 transmit channels (shared) */
#define MID_ATMDATASZ	48		/* need data in 48 byte blocks */

/*
 * card data structures, top down
 *
 * in order to have a portable driver, the netbsd guys will not let us
 * use structs.   we have a bus_space_handle_t which is the en_base address.
 * everything else is an offset from that base.   all card data must be 
 * accessed with bus_space_read_4()/bus_space_write_4():
 *
 * rv = bus_space_read_4(sc->en_memt, sc->en_base, BYTE_OFFSET);
 * bus_space_write_4(sc->en_memt, sc->en_base, BYTE_OFFSET, VALUE);
 *
 * en_card: the whole card (prom + phy + midway + obmem)
 * 	obmem contains: vci tab + dma queues (rx & tx) + service list + bufs
 */

/* byte offsets from en_base of various items */
#define MID_SUNIOFF	0x020000	/* SUNI offset */
#define MID_PHYOFF	0x030000	/* PHY offset */
#define MID_MIDOFF	0x040000	/* midway regs offset */
#define MID_RAMOFF	0x200000	/* RAM offset */
#define MID_DRQOFF	0x204000	/* DRQ offset */
#define MID_DRQEND	MID_DTQOFF	/* DRQ end */
#define MID_DTQOFF	0x205000	/* DTQ offset */
#define MID_DTQEND	MID_SLOFF	/* DTQ end */
#define MID_SLOFF	0x206000	/* service list */
#define MID_SLEND	MID_BUFOFF	/* service list end */
#define MID_BUFOFF	0x207000	/* buffer area */
#define MID_PROBEOFF	0x21fffc	/* start probe here */
#define MID_PROBSIZE	0x020000	/* 128 KB */
#define MID_MAXOFF	0x3ffffc	/* max offset */

/*
 * prom & phy: not defined here
 */
#define MID_ADPMACOFF	0xffc0		/* mac address offset (adaptec only) */
#define MID_NSUNI	256		/* suni registers */

/*
 * midway regs  (byte offsets from en_base)
 */
#define MID_RESID	0x40000		/* write=reset reg, read=ID reg */

#define MID_VER(X)	(((X) & 0xf0000000) >> 28) /* midway version # */
#define MID_MID(X)	(((X) & 0x700) >> 8) 	/* motherboard ID */
#define MID_IS_SABRE(X) ((X) & 0x80)		/* sabre controller? */
#define MID_IS_SUNI(X)	((X) & 0x40)		/* SUNI? vs utopia */
#define MID_IS_UPIPE(X)	((X) & 0x20)		/* utopia pipeline? */
#define MID_DID(X)	((X) & 0x1f)		/* daughterboard ID */

#define MID_INTACK	0x40004		/* interrupt ACK */
#define MID_INTSTAT	0x40008		/* interrupt status */
#define MID_INTENA	0x4000c		/* interrupt enable */

#define MID_TXCHAN(N) (1 << ((N) + 9))	/* ack/status/enable xmit channel bit*/
#define MID_INT_TX	0x1fe00		/* mask for any xmit interrupt */
#define MID_INT_DMA_OVR 0x00100		/* DMA overflow interrupt */
#define MID_INT_IDENT   0x00080		/* ident match error interrupt */
#define MID_INT_LERR    0x00040		/* LERR interrupt (sbus?) */
#define MID_INT_DMA_ERR 0x00020		/* DMA error interrupt */
#define MID_INT_DMA_RX  0x00010		/* DMA recv interrupt */
#define MID_INT_DMA_TX	0x00008		/* DMA xmit interrupt */
#define MID_INT_SERVICE 0x00004		/* service list interrupt */
#define MID_INT_SUNI	0x00002		/* SUNI interrupt */
#define MID_INT_STATS	0x00001		/* stats overflow interrupt */

#define MID_INT_ANY	0x1ffff		/* any interrupt? */

#define MID_INTBITS "\20\21T7\20T6\17T5\16T4\15T3\14T2\13T1\12T0\11DMAOVR\10ID\7LERR\6DMAERR\5RXDMA\4TXDMA\3SERV\2SUNI\1STAT"

#define MID_MAST_CSR	0x40010		/* master CSR */

#define MID_IPL(X)	(((X) & 0x1c0) >> 6) /* IPL */
#define MID_SETIPL(I)	((I) << 6)
#define MID_MCSR_TXLOCK	0x20		/* lock on xmit overflow mode */
/* NOTE: next 5 bits: write 1 means enable, write 0 means no change */
#define MID_MCSR_ENDMA	0x10		/* DMA enable */
#define MID_MCSR_ENTX	0x08		/* TX enable */
#define MID_MCSR_ENRX	0x04		/* RX enable */
#define MID_MCSR_W1MS	0x02		/* wait 1 msec */
#define MID_MCSR_W500US	0x01		/* wait 500 usec */

#define MID_MCSRBITS "\20\6LCK\5DMAON\4TXON\3RXON\2W1MS\1W500US"

#define MID_STAT	0x40014		/* stat register, clear on read */

#define MID_VTRASH(X) (((X) >> 16) & 0xffff)
					/* # cells trashed due to VCI's mode */
#define MID_OTRASH(X) ((X) & 0xffff)	/* # cells trashed due to overflow */

#define MID_SERV_WRITE	0x40018		/* 10 bit service write pointer (r/o) */
#define MID_DMA_ADDR	0x4001c		/* VA of DMA (r/o) */

  /* DMA queue pointers (bits 0 to 8) */
#define MID_DMA_WRRX	0x40020		/* write ptr. for DMA recv queue */
					/* (for adaptor -> host xfers) */
#define MID_DMA_RDRX	0x40024		/* read ptr for DMA recv queue (r/o) */
					/* (i.e. current adaptor->host xfer) */
#define MID_DMA_WRTX	0x40028		/* write ptr for DMA xmit queue */
					/* (for host -> adaptor xfers) */
#define MID_DMA_RDTX	0x4002c		/* read ptr for DMA xmit queue (r/o) */
					/* (i.e. current host->adaptor xfer) */

/* xmit channel regs (1 per channel, MID_NTX_CH max channels) */

#define MIDX_PLACE(N)	(0x40040+((N)*0x10))	/* xmit place */

#define MIDX_MKPLACE(SZ,LOC) ( ((SZ) << 11) | (LOC) )
#define MIDX_LOC(X)	((X) & 0x7ff)	/* location in obmem */
#define MIDX_SZ(X)	((X) >> 11)	/* (size of block / 256) in int32_t's*/
#define MIDX_BASE(X)	\
	(((MIDX_LOC(X) << MIDV_LOCTOPSHFT) * sizeof(uint32_t)) + MID_RAMOFF)

/* the following two regs are word offsets in the block */
/* xmit read pointer (r/o) */
#define MIDX_READPTR(N)		(0x40044 + ((N) * 0x10))
/* seg currently in DMA (r/o) */
#define MIDX_DESCSTART(N)	(0x40048 + ((N) * 0x10))

/*
 * obmem items
 */

/* 
 * vci table in obmem (offset from MID_VCTOFF)
 */
#define MID_VC(N)	(MID_RAMOFF + ((N) * 0x10))

#define MIDV_TRASH	0x00000000	/* ignore VC */
#define MIDV_AAL5	0x80000000	/* do AAL5 on it */
#define MIDV_NOAAL	0x40000000	/* do per-cell stuff on it */
#define MIDV_MASK	0xc0000000	/* mode mask */
#define MIDV_SETMODE(VC,M) (((VC) & ~(MIDV_MASK)) | (M))  /* new mode */
#define MIDV_PTI	0x20000000	/* save PTI cells? */
#define MIDV_LOCTOPSHFT	8		/* shift to get top 11 bits of 19 */
#define MIDV_LOCSHIFT	18
#define MIDV_LOCMASK	0x7ff
#define MIDV_LOC(X)	(((X) >> MIDV_LOCSHIFT) & MIDV_LOCMASK) 
					/* 11 most sig bits of addr */
#define MIDV_SZSHIFT	15
#define MIDV_SZ(X)	(((X) >> MIDV_SZSHIFT) & 7) 
					/* size encoded the usual way */
#define MIDV_INSERVICE	0x1		/* in service list */

#define MID_DST_RP(N)	(MID_VC(N)|0x4)

#define MIDV_DSTART_SHIFT	16		/* shift */
#define MIDV_DSTART(X) (((X) >> MIDV_DSTART_SHIFT) & 0x7fff)
#define MIDV_READP_MASK		0x7fff		/* valid bits, (shift = 0) */

#define MID_WP_ST_CNT(N) (MID_VC(N)|0x8)      /* write pointer/state/count */

#define MIDV_WRITEP_MASK	0x7fff0000	/* mask for write ptr. */
#define MIDV_WRITEP_SHIFT	16
#define MIDV_ST_IDLE		0x0000
#define MIDV_ST_TRASH		0xc000
#define MIDV_ST_REASS		0x4000
#define MIDV_CCOUNT		0x7ff		/* cell count */

#define MID_CRC(N)	(MID_VC(N)|0xc)		/* CRC */

/*
 * dma recv q.
 */
#define MID_DMA_END		(1 << 5)	/* for both tx and rx */
#define MID_DMA_CNT(X)		(((X) >> 16) & 0xffff)
#define MID_DMA_TXCHAN(X)	(((X) >> 6) & 0x7)
#define MID_DMA_RXVCI(X)  	(((X) >> 6) & 0x3ff)
#define MID_DMA_TYPE(X)		((X) & 0xf)

#define MID_DRQ_N		512		/* # of descriptors */
/* convert byte offset to reg value */
#define MID_DRQ_A2REG(N)	(((N) - MID_DRQOFF) >> 3)
/* and back */
#define MID_DRQ_REG2A(N)	(((N) << 3) + MID_DRQOFF)

/* note: format of word 1 of RXQ is different beween ENI and ADP cards */
#define MID_MK_RXQ_ENI(CNT, VC, END, TYPE) \
	(((CNT) << 16) | ((VC) << 6) | (END) | (TYPE))

#define MID_MK_RXQ_ADP(CNT, VC, END, JK) \
	(((CNT) << 12) | ((VC) << 2) | ((END) >> 4) | (((JK) != 0) ? 1 : 0))
/*
 * dma xmit q.
 */
#define MID_DTQ_N		512		/* # of descriptors */
/* convert byte offset to reg value */
#define MID_DTQ_A2REG(N)	(((N) - MID_DTQOFF) >> 3)
/* and back */
#define MID_DTQ_REG2A(N)	(((N) << 3) + MID_DTQOFF)

/* note: format of word 1 of TXQ is different beween ENI and ADP cards */
#define MID_MK_TXQ_ENI(CNT, CHN, END, TYPE) \
	(((CNT) << 16) | ((CHN) << 6) | (END) | (TYPE))

#define MID_MK_TXQ_ADP(CNT, CHN, END, JK) \
	(((CNT) << 12) | ((CHN) << 2) | ((END) >> 4) | (((JK) != 0) ? 1 : 0))

/*
 * dma types
 */
#define MIDDMA_JK	0x3	/* just kidding */
#define MIDDMA_BYTE	0x1	/* byte */
#define MIDDMA_2BYTE	0x2	/* 2 bytes */
#define MIDDMA_WORD	0x0	/* word */
#define MIDDMA_2WORD	0x7	/* 2 words */
#define MIDDMA_4WORD	0x4	/* 4 words */
#define MIDDMA_8WORD	0x5	/* 8 words */
#define MIDDMA_16WORD	0x6	/* 16 words!!! */
#define MIDDMA_2WMAYBE	0xf	/* 2 words, maybe */
#define MIDDMA_4WMAYBE	0xc	/* 4 words, maybe */
#define MIDDMA_8WMAYBE	0xd	/* 8 words, maybe */
#define MIDDMA_16WMAYBE	0xe	/* 16 words, maybe */

#define MIDDMA_MAYBE	0xc	/* mask to detect WMAYBE dma code */
#define MIDDMA_MAXBURST	(16 * sizeof(uint32_t))		/* largest burst */

/*
 * service list
 */
#define MID_SL_N		1024	/* max # entries on slist */
/* convert byte offset to reg value */
#define MID_SL_A2REG(N)		(((N) - MID_SLOFF) >> 2)
/* and back */
#define MID_SL_REG2A(N)		(((N) << 2) + MID_SLOFF)

/*
 * data in the buffer area of obmem
 */
/*
 * recv buffer desc. (1 uint32_t at start of buffer)
 */
#define MID_RBD_SIZE	4			/* RBD size */
#define MID_CHDR_SIZE	4			/* on aal0, cell header size */
#define MID_RBD_ID(X)	((X) & 0xfe000000)	/* get ID */
#define MID_RBD_STDID	0x36000000		/* standard ID */
#define MID_RBD_CLP	0x01000000		/* CLP: cell loss priority */
#define MID_RBD_CE	0x00010000		/* CE: congestion experienced */
#define MID_RBD_T	0x00001000		/* T: trashed due to overflow */
#define MID_RBD_CRCERR	0x00000800		/* CRC error */
#define MID_RBD_CNT(X)	((X) & 0x7ff)		/* cell count */

/*
 * xmit buffer desc. (2 uint32_t's at start of buffer)
 * (note we treat the PR & RATE as a single uint8_t)
 */
#define MID_TBD_SIZE	8
#define MID_TBD_MK1(AAL,PR_RATE,CNT) \
	(MID_TBD_STDID | (AAL) | ((PR_RATE) << 19) | (CNT))
#define MID_TBD_STDID	0xb0000000	/* standard ID */
#define MID_TBD_AAL5 	0x08000000	/* AAL 5 */
#define MID_TBD_NOAAL5	0x00000000	/* not AAL 5 */

#define MID_TBD_MK2(VCI,PTI,CLP) \
	(((VCI) << 4) | ((PTI) << 1) | (CLP))

/*
 * aal5 pdu tail, last 2 words of last cell of AAL5 frame
 * (word 2 is CRC .. handled by hw)
 */
#define MID_PDU_SIZE		8
#define MID_PDU_MK1(UU, CPI, LEN) \
	(((UU) << 24) | ((CPI) << 16) | (LEN))
#define MID_PDU_LEN(X) ((X) & 0xffff)
