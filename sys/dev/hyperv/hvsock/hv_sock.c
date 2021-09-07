/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/domain.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/sockbuf.h>
#include <sys/sx.h>
#include <sys/uio.h>

#include <net/vnet.h>

#include <dev/hyperv/vmbus/vmbus_reg.h>

#include "hv_sock.h"

#define HVSOCK_DBG_NONE			0x0
#define HVSOCK_DBG_INFO			0x1
#define HVSOCK_DBG_ERR			0x2
#define HVSOCK_DBG_VERBOSE		0x3


SYSCTL_NODE(_net, OID_AUTO, hvsock, CTLFLAG_RD, 0, "HyperV socket");

static int hvs_dbg_level;
SYSCTL_INT(_net_hvsock, OID_AUTO, hvs_dbg_level, CTLFLAG_RWTUN, &hvs_dbg_level,
    0, "hyperv socket debug level: 0 = none, 1 = info, 2 = error, 3 = verbose");


#define HVSOCK_DBG(level, ...) do {					\
	if (hvs_dbg_level >= (level))					\
		printf(__VA_ARGS__);					\
	} while (0)

MALLOC_DEFINE(M_HVSOCK, "hyperv_socket", "hyperv socket control structures");

static int hvs_dom_probe(void);

/* The MTU is 16KB per host side's design */
#define HVSOCK_MTU_SIZE		(1024 * 16)
#define HVSOCK_SEND_BUF_SZ	(PAGE_SIZE - sizeof(struct vmpipe_proto_header))

#define HVSOCK_HEADER_LEN	(sizeof(struct hvs_pkt_header))

#define HVSOCK_PKT_LEN(payload_len)	(HVSOCK_HEADER_LEN + \
					 roundup2(payload_len, 8) + \
					 sizeof(uint64_t))


static struct domain		hv_socket_domain;

/*
 * HyperV Transport sockets
 */
static struct pr_usrreqs	hvs_trans_usrreqs = {
	.pru_attach =		hvs_trans_attach,
	.pru_bind =		hvs_trans_bind,
	.pru_listen =		hvs_trans_listen,
	.pru_accept =		hvs_trans_accept,
	.pru_connect =		hvs_trans_connect,
	.pru_peeraddr =		hvs_trans_peeraddr,
	.pru_sockaddr =		hvs_trans_sockaddr,
	.pru_soreceive =	hvs_trans_soreceive,
	.pru_sosend =		hvs_trans_sosend,
	.pru_disconnect =	hvs_trans_disconnect,
	.pru_close =		hvs_trans_close,
	.pru_detach =		hvs_trans_detach,
	.pru_shutdown =		hvs_trans_shutdown,
	.pru_abort =		hvs_trans_abort,
};

/*
 * Definitions of protocols supported in HyperV socket domain
 */
static struct protosw		hv_socket_protosw[] = {
{
	.pr_type =		SOCK_STREAM,
	.pr_domain =		&hv_socket_domain,
	.pr_protocol =		HYPERV_SOCK_PROTO_TRANS,
	.pr_flags =		PR_CONNREQUIRED,
	.pr_init =		hvs_trans_init,
	.pr_usrreqs =		&hvs_trans_usrreqs,
},
};

static struct domain		hv_socket_domain = {
	.dom_family =		AF_HYPERV,
	.dom_name =		"hyperv",
	.dom_probe =		hvs_dom_probe,
	.dom_protosw =		hv_socket_protosw,
	.dom_protoswNPROTOSW =	&hv_socket_protosw[nitems(hv_socket_protosw)]
};

VNET_DOMAIN_SET(hv_socket_);

#define MAX_PORT			((uint32_t)0xFFFFFFFF)
#define MIN_PORT			((uint32_t)0x0)

/* 00000000-facb-11e6-bd58-64006a7986d3 */
static const struct hyperv_guid srv_id_template = {
	.hv_guid = {
	    0x00, 0x00, 0x00, 0x00, 0xcb, 0xfa, 0xe6, 0x11,
	    0xbd, 0x58, 0x64, 0x00, 0x6a, 0x79, 0x86, 0xd3 }
};

static int		hvsock_br_callback(void *, int, void *);
static uint32_t		hvsock_canread_check(struct hvs_pcb *);
static uint32_t		hvsock_canwrite_check(struct hvs_pcb *);
static int		hvsock_send_data(struct vmbus_channel *chan,
    struct uio *uio, uint32_t to_write, struct sockbuf *sb);



/* Globals */
static struct sx		hvs_trans_socks_sx;
static struct mtx		hvs_trans_socks_mtx;
static LIST_HEAD(, hvs_pcb)	hvs_trans_bound_socks;
static LIST_HEAD(, hvs_pcb)	hvs_trans_connected_socks;
static uint32_t			previous_auto_bound_port;

static void
hvsock_print_guid(struct hyperv_guid *guid)
{
	unsigned char *p = (unsigned char *)guid;

	HVSOCK_DBG(HVSOCK_DBG_INFO,
	    "0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x\n",
	    *(unsigned int *)p,
	    *((unsigned short *) &p[4]),
	    *((unsigned short *) &p[6]),
	    p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
}

static bool
is_valid_srv_id(const struct hyperv_guid *id)
{
	return !memcmp(&id->hv_guid[4],
	    &srv_id_template.hv_guid[4], sizeof(struct hyperv_guid) - 4);
}

static unsigned int
get_port_by_srv_id(const struct hyperv_guid *srv_id)
{
	return *((const unsigned int *)srv_id);
}

static void
set_port_by_srv_id(struct hyperv_guid *srv_id, unsigned int port)
{
	*((unsigned int *)srv_id) = port;
}


static void
__hvs_remove_pcb_from_list(struct hvs_pcb *pcb, unsigned char list)
{
	struct hvs_pcb *p = NULL;

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE, "%s: pcb is %p\n", __func__, pcb);

	if (!pcb)
		return;

	if (list & HVS_LIST_BOUND) {
		LIST_FOREACH(p, &hvs_trans_bound_socks, bound_next)
			if  (p == pcb)
				LIST_REMOVE(p, bound_next);
	}

	if (list & HVS_LIST_CONNECTED) {
		LIST_FOREACH(p, &hvs_trans_connected_socks, connected_next)
			if (p == pcb)
				LIST_REMOVE(pcb, connected_next);
	}
}

static void
__hvs_remove_socket_from_list(struct socket *so, unsigned char list)
{
	struct hvs_pcb *pcb = so2hvspcb(so);

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE, "%s: pcb is %p\n", __func__, pcb);

	__hvs_remove_pcb_from_list(pcb, list);
}

static void
__hvs_insert_socket_on_list(struct socket *so, unsigned char list)
{
	struct hvs_pcb *pcb = so2hvspcb(so);

	if (list & HVS_LIST_BOUND)
		LIST_INSERT_HEAD(&hvs_trans_bound_socks,
		   pcb, bound_next);

	if (list & HVS_LIST_CONNECTED)
		LIST_INSERT_HEAD(&hvs_trans_connected_socks,
		   pcb, connected_next);
}

