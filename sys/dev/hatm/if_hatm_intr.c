/*-
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
 * Author: Hartmut Brandt <harti@freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * ForeHE driver.
 *
 * Interrupt handler.
 */

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
#include <sys/queue.h>
#include <sys/syslog.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <vm/uma.h>

#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/if_atm.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_atm.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/utopia/utopia.h>
#include <dev/hatm/if_hatmconf.h>
#include <dev/hatm/if_hatmreg.h>
#include <dev/hatm/if_hatmvar.h>

CTASSERT(sizeof(struct mbuf_page) == MBUF_ALLOC_SIZE);
CTASSERT(sizeof(struct mbuf0_chunk) == MBUF0_CHUNK);
CTASSERT(sizeof(struct mbuf1_chunk) == MBUF1_CHUNK);
CTASSERT(sizeof(((struct mbuf0_chunk *)NULL)->storage) >= MBUF0_SIZE);
CTASSERT(sizeof(((struct mbuf1_chunk *)NULL)->storage) >= MBUF1_SIZE);
CTASSERT(sizeof(struct tpd) <= HE_TPD_SIZE);

CTASSERT(MBUF0_PER_PAGE <= 256);
CTASSERT(MBUF1_PER_PAGE <= 256);

static void hatm_mbuf_page_alloc(struct hatm_softc *sc, u_int group);

/*
 * Free an external mbuf to a list. We use atomic functions so that
 * we don't need a mutex for the list.
 *
 * Note that in general this algorithm is not safe when multiple readers
 * and writers are present. To cite from a mail from David Schultz
 * <das@freebsd.org>:
 *
 *	It looks like this is subject to the ABA problem.  For instance,
 *	suppose X, Y, and Z are the top things on the freelist and a
 *	thread attempts to make an allocation.  You set buf to X and load
 *	buf->link (Y) into a register.  Then the thread get preempted, and
 *	another thread allocates both X and Y, then frees X.  When the
 *	original thread gets the CPU again, X is still on top of the
 *	freelist, so the atomic operation succeeds.  However, the atomic
 *	op places Y on top of the freelist, even though Y is no longer
 *	free.
 *
 * We are, however sure that we have only one thread that ever allocates
 * buffers because the only place we're call from is the interrupt handler.
 * Under these circumstances the code looks safe.
 */
void
hatm_ext_free(struct mbufx_free **list, struct mbufx_free *buf)
{
	for (;;) {
		buf->link = *list;
		if (atomic_cmpset_ptr((uintptr_t *)list, (uintptr_t)buf->link,
		    (uintptr_t)buf))
			break;
	}
}

static __inline struct mbufx_free *
hatm_ext_alloc(struct hatm_softc *sc, u_int g)
{
	struct mbufx_free *buf;

	for (;;) {
		if ((buf = sc->mbuf_list[g]) == NULL)
			break;
		if (atomic_cmpset_ptr((uintptr_t *)&sc->mbuf_list[g],
			(uintptr_t)buf, (uintptr_t)buf->link))
			break;
	}
	if (buf == NULL) {
		hatm_mbuf_page_alloc(sc, g);
		for (;;) {
			if ((buf = sc->mbuf_list[g]) == NULL)
				break;
			if (atomic_cmpset_ptr((uintptr_t *)&sc->mbuf_list[g],
			    (uintptr_t)buf, (uintptr_t)buf->link))
				break;
		}
	}
	return (buf);
}

/*
 * Either the queue treshold was crossed or a TPD with the INTR bit set
 * was transmitted.
 */
static void
he_intr_tbrq(struct hatm_softc *sc, struct hetbrq *q, u_int group)
{
	uint32_t *tailp = &sc->hsp->group[group].tbrq_tail;
	u_int no;

	while (q->head != (*tailp >> 2)) {
		no = (q->tbrq[q->head].addr & HE_REGM_TBRQ_ADDR) >>
		    HE_REGS_TPD_ADDR;
		hatm_tx_complete(sc, TPD_ADDR(sc, no),
		    (q->tbrq[q->head].addr & HE_REGM_TBRQ_FLAGS));

		if (++q->head == q->size)
			q->head = 0;
	}
	WRITE4(sc, HE_REGO_TBRQ_H(group), q->head << 2);
}

