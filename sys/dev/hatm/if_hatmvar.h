/*
 * Copyright (c) 2001-2003
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
 * Fore HE driver for NATM
 */

/*
 * Debug statistics of the HE driver
 */
struct istats {
	uint32_t	tdprq_full;
	uint32_t	hbuf_error;
	uint32_t	crc_error;
	uint32_t	len_error;
	uint32_t	flow_closed;
	uint32_t	flow_drop;
	uint32_t	tpd_no_mem;
	uint32_t	rx_seg;
	uint32_t	empty_hbuf;
	uint32_t	short_aal5;
	uint32_t	badlen_aal5;
	uint32_t	bug_bad_isw;
	uint32_t	bug_no_irq_upd;
	uint32_t	itype_tbrq;
	uint32_t	itype_tpd;
	uint32_t	itype_rbps;
	uint32_t	itype_rbpl;
	uint32_t	itype_rbrq;
	uint32_t	itype_rbrqt;
	uint32_t	itype_unknown;
	uint32_t	itype_phys;
	uint32_t	itype_err;
	uint32_t	defrag;
	uint32_t	mcc;
	uint32_t	oec;
	uint32_t	dcc;
	uint32_t	cec;
	uint32_t	no_rcv_mbuf;
};

/* Card memory layout parameters */
#define HE_CONFIG_MEM_LAYOUT {						\
	{			/* 155 */				\
	  20,			/* cells_per_row */			\
	  1024,			/* bytes_per_row */			\
	  512,			/* r0_numrows */			\
	  1018,			/* tx_numrows */			\
	  512,			/* r1_numrows */			\
	  6,			/* r0_startrow */			\
	  2			/* cells_per_lbuf */			\
	}, {			/* 622 */				\
	  40,			/* cells_per_row */			\
	  2048,			/* bytes_per_row */			\
	  256,			/* r0_numrows */			\
	  512,			/* tx_numrows */			\
	  256,			/* r1_numrows */			\
	  0,			/* r0_startrow */			\
	  4			/* cells_per_lbuf */			\
	}								\
}

/*********************************************************************/
struct hatm_softc;

/*
 * A chunk of DMA-able memory
 */
struct dmamem {
	u_int		size;		/* in bytes */
	u_int		align;		/* alignement */
	bus_dma_tag_t	tag;		/* DMA tag */
	void		*base;		/* the memory */
	bus_addr_t	paddr;		/* physical address */
	bus_dmamap_t	map;		/* the MAP */
};

/*
 * RBP (Receive Buffer Pool) queue entry and queue.
 */
struct herbp {
	u_int		size;		/* RBP number of entries (power of two) */
	u_int		thresh;		/* interrupt treshold */
	uint32_t	bsize;		/* buffer size in bytes */
	u_int		offset;		/* free space at start for small bufs */
	uint32_t	mask;		/* mask for index */
	struct dmamem	mem;		/* the queue area */
	struct he_rbpen	*rbp;
	uint32_t	head, tail;	/* head and tail */
};

/*
 * RBRQ (Receive Buffer Return Queue) entry and queue.
 */
struct herbrq {
	u_int		size;		/* number of entries */
	u_int		thresh;		/* interrupt threshold */
	u_int		tout;		/* timeout value */
	u_int		pcnt;		/* packet count threshold */
	struct dmamem	mem;		/* memory */
	struct he_rbrqen *rbrq;
	uint32_t	head;		/* driver end */
};

/*
 * TPDRQ (Transmit Packet Descriptor Ready Queue) entry and queue
 */
struct hetpdrq {
	u_int		size;		/* number of entries */
	struct dmamem	mem;		/* memory */
	struct he_tpdrqen *tpdrq;
	u_int		head;		/* head (copy of adapter) */
	u_int		tail;		/* written back to adapter */
};

/*
 * TBRQ (Transmit Buffer Return Queue) entry and queue
 */
struct hetbrq {
	u_int		size;		/* number of entries */
	u_int		thresh;		/* interrupt threshold */
	struct dmamem	mem;		/* memory */
	struct he_tbrqen *tbrq;
	u_int		head;		/* adapter end */
};

/*==================================================================*/