void
hvs_remove_socket_from_list(struct socket *so, unsigned char list)
{
	if (!so || !so->so_pcb) {
		HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
		    "%s: socket or so_pcb is null\n", __func__);
		return;
	}

	mtx_lock(&hvs_trans_socks_mtx);
	__hvs_remove_socket_from_list(so, list);
	mtx_unlock(&hvs_trans_socks_mtx);
}

static void
hvs_insert_socket_on_list(struct socket *so, unsigned char list)
{
	if (!so || !so->so_pcb) {
		HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
		    "%s: socket or so_pcb is null\n", __func__);
		return;
	}

	mtx_lock(&hvs_trans_socks_mtx);
	__hvs_insert_socket_on_list(so, list);
	mtx_unlock(&hvs_trans_socks_mtx);
}

static struct socket *
__hvs_find_socket_on_list(struct sockaddr_hvs *addr, unsigned char list)
{
	struct hvs_pcb *p = NULL;

	if (list & HVS_LIST_BOUND)
		LIST_FOREACH(p, &hvs_trans_bound_socks, bound_next)
			if (p->so != NULL &&
			    addr->hvs_port == p->local_addr.hvs_port)
				return p->so;

	if (list & HVS_LIST_CONNECTED)
		LIST_FOREACH(p, &hvs_trans_connected_socks, connected_next)
			if (p->so != NULL &&
			    addr->hvs_port == p->local_addr.hvs_port)
				return p->so;

	return NULL;
}

static struct socket *
hvs_find_socket_on_list(struct sockaddr_hvs *addr, unsigned char list)
{
	struct socket *s = NULL;

	mtx_lock(&hvs_trans_socks_mtx);
	s = __hvs_find_socket_on_list(addr, list);
	mtx_unlock(&hvs_trans_socks_mtx);

	return s;
}

static inline void
hvs_addr_set(struct sockaddr_hvs *addr, unsigned int port)
{
	memset(addr, 0, sizeof(*addr));
	addr->sa_family = AF_HYPERV;
	addr->sa_len = sizeof(*addr);
	addr->hvs_port = port;
}

void
hvs_addr_init(struct sockaddr_hvs *addr, const struct hyperv_guid *svr_id)
{
	hvs_addr_set(addr, get_port_by_srv_id(svr_id));
}

int
hvs_trans_lock(void)
{
	sx_xlock(&hvs_trans_socks_sx);
	return (0);
}

void
hvs_trans_unlock(void)
{
	sx_xunlock(&hvs_trans_socks_sx);
}

static int
hvs_dom_probe(void)
{

	/* Don't even give us a chance to attach on non-HyperV. */
	if (vm_guest != VM_GUEST_HV)
		return (ENXIO);
	return (0);
}

void
hvs_trans_init(void)
{
	/* Skip initialization of globals for non-default instances. */
	if (!IS_DEFAULT_VNET(curvnet))
		return;

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: HyperV Socket hvs_trans_init called\n", __func__);

	/* Initialize Globals */
	previous_auto_bound_port = MAX_PORT;
	sx_init(&hvs_trans_socks_sx, "hvs_trans_sock_sx");
	mtx_init(&hvs_trans_socks_mtx,
	    "hvs_trans_socks_mtx", NULL, MTX_DEF);
	LIST_INIT(&hvs_trans_bound_socks);
	LIST_INIT(&hvs_trans_connected_socks);
}

/*
 * Called in two cases:
 * 1) When user calls socket();
 * 2) When we accept new incoming conneciton and call sonewconn().
 */
int
hvs_trans_attach(struct socket *so, int proto, struct thread *td)
{
	struct hvs_pcb *pcb = so2hvspcb(so);

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: HyperV Socket hvs_trans_attach called\n", __func__);

	if (so->so_type != SOCK_STREAM)
		return (ESOCKTNOSUPPORT);

	if (proto != 0 && proto != HYPERV_SOCK_PROTO_TRANS)
		return (EPROTONOSUPPORT);

	if (pcb != NULL)
		return (EISCONN);
	pcb = malloc(sizeof(struct hvs_pcb), M_HVSOCK, M_NOWAIT | M_ZERO);
	if (pcb == NULL)
		return (ENOMEM);

	pcb->so = so;
	so->so_pcb = (void *)pcb;

	return (0);
}

void
hvs_trans_detach(struct socket *so)
{
	struct hvs_pcb *pcb;

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: HyperV Socket hvs_trans_detach called\n", __func__);

	(void) hvs_trans_lock();
	pcb = so2hvspcb(so);
	if (pcb == NULL) {
		hvs_trans_unlock();
		return;
	}

	if (SOLISTENING(so)) {
		bzero(pcb, sizeof(*pcb));
		free(pcb, M_HVSOCK);
	}

	so->so_pcb = NULL;

	hvs_trans_unlock();
}

int
hvs_trans_bind(struct socket *so, struct sockaddr *addr, struct thread *td)
{
	struct hvs_pcb *pcb = so2hvspcb(so);
	struct sockaddr_hvs *sa = (struct sockaddr_hvs *) addr;
	int error = 0;

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: HyperV Socket hvs_trans_bind called\n", __func__);

	if (sa == NULL) {
		return (EINVAL);
	}

	if (pcb == NULL) {
		return (EINVAL);
	}

	if (sa->sa_family != AF_HYPERV) {
		HVSOCK_DBG(HVSOCK_DBG_ERR,
		    "%s: Not supported, sa_family is %u\n",
		    __func__, sa->sa_family);
		return (EAFNOSUPPORT);
	}
	if (sa->sa_len != sizeof(*sa)) {
		HVSOCK_DBG(HVSOCK_DBG_ERR,
		    "%s: Not supported, sa_len is %u\n",
		    __func__, sa->sa_len);
		return (EINVAL);
	}

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: binding port = 0x%x\n", __func__, sa->hvs_port);

	mtx_lock(&hvs_trans_socks_mtx);
	if (__hvs_find_socket_on_list(sa,
	    HVS_LIST_BOUND | HVS_LIST_CONNECTED)) {
		error = EADDRINUSE;
	} else {
		/*
		 * The address is available for us to bind.
		 * Add socket to the bound list.
		 */
		hvs_addr_set(&pcb->local_addr, sa->hvs_port);
		hvs_addr_set(&pcb->remote_addr, HVADDR_PORT_ANY);
		__hvs_insert_socket_on_list(so, HVS_LIST_BOUND);
	}
	mtx_unlock(&hvs_trans_socks_mtx);

	return (error);
}

