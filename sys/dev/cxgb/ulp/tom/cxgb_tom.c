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
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/eventhandler.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/socketvar.h>
#include <sys/taskqueue.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>

#include <dev/cxgb/cxgb_osdep.h>
#include <dev/cxgb/sys/mbufq.h>

#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_ofld.h>
#include <netinet/tcp_fsm.h>
#include <net/route.h>

#include <dev/cxgb/t3cdev.h>
#include <dev/cxgb/common/cxgb_firmware_exports.h>
#include <dev/cxgb/common/cxgb_tcb.h>
#include <dev/cxgb/cxgb_include.h>
#include <dev/cxgb/common/cxgb_ctl_defs.h>
#include <dev/cxgb/common/cxgb_t3_cpl.h>
#include <dev/cxgb/cxgb_offload.h>
#include <dev/cxgb/cxgb_l2t.h>
#include <dev/cxgb/ulp/toecore/cxgb_toedev.h>
#include <dev/cxgb/ulp/tom/cxgb_tom.h>
#include <dev/cxgb/ulp/tom/cxgb_defs.h>
#include <dev/cxgb/ulp/tom/cxgb_t3_ddp.h>
#include <dev/cxgb/ulp/tom/cxgb_toepcb.h>

static int activated = 1;
TUNABLE_INT("hw.t3toe.activated", &activated);
SYSCTL_NODE(_hw, OID_AUTO, t3toe, CTLFLAG_RD, 0, "T3 toe driver parameters");
SYSCTL_UINT(_hw_t3toe, OID_AUTO, activated, CTLFLAG_RDTUN, &activated, 0,
    "enable TOE at init time");

static TAILQ_HEAD(, tom_data) cxgb_list;
static struct mtx cxgb_list_lock;

static int t3_toe_attach(struct toedev *dev, const struct offload_id *entry);
/*
 * Handlers for each CPL opcode
 */
static cxgb_cpl_handler_func tom_cpl_handlers[NUM_CPL_CMDS];

static eventhandler_tag listen_tag;

static struct offload_id t3_toe_id_tab[] = {
	{ TOE_ID_CHELSIO_T3, 0 },
	{ TOE_ID_CHELSIO_T3B, 0 },
	{ 0 }
};

static struct tom_info t3_tom_info = {
	.ti_attach = t3_toe_attach,
	.ti_id_table = t3_toe_id_tab,
	.ti_name = "Chelsio-T3"
};

struct cxgb_client t3c_tom_client = {
	.name = "tom_cxgb3",
	.remove = NULL,
	.handlers = tom_cpl_handlers,
	.redirect = NULL
};

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
	
	toep = malloc(sizeof(struct toepcb), M_DEVBUF, M_NOWAIT);
	
	if (toep == NULL)
		return (NULL);

	toepcb_init(toep);
	return (toep);
}

