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
 * ForeHE driver.
 *
 * Receive.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_natm.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
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

void
hatm_rx(struct hatm_softc *sc, u_int cid, u_int flags, struct mbuf *m0,
    u_int len)
{
	struct hevcc *vcc;
	struct atm_pseudohdr aph;
	struct mbuf *m, *m1;
	u_int vpi, vci;
	u_char *ptr;

	DBG(sc, RX, ("cid=%#x flags=%#x len=%u mbuf=%p", cid, flags, len, m0));

	vcc = sc->vccs[cid];
	if (vcc == NULL)
		goto drop;

	if (flags & HE_REGM_RBRQ_CON_CLOSED) {
		if (vcc->vflags & HE_VCC_RX_CLOSING) {
			vcc->vflags &= ~HE_VCC_RX_CLOSING;
			if (vcc->param.flags & ATMIO_FLAG_ASYNC) {
				if (!(vcc->vflags & HE_VCC_OPEN))
					hatm_vcc_closed(sc, cid);
			} else
				cv_signal(&sc->vcc_cv);
		}
		goto drop;
	}

	if (!(vcc->vflags & HE_VCC_RX_OPEN))
		goto drop;

	if (flags & HE_REGM_RBRQ_HBUF_ERROR) {
		sc->istats.hbuf_error++;
		if (vcc->chain != NULL) {
			m_freem(vcc->chain);
			vcc->chain = vcc->last = NULL;
		}
		goto drop;
	}
	if (m0 == NULL) {
		sc->istats.no_rcv_mbuf++;
		return;
	}

	if ((m0->m_len = len) == 0) {
		sc->istats.empty_hbuf++;
		m_free(m0);

	} else if (vcc->chain == NULL) {
		sc->istats.rx_seg++;
		vcc->chain = vcc->last = m0;
		vcc->last->m_next = NULL;
		vcc->chain->m_pkthdr.len = m0->m_len;
		vcc->chain->m_pkthdr.rcvif = &sc->ifatm.ifnet;

	} else {
		sc->istats.rx_seg++;
		vcc->last->m_next = m0;
		vcc->last = m0;
		vcc->last->m_next = NULL;
		vcc->chain->m_pkthdr.len += m0->m_len;
	}

	if (!(flags & HE_REGM_RBRQ_END_PDU))
		return;

	if (flags & HE_REGM_RBRQ_CRC_ERROR) {
		if (vcc->chain)
			m_freem(vcc->chain);
		vcc->chain = vcc->last = NULL;
		sc->istats.crc_error++;
		sc->ifatm.ifnet.if_ierrors++;
		return;
	}
	if (flags & HE_REGM_RBRQ_LEN_ERROR) {
		if (vcc->chain)
			m_freem(vcc->chain);
		vcc->chain = vcc->last = NULL;
		sc->istats.len_error++;
		sc->ifatm.ifnet.if_ierrors++;
		return;
	}

#ifdef HATM_DEBUG
	if (sc->debug & DBG_DUMP) {
		struct mbuf *tmp;

		for (tmp = vcc->chain; tmp != NULL; tmp = tmp->m_next) {
			printf("mbuf %p: len=%u\n", tmp, tmp->m_len);
			for (ptr = mtod(tmp, u_char *);
			    ptr < mtod(tmp, u_char *) + tmp->m_len; ptr++)
				printf("%02x ", *ptr);
			printf("\n");
		}
	}
#endif

	if (vcc->param.aal == ATMIO_AAL_5) {
		/*
		 * Need to remove padding and the trailer. The trailer
		 * may be split accross buffers according to 2.10.1.2
		 * Assume that mbufs sizes are even (buffer sizes and cell
		 * payload sizes are) and that there are no empty mbufs.
		 */
		m = vcc->last;
		if (m->m_len == 2) {
			/* Ah, oh, only part of CRC */
			if (m == vcc->chain) {
				/* ups */
				sc->istats.short_aal5++;
				m_freem(vcc->chain);
				vcc->chain = vcc->last = NULL;
				return;
			}
			for (m1 = vcc->chain; m1->m_next != m; m1 = m1->m_next)
				;
			ptr = (u_char *)m1->m_data + m1->m_len - 4;

		} else if (m->m_len == 4) {
			/* Ah, oh, only CRC */
			if (m == vcc->chain) {
				/* ups */
				sc->istats.short_aal5++;
				m_freem(vcc->chain);
				vcc->chain = vcc->last = NULL;
				return;
			}
			for (m1 = vcc->chain; m1->m_next != m; m1 = m1->m_next)
				;
			ptr = (u_char *)m1->m_data + m1->m_len - 2;

		} else if (m->m_len >= 6) {
			ptr = (u_char *)m->m_data + m->m_len - 6;
		} else
			panic("hatm_rx: bad mbuf len %d", m->m_len);

		len = (ptr[0] << 8) + ptr[1];
		if (len > (u_int)vcc->chain->m_pkthdr.len - 4) {
			sc->istats.badlen_aal5++;
			m_freem(vcc->chain);
			vcc->chain = vcc->last = NULL;
			return;
		}
		m_adj(vcc->chain, -(vcc->chain->m_pkthdr.len - len));
	}
	m = vcc->chain;
	vcc->chain = vcc->last = NULL;

#ifdef ENABLE_BPF
	if (!(vcc->param.flags & ATMIO_FLAG_NG) &&
	    (vcc->param.aal == ATMIO_AAL_5) &&
	    (vcc->param.flags & ATM_PH_LLCSNAP))
		BPF_MTAP(&sc->ifatm.ifnet, m);
#endif

	vpi = HE_VPI(cid);
	vci = HE_VCI(cid);

	ATM_PH_FLAGS(&aph) = vcc->param.flags & 0xff;
	ATM_PH_VPI(&aph) = vpi;
	ATM_PH_SETVCI(&aph, vci);

	sc->ifatm.ifnet.if_ipackets++;
	/* this is in if_atmsubr.c */
	/* sc->ifatm.ifnet.if_ibytes += len; */

	vcc->ibytes += len;
	vcc->ipackets++;

#if 0
	{
		struct mbuf *tmp;

		for (tmp = m; tmp != NULL; tmp = tmp->m_next) {
			printf("mbuf %p: len=%u\n", tmp, tmp->m_len);
			for (ptr = mtod(tmp, u_char *);
			    ptr < mtod(tmp, u_char *) + tmp->m_len; ptr++)
				printf("%02x ", *ptr);
			printf("\n");
		}
	}
#endif

	atm_input(&sc->ifatm.ifnet, &aph, m, vcc->rxhand);

	return;

  drop:
	if (m0 != NULL)
		m_free(m0);
}

