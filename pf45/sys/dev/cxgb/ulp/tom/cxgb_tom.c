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
#include <sys/fcntl.h>
#include <sys/ktr.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/eventhandler.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/condvar.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/sockopt.h>
#include <sys/sockstate.h>
#include <sys/sockbuf.h>
#include <sys/syslog.h>
#include <sys/taskqueue.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>

#include <cxgb_osdep.h>
#include <sys/mbufq.h>

#include <netinet/in_pcb.h>

#include <ulp/tom/cxgb_tcp_offload.h>
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_offload.h>
#include <netinet/tcp_fsm.h>

#include <cxgb_include.h>

#include <net/if_vlan_var.h>
#include <net/route.h>

#include <t3cdev.h>
#include <common/cxgb_firmware_exports.h>
#include <common/cxgb_tcb.h>
#include <cxgb_include.h>
#include <common/cxgb_ctl_defs.h>
#include <common/cxgb_t3_cpl.h>
#include <cxgb_offload.h>
#include <ulp/toecore/cxgb_toedev.h>
#include <ulp/tom/cxgb_l2t.h>
#include <ulp/tom/cxgb_tom.h>
#include <ulp/tom/cxgb_defs.h>
#include <ulp/tom/cxgb_t3_ddp.h>
#include <ulp/tom/cxgb_toepcb.h>
#include <ulp/tom/cxgb_tcp.h>


TAILQ_HEAD(, adapter) adapter_list;
static struct rwlock adapter_list_lock;

static TAILQ_HEAD(, tom_data) cxgb_list;
static struct mtx cxgb_list_lock;
static const unsigned int MAX_ATIDS = 64 * 1024;
static const unsigned int ATID_BASE = 0x100000;

static int t3_toe_attach(struct toedev *dev, const struct offload_id *entry);
static void cxgb_register_listeners(void);
static void t3c_tom_add(struct t3cdev *cdev);

/*
 * Handlers for each CPL opcode
 */
static cxgb_cpl_handler_func tom_cpl_handlers[256];


static eventhandler_tag listen_tag;

static struct offload_id t3_toe_id_tab[] = {
	{ TOE_ID_CHELSIO_T3, 0 },
	{ TOE_ID_CHELSIO_T3B, 0 },
	{ TOE_ID_CHELSIO_T3C, 0 },
	{ 0 }
};

static struct tom_info t3_tom_info = {
	.ti_attach = t3_toe_attach,
	.ti_id_table = t3_toe_id_tab,
	.ti_name = "Chelsio-T3"
};

struct cxgb_client t3c_tom_client = {
	.name = "tom_cxgb3",
	.add = t3c_tom_add,
	.remove = NULL,
	.handlers = tom_cpl_handlers,
	.redirect = NULL
};

void
cxgb_log_tcb(struct adapter *sc, unsigned int tid)
{

	char buf[TCB_SIZE];
	uint64_t *tcb = (uint64_t *)buf;
	int i, error;
	struct mc7 *mem = &sc->cm;

	error = t3_mc7_bd_read(mem, tid*TCB_SIZE/8, TCB_SIZE/8, tcb);
	if (error)
		printf("cxgb_tcb_log failed\n");


	CTR1(KTR_CXGB, "TCB tid=%u", tid);
	for (i = 0; i < TCB_SIZE / 32; i++) {

		CTR5(KTR_CXGB, "%1d: %08x %08x %08x %08x",
		    i, (uint32_t)tcb[1], (uint32_t)(tcb[1] >> 32),
		    (uint32_t)tcb[0], (uint32_t)(tcb[0] >> 32));

		tcb += 2;
		CTR4(KTR_CXGB, "   %08x %08x %08x %08x",
		    (uint32_t)tcb[1], (uint32_t)(tcb[1] >> 32),
		    (uint32_t)tcb[0], (uint32_t)(tcb[0] >> 32));
		tcb += 2;
	}
}

/*
 * Add an skb to the deferred skb queue for processing from process context.
 */
void
t3_defer_reply(struct mbuf *m, struct toedev *dev, defer_handler_t handler)
{
	struct tom_data *td = TOM_DATA(dev);

	m_set_handler(m, handler);
	mtx_lock(&td->deferq.lock);
	
	mbufq_tail(&td->deferq, m);
	if (mbufq_len(&td->deferq) == 1)
		taskqueue_enqueue(td->tq, &td->deferq_task);
	mtx_lock(&td->deferq.lock);
}

struct toepcb *
toepcb_alloc(void)
{
	struct toepcb *toep;
	
	toep = malloc(sizeof(struct toepcb), M_CXGB, M_NOWAIT|M_ZERO);
	
	if (toep == NULL)
		return (NULL);

	toepcb_init(toep);
	return (toep);
}

void
toepcb_init(struct toepcb *toep)
{
	toep->tp_refcount = 1;
	cv_init(&toep->tp_cv, "toep cv");
}

void
toepcb_hold(struct toepcb *toep)
{
	atomic_add_acq_int(&toep->tp_refcount, 1);
}

void
toepcb_release(struct toepcb *toep)
{
	if (toep->tp_refcount == 1) {
		free(toep, M_CXGB);
		return;
	}
	atomic_add_acq_int(&toep->tp_refcount, -1);
}


/*
 * Add a T3 offload device to the list of devices we are managing.
 */
static void
t3cdev_add(struct tom_data *t)
{	
	mtx_lock(&cxgb_list_lock);
	TAILQ_INSERT_TAIL(&cxgb_list, t, entry);
	mtx_unlock(&cxgb_list_lock);
}

static inline int
cdev2type(struct t3cdev *cdev)
{
	int type = 0;

	switch (cdev->type) {
	case T3A:
		type = TOE_ID_CHELSIO_T3;
		break;
	case T3B:
		type = TOE_ID_CHELSIO_T3B;
		break;
	case T3C:
		type = TOE_ID_CHELSIO_T3C;
		break;
	}
	return (type);
}

/*
 * Allocate and initialize the TID tables.  Returns 0 on success.
 */
