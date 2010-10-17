/*
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a
 *    copy of which is available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

/***************************************************************************
 *
 *   Module:		 uDAPL
 *
 *   Filename:		 dapl_ib_cm.c
 *
 *   Author:		 Arlin Davis
 *
 *   Created:		 3/10/2005
 *
 *   Description: 
 *
 *   The uDAPL openib provider - connection management
 *
 ****************************************************************************
 *		   Source Control System Information
 *
 *    $Id: $
 *
 *	Copyright (c) 2005 Intel Corporation.  All rights reserved.
 *
 **************************************************************************/

#if defined(_WIN32)
#define FD_SETSIZE 1024
#define DAPL_FD_SETSIZE FD_SETSIZE
#endif

#include "dapl.h"
#include "dapl_adapter_util.h"
#include "dapl_evd_util.h"
#include "dapl_cr_util.h"
#include "dapl_name_service.h"
#include "dapl_ib_util.h"
#include "dapl_osd.h"

#if defined(_WIN32) || defined(_WIN64)
enum DAPL_FD_EVENTS {
	DAPL_FD_READ = 0x1,
	DAPL_FD_WRITE = 0x2,
	DAPL_FD_ERROR = 0x4
};

static int dapl_config_socket(DAPL_SOCKET s)
{
	unsigned long nonblocking = 1;
	int ret, opt = 1;

	ret = ioctlsocket(s, FIONBIO, &nonblocking);

	/* no delay for small packets */
	if (!ret)
		ret = setsockopt(s, IPPROTO_TCP, TCP_NODELAY, 
				 (char *)&opt, sizeof(opt));
	return ret;
}

static int dapl_connect_socket(DAPL_SOCKET s, struct sockaddr *addr,
			       int addrlen)
{
	int err;

	err = connect(s, addr, addrlen);
	if (err == SOCKET_ERROR)
		err = WSAGetLastError();
	return (err == WSAEWOULDBLOCK) ? EAGAIN : err;
}

struct dapl_fd_set {
	struct fd_set set[3];
};

static struct dapl_fd_set *dapl_alloc_fd_set(void)
{
	return dapl_os_alloc(sizeof(struct dapl_fd_set));
}

static void dapl_fd_zero(struct dapl_fd_set *set)
{
	FD_ZERO(&set->set[0]);
	FD_ZERO(&set->set[1]);
	FD_ZERO(&set->set[2]);
}

static int dapl_fd_set(DAPL_SOCKET s, struct dapl_fd_set *set,
		       enum DAPL_FD_EVENTS event)
{
	FD_SET(s, &set->set[(event == DAPL_FD_READ) ? 0 : 1]);
	FD_SET(s, &set->set[2]);
	return 0;
}

static enum DAPL_FD_EVENTS dapl_poll(DAPL_SOCKET s, enum DAPL_FD_EVENTS event)
{
	struct fd_set rw_fds;
	struct fd_set err_fds;
	struct timeval tv;
	int ret;

	FD_ZERO(&rw_fds);
	FD_ZERO(&err_fds);
	FD_SET(s, &rw_fds);
	FD_SET(s, &err_fds);

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	if (event == DAPL_FD_READ)
		ret = select(1, &rw_fds, NULL, &err_fds, &tv);
	else
		ret = select(1, NULL, &rw_fds, &err_fds, &tv);

	if (ret == 0)
		return 0;
	else if (ret == SOCKET_ERROR)
		return DAPL_FD_ERROR;
	else if (FD_ISSET(s, &rw_fds))
		return event;
	else
		return DAPL_FD_ERROR;
}

static int dapl_select(struct dapl_fd_set *set)
{
	int ret;

	dapl_dbg_log(DAPL_DBG_TYPE_CM, " dapl_select: sleep\n");
	ret = select(0, &set->set[0], &set->set[1], &set->set[2], NULL);
	dapl_dbg_log(DAPL_DBG_TYPE_CM, " dapl_select: wakeup\n");

	if (ret == SOCKET_ERROR)
		dapl_dbg_log(DAPL_DBG_TYPE_CM,
			     " dapl_select: error 0x%x\n", WSAGetLastError());

	return ret;
}

static int dapl_socket_errno(void)
{
	int err;

	err = WSAGetLastError();
	switch (err) {
	case WSAEACCES:
	case WSAEADDRINUSE:
		return EADDRINUSE;
	case WSAECONNRESET:
		return ECONNRESET;
	default:
		return err;
	}
}
#else				// _WIN32 || _WIN64
enum DAPL_FD_EVENTS {
	DAPL_FD_READ = POLLIN,
	DAPL_FD_WRITE = POLLOUT,
	DAPL_FD_ERROR = POLLERR
};

static int dapl_config_socket(DAPL_SOCKET s)
{
	int ret, opt = 1;

	/* non-blocking */
	ret = fcntl(s, F_GETFL);
	if (ret >= 0)
		ret = fcntl(s, F_SETFL, ret | O_NONBLOCK);

	/* no delay for small packets */
	if (!ret)
		ret = setsockopt(s, IPPROTO_TCP, TCP_NODELAY, 
				 (char *)&opt, sizeof(opt));
	return ret;
}

static int dapl_connect_socket(DAPL_SOCKET s, struct sockaddr *addr,
			       int addrlen)
{
	int ret;

	ret = connect(s, addr, addrlen);

	return (errno == EINPROGRESS) ? EAGAIN : ret;
}

struct dapl_fd_set {
	int index;
	struct pollfd set[DAPL_FD_SETSIZE];
};

static struct dapl_fd_set *dapl_alloc_fd_set(void)
{
	return dapl_os_alloc(sizeof(struct dapl_fd_set));
}

static void dapl_fd_zero(struct dapl_fd_set *set)
{
	set->index = 0;
}

static int dapl_fd_set(DAPL_SOCKET s, struct dapl_fd_set *set,
		       enum DAPL_FD_EVENTS event)
{
	if (set->index == DAPL_FD_SETSIZE - 1) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 "SCM ERR: cm_thread exceeded FD_SETSIZE %d\n",
			 set->index + 1);
		return -1;
	}

	set->set[set->index].fd = s;
	set->set[set->index].revents = 0;
	set->set[set->index++].events = event;
	return 0;
}

static enum DAPL_FD_EVENTS dapl_poll(DAPL_SOCKET s, enum DAPL_FD_EVENTS event)
{
	struct pollfd fds;
	int ret;

	fds.fd = s;
	fds.events = event;
	fds.revents = 0;
	ret = poll(&fds, 1, 0);
	dapl_log(DAPL_DBG_TYPE_CM, " dapl_poll: fd=%d ret=%d, evnts=0x%x\n",
		 s, ret, fds.revents);
	if (ret == 0)
		return 0;
	else if (ret < 0 || (fds.revents & (POLLERR | POLLHUP | POLLNVAL))) 
		return DAPL_FD_ERROR;
	else 
		return event;
}

static int dapl_select(struct dapl_fd_set *set)
{
	int ret;

	dapl_dbg_log(DAPL_DBG_TYPE_CM, " dapl_select: sleep, fds=%d\n", set->index);
	ret = poll(set->set, set->index, -1);
	dapl_dbg_log(DAPL_DBG_TYPE_CM, " dapl_select: wakeup, ret=0x%x\n", ret);
	return ret;
}

#define dapl_socket_errno() errno
#endif

dp_ib_cm_handle_t dapls_ib_cm_create(DAPL_EP *ep)
{
	dp_ib_cm_handle_t cm_ptr;

	/* Allocate CM, init lock, and initialize */
	if ((cm_ptr = dapl_os_alloc(sizeof(*cm_ptr))) == NULL)
		return NULL;

	(void)dapl_os_memzero(cm_ptr, sizeof(*cm_ptr));
	if (dapl_os_lock_init(&cm_ptr->lock))
		goto bail;

	cm_ptr->msg.ver = htons(DCM_VER);
	cm_ptr->socket = DAPL_INVALID_SOCKET;
	cm_ptr->ep = ep;
	return cm_ptr;
bail:
	dapl_os_free(cm_ptr, sizeof(*cm_ptr));
	return NULL;
}

