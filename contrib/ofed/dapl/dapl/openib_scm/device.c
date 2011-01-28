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
 *   Filename:		 dapl_ib_util.c
 *
 *   Author:		 Arlin Davis
 *
 *   Created:		 3/10/2005
 *
 *   Description: 
 *
 *   The uDAPL openib provider - init, open, close, utilities
 *
 ****************************************************************************
 *		   Source Control System Information
 *
 *    $Id: $
 *
 *	Copyright (c) 2005 Intel Corporation.  All rights reserved.
 *
 **************************************************************************/
#ifdef RCSID
static const char rcsid[] = "$Id:  $";
#endif

#include "openib_osd.h"
#include "dapl.h"
#include "dapl_adapter_util.h"
#include "dapl_ib_util.h"
#include "dapl_osd.h"

#include <stdlib.h>

ib_thread_state_t g_ib_thread_state = 0;
DAPL_OS_THREAD g_ib_thread;
DAPL_OS_LOCK g_hca_lock;
struct dapl_llist_entry *g_hca_list;

void dapli_thread(void *arg);
DAT_RETURN  dapli_ib_thread_init(void);
void dapli_ib_thread_destroy(void);

#if defined(_WIN64) || defined(_WIN32)
#include "..\..\..\..\..\etc\user\comp_channel.cpp"
#include <rdma\winverbs.h>

static COMP_SET ufds;

static int dapls_os_init(void)
{
	return CompSetInit(&ufds);
}

static void dapls_os_release(void)
{
	CompSetCleanup(&ufds);
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


static int32_t create_cr_pipe(IN DAPL_HCA * hca_ptr)
{
	DAPL_SOCKET listen_socket;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	int ret;

	listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_socket == DAPL_INVALID_SOCKET)
		return 1;

	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(0x7f000001);
	ret = bind(listen_socket, (struct sockaddr *)&addr, sizeof addr);
	if (ret)
		goto err1;

	ret = getsockname(listen_socket, (struct sockaddr *)&addr, &addrlen);
	if (ret)
		goto err1;

	ret = listen(listen_socket, 0);
	if (ret)
		goto err1;

	hca_ptr->ib_trans.scm[1] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (hca_ptr->ib_trans.scm[1] == DAPL_INVALID_SOCKET)
		goto err1;

	ret = connect(hca_ptr->ib_trans.scm[1], 
		      (struct sockaddr *)&addr, sizeof(addr));
	if (ret)
		goto err2;

	hca_ptr->ib_trans.scm[0] = accept(listen_socket, NULL, NULL);
	if (hca_ptr->ib_trans.scm[0] == DAPL_INVALID_SOCKET)
		goto err2;

	closesocket(listen_socket);
	return 0;

      err2:
	closesocket(hca_ptr->ib_trans.scm[1]);
      err1:
	closesocket(listen_socket);
	return 1;
}

static void destroy_cr_pipe(IN DAPL_HCA * hca_ptr)
{
	closesocket(hca_ptr->ib_trans.scm[0]);
	closesocket(hca_ptr->ib_trans.scm[1]);
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
	/* initialize hca_list */
	dapl_os_lock_init(&g_hca_lock);
	dapl_llist_init_head(&g_hca_list);

	if (dapls_os_init())
		return 1;

	return 0;
}

int32_t dapls_ib_release(void)
{
	dapli_ib_thread_destroy();
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
	struct ibv_device **dev_list;
	struct ibv_port_attr port_attr;
	int i;
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
		     " open_hca: %s - %p\n", hca_name, hca_ptr);

	/* get the IP address of the device */
	dat_status = getlocalipaddr((DAT_SOCK_ADDR *) &hca_ptr->hca_address,
				    sizeof(DAT_SOCK_ADDR6));
	if (dat_status != DAT_SUCCESS)
		return dat_status;

#ifdef DAPL_DBG
	/* DBG: unused port, set process id, lower 16 bits of pid */
	((struct sockaddr_in *)&hca_ptr->hca_address)->sin_port = 
					htons((uint16_t)dapl_os_getpid());
#endif
        /* Get list of all IB devices, find match, open */
	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     " open_hca: ibv_get_device_list() failed\n",
			     hca_name);
		return DAT_INTERNAL_ERROR;
	}

	for (i = 0; dev_list[i]; ++i) {
		hca_ptr->ib_trans.ib_dev = dev_list[i];
		if (!strcmp(ibv_get_device_name(hca_ptr->ib_trans.ib_dev),
			    hca_name))
			goto found;
	}

	dapl_log(DAPL_DBG_TYPE_ERR,
		 " open_hca: device %s not found\n", hca_name);
	goto err;

