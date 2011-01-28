/*
 * Copyright (c) 2005-2008 Intel Corporation.  All rights reserved.
 *
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

/**********************************************************************
 * 
 * MODULE: dapl_ib_util.c
 *
 * PURPOSE: OFED provider - init, open, close, utilities, work thread
 *
 * $Id:$
 *
 **********************************************************************/

#ifdef RCSID
static const char rcsid[] = "$Id:  $";
#endif

#include "openib_osd.h"
#include "dapl.h"
#include "dapl_adapter_util.h"
#include "dapl_ib_util.h"
#include "dapl_osd.h"

#include <stdlib.h>

struct rdma_event_channel *g_cm_events = NULL;
ib_thread_state_t g_ib_thread_state = 0;
DAPL_OS_THREAD g_ib_thread;
DAPL_OS_LOCK g_hca_lock;
struct dapl_llist_entry *g_hca_list;

#if defined(_WIN64) || defined(_WIN32)
#include "..\..\..\..\..\etc\user\comp_channel.cpp"
#include <rdma\winverbs.h>

static COMP_SET ufds;

static int getipaddr_netdev(char *name, char *addr, int addr_len)
{
	IWVProvider *prov;
	WV_DEVICE_ADDRESS devaddr;
	struct addrinfo *res, *ai;
	HRESULT hr;
	int index;

	if (strncmp(name, "rdma_dev", 8)) {
		return EINVAL;
	}

	index = atoi(name + 8);

	hr = WvGetObject(&IID_IWVProvider, (LPVOID *) &prov);
	if (FAILED(hr)) {
		return hr;
	}

	hr = getaddrinfo("..localmachine", NULL, NULL, &res);
	if (hr) {
		goto release;
	}

	for (ai = res; ai; ai = ai->ai_next) {
		hr = prov->lpVtbl->TranslateAddress(prov, ai->ai_addr, &devaddr);
		if (SUCCEEDED(hr) && (ai->ai_addrlen <= addr_len) && (index-- == 0)) {
			memcpy(addr, ai->ai_addr, ai->ai_addrlen);
			goto free;
		}
	}
	hr = ENODEV;

free:
	freeaddrinfo(res);
release:
	prov->lpVtbl->Release(prov);
	return hr;
}

static int dapls_os_init(void)
{
	return CompSetInit(&ufds);
}

static void dapls_os_release(void)
{
	CompSetCleanup(&ufds);
}

static int dapls_config_cm_channel(struct rdma_event_channel *channel)
{
	channel->channel.Milliseconds = 0;
	return 0;
}

static int dapls_config_verbs(struct ibv_context *verbs)
{
	verbs->channel.Milliseconds = 0;
	return 0;
}

static int dapls_config_comp_channel(struct ibv_comp_channel *channel)
{
	channel->comp_channel.Milliseconds = 0;
	return 0;
}

static int dapls_thread_signal(void)
{
	CompSetCancel(&ufds);
	return 0;
}
#else				// _WIN64 || WIN32
int g_ib_pipe[2];

static int dapls_os_init(void)
{
	/* create pipe for waking up work thread */
	return pipe(g_ib_pipe);
}

static void dapls_os_release(void)
{
	/* close pipe? */
}

/* Get IP address using network device name */
static int getipaddr_netdev(char *name, char *addr, int addr_len)
{
	struct ifreq ifr;
	int skfd, ret, len;

	/* Fill in the structure */
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", name);
#ifndef __FreeBSD__
	ifr.ifr_hwaddr.sa_family = ARPHRD_INFINIBAND;
#endif

	/* Create a socket fd */
	skfd = socket(PF_INET, SOCK_STREAM, 0);
	ret = ioctl(skfd, SIOCGIFADDR, &ifr);
	if (ret)
		goto bail;

	switch (ifr.ifr_addr.sa_family) {
#ifdef	AF_INET6
	case AF_INET6:
		len = sizeof(struct sockaddr_in6);
		break;
#endif
	case AF_INET:
	default:
		len = sizeof(struct sockaddr);
		break;
	}

	if (len <= addr_len)
		memcpy(addr, &ifr.ifr_addr, len);
	else
		ret = EINVAL;

      bail:
	close(skfd);
	return ret;
}

