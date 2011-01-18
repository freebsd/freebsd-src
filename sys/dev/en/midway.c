/*	$NetBSD: midway.c,v 1.30 1997/09/29 17:40:38 chuck Exp $	*/
/*	(sync'd to midway.c 1.68)	*/

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
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 *
 * m i d w a y . c   e n i 1 5 5   d r i v e r 
 *
 * author: Chuck Cranor <chuck@ccrc.wustl.edu>
 * started: spring, 1996 (written from scratch).
 *
 * notes from the author:
 *   Extra special thanks go to Werner Almesberger, EPFL LRC.   Werner's
 *   ENI driver was especially useful in figuring out how this card works.
 *   I would also like to thank Werner for promptly answering email and being
 *   generally helpful.
 */

#define	EN_DIAG
#define EN_DDBHOOK	1	/* compile in ddb functions */

/*
 * Note on EN_ENIDMAFIX: the byte aligner on the ENI version of the card
 * appears to be broken.   it works just fine if there is no load... however
 * when the card is loaded the data get corrupted.   to see this, one only
 * has to use "telnet" over ATM.   do the following command in "telnet":
 * 	cat /usr/share/misc/termcap
 * "telnet" seems to generate lots of 1023 byte mbufs (which make great
 * use of the byte aligner).   watch "netstat -s" for checksum errors.
 * 
 * I further tested this by adding a function that compared the transmit 
 * data on the card's SRAM with the data in the mbuf chain _after_ the 
 * "transmit DMA complete" interrupt.   using the "telnet" test I got data
 * mismatches where the byte-aligned data should have been.   using ddb
 * and en_dumpmem() I verified that the DTQs fed into the card were 
 * absolutely correct.   thus, we are forced to concluded that the ENI
 * hardware is buggy.   note that the Adaptec version of the card works
 * just fine with byte DMA.
 *
 * bottom line: we set EN_ENIDMAFIX to 1 to avoid byte DMAs on the ENI
 * card.
 */

#if defined(DIAGNOSTIC) && !defined(EN_DIAG)
#define EN_DIAG			/* link in with master DIAG option */
#endif

#define EN_COUNT(X) (X)++

#ifdef EN_DEBUG

#undef	EN_DDBHOOK
#define	EN_DDBHOOK	1

/*
 * This macro removes almost all the EN_DEBUG conditionals in the code that make
 * to code a good deal less readable.
 */
#define DBG(SC, FL, PRINT) do {						\
	if ((SC)->debug & DBG_##FL) {					\
		device_printf((SC)->dev, "%s: "#FL": ", __func__);	\
		printf PRINT;						\
		printf("\n");						\
	}								\
    } while (0)

enum {
	DBG_INIT	= 0x0001,	/* debug attach/detach */
	DBG_TX		= 0x0002,	/* debug transmitting */
	DBG_SERV	= 0x0004,	/* debug service interrupts */
	DBG_IOCTL	= 0x0008,	/* debug ioctls */
	DBG_VC		= 0x0010,	/* debug VC handling */
	DBG_INTR	= 0x0020,	/* debug interrupts */
	DBG_DMA		= 0x0040,	/* debug DMA probing */
	DBG_IPACKETS	= 0x0080,	/* print input packets */
	DBG_REG		= 0x0100,	/* print all register access */
	DBG_LOCK	= 0x0200,	/* debug locking */
};

#else /* EN_DEBUG */

#define DBG(SC, FL, PRINT) do { } while (0)

#endif /* EN_DEBUG */

#include "opt_inet.h"
#include "opt_natm.h"
#include "opt_ddb.h"

#ifdef DDB
#undef	EN_DDBHOOK
#define	EN_DDBHOOK	1
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/endian.h>
#include <sys/stdint.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <vm/uma.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_atm.h>

#if defined(NATM) || defined(INET) || defined(INET6)
#include <netinet/in.h>
#if defined(INET) || defined(INET6)
#include <netinet/if_atm.h>
#endif
#endif

#ifdef NATM
#include <netnatm/natm.h>
#endif

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <machine/resource.h>
#include <dev/utopia/utopia.h>
#include <dev/en/midwayreg.h>
#include <dev/en/midwayvar.h>

#include <net/bpf.h>

/*
 * params
 */
#ifndef EN_TXHIWAT
#define EN_TXHIWAT	(64 * 1024)	/* max 64 KB waiting to be DMAd out */
#endif

SYSCTL_DECL(_hw_atm);

/*
 * dma tables
 *
 * The plan is indexed by the number of words to transfer.
 * The maximum index is 15 for 60 words.
 */
struct en_dmatab {
	uint8_t bcode;		/* code */
	uint8_t divshift;	/* byte divisor */
};

static const struct en_dmatab en_dmaplan[] = {
  { 0, 0 },		/* 0 */		{ MIDDMA_WORD, 2},	/* 1 */
  { MIDDMA_2WORD, 3},	/* 2 */		{ MIDDMA_WORD, 2},	/* 3 */
  { MIDDMA_4WORD, 4},	/* 4 */		{ MIDDMA_WORD, 2},	/* 5 */
  { MIDDMA_2WORD, 3},	/* 6 */		{ MIDDMA_WORD, 2},	/* 7 */
  { MIDDMA_8WORD, 5},   /* 8 */		{ MIDDMA_WORD, 2},	/* 9 */
  { MIDDMA_2WORD, 3},	/* 10 */	{ MIDDMA_WORD, 2},	/* 11 */
  { MIDDMA_4WORD, 4},	/* 12 */	{ MIDDMA_WORD, 2},	/* 13 */
  { MIDDMA_2WORD, 3},	/* 14 */	{ MIDDMA_WORD, 2},	/* 15 */
  { MIDDMA_16WORD,6},	/* 16 */
};

/*
 * prototypes
 */
#ifdef EN_DDBHOOK
int en_dump(int unit, int level);
int en_dumpmem(int,int,int);
#endif
static void en_close_finish(struct en_softc *sc, struct en_vcc *vc);

#define EN_LOCK(SC)	do {				\
	DBG(SC, LOCK, ("ENLOCK %d\n", __LINE__));	\
	mtx_lock(&sc->en_mtx);				\
    } while (0)
#define EN_UNLOCK(SC)	do {				\
	DBG(SC, LOCK, ("ENUNLOCK %d\n", __LINE__));	\
	mtx_unlock(&sc->en_mtx);			\
    } while (0)
#define EN_CHECKLOCK(sc)	mtx_assert(&sc->en_mtx, MA_OWNED)

/*
 * While a transmit mbuf is waiting to get transmit DMA resources we
 * need to keep some information with it. We don't want to allocate
 * additional memory for this so we stuff it into free fields in the
 * mbuf packet header. Neither the checksum fields nor the rcvif field are used
 * so use these.
 */
#define TX_AAL5		0x1	/* transmit AAL5 PDU */
#define TX_HAS_TBD	0x2	/* TBD did fit into mbuf */
#define TX_HAS_PAD	0x4	/* padding did fit into mbuf */
#define TX_HAS_PDU	0x8	/* PDU trailer did fit into mbuf */

#define MBUF_SET_TX(M, VCI, FLAGS, DATALEN, PAD, MAP) do {		\
	(M)->m_pkthdr.csum_data = (VCI) | ((FLAGS) << MID_VCI_BITS);	\
	(M)->m_pkthdr.csum_flags = ((DATALEN) & 0xffff) |		\
	    ((PAD & 0x3f) << 16);					\
	(M)->m_pkthdr.rcvif = (void *)(MAP);				\
    } while (0)

#define MBUF_GET_TX(M, VCI, FLAGS, DATALEN, PAD, MAP) do {		\
	(VCI) = (M)->m_pkthdr.csum_data & ((1 << MID_VCI_BITS) - 1);	\
	(FLAGS) = ((M)->m_pkthdr.csum_data >> MID_VCI_BITS) & 0xf;	\
	(DATALEN) = (M)->m_pkthdr.csum_flags & 0xffff;			\
	(PAD) = ((M)->m_pkthdr.csum_flags >> 16) & 0x3f;		\
	(MAP) = (void *)((M)->m_pkthdr.rcvif);				\
    } while (0)


#define EN_WRAPADD(START, STOP, CUR, VAL) do {			\
	(CUR) = (CUR) + (VAL);					\
	if ((CUR) >= (STOP))					\
		(CUR) = (START) + ((CUR) - (STOP));		\
    } while (0)

#define WORD_IDX(START, X) (((X) - (START)) / sizeof(uint32_t))

#define SETQ_END(SC, VAL) ((SC)->is_adaptec ?			\
	((VAL) | (MID_DMA_END >> 4)) :				\
	((VAL) | (MID_DMA_END)))

/*
 * The dtq and drq members are set for each END entry in the corresponding
 * card queue entry. It is used to find out, when a buffer has been
 * finished DMAing and can be freed.
 *
 * We store sc->dtq and sc->drq data in the following format...
 * the 0x80000 ensures we != 0
 */
#define EN_DQ_MK(SLOT, LEN)	(((SLOT) << 20) | (LEN) | (0x80000))
#define EN_DQ_SLOT(X)		((X) >> 20)
#define EN_DQ_LEN(X)		((X) & 0x3ffff)

/*
 * Variables
 */
static uma_zone_t en_vcc_zone;

/***********************************************************************/

/*
 * en_read{x}: read a word from the card. These are the only functions
 * that read from the card.
 */
static __inline uint32_t
en_readx(struct en_softc *sc, uint32_t r)
{
	uint32_t v;

#ifdef EN_DIAG
	if (r > MID_MAXOFF || (r % 4))
		panic("en_read out of range, r=0x%x", r);
#endif
	v = bus_space_read_4(sc->en_memt, sc->en_base, r);
	return (v);
}

static __inline uint32_t
en_read(struct en_softc *sc, uint32_t r)
{
	uint32_t v;

#ifdef EN_DIAG
	if (r > MID_MAXOFF || (r % 4))
		panic("en_read out of range, r=0x%x", r);
#endif
	v = bus_space_read_4(sc->en_memt, sc->en_base, r);
	DBG(sc, REG, ("en_read(%#x) -> %08x", r, v));
	return (v);
}

/*
 * en_write: write a word to the card. This is the only function that
 * writes to the card.
 */
static __inline void
en_write(struct en_softc *sc, uint32_t r, uint32_t v)
{
#ifdef EN_DIAG
	if (r > MID_MAXOFF || (r % 4))
		panic("en_write out of range, r=0x%x", r);
#endif
	DBG(sc, REG, ("en_write(%#x) <- %08x", r, v));
	bus_space_write_4(sc->en_memt, sc->en_base, r, v);
}

/*
 * en_k2sz: convert KBytes to a size parameter (a log2)
 */
static __inline int
en_k2sz(int k)
{
	switch(k) {
	  case 1:   return (0);
	  case 2:   return (1);
	  case 4:   return (2);
	  case 8:   return (3);
	  case 16:  return (4);
	  case 32:  return (5);
	  case 64:  return (6);
	  case 128: return (7);
	  default:
		panic("en_k2sz");
	}
	return (0);
}
#define en_log2(X) en_k2sz(X)

/*
 * en_b2sz: convert a DMA burst code to its byte size
 */
static __inline int
en_b2sz(int b)
{
	switch (b) {
	  case MIDDMA_WORD:   return (1*4);
	  case MIDDMA_2WMAYBE:
	  case MIDDMA_2WORD:  return (2*4);
	  case MIDDMA_4WMAYBE:
	  case MIDDMA_4WORD:  return (4*4);
	  case MIDDMA_8WMAYBE:
	  case MIDDMA_8WORD:  return (8*4);
	  case MIDDMA_16WMAYBE:
	  case MIDDMA_16WORD: return (16*4);
	  default:
		panic("en_b2sz");
	}
	return (0);
}

/*
 * en_sz2b: convert a burst size (bytes) to DMA burst code
 */
static __inline int
en_sz2b(int sz)
{
	switch (sz) {
	  case 1*4:  return (MIDDMA_WORD);
	  case 2*4:  return (MIDDMA_2WORD);
	  case 4*4:  return (MIDDMA_4WORD);
	  case 8*4:  return (MIDDMA_8WORD);
	  case 16*4: return (MIDDMA_16WORD);
	  default:
		panic("en_sz2b");
	}
	return(0);
}

#ifdef EN_DEBUG
/*
 * Dump a packet
 */
static void
en_dump_packet(struct en_softc *sc, struct mbuf *m)
{
	int plen = m->m_pkthdr.len;
	u_int pos = 0;
	u_int totlen = 0;
	int len;
	u_char *ptr;

	device_printf(sc->dev, "packet len=%d", plen);
	while (m != NULL) {
		totlen += m->m_len;
		ptr = mtod(m, u_char *);
		for (len = 0; len < m->m_len; len++, pos++, ptr++) {
			if (pos % 16 == 8)
				printf(" ");
			if (pos % 16 == 0)
				printf("\n");
			printf(" %02x", *ptr);
		}
		m = m->m_next;
	}
	printf("\n");
	if (totlen != plen)
		printf("sum of m_len=%u\n", totlen);
}
#endif

/*********************************************************************/
/*
 * DMA maps
 */

/*
 * Map constructor for a MAP.
 *
 * This is called each time when a map is allocated
 * from the pool and about to be returned to the user. Here we actually
 * allocate the map if there isn't one. The problem is that we may fail
 * to allocate the DMA map yet have no means to signal this error. Therefor
 * when allocating a map, the call must check that there is a map. An
 * additional problem is, that i386 maps will be NULL, yet are ok and must
 * be freed so let's use a flag to signal allocation.
 *
 * Caveat: we have no way to know that we are called from an interrupt context
 * here. We rely on the fact, that bus_dmamap_create uses M_NOWAIT in all
 * its allocations.
 *
 * LOCK: any, not needed
 */
static int
en_map_ctor(void *mem, int size, void *arg, int flags)
{
	struct en_softc *sc = arg;
	struct en_map *map = mem;
	int err;

	err = bus_dmamap_create(sc->txtag, 0, &map->map);
	if (err != 0) {
		device_printf(sc->dev, "cannot create DMA map %d\n", err);
		return (err);
	}
	map->flags = ENMAP_ALLOC;
	map->sc = sc;
	return (0);
}

/*
 * Map destructor.
 *
 * Called when a map is disposed into the zone. If the map is loaded, unload
 * it.
 *
 * LOCK: any, not needed
 */