/* mark for destroy, remove all references, schedule cleanup */
/* cm_ptr == NULL (UD), then multi CR's, kill all associated with EP */
void dapls_ib_cm_free(dp_ib_cm_handle_t cm_ptr, DAPL_EP *ep)
{
	DAPL_IA *ia_ptr;
	DAPL_HCA *hca_ptr = NULL;
	dp_ib_cm_handle_t cr, next_cr;

	dapl_dbg_log(DAPL_DBG_TYPE_CM,
		     " cm_destroy: cm %p ep %p\n", cm_ptr, ep);

	if (cm_ptr == NULL)
		goto multi_cleanup;

	/* to notify cleanup thread */
	hca_ptr = cm_ptr->hca;

	/* cleanup, never made it to work queue */
	dapl_os_lock(&cm_ptr->lock);
	if (cm_ptr->state == DCM_INIT) {
		if (cm_ptr->socket != DAPL_INVALID_SOCKET) {
			shutdown(cm_ptr->socket, SHUT_RDWR);
			closesocket(cm_ptr->socket);
		}
		dapl_os_unlock(&cm_ptr->lock);
		dapl_os_free(cm_ptr, sizeof(*cm_ptr));
		return;
	}

	/* free could be called before disconnect, disc_clean will destroy */
	if (cm_ptr->state == DCM_CONNECTED) {
		dapl_os_unlock(&cm_ptr->lock);
		dapli_socket_disconnect(cm_ptr);
		return;
	}

	cm_ptr->state = DCM_DESTROY;
	if ((cm_ptr->ep) && (cm_ptr->ep->cm_handle == cm_ptr)) {
		cm_ptr->ep->cm_handle = IB_INVALID_HANDLE;
		cm_ptr->ep = NULL;
	}

	dapl_os_unlock(&cm_ptr->lock);
	goto notify_thread;

multi_cleanup:

	/* 
	 * UD CR objects are kept active because of direct private data references
	 * from CONN events. The cr->socket is closed and marked inactive but the 
	 * object remains allocated and queued on the CR resource list. There can
	 * be multiple CR's associated with a given EP. There is no way to determine 
	 * when consumer is finished with event until the dat_ep_free.
	 *
	 * Schedule destruction for all CR's associated with this EP, cr_thread will
 	 * complete the cleanup with state == DCM_DESTROY. 
	 */ 
	ia_ptr = ep->header.owner_ia;
	dapl_os_lock(&ia_ptr->hca_ptr->ib_trans.lock);
	if (!dapl_llist_is_empty((DAPL_LLIST_HEAD*)
				  &ia_ptr->hca_ptr->ib_trans.list))
            next_cr = dapl_llist_peek_head((DAPL_LLIST_HEAD*)
					    &ia_ptr->hca_ptr->ib_trans.list);
	else
	    next_cr = NULL;

	while (next_cr) {
		cr = next_cr;
		next_cr = dapl_llist_next_entry((DAPL_LLIST_HEAD*)
						&ia_ptr->hca_ptr->ib_trans.list,
						(DAPL_LLIST_ENTRY*)&cr->entry);
		if (cr->ep == ep)  {
			dapl_dbg_log(DAPL_DBG_TYPE_EP,
				     " qp_free CR: ep %p cr %p\n", ep, cr);
			dapli_socket_disconnect(cr);
			dapl_os_lock(&cr->lock);
			hca_ptr = cr->hca;
			cr->ep = NULL;
			if (cr->ah) {
				ibv_destroy_ah(cr->ah);
				cr->ah = NULL;
			}
			cr->state = DCM_DESTROY;
			dapl_os_unlock(&cr->lock);
		}
	}
	dapl_os_unlock(&ia_ptr->hca_ptr->ib_trans.lock);

notify_thread:

	/* wakeup work thread, if something destroyed */
	if (hca_ptr != NULL) 
		send(hca_ptr->ib_trans.scm[1], "w", sizeof "w", 0);
}

/* queue socket for processing CM work */
static void dapli_cm_queue(struct ib_cm_handle *cm_ptr)
{
	/* add to work queue for cr thread processing */
	dapl_llist_init_entry((DAPL_LLIST_ENTRY *) & cm_ptr->entry);
	dapl_os_lock(&cm_ptr->hca->ib_trans.lock);
	dapl_llist_add_tail(&cm_ptr->hca->ib_trans.list,
			    (DAPL_LLIST_ENTRY *) & cm_ptr->entry, cm_ptr);
	dapl_os_unlock(&cm_ptr->hca->ib_trans.lock);

	/* wakeup CM work thread */
	send(cm_ptr->hca->ib_trans.scm[1], "w", sizeof "w", 0);
}

/*
 * ACTIVE/PASSIVE: called from CR thread or consumer via ep_disconnect
 *                 or from ep_free
 */
DAT_RETURN dapli_socket_disconnect(dp_ib_cm_handle_t cm_ptr)
{
	DAPL_EP *ep_ptr = cm_ptr->ep;
	DAT_UINT32 disc_data = htonl(0xdead);

	if (ep_ptr == NULL)
		return DAT_SUCCESS;

	dapl_os_lock(&cm_ptr->lock);
	if (cm_ptr->state != DCM_CONNECTED) {
		dapl_os_unlock(&cm_ptr->lock);
		return DAT_SUCCESS;
	}

	/* send disc date, close socket, schedule destroy */
	dapls_modify_qp_state(ep_ptr->qp_handle, IBV_QPS_ERR, 0,0,0);
	cm_ptr->state = DCM_DISCONNECTED;
	send(cm_ptr->socket, (char *)&disc_data, sizeof(disc_data), 0);
	dapl_os_unlock(&cm_ptr->lock);

	/* disconnect events for RC's only */
	if (ep_ptr->param.ep_attr.service_type == DAT_SERVICE_TYPE_RC) {
		if (ep_ptr->cr_ptr) {
			dapls_cr_callback(cm_ptr,
					  IB_CME_DISCONNECTED,
					  NULL,
					  ((DAPL_CR *) ep_ptr->cr_ptr)->sp_ptr);
		} else {
			dapl_evd_connection_callback(ep_ptr->cm_handle,
						     IB_CME_DISCONNECTED,
						     NULL, ep_ptr);
		}
	}

	/* scheduled destroy via disconnect clean in callback */
	return DAT_SUCCESS;
}

/*
 * ACTIVE: socket connected, send QP information to peer 
 */
static void dapli_socket_connected(dp_ib_cm_handle_t cm_ptr, int err)
{
	int len, exp;
	struct iovec iov[2];
	struct dapl_ep *ep_ptr = cm_ptr->ep;

	if (err) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " CONN_PENDING: %s ERR %s -> %s %d\n",
			 err == -1 ? "POLL" : "SOCKOPT",
			 err == -1 ? strerror(errno) : strerror(err), 
			 inet_ntoa(((struct sockaddr_in *)
				&cm_ptr->addr)->sin_addr), 
			 ntohs(((struct sockaddr_in *)
				&cm_ptr->addr)->sin_port));
		goto bail;
	}

	cm_ptr->state = DCM_REP_PENDING;

	/* send qp info and pdata to remote peer */
	exp = sizeof(ib_cm_msg_t) - DCM_MAX_PDATA_SIZE;
	iov[0].iov_base = (void *)&cm_ptr->msg;
	iov[0].iov_len = exp;
	if (cm_ptr->msg.p_size) {
		iov[1].iov_base = cm_ptr->msg.p_data;
		iov[1].iov_len = ntohs(cm_ptr->msg.p_size);
		len = writev(cm_ptr->socket, iov, 2);
	} else {
		len = writev(cm_ptr->socket, iov, 1);
	}

	if (len != (exp + ntohs(cm_ptr->msg.p_size))) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " CONN_PENDING len ERR %s, wcnt=%d(%d) -> %s\n",
			 strerror(errno), len, 
			 exp + ntohs(cm_ptr->msg.p_size), 
			 inet_ntoa(((struct sockaddr_in *)
				   ep_ptr->param.
				   remote_ia_address_ptr)->sin_addr));
		goto bail;
	}

 	dapl_dbg_log(DAPL_DBG_TYPE_CM,
		     " CONN_PENDING: sending SRC lid=0x%x,"
		     " qpn=0x%x, psize=%d\n",
		     ntohs(cm_ptr->msg.saddr.ib.lid),
		     ntohl(cm_ptr->msg.saddr.ib.qpn), 
		     ntohs(cm_ptr->msg.p_size));
	dapl_dbg_log(DAPL_DBG_TYPE_CM,
		     " CONN_PENDING: SRC GID subnet %016llx id %016llx\n",
		     (unsigned long long)
		     htonll(*(uint64_t*)&cm_ptr->msg.saddr.ib.gid[0]),
		     (unsigned long long)
		     htonll(*(uint64_t*)&cm_ptr->msg.saddr.ib.gid[8]));
	return;

bail:
	/* close socket, free cm structure and post error event */
	dapls_ib_cm_free(cm_ptr, cm_ptr->ep);
	dapl_evd_connection_callback(NULL, IB_CME_LOCAL_FAILURE, NULL, ep_ptr);
}

/*
 * ACTIVE: Create socket, connect, defer exchange QP information to CR thread
 * to avoid blocking. 
 */
DAT_RETURN
dapli_socket_connect(DAPL_EP * ep_ptr,
		     DAT_IA_ADDRESS_PTR r_addr,
		     DAT_CONN_QUAL r_qual, DAT_COUNT p_size, DAT_PVOID p_data)
{
	dp_ib_cm_handle_t cm_ptr;
	int ret;
	socklen_t sl;
	DAPL_IA *ia_ptr = ep_ptr->header.owner_ia;
	DAT_RETURN dat_ret = DAT_INSUFFICIENT_RESOURCES;

	dapl_dbg_log(DAPL_DBG_TYPE_EP, " connect: r_qual %d p_size=%d\n",
		     r_qual, p_size);

	cm_ptr = dapls_ib_cm_create(ep_ptr);
	if (cm_ptr == NULL)
		return dat_ret;

	/* create, connect, sockopt, and exchange QP information */
	if ((cm_ptr->socket =
	     socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == DAPL_INVALID_SOCKET) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " connect: socket create ERR %s\n", strerror(errno));
		goto bail;
	}

	ret = dapl_config_socket(cm_ptr->socket);
	if (ret < 0) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " connect: config socket %d ERR %d %s\n",
			 cm_ptr->socket, ret, strerror(dapl_socket_errno()));
		dat_ret = DAT_INTERNAL_ERROR;
		goto bail;
	}

	/* save remote address */
	dapl_os_memcpy(&cm_ptr->addr, r_addr, sizeof(r_addr));

