/*-
 * Copyright (C) 2013 Intel Corporation
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/bpf.h>
#include <net/ethernet.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/bus.h>
#include <machine/cpufunc.h>
#include <machine/pmap.h>

#include "../ntb_hw/ntb_hw.h"

/*
 * The Non-Transparent Bridge (NTB) is a device on some Intel processors that
 * allows you to connect two systems using a PCI-e link.
 *
 * This module contains a protocol for sending and receiving messages, and
 * exposes that protocol through a simulated ethernet device called ntb.
 *
 * NOTE: Much of the code in this module is shared with Linux. Any patches may
 * be picked up and redistributed in Linux with a dual GPL/BSD license.
 */

/* TODO: These functions should really be part of the kernel */
#define test_bit(pos, bitmap_addr)  (*(bitmap_addr) & 1UL << (pos))
#define set_bit(pos, bitmap_addr)   *(bitmap_addr) |= 1UL << (pos)
#define clear_bit(pos, bitmap_addr) *(bitmap_addr) &= ~(1UL << (pos))

#define KTR_NTB KTR_SPARE3

#define NTB_TRANSPORT_VERSION	3
#define NTB_RX_MAX_PKTS		64
#define	NTB_RXQ_SIZE		300

static unsigned int transport_mtu = 0x4000 + ETHER_HDR_LEN + ETHER_CRC_LEN;

/*
 * This is an oversimplification to work around Xeon Errata.  The second client
 * may be usable for unidirectional traffic.
 */
static unsigned int max_num_clients = 1;

STAILQ_HEAD(ntb_queue_list, ntb_queue_entry);

struct ntb_queue_entry {
	/* ntb_queue list reference */
	STAILQ_ENTRY(ntb_queue_entry) entry;

	/* info on data to be transfered */
	void		*cb_data;
	void		*buf;
	uint64_t	len;
	uint64_t	flags;
};

struct ntb_rx_info {
	unsigned int entry;
};

struct ntb_transport_qp {
	struct ntb_netdev	*transport;
	struct ntb_softc	*ntb;

	void			*cb_data;

	bool			client_ready;
	bool			qp_link;
	uint8_t			qp_num;	/* Only 64 QPs are allowed.  0-63 */

	struct ntb_rx_info	*rx_info;
	struct ntb_rx_info	*remote_rx_info;

	void (*tx_handler) (struct ntb_transport_qp *qp, void *qp_data,
	    void *data, int len);
	struct ntb_queue_list	tx_free_q;
	struct mtx		ntb_tx_free_q_lock;
	void			*tx_mw;
	uint64_t		tx_index;
	uint64_t		tx_max_entry;
	uint64_t		tx_max_frame;

	void (*rx_handler) (struct ntb_transport_qp *qp, void *qp_data,
	    void *data, int len);
	struct ntb_queue_list	rx_pend_q;
	struct ntb_queue_list	rx_free_q;
	struct mtx		ntb_rx_pend_q_lock;
	struct mtx		ntb_rx_free_q_lock;
	struct task		rx_completion_task;
	void			*rx_buff;
	uint64_t		rx_index;
	uint64_t		rx_max_entry;
	uint64_t		rx_max_frame;

	void (*event_handler) (void *data, int status);
	struct callout		link_work;
	struct callout		queue_full;
	struct callout		rx_full;

	uint64_t		last_rx_no_buf;

	/* Stats */
	uint64_t		rx_bytes;
	uint64_t		rx_pkts;
	uint64_t		rx_ring_empty;
	uint64_t		rx_err_no_buf;
	uint64_t		rx_err_oflow;
	uint64_t		rx_err_ver;
	uint64_t		tx_bytes;
	uint64_t		tx_pkts;
	uint64_t		tx_ring_full;
};

struct ntb_queue_handlers {
	void (*rx_handler) (struct ntb_transport_qp *qp, void *qp_data,
	    void *data, int len);
	void (*tx_handler) (struct ntb_transport_qp *qp, void *qp_data,
	    void *data, int len);
	void (*event_handler) (void *data, int status);
};


struct ntb_transport_mw {
	size_t		size;
	void		*virt_addr;
	vm_paddr_t	dma_addr;
};

struct ntb_netdev {
	struct ntb_softc	*ntb;
	struct ifnet		*ifp;
	struct ntb_transport_mw	mw[NTB_NUM_MW];
	struct ntb_transport_qp	*qps;
	uint64_t		max_qps;
	uint64_t		qp_bitmap;
	bool			transport_link;
	struct callout		link_work;
	struct ntb_transport_qp *qp;
	uint64_t		bufsize;
	u_char			eaddr[ETHER_ADDR_LEN];
	struct mtx		tx_lock;
	struct mtx		rx_lock;
};

static struct ntb_netdev net_softc;

enum {
	IF_NTB_DESC_DONE_FLAG = 1 << 0,
	IF_NTB_LINK_DOWN_FLAG = 1 << 1,
};

struct ntb_payload_header {
	uint64_t ver;
	uint64_t len;
	uint64_t flags;
};

enum {
	/*
	 * The order of this enum is part of the if_ntb remote protocol.  Do
	 * not reorder without bumping protocol version (and it's probably best
	 * to keep the protocol in lock-step with the Linux NTB driver.
	 */
	IF_NTB_VERSION = 0,
	IF_NTB_QP_LINKS,
	IF_NTB_NUM_QPS,
	IF_NTB_NUM_MWS,
	/*
	 * N.B.: transport_link_work assumes MW1 enums = MW0 + 2.
	 */
	IF_NTB_MW0_SZ_HIGH,
	IF_NTB_MW0_SZ_LOW,
	IF_NTB_MW1_SZ_HIGH,
	IF_NTB_MW1_SZ_LOW,
	IF_NTB_MAX_SPAD,
};

#define QP_TO_MW(qp)		((qp) % NTB_NUM_MW)
#define NTB_QP_DEF_NUM_ENTRIES	100
#define NTB_LINK_DOWN_TIMEOUT	10

