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
 * The TST allocation algorithm is from the IDT driver which is:
 *
 *	Copyright (c) 2000, 2001 Richard Hodges and Matriplex, inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1996, 1997, 1998, 1999 Mark Tinguely
 *	All rights reserved.
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
#ifdef ENABLE_BPF
#include <net/bpf.h>
#endif
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

static struct mbuf *patm_tx_pad(struct patm_softc *sc, struct mbuf *m0);
static void patm_launch(struct patm_softc *sc, struct patm_scd *scd);

static struct patm_txmap *patm_txmap_get(struct patm_softc *);
static void patm_load_txbuf(void *, bus_dma_segment_t *, int,
    bus_size_t, int);

static void patm_tst_alloc(struct patm_softc *sc, struct patm_vcc *vcc);
static void patm_tst_free(struct patm_softc *sc, struct patm_vcc *vcc);
static void patm_tst_timer(void *p);
static void patm_tst_update(struct patm_softc *);

static void patm_tct_start(struct patm_softc *sc, struct patm_vcc *);

static const char *dump_scd(struct patm_softc *sc, struct patm_scd *scd)
    __unused;
static void patm_tct_print(struct patm_softc *sc, u_int cid) __unused;

/*
 * Structure for communication with the loader function for transmission
 */
struct txarg {
	struct patm_softc *sc;
	struct patm_scd	*scd;		/* scheduling channel */
	struct patm_vcc	*vcc;		/* the VCC of this PDU */
	struct mbuf	*mbuf;
	u_int		hdr;		/* cell header */
};

static __inline u_int
cbr2slots(struct patm_softc *sc, struct patm_vcc *vcc)
{
	/* compute the number of slots we need, make sure to get at least
	 * the specified PCR */
	return ((u_int)(((uint64_t)(sc->mmap->tst_size - 1) *
	    vcc->vcc.tparam.pcr + sc->ifatm.mib.pcr - 1) / sc->ifatm.mib.pcr));
}

static __inline u_int
slots2cr(struct patm_softc *sc, u_int slots)
{
	return ((slots * sc->ifatm.mib.pcr + sc->mmap->tst_size - 2) /
	    (sc->mmap->tst_size - 1));
}

/* check if we can open this one */
int
patm_tx_vcc_can_open(struct patm_softc *sc, struct patm_vcc *vcc)
{

	/* check resources */
	switch (vcc->vcc.traffic) {

	  case ATMIO_TRAFFIC_CBR:
	    {
		u_int slots = cbr2slots(sc, vcc);

		if (slots > sc->tst_free + sc->tst_reserve)
			return (EINVAL);
		break;
	    }

	  case ATMIO_TRAFFIC_VBR:
		if (vcc->vcc.tparam.scr > sc->bwrem)
			return (EINVAL);
		if (vcc->vcc.tparam.pcr > sc->ifatm.mib.pcr)
			return (EINVAL);
		if (vcc->vcc.tparam.scr > vcc->vcc.tparam.pcr ||
		    vcc->vcc.tparam.mbs == 0)
			return (EINVAL);
		break;

	  case ATMIO_TRAFFIC_ABR:
		if (vcc->vcc.tparam.tbe == 0 ||
		    vcc->vcc.tparam.nrm == 0)
			/* needed to compute CRM */
			return (EINVAL);
		if (vcc->vcc.tparam.pcr > sc->ifatm.mib.pcr ||
		    vcc->vcc.tparam.icr > vcc->vcc.tparam.pcr ||
		    vcc->vcc.tparam.mcr > vcc->vcc.tparam.icr)
			return (EINVAL);
		if (vcc->vcc.tparam.mcr > sc->bwrem ||
		    vcc->vcc.tparam.icr > sc->bwrem)
			return (EINVAL);
		break;
	}

	return (0);
}

#define	NEXT_TAG(T) do {				\
	(T) = ((T) + 1) % IDT_TSQE_TAG_SPACE;		\
    } while (0)

/*
 * open it
 */
void
patm_tx_vcc_open(struct patm_softc *sc, struct patm_vcc *vcc)
{
	struct patm_scd *scd;

	if (vcc->vcc.traffic == ATMIO_TRAFFIC_UBR) {
		/* we use UBR0 */
		vcc->scd = sc->scd0;
		vcc->vflags |= PATM_VCC_TX_OPEN;
		return;
	}

	/* get an SCD */
	scd = patm_scd_alloc(sc);
	if (scd == NULL) {
		/* should not happen */
		patm_printf(sc, "out of SCDs\n");
		return;
	}
	vcc->scd = scd;
	patm_scd_setup(sc, scd);
	patm_tct_setup(sc, scd, vcc);

	if (vcc->vcc.traffic != ATMIO_TRAFFIC_CBR)
		patm_tct_start(sc, vcc);

	vcc->vflags |= PATM_VCC_TX_OPEN;
}

/*
 * close the given vcc for transmission
 */
