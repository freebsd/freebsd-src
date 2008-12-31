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
__FBSDID("$FreeBSD: src/sys/dev/patm/if_patm_rx.c,v 1.8.6.1 2008/11/25 02:59:29 kensmith Exp $");

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

static void *patm_rcv_handle(struct patm_softc *sc, u_int handle);
static void patm_rcv_free(struct patm_softc *, void *, u_int handle);
static struct mbuf *patm_rcv_mbuf(struct patm_softc *, void *, u_int, int);

static __inline void
rct_write(struct patm_softc *sc, u_int cid, u_int w, u_int val)
{
	patm_sram_write(sc, sc->mmap->rct + cid * IDT_RCT_ENTRY_SIZE + w, val);
}
static __inline u_int
rct_read(struct patm_softc *sc, u_int cid, u_int w)
{
	return (patm_sram_read(sc, sc->mmap->rct +
	    cid * IDT_RCT_ENTRY_SIZE + w));
}

/* check if we can open this one */
int
patm_rx_vcc_can_open(struct patm_softc *sc, struct patm_vcc *vcc)
{
	return (0);
}

/*
 * open the VCC
 */
void
patm_rx_vcc_open(struct patm_softc *sc, struct patm_vcc *vcc)
{
	uint32_t w1 = IDT_RCT_OPEN;

	patm_debug(sc, VCC, "%u.%u RX opening", vcc->vcc.vpi, vcc->vcc.vci);

	switch (vcc->vcc.aal) {
	  case ATMIO_AAL_0:
		w1 |= IDT_RCT_AAL0 | IDT_RCT_FBP2 | IDT_RCT_RCI;
		break;
	  case ATMIO_AAL_34:
		w1 |= IDT_RCT_AAL34;
		break;
	  case ATMIO_AAL_5:
		w1 |= IDT_RCT_AAL5;
		break;
	  case ATMIO_AAL_RAW:
		w1 |= IDT_RCT_AALRAW | IDT_RCT_RCI;
		break;
	}

	if (vcc->cid != 0)
		patm_sram_write4(sc, sc->mmap->rct + vcc->cid *
		    IDT_RCT_ENTRY_SIZE, w1, 0, 0, 0xffffffff);
	else {
		/* switch the interface into promiscuous mode */
		patm_nor_write(sc, IDT_NOR_CFG, patm_nor_read(sc, IDT_NOR_CFG) |
		    IDT_CFG_ICAPT | IDT_CFG_VPECA);
	}

	vcc->vflags |= PATM_VCC_RX_OPEN;
}

/* close the given vcc for transmission */
void
patm_rx_vcc_close(struct patm_softc *sc, struct patm_vcc *vcc)
{
	u_int w1;

	patm_debug(sc, VCC, "%u.%u RX closing", vcc->vcc.vpi, vcc->vcc.vci);

	if (vcc->cid == 0) {
		/* switch off promiscuous mode */
		patm_nor_write(sc, IDT_NOR_CFG, patm_nor_read(sc, IDT_NOR_CFG) &
		    ~(IDT_CFG_ICAPT | IDT_CFG_VPECA));
		vcc->vflags &= ~PATM_VCC_RX_OPEN;
		return;
	}

	/* close the connection but keep state */
	w1 = rct_read(sc, vcc->cid, 0);
	w1 &= ~IDT_RCT_OPEN;
	rct_write(sc, vcc->cid, 0, w1);

	/* minimum idle count */
	w1 = (w1 & ~IDT_RCT_IACT_CNT_MASK) | (1 << IDT_RCT_IACT_CNT_SHIFT);
	rct_write(sc, vcc->cid, 0, w1);

	/* initialize scan */
	patm_nor_write(sc, IDT_NOR_IRCP, vcc->cid);

	vcc->vflags &= ~PATM_VCC_RX_OPEN;
	vcc->vflags |= PATM_VCC_RX_CLOSING;

	/*
	 * check the RSQ
	 * This is a hack. The problem is, that although an entry is written
	 * to the RSQ, no interrupt is generated. Also we must wait 1 cell
	 * time for the SAR to process the scan of our connection.
	 */
	DELAY(1);
	patm_intr_rsq(sc);
}