#ifdef DAPL_DBG
	/* DBG: Active PID [0], PASSIVE PID [2]*/
	*(uint16_t*)&cm_ptr->msg.resv[0] = htons((uint16_t)dapl_os_getpid()); 
	*(uint16_t*)&cm_ptr->msg.resv[2] = ((struct sockaddr_in *)&cm_ptr->addr)->sin_port;
#endif
	((struct sockaddr_in *)&cm_ptr->addr)->sin_port = htons(r_qual + 1000);
	ret = dapl_connect_socket(cm_ptr->socket, (struct sockaddr *)&cm_ptr->addr,
				  sizeof(cm_ptr->addr));
	if (ret && ret != EAGAIN) {
		dat_ret = DAT_INVALID_ADDRESS;
		goto bail;
	}

	/* REQ: QP info in msg.saddr, IA address in msg.daddr, and pdata */
	cm_ptr->msg.op = ntohs(DCM_REQ);
	cm_ptr->msg.saddr.ib.qpn = htonl(ep_ptr->qp_handle->qp_num);
	cm_ptr->msg.saddr.ib.qp_type = ep_ptr->qp_handle->qp_type;
	cm_ptr->msg.saddr.ib.lid = ia_ptr->hca_ptr->ib_trans.lid;
	dapl_os_memcpy(&cm_ptr->msg.saddr.ib.gid[0], 
		       &ia_ptr->hca_ptr->ib_trans.gid, 16);
	
	/* save references */
	cm_ptr->hca = ia_ptr->hca_ptr;
	cm_ptr->ep = ep_ptr;
	
	/* get local address information from socket */
	sl = sizeof(cm_ptr->msg.daddr.so);
	if (getsockname(cm_ptr->socket, (struct sockaddr *)&cm_ptr->msg.daddr.so, &sl)) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			" connect getsockname ERROR: %s -> %s r_qual %d\n",
			strerror(errno), 
			inet_ntoa(((struct sockaddr_in *)r_addr)->sin_addr),
			(unsigned int)r_qual);;
	}

	if (p_size) {
		cm_ptr->msg.p_size = htons(p_size);
		dapl_os_memcpy(cm_ptr->msg.p_data, p_data, p_size);
	}

	/* connected or pending, either way results via async event */
	if (ret == 0)
		dapli_socket_connected(cm_ptr, 0);
	else
		cm_ptr->state = DCM_CONN_PENDING;

	dapl_dbg_log(DAPL_DBG_TYPE_EP, " connect: p_data=%p %p\n",
		     cm_ptr->msg.p_data, cm_ptr->msg.p_data);

	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     " connect: %s r_qual %d pending, p_sz=%d, %d %d ...\n",
		     inet_ntoa(((struct sockaddr_in *)&cm_ptr->addr)->sin_addr), 
		     (unsigned int)r_qual, ntohs(cm_ptr->msg.p_size),
		     cm_ptr->msg.p_data[0], cm_ptr->msg.p_data[1]);

	dapli_cm_queue(cm_ptr);
	return DAT_SUCCESS;

bail:
	dapl_log(DAPL_DBG_TYPE_ERR,
		 " connect ERROR: %s -> %s r_qual %d\n",
		 strerror(errno), 
		 inet_ntoa(((struct sockaddr_in *)r_addr)->sin_addr),
		 (unsigned int)r_qual);

	/* close socket, free cm structure */
	dapls_ib_cm_free(cm_ptr, NULL);
	return dat_ret;
}

/*
 * ACTIVE: exchange QP information, called from CR thread
 */
static void dapli_socket_connect_rtu(dp_ib_cm_handle_t cm_ptr)
{
	DAPL_EP *ep_ptr = cm_ptr->ep;
	int len, exp = sizeof(ib_cm_msg_t) - DCM_MAX_PDATA_SIZE;
	ib_cm_events_t event = IB_CME_LOCAL_FAILURE;
	socklen_t sl;

	/* read DST information into cm_ptr, overwrite SRC info */
	dapl_dbg_log(DAPL_DBG_TYPE_EP, " connect_rtu: recv peer QP data\n");

	len = recv(cm_ptr->socket, (char *)&cm_ptr->msg, exp, 0);
	if (len != exp || ntohs(cm_ptr->msg.ver) != DCM_VER) {
		dapl_log(DAPL_DBG_TYPE_WARN,
			 " CONN_RTU read: sk %d ERR %s, rcnt=%d, v=%d -> %s PORT L-%x R-%x PID L-%x R-%x\n",
			 cm_ptr->socket, strerror(errno), len, ntohs(cm_ptr->msg.ver),
			 inet_ntoa(((struct sockaddr_in *)&cm_ptr->addr)->sin_addr),
			 ntohs(((struct sockaddr_in *)&cm_ptr->msg.daddr.so)->sin_port),
			 ntohs(((struct sockaddr_in *)&cm_ptr->addr)->sin_port),
			 ntohs(*(uint16_t*)&cm_ptr->msg.resv[0]),
			 ntohs(*(uint16_t*)&cm_ptr->msg.resv[2]));

		/* Retry; corner case where server tcp stack resets under load */
		if (dapl_socket_errno() == ECONNRESET) {
			closesocket(cm_ptr->socket);
			cm_ptr->socket = DAPL_INVALID_SOCKET;
			dapli_socket_connect(cm_ptr->ep, (DAT_IA_ADDRESS_PTR)&cm_ptr->addr, 
					     ntohs(((struct sockaddr_in *)&cm_ptr->addr)->sin_port) - 1000,
					     ntohs(cm_ptr->msg.p_size), &cm_ptr->msg.p_data);
			dapls_ib_cm_free(cm_ptr, NULL);
			return;
		}
		goto bail;
	}

	/* keep the QP, address info in network order */
	
	/* save remote address information, in msg.daddr */
	dapl_os_memcpy(&ep_ptr->remote_ia_address,
		       &cm_ptr->msg.daddr.so,
		       sizeof(union dcm_addr));

	/* save local address information from socket */
	sl = sizeof(cm_ptr->addr);
	getsockname(cm_ptr->socket,(struct sockaddr *)&cm_ptr->addr, &sl);

	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     " CONN_RTU: DST %s %d lid=0x%x,"
		     " qpn=0x%x, qp_type=%d, psize=%d\n",
		     inet_ntoa(((struct sockaddr_in *)
				&cm_ptr->msg.daddr.so)->sin_addr),
		     ntohs(((struct sockaddr_in *)
				&cm_ptr->msg.daddr.so)->sin_port),
		     ntohs(cm_ptr->msg.saddr.ib.lid),
		     ntohl(cm_ptr->msg.saddr.ib.qpn), 
		     cm_ptr->msg.saddr.ib.qp_type, 
		     ntohs(cm_ptr->msg.p_size));

	/* validate private data size before reading */
	if (ntohs(cm_ptr->msg.p_size) > DCM_MAX_PDATA_SIZE) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " CONN_RTU read: psize (%d) wrong -> %s\n",
			 ntohs(cm_ptr->msg.p_size), 
			 inet_ntoa(((struct sockaddr_in *)
				   ep_ptr->param.
				   remote_ia_address_ptr)->sin_addr));
		goto bail;
	}

	/* read private data into cm_handle if any present */
	dapl_dbg_log(DAPL_DBG_TYPE_EP," CONN_RTU: read private data\n");
	exp = ntohs(cm_ptr->msg.p_size);
	if (exp) {
		len = recv(cm_ptr->socket, cm_ptr->msg.p_data, exp, 0);
		if (len != exp) {
			dapl_log(DAPL_DBG_TYPE_ERR,
				 " CONN_RTU read pdata: ERR %s, rcnt=%d -> %s\n",
				 strerror(errno), len,
				 inet_ntoa(((struct sockaddr_in *)
					    ep_ptr->param.
					    remote_ia_address_ptr)->sin_addr));
			goto bail;
		}
	}

	/* check for consumer or protocol stack reject */
	if (ntohs(cm_ptr->msg.op) == DCM_REP)
		event = IB_CME_CONNECTED;
	else if (ntohs(cm_ptr->msg.op) == DCM_REJ_USER) 
		event = IB_CME_DESTINATION_REJECT_PRIVATE_DATA;
	else  
		event = IB_CME_DESTINATION_REJECT;
	
	if (event != IB_CME_CONNECTED) {
		dapl_log(DAPL_DBG_TYPE_CM,
			 " CONN_RTU: reject from %s %x\n",
			 inet_ntoa(((struct sockaddr_in *)
				    &cm_ptr->msg.daddr.so)->sin_addr),
			 ntohs(((struct sockaddr_in *)
				 &cm_ptr->msg.daddr.so)->sin_port));
		goto bail;
	}

	/* modify QP to RTR and then to RTS with remote info */
	dapl_os_lock(&ep_ptr->header.lock);
	if (dapls_modify_qp_state(ep_ptr->qp_handle,
				  IBV_QPS_RTR, 
				  cm_ptr->msg.saddr.ib.qpn,
				  cm_ptr->msg.saddr.ib.lid,
				  (ib_gid_handle_t)cm_ptr->msg.saddr.ib.gid) != DAT_SUCCESS) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " CONN_RTU: QPS_RTR ERR %s (%d,%d,%x,%x,%x) -> %s %x\n",
			 strerror(errno), ep_ptr->qp_handle->qp_type,
			 ep_ptr->qp_state, ep_ptr->qp_handle->qp_num,
			 ntohl(cm_ptr->msg.saddr.ib.qpn), 
			 ntohs(cm_ptr->msg.saddr.ib.lid),
			 inet_ntoa(((struct sockaddr_in *)
				    &cm_ptr->msg.daddr.so)->sin_addr),
			 ntohs(((struct sockaddr_in *)
				 &cm_ptr->msg.daddr.so)->sin_port));
		dapl_os_unlock(&ep_ptr->header.lock);
		goto bail;
	}
	if (dapls_modify_qp_state(ep_ptr->qp_handle,
				  IBV_QPS_RTS, 
				  cm_ptr->msg.saddr.ib.qpn,
				  cm_ptr->msg.saddr.ib.lid,
				  NULL) != DAT_SUCCESS) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " CONN_RTU: QPS_RTS ERR %s (%d,%d,%x,%x,%x) -> %s %x\n",
			 strerror(errno), ep_ptr->qp_handle->qp_type,
			 ep_ptr->qp_state, ep_ptr->qp_handle->qp_num,
			 ntohl(cm_ptr->msg.saddr.ib.qpn), 
			 ntohs(cm_ptr->msg.saddr.ib.lid),
			 inet_ntoa(((struct sockaddr_in *)
				    &cm_ptr->msg.daddr.so)->sin_addr),
			 ntohs(((struct sockaddr_in *)
				 &cm_ptr->msg.daddr.so)->sin_port));
		dapl_os_unlock(&ep_ptr->header.lock);
		goto bail;
	}
	dapl_os_unlock(&ep_ptr->header.lock);
	dapl_dbg_log(DAPL_DBG_TYPE_EP, " connect_rtu: send RTU\n");

	/* complete handshake after final QP state change, Just ver+op */
	cm_ptr->state = DCM_CONNECTED;
	cm_ptr->msg.op = ntohs(DCM_RTU);
	if (send(cm_ptr->socket, (char *)&cm_ptr->msg, 4, 0) == -1) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " CONN_RTU: write error = %s\n", strerror(errno));
		goto bail;
	}
	/* post the event with private data */
	event = IB_CME_CONNECTED;
	dapl_dbg_log(DAPL_DBG_TYPE_EP, " ACTIVE: connected!\n");