/*
 * TPDs are 32 byte and must be aligned on 64 byte boundaries. That means,
 * that half of the space is free. We use this space to plug in a link for
 * the list of free TPDs. Note, that the m_act member of the mbufs contain
 * a pointer to the dmamap.
 *
 * The maximum number of TDPs is the size of the common transmit packet
 * descriptor ready queue plus the sizes of the transmit buffer return queues
 * (currently only queue 0). We allocate and map these TPD when initializing
 * the card. We also allocate on DMA map for each TPD. Only the map in the
 * last TPD of a packets is used when a packet is transmitted.
 * This is signalled by having the mbuf member of this TPD non-zero and
 * pointing to the mbuf.
 */
#define HE_TPD_SIZE		64
struct tpd {
	struct he_tpd		tpd;	/* at beginning */
	SLIST_ENTRY(tpd)	link;	/* free cid list link */
	struct mbuf		*mbuf;	/* the buf chain */
	bus_dmamap_t		map;	/* map */
	uint32_t		cid;	/* CID */
	uint16_t		no;	/* number of this tpd */
};
SLIST_HEAD(tpd_list, tpd);

#define TPD_SET_USED(SC, I) do {				\
	(SC)->tpd_used[(I) / 8] |= (1 << ((I) % 8));		\
    } while (0)

#define TPD_CLR_USED(SC, I) do {				\
	(SC)->tpd_used[(I) / 8] &= ~(1 << ((I) % 8));		\
    } while (0)

#define TPD_TST_USED(SC, I) ((SC)->tpd_used[(I) / 8] & (1 << ((I) % 8)))

#define TPD_ADDR(SC, I) ((struct tpd *)((char *)sc->tpds.base +	\
    (I) * HE_TPD_SIZE))

/*==================================================================*/

/*
 * External MBUFs. The card needs a lot of mbufs in the pools for high
 * performance. The problem with using mbufs directly is that we would need
 * a dmamap for each of the mbufs. This can exhaust iommu space on the sparc
 * and it eats also a lot of processing time. So we use external mbufs
 * for the small buffers and clusters for the large buffers.
 * For receive group 0 we use 5 ATM cells, for group 1 one (52 byte) ATM
 * cell. The mbuf storage is allocated pagewise and one dmamap is used per
 * page.
 *
 * The handle we give to the card for the small buffers is a word combined
 * of the page number and the number of the chunk in the page. This restricts
 * the number of chunks per page to 256 (8 bit) and the number of pages to
 * 65536 (16 bits).
 *
 * A chunk may be in one of three states: free, on the card and floating around
 * in the system. If it is free, it is on one of the two free lists and
 * start with a struct mbufx_free. Each page has a bitmap that tracks where
 * its chunks are.
 *
 * For large buffers we use mbuf clusters. Here we have two problems: we need
 * to track the buffers on the card (in the case we want to stop it) and
 * we need to map the 64bit mbuf address to a 26bit handle for 64-bit machines.
 * The card uses the buffers in the order we give it to the card. Therefor
 * we can use a private array holding pointers to the mbufs as a circular
 * queue for both tasks. This is done with the lbufs member of softc. The
 * handle for these buffer is the lbufs index ored with a flag.
 */
#define MBUF0_SIZE	(5 * 48)	/* 240 */
#define MBUF1_SIZE	(52)

#define MBUF0_CHUNK	256		/* 16 free bytes */
#define MBUF1_CHUNK	96		/* 44 free bytes */
#ifdef XXX
#define MBUF0_OFFSET	(MBUF0_CHUNK - sizeof(struct mbuf_chunk_hdr) \
    - MBUF0_SIZE)
#else
#define MBUF0_OFFSET	0
#endif
#define MBUF1_OFFSET	(MBUF1_CHUNK - sizeof(struct mbuf_chunk_hdr) \
    - MBUF1_SIZE)
#define MBUFL_OFFSET	16		/* two pointers for HARP */

#define MBUF_ALLOC_SIZE	(PAGE_SIZE)

/* each allocated page has one of these structures at its very end. */
struct mbuf_page_hdr {
	uint8_t		card[32];	/* bitmap for on-card */
	uint16_t	nchunks;	/* chunks on this page */
	bus_dmamap_t	map;		/* the DMA MAP */
	uint32_t	phys;		/* physical base address */
	uint32_t	hdroff;		/* chunk header offset */
	uint32_t	chunksize;	/* chunk size */
};
struct mbuf_page {
	char	storage[MBUF_ALLOC_SIZE - sizeof(struct mbuf_page_hdr)];
	struct mbuf_page_hdr	hdr;
};