/* transmission side finally closed */
void
patm_rx_vcc_closed(struct patm_softc *sc, struct patm_vcc *vcc)
{
	patm_debug(sc, VCC, "%u.%u RX finally closed",
	    vcc->vcc.vpi, vcc->vcc.vci);
}

/*
 * Handle the given receive status queue entry
 */
void
patm_rx(struct patm_softc *sc, struct idt_rsqe *rsqe)
{
	struct mbuf *m;
	void *buf;
	u_int stat, cid, w, cells, len, h;
	struct patm_vcc *vcc;
	struct atm_pseudohdr aph;
	u_char *trail;

	cid = le32toh(rsqe->cid);
	stat = le32toh(rsqe->stat);
	h = le32toh(rsqe->handle);

	cid = PATM_CID(sc, IDT_RSQE_VPI(cid), IDT_RSQE_VCI(cid));
	vcc = sc->vccs[cid];

	if (IDT_RSQE_TYPE(stat) == IDT_RSQE_IDLE) {
		/* connection has gone idle */
		if (stat & IDT_RSQE_BUF)
			patm_rcv_free(sc, patm_rcv_handle(sc, h), h);

		w = rct_read(sc, cid, 0);
		if (w != 0 && !(w & IDT_RCT_OPEN))
			rct_write(sc, cid, 0, 0);
		if (vcc != NULL && (vcc->vflags & PATM_VCC_RX_CLOSING)) {
			patm_debug(sc, VCC, "%u.%u RX closed", vcc->vcc.vpi,
			    vcc->vcc.vci);
			vcc->vflags &= ~PATM_VCC_RX_CLOSING;
			if (vcc->vcc.flags & ATMIO_FLAG_ASYNC) {
				patm_rx_vcc_closed(sc, vcc);
				if (!(vcc->vflags & PATM_VCC_OPEN))
					patm_vcc_closed(sc, vcc);
			} else
				cv_signal(&sc->vcc_cv);
		}
		return;
	}

	buf = patm_rcv_handle(sc, h);

	if (vcc == NULL || (vcc->vflags & PATM_VCC_RX_OPEN) == 0) {
		patm_rcv_free(sc, buf, h);
		return;
	}

	cells = IDT_RSQE_CNT(stat);
	KASSERT(cells > 0, ("zero cell count"));

	if (vcc->vcc.aal == ATMIO_AAL_0) {
		/* deliver this packet as it is */
		if ((m = patm_rcv_mbuf(sc, buf, h, 1)) == NULL)
			return;

		m->m_len = cells * 48;
		m->m_pkthdr.len = m->m_len;
		m->m_pkthdr.rcvif = sc->ifp;

	} else if (vcc->vcc.aal == ATMIO_AAL_34) {
		/* XXX AAL3/4 */
		patm_rcv_free(sc, buf, h);
		return;

	} else if (vcc->vcc.aal == ATMIO_AAL_5) {
		if (stat & IDT_RSQE_CRC) {
			sc->ifp->if_ierrors++;
			if (vcc->chain != NULL) {
				m_freem(vcc->chain);
				vcc->chain = vcc->last = NULL;
			}
			return;
		}

		/* append to current chain */
		if (vcc->chain == NULL) {
			if ((m = patm_rcv_mbuf(sc, buf, h, 1)) == NULL)
				return;
			m->m_len = cells * 48;
			m->m_pkthdr.len = m->m_len;
			m->m_pkthdr.rcvif = sc->ifp;
			vcc->chain = vcc->last = m;
		} else {
			if ((m = patm_rcv_mbuf(sc, buf, h, 0)) == NULL)
				return;
			m->m_len = cells * 48;
			vcc->last->m_next = m;
			vcc->last = m;
			vcc->chain->m_pkthdr.len += m->m_len;
		}

		if (!(stat & IDT_RSQE_EPDU))
			return;

		trail = mtod(m, u_char *) + m->m_len - 6;
		len = (trail[0] << 8) + trail[1];

		if ((u_int)vcc->chain->m_pkthdr.len < len + 8) {
			patm_printf(sc, "%s: bad aal5 lengths %u %u\n",
			    __func__, (u_int)m->m_pkthdr.len, len);
			m_freem(vcc->chain);
			vcc->chain = vcc->last = NULL;
			return;
		}
		m->m_len -= vcc->chain->m_pkthdr.len - len;
		KASSERT(m->m_len >= 0, ("bad last mbuf"));

		m = vcc->chain;
		vcc->chain = vcc->last = NULL;
		m->m_pkthdr.len = len;
	} else
		panic("bad aal");

#if 0
	{
		u_int i;

		for (i = 0; i < m->m_len; i++) {
			printf("%02x ", mtod(m, u_char *)[i]);
		}
		printf("\n");
	}
#endif

	sc->ifp->if_ipackets++;
	/* this is in if_atmsubr.c */
	/* sc->ifp->if_ibytes += m->m_pkthdr.len; */

	vcc->ibytes += m->m_pkthdr.len;
	vcc->ipackets++;

	ATM_PH_FLAGS(&aph) = vcc->vcc.flags & 0xff;
	ATM_PH_VPI(&aph) = IDT_RSQE_VPI(cid);
	ATM_PH_SETVCI(&aph, IDT_RSQE_VCI(cid));

#ifdef ENABLE_BPF
	if (!(vcc->vcc.flags & ATMIO_FLAG_NG) &&
	    (vcc->vcc.aal == ATMIO_AAL_5) &&
	    (vcc->vcc.flags & ATM_PH_LLCSNAP))
		BPF_MTAP(sc->ifp, m);
#endif

	atm_input(sc->ifp, &aph, m, vcc->rxhand);
}