found:
	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " open_hca: Found dev %s %016llx\n",
		     ibv_get_device_name(hca_ptr->ib_trans.ib_dev),
		     (unsigned long long)
		     ntohll(ibv_get_device_guid(hca_ptr->ib_trans.ib_dev)));

	hca_ptr->ib_hca_handle = ibv_open_device(hca_ptr->ib_trans.ib_dev);
	if (!hca_ptr->ib_hca_handle) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " open_hca: dev open failed for %s, err=%s\n",
			 ibv_get_device_name(hca_ptr->ib_trans.ib_dev),
			 strerror(errno));
		goto err;
	}
	hca_ptr->ib_trans.ib_ctx = hca_ptr->ib_hca_handle;
	dapls_config_verbs(hca_ptr->ib_hca_handle);

	/* get lid for this hca-port, network order */
	if (ibv_query_port(hca_ptr->ib_hca_handle,
			   (uint8_t) hca_ptr->port_num, &port_attr)) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " open_hca: get lid ERR for %s, err=%s\n",
			 ibv_get_device_name(hca_ptr->ib_trans.ib_dev),
			 strerror(errno));
		goto err;
	} else {
		hca_ptr->ib_trans.lid = htons(port_attr.lid);
	}

	/* get gid for this hca-port, network order */
	if (ibv_query_gid(hca_ptr->ib_hca_handle,
			  (uint8_t) hca_ptr->port_num,
			  0, &hca_ptr->ib_trans.gid)) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " open_hca: query GID ERR for %s, err=%s\n",
			 ibv_get_device_name(hca_ptr->ib_trans.ib_dev),
			 strerror(errno));
		goto err;
	}

	/* set RC tunables via enviroment or default */
	hca_ptr->ib_trans.max_inline_send =
	    dapl_os_get_env_val("DAPL_MAX_INLINE", INLINE_SEND_DEFAULT);
	hca_ptr->ib_trans.ack_retry =
	    dapl_os_get_env_val("DAPL_ACK_RETRY", SCM_ACK_RETRY);
	hca_ptr->ib_trans.ack_timer =
	    dapl_os_get_env_val("DAPL_ACK_TIMER", SCM_ACK_TIMER);
	hca_ptr->ib_trans.rnr_retry =
	    dapl_os_get_env_val("DAPL_RNR_RETRY", SCM_RNR_RETRY);
	hca_ptr->ib_trans.rnr_timer =
	    dapl_os_get_env_val("DAPL_RNR_TIMER", SCM_RNR_TIMER);
	hca_ptr->ib_trans.global =
	    dapl_os_get_env_val("DAPL_GLOBAL_ROUTING", SCM_GLOBAL);
	hca_ptr->ib_trans.hop_limit =
	    dapl_os_get_env_val("DAPL_HOP_LIMIT", SCM_HOP_LIMIT);
	hca_ptr->ib_trans.tclass =
	    dapl_os_get_env_val("DAPL_TCLASS", SCM_TCLASS);
	hca_ptr->ib_trans.mtu =
	    dapl_ib_mtu(dapl_os_get_env_val("DAPL_IB_MTU", SCM_IB_MTU));


	/* EVD events without direct CQ channels, CNO support */
	hca_ptr->ib_trans.ib_cq =
	    ibv_create_comp_channel(hca_ptr->ib_hca_handle);
	if (hca_ptr->ib_trans.ib_cq == NULL) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " open_hca: ibv_create_comp_channel ERR %s\n",
			 strerror(errno));
		goto bail;
	}
	dapls_config_comp_channel(hca_ptr->ib_trans.ib_cq);
	
	dat_status = dapli_ib_thread_init();
	if (dat_status != DAT_SUCCESS) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " open_hca: failed to init cq thread lock\n");
		goto bail;
	}
	/* 
	 * Put new hca_transport on list for async and CQ event processing 
	 * Wakeup work thread to add to polling list
	 */
	dapl_llist_init_entry((DAPL_LLIST_ENTRY *)&hca_ptr->ib_trans.entry);
	dapl_os_lock(&g_hca_lock);
	dapl_llist_add_tail(&g_hca_list,
			    (DAPL_LLIST_ENTRY *) &hca_ptr->ib_trans.entry,
			    &hca_ptr->ib_trans.entry);
	if (dapls_thread_signal() == -1)
		dapl_log(DAPL_DBG_TYPE_UTIL,
			 " open_hca: thread wakeup error = %s\n",
			 strerror(errno));
	dapl_os_unlock(&g_hca_lock);

	/* initialize cr_list lock */
	dat_status = dapl_os_lock_init(&hca_ptr->ib_trans.lock);
	if (dat_status != DAT_SUCCESS) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " open_hca: failed to init cr_list lock\n");
		goto bail;
	}

	/* initialize CM list for listens on this HCA */
	dapl_llist_init_head(&hca_ptr->ib_trans.list);

	/* initialize pipe, user level wakeup on select */
	if (create_cr_pipe(hca_ptr)) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " open_hca: failed to init cr pipe - %s\n",
			 strerror(errno));
		goto bail;
	}

	/* create thread to process inbound connect request */
	hca_ptr->ib_trans.cr_state = IB_THREAD_INIT;
	dat_status = dapl_os_thread_create(cr_thread,
					   (void *)hca_ptr,
					   &hca_ptr->ib_trans.thread);
	if (dat_status != DAT_SUCCESS) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " open_hca: failed to create thread\n");
		goto bail;
	}

	/* wait for thread */
	while (hca_ptr->ib_trans.cr_state != IB_THREAD_RUN) {
		dapl_os_sleep_usec(1000);
	}

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
		     " open_hca: devname %s, port %d, hostname_IP %s\n",
		     ibv_get_device_name(hca_ptr->ib_trans.ib_dev),
		     hca_ptr->port_num, inet_ntoa(((struct sockaddr_in *)
						   &hca_ptr->hca_address)->
						  sin_addr));
	dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
		     " open_hca: LID 0x%x GID Subnet 0x" F64x " ID 0x" F64x
		     "\n", ntohs(hca_ptr->ib_trans.lid), (unsigned long long)
		     htonll(hca_ptr->ib_trans.gid.global.subnet_prefix),
		     (unsigned long long)htonll(hca_ptr->ib_trans.gid.global.
						interface_id));

	ibv_free_device_list(dev_list);
	return dat_status;

      bail:
	ibv_close_device(hca_ptr->ib_hca_handle);
	hca_ptr->ib_hca_handle = IB_INVALID_HANDLE;
      err:
	ibv_free_device_list(dev_list);
	return DAT_INTERNAL_ERROR;
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
	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " close_hca: %p\n", hca_ptr);

	if (hca_ptr->ib_hca_handle != IB_INVALID_HANDLE) {
		if (ibv_close_device(hca_ptr->ib_hca_handle))
			return (dapl_convert_errno(errno, "ib_close_device"));
		hca_ptr->ib_hca_handle = IB_INVALID_HANDLE;
	}

	dapl_os_lock(&g_hca_lock);
	if (g_ib_thread_state != IB_THREAD_RUN) {
		dapl_os_unlock(&g_hca_lock);
		return (DAT_SUCCESS);
	}
	dapl_os_unlock(&g_hca_lock);

	/* destroy cr_thread and lock */
	hca_ptr->ib_trans.cr_state = IB_THREAD_CANCEL;
	send(hca_ptr->ib_trans.scm[1], "w", sizeof "w", 0);
	while (hca_ptr->ib_trans.cr_state != IB_THREAD_EXIT) {
		dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
			     " close_hca: waiting for cr_thread\n");
		send(hca_ptr->ib_trans.scm[1], "w", sizeof "w", 0);
		dapl_os_sleep_usec(1000);
	}
	dapl_os_lock_destroy(&hca_ptr->ib_trans.lock);
	destroy_cr_pipe(hca_ptr); /* no longer need pipe */
	
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
		dapl_os_sleep_usec(1000);
	}

	return (DAT_SUCCESS);
}

