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
#include "dapl.h"
#include "dapl_adapter_util.h"
#include "dapl_ib_util.h"
#include "dapl_osd.h"

#include <stdlib.h>

int g_dapl_loopback_connection = 0;

enum ibv_mtu dapl_ib_mtu(int mtu)
{
	switch (mtu) {
	case 256:
		return IBV_MTU_256;
	case 512:
		return IBV_MTU_512;
	case 1024:
		return IBV_MTU_1024;
	case 2048:
		return IBV_MTU_2048;
	case 4096:
		return IBV_MTU_4096;
	default:
		return IBV_MTU_1024;
	}
}

char *dapl_ib_mtu_str(enum ibv_mtu mtu)
{
	switch (mtu) {
	case IBV_MTU_256:
		return "256";
	case IBV_MTU_512:
		return "512";
	case IBV_MTU_1024:
		return "1024";
	case IBV_MTU_2048:
		return "2048";
	case IBV_MTU_4096:
		return "4096";
	default:
		return "1024";
	}
}

DAT_RETURN getlocalipaddr(DAT_SOCK_ADDR * addr, int addr_len)
{
	struct sockaddr_in *sin;
	struct addrinfo *res, hint, *ai;
	int ret;
	char hostname[256];

	if (addr_len < sizeof(*sin)) {
		return DAT_INTERNAL_ERROR;
	}

	ret = gethostname(hostname, 256);
	if (ret)
		return dapl_convert_errno(ret, "gethostname");

	memset(&hint, 0, sizeof hint);
	hint.ai_flags = AI_PASSIVE;
	hint.ai_family = AF_INET;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_protocol = IPPROTO_TCP;

	ret = getaddrinfo(hostname, NULL, &hint, &res);
	if (ret) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " getaddrinfo ERR: %d %s\n", ret, gai_strerror(ret));
		return DAT_INVALID_ADDRESS;
	}

	ret = DAT_INVALID_ADDRESS;
	for (ai = res; ai; ai = ai->ai_next) {
		sin = (struct sockaddr_in *)ai->ai_addr;
		if (*((uint32_t *) & sin->sin_addr) != htonl(0x7f000001)) {
			*((struct sockaddr_in *)addr) = *sin;
			ret = DAT_SUCCESS;
			break;
		}
	}

	freeaddrinfo(res);
	return ret;
}

/*
 * dapls_ib_query_hca
 *
 * Query the hca attribute
 *
 * Input:
 *	hca_handl		hca handle	
 *	ia_attr			attribute of the ia
 *	ep_attr			attribute of the ep
 *	ip_addr			ip address of DET NIC
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INVALID_HANDLE
 */