static void
en_map_dtor(void *mem, int size, void *arg)
{
	struct en_map *map = mem;

	if (map->flags & ENMAP_LOADED) {
		bus_dmamap_unload(map->sc->txtag, map->map);
		map->flags &= ~ENMAP_LOADED;
	}
}

/*
 * Map finializer.
 *
 * This is called each time a map is returned from the zone to the system.
 * Get rid of the dmamap here.
 *
 * LOCK: any, not needed
 */
static void
en_map_fini(void *mem, int size)
{
	struct en_map *map = mem;

	bus_dmamap_destroy(map->sc->txtag, map->map);
}

/*********************************************************************/
/*
 * Transmission
 */

/*
 * Argument structure to load a transmit DMA map
 */
struct txarg {
	struct en_softc *sc;
	struct mbuf *m;
	u_int vci;
	u_int chan;		/* transmit channel */
	u_int datalen;		/* length of user data */
	u_int flags;
	u_int wait;		/* return: out of resources */
};

/*
 * TX DMA map loader helper. This function is the callback when the map
 * is loaded. It should fill the DMA segment descriptors into the hardware.
 *
 * LOCK: locked, needed
 */
static void
en_txdma_load(void *uarg, bus_dma_segment_t *segs, int nseg, bus_size_t mapsize,
    int error)
{
	struct txarg *tx = uarg;
	struct en_softc *sc = tx->sc;
	struct en_txslot *slot = &sc->txslot[tx->chan];
	uint32_t cur;		/* on-card buffer position (bytes offset) */
	uint32_t dtq;		/* on-card queue position (byte offset) */
	uint32_t last_dtq;	/* last DTQ we have written */
	uint32_t tmp;
	u_int free;		/* free queue entries on card */
	u_int needalign, cnt;
	bus_size_t rest;	/* remaining bytes in current segment */
	bus_addr_t addr;
	bus_dma_segment_t *s;
	uint32_t count, bcode;
	int i;

	if (error != 0)
		return;

	cur = slot->cur;
	dtq = sc->dtq_us;
	free = sc->dtq_free;

	last_dtq = 0;		/* make gcc happy */

	/*
	 * Local macro to add an entry to the transmit DMA area. If there
	 * are no entries left, return. Save the byte offset of the entry
	 * in last_dtq for later use.
	 */
#define PUT_DTQ_ENTRY(ENI, BCODE, COUNT, ADDR)				\
	if (free == 0) {						\
		EN_COUNT(sc->stats.txdtqout);				\
		tx->wait = 1;						\
		return;							\
	}								\
	last_dtq = dtq;							\
	en_write(sc, dtq + 0, (ENI || !sc->is_adaptec) ?		\
	    MID_MK_TXQ_ENI(COUNT, tx->chan, 0, BCODE) :			\
	    MID_MK_TXQ_ADP(COUNT, tx->chan, 0, BCODE));			\
	en_write(sc, dtq + 4, ADDR);					\
									\
	EN_WRAPADD(MID_DTQOFF, MID_DTQEND, dtq, 8);			\
	free--;

	/*
	 * Local macro to generate a DMA entry to DMA cnt bytes. Updates
	 * the current buffer byte offset accordingly.
	 */
#define DO_DTQ(TYPE) do {						\
	rest -= cnt;							\
	EN_WRAPADD(slot->start, slot->stop, cur, cnt);			\
	DBG(sc, TX, ("tx%d: "TYPE" %u bytes, %ju left, cur %#x",	\
	    tx->chan, cnt, (uintmax_t)rest, cur));			\
									\
	PUT_DTQ_ENTRY(1, bcode, count, addr);				\
									\
	addr += cnt;							\
    } while (0)

	if (!(tx->flags & TX_HAS_TBD)) {
		/*
		 * Prepend the TBD - it did not fit into the first mbuf
		 */
		tmp = MID_TBD_MK1((tx->flags & TX_AAL5) ?
		    MID_TBD_AAL5 : MID_TBD_NOAAL5,
		    sc->vccs[tx->vci]->txspeed,
		    tx->m->m_pkthdr.len / MID_ATMDATASZ);
		en_write(sc, cur, tmp);
		EN_WRAPADD(slot->start, slot->stop, cur, 4);

		tmp = MID_TBD_MK2(tx->vci, 0, 0);
		en_write(sc, cur, tmp);
		EN_WRAPADD(slot->start, slot->stop, cur, 4);

		/* update DMA address */
		PUT_DTQ_ENTRY(0, MIDDMA_JK, WORD_IDX(slot->start, cur), 0);
	}

	for (i = 0, s = segs; i < nseg; i++, s++) {
		rest = s->ds_len;
		addr = s->ds_addr;

		if (sc->is_adaptec) {
			/* adaptec card - simple */

			/* advance the on-card buffer pointer */
			EN_WRAPADD(slot->start, slot->stop, cur, rest);
			DBG(sc, TX, ("tx%d: adp %ju bytes %#jx (cur now 0x%x)",
			    tx->chan, (uintmax_t)rest, (uintmax_t)addr, cur));

			PUT_DTQ_ENTRY(0, 0, rest, addr);

			continue;
		}

		/*
		 * do we need to do a DMA op to align to the maximum
		 * burst? Note, that we are alway 32-bit aligned.
		 */
		if (sc->alburst &&
		    (needalign = (addr & sc->bestburstmask)) != 0) {
			/* compute number of bytes, words and code */
			cnt = sc->bestburstlen - needalign;
			if (cnt > rest)
				cnt = rest;
			count = cnt / sizeof(uint32_t);
			if (sc->noalbursts) {
				bcode = MIDDMA_WORD;
			} else {
				bcode = en_dmaplan[count].bcode;
				count = cnt >> en_dmaplan[count].divshift;
			}
			DO_DTQ("al_dma");
		}

		/* do we need to do a max-sized burst? */
		if (rest >= sc->bestburstlen) {
			count = rest >> sc->bestburstshift;
			cnt = count << sc->bestburstshift;
			bcode = sc->bestburstcode;
			DO_DTQ("best_dma");
		}

		/* do we need to do a cleanup burst? */
		if (rest != 0) {
			cnt = rest;
			count = rest / sizeof(uint32_t);
			if (sc->noalbursts) {
				bcode = MIDDMA_WORD;
			} else {
				bcode = en_dmaplan[count].bcode;
				count = cnt >> en_dmaplan[count].divshift;
			}
			DO_DTQ("clean_dma");
		}
	}

	KASSERT (tx->flags & TX_HAS_PAD, ("PDU not padded"));

	if ((tx->flags & TX_AAL5) && !(tx->flags & TX_HAS_PDU)) {
		/*
		 * Append the AAL5 PDU trailer
		 */
		tmp = MID_PDU_MK1(0, 0, tx->datalen);
		en_write(sc, cur, tmp);
		EN_WRAPADD(slot->start, slot->stop, cur, 4);

		en_write(sc, cur, 0);
		EN_WRAPADD(slot->start, slot->stop, cur, 4);

		/* update DMA address */
		PUT_DTQ_ENTRY(0, MIDDMA_JK, WORD_IDX(slot->start, cur), 0);
	}

	/* record the end for the interrupt routine */
	sc->dtq[MID_DTQ_A2REG(last_dtq)] =
	    EN_DQ_MK(tx->chan, tx->m->m_pkthdr.len);

	/* set the end flag in the last descriptor */
	en_write(sc, last_dtq + 0, SETQ_END(sc, en_read(sc, last_dtq + 0)));

#undef PUT_DTQ_ENTRY
#undef DO_DTQ

	/* commit */
	slot->cur = cur;
	sc->dtq_free = free;
	sc->dtq_us = dtq;

	/* tell card */
	en_write(sc, MID_DMA_WRTX, MID_DTQ_A2REG(sc->dtq_us));
}

/*
 * en_txdma: start transmit DMA on the given channel, if possible
 *
 * This is called from two places: when we got new packets from the upper
 * layer or when we found that buffer space has freed up during interrupt
 * processing.
 *
 * LOCK: locked, needed
 */
static void
en_txdma(struct en_softc *sc, struct en_txslot *slot)
{
	struct en_map *map;
	struct mbuf *lastm;
	struct txarg tx;
	u_int pad;
	int error;

	DBG(sc, TX, ("tx%td: starting ...", slot - sc->txslot));
  again:
	bzero(&tx, sizeof(tx));
	tx.chan = slot - sc->txslot;
	tx.sc = sc;

	/*
	 * get an mbuf waiting for DMA
	 */
	_IF_DEQUEUE(&slot->q, tx.m);
	if (tx.m == NULL) {
		DBG(sc, TX, ("tx%td: ...done!", slot - sc->txslot));
		return;
	}
	MBUF_GET_TX(tx.m, tx.vci, tx.flags, tx.datalen, pad, map);

	/*
	 * note: don't use the entire buffer space.  if WRTX becomes equal
	 * to RDTX, the transmitter stops assuming the buffer is empty!  --kjc
	 */
	if (tx.m->m_pkthdr.len >= slot->bfree) {
		EN_COUNT(sc->stats.txoutspace);
		DBG(sc, TX, ("tx%td: out of transmit space", slot - sc->txslot));
		goto waitres;
	}
  
	lastm = NULL;
	if (!(tx.flags & TX_HAS_PAD)) {
		if (pad != 0) {
			/* Append the padding buffer */
			(void)m_length(tx.m, &lastm);
			lastm->m_next = sc->padbuf;
			sc->padbuf->m_len = pad;
		}
		tx.flags |= TX_HAS_PAD;
	}

	/*
	 * Try to load that map
	 */
	error = bus_dmamap_load_mbuf(sc->txtag, map->map, tx.m,
	    en_txdma_load, &tx, BUS_DMA_NOWAIT);

	if (lastm != NULL)
		lastm->m_next = NULL;

	if (error != 0) {
		device_printf(sc->dev, "loading TX map failed %d\n",
		    error);
		goto dequeue_drop;
	}
	map->flags |= ENMAP_LOADED;
	if (tx.wait) {
		/* probably not enough space */
		bus_dmamap_unload(map->sc->txtag, map->map);
		map->flags &= ~ENMAP_LOADED;

		sc->need_dtqs = 1;
		DBG(sc, TX, ("tx%td: out of transmit DTQs", slot - sc->txslot));
		goto waitres;
	}

	EN_COUNT(sc->stats.launch);
	sc->ifp->if_opackets++;

	sc->vccs[tx.vci]->opackets++;
	sc->vccs[tx.vci]->obytes += tx.datalen;

#ifdef ENABLE_BPF
	if (bpf_peers_present(sc->ifp->if_bpf)) {
		/*
		 * adjust the top of the mbuf to skip the TBD if present
		 * before passing the packet to bpf.
		 * Also remove padding and the PDU trailer. Assume both of
		 * them to be in the same mbuf. pktlen, m_len and m_data
		 * are not needed anymore so we can change them.
		 */
		if (tx.flags & TX_HAS_TBD) {
			tx.m->m_data += MID_TBD_SIZE;
			tx.m->m_len -= MID_TBD_SIZE;
		}
		tx.m->m_pkthdr.len = m_length(tx.m, &lastm);
		if (tx.m->m_pkthdr.len > tx.datalen) {
			lastm->m_len -= tx.m->m_pkthdr.len - tx.datalen;
			tx.m->m_pkthdr.len = tx.datalen;
		}

		bpf_mtap(sc->ifp->if_bpf, tx.m);
	}
#endif

	/*
	 * do some housekeeping and get the next packet
	 */
	slot->bfree -= tx.m->m_pkthdr.len;
	_IF_ENQUEUE(&slot->indma, tx.m);

	goto again;

	/*
	 * error handling. This is jumped to when we just want to drop
	 * the packet. Must be unlocked here.
	 */
  dequeue_drop:
	if (map != NULL)
		uma_zfree(sc->map_zone, map);

	slot->mbsize -= tx.m->m_pkthdr.len;

	m_freem(tx.m);

	goto again;

  waitres:
	_IF_PREPEND(&slot->q, tx.m);
}

/*
 * Create a copy of a single mbuf. It can have either internal or
 * external data, it may have a packet header. External data is really
 * copied, so the new buffer is writeable.
 *
 * LOCK: any, not needed
 */
static struct mbuf *
copy_mbuf(struct mbuf *m)
{
	struct mbuf *new;

	MGET(new, M_WAIT, MT_DATA);

	if (m->m_flags & M_PKTHDR) {
		M_MOVE_PKTHDR(new, m);
		if (m->m_len > MHLEN)
			MCLGET(new, M_WAIT);
	} else {
		if (m->m_len > MLEN)
			MCLGET(new, M_WAIT);
	}

	bcopy(m->m_data, new->m_data, m->m_len);
	new->m_len = m->m_len;
	new->m_flags &= ~M_RDONLY;

	return (new);
}

/*
 * This function is called when we have an ENI adapter. It fixes the
 * mbuf chain, so that all addresses and lengths are 4 byte aligned.
 * The overall length is already padded to multiple of cells plus the
 * TBD so this must always succeed. The routine can fail, when it
 * needs to copy an mbuf (this may happen if an mbuf is readonly).
 *
 * We assume here, that aligning the virtual addresses to 4 bytes also
 * aligns the physical addresses.
 *
 * LOCK: locked, needed
 */
static struct mbuf *
en_fix_mchain(struct en_softc *sc, struct mbuf *m0, u_int *pad)
{
	struct mbuf **prev = &m0;
	struct mbuf *m = m0;
	struct mbuf *new;
	u_char *d;
	int off;

	while (m != NULL) {
		d = mtod(m, u_char *);
		if ((off = (uintptr_t)d % sizeof(uint32_t)) != 0) {
			EN_COUNT(sc->stats.mfixaddr);
			if (M_WRITABLE(m)) {
				bcopy(d, d - off, m->m_len);
				m->m_data -= off;
			} else {
				if ((new = copy_mbuf(m)) == NULL) {
					EN_COUNT(sc->stats.mfixfail);
					m_freem(m0);
					return (NULL);
				}
				new->m_next = m_free(m);
				*prev = m = new;
			}
		}

		if ((off = m->m_len % sizeof(uint32_t)) != 0) {
			EN_COUNT(sc->stats.mfixlen);
			if (!M_WRITABLE(m)) {
				if ((new = copy_mbuf(m)) == NULL) {
					EN_COUNT(sc->stats.mfixfail);
					m_freem(m0);
					return (NULL);
				}
				new->m_next = m_free(m);
				*prev = m = new;
			}
			d = mtod(m, u_char *) + m->m_len;
			off = 4 - off;
			while (off) {
				while (m->m_next && m->m_next->m_len == 0)
					m->m_next = m_free(m->m_next);

				if (m->m_next == NULL) {
					*d++ = 0;
					KASSERT(*pad > 0, ("no padding space"));
					(*pad)--;
				} else {
					*d++ = *mtod(m->m_next, u_char *);
					m->m_next->m_len--;
					m->m_next->m_data++;
				}
				m->m_len++;
				off--;
			}
		}

		prev = &m->m_next;
		m = m->m_next;
	}

	return (m0);
}