DAT_RETURN dapli_ib_thread_init(void)
{
	DAT_RETURN dat_status;

	dapl_os_lock(&g_hca_lock);
	if (g_ib_thread_state != IB_THREAD_INIT) {
		dapl_os_unlock(&g_hca_lock);
		return DAT_SUCCESS;
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
	while (g_ib_thread_state != IB_THREAD_EXIT) {
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
	int ret, idx, cnt;

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " ib_thread(%d,0x%x): ENTER: \n",
		     dapl_os_getpid(), g_ib_thread);

	dapl_os_lock(&g_hca_lock);
	for (g_ib_thread_state = IB_THREAD_RUN;
	     g_ib_thread_state == IB_THREAD_RUN; 
	     dapl_os_lock(&g_hca_lock)) {

		CompSetZero(&ufds);
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
		     " ib_thread(%d,0x%x): ENTER: pipe %d \n",
		     dapl_os_getpid(), g_ib_thread, g_ib_pipe[0]);

	/* Poll across pipe, CM, AT never changes */
	dapl_os_lock(&g_hca_lock);
	g_ib_thread_state = IB_THREAD_RUN;

	ufds[0].fd = g_ib_pipe[0];	/* pipe */
	ufds[0].events = POLLIN;

	while (g_ib_thread_state == IB_THREAD_RUN) {

		/* build ufds after pipe and uCMA events */
		ufds[0].revents = 0;
		idx = 0;

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
				     " async=%d pipe=%d \n",
				     dapl_os_getpid(), hca, ufds[idx - 1].fd,
				     ufds[0].fd);

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
			     " async=0x%x pipe=0x%x \n",
			     dapl_os_getpid(), ufds[idx].revents,
			     ufds[0].revents);

		/* check and process CQ and ASYNC events, per device */
		for (idx = 1; idx < fds; idx++) {
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
			for (idx = 1; idx < fds; idx++) {
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