/*
 * DMA loader function for external mbuf page.
 */
static void
hatm_extbuf_helper(void *arg, bus_dma_segment_t *segs, int nsegs,
    int error)
{
	if (error) {
		printf("%s: mapping error %d\n", __func__, error);
		return;
	}
	KASSERT(nsegs == 1,
	    ("too many segments for DMA: %d", nsegs));
	KASSERT(segs[0].ds_addr <= 0xffffffffLU,
	    ("phys addr too large %lx", (u_long)segs[0].ds_addr));

	*(uint32_t *)arg = segs[0].ds_addr;
}

/*
 * Allocate a page of external mbuf storage for the small pools.
 * Create a DMA map and load it. Put all the chunks onto the right
 * free list.
 */
static void
hatm_mbuf_page_alloc(struct hatm_softc *sc, u_int group)
{
	struct mbuf_page *pg;
	int err;
	u_int i;

	if (sc->mbuf_npages == sc->mbuf_max_pages)
		return;
	if ((pg = malloc(MBUF_ALLOC_SIZE, M_DEVBUF, M_NOWAIT)) == NULL)
		return;

	err = bus_dmamap_create(sc->mbuf_tag, 0, &pg->hdr.map);
	if (err != 0) {
		if_printf(sc->ifp, "%s -- bus_dmamap_create: %d\n",
		    __func__, err);
		free(pg, M_DEVBUF);
		return;
	}
	err = bus_dmamap_load(sc->mbuf_tag, pg->hdr.map, pg, MBUF_ALLOC_SIZE,
	    hatm_extbuf_helper, &pg->hdr.phys, BUS_DMA_NOWAIT);
	if (err != 0) {
		if_printf(sc->ifp, "%s -- mbuf mapping failed %d\n",
		    __func__, err);
		bus_dmamap_destroy(sc->mbuf_tag, pg->hdr.map);
		free(pg, M_DEVBUF);
		return;
	}

	sc->mbuf_pages[sc->mbuf_npages] = pg;

	if (group == 0) {
		struct mbuf0_chunk *c;

		pg->hdr.pool = 0;
		pg->hdr.nchunks = MBUF0_PER_PAGE;
		pg->hdr.chunksize = MBUF0_CHUNK;
		pg->hdr.hdroff = sizeof(c->storage);
		c = (struct mbuf0_chunk *)pg;
		for (i = 0; i < MBUF0_PER_PAGE; i++, c++) {
			c->hdr.pageno = sc->mbuf_npages;
			c->hdr.chunkno = i;
			c->hdr.flags = 0;
			hatm_ext_free(&sc->mbuf_list[0],
			    (struct mbufx_free *)c);
		}
	} else {
		struct mbuf1_chunk *c;

		pg->hdr.pool = 1;
		pg->hdr.nchunks = MBUF1_PER_PAGE;
		pg->hdr.chunksize = MBUF1_CHUNK;
		pg->hdr.hdroff = sizeof(c->storage);
		c = (struct mbuf1_chunk *)pg;
		for (i = 0; i < MBUF1_PER_PAGE; i++, c++) {
			c->hdr.pageno = sc->mbuf_npages;
			c->hdr.chunkno = i;
			c->hdr.flags = 0;
			hatm_ext_free(&sc->mbuf_list[1],
			    (struct mbufx_free *)c);
		}
	}
	sc->mbuf_npages++;
}

/*
 * Free an mbuf and put it onto the free list.
 */
static void
hatm_mbuf0_free(struct mbuf *m, void *buf, void *args)
{
	struct hatm_softc *sc = args;
	struct mbuf0_chunk *c = buf;

	KASSERT((c->hdr.flags & (MBUF_USED | MBUF_CARD)) == MBUF_USED,
	    ("freeing unused mbuf %x", c->hdr.flags));
	c->hdr.flags &= ~MBUF_USED;
	hatm_ext_free(&sc->mbuf_list[0], (struct mbufx_free *)c);
}
static void
hatm_mbuf1_free(struct mbuf *m, void *buf, void *args)
{
	struct hatm_softc *sc = args;
	struct mbuf1_chunk *c = buf;

	KASSERT((c->hdr.flags & (MBUF_USED | MBUF_CARD)) == MBUF_USED,
	    ("freeing unused mbuf %x", c->hdr.flags));
	c->hdr.flags &= ~MBUF_USED;
	hatm_ext_free(&sc->mbuf_list[1], (struct mbufx_free *)c);
}

