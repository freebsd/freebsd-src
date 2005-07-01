/*-
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
 * ForeHE driver.
 *
 * Transmission.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_natm.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
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
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/utopia/utopia.h>
#include <dev/hatm/if_hatmconf.h>
#include <dev/hatm/if_hatmreg.h>
#include <dev/hatm/if_hatmvar.h>


/*
 * These macros are used to trace the flow of transmit mbufs and to
 * detect transmit mbuf leaks in the driver.
 */
#ifdef HATM_DEBUG
#define	hatm_free_txmbuf(SC)						\
	do {								\
		if (--sc->txmbuf < 0)					\
			DBG(sc, TX, ("txmbuf below 0!"));		\
		else if (sc->txmbuf == 0)				\
			DBG(sc, TX, ("txmbuf now 0"));			\
	} while (0)
#define	hatm_get_txmbuf(SC)						\
	do {								\
		if (++sc->txmbuf > 20000)				\
			DBG(sc,	TX, ("txmbuf %u", sc->txmbuf));		\
		else if (sc->txmbuf == 1)				\
			DBG(sc, TX, ("txmbuf leaves 0"));		\
	} while (0)
#else
#define	hatm_free_txmbuf(SC)	do { } while (0)
#define	hatm_get_txmbuf(SC)	do { } while (0)
#endif

/*
 * Allocate a new TPD, zero the TPD part. Cannot return NULL if
 * flag is 0. The TPD is removed from the free list and its used
 * bit is set.
 */
static struct tpd *
hatm_alloc_tpd(struct hatm_softc *sc, u_int flags)
{
	struct tpd *t;

	/* if we allocate a transmit TPD check for the reserve */
	if (flags & M_NOWAIT) {
		if (sc->tpd_nfree <= HE_CONFIG_TPD_RESERVE)
			return (NULL);
	} else {
		if (sc->tpd_nfree == 0)
			return (NULL);
	}

	/* make it beeing used */
	t = SLIST_FIRST(&sc->tpd_free);
	KASSERT(t != NULL, ("tpd botch"));
	SLIST_REMOVE_HEAD(&sc->tpd_free, link);
	TPD_SET_USED(sc, t->no);
	sc->tpd_nfree--;

	/* initialize */
	t->mbuf = NULL;
	t->cid = 0;
	bzero(&t->tpd, sizeof(t->tpd));
	t->tpd.addr = t->no << HE_REGS_TPD_ADDR;

	return (t);
}

/*
 * Free a TPD. If the mbuf pointer in that TPD is not zero, it is assumed, that
 * the DMA map of this TPD was used to load this mbuf. The map is unloaded
 * and the mbuf is freed. The TPD is put back onto the free list and
 * its used bit is cleared.
 */
static void
hatm_free_tpd(struct hatm_softc *sc, struct tpd *tpd)
{
	if (tpd->mbuf != NULL) {
		bus_dmamap_unload(sc->tx_tag, tpd->map);
		hatm_free_txmbuf(sc);
		m_freem(tpd->mbuf);
		tpd->mbuf = NULL;
	}

	/* insert TPD into free list */
	SLIST_INSERT_HEAD(&sc->tpd_free, tpd, link);
	TPD_CLR_USED(sc, tpd->no);
	sc->tpd_nfree++;
}

/*
 * Queue a number of TPD. If there is not enough space none of the TPDs
 * is queued and an error code is returned.
 */