int
hvs_trans_listen(struct socket *so, int backlog, struct thread *td)
{
	struct hvs_pcb *pcb = so2hvspcb(so);
	struct socket *bound_so;
	int error;

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: HyperV Socket hvs_trans_listen called\n", __func__);

	if (pcb == NULL)
		return (EINVAL);

	/* Check if the address is already bound and it was by us. */
	bound_so = hvs_find_socket_on_list(&pcb->local_addr, HVS_LIST_BOUND);
	if (bound_so == NULL || bound_so != so) {
		HVSOCK_DBG(HVSOCK_DBG_ERR,
		    "%s: Address not bound or not by us.\n", __func__);
		return (EADDRNOTAVAIL);
	}

	SOCK_LOCK(so);
	error = solisten_proto_check(so);
	if (error == 0)
		solisten_proto(so, backlog);
	SOCK_UNLOCK(so);

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: HyperV Socket listen error = %d\n", __func__, error);
	return (error);
}

int
hvs_trans_accept(struct socket *so, struct sockaddr **nam)
{
	struct hvs_pcb *pcb = so2hvspcb(so);

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: HyperV Socket hvs_trans_accept called\n", __func__);

	if (pcb == NULL)
		return (EINVAL);

	*nam = sodupsockaddr((struct sockaddr *) &pcb->remote_addr,
	    M_NOWAIT);

	return ((*nam == NULL) ? ENOMEM : 0);
}

int
hvs_trans_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct hvs_pcb *pcb = so2hvspcb(so);
	struct sockaddr_hvs *raddr = (struct sockaddr_hvs *)nam;
	bool found_auto_bound_port = false;
	int i, error = 0;

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: HyperV Socket hvs_trans_connect called, remote port is %x\n",
	    __func__, raddr->hvs_port);

	if (pcb == NULL)
		return (EINVAL);

	/* Verify the remote address */
	if (raddr == NULL)
		return (EINVAL);
	if (raddr->sa_family != AF_HYPERV)
		return (EAFNOSUPPORT);
	if (raddr->sa_len != sizeof(*raddr))
		return (EINVAL);

	mtx_lock(&hvs_trans_socks_mtx);
	if (so->so_state &
	    (SS_ISCONNECTED|SS_ISDISCONNECTING|SS_ISCONNECTING)) {
			HVSOCK_DBG(HVSOCK_DBG_ERR,
			    "%s: socket connect in progress\n",
			    __func__);
			error = EINPROGRESS;
			goto out;
	}

	/*
	 * Find an available port for us to auto bind the local
	 * address.
	 */
	hvs_addr_set(&pcb->local_addr, 0);

	for (i = previous_auto_bound_port - 1;
	    i != previous_auto_bound_port; i --) {
		if (i == MIN_PORT)
			i = MAX_PORT;

		pcb->local_addr.hvs_port = i;

		if (__hvs_find_socket_on_list(&pcb->local_addr,
		    HVS_LIST_BOUND | HVS_LIST_CONNECTED) == NULL) {
			found_auto_bound_port = true;
			previous_auto_bound_port = i;
			HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
			    "%s: found local bound port is %x\n",
			    __func__, pcb->local_addr.hvs_port);
			break;
		}
	}

	if (found_auto_bound_port == true) {
		/* Found available port for auto bound, put on list */
		__hvs_insert_socket_on_list(so, HVS_LIST_BOUND);
		/* Set VM service ID */
		pcb->vm_srv_id = srv_id_template;
		set_port_by_srv_id(&pcb->vm_srv_id, pcb->local_addr.hvs_port);
		/* Set host service ID and remote port */
		pcb->host_srv_id = srv_id_template;
		set_port_by_srv_id(&pcb->host_srv_id, raddr->hvs_port);
		hvs_addr_set(&pcb->remote_addr, raddr->hvs_port);

		/* Change the socket state to SS_ISCONNECTING */
		soisconnecting(so);
	} else {
		HVSOCK_DBG(HVSOCK_DBG_ERR,
		    "%s: No local port available for auto bound\n",
		    __func__);
		error = EADDRINUSE;
	}

	HVSOCK_DBG(HVSOCK_DBG_INFO, "Connect vm_srv_id is ");
	hvsock_print_guid(&pcb->vm_srv_id);
	HVSOCK_DBG(HVSOCK_DBG_INFO, "Connect host_srv_id is ");
	hvsock_print_guid(&pcb->host_srv_id);

out:
	mtx_unlock(&hvs_trans_socks_mtx);

	if (found_auto_bound_port == true)
		 vmbus_req_tl_connect(&pcb->vm_srv_id, &pcb->host_srv_id);

	return (error);
}

int
hvs_trans_disconnect(struct socket *so)
{
	struct hvs_pcb *pcb;

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: HyperV Socket hvs_trans_disconnect called\n", __func__);

	(void) hvs_trans_lock();
	pcb = so2hvspcb(so);
	if (pcb == NULL) {
		hvs_trans_unlock();
		return (EINVAL);
	}

	/* If socket is already disconnected, skip this */
	if ((so->so_state & SS_ISDISCONNECTED) == 0)
		soisdisconnecting(so);

	hvs_trans_unlock();

	return (0);
}

#define SBLOCKWAIT(f)	(((f) & MSG_DONTWAIT) ? 0 : SBL_WAIT)
struct hvs_callback_arg {
	struct uio *uio;
	struct sockbuf *sb;
};

int
hvs_trans_soreceive(struct socket *so, struct sockaddr **paddr,
    struct uio *uio, struct mbuf **mp0, struct mbuf **controlp, int *flagsp)
{
	struct hvs_pcb *pcb = so2hvspcb(so);
	struct sockbuf *sb;
	ssize_t orig_resid;
	uint32_t canread, to_read;
	int flags, error = 0;
	struct hvs_callback_arg cbarg;

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: HyperV Socket hvs_trans_soreceive called\n", __func__);

	if (so->so_type != SOCK_STREAM)
		return (EINVAL);
	if (pcb == NULL)
		return (EINVAL);

	if (flagsp != NULL)
		flags = *flagsp &~ MSG_EOR;
	else
		flags = 0;

	if (flags & MSG_PEEK)
		return (EOPNOTSUPP);

	/* If no space to copy out anything */
	if (uio->uio_resid == 0 || uio->uio_rw != UIO_READ)
		return (EINVAL);

	orig_resid = uio->uio_resid;

	/* Prevent other readers from entering the socket. */
	error = SOCK_IO_RECV_LOCK(so, SBLOCKWAIT(flags));
	if (error) {
		HVSOCK_DBG(HVSOCK_DBG_ERR,
		    "%s: soiolock returned error = %d\n", __func__, error);
		return (error);
	}

	sb = &so->so_rcv;
	SOCKBUF_LOCK(sb);

	cbarg.uio = uio;
	cbarg.sb = sb;
	/*
	 * If the socket is closing, there might still be some data
	 * in rx br to read. However we need to make sure
	 * the channel is still open.
	 */
	if ((sb->sb_state & SBS_CANTRCVMORE) &&
	    (so->so_state & SS_ISDISCONNECTED)) {
		/* Other thread already closed the channel */
		error = EPIPE;
		goto out;
	}

