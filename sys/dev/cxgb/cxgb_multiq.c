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

#ifdef IFNET_MULTIQUEUE
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
#include <sys/unistd.h>

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

#ifdef CONFIG_DEFINED
#include <cxgb_include.h>
#include <sys/mvec.h>
#else
#include <dev/cxgb/cxgb_include.h>
#include <dev/cxgb/sys/mvec.h>
#endif

extern struct sysctl_oid_list sysctl__hw_cxgb_children;
static int cxgb_pcpu_tx_coalesce = 1;
TUNABLE_INT("hw.cxgb.tx_coalesce", &cxgb_pcpu_tx_coalesce);
SYSCTL_UINT(_hw_cxgb, OID_AUTO, tx_coalesce, CTLFLAG_RDTUN, &cxgb_pcpu_tx_coalesce, 0,
    "coalesce small packets into a single work request");

static int sleep_ticks = 1;
TUNABLE_INT("hw.cxgb.sleep_ticks", &sleep_ticks);
SYSCTL_UINT(_hw_cxgb, OID_AUTO, sleep_ticks, CTLFLAG_RDTUN, &sleep_ticks, 0,
    "ticks to sleep between checking pcpu queues");

int cxgb_txq_buf_ring_size = TX_ETH_Q_SIZE;
TUNABLE_INT("hw.cxgb.txq_mr_size", &cxgb_txq_buf_ring_size);
SYSCTL_UINT(_hw_cxgb, OID_AUTO, txq_mr_size, CTLFLAG_RDTUN, &cxgb_txq_buf_ring_size, 0,
    "size of per-queue mbuf ring");


static inline int32_t cxgb_pcpu_calc_cookie(struct ifnet *ifp, struct mbuf *immpkt);
static void cxgb_pcpu_start_proc(void *arg);
static int cxgb_pcpu_cookie_to_qidx(struct port_info *, uint32_t cookie);

static inline int
cxgb_pcpu_enqueue_packet_(struct sge_qset *qs, struct mbuf *m)
{
	struct sge_txq *txq;
	struct buf_ring *mr;
	int prod, cons, mask;
	int err = 0;
	
	if (qs->qs_flags & QS_EXITING) {
		m_freem(m);
		return (ENXIO);
	}
	txq = &qs->txq[TXQ_ETH];

	mr = &txq->txq_mr;
	mtx_lock(&mr->br_lock);
	cons = mr->br_cons;
	prod = mr->br_prod;
	mask = mr->br_size - 1;
	if (((prod + 1) & mask) != cons) {
		mr->br_ring[prod] = (caddr_t)m;
		mb();
		mr->br_prod = (prod + 1) & mask;
	} else {
		txq->txq_drops++;
		err = ENOBUFS;
	}
	mtx_unlock(&mr->br_lock);
	if ((qs->txq[TXQ_ETH].flags & TXQ_TRANSMITTING) == 0)
		wakeup(qs);
	if (err) 
		m_freem(m);

	return (err);
}
	
int
cxgb_pcpu_enqueue_packet(struct ifnet *ifp, struct mbuf *m)
{
	struct port_info *pi;
	struct sge_qset *qs;
	int err, qidx;
	int32_t calc_cookie;

	pi = ifp->if_softc;
	err = 0;

	calc_cookie = m->m_pkthdr.rss_hash;
	qidx = cxgb_pcpu_cookie_to_qidx(pi, calc_cookie);
	qs = &pi->adapter->sge.qs[qidx];

	err = cxgb_pcpu_enqueue_packet_(qs, m);
	
	return (err);
}

