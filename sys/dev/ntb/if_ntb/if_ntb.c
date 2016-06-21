/*-
 * Copyright (C) 2013 Intel Corporation
 * Copyright (C) 2015 EMC Corporation
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
#include <sys/bitset.h>
#include <sys/bus.h>
#include <sys/ktr.h>
#include <sys/limits.h>
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

#include <netinet/in.h>
#include <netinet/ip.h>

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

#define QP_SETSIZE	64
BITSET_DEFINE(_qpset, QP_SETSIZE);
#define test_bit(pos, addr)	BIT_ISSET(QP_SETSIZE, (pos), (addr))
#define set_bit(pos, addr)	BIT_SET(QP_SETSIZE, (pos), (addr))
#define clear_bit(pos, addr)	BIT_CLR(QP_SETSIZE, (pos), (addr))
#define ffs_bit(addr)		BIT_FFS(QP_SETSIZE, (addr))

#define KTR_NTB KTR_SPARE3

#define NTB_TRANSPORT_VERSION	4
#define NTB_RX_MAX_PKTS		64
#define	NTB_RXQ_SIZE		300

enum ntb_link_event {
	NTB_LINK_DOWN = 0,
	NTB_LINK_UP,
};

static SYSCTL_NODE(_hw, OID_AUTO, if_ntb, CTLFLAG_RW, 0, "if_ntb");

static unsigned g_if_ntb_debug_level;
SYSCTL_UINT(_hw_if_ntb, OID_AUTO, debug_level, CTLFLAG_RWTUN,
    &g_if_ntb_debug_level, 0, "if_ntb log level -- higher is more verbose");
#define ntb_printf(lvl, ...) do {			\
	if ((lvl) <= g_if_ntb_debug_level) {		\
		if_printf(nt->ifp, __VA_ARGS__);	\
	}						\
} while (0)

static unsigned transport_mtu = IP_MAXPACKET + ETHER_HDR_LEN + ETHER_CRC_LEN;

static uint64_t max_mw_size;
SYSCTL_UQUAD(_hw_if_ntb, OID_AUTO, max_mw_size, CTLFLAG_RDTUN, &max_mw_size, 0,
    "If enabled (non-zero), limit the size of large memory windows. "
    "Both sides of the NTB MUST set the same value here.");

static unsigned max_num_clients;
SYSCTL_UINT(_hw_if_ntb, OID_AUTO, max_num_clients, CTLFLAG_RDTUN,
    &max_num_clients, 0, "Maximum number of NTB transport clients.  "
    "0 (default) - use all available NTB memory windows; "
    "positive integer N - Limit to N memory windows.");

static unsigned enable_xeon_watchdog;
SYSCTL_UINT(_hw_if_ntb, OID_AUTO, enable_xeon_watchdog, CTLFLAG_RDTUN,
    &enable_xeon_watchdog, 0, "If non-zero, write a register every second to "
    "keep a watchdog from tearing down the NTB link");

STAILQ_HEAD(ntb_queue_list, ntb_queue_entry);

typedef uint32_t ntb_q_idx_t;

struct ntb_queue_entry {
	/* ntb_queue list reference */
	STAILQ_ENTRY(ntb_queue_entry) entry;

	/* info on data to be transferred */
	void		*cb_data;
	void		*buf;
	uint32_t	len;
	uint32_t	flags;

	struct ntb_transport_qp		*qp;
	struct ntb_payload_header	*x_hdr;
	ntb_q_idx_t	index;
};

struct ntb_rx_info {
	ntb_q_idx_t	entry;
};

struct ntb_transport_qp {
	struct ntb_transport_ctx	*transport;
	struct ntb_softc	*ntb;

	void			*cb_data;

	bool			client_ready;
	volatile bool		link_is_up;
	uint8_t			qp_num;	/* Only 64 QPs are allowed.  0-63 */

	struct ntb_rx_info	*rx_info;
	struct ntb_rx_info	*remote_rx_info;

	void (*tx_handler)(struct ntb_transport_qp *qp, void *qp_data,
	    void *data, int len);
	struct ntb_queue_list	tx_free_q;
	struct mtx		ntb_tx_free_q_lock;
	caddr_t			tx_mw;
	bus_addr_t		tx_mw_phys;
	ntb_q_idx_t		tx_index;
	ntb_q_idx_t		tx_max_entry;
	uint64_t		tx_max_frame;

	void (*rx_handler)(struct ntb_transport_qp *qp, void *qp_data,
	    void *data, int len);
	struct ntb_queue_list	rx_post_q;
	struct ntb_queue_list	rx_pend_q;
	/* ntb_rx_q_lock: synchronize access to rx_XXXX_q */
	struct mtx		ntb_rx_q_lock;
	struct task		rx_completion_task;
	struct task		rxc_db_work;
	caddr_t			rx_buff;
	ntb_q_idx_t		rx_index;
	ntb_q_idx_t		rx_max_entry;
	uint64_t		rx_max_frame;

	void (*event_handler)(void *data, enum ntb_link_event status);
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
	uint64_t		tx_err_no_buf;
};

struct ntb_queue_handlers {
	void (*rx_handler)(struct ntb_transport_qp *qp, void *qp_data,
	    void *data, int len);
	void (*tx_handler)(struct ntb_transport_qp *qp, void *qp_data,
	    void *data, int len);
	void (*event_handler)(void *data, enum ntb_link_event status);
};

struct ntb_transport_mw {
	vm_paddr_t	phys_addr;
	size_t		phys_size;
	size_t		xlat_align;
	size_t		xlat_align_size;
	bus_addr_t	addr_limit;
	/* Tx buff is off vbase / phys_addr */
	caddr_t		vbase;
	size_t		xlat_size;
	size_t		buff_size;
	/* Rx buff is off virt_addr / dma_addr */
	caddr_t		virt_addr;
	bus_addr_t	dma_addr;
};