static int ntb_handle_module_events(struct module *m, int what, void *arg);
static int ntb_setup_interface(void);
static int ntb_teardown_interface(void);
static void ntb_net_init(void *arg);
static int ntb_ioctl(struct ifnet *ifp, u_long command, caddr_t data);
static void ntb_start(struct ifnet *ifp);
static void ntb_net_tx_handler(struct ntb_transport_qp *qp, void *qp_data,
    void *data, int len);
static void ntb_net_rx_handler(struct ntb_transport_qp *qp, void *qp_data,
    void *data, int len);
static void ntb_net_event_handler(void *data, int status);
static int ntb_transport_init(struct ntb_softc *ntb);
static void ntb_transport_free(void *transport);
static void ntb_transport_init_queue(struct ntb_netdev *nt,
    unsigned int qp_num);
static void ntb_transport_free_queue(struct ntb_transport_qp *qp);
static struct ntb_transport_qp * ntb_transport_create_queue(void *data,
    struct ntb_softc *pdev, const struct ntb_queue_handlers *handlers);
static void ntb_transport_link_up(struct ntb_transport_qp *qp);
static int ntb_transport_tx_enqueue(struct ntb_transport_qp *qp, void *cb,
    void *data, unsigned int len);
static int ntb_process_tx(struct ntb_transport_qp *qp,
    struct ntb_queue_entry *entry);
static void ntb_tx_copy_task(struct ntb_transport_qp *qp,
    struct ntb_queue_entry *entry, void *offset);
static void ntb_qp_full(void *arg);
static void ntb_transport_rxc_db(void *data, int db_num);
static void ntb_rx_pendq_full(void *arg);
static void ntb_transport_rx(struct ntb_transport_qp *qp);
static int ntb_process_rxc(struct ntb_transport_qp *qp);
static void ntb_rx_copy_task(struct ntb_transport_qp *qp,
    struct ntb_queue_entry *entry, void *offset);
static void ntb_rx_completion_task(void *arg, int pending);
static void ntb_transport_event_callback(void *data, enum ntb_hw_event event);
static void ntb_transport_link_work(void *arg);
static int ntb_set_mw(struct ntb_netdev *nt, int num_mw, unsigned int size);
static void ntb_free_mw(struct ntb_netdev *nt, int num_mw);
static void ntb_transport_setup_qp_mw(struct ntb_netdev *nt,
    unsigned int qp_num);
static void ntb_qp_link_work(void *arg);
static void ntb_transport_link_cleanup(struct ntb_netdev *nt);
static void ntb_qp_link_down(struct ntb_transport_qp *qp);
static void ntb_qp_link_cleanup(struct ntb_transport_qp *qp);
static void ntb_transport_link_down(struct ntb_transport_qp *qp);
static void ntb_send_link_down(struct ntb_transport_qp *qp);
static void ntb_list_add(struct mtx *lock, struct ntb_queue_entry *entry,
    struct ntb_queue_list *list);
static struct ntb_queue_entry *ntb_list_rm(struct mtx *lock,
    struct ntb_queue_list *list);
static void create_random_local_eui48(u_char *eaddr);
static unsigned int ntb_transport_max_size(struct ntb_transport_qp *qp);

MALLOC_DEFINE(M_NTB_IF, "if_ntb", "ntb network driver");

/* Module setup and teardown */
static int
ntb_handle_module_events(struct module *m, int what, void *arg)
{
	int err = 0;

	switch (what) {
	case MOD_LOAD:
		err = ntb_setup_interface();
		break;
	case MOD_UNLOAD:
		err = ntb_teardown_interface();
		break;
	default:
		err = EOPNOTSUPP;
		break;
	}
	return (err);
}

static moduledata_t if_ntb_mod = {
	"if_ntb",
	ntb_handle_module_events,
	NULL
};

DECLARE_MODULE(if_ntb, if_ntb_mod, SI_SUB_KLD, SI_ORDER_ANY);
MODULE_DEPEND(if_ntb, ntb_hw, 1, 1, 1);

static int
ntb_setup_interface(void)
{
	struct ifnet *ifp;
	struct ntb_queue_handlers handlers = { ntb_net_rx_handler,
	    ntb_net_tx_handler, ntb_net_event_handler };

	net_softc.ntb = devclass_get_softc(devclass_find("ntb_hw"), 0);
	if (net_softc.ntb == NULL) {
		printf("ntb: Cannot find devclass\n");
		return (ENXIO);
	}

	ntb_transport_init(net_softc.ntb);

	ifp = net_softc.ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		printf("ntb: cannot allocate ifnet structure\n");
		return (ENOMEM);
	}

	net_softc.qp = ntb_transport_create_queue(ifp, net_softc.ntb,
	    &handlers);
	if_initname(ifp, "ntb", 0);
	ifp->if_init = ntb_net_init;
	ifp->if_softc = &net_softc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX;
	ifp->if_ioctl = ntb_ioctl;
	ifp->if_start = ntb_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);
	create_random_local_eui48(net_softc.eaddr);
	ether_ifattach(ifp, net_softc.eaddr);
	ifp->if_capabilities = IFCAP_HWCSUM | IFCAP_JUMBO_MTU;
	ifp->if_capenable = ifp->if_capabilities;

	ntb_transport_link_up(net_softc.qp);
	net_softc.bufsize = ntb_transport_max_size(net_softc.qp) +
	    sizeof(struct ether_header);
	return (0);
}

static int
ntb_teardown_interface(void)
{

	if (net_softc.qp != NULL)
		ntb_transport_link_down(net_softc.qp);

	if (net_softc.ifp != NULL) {
		ether_ifdetach(net_softc.ifp);
		if_free(net_softc.ifp);
	}

	if (net_softc.qp != NULL) {
		ntb_transport_free_queue(net_softc.qp);
		ntb_transport_free(&net_softc);
	}

	return (0);
}

/* Network device interface */

