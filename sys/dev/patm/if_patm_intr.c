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
 * Driver for IDT77252 based cards like ProSum's.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_natm.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/condvar.h>
#include <sys/endian.h>
#include <vm/uma.h>

#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_atm.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_atm.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/mbpool.h>

#include <dev/utopia/utopia.h>
#include <dev/patm/idt77252reg.h>
#include <dev/patm/if_patmvar.h>

static void patm_feed_sbufs(struct patm_softc *sc);
static void patm_feed_lbufs(struct patm_softc *sc);
static void patm_feed_vbufs(struct patm_softc *sc);
static void patm_intr_tsif(struct patm_softc *sc);
static void patm_intr_raw(struct patm_softc *sc);

#ifdef PATM_DEBUG
static int patm_mbuf_cnt(u_int unit) __unused;
#endif

/*
 * Write free buf Q
 */
static __inline void
patm_fbq_write(struct patm_softc *sc, u_int queue, uint32_t h0,
    uint32_t p0, uint32_t h1, uint32_t p1)
{
	patm_debug(sc, FREEQ, "supplying(%u,%#x,%#x,%#x,%#x)",
	    queue, h0, p0, h1, p1);
	patm_nor_write(sc, IDT_NOR_D0, h0);
	patm_nor_write(sc, IDT_NOR_D1, p0);
	patm_nor_write(sc, IDT_NOR_D2, h1);
	patm_nor_write(sc, IDT_NOR_D3, p1);
	patm_cmd_exec(sc, IDT_CMD_WFBQ | queue);
}

/*
 * Interrupt
 */
void
patm_intr(void *p)
{
	struct patm_softc *sc = p;
	uint32_t stat, cfg;
	u_int cnt;
	const uint32_t ints = IDT_STAT_TSIF | IDT_STAT_TXICP | IDT_STAT_TSQF |
	    IDT_STAT_TMROF | IDT_STAT_PHYI | IDT_STAT_RSQF | IDT_STAT_EPDU |
	    IDT_STAT_RAWCF | IDT_STAT_RSQAF;
	const uint32_t fbqa = IDT_STAT_FBQ3A | IDT_STAT_FBQ2A |
	    IDT_STAT_FBQ1A | IDT_STAT_FBQ0A;

	mtx_lock(&sc->mtx);

	stat = patm_nor_read(sc, IDT_NOR_STAT);
	patm_nor_write(sc, IDT_NOR_STAT, stat & (ints | fbqa));

	if (!(sc->ifatm.ifnet.if_flags & IFF_RUNNING)) {
		/* if we are stopped ack all interrupts and handle PHYI */
		if (stat & IDT_STAT_PHYI) {
			patm_debug(sc, INTR, "PHYI (stopped)");
			utopia_intr(&sc->utopia);
		}
		mtx_unlock(&sc->mtx);
		return;
	}

	patm_debug(sc, INTR, "stat=%08x", stat);

	/*
	 * If the buffer queues are empty try to fill them. If this fails
	 * disable the interrupt. Otherwise enable the interrupt.
	 */
	if (stat & fbqa) {
		cfg = patm_nor_read(sc, IDT_NOR_CFG);
		if (stat & IDT_STAT_FBQ0A)
			patm_feed_sbufs(sc);
		if (stat & IDT_STAT_FBQ1A)
			patm_feed_lbufs(sc);
		if (stat & IDT_STAT_FBQ2A) {
			/*
			 * Workaround for missing interrupt on AAL0. Check the
			 * receive status queue if the FBQ2 is not full.
			 */
			patm_intr_rsq(sc);
			patm_feed_vbufs(sc);
		}
		if ((patm_nor_read(sc, IDT_NOR_STAT) & fbqa) &&
		    (cfg & IDT_CFG_FBIE)) {
			/* failed */
			patm_nor_write(sc, IDT_NOR_CFG, cfg & ~IDT_CFG_FBIE);
			patm_printf(sc, "out of buffers -- intr disabled\n");
		} else if (!(cfg & IDT_CFG_FBIE)) {
			patm_printf(sc, "bufQ intr re-enabled\n");
			patm_nor_write(sc, IDT_NOR_CFG, cfg | IDT_CFG_FBIE);
		}
		patm_nor_write(sc, IDT_NOR_STAT, fbqa);
	}

	cnt = 0;
	while ((stat & ints) != 0) {
		if (++cnt == 200) {
			patm_printf(sc, "%s: excessive interrupts\n", __func__);
			patm_stop(sc);
			break;
		}
		if (stat & IDT_STAT_TSIF) {
			patm_debug(sc, INTR, "TSIF");
			patm_intr_tsif(sc);
		}
		if (stat & IDT_STAT_TXICP) {
			patm_printf(sc, "incomplete PDU transmitted\n");
		}
		if (stat & IDT_STAT_TSQF) {
			patm_printf(sc, "TSQF\n");
			patm_intr_tsif(sc);
		}
		if (stat & IDT_STAT_TMROF) {
			patm_debug(sc, INTR, "TMROF");
			patm_intr_tsif(sc);
		}
		if (stat & IDT_STAT_PHYI) {
			patm_debug(sc, INTR, "PHYI");
			utopia_intr(&sc->utopia);
		}
		if (stat & IDT_STAT_RSQF) {
			patm_printf(sc, "RSQF\n");
			patm_intr_rsq(sc);
		}
		if (stat & IDT_STAT_EPDU) {
			patm_debug(sc, INTR, "EPDU");
			patm_intr_rsq(sc);
		}
		if (stat & IDT_STAT_RAWCF) {
			patm_debug(sc, INTR, "RAWCF");
			patm_intr_raw(sc);
		}
		if (stat & IDT_STAT_RSQAF) {
			patm_debug(sc, INTR, "RSQAF");
			patm_intr_rsq(sc);
		} else if (IDT_STAT_FRAC2(stat) != 0xf) {
			/*
			 * Workaround for missing interrupt on AAL0. Check the
			 * receive status queue if the FBQ2 is not full.
			 */
			patm_intr_rsq(sc);
		}

		stat = patm_nor_read(sc, IDT_NOR_STAT);
		patm_nor_write(sc, IDT_NOR_STAT, ints & stat);
		patm_debug(sc, INTR, "stat=%08x", stat);
	}

	mtx_unlock(&sc->mtx);

	patm_debug(sc, INTR, "... exit");
}