static int dapls_config_fd(int fd)
{
	int opts;

	opts = fcntl(fd, F_GETFL);
	if (opts < 0 || fcntl(fd, F_SETFL, opts | O_NONBLOCK) < 0) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " dapls_config_fd: fcntl on fd %d ERR %d %s\n",
			 fd, opts, strerror(errno));
		return errno;
	}

	return 0;
}

static int dapls_config_cm_channel(struct rdma_event_channel *channel)
{
	return dapls_config_fd(channel->fd);
}

static int dapls_config_verbs(struct ibv_context *verbs)
{
	return dapls_config_fd(verbs->async_fd);
}

static int dapls_config_comp_channel(struct ibv_comp_channel *channel)
{
	return dapls_config_fd(channel->fd);
}

static int dapls_thread_signal(void)
{
	return write(g_ib_pipe[1], "w", sizeof "w");
}
#endif

/* Get IP address using network name, address, or device name */
static int getipaddr(char *name, char *addr, int len)
{
	struct addrinfo *res;

	/* assume netdev for first attempt, then network and address type */
	if (getipaddr_netdev(name, addr, len)) {
		if (getaddrinfo(name, NULL, NULL, &res)) {
			dapl_log(DAPL_DBG_TYPE_ERR,
				 " open_hca: getaddr_netdev ERROR:"
				 " %s. Is %s configured?\n",
				 strerror(errno), name);
			return 1;
		} else {
			if (len >= res->ai_addrlen)
				memcpy(addr, res->ai_addr, res->ai_addrlen);
			else {
				freeaddrinfo(res);
				return 1;
			}
			freeaddrinfo(res);
		}
	}

	dapl_dbg_log(
		DAPL_DBG_TYPE_UTIL,
		" getipaddr: family %d port %d addr %d.%d.%d.%d\n",
		((struct sockaddr_in *)addr)->sin_family,
		((struct sockaddr_in *)addr)->sin_port,
		((struct sockaddr_in *)addr)->sin_addr.s_addr >> 0 & 0xff,
		((struct sockaddr_in *)addr)->sin_addr.s_addr >> 8 & 0xff,
		((struct sockaddr_in *)addr)->sin_addr.s_addr >> 16 & 0xff,
		((struct sockaddr_in *)addr)->sin_addr.
		 s_addr >> 24 & 0xff);

	return 0;
}

/*
 * dapls_ib_init, dapls_ib_release
 *
 * Initialize Verb related items for device open
 *
 * Input:
 * 	none
 *
 * Output:
 *	none
 *
 * Returns:
 * 	0 success, -1 error
 *
 */
int32_t dapls_ib_init(void)
{
	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " dapl_ib_init: \n");

	/* initialize hca_list lock */
	dapl_os_lock_init(&g_hca_lock);

	/* initialize hca list for CQ events */
	dapl_llist_init_head(&g_hca_list);

	if (dapls_os_init())
		return 1;

	return 0;
}

int32_t dapls_ib_release(void)
{
	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " dapl_ib_release: \n");
	dapli_ib_thread_destroy();
	if (g_cm_events != NULL)
		rdma_destroy_event_channel(g_cm_events);
	dapls_os_release();
	return 0;
}

/*
 * dapls_ib_open_hca
 *
 * Open HCA
 *
 * Input:
 *      *hca_name         pointer to provider device name
 *      *ib_hca_handle_p  pointer to provide HCA handle
 *
 * Output:
 *      none
 *
 * Return:
 *      DAT_SUCCESS
 *      dapl_convert_errno
 *
 */