static void
ntb_net_init(void *arg)
{
	struct ntb_netdev *ntb_softc = arg;
	struct ifnet *ifp = ntb_softc->ifp;

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_flags |= IFF_UP;
	if_link_state_change(ifp, LINK_STATE_UP);
}

static int
ntb_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct ntb_netdev *nt = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0;

	switch (command) {
	case SIOCSIFMTU:
	    {
		if (ifr->ifr_mtu > ntb_transport_max_size(nt->qp) -
		    ETHER_HDR_LEN - ETHER_CRC_LEN) {
			error = EINVAL;
			break;
		}

		ifp->if_mtu = ifr->ifr_mtu;
		break;
	    }
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}


static void
ntb_start(struct ifnet *ifp)
{
	struct mbuf *m_head;
	struct ntb_netdev *nt = ifp->if_softc;
	int rc;

	mtx_lock(&nt->tx_lock);
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	CTR0(KTR_NTB, "TX: ntb_start");
	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		CTR1(KTR_NTB, "TX: start mbuf %p", m_head);
		rc = ntb_transport_tx_enqueue(nt->qp, m_head, m_head,
			     m_length(m_head, NULL));
		if (rc != 0) {
			CTR1(KTR_NTB,
			    "TX: could not tx mbuf %p. Returning to snd q",
			    m_head);
			if (rc == EAGAIN) {
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
				IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
				callout_reset(&nt->qp->queue_full, hz / 1000,
				    ntb_qp_full, ifp);
			}
			break;
		}

	}
	mtx_unlock(&nt->tx_lock);
}

/* Network Device Callbacks */
static void
ntb_net_tx_handler(struct ntb_transport_qp *qp, void *qp_data, void *data,
    int len)
{

	m_freem(data);
	CTR1(KTR_NTB, "TX: tx_handler freeing mbuf %p", data);
}

static void
ntb_net_rx_handler(struct ntb_transport_qp *qp, void *qp_data, void *data,
    int len)
{
	struct mbuf *m = data;
	struct ifnet *ifp = qp_data;

	CTR0(KTR_NTB, "RX: rx handler");
	(*ifp->if_input)(ifp, m);
}

static void
ntb_net_event_handler(void *data, int status)
{

}

/* Transport Init and teardown */

static int
ntb_transport_init(struct ntb_softc *ntb)
{
	struct ntb_netdev *nt = &net_softc;
	int rc, i;

	nt->max_qps = max_num_clients;
	ntb_register_transport(ntb, nt);
	mtx_init(&nt->tx_lock, "ntb transport tx", NULL, MTX_DEF);
	mtx_init(&nt->rx_lock, "ntb transport rx", NULL, MTX_DEF);

	nt->qps = malloc(nt->max_qps * sizeof(struct ntb_transport_qp),
			  M_NTB_IF, M_WAITOK|M_ZERO);

	nt->qp_bitmap = ((uint64_t) 1 << nt->max_qps) - 1;

	for (i = 0; i < nt->max_qps; i++)
		ntb_transport_init_queue(nt, i);

	callout_init(&nt->link_work, 0);

	rc = ntb_register_event_callback(ntb,
					 ntb_transport_event_callback);
	if (rc != 0)
		goto err;

	if (ntb_query_link_status(ntb)) {
		if (bootverbose)
			device_printf(ntb_get_device(ntb), "link up\n");
		callout_reset(&nt->link_work, 0, ntb_transport_link_work, nt);
	}

	return (0);

err:
	free(nt->qps, M_NTB_IF);
	ntb_unregister_transport(ntb);
	return (rc);
}

static void
ntb_transport_free(void *transport)
{
	struct ntb_netdev *nt = transport;
	struct ntb_softc *ntb = nt->ntb;
	int i;

	nt->transport_link = NTB_LINK_DOWN;

	callout_drain(&nt->link_work);

	/* verify that all the qps are freed */
	for (i = 0; i < nt->max_qps; i++)
		if (!test_bit(i, &nt->qp_bitmap))
			ntb_transport_free_queue(&nt->qps[i]);

	ntb_unregister_event_callback(ntb);

	for (i = 0; i < NTB_NUM_MW; i++)
		ntb_free_mw(nt, i);

	free(nt->qps, M_NTB_IF);
	ntb_unregister_transport(ntb);
}

static void
ntb_transport_init_queue(struct ntb_netdev *nt, unsigned int qp_num)
{
	struct ntb_transport_qp *qp;
	unsigned int num_qps_mw, tx_size;
	uint8_t mw_num = QP_TO_MW(qp_num);

	qp = &nt->qps[qp_num];
	qp->qp_num = qp_num;
	qp->transport = nt;
	qp->ntb = nt->ntb;
	qp->qp_link = NTB_LINK_DOWN;
	qp->client_ready = NTB_LINK_DOWN;
	qp->event_handler = NULL;

	if (nt->max_qps % NTB_NUM_MW && mw_num < nt->max_qps % NTB_NUM_MW)
		num_qps_mw = nt->max_qps / NTB_NUM_MW + 1;
	else
		num_qps_mw = nt->max_qps / NTB_NUM_MW;

	tx_size = (unsigned int) ntb_get_mw_size(qp->ntb, mw_num) / num_qps_mw;
	qp->rx_info = (struct ntb_rx_info *)
	    ((char *)ntb_get_mw_vbase(qp->ntb, mw_num) +
	    (qp_num / NTB_NUM_MW * tx_size));
	tx_size -= sizeof(struct ntb_rx_info);

	qp->tx_mw = qp->rx_info + 1;
	/* Due to house-keeping, there must be at least 2 buffs */
	qp->tx_max_frame = min(transport_mtu + sizeof(struct ntb_payload_header),
	    tx_size / 2);
	qp->tx_max_entry = tx_size / qp->tx_max_frame;

	callout_init(&qp->link_work, 0);
	callout_init(&qp->queue_full, 1);
	callout_init(&qp->rx_full, 1);

	mtx_init(&qp->ntb_rx_pend_q_lock, "ntb rx pend q", NULL, MTX_SPIN);
	mtx_init(&qp->ntb_rx_free_q_lock, "ntb rx free q", NULL, MTX_SPIN);
	mtx_init(&qp->ntb_tx_free_q_lock, "ntb tx free q", NULL, MTX_SPIN);
	TASK_INIT(&qp->rx_completion_task, 0, ntb_rx_completion_task, qp);

	STAILQ_INIT(&qp->rx_pend_q);
	STAILQ_INIT(&qp->rx_free_q);
	STAILQ_INIT(&qp->tx_free_q);
}