	while (true) {
		while (uio->uio_resid > 0 &&
		    (canread = hvsock_canread_check(pcb)) > 0) {
			to_read = MIN(canread, uio->uio_resid);
			HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
			    "%s: to_read = %u, skip = %u\n", __func__, to_read,
			    (unsigned int)(sizeof(struct hvs_pkt_header) +
			    pcb->recv_data_off));

			error = vmbus_chan_recv_peek_call(pcb->chan, to_read,
			    sizeof(struct hvs_pkt_header) + pcb->recv_data_off,
			    hvsock_br_callback, (void *)&cbarg);
			/*
			 * It is possible socket is disconnected becasue
			 * we released lock in hvsock_br_callback. So we
			 * need to check the state to make sure it is not
			 * disconnected.
			 */
			if (error || so->so_state & SS_ISDISCONNECTED) {
				break;
			}

			pcb->recv_data_len -= to_read;
			pcb->recv_data_off += to_read;
		}

		if (error)
			break;

		/* Abort if socket has reported problems. */
		if (so->so_error) {
			if (so->so_error == ESHUTDOWN &&
			    orig_resid > uio->uio_resid) {
				/*
				 * Although we got a FIN, we also received
				 * some data in this round. Delivery it
				 * to user.
				 */
				error = 0;
			} else {
				if (so->so_error != ESHUTDOWN)
					error = so->so_error;
			}

			break;
		}

		/* Cannot received more. */
		if (sb->sb_state & SBS_CANTRCVMORE)
			break;

		/* We are done if buffer has been filled */
		if (uio->uio_resid == 0)
			break;

		if (!(flags & MSG_WAITALL) && orig_resid > uio->uio_resid)
			break;

		/* Buffer ring is empty and we shall not block */
		if ((so->so_state & SS_NBIO) ||
		    (flags & (MSG_DONTWAIT|MSG_NBIO))) {
			if (orig_resid == uio->uio_resid) {
				/* We have not read anything */
				error = EAGAIN;
			}
			HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
			    "%s: non blocked read return, error %d.\n",
			    __func__, error);
			break;
		}

		/*
		 * Wait and block until (more) data comes in.
		 * Note: Drops the sockbuf lock during wait.
		 */
		error = sbwait(sb);

		if (error)
			break;

		HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
		    "%s: wake up from sbwait, read available is %u\n",
		    __func__, vmbus_chan_read_available(pcb->chan));
	}

out:
	SOCKBUF_UNLOCK(sb);
	SOCK_IO_RECV_UNLOCK(so);

	/* We recieved a FIN in this call */
	if (so->so_error == ESHUTDOWN) {
		if (so->so_snd.sb_state & SBS_CANTSENDMORE) {
			/* Send has already closed */
			soisdisconnecting(so);
		} else {
			/* Just close the receive side */
			socantrcvmore(so);
		}
	}

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: returning error = %d, so_error = %d\n",
	    __func__, error, so->so_error);

	return (error);
}

int
hvs_trans_sosend(struct socket *so, struct sockaddr *addr, struct uio *uio,
    struct mbuf *top, struct mbuf *controlp, int flags, struct thread *td)
{
	struct hvs_pcb *pcb = so2hvspcb(so);
	struct sockbuf *sb;
	ssize_t orig_resid;
	uint32_t canwrite, to_write;
	int error = 0;

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: HyperV Socket hvs_trans_sosend called, uio_resid = %zd\n",
	    __func__, uio->uio_resid);

	if (so->so_type != SOCK_STREAM)
		return (EINVAL);
	if (pcb == NULL)
		return (EINVAL);

	/* If nothing to send */
	if (uio->uio_resid == 0 || uio->uio_rw != UIO_WRITE)
		return (EINVAL);

	orig_resid = uio->uio_resid;

	/* Prevent other writers from entering the socket. */
	error = SOCK_IO_SEND_LOCK(so, SBLOCKWAIT(flags));
	if (error) {
		HVSOCK_DBG(HVSOCK_DBG_ERR,
		    "%s: soiolocak returned error = %d\n", __func__, error);
		return (error);
	}

	sb = &so->so_snd;
	SOCKBUF_LOCK(sb);

	if ((sb->sb_state & SBS_CANTSENDMORE) ||
	    so->so_error == ESHUTDOWN) {
		error = EPIPE;
		goto out;
	}

	while (uio->uio_resid > 0) {
		canwrite = hvsock_canwrite_check(pcb);
		if (canwrite == 0) {
			/* We have sent some data */
			if (orig_resid > uio->uio_resid)
				break;
			/*
			 * We have not sent any data and it is
			 * non-blocked io
			 */
			if (so->so_state & SS_NBIO ||
			    (flags & (MSG_NBIO | MSG_DONTWAIT)) != 0) {
				error = EWOULDBLOCK;
				break;
			} else {
				/*
				 * We are here because there is no space on
				 * send buffer ring. Signal the other side
				 * to read and free more space.
				 * Sleep wait until space avaiable to send
				 * Note: Drops the sockbuf lock during wait.
				 */
				error = sbwait(sb);

				if (error)
					break;

				HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
				    "%s: wake up from sbwait, space avail on "
				    "tx ring is %u\n",
				    __func__,
				    vmbus_chan_write_available(pcb->chan));

				continue;
			}
		}
		to_write = MIN(canwrite, uio->uio_resid);
		to_write = MIN(to_write, HVSOCK_SEND_BUF_SZ);

		HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
		    "%s: canwrite is %u, to_write = %u\n", __func__,
		    canwrite, to_write);
		error = hvsock_send_data(pcb->chan, uio, to_write, sb);

		if (error)
			break;
	}

out:
	SOCKBUF_UNLOCK(sb);
	SOCK_IO_SEND_UNLOCK(so);

	return (error);
}

int
hvs_trans_peeraddr(struct socket *so, struct sockaddr **nam)
{
	struct hvs_pcb *pcb = so2hvspcb(so);

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: HyperV Socket hvs_trans_peeraddr called\n", __func__);

	if (pcb == NULL)
		return (EINVAL);

	*nam = sodupsockaddr((struct sockaddr *) &pcb->remote_addr, M_NOWAIT);

	return ((*nam == NULL)? ENOMEM : 0);
}

int
hvs_trans_sockaddr(struct socket *so, struct sockaddr **nam)
{
	struct hvs_pcb *pcb = so2hvspcb(so);

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: HyperV Socket hvs_trans_sockaddr called\n", __func__);

	if (pcb == NULL)
		return (EINVAL);

	*nam = sodupsockaddr((struct sockaddr *) &pcb->local_addr, M_NOWAIT);

	return ((*nam == NULL)? ENOMEM : 0);
}