static int
hatm_queue_tpds(struct hatm_softc *sc, u_int count, struct tpd **list,
    u_int cid)
{
	u_int space;
	u_int i;

	if (count >= sc->tpdrq.size) {
		sc->istats.tdprq_full++;
		return (EBUSY);
	}

	if (sc->tpdrq.tail < sc->tpdrq.head)
		space = sc->tpdrq.head - sc->tpdrq.tail;
	else
		space = sc->tpdrq.head - sc->tpdrq.tail +  sc->tpdrq.size;

	if (space <= count) {
		sc->tpdrq.head =
		    (READ4(sc, HE_REGO_TPDRQ_H) >> HE_REGS_TPDRQ_H_H) &
		    (sc->tpdrq.size - 1);

		if (sc->tpdrq.tail < sc->tpdrq.head)
			space = sc->tpdrq.head - sc->tpdrq.tail;
		else
			space = sc->tpdrq.head - sc->tpdrq.tail +
			    sc->tpdrq.size;

		if (space <= count) {
			if_printf(sc->ifp, "TPDRQ full\n");
			sc->istats.tdprq_full++;
			return (EBUSY);
		}
	}

	/* we are going to write to the TPD queue space */
	bus_dmamap_sync(sc->tpdrq.mem.tag, sc->tpdrq.mem.map,
	    BUS_DMASYNC_PREWRITE);

	/* put the entries into the TPD space */
	for (i = 0; i < count; i++) {
		/* we are going to 'write' the TPD to the device */
		bus_dmamap_sync(sc->tpds.tag, sc->tpds.map,
		    BUS_DMASYNC_PREWRITE);

		sc->tpdrq.tpdrq[sc->tpdrq.tail].tpd =
		    sc->tpds.paddr + HE_TPD_SIZE * list[i]->no;
		sc->tpdrq.tpdrq[sc->tpdrq.tail].cid = cid;

		if (++sc->tpdrq.tail == sc->tpdrq.size)
			sc->tpdrq.tail = 0;
	}

	/* update tail pointer */
	WRITE4(sc, HE_REGO_TPDRQ_T, (sc->tpdrq.tail << HE_REGS_TPDRQ_T_T));

	return (0);
}

/*
 * Helper struct for communication with the DMA load helper.
 */
struct load_txbuf_arg {
	struct hatm_softc *sc;
	struct tpd *first;
	struct mbuf *mbuf;
	struct hevcc *vcc;
	int error;
	u_int pti;
	u_int vpi, vci;
};

/*
 * Loader callback for the mbuf. This function allocates the TPDs and
 * fills them. It puts the dmamap and and the mbuf pointer into the last
 * TPD and then tries to queue all the TPDs. If anything fails, all TPDs
 * allocated by this function are freed and the error flag is set in the
 * argument structure. The first TPD must then be freed by the caller.
 */
static void
hatm_load_txbuf(void *uarg, bus_dma_segment_t *segs, int nseg,
    bus_size_t mapsize, int error)
{
	struct load_txbuf_arg *arg = uarg;
	u_int tpds_needed, i, n, tpd_cnt;
	int need_intr;
	struct tpd *tpd;
	struct tpd *tpd_list[HE_CONFIG_MAX_TPD_PER_PACKET];

	if (error != 0) {
		DBG(arg->sc, DMA, ("%s -- error=%d plen=%d\n",
		    __func__, error, arg->mbuf->m_pkthdr.len));
		return;
	}

	/* ensure, we have enough TPDs (remember, we already have one) */
	tpds_needed = (nseg + 2) / 3;
	if (HE_CONFIG_TPD_RESERVE + tpds_needed - 1 > arg->sc->tpd_nfree) {
		if_printf(arg->sc->ifp, "%s -- out of TPDs (need %d, "
		    "have %u)\n", __func__, tpds_needed - 1,
		    arg->sc->tpd_nfree + 1);
		arg->error = 1;
		return;
	}

	/*
	 * Check for the maximum number of TPDs on the connection.
	 */
	need_intr = 0;
	if (arg->sc->max_tpd > 0) {
		if (arg->vcc->ntpds + tpds_needed > arg->sc->max_tpd) {
			arg->sc->istats.flow_closed++;
			arg->vcc->vflags |= HE_VCC_FLOW_CTRL;
			ATMEV_SEND_FLOW_CONTROL(IFP2IFATM(arg->sc->ifp),
			    arg->vpi, arg->vci, 1);
			arg->error = 1;
			return;
		}
		if (arg->vcc->ntpds + tpds_needed >
		    (9 * arg->sc->max_tpd) / 10)
			need_intr = 1;
	}

	tpd = arg->first;
	tpd_cnt = 0;
	tpd_list[tpd_cnt++] = tpd;
	for (i = n = 0; i < nseg; i++, n++) {
		if (n == 3) {
			if ((tpd = hatm_alloc_tpd(arg->sc, M_NOWAIT)) == NULL)
				/* may not fail (see check above) */
				panic("%s: out of TPDs", __func__);
			tpd->cid = arg->first->cid;
			tpd->tpd.addr |= arg->pti;
			tpd_list[tpd_cnt++] = tpd;
			n = 0;
		}
		KASSERT(segs[i].ds_addr <= 0xffffffffLU,
		    ("phys addr too large %lx", (u_long)segs[i].ds_addr));

		DBG(arg->sc, DMA, ("DMA loaded: %lx/%lu",
		    (u_long)segs[i].ds_addr, (u_long)segs[i].ds_len));

		tpd->tpd.bufs[n].addr = segs[i].ds_addr;
		tpd->tpd.bufs[n].len = segs[i].ds_len;

		DBG(arg->sc, TX, ("seg[%u]=tpd[%u,%u]=%x/%u", i,
		    tpd_cnt, n, tpd->tpd.bufs[n].addr, tpd->tpd.bufs[n].len));

		if (i == nseg - 1)
			tpd->tpd.bufs[n].len |= HE_REGM_TPD_LST;
	}

	/*
	 * Swap the MAP in the first and the last TPD and set the mbuf
	 * pointer into the last TPD. We use the map in the last TPD, because
	 * the map must stay valid until the last TPD is processed by the card.
	 */
	if (tpd_cnt > 1) {
		bus_dmamap_t tmp;

		tmp = arg->first->map;
		arg->first->map = tpd_list[tpd_cnt - 1]->map;
		tpd_list[tpd_cnt - 1]->map = tmp;
	}
	tpd_list[tpd_cnt - 1]->mbuf = arg->mbuf;

	if (need_intr)
		tpd_list[tpd_cnt - 1]->tpd.addr |= HE_REGM_TPD_INTR;

	/* queue the TPDs */
	if (hatm_queue_tpds(arg->sc, tpd_cnt, tpd_list, arg->first->cid)) {
		/* free all, except the first TPD */
		for (i = 1; i < tpd_cnt; i++)
			hatm_free_tpd(arg->sc, tpd_list[i]);
		arg->error = 1;
		return;
	}
	arg->vcc->ntpds += tpd_cnt;
}