static void
ntb_transport_free_queue(struct ntb_transport_qp *qp)
{
	struct ntb_queue_entry *entry;

	if (qp == NULL)
		return;

	callout_drain(&qp->link_work);

	ntb_unregister_db_callback(qp->ntb, qp->qp_num);

	while ((entry = ntb_list_rm(&qp->ntb_rx_free_q_lock, &qp->rx_free_q)))
		free(entry, M_NTB_IF);

	while ((entry = ntb_list_rm(&qp->ntb_rx_pend_q_lock, &qp->rx_pend_q)))
		free(entry, M_NTB_IF);

	while ((entry = ntb_list_rm(&qp->ntb_tx_free_q_lock, &qp->tx_free_q)))
		free(entry, M_NTB_IF);

	set_bit(qp->qp_num, &qp->transport->qp_bitmap);
}

/**
 * ntb_transport_create_queue - Create a new NTB transport layer queue
 * @rx_handler: receive callback function
 * @tx_handler: transmit callback function
 * @event_handler: event callback function
 *
 * Create a new NTB transport layer queue and provide the queue with a callback
 * routine for both transmit and receive.  The receive callback routine will be
 * used to pass up data when the transport has received it on the queue.   The
 * transmit callback routine will be called when the transport has completed the
 * transmission of the data on the queue and the data is ready to be freed.
 *
 * RETURNS: pointer to newly created ntb_queue, NULL on error.
 */
static struct ntb_transport_qp *
ntb_transport_create_queue(void *data, struct ntb_softc *pdev,
    const struct ntb_queue_handlers *handlers)
{
	struct ntb_queue_entry *entry;
	struct ntb_transport_qp *qp;
	struct ntb_netdev *nt;
	unsigned int free_queue;
	int rc, i;

	nt = ntb_find_transport(pdev);
	if (nt == NULL)
		goto err;

	free_queue = ffs(nt->qp_bitmap);
	if (free_queue == 0)
		goto err;

	/* decrement free_queue to make it zero based */
	free_queue--;

	clear_bit(free_queue, &nt->qp_bitmap);

	qp = &nt->qps[free_queue];
	qp->cb_data = data;
	qp->rx_handler = handlers->rx_handler;
	qp->tx_handler = handlers->tx_handler;
	qp->event_handler = handlers->event_handler;

	for (i = 0; i < NTB_QP_DEF_NUM_ENTRIES; i++) {
		entry = malloc(sizeof(struct ntb_queue_entry), M_NTB_IF,
		    M_WAITOK|M_ZERO);
		entry->cb_data = nt->ifp;
		entry->buf = NULL;
		entry->len = transport_mtu;
		ntb_list_add(&qp->ntb_rx_pend_q_lock, entry, &qp->rx_pend_q);
	}

	for (i = 0; i < NTB_QP_DEF_NUM_ENTRIES; i++) {
		entry = malloc(sizeof(struct ntb_queue_entry), M_NTB_IF,
		    M_WAITOK|M_ZERO);
		ntb_list_add(&qp->ntb_tx_free_q_lock, entry, &qp->tx_free_q);
	}

	rc = ntb_register_db_callback(qp->ntb, free_queue, qp,
				      ntb_transport_rxc_db);
	if (rc != 0)
		goto err1;

	return (qp);

err1:
	while ((entry = ntb_list_rm(&qp->ntb_tx_free_q_lock, &qp->tx_free_q)))
		free(entry, M_NTB_IF);
	while ((entry = ntb_list_rm(&qp->ntb_rx_free_q_lock, &qp->rx_free_q)))
		free(entry, M_NTB_IF);
	set_bit(free_queue, &nt->qp_bitmap);
err:
	return (NULL);
}

/**
 * ntb_transport_link_up - Notify NTB transport of client readiness to use queue
 * @qp: NTB transport layer queue to be enabled
 *
 * Notify NTB transport layer of client readiness to use queue
 */
static void
ntb_transport_link_up(struct ntb_transport_qp *qp)
{

	if (qp == NULL)
		return;

	qp->client_ready = NTB_LINK_UP;
	if (bootverbose)
		device_printf(ntb_get_device(qp->ntb), "qp client ready\n");

	if (qp->transport->transport_link == NTB_LINK_UP)
		callout_reset(&qp->link_work, 0, ntb_qp_link_work, qp);
}



/* Transport Tx */

/**
 * ntb_transport_tx_enqueue - Enqueue a new NTB queue entry
 * @qp: NTB transport layer queue the entry is to be enqueued on
 * @cb: per buffer pointer for callback function to use
 * @data: pointer to data buffer that will be sent
 * @len: length of the data buffer
 *
 * Enqueue a new transmit buffer onto the transport queue from which a NTB
 * payload will be transmitted.  This assumes that a lock is behing held to
 * serialize access to the qp.
 *
 * RETURNS: An appropriate ERRNO error value on error, or zero for success.
 */