/*
 * en_start: start transmitting the next packet that needs to go out
 * if there is one. We take off all packets from the interface's queue and
 * put them into the channels queue.
 *
 * Here we also prepend the transmit packet descriptor and append the padding
 * and (for aal5) the PDU trailer. This is different from the original driver:
 * we assume, that allocating one or two additional mbufs is actually cheaper
 * than all this algorithmic fiddling we would need otherwise.
 *
 * While the packet is on the channels wait queue we use the csum_* fields
 * in the packet header to hold the original datalen, the AAL5 flag and the
 * VCI. The packet length field in the header holds the needed buffer space.
 * This may actually be more than the length of the current mbuf chain (when
 * one or more of TBD, padding and PDU do not fit).
 *
 * LOCK: unlocked, needed
 */
static void
en_start(struct ifnet *ifp)
{
	struct en_softc *sc = (struct en_softc *)ifp->if_softc;
	struct mbuf *m, *lastm;
	struct atm_pseudohdr *ap;
	u_int pad;		/* 0-bytes to pad at PDU end */
	u_int datalen;		/* length of user data */
	u_int vci;		/* the VCI we are transmitting on */
	u_int flags;
	uint32_t tbd[2];
	uint32_t pdu[2];
	struct en_vcc *vc;
	struct en_map *map;
	struct en_txslot *tx;

	while (1) {
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			return;

		flags = 0;

	    	ap = mtod(m, struct atm_pseudohdr *);
		vci = ATM_PH_VCI(ap);

		if (ATM_PH_VPI(ap) != 0 || vci >= MID_N_VC ||
		    (vc = sc->vccs[vci]) == NULL ||
		    (vc->vflags & VCC_CLOSE_RX)) {
			DBG(sc, TX, ("output vpi=%u, vci=%u -- drop",
			    ATM_PH_VPI(ap), vci));
			m_freem(m);
			continue;
		}
		if (vc->vcc.aal == ATMIO_AAL_5)
			flags |= TX_AAL5;
		m_adj(m, sizeof(struct atm_pseudohdr));

		/*
		 * (re-)calculate size of packet (in bytes)
		 */
		m->m_pkthdr.len = datalen = m_length(m, &lastm);

		/*
		 * computing how much padding we need on the end of the mbuf,
		 * then see if we can put the TBD at the front of the mbuf
		 * where the link header goes (well behaved protocols will
		 * reserve room for us). Last, check if room for PDU tail.
		 */
		if (flags & TX_AAL5)
			m->m_pkthdr.len += MID_PDU_SIZE;
		m->m_pkthdr.len = roundup(m->m_pkthdr.len, MID_ATMDATASZ);
		pad = m->m_pkthdr.len - datalen;
		if (flags & TX_AAL5)
			pad -= MID_PDU_SIZE;
		m->m_pkthdr.len += MID_TBD_SIZE;

		DBG(sc, TX, ("txvci%d: buflen=%u datalen=%u lead=%d trail=%d",
		    vci, m->m_pkthdr.len, datalen, (int)M_LEADINGSPACE(m),
		    (int)M_TRAILINGSPACE(lastm)));

		/*
		 * From here on we need access to sc
		 */
		EN_LOCK(sc);

		/*
		 * Allocate a map. We do this here rather then in en_txdma,
		 * because en_txdma is also called from the interrupt handler
		 * and we are going to have a locking problem then. We must
		 * use NOWAIT here, because the ip_output path holds various
		 * locks.
		 */
		map = uma_zalloc_arg(sc->map_zone, sc, M_NOWAIT);
		if (map == NULL) {
			/* drop that packet */
			EN_COUNT(sc->stats.txnomap);
			EN_UNLOCK(sc);
			m_freem(m);
			continue;
		}

		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
			EN_UNLOCK(sc);
			uma_zfree(sc->map_zone, map);
			m_freem(m);
			continue;
		}

		/*
		 * Look, whether we can prepend the TBD (8 byte)
		 */
		if (M_WRITABLE(m) && M_LEADINGSPACE(m) >= MID_TBD_SIZE) {
			tbd[0] = htobe32(MID_TBD_MK1((flags & TX_AAL5) ?
			    MID_TBD_AAL5 : MID_TBD_NOAAL5,
			    vc->txspeed, m->m_pkthdr.len / MID_ATMDATASZ));
			tbd[1] = htobe32(MID_TBD_MK2(vci, 0, 0));

			m->m_data -= MID_TBD_SIZE;
			bcopy(tbd, m->m_data, MID_TBD_SIZE);
			m->m_len += MID_TBD_SIZE;
			flags |= TX_HAS_TBD;
		}

		/*
		 * Check whether the padding fits (must be writeable -
		 * we pad with zero).
		 */
		if (M_WRITABLE(lastm) && M_TRAILINGSPACE(lastm) >= pad) {
			bzero(lastm->m_data + lastm->m_len, pad);
			lastm->m_len += pad;
			flags |= TX_HAS_PAD;

			if ((flags & TX_AAL5) &&
			    M_TRAILINGSPACE(lastm) > MID_PDU_SIZE) {
				pdu[0] = htobe32(MID_PDU_MK1(0, 0, datalen));
				pdu[1] = 0;
				bcopy(pdu, lastm->m_data + lastm->m_len,
				    MID_PDU_SIZE);
				lastm->m_len += MID_PDU_SIZE;
				flags |= TX_HAS_PDU;
			}
		}

		if (!sc->is_adaptec &&
		    (m = en_fix_mchain(sc, m, &pad)) == NULL) {
			EN_UNLOCK(sc);
			uma_zfree(sc->map_zone, map);
			continue;
		}

		/*
		 * get assigned channel (will be zero unless txspeed is set)
		 */
		tx = vc->txslot;

		if (m->m_pkthdr.len > EN_TXSZ * 1024) {
			DBG(sc, TX, ("tx%td: packet larger than xmit buffer "
			    "(%d > %d)\n", tx - sc->txslot, m->m_pkthdr.len,
			    EN_TXSZ * 1024));
			EN_UNLOCK(sc);
			m_freem(m);
			uma_zfree(sc->map_zone, map);
			continue;
		}

		if (tx->mbsize > EN_TXHIWAT) {
			EN_COUNT(sc->stats.txmbovr);
			DBG(sc, TX, ("tx%td: buffer space shortage",
			    tx - sc->txslot));
			EN_UNLOCK(sc);
			m_freem(m);
			uma_zfree(sc->map_zone, map);
			continue;
		}

		/* commit */
		tx->mbsize += m->m_pkthdr.len;

		DBG(sc, TX, ("tx%td: VCI=%d, speed=0x%x, buflen=%d, mbsize=%d",
		    tx - sc->txslot, vci, sc->vccs[vci]->txspeed,
		    m->m_pkthdr.len, tx->mbsize));

		MBUF_SET_TX(m, vci, flags, datalen, pad, map);

		_IF_ENQUEUE(&tx->q, m);

		en_txdma(sc, tx);

		EN_UNLOCK(sc);
	}
}

/*********************************************************************/
/*
 * VCs
 */

/*
 * en_loadvc: load a vc tab entry from a slot
 *
 * LOCK: locked, needed
 */
static void
en_loadvc(struct en_softc *sc, struct en_vcc *vc)
{
	uint32_t reg = en_read(sc, MID_VC(vc->vcc.vci));

	reg = MIDV_SETMODE(reg, MIDV_TRASH);
	en_write(sc, MID_VC(vc->vcc.vci), reg);
	DELAY(27);

	/* no need to set CRC */

	/* read pointer = 0, desc. start = 0 */
	en_write(sc, MID_DST_RP(vc->vcc.vci), 0);
	/* write pointer = 0 */
	en_write(sc, MID_WP_ST_CNT(vc->vcc.vci), 0);
	/* set mode, size, loc */
	en_write(sc, MID_VC(vc->vcc.vci), vc->rxslot->mode);

	vc->rxslot->cur = vc->rxslot->start;

	DBG(sc, VC, ("rx%td: assigned to VCI %d", vc->rxslot - sc->rxslot,
	    vc->vcc.vci));
}

/*
 * Open the given vcc.
 *
 * LOCK: unlocked, needed
 */
static int
en_open_vcc(struct en_softc *sc, struct atmio_openvcc *op)
{
	uint32_t oldmode, newmode;
	struct en_rxslot *slot;
	struct en_vcc *vc;
	int error = 0;

	DBG(sc, IOCTL, ("enable vpi=%d, vci=%d, flags=%#x",
	    op->param.vpi, op->param.vci, op->param.flags));

	if (op->param.vpi != 0 || op->param.vci >= MID_N_VC)
		return (EINVAL);

	vc = uma_zalloc(en_vcc_zone, M_NOWAIT | M_ZERO);
	if (vc == NULL)
		return (ENOMEM);

	EN_LOCK(sc);

	if (sc->vccs[op->param.vci] != NULL) {
		error = EBUSY;
		goto done;
	}

	/* find a free receive slot */
	for (slot = sc->rxslot; slot < &sc->rxslot[sc->en_nrx]; slot++)
		if (slot->vcc == NULL)
			break;
	if (slot == &sc->rxslot[sc->en_nrx]) {
		error = ENOSPC;
		goto done;
	}

	vc->rxslot = slot;
	vc->rxhand = op->rxhand;
	vc->vcc = op->param;

	oldmode = slot->mode;
	newmode = (op->param.aal == ATMIO_AAL_5) ? MIDV_AAL5 : MIDV_NOAAL;
	slot->mode = MIDV_SETMODE(oldmode, newmode);
	slot->vcc = vc;

	KASSERT (_IF_QLEN(&slot->indma) == 0 && _IF_QLEN(&slot->q) == 0,
	    ("en_rxctl: left over mbufs on enable slot=%td",
	    vc->rxslot - sc->rxslot));

	vc->txspeed = 0;
	vc->txslot = sc->txslot;
	vc->txslot->nref++;	/* bump reference count */

	en_loadvc(sc, vc);	/* does debug printf for us */

	/* don't free below */
	sc->vccs[vc->vcc.vci] = vc;
	vc = NULL;
	sc->vccs_open++;

  done:
	if (vc != NULL)
		uma_zfree(en_vcc_zone, vc);

	EN_UNLOCK(sc);
	return (error);
}

/*
 * Close finished
 */
static void
en_close_finish(struct en_softc *sc, struct en_vcc *vc)
{

	if (vc->rxslot != NULL)
		vc->rxslot->vcc = NULL;

	DBG(sc, VC, ("vci: %u free (%p)", vc->vcc.vci, vc));

	sc->vccs[vc->vcc.vci] = NULL;
	uma_zfree(en_vcc_zone, vc);
	sc->vccs_open--;
}

/*
 * LOCK: unlocked, needed
 */