void
hatm_rx_vcc_open(struct hatm_softc *sc, u_int cid)
{
	struct hevcc *vcc = sc->vccs[cid];
	uint32_t rsr0, rsr1, rsr4;

	rsr0 = rsr1 = rsr4 = 0;

	if (vcc->param.traffic == ATMIO_TRAFFIC_ABR) {
		rsr1 |= HE_REGM_RSR1_AQI;
		rsr4 |= HE_REGM_RSR4_AQI;
	}

	if (vcc->param.aal == ATMIO_AAL_5) {
		rsr0 |= HE_REGM_RSR0_STARTPDU | HE_REGM_RSR0_AAL_5;
	} else if (vcc->param.aal == ATMIO_AAL_0) {
		rsr0 |= HE_REGM_RSR0_AAL_0;
	} else {
		if (sc->rbp_s1.size != 0) {
			rsr1 |= (1 << HE_REGS_RSR1_GROUP);
			rsr4 |= (1 << HE_REGS_RSR4_GROUP);
		}
		rsr0 |= HE_REGM_RSR0_AAL_RAW | HE_REGM_RSR0_PTI7 |
		    HE_REGM_RSR0_RM | HE_REGM_RSR0_F5OAM;
	}
	rsr0 |= HE_REGM_RSR0_OPEN;

	WRITE_RSR(sc, cid, 0, 0xf, rsr0);
	WRITE_RSR(sc, cid, 1, 0xf, rsr1);
	WRITE_RSR(sc, cid, 4, 0xf, rsr4);

	vcc->vflags |= HE_VCC_RX_OPEN;
}

/*
 * Close the RX side of a VCC.
 */
void
hatm_rx_vcc_close(struct hatm_softc *sc, u_int cid)
{
	struct hevcc *vcc = sc->vccs[cid];
	uint32_t v;

	vcc->vflags |= HE_VCC_RX_CLOSING;
	WRITE_RSR(sc, cid, 0, 0xf, 0);

	v = READ4(sc, HE_REGO_RCCSTAT);
	while ((sc->ifatm.ifnet.if_flags & IFF_RUNNING) &&
	       (READ4(sc, HE_REGO_RCCSTAT) & HE_REGM_RCCSTAT_PROG))
		cv_timedwait(&sc->cv_rcclose, &sc->mtx, 1);

	if (!(sc->ifatm.ifnet.if_flags & IFF_RUNNING))
		return;

	WRITE_MBOX4(sc, HE_REGO_RCON_CLOSE, cid);

	vcc->vflags |= HE_VCC_RX_CLOSING;
	vcc->vflags &= ~HE_VCC_RX_OPEN;
}
