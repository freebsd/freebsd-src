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
 * Fore PCA200E driver definitions.
 */
/*
 * Debug statistics of the PCA200 driver
 */
struct istats {
	uint32_t	cmd_queue_full;
	uint32_t	get_stat_errors;
	uint32_t	clr_stat_errors;
	uint32_t	get_prom_errors;
	uint32_t	suni_reg_errors;
	uint32_t	tx_queue_full;
	uint32_t	tx_queue_almost_full;
	uint32_t	tx_pdu2big;
	uint32_t	tx_too_many_segs;
	uint32_t	tx_retry;
	uint32_t	fix_empty;
	uint32_t	fix_addr_copy;
	uint32_t	fix_addr_noext;
	uint32_t	fix_addr_ext;
	uint32_t	fix_len_noext;
	uint32_t	fix_len_copy;
	uint32_t	fix_len;
	uint32_t	rx_badvc;
	uint32_t	rx_closed;
};

/*
 * Addresses on the on-board RAM are expressed as offsets to the
 * start of that RAM.
 */
typedef uint32_t cardoff_t;

/*
 * The card uses a number of queues for communication with the host.
 * Parts of the queue are located on the card (pointers to the status
 * word and the ioblk and the command blocks), the rest in host memory.
 * Each of these queues forms a ring, where the head and tail pointers are
 * managed * either by the card or the host. For the receive queue the
 * head is managed by the card (and not used altogether by the host) and the
 * tail by the host - for all other queues its the other way around.
 * The host resident parts of the queue entries contain pointers to
 * the host resident status and the host resident ioblk (the latter not for
 * the command queue) as well as DMA addresses for supply to the card.
 */
struct fqelem {
	cardoff_t	card;		/* corresponding element on card */
	bus_addr_t	card_ioblk;	/* ioblk address to supply to card */
	volatile uint32_t *statp;		/* host status pointer */
	void		*ioblk;		/* host ioblk (not for commands) */
};

struct fqueue {
	struct fqelem	*chunk;		/* pointer to the element array */
	int		head;		/* queue head */
	int		tail;		/* queue tail */
};

/*
 * Queue manipulation macros
 */
#define	NEXT_QUEUE_ENTRY(HEAD,LEN) ((HEAD) = ((HEAD) + 1) % LEN)
#define	GET_QUEUE(Q,TYPE,IDX)    (&((TYPE *)(Q).chunk)[(IDX)])

/*
 * Now define structures for the different queues. Each of these structures
 * must start with a struct fqelem.
 */
struct txqueue {		/* transmit queue element */
	struct fqelem	q;
	struct mbuf	*m;	/* the chain we are transmitting */
	bus_dmamap_t	map;	/* map for the packet */
};

struct rxqueue {		/* receive queue element */
	struct fqelem	q;
};

struct supqueue {		/* supply queue element */
	struct fqelem	q;
};

struct cmdqueue;
struct fatm_softc;

typedef void (*completion_cb)(struct fatm_softc *, struct cmdqueue *);

struct cmdqueue {		/* command queue element */
	struct fqelem	q;
	completion_cb	cb;	/* call on command completion */
	int		error;	/* set if error occured */
};

/*
 * Card-DMA-able memory is managed by means of the bus_dma* functions.
 * To allocate a chunk of memory with a specific size and alignment one
 * has to:
 *	1. create a DMA tag
 *	2. allocate the memory
 *	3. load the memory into a map.
 * This finally gives the physical address that can be given to the card.
 * The card can DMA the entire 32-bit space without boundaries. We assume,
 * that all the allocations can be mapped in one contiguous segment. This
 * may be wrong in the future if we have more than 32 bit addresses.
 * Allocation is done at attach time and managed by the following structure.
 *
 * This could be done easier with the NetBSD bus_dma* functions. They appear
 * to be more useful and consistent.
 */
struct fatm_mem {
	u_int		size;		/* size */
	u_int		align;		/* alignment */
	bus_dma_tag_t	dmat;		/* DMA tag */
	void 		*mem;		/* memory block */
	bus_addr_t	paddr;		/* pysical address */
	bus_dmamap_t	map;		/* map */
};

/*
 * Each of these structures describes one receive buffer while the buffer
 * is on the card or in the receive return queue. These structures are
 * allocated at initialisation time together with the DMA maps. The handle that
 * is given to the card is the index into the array of these structures.
 */
struct rbuf {
	struct mbuf	*m;	/* the mbuf while we are on the card */
	bus_dmamap_t	map;	/* the map */
	LIST_ENTRY(rbuf) link;	/* the free list link */
};
LIST_HEAD(rbuf_list, rbuf);

/*
 * The driver maintains a list of all open VCCs. Because we
 * use only VPI=0 and a maximum VCI of 1024, the list is rather an array
 * than a list. We also store the atm pseudoheader flags here and the
 * rxhand (aka. protocol block).
 */