/*
 * Compute the amount of buffers to feed into a given free buffer queue
 *
 * Feeding buffers is actually not so easy as it seems. We cannot use the
 * fraction fields in the status registers, because they round down, i.e.
 * if we have 34 buffers in the queue, it will show 1. If we now feed
 * 512 - 1 * 32 buffers, we loose two buffers. The only reliable way to know
 * how many buffers are in the queue are the FBQP registers.
 */
static u_int
patm_feed_cnt(struct patm_softc *sc, u_int q)
{
	u_int w, r, reg;
	u_int feed;
	int free;

	/* get the FBQ read and write pointers */
	reg = patm_nor_read(sc, IDT_NOR_FBQP0 + 4 * q);
	r = (reg & 0x7ff) >> 1;
	w = ((reg >> 16) & 0x7ff) >> 1;
	/* compute amount of free buffers */
	if ((free = w - r) < 0)
		free += 0x400;
	KASSERT(free <= 512, ("bad FBQP 0x%x", reg));
	feed = 512 - free;

	/* can only feed pairs of buffers */
	feed &= ~1;

	if (feed > 0)
		feed -= 2;

	patm_debug(sc, FREEQ, "feeding %u buffers into queue %u", feed, q);

	return (feed);
}

/*
 * Feed small buffers into buffer queue 0
 *
 */
static void
patm_feed_sbufs(struct patm_softc *sc)
{
	u_int feed;
	bus_addr_t p0, p1;
	void *v0, *v1;
	uint32_t h0, h1;

	feed = patm_feed_cnt(sc, 0);

	while (feed > 0) {
		if ((v0 = mbp_alloc(sc->sbuf_pool, &p0, &h0)) == NULL)
			break;
		if ((v1 = mbp_alloc(sc->sbuf_pool, &p1, &h1)) == NULL) {
			mbp_free(sc->sbuf_pool, v0);
			break;
		}
		patm_fbq_write(sc, 0,
		    h0 | MBUF_SHANDLE, (p0 + SMBUF_OFFSET),
		    h1 | MBUF_SHANDLE, (p1 + SMBUF_OFFSET));

		feed -= 2;
	}
}