int
cxgb_dequeue_packet(struct ifnet *unused, struct sge_txq *txq, struct mbuf **m_vec)
{
	struct mbuf *m;
	struct sge_qset *qs;
	int count, size, coalesced;
	struct adapter *sc;
	struct buf_ring *mr;
	
	mr = &txq->txq_mr;
	coalesced = count = size = 0;

	qs = txq_to_qset(txq, TXQ_ETH);
	if (qs->qs_flags & QS_EXITING)
		return (0);

	if (txq->immpkt != NULL) {
		DPRINTF("immediate packet\n");
		m_vec[0] = txq->immpkt;
		txq->immpkt = NULL;
		return (1);
	}
	sc = qs->port->adapter;

	m = buf_ring_dequeue(mr);
	if (m == NULL)
		return (0);

	m_vec[0] = m;
	if (m->m_pkthdr.tso_segsz > 0 || m->m_pkthdr.len > TX_WR_SIZE_MAX || m->m_next != NULL) {
		DPRINTF("returning 1 packet\n");
		return (1);
	}
	count = 1;
	size = m->m_pkthdr.len;
	for (m = buf_ring_peek(mr); m != NULL; m = buf_ring_peek(mr)) {

		if (m->m_pkthdr.tso_segsz > 0 || size + m->m_pkthdr.len > TX_WR_SIZE_MAX || m->m_next != NULL)
			break;

		buf_ring_dequeue(mr);
		size += m->m_pkthdr.len;
		m_vec[count++] = m;


		if (count == TX_WR_COUNT_MAX || (cxgb_pcpu_tx_coalesce == 0))
			break;

		coalesced++;
	}
	txq->txq_coalesced += coalesced;
	
	return (count);
}

int32_t
cxgb_pcpu_get_cookie(struct ifnet *ifp, struct in6_addr *lip, uint16_t lport, struct in6_addr *rip, uint16_t rport, int ipv6)
{
	uint32_t base;
	uint8_t buf[36];
	int count;
	int32_t cookie;

	critical_enter();
	/* 
	 * Can definitely bypass bcopy XXX
	 */
	if (ipv6 == 0) {
		count = 12;
		bcopy(rip, &buf[0], 4);
		bcopy(lip, &buf[4], 4);
		bcopy(&rport, &buf[8], 2);
		bcopy(&lport, &buf[10], 2);
	} else {
		count = 36;
		bcopy(rip, &buf[0], 16);
		bcopy(lip, &buf[16], 16);
		bcopy(&rport, &buf[32], 2);
		bcopy(&lport, &buf[34], 2);
	}
	
	base = 0xffffffff;
	base = update_crc32(base, buf, count);
	base = sctp_csum_finalize(base);

	/*
	 * Indirection table is 128 bits
	 * -> cookie indexes into indirection table which maps connection to queue
	 * -> RSS map maps queue to CPU
	 */
	cookie = (base & (RSS_TABLE_SIZE-1));
	critical_exit();
	
	return (cookie);
}