/* numbers per page */
#define MBUF0_PER_PAGE	((MBUF_ALLOC_SIZE - sizeof(struct mbuf_page_hdr)) / \
    MBUF0_CHUNK)
#define MBUF1_PER_PAGE	((MBUF_ALLOC_SIZE - sizeof(struct mbuf_page_hdr)) / \
    MBUF1_CHUNK)

#define MBUF_CLR_BIT(ARRAY, BIT) ((ARRAY)[(BIT) / 8] &= ~(1 << ((BIT) % 8)))
#define MBUF_SET_BIT(ARRAY, BIT) ((ARRAY)[(BIT) / 8] |= (1 << ((BIT) % 8)))
#define MBUF_TST_BIT(ARRAY, BIT) ((ARRAY)[(BIT) / 8] & (1 << ((BIT) % 8)))

#define MBUF_MAKE_HANDLE(PAGENO, CHUNKNO) \
	(((PAGENO) << 10) | (CHUNKNO))

#define MBUF_PARSE_HANDLE(HANDLE, PAGENO, CHUNKNO) do {	\
	(CHUNKNO) = (HANDLE) & 0x3ff;			\
	(PAGENO) = ((HANDLE) >> 10) & 0x3ff;		\
    } while (0)

#define MBUF_LARGE_FLAG	(1 << 20)

/* chunks have the following structure at the end (4 byte) */
struct mbuf_chunk_hdr {
	uint16_t		pageno;
	uint16_t		chunkno;
};

#define MBUFX_STORAGE_SIZE(X) (MBUF##X##_CHUNK	\
    - sizeof(struct mbuf_chunk_hdr))

struct mbuf0_chunk {
	char			storage[MBUFX_STORAGE_SIZE(0)];
	struct mbuf_chunk_hdr	hdr;
};

struct mbuf1_chunk {
	char			storage[MBUFX_STORAGE_SIZE(1)];
	struct mbuf_chunk_hdr	hdr;
};

struct mbufx_free {
	struct mbufx_free	*link;
};

/*==================================================================*/

/*
 * Interrupt queue
 */
struct heirq {
	u_int		size;	/* number of entries */
	u_int		thresh;	/* re-interrupt threshold */
	u_int		line;	/* interrupt line to use */
	struct dmamem	mem;	/* interrupt queues */
	uint32_t *	irq;	/* interrupt queue */
	uint32_t 	head;	/* head index */
	uint32_t *	tailp;	/* pointer to tail */
	struct hatm_softc *sc;	/* back pointer */
	u_int		group;	/* interrupt group */
};

/*
 * This structure describes all information for a VCC open on the card.
 * The array of these structures is indexed by the compressed connection ID
 * (CID). This structure must begin with the atmio_vcc.
 */
struct hevcc {
	struct atmio_vcc param;		/* traffic parameters */
	void *		rxhand;		/* NATM protocol block */
	u_int		vflags;		/* private flags */
	uint32_t	ipackets;
	uint32_t	opackets;
	uint32_t	ibytes;
	uint32_t	obytes;

	u_int		rc;		/* rate control group for CBR */
	struct mbuf *	chain;		/* partial received PDU */
	struct mbuf *	last;		/* last mbuf in chain */
	u_int		ntpds;		/* number of active TPDs */
};
#define HE_VCC_OPEN		0x000f0000
#define HE_VCC_RX_OPEN		0x00010000
#define HE_VCC_RX_CLOSING	0x00020000
#define HE_VCC_TX_OPEN		0x00040000
#define HE_VCC_TX_CLOSING	0x00080000
#define HE_VCC_FLOW_CTRL	0x00100000

/*
 * CBR rate groups
 */
struct herg {
	u_int	refcnt;		/* how many connections reference this group */
	u_int	rate;		/* the value */
};

/*
 * Softc
 */