#ifdef DAT_EXTENSIONS
ud_bail:
	if (cm_ptr->msg.saddr.ib.qp_type == IBV_QPT_UD) {
		DAT_IB_EXTENSION_EVENT_DATA xevent;
		ib_pd_handle_t pd_handle = 
			((DAPL_PZ *)ep_ptr->param.pz_handle)->pd_handle;

		if (event == IB_CME_CONNECTED) {
			cm_ptr->ah = dapls_create_ah(cm_ptr->hca, pd_handle,
						     ep_ptr->qp_handle,
						     cm_ptr->msg.saddr.ib.lid, 
						     NULL);
			if (cm_ptr->ah) {
				/* post UD extended EVENT */
				xevent.status = 0;
				xevent.type = DAT_IB_UD_REMOTE_AH;
				xevent.remote_ah.ah = cm_ptr->ah;
				xevent.remote_ah.qpn = ntohl(cm_ptr->msg.saddr.ib.qpn);
				dapl_os_memcpy(&xevent.remote_ah.ia_addr,
						&ep_ptr->remote_ia_address,
						sizeof(union dcm_addr));
				event = DAT_IB_UD_CONNECTION_EVENT_ESTABLISHED;

				dapl_log(DAPL_DBG_TYPE_CM, 
					" CONN_RTU: UD AH %p for lid 0x%x"
					" qpn 0x%x\n", 
					cm_ptr->ah, 
					ntohs(cm_ptr->msg.saddr.ib.lid),
					ntohl(cm_ptr->msg.saddr.ib.qpn));
	
			} else 
				event = DAT_IB_UD_CONNECTION_ERROR_EVENT;
			
		} else if (event == IB_CME_LOCAL_FAILURE) {
			event = DAT_IB_UD_CONNECTION_ERROR_EVENT;
		} else  
			event = DAT_IB_UD_CONNECTION_REJECT_EVENT;
		
		dapls_evd_post_connection_event_ext(
				(DAPL_EVD *) ep_ptr->param.connect_evd_handle,
				event,
				(DAT_EP_HANDLE) ep_ptr,
				(DAT_COUNT) exp,
				(DAT_PVOID *) cm_ptr->msg.p_data,
				(DAT_PVOID *) &xevent);

		/* done with socket, don't destroy cm_ptr, need pdata */
		closesocket(cm_ptr->socket);
		cm_ptr->socket = DAPL_INVALID_SOCKET;
		cm_ptr->state = DCM_RELEASED;
	} else
#endif
	{
		ep_ptr->cm_handle = cm_ptr; /* only RC, multi CR's on UD */
		dapl_evd_connection_callback(cm_ptr, event, cm_ptr->msg.p_data, ep_ptr);
	}
	return;

bail:

#ifdef DAT_EXTENSIONS
	if (cm_ptr->msg.saddr.ib.qp_type == IBV_QPT_UD) 
		goto ud_bail;
#endif
	/* close socket, and post error event */
	dapls_modify_qp_state(ep_ptr->qp_handle, IBV_QPS_ERR, 0, 0, 0);
	closesocket(cm_ptr->socket);
	cm_ptr->socket = DAPL_INVALID_SOCKET;
	dapl_evd_connection_callback(NULL, event, cm_ptr->msg.p_data, ep_ptr);
}

/*
 * PASSIVE: Create socket, listen, accept, exchange QP information 
 */
DAT_RETURN
dapli_socket_listen(DAPL_IA * ia_ptr, DAT_CONN_QUAL serviceID, DAPL_SP * sp_ptr)
{
	struct sockaddr_in addr;
	ib_cm_srvc_handle_t cm_ptr = NULL;
	DAT_RETURN dat_status = DAT_SUCCESS;

	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     " listen(ia_ptr %p ServiceID %d sp_ptr %p)\n",
		     ia_ptr, serviceID, sp_ptr);

	cm_ptr = dapls_ib_cm_create(NULL);
	if (cm_ptr == NULL)
		return DAT_INSUFFICIENT_RESOURCES;

	cm_ptr->sp = sp_ptr;
	cm_ptr->hca = ia_ptr->hca_ptr;

	/* bind, listen, set sockopt, accept, exchange data */
	if ((cm_ptr->socket =
	     socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == DAPL_INVALID_SOCKET) {
		dapl_log(DAPL_DBG_TYPE_ERR, " ERR: listen socket create: %s\n",
			 strerror(errno));
		dat_status = DAT_INSUFFICIENT_RESOURCES;
		goto bail;
	}

	addr.sin_port = htons(serviceID + 1000);
	addr.sin_family = AF_INET;
	addr.sin_addr = ((struct sockaddr_in *) &ia_ptr->hca_ptr->hca_address)->sin_addr;

	if ((bind(cm_ptr->socket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	    || (listen(cm_ptr->socket, 128) < 0)) {
		dapl_dbg_log(DAPL_DBG_TYPE_CM,
			     " listen: ERROR %s on conn_qual 0x%x\n",
			     strerror(errno), serviceID + 1000);
		if (dapl_socket_errno() == EADDRINUSE)
			dat_status = DAT_CONN_QUAL_IN_USE;
		else
			dat_status = DAT_CONN_QUAL_UNAVAILABLE;
		goto bail;
	}

	/* set cm_handle for this service point, save listen socket */
	sp_ptr->cm_srvc_handle = cm_ptr;
	dapl_os_memcpy(&cm_ptr->addr, &addr, sizeof(addr)); 

	/* queue up listen socket to process inbound CR's */
	cm_ptr->state = DCM_LISTEN;
	dapli_cm_queue(cm_ptr);

	dapl_dbg_log(DAPL_DBG_TYPE_CM,
		     " listen: qual 0x%x cr %p s_fd %d\n",
		     ntohs(serviceID + 1000), cm_ptr, cm_ptr->socket);

	return dat_status;
bail:
	dapl_dbg_log(DAPL_DBG_TYPE_CM,
		     " listen: ERROR on conn_qual 0x%x\n", serviceID + 1000);
	dapls_ib_cm_free(cm_ptr, cm_ptr->ep);
	return dat_status;
}

/*
 * PASSIVE: accept socket 
 */
static void dapli_socket_accept(ib_cm_srvc_handle_t cm_ptr)
{
	dp_ib_cm_handle_t acm_ptr;
	int ret, len, opt = 1;
	socklen_t sl;

	/* 
	 * Accept all CR's on this port to avoid half-connection (SYN_RCV)
	 * stalls with many to one connection storms
	 */
	do {
		/* Allocate accept CM and initialize */
		if ((acm_ptr = dapls_ib_cm_create(NULL)) == NULL)
			return;

		acm_ptr->sp = cm_ptr->sp;
		acm_ptr->hca = cm_ptr->hca;

		len = sizeof(union dcm_addr);
		acm_ptr->socket = accept(cm_ptr->socket,
					(struct sockaddr *)
					&acm_ptr->msg.daddr.so,
					(socklen_t *) &len);
		if (acm_ptr->socket == DAPL_INVALID_SOCKET) {
			dapl_log(DAPL_DBG_TYPE_ERR,
				" ACCEPT: ERR %s on FD %d l_cr %p\n",
				strerror(errno), cm_ptr->socket, cm_ptr);
			dapls_ib_cm_free(acm_ptr, acm_ptr->ep);
			return;
		}
		dapl_dbg_log(DAPL_DBG_TYPE_CM, " accepting from %s %x\n",
			     inet_ntoa(((struct sockaddr_in *)
					&acm_ptr->msg.daddr.so)->sin_addr),
			     ntohs(((struct sockaddr_in *)
					&acm_ptr->msg.daddr.so)->sin_port));

		/* no delay for small packets */
		ret = setsockopt(acm_ptr->socket, IPPROTO_TCP, TCP_NODELAY,
			   (char *)&opt, sizeof(opt));
		if (ret)
			dapl_log(DAPL_DBG_TYPE_ERR,
				 " ACCEPT: NODELAY setsockopt: 0x%x 0x%x %s\n",
			 	 ret, dapl_socket_errno(), strerror(dapl_socket_errno()));
		
		/* get local address information from socket */
		sl = sizeof(acm_ptr->addr);
		getsockname(acm_ptr->socket, (struct sockaddr *)&acm_ptr->addr, &sl);
		acm_ptr->state = DCM_ACCEPTING;
		dapli_cm_queue(acm_ptr);
	
	} while (dapl_poll(cm_ptr->socket, DAPL_FD_READ) == DAPL_FD_READ);
}

/*
 * PASSIVE: receive peer QP information, private data, post cr_event 
 */
static void dapli_socket_accept_data(ib_cm_srvc_handle_t acm_ptr)
{
	int len, exp = sizeof(ib_cm_msg_t) - DCM_MAX_PDATA_SIZE;
	void *p_data = NULL;

	dapl_dbg_log(DAPL_DBG_TYPE_EP, " socket accepted, read QP data\n");

	/* read in DST QP info, IA address. check for private data */
	len = recv(acm_ptr->socket, (char *)&acm_ptr->msg, exp, 0);
	if (len != exp || ntohs(acm_ptr->msg.ver) != DCM_VER) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " ACCEPT read: ERR %s, rcnt=%d, ver=%d\n",
			 strerror(errno), len, ntohs(acm_ptr->msg.ver));
		goto bail;
	}

	/* keep the QP, address info in network order */

	/* validate private data size before reading */
	exp = ntohs(acm_ptr->msg.p_size);
	if (exp > DCM_MAX_PDATA_SIZE) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			     " accept read: psize (%d) wrong\n",
			     acm_ptr->msg.p_size);
		goto bail;
	}

	/* read private data into cm_handle if any present */
	if (exp) {
		len = recv(acm_ptr->socket, acm_ptr->msg.p_data, exp, 0);
		if (len != exp) {
			dapl_log(DAPL_DBG_TYPE_ERR,
				 " accept read pdata: ERR %s, rcnt=%d\n",
				 strerror(errno), len);
			goto bail;
		}
		p_data = acm_ptr->msg.p_data;
	}

	acm_ptr->state = DCM_ACCEPTING_DATA;

	dapl_dbg_log(DAPL_DBG_TYPE_CM,
		     " ACCEPT: DST %s %x lid=0x%x, qpn=0x%x, psz=%d\n",
		     inet_ntoa(((struct sockaddr_in *)
				&acm_ptr->msg.daddr.so)->sin_addr), 
		     ntohs(((struct sockaddr_in *)
			     &acm_ptr->msg.daddr.so)->sin_port),
		     ntohs(acm_ptr->msg.saddr.ib.lid), 
		     ntohl(acm_ptr->msg.saddr.ib.qpn), exp);