struct ntb_transport_ctx {
	struct ntb_softc	*ntb;
	struct ifnet		*ifp;
	struct ntb_transport_mw	mw_vec[NTB_MAX_NUM_MW];
	struct ntb_transport_qp	*qp_vec;
	struct _qpset		qp_bitmap;
	struct _qpset		qp_bitmap_free;
	unsigned		mw_count;
	unsigned		qp_count;
	volatile bool		link_is_up;
	struct callout		link_work;
	struct callout		link_watchdog;
	struct task		link_cleanup;
	uint64_t		bufsize;
	u_char			eaddr[ETHER_ADDR_LEN];
	struct mtx		tx_lock;
	struct mtx		rx_lock;

	/* The hardcoded single queuepair in ntb_setup_interface() */
	struct ntb_transport_qp *qp;
};

static struct ntb_transport_ctx net_softc;

enum {
	IF_NTB_DESC_DONE_FLAG = 1 << 0,
	IF_NTB_LINK_DOWN_FLAG = 1 << 1,
};

struct ntb_payload_header {
	ntb_q_idx_t ver;
	uint32_t len;
	uint32_t flags;
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

	/*
	 * Some NTB-using hardware have a watchdog to work around NTB hangs; if
	 * a register or doorbell isn't written every few seconds, the link is
	 * torn down.  Write an otherwise unused register every few seconds to
	 * work around this watchdog.
	 */
	IF_NTB_WATCHDOG_SPAD = 15
};
CTASSERT(IF_NTB_WATCHDOG_SPAD < XEON_SPAD_COUNT &&
    IF_NTB_WATCHDOG_SPAD < ATOM_SPAD_COUNT);

#define QP_TO_MW(nt, qp)	((qp) % nt->mw_count)
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
static void ntb_net_event_handler(void *data, enum ntb_link_event status);
static int ntb_transport_probe(struct ntb_softc *ntb);
static void ntb_transport_free(struct ntb_transport_ctx *);
static void ntb_transport_init_queue(struct ntb_transport_ctx *nt,
    unsigned int qp_num);
static void ntb_transport_free_queue(struct ntb_transport_qp *qp);
static struct ntb_transport_qp *ntb_transport_create_queue(void *data,
    struct ntb_softc *pdev, const struct ntb_queue_handlers *handlers);
static void ntb_transport_link_up(struct ntb_transport_qp *qp);
static int ntb_transport_tx_enqueue(struct ntb_transport_qp *qp, void *cb,
    void *data, unsigned int len);
static int ntb_process_tx(struct ntb_transport_qp *qp,
    struct ntb_queue_entry *entry);
static void ntb_memcpy_tx(struct ntb_transport_qp *qp,
    struct ntb_queue_entry *entry, void *offset);
static void ntb_qp_full(void *arg);
static void ntb_transport_rxc_db(void *arg, int pending);
static int ntb_process_rxc(struct ntb_transport_qp *qp);
static void ntb_memcpy_rx(struct ntb_transport_qp *qp,
    struct ntb_queue_entry *entry, void *offset);
static inline void ntb_rx_copy_callback(struct ntb_transport_qp *qp,
    void *data);
static void ntb_complete_rxc(void *arg, int pending);
static void ntb_transport_doorbell_callback(void *data, uint32_t vector);
static void ntb_transport_event_callback(void *data);
static void ntb_transport_link_work(void *arg);
static int ntb_set_mw(struct ntb_transport_ctx *, int num_mw, size_t size);
static void ntb_free_mw(struct ntb_transport_ctx *nt, int num_mw);
static int ntb_transport_setup_qp_mw(struct ntb_transport_ctx *nt,
    unsigned int qp_num);
static void ntb_qp_link_work(void *arg);
static void ntb_transport_link_cleanup(struct ntb_transport_ctx *nt);
static void ntb_transport_link_cleanup_work(void *, int);
static void ntb_qp_link_down(struct ntb_transport_qp *qp);
static void ntb_qp_link_down_reset(struct ntb_transport_qp *qp);
static void ntb_qp_link_cleanup(struct ntb_transport_qp *qp);
static void ntb_transport_link_down(struct ntb_transport_qp *qp);
static void ntb_send_link_down(struct ntb_transport_qp *qp);
static void ntb_list_add(struct mtx *lock, struct ntb_queue_entry *entry,
    struct ntb_queue_list *list);
static struct ntb_queue_entry *ntb_list_rm(struct mtx *lock,
    struct ntb_queue_list *list);
static struct ntb_queue_entry *ntb_list_mv(struct mtx *lock,
    struct ntb_queue_list *from, struct ntb_queue_list *to);
static void create_random_local_eui48(u_char *eaddr);
static unsigned int ntb_transport_max_size(struct ntb_transport_qp *qp);
static void xeon_link_watchdog_hb(void *);

static const struct ntb_ctx_ops ntb_transport_ops = {
	.link_event = ntb_transport_event_callback,
	.db_event = ntb_transport_doorbell_callback,
};

MALLOC_DEFINE(M_NTB_IF, "if_ntb", "ntb network driver");

static inline void
iowrite32(uint32_t val, void *addr)
{

	bus_space_write_4(X86_BUS_SPACE_MEM, 0/* HACK */, (uintptr_t)addr,
	    val);
}

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
	int rc;

	net_softc.ntb = devclass_get_softc(devclass_find("ntb_hw"), 0);
	if (net_softc.ntb == NULL) {
		printf("ntb: Cannot find devclass\n");
		return (ENXIO);
	}

	ifp = net_softc.ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		ntb_transport_free(&net_softc);
		printf("ntb: Cannot allocate ifnet structure\n");
		return (ENOMEM);
	}
	if_initname(ifp, "ntb", 0);

	rc = ntb_transport_probe(net_softc.ntb);
	if (rc != 0) {
		printf("ntb: Cannot init transport: %d\n", rc);
		if_free(net_softc.ifp);
		return (rc);
	}

	net_softc.qp = ntb_transport_create_queue(ifp, net_softc.ntb,
	    &handlers);
	ifp->if_init = ntb_net_init;
	ifp->if_softc = &net_softc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ntb_ioctl;
	ifp->if_start = ntb_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);
	create_random_local_eui48(net_softc.eaddr);
	ether_ifattach(ifp, net_softc.eaddr);
	ifp->if_capabilities = IFCAP_HWCSUM | IFCAP_JUMBO_MTU;
	ifp->if_capenable = ifp->if_capabilities;
	ifp->if_mtu = ntb_transport_max_size(net_softc.qp) - ETHER_HDR_LEN -
	    ETHER_CRC_LEN;

	ntb_transport_link_up(net_softc.qp);
	net_softc.bufsize = ntb_transport_max_size(net_softc.qp) +
	    sizeof(struct ether_header);
	return (0);
}