static int
init_tid_tabs(struct tid_info *t, unsigned int ntids,
			 unsigned int natids, unsigned int nstids,
			 unsigned int atid_base, unsigned int stid_base)
{
	unsigned long size = ntids * sizeof(*t->tid_tab) +
	    natids * sizeof(*t->atid_tab) + nstids * sizeof(*t->stid_tab);

	t->tid_tab = cxgb_alloc_mem(size);
	if (!t->tid_tab)
		return (ENOMEM);

	t->stid_tab = (union listen_entry *)&t->tid_tab[ntids];
	t->atid_tab = (union active_open_entry *)&t->stid_tab[nstids];
	t->ntids = ntids;
	t->nstids = nstids;
	t->stid_base = stid_base;
	t->sfree = NULL;
	t->natids = natids;
	t->atid_base = atid_base;
	t->afree = NULL;
	t->stids_in_use = t->atids_in_use = 0;
	t->tids_in_use = 0;
	mtx_init(&t->stid_lock, "stid", NULL, MTX_DUPOK|MTX_DEF);
	mtx_init(&t->atid_lock, "atid", NULL, MTX_DUPOK|MTX_DEF);

	/*
	 * Setup the free lists for stid_tab and atid_tab.
	 */
	if (nstids) {
		while (--nstids)
			t->stid_tab[nstids - 1].next = &t->stid_tab[nstids];
		t->sfree = t->stid_tab;
	}
	if (natids) {
		while (--natids)
			t->atid_tab[natids - 1].next = &t->atid_tab[natids];
		t->afree = t->atid_tab;
	}
	return 0;
}

static void
free_tid_maps(struct tid_info *t)
{
	mtx_destroy(&t->stid_lock);
	mtx_destroy(&t->atid_lock);
	cxgb_free_mem(t->tid_tab);
}

static inline void
add_adapter(adapter_t *adap)
{
	rw_wlock(&adapter_list_lock);
	TAILQ_INSERT_TAIL(&adapter_list, adap, adapter_entry);
	rw_wunlock(&adapter_list_lock);
}

static inline void
remove_adapter(adapter_t *adap)
{
	rw_wlock(&adapter_list_lock);
	TAILQ_REMOVE(&adapter_list, adap, adapter_entry);
	rw_wunlock(&adapter_list_lock);
}

/*
 * Populate a TID_RELEASE WR.  The mbuf must be already propely sized.
 */
static inline void
mk_tid_release(struct mbuf *m, unsigned int tid)
{
	struct cpl_tid_release *req;

	m_set_priority(m, CPL_PRIORITY_SETUP);
	req = mtod(m, struct cpl_tid_release *);
	m->m_pkthdr.len = m->m_len = sizeof(*req);
	req->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_TID_RELEASE, tid));
}

static void
t3_process_tid_release_list(void *data, int pending)
{
	struct mbuf *m;
	struct t3cdev *tdev = data;
	struct t3c_data *td = T3C_DATA (tdev);

	mtx_lock(&td->tid_release_lock);
	while (td->tid_release_list) {
		struct toe_tid_entry *p = td->tid_release_list;

		td->tid_release_list = (struct toe_tid_entry *)p->ctx;
		mtx_unlock(&td->tid_release_lock);
		m = m_get(M_WAIT, MT_DATA);
		mk_tid_release(m, p - td->tid_maps.tid_tab);
		cxgb_ofld_send(tdev, m);
		p->ctx = NULL;
		mtx_lock(&td->tid_release_lock);
	}
	mtx_unlock(&td->tid_release_lock);
}

int
cxgb_offload_activate(struct adapter *adapter)
{
	struct t3cdev *dev = &adapter->tdev;
	int natids, err;
	struct t3c_data *t;
	struct tid_range stid_range, tid_range;
	struct mtutab mtutab;
	unsigned int l2t_capacity;

	t = malloc(sizeof(*t), M_CXGB, M_NOWAIT|M_ZERO);
	if (!t)
		return (ENOMEM);
	dev->adapter = adapter;

	err = (EOPNOTSUPP);
	if (dev->ctl(dev, GET_TX_MAX_CHUNK, &t->tx_max_chunk) < 0 ||
	    dev->ctl(dev, GET_MAX_OUTSTANDING_WR, &t->max_wrs) < 0 ||
	    dev->ctl(dev, GET_L2T_CAPACITY, &l2t_capacity) < 0 ||
	    dev->ctl(dev, GET_MTUS, &mtutab) < 0 ||
	    dev->ctl(dev, GET_TID_RANGE, &tid_range) < 0 ||
	    dev->ctl(dev, GET_STID_RANGE, &stid_range) < 0) {
		device_printf(adapter->dev, "%s: dev->ctl check failed\n", __FUNCTION__);
		goto out_free;
	}
      
	err = (ENOMEM);
	L2DATA(dev) = t3_init_l2t(l2t_capacity);
	if (!L2DATA(dev)) {
		device_printf(adapter->dev, "%s: t3_init_l2t failed\n", __FUNCTION__);
		goto out_free;
	}
	natids = min(tid_range.num / 2, MAX_ATIDS);
	err = init_tid_tabs(&t->tid_maps, tid_range.num, natids,
			    stid_range.num, ATID_BASE, stid_range.base);
	if (err) {	
		device_printf(adapter->dev, "%s: init_tid_tabs failed\n", __FUNCTION__);
		goto out_free_l2t;
	}
	
	t->mtus = mtutab.mtus;
	t->nmtus = mtutab.size;

	TASK_INIT(&t->tid_release_task, 0 /* XXX? */, t3_process_tid_release_list, dev);
	mtx_init(&t->tid_release_lock, "tid release", NULL, MTX_DUPOK|MTX_DEF);
	t->dev = dev;

	T3C_DATA (dev) = t;
	dev->recv = process_rx;
	dev->arp_update = t3_l2t_update;
	/* Register netevent handler once */
	if (TAILQ_EMPTY(&adapter_list)) {
#if defined(CONFIG_CHELSIO_T3_MODULE)
		if (prepare_arp_with_t3core())
			log(LOG_ERR, "Unable to set offload capabilities\n");
#endif
	}
	CTR1(KTR_CXGB, "adding adapter %p", adapter); 
	add_adapter(adapter);
	device_printf(adapter->dev, "offload started\n");
	adapter->flags |= CXGB_OFLD_INIT;
	return (0);

out_free_l2t:
	t3_free_l2t(L2DATA(dev));
	L2DATA(dev) = NULL;
out_free:
	free(t, M_CXGB);
	return (err);
}

void
cxgb_offload_deactivate(struct adapter *adapter)
{
	struct t3cdev *tdev = &adapter->tdev;
	struct t3c_data *t = T3C_DATA(tdev);

	printf("removing adapter %p\n", adapter);
	remove_adapter(adapter);
	if (TAILQ_EMPTY(&adapter_list)) {
#if defined(CONFIG_CHELSIO_T3_MODULE)
		restore_arp_sans_t3core();
#endif
	}
	free_tid_maps(&t->tid_maps);
	T3C_DATA(tdev) = NULL;
	t3_free_l2t(L2DATA(tdev));
	L2DATA(tdev) = NULL;
	mtx_destroy(&t->tid_release_lock);
	free(t, M_CXGB);
}