#ifdef DAT_EXTENSIONS
	if (acm_ptr->msg.saddr.ib.qp_type == IBV_QPT_UD) {
		DAT_IB_EXTENSION_EVENT_DATA xevent;

		/* post EVENT, modify_qp created ah */
		xevent.status = 0;
		xevent.type = DAT_IB_UD_CONNECT_REQUEST;

		dapls_evd_post_cr_event_ext(acm_ptr->sp,
					    DAT_IB_UD_CONNECTION_REQUEST_EVENT,
					    acm_ptr,
					    (DAT_COUNT) exp,
					    (DAT_PVOID *) acm_ptr->msg.p_data,
					    (DAT_PVOID *) &xevent);
	} else
#endif
		/* trigger CR event and return SUCCESS */
		dapls_cr_callback(acm_ptr,
				  IB_CME_CONNECTION_REQUEST_PENDING,
				  p_data, acm_ptr->sp);
	return;
bail:
	/* close socket, free cm structure, active will see close as rej */
	dapls_ib_cm_free(acm_ptr, acm_ptr->ep);
	return;
}

/*
 * PASSIVE: consumer accept, send local QP information, private data, 
 * queue on work thread to receive RTU information to avoid blocking
 * user thread. 
 */
static DAT_RETURN
dapli_socket_accept_usr(DAPL_EP * ep_ptr,
			DAPL_CR * cr_ptr, DAT_COUNT p_size, DAT_PVOID p_data)
{
	DAPL_IA *ia_ptr = ep_ptr->header.owner_ia;
	dp_ib_cm_handle_t cm_ptr = cr_ptr->ib_cm_handle;
	ib_cm_msg_t local;
	struct iovec iov[2];
	int len, exp = sizeof(ib_cm_msg_t) - DCM_MAX_PDATA_SIZE;
	DAT_RETURN ret = DAT_INTERNAL_ERROR;
	socklen_t sl;

	if (p_size > DCM_MAX_PDATA_SIZE) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " accept_usr: psize(%d) too large\n", p_size);
		return DAT_LENGTH_ERROR;
	}

	/* must have a accepted socket */
	if (cm_ptr->socket == DAPL_INVALID_SOCKET) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " accept_usr: cm socket invalid\n");
		goto bail;
	}

	dapl_dbg_log(DAPL_DBG_TYPE_CM,
		     " ACCEPT_USR: remote lid=0x%x"
		     " qpn=0x%x qp_type %d, psize=%d\n",
		     ntohs(cm_ptr->msg.saddr.ib.lid),
		     ntohl(cm_ptr->msg.saddr.ib.qpn), 
		     cm_ptr->msg.saddr.ib.qp_type, 
		     ntohs(cm_ptr->msg.p_size));

#ifdef DAT_EXTENSIONS
	if (cm_ptr->msg.saddr.ib.qp_type == IBV_QPT_UD &&
	    ep_ptr->qp_handle->qp_type != IBV_QPT_UD) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " ACCEPT_USR: ERR remote QP is UD,"
			 ", but local QP is not\n");
		ret = (DAT_INVALID_HANDLE | DAT_INVALID_HANDLE_EP);
		goto bail;
	}
#endif

	/* modify QP to RTR and then to RTS with remote info already read */
	dapl_os_lock(&ep_ptr->header.lock);
	if (dapls_modify_qp_state(ep_ptr->qp_handle,
				  IBV_QPS_RTR, 
				  cm_ptr->msg.saddr.ib.qpn,
				  cm_ptr->msg.saddr.ib.lid,
				  (ib_gid_handle_t)cm_ptr->msg.saddr.ib.gid) != DAT_SUCCESS) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " ACCEPT_USR: QPS_RTR ERR %s -> %s\n",
			 strerror(errno), 
			 inet_ntoa(((struct sockaddr_in *)
				     &cm_ptr->msg.daddr.so)->sin_addr));
		dapl_os_unlock(&ep_ptr->header.lock);
		goto bail;
	}
	if (dapls_modify_qp_state(ep_ptr->qp_handle,
				  IBV_QPS_RTS, 
				  cm_ptr->msg.saddr.ib.qpn,
				  cm_ptr->msg.saddr.ib.lid,
				  NULL) != DAT_SUCCESS) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " ACCEPT_USR: QPS_RTS ERR %s -> %s\n",
			 strerror(errno), 
			 inet_ntoa(((struct sockaddr_in *)
				     &cm_ptr->msg.daddr.so)->sin_addr));
		dapl_os_unlock(&ep_ptr->header.lock);
		goto bail;
	}
	dapl_os_unlock(&ep_ptr->header.lock);

	/* save remote address information */
	dapl_os_memcpy(&ep_ptr->remote_ia_address,
		       &cm_ptr->msg.daddr.so,
		       sizeof(union dcm_addr));

	/* send our QP info, IA address, pdata. Don't overwrite dst data */
	local.ver = htons(DCM_VER);
	local.op = htons(DCM_REP);
	local.saddr.ib.qpn = htonl(ep_ptr->qp_handle->qp_num);
	local.saddr.ib.qp_type = ep_ptr->qp_handle->qp_type;
	local.saddr.ib.lid = ia_ptr->hca_ptr->ib_trans.lid;
	dapl_os_memcpy(&local.saddr.ib.gid[0], 
		       &ia_ptr->hca_ptr->ib_trans.gid, 16);
	
	/* Get local address information from socket */
	sl = sizeof(local.daddr.so);
	getsockname(cm_ptr->socket, (struct sockaddr *)&local.daddr.so, &sl);

#ifdef DAPL_DBG
	/* DBG: Active PID [0], PASSIVE PID [2] */
	*(uint16_t*)&cm_ptr->msg.resv[2] = htons((uint16_t)dapl_os_getpid()); 
	dapl_os_memcpy(local.resv, cm_ptr->msg.resv, 4); 