DAT_RETURN dapls_ib_open_hca(IN IB_HCA_NAME hca_name, IN DAPL_HCA * hca_ptr)
{
	struct rdma_cm_id *cm_id = NULL;
	union ibv_gid *gid;
	int ret;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
		     " open_hca: %s - %p\n", hca_name, hca_ptr);

	/* Setup the global cm event channel */
	dapl_os_lock(&g_hca_lock);
	if (g_cm_events == NULL) {
		g_cm_events = rdma_create_event_channel();
		if (g_cm_events == NULL) {
			dapl_dbg_log(DAPL_DBG_TYPE_ERR,
				     " open_hca: ERR - RDMA channel %s\n",
				     strerror(errno));
			dapl_os_unlock(&g_hca_lock);
			return DAT_INTERNAL_ERROR;
		}
	}
	dapl_os_unlock(&g_hca_lock);

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
		     " open_hca: RDMA channel created (%p)\n", g_cm_events);

	/* HCA name will be hostname or IP address */
	if (getipaddr((char *)hca_name,
		      (char *)&hca_ptr->hca_address, 
		      sizeof(DAT_SOCK_ADDR6)))
		return DAT_INVALID_ADDRESS;

	/* cm_id will bind local device/GID based on IP address */
	if (rdma_create_id(g_cm_events, &cm_id, 
			   (void *)hca_ptr, RDMA_PS_TCP)) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " open_hca: rdma_create ERR %s\n", strerror(errno));
		return DAT_INTERNAL_ERROR;
	}
	ret = rdma_bind_addr(cm_id, (struct sockaddr *)&hca_ptr->hca_address);
	if ((ret) || (cm_id->verbs == NULL)) {
		rdma_destroy_id(cm_id);
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " open_hca: rdma_bind ERR %s."
			 " Is %s configured?\n", strerror(errno), hca_name);
		rdma_destroy_id(cm_id);
		return DAT_INVALID_ADDRESS;
	}

	/* keep reference to IB device and cm_id */
	hca_ptr->ib_trans.cm_id = cm_id;
	hca_ptr->ib_hca_handle = cm_id->verbs;
	dapls_config_verbs(cm_id->verbs);
	hca_ptr->port_num = cm_id->port_num;
	hca_ptr->ib_trans.ib_dev = cm_id->verbs->device;
	hca_ptr->ib_trans.ib_ctx = cm_id->verbs;
	gid = &cm_id->route.addr.addr.ibaddr.sgid;

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
		     " open_hca: ctx=%p port=%d GID subnet %016llx"
		     " id %016llx\n", cm_id->verbs, cm_id->port_num,
		     (unsigned long long)ntohll(gid->global.subnet_prefix),
		     (unsigned long long)ntohll(gid->global.interface_id));

	/* support for EVD's with CNO's: one channel via thread */
	hca_ptr->ib_trans.ib_cq =
	    ibv_create_comp_channel(hca_ptr->ib_hca_handle);
	if (hca_ptr->ib_trans.ib_cq == NULL) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " open_hca: ibv_create_comp_channel ERR %s\n",
			 strerror(errno));
		rdma_destroy_id(cm_id);
		return DAT_INTERNAL_ERROR;
	}
	if (dapls_config_comp_channel(hca_ptr->ib_trans.ib_cq)) {
		rdma_destroy_id(cm_id);
		return DAT_INTERNAL_ERROR;
	}

	/* set inline max with env or default, get local lid and gid 0 */
	if (hca_ptr->ib_hca_handle->device->transport_type
	    == IBV_TRANSPORT_IWARP)
		hca_ptr->ib_trans.max_inline_send =
		    dapl_os_get_env_val("DAPL_MAX_INLINE",
					INLINE_SEND_IWARP_DEFAULT);
	else
		hca_ptr->ib_trans.max_inline_send =
		    dapl_os_get_env_val("DAPL_MAX_INLINE",
					INLINE_SEND_IB_DEFAULT);

	/* set CM timer defaults */
	hca_ptr->ib_trans.max_cm_timeout =
	    dapl_os_get_env_val("DAPL_MAX_CM_RESPONSE_TIME",
				IB_CM_RESPONSE_TIMEOUT);
	hca_ptr->ib_trans.max_cm_retries =
	    dapl_os_get_env_val("DAPL_MAX_CM_RETRIES", IB_CM_RETRIES);
	
	/* set default IB MTU */
	hca_ptr->ib_trans.mtu = dapl_ib_mtu(2048);

	dat_status = dapli_ib_thread_init();
	if (dat_status != DAT_SUCCESS)
		return dat_status;
	/* 
	 * Put new hca_transport on list for async and CQ event processing 
	 * Wakeup work thread to add to polling list
	 */
	dapl_llist_init_entry((DAPL_LLIST_ENTRY *) &hca_ptr->ib_trans.entry);
	dapl_os_lock(&g_hca_lock);
	dapl_llist_add_tail(&g_hca_list,
			    (DAPL_LLIST_ENTRY *) &hca_ptr->ib_trans.entry,
			    &hca_ptr->ib_trans.entry);
	if (dapls_thread_signal() == -1)
		dapl_log(DAPL_DBG_TYPE_UTIL,
			 " open_hca: thread wakeup error = %s\n",
			 strerror(errno));
	dapl_os_unlock(&g_hca_lock);

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
		     " open_hca: %s, %s %d.%d.%d.%d INLINE_MAX=%d\n", hca_name,
		     ((struct sockaddr_in *)
		     &hca_ptr->hca_address)->sin_family == AF_INET ?
		     "AF_INET" : "AF_INET6", 
		     ((struct sockaddr_in *)
		     &hca_ptr->hca_address)->sin_addr.s_addr >> 0 & 0xff, 
		     ((struct sockaddr_in *)
		     &hca_ptr->hca_address)->sin_addr.s_addr >> 8 & 0xff, 
		     ((struct sockaddr_in *)
		     &hca_ptr->hca_address)->sin_addr.s_addr >> 16 & 0xff, 
		     ((struct sockaddr_in *)
		     &hca_ptr->hca_address)->sin_addr.s_addr >> 24 & 0xff, 
		     hca_ptr->ib_trans.max_inline_send);

	return DAT_SUCCESS;
}