static int
ntb_transport_tx_enqueue(struct ntb_transport_qp *qp, void *cb, void *data,
    unsigned int len)
{
	struct ntb_queue_entry *entry;
	int rc;

	if (qp == NULL || qp->qp_link != NTB_LINK_UP || len == 0) {
		CTR0(KTR_NTB, "TX: link not up");
		return (EINVAL);
	}

	entry = ntb_list_rm(&qp->ntb_tx_free_q_lock, &qp->tx_free_q);
	if (entry == NULL) {
		CTR0(KTR_NTB, "TX: could not get entry from tx_free_q");
		return (ENOMEM);
	}
	CTR1(KTR_NTB, "TX: got entry %p from tx_free_q", entry);

	entry->cb_data = cb;
	entry->buf = data;
	entry->len = len;
	entry->flags = 0;

	rc = ntb_process_tx(qp, entry);
	if (rc != 0) {
		ntb_list_add(&qp->ntb_tx_free_q_lock, entry, &qp->tx_free_q);
		CTR1(KTR_NTB,
		    "TX: process_tx failed. Returning entry %p to tx_free_q",
		    entry);
	}
	return (rc);
}

static int
ntb_process_tx(struct ntb_transport_qp *qp, struct ntb_queue_entry *entry)
{
	void *offset;

	offset = (char *)qp->tx_mw + qp->tx_max_frame * qp->tx_index;
	CTR3(KTR_NTB,
	    "TX: process_tx: tx_pkts=%u, tx_index=%u, remote entry=%u",
	    qp->tx_pkts, qp->tx_index, qp->remote_rx_info->entry);
	if (qp->tx_index == qp->remote_rx_info->entry) {
		CTR0(KTR_NTB, "TX: ring full");
		qp->tx_ring_full++;
		return (EAGAIN);
	}

	if (entry->len > qp->tx_max_frame - sizeof(struct ntb_payload_header)) {
		if (qp->tx_handler != NULL)
			qp->tx_handler(qp, qp->cb_data, entry->buf,
				       EIO);

		ntb_list_add(&qp->ntb_tx_free_q_lock, entry, &qp->tx_free_q);
		CTR1(KTR_NTB,
		    "TX: frame too big. returning entry %p to tx_free_q",
		    entry);
		return (0);
	}
	CTR2(KTR_NTB, "TX: copying entry %p to offset %p", entry, offset);
	ntb_tx_copy_task(qp, entry, offset);

	qp->tx_index++;
	qp->tx_index %= qp->tx_max_entry;

	qp->tx_pkts++;

	return (0);
}

static void
ntb_tx_copy_task(struct ntb_transport_qp *qp, struct ntb_queue_entry *entry,
    void *offset)
{
	struct ntb_payload_header *hdr;

	CTR2(KTR_NTB, "TX: copying %d bytes to offset %p", entry->len, offset);
	if (entry->buf != NULL)
		m_copydata((struct mbuf *)entry->buf, 0, entry->len, offset);

	hdr = (struct ntb_payload_header *)((char *)offset + qp->tx_max_frame -
	    sizeof(struct ntb_payload_header));
	hdr->len = entry->len; /* TODO: replace with bus_space_write */
	hdr->ver = qp->tx_pkts; /* TODO: replace with bus_space_write */
	wmb();
	/* TODO: replace with bus_space_write */
	hdr->flags = entry->flags | IF_NTB_DESC_DONE_FLAG;

	ntb_ring_doorbell(qp->ntb, qp->qp_num);

	/* 
	 * The entry length can only be zero if the packet is intended to be a
	 * "link down" or similar.  Since no payload is being sent in these
	 * cases, there is nothing to add to the completion queue.
	 */
	if (entry->len > 0) {
		qp->tx_bytes += entry->len;

		if (qp->tx_handler)
			qp->tx_handler(qp, qp->cb_data, entry->cb_data,
				       entry->len);
	}

	CTR2(KTR_NTB,
	    "TX: entry %p sent. hdr->ver = %d, Returning to tx_free_q", entry,
	    hdr->ver);
	ntb_list_add(&qp->ntb_tx_free_q_lock, entry, &qp->tx_free_q);
}

static void
ntb_qp_full(void *arg)
{

	CTR0(KTR_NTB, "TX: qp_full callout");
	ntb_start(arg);
}

/* Transport Rx */
static void
ntb_transport_rxc_db(void *data, int db_num)
{
	struct ntb_transport_qp *qp = data;

	ntb_transport_rx(qp);
}

static void
ntb_rx_pendq_full(void *arg)
{

	CTR0(KTR_NTB, "RX: ntb_rx_pendq_full callout");
	ntb_transport_rx(arg);
}

static void
ntb_transport_rx(struct ntb_transport_qp *qp)
{
	uint64_t i;
	int rc;

	/* 
	 * Limit the number of packets processed in a single interrupt to
	 * provide fairness to others
	 */
	mtx_lock(&qp->transport->rx_lock);
	CTR0(KTR_NTB, "RX: transport_rx");
	for (i = 0; i < qp->rx_max_entry; i++) {
		rc = ntb_process_rxc(qp);
		if (rc != 0) {
			CTR0(KTR_NTB, "RX: process_rxc failed");
			break;
		}
	}
	mtx_unlock(&qp->transport->rx_lock);
}