#endif

	cm_ptr->ep = ep_ptr;
	cm_ptr->hca = ia_ptr->hca_ptr;
	cm_ptr->state = DCM_ACCEPTED;

	local.p_size = htons(p_size);
	iov[0].iov_base = (void *)&local;
	iov[0].iov_len = exp;
	
	if (p_size) {
		iov[1].iov_base = p_data;
		iov[1].iov_len = p_size;
		len = writev(cm_ptr->socket, iov, 2);
	} else 
		len = writev(cm_ptr->socket, iov, 1);
	
	if (len != (p_size + exp)) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " ACCEPT_USR: ERR %s, wcnt=%d -> %s\n",
			 strerror(errno), len, 
			 inet_ntoa(((struct sockaddr_in *)
				   &cm_ptr->msg.daddr.so)->sin_addr));
		cm_ptr->ep = NULL;
		goto bail;
	}

	dapl_dbg_log(DAPL_DBG_TYPE_CM,
		     " ACCEPT_USR: local lid=0x%x qpn=0x%x psz=%d\n",
		     ntohs(local.saddr.ib.lid),
		     ntohl(local.saddr.ib.qpn), ntohs(local.p_size));
	dapl_dbg_log(DAPL_DBG_TYPE_CM,
		     " ACCEPT_USR: SRC GID subnet %016llx id %016llx\n",
		     (unsigned long long)
		     htonll(*(uint64_t*)&local.saddr.ib.gid[0]),
		     (unsigned long long)
		     htonll(*(uint64_t*)&local.saddr.ib.gid[8]));

	dapl_dbg_log(DAPL_DBG_TYPE_EP, " PASSIVE: accepted!\n");
	return DAT_SUCCESS;
bail:
	dapls_ib_cm_free(cm_ptr, NULL);
	return ret;
}

/*
 * PASSIVE: read RTU from active peer, post CONN event
 */
static void dapli_socket_accept_rtu(dp_ib_cm_handle_t cm_ptr)
{
	int len;
	ib_cm_events_t event = IB_CME_CONNECTED;

	/* complete handshake after final QP state change, VER and OP */
	len = recv(cm_ptr->socket, (char *)&cm_ptr->msg, 4, 0);
	if (len != 4 || ntohs(cm_ptr->msg.op) != DCM_RTU) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " ACCEPT_RTU: rcv ERR, rcnt=%d op=%x\n",
			 len, ntohs(cm_ptr->msg.op),
			 inet_ntoa(((struct sockaddr_in *)
				    &cm_ptr->msg.daddr.so)->sin_addr));
		event = IB_CME_DESTINATION_REJECT;
		goto bail;
	}

	/* save state and reference to EP, queue for disc event */
	cm_ptr->state = DCM_CONNECTED;

	/* final data exchange if remote QP state is good to go */
	dapl_dbg_log(DAPL_DBG_TYPE_EP, " PASSIVE: connected!\n");

#ifdef DAT_EXTENSIONS
ud_bail:
	if (cm_ptr->msg.saddr.ib.qp_type == IBV_QPT_UD) {
		DAT_IB_EXTENSION_EVENT_DATA xevent;

		ib_pd_handle_t pd_handle = 
			((DAPL_PZ *)cm_ptr->ep->param.pz_handle)->pd_handle;
		
		if (event == IB_CME_CONNECTED) {
			cm_ptr->ah = dapls_create_ah(cm_ptr->hca, pd_handle,
						cm_ptr->ep->qp_handle,
						cm_ptr->msg.saddr.ib.lid, 
						NULL);
			if (cm_ptr->ah) { 
				/* post EVENT, modify_qp created ah */
				xevent.status = 0;
				xevent.type = DAT_IB_UD_PASSIVE_REMOTE_AH;
				xevent.remote_ah.ah = cm_ptr->ah;
				xevent.remote_ah.qpn = ntohl(cm_ptr->msg.saddr.ib.qpn);
				dapl_os_memcpy(&xevent.remote_ah.ia_addr,
					&cm_ptr->msg.daddr.so,
					sizeof(union dcm_addr));
				event = DAT_IB_UD_CONNECTION_EVENT_ESTABLISHED;
			} else 
				event = DAT_IB_UD_CONNECTION_ERROR_EVENT;
		} else 
			event = DAT_IB_UD_CONNECTION_ERROR_EVENT;

		dapl_log(DAPL_DBG_TYPE_CM, 
			" CONN_RTU: UD AH %p for lid 0x%x qpn 0x%x\n", 
			cm_ptr->ah, ntohs(cm_ptr->msg.saddr.ib.lid),
			ntohl(cm_ptr->msg.saddr.ib.qpn));

		dapls_evd_post_connection_event_ext(
				(DAPL_EVD *) 
				cm_ptr->ep->param.connect_evd_handle,
				event,
				(DAT_EP_HANDLE) cm_ptr->ep,
				(DAT_COUNT) ntohs(cm_ptr->msg.p_size),
				(DAT_PVOID *) cm_ptr->msg.p_data,
				(DAT_PVOID *) &xevent);

                /* done with socket, don't destroy cm_ptr, need pdata */
                closesocket(cm_ptr->socket);
                cm_ptr->socket = DAPL_INVALID_SOCKET;
		cm_ptr->state = DCM_RELEASED;
	} else 
#endif
	{
		cm_ptr->ep->cm_handle = cm_ptr; /* only RC, multi CR's on UD */
		dapls_cr_callback(cm_ptr, event, NULL, cm_ptr->sp);
	}
	return;
      
bail:
#ifdef DAT_EXTENSIONS
	if (cm_ptr->msg.saddr.ib.qp_type == IBV_QPT_UD) 
		goto ud_bail;
#endif
	dapls_modify_qp_state(cm_ptr->ep->qp_handle, IBV_QPS_ERR, 0, 0, 0);
	dapls_ib_cm_free(cm_ptr, cm_ptr->ep);
	dapls_cr_callback(cm_ptr, event, NULL, cm_ptr->sp);
}

/*
 * dapls_ib_connect
 *
 * Initiate a connection with the passive listener on another node
 *
 * Input:
 *	ep_handle,
 *	remote_ia_address,
 *	remote_conn_qual,
 *	prd_size		size of private data and structure
 *	prd_prt			pointer to private data structure
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INVALID_PARAMETER
 *
 */
DAT_RETURN
dapls_ib_connect(IN DAT_EP_HANDLE ep_handle,
		 IN DAT_IA_ADDRESS_PTR remote_ia_address,
		 IN DAT_CONN_QUAL remote_conn_qual,
		 IN DAT_COUNT private_data_size, IN void *private_data)
{
	DAPL_EP *ep_ptr;
	ib_qp_handle_t qp_ptr;

	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     " connect(ep_handle %p ....)\n", ep_handle);

	ep_ptr = (DAPL_EP *) ep_handle;
	qp_ptr = ep_ptr->qp_handle;

	return (dapli_socket_connect(ep_ptr, remote_ia_address,
				     remote_conn_qual,
				     private_data_size, private_data));
}

/*
 * dapls_ib_disconnect
 *
 * Disconnect an EP
 *
 * Input:
 *	ep_handle,
 *	disconnect_flags
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 */
DAT_RETURN
dapls_ib_disconnect(IN DAPL_EP * ep_ptr, IN DAT_CLOSE_FLAGS close_flags)
{
	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     "dapls_ib_disconnect(ep_handle %p ....)\n", ep_ptr);

	/* Transition to error state to flush queue */
        dapls_modify_qp_state(ep_ptr->qp_handle, IBV_QPS_ERR, 0, 0, 0);
	
	if (ep_ptr->cm_handle == NULL ||
	    ep_ptr->param.ep_state == DAT_EP_STATE_DISCONNECTED)
		return DAT_SUCCESS;
	else
		return (dapli_socket_disconnect(ep_ptr->cm_handle));
}

/*
 * dapls_ib_disconnect_clean
 *
 * Clean up outstanding connection data. This routine is invoked
 * after the final disconnect callback has occurred. Only on the
 * ACTIVE side of a connection. It is also called if dat_ep_connect
 * times out using the consumer supplied timeout value.
 *
 * Input:
 *	ep_ptr		DAPL_EP
 *	active		Indicates active side of connection
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	void
 *
 */
void
dapls_ib_disconnect_clean(IN DAPL_EP * ep_ptr,
			  IN DAT_BOOLEAN active,
			  IN const ib_cm_events_t ib_cm_event)
{
	/* NOTE: SCM will only initialize cm_handle with RC type
	 * 
	 * For UD there can many in-flight CR's so you 
	 * cannot cleanup timed out CR's with EP reference 
	 * alone since they share the same EP. The common
	 * code that handles connection timeout logic needs 
	 * updated for UD support.
	 */
	if (ep_ptr->cm_handle)
		dapls_ib_cm_free(ep_ptr->cm_handle, ep_ptr);

	return;
}

/*
 * dapl_ib_setup_conn_listener
 *
 * Have the CM set up a connection listener.
 *
 * Input:
 *	ibm_hca_handle		HCA handle
 *	qp_handle			QP handle
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INTERNAL_ERROR
 *	DAT_CONN_QUAL_UNAVAILBLE
 *	DAT_CONN_QUAL_IN_USE
 *
 */
DAT_RETURN
dapls_ib_setup_conn_listener(IN DAPL_IA * ia_ptr,
			     IN DAT_UINT64 ServiceID, IN DAPL_SP * sp_ptr)
{
	return (dapli_socket_listen(ia_ptr, ServiceID, sp_ptr));
}

/*
 * dapl_ib_remove_conn_listener
 *
 * Have the CM remove a connection listener.
 *
 * Input:
 *	ia_handle		IA handle
 *	ServiceID		IB Channel Service ID
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INVALID_STATE
 *
 */