/*
 * Sends an sk_buff to a T3C driver after dealing with any active network taps.
 */
int
cxgb_ofld_send(struct t3cdev *dev, struct mbuf *m)
{
	int r;

	r = dev->send(dev, m);
	return r;
}

static struct ifnet *
get_iff_from_mac(adapter_t *adapter, const uint8_t *mac, unsigned int vlan)
{
	int i;

	for_each_port(adapter, i) {
#ifdef notyet		
		const struct vlan_group *grp;
#endif		
		const struct port_info *p = &adapter->port[i];
		struct ifnet *ifp = p->ifp;

		if (!memcmp(p->hw_addr, mac, ETHER_ADDR_LEN)) {
#ifdef notyet	
			
			if (vlan && vlan != EVL_VLID_MASK) {
				grp = p->vlan_grp;
				dev = grp ? grp->vlan_devices[vlan] : NULL;
			} else
				while (dev->master)
					dev = dev->master;
#endif			
			return (ifp);
		}
	}
	return (NULL);
}

static inline void
failover_fixup(adapter_t *adapter, int port)
{
	if (adapter->params.rev == 0) {
		struct ifnet *ifp = adapter->port[port].ifp;
		struct cmac *mac = &adapter->port[port].mac;
		if (!(ifp->if_flags & IFF_UP)) {
			/* Failover triggered by the interface ifdown */
			t3_write_reg(adapter, A_XGM_TX_CTRL + mac->offset,
				     F_TXEN);
			t3_read_reg(adapter, A_XGM_TX_CTRL + mac->offset);
		} else {
			/* Failover triggered by the interface link down */
			t3_write_reg(adapter, A_XGM_RX_CTRL + mac->offset, 0);
			t3_read_reg(adapter, A_XGM_RX_CTRL + mac->offset);
			t3_write_reg(adapter, A_XGM_RX_CTRL + mac->offset,
				     F_RXEN);
		}
	}
}

static int
cxgb_ulp_iscsi_ctl(adapter_t *adapter, unsigned int req, void *data)
{
	int ret = 0;
	struct ulp_iscsi_info *uiip = data;

	switch (req) {
	case ULP_ISCSI_GET_PARAMS:
		uiip->llimit = t3_read_reg(adapter, A_ULPRX_ISCSI_LLIMIT);
		uiip->ulimit = t3_read_reg(adapter, A_ULPRX_ISCSI_ULIMIT);
		uiip->tagmask = t3_read_reg(adapter, A_ULPRX_ISCSI_TAGMASK);
		/*
		 * On tx, the iscsi pdu has to be <= tx page size and has to
		 * fit into the Tx PM FIFO.
		 */
		uiip->max_txsz = min(adapter->params.tp.tx_pg_size,
				     t3_read_reg(adapter, A_PM1_TX_CFG) >> 17);
		/* on rx, the iscsi pdu has to be < rx page size and the
		   whole pdu + cpl headers has to fit into one sge buffer */
		/* also check the max rx data length programmed in TP */
		uiip->max_rxsz = min(uiip->max_rxsz,
				     ((t3_read_reg(adapter, A_TP_PARA_REG2))
					>> S_MAXRXDATA) & M_MAXRXDATA);
		break;
	case ULP_ISCSI_SET_PARAMS:
		t3_write_reg(adapter, A_ULPRX_ISCSI_TAGMASK, uiip->tagmask);
		break;
	default:
		ret = (EOPNOTSUPP);
	}
	return ret;
}

/* Response queue used for RDMA events. */
#define ASYNC_NOTIF_RSPQ 0

static int
cxgb_rdma_ctl(adapter_t *adapter, unsigned int req, void *data)
{
	int ret = 0;

	switch (req) {
	case RDMA_GET_PARAMS: {
		struct rdma_info *req = data;

		req->udbell_physbase = rman_get_start(adapter->udbs_res);
		req->udbell_len = rman_get_size(adapter->udbs_res);
		req->tpt_base = t3_read_reg(adapter, A_ULPTX_TPT_LLIMIT);
		req->tpt_top  = t3_read_reg(adapter, A_ULPTX_TPT_ULIMIT);
		req->pbl_base = t3_read_reg(adapter, A_ULPTX_PBL_LLIMIT);
		req->pbl_top  = t3_read_reg(adapter, A_ULPTX_PBL_ULIMIT);
		req->rqt_base = t3_read_reg(adapter, A_ULPRX_RQ_LLIMIT);
		req->rqt_top  = t3_read_reg(adapter, A_ULPRX_RQ_ULIMIT);
		req->kdb_addr =  (void *)((unsigned long)rman_get_virtual(adapter->regs_res) + A_SG_KDOORBELL);		break;
	}
	case RDMA_CQ_OP: {
		struct rdma_cq_op *req = data;

		/* may be called in any context */
		mtx_lock_spin(&adapter->sge.reg_lock);
		ret = t3_sge_cqcntxt_op(adapter, req->id, req->op,
					req->credits);
		mtx_unlock_spin(&adapter->sge.reg_lock);
		break;
	}
	case RDMA_GET_MEM: {
		struct ch_mem_range *t = data;
		struct mc7 *mem;

		if ((t->addr & 7) || (t->len & 7))
			return (EINVAL);
		if (t->mem_id == MEM_CM)
			mem = &adapter->cm;
		else if (t->mem_id == MEM_PMRX)
			mem = &adapter->pmrx;
		else if (t->mem_id == MEM_PMTX)
			mem = &adapter->pmtx;
		else
			return (EINVAL);

		ret = t3_mc7_bd_read(mem, t->addr/8, t->len/8, (u64 *)t->buf);
		if (ret)
			return (ret);
		break;
	}
	case RDMA_CQ_SETUP: {
		struct rdma_cq_setup *req = data;

		mtx_lock_spin(&adapter->sge.reg_lock);
		ret = t3_sge_init_cqcntxt(adapter, req->id, req->base_addr,
					  req->size, ASYNC_NOTIF_RSPQ,
					  req->ovfl_mode, req->credits,
					  req->credit_thres);
		mtx_unlock_spin(&adapter->sge.reg_lock);
		break;
	}
	case RDMA_CQ_DISABLE:
		mtx_lock_spin(&adapter->sge.reg_lock);
		ret = t3_sge_disable_cqcntxt(adapter, *(unsigned int *)data);
		mtx_unlock_spin(&adapter->sge.reg_lock);
		break;
	case RDMA_CTRL_QP_SETUP: {
		struct rdma_ctrlqp_setup *req = data;

		mtx_lock_spin(&adapter->sge.reg_lock);
		ret = t3_sge_init_ecntxt(adapter, FW_RI_SGEEC_START, 0,
					 SGE_CNTXT_RDMA, ASYNC_NOTIF_RSPQ,
					 req->base_addr, req->size,
					 FW_RI_TID_START, 1, 0);
		mtx_unlock_spin(&adapter->sge.reg_lock);
		break;
	}
	default:
		ret = EOPNOTSUPP;
	}
	return (ret);
}