void
patm_tx_vcc_close(struct patm_softc *sc, struct patm_vcc *vcc)
{
	struct patm_scd *scd;
	struct mbuf *m;

	vcc->vflags |= PATM_VCC_TX_CLOSING;

	if (vcc->vcc.traffic == ATMIO_TRAFFIC_UBR) {
		/* let the queue PDUs go out */
		vcc->scd = NULL;
		vcc->vflags &= ~(PATM_VCC_TX_OPEN | PATM_VCC_TX_CLOSING);
		return;
	}
	scd = vcc->scd;

	/* empty the waitq */
	for (;;) {
		_IF_DEQUEUE(&scd->q, m);
		if (m == NULL)
			break;
		m_freem(m);
	}

	if (scd->num_on_card == 0) {
		/* we are idle */
		vcc->vflags &= ~PATM_VCC_TX_OPEN;

		if (vcc->vcc.traffic == ATMIO_TRAFFIC_CBR)
			patm_tst_free(sc, vcc);

		patm_sram_write4(sc, scd->sram + 0, 0, 0, 0, 0);
		patm_sram_write4(sc, scd->sram + 4, 0, 0, 0, 0);
		patm_scd_free(sc, scd);

		vcc->scd = NULL;
		vcc->vflags &= ~PATM_VCC_TX_CLOSING;

		return;
	}

	/* speed up transmission */
	patm_nor_write(sc, IDT_NOR_TCMDQ, IDT_TCMDQ_UIER(vcc->cid, 0xff));
	patm_nor_write(sc, IDT_NOR_TCMDQ, IDT_TCMDQ_ULACR(vcc->cid, 0xff));

	/* wait for the interrupt to drop the number to 0 */
	patm_debug(sc, VCC, "%u buffers still on card", scd->num_on_card);
}

/* transmission side finally closed */
void
patm_tx_vcc_closed(struct patm_softc *sc, struct patm_vcc *vcc)
{

	patm_debug(sc, VCC, "%u.%u TX closed", vcc->vcc.vpi, vcc->vcc.vci);

	if (vcc->vcc.traffic == ATMIO_TRAFFIC_VBR)
		sc->bwrem += vcc->vcc.tparam.scr;
}

/*
 * Pull off packets from the interface queue and try to transmit them.
 * If the transmission fails because of a full transmit channel, we drop
 * packets for CBR and queue them for other channels up to limit.
 * This limit should depend on the CDVT for VBR and ABR, but it doesn't.
 */
void
patm_start(struct ifnet *ifp)
{
	struct patm_softc *sc = (struct patm_softc *)ifp->if_softc;
	struct mbuf *m;
	struct atm_pseudohdr *aph;
	u_int vpi, vci, cid;
	struct patm_vcc *vcc;

	mtx_lock(&sc->mtx);
	if (!(ifp->if_flags & IFF_RUNNING)) {
		mtx_unlock(&sc->mtx);
		return;
	}

	while (1) {
		/* get a new mbuf */
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		/* split of pseudo header */
		if (m->m_len < sizeof(*aph) &&
		    (m = m_pullup(m, sizeof(*aph))) == NULL) {
			sc->ifatm.ifnet.if_oerrors++;
			continue;
		}

		aph = mtod(m, struct atm_pseudohdr *);
		vci = ATM_PH_VCI(aph);
		vpi = ATM_PH_VPI(aph);
		m_adj(m, sizeof(*aph));

		/* reject empty packets */
		if (m->m_pkthdr.len == 0) {
			m_freem(m);
			sc->ifatm.ifnet.if_oerrors++;
			continue;
		}

		/* check whether this is a legal vcc */
		if (!LEGAL_VPI(sc, vpi) || !LEGAL_VCI(sc, vci) || vci == 0) {
			m_freem(m);
			sc->ifatm.ifnet.if_oerrors++;
			continue;
		}
		cid = PATM_CID(sc, vpi, vci);
		vcc = sc->vccs[cid];
		if (vcc == NULL) {
			m_freem(m);
			sc->ifatm.ifnet.if_oerrors++;
			continue;
		}

		/* must be multiple of 48 if not AAL5 */
		if (vcc->vcc.aal == ATMIO_AAL_0 ||
		    vcc->vcc.aal == ATMIO_AAL_34) {
			/* XXX AAL3/4 format? */
			if (m->m_pkthdr.len % 48 != 0 &&
			    (m = patm_tx_pad(sc, m)) == NULL) {
				sc->ifatm.ifnet.if_oerrors++;
				continue;
			}
		} else if (vcc->vcc.aal == ATMIO_AAL_RAW) {
			switch (vcc->vflags & PATM_RAW_FORMAT) {

			  default:
			  case PATM_RAW_CELL:
				if (m->m_pkthdr.len != 53) {
					sc->ifatm.ifnet.if_oerrors++;
					m_freem(m);
					continue;
				}
				break;

			  case PATM_RAW_NOHEC:
				if (m->m_pkthdr.len != 52) {
					sc->ifatm.ifnet.if_oerrors++;
					m_freem(m);
					continue;
				}
				break;

			  case PATM_RAW_CS:
				if (m->m_pkthdr.len != 64) {
					sc->ifatm.ifnet.if_oerrors++;
					m_freem(m);
					continue;
				}
				break;
			}
		}

		/* save data */
		m->m_pkthdr.header = vcc;

		/* try to put it on the channels queue */
		if (_IF_QFULL(&vcc->scd->q)) {
			sc->ifatm.ifnet.if_oerrors++;
			sc->stats.tx_qfull++;
			m_freem(m);
			continue;
		}
		_IF_ENQUEUE(&vcc->scd->q, m);

#ifdef ENABLE_BPF
		if (!(vcc->vcc.flags & ATMIO_FLAG_NG) &&
		    (vcc->vcc.flags & ATM_PH_AAL5) &&
		    (vcc->vcc.flags & ATM_PH_LLCSNAP))
		 	BPF_MTAP(ifp, m);
#endif

		/* kick the channel to life */
		patm_launch(sc, vcc->scd);

	}
	mtx_unlock(&sc->mtx);
}

