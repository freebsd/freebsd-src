/**************************************************************************

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/pciio.h>
#include <sys/conf.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus_dma.h>
#include <sys/kthread.h>
#include <sys/rman.h>
#include <sys/ioccom.h>
#include <sys/mbuf.h>
#include <sys/linker.h>
#include <sys/firmware.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/unistd.h>
#include <sys/syslog.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/sctp_crc32.h>
#include <netinet/sctp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>


#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <cxgb_include.h>
#include <sys/mvec.h>

extern int txq_fills;
int multiq_tx_enable = 1;
int coalesce_tx_enable = 1;
int wakeup_tx_thread = 0;

extern struct sysctl_oid_list sysctl__hw_cxgb_children;
static int sleep_ticks = 1;
TUNABLE_INT("hw.cxgb.sleep_ticks", &sleep_ticks);
SYSCTL_UINT(_hw_cxgb, OID_AUTO, sleep_ticks, CTLFLAG_RDTUN, &sleep_ticks, 0,
    "ticks to sleep between checking pcpu queues");

int cxgb_txq_buf_ring_size = TX_ETH_Q_SIZE;
TUNABLE_INT("hw.cxgb.txq_mr_size", &cxgb_txq_buf_ring_size);
SYSCTL_UINT(_hw_cxgb, OID_AUTO, txq_mr_size, CTLFLAG_RDTUN, &cxgb_txq_buf_ring_size, 0,
    "size of per-queue mbuf ring");


static void cxgb_pcpu_start_proc(void *arg);
static int cxgb_tx(struct sge_qset *qs, uint32_t txmax);

#ifdef IFNET_MULTIQUEUE
static int cxgb_pcpu_cookie_to_qidx(struct port_info *pi, uint32_t cookie);
#endif

static inline int
cxgb_pcpu_enqueue_packet_(struct sge_qset *qs, struct mbuf *m)
{
	struct sge_txq *txq;
	int err = 0;

	KASSERT(m != NULL, ("null mbuf"));
	KASSERT(m->m_type == MT_DATA, ("bad mbuf type %d", m->m_type));
	if (qs->qs_flags & QS_EXITING) {
		m_freem(m);
		return (ENXIO);
	}
	txq = &qs->txq[TXQ_ETH];
	err = buf_ring_enqueue(txq->txq_mr, m);
	if (err) {
		txq->txq_drops++;
		m_freem(m);
	}
	if (wakeup_tx_thread && !err &&
	    ((txq->flags & TXQ_TRANSMITTING) == 0))
		wakeup(qs);
	
	return (err);
}

int
cxgb_pcpu_enqueue_packet(struct ifnet *ifp, struct mbuf *m)
{
	struct port_info *pi = ifp->if_softc;
	struct sge_qset *qs;
	int err = 0, qidx;
#ifdef IFNET_MULTIQUEUE
	int32_t calc_cookie;

	calc_cookie = m->m_pkthdr.flowid;
	qidx = cxgb_pcpu_cookie_to_qidx(pi, calc_cookie);
#else
	qidx = 0;
#endif	    
	qs = &pi->adapter->sge.qs[qidx];
	if (ALTQ_IS_ENABLED(&ifp->if_snd)) {
		IFQ_ENQUEUE(qs->txq[0].txq_ifq, m, err);
	} else {
		err = cxgb_pcpu_enqueue_packet_(qs, m);
	}
	return (err);
}

static int
cxgb_dequeue_packet(struct sge_txq *txq, struct mbuf **m_vec)
{
	struct mbuf *m, *m0;
	struct sge_qset *qs;
	int count, size, coalesced;
	struct adapter *sc;

#ifndef IFNET_MULTIQUEUE
	struct port_info *pi = txq->port;

	mtx_assert(&txq->lock, MA_OWNED);
	if (txq->immpkt != NULL)
		panic("immediate packet set");

	IFQ_DRV_DEQUEUE(&pi->ifp->if_snd, m);
	if (m == NULL)
		return (0);
	
	m_vec[0] = m;
	return (1);
#endif
	if (ALTQ_IS_ENABLED(txq->txq_ifq)) {
		IFQ_DRV_DEQUEUE(txq->txq_ifq, m);
		if (m == NULL)
			return (0);
	
		m_vec[0] = m;
		return (1);		
	}
	
	mtx_assert(&txq->lock, MA_OWNED);
	coalesced = count = size = 0;
	qs = txq_to_qset(txq, TXQ_ETH);
	if (qs->qs_flags & QS_EXITING)
		return (0);

	if (txq->immpkt != NULL) {
		m_vec[0] = txq->immpkt;
		txq->immpkt = NULL;
		return (1);
	}
	sc = qs->port->adapter;

	m = buf_ring_dequeue_sc(txq->txq_mr);
	if (m == NULL) 
		return (0);

	count = 1;

	m_vec[0] = m;
	if (m->m_pkthdr.tso_segsz > 0 ||
	    m->m_pkthdr.len > TX_WR_SIZE_MAX ||
	    m->m_next != NULL ||
	    (coalesce_tx_enable == 0)) {
		return (count);
	}

	size = m->m_pkthdr.len;
	for (m = buf_ring_peek(txq->txq_mr); m != NULL;
	     m = buf_ring_peek(txq->txq_mr)) {

		if (m->m_pkthdr.tso_segsz > 0
		    || size + m->m_pkthdr.len > TX_WR_SIZE_MAX
		    || m->m_next != NULL)
			break;

		m0 = buf_ring_dequeue_sc(txq->txq_mr);
#ifdef DEBUG_BUFRING
		if (m0 != m)
			panic("peek and dequeue don't match");
#endif		
		size += m->m_pkthdr.len;
		m_vec[count++] = m;

		if (count == TX_WR_COUNT_MAX)
			break;

		coalesced++;
	}
	txq->txq_coalesced += coalesced;
	
	return (count);
}

static void
cxgb_pcpu_free(struct sge_qset *qs)
{
	struct mbuf *m;
	struct sge_txq *txq = &qs->txq[TXQ_ETH];

	mtx_lock(&txq->lock);
	while ((m = mbufq_dequeue(&txq->sendq)) != NULL) 
		m_freem(m);
	while ((m = buf_ring_dequeue_sc(txq->txq_mr)) != NULL) 
		m_freem(m);

	t3_free_tx_desc_all(txq);
	mtx_unlock(&txq->lock);
}

static int
cxgb_pcpu_reclaim_tx(struct sge_txq *txq)
{
	int reclaimable;
	struct sge_qset *qs = txq_to_qset(txq, TXQ_ETH);

	mtx_assert(&txq->lock, MA_OWNED);
	
	reclaimable = desc_reclaimable(txq);
	if (reclaimable == 0)
		return (0);
	
	t3_free_tx_desc(txq, reclaimable);
		
	txq->cleaned += reclaimable;
	txq->in_use -= reclaimable;
	if (isset(&qs->txq_stopped, TXQ_ETH)) {
		qs->port->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;	
		clrbit(&qs->txq_stopped, TXQ_ETH);
	}
	
	return (reclaimable);
}

static int
cxgb_pcpu_start_(struct sge_qset *qs, struct mbuf *immpkt, int tx_flush)
{
	int i, err, initerr, flush, reclaimed, stopped;
	struct port_info *pi; 
	struct sge_txq *txq;
	adapter_t *sc;
	uint32_t max_desc;
	
	pi = qs->port;
	initerr = err = i = reclaimed = 0;
	sc = pi->adapter;
	txq = &qs->txq[TXQ_ETH];
	
	mtx_assert(&txq->lock, MA_OWNED);
	
 retry:	
	if (!pi->link_config.link_ok)
		initerr = ENXIO;
	else if (qs->qs_flags & QS_EXITING)
		initerr = ENXIO;
	else if ((pi->ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		initerr = ENXIO;
	else if ((pi->ifp->if_flags & IFF_UP) == 0)
		initerr = ENXIO;
	else if (immpkt) {

		if (!buf_ring_empty(txq->txq_mr)
		    || ALTQ_IS_ENABLED(&pi->ifp->if_snd)) 
			initerr = cxgb_pcpu_enqueue_packet_(qs, immpkt);
		else
			txq->immpkt = immpkt;

		immpkt = NULL;
	}
	if (initerr) {
		if (immpkt)
			m_freem(immpkt);
		if (initerr == ENOBUFS && !tx_flush)
			wakeup(qs);
		return (initerr);
	}

	if ((tx_flush && (desc_reclaimable(txq) > 0)) ||
	    (desc_reclaimable(txq) > (TX_ETH_Q_SIZE>>3))) {
		cxgb_pcpu_reclaim_tx(txq);
	}

	stopped = isset(&qs->txq_stopped, TXQ_ETH);
	flush = ((
#ifdef IFNET_MULTIQUEUE
		 !buf_ring_empty(txq->txq_mr)
#else			     
		 !IFQ_DRV_IS_EMPTY(&pi->ifp->if_snd)
#endif
		 && !stopped) || txq->immpkt); 
	max_desc = tx_flush ? TX_ETH_Q_SIZE : TX_START_MAX_DESC;
	
	err = flush ? cxgb_tx(qs, max_desc) : 0;

	if ((tx_flush && flush && err == 0) &&
	    (!buf_ring_empty(txq->txq_mr)  ||
		!IFQ_DRV_IS_EMPTY(&pi->ifp->if_snd))) {
		struct thread *td = curthread;

		if (++i > 1) {
			thread_lock(td);
			sched_prio(td, PRI_MIN_TIMESHARE);
			thread_unlock(td);
		}
		if (i > 200) {
			device_printf(qs->port->adapter->dev,
				    "exceeded max enqueue tries\n");
			return (EBUSY);
		}
		goto retry;
	}
	return (err);
}

int
cxgb_pcpu_transmit(struct ifnet *ifp, struct mbuf *immpkt)
{
	uint32_t cookie;
	int err, qidx, locked, resid;
	struct port_info *pi;
	struct sge_qset *qs;
	struct sge_txq *txq = NULL /* gcc is dumb */;
	struct adapter *sc;
	
	pi = ifp->if_softc;
	sc = pi->adapter;
	qs = NULL;
	qidx = resid = err = cookie = locked = 0;