static int
ntb_teardown_interface(void)
{

	if (net_softc.qp != NULL) {
		ntb_transport_link_down(net_softc.qp);

		ntb_transport_free_queue(net_softc.qp);
		ntb_transport_free(&net_softc);
	}

	if (net_softc.ifp != NULL) {
		ether_ifdetach(net_softc.ifp);
		if_free(net_softc.ifp);
		net_softc.ifp = NULL;
	}

	return (0);
}

/* Network device interface */

static void
ntb_net_init(void *arg)
{
	struct ntb_transport_ctx *ntb_softc = arg;
	struct ifnet *ifp = ntb_softc->ifp;

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_flags |= IFF_UP;
	if_link_state_change(ifp, LINK_STATE_UP);
}

static int
ntb_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct ntb_transport_ctx *nt = ifp->if_softc;
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
	struct ntb_transport_ctx *nt = ifp->if_softc;
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
ntb_net_event_handler(void *data, enum ntb_link_event status)
{
	struct ifnet *ifp;

	ifp = data;
	(void)ifp;

	/* XXX The Linux driver munges with the carrier status here. */

	switch (status) {
	case NTB_LINK_DOWN:
		break;
	case NTB_LINK_UP:
		break;
	default:
		panic("Bogus ntb_link_event %u\n", status);
	}
}

/* Transport Init and teardown */

static void
xeon_link_watchdog_hb(void *arg)
{
	struct ntb_transport_ctx *nt;

	nt = arg;
	ntb_spad_write(nt->ntb, IF_NTB_WATCHDOG_SPAD, 0);
	callout_reset(&nt->link_watchdog, 1 * hz, xeon_link_watchdog_hb, nt);
}

static int
ntb_transport_probe(struct ntb_softc *ntb)
{
	struct ntb_transport_ctx *nt = &net_softc;
	struct ntb_transport_mw *mw;
	uint64_t qp_bitmap;
	int rc;
	unsigned i;

	nt->mw_count = ntb_mw_count(ntb);
	for (i = 0; i < nt->mw_count; i++) {
		mw = &nt->mw_vec[i];

		rc = ntb_mw_get_range(ntb, i, &mw->phys_addr, &mw->vbase,
		    &mw->phys_size, &mw->xlat_align, &mw->xlat_align_size,
		    &mw->addr_limit);
		if (rc != 0)
			goto err;

		mw->buff_size = 0;
		mw->xlat_size = 0;
		mw->virt_addr = NULL;
		mw->dma_addr = 0;

		rc = ntb_mw_set_wc(nt->ntb, i, VM_MEMATTR_WRITE_COMBINING);
		if (rc)
			ntb_printf(0, "Unable to set mw%d caching\n", i);
	}

	qp_bitmap = ntb_db_valid_mask(ntb);
	nt->qp_count = flsll(qp_bitmap);
	KASSERT(nt->qp_count != 0, ("bogus db bitmap"));
	nt->qp_count -= 1;

	if (max_num_clients != 0 && max_num_clients < nt->qp_count)
		nt->qp_count = max_num_clients;
	else if (nt->mw_count < nt->qp_count)
		nt->qp_count = nt->mw_count;
	KASSERT(nt->qp_count <= QP_SETSIZE, ("invalid qp_count"));

	mtx_init(&nt->tx_lock, "ntb transport tx", NULL, MTX_DEF);
	mtx_init(&nt->rx_lock, "ntb transport rx", NULL, MTX_DEF);

	nt->qp_vec = malloc(nt->qp_count * sizeof(*nt->qp_vec), M_NTB_IF,
	    M_WAITOK | M_ZERO);

	for (i = 0; i < nt->qp_count; i++) {
		set_bit(i, &nt->qp_bitmap);
		set_bit(i, &nt->qp_bitmap_free);
		ntb_transport_init_queue(nt, i);
	}

	callout_init(&nt->link_work, 0);
	callout_init(&nt->link_watchdog, 0);
	TASK_INIT(&nt->link_cleanup, 0, ntb_transport_link_cleanup_work, nt);

	rc = ntb_set_ctx(ntb, nt, &ntb_transport_ops);
	if (rc != 0)
		goto err;

	nt->link_is_up = false;
	ntb_link_enable(ntb, NTB_SPEED_AUTO, NTB_WIDTH_AUTO);
	ntb_link_event(ntb);

	callout_reset(&nt->link_work, 0, ntb_transport_link_work, nt);
	if (enable_xeon_watchdog != 0)
		callout_reset(&nt->link_watchdog, 0, xeon_link_watchdog_hb, nt);
	return (0);

err:
	free(nt->qp_vec, M_NTB_IF);
	nt->qp_vec = NULL;
	return (rc);
}

static void
ntb_transport_free(struct ntb_transport_ctx *nt)
{
	struct ntb_softc *ntb = nt->ntb;
	struct _qpset qp_bitmap_alloc;
	uint8_t i;

	ntb_transport_link_cleanup(nt);
	taskqueue_drain(taskqueue_swi, &nt->link_cleanup);
	callout_drain(&nt->link_work);
	callout_drain(&nt->link_watchdog);

	BIT_COPY(QP_SETSIZE, &nt->qp_bitmap, &qp_bitmap_alloc);
	BIT_NAND(QP_SETSIZE, &qp_bitmap_alloc, &nt->qp_bitmap_free);

	/* Verify that all the QPs are freed */
	for (i = 0; i < nt->qp_count; i++)
		if (test_bit(i, &qp_bitmap_alloc))
			ntb_transport_free_queue(&nt->qp_vec[i]);

	ntb_link_disable(ntb);
	ntb_clear_ctx(ntb);

	for (i = 0; i < nt->mw_count; i++)
		ntb_free_mw(nt, i);

	free(nt->qp_vec, M_NTB_IF);
}