/*
 * Start output on the interface
 */
void
hatm_start(struct ifnet *ifp)
{
	struct hatm_softc *sc = ifp->if_softc;
	struct mbuf *m;
	struct atm_pseudohdr *aph;
	u_int cid;
	struct tpd *tpd;
	struct load_txbuf_arg arg;
	u_int len;
	int error;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;
	mtx_lock(&sc->mtx);
	arg.sc = sc;

	while (1) {
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		hatm_get_txmbuf(sc);

		if (m->m_len < sizeof(*aph))
			if ((m = m_pullup(m, sizeof(*aph))) == NULL) {
				hatm_free_txmbuf(sc);
				continue;
			}

		aph = mtod(m, struct atm_pseudohdr *);
		arg.vci = ATM_PH_VCI(aph);
		arg.vpi = ATM_PH_VPI(aph);
		m_adj(m, sizeof(*aph));

		if ((len = m->m_pkthdr.len) == 0) {
			hatm_free_txmbuf(sc);
			m_freem(m);
			continue;
		}

		if ((arg.vpi & ~HE_VPI_MASK) || (arg.vci & ~HE_VCI_MASK) ||
		    (arg.vci == 0)) {
			hatm_free_txmbuf(sc);
			m_freem(m);
			continue;
		}
		cid = HE_CID(arg.vpi, arg.vci);
		arg.vcc = sc->vccs[cid];

		if (arg.vcc == NULL || !(arg.vcc->vflags & HE_VCC_OPEN)) {
			hatm_free_txmbuf(sc);
			m_freem(m);
			continue;
		}
		if (arg.vcc->vflags & HE_VCC_FLOW_CTRL) {
			hatm_free_txmbuf(sc);
			m_freem(m);
			sc->istats.flow_drop++;
			continue;
		}

		arg.pti = 0;
		if (arg.vcc->param.aal == ATMIO_AAL_RAW) {
			if (len < 52) {
				/* too short */
				hatm_free_txmbuf(sc);
				m_freem(m);
				continue;
			}

			/*
			 * Get the header and ignore except
			 * payload type and CLP.
			 */
			if (m->m_len < 4 && (m = m_pullup(m, 4)) == NULL) {
				hatm_free_txmbuf(sc);
				continue;
			}
			arg.pti = mtod(m, u_char *)[3] & 0xf;
			arg.pti = ((arg.pti & 0xe) << 2) | ((arg.pti & 1) << 1);
			m_adj(m, 4);
			len -= 4;

			if (len % 48 != 0) {
				m_adj(m, -((int)(len % 48)));
				len -= len % 48;
			}
		}

#ifdef ENABLE_BPF
		if (!(arg.vcc->param.flags & ATMIO_FLAG_NG) &&
		    (arg.vcc->param.aal == ATMIO_AAL_5) &&
		    (arg.vcc->param.flags & ATM_PH_LLCSNAP))
		 	BPF_MTAP(ifp, m);
#endif

		/* Now load a DMA map with the packet. Allocate the first
		 * TPD to get a map. Additional TPDs may be allocated by the
		 * callback. */
		if ((tpd = hatm_alloc_tpd(sc, M_NOWAIT)) == NULL) {
			hatm_free_txmbuf(sc);
			m_freem(m);
			sc->ifp->if_oerrors++;
			continue;
		}
		tpd->cid = cid;
		tpd->tpd.addr |= arg.pti;
		arg.first = tpd;
		arg.error = 0;
		arg.mbuf = m;

		error = bus_dmamap_load_mbuf(sc->tx_tag, tpd->map, m,
		    hatm_load_txbuf, &arg, BUS_DMA_NOWAIT);

		if (error == EFBIG) {
			/* try to defragment the packet */
			sc->istats.defrag++;
			m = m_defrag(m, M_DONTWAIT);
			if (m == NULL) {
				tpd->mbuf = NULL;
				hatm_free_txmbuf(sc);
				hatm_free_tpd(sc, tpd);
				sc->ifp->if_oerrors++;
				continue;
			}
			arg.mbuf = m;
			error = bus_dmamap_load_mbuf(sc->tx_tag, tpd->map, m,
			    hatm_load_txbuf, &arg, BUS_DMA_NOWAIT);
		}

		if (error != 0) {
			if_printf(sc->ifp, "mbuf loaded error=%d\n",
			    error);
			hatm_free_tpd(sc, tpd);
			sc->ifp->if_oerrors++;
			continue;
		}
		if (arg.error) {
			hatm_free_tpd(sc, tpd);
			sc->ifp->if_oerrors++;
			continue;
		}
		arg.vcc->opackets++;
		arg.vcc->obytes += len;
		sc->ifp->if_opackets++;
	}
	mtx_unlock(&sc->mtx);
}