static int
cxgb_offload_ctl(struct t3cdev *tdev, unsigned int req, void *data)
{
	struct adapter *adapter = tdev2adap(tdev);
	struct tid_range *tid;
	struct mtutab *mtup;
	struct iff_mac *iffmacp;
	struct ddp_params *ddpp;
	struct adap_ports *ports;
	struct ofld_page_info *rx_page_info;
	struct tp_params *tp = &adapter->params.tp;
	int port;

	switch (req) {
	case GET_MAX_OUTSTANDING_WR:
		*(unsigned int *)data = FW_WR_NUM;
		break;
	case GET_WR_LEN:
		*(unsigned int *)data = WR_FLITS;
		break;
	case GET_TX_MAX_CHUNK:
		*(unsigned int *)data = 1 << 20;  /* 1MB */
		break;
	case GET_TID_RANGE:
		tid = data;
		tid->num = t3_mc5_size(&adapter->mc5) -
			adapter->params.mc5.nroutes -
			adapter->params.mc5.nfilters -
			adapter->params.mc5.nservers;
		tid->base = 0;
		break;
	case GET_STID_RANGE:
		tid = data;
		tid->num = adapter->params.mc5.nservers;
		tid->base = t3_mc5_size(&adapter->mc5) - tid->num -
			adapter->params.mc5.nfilters -
			adapter->params.mc5.nroutes;
		break;
	case GET_L2T_CAPACITY:
		*(unsigned int *)data = 2048;
		break;
	case GET_MTUS:
		mtup = data;
		mtup->size = NMTUS;
		mtup->mtus = adapter->params.mtus;
		break;
	case GET_IFF_FROM_MAC:
		iffmacp = data;
		iffmacp->dev = get_iff_from_mac(adapter, iffmacp->mac_addr,
					  iffmacp->vlan_tag & EVL_VLID_MASK);
		break;
	case GET_DDP_PARAMS:
		ddpp = data;
		ddpp->llimit = t3_read_reg(adapter, A_ULPRX_TDDP_LLIMIT);
		ddpp->ulimit = t3_read_reg(adapter, A_ULPRX_TDDP_ULIMIT);
		ddpp->tag_mask = t3_read_reg(adapter, A_ULPRX_TDDP_TAGMASK);
		break;
	case GET_PORTS:
		ports = data;
		ports->nports   = adapter->params.nports;
		for_each_port(adapter, port)
			ports->lldevs[port] = adapter->port[port].ifp;
		break;
	case FAILOVER:
		port = *(int *)data;
		t3_port_failover(adapter, port);
		failover_fixup(adapter, port);
		break;
	case FAILOVER_DONE:
		port = *(int *)data;
		t3_failover_done(adapter, port);
		break;
	case FAILOVER_CLEAR:
		t3_failover_clear(adapter);
		break;
	case GET_RX_PAGE_INFO:
		rx_page_info = data;
		rx_page_info->page_size = tp->rx_pg_size;
		rx_page_info->num = tp->rx_num_pgs;
		break;
	case ULP_ISCSI_GET_PARAMS:
	case ULP_ISCSI_SET_PARAMS:
		if (!offload_running(adapter))
			return (EAGAIN);
		return cxgb_ulp_iscsi_ctl(adapter, req, data);
	case RDMA_GET_PARAMS:
	case RDMA_CQ_OP:
	case RDMA_CQ_SETUP:
	case RDMA_CQ_DISABLE:
	case RDMA_CTRL_QP_SETUP:
	case RDMA_GET_MEM:
		if (!offload_running(adapter))
			return (EAGAIN);
		return cxgb_rdma_ctl(adapter, req, data);
	default:
		return (EOPNOTSUPP);
	}
	return 0;
}

/*
 * Allocate a TOM data structure,
 * initialize its cpl_handlers
 * and register it as a T3C client
 */
static void
t3c_tom_add(struct t3cdev *cdev)
{
	int i;
	unsigned int wr_len;
	struct tom_data *t;
	struct toedev *tdev;
	struct adap_ports *port_info;

	t = malloc(sizeof(*t), M_CXGB, M_NOWAIT|M_ZERO);
	if (t == NULL)
		return;

	cdev->send = t3_offload_tx;
	cdev->ctl = cxgb_offload_ctl;
	
	if (cdev->ctl(cdev, GET_WR_LEN, &wr_len) < 0)
		goto out_free_tom;

	port_info = malloc(sizeof(*port_info), M_CXGB, M_NOWAIT|M_ZERO);
	if (!port_info)
		goto out_free_tom;

	if (cdev->ctl(cdev, GET_PORTS, port_info) < 0)
		goto out_free_all;

	t3_init_wr_tab(wr_len);
	t->cdev = cdev;
	t->client = &t3c_tom_client;

	/* Register TCP offload device */
	tdev = &t->tdev;
	tdev->tod_ttid = cdev2type(cdev);
	tdev->tod_lldev = cdev->lldev;
	
	if (register_toedev(tdev, "toe%d")) {
		printf("unable to register offload device");
		goto out_free_all;
	}
	TOM_DATA(tdev) = t;

	for (i = 0; i < port_info->nports; i++) {
		struct ifnet *ifp = port_info->lldevs[i];
		TOEDEV(ifp) = tdev;

		CTR1(KTR_TOM, "enabling toe on %p", ifp);
		ifp->if_capabilities |= IFCAP_TOE4;
		ifp->if_capenable |= IFCAP_TOE4;
	}
	t->ports = port_info;

	/* Add device to the list of offload devices */
	t3cdev_add(t);

	/* Activate TCP offload device */
	cxgb_offload_activate(TOM_DATA(tdev)->cdev->adapter);

	activate_offload(tdev);
	cxgb_register_listeners();
	return;

out_free_all:
	printf("out_free_all fail\n");
	free(port_info, M_CXGB);
out_free_tom:
	printf("out_free_tom fail\n");
	free(t, M_CXGB);
	return;
}