static int
en_close_vcc(struct en_softc *sc, struct atmio_closevcc *cl)
{
	uint32_t oldmode, newmode;
	struct en_vcc *vc;
	int error = 0;

	DBG(sc, IOCTL, ("disable vpi=%d, vci=%d", cl->vpi, cl->vci));

	if (cl->vpi != 0 || cl->vci >= MID_N_VC)
		return (EINVAL);

	EN_LOCK(sc);
	if ((vc = sc->vccs[cl->vci]) == NULL) {
		error = ENOTCONN;
		goto done;
	}

	/*
	 * turn off VCI
	 */
	if (vc->rxslot == NULL) {
		error = ENOTCONN;
		goto done;
	}
	if (vc->vflags & VCC_DRAIN) {
		error = EINVAL;
		goto done;
	}

	oldmode = en_read(sc, MID_VC(cl->vci));
	newmode = MIDV_SETMODE(oldmode, MIDV_TRASH) & ~MIDV_INSERVICE;
	en_write(sc, MID_VC(cl->vci), (newmode | (oldmode & MIDV_INSERVICE)));

	/* halt in tracks, be careful to preserve inservice bit */
	DELAY(27);
	vc->rxslot->mode = newmode;

	vc->txslot->nref--;

	/* if stuff is still going on we are going to have to drain it out */
	if (_IF_QLEN(&vc->rxslot->indma) == 0 &&
	    _IF_QLEN(&vc->rxslot->q) == 0 &&
	    (vc->vflags & VCC_SWSL) == 0) {
		en_close_finish(sc, vc);
		goto done;
	}

	vc->vflags |= VCC_DRAIN;
	DBG(sc, IOCTL, ("VCI %u now draining", cl->vci));

	if (vc->vcc.flags & ATMIO_FLAG_ASYNC)
		goto done;

	vc->vflags |= VCC_CLOSE_RX;
	while ((sc->ifp->if_drv_flags & IFF_DRV_RUNNING) &&
	    (vc->vflags & VCC_DRAIN))
		cv_wait(&sc->cv_close, &sc->en_mtx);

	en_close_finish(sc, vc);
	if (!(sc->ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		error = EIO;
		goto done;
	}


  done:
	EN_UNLOCK(sc);
	return (error);
}

/*********************************************************************/
/*
 * starting/stopping the card
 */

/*
 * en_reset_ul: reset the board, throw away work in progress.
 * must en_init to recover.
 *
 * LOCK: locked, needed
 */
static void
en_reset_ul(struct en_softc *sc)
{
	struct en_map *map;
	struct mbuf *m;
	struct en_rxslot *rx;
	int lcv;

	device_printf(sc->dev, "reset\n");
	sc->ifp->if_drv_flags &= ~IFF_DRV_RUNNING;

	if (sc->en_busreset)
		sc->en_busreset(sc);
	en_write(sc, MID_RESID, 0x0);	/* reset hardware */

	/*
	 * recv: dump any mbufs we are dma'ing into, if DRAINing, then a reset
	 * will free us! Don't release the rxslot from the channel.
	 */
	for (lcv = 0 ; lcv < MID_N_VC ; lcv++) {
		if (sc->vccs[lcv] == NULL)
			continue;
		rx = sc->vccs[lcv]->rxslot;

		for (;;) {
			_IF_DEQUEUE(&rx->indma, m);
			if (m == NULL)
				break;
			map = (void *)m->m_pkthdr.rcvif;
			uma_zfree(sc->map_zone, map);
			m_freem(m);
		}
		for (;;) {
			_IF_DEQUEUE(&rx->q, m);
			if (m == NULL)
				break;
			m_freem(m);
		}
		sc->vccs[lcv]->vflags = 0;
	}

	/*
	 * xmit: dump everything
	 */
	for (lcv = 0 ; lcv < EN_NTX ; lcv++) {
		for (;;) {
			_IF_DEQUEUE(&sc->txslot[lcv].indma, m);
			if (m == NULL)
				break;
			map = (void *)m->m_pkthdr.rcvif;
			uma_zfree(sc->map_zone, map);
			m_freem(m);
		}
		for (;;) {
			_IF_DEQUEUE(&sc->txslot[lcv].q, m);
			if (m == NULL)
				break;
			map = (void *)m->m_pkthdr.rcvif;
			uma_zfree(sc->map_zone, map);
			m_freem(m);
		}
		sc->txslot[lcv].mbsize = 0;
	}

	/*
	 * Unstop all waiters
	 */
	cv_broadcast(&sc->cv_close);
}

/*
 * en_reset: reset the board, throw away work in progress.
 * must en_init to recover.
 *
 * LOCK: unlocked, needed
 *
 * Use en_reset_ul if you alreay have the lock
 */
void
en_reset(struct en_softc *sc)
{
	EN_LOCK(sc);
	en_reset_ul(sc);
	EN_UNLOCK(sc);
}


/*
 * en_init: init board and sync the card with the data in the softc.
 *
 * LOCK: locked, needed
 */
static void
en_init(struct en_softc *sc)
{
	int vc, slot;
	uint32_t loc;

	if ((sc->ifp->if_flags & IFF_UP) == 0) {
		DBG(sc, INIT, ("going down"));
		en_reset(sc);				/* to be safe */
		return;
	}

	DBG(sc, INIT, ("going up"));
	sc->ifp->if_drv_flags |= IFF_DRV_RUNNING;	/* enable */

	if (sc->en_busreset)
		sc->en_busreset(sc);
	en_write(sc, MID_RESID, 0x0);		/* reset */

	/* zero memory */
	bus_space_set_region_4(sc->en_memt, sc->en_base,
	    MID_RAMOFF, 0, sc->en_obmemsz / 4);

	/*
	 * init obmem data structures: vc tab, dma q's, slist.
	 *
	 * note that we set drq_free/dtq_free to one less than the total number
	 * of DTQ/DRQs present.   we do this because the card uses the condition
	 * (drq_chip == drq_us) to mean "list is empty"... but if you allow the
	 * circular list to be completely full then (drq_chip == drq_us) [i.e.
	 * the drq_us pointer will wrap all the way around].   by restricting
	 * the number of active requests to (N - 1) we prevent the list from
	 * becoming completely full.    note that the card will sometimes give
	 * us an interrupt for a DTQ/DRQ we have already processes... this helps
	 * keep that interrupt from messing us up.
	 */
	bzero(&sc->drq, sizeof(sc->drq));
	sc->drq_free = MID_DRQ_N - 1;
	sc->drq_chip = MID_DRQ_REG2A(en_read(sc, MID_DMA_RDRX));
	en_write(sc, MID_DMA_WRRX, MID_DRQ_A2REG(sc->drq_chip)); 
	sc->drq_us = sc->drq_chip;

	bzero(&sc->dtq, sizeof(sc->dtq));
	sc->dtq_free = MID_DTQ_N - 1;
	sc->dtq_chip = MID_DTQ_REG2A(en_read(sc, MID_DMA_RDTX));
	en_write(sc, MID_DMA_WRTX, MID_DRQ_A2REG(sc->dtq_chip)); 
	sc->dtq_us = sc->dtq_chip;

	sc->hwslistp = MID_SL_REG2A(en_read(sc, MID_SERV_WRITE));
	sc->swsl_size = sc->swsl_head = sc->swsl_tail = 0;

	DBG(sc, INIT, ("drq free/chip: %d/0x%x, dtq free/chip: %d/0x%x, "
	    "hwslist: 0x%x", sc->drq_free, sc->drq_chip, sc->dtq_free,
	    sc->dtq_chip, sc->hwslistp));

	for (slot = 0 ; slot < EN_NTX ; slot++) {
		sc->txslot[slot].bfree = EN_TXSZ * 1024;
		en_write(sc, MIDX_READPTR(slot), 0);
		en_write(sc, MIDX_DESCSTART(slot), 0);
		loc = sc->txslot[slot].cur = sc->txslot[slot].start;
		loc = loc - MID_RAMOFF;
		/* mask, cvt to words */
		loc = (loc & ~((EN_TXSZ * 1024) - 1)) >> 2;
		/* top 11 bits */
		loc = loc >> MIDV_LOCTOPSHFT;
		en_write(sc, MIDX_PLACE(slot), MIDX_MKPLACE(en_k2sz(EN_TXSZ),
		    loc));
		DBG(sc, INIT, ("tx%d: place 0x%x", slot,
		    (u_int)en_read(sc, MIDX_PLACE(slot))));
	}

	for (vc = 0; vc < MID_N_VC; vc++) 
		if (sc->vccs[vc] != NULL)
			en_loadvc(sc, sc->vccs[vc]);

	/*
	 * enable!
	 */
	en_write(sc, MID_INTENA, MID_INT_TX | MID_INT_DMA_OVR | MID_INT_IDENT |
	    MID_INT_LERR | MID_INT_DMA_ERR | MID_INT_DMA_RX | MID_INT_DMA_TX |
	    MID_INT_SERVICE | MID_INT_SUNI | MID_INT_STATS);
	en_write(sc, MID_MAST_CSR, MID_SETIPL(sc->ipl) | MID_MCSR_ENDMA |
	    MID_MCSR_ENTX | MID_MCSR_ENRX);
}

/*********************************************************************/
/*
 * Ioctls
 */
/*
 * en_ioctl: handle ioctl requests
 *
 * NOTE: if you add an ioctl to set txspeed, you should choose a new
 * TX channel/slot.   Choose the one with the lowest sc->txslot[slot].nref
 * value, subtract one from sc->txslot[0].nref, add one to the
 * sc->txslot[slot].nref, set sc->txvc2slot[vci] = slot, and then set
 * txspeed[vci].
 *
 * LOCK: unlocked, needed
 */
static int
en_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct en_softc *sc = (struct en_softc *)ifp->if_softc;
#if defined(INET) || defined(INET6)
	struct ifaddr *ifa = (struct ifaddr *)data;
#endif
	struct ifreq *ifr = (struct ifreq *)data;
	struct atmio_vcctable *vtab;
	int error = 0;

	switch (cmd) {

	  case SIOCSIFADDR: 
		EN_LOCK(sc);
		ifp->if_flags |= IFF_UP;
#if defined(INET) || defined(INET6)
		if (ifa->ifa_addr->sa_family == AF_INET
		    || ifa->ifa_addr->sa_family == AF_INET6) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				en_reset_ul(sc);
				en_init(sc);
			}
			ifa->ifa_rtrequest = atm_rtrequest; /* ??? */
			EN_UNLOCK(sc);
			break;
		}
#endif /* INET */
		if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			en_reset_ul(sc);
			en_init(sc);
		}
		EN_UNLOCK(sc);
		break;

	case SIOCSIFFLAGS: 
		EN_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
				en_init(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				en_reset_ul(sc);
		}
		EN_UNLOCK(sc);
		break;

	  case SIOCSIFMTU:
		/*
		 * Set the interface MTU.
		 */
		if (ifr->ifr_mtu > ATMMTU) {
			error = EINVAL;
			break;
		}
		ifp->if_mtu = ifr->ifr_mtu;
		break;

	  case SIOCSIFMEDIA:
	  case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->media, cmd);
		break;

	  case SIOCATMOPENVCC:		/* kernel internal use */
		error = en_open_vcc(sc, (struct atmio_openvcc *)data);
		break;

	  case SIOCATMCLOSEVCC:		/* kernel internal use */
		error = en_close_vcc(sc, (struct atmio_closevcc *)data);
		break;

	  case SIOCATMGETVCCS:	/* internal netgraph use */
		vtab = atm_getvccs((struct atmio_vcc **)sc->vccs,
		    MID_N_VC, sc->vccs_open, &sc->en_mtx, 0);
		if (vtab == NULL) {
			error = ENOMEM;
			break;
		}
		*(void **)data = vtab;
		break;

	  case SIOCATMGVCCS:	/* return vcc table */
		vtab = atm_getvccs((struct atmio_vcc **)sc->vccs,
		    MID_N_VC, sc->vccs_open, &sc->en_mtx, 1);
		error = copyout(vtab, ifr->ifr_data, sizeof(*vtab) +
		    vtab->count * sizeof(vtab->vccs[0]));
		free(vtab, M_DEVBUF);
		break;

	  default: 
		error = EINVAL;
		break;
	}
	return (error);
}

/*********************************************************************/
/*
 * Sysctl's
 */

/*
 * Sysctl handler for internal statistics
 *
 * LOCK: unlocked, needed
 */
static int
en_sysctl_istats(SYSCTL_HANDLER_ARGS)
{
	struct en_softc *sc = arg1;
	uint32_t *ret;
	int error;

	ret = malloc(sizeof(sc->stats), M_TEMP, M_WAITOK);

	EN_LOCK(sc);
	bcopy(&sc->stats, ret, sizeof(sc->stats));
	EN_UNLOCK(sc);

	error = SYSCTL_OUT(req, ret, sizeof(sc->stats));
	free(ret, M_TEMP);

	return (error);
}

/*********************************************************************/
/*
 * Interrupts
 */

/*
 * Transmit interrupt handler
 *
 * check for tx complete, if detected then this means that some space
 * has come free on the card.   we must account for it and arrange to
 * kick the channel to life (in case it is stalled waiting on the card).
 *
 * LOCK: locked, needed
 */
static uint32_t
en_intr_tx(struct en_softc *sc, uint32_t reg)
{
	uint32_t kick;
	uint32_t mask;
	uint32_t val;
	int chan;

	kick = 0;		/* bitmask of channels to kick */

	for (mask = 1, chan = 0; chan < EN_NTX; chan++, mask *= 2) {
		if (!(reg & MID_TXCHAN(chan)))
			continue;

		kick = kick | mask;

		/* current read pointer */
		val = en_read(sc, MIDX_READPTR(chan));
		/* as offset */
		val = (val * sizeof(uint32_t)) + sc->txslot[chan].start;
		if (val > sc->txslot[chan].cur)
			sc->txslot[chan].bfree = val - sc->txslot[chan].cur;
		else
			sc->txslot[chan].bfree = (val + (EN_TXSZ * 1024)) -
			    sc->txslot[chan].cur;
		DBG(sc, INTR, ("tx%d: transmit done. %d bytes now free in "
		    "buffer", chan, sc->txslot[chan].bfree));
	}
	return (kick);
}

/*
 * TX DMA interrupt
 *
 * check for TX DMA complete, if detected then this means
 * that some DTQs are now free.   it also means some indma
 * mbufs can be freed. if we needed DTQs, kick all channels.
 *
 * LOCK: locked, needed
 */
static uint32_t
en_intr_tx_dma(struct en_softc *sc)
{
	uint32_t kick = 0;
	uint32_t val;
	uint32_t idx;
	uint32_t slot;
	uint32_t dtq;
	struct en_map *map;
	struct mbuf *m;

	val = en_read(sc, MID_DMA_RDTX); 	/* chip's current location */
	idx = MID_DTQ_A2REG(sc->dtq_chip);	/* where we last saw chip */

	if (sc->need_dtqs) {
		kick = MID_NTX_CH - 1;	/* assume power of 2, kick all! */
		sc->need_dtqs = 0;	/* recalculated in "kick" loop below */
		DBG(sc, INTR, ("cleared need DTQ condition"));
	}

	while (idx != val) {
		sc->dtq_free++;
		if ((dtq = sc->dtq[idx]) != 0) {
			/* don't forget to zero it out when done */
			sc->dtq[idx] = 0;
			slot = EN_DQ_SLOT(dtq);

			_IF_DEQUEUE(&sc->txslot[slot].indma, m);
			if (m == NULL)
				panic("enintr: dtqsync");
			map = (void *)m->m_pkthdr.rcvif;
			uma_zfree(sc->map_zone, map);
			m_freem(m);

			sc->txslot[slot].mbsize -= EN_DQ_LEN(dtq);
			DBG(sc, INTR, ("tx%d: free %d dma bytes, mbsize now "
			    "%d", slot, EN_DQ_LEN(dtq), 
			    sc->txslot[slot].mbsize));
		}
		EN_WRAPADD(0, MID_DTQ_N, idx, 1);
	}
	sc->dtq_chip = MID_DTQ_REG2A(val);	/* sync softc */

	return (kick);
}

/*
 * Service interrupt
 *
 * LOCK: locked, needed
 */
static int
en_intr_service(struct en_softc *sc)
{
	uint32_t chip;
	uint32_t vci;
	int need_softserv = 0;
	struct en_vcc *vc;

	chip = MID_SL_REG2A(en_read(sc, MID_SERV_WRITE));

	while (sc->hwslistp != chip) {
		/* fetch and remove it from hardware service list */
		vci = en_read(sc, sc->hwslistp);
		EN_WRAPADD(MID_SLOFF, MID_SLEND, sc->hwslistp, 4);

		if ((vc = sc->vccs[vci]) == NULL ||
		    (vc->vcc.flags & ATMIO_FLAG_NORX)) {
			DBG(sc, INTR, ("unexpected rx interrupt VCI %d", vci));
			en_write(sc, MID_VC(vci), MIDV_TRASH);  /* rx off */
			continue;
		}

		/* remove from hwsl */
		en_write(sc, MID_VC(vci), vc->rxslot->mode);
		EN_COUNT(sc->stats.hwpull);

		DBG(sc, INTR, ("pulled VCI %d off hwslist", vci));

		/* add it to the software service list (if needed) */
		if ((vc->vflags & VCC_SWSL) == 0) {
			EN_COUNT(sc->stats.swadd);
			need_softserv = 1;
			vc->vflags |= VCC_SWSL;
			sc->swslist[sc->swsl_tail] = vci;
			EN_WRAPADD(0, MID_SL_N, sc->swsl_tail, 1);
			sc->swsl_size++;
			DBG(sc, INTR, ("added VCI %d to swslist", vci));
		}
	}
	return (need_softserv);
}

