/*
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
 * Driver for IDT77252 (ABR) based cards like ProSum's.
 */

/* legal values are 0, 1, 2 and 8 */
#define	PATM_VPI_BITS	2
#define	PATM_CFG_VPI	IDT_CFG_VP2

/* receive status queue size */
#define	PATM_RSQ_SIZE		512
#define	PATM_CFQ_RSQ_SIZE	IDT_CFG_RXQ512	

/* alignment for SQ memory */
#define	PATM_SQ_ALIGNMENT	8192

#define	PATM_PROATM_NAME_OFFSET	060
#define	PATM_PROATM_NAME	"PROATM"
#define	PATM_PROATM_MAC_OFFSET	044
#define	PATM_IDT_MAC_OFFSET	0154

/* maximum number of packets on UBR queue */
#define	PATM_DLFT_MAXQ		1000

/* maximum number of packets on other queues. This should depend on the
 * traffic contract. */
#define	PATM_TX_IFQLEN		100

/*
 * Maximum number of DMA maps we allocate. This is the minimum that can be
 * set larger via a sysctl.
 * Starting number of DMA maps.
 * Step for growing.
 */
#define	PATM_CFG_TXMAPS_MAX	1024
#define	PATM_CFG_TXMAPS_INIT	128
#define	PATM_CFG_TXMAPS_STEP	128

/* percents of TST slots to keep for non-CBR traffic */
#define	PATM_TST_RESERVE	2

/*
 * Structure to hold TX DMA maps
 */
struct patm_txmap {
	SLIST_ENTRY(patm_txmap) link;
	bus_dmamap_t	map;
};

/*
 * Receive buffers.
 *
 * We manage our own external mbufs for small receive buffers for two reasons:
 * the card may consume a rather large number of buffers. Mapping each buffer
 * would consume a lot of iospace on sparc64. Also the card allows us to set
 * a 32-bit handle for identification of the buffers. On a 64-bit system this
 * requires us to use a mapping between buffers and handles.
 *
 * For large buffers we use mbuf clusters directly. We track these by using
 * an array of pointers (lbufs) to special structs and a free list of these
 * structs.
 *
 * For AAL0 cell we use FBQ2 and make the 1 cell long.
 */
/*
 * Define the small buffer chunk so that we have at least 16 byte free
 * at the end of the chunk and that there is an integral number of chunks
 * in a page.
 */
#define	SMBUF_PAGE_SIZE		16384	/* 16k pages */
#define	SMBUF_MAX_PAGES		64	/* maximum number of pages */
#define	SMBUF_CHUNK_SIZE	256	/* 256 bytes per chunk */
#define	SMBUF_CELLS		5
#define	SMBUF_SIZE		(SMBUF_CELLS * 48)
#define	SMBUF_THRESHOLD		9	/* 9/16 of queue size */
#define	SMBUF_NI_THRESH		3
#define	SMBUF_CI_THRESH		1

#define	VMBUF_PAGE_SIZE		16384	/* 16k pages */
#define	VMBUF_MAX_PAGES		16	/* maximum number of pages */
#define	VMBUF_CHUNK_SIZE	64	/* 64 bytes per chunk */
#define	VMBUF_CELLS		1
#define	VMBUF_SIZE		(VMBUF_CELLS * 48)
#define	VMBUF_THRESHOLD		15	/* 15/16 of size */

#define	SMBUF_OFFSET	(SMBUF_CHUNK_SIZE - 8 - SMBUF_SIZE)
#define	VMBUF_OFFSET	0

#define	MBUF_SHANDLE	0x00000000
#define	MBUF_LHANDLE	0x80000000
#define	MBUF_VHANDLE	0x40000000
#define	MBUF_HMASK	0x3fffffff

/*
 * Large buffers
 *
 * The problem with these is the maximum count. When the card assembles
 * a AAL5 pdu it moves a buffer from the FBQ to the VC. This frees space
 * in the FBQ, put the buffer may pend on the card for an unlimited amount
 * of time (we don't idle connections). This means that the upper limit
 * on buffers on the card may be (no-of-open-vcs + FBQ_SIZE). Because
 * this is far too much, make this a tuneable. We could also make
 * this dynamic by allocating pages of several lbufs at once during run time.
 */
#define	LMBUF_MAX		(IDT_FBQ_SIZE * 2)
#define	LMBUF_CELLS		(MCLBYTES / 48)	/* 42 cells = 2048 byte */
#define	LMBUF_SIZE		(LMBUF_CELLS * 48)
#define	LMBUF_THRESHOLD		9	/* 9/16 of queue size */
#define	LMBUF_OFFSET		(MCLBYTES - LMBUF_SIZE)
#define	LMBUF_NI_THRESH		3
#define	LMBUF_CI_THRESH		1

#define	LMBUF_HANDLE		0x80000000