void
hatm_tx_complete(struct hatm_softc *sc, struct tpd *tpd, uint32_t flags)
{
	struct hevcc *vcc = sc->vccs[tpd->cid];

	DBG(sc, TX, ("tx_complete cid=%#x flags=%#x", tpd->cid, flags));

	if (vcc == NULL)
		return;
	if ((flags & HE_REGM_TBRQ_EOS) && (vcc->vflags & HE_VCC_TX_CLOSING)) {
		vcc->vflags &= ~HE_VCC_TX_CLOSING;
		if (vcc->param.flags & ATMIO_FLAG_ASYNC) {
			hatm_tx_vcc_closed(sc, tpd->cid);
			if (!(vcc->vflags & HE_VCC_OPEN)) {
				hatm_vcc_closed(sc, tpd->cid);
				vcc = NULL;
			}
		} else
			cv_signal(&sc->vcc_cv);
	}
	hatm_free_tpd(sc, tpd);

	if (vcc == NULL)
		return;

	vcc->ntpds--;

	if ((vcc->vflags & HE_VCC_FLOW_CTRL) &&
	    vcc->ntpds <= HE_CONFIG_TPD_FLOW_ENB) {
		vcc->vflags &= ~HE_VCC_FLOW_CTRL;
		ATMEV_SEND_FLOW_CONTROL(IFP2IFATM(sc->ifp),
		    HE_VPI(tpd->cid), HE_VCI(tpd->cid), 0);
	}
}

/*
 * Convert CPS to Rate for a rate group
 */
static u_int
cps_to_rate(struct hatm_softc *sc, uint32_t cps)
{
	u_int clk = sc->he622 ? HE_622_CLOCK : HE_155_CLOCK;
	u_int period, rate;

	/* how many double ticks between two cells */
	period = (clk + 2 * cps - 1) / (2 * cps);
	rate = hatm_cps2atmf(period);
	if (hatm_atmf2cps(rate) < period)
		rate++;

	return (rate);
}

