/*	$FreeBSD$	*/
/*	$NecBSD: dp83932var.h,v 1.3 1999/01/24 01:39:51 kmatsuda Exp $	*/
/*	$NetBSD: if_snvar.h,v 1.12 1998/05/01 03:42:47 scottr Exp $	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1997, 1998, 1999
 *	Kouichi Matsuda.  All rights reserved.
 */
/*
 * Copyright (c) 1991   Algorithmics Ltd (http://www.algor.co.uk)
 * You may use, copy, and modify this program so long as you retain the
 * copyright line.
 */

/*
 * if_snvar.h -- National Semiconductor DP8393X (SONIC) NetBSD/mac68k vars
 */
/*
 * Modified for NetBSD/pc98 1.2.1 from NetBSD/mac68k 1.2D by Kouichi Matsuda.
 * Make adapted for NEC PC-9801-83, 84, PC-9801-103, 104, PC-9801N-25 and
 * PC-9801N-J02, J02R, which uses National Semiconductor DP83934AVQB as
 * Ethernet Controller and National Semiconductor NS46C46 as
 * (64 * 16 bits) Microwire Serial EEPROM.
 */

/* borrow from arch/mac68k/dev/if_mcvar.h for debug. */
#ifdef DDB
#define	integrate
#define hide
#else
#define	integrate	static __inline
#define hide		static
#endif

/* NetBSD Emulation */
#ifdef __FreeBSD__
#ifndef NBPG
#define NBPG PAGE_SIZE
#endif
#ifndef PGOFSET
#define PGOFSET PAGE_MASK
#endif
typedef unsigned long ulong;
#define delay(x) DELAY(x)
#endif

/*
 * Vendor types
 */

/*
 * SONIC buffers need to be aligned 16 or 32 bit aligned.
 * These macros calculate and verify alignment.
 */
#define	ROUNDUP(p, N)	(((int) p + N - 1) & ~(N - 1))

#define SOALIGN(m, array)	(m ? (ROUNDUP(array, 4)) : (ROUNDUP(array, 2)))

#define LOWER(x) ((unsigned)(x) & 0xffff)
#define UPPER(x) ((unsigned)(x) >> 16)

/*
 * Memory access macros. Since we handle SONIC in 16 bit mode (PB5X0)
 * and 32 bit mode (everything else) using a single GENERIC kernel
 * binary, all structures have to be accessed using macros which can
 * adjust the offsets appropriately.
 */
/* m is not sc->bitmode, we treat m as sc. */
#define	SWO(m, a, o, x)	(*(m)->sc_writetodesc)((m), (a), (o), (x))
#define	SRO(m, a, o)	(*(m)->sc_readfromdesc)((m), (a), (o))

/*
 * Register access macros. We use bus_space_* to talk to the Sonic
 * registers. A mapping table is used in case a particular configuration
 * hooked the regs up at non-word offsets.
 */
#define	NIC_GET(sc, reg)	(*(sc)->sc_nic_get)(sc, reg)
#define	NIC_PUT(sc, reg, val)	(*(sc)->sc_nic_put)(sc, reg, val)

#define	SONIC_GETDMA(p)	(p)

/* pc98 does not have any write buffers to flush... */
#define	wbflush()

/*
 * buffer sizes in 32 bit mode
 * 1 TXpkt is 4 hdr words + (3 * FRAGMAX) + 1 link word == 23 words == 92 bytes
 *
 * 1 RxPkt is 7 words == 28 bytes
 * 1 Rda   is 4 words == 16 bytes
 *
 * The CDA is 17 words == 68 bytes
 *
 * total space in page 0 = NTDA * 92 + NRRA * 16 + NRDA * 28 + 68
 */

#define NRBA    16		/* # receive buffers < NRRA */
#define RBAMASK (NRBA-1)
#define NTDA    16		/* # transmit descriptors */
#define NRRA    64		/* # receive resource descriptors */
#define RRAMASK (NRRA-1)	/* the reason why NRRA must be power of two */

#define FCSSIZE 4		/* size of FCS appended to packets */