/*
 * Pad non-AAL5 packet to a multiple of 48-byte.
 * We assume AAL0 only. We have still to decide on the format of AAL3/4.
 */
static struct mbuf *
patm_tx_pad(struct patm_softc *sc, struct mbuf *m0)
{
	struct mbuf *last, *m;
	u_int plen, pad, space;

	plen = m_length(m0, &last);
	if (plen != m0->m_pkthdr.len) {
		patm_printf(sc, "%s: mbuf length mismatch %d %u\n", __func__,
		    m0->m_pkthdr.len, plen);
		m0->m_pkthdr.len = plen;
		if (plen == 0) {
			m_freem(m0);
			sc->ifatm.ifnet.if_oerrors++;
			return (NULL);
		}
		if (plen % 48 == 0)
			return (m0);
	}
	pad = 48 - plen % 48;
	if (M_WRITABLE(last)) {
		if (M_TRAILINGSPACE(last) >= pad) {
			bzero(last->m_data + last->m_len, pad);
			last->m_len += pad;
			return (m0);
		}
		space = M_LEADINGSPACE(last);
		if (space + M_TRAILINGSPACE(last) >= pad) {
			bcopy(last->m_data, last->m_data + space, last->m_len);
			last->m_data -= space;
			bzero(last->m_data + last->m_len, pad);
			last->m_len += pad;
			return (m0);
		}
	}
	MGET(m, M_DONTWAIT, MT_DATA);
	if (m == 0) {
		m_freem(m0);
		sc->ifatm.ifnet.if_oerrors++;
		return (NULL);
	}
	bzero(mtod(m, u_char *), pad);
	m->m_len = pad;
	last->m_next = m;

	return (m0);
}

/*
 * Try to put as many packets from the channels queue onto the channel
 */
static void
patm_launch(struct patm_softc *sc, struct patm_scd *scd)
{
	struct txarg a;
	struct mbuf *m, *tmp;
	u_int segs;
	struct patm_txmap *map;
	int error;

	a.sc = sc;
	a.scd = scd;

	/* limit the number of outstanding packets to the tag space */
	while (scd->num_on_card < IDT_TSQE_TAG_SPACE) {
		/* get the next packet */
		_IF_DEQUEUE(&scd->q, m);
		if (m == NULL)
			break;

		a.vcc = m->m_pkthdr.header;

		/* we must know the number of segments beforehand - count
		 * this may actually give a wrong number of segments for
		 * AAL_RAW where we still need to remove the cell header */
		segs = 0;
		for (tmp = m; tmp != NULL; tmp = tmp->m_next)
			if (tmp->m_len != 0)
				segs++;

		/* check whether there is space in the queue */
		if (segs >= scd->space) {
			/* put back */
			_IF_PREPEND(&scd->q, m);
			sc->stats.tx_out_of_tbds++;
			break;
		}

		/* get a DMA map */
		if ((map = patm_txmap_get(sc)) == NULL) {
			_IF_PREPEND(&scd->q, m);
			sc->stats.tx_out_of_maps++;
			break;
		}

		/* load the map */
		m->m_pkthdr.header = map;
		a.mbuf = m;

		/* handle AAL_RAW */
		if (a.vcc->vcc.aal == ATMIO_AAL_RAW) {
			u_char hdr[4];

			m_copydata(m, 0, 4, hdr);
			a.hdr = (hdr[0] << 24) | (hdr[1] << 16) |
			    (hdr[2] << 8) | hdr[3];

			switch (a.vcc->vflags & PATM_RAW_FORMAT) {

			  default:
			  case PATM_RAW_CELL:
				m_adj(m, 5);
				break;

			  case PATM_RAW_NOHEC:
				m_adj(m, 4);
				break;

			  case PATM_RAW_CS:
				m_adj(m, 16);
				break;
			}
		} else
			a.hdr = IDT_TBD_HDR(a.vcc->vcc.vpi, a.vcc->vcc.vci,
			    0, 0);

		error = bus_dmamap_load_mbuf(sc->tx_tag, map->map, m,
		    patm_load_txbuf, &a, BUS_DMA_NOWAIT);
		if (error == EFBIG) {
			if ((m = m_defrag(m, M_DONTWAIT)) == NULL) {
				sc->ifatm.ifnet.if_oerrors++;
				continue;
			}
			error = bus_dmamap_load_mbuf(sc->tx_tag, map->map, m,
			    patm_load_txbuf, &a, BUS_DMA_NOWAIT);
		}
		if (error != 0) {
			sc->stats.tx_load_err++;
			sc->ifatm.ifnet.if_oerrors++;
			SLIST_INSERT_HEAD(&sc->tx_maps_free, map, link);
			m_freem(m);
			continue;
		}

		sc->ifatm.ifnet.if_opackets++;
	}
}