static void
ntb_transport_init_queue(struct ntb_transport_ctx *nt, unsigned int qp_num)
{
	struct ntb_transport_mw *mw;
	struct ntb_transport_qp *qp;
	vm_paddr_t mw_base;
	uint64_t mw_size, qp_offset;
	size_t tx_size;
	unsigned num_qps_mw, mw_num, mw_count;

	mw_count = nt->mw_count;
	mw_num = QP_TO_MW(nt, qp_num);
	mw = &nt->mw_vec[mw_num];

	qp = &nt->qp_vec[qp_num];
	qp->qp_num = qp_num;
	qp->transport = nt;
	qp->ntb = nt->ntb;
	qp->client_ready = false;
	qp->event_handler = NULL;
	ntb_qp_link_down_reset(qp);

	if (nt->qp_count % mw_count && mw_num + 1 < nt->qp_count / mw_count)
		num_qps_mw = nt->qp_count / mw_count + 1;
	else
		num_qps_mw = nt->qp_count / mw_count;

	mw_base = mw->phys_addr;
	mw_size = mw->phys_size;

	tx_size = mw_size / num_qps_mw;
	qp_offset = tx_size * (qp_num / mw_count);

	qp->tx_mw = mw->vbase + qp_offset;
	KASSERT(qp->tx_mw != NULL, ("uh oh?"));

	/* XXX Assumes that a vm_paddr_t is equivalent to bus_addr_t */
	qp->tx_mw_phys = mw_base + qp_offset;
	KASSERT(qp->tx_mw_phys != 0, ("uh oh?"));

	tx_size -= sizeof(struct ntb_rx_info);
	qp->rx_info = (void *)(qp->tx_mw + tx_size);

	/* Due to house-keeping, there must be at least 2 buffs */
	qp->tx_max_frame = qmin(tx_size / 2,
	    transport_mtu + sizeof(struct ntb_payload_header));
	qp->tx_max_entry = tx_size / qp->tx_max_frame;

	callout_init(&qp->link_work, 0);
	callout_init(&qp->queue_full, 1);
	callout_init(&qp->rx_full, 1);

	mtx_init(&qp->ntb_rx_q_lock, "ntb rx q", NULL, MTX_SPIN);
	mtx_init(&qp->ntb_tx_free_q_lock, "ntb tx free q", NULL, MTX_SPIN);
	TASK_INIT(&qp->rx_completion_task, 0, ntb_complete_rxc, qp);
	TASK_INIT(&qp->rxc_db_work, 0, ntb_transport_rxc_db, qp);

	STAILQ_INIT(&qp->rx_post_q);
	STAILQ_INIT(&qp->rx_pend_q);
	STAILQ_INIT(&qp->tx_free_q);

	callout_reset(&qp->link_work, 0, ntb_qp_link_work, qp);
}