struct lmbuf {
	SLIST_ENTRY(lmbuf)	link;	/* free list link */
	bus_dmamap_t		map;	/* DMA map */
	u_int			handle;	/* this is the handle index */
	struct mbuf		*m;	/* the current mbuf */
	bus_addr_t		phy;	/* phy addr */
};

#define	PATM_CID(SC, VPI, VCI)	\
    (((VPI) << (SC)->ifatm.mib.vci_bits) | (VCI))

/*
 * Internal driver statistics
 */
struct patm_stats {
	uint32_t	raw_cells;
	uint32_t	raw_no_vcc;
	uint32_t	raw_no_buf;
	uint32_t	tx_qfull;
	uint32_t	tx_out_of_tbds;
	uint32_t	tx_out_of_maps;
	uint32_t	tx_load_err;
};

/*
 * These are allocated as DMA able memory
 */
struct patm_scd {
	struct idt_tbd	scq[IDT_SCQ_SIZE];
	LIST_ENTRY(patm_scd) link;	/* all active SCDs */
	uint32_t	sram;		/* SRAM address */
	bus_addr_t	phy;		/* physical address */
	bus_dmamap_t	map;		/* DMA map */
	u_int		tail;		/* next free entry for host */
	int		space;		/* number of free entries (minus one) */
	u_int		slots;		/* CBR slots allocated */
	uint8_t		tag;		/* next tag for TSI */
	uint8_t		last_tag;	/* last tag checked in interrupt */
	uint8_t		num_on_card;	/* number of PDUs on tx queue */
	uint8_t		lacr;		/* LogACR value */
	uint8_t		init_er;	/* LogER value */
	struct ifqueue	q;		/* queue of packets */
	struct mbuf	*on_card[IDT_TSQE_TAG_SPACE];
};

/*
 * Per-VCC data
 */
struct patm_vcc {
	struct atmio_vcc vcc;		/* caller's parameters */
	void		*rxhand;	/* NATM handle */
	u_int		vflags;		/* open and other flags */
	uint32_t	ipackets;	/* packets received */
	uint32_t	opackets;	/* packets sent */
	uint64_t	ibytes;		/* bytes received */
	uint64_t	obytes;		/* bytes sent */
	struct mbuf	*chain;		/* currently received chain */
	struct mbuf	*last;		/* end of chain */
	u_int		cid;		/* index */
	u_int		cps;		/* last ABR cps */
	struct patm_scd	*scd;
};
#define	PATM_VCC_TX_OPEN	0x0001
#define	PATM_VCC_RX_OPEN	0x0002
#define	PATM_VCC_TX_CLOSING	0x0004
#define	PATM_VCC_RX_CLOSING	0x0008
#define	PATM_VCC_OPEN		0x000f	/* all the above */
#define	PATM_VCC_ASYNC		0x0010

#define	PATM_RAW_CELL		0x0000	/* 53 byte cells */
#define	PATM_RAW_NOHEC		0x0100	/* 52 byte cells */
#define	PATM_RAW_CS		0x0200	/* 64 byte cell stream */
#define	PATM_RAW_FORMAT		0x0300	/* format mask */

/*
 * Per adapter data
 */
struct patm_softc {
	struct ifatm		ifatm;		/* common ATM stuff */
	struct mtx		mtx;		/* lock */
	struct ifmedia		media;		/* media */
	device_t		dev;		/* device */
	struct resource *	memres;		/* memory resource */
	bus_space_handle_t	memh;		/* handle */
	bus_space_tag_t		memt;		/* ... and tag */
	int			irqid;		/* resource id */
	struct resource *	irqres;		/* resource */
	void *			ih;		/* interrupt handle */
	struct utopia		utopia;		/* phy state */
	const struct idt_mmap	*mmap;		/* SRAM memory map */
	u_int			flags;		/* see below */
	u_int			revision;	/* chip revision */

	/* DMAable status queue memory */
	size_t			sq_size;	/* size of memory area */
	bus_dma_tag_t		sq_tag;		/* DMA tag */
	bus_dmamap_t		sq_map;		/* map */

	bus_addr_t		tsq_phy;	/* phys addr. */
	struct idt_tsqe		*tsq;		/* transmit status queue */
	struct idt_tsqe		*tsq_next;	/* last processed entry */
	struct idt_rsqe		*rsq;		/* receive status queue */
	bus_addr_t		rsq_phy;	/* phys addr. */
	u_int			rsq_last;	/* last processed entry */
	struct idt_rawhnd	*rawhnd;	/* raw cell handle */
	bus_addr_t		rawhnd_phy;	/* phys addr. */