static int
ntb_process_rxc(struct ntb_transport_qp *qp)
{
	struct ntb_payload_header *hdr;
	struct ntb_queue_entry *entry;
	void *offset;

	offset = (void *)
	    ((char *)qp->rx_buff + qp->rx_max_frame * qp->rx_index);
	hdr = (void *)
	    ((char *)offset + qp->rx_max_frame -
		sizeof(struct ntb_payload_header));

	CTR1(KTR_NTB, "RX: process_rxc rx_index = %u", qp->rx_index);
	entry = ntb_list_rm(&qp->ntb_rx_pend_q_lock, &qp->rx_pend_q);
	if (entry == NULL) {
		qp->rx_err_no_buf++;
		CTR0(KTR_NTB, "RX: No entries in rx_pend_q");
		return (ENOMEM);
	}
	callout_stop(&qp->rx_full);
	CTR1(KTR_NTB, "RX: rx entry %p from rx_pend_q", entry);

	if ((hdr->flags & IF_NTB_DESC_DONE_FLAG) == 0) {
		CTR1(KTR_NTB,
		    "RX: hdr not done. Returning entry %p to rx_pend_q", entry);
		ntb_list_add(&qp->ntb_rx_pend_q_lock, entry, &qp->rx_pend_q);
		qp->rx_ring_empty++;
		return (EAGAIN);
	}

	if (hdr->ver != (uint32_t) qp->rx_pkts) {
		CTR3(KTR_NTB,"RX: ver != rx_pkts (%x != %lx). "
		    "Returning entry %p to rx_pend_q", hdr->ver, qp->rx_pkts,
		    entry);
		ntb_list_add(&qp->ntb_rx_pend_q_lock, entry, &qp->rx_pend_q);
		qp->rx_err_ver++;
		return (EIO);
	}

	if ((hdr->flags & IF_NTB_LINK_DOWN_FLAG) != 0) {
		ntb_qp_link_down(qp);
		CTR1(KTR_NTB,
		    "RX: link down. adding entry %p back to rx_pend_q", entry);
		ntb_list_add(&qp->ntb_rx_pend_q_lock, entry, &qp->rx_pend_q);
		goto out;
	}

	if (hdr->len <= entry->len) {
		entry->len = hdr->len;
		ntb_rx_copy_task(qp, entry, offset);
	} else {
		CTR1(KTR_NTB,
		    "RX: len too long. Returning entry %p to rx_pend_q", entry);
		ntb_list_add(&qp->ntb_rx_pend_q_lock, entry, &qp->rx_pend_q);

		qp->rx_err_oflow++;
	}

	qp->rx_bytes += hdr->len;
	qp->rx_pkts++;
	CTR1(KTR_NTB, "RX: received %ld rx_pkts", qp->rx_pkts);


out:
	/* Ensure that the data is globally visible before clearing the flag */
	wmb();
	hdr->flags = 0;
	/* TODO: replace with bus_space_write */
	qp->rx_info->entry = qp->rx_index;

	qp->rx_index++;
	qp->rx_index %= qp->rx_max_entry;

	return (0);
}

static void
ntb_rx_copy_task(struct ntb_transport_qp *qp, struct ntb_queue_entry *entry,
    void *offset)
{
	struct ifnet *ifp = entry->cb_data;
	unsigned int len = entry->len;
	struct mbuf *m;

	CTR2(KTR_NTB, "RX: copying %d bytes from offset %p", len, offset);
	m = m_devget(offset, len, 0, ifp, NULL);
	m->m_pkthdr.csum_flags = CSUM_IP_CHECKED | CSUM_IP_VALID;

	entry->buf = (void *)m;

	CTR2(KTR_NTB,
	    "RX: copied entry %p to mbuf %p. Adding entry to rx_free_q", entry,
	    m);
	ntb_list_add(&qp->ntb_rx_free_q_lock, entry, &qp->rx_free_q);

	taskqueue_enqueue(taskqueue_swi, &qp->rx_completion_task);
}

static void
ntb_rx_completion_task(void *arg, int pending)
{
	struct ntb_transport_qp *qp = arg;
	struct mbuf *m;
	struct ntb_queue_entry *entry;

	CTR0(KTR_NTB, "RX: rx_completion_task");

	while ((entry = ntb_list_rm(&qp->ntb_rx_free_q_lock, &qp->rx_free_q))) {
		m = entry->buf;
		CTR2(KTR_NTB, "RX: completing entry %p, mbuf %p", entry, m);
		if (qp->rx_handler && qp->client_ready == NTB_LINK_UP)
			qp->rx_handler(qp, qp->cb_data, m, entry->len);

		entry->buf = NULL;
		entry->len = qp->transport->bufsize;

		CTR1(KTR_NTB,"RX: entry %p removed from rx_free_q "
		    "and added to rx_pend_q", entry);
		ntb_list_add(&qp->ntb_rx_pend_q_lock, entry, &qp->rx_pend_q);
		if (qp->rx_err_no_buf > qp->last_rx_no_buf) {
			qp->last_rx_no_buf = qp->rx_err_no_buf;
			CTR0(KTR_NTB, "RX: could spawn rx task");
			callout_reset(&qp->rx_full, hz / 1000, ntb_rx_pendq_full,
			    qp);
		}
	}
}

/* Link Event handler */
static void
ntb_transport_event_callback(void *data, enum ntb_hw_event event)
{
	struct ntb_netdev *nt = data;

	switch (event) {
	case NTB_EVENT_HW_LINK_UP:
		if (bootverbose)
			device_printf(ntb_get_device(nt->ntb), "HW link up\n");
		callout_reset(&nt->link_work, 0, ntb_transport_link_work, nt);
		break;
	case NTB_EVENT_HW_LINK_DOWN:
		if (bootverbose)
			device_printf(ntb_get_device(nt->ntb), "HW link down\n");
		ntb_transport_link_cleanup(nt);
		break;
	default:
		panic("ntb: Unknown NTB event");
	}
}