void
hvs_trans_close(struct socket *so)
{
	struct hvs_pcb *pcb;

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: HyperV Socket hvs_trans_close called\n", __func__);

	(void) hvs_trans_lock();
	pcb = so2hvspcb(so);
	if (!pcb) {
		hvs_trans_unlock();
		return;
	}

	if (so->so_state & SS_ISCONNECTED) {
		/* Send a FIN to peer */
		HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
		    "%s: hvs_trans_close sending a FIN to host\n", __func__);
		(void) hvsock_send_data(pcb->chan, NULL, 0, NULL);
	}

	if (so->so_state &
	    (SS_ISCONNECTED|SS_ISCONNECTING|SS_ISDISCONNECTING))
		soisdisconnected(so);

	pcb->chan = NULL;
	pcb->so = NULL;

	if (SOLISTENING(so)) {
		mtx_lock(&hvs_trans_socks_mtx);
		/* Remove from bound list */
		__hvs_remove_socket_from_list(so, HVS_LIST_BOUND);
		mtx_unlock(&hvs_trans_socks_mtx);
	}

	hvs_trans_unlock();

	return;
}

void
hvs_trans_abort(struct socket *so)
{
	struct hvs_pcb *pcb = so2hvspcb(so);

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: HyperV Socket hvs_trans_abort called\n", __func__);

	(void) hvs_trans_lock();
	if (pcb == NULL) {
		hvs_trans_unlock();
		return;
	}

	if (SOLISTENING(so)) {
		mtx_lock(&hvs_trans_socks_mtx);
		/* Remove from bound list */
		__hvs_remove_socket_from_list(so, HVS_LIST_BOUND);
		mtx_unlock(&hvs_trans_socks_mtx);
	}

	if (so->so_state & SS_ISCONNECTED) {
		(void) sodisconnect(so);
	}
	hvs_trans_unlock();

	return;
}

int
hvs_trans_shutdown(struct socket *so)
{
	struct hvs_pcb *pcb = so2hvspcb(so);
	struct sockbuf *sb;

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: HyperV Socket hvs_trans_shutdown called\n", __func__);

	if (pcb == NULL)
		return (EINVAL);

	/*
	 * Only get called with the shutdown method is SHUT_WR or
	 * SHUT_RDWR.
	 * When the method is SHUT_RD or SHUT_RDWR, the caller
	 * already set the SBS_CANTRCVMORE on receive side socket
	 * buffer.
	 */
	if ((so->so_rcv.sb_state & SBS_CANTRCVMORE) == 0) {
		/*
		 * SHUT_WR only case.
		 * Receive side is still open. Just close
		 * the send side.
		 */
		socantsendmore(so);
	} else {
		/* SHUT_RDWR case */
		if (so->so_state & SS_ISCONNECTED) {
			/* Send a FIN to peer */
			sb = &so->so_snd;
			SOCKBUF_LOCK(sb);
			(void) hvsock_send_data(pcb->chan, NULL, 0, sb);
			SOCKBUF_UNLOCK(sb);

			soisdisconnecting(so);
		}
	}

	return (0);
}

/* In the VM, we support Hyper-V Sockets with AF_HYPERV, and the endpoint is
 * <port> (see struct sockaddr_hvs).
 *
 * On the host, Hyper-V Sockets are supported by Winsock AF_HYPERV:
 * https://docs.microsoft.com/en-us/virtualization/hyper-v-on-windows/user-
 * guide/make-integration-service, and the endpoint is <VmID, ServiceId> with
 * the below sockaddr:
 *
 * struct SOCKADDR_HV
 * {
 *    ADDRESS_FAMILY Family;
 *    USHORT Reserved;
 *    GUID VmId;
 *    GUID ServiceId;
 * };
 * Note: VmID is not used by FreeBSD VM and actually it isn't transmitted via
 * VMBus, because here it's obvious the host and the VM can easily identify
 * each other. Though the VmID is useful on the host, especially in the case
 * of Windows container, FreeBSD VM doesn't need it at all.
 *
 * To be compatible with similar infrastructure in Linux VMs, we have
 * to limit the available GUID space of SOCKADDR_HV so that we can create
 * a mapping between FreeBSD AF_HYPERV port and SOCKADDR_HV Service GUID.
 * The rule of writing Hyper-V Sockets apps on the host and in FreeBSD VM is:
 *
 ****************************************************************************
 * The only valid Service GUIDs, from the perspectives of both the host and *
 * FreeBSD VM, that can be connected by the other end, must conform to this *
 * format: <port>-facb-11e6-bd58-64006a7986d3.                              *
 ****************************************************************************
 *
 * When we write apps on the host to connect(), the GUID ServiceID is used.
 * When we write apps in FreeBSD VM to connect(), we only need to specify the
 * port and the driver will form the GUID and use that to request the host.
 *
 * From the perspective of FreeBSD VM, the remote ephemeral port (i.e. the
 * auto-generated remote port for a connect request initiated by the host's
 * connect()) is set to HVADDR_PORT_UNKNOWN, which is not realy used on the
 * FreeBSD guest.
 */

/*
 * Older HyperV hosts (vmbus version 'VMBUS_VERSION_WIN10' or before)
 * restricts HyperV socket ring buffer size to six 4K pages. Newer
 * HyperV hosts doen't have this limit.
 */
#define HVS_RINGBUF_RCV_SIZE	(PAGE_SIZE * 6)
#define HVS_RINGBUF_SND_SIZE	(PAGE_SIZE * 6)
#define HVS_RINGBUF_MAX_SIZE	(PAGE_SIZE * 64)

struct hvsock_sc {
	device_t		dev;
	struct hvs_pcb		*pcb;
	struct vmbus_channel	*channel;
};

static bool
hvsock_chan_readable(struct vmbus_channel *chan)
{
	uint32_t readable = vmbus_chan_read_available(chan);

	return (readable >= HVSOCK_PKT_LEN(0));
}

static void
hvsock_chan_cb(struct vmbus_channel *chan, void *context)
{
	struct hvs_pcb *pcb = (struct hvs_pcb *) context;
	struct socket *so;
	uint32_t canwrite;

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: host send us a wakeup on rb data, pcb = %p\n",
	    __func__, pcb);

	/*
	 * Check if the socket is still attached and valid.
	 * Here we know channel is still open. Need to make
	 * sure the socket has not been closed or freed.
	 */
	(void) hvs_trans_lock();
	so = hsvpcb2so(pcb);

	if (pcb->chan != NULL && so != NULL) {
		/*
		 * Wake up reader if there are data to read.
		 */
		SOCKBUF_LOCK(&(so)->so_rcv);

		HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
		    "%s: read available = %u\n", __func__,
		    vmbus_chan_read_available(pcb->chan));

		if (hvsock_chan_readable(pcb->chan))
			sorwakeup_locked(so);
		else
			SOCKBUF_UNLOCK(&(so)->so_rcv);

		/*
		 * Wake up sender if space becomes available to write.
		 */
		SOCKBUF_LOCK(&(so)->so_snd);
		canwrite = hvsock_canwrite_check(pcb);

		HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
		    "%s: canwrite = %u\n", __func__, canwrite);

		if (canwrite > 0) {
			sowwakeup_locked(so);
		} else {
			SOCKBUF_UNLOCK(&(so)->so_snd);
		}
	}

	hvs_trans_unlock();

	return;
}