/*
 * Load the DMA segments into the scheduling channel
 */
static void
patm_load_txbuf(void *uarg, bus_dma_segment_t *segs, int nseg,
    bus_size_t mapsize, int error)
{
	struct txarg *a= uarg;
	struct patm_scd *scd = a->scd;
	u_int w1, w3, cnt;
	struct idt_tbd *tbd = NULL;
	u_int rest = mapsize;

	if (error != 0)
		return;

	cnt = 0;
	while (nseg > 0) {
		if (segs->ds_len == 0) {
			/* transmit buffer length must be > 0 */
			nseg--;
			segs++;
			continue;
		}
		/* rest after this buffer */
		rest -= segs->ds_len;

		/* put together status word */
		w1 = 0;
		if (rest < 48 /* && a->vcc->vcc.aal != ATMIO_AAL_5 */)
			/* last cell is in this buffer */
			w1 |= IDT_TBD_EPDU;

		if (a->vcc->vcc.aal == ATMIO_AAL_5)
			w1 |= IDT_TBD_AAL5;
		else if (a->vcc->vcc.aal == ATMIO_AAL_34)
			w1 |= IDT_TBD_AAL34;
		else
			w1 |= IDT_TBD_AAL0;

		w1 |= segs->ds_len;

		/* AAL5 PDU length (unpadded) */
		if (a->vcc->vcc.aal == ATMIO_AAL_5)
			w3 = mapsize;
		else
			w3 = 0;

		if (rest == 0)
			w1 |= IDT_TBD_TSIF | IDT_TBD_GTSI |
			    (scd->tag << IDT_TBD_TAG_SHIFT);

		tbd = &scd->scq[scd->tail];

		tbd->flags = htole32(w1);
		tbd->addr = htole32(segs->ds_addr);
		tbd->aal5 = htole32(w3);
		tbd->hdr = htole32(a->hdr);

		patm_debug(a->sc, TX, "TBD(%u): %08x %08x %08x %08x",
		    scd->tail, w1, segs->ds_addr, w3, a->hdr);

		/* got to next entry */
		if (++scd->tail == IDT_SCQ_SIZE)
			scd->tail = 0;
		cnt++;
		nseg--;
		segs++;
	}
	scd->space -= cnt;
	scd->num_on_card++;

	KASSERT(rest == 0, ("bad mbuf"));
	KASSERT(cnt > 0, ("no segs"));
	KASSERT(scd->space > 0, ("scq full"));

	KASSERT(scd->on_card[scd->tag] == NULL,
	    ("scd on_card wedged %u%s", scd->tag, dump_scd(a->sc, scd)));
	scd->on_card[scd->tag] = a->mbuf;
	a->mbuf->m_pkthdr.csum_data = cnt;

	NEXT_TAG(scd->tag);

	patm_debug(a->sc, TX, "SCD tail %u (%lx:%lx)", scd->tail,
	    (u_long)scd->phy, (u_long)scd->phy + (scd->tail << IDT_TBD_SHIFT));
	patm_sram_write(a->sc, scd->sram,
	    scd->phy + (scd->tail << IDT_TBD_SHIFT));

	if (patm_sram_read(a->sc, a->vcc->cid * 8 + 3) & IDT_TCT_IDLE) {
		/*
		 * if the connection is idle start it. We cannot rely
		 * on a flag set by patm_tx_idle() here, because sometimes
		 * the card seems to place an idle TSI into the TSQ but
		 * forgets to raise an interrupt.
		 */
		patm_nor_write(a->sc, IDT_NOR_TCMDQ,
		    IDT_TCMDQ_START(a->vcc->cid));
	}
}

/*
 * packet transmitted
 */