DAT_RETURN
dapls_ib_remove_conn_listener(IN DAPL_IA * ia_ptr, IN DAPL_SP * sp_ptr)
{
	ib_cm_srvc_handle_t cm_ptr = sp_ptr->cm_srvc_handle;

	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     "dapls_ib_remove_conn_listener(ia_ptr %p sp_ptr %p cm_ptr %p)\n",
		     ia_ptr, sp_ptr, cm_ptr);

	/* close accepted socket, free cm_srvc_handle and return */
	if (cm_ptr != NULL) {
		/* cr_thread will free */
		dapl_os_lock(&cm_ptr->lock);
		cm_ptr->state = DCM_DESTROY;
		sp_ptr->cm_srvc_handle = NULL;
		send(cm_ptr->hca->ib_trans.scm[1], "w", sizeof "w", 0);
		dapl_os_unlock(&cm_ptr->lock);
	}
	return DAT_SUCCESS;
}

/*
 * dapls_ib_accept_connection
 *
 * Perform necessary steps to accept a connection
 *
 * Input:
 *	cr_handle
 *	ep_handle
 *	private_data_size
 *	private_data
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INTERNAL_ERROR
 *
 */
DAT_RETURN
dapls_ib_accept_connection(IN DAT_CR_HANDLE cr_handle,
			   IN DAT_EP_HANDLE ep_handle,
			   IN DAT_COUNT p_size, IN const DAT_PVOID p_data)
{
	DAPL_CR *cr_ptr;
	DAPL_EP *ep_ptr;

	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     "dapls_ib_accept_connection(cr %p ep %p prd %p,%d)\n",
		     cr_handle, ep_handle, p_data, p_size);

	cr_ptr = (DAPL_CR *) cr_handle;
	ep_ptr = (DAPL_EP *) ep_handle;

	/* allocate and attach a QP if necessary */
	if (ep_ptr->qp_state == DAPL_QP_STATE_UNATTACHED) {
		DAT_RETURN status;
		status = dapls_ib_qp_alloc(ep_ptr->header.owner_ia,
					   ep_ptr, ep_ptr);
		if (status != DAT_SUCCESS)
			return status;
	}
	return (dapli_socket_accept_usr(ep_ptr, cr_ptr, p_size, p_data));
}

/*
 * dapls_ib_reject_connection
 *
 * Reject a connection
 *
 * Input:
 *	cr_handle
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INTERNAL_ERROR
 *
 */
DAT_RETURN
dapls_ib_reject_connection(IN dp_ib_cm_handle_t cm_ptr,
			   IN int reason,
			   IN DAT_COUNT psize, IN const DAT_PVOID pdata)
{
	struct iovec iov[2];

	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     " reject(cm %p reason %x, pdata %p, psize %d)\n",
		     cm_ptr, reason, pdata, psize);

        if (psize > DCM_MAX_PDATA_SIZE)
                return DAT_LENGTH_ERROR;

	dapl_os_lock(&cm_ptr->lock);

	/* write reject data to indicate reject */
	cm_ptr->msg.op = htons(DCM_REJ_USER);
	cm_ptr->msg.p_size = htons(psize);
	
	iov[0].iov_base = (void *)&cm_ptr->msg;
	iov[0].iov_len = sizeof(ib_cm_msg_t) - DCM_MAX_PDATA_SIZE;
	if (psize) {
		iov[1].iov_base = pdata;
		iov[1].iov_len = psize;
		writev(cm_ptr->socket, iov, 2);
	} else {
		writev(cm_ptr->socket, iov, 1);
	}

	/* cr_thread will destroy CR */
	cm_ptr->state = DCM_DESTROY;
	send(cm_ptr->hca->ib_trans.scm[1], "w", sizeof "w", 0);
	dapl_os_unlock(&cm_ptr->lock);
	return DAT_SUCCESS;
}

/*
 * dapls_ib_cm_remote_addr
 *
 * Obtain the remote IP address given a connection
 *
 * Input:
 *	cr_handle
 *
 * Output:
 *	remote_ia_address: where to place the remote address
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INVALID_HANDLE
 *
 */
DAT_RETURN
dapls_ib_cm_remote_addr(IN DAT_HANDLE dat_handle,
			OUT DAT_SOCK_ADDR6 * remote_ia_address)
{
	DAPL_HEADER *header;
	dp_ib_cm_handle_t ib_cm_handle;

	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     "dapls_ib_cm_remote_addr(dat_handle %p, ....)\n",
		     dat_handle);

	header = (DAPL_HEADER *) dat_handle;

	if (header->magic == DAPL_MAGIC_EP)
		ib_cm_handle = ((DAPL_EP *) dat_handle)->cm_handle;
	else if (header->magic == DAPL_MAGIC_CR)
		ib_cm_handle = ((DAPL_CR *) dat_handle)->ib_cm_handle;
	else
		return DAT_INVALID_HANDLE;

	dapl_os_memcpy(remote_ia_address,
		       &ib_cm_handle->msg.daddr.so, sizeof(DAT_SOCK_ADDR6));

	return DAT_SUCCESS;
}

/*
 * dapls_ib_private_data_size
 *
 * Return the size of private data given a connection op type
 *
 * Input:
 *	prd_ptr		private data pointer
 *	conn_op		connection operation type
 *
 * If prd_ptr is NULL, this is a query for the max size supported by
 * the provider, otherwise it is the actual size of the private data
 * contained in prd_ptr.
 *
 *
 * Output:
 *	None
 *
 * Returns:
 * 	length of private data
 *
 */
int dapls_ib_private_data_size(IN DAPL_PRIVATE * prd_ptr,
			       IN DAPL_PDATA_OP conn_op, IN DAPL_HCA * hca_ptr)
{
	int size;

	switch (conn_op) {
		case DAPL_PDATA_CONN_REQ:
		case DAPL_PDATA_CONN_REP:
		case DAPL_PDATA_CONN_REJ:
		case DAPL_PDATA_CONN_DREQ:
		case DAPL_PDATA_CONN_DREP:
			size = DCM_MAX_PDATA_SIZE;
			break;
		default:
			size = 0;
	}			
	return size;
}

/*
 * Map all socket CM event codes to the DAT equivelent.
 */
#define DAPL_IB_EVENT_CNT	10

static struct ib_cm_event_map {
	const ib_cm_events_t ib_cm_event;
	DAT_EVENT_NUMBER dat_event_num;
} ib_cm_event_map[DAPL_IB_EVENT_CNT] = {
/* 00 */ {IB_CME_CONNECTED, 
	  DAT_CONNECTION_EVENT_ESTABLISHED},
/* 01 */ {IB_CME_DISCONNECTED, 
	  DAT_CONNECTION_EVENT_DISCONNECTED},
/* 02 */ {IB_CME_DISCONNECTED_ON_LINK_DOWN,
	  DAT_CONNECTION_EVENT_DISCONNECTED},
/* 03 */ {IB_CME_CONNECTION_REQUEST_PENDING, 
	  DAT_CONNECTION_REQUEST_EVENT},
/* 04 */ {IB_CME_CONNECTION_REQUEST_PENDING_PRIVATE_DATA,
	  DAT_CONNECTION_REQUEST_EVENT},
/* 05 */ {IB_CME_DESTINATION_REJECT,
	  DAT_CONNECTION_EVENT_NON_PEER_REJECTED},
/* 06 */ {IB_CME_DESTINATION_REJECT_PRIVATE_DATA,
	  DAT_CONNECTION_EVENT_PEER_REJECTED},
/* 07 */ {IB_CME_DESTINATION_UNREACHABLE, 
	  DAT_CONNECTION_EVENT_UNREACHABLE},
/* 08 */ {IB_CME_TOO_MANY_CONNECTION_REQUESTS,
	  DAT_CONNECTION_EVENT_NON_PEER_REJECTED},
/* 09 */ {IB_CME_LOCAL_FAILURE, 
	  DAT_CONNECTION_EVENT_BROKEN}
};

/*
 * dapls_ib_get_cm_event
 *
 * Return a DAT connection event given a provider CM event.
 *
 * Input:
 *	dat_event_num	DAT event we need an equivelent CM event for
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	ib_cm_event of translated DAPL value
 */
DAT_EVENT_NUMBER
dapls_ib_get_dat_event(IN const ib_cm_events_t ib_cm_event,
		       IN DAT_BOOLEAN active)
{
	DAT_EVENT_NUMBER dat_event_num;
	int i;

	active = active;

	if (ib_cm_event > IB_CME_LOCAL_FAILURE)
		return (DAT_EVENT_NUMBER) 0;

	dat_event_num = 0;
	for (i = 0; i < DAPL_IB_EVENT_CNT; i++) {
		if (ib_cm_event == ib_cm_event_map[i].ib_cm_event) {
			dat_event_num = ib_cm_event_map[i].dat_event_num;
			break;
		}
	}
	dapl_dbg_log(DAPL_DBG_TYPE_CALLBACK,
		     "dapls_ib_get_dat_event: event translate(%s) ib=0x%x dat=0x%x\n",
		     active ? "active" : "passive", ib_cm_event, dat_event_num);

	return dat_event_num;
}

/*
 * dapls_ib_get_dat_event
 *
 * Return a DAT connection event given a provider CM event.
 * 
 * Input:
 *	ib_cm_event	event provided to the dapl callback routine
 *	active		switch indicating active or passive connection
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_EVENT_NUMBER of translated provider value
 */
ib_cm_events_t dapls_ib_get_cm_event(IN DAT_EVENT_NUMBER dat_event_num)
{
	ib_cm_events_t ib_cm_event;
	int i;

	ib_cm_event = 0;
	for (i = 0; i < DAPL_IB_EVENT_CNT; i++) {
		if (dat_event_num == ib_cm_event_map[i].dat_event_num) {
			ib_cm_event = ib_cm_event_map[i].ib_cm_event;
			break;
		}
	}
	return ib_cm_event;
}