struct hatm_softc {
	struct ifatm		ifatm;		/* common ATM stuff */
	struct mtx		mtx;		/* lock */
	struct ifmedia		media;		/* media */
	device_t		dev;		/* device */
	int			memid;		/* resoure id for memory */
	struct resource *	memres;		/* memory resource */
	bus_space_handle_t	memh;		/* handle */
	bus_space_tag_t		memt;		/* ... and tag */
	bus_dma_tag_t		parent_tag;	/* global restriction */
	struct cv		vcc_cv;		/* condition variable */
	int			irqid;		/* resource id */
	struct resource *	irqres;		/* resource */
	void *			ih;		/* interrupt handle */
	struct utopia		utopia;		/* utopia state */

	/* rest has to be reset by stop */
	int			he622;		/* this is a HE622 */
	int			pci64;		/* 64bit bus */
	char			prod_id[HE_EEPROM_PROD_ID_LEN + 1];
	char			rev[HE_EEPROM_REV_LEN + 1];
	struct heirq		irq_0;		/* interrupt queues 0 */

	/* generic network controller state */
	u_int			cells_per_row;
	u_int			bytes_per_row;
	u_int			r0_numrows;
	u_int			tx_numrows;
	u_int			r1_numrows;
	u_int			r0_startrow;
	u_int			tx_startrow;
	u_int			r1_startrow;
	u_int			cells_per_lbuf;
	u_int			r0_numbuffs;
	u_int			r1_numbuffs;
	u_int			tx_numbuffs;

	/* HSP */
	struct he_hsp		*hsp;
	struct dmamem		hsp_mem;

	/*** TX ***/
	struct hetbrq		tbrq;		/* TBRQ 0 */
	struct hetpdrq		tpdrq;		/* TPDRQ */
	struct tpd_list		tpd_free;	/* Free TPDs */
	u_int			tpd_nfree;	/* number of free TPDs */
	u_int			tpd_total;	/* total TPDs */
	uint8_t			*tpd_used;	/* bitmap of used TPDs */
	struct dmamem		tpds;		/* TPD memory */
	bus_dma_tag_t		tx_tag;		/* DMA tag for all tx mbufs */

	/*** RX ***/
	/* receive/transmit groups */
	struct herbp		rbp_s0;		/* RBPS0 */
	struct herbp		rbp_l0;		/* RBPL0 */
	struct herbp		rbp_s1;		/* RBPS1 */
	struct herbrq		rbrq_0;		/* RBRQ0 */
	struct herbrq		rbrq_1;		/* RBRQ1 */

	/* list of external mbuf storage */
	bus_dma_tag_t		mbuf_tag;
	struct mbuf_page	**mbuf_pages;
	u_int			mbuf_npages;
	struct mbufx_free	*mbuf_list[2];

	/* mbuf cluster tracking and mapping for group 0 */
	struct mbuf		**lbufs;	/* mbufs */
	bus_dmamap_t		*rmaps;		/* DMA maps */
	u_int			lbufs_size;
	u_int			lbufs_next;

	/* VCCs */
	struct hevcc		*vccs[HE_MAX_VCCS];
	u_int			cbr_bw;		/* BW allocated to CBR */
	u_int			max_tpd;	/* per VCC */
	u_int			open_vccs;
	uma_zone_t		vcc_zone;

	/* rate groups */
	struct herg		rate_ctrl[HE_REGN_CS_STPER];

	/* memory offsets */
	u_int			tsrb, tsrc, tsrd;
	u_int			rsrb;

	struct cv		cv_rcclose;	/* condition variable */
	uint32_t		rate_grid[16][16]; /* our copy */

	/* sysctl support */
	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;

	/* internal statistics */
	struct istats		istats;

#ifdef HATM_DEBUG
	/* debugging */
	u_int			debug;
#endif
};

#define READ4(SC,OFF)	bus_space_read_4(SC->memt, SC->memh, (OFF))
#define READ2(SC,OFF)	bus_space_read_2(SC->memt, SC->memh, (OFF))
#define READ1(SC,OFF)	bus_space_read_1(SC->memt, SC->memh, (OFF))

#define WRITE4(SC,OFF,VAL) bus_space_write_4(SC->memt, SC->memh, (OFF), (VAL))
#define WRITE2(SC,OFF,VAL) bus_space_write_2(SC->memt, SC->memh, (OFF), (VAL))
#define WRITE1(SC,OFF,VAL) bus_space_write_1(SC->memt, SC->memh, (OFF), (VAL))