static int32_t
cxgb_pcpu_calc_cookie(struct ifnet *ifp, struct mbuf *immpkt)
{
	struct in6_addr lip, rip;
	uint16_t lport, rport;
	struct ether_header *eh;
	int32_t cookie;
	struct ip *ip;
	struct ip6_hdr *ip6;
	struct tcphdr *th;
	struct udphdr *uh;
	struct sctphdr *sh;
	uint8_t *next, proto;
	int etype;

	if (immpkt == NULL)
		return -1;

#if 1	
	/*
	 * XXX perf test
	 */
	return (0);
#endif	
	rport = lport = 0;
	cookie = -1;
	next = NULL;
	eh = mtod(immpkt, struct ether_header *);
	etype = ntohs(eh->ether_type);

	switch (etype) {
	case ETHERTYPE_IP:
		ip = (struct ip *)(eh + 1);
		next = (uint8_t *)(ip + 1);
		bcopy(&ip->ip_src, &lip, 4);
		bcopy(&ip->ip_dst, &rip, 4);
		proto = ip->ip_p;
		break;
	case ETHERTYPE_IPV6:
		ip6 = (struct ip6_hdr *)(eh + 1);
		next = (uint8_t *)(ip6 + 1);
		bcopy(&ip6->ip6_src, &lip, sizeof(struct in6_addr));
		bcopy(&ip6->ip6_dst, &rip, sizeof(struct in6_addr));
		if (ip6->ip6_nxt == IPPROTO_HOPOPTS) {
			struct ip6_hbh *hbh;

			hbh = (struct ip6_hbh *)(ip6 + 1);
			proto = hbh->ip6h_nxt;
		} else 
			proto = ip6->ip6_nxt;
		break;
	case ETHERTYPE_ARP:
	default:
		/*
		 * Default to queue zero
		 */
		proto = cookie = 0;
	}
	if (proto) {
		switch (proto) {
		case IPPROTO_TCP:
			th = (struct tcphdr *)next;
			lport = th->th_sport;
			rport = th->th_dport;
			break;
		case IPPROTO_UDP:
			uh = (struct udphdr *)next;
			lport = uh->uh_sport;
			rport = uh->uh_dport;
			break;
		case IPPROTO_SCTP:
			sh = (struct sctphdr *)next;
			lport = sh->src_port;
			rport = sh->dest_port;
			break;
		default:
			/* nothing to do */
			break;
		}
	}
	
	if (cookie) 		
		cookie = cxgb_pcpu_get_cookie(ifp, &lip, lport, &rip, rport, (etype == ETHERTYPE_IPV6));

	return (cookie);
}

static void
cxgb_pcpu_free(struct sge_qset *qs)
{
	struct mbuf *m;
	struct sge_txq *txq = &qs->txq[TXQ_ETH];
	
	while ((m = mbufq_dequeue(&txq->sendq)) != NULL) 
		m_freem(m);
	while ((m = buf_ring_dequeue(&txq->txq_mr)) != NULL) 
		m_freem(m);

	t3_free_tx_desc_all(txq);
}

static int
cxgb_pcpu_reclaim_tx(struct sge_txq *txq)
{
	int reclaimable;
	struct sge_qset *qs = txq_to_qset(txq, TXQ_ETH);

#ifdef notyet
	KASSERT(qs->qs_cpuid == curcpu, ("cpu qset mismatch cpuid=%d curcpu=%d",
			qs->qs_cpuid, curcpu));
#endif
	
	reclaimable = desc_reclaimable(txq);
	if (reclaimable == 0)
		return (0);
	
	t3_free_tx_desc(txq, reclaimable);
		
	txq->cleaned += reclaimable;
	txq->in_use -= reclaimable;
	if (isset(&qs->txq_stopped, TXQ_ETH))
		clrbit(&qs->txq_stopped, TXQ_ETH);
	
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
	
 retry:	
	if (!pi->link_config.link_ok)
		initerr = ENXIO;
	else if (qs->qs_flags & QS_EXITING)
		initerr = ENXIO;
	else {
		txq = &qs->txq[TXQ_ETH];

		if (!buf_ring_empty(&txq->txq_mr)) 
			initerr = cxgb_pcpu_enqueue_packet_(qs, immpkt);
		else
			txq->immpkt = immpkt;

		immpkt = NULL;
	}
	if (initerr && initerr != ENOBUFS) {
		if (cxgb_debug)
			printf("cxgb link down\n");
		if (immpkt)
			m_freem(immpkt);
		return (initerr);
	}

	if ((tx_flush && (desc_reclaimable(txq) > 0)) || (desc_reclaimable(txq) > (TX_ETH_Q_SIZE>>1))) {
		int reclaimed = 0;

		if (cxgb_debug) {
			device_printf(qs->port->adapter->dev,
			    "cpuid=%d curcpu=%d reclaimable=%d txq=%p txq->cidx=%d txq->pidx=%d ",
			    qs->qs_cpuid, curcpu, desc_reclaimable(txq),
			    txq, txq->cidx, txq->pidx);
		}
		reclaimed = cxgb_pcpu_reclaim_tx(txq);
		if (cxgb_debug)
			printf("reclaimed=%d\n", reclaimed);
	}

	stopped = isset(&qs->txq_stopped, TXQ_ETH);
	flush = ((!buf_ring_empty(&txq->txq_mr) && !stopped) || txq->immpkt); 
	max_desc = tx_flush ? TX_ETH_Q_SIZE : TX_START_MAX_DESC;
	err = flush ? cxgb_tx_common(qs->port->ifp, qs, max_desc) : ENOSPC;
	
	if ((tx_flush && flush && err == 0) && !buf_ring_empty(&txq->txq_mr)) {
		struct thread *td = curthread;

		if (++i > 1) {
			thread_lock(td);
			sched_prio(td, PRI_MIN_TIMESHARE);
			thread_unlock(td);
		}
		if (i > 50) {
			if (cxgb_debug)
				device_printf(qs->port->adapter->dev,
				    "exceeded max enqueue tries\n");
			return (EBUSY);
		}
		goto retry;
	}
	err = (initerr != 0) ? initerr : err;

	return (err);
}