/*
 * Feed small buffers into buffer queue 0
 */
static void
patm_feed_vbufs(struct patm_softc *sc)
{
	u_int feed;
	bus_addr_t p0, p1;
	void *v0, *v1;
	uint32_t h0, h1;

	feed = patm_feed_cnt(sc, 2);

	while (feed > 0) {
		if ((v0 = mbp_alloc(sc->vbuf_pool, &p0, &h0)) == NULL)
			break;
		if ((v1 = mbp_alloc(sc->vbuf_pool, &p1, &h1)) == NULL) {
			mbp_free(sc->vbuf_pool, v0);
			break;
		}
		patm_fbq_write(sc, 2,
		    h0 | MBUF_VHANDLE, (p0 + VMBUF_OFFSET),
		    h1 | MBUF_VHANDLE, (p1 + VMBUF_OFFSET));

		feed -= 2;
	}
}

/*
 * Allocate a large buffer
 */
static struct lmbuf *
patm_lmbuf_alloc(struct patm_softc *sc)
{
	int error;
	struct mbuf *m;
	struct lmbuf *b;

	m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL)
		return (NULL);
	m->m_data += LMBUF_OFFSET;

	if ((b = SLIST_FIRST(&sc->lbuf_free_list)) == NULL) {
		m_freem(m);
		return (NULL);
	}

	b->phy = 0;		/* alignment */
	error = bus_dmamap_load(sc->lbuf_tag, b->map, m->m_data, LMBUF_SIZE,
	    patm_load_callback, &b->phy, BUS_DMA_NOWAIT);
	if (error) {
		patm_printf(sc, "%s -- bus_dmamap_load: %d\n", __func__, error);
		m_free(m);
		return (NULL);
	}

	SLIST_REMOVE_HEAD(&sc->lbuf_free_list, link);
	b->m = m;

	return (b);
}

/*
 * Feed large buffers into buffer queue 1
 */
static void
patm_feed_lbufs(struct patm_softc *sc)
{
	u_int feed;
	struct lmbuf *b0, *b1;

	feed = patm_feed_cnt(sc, 1);

	while (feed > 0) {
		if ((b0 = patm_lmbuf_alloc(sc)) == NULL)
			break;
		if ((b1 = patm_lmbuf_alloc(sc)) == NULL) {
			patm_lbuf_free(sc, b0);
			break;
		}
		patm_fbq_write(sc, 1,
		    LMBUF_HANDLE | b0->handle, b0->phy,
		    LMBUF_HANDLE | b1->handle, b1->phy);

		feed -= 2;
	}
}

/*
 * Handle transmit status interrupt
 */
static void
patm_intr_tsif(struct patm_softc *sc)
{
	struct idt_tsqe *tsqe = sc->tsq_next;;
	struct idt_tsqe *prev = NULL;
	uint32_t stamp;

	stamp = le32toh(tsqe->stamp);
	if (stamp & IDT_TSQE_EMPTY)
		return;

	do {
		switch (IDT_TSQE_TYPE(stamp)) {

		  case IDT_TSQE_TBD:
			patm_tx(sc, stamp, le32toh(tsqe->stat));
			break;

		  case IDT_TSQE_IDLE:
			patm_tx_idle(sc, le32toh(tsqe->stat));
			break;
		}

		/* recycle */
		tsqe->stat = 0;
		tsqe->stamp = htole32(IDT_TSQE_EMPTY);

		/* save pointer to this entry and advance */
		prev = tsqe;
		if (++tsqe == &sc->tsq[IDT_TSQ_SIZE])
			tsqe = &sc->tsq[0];

		stamp = le32toh(tsqe->stamp);
	} while (!(stamp & IDT_TSQE_EMPTY));

	sc->tsq_next = tsqe;
	patm_nor_write(sc, IDT_NOR_TSQH, ((prev - sc->tsq) << IDT_TSQE_SHIFT));
}

/*
 * Handle receive interrupt
 */