	/* TST */
	u_int			tst_state;	/* active TST and others */
	u_int			tst_jump[2];	/* address of the jumps */
	u_int			tst_base[2];	/* base address of TST */
	u_int			*tst_soft;	/* soft TST */
	struct mtx		tst_lock;
	struct callout		tst_callout;
	u_int			tst_free;	/* free slots */
	u_int			tst_reserve;	/* non-CBR reserve */
	u_int			bwrem;		/* remaining bandwith */

	/* sysctl support */
	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;

	/* EEPROM contents */
	uint8_t			eeprom[256];

	/* large buffer mapping */
	bus_dma_tag_t		lbuf_tag;	/* DMA tag */
	u_int			lbuf_max;	/* maximum number */
	struct lmbuf		*lbufs;		/* array for indexing */
	SLIST_HEAD(,lmbuf)	lbuf_free_list;	/* free list */

	/* small buffer handling */
	bus_dma_tag_t		sbuf_tag;	/* DMA tag */
	struct mbpool		*sbuf_pool;	/* pool */
	struct mbpool		*vbuf_pool;	/* pool */

	/* raw cell queue */
	struct lmbuf		*rawh;		/* current header buf */
	u_int			rawi;		/* cell index into buffer */

	/* statistics */
	struct patm_stats	stats;		/* statistics */

	/* Vccs */
	struct patm_vcc		**vccs;		/* channel pointer array */
	u_int			vccs_open;	/* number of open channels */
	uma_zone_t		vcc_zone;
	struct cv		vcc_cv;

	/* SCDs */
	uint32_t		scd_free;	/* SRAM of first free SCD */
	bus_dma_tag_t		scd_tag;
	struct patm_scd		*scd0;
	LIST_HEAD(, patm_scd)	scd_list;	/* list of all active SCDs */

	/* Tx */
	bus_dma_tag_t		tx_tag;		/* for transmission */
	SLIST_HEAD(, patm_txmap) tx_maps_free;	/* free maps */
	u_int			tx_nmaps;	/* allocated maps */
	u_int			tx_maxmaps;	/* maximum number */
	struct uma_zone		*tx_mapzone;	/* zone for maps */

#ifdef PATM_DEBUG
	/* debugging */
	u_int			debug;
#endif
};

/* flags */
#define	PATM_25M	0x0001		/* 25MBit card */
#define	PATM_SBUFW	0x0002		/* warned */
#define	PATM_VBUFW	0x0004		/* warned */
#define	PATM_UNASS	0x0010		/* unassigned cells */

#define	PATM_CLR	0x0007		/* clear on stop */

/* tst - uses unused fields */
#define	TST_BOTH	0x03000000
#define	TST_CH0		0x01000000
#define	TST_CH1		0x02000000
/* tst_state */
#define	TST_ACT1	0x0001		/* active TST */
#define	TST_PENDING	0x0002		/* need update */
#define	TST_WAIT	0x0004		/* wait fo jump */

#define	patm_printf(SC, ...)	if_printf(&(SC)->ifatm.ifnet, __VA_ARGS__);

#ifdef PATM_DEBUG
/*
 * Debugging
 */
enum {
	DBG_ATTACH	= 0x0001,	/* attaching the card */
	DBG_INTR	= 0x0002,	/* interrupts */
	DBG_REG		= 0x0004,	/* register access */
	DBG_SRAM	= 0x0008,	/* SRAM access */
	DBG_PHY		= 0x0010,	/* PHY access */
	DBG_IOCTL	= 0x0020,	/* ioctl */
	DBG_FREEQ	= 0x0040,	/* free bufq supply */
	DBG_VCC		= 0x0080,	/* open/close */
	DBG_TX		= 0x0100,	/* transmission */
	DBG_TST		= 0x0200,	/* TST */

	DBG_ALL		= 0xffff
};

#define	patm_debug(SC, FLAG, ...) do {					\
	if((SC)->debug & DBG_##FLAG) { 					\
		if_printf(&(SC)->ifatm.ifnet, "%s: ", __func__);	\
		printf(__VA_ARGS__);					\
		printf("\n");						\
	}								\
    } while (0)
#else

#define patm_debug(SC, FLAG, ...) do { } while (0)

#endif

/* start output */
void patm_start(struct ifnet *);

/* ioctl handler */
int patm_ioctl(struct ifnet *, u_long, caddr_t);

/* start the interface */
void patm_init(void *);

/* start the interface with the lock held */
void patm_initialize(struct patm_softc *);

/* stop the interface */
void patm_stop(struct patm_softc *);

/* software reset of interface */
void patm_reset(struct patm_softc *);

/* interrupt handler */
void patm_intr(void *);

/* check RSQ */
void patm_intr_rsq(struct patm_softc *sc);

/* close the given vcc for transmission */
void patm_tx_vcc_close(struct patm_softc *, struct patm_vcc *);

/* close the given vcc for receive */
void patm_rx_vcc_close(struct patm_softc *, struct patm_vcc *);