void
patm_tx(struct patm_softc *sc, u_int stamp, u_int status)
{
	u_int cid, tag, last;
	struct mbuf *m;
	struct patm_vcc *vcc;
	struct patm_scd *scd;
	struct patm_txmap *map;

	/* get the connection */
	cid = PATM_CID(sc, IDT_TBD_VPI(status), IDT_TBD_VCI(status));
	if ((vcc = sc->vccs[cid]) == NULL) {
		/* closed UBR connection */
		return;
	}
	scd = vcc->scd;

	tag = IDT_TSQE_TAG(stamp);

	last = scd->last_tag;
	if (tag == last) {
		patm_printf(sc, "same tag %u\n", tag);
		return;
	}

	/* Errata 12 requests us to free all entries up to the one
	 * with the given tag. */
	do {
		/* next tag to try */
		NEXT_TAG(last);

		m = scd->on_card[last];
		KASSERT(m != NULL, ("%stag=%u", dump_scd(sc, scd), tag));
		scd->on_card[last] = NULL;
		patm_debug(sc, TX, "ok tag=%x", last);

		map = m->m_pkthdr.header;
		scd->space += m->m_pkthdr.csum_data;

		bus_dmamap_sync(sc->tx_tag, map->map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->tx_tag, map->map);
		m_freem(m);
		SLIST_INSERT_HEAD(&sc->tx_maps_free, map, link);
		scd->num_on_card--;

		if (vcc->vflags & PATM_VCC_TX_CLOSING) {
			if (scd->num_on_card == 0) {
				/* done with this VCC */
				if (vcc->vcc.traffic == ATMIO_TRAFFIC_CBR)
					patm_tst_free(sc, vcc);

				patm_sram_write4(sc, scd->sram + 0, 0, 0, 0, 0);
				patm_sram_write4(sc, scd->sram + 4, 0, 0, 0, 0);
				patm_scd_free(sc, scd);

				vcc->scd = NULL;
				vcc->vflags &= ~PATM_VCC_TX_CLOSING;

				if (vcc->vflags & PATM_VCC_ASYNC) {
					patm_tx_vcc_closed(sc, vcc);
					if (!(vcc->vflags & PATM_VCC_OPEN))
						patm_vcc_closed(sc, vcc);
				} else
					cv_signal(&sc->vcc_cv);
				return;
			}
			patm_debug(sc, VCC, "%u buffers still on card",
			    scd->num_on_card);

			if (vcc->vcc.traffic == ATMIO_TRAFFIC_ABR) {
				/* insist on speeding up transmission for ABR */
				patm_nor_write(sc, IDT_NOR_TCMDQ,
				    IDT_TCMDQ_UIER(vcc->cid, 0xff));
				patm_nor_write(sc, IDT_NOR_TCMDQ,
				    IDT_TCMDQ_ULACR(vcc->cid, 0xff));
			}
		}

	} while (last != tag);
	scd->last_tag = tag;

	if (vcc->vcc.traffic == ATMIO_TRAFFIC_ABR) {
		u_int acri, cps;

		acri = (patm_sram_read(sc, 8 * cid + 2) >> IDT_TCT_ACRI_SHIFT)
		    & 0x3fff;
		cps = sc->ifatm.mib.pcr * 32 /
		    ((1 << (acri >> 10)) * (acri & 0x3ff));

		if (cps != vcc->cps) {
			/* send message */
			patm_debug(sc, VCC, "ACRI=%04x CPS=%u", acri, cps);
			vcc->cps = cps;
		}
	}

	patm_launch(sc, scd);
}

/*
 * VBR/ABR connection went idle
 * Either restart it or set the idle flag.
 */
void
patm_tx_idle(struct patm_softc *sc, u_int cid)
{
	struct patm_vcc *vcc;

	patm_debug(sc, VCC, "idle %u", cid);

	if ((vcc = sc->vccs[cid]) != NULL &&
	    (vcc->vflags & (PATM_VCC_TX_OPEN | PATM_VCC_TX_CLOSING)) != 0 &&
	    vcc->scd != NULL && (vcc->scd->num_on_card != 0 ||
	    _IF_QLEN(&vcc->scd->q) != 0)) {
		/*
		 * If there is any packet outstanding in the SCD re-activate
		 * the channel and kick it.
		 */
		patm_nor_write(sc, IDT_NOR_TCMDQ,
		    IDT_TCMDQ_START(vcc->cid));

		patm_launch(sc, vcc->scd);
	}
}

/*
 * Convert a (24bit) rate to the atm-forum form
 * Our rate is never larger than 19 bit.
 */
static u_int
cps2atmf(u_int cps)
{
	u_int e;

	if (cps == 0)
		return (0);
	cps <<= 9;
	e = 0;
	while (cps > (1024 - 1)) {
		e++;
		cps >>= 1;
	}
	return ((1 << 14) | (e << 9) | (cps & 0x1ff));
}

/*
 * Do a binary search on the log2rate table to convert the rate
 * to its log form. This assumes that the ATM-Forum form is monotonically
 * increasing with the plain cell rate.
 */
static u_int
rate2log(struct patm_softc *sc, u_int rate)
{
	const uint32_t *tbl;
	u_int lower, upper, mid, done, val, afr;

	afr = cps2atmf(rate);

	if (sc->flags & PATM_25M)
		tbl = patm_rtables25;
	else
		tbl = patm_rtables155;

	lower = 0;
	upper = 255;
	done = 0;
	while (!done) {
		mid = (lower + upper) / 2;
		val = tbl[mid] >> 17;
		if (val == afr || upper == lower)
			break;
		if (afr > val)
			lower = mid + 1;
		else
			upper = mid - 1;
	}
	if (val > afr && mid > 0)
		mid--;
	return (mid);
}

/*
 * Return the table index for an increase table. The increase table
 * must be selected not by the RIF itself, but by PCR/2^RIF. Each table
 * represents an additive increase of a cell rate that can be computed
 * from the first table entry (the value in this entry will not be clamped
 * by the link rate).
 */
static u_int
get_air_table(struct patm_softc *sc, u_int rif, u_int pcr)
{
	const uint32_t *tbl;
	u_int increase, base, lair0, ret, t, cps;

#define	GET_ENTRY(TAB, IDX) (0xffff & ((IDX & 1) ?			\
	(tbl[512 + (IDX / 2) + 128 * (TAB)] >> 16) :			\
	(tbl[512 + (IDX / 2) + 128 * (TAB)])))