static void
hatm_mbuf_helper(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	uint32_t *ptr = (uint32_t *)arg;

	if (nsegs == 0) {
		printf("%s: error=%d\n", __func__, error);
		return;
	}
	KASSERT(nsegs == 1, ("too many segments for mbuf: %d", nsegs));
	KASSERT(segs[0].ds_addr <= 0xffffffffLU,
	    ("phys addr too large %lx", (u_long)segs[0].ds_addr));

	*ptr = segs[0].ds_addr;
}

/*
 * Receive buffer pool interrupt. This means the number of entries in the
 * queue has dropped below the threshold. Try to supply new buffers.
 */
static void
he_intr_rbp(struct hatm_softc *sc, struct herbp *rbp, u_int large,
    u_int group)
{
	u_int ntail;
	struct mbuf *m;
	int error;
	struct mbufx_free *cf;
	struct mbuf_page *pg;
	struct mbuf0_chunk *buf0;
	struct mbuf1_chunk *buf1;

	DBG(sc, INTR, ("%s buffer supply threshold crossed for group %u",
	   large ? "large" : "small", group));

	rbp->head = (READ4(sc, HE_REGO_RBP_S(large, group)) >> HE_REGS_RBP_HEAD)
	    & (rbp->size - 1);

	for (;;) {
		if ((ntail = rbp->tail + 1) == rbp->size)
			ntail = 0;
		if (ntail == rbp->head)
			break;
		m = NULL;

		if (large) {
			/* allocate the MBUF */
			if ((m = m_getcl(M_NOWAIT, MT_DATA,
			    M_PKTHDR)) == NULL) {
				if_printf(sc->ifp,
				    "no mbuf clusters\n");
				break;
			}
			m->m_data += MBUFL_OFFSET;

			if (sc->lbufs[sc->lbufs_next] != NULL)
				panic("hatm: lbufs full %u", sc->lbufs_next);
			sc->lbufs[sc->lbufs_next] = m;

			if ((error = bus_dmamap_load(sc->mbuf_tag,
			    sc->rmaps[sc->lbufs_next],
			    m->m_data, rbp->bsize, hatm_mbuf_helper,
			    &rbp->rbp[rbp->tail].phys, BUS_DMA_NOWAIT)) != 0)
				panic("hatm: mbuf mapping failed %d", error);

			bus_dmamap_sync(sc->mbuf_tag,
			    sc->rmaps[sc->lbufs_next],
			    BUS_DMASYNC_PREREAD);

			rbp->rbp[rbp->tail].handle =
			    MBUF_MAKE_LHANDLE(sc->lbufs_next);

			if (++sc->lbufs_next == sc->lbufs_size)
				sc->lbufs_next = 0;

		} else if (group == 0) {
			/*
			 * Allocate small buffer in group 0
			 */
			if ((cf = hatm_ext_alloc(sc, 0)) == NULL)
				break;
			buf0 = (struct mbuf0_chunk *)cf;
			pg = sc->mbuf_pages[buf0->hdr.pageno];
			buf0->hdr.flags |= MBUF_CARD;
			rbp->rbp[rbp->tail].phys = pg->hdr.phys +
			    buf0->hdr.chunkno * MBUF0_CHUNK + MBUF0_OFFSET;
			rbp->rbp[rbp->tail].handle =
			    MBUF_MAKE_HANDLE(buf0->hdr.pageno,
			    buf0->hdr.chunkno);

			bus_dmamap_sync(sc->mbuf_tag, pg->hdr.map,
			    BUS_DMASYNC_PREREAD);

		} else if (group == 1) {
			/*
			 * Allocate small buffer in group 1
			 */
			if ((cf = hatm_ext_alloc(sc, 1)) == NULL)
				break;
			buf1 = (struct mbuf1_chunk *)cf;
			pg = sc->mbuf_pages[buf1->hdr.pageno];
			buf1->hdr.flags |= MBUF_CARD;
			rbp->rbp[rbp->tail].phys = pg->hdr.phys +
			    buf1->hdr.chunkno * MBUF1_CHUNK + MBUF1_OFFSET;
			rbp->rbp[rbp->tail].handle =
			    MBUF_MAKE_HANDLE(buf1->hdr.pageno,
			    buf1->hdr.chunkno);

			bus_dmamap_sync(sc->mbuf_tag, pg->hdr.map,
			    BUS_DMASYNC_PREREAD);

		} else
			/* ups */
			break;

		DBG(sc, DMA, ("MBUF loaded: handle=%x m=%p phys=%x",
		    rbp->rbp[rbp->tail].handle, m, rbp->rbp[rbp->tail].phys));

		rbp->tail = ntail;
	}
	WRITE4(sc, HE_REGO_RBP_T(large, group),
	    (rbp->tail << HE_REGS_RBP_TAIL));
}