DAT_RETURN dapls_ib_query_hca(IN DAPL_HCA * hca_ptr,
			      OUT DAT_IA_ATTR * ia_attr,
			      OUT DAT_EP_ATTR * ep_attr,
			      OUT DAT_SOCK_ADDR6 * ip_addr)
{
	struct ibv_device_attr dev_attr;
	struct ibv_port_attr port_attr;

	if (hca_ptr->ib_hca_handle == NULL) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR, " query_hca: BAD handle\n");
		return (DAT_INVALID_HANDLE);
	}

	/* local IP address of device, set during ia_open */
	if (ip_addr != NULL)
		memcpy(ip_addr, &hca_ptr->hca_address, sizeof(DAT_SOCK_ADDR6));

	if (ia_attr == NULL && ep_attr == NULL)
		return DAT_SUCCESS;

	/* query verbs for this device and port attributes */
	if (ibv_query_device(hca_ptr->ib_hca_handle, &dev_attr) ||
	    ibv_query_port(hca_ptr->ib_hca_handle,
			   hca_ptr->port_num, &port_attr))
		return (dapl_convert_errno(errno, "ib_query_hca"));

	if (ia_attr != NULL) {
		(void)dapl_os_memzero(ia_attr, sizeof(*ia_attr));
		ia_attr->adapter_name[DAT_NAME_MAX_LENGTH - 1] = '\0';
		ia_attr->vendor_name[DAT_NAME_MAX_LENGTH - 1] = '\0';
		ia_attr->ia_address_ptr =
		    (DAT_IA_ADDRESS_PTR) & hca_ptr->hca_address;

		dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
			     " query_hca: %s %s \n",
			     ibv_get_device_name(hca_ptr->ib_trans.ib_dev),
			     inet_ntoa(((struct sockaddr_in *)
					&hca_ptr->hca_address)->sin_addr));

		ia_attr->hardware_version_major = dev_attr.hw_ver;
		/* ia_attr->hardware_version_minor   = dev_attr.fw_ver; */
		ia_attr->max_eps = dev_attr.max_qp;
		ia_attr->max_dto_per_ep = dev_attr.max_qp_wr;
		ia_attr->max_rdma_read_in = dev_attr.max_qp_rd_atom;
		ia_attr->max_rdma_read_out = dev_attr.max_qp_init_rd_atom;
		ia_attr->max_rdma_read_per_ep_in = dev_attr.max_qp_rd_atom;
		ia_attr->max_rdma_read_per_ep_out =
		    dev_attr.max_qp_init_rd_atom;
		ia_attr->max_rdma_read_per_ep_in_guaranteed = DAT_TRUE;
		ia_attr->max_rdma_read_per_ep_out_guaranteed = DAT_TRUE;
		ia_attr->max_evds = dev_attr.max_cq;
		ia_attr->max_evd_qlen = dev_attr.max_cqe;
		ia_attr->max_iov_segments_per_dto = dev_attr.max_sge;
		ia_attr->max_lmrs = dev_attr.max_mr;
		/* 32bit attribute from 64bit, 4G-1 limit, DAT v2 needs fix */
		ia_attr->max_lmr_block_size = 
		    (dev_attr.max_mr_size >> 32) ? ~0 : dev_attr.max_mr_size;
		ia_attr->max_rmrs = dev_attr.max_mw;
		ia_attr->max_lmr_virtual_address = dev_attr.max_mr_size;
		ia_attr->max_rmr_target_address = dev_attr.max_mr_size;
		ia_attr->max_pzs = dev_attr.max_pd;
		ia_attr->max_message_size = port_attr.max_msg_sz;
		ia_attr->max_rdma_size = port_attr.max_msg_sz;
		/* iWARP spec. - 1 sge for RDMA reads */
		if (hca_ptr->ib_hca_handle->device->transport_type
		    == IBV_TRANSPORT_IWARP)
			ia_attr->max_iov_segments_per_rdma_read = 1;
		else
			ia_attr->max_iov_segments_per_rdma_read =
			    dev_attr.max_sge;
		ia_attr->max_iov_segments_per_rdma_write = dev_attr.max_sge;
		ia_attr->num_transport_attr = 0;
		ia_attr->transport_attr = NULL;
		ia_attr->num_vendor_attr = 0;
		ia_attr->vendor_attr = NULL;
#ifdef DAT_EXTENSIONS
		ia_attr->extension_supported = DAT_EXTENSION_IB;
		ia_attr->extension_version = DAT_IB_EXTENSION_VERSION;