/*
 * dapls_ib_close_hca
 *
 * Open HCA
 *
 * Input:
 *      DAPL_HCA   provide CA handle
 *
 * Output:
 *      none
 *
 * Return:
 *      DAT_SUCCESS
 *	dapl_convert_errno 
 *
 */
DAT_RETURN dapls_ib_close_hca(IN DAPL_HCA * hca_ptr)
{
	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " close_hca: %p->%p\n",
		     hca_ptr, hca_ptr->ib_hca_handle);

	if (hca_ptr->ib_hca_handle != IB_INVALID_HANDLE) {
		if (rdma_destroy_id(hca_ptr->ib_trans.cm_id))
			return (dapl_convert_errno(errno, "ib_close_device"));
		hca_ptr->ib_hca_handle = IB_INVALID_HANDLE;
	}

	dapl_os_lock(&g_hca_lock);
	if (g_ib_thread_state != IB_THREAD_RUN) {
		dapl_os_unlock(&g_hca_lock);
		goto bail;
	}
	dapl_os_unlock(&g_hca_lock);

	/* 
	 * Remove hca from async event processing list
	 * Wakeup work thread to remove from polling list
	 */
	hca_ptr->ib_trans.destroy = 1;
	if (dapls_thread_signal() == -1)
		dapl_log(DAPL_DBG_TYPE_UTIL,
			 " destroy: thread wakeup error = %s\n",
			 strerror(errno));

	/* wait for thread to remove HCA references */
	while (hca_ptr->ib_trans.destroy != 2) {
		if (dapls_thread_signal() == -1)
			dapl_log(DAPL_DBG_TYPE_UTIL,
				 " destroy: thread wakeup error = %s\n",
				 strerror(errno));
		dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
			     " ib_thread_destroy: wait on hca %p destroy\n");
		dapl_os_sleep_usec(1000);
	}
bail:
	return (DAT_SUCCESS);
}


DAT_RETURN dapli_ib_thread_init(void)
{
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
		     " ib_thread_init(%d)\n", dapl_os_getpid());

	dapl_os_lock(&g_hca_lock);
	if (g_ib_thread_state != IB_THREAD_INIT) {
		dapl_os_unlock(&g_hca_lock);
		return DAT_SUCCESS;
	}

	/* uCMA events non-blocking */
	if (dapls_config_cm_channel(g_cm_events)) {
		dapl_os_unlock(&g_hca_lock);
		return (dapl_convert_errno(errno, "create_thread ERR: cm_fd"));
	}

	g_ib_thread_state = IB_THREAD_CREATE;
	dapl_os_unlock(&g_hca_lock);

	/* create thread to process inbound connect request */
	dat_status = dapl_os_thread_create(dapli_thread, NULL, &g_ib_thread);
	if (dat_status != DAT_SUCCESS)
		return (dapl_convert_errno(errno,
					   "create_thread ERR:"
					   " check resource limits"));

	/* wait for thread to start */
	dapl_os_lock(&g_hca_lock);
	while (g_ib_thread_state != IB_THREAD_RUN) {
		dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
			     " ib_thread_init: waiting for ib_thread\n");
		dapl_os_unlock(&g_hca_lock);
		dapl_os_sleep_usec(1000);
		dapl_os_lock(&g_hca_lock);
	}
	dapl_os_unlock(&g_hca_lock);

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
		     " ib_thread_init(%d) exit\n", dapl_os_getpid());

	return DAT_SUCCESS;
}