static int
hvsock_br_callback(void *datap, int cplen, void *cbarg)
{
	struct hvs_callback_arg *arg = (struct hvs_callback_arg *)cbarg;
	struct uio *uio = arg->uio;
	struct sockbuf *sb = arg->sb;
	int error = 0;

	if (cbarg == NULL || datap == NULL)
		return (EINVAL);

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: called, uio_rw = %s, uio_resid = %zd, cplen = %u, "
	    "datap = %p\n",
	    __func__, (uio->uio_rw == UIO_READ) ? "read from br":"write to br",
	    uio->uio_resid, cplen, datap);

	if (sb)
		SOCKBUF_UNLOCK(sb);

	error = uiomove(datap, cplen, uio);

	if (sb)
		SOCKBUF_LOCK(sb);

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: after uiomove, uio_resid = %zd, error = %d\n",
	    __func__, uio->uio_resid, error);

	return (error);
}

static int
hvsock_send_data(struct vmbus_channel *chan, struct uio *uio,
    uint32_t to_write, struct sockbuf *sb)
{
	struct hvs_pkt_header hvs_pkt;
	int hvs_pkthlen, hvs_pktlen, pad_pktlen, hlen, error = 0;
	uint64_t pad = 0;
	struct iovec iov[3];
	struct hvs_callback_arg cbarg;

	if (chan == NULL)
		return (ENOTCONN);

	hlen = sizeof(struct vmbus_chanpkt_hdr);
	hvs_pkthlen = sizeof(struct hvs_pkt_header);
	hvs_pktlen = hvs_pkthlen + to_write;
	pad_pktlen = VMBUS_CHANPKT_TOTLEN(hvs_pktlen);

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: hlen = %u, hvs_pkthlen = %u, hvs_pktlen = %u, "
	    "pad_pktlen = %u, data_len = %u\n",
	    __func__, hlen, hvs_pkthlen, hvs_pktlen, pad_pktlen, to_write);

	hvs_pkt.chan_pkt_hdr.cph_type = VMBUS_CHANPKT_TYPE_INBAND;
	hvs_pkt.chan_pkt_hdr.cph_flags = 0;
	VMBUS_CHANPKT_SETLEN(hvs_pkt.chan_pkt_hdr.cph_hlen, hlen);
	VMBUS_CHANPKT_SETLEN(hvs_pkt.chan_pkt_hdr.cph_tlen, pad_pktlen);
	hvs_pkt.chan_pkt_hdr.cph_xactid = 0;

	hvs_pkt.vmpipe_pkt_hdr.vmpipe_pkt_type = 1;
	hvs_pkt.vmpipe_pkt_hdr.vmpipe_data_size = to_write;

	cbarg.uio = uio;
	cbarg.sb = sb;

	if (uio && to_write > 0) {
		iov[0].iov_base = &hvs_pkt;
		iov[0].iov_len = hvs_pkthlen;
		iov[1].iov_base = NULL;
		iov[1].iov_len = to_write;
		iov[2].iov_base = &pad;
		iov[2].iov_len = pad_pktlen - hvs_pktlen;

		error = vmbus_chan_iov_send(chan, iov, 3,
		    hvsock_br_callback, &cbarg);
	} else {
		if (to_write == 0) {
			iov[0].iov_base = &hvs_pkt;
			iov[0].iov_len = hvs_pkthlen;
			iov[1].iov_base = &pad;
			iov[1].iov_len = pad_pktlen - hvs_pktlen;
			error = vmbus_chan_iov_send(chan, iov, 2, NULL, NULL);
		}
	}

	if (error) {
		HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
		    "%s: error = %d\n", __func__, error);
	}

	return (error);
}

/*
 * Check if we have data on current ring buffer to read
 * or not. If not, advance the ring buffer read index to
 * next packet. Update the recev_data_len and recev_data_off
 * to new value.
 * Return the number of bytes can read.
 */
static uint32_t
hvsock_canread_check(struct hvs_pcb *pcb)
{
	uint32_t advance;
	uint32_t tlen, hlen, dlen;
	uint32_t bytes_canread = 0;
	int error;

	if (pcb == NULL || pcb->chan == NULL) {
		pcb->so->so_error = EIO;
		return (0);
	}

	/* Still have data not read yet on current packet */
	if (pcb->recv_data_len > 0)
		return (pcb->recv_data_len);

	if (pcb->rb_init)
		advance =
		    VMBUS_CHANPKT_GETLEN(pcb->hvs_pkt.chan_pkt_hdr.cph_tlen);
	else
		advance = 0;

	bytes_canread = vmbus_chan_read_available(pcb->chan);

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: bytes_canread on br = %u, advance = %u\n",
	    __func__, bytes_canread, advance);

	if (pcb->rb_init && bytes_canread == (advance + sizeof(uint64_t))) {
		/*
		 * Nothing to read. Need to advance the rindex before
		 * calling sbwait, so host knows to wake us up when data
		 * is available to read on rb.
		 */
		error = vmbus_chan_recv_idxadv(pcb->chan, advance);
		if (error) {
			HVSOCK_DBG(HVSOCK_DBG_ERR,
			    "%s: after calling vmbus_chan_recv_idxadv, "
			    "got error = %d\n",  __func__, error);
			return (0);
		} else {
			pcb->rb_init = false;
			pcb->recv_data_len = 0;
			pcb->recv_data_off = 0;
			bytes_canread = vmbus_chan_read_available(pcb->chan);

			HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
			    "%s: advanced %u bytes, "
			    " bytes_canread on br now = %u\n",
			    __func__, advance, bytes_canread);

			if (bytes_canread == 0)
				return (0);
			else
				advance = 0;
		}
	}

	if (bytes_canread <
	    advance + (sizeof(struct hvs_pkt_header) + sizeof(uint64_t)))
		return (0);

	error = vmbus_chan_recv_peek(pcb->chan, &pcb->hvs_pkt,
	    sizeof(struct hvs_pkt_header), advance);

	/* Don't have anything to read */
	if (error) {
		HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
		    "%s: after calling vmbus_chan_recv_peek, got error = %d\n",
		    __func__, error);
		return (0);
	}

	/*
	 * We just read in a new packet header. Do some sanity checks.
	 */
	tlen = VMBUS_CHANPKT_GETLEN(pcb->hvs_pkt.chan_pkt_hdr.cph_tlen);
	hlen = VMBUS_CHANPKT_GETLEN(pcb->hvs_pkt.chan_pkt_hdr.cph_hlen);
	dlen = pcb->hvs_pkt.vmpipe_pkt_hdr.vmpipe_data_size;
	if (__predict_false(hlen < sizeof(struct vmbus_chanpkt_hdr)) ||
	    __predict_false(hlen > tlen) ||
	    __predict_false(tlen < dlen + sizeof(struct hvs_pkt_header))) {
		HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
		    "invalid tlen(%u), hlen(%u) or dlen(%u)\n",
		    tlen, hlen, dlen);
		pcb->so->so_error = EIO;
		return (0);
	}
	if (pcb->rb_init == false)
		pcb->rb_init = true;

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "Got new pkt tlen(%u), hlen(%u) or dlen(%u)\n",
	    tlen, hlen, dlen);

	/* The other side has sent a close FIN */
	if (dlen == 0) {
		HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
		    "%s: Received FIN from other side\n", __func__);
		/* inform the caller by seting so_error to ESHUTDOWN */
		pcb->so->so_error = ESHUTDOWN;
	}

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: canread on receive ring is %u \n", __func__, dlen);

	pcb->recv_data_len = dlen;
	pcb->recv_data_off = 0;

	return (pcb->recv_data_len);
}