/*
 * Extract the buffer and hand it to the receive routine
 */
static struct mbuf *
hatm_rx_buffer(struct hatm_softc *sc, u_int group, u_int handle)
{
	u_int pageno;
	u_int chunkno;
	struct mbuf *m;

	if (handle & MBUF_LARGE_FLAG) {
		/* large buffer - sync and unload */
		MBUF_PARSE_LHANDLE(handle, handle);
		DBG(sc, RX, ("RX large handle=%x", handle));

		bus_dmamap_sync(sc->mbuf_tag, sc->rmaps[handle],
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->mbuf_tag, sc->rmaps[handle]);

		m = sc->lbufs[handle];
		sc->lbufs[handle] = NULL;

		return (m);
	}

	MBUF_PARSE_HANDLE(handle, pageno, chunkno);

	DBG(sc, RX, ("RX group=%u handle=%x page=%u chunk=%u", group, handle,
	    pageno, chunkno));

	MGETHDR(m, M_NOWAIT, MT_DATA);

	if (group == 0) {
		struct mbuf0_chunk *c0;

		c0 = (struct mbuf0_chunk *)sc->mbuf_pages[pageno] + chunkno;
		KASSERT(c0->hdr.pageno == pageno, ("pageno = %u/%u",
		    c0->hdr.pageno, pageno));
		KASSERT(c0->hdr.chunkno == chunkno, ("chunkno = %u/%u",
		    c0->hdr.chunkno, chunkno));
		KASSERT(c0->hdr.flags & MBUF_CARD, ("mbuf not on card %u/%u",
		    pageno, chunkno));
		KASSERT(!(c0->hdr.flags & MBUF_USED), ("used mbuf %u/%u",
		    pageno, chunkno));

		c0->hdr.flags |= MBUF_USED;
		c0->hdr.flags &= ~MBUF_CARD;

		if (m != NULL) {
			m->m_ext.ext_cnt = &c0->hdr.ref_cnt;
			MEXTADD(m, (void *)c0, MBUF0_SIZE,
			    hatm_mbuf0_free, c0, sc, M_PKTHDR, EXT_EXTREF);
			m->m_data += MBUF0_OFFSET;
		} else
			(void)hatm_mbuf0_free(NULL, c0, sc);

	} else {
		struct mbuf1_chunk *c1;

		c1 = (struct mbuf1_chunk *)sc->mbuf_pages[pageno] + chunkno;
		KASSERT(c1->hdr.pageno == pageno, ("pageno = %u/%u",
		    c1->hdr.pageno, pageno));
		KASSERT(c1->hdr.chunkno == chunkno, ("chunkno = %u/%u",
		    c1->hdr.chunkno, chunkno));
		KASSERT(c1->hdr.flags & MBUF_CARD, ("mbuf not on card %u/%u",
		    pageno, chunkno));
		KASSERT(!(c1->hdr.flags & MBUF_USED), ("used mbuf %u/%u",
		    pageno, chunkno));

		c1->hdr.flags |= MBUF_USED;
		c1->hdr.flags &= ~MBUF_CARD;

		if (m != NULL) {
			m->m_ext.ext_cnt = &c1->hdr.ref_cnt;
			MEXTADD(m, (void *)c1, MBUF1_SIZE,
			    hatm_mbuf1_free, c1, sc, M_PKTHDR, EXT_EXTREF);
			m->m_data += MBUF1_OFFSET;
		} else
			(void)hatm_mbuf1_free(NULL, c1, sc);
	}

	return (m);
}