/*
 * Get the buffer for a receive handle. This is either an mbuf for
 * a large handle or a pool buffer for the others.
 */
static void *
patm_rcv_handle(struct patm_softc *sc, u_int handle)
{
	void *buf;
	u_int c;

	if ((handle & ~MBUF_HMASK) == LMBUF_HANDLE) {
		struct lmbuf *b;

		c = handle & MBUF_HMASK;
		b = &sc->lbufs[c];

		buf = b->m;
		b->m = NULL;

		bus_dmamap_sync(sc->lbuf_tag, b->map, BUS_DMASYNC_POSTREAD);
		patm_lbuf_free(sc, b);

	} else if ((handle & ~MBUF_HMASK) == MBUF_VHANDLE) {
		mbp_sync(sc->vbuf_pool, handle,
		    0, VMBUF_SIZE, BUS_DMASYNC_POSTREAD);
		buf = mbp_get(sc->vbuf_pool, handle);

	} else {
		mbp_sync(sc->sbuf_pool, handle,
		    0, SMBUF_SIZE, BUS_DMASYNC_POSTREAD);
		buf = mbp_get(sc->sbuf_pool, handle);
	}

	return (buf);
}

/*
 * Free a buffer.
 */
static void
patm_rcv_free(struct patm_softc *sc, void *p, u_int handle)
{
	if ((handle & ~MBUF_HMASK) == LMBUF_HANDLE)
		m_free((struct mbuf *)p);

	else if ((handle & ~MBUF_HMASK) == MBUF_VHANDLE)
		mbp_free(sc->vbuf_pool, p);

	else
		mbp_free(sc->sbuf_pool, p);
}

/*
 * Make an mbuf around the buffer
 */
static struct mbuf *
patm_rcv_mbuf(struct patm_softc *sc, void *buf, u_int h, int hdr)
{
	struct mbuf *m;

	if ((h & ~MBUF_HMASK) == MBUF_LHANDLE)
		return ((struct mbuf *)buf);

	if (hdr)
		MGETHDR(m, M_DONTWAIT, MT_DATA);
	else
		MGET(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		patm_rcv_free(sc, buf, h);
		return (NULL);
	}

	if ((h & ~MBUF_HMASK) == MBUF_VHANDLE) {
		MEXTADD(m, (caddr_t)buf, VMBUF_SIZE, mbp_ext_free,
		    sc->vbuf_pool, M_PKTHDR, EXT_NET_DRV);
		m->m_data += VMBUF_OFFSET;
	} else {
		MEXTADD(m, (caddr_t)buf, SMBUF_SIZE, mbp_ext_free,
		    sc->sbuf_pool, M_PKTHDR, EXT_NET_DRV);
		m->m_data += SMBUF_OFFSET;
	}

	if (!(m->m_flags & M_EXT)) {
		patm_rcv_free(sc, buf, h);
		m_free(m);
		return (NULL);
	}
	return (m);
}