/*
 * Check whether the VCC is really closed on the hardware and available for
 * open. Check that we have enough resources. If this function returns ok,
 * a later actual open must succeed. Assume, that we are locked between this
 * function and the next one, so that nothing does change. For CBR this
 * assigns the rate group and set the rate group's parameter.
 */
int
hatm_tx_vcc_can_open(struct hatm_softc *sc, u_int cid, struct hevcc *vcc)
{
	uint32_t v, line_rate;
	u_int rc, idx, free_idx;
	struct atmio_tparam *t = &vcc->param.tparam;

	/* verify that connection is closed */
#if 0
	v = READ_TSR(sc, cid, 4);
	if(!(v & HE_REGM_TSR4_SESS_END)) {
		if_printf(sc->ifp, "cid=%#x not closed (TSR4)\n", cid);
		return (EBUSY);
	}
#endif
	v = READ_TSR(sc, cid, 0);
	if((v & HE_REGM_TSR0_CONN_STATE) != 0) {
		if_printf(sc->ifp, "cid=%#x not closed (TSR0=%#x)\n",
		    cid, v);
		return (EBUSY);
	}

	/* check traffic parameters */
	line_rate = sc->he622 ? ATM_RATE_622M : ATM_RATE_155M;
	switch (vcc->param.traffic) {

	  case ATMIO_TRAFFIC_UBR:
		if (t->pcr == 0 || t->pcr > line_rate)
			t->pcr = line_rate;
		if (t->mcr != 0 || t->icr != 0 || t->tbe != 0 || t->nrm != 0 ||
		    t->trm != 0 || t->adtf != 0 || t->rif != 0 || t->rdf != 0 ||
		    t->cdf != 0)
			return (EINVAL);
		break;

	  case ATMIO_TRAFFIC_CBR:
		/*
		 * Compute rate group index
		 */
		if (t->pcr < 10)
			t->pcr = 10;
		if (sc->cbr_bw + t->pcr > line_rate)
			return (EINVAL);
		if (t->mcr != 0 || t->icr != 0 || t->tbe != 0 || t->nrm != 0 ||
		    t->trm != 0 || t->adtf != 0 || t->rif != 0 || t->rdf != 0 ||
		    t->cdf != 0)
			return (EINVAL);

		rc = cps_to_rate(sc, t->pcr);
		free_idx = HE_REGN_CS_STPER;
		for (idx = 0; idx < HE_REGN_CS_STPER; idx++) {
			if (sc->rate_ctrl[idx].refcnt == 0) {
				if (free_idx == HE_REGN_CS_STPER)
					free_idx = idx;
			} else {
				if (sc->rate_ctrl[idx].rate == rc)
					break;
			}
		}
		if (idx == HE_REGN_CS_STPER) {
			if ((idx = free_idx) == HE_REGN_CS_STPER)
				return (EBUSY);
			sc->rate_ctrl[idx].rate = rc;
		}
		vcc->rc = idx;

		/* commit */
		sc->rate_ctrl[idx].refcnt++;
		sc->cbr_bw += t->pcr;
		break;

	  case ATMIO_TRAFFIC_ABR:
		if (t->pcr > line_rate)
			t->pcr = line_rate;
		if (t->mcr > line_rate)
			t->mcr = line_rate;
		if (t->icr > line_rate)
			t->icr = line_rate;
		if (t->tbe == 0 || t->tbe >= 1 << 24 || t->nrm > 7 ||
		    t->trm > 7 || t->adtf >= 1 << 10 || t->rif > 15 ||
		    t->rdf > 15 || t->cdf > 7)
			return (EINVAL);
		break;

	  default:
		return (EINVAL);
	}
	return (0);
}

#define NRM_CODE2VAL(CODE) (2 * (1 << (CODE)))

/*
 * Actually open the transmit VCC
 */