void dapli_ib_thread_destroy(void)
{
	dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
		     " ib_thread_destroy(%d)\n", dapl_os_getpid());
	/* 
	 * wait for async thread to terminate. 
	 * pthread_join would be the correct method
	 * but some applications have some issues
	 */

	/* destroy ib_thread, wait for termination, if not already */
	dapl_os_lock(&g_hca_lock);
	if (g_ib_thread_state != IB_THREAD_RUN)
		goto bail;

	g_ib_thread_state = IB_THREAD_CANCEL;
	while ((g_ib_thread_state != IB_THREAD_EXIT)) {
		dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
			     " ib_thread_destroy: waiting for ib_thread\n");
		if (dapls_thread_signal() == -1)
			dapl_log(DAPL_DBG_TYPE_UTIL,
				 " destroy: thread wakeup error = %s\n",
				 strerror(errno));
		dapl_os_unlock(&g_hca_lock);
		dapl_os_sleep_usec(2000);
		dapl_os_lock(&g_hca_lock);
	}
bail:
	dapl_os_unlock(&g_hca_lock);

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
		     " ib_thread_destroy(%d) exit\n", dapl_os_getpid());
}

#if defined(_WIN64) || defined(_WIN32)
/* work thread for uAT, uCM, CQ, and async events */
void dapli_thread(void *arg)
{
	struct _ib_hca_transport *hca;
	struct _ib_hca_transport *uhca[8];
	COMP_CHANNEL *channel;
	int ret, idx, cnt;

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " ib_thread(%d,0x%x): ENTER: \n",
		     dapl_os_getpid(), g_ib_thread);

	dapl_os_lock(&g_hca_lock);
	for (g_ib_thread_state = IB_THREAD_RUN;
	     g_ib_thread_state == IB_THREAD_RUN; 
	     dapl_os_lock(&g_hca_lock)) {

		CompSetZero(&ufds);
		CompSetAdd(&g_cm_events->channel, &ufds);

		idx = 0;
		hca = dapl_llist_is_empty(&g_hca_list) ? NULL :
		      dapl_llist_peek_head(&g_hca_list);

		while (hca) {
			CompSetAdd(&hca->ib_ctx->channel, &ufds);
			CompSetAdd(&hca->ib_cq->comp_channel, &ufds);
			uhca[idx++] = hca;
			hca = dapl_llist_next_entry(&g_hca_list,
						    (DAPL_LLIST_ENTRY *)
						    &hca->entry);
		}
		cnt = idx;

		dapl_os_unlock(&g_hca_lock);
		ret = CompSetPoll(&ufds, INFINITE);

		dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
			     " ib_thread(%d) poll_event 0x%x\n",
			     dapl_os_getpid(), ret);

		dapli_cma_event_cb();

		/* check and process ASYNC events, per device */
		for (idx = 0; idx < cnt; idx++) {
			if (uhca[idx]->destroy == 1) {
				dapl_os_lock(&g_hca_lock);
				dapl_llist_remove_entry(&g_hca_list,
							(DAPL_LLIST_ENTRY *)
							&uhca[idx]->entry);
				dapl_os_unlock(&g_hca_lock);
				uhca[idx]->destroy = 2;
			} else {
				dapli_cq_event_cb(uhca[idx]);
				dapli_async_event_cb(uhca[idx]);
			}
		}
	}

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " ib_thread(%d) EXIT\n",
		     dapl_os_getpid());
	g_ib_thread_state = IB_THREAD_EXIT;
	dapl_os_unlock(&g_hca_lock);
}
#else				// _WIN64 || WIN32

