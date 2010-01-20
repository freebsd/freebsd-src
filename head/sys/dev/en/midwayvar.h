/*	$NetBSD: midwayvar.h,v 1.10 1997/03/20 21:34:46 chuck Exp $	*/

/*-
 * Copyright (c) 1996 Charles D. Cranor and Washington University.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Charles D. Cranor and
 *	Washington University.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * m i d w a y v a r . h
 *
 * we define the en_softc here so that bus specific modules can allocate
 * it as the first item in their softc. 
 *
 * author: Chuck Cranor <chuck@ccrc.wustl.edu>
 */

/*
 * params needed to determine softc size
 */
#ifndef EN_NTX
#define EN_NTX          8       /* number of tx bufs to use */
#endif
#ifndef EN_TXSZ
#define EN_TXSZ         32      /* trasmit buf size in KB */
#endif
#ifndef EN_RXSZ
#define EN_RXSZ         32      /* recv buf size in KB */
#endif

/* largest possible NRX (depends on RAM size) */
#define EN_MAXNRX       ((2048 - (EN_NTX * EN_TXSZ)) / EN_RXSZ)

#ifndef EN_MAX_DMASEG
#define EN_MAX_DMASEG	32
#endif

/* number of bytes to use in the first receive buffer. This must not be larger
 * than MHLEN, should be a multiple of 64 and must be a multiple of 4. */
#define EN_RX1BUF	128

/*
 * Structure to hold DMA maps. These are handle via a typestable uma zone.
 */
struct en_map {
	uintptr_t	flags;		/* map flags */
	struct en_map	*rsvd2;		/* see uma_zalloc(9) */
	struct en_softc	*sc;		/* back pointer */
	bus_dmamap_t	map;		/* the map */
};
#define ENMAP_LOADED	0x02
#define ENMAP_ALLOC	0x01

#define EN_MAX_MAPS	400

/*
 * Statistics
 */
struct en_stats {
	uint32_t vtrash;	/* sw copy of counter */
	uint32_t otrash;	/* sw copy of counter */
	uint32_t ttrash;	/* # of RBD's with T bit set */
	uint32_t mfixaddr;	/* # of times we had to mfix an address */
	uint32_t mfixlen;	/* # of times we had to mfix a lenght*/
	uint32_t mfixfail;	/* # of times mfix failed */
	uint32_t txmbovr;	/* # of times we dropped due to mbsize */
	uint32_t dmaovr;	/* tx dma overflow count */
	uint32_t txoutspace;	/* out of space in xmit buffer */
	uint32_t txdtqout;	/* out of DTQs */
	uint32_t launch;	/* total # of launches */
	uint32_t hwpull;	/* # of pulls off hardware service list */
	uint32_t swadd;		/* # of pushes on sw service list */
	uint32_t rxqnotus;	/* # of times we pull from rx q, but fail */
	uint32_t rxqus;		/* # of good pulls from rx q */
	uint32_t rxdrqout;	/* # of times out of DRQs */
	uint32_t rxmbufout;	/* # of time out of mbufs */
	uint32_t txnomap;	/* out of DMA maps in TX */
};

/*
 * Each of these structures describes one of the eight transmit channels
 */
struct en_txslot {
	uint32_t	mbsize;		/* # mbuf bytes in use (max=TXHIWAT) */
	uint32_t	bfree;		/* # free bytes in buffer */
	uint32_t	start;		/* start of buffer area (byte offset) */
	uint32_t	stop;		/* ends of buffer area (byte offset) */
	uint32_t	cur;		/* next free area (byte offset) */
	uint32_t	nref;		/* # of VCs using this channel */
	struct ifqueue	q;		/* mbufs waiting for DMA now */
	struct ifqueue	indma;		/* mbufs waiting for DMA now */
};

/*
 * Each of these structures is used for each of the receive buffers on the
 * card.
 */
struct en_rxslot {
	uint32_t	mode;		/* saved copy of mode info */
	uint32_t	start;		/* begin of my buffer area */
	uint32_t	stop;		/* end of my buffer area */
	uint32_t	cur;		/* where I am at in the buffer */
	struct en_vcc	*vcc;		/* backpointer to VCI */
	struct ifqueue	q;		/* mbufs waiting for dma now */
	struct ifqueue	indma;		/* mbufs being dma'd now */
};