/*
 * maximum receive packet size plus 2 byte pad to make each
 * one aligned. 4 byte slop (required for eobc)
 */
#define RBASIZE(sc)	(sizeof(struct ether_header) + ETHERMTU + FCSSIZE + \
			 ((sc)->bitmode ? 6 : 2))

/*
 * transmit buffer area
 */
#define TXBSIZE	1536	/* 6*2^8 -- the same size as the 8390 TXBUF */

#define	SN_NPAGES	2 + NRBA + (NTDA/2)

typedef struct mtd {
	u_int32_t	mtd_vtxp;
	u_int32_t	mtd_vbuf;
	struct mbuf	*mtd_mbuf;
} mtd_t;

/*
 * The snc_softc for PC-98 if_snc.
 */
typedef struct snc_softc {
	struct arpcom	sc_ethercom;
#define	sc_if		sc_ethercom.ac_if	/* network visible interface */

	device_t	sc_dev;

	struct resource *	ioport;
	int			ioport_rid;
	struct resource *	iomem;
	int			iomem_rid;
	struct resource *	irq;
	int			irq_rid;
	void *			irq_handle;

	bus_space_tag_t		sc_iot;		/* bus identifier for io */
	bus_space_tag_t		sc_memt;	/* bus identifier for mem */
	bus_space_handle_t	sc_ioh;		/* io handle */
	bus_space_handle_t	sc_memh;	/* bus memory handle */

	int		bitmode;	/* 32 bit mode == 1, 16 == 0 */

	u_int16_t	sncr_dcr;	/* DCR for this instance */
	u_int16_t	sncr_dcr2;	/* DCR2 for this instance */

	int		sc_rramark;	/* index into v_rra of wp */
	u_int32_t	v_rra[NRRA];	/* DMA addresses of v_rra */
	u_int32_t	v_rea;		/* ptr to the end of the rra space */

	int		sc_rxmark;	/* current hw pos in rda ring */
	int		sc_rdamark;	/* current sw pos in rda ring */
	int		sc_nrda;	/* total number of RDAs */
	u_int32_t	v_rda;

	u_int32_t	rbuf[NRBA];

	struct mtd	mtda[NTDA];
	int		mtd_hw;		/* idx of first mtd given to hw */
	int		mtd_prev;	/* idx of last mtd given to hardware */
	int		mtd_free;	/* next free mtd to use */
	int		mtd_tlinko;	/*
					 * offset of tlink of last txp given
					 * to SONIC. Need to clear EOL on
					 * this word to add a desc.
					 */
	int		mtd_pint;	/* Counter to set TXP_PINT */

	u_int32_t	v_cda;

	u_int8_t	curbank;	/* current window bank */

	struct	ifmedia sc_media;	/* supported media information */

	/*
	 * NIC register access functions:
	 */
	u_int16_t	(*sc_nic_get)
		__P((struct snc_softc *, u_int8_t));
	void		(*sc_nic_put)
		__P((struct snc_softc *, u_int8_t, u_int16_t));

	/*
	 * Memory functions:
	 *
	 *	copy to/from descriptor
	 *	copy to/from buffer
	 *	zero bytes in buffer
	 */
	void		(*sc_writetodesc)
		__P((struct snc_softc *, u_int32_t, u_int32_t, u_int16_t));
	u_int16_t	(*sc_readfromdesc)
		__P((struct snc_softc *, u_int32_t, u_int32_t));
	void		(*sc_copytobuf)
		__P((struct snc_softc *, void *, u_int32_t, size_t));
	void		(*sc_copyfrombuf)
		__P((struct snc_softc *, void *, u_int32_t, size_t));
	void		(*sc_zerobuf)
		__P((struct snc_softc *, u_int32_t, size_t));

	/*
	 * Machine-dependent functions:
	 *
	 *	hardware reset hook - may be NULL
	 *	hardware init hook - may be NULL
	 *	media change hook - may be NULL
	 */
	void	(*sc_hwreset) __P((struct snc_softc *));
	void	(*sc_hwinit) __P((struct snc_softc *));
	int	(*sc_mediachange) __P((struct snc_softc *));
	void	(*sc_mediastatus) __P((struct snc_softc *,
		    struct ifmediareq *));

	int	sc_enabled;	/* boolean; power enabled on interface */

	int	(*sc_enable) __P((struct snc_softc *));
	void	(*sc_disable) __P((struct snc_softc *));

	void	*sc_sh;		/* shutdownhook cookie */
	int	gone;

#if NRND > 0
	rndsource_element_t	rnd_source;
#endif
} snc_softc_t;