/* work thread for uAT, uCM, CQ, and async events */
void dapli_thread(void *arg)
{
	struct pollfd ufds[FD_SETSIZE];
	struct _ib_hca_transport *uhca[FD_SETSIZE] = { NULL };
	struct _ib_hca_transport *hca;
	int ret, idx, fds;
	char rbuf[2];

	dapl_dbg_log(DAPL_DBG_TYPE_THREAD,
		     " ib_thread(%d,0x%x): ENTER: pipe %d ucma %d\n",
		     dapl_os_getpid(), g_ib_thread, g_ib_pipe[0],
		     g_cm_events->fd);

	/* Poll across pipe, CM, AT never changes */
	dapl_os_lock(&g_hca_lock);
	g_ib_thread_state = IB_THREAD_RUN;

	ufds[0].fd = g_ib_pipe[0];	/* pipe */
	ufds[0].events = POLLIN;
	ufds[1].fd = g_cm_events->fd;	/* uCMA */
	ufds[1].events = POLLIN;

	while (g_ib_thread_state == IB_THREAD_RUN) {

		/* build ufds after pipe and uCMA events */
		ufds[0].revents = 0;
		ufds[1].revents = 0;
		idx = 1;

		/*  Walk HCA list and setup async and CQ events */
		if (!dapl_llist_is_empty(&g_hca_list))
			hca = dapl_llist_peek_head(&g_hca_list);
		else
			hca = NULL;

		while (hca) {

			/* uASYNC events */
			ufds[++idx].fd = hca->ib_ctx->async_fd;
			ufds[idx].events = POLLIN;
			ufds[idx].revents = 0;
			uhca[idx] = hca;

			/* CQ events are non-direct with CNO's */
			ufds[++idx].fd = hca->ib_cq->fd;
			ufds[idx].events = POLLIN;
			ufds[idx].revents = 0;
			uhca[idx] = hca;

			dapl_dbg_log(DAPL_DBG_TYPE_THREAD,
				     " ib_thread(%d) poll_fd: hca[%d]=%p,"
				     " async=%d pipe=%d cm=%d \n",
				     dapl_os_getpid(), hca, ufds[idx - 1].fd,
				     ufds[0].fd, ufds[1].fd);

			hca = dapl_llist_next_entry(&g_hca_list,
						    (DAPL_LLIST_ENTRY *)
						    &hca->entry);
		}

		/* unlock, and setup poll */
		fds = idx + 1;
		dapl_os_unlock(&g_hca_lock);
		ret = poll(ufds, fds, -1);
		if (ret <= 0) {
			dapl_dbg_log(DAPL_DBG_TYPE_THREAD,
				     " ib_thread(%d): ERR %s poll\n",
				     dapl_os_getpid(), strerror(errno));
			dapl_os_lock(&g_hca_lock);
			continue;
		}

		dapl_dbg_log(DAPL_DBG_TYPE_THREAD,
			     " ib_thread(%d) poll_event: "
			     " async=0x%x pipe=0x%x cm=0x%x \n",
			     dapl_os_getpid(), ufds[idx].revents,
			     ufds[0].revents, ufds[1].revents);

		/* uCMA events */
		if (ufds[1].revents == POLLIN)
			dapli_cma_event_cb();

		/* check and process CQ and ASYNC events, per device */
		for (idx = 2; idx < fds; idx++) {
			if (ufds[idx].revents == POLLIN) {
				dapli_cq_event_cb(uhca[idx]);
				dapli_async_event_cb(uhca[idx]);
			}
		}

		/* check and process user events, PIPE */
		if (ufds[0].revents == POLLIN) {
			if (read(g_ib_pipe[0], rbuf, 2) == -1)
				dapl_log(DAPL_DBG_TYPE_THREAD,
					 " cr_thread: pipe rd err= %s\n",
					 strerror(errno));

			/* cleanup any device on list marked for destroy */
			for (idx = 3; idx < fds; idx++) {
				if (uhca[idx] && uhca[idx]->destroy == 1) {
					dapl_os_lock(&g_hca_lock);
					dapl_llist_remove_entry(
						&g_hca_list,
						(DAPL_LLIST_ENTRY*)
						&uhca[idx]->entry);
					dapl_os_unlock(&g_hca_lock);
					uhca[idx]->destroy = 2;
				}
			}
		}
		dapl_os_lock(&g_hca_lock);
	}

	dapl_dbg_log(DAPL_DBG_TYPE_THREAD, " ib_thread(%d) EXIT\n",
		     dapl_os_getpid());
	g_ib_thread_state = IB_THREAD_EXIT;
	dapl_os_unlock(&g_hca_lock);
}
#endif