/* Link bring up */
static void
ntb_transport_link_work(void *arg)
{
	struct ntb_netdev *nt = arg;
	struct ntb_softc *ntb = nt->ntb;
	struct ntb_transport_qp *qp;
	uint64_t val64;
	uint32_t val, i, num_mw;
	int rc;

	if (ntb_has_feature(ntb, NTB_REGS_THRU_MW))
		num_mw = NTB_NUM_MW - 1;
	else
		num_mw = NTB_NUM_MW;

	/* send the local info, in the opposite order of the way we read it */
	for (i = 0; i < num_mw; i++) {
		rc = ntb_write_remote_spad(ntb, IF_NTB_MW0_SZ_HIGH + (i * 2),
		    ntb_get_mw_size(ntb, i) >> 32);
		if (rc != 0)
			goto out;

		rc = ntb_write_remote_spad(ntb, IF_NTB_MW0_SZ_LOW + (i * 2),
		    (uint32_t)ntb_get_mw_size(ntb, i));
		if (rc != 0)
			goto out;
	}

	rc = ntb_write_remote_spad(ntb, IF_NTB_NUM_MWS, num_mw);
	if (rc != 0)
		goto out;

	rc = ntb_write_remote_spad(ntb, IF_NTB_NUM_QPS, nt->max_qps);
	if (rc != 0)
		goto out;

	rc = ntb_write_remote_spad(ntb, IF_NTB_VERSION, NTB_TRANSPORT_VERSION);
	if (rc != 0)
		goto out;

	/* Query the remote side for its info */
	rc = ntb_read_local_spad(ntb, IF_NTB_VERSION, &val);
	if (rc != 0)
		goto out;

	if (val != NTB_TRANSPORT_VERSION)
		goto out;

	rc = ntb_read_local_spad(ntb, IF_NTB_NUM_QPS, &val);
	if (rc != 0)
		goto out;

	if (val != nt->max_qps)
		goto out;

	rc = ntb_read_local_spad(ntb, IF_NTB_NUM_MWS, &val);
	if (rc != 0)
		goto out;

	if (val != num_mw)
		goto out;

	for (i = 0; i < num_mw; i++) {
		rc = ntb_read_local_spad(ntb, IF_NTB_MW0_SZ_HIGH + (i * 2),
		    &val);
		if (rc != 0)
			goto free_mws;

		val64 = (uint64_t)val << 32;

		rc = ntb_read_local_spad(ntb, IF_NTB_MW0_SZ_LOW + (i * 2),
		    &val);
		if (rc != 0)
			goto free_mws;

		val64 |= val;

		rc = ntb_set_mw(nt, i, val64);
		if (rc != 0)
			goto free_mws;
	}

	nt->transport_link = NTB_LINK_UP;
	if (bootverbose)
		device_printf(ntb_get_device(ntb), "transport link up\n");

	for (i = 0; i < nt->max_qps; i++) {
		qp = &nt->qps[i];

		ntb_transport_setup_qp_mw(nt, i);

		if (qp->client_ready == NTB_LINK_UP)
			callout_reset(&qp->link_work, 0, ntb_qp_link_work, qp);
	}

	return;

free_mws:
	for (i = 0; i < NTB_NUM_MW; i++)
		ntb_free_mw(nt, i);
out:
	if (ntb_query_link_status(ntb))
		callout_reset(&nt->link_work,
		    NTB_LINK_DOWN_TIMEOUT * hz / 1000, ntb_transport_link_work, nt);
}

static int
ntb_set_mw(struct ntb_netdev *nt, int num_mw, unsigned int size)
{
	struct ntb_transport_mw *mw = &nt->mw[num_mw];

	/* No need to re-setup */
	if (mw->size == size)
		return (0);

	if (mw->size != 0)
		ntb_free_mw(nt, num_mw);

	/* Alloc memory for receiving data.  Must be 4k aligned */
	mw->size = size;

	mw->virt_addr = contigmalloc(mw->size, M_NTB_IF, M_ZERO, 0,
	    BUS_SPACE_MAXADDR, mw->size, 0);
	if (mw->virt_addr == NULL) {
		mw->size = 0;
		printf("ntb: Unable to allocate MW buffer of size %d\n",
		    (int)mw->size);
		return (ENOMEM);
	}
	/* TODO: replace with bus_space_* functions */
	mw->dma_addr = vtophys(mw->virt_addr);

	/* Notify HW the memory location of the receive buffer */
	ntb_set_mw_addr(nt->ntb, num_mw, mw->dma_addr);

	return (0);
}

static void
ntb_free_mw(struct ntb_netdev *nt, int num_mw)
{
	struct ntb_transport_mw *mw = &nt->mw[num_mw];

	if (mw->virt_addr == NULL)
		return;

	contigfree(mw->virt_addr, mw->size, M_NTB_IF);
	mw->virt_addr = NULL;
}

static void
ntb_transport_setup_qp_mw(struct ntb_netdev *nt, unsigned int qp_num)
{
	struct ntb_transport_qp *qp = &nt->qps[qp_num];
	void *offset;
	unsigned int rx_size, num_qps_mw;
	uint8_t mw_num = QP_TO_MW(qp_num);
	unsigned int i;

	if (nt->max_qps % NTB_NUM_MW && mw_num < nt->max_qps % NTB_NUM_MW)
		num_qps_mw = nt->max_qps / NTB_NUM_MW + 1;
	else
		num_qps_mw = nt->max_qps / NTB_NUM_MW;

	rx_size = (unsigned int) nt->mw[mw_num].size / num_qps_mw;
	qp->remote_rx_info = (void *)((uint8_t *)nt->mw[mw_num].virt_addr +
			     (qp_num / NTB_NUM_MW * rx_size));
	rx_size -= sizeof(struct ntb_rx_info);

	qp->rx_buff = qp->remote_rx_info + 1;
	/* Due to house-keeping, there must be at least 2 buffs */
	qp->rx_max_frame = min(transport_mtu + sizeof(struct ntb_payload_header),
	    rx_size / 2);
	qp->rx_max_entry = rx_size / qp->rx_max_frame;
	qp->rx_index = 0;

	qp->remote_rx_info->entry = qp->rx_max_entry - 1;

	/* setup the hdr offsets with 0's */
	for (i = 0; i < qp->rx_max_entry; i++) {
		offset = (void *)((uint8_t *)qp->rx_buff +
		    qp->rx_max_frame * (i + 1) -
		    sizeof(struct ntb_payload_header));
		memset(offset, 0, sizeof(struct ntb_payload_header));
	}

	qp->rx_pkts = 0;
	qp->tx_pkts = 0;
	qp->tx_index = 0;
}