#define	MANT_BITS	10
#define	FRAC_BITS	16

#define	DIFF_TO_FP(D)	(((D) & ((1 << MANT_BITS) - 1)) << ((D) >> MANT_BITS))
#define	AFR_TO_INT(A)	((1 << (((A) >> 9) & 0x1f)) * \
			    (512 + ((A) & 0x1ff)) / 512 * ((A) >> 14))

	if (sc->flags & PATM_25M)
		tbl = patm_rtables25;
	else
		tbl = patm_rtables155;
	if (rif >= patm_rtables_ntab)
		rif = patm_rtables_ntab - 1;
	increase = pcr >> rif;

	ret = 0;
	for (t = 0; t < patm_rtables_ntab; t++) {
		/* get base rate of this table */
		base = GET_ENTRY(t, 0);
		/* convert this to fixed point */
		lair0 = DIFF_TO_FP(base) >> FRAC_BITS;

		/* get the CPS from the log2rate table */
		cps = AFR_TO_INT(tbl[lair0] >> 17) - 10;

		if (increase >= cps)
			break;

		ret = t;
	}
	return (ret + 4);
}

/*
 * Setup the TCT
 */
void
patm_tct_setup(struct patm_softc *sc, struct patm_scd *scd,
    struct patm_vcc *vcc)
{
	uint32_t tct[8];
	u_int sram;
	u_int mbs, token;
	u_int tmp, crm, rdf, cdf, air, mcr;

	bzero(tct, sizeof(tct));
	if (vcc == NULL) {
		/* special case for UBR0 */
		sram = 0;
		tct[0] = IDT_TCT_UBR | scd->sram;
		tct[7] = IDT_TCT_UBR_FLG;

	} else {
		sram = vcc->cid * 8;
		switch (vcc->vcc.traffic) {

		  case ATMIO_TRAFFIC_CBR:
			patm_tst_alloc(sc, vcc);
			tct[0] = IDT_TCT_CBR | scd->sram;
			/* must account for what was really allocated */
			break;

		  case ATMIO_TRAFFIC_VBR:
			/* compute parameters for the TCT */
			scd->init_er = rate2log(sc, vcc->vcc.tparam.pcr);
			scd->lacr = rate2log(sc, vcc->vcc.tparam.scr);

			/* get the 16-bit fraction of SCR/PCR
			 * both a 24 bit. Do it the simple way. */
			token = (uint64_t)(vcc->vcc.tparam.scr << 16) /
			    vcc->vcc.tparam.pcr;

			patm_debug(sc, VCC, "VBR: init_er=%u lacr=%u "
			    "token=0x%04x\n", scd->init_er, scd->lacr, token);

			tct[0] = IDT_TCT_VBR | scd->sram;
			tct[2] = IDT_TCT_TSIF;
			tct[3] = IDT_TCT_IDLE | IDT_TCT_HALT;
			tct[4] = IDT_TCT_MAXIDLE;
			tct[5] = 0x01000000;
			if ((mbs = vcc->vcc.tparam.mbs) > 0xff)
				mbs = 0xff;
			tct[6] = (mbs << 16) | token;
			sc->bwrem -= vcc->vcc.tparam.scr;
			break;

		  case ATMIO_TRAFFIC_ABR:
			scd->init_er = rate2log(sc, vcc->vcc.tparam.pcr);
			scd->lacr = rate2log(sc, vcc->vcc.tparam.icr);
			mcr = rate2log(sc, vcc->vcc.tparam.mcr);

			/* compute CRM */
			tmp = vcc->vcc.tparam.tbe / vcc->vcc.tparam.nrm;
			if (tmp * vcc->vcc.tparam.nrm < vcc->vcc.tparam.tbe)
				tmp++;
			for (crm = 1; tmp > (1 << crm); crm++)
				;
			if (crm > 0x7)
				crm = 7;

			air = get_air_table(sc, vcc->vcc.tparam.rif,
			    vcc->vcc.tparam.pcr);

			if ((rdf = vcc->vcc.tparam.rdf) >= patm_rtables_ntab)
				rdf = patm_rtables_ntab - 1;
			rdf += patm_rtables_ntab + 4;

			if ((cdf = vcc->vcc.tparam.cdf) >= patm_rtables_ntab)
				cdf = patm_rtables_ntab - 1;
			cdf += patm_rtables_ntab + 4;

			patm_debug(sc, VCC, "ABR: init_er=%u lacr=%u mcr=%u "
			    "crm=%u air=%u rdf=%u cdf=%u\n", scd->init_er,
			    scd->lacr, mcr, crm, air, rdf, cdf);

			tct[0] = IDT_TCT_ABR | scd->sram;
			tct[1] = crm << IDT_TCT_CRM_SHIFT;
			tct[3] = IDT_TCT_HALT | IDT_TCT_IDLE |
			    (4 << IDT_TCT_NAGE_SHIFT);
			tct[4] = mcr << IDT_TCT_LMCR_SHIFT;
			tct[5] = (cdf << IDT_TCT_CDF_SHIFT) |
			    (rdf << IDT_TCT_RDF_SHIFT) |
			    (air << IDT_TCT_AIR_SHIFT);

			sc->bwrem -= vcc->vcc.tparam.mcr;
			break;
		}
	}