struct en_vcc {
	struct atmio_vcc vcc;		/* required by common code */
	void		*rxhand;
	u_int		vflags;
	uint32_t	ipackets;
	uint32_t	opackets;
	uint32_t	ibytes;
	uint32_t	obytes;

	uint8_t		txspeed;
	struct en_txslot *txslot;	/* transmit slot */
	struct en_rxslot *rxslot;	/* receive slot */
};
#define	VCC_DRAIN	0x0001		/* closed, but draining rx */
#define	VCC_SWSL	0x0002		/* on rx software service list */
#define	VCC_CLOSE_RX	0x0004		/* currently closing */

/*
 * softc
 */
struct en_softc {
	struct ifnet	*ifp;
	device_t dev;

	/* bus glue */
	bus_space_tag_t en_memt;	/* for EN_READ/EN_WRITE */
	bus_space_handle_t en_base;	/* base of en card */
	bus_size_t en_obmemsz;		/* size of en card (bytes) */
	void (*en_busreset)(void *);	/* bus specific reset function */
	bus_dma_tag_t txtag;		/* TX DMA tag */

	/* serv list */
	uint32_t hwslistp;	/* hw pointer to service list (byte offset) */
	uint16_t swslist[MID_SL_N]; /* software svc list (see en_service()) */
	uint16_t swsl_head; 	/* ends of swslist (index into swslist) */
	uint16_t swsl_tail;
	uint32_t swsl_size;	/* # of items in swsl */

	/* xmit dma */
	uint32_t dtq[MID_DTQ_N];/* sw copy of dma q (see EN_DQ_MK macros) */
	uint32_t dtq_free;	/* # of dtq's free */
	uint32_t dtq_us;	/* software copy of our pointer (byte offset) */
	uint32_t dtq_chip;	/* chip's pointer (byte offset) */
	uint32_t need_dtqs;	/* true if we ran out of DTQs */

	/* recv dma */
	uint32_t drq[MID_DRQ_N];/* sw copy of dma q (see ENIDQ macros) */
	uint32_t drq_free;	/* # of drq's free */
	uint32_t drq_us;	/* software copy of our pointer (byte offset) */
	uint32_t drq_chip;	/* chip's pointer (byte offset) */
	uint32_t need_drqs;	/* true if we ran out of DRQs */

	/* xmit buf ctrl. (per channel) */
	struct en_txslot txslot[MID_NTX_CH];

	/* recv buf ctrl. (per recv slot) */
	struct en_rxslot rxslot[EN_MAXNRX];
	int en_nrx;			/* # of active rx slots */

	/* vccs */
	struct en_vcc **vccs;
	u_int vccs_open;
	struct cv cv_close;		/* close CV */

	/* stats */
	struct en_stats stats;

	/* random stuff */
	uint32_t ipl;		/* sbus interrupt lvl (1 on pci?) */
	uint8_t bestburstcode;	/* code of best burst we can use */
	uint8_t bestburstlen;	/* length of best burst (bytes) */
	uint8_t bestburstshift;	/* (x >> shift) == (x / bestburstlen) */
	uint8_t bestburstmask;	/* bits to check if not multiple of burst */
	uint8_t alburst;	/* align dma bursts? */
	uint8_t noalbursts;	/* don't use unaligned > 4 byte bursts */
	uint8_t is_adaptec;	/* adaptec version of midway? */
	struct mbuf *padbuf;	/* buffer of zeros for TX padding */

	/* mutex to protect this structure and the associated hardware */
	struct mtx en_mtx;

	/* sysctl support */
	struct sysctl_ctx_list sysctl_ctx;
	struct sysctl_oid *sysctl_tree;

	/* memory zones */
	uma_zone_t map_zone;

	/* media and phy */
	struct ifmedia media;
	struct utopia utopia;

#ifdef EN_DEBUG
	/* debugging */
	u_int debug;
#endif
};

/*
 * exported functions
 */
int	en_attach(struct en_softc *);
void	en_destroy(struct en_softc *);
void	en_intr(void *);
void	en_reset(struct en_softc *);
int	en_modevent(module_t, int, void *arg);