int
cxgb_pcpu_start(struct ifnet *ifp, struct mbuf *immpkt)
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
	
	if (immpkt && (immpkt->m_pkthdr.rss_hash != 0)) {
		cookie = immpkt->m_pkthdr.rss_hash;
		qidx = cxgb_pcpu_cookie_to_qidx(pi, cookie);
		DPRINTF("hash=0x%x qidx=%d cpu=%d\n", immpkt->m_pkthdr.rss_hash, qidx, curcpu);
		qs = &pi->adapter->sge.qs[qidx];
	} else 
		qs = &pi->adapter->sge.qs[pi->first_qset];
	
	txq = &qs->txq[TXQ_ETH];

	if (((sc->tunq_coalesce == 0) || (buf_ring_count(&txq->txq_mr) > TX_WR_COUNT_MAX)) && mtx_trylock(&txq->lock)) {
		txq->flags |= TXQ_TRANSMITTING;
		err = cxgb_pcpu_start_(qs, immpkt, FALSE);
		txq->flags &= ~TXQ_TRANSMITTING;
		resid = (buf_ring_count(&txq->txq_mr) > 64) || (desc_reclaimable(txq) > 64);
		mtx_unlock(&txq->lock);
	} else if (immpkt)
		err = cxgb_pcpu_enqueue_packet_(qs, immpkt);

	if (resid && (txq->flags & TXQ_TRANSMITTING) == 0)
		wakeup(qs);

	return ((err == ENOSPC) ? 0 : err);
}

void
cxgb_start(struct ifnet *ifp)
{
	struct port_info *pi = ifp->if_softc;	
	struct sge_qset *qs;
	struct mbuf *m, *head, *tail, *lhead, *ltail;
	int calc_cookie, qidx, i;

	IFQ_LOCK(&ifp->if_snd);
	IFQ_DEQUEUE_NOLOCK(&ifp->if_snd, m);
	head = tail = m;
	i = 0;
	while (m != NULL) {
		IFQ_DEQUEUE_NOLOCK(&ifp->if_snd, m);
		tail->m_nextpkt = m;
		tail = m;
		i++;
	} 
	IFQ_UNLOCK(&ifp->if_snd);
	printf("dequeued %d packets\n", i);
	lhead = ltail = NULL;
	for (m = head; m != NULL; m = head->m_nextpkt) {
		calc_cookie = cxgb_pcpu_calc_cookie(ifp, m);
		qidx = cxgb_pcpu_cookie_to_qidx(pi, calc_cookie);
		qs = &pi->adapter->sge.qs[qidx];
		critical_enter();
		if (qs->qs_cpuid == curcpu) {
			if (lhead == NULL)
				lhead = m;
			else
				ltail->m_nextpkt = m;
			ltail = m;
			critical_exit();
		} else {
			critical_exit();
			cxgb_pcpu_enqueue_packet_(qs, m);
		}
	}
	if (curcpu < SGE_QSETS) {
		qs = &pi->adapter->sge.qs[curcpu]; 
		/*
		 * Assume one-to-one mapping of qset to CPU for now XXX 
		 */

		(void)cxgb_pcpu_start_(qs, NULL, TRUE);
		/*
			 * XXX multiple packets
			 */
			cxgb_pcpu_enqueue_packet_(qs, lhead);
	}
}