/* outbound/inbound CR processing thread to avoid blocking applications */
void cr_thread(void *arg)
{
	struct dapl_hca *hca_ptr = arg;
	dp_ib_cm_handle_t cr, next_cr;
	int opt, ret;
	socklen_t opt_len;
	char rbuf[2];
	struct dapl_fd_set *set;
	enum DAPL_FD_EVENTS event;

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " cr_thread: ENTER hca %p\n", hca_ptr);
	set = dapl_alloc_fd_set();
	if (!set)
		goto out;

	dapl_os_lock(&hca_ptr->ib_trans.lock);
	hca_ptr->ib_trans.cr_state = IB_THREAD_RUN;

	while (1) {
		dapl_fd_zero(set);
		dapl_fd_set(hca_ptr->ib_trans.scm[0], set, DAPL_FD_READ);

		if (!dapl_llist_is_empty(&hca_ptr->ib_trans.list))
			next_cr = dapl_llist_peek_head(&hca_ptr->ib_trans.list);
		else
			next_cr = NULL;

		while (next_cr) {
			cr = next_cr;
			next_cr = dapl_llist_next_entry(&hca_ptr->ib_trans.list,
							(DAPL_LLIST_ENTRY *) &
							cr->entry);
			dapl_os_lock(&cr->lock);
			if (cr->state == DCM_DESTROY
			    || hca_ptr->ib_trans.cr_state != IB_THREAD_RUN) {
				dapl_os_unlock(&cr->lock);
				dapl_llist_remove_entry(&hca_ptr->ib_trans.list,
							(DAPL_LLIST_ENTRY *) &
							cr->entry);
				dapl_dbg_log(DAPL_DBG_TYPE_CM, 
					     " CR FREE: %p ep=%p st=%d sock=%d\n", 
					     cr, cr->ep, cr->state, cr->socket);

				if (cr->socket != DAPL_INVALID_SOCKET) {
					shutdown(cr->socket, SHUT_RDWR);
					closesocket(cr->socket);
				}
				dapl_os_free(cr, sizeof(*cr));
				continue;
			}
			if (cr->socket == DAPL_INVALID_SOCKET) {
				dapl_os_unlock(&cr->lock);
				continue;
			}

			event = (cr->state == DCM_CONN_PENDING) ?
						DAPL_FD_WRITE : DAPL_FD_READ;

			if (dapl_fd_set(cr->socket, set, event)) {
				dapl_log(DAPL_DBG_TYPE_ERR,
					 " cr_thread: DESTROY CR st=%d fd %d"
					 " -> %s\n", cr->state, cr->socket,
					 inet_ntoa(((struct sockaddr_in *)
						&cr->msg.daddr.so)->sin_addr));
				dapl_os_unlock(&cr->lock);
				dapls_ib_cm_free(cr, cr->ep);
				continue;
			}
			dapl_os_unlock(&cr->lock);
			dapl_dbg_log(DAPL_DBG_TYPE_CM,
				     " poll cr=%p, sck=%d\n", cr, cr->socket);
			dapl_os_unlock(&hca_ptr->ib_trans.lock);

			ret = dapl_poll(cr->socket, event);

			dapl_dbg_log(DAPL_DBG_TYPE_CM,
				     " poll ret=0x%x cr->state=%d sck=%d\n",
				     ret, cr->state, cr->socket);

			/* data on listen, qp exchange, and on disc req */
			if ((ret == DAPL_FD_READ) || 
			    (cr->state != DCM_CONN_PENDING &&
			     ret == DAPL_FD_ERROR)) {
				if (cr->socket != DAPL_INVALID_SOCKET) {
					switch (cr->state) {
					case DCM_LISTEN:
						dapli_socket_accept(cr);
						break;
					case DCM_ACCEPTING:
						dapli_socket_accept_data(cr);
						break;
					case DCM_ACCEPTED:
						dapli_socket_accept_rtu(cr);
						break;
					case DCM_REP_PENDING:
						dapli_socket_connect_rtu(cr);
						break;
					case DCM_CONNECTED:
						dapli_socket_disconnect(cr);
						break;
					default:
						break;
					}
				}
			/* ASYNC connections, writable, readable, error; check status */
			} else if (ret == DAPL_FD_WRITE ||
				   (cr->state == DCM_CONN_PENDING && 
				    ret == DAPL_FD_ERROR)) {

			        if (ret == DAPL_FD_ERROR)
					dapl_log(DAPL_DBG_TYPE_ERR, " CONN_PENDING - FD_ERROR\n");
				
				opt = 0;
				opt_len = sizeof(opt);
				ret = getsockopt(cr->socket, SOL_SOCKET,
						 SO_ERROR, (char *)&opt,
						 &opt_len);
				if (!ret && !opt)
					dapli_socket_connected(cr, opt);
				else
					dapli_socket_connected(cr, opt ? opt : dapl_socket_errno());
			} 
			dapl_os_lock(&hca_ptr->ib_trans.lock);
		}

		/* set to exit and all resources destroyed */
		if ((hca_ptr->ib_trans.cr_state != IB_THREAD_RUN) &&
		    (dapl_llist_is_empty(&hca_ptr->ib_trans.list)))
			break;

		dapl_os_unlock(&hca_ptr->ib_trans.lock);
		dapl_select(set);

		/* if pipe used to wakeup, consume */
		while (dapl_poll(hca_ptr->ib_trans.scm[0], 
				 DAPL_FD_READ) == DAPL_FD_READ) {
			if (recv(hca_ptr->ib_trans.scm[0], rbuf, 2, 0) == -1)
				dapl_log(DAPL_DBG_TYPE_CM,
					 " cr_thread: read pipe error = %s\n",
					 strerror(errno));
		}
		dapl_os_lock(&hca_ptr->ib_trans.lock);
		
		/* set to exit and all resources destroyed */
		if ((hca_ptr->ib_trans.cr_state != IB_THREAD_RUN) &&
		    (dapl_llist_is_empty(&hca_ptr->ib_trans.list)))
			break;
	}

	dapl_os_unlock(&hca_ptr->ib_trans.lock);
	dapl_os_free(set, sizeof(struct dapl_fd_set));
out:
	hca_ptr->ib_trans.cr_state = IB_THREAD_EXIT;
	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " cr_thread(hca %p) exit\n", hca_ptr);
}


#ifdef DAPL_COUNTERS
/* Debug aid: List all Connections in process and state */
void dapls_print_cm_list(IN DAPL_IA *ia_ptr)
{
	/* Print in process CR's for this IA, if debug type set */
	int i = 0;
	dp_ib_cm_handle_t cr, next_cr;

	dapl_os_lock(&ia_ptr->hca_ptr->ib_trans.lock);
	if (!dapl_llist_is_empty((DAPL_LLIST_HEAD*)
				 &ia_ptr->hca_ptr->ib_trans.list))
				 next_cr = dapl_llist_peek_head((DAPL_LLIST_HEAD*)
				 &ia_ptr->hca_ptr->ib_trans.list);
 	else
		next_cr = NULL;

        printf("\n DAPL IA CONNECTIONS IN PROCESS:\n");
	while (next_cr) {
		cr = next_cr;
		next_cr = dapl_llist_next_entry((DAPL_LLIST_HEAD*)
				 &ia_ptr->hca_ptr->ib_trans.list,
				(DAPL_LLIST_ENTRY*)&cr->entry);

		printf( "  CONN[%d]: sp %p ep %p sock %d %s %s %s %s %s %s PORT L-%x R-%x PID L-%x R-%x\n",
			i, cr->sp, cr->ep, cr->socket,
			cr->msg.saddr.ib.qp_type == IBV_QPT_RC ? "RC" : "UD",
			dapl_cm_state_str(cr->state), dapl_cm_op_str(ntohs(cr->msg.op)),
			ntohs(cr->msg.op) == DCM_REQ ? /* local address */
				inet_ntoa(((struct sockaddr_in *)&cr->msg.daddr.so)->sin_addr) :
				inet_ntoa(((struct sockaddr_in *)&cr->addr)->sin_addr),
			cr->sp ? "<-" : "->",
                       	ntohs(cr->msg.op) == DCM_REQ ? /* remote address */
				inet_ntoa(((struct sockaddr_in *)&cr->addr)->sin_addr) :
				inet_ntoa(((struct sockaddr_in *)&cr->msg.daddr.so)->sin_addr),

			ntohs(cr->msg.op) == DCM_REQ ? /* local port */
				ntohs(((struct sockaddr_in *)&cr->msg.daddr.so)->sin_port) :
				ntohs(((struct sockaddr_in *)&cr->addr)->sin_port),

			ntohs(cr->msg.op) == DCM_REQ ? /* remote port */
				ntohs(((struct sockaddr_in *)&cr->addr)->sin_port) :
				ntohs(((struct sockaddr_in *)&cr->msg.daddr.so)->sin_port),

			cr->sp ? ntohs(*(uint16_t*)&cr->msg.resv[2]) : ntohs(*(uint16_t*)&cr->msg.resv[0]),
			cr->sp ? ntohs(*(uint16_t*)&cr->msg.resv[0]) : ntohs(*(uint16_t*)&cr->msg.resv[2]));

		i++;
	}
	printf("\n");
	dapl_os_unlock(&ia_ptr->hca_ptr->ib_trans.lock);
}
#endif