static int
do_act_open_rpl(struct t3cdev *dev, struct mbuf *m)
{
	struct cpl_act_open_rpl *rpl = cplhdr(m);
	unsigned int atid = G_TID(ntohl(rpl->atid));
	struct toe_tid_entry *toe_tid;

	toe_tid = lookup_atid(&(T3C_DATA (dev))->tid_maps, atid);
	if (toe_tid->ctx && toe_tid->client && toe_tid->client->handlers &&
		toe_tid->client->handlers[CPL_ACT_OPEN_RPL]) {
		return toe_tid->client->handlers[CPL_ACT_OPEN_RPL] (dev, m,
			toe_tid->ctx);
	} else {
		log(LOG_ERR, "%s: received clientless CPL command 0x%x\n",
			dev->name, CPL_ACT_OPEN_RPL);
		return CPL_RET_BUF_DONE | CPL_RET_BAD_MSG;
	}
}

static int
do_stid_rpl(struct t3cdev *dev, struct mbuf *m)
{
	union opcode_tid *p = cplhdr(m);
	unsigned int stid = G_TID(ntohl(p->opcode_tid));
	struct toe_tid_entry *toe_tid;

	toe_tid = lookup_stid(&(T3C_DATA (dev))->tid_maps, stid);
	if (toe_tid->ctx && toe_tid->client->handlers &&
		toe_tid->client->handlers[p->opcode]) {
		return toe_tid->client->handlers[p->opcode] (dev, m, toe_tid->ctx);
	} else {
		log(LOG_ERR, "%s: received clientless CPL command 0x%x\n",
			dev->name, p->opcode);
		return CPL_RET_BUF_DONE | CPL_RET_BAD_MSG;
	}
}

static int
do_hwtid_rpl(struct t3cdev *dev, struct mbuf *m)
{
	union opcode_tid *p = cplhdr(m);
	unsigned int hwtid;
	struct toe_tid_entry *toe_tid;
	
	DPRINTF("do_hwtid_rpl opcode=0x%x\n", p->opcode);
	hwtid = G_TID(ntohl(p->opcode_tid));

	toe_tid = lookup_tid(&(T3C_DATA (dev))->tid_maps, hwtid);
	if (toe_tid->ctx && toe_tid->client->handlers &&
		toe_tid->client->handlers[p->opcode]) {
		return toe_tid->client->handlers[p->opcode]
						(dev, m, toe_tid->ctx);
	} else {
		log(LOG_ERR, "%s: received clientless CPL command 0x%x\n",
			dev->name, p->opcode);
		return CPL_RET_BUF_DONE | CPL_RET_BAD_MSG;
	}
}

static int
do_cr(struct t3cdev *dev, struct mbuf *m)
{
	struct cpl_pass_accept_req *req = cplhdr(m);
	unsigned int stid = G_PASS_OPEN_TID(ntohl(req->tos_tid));
	struct toe_tid_entry *toe_tid;

	toe_tid = lookup_stid(&(T3C_DATA (dev))->tid_maps, stid);
	if (toe_tid->ctx && toe_tid->client->handlers &&
		toe_tid->client->handlers[CPL_PASS_ACCEPT_REQ]) {
		return toe_tid->client->handlers[CPL_PASS_ACCEPT_REQ]
						(dev, m, toe_tid->ctx);
	} else {
		log(LOG_ERR, "%s: received clientless CPL command 0x%x\n",
			dev->name, CPL_PASS_ACCEPT_REQ);
		return CPL_RET_BUF_DONE | CPL_RET_BAD_MSG;
	}
}

static int
do_abort_req_rss(struct t3cdev *dev, struct mbuf *m)
{
	union opcode_tid *p = cplhdr(m);
	unsigned int hwtid = G_TID(ntohl(p->opcode_tid));
	struct toe_tid_entry *toe_tid;

	toe_tid = lookup_tid(&(T3C_DATA (dev))->tid_maps, hwtid);
	if (toe_tid->ctx && toe_tid->client->handlers &&
		toe_tid->client->handlers[p->opcode]) {
		return toe_tid->client->handlers[p->opcode]
						(dev, m, toe_tid->ctx);
	} else {
		struct cpl_abort_req_rss *req = cplhdr(m);
		struct cpl_abort_rpl *rpl;
		
		struct mbuf *m = m_get(M_NOWAIT, MT_DATA);
		if (!m) {
			log(LOG_NOTICE, "do_abort_req_rss: couldn't get mbuf!\n");
			goto out;
		}

		m_set_priority(m, CPL_PRIORITY_DATA);
		rpl = cplhdr(m);
		rpl->wr.wr_hi = 
			htonl(V_WR_OP(FW_WROPCODE_OFLD_HOST_ABORT_CON_RPL));
		rpl->wr.wr_lo = htonl(V_WR_TID(GET_TID(req)));
		OPCODE_TID(rpl) =
			htonl(MK_OPCODE_TID(CPL_ABORT_RPL, GET_TID(req)));
		rpl->cmd = req->status;
		cxgb_ofld_send(dev, m);
 out:
		return (CPL_RET_BUF_DONE);
	}
}

static int
do_act_establish(struct t3cdev *dev, struct mbuf *m)
{
	struct cpl_act_establish *req;
	unsigned int atid;
	struct toe_tid_entry *toe_tid;

	req = cplhdr(m);
	atid = G_PASS_OPEN_TID(ntohl(req->tos_tid));
	toe_tid = lookup_atid(&(T3C_DATA (dev))->tid_maps, atid);
	if (toe_tid && toe_tid->ctx && toe_tid->client->handlers &&
		toe_tid->client->handlers[CPL_ACT_ESTABLISH]) {
		
		return toe_tid->client->handlers[CPL_ACT_ESTABLISH]
						(dev, m, toe_tid->ctx);
	} else {
	
		log(LOG_ERR, "%s: received clientless CPL command 0x%x\n",
			dev->name, CPL_ACT_ESTABLISH);
		return CPL_RET_BUF_DONE | CPL_RET_BAD_MSG;
	}
}


static int
do_term(struct t3cdev *dev, struct mbuf *m)
{
	unsigned int hwtid = ntohl(m_get_priority(m)) >> 8 & 0xfffff;
	unsigned int opcode = G_OPCODE(ntohl(m->m_pkthdr.csum_data));
	struct toe_tid_entry *toe_tid;

	toe_tid = lookup_tid(&(T3C_DATA (dev))->tid_maps, hwtid);
	if (toe_tid && toe_tid->ctx && toe_tid->client->handlers &&
		toe_tid->client->handlers[opcode]) {
		return toe_tid->client->handlers[opcode](dev, m, toe_tid->ctx);
	} else {
		log(LOG_ERR, "%s: received clientless CPL command 0x%x\n",
			dev->name, opcode);
		return CPL_RET_BUF_DONE | CPL_RET_BAD_MSG;
	}
	return (0);
}