#define BARRIER_R(SC) bus_space_barrier(SC->memt, SC->memh, 0, HE_REGO_END, \
	BUS_SPACE_BARRIER_READ)
#define BARRIER_W(SC) bus_space_barrier(SC->memt, SC->memh, 0, HE_REGO_END, \
	BUS_SPACE_BARRIER_WRITE)
#define BARRIER_RW(SC) bus_space_barrier(SC->memt, SC->memh, 0, HE_REGO_END, \
	BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE)

#define READ_SUNI(SC,OFF) READ4(SC, HE_REGO_SUNI + 4 * (OFF))
#define WRITE_SUNI(SC,OFF,VAL) WRITE4(SC, HE_REGO_SUNI + 4 * (OFF), (VAL))

#define READ_LB4(SC,OFF)						\
    ({									\
	WRITE4(SC, HE_REGO_LB_MEM_ADDR, (OFF));				\
	WRITE4(SC, HE_REGO_LB_MEM_ACCESS,				\
	    (HE_REGM_LB_MEM_HNDSHK | HE_REGM_LB_MEM_READ));		\
	while((READ4(SC, HE_REGO_LB_MEM_ACCESS) & HE_REGM_LB_MEM_HNDSHK))\
		;							\
	READ4(SC, HE_REGO_LB_MEM_DATA);					\
    })
#define WRITE_LB4(SC,OFF,VAL)						\
    do {								\
	WRITE4(SC, HE_REGO_LB_MEM_ADDR, (OFF));				\
	WRITE4(SC, HE_REGO_LB_MEM_DATA, (VAL));				\
	WRITE4(SC, HE_REGO_LB_MEM_ACCESS,				\
	    (HE_REGM_LB_MEM_HNDSHK | HE_REGM_LB_MEM_WRITE));		\
	while((READ4(SC, HE_REGO_LB_MEM_ACCESS) & HE_REGM_LB_MEM_HNDSHK))\
		;							\
    } while(0)

#define WRITE_MEM4(SC,OFF,VAL,SPACE)					\
    do {								\
	WRITE4(SC, HE_REGO_CON_DAT, (VAL));				\
	WRITE4(SC, HE_REGO_CON_CTL,					\
	    (SPACE | HE_REGM_CON_WE | HE_REGM_CON_STATUS | (OFF)));	\
	while((READ4(SC, HE_REGO_CON_CTL) & HE_REGM_CON_STATUS) != 0)	\
		;							\
    } while(0)

#define READ_MEM4(SC,OFF,SPACE)					\
    ({									\
	WRITE4(SC, HE_REGO_CON_CTL,					\
	    (SPACE | HE_REGM_CON_STATUS | (OFF)));			\
	while((READ4(SC, HE_REGO_CON_CTL) & HE_REGM_CON_STATUS) != 0)	\
		;							\
	READ4(SC, HE_REGO_CON_DAT);					\
    })

#define WRITE_TCM4(SC,OFF,VAL) WRITE_MEM4(SC,(OFF),(VAL),HE_REGM_CON_TCM)
#define WRITE_RCM4(SC,OFF,VAL) WRITE_MEM4(SC,(OFF),(VAL),HE_REGM_CON_RCM)
#define WRITE_MBOX4(SC,OFF,VAL) WRITE_MEM4(SC,(OFF),(VAL),HE_REGM_CON_MBOX)

#define READ_TCM4(SC,OFF) READ_MEM4(SC,(OFF),HE_REGM_CON_TCM)
#define READ_RCM4(SC,OFF) READ_MEM4(SC,(OFF),HE_REGM_CON_RCM)
#define READ_MBOX4(SC,OFF) READ_MEM4(SC,(OFF),HE_REGM_CON_MBOX)

#define WRITE_TCM(SC,OFF,BYTES,VAL) 					\
	WRITE_MEM4(SC,(OFF) | ((~(BYTES) & 0xf) << HE_REGS_CON_DIS),	\
	    (VAL), HE_REGM_CON_TCM)
#define WRITE_RCM(SC,OFF,BYTES,VAL) 					\
	WRITE_MEM4(SC,(OFF) | ((~(BYTES) & 0xf) << HE_REGS_CON_DIS),	\
	    (VAL), HE_REGM_CON_RCM)