/* transmission side finally closed */
void patm_tx_vcc_closed(struct patm_softc *, struct patm_vcc *);

/* receive side finally closed */
void patm_rx_vcc_closed(struct patm_softc *, struct patm_vcc *);

/* vcc closed */
void patm_vcc_closed(struct patm_softc *, struct patm_vcc *);

/* check if we can open this one */
int patm_tx_vcc_can_open(struct patm_softc *, struct patm_vcc *);

/* check if we can open this one */
int patm_rx_vcc_can_open(struct patm_softc *, struct patm_vcc *);

/* open it */
void patm_tx_vcc_open(struct patm_softc *, struct patm_vcc *);

/* open it */
void patm_rx_vcc_open(struct patm_softc *, struct patm_vcc *);

/* receive packet */
void patm_rx(struct patm_softc *, struct idt_rsqe *);

/* packet transmitted */
void patm_tx(struct patm_softc *, u_int, u_int);

/* VBR connection went idle */
void patm_tx_idle(struct patm_softc *, u_int);

/* allocate an SCQ */
struct patm_scd *patm_scd_alloc(struct patm_softc *);

/* free an SCD */
void patm_scd_free(struct patm_softc *sc, struct patm_scd *scd);

/* setup SCD in SRAM */
void patm_scd_setup(struct patm_softc *sc, struct patm_scd *scd);

/* setup TCT entry in SRAM */
void patm_tct_setup(struct patm_softc *, struct patm_scd *, struct patm_vcc *);

/* free a large buffer */
void patm_lbuf_free(struct patm_softc *sc, struct lmbuf *b);

/* Process the raw cell at the given address */
void patm_rx_raw(struct patm_softc *sc, u_char *cell);

/* load a one segment DMA map */
void patm_load_callback(void *, bus_dma_segment_t *, int, int);

/* network operation register access */
static __inline uint32_t
patm_nor_read(struct patm_softc *sc, u_int reg)
{
	uint32_t val;

	val = bus_space_read_4(sc->memt, sc->memh, reg);
	patm_debug(sc, REG, "reg(0x%x)=%04x", reg, val);
	return (val);
}
static __inline void
patm_nor_write(struct patm_softc *sc, u_int reg, uint32_t val)
{

	patm_debug(sc, REG, "reg(0x%x)=%04x", reg, val);
	bus_space_write_4(sc->memt, sc->memh, reg, val);
}

/* Execute command */
static __inline void
patm_cmd_wait(struct patm_softc *sc)
{
	while (patm_nor_read(sc, IDT_NOR_STAT) & IDT_STAT_CMDBZ)
		;
}
static __inline void
patm_cmd_exec(struct patm_softc *sc, uint32_t cmd)
{
	patm_cmd_wait(sc);
	patm_nor_write(sc, IDT_NOR_CMD, cmd);
}

/* Read/write SRAM at the given word address. */
static __inline uint32_t
patm_sram_read(struct patm_softc *sc, u_int addr)
{
	uint32_t val;

	patm_cmd_exec(sc, IDT_MKCMD_RSRAM(addr));
	patm_cmd_wait(sc);
	val = patm_nor_read(sc, IDT_NOR_D0);
	patm_debug(sc, SRAM, "read %04x=%08x", addr, val);
	return (val);
}
static __inline void
patm_sram_write(struct patm_softc *sc, u_int addr, uint32_t val)
{
	patm_debug(sc, SRAM, "write %04x=%08x", addr, val);
	patm_cmd_wait(sc);
	patm_nor_write(sc, IDT_NOR_D0, val);
	patm_cmd_exec(sc, IDT_MKCMD_WSRAM(addr, 0));
}
static __inline void
patm_sram_write4(struct patm_softc *sc, u_int addr, uint32_t v0, uint32_t v1,
    uint32_t v2, uint32_t v3)
{
	patm_debug(sc, SRAM, "write %04x=%08x,%08x,%08x,%08x",
	    addr, v0, v1, v2, v3);
	patm_cmd_wait(sc);
	patm_nor_write(sc, IDT_NOR_D0, v0);
	patm_nor_write(sc, IDT_NOR_D1, v1);
	patm_nor_write(sc, IDT_NOR_D2, v2);
	patm_nor_write(sc, IDT_NOR_D3, v3);
	patm_cmd_exec(sc, IDT_MKCMD_WSRAM(addr, 3));
}

#define	LEGAL_VPI(SC, VPI) \
	(((VPI) & ~((1 << (SC)->ifatm.mib.vpi_bits) - 1)) == 0)
#define	LEGAL_VCI(SC, VCI) \
	(((VCI) & ~((1 << (SC)->ifatm.mib.vci_bits) - 1)) == 0)

extern const uint32_t patm_rtables155[];
extern const uint32_t patm_rtables25[];
extern const u_int patm_rtables_size;
extern const u_int patm_rtables_ntab;