/*
 * Process a received packet with an unknown/unexpected CPL opcode.
 */
static int
do_bad_cpl(struct t3cdev *cdev, struct mbuf *m, void *ctx)
{
	log(LOG_ERR, "%s: received bad CPL command %u\n", cdev->name,
	    0xFF & *mtod(m, unsigned int *));
	return (CPL_RET_BUF_DONE | CPL_RET_BAD_MSG);
}

/*
 * Add a new handler to the CPL dispatch table.  A NULL handler may be supplied
 * to unregister an existing handler.
 */
void
t3tom_register_cpl_handler(unsigned int opcode, cxgb_cpl_handler_func h)
{
	if (opcode < UCHAR_MAX)
		tom_cpl_handlers[opcode] = h ? h : do_bad_cpl;
	else
		log(LOG_ERR, "Chelsio T3 TOM: handler registration for "
		       "opcode %u failed\n", opcode);
}

/*
 * Make a preliminary determination if a connection can be offloaded.  It's OK
 * to fail the offload later if we say we can offload here.  For now this
 * always accepts the offload request unless there are IP options.
 */
static int
can_offload(struct toedev *dev, struct socket *so)
{
	struct tom_data *tomd = TOM_DATA(dev);
	struct t3cdev *cdev = T3CDEV(dev->tod_lldev);
	struct tid_info *t = &(T3C_DATA(cdev))->tid_maps;

	return so_sotoinpcb(so)->inp_depend4.inp4_options == NULL &&
	    tomd->conf.activated &&
	    (tomd->conf.max_conn < 0 ||
	     atomic_load_acq_int(&t->tids_in_use) + t->atids_in_use < tomd->conf.max_conn);
}

static int
tom_ctl(struct toedev *dev, unsigned int req, void *data)
{
	struct tom_data *t = TOM_DATA(dev);
	struct t3cdev *cdev = t->cdev;

	if (cdev->ctl)
		return cdev->ctl(cdev, req, data);

	return (EOPNOTSUPP);
}

/*
 * Free an active-open TID.
 */
void *
cxgb_free_atid(struct t3cdev *tdev, int atid)
{
	struct tid_info *t = &(T3C_DATA(tdev))->tid_maps;
	union active_open_entry *p = atid2entry(t, atid);
	void *ctx = p->toe_tid.ctx;

	mtx_lock(&t->atid_lock);
	p->next = t->afree;
	t->afree = p;
	t->atids_in_use--;
	mtx_unlock(&t->atid_lock);

	return ctx;
}

/*
 * Free a server TID and return it to the free pool.
 */
void
cxgb_free_stid(struct t3cdev *tdev, int stid)
{
	struct tid_info *t = &(T3C_DATA (tdev))->tid_maps;
	union listen_entry *p = stid2entry(t, stid);

	mtx_lock(&t->stid_lock);
	p->next = t->sfree;
	t->sfree = p;
	t->stids_in_use--;
	mtx_unlock(&t->stid_lock);
}

/*
 * Free a server TID and return it to the free pool.
 */
void *
cxgb_get_lctx(struct t3cdev *tdev, int stid)
{
	struct tid_info *t = &(T3C_DATA (tdev))->tid_maps;
	union listen_entry *p = stid2entry(t, stid);

	return (p->toe_tid.ctx);
}

void
cxgb_insert_tid(struct t3cdev *tdev, struct cxgb_client *client,
	void *ctx, unsigned int tid)
{
	struct tid_info *t = &(T3C_DATA (tdev))->tid_maps;

	t->tid_tab[tid].client = client;
	t->tid_tab[tid].ctx = ctx;
	atomic_add_int(&t->tids_in_use, 1);
}

/* use ctx as a next pointer in the tid release list */
void
cxgb_queue_tid_release(struct t3cdev *tdev, unsigned int tid)
{
	struct t3c_data *td = T3C_DATA (tdev);
	struct toe_tid_entry *p = &td->tid_maps.tid_tab[tid];
	
	CTR0(KTR_TOM, "queuing tid release\n");
	
	mtx_lock(&td->tid_release_lock);
	p->ctx = td->tid_release_list;
	td->tid_release_list = p;

	if (!p->ctx)
		taskqueue_enqueue(tdev->adapter->tq, &td->tid_release_task);

	mtx_unlock(&td->tid_release_lock);
}

/*
 * Remove a tid from the TID table.  A client may defer processing its last
 * CPL message if it is locked at the time it arrives, and while the message
 * sits in the client's backlog the TID may be reused for another connection.
 * To handle this we atomically switch the TID association if it still points
 * to the original client context.
 */
void
cxgb_remove_tid(struct t3cdev *tdev, void *ctx, unsigned int tid)
{
	struct tid_info *t = &(T3C_DATA (tdev))->tid_maps;

	if (tid >= t->ntids)
		panic("tid=%d >= t->ntids=%d", tid, t->ntids);
	
	if (tdev->type == T3A)
		atomic_cmpset_ptr((uintptr_t *)&t->tid_tab[tid].ctx, (long)NULL, (long)ctx);
	else {
		struct mbuf *m;

		m = m_get(M_NOWAIT, MT_DATA);
		if (__predict_true(m != NULL)) {
			mk_tid_release(m, tid);
			CTR1(KTR_CXGB, "releasing tid=%u", tid);
			
			cxgb_ofld_send(tdev, m);
			t->tid_tab[tid].ctx = NULL;
		} else
			cxgb_queue_tid_release(tdev, tid);
	}
	atomic_add_int(&t->tids_in_use, -1);
}

int
cxgb_alloc_atid(struct t3cdev *tdev, struct cxgb_client *client,
		     void *ctx)
{
	int atid = -1;
	struct tid_info *t = &(T3C_DATA (tdev))->tid_maps;

	mtx_lock(&t->atid_lock);
	if (t->afree) {
		union active_open_entry *p = t->afree;

		atid = (p - t->atid_tab) + t->atid_base;
		t->afree = p->next;
		p->toe_tid.ctx = ctx;
		p->toe_tid.client = client;
		t->atids_in_use++;
	}
	mtx_unlock(&t->atid_lock);
	return atid;
}

int
cxgb_alloc_stid(struct t3cdev *tdev, struct cxgb_client *client,
		     void *ctx)
{
	int stid = -1;
	struct tid_info *t = &(T3C_DATA (tdev))->tid_maps;

	mtx_lock(&t->stid_lock);
	if (t->sfree) {
		union listen_entry *p = t->sfree;

		stid = (p - t->stid_tab) + t->stid_base;
		t->sfree = p->next;
		p->toe_tid.ctx = ctx;
		p->toe_tid.client = client;
		t->stids_in_use++;
	}
	mtx_unlock(&t->stid_lock);
	return stid;
}