#define READ_TSR(SC,CID,NR)						\
    ({									\
	uint32_t _v;							\
	if((NR) <= 7) {							\
		_v = READ_TCM4(SC, HE_REGO_TSRA(0,CID,NR));		\
	} else if((NR) <= 11) {						\
		_v = READ_TCM4(SC, HE_REGO_TSRB((SC)->tsrb,CID,(NR-8)));\
	} else if((NR) <= 13) {						\
		_v = READ_TCM4(SC, HE_REGO_TSRC((SC)->tsrc,CID,(NR-12)));\
	} else {							\
		_v = READ_TCM4(SC, HE_REGO_TSRD((SC)->tsrd,CID));	\
	}								\
	_v;								\
    })

#define WRITE_TSR(SC,CID,NR,BEN,VAL)					\
    do {								\
	if((NR) <= 7) {							\
		WRITE_TCM(SC, HE_REGO_TSRA(0,CID,NR),BEN,VAL);		\
	} else if((NR) <= 11) {						\
		WRITE_TCM(SC, HE_REGO_TSRB((SC)->tsrb,CID,(NR-8)),BEN,VAL);\
	} else if((NR) <= 13) {						\
		WRITE_TCM(SC, HE_REGO_TSRC((SC)->tsrc,CID,(NR-12)),BEN,VAL);\
	} else {							\
		WRITE_TCM(SC, HE_REGO_TSRD((SC)->tsrd,CID),BEN,VAL);	\
	}								\
    } while(0)

#define READ_RSR(SC,CID,NR)						\
    ({									\
	uint32_t _v;							\
	if((NR) <= 7) {							\
		_v = READ_RCM4(SC, HE_REGO_RSRA(0,CID,NR));		\
	} else {							\
		_v = READ_RCM4(SC, HE_REGO_RSRB((SC)->rsrb,CID,(NR-8)));\
	}								\
	_v;								\
    })

#define WRITE_RSR(SC,CID,NR,BEN,VAL)					\
    do {								\
	if((NR) <= 7) {							\
		WRITE_RCM(SC, HE_REGO_RSRA(0,CID,NR),BEN,VAL);		\
	} else {							\
		WRITE_RCM(SC, HE_REGO_RSRB((SC)->rsrb,CID,(NR-8)),BEN,VAL);\
	}								\
    } while(0)

#ifdef HATM_DEBUG
#define DBG(SC, FL, PRINT) do {						\
	if((SC)->debug & DBG_##FL) { 					\
		if_printf(&(SC)->ifatm.ifnet, "%s: ", __func__);	\
		printf PRINT;						\
		printf("\n");						\
	}								\
    } while (0)

enum {
	DBG_RX		= 0x0001,
	DBG_TX		= 0x0002,
	DBG_VCC		= 0x0004,
	DBG_IOCTL	= 0x0008,
	DBG_ATTACH	= 0x0010,
	DBG_INTR	= 0x0020,
	DBG_DMA		= 0x0040,
	DBG_DMAH	= 0x0080,

	DBG_ALL		= 0x00ff
};

#else
#define DBG(SC, FL, PRINT)
#endif

u_int hatm_cps2atmf(uint32_t);
u_int hatm_atmf2cps(uint32_t);

void hatm_intr(void *);
int hatm_ioctl(struct ifnet *, u_long, caddr_t);
void hatm_initialize(struct hatm_softc *);
void hatm_stop(struct hatm_softc *sc);
void hatm_start(struct ifnet *);

void hatm_rx(struct hatm_softc *sc, u_int cid, u_int flags, struct mbuf *m,
    u_int len);
void hatm_tx_complete(struct hatm_softc *sc, struct tpd *tpd, uint32_t);

int hatm_tx_vcc_can_open(struct hatm_softc *sc, u_int cid, struct hevcc *);
void hatm_tx_vcc_open(struct hatm_softc *sc, u_int cid);
void hatm_rx_vcc_open(struct hatm_softc *sc, u_int cid);
void hatm_tx_vcc_close(struct hatm_softc *sc, u_int cid);
void hatm_rx_vcc_close(struct hatm_softc *sc, u_int cid);
void hatm_tx_vcc_closed(struct hatm_softc *sc, u_int cid);
void hatm_vcc_closed(struct hatm_softc *sc, u_int cid);
void hatm_load_vc(struct hatm_softc *sc, u_int cid, int reopen);