static void
ntb_qp_link_work(void *arg)
{
	struct ntb_transport_qp *qp = arg;
	struct ntb_softc *ntb = qp->ntb;
	struct ntb_netdev *nt = qp->transport;
	int rc, val;


	rc = ntb_read_remote_spad(ntb, IF_NTB_QP_LINKS, &val);
	if (rc != 0)
		return;

	rc = ntb_write_remote_spad(ntb, IF_NTB_QP_LINKS, val | 1 << qp->qp_num);

	/* query remote spad for qp ready bits */
	rc = ntb_read_local_spad(ntb, IF_NTB_QP_LINKS, &val);

	/* See if the remote side is up */
	if ((1 << qp->qp_num & val) != 0) {
		qp->qp_link = NTB_LINK_UP;
		if (qp->event_handler != NULL)
			qp->event_handler(qp->cb_data, NTB_LINK_UP);
		if (bootverbose)
			device_printf(ntb_get_device(ntb), "qp link up\n");
	} else if (nt->transport_link == NTB_LINK_UP) {
		callout_reset(&qp->link_work,
		    NTB_LINK_DOWN_TIMEOUT * hz / 1000, ntb_qp_link_work, qp);
	}
}

/* Link down event*/
static void
ntb_transport_link_cleanup(struct ntb_netdev *nt)
{
	int i;

	if (nt->transport_link == NTB_LINK_DOWN)
		callout_drain(&nt->link_work);
	else
		nt->transport_link = NTB_LINK_DOWN;

	/* Pass along the info to any clients */
	for (i = 0; i < nt->max_qps; i++)
		if (!test_bit(i, &nt->qp_bitmap))
			ntb_qp_link_down(&nt->qps[i]);

	/* 
	 * The scratchpad registers keep the values if the remote side
	 * goes down, blast them now to give them a sane value the next
	 * time they are accessed
	 */
	for (i = 0; i < IF_NTB_MAX_SPAD; i++)
		ntb_write_local_spad(nt->ntb, i, 0);
}


static void
ntb_qp_link_down(struct ntb_transport_qp *qp)
{

	ntb_qp_link_cleanup(qp);
}

static void
ntb_qp_link_cleanup(struct ntb_transport_qp *qp)
{
	struct ntb_netdev *nt = qp->transport;

	if (qp->qp_link == NTB_LINK_DOWN) {
		callout_drain(&qp->link_work);
		return;
	}

	if (qp->event_handler != NULL)
		qp->event_handler(qp->cb_data, NTB_LINK_DOWN);

	qp->qp_link = NTB_LINK_DOWN;

	if (nt->transport_link == NTB_LINK_UP)
		callout_reset(&qp->link_work,
		    NTB_LINK_DOWN_TIMEOUT * hz / 1000, ntb_qp_link_work, qp);
}

/* Link commanded down */
/**
 * ntb_transport_link_down - Notify NTB transport to no longer enqueue data
 * @qp: NTB transport layer queue to be disabled
 *
 * Notify NTB transport layer of client's desire to no longer receive data on
 * transport queue specified.  It is the client's responsibility to ensure all
 * entries on queue are purged or otherwise handled appropraitely.
 */
static void
ntb_transport_link_down(struct ntb_transport_qp *qp)
{
	int rc, val;

	if (qp == NULL)
		return;

	qp->client_ready = NTB_LINK_DOWN;

	rc = ntb_read_remote_spad(qp->ntb, IF_NTB_QP_LINKS, &val);
	if (rc != 0)
		return;

	rc = ntb_write_remote_spad(qp->ntb, IF_NTB_QP_LINKS,
	   val & ~(1 << qp->qp_num));

	if (qp->qp_link == NTB_LINK_UP)
		ntb_send_link_down(qp);
	else
		callout_drain(&qp->link_work);

}

static void
ntb_send_link_down(struct ntb_transport_qp *qp)
{
	struct ntb_queue_entry *entry;
	int i, rc;

	if (qp->qp_link == NTB_LINK_DOWN)
		return;

	qp->qp_link = NTB_LINK_DOWN;

	for (i = 0; i < NTB_LINK_DOWN_TIMEOUT; i++) {
		entry = ntb_list_rm(&qp->ntb_tx_free_q_lock, &qp->tx_free_q);
		if (entry != NULL)
			break;
		pause("NTB Wait for link down", hz / 10);
	}

	if (entry == NULL)
		return;

	entry->cb_data = NULL;
	entry->buf = NULL;
	entry->len = 0;
	entry->flags = IF_NTB_LINK_DOWN_FLAG;

	mtx_lock(&qp->transport->tx_lock);
	rc = ntb_process_tx(qp, entry);
	if (rc != 0)
		printf("ntb: Failed to send link down\n");
	mtx_unlock(&qp->transport->tx_lock);
}


/* List Management */

static void
ntb_list_add(struct mtx *lock, struct ntb_queue_entry *entry,
    struct ntb_queue_list *list)
{

	mtx_lock_spin(lock);
	STAILQ_INSERT_TAIL(list, entry, entry);
	mtx_unlock_spin(lock);
}

static struct ntb_queue_entry *
ntb_list_rm(struct mtx *lock, struct ntb_queue_list *list)
{
	struct ntb_queue_entry *entry;

	mtx_lock_spin(lock);
	if (STAILQ_EMPTY(list)) {
		entry = NULL;
		goto out;
	}
	entry = STAILQ_FIRST(list);
	STAILQ_REMOVE_HEAD(list, entry);
out:
	mtx_unlock_spin(lock);

	return (entry);
}

/* Helper functions */
/* TODO: This too should really be part of the kernel */
#define EUI48_MULTICAST			1 << 0
#define EUI48_LOCALLY_ADMINISTERED	1 << 1
static void
create_random_local_eui48(u_char *eaddr)
{
	static uint8_t counter = 0;
	uint32_t seed = ticks;

	eaddr[0] = EUI48_LOCALLY_ADMINISTERED;
	memcpy(&eaddr[1], &seed, sizeof(uint32_t));
	eaddr[5] = counter++;
}

/**
 * ntb_transport_max_size - Query the max payload size of a qp
 * @qp: NTB transport layer queue to be queried
 *
 * Query the maximum payload size permissible on the given qp
 *
 * RETURNS: the max payload size of a qp
 */
static unsigned int
ntb_transport_max_size(struct ntb_transport_qp *qp)
{

	if (qp == NULL)
		return (0);

	return (qp->tx_max_frame - sizeof(struct ntb_payload_header));
}