static uint32_t
hvsock_canwrite_check(struct hvs_pcb *pcb)
{
	uint32_t writeable;
	uint32_t ret;

	if (pcb == NULL || pcb->chan == NULL)
		return (0);

	writeable = vmbus_chan_write_available(pcb->chan);

	/*
	 * We must always reserve a 0-length-payload packet for the FIN.
	 */
	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: writeable is %u, should be greater than %ju\n",
	    __func__, writeable,
	    (uintmax_t)(HVSOCK_PKT_LEN(1) + HVSOCK_PKT_LEN(0)));

	if (writeable < HVSOCK_PKT_LEN(1) + HVSOCK_PKT_LEN(0)) {
		/*
		 * The Tx ring seems full.
		 */
		return (0);
	}

	ret = writeable - HVSOCK_PKT_LEN(0) - HVSOCK_PKT_LEN(0);

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
	    "%s: available size is %u\n", __func__, rounddown2(ret, 8));

	return (rounddown2(ret, 8));
}

static void
hvsock_set_chan_pending_send_size(struct vmbus_channel *chan)
{
	vmbus_chan_set_pending_send_size(chan,
	    HVSOCK_PKT_LEN(HVSOCK_SEND_BUF_SZ));
}

static int
hvsock_open_channel(struct vmbus_channel *chan, struct socket *so)
{
	unsigned int rcvbuf, sndbuf;
	struct hvs_pcb *pcb = so2hvspcb(so);
	int ret;

	if (vmbus_current_version < VMBUS_VERSION_WIN10_V5) {
		sndbuf = HVS_RINGBUF_SND_SIZE;
		rcvbuf = HVS_RINGBUF_RCV_SIZE;
	} else {
		sndbuf = MAX(so->so_snd.sb_hiwat, HVS_RINGBUF_SND_SIZE);
		sndbuf = MIN(sndbuf, HVS_RINGBUF_MAX_SIZE);
		sndbuf = rounddown2(sndbuf, PAGE_SIZE);
		rcvbuf = MAX(so->so_rcv.sb_hiwat, HVS_RINGBUF_RCV_SIZE);
		rcvbuf = MIN(rcvbuf, HVS_RINGBUF_MAX_SIZE);
		rcvbuf = rounddown2(rcvbuf, PAGE_SIZE);
	}

	/*
	 * Can only read whatever user provided size of data
	 * from ring buffer. Turn off batched reading.
	 */
	vmbus_chan_set_readbatch(chan, false);

	ret = vmbus_chan_open(chan, sndbuf, rcvbuf, NULL, 0,
	    hvsock_chan_cb, pcb);

	if (ret != 0) {
		HVSOCK_DBG(HVSOCK_DBG_ERR,
		    "%s: failed to open hvsock channel, sndbuf = %u, "
		    "rcvbuf = %u\n", __func__, sndbuf, rcvbuf);
	} else {
		HVSOCK_DBG(HVSOCK_DBG_INFO,
		    "%s: hvsock channel opened, sndbuf = %u, i"
		    "rcvbuf = %u\n", __func__, sndbuf, rcvbuf);
		/*
		 * Se the pending send size so to receive wakeup
		 * signals from host when there is enough space on
		 * rx buffer ring to write.
		 */
		hvsock_set_chan_pending_send_size(chan);
	}

	return ret;
}

/*
 * Guest is listening passively on the socket. Open channel and
 * create a new socket for the conneciton.
 */
static void
hvsock_open_conn_passive(struct vmbus_channel *chan, struct socket *so,
    struct hvsock_sc *sc)
{
	struct socket *new_so;
	struct hvs_pcb *new_pcb, *pcb;
	int error;

	/* Do nothing if socket is not listening */
	if (!SOLISTENING(so)) {
		HVSOCK_DBG(HVSOCK_DBG_ERR,
		    "%s: socket is not a listening one\n", __func__);
		return;
	}

	/*
	 * Create a new socket. This will call pru_attach to complete
	 * the socket initialization and put the new socket onto
	 * listening socket's sol_incomp list, waiting to be promoted
	 * to sol_comp list.
	 * The new socket created has ref count 0. There is no other
	 * thread that changes the state of this new one at the
	 * moment, so we don't need to hold its lock while opening
	 * channel and filling out its pcb information.
	 */
	new_so = sonewconn(so, 0);
	if (!new_so)
		HVSOCK_DBG(HVSOCK_DBG_ERR,
		    "%s: creating new socket failed\n", __func__);

	/*
	 * Now open the vmbus channel. If it fails, the socket will be
	 * on the listening socket's sol_incomp queue until it is
	 * replaced and aborted.
	 */
	error = hvsock_open_channel(chan, new_so);
	if (error) {
		new_so->so_error = error;
		return;
	}

	pcb = so->so_pcb;
	new_pcb = new_so->so_pcb;

	hvs_addr_set(&(new_pcb->local_addr), pcb->local_addr.hvs_port);
	/* Remote port is unknown to guest in this type of conneciton */
	hvs_addr_set(&(new_pcb->remote_addr), HVADDR_PORT_UNKNOWN);
	new_pcb->chan = chan;
	new_pcb->recv_data_len = 0;
	new_pcb->recv_data_off = 0;
	new_pcb->rb_init = false;

	new_pcb->vm_srv_id = *vmbus_chan_guid_type(chan);
	new_pcb->host_srv_id = *vmbus_chan_guid_inst(chan);

	hvs_insert_socket_on_list(new_so, HVS_LIST_CONNECTED);

	sc->pcb = new_pcb;

	/*
	 * Change the socket state to SS_ISCONNECTED. This will promote
	 * the socket to sol_comp queue and wake up the thread which
	 * is accepting connection.
	 */
	soisconnected(new_so);
}


/*
 * Guest is actively connecting to host.
 */