#endif
		/* save key device attributes for CM exchange */
		hca_ptr->ib_trans.rd_atom_in  = dev_attr.max_qp_rd_atom;
		hca_ptr->ib_trans.rd_atom_out = dev_attr.max_qp_init_rd_atom;
		
		hca_ptr->ib_trans.mtu = DAPL_MIN(port_attr.active_mtu,
						 hca_ptr->ib_trans.mtu);
		hca_ptr->ib_trans.ack_timer =
		    DAPL_MAX(dev_attr.local_ca_ack_delay,
			     hca_ptr->ib_trans.ack_timer);

		/* set MTU in transport specific named attribute */
		hca_ptr->ib_trans.named_attr.name = "DAT_IB_TRANSPORT_MTU";
		hca_ptr->ib_trans.named_attr.value =
		    dapl_ib_mtu_str(hca_ptr->ib_trans.mtu);

		dapl_log(DAPL_DBG_TYPE_UTIL,
			     " query_hca: (%x.%x) eps %d, sz %d evds %d,"
			     " sz %d mtu %d\n",
			     ia_attr->hardware_version_major,
			     ia_attr->hardware_version_minor,
			     ia_attr->max_eps, ia_attr->max_dto_per_ep,
			     ia_attr->max_evds, ia_attr->max_evd_qlen,
			     128 << hca_ptr->ib_trans.mtu);

		dapl_log(DAPL_DBG_TYPE_UTIL,
			     " query_hca: msg %llu rdma %llu iov %d lmr %d rmr %d"
			     " ack_time %d mr %u\n",
			     ia_attr->max_message_size, ia_attr->max_rdma_size,
			     ia_attr->max_iov_segments_per_dto,
			     ia_attr->max_lmrs, ia_attr->max_rmrs,
			     hca_ptr->ib_trans.ack_timer,
			     ia_attr->max_lmr_block_size);
	}

	if (ep_attr != NULL) {
		(void)dapl_os_memzero(ep_attr, sizeof(*ep_attr));
		ep_attr->max_message_size = port_attr.max_msg_sz;
		ep_attr->max_rdma_size = port_attr.max_msg_sz;
		ep_attr->max_recv_dtos = dev_attr.max_qp_wr;
		ep_attr->max_request_dtos = dev_attr.max_qp_wr;
		ep_attr->max_recv_iov = dev_attr.max_sge;
		ep_attr->max_request_iov = dev_attr.max_sge;
		ep_attr->max_rdma_read_in = dev_attr.max_qp_rd_atom;
		ep_attr->max_rdma_read_out = dev_attr.max_qp_init_rd_atom;
		ep_attr->max_rdma_read_iov = dev_attr.max_sge;
		ep_attr->max_rdma_write_iov = dev_attr.max_sge;
		dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
			     " query_hca: MAX msg %llu mtu %d qsz %d iov %d"
			     " rdma i%d,o%d\n",
			     ep_attr->max_message_size,
			     128 << hca_ptr->ib_trans.mtu,
			     ep_attr->max_recv_dtos, 
			     ep_attr->max_recv_iov,
			     ep_attr->max_rdma_read_in,
			     ep_attr->max_rdma_read_out);
	}
	return DAT_SUCCESS;
}

/*
 * dapls_ib_setup_async_callback
 *
 * Set up an asynchronous callbacks of various kinds
 *
 * Input:
 *	ia_handle		IA handle
 *	handler_type		type of handler to set up
 *	callback_handle 	handle param for completion callbacks
 *	callback		callback routine pointer
 *	context 		argument for callback routine
 *
 * Output:
 *	none
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INVALID_PARAMETER
 *
 */
DAT_RETURN dapls_ib_setup_async_callback(IN DAPL_IA * ia_ptr,
					 IN DAPL_ASYNC_HANDLER_TYPE
					 handler_type, IN DAPL_EVD * evd_ptr,
					 IN ib_async_handler_t callback,
					 IN void *context)
{
	ib_hca_transport_t *hca_ptr;

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
		     " setup_async_cb: ia %p type %d handle %p cb %p ctx %p\n",
		     ia_ptr, handler_type, evd_ptr, callback, context);

	hca_ptr = &ia_ptr->hca_ptr->ib_trans;
	switch (handler_type) {
	case DAPL_ASYNC_UNAFILIATED:
		hca_ptr->async_unafiliated = (ib_async_handler_t) callback;
		hca_ptr->async_un_ctx = context;
		break;
	case DAPL_ASYNC_CQ_ERROR:
		hca_ptr->async_cq_error = (ib_async_cq_handler_t) callback;
		break;
	case DAPL_ASYNC_CQ_COMPLETION:
		hca_ptr->async_cq = (ib_async_dto_handler_t) callback;
		break;
	case DAPL_ASYNC_QP_ERROR:
		hca_ptr->async_qp_error = (ib_async_qp_handler_t) callback;
		break;
	default:
		break;
	}
	return DAT_SUCCESS;
}