static int
is_offloading(struct ifnet *ifp)
{
	struct adapter *adapter;
	int port;

	rw_rlock(&adapter_list_lock);
	TAILQ_FOREACH(adapter, &adapter_list, adapter_entry) {
		for_each_port(adapter, port) {
			if (ifp == adapter->port[port].ifp) {
				rw_runlock(&adapter_list_lock);
				return 1;
			}
		}
	}
	rw_runlock(&adapter_list_lock);
	return 0;
}


static void
cxgb_arp_update_event(void *unused, struct rtentry *rt0,
    uint8_t *enaddr, struct sockaddr *sa)
{

	if (!is_offloading(rt0->rt_ifp))
		return;

	RT_ADDREF(rt0);
	RT_UNLOCK(rt0);
	cxgb_neigh_update(rt0, enaddr, sa);
	RT_LOCK(rt0);
	RT_REMREF(rt0);
}

static void
cxgb_redirect_event(void *unused, int event, struct rtentry *rt0,
    struct rtentry *rt1, struct sockaddr *sa)
{
	/* 
	 * ignore events on non-offloaded interfaces
	 */
	if (!is_offloading(rt0->rt_ifp))
		return;

	/*
	 * Cannot redirect to non-offload device.
	 */
	if (!is_offloading(rt1->rt_ifp)) {
		log(LOG_WARNING, "%s: Redirect to non-offload"
		    "device ignored.\n", __FUNCTION__);
		return;
	}

        /*
	 * avoid LORs by dropping the route lock but keeping a reference
	 * 
	 */
	RT_ADDREF(rt0);
	RT_UNLOCK(rt0);
	RT_ADDREF(rt1);
	RT_UNLOCK(rt1);
	
	cxgb_redirect(rt0, rt1, sa);
	cxgb_neigh_update(rt1, NULL, sa);

	RT_LOCK(rt0);
	RT_REMREF(rt0);
	RT_LOCK(rt1);
	RT_REMREF(rt1);
}

void
cxgb_neigh_update(struct rtentry *rt, uint8_t *enaddr, struct sockaddr *sa)
{

	if (rt->rt_ifp && is_offloading(rt->rt_ifp) && (rt->rt_ifp->if_flags & IFCAP_TOE)) {
		struct t3cdev *tdev = T3CDEV(rt->rt_ifp);

		PANIC_IF(!tdev);
		t3_l2t_update(tdev, rt, enaddr, sa);
	}
}

static void
set_l2t_ix(struct t3cdev *tdev, u32 tid, struct l2t_entry *e)
{
	struct mbuf *m;
	struct cpl_set_tcb_field *req;

	m = m_gethdr(M_NOWAIT, MT_DATA);
	if (!m) {
		log(LOG_ERR, "%s: cannot allocate mbuf!\n", __FUNCTION__);
		return;
	}
	
	m_set_priority(m, CPL_PRIORITY_CONTROL);
	req = mtod(m, struct cpl_set_tcb_field *);
	m->m_pkthdr.len = m->m_len = sizeof(*req);
	
	req->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_SET_TCB_FIELD, tid));
	req->reply = 0;
	req->cpu_idx = 0;
	req->word = htons(W_TCB_L2T_IX);
	req->mask = htobe64(V_TCB_L2T_IX(M_TCB_L2T_IX));
	req->val = htobe64(V_TCB_L2T_IX(e->idx));
	tdev->send(tdev, m);
}

void
cxgb_redirect(struct rtentry *old, struct rtentry *new, struct sockaddr *sa)
{
	struct ifnet *olddev, *newdev;
	struct tid_info *ti;
	struct t3cdev *tdev;
	u32 tid;
	int update_tcb;
	struct l2t_entry *e;
	struct toe_tid_entry *te;

	olddev = old->rt_ifp;
	newdev = new->rt_ifp;
	if (!is_offloading(olddev))
		return;
	if (!is_offloading(newdev)) {
		log(LOG_WARNING, "%s: Redirect to non-offload"
		    "device ignored.\n", __FUNCTION__);
		return;
	}
	tdev = T3CDEV(olddev);
	PANIC_IF(!tdev);
	if (tdev != T3CDEV(newdev)) {
		log(LOG_WARNING, "%s: Redirect to different "
		    "offload device ignored.\n", __FUNCTION__);
		return;
	}

	/* Add new L2T entry */
	e = t3_l2t_get(tdev, new, new->rt_ifp, sa);
	if (!e) {
		log(LOG_ERR, "%s: couldn't allocate new l2t entry!\n",
		       __FUNCTION__);
		return;
	}

	/* Walk tid table and notify clients of dst change. */
	ti = &(T3C_DATA (tdev))->tid_maps;
	for (tid=0; tid < ti->ntids; tid++) {
		te = lookup_tid(ti, tid);
		PANIC_IF(!te);
		if (te->ctx && te->client && te->client->redirect) {
			update_tcb = te->client->redirect(te->ctx, old, new,
							  e);
			if (update_tcb)  {
				l2t_hold(L2DATA(tdev), e);
				set_l2t_ix(tdev, tid, e);
			}
		}
	}
	l2t_release(L2DATA(tdev), e);
}

/*
 * Initialize the CPL dispatch table.
 */
static void
init_cpl_handlers(void)
{
	int i;

	for (i = 0; i < 256; ++i)
		tom_cpl_handlers[i] = do_bad_cpl;

	t3_init_listen_cpl_handlers();
}

static int
t3_toe_attach(struct toedev *dev, const struct offload_id *entry)
{
	struct tom_data *t = TOM_DATA(dev);
	struct t3cdev *cdev = t->cdev;
	struct ddp_params ddp;
	struct ofld_page_info rx_page_info;
	int err;
	
	t3_init_tunables(t);
	mtx_init(&t->listen_lock, "tom data listeners", NULL, MTX_DEF);
	CTR2(KTR_TOM, "t3_toe_attach dev=%p entry=%p", dev, entry);

	dev->tod_can_offload = can_offload;
	dev->tod_connect = t3_connect;
	dev->tod_ctl = tom_ctl;
#if 0	
	dev->tod_failover = t3_failover;
#endif
	err = cdev->ctl(cdev, GET_DDP_PARAMS, &ddp);
	if (err)
		return err;

	err = cdev->ctl(cdev, GET_RX_PAGE_INFO, &rx_page_info);
	if (err)
		return err;

	t->ddp_llimit = ddp.llimit;
	t->ddp_ulimit = ddp.ulimit;
	t->pdev = ddp.pdev;
	t->rx_page_size = rx_page_info.page_size;
	/* OK if this fails, we just can't do DDP */
	t->nppods = (ddp.ulimit + 1 - ddp.llimit) / PPOD_SIZE;
	t->ppod_map = malloc(t->nppods, M_DEVBUF, M_NOWAIT|M_ZERO);

	mtx_init(&t->ppod_map_lock, "ppod map", NULL, MTX_DEF);


	t3_sysctl_register(cdev->adapter, &t->conf);
	return (0);
}