/*
 * Accessing SONIC data structures and registers as 32 bit values
 * makes code endianess independent.  The SONIC is however always in
 * bigendian mode so it is necessary to ensure that data structures shared
 * between the CPU and the SONIC are always in bigendian order.
 */

/*
 * Receive Resource Descriptor
 * This structure describes the buffers into which packets
 * will be received.  Note that more than one packet may be
 * packed into a single buffer if constraints permit.
 */
#define	RXRSRC_PTRLO	0	/* buffer address LO */
#define	RXRSRC_PTRHI	1	/* buffer address HI */
#define	RXRSRC_WCLO	2	/* buffer size (16bit words) LO */
#define	RXRSRC_WCHI	3	/* buffer size (16bit words) HI */

#define	RXRSRC_SIZE(sc)	(sc->bitmode ? (4 * 4) : (4 * 2))

/*
 * Receive Descriptor
 * This structure holds information about packets received.
 */
#define	RXPKT_STATUS	0
#define	RXPKT_BYTEC	1
#define	RXPKT_PTRLO	2
#define	RXPKT_PTRHI	3
#define	RXPKT_SEQNO	4
#define	RXPKT_RLINK	5
#define	RXPKT_INUSE	6
#define	RXPKT_SIZE(sc)	(sc->bitmode ? (7 * 4) : (7 * 2))

#define RBASEQ(x) (((x)>>8)&0xff)
#define PSNSEQ(x) ((x) & 0xff)

/*
 * Transmit Descriptor
 * This structure holds information about packets to be transmitted.
 */
#define FRAGMAX	8		/* maximum number of fragments in a packet */

#define	TXP_STATUS	0	/* + transmitted packet status */
#define	TXP_CONFIG	1	/* transmission configuration */
#define	TXP_PKTSIZE	2	/* entire packet size in bytes */
#define	TXP_FRAGCNT	3	/* # fragments in packet */

#define	TXP_FRAGOFF	4	/* offset to first fragment */
#define	TXP_FRAGSIZE	3	/* size of each fragment desc */
#define	TXP_FPTRLO	0	/* ptr to packet fragment LO */
#define	TXP_FPTRHI	1	/* ptr to packet fragment HI */
#define	TXP_FSIZE	2	/* fragment size */

#define	TXP_WORDS	(TXP_FRAGOFF + (FRAGMAX*TXP_FRAGSIZE) + 1)	/* 1 for tlink */
#define	TXP_SIZE(sc)	((sc->bitmode) ? (TXP_WORDS*4) : (TXP_WORDS*2))

#define EOL	0x0001		/* end of list marker for link fields */

/*
 * CDA, the CAM descriptor area. The SONIC has a 16 entry CAM to
 * match incoming addresses against. It is programmed via DMA
 * from a memory region.
 */
#define MAXCAM	16	/* number of user entries in CAM */
#define	CDA_CAMDESC	4	/* # words i na descriptor */
#define	CDA_CAMEP	0	/* CAM Address Port 0 xx-xx-xx-xx-YY-YY */
#define	CDA_CAMAP0	1	/* CAM Address Port 1 xx-xx-YY-YY-xx-xx */
#define	CDA_CAMAP1	2	/* CAM Address Port 2 YY-YY-xx-xx-xx-xx */
#define	CDA_CAMAP2	3
#define	CDA_ENABLE	64	/* mask enabling CAM entries */
#define	CDA_SIZE(sc)	((4*16 + 1) * ((sc->bitmode) ? 4 : 2))

void	sncconfig __P((struct snc_softc *, int *, int, int, u_int8_t *));
void	sncintr __P((void *));
void	sncshutdown __P((void *));