static void
ntb_transport_free_queue(struct ntb_transport_qp *qp)
{
	struct ntb_queue_entry *entry;

	if (qp == NULL)
		return;

	callout_drain(&qp->link_work);

	ntb_db_set_mask(qp->ntb, 1ull << qp->qp_num);
	taskqueue_drain(taskqueue_swi, &qp->rxc_db_work);
	taskqueue_drain(taskqueue_swi, &qp->rx_completion_task);

	qp->cb_data = NULL;
	qp->rx_handler = NULL;
	qp->tx_handler = NULL;
	qp->event_handler = NULL;

	while ((entry = ntb_list_rm(&qp->ntb_rx_q_lock, &qp->rx_pend_q)))
		free(entry, M_NTB_IF);

	while ((entry = ntb_list_rm(&qp->ntb_rx_q_lock, &qp->rx_post_q)))
		free(entry, M_NTB_IF);

	while ((entry = ntb_list_rm(&qp->ntb_tx_free_q_lock, &qp->tx_free_q)))
		free(entry, M_NTB_IF);

	set_bit(qp->qp_num, &qp->transport->qp_bitmap_free);
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
ntb_transport_create_queue(void *data, struct ntb_softc *ntb,
    const struct ntb_queue_handlers *handlers)
{
	struct ntb_queue_entry *entry;
	struct ntb_transport_qp *qp;
	struct ntb_transport_ctx *nt;
	unsigned int free_queue;
	int i;

	nt = ntb_get_ctx(ntb, NULL);
	KASSERT(nt != NULL, ("bogus"));

	free_queue = ffs_bit(&nt->qp_bitmap);
	if (free_queue == 0)
		return (NULL);

	/* decrement free_queue to make it zero based */
	free_queue--;

	qp = &nt->qp_vec[free_queue];
	clear_bit(qp->qp_num, &nt->qp_bitmap_free);
	qp->cb_data = data;
	qp->rx_handler = handlers->rx_handler;
	qp->tx_handler = handlers->tx_handler;
	qp->event_handler = handlers->event_handler;

	for (i = 0; i < NTB_QP_DEF_NUM_ENTRIES; i++) {
		entry = malloc(sizeof(*entry), M_NTB_IF, M_WAITOK | M_ZERO);
		entry->cb_data = nt->ifp;
		entry->buf = NULL;
		entry->len = transport_mtu;
		ntb_list_add(&qp->ntb_rx_q_lock, entry, &qp->rx_pend_q);
	}

	for (i = 0; i < NTB_QP_DEF_NUM_ENTRIES; i++) {
		entry = malloc(sizeof(*entry), M_NTB_IF, M_WAITOK | M_ZERO);
		ntb_list_add(&qp->ntb_tx_free_q_lock, entry, &qp->tx_free_q);
	}

	ntb_db_clear(ntb, 1ull << qp->qp_num);
	ntb_db_clear_mask(ntb, 1ull << qp->qp_num);
	return (qp);
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
	struct ntb_transport_ctx *nt;

	if (qp == NULL)
		return;

	qp->client_ready = true;

	nt = qp->transport;
	ntb_printf(2, "qp client ready\n");

	if (qp->transport->link_is_up)
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
 * payload will be transmitted.  This assumes that a lock is being held to
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

	if (qp == NULL || !qp->link_is_up || len == 0) {
		CTR0(KTR_NTB, "TX: link not up");
		return (EINVAL);
	}

	entry = ntb_list_rm(&qp->ntb_tx_free_q_lock, &qp->tx_free_q);
	if (entry == NULL) {
		CTR0(KTR_NTB, "TX: could not get entry from tx_free_q");
		qp->tx_err_no_buf++;
		return (EBUSY);
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

	offset = qp->tx_mw + qp->tx_max_frame * qp->tx_index;
	CTR3(KTR_NTB,
	    "TX: process_tx: tx_pkts=%lu, tx_index=%u, remote entry=%u",
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
		else
			m_freem(entry->buf);

		entry->buf = NULL;
		ntb_list_add(&qp->ntb_tx_free_q_lock, entry, &qp->tx_free_q);
		CTR1(KTR_NTB,
		    "TX: frame too big. returning entry %p to tx_free_q",
		    entry);
		return (0);
	}
	CTR2(KTR_NTB, "TX: copying entry %p to offset %p", entry, offset);
	ntb_memcpy_tx(qp, entry, offset);

	qp->tx_index++;
	qp->tx_index %= qp->tx_max_entry;

	qp->tx_pkts++;

	return (0);
}

static void
ntb_memcpy_tx(struct ntb_transport_qp *qp, struct ntb_queue_entry *entry,
    void *offset)
{
	struct ntb_payload_header *hdr;

	/* This piece is from Linux' ntb_async_tx() */
	hdr = (struct ntb_payload_header *)((char *)offset + qp->tx_max_frame -
	    sizeof(struct ntb_payload_header));
	entry->x_hdr = hdr;
	iowrite32(entry->len, &hdr->len);
	iowrite32(qp->tx_pkts, &hdr->ver);

	/* This piece is ntb_memcpy_tx() */
	CTR2(KTR_NTB, "TX: copying %d bytes to offset %p", entry->len, offset);
	if (entry->buf != NULL) {
		m_copydata((struct mbuf *)entry->buf, 0, entry->len, offset);

		/*
		 * Ensure that the data is fully copied before setting the
		 * flags
		 */
		wmb();
	}

	/* The rest is ntb_tx_copy_callback() */
	iowrite32(entry->flags | IF_NTB_DESC_DONE_FLAG, &hdr->flags);
	CTR1(KTR_NTB, "TX: hdr %p set DESC_DONE", hdr);

	ntb_peer_db_set(qp->ntb, 1ull << qp->qp_num);

	/*
	 * The entry length can only be zero if the packet is intended to be a
	 * "link down" or similar.  Since no payload is being sent in these
	 * cases, there is nothing to add to the completion queue.
	 */
	if (entry->len > 0) {
		qp->tx_bytes += entry->len;

		if (qp->tx_handler)
			qp->tx_handler(qp, qp->cb_data, entry->buf,
			    entry->len);
		else
			m_freem(entry->buf);
		entry->buf = NULL;
	}

	CTR3(KTR_NTB,
	    "TX: entry %p sent. hdr->ver = %u, hdr->flags = 0x%x, Returning "
	    "to tx_free_q", entry, hdr->ver, hdr->flags);
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
ntb_transport_rxc_db(void *arg, int pending __unused)
{
	struct ntb_transport_qp *qp = arg;
	ntb_q_idx_t i;
	int rc;

	/*
	 * Limit the number of packets processed in a single interrupt to
	 * provide fairness to others
	 */
	CTR0(KTR_NTB, "RX: transport_rx");
	mtx_lock(&qp->transport->rx_lock);
	for (i = 0; i < qp->rx_max_entry; i++) {
		rc = ntb_process_rxc(qp);
		if (rc != 0) {
			CTR0(KTR_NTB, "RX: process_rxc failed");
			break;
		}
	}
	mtx_unlock(&qp->transport->rx_lock);

	if (i == qp->rx_max_entry)
		taskqueue_enqueue(taskqueue_swi, &qp->rxc_db_work);
	else if ((ntb_db_read(qp->ntb) & (1ull << qp->qp_num)) != 0) {
		/* If db is set, clear it and read it back to commit clear. */
		ntb_db_clear(qp->ntb, 1ull << qp->qp_num);
		(void)ntb_db_read(qp->ntb);

		/*
		 * An interrupt may have arrived between finishing
		 * ntb_process_rxc and clearing the doorbell bit: there might
		 * be some more work to do.
		 */
		taskqueue_enqueue(taskqueue_swi, &qp->rxc_db_work);
	}
}

static int
ntb_process_rxc(struct ntb_transport_qp *qp)
{
	struct ntb_payload_header *hdr;
	struct ntb_queue_entry *entry;
	caddr_t offset;

	offset = qp->rx_buff + qp->rx_max_frame * qp->rx_index;
	hdr = (void *)(offset + qp->rx_max_frame -
	    sizeof(struct ntb_payload_header));

	CTR1(KTR_NTB, "RX: process_rxc rx_index = %u", qp->rx_index);
	if ((hdr->flags & IF_NTB_DESC_DONE_FLAG) == 0) {
		CTR0(KTR_NTB, "RX: hdr not done");
		qp->rx_ring_empty++;
		return (EAGAIN);
	}

	if ((hdr->flags & IF_NTB_LINK_DOWN_FLAG) != 0) {
		CTR0(KTR_NTB, "RX: link down");
		ntb_qp_link_down(qp);
		hdr->flags = 0;
		return (EAGAIN);
	}

	if (hdr->ver != (uint32_t)qp->rx_pkts) {
		CTR2(KTR_NTB,"RX: ver != rx_pkts (%x != %lx). "
		    "Returning entry to rx_pend_q", hdr->ver, qp->rx_pkts);
		qp->rx_err_ver++;
		return (EIO);
	}

	entry = ntb_list_mv(&qp->ntb_rx_q_lock, &qp->rx_pend_q, &qp->rx_post_q);
	if (entry == NULL) {
		qp->rx_err_no_buf++;
		CTR0(KTR_NTB, "RX: No entries in rx_pend_q");
		return (EAGAIN);
	}
	callout_stop(&qp->rx_full);
	CTR1(KTR_NTB, "RX: rx entry %p from rx_pend_q", entry);

	entry->x_hdr = hdr;
	entry->index = qp->rx_index;

	if (hdr->len > entry->len) {
		CTR2(KTR_NTB, "RX: len too long. Wanted %ju got %ju",
		    (uintmax_t)hdr->len, (uintmax_t)entry->len);
		qp->rx_err_oflow++;

		entry->len = -EIO;
		entry->flags |= IF_NTB_DESC_DONE_FLAG;

		taskqueue_enqueue(taskqueue_swi, &qp->rx_completion_task);
	} else {
		qp->rx_bytes += hdr->len;
		qp->rx_pkts++;

		CTR1(KTR_NTB, "RX: received %ld rx_pkts", qp->rx_pkts);

		entry->len = hdr->len;

		ntb_memcpy_rx(qp, entry, offset);
	}

	qp->rx_index++;
	qp->rx_index %= qp->rx_max_entry;
	return (0);
}

static void
ntb_memcpy_rx(struct ntb_transport_qp *qp, struct ntb_queue_entry *entry,
    void *offset)
{
	struct ifnet *ifp = entry->cb_data;
	unsigned int len = entry->len;
	struct mbuf *m;

	CTR2(KTR_NTB, "RX: copying %d bytes from offset %p", len, offset);
	m = m_devget(offset, len, 0, ifp, NULL);
	m->m_pkthdr.csum_flags = CSUM_IP_CHECKED | CSUM_IP_VALID;

	entry->buf = (void *)m;

	/* Ensure that the data is globally visible before clearing the flag */
	wmb();

	CTR2(KTR_NTB, "RX: copied entry %p to mbuf %p.", entry, m);
	ntb_rx_copy_callback(qp, entry);
}

static inline void
ntb_rx_copy_callback(struct ntb_transport_qp *qp, void *data)
{
	struct ntb_queue_entry *entry;

	entry = data;
	entry->flags |= IF_NTB_DESC_DONE_FLAG;
	taskqueue_enqueue(taskqueue_swi, &qp->rx_completion_task);
}

static void
ntb_complete_rxc(void *arg, int pending)
{
	struct ntb_transport_qp *qp = arg;
	struct ntb_queue_entry *entry;
	struct mbuf *m;
	unsigned len;

	CTR0(KTR_NTB, "RX: rx_completion_task");

	mtx_lock_spin(&qp->ntb_rx_q_lock);

	while (!STAILQ_EMPTY(&qp->rx_post_q)) {
		entry = STAILQ_FIRST(&qp->rx_post_q);
		if ((entry->flags & IF_NTB_DESC_DONE_FLAG) == 0)
			break;

		entry->x_hdr->flags = 0;
		iowrite32(entry->index, &qp->rx_info->entry);

		STAILQ_REMOVE_HEAD(&qp->rx_post_q, entry);

		len = entry->len;
		m = entry->buf;

		/*
		 * Re-initialize queue_entry for reuse; rx_handler takes
		 * ownership of the mbuf.
		 */
		entry->buf = NULL;
		entry->len = transport_mtu;
		entry->cb_data = qp->transport->ifp;

		STAILQ_INSERT_TAIL(&qp->rx_pend_q, entry, entry);

		mtx_unlock_spin(&qp->ntb_rx_q_lock);

		CTR2(KTR_NTB, "RX: completing entry %p, mbuf %p", entry, m);
		if (qp->rx_handler != NULL && qp->client_ready)
			qp->rx_handler(qp, qp->cb_data, m, len);
		else
			m_freem(m);

		mtx_lock_spin(&qp->ntb_rx_q_lock);
	}

	mtx_unlock_spin(&qp->ntb_rx_q_lock);
}

static void
ntb_transport_doorbell_callback(void *data, uint32_t vector)
{
	struct ntb_transport_ctx *nt = data;
	struct ntb_transport_qp *qp;
	struct _qpset db_bits;
	uint64_t vec_mask;
	unsigned qp_num;

	BIT_COPY(QP_SETSIZE, &nt->qp_bitmap, &db_bits);
	BIT_NAND(QP_SETSIZE, &db_bits, &nt->qp_bitmap_free);

	vec_mask = ntb_db_vector_mask(nt->ntb, vector);
	while (vec_mask != 0) {
		qp_num = ffsll(vec_mask) - 1;

		if (test_bit(qp_num, &db_bits)) {
			qp = &nt->qp_vec[qp_num];
			taskqueue_enqueue(taskqueue_swi, &qp->rxc_db_work);
		}

		vec_mask &= ~(1ull << qp_num);
	}
}

/* Link Event handler */
static void
ntb_transport_event_callback(void *data)
{
	struct ntb_transport_ctx *nt = data;

	if (ntb_link_is_up(nt->ntb, NULL, NULL)) {
		ntb_printf(1, "HW link up\n");
		callout_reset(&nt->link_work, 0, ntb_transport_link_work, nt);
	} else {
		ntb_printf(1, "HW link down\n");
		taskqueue_enqueue(taskqueue_swi, &nt->link_cleanup);
	}
}

/* Link bring up */
static void
ntb_transport_link_work(void *arg)
{
	struct ntb_transport_ctx *nt = arg;
	struct ntb_softc *ntb = nt->ntb;
	struct ntb_transport_qp *qp;
	uint64_t val64, size;
	uint32_t val;
	unsigned i;
	int rc;

	/* send the local info, in the opposite order of the way we read it */
	for (i = 0; i < nt->mw_count; i++) {
		size = nt->mw_vec[i].phys_size;

		if (max_mw_size != 0 && size > max_mw_size)
			size = max_mw_size;

		ntb_peer_spad_write(ntb, IF_NTB_MW0_SZ_HIGH + (i * 2),
		    size >> 32);
		ntb_peer_spad_write(ntb, IF_NTB_MW0_SZ_LOW + (i * 2), size);
	}

	ntb_peer_spad_write(ntb, IF_NTB_NUM_MWS, nt->mw_count);

	ntb_peer_spad_write(ntb, IF_NTB_NUM_QPS, nt->qp_count);

	ntb_peer_spad_write(ntb, IF_NTB_VERSION, NTB_TRANSPORT_VERSION);

	/* Query the remote side for its info */
	val = 0;
	ntb_spad_read(ntb, IF_NTB_VERSION, &val);
	if (val != NTB_TRANSPORT_VERSION)
		goto out;

	ntb_spad_read(ntb, IF_NTB_NUM_QPS, &val);
	if (val != nt->qp_count)
		goto out;

	ntb_spad_read(ntb, IF_NTB_NUM_MWS, &val);
	if (val != nt->mw_count)
		goto out;

	for (i = 0; i < nt->mw_count; i++) {
		ntb_spad_read(ntb, IF_NTB_MW0_SZ_HIGH + (i * 2), &val);
		val64 = (uint64_t)val << 32;

		ntb_spad_read(ntb, IF_NTB_MW0_SZ_LOW + (i * 2), &val);
		val64 |= val;

		rc = ntb_set_mw(nt, i, val64);
		if (rc != 0)
			goto free_mws;
	}

	nt->link_is_up = true;
	ntb_printf(1, "transport link up\n");

	for (i = 0; i < nt->qp_count; i++) {
		qp = &nt->qp_vec[i];

		ntb_transport_setup_qp_mw(nt, i);

		if (qp->client_ready)
			callout_reset(&qp->link_work, 0, ntb_qp_link_work, qp);
	}

	return;

free_mws:
	for (i = 0; i < nt->mw_count; i++)
		ntb_free_mw(nt, i);
out:
	if (ntb_link_is_up(ntb, NULL, NULL))
		callout_reset(&nt->link_work,
		    NTB_LINK_DOWN_TIMEOUT * hz / 1000, ntb_transport_link_work, nt);
}

static int
ntb_set_mw(struct ntb_transport_ctx *nt, int num_mw, size_t size)
{
	struct ntb_transport_mw *mw = &nt->mw_vec[num_mw];
	size_t xlat_size, buff_size;
	int rc;

	if (size == 0)
		return (EINVAL);

	xlat_size = roundup(size, mw->xlat_align_size);
	buff_size = xlat_size;

	/* No need to re-setup */
	if (mw->xlat_size == xlat_size)
		return (0);

	if (mw->buff_size != 0)
		ntb_free_mw(nt, num_mw);

	/* Alloc memory for receiving data.  Must be aligned */
	mw->xlat_size = xlat_size;
	mw->buff_size = buff_size;

	mw->virt_addr = contigmalloc(mw->buff_size, M_NTB_IF, M_ZERO, 0,
	    mw->addr_limit, mw->xlat_align, 0);
	if (mw->virt_addr == NULL) {
		ntb_printf(0, "Unable to allocate MW buffer of size %zu/%zu\n",
		    mw->buff_size, mw->xlat_size);
		mw->xlat_size = 0;
		mw->buff_size = 0;
		return (ENOMEM);
	}
	/* TODO: replace with bus_space_* functions */
	mw->dma_addr = vtophys(mw->virt_addr);

	/*
	 * Ensure that the allocation from contigmalloc is aligned as
	 * requested.  XXX: This may not be needed -- brought in for parity
	 * with the Linux driver.
	 */
	if (mw->dma_addr % mw->xlat_align != 0) {
		ntb_printf(0,
		    "DMA memory 0x%jx not aligned to BAR size 0x%zx\n",
		    (uintmax_t)mw->dma_addr, size);
		ntb_free_mw(nt, num_mw);
		return (ENOMEM);
	}

	/* Notify HW the memory location of the receive buffer */
	rc = ntb_mw_set_trans(nt->ntb, num_mw, mw->dma_addr, mw->xlat_size);
	if (rc) {
		ntb_printf(0, "Unable to set mw%d translation\n", num_mw);
		ntb_free_mw(nt, num_mw);
		return (rc);
	}

	return (0);
}

static void
ntb_free_mw(struct ntb_transport_ctx *nt, int num_mw)
{
	struct ntb_transport_mw *mw = &nt->mw_vec[num_mw];

	if (mw->virt_addr == NULL)
		return;

	ntb_mw_clear_trans(nt->ntb, num_mw);
	contigfree(mw->virt_addr, mw->xlat_size, M_NTB_IF);
	mw->xlat_size = 0;
	mw->buff_size = 0;
	mw->virt_addr = NULL;
}

static int
ntb_transport_setup_qp_mw(struct ntb_transport_ctx *nt, unsigned int qp_num)
{
	struct ntb_transport_qp *qp = &nt->qp_vec[qp_num];
	struct ntb_transport_mw *mw;
	void *offset;
	ntb_q_idx_t i;
	size_t rx_size;
	unsigned num_qps_mw, mw_num, mw_count;

	mw_count = nt->mw_count;
	mw_num = QP_TO_MW(nt, qp_num);
	mw = &nt->mw_vec[mw_num];

	if (mw->virt_addr == NULL)
		return (ENOMEM);

	if (nt->qp_count % mw_count && mw_num + 1 < nt->qp_count / mw_count)
		num_qps_mw = nt->qp_count / mw_count + 1;
	else
		num_qps_mw = nt->qp_count / mw_count;

	rx_size = mw->xlat_size / num_qps_mw;
	qp->rx_buff = mw->virt_addr + rx_size * (qp_num / mw_count);
	rx_size -= sizeof(struct ntb_rx_info);

	qp->remote_rx_info = (void*)(qp->rx_buff + rx_size);

	/* Due to house-keeping, there must be at least 2 buffs */
	qp->rx_max_frame = qmin(rx_size / 2,
	    transport_mtu + sizeof(struct ntb_payload_header));
	qp->rx_max_entry = rx_size / qp->rx_max_frame;
	qp->rx_index = 0;

	qp->remote_rx_info->entry = qp->rx_max_entry - 1;

	/* Set up the hdr offsets with 0s */
	for (i = 0; i < qp->rx_max_entry; i++) {
		offset = (void *)(qp->rx_buff + qp->rx_max_frame * (i + 1) -
		    sizeof(struct ntb_payload_header));
		memset(offset, 0, sizeof(struct ntb_payload_header));
	}

	qp->rx_pkts = 0;
	qp->tx_pkts = 0;
	qp->tx_index = 0;

	return (0);
}

static void
ntb_qp_link_work(void *arg)
{
	struct ntb_transport_qp *qp = arg;
	struct ntb_softc *ntb = qp->ntb;
	struct ntb_transport_ctx *nt = qp->transport;
	uint32_t val, dummy;

	ntb_spad_read(ntb, IF_NTB_QP_LINKS, &val);

	ntb_peer_spad_write(ntb, IF_NTB_QP_LINKS, val | (1ull << qp->qp_num));

	/* query remote spad for qp ready bits */
	ntb_peer_spad_read(ntb, IF_NTB_QP_LINKS, &dummy);

	/* See if the remote side is up */
	if ((val & (1ull << qp->qp_num)) != 0) {
		ntb_printf(2, "qp link up\n");
		qp->link_is_up = true;

		if (qp->event_handler != NULL)
			qp->event_handler(qp->cb_data, NTB_LINK_UP);

		taskqueue_enqueue(taskqueue_swi, &qp->rxc_db_work);
	} else if (nt->link_is_up)
		callout_reset(&qp->link_work,
		    NTB_LINK_DOWN_TIMEOUT * hz / 1000, ntb_qp_link_work, qp);
}

/* Link down event*/
static void
ntb_transport_link_cleanup(struct ntb_transport_ctx *nt)
{
	struct ntb_transport_qp *qp;
	struct _qpset qp_bitmap_alloc;
	unsigned i;

	BIT_COPY(QP_SETSIZE, &nt->qp_bitmap, &qp_bitmap_alloc);
	BIT_NAND(QP_SETSIZE, &qp_bitmap_alloc, &nt->qp_bitmap_free);

	/* Pass along the info to any clients */
	for (i = 0; i < nt->qp_count; i++)
		if (test_bit(i, &qp_bitmap_alloc)) {
			qp = &nt->qp_vec[i];
			ntb_qp_link_cleanup(qp);
			callout_drain(&qp->link_work);
		}

	if (!nt->link_is_up)
		callout_drain(&nt->link_work);

	/*
	 * The scratchpad registers keep the values if the remote side
	 * goes down, blast them now to give them a sane value the next
	 * time they are accessed
	 */
	for (i = 0; i < IF_NTB_MAX_SPAD; i++)
		ntb_spad_write(nt->ntb, i, 0);
}

static void
ntb_transport_link_cleanup_work(void *arg, int pending __unused)
{

	ntb_transport_link_cleanup(arg);
}

static void
ntb_qp_link_down(struct ntb_transport_qp *qp)
{

	ntb_qp_link_cleanup(qp);
}

static void
ntb_qp_link_down_reset(struct ntb_transport_qp *qp)
{

	qp->link_is_up = false;

	qp->tx_index = qp->rx_index = 0;
	qp->tx_bytes = qp->rx_bytes = 0;
	qp->tx_pkts = qp->rx_pkts = 0;

	qp->rx_ring_empty = 0;
	qp->tx_ring_full = 0;

	qp->rx_err_no_buf = qp->tx_err_no_buf = 0;
	qp->rx_err_oflow = qp->rx_err_ver = 0;
}

static void
ntb_qp_link_cleanup(struct ntb_transport_qp *qp)
{
	struct ntb_transport_ctx *nt = qp->transport;

	callout_drain(&qp->link_work);
	ntb_qp_link_down_reset(qp);

	if (qp->event_handler != NULL)
		qp->event_handler(qp->cb_data, NTB_LINK_DOWN);

	if (nt->link_is_up)
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
 * entries on queue are purged or otherwise handled appropriately.
 */
static void
ntb_transport_link_down(struct ntb_transport_qp *qp)
{
	uint32_t val;

	if (qp == NULL)
		return;

	qp->client_ready = false;

	ntb_spad_read(qp->ntb, IF_NTB_QP_LINKS, &val);

	ntb_peer_spad_write(qp->ntb, IF_NTB_QP_LINKS,
	   val & ~(1 << qp->qp_num));

	if (qp->link_is_up)
		ntb_send_link_down(qp);
	else
		callout_drain(&qp->link_work);
}

static void
ntb_send_link_down(struct ntb_transport_qp *qp)
{
	struct ntb_queue_entry *entry;
	int i, rc;

	if (!qp->link_is_up)
		return;

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

	ntb_qp_link_down_reset(qp);
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

static struct ntb_queue_entry *
ntb_list_mv(struct mtx *lock, struct ntb_queue_list *from,
    struct ntb_queue_list *to)
{
	struct ntb_queue_entry *entry;

	mtx_lock_spin(lock);
	if (STAILQ_EMPTY(from)) {
		entry = NULL;
		goto out;
	}
	entry = STAILQ_FIRST(from);
	STAILQ_REMOVE_HEAD(from, entry);
	STAILQ_INSERT_TAIL(to, entry, entry);

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