/*
 * Process the raw cell at the given address.
 */
void
patm_rx_raw(struct patm_softc *sc, u_char *cell)
{
	u_int vpi, vci, cid;
	struct patm_vcc *vcc;
	struct mbuf *m;
	u_char *dst;
	struct timespec ts;
	struct atm_pseudohdr aph;
	uint64_t cts;

	sc->stats.raw_cells++;

	/*
	 * For some non-appearant reason the cell header
	 * is in the wrong endian.
	 */
	*(uint32_t *)cell = bswap32(*(uint32_t *)cell);

	vpi = ((cell[0] & 0xf) << 4) | ((cell[1] & 0xf0) >> 4);
	vci = ((cell[1] & 0xf) << 12) | (cell[2] << 4) | ((cell[3] & 0xf0) >> 4);
	cid = PATM_CID(sc, vpi, vci);

	vcc = sc->vccs[cid];
	if (vcc == NULL || !(vcc->vflags & PATM_VCC_RX_OPEN) ||
	    vcc->vcc.aal != ATMIO_AAL_RAW) {
		vcc = sc->vccs[0];
		if (vcc == NULL || !(vcc->vflags & PATM_VCC_RX_OPEN)) {
			sc->stats.raw_no_vcc++;
			return;
		}
	}

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		sc->stats.raw_no_buf++;
		return;
	}
	m->m_pkthdr.rcvif = sc->ifp;

	switch (vcc->vflags & PATM_RAW_FORMAT) {

	  default:
	  case PATM_RAW_CELL:
		m->m_len = m->m_pkthdr.len = 53;
		MH_ALIGN(m, 53);
		dst = mtod(m, u_char *);
		*dst++ = *cell++;
		*dst++ = *cell++;
		*dst++ = *cell++;
		*dst++ = *cell++;
		*dst++ = 0;		/* HEC */
		bcopy(cell + 12, dst, 48);
		break;

	  case PATM_RAW_NOHEC:
		m->m_len = m->m_pkthdr.len = 52;
		MH_ALIGN(m, 52);
		dst = mtod(m, u_char *);
		*dst++ = *cell++;
		*dst++ = *cell++;
		*dst++ = *cell++;
		*dst++ = *cell++;
		bcopy(cell + 12, dst, 48);
		break;

	  case PATM_RAW_CS:
		m->m_len = m->m_pkthdr.len = 64;
		MH_ALIGN(m, 64);
		dst = mtod(m, u_char *);
		*dst++ = *cell++;
		*dst++ = *cell++;
		*dst++ = *cell++;
		*dst++ = *cell++;
		*dst++ = 0;		/* HEC */
		*dst++ = 0;		/* flags */
		*dst++ = 0;		/* reserved */
		*dst++ = 0;		/* reserved */
		nanotime(&ts);
		cts = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
		bcopy(dst, &cts, 8);
		bcopy(cell + 12, dst + 8, 48);
		break;
	}

	sc->ifp->if_ipackets++;
	/* this is in if_atmsubr.c */
	/* sc->ifp->if_ibytes += m->m_pkthdr.len; */

	vcc->ibytes += m->m_pkthdr.len;
	vcc->ipackets++;

	ATM_PH_FLAGS(&aph) = vcc->vcc.flags & 0xff;
	ATM_PH_VPI(&aph) = vcc->vcc.vpi;
	ATM_PH_SETVCI(&aph, vcc->vcc.vci);

	atm_input(sc->ifp, &aph, m, vcc->rxhand);
}