/*
 * Interrupt because of receive buffer returned.
 */
static void
he_intr_rbrq(struct hatm_softc *sc, struct herbrq *rq, u_int group)
{
	struct he_rbrqen *e;
	uint32_t flags, tail;
	u_int cid, len;
	struct mbuf *m;

	for (;;) {
		tail = sc->hsp->group[group].rbrq_tail >> 3;

		if (rq->head == tail)
			break;

		e = &rq->rbrq[rq->head];

		flags = e->addr & HE_REGM_RBRQ_FLAGS;
		if (!(flags & HE_REGM_RBRQ_HBUF_ERROR))
			m = hatm_rx_buffer(sc, group, e->addr);
		else
			m = NULL;

		cid = (e->len & HE_REGM_RBRQ_CID) >> HE_REGS_RBRQ_CID;
		len = 4 * (e->len & HE_REGM_RBRQ_LEN);

		hatm_rx(sc, cid, flags, m, len);

		if (++rq->head == rq->size)
			rq->head = 0;
	}
	WRITE4(sc, HE_REGO_RBRQ_H(group), rq->head << 3);
}

void
hatm_intr(void *p)
{
	struct heirq *q = p;
	struct hatm_softc *sc = q->sc;
	u_int status;
	u_int tail;

	/* if we have a stray interrupt with a non-initialized card,
	 * we cannot even lock before looking at the flag */
	if (!(sc->ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	mtx_lock(&sc->mtx);
	(void)READ4(sc, HE_REGO_INT_FIFO);

	tail = *q->tailp;
	if (q->head == tail) {
		/* workaround for tail pointer not updated bug (8.1.1) */
		DBG(sc, INTR, ("hatm: intr tailq not updated bug triggered"));

		/* read the tail pointer from the card */
		tail = READ4(sc, HE_REGO_IRQ_BASE(q->group)) &
		    HE_REGM_IRQ_BASE_TAIL;
		BARRIER_R(sc);

		sc->istats.bug_no_irq_upd++;
	}

	/* clear the interrupt */
	WRITE4(sc, HE_REGO_INT_FIFO, HE_REGM_INT_FIFO_CLRA);
	BARRIER_W(sc);

	while (q->head != tail) {
		status = q->irq[q->head];
		q->irq[q->head] = HE_REGM_ITYPE_INVALID;
		if (++q->head == (q->size - 1))
			q->head = 0;

		switch (status & HE_REGM_ITYPE) {

		  case HE_REGM_ITYPE_TBRQ:
			DBG(sc, INTR, ("TBRQ treshold %u", status & HE_REGM_IGROUP));
			sc->istats.itype_tbrq++;
			he_intr_tbrq(sc, &sc->tbrq, status & HE_REGM_IGROUP);
			break;

		  case HE_REGM_ITYPE_TPD:
			DBG(sc, INTR, ("TPD ready %u", status & HE_REGM_IGROUP));
			sc->istats.itype_tpd++;
			he_intr_tbrq(sc, &sc->tbrq, status & HE_REGM_IGROUP);
			break;

		  case HE_REGM_ITYPE_RBPS:
			sc->istats.itype_rbps++;
			switch (status & HE_REGM_IGROUP) {

			  case 0:
				he_intr_rbp(sc, &sc->rbp_s0, 0, 0);
				break;

			  case 1:
				he_intr_rbp(sc, &sc->rbp_s1, 0, 1);
				break;

			  default:
				if_printf(sc->ifp, "bad INTR RBPS%u\n",
				    status & HE_REGM_IGROUP);
				break;
			}
			break;

		  case HE_REGM_ITYPE_RBPL:
			sc->istats.itype_rbpl++;
			switch (status & HE_REGM_IGROUP) {

			  case 0:
				he_intr_rbp(sc, &sc->rbp_l0, 1, 0);
				break;

			  default:
				if_printf(sc->ifp, "bad INTR RBPL%u\n",
				    status & HE_REGM_IGROUP);
				break;
			}
			break;

		  case HE_REGM_ITYPE_RBRQ:
			DBG(sc, INTR, ("INTERRUPT RBRQ %u", status & HE_REGM_IGROUP));
			sc->istats.itype_rbrq++;
			switch (status & HE_REGM_IGROUP) {

			  case 0:
				he_intr_rbrq(sc, &sc->rbrq_0, 0);
				break;

			  case 1:
				if (sc->rbrq_1.size > 0) {
					he_intr_rbrq(sc, &sc->rbrq_1, 1);
					break;
				}
				/* FALLTHRU */

			  default:
				if_printf(sc->ifp, "bad INTR RBRQ%u\n",
				    status & HE_REGM_IGROUP);
				break;
			}
			break;

		  case HE_REGM_ITYPE_RBRQT:
			DBG(sc, INTR, ("INTERRUPT RBRQT %u", status & HE_REGM_IGROUP));
			sc->istats.itype_rbrqt++;
			switch (status & HE_REGM_IGROUP) {

			  case 0:
				he_intr_rbrq(sc, &sc->rbrq_0, 0);
				break;

			  case 1:
				if (sc->rbrq_1.size > 0) {
					he_intr_rbrq(sc, &sc->rbrq_1, 1);
					break;
				}
				/* FALLTHRU */

			  default:
				if_printf(sc->ifp, "bad INTR RBRQT%u\n",
				    status & HE_REGM_IGROUP);
				break;
			}
			break;

		  case HE_REGM_ITYPE_PHYS:
			sc->istats.itype_phys++;
			utopia_intr(&sc->utopia);
			break;

#if HE_REGM_ITYPE_UNKNOWN != HE_REGM_ITYPE_INVALID
		  case HE_REGM_ITYPE_UNKNOWN:
			sc->istats.itype_unknown++;
			if_printf(sc->ifp, "bad interrupt\n");
			break;
#endif

		  case HE_REGM_ITYPE_ERR:
			sc->istats.itype_err++;
			switch (status) {

			  case HE_REGM_ITYPE_PERR:
				if_printf(sc->ifp, "parity error\n");
				break;

			  case HE_REGM_ITYPE_ABORT:
				if_printf(sc->ifp, "abort interrupt "
				    "addr=0x%08x\n",
				    READ4(sc, HE_REGO_ABORT_ADDR));
				break;

			  default:
				if_printf(sc->ifp,
				    "bad interrupt type %08x\n", status);
				break;
			}
			break;

		  case HE_REGM_ITYPE_INVALID:
			/* this is the documented fix for the ISW bug 8.1.1
			 * Note, that the documented fix is partly wrong:
			 * the ISWs should be intialized to 0xf8 not 0xff */
			sc->istats.bug_bad_isw++;
			DBG(sc, INTR, ("hatm: invalid ISW bug triggered"));
			he_intr_tbrq(sc, &sc->tbrq, 0);
			he_intr_rbp(sc, &sc->rbp_s0, 0, 0);
			he_intr_rbp(sc, &sc->rbp_l0, 1, 0);
			he_intr_rbp(sc, &sc->rbp_s1, 0, 1);
			he_intr_rbrq(sc, &sc->rbrq_0, 0);
			he_intr_rbrq(sc, &sc->rbrq_1, 1);
			utopia_intr(&sc->utopia);
			break;

		  default:
			if_printf(sc->ifp, "bad interrupt type %08x\n",
			    status);
			break;
		}
	}

	/* write back head to clear queue */
	WRITE4(sc, HE_REGO_IRQ_HEAD(0),
	    ((q->size - 1) << HE_REGS_IRQ_HEAD_SIZE) |
	    (q->thresh << HE_REGS_IRQ_HEAD_THRESH) |
	    (q->head << HE_REGS_IRQ_HEAD_HEAD));
	BARRIER_W(sc);

	/* workaround the back-to-back irq access problem (8.1.2) */
	(void)READ4(sc, HE_REGO_INT_FIFO);
	BARRIER_R(sc);

	mtx_unlock(&sc->mtx);
}