	patm_sram_write4(sc, sram + 0, tct[0], tct[1], tct[2], tct[3]);
	patm_sram_write4(sc, sram + 4, tct[4], tct[5], tct[6], tct[7]);

	patm_debug(sc, VCC, "TCT[%u]: %08x %08x %08x %08x  %08x %08x %08x %08x",
	    sram / 8, patm_sram_read(sc, sram + 0),
	    patm_sram_read(sc, sram + 1), patm_sram_read(sc, sram + 2),
	    patm_sram_read(sc, sram + 3), patm_sram_read(sc, sram + 4),
	    patm_sram_read(sc, sram + 5), patm_sram_read(sc, sram + 6),
	    patm_sram_read(sc, sram + 7));
}

/*
 * Start a channel
 */
static void
patm_tct_start(struct patm_softc *sc, struct patm_vcc *vcc)
{

	patm_nor_write(sc, IDT_NOR_TCMDQ, IDT_TCMDQ_UIER(vcc->cid,
	    vcc->scd->init_er));
	patm_nor_write(sc, IDT_NOR_TCMDQ, IDT_TCMDQ_SLACR(vcc->cid,
	    vcc->scd->lacr));
}

static void
patm_tct_print(struct patm_softc *sc, u_int cid)
{
#ifdef PATM_DEBUG
	u_int sram = cid * 8;
#endif

	patm_debug(sc, VCC, "TCT[%u]: %08x %08x %08x %08x  %08x %08x %08x %08x",
	    sram / 8, patm_sram_read(sc, sram + 0),
	    patm_sram_read(sc, sram + 1), patm_sram_read(sc, sram + 2),
	    patm_sram_read(sc, sram + 3), patm_sram_read(sc, sram + 4),
	    patm_sram_read(sc, sram + 5), patm_sram_read(sc, sram + 6),
	    patm_sram_read(sc, sram + 7));
}

/*
 * Setup the SCD
 */
void
patm_scd_setup(struct patm_softc *sc, struct patm_scd *scd)
{
	patm_sram_write4(sc, scd->sram + 0,
	    scd->phy, 0, 0xffffffff, 0);
	patm_sram_write4(sc, scd->sram + 4,
	    0, 0, 0, 0);

	patm_debug(sc, VCC, "SCD(%x): %08x %08x %08x %08x %08x %08x %08x %08x",
	    scd->sram,
	    patm_sram_read(sc, scd->sram + 0),
	    patm_sram_read(sc, scd->sram + 1),
	    patm_sram_read(sc, scd->sram + 2),
	    patm_sram_read(sc, scd->sram + 3),
	    patm_sram_read(sc, scd->sram + 4),
	    patm_sram_read(sc, scd->sram + 5),
	    patm_sram_read(sc, scd->sram + 6),
	    patm_sram_read(sc, scd->sram + 7));
}

/*
 * Grow the TX map table if possible
 */
static void
patm_txmaps_grow(struct patm_softc *sc)
{
	u_int i;
	struct patm_txmap *map;
	int err;

	if (sc->tx_nmaps >= sc->tx_maxmaps)
		return;

	for (i = sc->tx_nmaps; i < sc->tx_nmaps + PATM_CFG_TXMAPS_STEP; i++) {
		map = uma_zalloc(sc->tx_mapzone, M_NOWAIT);
		err = bus_dmamap_create(sc->tx_tag, 0, &map->map);
		if (err) {
			uma_zfree(sc->tx_mapzone, map);
			break;
		}
		SLIST_INSERT_HEAD(&sc->tx_maps_free, map, link);
	}

	sc->tx_nmaps = i;
}

/*
 * Allocate a transmission map
 */
static struct patm_txmap *
patm_txmap_get(struct patm_softc *sc)
{
	struct patm_txmap *map;

	if ((map = SLIST_FIRST(&sc->tx_maps_free)) == NULL) {
		patm_txmaps_grow(sc);
		if ((map = SLIST_FIRST(&sc->tx_maps_free)) == NULL)
			return (NULL);
	}
	SLIST_REMOVE_HEAD(&sc->tx_maps_free, link);
	return (map);
}

/*
 * Look whether we are in the process of updating the TST on the chip.
 * If we are set the flag that we need another update.
 * If we are not start the update.
 */
static __inline void
patm_tst_start(struct patm_softc *sc)
{

	if (!(sc->tst_state & TST_PENDING)) {
		sc->tst_state |= TST_PENDING;
		if (!(sc->tst_state & TST_WAIT)) {
			/* timer not running */
			patm_tst_update(sc);
		}
	}
}

/*
 * Allocate TST entries to a CBR connection
 */