/*
 * Handle a receive DMA completion
 */
static void
en_rx_drain(struct en_softc *sc, u_int drq)
{
	struct en_rxslot *slot;
	struct en_vcc *vc;
	struct mbuf *m;
	struct atm_pseudohdr ah;

	slot = &sc->rxslot[EN_DQ_SLOT(drq)];

	m = NULL;	/* assume "JK" trash DMA */
	if (EN_DQ_LEN(drq) != 0) {
		_IF_DEQUEUE(&slot->indma, m);
		KASSERT(m != NULL, ("drqsync: %s: lost mbuf in slot %td!",
		    sc->ifp->if_xname, slot - sc->rxslot));
		uma_zfree(sc->map_zone, (struct en_map *)m->m_pkthdr.rcvif);
	}
	if ((vc = slot->vcc) == NULL) {
		/* ups */
		if (m != NULL)
			m_freem(m);
		return;
	}

	/* do something with this mbuf */
	if (vc->vflags & VCC_DRAIN) {
		/* drain? */
		if (m != NULL)
			m_freem(m);
		if (_IF_QLEN(&slot->indma) == 0 && _IF_QLEN(&slot->q) == 0 &&
		    (en_read(sc, MID_VC(vc->vcc.vci)) & MIDV_INSERVICE) == 0 &&
		    (vc->vflags & VCC_SWSL) == 0) {
			vc->vflags &= ~VCC_CLOSE_RX;
			if (vc->vcc.flags & ATMIO_FLAG_ASYNC)
				en_close_finish(sc, vc);
			else
				cv_signal(&sc->cv_close);
		}
		return;
	}

	if (m != NULL) {
		ATM_PH_FLAGS(&ah) = vc->vcc.flags;
		ATM_PH_VPI(&ah) = 0;
		ATM_PH_SETVCI(&ah, vc->vcc.vci);

		DBG(sc, INTR, ("rx%td: rxvci%d: atm_input, mbuf %p, len %d, "
		    "hand %p", slot - sc->rxslot, vc->vcc.vci, m,
		    EN_DQ_LEN(drq), vc->rxhand));

		m->m_pkthdr.rcvif = sc->ifp;
		sc->ifp->if_ipackets++;

		vc->ipackets++;
		vc->ibytes += m->m_pkthdr.len;

#ifdef EN_DEBUG
		if (sc->debug & DBG_IPACKETS)
			en_dump_packet(sc, m);
#endif
#ifdef ENABLE_BPF
		BPF_MTAP(sc->ifp, m);
#endif
		EN_UNLOCK(sc);
		atm_input(sc->ifp, &ah, m, vc->rxhand);
		EN_LOCK(sc);
	}
}

/*
 * check for RX DMA complete, and pass the data "upstairs"
 *
 * LOCK: locked, needed
 */
static int
en_intr_rx_dma(struct en_softc *sc)
{
	uint32_t val;
	uint32_t idx;
	uint32_t drq;

	val = en_read(sc, MID_DMA_RDRX); 	/* chip's current location */
	idx = MID_DRQ_A2REG(sc->drq_chip);	/* where we last saw chip */

	while (idx != val) {
		sc->drq_free++;
		if ((drq = sc->drq[idx]) != 0) {
			/* don't forget to zero it out when done */
			sc->drq[idx] = 0;
			en_rx_drain(sc, drq);
		}
		EN_WRAPADD(0, MID_DRQ_N, idx, 1);
	}
	sc->drq_chip = MID_DRQ_REG2A(val);	/* sync softc */

	if (sc->need_drqs) {
		/* true if we had a DRQ shortage */
		sc->need_drqs = 0;
		DBG(sc, INTR, ("cleared need DRQ condition"));
		return (1);
	} else
		return (0);
}

/*
 * en_mget: get an mbuf chain that can hold totlen bytes and return it
 * (for recv). For the actual allocation totlen is rounded up to a multiple
 * of 4. We also ensure, that each mbuf has a multiple of 4 bytes.
 *
 * After this call the sum of all the m_len's in the chain will be totlen.
 * This is called at interrupt time, so we can't wait here.
 *
 * LOCK: any, not needed
 */
static struct mbuf *
en_mget(struct en_softc *sc, u_int pktlen)
{
	struct mbuf *m, *tmp;
	u_int totlen, pad;

	totlen = roundup(pktlen, sizeof(uint32_t));
	pad = totlen - pktlen;

	/*
	 * First get an mbuf with header. Keep space for a couple of
	 * words at the begin.
	 */
	/* called from interrupt context */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);

	m->m_pkthdr.rcvif = NULL;
	m->m_pkthdr.len = pktlen;
	m->m_len = EN_RX1BUF;
	MH_ALIGN(m, EN_RX1BUF);
	if (m->m_len >= totlen) {
		m->m_len = totlen;

	} else {
		totlen -= m->m_len;

		/* called from interrupt context */
		tmp = m_getm(m, totlen, M_DONTWAIT, MT_DATA);
		if (tmp == NULL) {
			m_free(m);
			return (NULL);
		}
		tmp = m->m_next;
		/* m_getm could do this for us */
		while (tmp != NULL) {
			tmp->m_len = min(MCLBYTES, totlen);
			totlen -= tmp->m_len;
			tmp = tmp->m_next;
		}
	}

	return (m);
}

/*
 * Argument for RX DMAMAP loader.
 */
struct rxarg {
	struct en_softc *sc;
	struct mbuf *m;
	u_int pre_skip;		/* number of bytes to skip at begin */
	u_int post_skip;	/* number of bytes to skip at end */
	struct en_vcc *vc;	/* vc we are receiving on */
	int wait;		/* wait for DRQ entries */
};

/*
 * Copy the segment table to the buffer for later use. And compute the
 * number of dma queue entries we need.
 *
 * LOCK: locked, needed
 */
static void
en_rxdma_load(void *uarg, bus_dma_segment_t *segs, int nseg,
    bus_size_t mapsize, int error)
{
	struct rxarg *rx = uarg;
	struct en_softc *sc = rx->sc;
	struct en_rxslot *slot = rx->vc->rxslot;
	u_int		free;		/* number of free DRQ entries */
	uint32_t	cur;		/* current buffer offset */
	uint32_t	drq;		/* DRQ entry pointer */
	uint32_t	last_drq;	/* where we have written last */
	u_int		needalign, cnt, count, bcode;
	bus_addr_t	addr;
	bus_size_t	rest;
	int		i;

	if (error != 0)
		return;
	if (nseg > EN_MAX_DMASEG)
		panic("too many DMA segments");

	rx->wait = 0;

	free = sc->drq_free;
	drq = sc->drq_us;
	cur = slot->cur;

	last_drq = 0;

	/*
	 * Local macro to add an entry to the receive DMA area. If there
	 * are no entries left, return. Save the byte offset of the entry
	 * in last_drq for later use.
	 */
#define PUT_DRQ_ENTRY(ENI, BCODE, COUNT, ADDR)				\
	if (free == 0) {						\
		EN_COUNT(sc->stats.rxdrqout);				\
		rx->wait = 1;						\
		return;							\
	}								\
	last_drq = drq;							\
	en_write(sc, drq + 0, (ENI || !sc->is_adaptec) ?		\
	    MID_MK_RXQ_ENI(COUNT, rx->vc->vcc.vci, 0, BCODE) :		\
	    MID_MK_RXQ_ADP(COUNT, rx->vc->vcc.vci, 0, BCODE));		\
	en_write(sc, drq + 4, ADDR);					\
									\
	EN_WRAPADD(MID_DRQOFF, MID_DRQEND, drq, 8);			\
	free--;

	/*
	 * Local macro to generate a DMA entry to DMA cnt bytes. Updates
	 * the current buffer byte offset accordingly.
	 */
#define DO_DRQ(TYPE) do {						\
	rest -= cnt;							\
	EN_WRAPADD(slot->start, slot->stop, cur, cnt);			\
	DBG(sc, SERV, ("rx%td: "TYPE" %u bytes, %ju left, cur %#x",	\
	    slot - sc->rxslot, cnt, (uintmax_t)rest, cur));		\
									\
	PUT_DRQ_ENTRY(1, bcode, count, addr);				\
									\
	addr += cnt;							\
    } while (0)

	/*
	 * Skip the RBD at the beginning
	 */
	if (rx->pre_skip > 0) {
		/* update DMA address */
		EN_WRAPADD(slot->start, slot->stop, cur, rx->pre_skip);

		PUT_DRQ_ENTRY(0, MIDDMA_JK, WORD_IDX(slot->start, cur), 0);
	}

	for (i = 0; i < nseg; i++, segs++) {
		addr = segs->ds_addr;
		rest = segs->ds_len;

		if (sc->is_adaptec) {
			/* adaptec card - simple */

			/* advance the on-card buffer pointer */
			EN_WRAPADD(slot->start, slot->stop, cur, rest);
			DBG(sc, SERV, ("rx%td: adp %ju bytes %#jx "
			    "(cur now 0x%x)", slot - sc->rxslot,
			    (uintmax_t)rest, (uintmax_t)addr, cur));

			PUT_DRQ_ENTRY(0, 0, rest, addr);

			continue;
		}

		/*
		 * do we need to do a DMA op to align to the maximum
		 * burst? Note, that we are alway 32-bit aligned.
		 */
		if (sc->alburst &&
		    (needalign = (addr & sc->bestburstmask)) != 0) {
			/* compute number of bytes, words and code */
			cnt = sc->bestburstlen - needalign;
			if (cnt > rest)
				cnt = rest;
			count = cnt / sizeof(uint32_t);
			if (sc->noalbursts) {
				bcode = MIDDMA_WORD;
			} else {
				bcode = en_dmaplan[count].bcode;
				count = cnt >> en_dmaplan[count].divshift;
			}
			DO_DRQ("al_dma");
		}

		/* do we need to do a max-sized burst? */
		if (rest >= sc->bestburstlen) {
			count = rest >> sc->bestburstshift;
			cnt = count << sc->bestburstshift;
			bcode = sc->bestburstcode;
			DO_DRQ("best_dma");
		}

		/* do we need to do a cleanup burst? */
		if (rest != 0) {
			cnt = rest;
			count = rest / sizeof(uint32_t);
			if (sc->noalbursts) {
				bcode = MIDDMA_WORD;
			} else {
				bcode = en_dmaplan[count].bcode;
				count = cnt >> en_dmaplan[count].divshift;
			}
			DO_DRQ("clean_dma");
		}
	}

	/*
	 * Skip stuff at the end
	 */
	if (rx->post_skip > 0) {
		/* update DMA address */
		EN_WRAPADD(slot->start, slot->stop, cur, rx->post_skip);

		PUT_DRQ_ENTRY(0, MIDDMA_JK, WORD_IDX(slot->start, cur), 0);
	}

	/* record the end for the interrupt routine */
	sc->drq[MID_DRQ_A2REG(last_drq)] =
	    EN_DQ_MK(slot - sc->rxslot, rx->m->m_pkthdr.len);

	/* set the end flag in the last descriptor */
	en_write(sc, last_drq + 0, SETQ_END(sc, en_read(sc, last_drq + 0)));

#undef PUT_DRQ_ENTRY
#undef DO_DRQ

	/* commit */
	slot->cur = cur;
	sc->drq_free = free;
	sc->drq_us = drq;

	/* signal to card */
	en_write(sc, MID_DMA_WRRX, MID_DRQ_A2REG(sc->drq_us));
}

/*
 * en_service: handle a service interrupt
 *
 * Q: why do we need a software service list?
 *
 * A: if we remove a VCI from the hardware list and we find that we are
 *    out of DRQs we must defer processing until some DRQs become free.
 *    so we must remember to look at this RX VCI/slot later, but we can't
 *    put it back on the hardware service list (since that isn't allowed).
 *    so we instead save it on the software service list.   it would be nice 
 *    if we could peek at the VCI on top of the hwservice list without removing
 *    it, however this leads to a race condition: if we peek at it and
 *    decide we are done with it new data could come in before we have a 
 *    chance to remove it from the hwslist.   by the time we get it out of
 *    the list the interrupt for the new data will be lost.   oops!
 *
 * LOCK: locked, needed
 */