struct card_vcc {
	void		*rxhand;
	uint32_t	pcr;
	uint32_t	flags;
	uint8_t		aal;
	uint8_t		traffic;
};

#define	FATM_VCC_OPEN		0x00010000	/* is open */
#define	FATM_VCC_TRY_OPEN	0x00020000	/* is currently opening */
#define	FATM_VCC_TRY_CLOSE	0x00040000	/* is currently closing */
#define	FATM_VCC_BUSY		0x00070000	/* one of the above */

/*
 * Finally the softc structure
 */
struct fatm_softc {
	struct ifatm	ifatm;		/* common part */
	struct mtx	mtx;		/* lock this structure */
	struct ifmedia	media;		/* media */

	int		init_state;	/* initialisation step */
	int		memid;		/* resource id for card memory */
	struct resource *memres;	/* resource for card memory */
	bus_space_handle_t memh;	/* handle for card memory */
	bus_space_tag_t	memt;		/* tag for card memory */
	int		irqid;		/* resource id for interrupt */
	struct resource *irqres;	/* resource for interrupt */
	void		*ih;		/* interrupt handler */

	bus_dma_tag_t	parent_dmat;	/* parent DMA tag */
	struct fatm_mem	stat_mem;	/* memory for status blocks */
	struct fatm_mem	txq_mem;	/* TX descriptor queue */
	struct fatm_mem	rxq_mem;	/* RX descriptor queue */
	struct fatm_mem	s1q_mem;	/* Small buffer 1 queue */
	struct fatm_mem	l1q_mem;	/* Large buffer 1 queue */
	struct fatm_mem	prom_mem;	/* PROM memory */

	struct fqueue	txqueue;	/* transmission queue */
	struct fqueue	rxqueue;	/* receive queue */
	struct fqueue	s1queue;	/* SMALL S1 queue */
	struct fqueue	l1queue;	/* LARGE S1 queue */
	struct fqueue	cmdqueue;	/* command queue */

	/* fields for access to the SUNI registers */
	struct fatm_mem	reg_mem;	/* DMAable memory for readregs */
	struct cv	cv_regs;	/* to serialize access to reg_mem */

	/* fields for access to statistics */
	struct fatm_mem	sadi_mem;	/* sadistics memory */
	struct cv	cv_stat;	/* to serialize access to sadi_mem */

	u_int		flags;
#define	FATM_STAT_INUSE	0x0001
#define	FATM_REGS_INUSE	0x0002
	u_int		txcnt;		/* number of used transmit desc */
	int		retry_tx;	/* keep mbufs in queue if full */

	struct card_vcc	*vccs;		/* table of vccs */
	int		open_vccs;	/* number of vccs in use */
	int		small_cnt;	/* number of buffers owned by card */
	int		large_cnt;	/* number of buffers owned by card */

	/* receiving */
	struct rbuf	*rbufs;		/* rbuf array */
	struct rbuf_list rbuf_free;	/* free rbufs list */
	struct rbuf_list rbuf_used;	/* used rbufs list */
	u_int		rbuf_total;	/* total number of buffs */
	bus_dma_tag_t	rbuf_tag;	/* tag for rbuf mapping */

	/* transmission */
	bus_dma_tag_t	tx_tag;		/* transmission tag */

	uint32_t	heartbeat;	/* last heartbeat */
	u_int		stop_cnt;	/* how many times checked */

	struct istats	istats;		/* internal statistics */

	/* SUNI state */
	struct utopia	utopia;

	/* sysctl support */
	struct sysctl_ctx_list sysctl_ctx;
	struct sysctl_oid *sysctl_tree;

#ifdef FATM_DEBUG
	/* debugging */
	u_int		debug;
#endif
};

#ifndef FATM_DEBUG
#define	FATM_LOCK(SC)		mtx_lock(&(SC)->mtx)
#define	FATM_UNLOCK(SC)		mtx_unlock(&(SC)->mtx)
#else
#define	FATM_LOCK(SC)	do {					\
	DBG(SC, LOCK, ("locking in line %d", __LINE__));	\
	mtx_lock(&(SC)->mtx);					\
    } while (0)
#define	FATM_UNLOCK(SC)	do {					\
	DBG(SC, LOCK, ("unlocking in line %d", __LINE__));	\
	mtx_unlock(&(SC)->mtx);					\
    } while (0)
#endif
#define	FATM_CHECKLOCK(SC)	mtx_assert(&sc->mtx, MA_OWNED)

/*
 * Macros to access host memory fields that are also access by the card.
 * These fields need to little-endian always.
 */
#define	H_GETSTAT(STATP)	(le32toh(*(STATP)))
#define	H_SETSTAT(STATP, S)	do { *(STATP) = htole32(S); } while (0)
#define	H_SETDESC(DESC, D)	do { (DESC) = htole32(D); } while (0)