void
hatm_tx_vcc_open(struct hatm_softc *sc, u_int cid)
{
	struct hevcc *vcc = sc->vccs[cid];
	uint32_t tsr0, tsr4, atmf, crm;
	const struct atmio_tparam *t = &vcc->param.tparam;

	if (vcc->param.aal == ATMIO_AAL_5) {
		tsr0 = HE_REGM_TSR0_AAL_5 << HE_REGS_TSR0_AAL;
		tsr4 = HE_REGM_TSR4_AAL_5 << HE_REGS_TSR4_AAL;
	} else {
		tsr0 = HE_REGM_TSR0_AAL_0 << HE_REGS_TSR0_AAL;
		tsr4 = HE_REGM_TSR4_AAL_0 << HE_REGS_TSR4_AAL;
	}
	tsr4 |= 1;

	switch (vcc->param.traffic) {

	  case ATMIO_TRAFFIC_UBR:
		atmf = hatm_cps2atmf(t->pcr);

		tsr0 |= HE_REGM_TSR0_TRAFFIC_UBR << HE_REGS_TSR0_TRAFFIC;
		tsr0 |= HE_REGM_TSR0_USE_WMIN | HE_REGM_TSR0_UPDATE_GER;

		WRITE_TSR(sc, cid, 0, 0xf, tsr0);
		WRITE_TSR(sc, cid, 4, 0xf, tsr4);
		WRITE_TSR(sc, cid, 1, 0xf, (atmf << HE_REGS_TSR1_PCR));
		WRITE_TSR(sc, cid, 2, 0xf, (atmf << HE_REGS_TSR2_ACR));
		WRITE_TSR(sc, cid, 9, 0xf, HE_REGM_TSR9_INIT);
		WRITE_TSR(sc, cid, 3, 0xf, 0);
		WRITE_TSR(sc, cid, 5, 0xf, 0);
		WRITE_TSR(sc, cid, 6, 0xf, 0);
		WRITE_TSR(sc, cid, 7, 0xf, 0);
		WRITE_TSR(sc, cid, 8, 0xf, 0);
		WRITE_TSR(sc, cid, 10, 0xf, 0);
		WRITE_TSR(sc, cid, 11, 0xf, 0);
		WRITE_TSR(sc, cid, 12, 0xf, 0);
		WRITE_TSR(sc, cid, 13, 0xf, 0);
		WRITE_TSR(sc, cid, 14, 0xf, 0);
		break;

	  case ATMIO_TRAFFIC_CBR:
		atmf = hatm_cps2atmf(t->pcr);

		if (sc->rate_ctrl[vcc->rc].refcnt == 1)
			WRITE_MBOX4(sc, HE_REGO_CS_STPER(vcc->rc),
			    sc->rate_ctrl[vcc->rc].rate);

		tsr0 |= HE_REGM_TSR0_TRAFFIC_CBR << HE_REGS_TSR0_TRAFFIC;
		tsr0 |= vcc->rc;

		WRITE_TSR(sc, cid, 1, 0xf, (atmf << HE_REGS_TSR1_PCR));
		WRITE_TSR(sc, cid, 2, 0xf, (atmf << HE_REGS_TSR2_ACR));
		WRITE_TSR(sc, cid, 3, 0xf, 0);
		WRITE_TSR(sc, cid, 5, 0xf, 0);
		WRITE_TSR(sc, cid, 6, 0xf, 0);
		WRITE_TSR(sc, cid, 7, 0xf, 0);
		WRITE_TSR(sc, cid, 8, 0xf, 0);
		WRITE_TSR(sc, cid, 10, 0xf, 0);
		WRITE_TSR(sc, cid, 11, 0xf, 0);
		WRITE_TSR(sc, cid, 12, 0xf, 0);
		WRITE_TSR(sc, cid, 13, 0xf, 0);
		WRITE_TSR(sc, cid, 14, 0xf, 0);
		WRITE_TSR(sc, cid, 4, 0xf, tsr4);
		WRITE_TSR(sc, cid, 9, 0xf, HE_REGM_TSR9_INIT);
		WRITE_TSR(sc, cid, 0, 0xf, tsr0);

		break;

	  case ATMIO_TRAFFIC_ABR:
		if ((crm = t->tbe / NRM_CODE2VAL(t->nrm)) > 0xffff)
			crm = 0xffff;

		tsr0 |= HE_REGM_TSR0_TRAFFIC_ABR << HE_REGS_TSR0_TRAFFIC;
		tsr0 |= HE_REGM_TSR0_USE_WMIN | HE_REGM_TSR0_UPDATE_GER;

		WRITE_TSR(sc, cid, 0, 0xf, tsr0);
		WRITE_TSR(sc, cid, 4, 0xf, tsr4);

		WRITE_TSR(sc, cid, 1, 0xf,
		    ((hatm_cps2atmf(t->pcr) << HE_REGS_TSR1_PCR) |
		     (hatm_cps2atmf(t->mcr) << HE_REGS_TSR1_MCR)));
		WRITE_TSR(sc, cid, 2, 0xf,
		    (hatm_cps2atmf(t->icr) << HE_REGS_TSR2_ACR));
		WRITE_TSR(sc, cid, 3, 0xf,
		    ((NRM_CODE2VAL(t->nrm) - 1) << HE_REGS_TSR3_NRM) |
		    (crm << HE_REGS_TSR3_CRM));

		WRITE_TSR(sc, cid, 5, 0xf, 0);
		WRITE_TSR(sc, cid, 6, 0xf, 0);
		WRITE_TSR(sc, cid, 7, 0xf, 0);
		WRITE_TSR(sc, cid, 8, 0xf, 0);
		WRITE_TSR(sc, cid, 10, 0xf, 0);
		WRITE_TSR(sc, cid, 12, 0xf, 0);
		WRITE_TSR(sc, cid, 14, 0xf, 0);
		WRITE_TSR(sc, cid, 9, 0xf, HE_REGM_TSR9_INIT);

		WRITE_TSR(sc, cid, 11, 0xf,
		    (hatm_cps2atmf(t->icr) << HE_REGS_TSR11_ICR) |
		    (t->trm << HE_REGS_TSR11_TRM) |
		    (t->nrm << HE_REGS_TSR11_NRM) |
		    (t->adtf << HE_REGS_TSR11_ADTF));

		WRITE_TSR(sc, cid, 13, 0xf,
		    (t->rdf << HE_REGS_TSR13_RDF) |
		    (t->rif << HE_REGS_TSR13_RIF) |
		    (t->cdf << HE_REGS_TSR13_CDF) |
		    (crm << HE_REGS_TSR13_CRM));

		break;

	  default:
		return;
	}

	vcc->vflags |= HE_VCC_TX_OPEN;
}