static void
en_service(struct en_softc *sc)
{
	struct mbuf	*m, *lastm;
	struct en_map	*map;
	struct rxarg	rx;
	uint32_t	cur;
	uint32_t	dstart;		/* data start (as reported by card) */
	uint32_t	rbd;		/* receive buffer descriptor */
	uint32_t	pdu;		/* AAL5 trailer */
	int		mlen;
	int		error;
	struct en_rxslot *slot;
	struct en_vcc *vc;

	rx.sc = sc;

  next_vci:
	if (sc->swsl_size == 0) {
		DBG(sc, SERV, ("en_service done"));
		return;
	}

	/*
	 * get vcc to service
	 */
	rx.vc = vc = sc->vccs[sc->swslist[sc->swsl_head]];
	slot = vc->rxslot;
	KASSERT (slot->vcc->rxslot == slot, ("en_service: rx slot/vci sync"));

	/*
	 * determine our mode and if we've got any work to do
	 */
	DBG(sc, SERV, ("rx%td: service vci=%d start/stop/cur=0x%x 0x%x "
	    "0x%x", slot - sc->rxslot, vc->vcc.vci, slot->start,
	    slot->stop, slot->cur));

  same_vci:
	cur = slot->cur;

	dstart = MIDV_DSTART(en_read(sc, MID_DST_RP(vc->vcc.vci)));
	dstart = (dstart * sizeof(uint32_t)) + slot->start;

	/* check to see if there is any data at all */
	if (dstart == cur) {
		EN_WRAPADD(0, MID_SL_N, sc->swsl_head, 1); 
		/* remove from swslist */
		vc->vflags &= ~VCC_SWSL;
		sc->swsl_size--;
		DBG(sc, SERV, ("rx%td: remove vci %d from swslist",
		    slot - sc->rxslot, vc->vcc.vci));
		goto next_vci;
	}

	/*
	 * figure out how many bytes we need
	 * [mlen = # bytes to go in mbufs]
	 */
	rbd = en_read(sc, cur);
	if (MID_RBD_ID(rbd) != MID_RBD_STDID) 
		panic("en_service: id mismatch");

	if (rbd & MID_RBD_T) {
		mlen = 0;		/* we've got trash */
		rx.pre_skip = MID_RBD_SIZE;
		rx.post_skip = 0;
		EN_COUNT(sc->stats.ttrash);
		DBG(sc, SERV, ("RX overflow lost %d cells!", MID_RBD_CNT(rbd)));

	} else if (vc->vcc.aal != ATMIO_AAL_5) {
		/* 1 cell (ick!) */
		mlen = MID_CHDR_SIZE + MID_ATMDATASZ;
		rx.pre_skip = MID_RBD_SIZE;
		rx.post_skip = 0;

	} else {
		rx.pre_skip = MID_RBD_SIZE;

		/* get PDU trailer in correct byte order */
		pdu = cur + MID_RBD_CNT(rbd) * MID_ATMDATASZ +
		    MID_RBD_SIZE - MID_PDU_SIZE;
		if (pdu >= slot->stop)
			pdu -= EN_RXSZ * 1024;
		pdu = en_read(sc, pdu);

		if (MID_RBD_CNT(rbd) * MID_ATMDATASZ <
		    MID_PDU_LEN(pdu)) {
			device_printf(sc->dev, "invalid AAL5 length\n");
			rx.post_skip = MID_RBD_CNT(rbd) * MID_ATMDATASZ;
			mlen = 0;
			sc->ifp->if_ierrors++;

		} else if (rbd & MID_RBD_CRCERR) {
			device_printf(sc->dev, "CRC error\n");
			rx.post_skip = MID_RBD_CNT(rbd) * MID_ATMDATASZ;
			mlen = 0;
			sc->ifp->if_ierrors++;

		} else {
			mlen = MID_PDU_LEN(pdu);
			rx.post_skip = MID_RBD_CNT(rbd) * MID_ATMDATASZ - mlen;
		}
	}

	/*
	 * now allocate mbufs for mlen bytes of data, if out of mbufs, trash all
	 *
	 * notes:
	 *  1. it is possible that we've already allocated an mbuf for this pkt
	 *     but ran out of DRQs, in which case we saved the allocated mbuf
	 *     on "q".
	 *  2. if we save an buf in "q" we store the "cur" (pointer) in the
	 *     buf as an identity (that we can check later).
	 *  3. after this block of code, if m is still NULL then we ran out of
	 *     mbufs
	 */
	_IF_DEQUEUE(&slot->q, m);
	if (m != NULL) {
		if (m->m_pkthdr.csum_data != cur) {
			/* wasn't ours */
			DBG(sc, SERV, ("rx%td: q'ed buf %p not ours",
			    slot - sc->rxslot, m));
			_IF_PREPEND(&slot->q, m);
			m = NULL;
			EN_COUNT(sc->stats.rxqnotus);
		} else {
			EN_COUNT(sc->stats.rxqus);
			DBG(sc, SERV, ("rx%td: recovered q'ed buf %p",
			    slot - sc->rxslot, m));
		}
	}
	if (mlen == 0 && m != NULL) {
		/* should not happen */
		m_freem(m);
		m = NULL;
	}

	if (mlen != 0 && m == NULL) {
		m = en_mget(sc, mlen);
		if (m == NULL) {
			rx.post_skip += mlen;
			mlen = 0;
			EN_COUNT(sc->stats.rxmbufout);
			DBG(sc, SERV, ("rx%td: out of mbufs",
			    slot - sc->rxslot));
		} else
			rx.post_skip -= roundup(mlen, sizeof(uint32_t)) - mlen;

		DBG(sc, SERV, ("rx%td: allocate buf %p, mlen=%d",
		    slot - sc->rxslot, m, mlen));
	}

	DBG(sc, SERV, ("rx%td: VCI %d, rbuf %p, mlen %d, skip %u/%u",
	    slot - sc->rxslot, vc->vcc.vci, m, mlen, rx.pre_skip,
	    rx.post_skip));

	if (m != NULL) {
		/* M_NOWAIT - called from interrupt context */
		map = uma_zalloc_arg(sc->map_zone, sc, M_NOWAIT);
		if (map == NULL) {
			rx.post_skip += mlen;
			m_freem(m);
			DBG(sc, SERV, ("rx%td: out of maps",
			    slot - sc->rxslot));
			goto skip;
		}
		rx.m = m;
		error = bus_dmamap_load_mbuf(sc->txtag, map->map, m,
		    en_rxdma_load, &rx, BUS_DMA_NOWAIT);

		if (error != 0) {
			device_printf(sc->dev, "loading RX map failed "
			    "%d\n", error);
			uma_zfree(sc->map_zone, map);
			m_freem(m);
			rx.post_skip += mlen;
			goto skip;

		}
		map->flags |= ENMAP_LOADED;

		if (rx.wait) {
			/* out of DRQs - wait */
			uma_zfree(sc->map_zone, map);

			m->m_pkthdr.csum_data = cur;
			_IF_ENQUEUE(&slot->q, m);
			EN_COUNT(sc->stats.rxdrqout);

			sc->need_drqs = 1;	/* flag condition */
			return;

		}
		(void)m_length(m, &lastm);
		lastm->m_len -= roundup(mlen, sizeof(uint32_t)) - mlen;

		m->m_pkthdr.rcvif = (void *)map;
		_IF_ENQUEUE(&slot->indma, m);

		/* get next packet in this slot */
		goto same_vci;
	}
  skip:
	/*
	 * Here we end if we should drop the packet from the receive buffer.
	 * The number of bytes to drop is in fill. We can do this with on
	 * JK entry. If we don't even have that one - wait.
	 */
	if (sc->drq_free == 0) {
		sc->need_drqs = 1;	/* flag condition */
		return;
	}
	rx.post_skip += rx.pre_skip;
	DBG(sc, SERV, ("rx%td: skipping %u", slot - sc->rxslot, rx.post_skip));

	/* advance buffer address */
	EN_WRAPADD(slot->start, slot->stop, cur, rx.post_skip);

	/* write DRQ entry */
	if (sc->is_adaptec)
		en_write(sc, sc->drq_us,
		    MID_MK_RXQ_ADP(WORD_IDX(slot->start, cur),
		    vc->vcc.vci, MID_DMA_END, MIDDMA_JK));
	else
	  	en_write(sc, sc->drq_us,
		    MID_MK_RXQ_ENI(WORD_IDX(slot->start, cur),
		    vc->vcc.vci, MID_DMA_END, MIDDMA_JK));
	en_write(sc, sc->drq_us + 4, 0);
	EN_WRAPADD(MID_DRQOFF, MID_DRQEND, sc->drq_us, 8);
	sc->drq_free--;

	/* signal to RX interrupt */
	sc->drq[MID_DRQ_A2REG(sc->drq_us)] = EN_DQ_MK(slot - sc->rxslot, 0);
	slot->cur = cur;

	/* signal to card */
	en_write(sc, MID_DMA_WRRX, MID_DRQ_A2REG(sc->drq_us));

	goto same_vci;
}

/*
 * interrupt handler
 *
 * LOCK: unlocked, needed
 */
void
en_intr(void *arg)
{
	struct en_softc *sc = arg;
	uint32_t reg, kick, mask;
	int lcv, need_softserv;

	EN_LOCK(sc);

	reg = en_read(sc, MID_INTACK);
	DBG(sc, INTR, ("interrupt=0x%b", reg, MID_INTBITS));

	if ((reg & MID_INT_ANY) == 0) {
		EN_UNLOCK(sc);
		return;
	}

	/*
	 * unexpected errors that need a reset
	 */
	if ((reg & (MID_INT_IDENT | MID_INT_LERR | MID_INT_DMA_ERR)) != 0) {
		device_printf(sc->dev, "unexpected interrupt=0x%b, "
		    "resetting\n", reg, MID_INTBITS);
#ifdef EN_DEBUG
		panic("en: unexpected error");
#else
		en_reset_ul(sc);
		en_init(sc);
#endif
		EN_UNLOCK(sc);
		return;
	}

	if (reg & MID_INT_SUNI)
		utopia_intr(&sc->utopia);

	kick = 0;
	if (reg & MID_INT_TX)
		kick |= en_intr_tx(sc, reg);

	if (reg & MID_INT_DMA_TX)
		kick |= en_intr_tx_dma(sc);

	/*
	 * kick xmit channels as needed.
	 */
	if (kick) {
		DBG(sc, INTR, ("tx kick mask = 0x%x", kick));
		for (mask = 1, lcv = 0 ; lcv < EN_NTX ; lcv++, mask = mask * 2)
			if ((kick & mask) && _IF_QLEN(&sc->txslot[lcv].q) != 0)
				en_txdma(sc, &sc->txslot[lcv]);
	}

	need_softserv = 0;
	if (reg & MID_INT_DMA_RX)
		need_softserv |= en_intr_rx_dma(sc);

	if (reg & MID_INT_SERVICE)
		need_softserv |= en_intr_service(sc);

	if (need_softserv)
		en_service(sc);

	/*
	 * keep our stats
	 */
	if (reg & MID_INT_DMA_OVR) {
		EN_COUNT(sc->stats.dmaovr);
		DBG(sc, INTR, ("MID_INT_DMA_OVR"));
	}
	reg = en_read(sc, MID_STAT);
	sc->stats.otrash += MID_OTRASH(reg);
	sc->stats.vtrash += MID_VTRASH(reg);

	EN_UNLOCK(sc);
}

/*
 * Read at most n SUNI regs starting at reg into val
 */
static int
en_utopia_readregs(struct ifatm *ifatm, u_int reg, uint8_t *val, u_int *n)
{
	struct en_softc *sc = ifatm->ifp->if_softc;
	u_int i;

	EN_CHECKLOCK(sc);
	if (reg >= MID_NSUNI)
		return (EINVAL);
	if (reg + *n > MID_NSUNI)
		*n = MID_NSUNI - reg;

	for (i = 0; i < *n; i++)
		val[i] = en_read(sc, MID_SUNIOFF + 4 * (reg + i));

	return (0);
}

/*
 * change the bits given by mask to them in val in register reg
 */
static int
en_utopia_writereg(struct ifatm *ifatm, u_int reg, u_int mask, u_int val)
{
	struct en_softc *sc = ifatm->ifp->if_softc;
	uint32_t regval;

	EN_CHECKLOCK(sc);
	if (reg >= MID_NSUNI)
		return (EINVAL);
	regval = en_read(sc, MID_SUNIOFF + 4 * reg);
	regval = (regval & ~mask) | (val & mask);
	en_write(sc, MID_SUNIOFF + 4 * reg, regval);
	return (0);
}

static const struct utopia_methods en_utopia_methods = {
	en_utopia_readregs,
	en_utopia_writereg
};

/*********************************************************************/
/*
 * Probing the DMA brokeness of the card
 */

/*
 * Physical address load helper function for DMA probe
 *
 * LOCK: unlocked, not needed
 */
static void
en_dmaprobe_load(void *uarg, bus_dma_segment_t *segs, int nseg, int error)
{
	if (error == 0)
		*(bus_addr_t *)uarg = segs[0].ds_addr;
}

/*
 * en_dmaprobe: helper function for en_attach.
 *
 * see how the card handles DMA by running a few DMA tests.   we need
 * to figure out the largest number of bytes we can DMA in one burst
 * ("bestburstlen"), and if the starting address for a burst needs to
 * be aligned on any sort of boundary or not ("alburst").
 *
 * Things turn out more complex than that, because on my (harti) brand
 * new motherboard (2.4GHz) we can do 64byte aligned DMAs, but everything
 * we more than 4 bytes fails (with an RX DMA timeout) for physical
 * addresses that end with 0xc. Therefor we search not only the largest
 * burst that is supported (hopefully 64) but also check what is the largerst
 * unaligned supported size. If that appears to be lesser than 4 words,
 * set the noalbursts flag. That will be set only if also alburst is set.
 */

/*
 * en_dmaprobe_doit: do actual testing for the DMA test.
 * Cycle through all bursts sizes from 8 up to 64 and try whether it works.
 * Return the largest one that works.
 *
 * LOCK: unlocked, not needed
 */