void dapli_async_event_cb(struct _ib_hca_transport *hca)
{
	struct ibv_async_event event;

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " async_event(%p)\n", hca);

	if (hca->destroy)
		return;

	if (!ibv_get_async_event(hca->ib_ctx, &event)) {

		switch (event.event_type) {
		case IBV_EVENT_CQ_ERR:
		{
			struct dapl_ep *evd_ptr =
				event.element.cq->cq_context;

			dapl_log(DAPL_DBG_TYPE_ERR,
				 "dapl async_event CQ (%p) ERR %d\n",
				 evd_ptr, event.event_type);

			/* report up if async callback still setup */
			if (hca->async_cq_error)
				hca->async_cq_error(hca->ib_ctx,
						    event.element.cq,
						    &event,
						    (void *)evd_ptr);
			break;
		}
		case IBV_EVENT_COMM_EST:
		{
			/* Received msgs on connected QP before RTU */
			dapl_log(DAPL_DBG_TYPE_UTIL,
				 " async_event COMM_EST(%p) rdata beat RTU\n",
				 event.element.qp);

			break;
		}
		case IBV_EVENT_QP_FATAL:
		case IBV_EVENT_QP_REQ_ERR:
		case IBV_EVENT_QP_ACCESS_ERR:
		case IBV_EVENT_QP_LAST_WQE_REACHED:
		case IBV_EVENT_SRQ_ERR:
		case IBV_EVENT_SRQ_LIMIT_REACHED:
		case IBV_EVENT_SQ_DRAINED:
		{
			struct dapl_ep *ep_ptr =
				event.element.qp->qp_context;

			dapl_log(DAPL_DBG_TYPE_ERR,
				 "dapl async_event QP (%p) ERR %d\n",
				 ep_ptr, event.event_type);

			/* report up if async callback still setup */
			if (hca->async_qp_error)
				hca->async_qp_error(hca->ib_ctx,
						    ep_ptr->qp_handle,
						    &event,
						    (void *)ep_ptr);
			break;
		}
		case IBV_EVENT_PATH_MIG:
		case IBV_EVENT_PATH_MIG_ERR:
		case IBV_EVENT_DEVICE_FATAL:
		case IBV_EVENT_PORT_ACTIVE:
		case IBV_EVENT_PORT_ERR:
		case IBV_EVENT_LID_CHANGE:
		case IBV_EVENT_PKEY_CHANGE:
		case IBV_EVENT_SM_CHANGE:
		{
			dapl_log(DAPL_DBG_TYPE_WARN,
				 "dapl async_event: DEV ERR %d\n",
				 event.event_type);

			/* report up if async callback still setup */
			if (hca->async_unafiliated)
				hca->async_unafiliated(hca->ib_ctx, 
						       &event,	
						       hca->async_un_ctx);
			break;
		}
		case IBV_EVENT_CLIENT_REREGISTER:
			/* no need to report this event this time */
			dapl_log(DAPL_DBG_TYPE_UTIL,
				 " async_event: IBV_CLIENT_REREGISTER\n");
			break;

		default:
			dapl_log(DAPL_DBG_TYPE_WARN,
				 "dapl async_event: %d UNKNOWN\n",
				 event.event_type);
			break;

		}
		ibv_ack_async_event(&event);
	}
}

/*
 * dapls_set_provider_specific_attr
 *
 * Input:
 *      attr_ptr        Pointer provider specific attributes
 *
 * Output:
 *      none
 *
 * Returns:
 *      void
 */
DAT_NAMED_ATTR ib_attrs[] = {
	{
	 "DAT_IB_TRANSPORT_MTU", "2048"}
	,
#ifdef DAT_EXTENSIONS
	{
	 "DAT_EXTENSION_INTERFACE", "TRUE"}
	,
	{
	 DAT_IB_ATTR_FETCH_AND_ADD, "TRUE"}
	,
	{
	 DAT_IB_ATTR_CMP_AND_SWAP, "TRUE"}
	,
	{
	 DAT_IB_ATTR_IMMED_DATA, "TRUE"}
	,
#ifndef _OPENIB_CMA_
	{
	 DAT_IB_ATTR_UD, "TRUE"}
	,
#endif
#ifdef DAPL_COUNTERS
	{
	 DAT_ATTR_COUNTERS, "TRUE"}
	,
#endif				/* DAPL_COUNTERS */
#endif
};

#define SPEC_ATTR_SIZE( x )     (sizeof( x ) / sizeof( DAT_NAMED_ATTR))

void dapls_query_provider_specific_attr(IN DAPL_IA * ia_ptr,
					IN DAT_PROVIDER_ATTR * attr_ptr)
{
	attr_ptr->num_provider_specific_attr = SPEC_ATTR_SIZE(ib_attrs);
	attr_ptr->provider_specific_attr = ib_attrs;

	/* set MTU to actual settings */
	ib_attrs[0].value = ia_ptr->hca_ptr->ib_trans.named_attr.value;
}