static void
cxgb_toe_listen_start(void *unused, struct tcpcb *tp)
{
	struct socket *so = inp_inpcbtosocket(tp->t_inpcb);
	struct tom_data *p;
	
	mtx_lock(&cxgb_list_lock);
	TAILQ_FOREACH(p, &cxgb_list, entry) {
			t3_listen_start(&p->tdev, so, p->cdev);
	}
	mtx_unlock(&cxgb_list_lock);
}

static void
cxgb_toe_listen_stop(void *unused, struct tcpcb *tp)
{
	struct socket *so = inp_inpcbtosocket(tp->t_inpcb);
	struct tom_data *p;
	
	mtx_lock(&cxgb_list_lock);
	TAILQ_FOREACH(p, &cxgb_list, entry) {
		if (tp->t_state == TCPS_LISTEN)
			t3_listen_stop(&p->tdev, so, p->cdev);
	}
	mtx_unlock(&cxgb_list_lock);
}

static void
cxgb_toe_listen_start_handler(struct inpcb *inp, void *arg)
{
	struct tcpcb *tp = intotcpcb(inp);

	if (tp->t_state == TCPS_LISTEN)
		cxgb_toe_listen_start(NULL, tp);
}

static void
cxgb_register_listeners(void)
{

	inp_apply_all(cxgb_toe_listen_start_handler, NULL);
}

static int
t3_tom_init(void)
{
	init_cpl_handlers();
	if (t3_init_cpl_io() < 0) {
		log(LOG_ERR,
		    "Unable to initialize cpl io ops\n");
		return -1;
	}
	t3_init_socket_ops();

	 /* Register with the TOE device layer. */

	if (register_tom(&t3_tom_info) != 0) {
		log(LOG_ERR,
		    "Unable to register Chelsio T3 TCP offload module.\n");
		return -1;
	}

	rw_init(&adapter_list_lock, "ofld adap list");
	TAILQ_INIT(&adapter_list);
	EVENTHANDLER_REGISTER(route_arp_update_event, cxgb_arp_update_event,
	    NULL, EVENTHANDLER_PRI_ANY);
	EVENTHANDLER_REGISTER(route_redirect_event, cxgb_redirect_event,
	    NULL, EVENTHANDLER_PRI_ANY);
	
	mtx_init(&cxgb_list_lock, "cxgb tom list", NULL, MTX_DEF);
	listen_tag = EVENTHANDLER_REGISTER(tcp_offload_listen_start,
	    cxgb_toe_listen_start, NULL, EVENTHANDLER_PRI_ANY);
	listen_tag = EVENTHANDLER_REGISTER(tcp_offload_listen_stop,
	    cxgb_toe_listen_stop, NULL, EVENTHANDLER_PRI_ANY);
	TAILQ_INIT(&cxgb_list);
	


	t3_register_cpl_handler(CPL_PASS_OPEN_RPL, do_stid_rpl);
	t3_register_cpl_handler(CPL_CLOSE_LISTSRV_RPL, do_stid_rpl);
	t3_register_cpl_handler(CPL_PASS_ACCEPT_REQ, do_cr);
	t3_register_cpl_handler(CPL_PASS_ESTABLISH, do_hwtid_rpl);
	t3_register_cpl_handler(CPL_ABORT_RPL_RSS, do_hwtid_rpl);
	t3_register_cpl_handler(CPL_ABORT_RPL, do_hwtid_rpl);
	t3_register_cpl_handler(CPL_RX_URG_NOTIFY, do_hwtid_rpl);
	t3_register_cpl_handler(CPL_RX_DATA, do_hwtid_rpl);
	t3_register_cpl_handler(CPL_TX_DATA_ACK, do_hwtid_rpl);
	t3_register_cpl_handler(CPL_TX_DMA_ACK, do_hwtid_rpl);
	t3_register_cpl_handler(CPL_ACT_OPEN_RPL, do_act_open_rpl);
	t3_register_cpl_handler(CPL_PEER_CLOSE, do_hwtid_rpl);
	t3_register_cpl_handler(CPL_CLOSE_CON_RPL, do_hwtid_rpl);
	t3_register_cpl_handler(CPL_ABORT_REQ_RSS, do_abort_req_rss);
	t3_register_cpl_handler(CPL_ACT_ESTABLISH, do_act_establish);
	t3_register_cpl_handler(CPL_RDMA_TERMINATE, do_term);
	t3_register_cpl_handler(CPL_RDMA_EC_STATUS, do_hwtid_rpl);
	t3_register_cpl_handler(CPL_RX_DATA_DDP, do_hwtid_rpl);
	t3_register_cpl_handler(CPL_RX_DDP_COMPLETE, do_hwtid_rpl);
	t3_register_cpl_handler(CPL_ISCSI_HDR, do_hwtid_rpl);
	t3_register_cpl_handler(CPL_GET_TCB_RPL, do_hwtid_rpl);
	t3_register_cpl_handler(CPL_SET_TCB_RPL, do_hwtid_rpl);

	/* Register to offloading devices */
	cxgb_register_client(&t3c_tom_client);
	
	return (0);
}

static int
t3_tom_load(module_t mod, int cmd, void *arg)
{
	int err = 0;

	switch (cmd) {
	case MOD_LOAD:
		t3_tom_init();
		break;
	case MOD_QUIESCE:
		break;
	case MOD_UNLOAD:
		printf("uhm, ... unloading isn't really supported for toe\n");
		break;
	case MOD_SHUTDOWN:
		break;
	default:
		err = EOPNOTSUPP;
		break;
	}

	return (err);
}

static moduledata_t mod_data= {
	"t3_tom",
	t3_tom_load,
	0
};
MODULE_VERSION(t3_tom, 1);
MODULE_DEPEND(t3_tom, toecore, 1, 1, 1);
MODULE_DEPEND(t3_tom, if_cxgb, 1, 1, 1);
DECLARE_MODULE(t3_tom, mod_data, SI_SUB_EXEC, SI_ORDER_ANY);