static int
en_dmaprobe_doit(struct en_softc *sc, uint8_t *sp, bus_addr_t psp)
{
	uint8_t *dp = sp + MIDDMA_MAXBURST;
	bus_addr_t pdp = psp + MIDDMA_MAXBURST;
	int lcv, retval = 4, cnt;
	uint32_t reg, bcode, midvloc;

	if (sc->en_busreset)
		sc->en_busreset(sc);
	en_write(sc, MID_RESID, 0x0);	/* reset card before touching RAM */

	/*
	 * set up a 1k buffer at MID_BUFOFF
	 */
	midvloc = ((MID_BUFOFF - MID_RAMOFF) / sizeof(uint32_t))
	    >> MIDV_LOCTOPSHFT;
	en_write(sc, MIDX_PLACE(0), MIDX_MKPLACE(en_k2sz(1), midvloc));
	en_write(sc, MID_VC(0), (midvloc << MIDV_LOCSHIFT) 
	    | (en_k2sz(1) << MIDV_SZSHIFT) | MIDV_TRASH);
	en_write(sc, MID_DST_RP(0), 0);
	en_write(sc, MID_WP_ST_CNT(0), 0);

 	/* set up sample data */
	for (lcv = 0 ; lcv < MIDDMA_MAXBURST; lcv++)
		sp[lcv] = lcv + 1;

	/* enable DMA (only) */
	en_write(sc, MID_MAST_CSR, MID_MCSR_ENDMA);

	sc->drq_chip = MID_DRQ_REG2A(en_read(sc, MID_DMA_RDRX));
	sc->dtq_chip = MID_DTQ_REG2A(en_read(sc, MID_DMA_RDTX));

	/*
	 * try it now . . .  DMA it out, then DMA it back in and compare
	 *
	 * note: in order to get the dma stuff to reverse directions it wants
	 * the "end" flag set!   since we are not dma'ing valid data we may
	 * get an ident mismatch interrupt (which we will ignore).
	 */
	DBG(sc, DMA, ("test sp=%p/%#lx, dp=%p/%#lx", 
	    sp, (u_long)psp, dp, (u_long)pdp));
	for (lcv = 8 ; lcv <= MIDDMA_MAXBURST ; lcv = lcv * 2) {
		DBG(sc, DMA, ("test lcv=%d", lcv));

		/* zero SRAM and dest buffer */
		bus_space_set_region_4(sc->en_memt, sc->en_base,
		    MID_BUFOFF, 0, 1024 / 4);
		bzero(dp, MIDDMA_MAXBURST);

		bcode = en_sz2b(lcv);

		/* build lcv-byte-DMA x NBURSTS */
		if (sc->is_adaptec)
			en_write(sc, sc->dtq_chip,
			    MID_MK_TXQ_ADP(lcv, 0, MID_DMA_END, 0));
		else
			en_write(sc, sc->dtq_chip,
			    MID_MK_TXQ_ENI(1, 0, MID_DMA_END, bcode));
		en_write(sc, sc->dtq_chip + 4, psp);
		EN_WRAPADD(MID_DTQOFF, MID_DTQEND, sc->dtq_chip, 8);
		en_write(sc, MID_DMA_WRTX, MID_DTQ_A2REG(sc->dtq_chip));

		cnt = 1000;
		while ((reg = en_readx(sc, MID_DMA_RDTX)) !=
		    MID_DTQ_A2REG(sc->dtq_chip)) {
			DELAY(1);
			if (--cnt == 0) {
				DBG(sc, DMA, ("unexpected timeout in tx "
				    "DMA test\n  alignment=0x%lx, burst size=%d"
				    ", dma addr reg=%#x, rdtx=%#x, stat=%#x\n",
				    (u_long)sp & 63, lcv,
				    en_read(sc, MID_DMA_ADDR), reg,
				    en_read(sc, MID_INTSTAT)));
				return (retval);
			}
		}

		reg = en_read(sc, MID_INTACK); 
		if ((reg & MID_INT_DMA_TX) != MID_INT_DMA_TX) {
			DBG(sc, DMA, ("unexpected status in tx DMA test: %#x\n",
			    reg));
			return (retval);
		}
		/* re-enable DMA (only) */
		en_write(sc, MID_MAST_CSR, MID_MCSR_ENDMA);

		/* "return to sender..."  address is known ... */

		/* build lcv-byte-DMA x NBURSTS */
		if (sc->is_adaptec)
			en_write(sc, sc->drq_chip,
			    MID_MK_RXQ_ADP(lcv, 0, MID_DMA_END, 0));
		else
			en_write(sc, sc->drq_chip,
			    MID_MK_RXQ_ENI(1, 0, MID_DMA_END, bcode));
		en_write(sc, sc->drq_chip + 4, pdp);
		EN_WRAPADD(MID_DRQOFF, MID_DRQEND, sc->drq_chip, 8);
		en_write(sc, MID_DMA_WRRX, MID_DRQ_A2REG(sc->drq_chip));
		cnt = 1000;
		while ((reg = en_readx(sc, MID_DMA_RDRX)) !=
		    MID_DRQ_A2REG(sc->drq_chip)) {
			DELAY(1);
			cnt--;
			if (--cnt == 0) {
				DBG(sc, DMA, ("unexpected timeout in rx "
				    "DMA test, rdrx=%#x\n", reg));
				return (retval);
			}
		}
		reg = en_read(sc, MID_INTACK); 
		if ((reg & MID_INT_DMA_RX) != MID_INT_DMA_RX) {
			DBG(sc, DMA, ("unexpected status in rx DMA "
			    "test: 0x%x\n", reg));
			return (retval);
		}
		if (bcmp(sp, dp, lcv)) {
			DBG(sc, DMA, ("DMA test failed! lcv=%d, sp=%p, "
			    "dp=%p", lcv, sp, dp));
			return (retval);
		}

		retval = lcv;
	}
	return (retval);	/* studly 64 byte DMA present!  oh baby!! */
}

/*
 * Find the best DMA parameters
 *
 * LOCK: unlocked, not needed
 */
static void
en_dmaprobe(struct en_softc *sc)
{
	bus_dma_tag_t tag;
	bus_dmamap_t map;
	int err;
	void *buffer;
	int bestalgn, lcv, try, bestnoalgn;
	bus_addr_t phys;
	uint8_t *addr;

	sc->alburst = 0;
	sc->noalbursts = 0;

	/*
	 * Allocate some DMA-able memory.
	 * We need 3 times the max burst size aligned to the max burst size.
	 */
	err = bus_dma_tag_create(NULL, MIDDMA_MAXBURST, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    3 * MIDDMA_MAXBURST, 1, 3 * MIDDMA_MAXBURST, 0,
	    NULL, NULL, &tag);
	if (err)
		panic("%s: cannot create test DMA tag %d", __func__, err);

	err = bus_dmamem_alloc(tag, &buffer, 0, &map);
	if (err)
		panic("%s: cannot allocate test DMA memory %d", __func__, err);

	err = bus_dmamap_load(tag, map, buffer, 3 * MIDDMA_MAXBURST,
	    en_dmaprobe_load, &phys, BUS_DMA_NOWAIT);
	if (err)
		panic("%s: cannot load test DMA map %d", __func__, err);
	addr = buffer;
	DBG(sc, DMA, ("phys=%#lx addr=%p", (u_long)phys, addr));

	/*
	 * Now get the best burst size of the aligned case.
	 */
	bestalgn = bestnoalgn = en_dmaprobe_doit(sc, addr, phys);

	/*
	 * Now try unaligned. 
	 */
	for (lcv = 4; lcv < MIDDMA_MAXBURST; lcv += 4) {
		try = en_dmaprobe_doit(sc, addr + lcv, phys + lcv);

		if (try < bestnoalgn)
			bestnoalgn = try;
	}

	if (bestnoalgn < bestalgn) {
		sc->alburst = 1;
		if (bestnoalgn < 32)
			sc->noalbursts = 1;
	}

	sc->bestburstlen = bestalgn;
	sc->bestburstshift = en_log2(bestalgn);
	sc->bestburstmask = sc->bestburstlen - 1; /* must be power of 2 */
	sc->bestburstcode = en_sz2b(bestalgn);

	/*
	 * Reset the chip before freeing the buffer. It may still be trying
	 * to DMA.
	 */
	if (sc->en_busreset)
		sc->en_busreset(sc);
	en_write(sc, MID_RESID, 0x0);	/* reset card before touching RAM */

	DELAY(10000);			/* may still do DMA */

	/*
	 * Free the DMA stuff
	 */
	bus_dmamap_unload(tag, map);
	bus_dmamem_free(tag, buffer, map);
	bus_dma_tag_destroy(tag);
}

/*********************************************************************/
/*
 * Attach/detach.
 */

/*
 * Attach to the card.
 *
 * LOCK: unlocked, not needed (but initialized)
 */
int
en_attach(struct en_softc *sc)
{
	struct ifnet *ifp = sc->ifp;
	int sz;
	uint32_t reg, lcv, check, ptr, sav, midvloc;

#ifdef EN_DEBUG
	sc->debug = EN_DEBUG;
#endif

	/*
	 * Probe card to determine memory size.
	 *
	 * The stupid ENI card always reports to PCI that it needs 4MB of
	 * space (2MB regs and 2MB RAM). If it has less than 2MB RAM the
	 * addresses wrap in the RAM address space (i.e. on a 512KB card
	 * addresses 0x3ffffc, 0x37fffc, and 0x2ffffc are aliases for
	 * 0x27fffc  [note that RAM starts at offset 0x200000]).
	 */

	/* reset card before touching RAM */
	if (sc->en_busreset)
		sc->en_busreset(sc);
	en_write(sc, MID_RESID, 0x0);

	for (lcv = MID_PROBEOFF; lcv <= MID_MAXOFF ; lcv += MID_PROBSIZE) {
		en_write(sc, lcv, lcv);	/* data[address] = address */
		for (check = MID_PROBEOFF; check < lcv ;check += MID_PROBSIZE) {
			reg = en_read(sc, check);
			if (reg != check)
				/* found an alias! - quit */
				goto done_probe;
		}
	}
  done_probe:
	lcv -= MID_PROBSIZE;			/* take one step back */
	sc->en_obmemsz = (lcv + 4) - MID_RAMOFF;

	/*
	 * determine the largest DMA burst supported
	 */
	en_dmaprobe(sc);

	/*
	 * "hello world"
	 */

	/* reset */
	if (sc->en_busreset)
		sc->en_busreset(sc);
	en_write(sc, MID_RESID, 0x0);		/* reset */

	/* zero memory */
	bus_space_set_region_4(sc->en_memt, sc->en_base,
	    MID_RAMOFF, 0, sc->en_obmemsz / 4);

	reg = en_read(sc, MID_RESID);

	device_printf(sc->dev, "ATM midway v%d, board IDs %d.%d, %s%s%s, "
	    "%ldKB on-board RAM\n", MID_VER(reg), MID_MID(reg), MID_DID(reg), 
	    (MID_IS_SABRE(reg)) ? "sabre controller, " : "",
	    (MID_IS_SUNI(reg)) ? "SUNI" : "Utopia",
	    (!MID_IS_SUNI(reg) && MID_IS_UPIPE(reg)) ? " (pipelined)" : "",
	    (long)sc->en_obmemsz / 1024);

	/*
	 * fill in common ATM interface stuff
	 */
	IFP2IFATM(sc->ifp)->mib.hw_version = (MID_VER(reg) << 16) |
	    (MID_MID(reg) << 8) | MID_DID(reg);
	if (MID_DID(reg) & 0x4)
		IFP2IFATM(sc->ifp)->mib.media = IFM_ATM_UTP_155;
	else
		IFP2IFATM(sc->ifp)->mib.media = IFM_ATM_MM_155;

	IFP2IFATM(sc->ifp)->mib.pcr = ATM_RATE_155M;
	IFP2IFATM(sc->ifp)->mib.vpi_bits = 0;
	IFP2IFATM(sc->ifp)->mib.vci_bits = MID_VCI_BITS;
	IFP2IFATM(sc->ifp)->mib.max_vccs = MID_N_VC;
	IFP2IFATM(sc->ifp)->mib.max_vpcs = 0;

	if (sc->is_adaptec) {
		IFP2IFATM(sc->ifp)->mib.device = ATM_DEVICE_ADP155P;
		if (sc->bestburstlen == 64 && sc->alburst == 0)
			device_printf(sc->dev,
			    "passed 64 byte DMA test\n");
		else
			device_printf(sc->dev, "FAILED DMA TEST: "
			    "burst=%d, alburst=%d\n", sc->bestburstlen,
			    sc->alburst);
	} else {
		IFP2IFATM(sc->ifp)->mib.device = ATM_DEVICE_ENI155P;
		device_printf(sc->dev, "maximum DMA burst length = %d "
		    "bytes%s\n", sc->bestburstlen, sc->alburst ?
		    sc->noalbursts ?  " (no large bursts)" : " (must align)" :
		    "");
	}

	/*
	 * link into network subsystem and prepare card
	 */
	sc->ifp->if_softc = sc;
	ifp->if_flags = IFF_SIMPLEX;
	ifp->if_ioctl = en_ioctl;
	ifp->if_start = en_start;

	mtx_init(&sc->en_mtx, device_get_nameunit(sc->dev),
	    MTX_NETWORK_LOCK, MTX_DEF);
	cv_init(&sc->cv_close, "VC close");

	/*
	 * Make the sysctl tree
	 */
	sysctl_ctx_init(&sc->sysctl_ctx);

	if ((sc->sysctl_tree = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
	    SYSCTL_STATIC_CHILDREN(_hw_atm), OID_AUTO,
	    device_get_nameunit(sc->dev), CTLFLAG_RD, 0, "")) == NULL)
		goto fail;

	if (SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "istats", CTLTYPE_OPAQUE | CTLFLAG_RD, sc, 0,
	    en_sysctl_istats, "S", "internal statistics") == NULL)
		goto fail;

#ifdef EN_DEBUG
	if (SYSCTL_ADD_UINT(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "debug", CTLFLAG_RW , &sc->debug, 0, "") == NULL)
		goto fail;
#endif

	IFP2IFATM(sc->ifp)->phy = &sc->utopia;
	utopia_attach(&sc->utopia, IFP2IFATM(sc->ifp), &sc->media, &sc->en_mtx,
	    &sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    &en_utopia_methods);
	utopia_init_media(&sc->utopia);

	MGET(sc->padbuf, M_WAIT, MT_DATA);
	bzero(sc->padbuf->m_data, MLEN);

	if (bus_dma_tag_create(NULL, 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    EN_TXSZ * 1024, EN_MAX_DMASEG, EN_TXSZ * 1024, 0,
	    NULL, NULL, &sc->txtag))
		goto fail;

	sc->map_zone = uma_zcreate("en dma maps", sizeof(struct en_map),
	    en_map_ctor, en_map_dtor, NULL, en_map_fini, UMA_ALIGN_PTR,
	    UMA_ZONE_ZINIT);
	if (sc->map_zone == NULL)
		goto fail;
	uma_zone_set_max(sc->map_zone, EN_MAX_MAPS);

	/*
	 * init softc
	 */
	sc->vccs = malloc(MID_N_VC * sizeof(sc->vccs[0]),
	    M_DEVBUF, M_ZERO | M_WAITOK);

	sz = sc->en_obmemsz - (MID_BUFOFF - MID_RAMOFF);
	ptr = sav = MID_BUFOFF;
	ptr = roundup(ptr, EN_TXSZ * 1024);	/* align */
	sz = sz - (ptr - sav);
	if (EN_TXSZ*1024 * EN_NTX > sz) {
		device_printf(sc->dev, "EN_NTX/EN_TXSZ too big\n");
		goto fail;
	}
	for (lcv = 0 ;lcv < EN_NTX ;lcv++) {
		sc->txslot[lcv].mbsize = 0;
		sc->txslot[lcv].start = ptr;
		ptr += (EN_TXSZ * 1024);
		sz -= (EN_TXSZ * 1024);
		sc->txslot[lcv].stop = ptr;
		sc->txslot[lcv].nref = 0;
		DBG(sc, INIT, ("tx%d: start 0x%x, stop 0x%x", lcv,
		    sc->txslot[lcv].start, sc->txslot[lcv].stop));
	}

	sav = ptr;
	ptr = roundup(ptr, EN_RXSZ * 1024);	/* align */
	sz = sz - (ptr - sav);
	sc->en_nrx = sz / (EN_RXSZ * 1024);
	if (sc->en_nrx <= 0) {
		device_printf(sc->dev, "EN_NTX/EN_TXSZ/EN_RXSZ too big\n");
		goto fail;
	}

	/* 
	 * ensure that there is always one VC slot on the service list free
	 * so that we can tell the difference between a full and empty list.
	 */
	if (sc->en_nrx >= MID_N_VC)
		sc->en_nrx = MID_N_VC - 1;

	for (lcv = 0 ; lcv < sc->en_nrx ; lcv++) {
		sc->rxslot[lcv].vcc = NULL;
		midvloc = sc->rxslot[lcv].start = ptr;
		ptr += (EN_RXSZ * 1024);
		sz -= (EN_RXSZ * 1024);
		sc->rxslot[lcv].stop = ptr;
		midvloc = midvloc - MID_RAMOFF;
		/* mask, cvt to words */
		midvloc = (midvloc & ~((EN_RXSZ*1024) - 1)) >> 2;
		/* we only want the top 11 bits */
		midvloc = midvloc >> MIDV_LOCTOPSHFT;
		midvloc = (midvloc & MIDV_LOCMASK) << MIDV_LOCSHIFT;
		sc->rxslot[lcv].mode = midvloc | 
		    (en_k2sz(EN_RXSZ) << MIDV_SZSHIFT) | MIDV_TRASH;

		DBG(sc, INIT, ("rx%d: start 0x%x, stop 0x%x, mode 0x%x", lcv,
		    sc->rxslot[lcv].start, sc->rxslot[lcv].stop,
		    sc->rxslot[lcv].mode));
	}

	device_printf(sc->dev, "%d %dKB receive buffers, %d %dKB transmit "
	    "buffers\n", sc->en_nrx, EN_RXSZ, EN_NTX, EN_TXSZ);
	device_printf(sc->dev, "end station identifier (mac address) "
	    "%6D\n", IFP2IFATM(sc->ifp)->mib.esi, ":");

	/*
	 * Start SUNI stuff. This will call our readregs/writeregs
	 * functions and these assume the lock to be held so we must get it
	 * here.
	 */
	EN_LOCK(sc);
	utopia_start(&sc->utopia);
	utopia_reset(&sc->utopia);
	EN_UNLOCK(sc);

	/*
	 * final commit
	 */
	atm_ifattach(ifp); 