void
patm_intr_rsq(struct patm_softc *sc)
{
	struct idt_rsqe *rsqe;
	u_int stat;

	if (sc->rsq_last + 1 == PATM_RSQ_SIZE)
		rsqe = &sc->rsq[0];
	else
		rsqe = &sc->rsq[sc->rsq_last + 1];
	stat = le32toh(rsqe->stat);
	if (!(stat & IDT_RSQE_VALID))
		return;

	while (stat & IDT_RSQE_VALID) {
		patm_rx(sc, rsqe);

		/* recycle RSQE */
		rsqe->cid = 0;
		rsqe->handle = 0;
		rsqe->crc = 0;
		rsqe->stat = 0;

		/* save pointer to this entry and advance */
		if (++sc->rsq_last == PATM_RSQ_SIZE)
			sc->rsq_last = 0;
		if (++rsqe == &sc->rsq[PATM_RSQ_SIZE])
			rsqe = sc->rsq;

		stat = le32toh(rsqe->stat);
	}

	patm_nor_write(sc, IDT_NOR_RSQH, sc->rsq_phy | (sc->rsq_last << 2));

	patm_feed_sbufs(sc);
	patm_feed_lbufs(sc);
	patm_feed_vbufs(sc);
}

/*
 * Handle raw cell receive.
 *
 * Note that the description on page 3-8 is wrong. The RAWHND contains not
 * the same value as RAWCT. RAWCT points to the next address the chip is
 * going to write to whike RAWHND points to the last cell's address the chip
 * has written to.
 */
static void
patm_intr_raw(struct patm_softc *sc)
{
	uint32_t tail;
	uint32_t h, *cell;

#ifdef notyet
	bus_dma_sync_size(sc->sq_tag, sc->sq_map, IDT_TSQ_SIZE * IDT_TSQE_SIZE +
	    PATM_RSQ_SIZE * IDT_RSQE_SIZE, sizeof(*sc->rawhnd),
	    BUS_DMASYNC_POSTREAD);
#endif
	/* first turn */
	if (sc->rawh == NULL) {
		sc->rawh = &sc->lbufs[le32toh(sc->rawhnd->handle) & MBUF_HMASK];
	}
	tail = le32toh(sc->rawhnd->tail);
	if (tail == sc->rawh->phy)
		/* not really a raw interrupt */
		return;

	while (tail + 64 != sc->rawh->phy + sc->rawi * 64) {
#ifdef notyet
		bus_dmamap_sync_size(sc->lbuf_tag, sc->rawh->map,
		    sc->rawi * 64, 64, BUS_DMASYNC_POSTREAD);
#endif
		cell = (uint32_t *)(mtod(sc->rawh->m, u_char *) +
		    sc->rawi * 64);
		if (sc->rawi == (LMBUF_SIZE / 64) - 1) {
			/* chain */
			h = le32toh(cell[1]);
			patm_lbuf_free(sc, sc->rawh);
			sc->rawh = &sc->lbufs[h & MBUF_HMASK];
			sc->rawi = 0;
			continue;
		}

		patm_rx_raw(sc, (u_char *)cell);
		sc->rawi++;
	}
}

/*
 * Free a large mbuf. This is called by us.
 */
void
patm_lbuf_free(struct patm_softc *sc, struct lmbuf *b)
{

	bus_dmamap_unload(sc->lbuf_tag, b->map);
	if (b->m != NULL) {
		m_free(b->m);
		b->m = NULL;
	}
	SLIST_INSERT_HEAD(&sc->lbuf_free_list, b, link);
}

#ifdef PATM_DEBUG
static int
patm_mbuf_cnt(u_int unit)
{
	devclass_t dc;
	struct patm_softc *sc;
	u_int used, card, free;

	dc = devclass_find("patm");
	if (dc == NULL) {
		printf("%s: can't find devclass\n", __func__);
		return (0);
	}
	sc = devclass_get_softc(dc, unit);
	if (sc == NULL) {
		printf("%s: invalid unit number: %d\n", __func__, unit);
		return (0);
	}

	mbp_count(sc->sbuf_pool, &used, &card, &free);
	printf("sbufs: %u on card, %u used, %u free\n", card, used, free);

	mbp_count(sc->vbuf_pool, &used, &card, &free);
	printf("aal0 bufs: %u on card, %u used, %u free\n", card, used, free);

	return (0);
}
#endif