/*
 * Close the TX side of a VCC. Set the CLOSING flag.
 */
void
hatm_tx_vcc_close(struct hatm_softc *sc, u_int cid)
{
	struct hevcc *vcc = sc->vccs[cid];
	struct tpd *tpd_list[1];
	u_int i, pcr = 0;

	WRITE_TSR(sc, cid, 4, 0x8, HE_REGM_TSR4_FLUSH);

	switch (vcc->param.traffic) {

	  case ATMIO_TRAFFIC_CBR:
		WRITE_TSR(sc, cid, 14, 0x8, HE_REGM_TSR14_CBR_DELETE);
		break;

	  case ATMIO_TRAFFIC_ABR:
		WRITE_TSR(sc, cid, 14, 0x4, HE_REGM_TSR14_ABR_CLOSE);
		pcr = vcc->param.tparam.pcr;
		/* FALL THROUGH */

	  case ATMIO_TRAFFIC_UBR:
		WRITE_TSR(sc, cid, 1, 0xf,
		    hatm_cps2atmf(HE_CONFIG_FLUSH_RATE) << HE_REGS_TSR1_MCR |
		    hatm_cps2atmf(pcr) << HE_REGS_TSR1_PCR);
		break;
	}

	tpd_list[0] = hatm_alloc_tpd(sc, 0);
	tpd_list[0]->tpd.addr |= HE_REGM_TPD_EOS | HE_REGM_TPD_INTR;
	tpd_list[0]->cid = cid;

	vcc->vflags |= HE_VCC_TX_CLOSING;
	vcc->vflags &= ~HE_VCC_TX_OPEN;

	i = 0;
	while (hatm_queue_tpds(sc, 1, tpd_list, cid) != 0) {
		if (++i == 1000)
			panic("TPDRQ permanently full");
		DELAY(1000);
	}
}

void
hatm_tx_vcc_closed(struct hatm_softc *sc, u_int cid)
{
	if (sc->vccs[cid]->param.traffic == ATMIO_TRAFFIC_CBR) {
		sc->cbr_bw -= sc->vccs[cid]->param.tparam.pcr;
		sc->rate_ctrl[sc->vccs[cid]->rc].refcnt--;
	}
}