#ifdef IFNET_MULTIQUEUE	
	if (immpkt && (immpkt->m_pkthdr.flowid != 0)) {
		cookie = immpkt->m_pkthdr.flowid;
		qidx = cxgb_pcpu_cookie_to_qidx(pi, cookie);
		qs = &pi->adapter->sge.qs[qidx];
	} else
#endif		
		qs = &pi->adapter->sge.qs[pi->first_qset];
	
	txq = &qs->txq[TXQ_ETH];
	if (((sc->tunq_coalesce == 0) ||
		(buf_ring_count(txq->txq_mr) >= TX_WR_COUNT_MAX) ||
		(coalesce_tx_enable == 0)) && mtx_trylock(&txq->lock)) {
		txq->flags |= TXQ_TRANSMITTING;
		err = cxgb_pcpu_start_(qs, immpkt, FALSE);
		txq->flags &= ~TXQ_TRANSMITTING;
		mtx_unlock(&txq->lock);
	} else if (immpkt)
		return (cxgb_pcpu_enqueue_packet_(qs, immpkt));
	return ((err == EBUSY) ? 0 : err);
}

void
cxgb_start(struct ifnet *ifp)
{
	struct port_info *p = ifp->if_softc;
		
	if (!p->link_config.link_ok)
		return;

	if (IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		return;

	cxgb_pcpu_transmit(ifp, NULL);
}

static void
cxgb_pcpu_start_proc(void *arg)
{
	struct sge_qset *qs = arg;
	struct thread *td;
	struct sge_txq *txq = &qs->txq[TXQ_ETH];
	int idleticks, err = 0;

	td = curthread;
	sleep_ticks = max(hz/1000, 1);
	qs->qs_flags |= QS_RUNNING;
	thread_lock(td);
	sched_bind(td, qs->qs_cpuid);
	thread_unlock(td);

	DELAY(qs->qs_cpuid*100000);
	if (bootverbose)
		printf("bound to %d running on %d\n", qs->qs_cpuid, curcpu);
	
	for (;;) {
		if (qs->qs_flags & QS_EXITING)
			break;

		if ((qs->port->ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
			idleticks = hz;
			if (!buf_ring_empty(txq->txq_mr) ||
			    !mbufq_empty(&txq->sendq))
				cxgb_pcpu_free(qs);
			goto done;
		} else
			idleticks = sleep_ticks;
		if (mtx_trylock(&txq->lock)) {
			txq->flags |= TXQ_TRANSMITTING;
			err = cxgb_pcpu_start_(qs, NULL, TRUE);
			txq->flags &= ~TXQ_TRANSMITTING;
			mtx_unlock(&txq->lock);
		} else
			err = EINPROGRESS;
#ifdef notyet
		if (mtx_trylock(&qs->rspq.lock)) {
			process_responses(sc, qs, -1);

			refill_fl_service(sc, &qs->fl[0]);
			refill_fl_service(sc, &qs->fl[1]);
			t3_write_reg(sc, A_SG_GTS, V_RSPQ(qs->rspq.cntxt_id) |
			    V_NEWTIMER(qs->rspq.next_holdoff) | V_NEWINDEX(qs->rspq.cidx));

			mtx_unlock(&qs->rspq.lock);
		}
#endif		
		if ((!buf_ring_empty(txq->txq_mr)) && err == 0) {
#if 0
			if (cxgb_debug)
				printf("head=%p cons=%d prod=%d\n",
				    txq->sendq.head, txq->txq_mr.br_cons,
				    txq->txq_mr.br_prod);
#endif			
			continue;
		}
	done:	
		tsleep(qs, 1, "cxgbidle", idleticks);
	}

	if (bootverbose)
		device_printf(qs->port->adapter->dev, "exiting thread for cpu%d\n", qs->qs_cpuid);


	cxgb_pcpu_free(qs);
	t3_free_qset(qs->port->adapter, qs);

	qs->qs_flags &= ~QS_RUNNING;
#if __FreeBSD_version >= 800002
	kproc_exit(0);
#else
	kthread_exit(0);
#endif
}

#ifdef IFNET_MULTIQUEUE
static int
cxgb_pcpu_cookie_to_qidx(struct port_info *pi, uint32_t cookie)
{
	int qidx;
	uint32_t tmp;

	if (multiq_tx_enable == 0)
		return (pi->first_qset);
	
	 /*
	 * Will probably need to be changed for 4-port XXX
	 */
	tmp = pi->tx_chan ? cookie : cookie & ((RSS_TABLE_SIZE>>1)-1);
	DPRINTF(" tmp=%d ", tmp);
	qidx = (tmp & (pi->nqsets -1)) + pi->first_qset;

	return (qidx);
}
#endif

void
cxgb_pcpu_startup_threads(struct adapter *sc)
{
	int i, j, nqsets;
	struct proc *p;


	for (i = 0; i < (sc)->params.nports; ++i) {
			struct port_info *pi = adap2pinfo(sc, i);

#ifdef IFNET_MULTIQUEUE
	nqsets = pi->nqsets;
#else
	nqsets = 1;
#endif	
		for (j = 0; j < nqsets; ++j) {
			struct sge_qset *qs;

			qs = &sc->sge.qs[pi->first_qset + j];
			qs->port = pi;
			qs->qs_cpuid = ((pi->first_qset + j) % mp_ncpus);
			device_printf(sc->dev, "starting thread for %d\n",
			    qs->qs_cpuid);

#if __FreeBSD_version >= 800002
			kproc_create(cxgb_pcpu_start_proc, qs, &p,
			    RFNOWAIT, 0, "cxgbsp");
#else
			kthread_create(cxgb_pcpu_start_proc, qs, &p,
			    RFNOWAIT, 0, "cxgbsp");
#endif
			DELAY(200);
		}
	}
}

void
cxgb_pcpu_shutdown_threads(struct adapter *sc)
{
	int i, j;
	int nqsets;

	for (i = 0; i < sc->params.nports; i++) {
		struct port_info *pi = &sc->port[i];
		int first = pi->first_qset;

#ifdef IFNET_MULTIQUEUE
	nqsets = pi->nqsets;
#else
	nqsets = 1;
#endif	
		for (j = 0; j < nqsets; j++) {
			struct sge_qset *qs = &sc->sge.qs[first + j];

			qs->qs_flags |= QS_EXITING;
			wakeup(qs);
			tsleep(&sc, PRI_MIN_TIMESHARE, "cxgb unload 0", hz>>2);
			while (qs->qs_flags & QS_RUNNING) {
				qs->qs_flags |= QS_EXITING;
				device_printf(sc->dev, "qset thread %d still running - sleeping\n", first + j);
				tsleep(&sc, PRI_MIN_TIMESHARE, "cxgb unload 1", 2*hz);
			}
		}
	}
}

static __inline void
check_pkt_coalesce(struct sge_qset *qs)
{
	struct adapter *sc;
	struct sge_txq *txq;

	txq = &qs->txq[TXQ_ETH];
	sc = qs->port->adapter;

	if (sc->tunq_fill[qs->idx] && (txq->in_use < (txq->size - (txq->size>>2)))) 
		sc->tunq_fill[qs->idx] = 0;
	else if (!sc->tunq_fill[qs->idx] && (txq->in_use > (txq->size - (txq->size>>2)))) 
		sc->tunq_fill[qs->idx] = 1;
}

static int
cxgb_tx(struct sge_qset *qs, uint32_t txmax)
{
	struct sge_txq *txq;
	struct ifnet *ifp = qs->port->ifp;
	int i, err, in_use_init, count;
	struct mbuf *m_vec[TX_WR_COUNT_MAX];

	txq = &qs->txq[TXQ_ETH];
	ifp = qs->port->ifp;
	in_use_init = txq->in_use;
	count = err = 0;

	mtx_assert(&txq->lock, MA_OWNED);
	while ((txq->in_use - in_use_init < txmax) &&
	    (txq->size > txq->in_use + TX_MAX_DESC)) {
		check_pkt_coalesce(qs);
		count = cxgb_dequeue_packet(txq, m_vec);
		if (count == 0) 
			break;
		for (i = 0; i < count; i++)
			ETHER_BPF_MTAP(ifp, m_vec[i]);
		
		if ((err = t3_encap(qs, m_vec, count)) != 0)
			break;
		txq->txq_enqueued += count;
	}
	if (txq->size <= txq->in_use + TX_MAX_DESC) {
		txq_fills++;
		setbit(&qs->txq_stopped, TXQ_ETH);
	}
	if (err == ENOMEM) {
		int i;
		/*
		 * Sub-optimal :-/
		 */
		printf("ENOMEM!!!");
		for (i = 0; i < count; i++)
			m_freem(m_vec[i]);
	}
	return (err);
}