static void
patm_tst_alloc(struct patm_softc *sc, struct patm_vcc *vcc)
{
	u_int slots;
	u_int qptr, pptr;
	u_int qmax, pmax;
	u_int pspc, last;

	mtx_lock(&sc->tst_lock);

	/* compute the number of slots we need, make sure to get at least
	 * the specified PCR */
	slots = cbr2slots(sc, vcc);
	vcc->scd->slots = slots;
	sc->bwrem -= slots2cr(sc, slots);

	patm_debug(sc, TST, "tst_alloc: cbr=%u link=%u tst=%u slots=%u",
	    vcc->vcc.tparam.pcr, sc->ifatm.mib.pcr, sc->mmap->tst_size, slots);

	qmax = sc->mmap->tst_size - 1;
	pmax = qmax << 8;

	pspc = pmax / slots;

	pptr = pspc >> 1;	/* starting point */
	qptr = pptr >> 8;

	last = qptr;

	while (slots > 0) {
		if (qptr >= qmax)
			qptr -= qmax;
		if (sc->tst_soft[qptr] != IDT_TST_VBR) {
			/* used - try next */
			qptr++;
			continue;
		}
		patm_debug(sc, TST, "slot[%u] = %u.%u diff=%d", qptr,
		    vcc->vcc.vpi, vcc->vcc.vci, (int)qptr - (int)last);
		last = qptr;

		sc->tst_soft[qptr] = IDT_TST_CBR | vcc->cid | TST_BOTH;
		sc->tst_free--;

		if ((pptr += pspc) >= pmax)
			pptr -= pmax;
		qptr = pptr >> 8;

		slots--;
	}
	patm_tst_start(sc);
	mtx_unlock(&sc->tst_lock);
}

/*
 * Free a CBR connection's TST entries
 */
static void
patm_tst_free(struct patm_softc *sc, struct patm_vcc *vcc)
{
	u_int i;

	mtx_lock(&sc->tst_lock);
	for (i = 0; i < sc->mmap->tst_size - 1; i++) {
		if ((sc->tst_soft[i] & IDT_TST_MASK) == vcc->cid) {
			sc->tst_soft[i] = IDT_TST_VBR | TST_BOTH;
			sc->tst_free++;
		}
	}
	sc->bwrem += slots2cr(sc, vcc->scd->slots);
	patm_tst_start(sc);
	mtx_unlock(&sc->tst_lock);
}

/*
 * Write the soft TST into the idle incore TST and start the wait timer.
 * We assume that we hold the tst lock.
 */
static void
patm_tst_update(struct patm_softc *sc)
{
	u_int flag;		/* flag to clear from soft TST */
	u_int idle;		/* the idle TST */
	u_int act;		/* the active TST */
	u_int i;

	if (sc->tst_state & TST_ACT1) {
		act = 1;
		idle = 0;
		flag = TST_CH0;
	} else {
		act = 0;
		idle = 1;
		flag = TST_CH1;
	}
	/* update the idle one */
	for (i = 0; i < sc->mmap->tst_size - 1; i++)
		if (sc->tst_soft[i] & flag) {
			patm_sram_write(sc, sc->tst_base[idle] + i,
			    sc->tst_soft[i] & ~TST_BOTH);
			sc->tst_soft[i] &= ~flag;
		}
	/* the used one jump to the idle one */
	patm_sram_write(sc, sc->tst_jump[act],
	    IDT_TST_BR | (sc->tst_base[idle] << 2));

	/* wait for the chip to jump */
	sc->tst_state &= ~TST_PENDING;
	sc->tst_state |= TST_WAIT;

	callout_reset(&sc->tst_callout, 1, patm_tst_timer, sc);
}

/*
 * Timer for TST updates
 */
static void
patm_tst_timer(void *p)
{
	struct patm_softc *sc = p;
	u_int act;	/* active TST */
	u_int now;	/* current place in TST */

	mtx_lock(&sc->tst_lock);

	if (sc->tst_state & TST_WAIT) {
		/* ignore the PENDING state while we are waiting for
		 * the chip to switch tables. Once the switch is done,
		 * we will again lock at PENDING */
		act = (sc->tst_state & TST_ACT1) ? 1 : 0;
		now = patm_nor_read(sc, IDT_NOR_NOW) >> 2;
		if (now >= sc->tst_base[act] && now <= sc->tst_jump[act]) {
			/* not yet */
			callout_reset(&sc->tst_callout, 1, patm_tst_timer, sc);
			goto done;
		}
		sc->tst_state &= ~TST_WAIT;
		/* change back jump */
		patm_sram_write(sc, sc->tst_jump[act],
		    IDT_TST_BR | (sc->tst_base[act] << 2));

		/* switch */
		sc->tst_state ^= TST_ACT1;
	}

	if (sc->tst_state & TST_PENDING)
		/* we got another update request while the timer was running. */
		patm_tst_update(sc);

  done:
	mtx_unlock(&sc->tst_lock);
}

static const char *
dump_scd(struct patm_softc *sc, struct patm_scd *scd)
{
	u_int i;

	for (i = 0; i < IDT_TSQE_TAG_SPACE; i++)
		printf("on_card[%u] = %p\n", i, scd->on_card[i]);
	printf("space=%u tag=%u num_on_card=%u last_tag=%u\n",
	    scd->space, scd->tag, scd->num_on_card, scd->last_tag);

	return ("");
}