static void
cxgb_pcpu_start_proc(void *arg)
{
	struct sge_qset *qs = arg;
	struct thread *td;
	struct sge_txq *txq = &qs->txq[TXQ_ETH];
	int idleticks, err = 0;
	struct adapter *sc = qs->port->adapter;

	td = curthread;

	sleep_ticks = hz/100;
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

		if ((qs->port->ifp->if_drv_flags && IFF_DRV_RUNNING) == 0) {
			idleticks = hz;
			if (!buf_ring_empty(&txq->txq_mr) ||
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

		if (mtx_trylock(&qs->rspq.lock)) {
			process_responses(sc, qs, -1);

			refill_fl_service(sc, &qs->fl[0]);
			refill_fl_service(sc, &qs->fl[1]);
			t3_write_reg(sc, A_SG_GTS, V_RSPQ(qs->rspq.cntxt_id) |
			    V_NEWTIMER(qs->rspq.next_holdoff) | V_NEWINDEX(qs->rspq.cidx));

			mtx_unlock(&qs->rspq.lock);
		}
		if ((!buf_ring_empty(&txq->txq_mr)) && err == 0) {
			if (cxgb_debug)
				printf("head=%p cons=%d prod=%d\n",
				    txq->sendq.head, txq->txq_mr.br_cons,
				    txq->txq_mr.br_prod);
			continue;
		}
	done:	
		tsleep(qs, 1, "cxgbidle", sleep_ticks);
	}

	if (bootverbose)
		device_printf(qs->port->adapter->dev, "exiting thread for cpu%d\n", qs->qs_cpuid);


	cxgb_pcpu_free(qs);
	t3_free_qset(qs->port->adapter, qs);

	qs->qs_flags &= ~QS_RUNNING;
	kproc_exit(0);
}

static int
cxgb_pcpu_cookie_to_qidx(struct port_info *pi, uint32_t cookie)
{
	int qidx;
	uint32_t tmp;
	
	 /*
	 * Will probably need to be changed for 4-port XXX
	 */
	tmp = pi->tx_chan ? cookie : cookie & ((RSS_TABLE_SIZE>>1)-1);
	DPRINTF(" tmp=%d ", tmp);
	qidx = (tmp & (pi->nqsets -1)) + pi->first_qset;

	return (qidx);
}


void
cxgb_pcpu_startup_threads(struct adapter *sc)
{
	int i, j;
	struct proc *p;
	
	for (i = 0; i < (sc)->params.nports; ++i) {
			struct port_info *pi = adap2pinfo(sc, i);
		
		for (j = 0; j < pi->nqsets; ++j) {
			struct sge_qset *qs;

			qs = &sc->sge.qs[pi->first_qset + j];
			qs->port = pi;
			qs->qs_cpuid = ((pi->first_qset + j) % mp_ncpus);
			device_printf(sc->dev, "starting thread for %d\n", qs->qs_cpuid);

			kproc_create(cxgb_pcpu_start_proc, qs, &p,
			    RFNOWAIT, 0, "cxgbsp");
			DELAY(200);
		}
	}
}

void
cxgb_pcpu_shutdown_threads(struct adapter *sc)
{
	int i, j;
	
	for (i = 0; i < sc->params.nports; i++) {
		struct port_info *pi = &sc->port[i];
		int first = pi->first_qset;
		for (j = 0; j < pi->nqsets; j++) {
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
#endif