#ifdef notyet
#define	H_SYNCSTAT_POSTREAD(SC, P)					\
	bus_dmamap_sync_size((SC)->stat_mem.dmat,			\
	    (SC)->stat_mem.map,						\
	    (volatile char *)(P) - (volatile char *)(SC)->stat_mem.mem,	\
	    sizeof(volatile uint32_t), BUS_DMASYNC_POSTREAD)

#define	H_SYNCSTAT_PREWRITE(SC, P)					\
	bus_dmamap_sync_size((SC)->stat_mem.dmat,			\
	    (SC)->stat_mem.map,						\
	    (volatile char *)(P) - (volatile char *)(SC)->stat_mem.mem,	\
	    sizeof(volatile uint32_t), BUS_DMASYNC_PREWRITE)

#define	H_SYNCQ_PREWRITE(M, P, SZ)					\
	bus_dmamap_sync_size((M)->dmat, (M)->map,			\
	    (volatile char *)(P) - (volatile char *)(M)->mem, (SZ),	\
	    BUS_DMASYNC_PREWRITE)

#define	H_SYNCQ_POSTREAD(M, P, SZ)					\
	bus_dmamap_sync_size((M)->dmat, (M)->map,			\
	    (volatile char *)(P) - (volatile char *)(M)->mem, (SZ),	\
	    BUS_DMASYNC_POSTREAD)
#else
#define	H_SYNCSTAT_POSTREAD(SC, P)	do { } while (0)
#define	H_SYNCSTAT_PREWRITE(SC, P)	do { } while (0)
#define	H_SYNCQ_PREWRITE(M, P, SZ)	do { } while (0)
#define	H_SYNCQ_POSTREAD(M, P, SZ)	do { } while (0)
#endif

/*
 * Macros to manipulate VPVCs
 */
#define	MKVPVC(VPI,VCI)	(((VPI) << 16) | (VCI))
#define	GETVPI(VPVC)		(((VPVC) >> 16) & 0xff)
#define	GETVCI(VPVC)		((VPVC) & 0xffff)

/*
 * These macros encapsulate the bus_space functions for better readabiliy.
 */
#define	WRITE4(SC, OFF, VAL) bus_space_write_4(SC->memt, SC->memh, OFF, VAL)
#define	WRITE1(SC, OFF, VAL) bus_space_write_1(SC->memt, SC->memh, OFF, VAL)

#define	READ4(SC, OFF) bus_space_read_4(SC->memt, SC->memh, OFF)
#define	READ1(SC, OFF) bus_space_read_1(SC->memt, SC->memh, OFF)

#define	BARRIER_R(SC) \
	bus_space_barrier(SC->memt, SC->memh, 0, FATMO_END, \
	    BUS_SPACE_BARRIER_READ)
#define	BARRIER_W(SC) \
	bus_space_barrier(SC->memt, SC->memh, 0, FATMO_END, \
	    BUS_SPACE_BARRIER_WRITE)
#define	BARRIER_RW(SC) \
	bus_space_barrier(SC->memt, SC->memh, 0, FATMO_END, \
	    BUS_SPACE_BARRIER_WRITE|BUS_SPACE_BARRIER_READ)

#ifdef FATM_DEBUG
#define	DBG(SC, FL, PRINT) do {						\
	if ((SC)->debug & DBG_##FL) { 					\
		if_printf(&(SC)->ifatm.ifnet, "%s: ", __func__);	\
		printf PRINT;						\
		printf("\n");						\
	}								\
    } while (0)
#define	DBGC(SC, FL, PRINT) do {					\
	if ((SC)->debug & DBG_##FL) 					\
		printf PRINT;						\
    } while (0)

enum {
	DBG_RCV		= 0x0001,
	DBG_XMIT	= 0x0002,
	DBG_VCC		= 0x0004,
	DBG_IOCTL	= 0x0008,
	DBG_ATTACH	= 0x0010,
	DBG_INIT	= 0x0020,
	DBG_DMA		= 0x0040,
	DBG_BEAT	= 0x0080,
	DBG_UART	= 0x0100,
	DBG_LOCK	= 0x0200,

	DBG_ALL		= 0xffff
};

#else
#define	DBG(SC, FL, PRINT)
#define	DBGC(SC, FL, PRINT)
#endif

/*
 * Configuration.
 *
 * This section contains tunable parameters and dependend defines.
 */
#define	FATM_CMD_QLEN		16		/* command queue length */
#ifndef TEST_DMA_SYNC
#define	FATM_TX_QLEN		128		/* transmit queue length */
#define	FATM_RX_QLEN		64		/* receive queue length */
#else
#define	FATM_TX_QLEN		8		/* transmit queue length */
#define	FATM_RX_QLEN		8		/* receive queue length */
#endif

#define	SMALL_SUPPLY_QLEN	16
#define	SMALL_POOL_SIZE		256
#define	SMALL_SUPPLY_BLKSIZE	8

#define	LARGE_SUPPLY_QLEN	16
#define	LARGE_POOL_SIZE		128
#define	LARGE_SUPPLY_BLKSIZE	8