static void
hvsock_open_conn_active(struct vmbus_channel *chan, struct socket *so)
{
	struct hvs_pcb *pcb;
	int error;

	error = hvsock_open_channel(chan, so);
	if (error) {
		so->so_error = error;
		return;
	}

	pcb = so->so_pcb;
	pcb->chan = chan;
	pcb->recv_data_len = 0;
	pcb->recv_data_off = 0;
	pcb->rb_init = false;

	mtx_lock(&hvs_trans_socks_mtx);
	__hvs_remove_socket_from_list(so, HVS_LIST_BOUND);
	__hvs_insert_socket_on_list(so, HVS_LIST_CONNECTED);
	mtx_unlock(&hvs_trans_socks_mtx);

	/*
	 * Change the socket state to SS_ISCONNECTED. This will wake up
	 * the thread sleeping in connect call.
	 */
	soisconnected(so);
}

static void
hvsock_open_connection(struct vmbus_channel *chan, struct hvsock_sc *sc)
{
	struct hyperv_guid *inst_guid, *type_guid;
	bool conn_from_host;
	struct sockaddr_hvs addr;
	struct socket *so;
	struct hvs_pcb *pcb;

	type_guid = (struct hyperv_guid *) vmbus_chan_guid_type(chan);
	inst_guid = (struct hyperv_guid *) vmbus_chan_guid_inst(chan);
	conn_from_host = vmbus_chan_is_hvs_conn_from_host(chan);

	HVSOCK_DBG(HVSOCK_DBG_INFO, "type_guid is ");
	hvsock_print_guid(type_guid);
	HVSOCK_DBG(HVSOCK_DBG_INFO, "inst_guid is ");
	hvsock_print_guid(inst_guid);
	HVSOCK_DBG(HVSOCK_DBG_INFO, "connection %s host\n",
	    (conn_from_host == true ) ? "from" : "to");

	/*
	 * The listening port should be in [0, MAX_LISTEN_PORT]
	 */
	if (!is_valid_srv_id(type_guid))
		return;

	/*
	 * There should be a bound socket already created no matter
	 * it is a passive or active connection.
	 * For host initiated connection (passive on guest side),
	 * the  type_guid contains the port which guest is bound and
	 * listening.
	 * For the guest initiated connection (active on guest side),
	 * the inst_guid contains the port that guest has auto bound
	 * to.
	 */
	hvs_addr_init(&addr, conn_from_host ? type_guid : inst_guid);
	so = hvs_find_socket_on_list(&addr, HVS_LIST_BOUND);
	if (!so) {
		HVSOCK_DBG(HVSOCK_DBG_ERR,
		    "%s: no bound socket found for port %u\n",
		    __func__, addr.hvs_port);
		return;
	}

	if (conn_from_host) {
		hvsock_open_conn_passive(chan, so, sc);
	} else {
		(void) hvs_trans_lock();
		pcb = so->so_pcb;
		if (pcb && pcb->so) {
			sc->pcb = so2hvspcb(so);
			hvsock_open_conn_active(chan, so);
		} else {
			HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
			    "%s: channel detached before open\n", __func__);
		}
		hvs_trans_unlock();
	}

}

static int
hvsock_probe(device_t dev)
{
	struct vmbus_channel *channel = vmbus_get_channel(dev);

	if (!channel || !vmbus_chan_is_hvs(channel)) {
		HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
		    "hvsock_probe called but not a hvsock channel id %u\n",
		    vmbus_chan_id(channel));

		return ENXIO;
	} else {
		HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
		    "hvsock_probe got a hvsock channel id %u\n",
		    vmbus_chan_id(channel));

		return BUS_PROBE_DEFAULT;
	}
}

static int
hvsock_attach(device_t dev)
{
	struct vmbus_channel *channel = vmbus_get_channel(dev);
	struct hvsock_sc *sc = (struct hvsock_sc *)device_get_softc(dev);

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE, "hvsock_attach called.\n");

	hvsock_open_connection(channel, sc);

	/*
	 * Always return success. On error the host will rescind the device
	 * in 30 seconds and we can do cleanup at that time in
	 * vmbus_chan_msgproc_chrescind().
	 */
	return (0);
}

static int
hvsock_detach(device_t dev)
{
	struct hvsock_sc *sc = (struct hvsock_sc *)device_get_softc(dev);
	struct socket *so;
	int retry;

	if (bootverbose)
		device_printf(dev, "hvsock_detach called.\n");

	HVSOCK_DBG(HVSOCK_DBG_VERBOSE, "hvsock_detach called.\n");

	if (sc->pcb != NULL) {
		(void) hvs_trans_lock();

		so = hsvpcb2so(sc->pcb);
		if (so) {
			/* Close the connection */
			if (so->so_state &
			    (SS_ISCONNECTED|SS_ISCONNECTING|SS_ISDISCONNECTING))
				soisdisconnected(so);
		}

		mtx_lock(&hvs_trans_socks_mtx);
		__hvs_remove_pcb_from_list(sc->pcb,
		    HVS_LIST_BOUND | HVS_LIST_CONNECTED);
		mtx_unlock(&hvs_trans_socks_mtx);

		/*
		 * Close channel while no reader and sender are working
		 * on the buffer rings.
		 */
		if (so) {
			retry = 0;
			while (SOCK_IO_RECV_LOCK(so, 0) == EWOULDBLOCK) {
				/*
				 * Someone is reading, rx br is busy
				 */
				soisdisconnected(so);
				DELAY(500);
				HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
				    "waiting for rx reader to exit, "
				    "retry = %d\n", retry++);
			}
			retry = 0;
			while (SOCK_IO_SEND_LOCK(so, 0) == EWOULDBLOCK) {
				/*
				 * Someone is sending, tx br is busy
				 */
				soisdisconnected(so);
				DELAY(500);
				HVSOCK_DBG(HVSOCK_DBG_VERBOSE,
				    "waiting for tx sender to exit, "
				    "retry = %d\n", retry++);
			}
		}


		bzero(sc->pcb, sizeof(struct hvs_pcb));
		free(sc->pcb, M_HVSOCK);
		sc->pcb = NULL;

		if (so) {
			SOCK_IO_RECV_UNLOCK(so);
			SOCK_IO_SEND_UNLOCK(so);
			so->so_pcb = NULL;
		}

		hvs_trans_unlock();
	}

	vmbus_chan_close(vmbus_get_channel(dev));

	return (0);
}

static device_method_t hvsock_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, hvsock_probe),
	DEVMETHOD(device_attach, hvsock_attach),
	DEVMETHOD(device_detach, hvsock_detach),
	DEVMETHOD_END
};

static driver_t hvsock_driver = {
	"hv_sock",
	hvsock_methods,
	sizeof(struct hvsock_sc)
};

static devclass_t hvsock_devclass;

DRIVER_MODULE(hvsock, vmbus, hvsock_driver, hvsock_devclass, NULL, NULL);
MODULE_VERSION(hvsock, 1);
MODULE_DEPEND(hvsock, vmbus, 1, 1, 1);