#ifdef ENABLE_BPF
	bpfattach(ifp, DLT_ATM_RFC1483, sizeof(struct atmllc));
#endif

	return (0);

 fail:
	en_destroy(sc);
	return (-1);
}

/*
 * Free all internal resources. No access to bus resources here.
 * No locking required here (interrupt is already disabled).
 *
 * LOCK: unlocked, needed (but destroyed)
 */
void
en_destroy(struct en_softc *sc)
{
	u_int i;

	if (sc->utopia.state & UTP_ST_ATTACHED) {
		/* these assume the lock to be held */
		EN_LOCK(sc);
		utopia_stop(&sc->utopia);
		utopia_detach(&sc->utopia);
		EN_UNLOCK(sc);
	}

	if (sc->vccs != NULL) {
		/* get rid of sticky VCCs */
		for (i = 0; i < MID_N_VC; i++)
			if (sc->vccs[i] != NULL)
				uma_zfree(en_vcc_zone, sc->vccs[i]);
		free(sc->vccs, M_DEVBUF);
	}

	if (sc->padbuf != NULL)
		m_free(sc->padbuf);

	/*
	 * Destroy the map zone before the tag (the fini function will
	 * destroy the DMA maps using the tag)
	 */
	if (sc->map_zone != NULL)
		uma_zdestroy(sc->map_zone);

	if (sc->txtag != NULL)
		bus_dma_tag_destroy(sc->txtag);

	(void)sysctl_ctx_free(&sc->sysctl_ctx);

	cv_destroy(&sc->cv_close);
	mtx_destroy(&sc->en_mtx);
}

/*
 * Module loaded/unloaded
 */
int
en_modevent(module_t mod __unused, int event, void *arg __unused)
{

	switch (event) {

	  case MOD_LOAD:
		en_vcc_zone = uma_zcreate("EN vccs", sizeof(struct en_vcc),
		    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
		if (en_vcc_zone == NULL)
			return (ENOMEM);
		break;

	  case MOD_UNLOAD:
		uma_zdestroy(en_vcc_zone);
		break;
	}
	return (0);
}

/*********************************************************************/
/*
 * Debugging support
 */

#ifdef EN_DDBHOOK
/*
 * functions we can call from ddb
 */

/*
 * en_dump: dump the state
 */
#define END_SWSL	0x00000040		/* swsl state */
#define END_DRQ		0x00000020		/* drq state */
#define END_DTQ		0x00000010		/* dtq state */
#define END_RX		0x00000008		/* rx state */
#define END_TX		0x00000004		/* tx state */
#define END_MREGS	0x00000002		/* registers */
#define END_STATS	0x00000001		/* dump stats */

#define END_BITS "\20\7SWSL\6DRQ\5DTQ\4RX\3TX\2MREGS\1STATS"

static void
en_dump_stats(const struct en_stats *s)
{
	printf("en_stats:\n");
	printf("\t%d/%d mfix (%d failed)\n", s->mfixaddr, s->mfixlen,
	    s->mfixfail);
	printf("\t%d rx dma overflow interrupts\n", s->dmaovr);
	printf("\t%d times out of TX space and stalled\n", s->txoutspace);
	printf("\t%d times out of DTQs\n", s->txdtqout);
	printf("\t%d times launched a packet\n", s->launch);
	printf("\t%d times pulled the hw service list\n", s->hwpull);
	printf("\t%d times pushed a vci on the sw service list\n", s->swadd);
	printf("\t%d times RX pulled an mbuf from Q that wasn't ours\n",
	    s->rxqnotus);
	printf("\t%d times RX pulled a good mbuf from Q\n", s->rxqus);
	printf("\t%d times ran out of DRQs\n", s->rxdrqout);
	printf("\t%d transmit packets dropped due to mbsize\n", s->txmbovr);
	printf("\t%d cells trashed due to turned off rxvc\n", s->vtrash);
	printf("\t%d cells trashed due to totally full buffer\n", s->otrash);
	printf("\t%d cells trashed due almost full buffer\n", s->ttrash);
	printf("\t%d rx mbuf allocation failures\n", s->rxmbufout);
	printf("\t%d times out of tx maps\n", s->txnomap);
#ifdef NATM
#ifdef NATM_STAT
	printf("\tnatmintr so_rcv: ok/drop cnt: %d/%d, ok/drop bytes: %d/%d\n",
	    natm_sookcnt, natm_sodropcnt, natm_sookbytes, natm_sodropbytes);
#endif
#endif
}

static void
en_dump_mregs(struct en_softc *sc)
{
	u_int cnt;

	printf("mregs:\n");
	printf("resid = 0x%x\n", en_read(sc, MID_RESID));
	printf("interrupt status = 0x%b\n",
	    (int)en_read(sc, MID_INTSTAT), MID_INTBITS);
	printf("interrupt enable = 0x%b\n", 
	     (int)en_read(sc, MID_INTENA), MID_INTBITS);
	printf("mcsr = 0x%b\n", (int)en_read(sc, MID_MAST_CSR), MID_MCSRBITS);
	printf("serv_write = [chip=%u] [us=%u]\n", en_read(sc, MID_SERV_WRITE),
	     MID_SL_A2REG(sc->hwslistp));
	printf("dma addr = 0x%x\n", en_read(sc, MID_DMA_ADDR));
	printf("DRQ: chip[rd=0x%x,wr=0x%x], sc[chip=0x%x,us=0x%x]\n",
	    MID_DRQ_REG2A(en_read(sc, MID_DMA_RDRX)), 
	    MID_DRQ_REG2A(en_read(sc, MID_DMA_WRRX)), sc->drq_chip, sc->drq_us);
	printf("DTQ: chip[rd=0x%x,wr=0x%x], sc[chip=0x%x,us=0x%x]\n",
	    MID_DTQ_REG2A(en_read(sc, MID_DMA_RDTX)), 
	    MID_DTQ_REG2A(en_read(sc, MID_DMA_WRTX)), sc->dtq_chip, sc->dtq_us);

	printf("  unusal txspeeds:");
	for (cnt = 0 ; cnt < MID_N_VC ; cnt++)
		if (sc->vccs[cnt]->txspeed)
			printf(" vci%d=0x%x", cnt, sc->vccs[cnt]->txspeed);
	printf("\n");

	printf("  rxvc slot mappings:");
	for (cnt = 0 ; cnt < MID_N_VC ; cnt++)
		if (sc->vccs[cnt]->rxslot != NULL)
			printf("  %d->%td", cnt,
			    sc->vccs[cnt]->rxslot - sc->rxslot);
	printf("\n");
}

static void
en_dump_tx(struct en_softc *sc)
{
	u_int slot;

	printf("tx:\n");
	for (slot = 0 ; slot < EN_NTX; slot++) {
		printf("tx%d: start/stop/cur=0x%x/0x%x/0x%x [%d]  ", slot,
		    sc->txslot[slot].start, sc->txslot[slot].stop,
		    sc->txslot[slot].cur,
		    (sc->txslot[slot].cur - sc->txslot[slot].start) / 4);
		printf("mbsize=%d, bfree=%d\n", sc->txslot[slot].mbsize,
		    sc->txslot[slot].bfree);
		printf("txhw: base_address=0x%x, size=%u, read=%u, "
		    "descstart=%u\n",
		    (u_int)MIDX_BASE(en_read(sc, MIDX_PLACE(slot))), 
		    MIDX_SZ(en_read(sc, MIDX_PLACE(slot))),
		    en_read(sc, MIDX_READPTR(slot)),
		    en_read(sc, MIDX_DESCSTART(slot)));
	}
}

static void
en_dump_rx(struct en_softc *sc)
{
	struct en_rxslot *slot;

	printf("  recv slots:\n");
	for (slot = sc->rxslot ; slot < &sc->rxslot[sc->en_nrx]; slot++) {
		printf("rx%td: start/stop/cur=0x%x/0x%x/0x%x mode=0x%x ",
		    slot - sc->rxslot, slot->start, slot->stop, slot->cur,
		    slot->mode);
		if (slot->vcc != NULL) {
			printf("vci=%u\n", slot->vcc->vcc.vci);
			printf("RXHW: mode=0x%x, DST_RP=0x%x, WP_ST_CNT=0x%x\n",
			    en_read(sc, MID_VC(slot->vcc->vcc.vci)),
			    en_read(sc, MID_DST_RP(slot->vcc->vcc.vci)),
			    en_read(sc, MID_WP_ST_CNT(slot->vcc->vcc.vci)));
		}
	}
}

/*
 * This is only correct for non-adaptec adapters
 */
static void
en_dump_dtqs(struct en_softc *sc)
{
	uint32_t ptr, reg;

	printf("  dtq [need_dtqs=%d,dtq_free=%d]:\n", sc->need_dtqs,
	    sc->dtq_free);
	ptr = sc->dtq_chip;
	while (ptr != sc->dtq_us) {
		reg = en_read(sc, ptr);
		printf("\t0x%x=[%#x cnt=%d, chan=%d, end=%d, type=%d @ 0x%x]\n", 
		    sc->dtq[MID_DTQ_A2REG(ptr)], reg, MID_DMA_CNT(reg),
		    MID_DMA_TXCHAN(reg), (reg & MID_DMA_END) != 0,
		    MID_DMA_TYPE(reg), en_read(sc, ptr + 4));
		EN_WRAPADD(MID_DTQOFF, MID_DTQEND, ptr, 8);
	}
}

static void
en_dump_drqs(struct en_softc *sc)
{
	uint32_t ptr, reg;

	printf("  drq [need_drqs=%d,drq_free=%d]:\n", sc->need_drqs,
	    sc->drq_free);
	ptr = sc->drq_chip;
	while (ptr != sc->drq_us) {
		reg = en_read(sc, ptr);
		printf("\t0x%x=[cnt=%d, chan=%d, end=%d, type=%d @ 0x%x]\n", 
		    sc->drq[MID_DRQ_A2REG(ptr)], MID_DMA_CNT(reg),
		    MID_DMA_RXVCI(reg), (reg & MID_DMA_END) != 0,
		    MID_DMA_TYPE(reg), en_read(sc, ptr + 4));
		EN_WRAPADD(MID_DRQOFF, MID_DRQEND, ptr, 8);
	}
}

/* Do not staticize - meant for calling from DDB! */
int
en_dump(int unit, int level)
{
	struct en_softc *sc;
	int lcv, cnt;
	devclass_t dc;
	int maxunit;

	dc = devclass_find("en");
	if (dc == NULL) {
		printf("%s: can't find devclass!\n", __func__);
		return (0);
	}
	maxunit = devclass_get_maxunit(dc);
	for (lcv = 0 ; lcv < maxunit ; lcv++) {
		sc = devclass_get_softc(dc, lcv);
		if (sc == NULL)
			continue;
		if (unit != -1 && unit != lcv)
			continue;

		device_printf(sc->dev, "dumping device at level 0x%b\n",
		    level, END_BITS);

		if (sc->dtq_us == 0) {
			printf("<hasn't been en_init'd yet>\n");
			continue;
		}

		if (level & END_STATS)
			en_dump_stats(&sc->stats);
		if (level & END_MREGS)
			en_dump_mregs(sc);
		if (level & END_TX)
			en_dump_tx(sc);
		if (level & END_RX)
			en_dump_rx(sc);
		if (level & END_DTQ)
			en_dump_dtqs(sc);
		if (level & END_DRQ)
			en_dump_drqs(sc);

		if (level & END_SWSL) {
			printf(" swslist [size=%d]: ", sc->swsl_size);
			for (cnt = sc->swsl_head ; cnt != sc->swsl_tail ; 
			    cnt = (cnt + 1) % MID_SL_N)
				printf("0x%x ", sc->swslist[cnt]);
			printf("\n");
		}
	}
	return (0);
}

/*
 * en_dumpmem: dump the memory
 *
 * Do not staticize - meant for calling from DDB!
 */
int
en_dumpmem(int unit, int addr, int len)
{
	struct en_softc *sc;
	uint32_t reg;
	devclass_t dc;

	dc = devclass_find("en");
	if (dc == NULL) {
		printf("%s: can't find devclass\n", __func__);
		return (0);
	}
	sc = devclass_get_softc(dc, unit);
	if (sc == NULL) {
		printf("%s: invalid unit number: %d\n", __func__, unit);
		return (0);
	}

	addr = addr & ~3;
	if (addr < MID_RAMOFF || addr + len * 4 > MID_MAXOFF || len <= 0) {
		printf("invalid addr/len number: %d, %d\n", addr, len);
		return (0);
	}
	printf("dumping %d words starting at offset 0x%x\n", len, addr);
	while (len--) {
		reg = en_read(sc, addr);
		printf("mem[0x%x] = 0x%x\n", addr, reg);
		addr += 4;
	}
	return (0);
}
#endif