void
toepcb_init(struct toepcb *toep)
{
	bzero(toep, sizeof(*toep));
	toep->tp_refcount = 1;
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
		printf("doing final toepcb free\n");
		
		free(toep, M_DEVBUF);
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

/*
 * Allocate a TOM data structure,
 * initialize its cpl_handlers
 * and register it as a T3C client
 */
static void t3c_tom_add(struct t3cdev *cdev)
{
	int i;
	unsigned int wr_len;
	struct tom_data *t;
	struct toedev *tdev;
	struct adap_ports *port_info;

	t = malloc(sizeof(*t), M_CXGB, M_NOWAIT|M_ZERO);
	
	if (!t)
		return;

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
	tdev->tod_ttid = (cdev->type == T3A ?
		      TOE_ID_CHELSIO_T3 : TOE_ID_CHELSIO_T3B);
	tdev->tod_lldev = cdev->lldev;

	if (register_toedev(tdev, "toe%d")) {
		printf("unable to register offload device");
		goto out_free_all;
	}
	TOM_DATA(tdev) = t;

	for (i = 0; i < port_info->nports; i++) {
		struct ifnet *ifp = port_info->lldevs[i];
		TOEDEV(ifp) = tdev;
		
		ifp->if_capabilities |= IFCAP_TOE;
	}
	t->ports = port_info;

	/* Add device to the list of offload devices */
	t3cdev_add(t);

	/* Activate TCP offload device */
	activate_offload(tdev);
	return;

out_free_all:
	free(port_info, M_CXGB);
out_free_tom:
	free(t, M_CXGB);
	return;
}

/*
 * Process a received packet with an unknown/unexpected CPL opcode.
 */
static int
do_bad_cpl(struct t3cdev *cdev, struct mbuf *m, void *ctx)
{
	log(LOG_ERR, "%s: received bad CPL command %u\n", cdev->name,
	    *mtod(m, unsigned int *));

	return (CPL_RET_BUF_DONE | CPL_RET_BAD_MSG);
}


/*
 * Add a new handler to the CPL dispatch table.  A NULL handler may be supplied
 * to unregister an existing handler.
 */
void
t3tom_register_cpl_handler(unsigned int opcode, cxgb_cpl_handler_func h)
{
	if (opcode < NUM_CPL_CMDS)
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

	return sotoinpcb(so)->inp_depend4.inp4_options == NULL &&
	    tomd->conf.activated &&
	    (tomd->conf.max_conn < 0 ||
	     atomic_load_acq_int(&t->tids_in_use) + t->atids_in_use < tomd->conf.max_conn);
}


static int tom_ctl(struct toedev *dev, unsigned int req, void *data)
{
	struct tom_data *t = TOM_DATA(dev);
	struct t3cdev *cdev = t->cdev;

	if (cdev->ctl)
		return cdev->ctl(cdev, req, data);

	return (EOPNOTSUPP);
}

/*
 * Initialize the CPL dispatch table.
 */
static void
init_cpl_handlers(void)
{
	int i;

	for (i = 0; i < NUM_CPL_CMDS; ++i)
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
	
#if 0
	skb_queue_head_init(&t->deferq);
	T3_INIT_WORK(&t->deferq_task, process_deferq, t);
	spin_lock_init(&t->listen_lock);
#endif
	t3_init_tunables(t);
	mtx_init(&t->listen_lock, "tom data listeners", NULL, MTX_DEF);

	/* Adjust TOE activation for this module */
	t->conf.activated = activated;

	dev->tod_can_offload = can_offload;
	dev->tod_connect = t3_connect;
	dev->tod_ctl = tom_ctl;
#if 0	
#ifndef NETEVENT
	dev->tod_neigh_update = tom_neigh_update;
#endif
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
#ifdef notyet
	/* OK if this fails, we just can't do DDP */
	t->nppods = (ddp.ulimit + 1 - ddp.llimit) / PPOD_SIZE;
	t->ppod_map = t3_alloc_mem(t->nppods);
#endif

#if 0
	spin_lock_init(&t->ppod_map_lock);
	tom_proc_init(dev);
#ifdef CONFIG_SYSCTL
	t->sysctl = t3_sysctl_register(dev, &t->conf);
#endif
#endif
	return (0);
}

static void
cxgb_toe_listen(void *unused, int event, struct tcpcb *tp)
{
	struct socket *so = tp->t_inpcb->inp_socket;
	struct tom_data *p;

	switch (event) {
	case OFLD_LISTEN_OPEN:
	case OFLD_LISTEN_CLOSE:
		mtx_lock(&cxgb_list_lock);
		TAILQ_FOREACH(p, &cxgb_list, entry) {
			if (event == OFLD_LISTEN_OPEN)
				t3_listen_start(&p->tdev, so, p->cdev);
			else if (tp->t_state == TCPS_LISTEN) {
				printf("stopping listen on port=%d\n",
				    ntohs(tp->t_inpcb->inp_lport));
				
				t3_listen_stop(&p->tdev, so, p->cdev);
			}
			
		}
		mtx_unlock(&cxgb_list_lock);
		break;
	default:
		log(LOG_ERR, "unrecognized listen event %d\n", event);
		break;
	}
}

static void
cxgb_register_listeners(void)
{
	struct inpcb *inp;
	struct tcpcb *tp;
	
	INP_INFO_RLOCK(&tcbinfo);
	LIST_FOREACH(inp, tcbinfo.ipi_listhead, inp_list) {
		tp = intotcpcb(inp);

		if (tp->t_state == TCPS_LISTEN)
			cxgb_toe_listen(NULL, OFLD_LISTEN_OPEN, tp);
	}
	INP_INFO_RUNLOCK(&tcbinfo);
}

static int
t3_tom_init(void)
{

#if 0
	struct socket *sock;
	err = sock_create_kern(PF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
	if (err < 0) {
		printk(KERN_ERR "Could not create TCP socket, error %d\n", err);
		return err;
	}

	t3_def_state_change = sock->sk->sk_state_change;
	t3_def_data_ready = sock->sk->sk_data_ready;
	t3_def_error_report = sock->sk->sk_error_report;
	sock_release(sock);
#endif
	init_cpl_handlers();
	if (t3_init_cpl_io() < 0)
		return -1;
	t3_init_socket_ops();

	 /* Register with the TOE device layer. */

	if (register_tom(&t3_tom_info) != 0) {
		log(LOG_ERR,
		    "Unable to register Chelsio T3 TCP offload module.\n");
		return -1;
	}

	mtx_init(&cxgb_list_lock, "cxgb tom list", NULL, MTX_DEF);
	listen_tag = EVENTHANDLER_REGISTER(ofld_listen, cxgb_toe_listen, NULL, EVENTHANDLER_PRI_ANY);
	TAILQ_INIT(&cxgb_list);
	
	/* Register to offloading devices */
	t3c_tom_client.add = t3c_tom_add;
	cxgb_register_client(&t3c_tom_client);
	cxgb_register_listeners();
	return (0);
}

static int
t3_tom_load(module_t mod, int cmd, void *arg)
{
	int err = 0;

	switch (cmd) {
	case MOD_LOAD:
		printf("wheeeeee ...\n");
		
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

