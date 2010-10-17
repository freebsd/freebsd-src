/*
 * Copyright (c) 2002-2006, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under all of the following licenses:
 * 
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see 
 *    http://www.opensource.org/licenses/cpl.php.
 * 
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 * 
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a
 *    copy of which is in the file LICENSE3.txt in the root directory. The
 *    license is also available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 * 
 * Licensee has the right to choose one of the above licenses.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain both the above copyright
 * notice and one of the license notices.
 * 
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of Network Appliance, Inc. nor the names of other DAT
 * Collaborative contributors may be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 */

/****************************************************************
 *
 * HEADER: dat_redirection.h
 *
 * PURPOSE: Defines the common redirection macros
 *
 * Description: Macros to invoke DAPL functions from the dat_registry
 *
 * Mapping rules:
 *      All global symbols are prepended with DAT_ or dat_
 *      All DAT objects have an 'api' tag which, such as 'ep' or 'lmr'
 *      The method table is in the provider definition structure.
 *
 **********************************************************/
#ifndef _DAT_REDIRECTION_H_
#define _DAT_REDIRECTION_H_

typedef struct dat_provider DAT_PROVIDER;

#ifndef DAT_HANDLE_TO_PROVIDER

/* A utility macro to fetch the Provider Library for any object
 *
 * An alternate version could be defined for single library systems.
 * it would look something like:
 *      extern const struct dat_ia my_single_ia_provider;
 *      #define DAT_HANDLE_TO_PROVIDER(ignore) &my_single_ia_provider
 *
 * This would allow a good compiler to avoid indirection overhead when
 * making function calls.
 */

#define DAT_HANDLE_TO_PROVIDER(handle) (*(DAT_PROVIDER **)(handle))
#endif

#define DAT_IA_QUERY(ia, evd, ia_msk, ia_ptr, p_msk, p_ptr) \
	(*DAT_HANDLE_TO_PROVIDER (ia)->ia_query_func) (\
                (ia),               \
		(evd),              \
		(ia_msk),           \
		(ia_ptr),           \
		(p_msk),            \
		(p_ptr))

#define DAT_SET_CONSUMER_CONTEXT(handle, context) \
	(*DAT_HANDLE_TO_PROVIDER (handle)->set_consumer_context_func) (\
		(handle),           \
		(context))

#define DAT_GET_CONSUMER_CONTEXT(handle, context) \
	(*DAT_HANDLE_TO_PROVIDER (handle)->get_consumer_context_func) (\
		(handle),           \
		(context))

#define DAT_GET_HANDLE_TYPE(handle, handle_type) \
	(*DAT_HANDLE_TO_PROVIDER (handle)->get_handle_type_func) (\
		(handle),           \
		(handle_type))

#define DAT_CR_QUERY(cr, mask, param) \
	(*DAT_HANDLE_TO_PROVIDER (cr)->cr_query_func) (\
		(cr),               \
		(mask),             \
		(param))

#define DAT_CR_ACCEPT(cr, ep, size, pdata) \
	(*DAT_HANDLE_TO_PROVIDER (cr)->cr_accept_func) (\
		(cr),               \
		(ep),               \
		(size),             \
		(pdata))

#define DAT_CR_REJECT(cr, size, pdata) \
	(*DAT_HANDLE_TO_PROVIDER (cr)->cr_reject_func) (\
		(cr),		\
		(size),		\
		(pdata))

#define DAT_CR_HANDOFF(cr,qual) \
	(*DAT_HANDLE_TO_PROVIDER(cr)->cr_handoff_func)(\
		(cr),               \
		(qual))

#define DAT_EVD_QUERY(evd, mask, param) \
	(*DAT_HANDLE_TO_PROVIDER (evd)->evd_query_func) (\
		(evd),              \
		(mask),             \
		(param))

#define DAT_EVD_RESIZE(evd, qsize) \
	(*DAT_HANDLE_TO_PROVIDER (evd)->evd_resize_func) (\
		(evd),              \
		(qsize))

#define DAT_EVD_POST_SE(evd, event) \
	(*DAT_HANDLE_TO_PROVIDER (evd)->evd_post_se_func) (\
		(evd),              \
		(event))

#define DAT_EVD_DEQUEUE(evd, event) \
	(*DAT_HANDLE_TO_PROVIDER (evd)->evd_dequeue_func) (\
		(evd),              \
		(event))

#define DAT_EVD_FREE(evd)\
	(*DAT_HANDLE_TO_PROVIDER (evd)->evd_free_func) (\
		(evd))

#define DAT_EP_CREATE(ia, pz, in_evd, out_evd, connect_evd, attr, ep) \
	(*DAT_HANDLE_TO_PROVIDER (ia)->ep_create_func) (\
		(ia),               \
		(pz),               \
		(in_evd),           \
		(out_evd),          \
		(connect_evd),      \
		(attr),             \
		(ep))

#define DAT_EP_CREATE_WITH_SRQ(ia, pz, in_evd, out_evd, \
	connect_evd, srq, attr, ep) \
		(*DAT_HANDLE_TO_PROVIDER (ia)->ep_create_with_srq_func) (\
		(ia),                \
		(pz),                \
		(in_evd),            \
		(out_evd),           \
		(connect_evd),       \
		(srq),               \
		(attr),              \
		(ep))

#define DAT_EP_QUERY(ep, mask, param) \
	(*DAT_HANDLE_TO_PROVIDER (ep)->ep_query_func) (\
		(ep),                \
		(mask),              \
		(param))

#define DAT_EP_MODIFY(ep, mask, param) \
	(*DAT_HANDLE_TO_PROVIDER (ep)->ep_modify_func) (\
		(ep),                \
		(mask),              \
		(param))

#define DAT_EP_CONNECT(ep, ia_addr, conn_qual, \
	timeout, psize, pdata, qos, flags) \
	(*DAT_HANDLE_TO_PROVIDER (ep)->ep_connect_func) (\
		(ep),                \
		(ia_addr),           \
		(conn_qual),         \
		(timeout),           \
		(psize),             \
		(pdata),             \
		(qos),               \
		(flags))

#define DAT_EP_COMMON_CONNECT(ep, addr, timeout, psize, pdata) \
        (*DAT_HANDLE_TO_PROVIDER(ep)->ep_common_connect_func)(\
		(ep),		\
		(addr),		\
		(timeout),	\
		(psize),	\
		(pdata))
		
#define DAT_EP_DUP_CONNECT(ep, dup, timeout, psize, pdata, qos) \
	(*DAT_HANDLE_TO_PROVIDER (ep)->ep_dup_connect_func) (\
		(ep),                \
		(dup),               \
		(timeout),           \
		(psize),             \
		(pdata),             \
		(qos))

#define DAT_EP_DISCONNECT(ep, flags) \
	(*DAT_HANDLE_TO_PROVIDER(ep)->ep_disconnect_func) (\
		(ep),                \
		(flags))

#define DAT_EP_POST_SEND(ep, size, lbuf, cookie, flags) \
	(*DAT_HANDLE_TO_PROVIDER (ep)->ep_post_send_func) (\
		(ep),                \
		(size),              \
		(lbuf),              \
		(cookie),            \
		(flags))

#define DAT_EP_POST_SEND_WITH_INVALIDATE( \
		ep,size,lbuf,cookie,flags,inv_flag,rmr_context) \
	(*DAT_HANDLE_TO_PROVIDER(ep)-> \
	ep_post_send_with_invalidate_func)(\
		(ep),		\
		(size),		\
		(lbuf),		\
		(cookie),	\
		(flags), 	\
		(inv_flag), 	\
		(rmr_context))

#define DAT_EP_POST_RECV(ep, size, lbuf, cookie, flags) \
	(*DAT_HANDLE_TO_PROVIDER (ep)->ep_post_recv_func) (\
		(ep),                \
		(size),              \
		(lbuf),              \
		(cookie),            \
		(flags))

#define DAT_EP_POST_RDMA_READ(ep, size, lbuf, cookie, rbuf, flags) \
	(*DAT_HANDLE_TO_PROVIDER (ep)->ep_post_rdma_read_func) (\
		(ep),                \
		(size),              \
		(lbuf),              \
		(cookie),            \
		(rbuf),              \
		(flags))

#define DAT_EP_POST_RDMA_READ_TO_RMR(ep, lbuf, cookie, rbuf, flags) \
	        (*DAT_HANDLE_TO_PROVIDER(ep)->ep_post_rdma_read_to_rmr_func)(\
		(ep),		\
		(lbuf),		\
		(cookie),	\
		(rbuf),		\
		(flags))

#define DAT_EP_POST_RDMA_WRITE(ep, size, lbuf, cookie, rbuf, flags) \
	(*DAT_HANDLE_TO_PROVIDER (ep)->ep_post_rdma_write_func) (\
		(ep),                \
		(size),              \
		(lbuf),              \
		(cookie),            \
		(rbuf),              \
		(flags))

#define DAT_EP_GET_STATUS(ep, ep_state, recv_idle, request_idle) \
	(*DAT_HANDLE_TO_PROVIDER (ep)->ep_get_status_func) (\
		(ep),                \
		(ep_state),          \
		(recv_idle),         \
		(request_idle))

#define DAT_EP_FREE(ep)\
	(*DAT_HANDLE_TO_PROVIDER (ep)->ep_free_func) (\
		(ep))

#define DAT_EP_RESET(ep)\
	(*DAT_HANDLE_TO_PROVIDER (ep)->ep_reset_func) (\
		(ep))

#define DAT_EP_RECV_QUERY(ep, nbuf_alloc, buf_span)\
	(*DAT_HANDLE_TO_PROVIDER (ep)->ep_recv_query_func) (\
		(ep),                \
		(nbuf_alloc),        \
		(buf_span))

#define DAT_EP_SET_WATERMARK(ep, soft_wm, hard_wm)\
	(*DAT_HANDLE_TO_PROVIDER (ep)->ep_set_watermark_func) (\
		(ep),                \
		(soft_wm),           \
		(hard_wm))

#define DAT_LMR_QUERY(lmr, mask, param)\
	(*DAT_HANDLE_TO_PROVIDER (lmr)->lmr_query_func) (\
		(lmr),               \
		(mask),              \
		(param))

#define DAT_LMR_FREE(lmr)\
	(*DAT_HANDLE_TO_PROVIDER (lmr)->lmr_free_func) (\
		(lmr))

#define DAT_LMR_SYNC_RDMA_READ(ia, lbuf, size)\
	(*DAT_HANDLE_TO_PROVIDER (ia)->lmr_sync_rdma_read_func) (\
		(ia),                \
		(lbuf),              \
		(size))

#define DAT_LMR_SYNC_RDMA_WRITE(ia, lbuf, size)\
	(*DAT_HANDLE_TO_PROVIDER (ia)->lmr_sync_rdma_write_func) (\
		(ia),                \
		(lbuf),              \
		(size))

#define DAT_RMR_CREATE(pz, rmr) \
	(*DAT_HANDLE_TO_PROVIDER (pz)->rmr_create_func) (\
		(pz),                \
		(rmr))

#define DAT_RMR_CREATE_FOR_EP(pz, rmr) \
	(*DAT_HANDLE_TO_PROVIDER (pz)->rmr_create_for_ep_func) (\
		(pz),                \
		(rmr))

#define DAT_RMR_QUERY(rmr, mask, param) \
	(*DAT_HANDLE_TO_PROVIDER (rmr)->rmr_query_func) (\
		(rmr),               \
		(mask),              \
		(param))

#define DAT_RMR_BIND(rmr, lmr, lmr_triplet, mem_priv, \
		va_type, ep, cookie, flags, context) \
	(*DAT_HANDLE_TO_PROVIDER (rmr)->rmr_bind_func) (\
		(rmr),               \
		(lmr),               \
		(lmr_triplet),		\
		(mem_priv),          \
		(va_type), 		\
		(ep),                \
		(cookie),            \
		(flags),             \
		(context))

#define DAT_RMR_FREE(rmr)\
	(*DAT_HANDLE_TO_PROVIDER (rmr)->rmr_free_func) (\
		(rmr))

#define DAT_PSP_CREATE(ia, conn_qual, evd, flags, handle) \
	(*DAT_HANDLE_TO_PROVIDER (ia)->psp_create_func) (\
		(ia),                \
		(conn_qual),         \
		(evd),               \
		(flags),             \
		(handle))

#define DAT_PSP_CREATE_ANY(ia, conn_qual, evd, flags, handle) \
	(*DAT_HANDLE_TO_PROVIDER (ia)->psp_create_any_func) (\
		(ia),                \
		(conn_qual),         \
		(evd),               \
		(flags),             \
		(handle))

#define DAT_PSP_QUERY(psp, mask, param) \
	(*DAT_HANDLE_TO_PROVIDER (psp)->psp_query_func) (\
		(psp),               \
		(mask),              \
		(param))

#define DAT_PSP_FREE(psp)\
	(*DAT_HANDLE_TO_PROVIDER (psp)->psp_free_func) (\
		(psp))

#define DAT_RSP_CREATE(ia, conn_qual, ep, evd, handle) \
	(*DAT_HANDLE_TO_PROVIDER (ia)->rsp_create_func) (\
		(ia),                \
		(conn_qual),         \
		(ep),                \
		(evd),               \
		(handle))

#define DAT_RSP_QUERY(rsp, mask, param) \
	(*DAT_HANDLE_TO_PROVIDER (rsp)->rsp_query_func) (\
		(rsp),               \
		(mask),              \
		(param))

#define DAT_RSP_FREE(rsp)\
	(*DAT_HANDLE_TO_PROVIDER (rsp)->rsp_free_func) (\
		(rsp))

#define DAT_CSP_CREATE(ia, comm, addr, evd, handle) \
	        (*DAT_HANDLE_TO_PROVIDER(ia)->csp_create_func)(\
	(ia),\
	(comm),\
	(addr),\
	(evd),\
	(handle))

#define DAT_CSP_QUERY(csp, mask, param) \
	        (*DAT_HANDLE_TO_PROVIDER(csp)->csp_query_func)(\
	(csp),\
	(mask),\
	(param))

#define DAT_CSP_FREE(csp)\
	        (*DAT_HANDLE_TO_PROVIDER(csp)->csp_free_func)(\
	(csp))

#define DAT_PZ_CREATE(ia, pz) \
	(*DAT_HANDLE_TO_PROVIDER (ia)->pz_create_func) (\
		(ia),                \
		(pz))

#define DAT_PZ_QUERY(pz, mask, param) \
	(*DAT_HANDLE_TO_PROVIDER (pz)->pz_query_func) (\
		(pz),                \
		(mask),              \
		(param))

#define DAT_PZ_FREE(pz) \
	(*DAT_HANDLE_TO_PROVIDER (pz)->pz_free_func) (\
		(pz))

#define DAT_SRQ_CREATE(ia, pz, attr, srq) \
	(*DAT_HANDLE_TO_PROVIDER (ia)->srq_create_func) (\
		(ia),                \
		(pz),                \
		(attr),              \
		(srq))

#define DAT_SRQ_SET_LW(srq, lw) \
	(*DAT_HANDLE_TO_PROVIDER (srq)->srq_set_lw_func) (\
		(srq),               \
		(lw))

#define DAT_SRQ_FREE(srq) \
	(*DAT_HANDLE_TO_PROVIDER (srq)->srq_free_func) (\
		(srq))

#define DAT_SRQ_QUERY(srq, mask, param) \
	(*DAT_HANDLE_TO_PROVIDER (srq)->srq_query_func) (\
		(srq),               \
		(mask),              \
		(param))

#define DAT_SRQ_RESIZE(srq, qsize) \
	(*DAT_HANDLE_TO_PROVIDER (srq)->srq_resize_func) (\
		(srq),               \
		(qsize))

#define DAT_SRQ_POST_RECV(srq, size, lbuf, cookie) \
	(*DAT_HANDLE_TO_PROVIDER (srq)->srq_post_recv_func) (\
		(srq),                \
		(size),              \
		(lbuf),              \
		(cookie))

#define DAT_IA_HA_RELATED (ia, name, answer) \
	                (*DAT_HANDLE_TO_PROVIDER(ia)->ia_ha_related) (\
	(ia), \
	(name), \
	(answer))

#ifdef DAT_EXTENSIONS
#define DAT_HANDLE_EXTENDEDOP(handle, op, args) \
	(*DAT_HANDLE_TO_PROVIDER (handle)->handle_extendedop_func) (\
		(handle), \
		(op), \
		(args))
#endif

/***************************************************************
 *
 * FUNCTION PROTOTYPES
 ****************************************************************/

typedef DAT_RETURN (DAT_API *DAT_IA_OPEN_FUNC) (
	IN      const DAT_NAME_PTR,	/* provider             */
	IN      DAT_COUNT,		/* asynch_evd_min_qlen  */
	INOUT   DAT_EVD_HANDLE *,	/* asynch_evd_handle    */
	OUT     DAT_IA_HANDLE *);	/* ia_handle            */

typedef DAT_RETURN (DAT_API *DAT_IA_OPENV_FUNC) (
	IN      const DAT_NAME_PTR,	/* provider             */
	IN      DAT_COUNT,		/* asynch_evd_min_qlen  */
	INOUT   DAT_EVD_HANDLE *,	/* asynch_evd_handle    */
	OUT     DAT_IA_HANDLE *,	/* ia_handle            */
	IN      DAT_UINT32,		/* dat_major_version number */
	IN      DAT_UINT32,		/* dat_minor_version number */
	IN      DAT_BOOLEAN);		/* dat_thread_safety    */

typedef DAT_RETURN (DAT_API *DAT_IA_CLOSE_FUNC) (
	IN      DAT_IA_HANDLE,		/* ia_handle            */
	IN      DAT_CLOSE_FLAGS );	/* close_flags          */

typedef DAT_RETURN (DAT_API *DAT_IA_QUERY_FUNC) (
	IN      DAT_IA_HANDLE,		/* ia_handle            */
	OUT     DAT_EVD_HANDLE *,	/* async_evd_handle     */
	IN      DAT_IA_ATTR_MASK,	/* ia_attr_mask         */
	OUT     DAT_IA_ATTR *,		/* ia_attr              */
	IN      DAT_PROVIDER_ATTR_MASK, /* provider_attr_mask   */
	OUT     DAT_PROVIDER_ATTR * );	/* provider_attr        */

/* helper functions */

typedef DAT_RETURN (DAT_API *DAT_SET_CONSUMER_CONTEXT_FUNC) (
	IN      DAT_HANDLE,		/* dat_handle           */
	IN      DAT_CONTEXT);		/* context              */

typedef DAT_RETURN (DAT_API *DAT_GET_CONSUMER_CONTEXT_FUNC) (
	IN      DAT_HANDLE,		/* dat_handle           */
	OUT     DAT_CONTEXT * );	/* context              */

typedef DAT_RETURN (DAT_API *DAT_GET_HANDLE_TYPE_FUNC) (
	IN      DAT_HANDLE,		/* dat_handle           */
	OUT     DAT_HANDLE_TYPE * );	/* dat_handle_type       */

/* CR functions */

typedef DAT_RETURN (DAT_API *DAT_CR_QUERY_FUNC) (
	IN      DAT_CR_HANDLE,		/* cr_handle            */
	IN      DAT_CR_PARAM_MASK,	/* cr_param_mask        */
	OUT     DAT_CR_PARAM * );	/* cr_param             */

typedef DAT_RETURN (DAT_API *DAT_CR_ACCEPT_FUNC) (
	IN      DAT_CR_HANDLE,		/* cr_handle            */
	IN      DAT_EP_HANDLE,		/* ep_handle            */
	IN      DAT_COUNT,		/* private_data_size    */
	IN      const DAT_PVOID );	/* private_data         */

typedef DAT_RETURN (DAT_API *DAT_CR_REJECT_FUNC) (
	IN      DAT_CR_HANDLE, 		/* cr_handle            */
	IN	DAT_COUNT,		/* private_data_size	*/
	IN	const DAT_PVOID );      /* private_data         */

/* For DAT-1.1 this function is defined for both uDAPL and kDAPL.
 * For DAT-1.0 it was only defined for uDAPL.
 */

typedef DAT_RETURN (DAT_API *DAT_CR_HANDOFF_FUNC) (
    IN          DAT_CR_HANDLE,		/* cr_handle            */
    IN          DAT_CONN_QUAL);		/* handoff              */

/* EVD functions */

typedef DAT_RETURN (DAT_API *DAT_EVD_RESIZE_FUNC) (
	IN      DAT_EVD_HANDLE,	       /* evd_handle           */
	IN      DAT_COUNT );	       /* evd_min_qlen         */

typedef DAT_RETURN (DAT_API *DAT_EVD_POST_SE_FUNC) (
	IN      DAT_EVD_HANDLE,	       /* evd_handle           */
	IN      const DAT_EVENT * );   /* event                */

typedef DAT_RETURN (DAT_API *DAT_EVD_DEQUEUE_FUNC) (
	IN      DAT_EVD_HANDLE,		/* evd_handle           */
	OUT     DAT_EVENT * );		/* event                */

typedef DAT_RETURN (DAT_API *DAT_EVD_FREE_FUNC) (
	IN      DAT_EVD_HANDLE );	/* evd_handle           */

typedef DAT_RETURN (DAT_API *DAT_EVD_QUERY_FUNC) (
	IN      DAT_EVD_HANDLE,		/* evd_handle           */
	IN      DAT_EVD_PARAM_MASK,	/* evd_param_mask       */
	OUT     DAT_EVD_PARAM * );	/* evd_param            */

/* EP functions */

typedef DAT_RETURN (DAT_API *DAT_EP_CREATE_FUNC) (
	IN      DAT_IA_HANDLE,		/* ia_handle            */
	IN      DAT_PZ_HANDLE,		/* pz_handle            */
	IN      DAT_EVD_HANDLE,		/* recv_completion_evd_handle */
	IN      DAT_EVD_HANDLE,		/* request_completion_evd_handle */
	IN      DAT_EVD_HANDLE,		/* connect_evd_handle   */
	IN      const DAT_EP_ATTR *,	/* ep_attributes        */
	OUT     DAT_EP_HANDLE * );	/* ep_handle            */

typedef DAT_RETURN (DAT_API *DAT_EP_CREATE_WITH_SRQ_FUNC) (
	IN      DAT_IA_HANDLE,		/* ia_handle            */
	IN      DAT_PZ_HANDLE,		/* pz_handle            */
	IN      DAT_EVD_HANDLE,		/* recv_completion_evd_handle */
	IN      DAT_EVD_HANDLE,		/* request_completion_evd_handle */
	IN      DAT_EVD_HANDLE,		/* connect_evd_handle   */
	IN      DAT_SRQ_HANDLE,		/* srq_handle           */
	IN      const DAT_EP_ATTR *,	/* ep_attributes        */
	OUT     DAT_EP_HANDLE * );	/* ep_handle            */

typedef DAT_RETURN (DAT_API *DAT_EP_QUERY_FUNC) (
	IN      DAT_EP_HANDLE,		/* ep_handle            */
	IN      DAT_EP_PARAM_MASK,	/* ep_param_mask        */
	OUT     DAT_EP_PARAM * );	/* ep_param             */

typedef DAT_RETURN (DAT_API *DAT_EP_MODIFY_FUNC) (
	IN      DAT_EP_HANDLE,		/* ep_handle            */
	IN      DAT_EP_PARAM_MASK,	/* ep_param_mask        */
	IN      const DAT_EP_PARAM * ); /* ep_param             */

typedef DAT_RETURN (DAT_API *DAT_EP_CONNECT_FUNC) (
	IN      DAT_EP_HANDLE,		/* ep_handle            */
	IN      DAT_IA_ADDRESS_PTR,	/* remote_ia_address    */
	IN      DAT_CONN_QUAL,		/* remote_conn_qual     */
	IN      DAT_TIMEOUT,		/* timeout              */
	IN      DAT_COUNT,		/* private_data_size    */
	IN      const DAT_PVOID,	/* private_data         */
	IN      DAT_QOS,		/* quality_of_service   */
	IN      DAT_CONNECT_FLAGS );	/* connect_flags        */

typedef DAT_RETURN (DAT_API *DAT_EP_COMMON_CONNECT_FUNC) (
	IN      DAT_EP_HANDLE,		/* ep_handle            */
	IN      DAT_IA_ADDRESS_PTR,	/* remote_ia_address    */
	IN      DAT_TIMEOUT,		/* timeout              */
	IN      DAT_COUNT,		/* private_data_size    */
	IN      const DAT_PVOID	);	/* private_data         */

typedef DAT_RETURN (DAT_API *DAT_EP_DUP_CONNECT_FUNC) (
	IN      DAT_EP_HANDLE,		/* ep_handle            */
	IN      DAT_EP_HANDLE,		/* ep_dup_handle        */
	IN      DAT_TIMEOUT,		/* timeout              */
	IN      DAT_COUNT,		/* private_data_size    */
	IN      const DAT_PVOID,	/* private_data         */
	IN      DAT_QOS);		/* quality_of_service   */

typedef DAT_RETURN (DAT_API *DAT_EP_DISCONNECT_FUNC) (
	IN      DAT_EP_HANDLE,		/* ep_handle            */
	IN      DAT_CLOSE_FLAGS );	/* close_flags          */

typedef DAT_RETURN (DAT_API *DAT_EP_POST_SEND_FUNC) (
	IN      DAT_EP_HANDLE,		/* ep_handle            */
	IN      DAT_COUNT,		/* num_segments         */
	IN      DAT_LMR_TRIPLET *,	/* local_iov            */
	IN      DAT_DTO_COOKIE,		/* user_cookie          */
	IN      DAT_COMPLETION_FLAGS ); /* completion_flags     */

typedef DAT_RETURN (DAT_API *DAT_EP_POST_SEND_WITH_INVALIDATE_FUNC) (
	IN      DAT_EP_HANDLE,          /* ep_handle            */
	IN      DAT_COUNT,              /* num_segments         */
	IN      DAT_LMR_TRIPLET *,      /* local_iov            */
	IN      DAT_DTO_COOKIE,         /* user_cookie          */
	IN      DAT_COMPLETION_FLAGS, /* completion_flags     */
	IN      DAT_BOOLEAN, /* invalidate_flag */
	IN      DAT_RMR_CONTEXT ); /* RMR context */

typedef DAT_RETURN (DAT_API *DAT_EP_POST_RECV_FUNC) (
	IN      DAT_EP_HANDLE,		/* ep_handle            */
	IN      DAT_COUNT,		/* num_segments         */
	IN      DAT_LMR_TRIPLET *,	/* local_iov            */
	IN      DAT_DTO_COOKIE,		/* user_cookie          */
	IN      DAT_COMPLETION_FLAGS ); /* completion_flags     */

typedef DAT_RETURN (DAT_API *DAT_EP_POST_RDMA_READ_FUNC) (
	IN      DAT_EP_HANDLE,		/* ep_handle            */
	IN      DAT_COUNT,		/* num_segments         */
	IN      DAT_LMR_TRIPLET *,	/* local_iov            */
	IN      DAT_DTO_COOKIE,		/* user_cookie          */
	IN      const DAT_RMR_TRIPLET *,/* remote_iov           */
	IN      DAT_COMPLETION_FLAGS ); /* completion_flags     */

typedef DAT_RETURN (DAT_API *DAT_EP_POST_RDMA_READ_TO_RMR_FUNC) (
	IN      DAT_EP_HANDLE,          /* ep_handle            */
	IN      const DAT_RMR_TRIPLET *,        /* local_iov            */
	IN      DAT_DTO_COOKIE,         /* user_cookie          */
	IN      const DAT_RMR_TRIPLET *,/* remote_iov           */
	IN      DAT_COMPLETION_FLAGS ); /* completion_flags     */

typedef DAT_RETURN (DAT_API *DAT_EP_POST_RDMA_WRITE_FUNC) (
	IN      DAT_EP_HANDLE,		/* ep_handle            */
	IN      DAT_COUNT,		/* num_segments         */
	IN      DAT_LMR_TRIPLET *,	/* local_iov            */
	IN      DAT_DTO_COOKIE,		/* user_cookie          */
	IN      const DAT_RMR_TRIPLET *,/* remote_iov           */
	IN      DAT_COMPLETION_FLAGS ); /* completion_flags     */

typedef DAT_RETURN (DAT_API *DAT_EP_GET_STATUS_FUNC) (
	IN      DAT_EP_HANDLE,		/* ep_handle            */
	OUT     DAT_EP_STATE *,		/* ep_state             */
	OUT     DAT_BOOLEAN *,		/* recv_idle            */
	OUT     DAT_BOOLEAN * );	/* request_idle         */

typedef DAT_RETURN (DAT_API *DAT_EP_FREE_FUNC) (
	IN      DAT_EP_HANDLE);		/* ep_handle            */

typedef DAT_RETURN (DAT_API *DAT_EP_RESET_FUNC) (
	IN      DAT_EP_HANDLE);		/* ep_handle            */

typedef DAT_RETURN (DAT_API *DAT_EP_RECV_QUERY_FUNC) (
	IN      DAT_EP_HANDLE,		/* ep_handle            */
	OUT     DAT_COUNT *,		/* nbufs_allocated      */
	OUT     DAT_COUNT *);		/* bufs_alloc_span      */

typedef DAT_RETURN (DAT_API *DAT_EP_SET_WATERMARK_FUNC) (
	IN      DAT_EP_HANDLE,		/* ep_handle            */
	IN      DAT_COUNT,		/* ep_soft_high_watermark*/
	IN      DAT_COUNT );		/* ep_hard_high_watermark*/

/* LMR functions */

typedef DAT_RETURN (DAT_API *DAT_LMR_FREE_FUNC) (
	IN      DAT_LMR_HANDLE );	/* lmr_handle           */

typedef DAT_RETURN (DAT_API *DAT_LMR_SYNC_RDMA_READ_FUNC) (
	IN      DAT_IA_HANDLE,		/* ia_handle            */
	IN      const DAT_LMR_TRIPLET *,/* local segments       */
	IN      DAT_VLEN );		/* num_segments         */

typedef DAT_RETURN (DAT_API *DAT_LMR_SYNC_RDMA_WRITE_FUNC) (
	IN      DAT_IA_HANDLE,		/* ia_handle            */
	IN      const DAT_LMR_TRIPLET *, /* local_segments      */
	IN      DAT_VLEN );		/* num_segments         */

/* RMR functions */

typedef DAT_RETURN (DAT_API *DAT_RMR_CREATE_FUNC) (
	IN      DAT_PZ_HANDLE,		/* pz_handle            */
	OUT     DAT_RMR_HANDLE *);	/* rmr_handle           */

typedef DAT_RETURN (DAT_API *DAT_RMR_CREATE_FOR_EP_FUNC) (
	IN      DAT_PZ_HANDLE,		/* pz_handle            */
	OUT     DAT_RMR_HANDLE *);	/* rmr_handle           */

typedef DAT_RETURN (DAT_API *DAT_RMR_QUERY_FUNC) (
	IN      DAT_RMR_HANDLE,		/* rmr_handle           */
	IN      DAT_RMR_PARAM_MASK,	/* rmr_param_mask       */
	OUT     DAT_RMR_PARAM *);	/* rmr_param            */

typedef DAT_RETURN (DAT_API *DAT_RMR_BIND_FUNC) (
	IN      DAT_RMR_HANDLE,		/* rmr_handle           */
	IN      DAT_LMR_HANDLE,		/* lmr_handle           */
	IN      const DAT_LMR_TRIPLET *,/* lmr_triplet          */
	IN      DAT_MEM_PRIV_FLAGS,	/* mem_priv             */
	IN      DAT_VA_TYPE,		/* va_type		*/
	IN      DAT_EP_HANDLE,		/* ep_handle            */
	IN      DAT_RMR_COOKIE,		/* user_cookie          */
	IN      DAT_COMPLETION_FLAGS,	/* completion_flags     */
	OUT     DAT_RMR_CONTEXT * );	/* context              */

typedef DAT_RETURN (DAT_API *DAT_RMR_FREE_FUNC) (
	IN      DAT_RMR_HANDLE);	/* rmr_handle           */

/* PSP functions */

typedef DAT_RETURN (DAT_API *DAT_PSP_CREATE_FUNC) (
	IN      DAT_IA_HANDLE,		/* ia_handle            */
	IN      DAT_CONN_QUAL,		/* conn_qual            */
	IN      DAT_EVD_HANDLE,		/* evd_handle           */
	IN      DAT_PSP_FLAGS,		/* psp_flags            */
	OUT     DAT_PSP_HANDLE * );	/* psp_handle           */

typedef DAT_RETURN (DAT_API *DAT_PSP_CREATE_ANY_FUNC) (
	IN      DAT_IA_HANDLE,		/* ia_handle            */
	OUT     DAT_CONN_QUAL *,	/* conn_qual            */
	IN      DAT_EVD_HANDLE,		/* evd_handle           */
	IN      DAT_PSP_FLAGS,		/* psp_flags            */
	OUT     DAT_PSP_HANDLE * );	/* psp_handle           */

typedef DAT_RETURN (DAT_API *DAT_PSP_QUERY_FUNC) (
	IN      DAT_PSP_HANDLE, 	/* psp_handle           */
	IN      DAT_PSP_PARAM_MASK,	/* psp_param_mask       */
	OUT     DAT_PSP_PARAM * );	/* psp_param            */

typedef DAT_RETURN (DAT_API *DAT_PSP_FREE_FUNC) (
	IN      DAT_PSP_HANDLE );	/* psp_handle           */

/* RSP functions */

typedef DAT_RETURN (DAT_API *DAT_RSP_CREATE_FUNC) (
	IN      DAT_IA_HANDLE,		/* ia_handle            */
	IN      DAT_CONN_QUAL,		/* conn_qual            */
	IN      DAT_EP_HANDLE,		/* ep_handle            */
	IN      DAT_EVD_HANDLE,		/* evd_handle           */
	OUT     DAT_RSP_HANDLE * );	/* rsp_handle           */

typedef DAT_RETURN (DAT_API *DAT_RSP_QUERY_FUNC) (
	IN      DAT_RSP_HANDLE,		/* rsp_handle           */
	IN      DAT_RSP_PARAM_MASK,	/* rsp_param_mask       */
	OUT     DAT_RSP_PARAM * );	/* rsp_param           */

typedef DAT_RETURN (DAT_API *DAT_RSP_FREE_FUNC) (
	IN      DAT_RSP_HANDLE );	/* rsp_handle           */

	/* CSP functions functions - DAT 2.0 */

typedef DAT_RETURN (DAT_API *DAT_CSP_CREATE_FUNC) (
	IN      DAT_IA_HANDLE,          /* ia_handle */
	IN      DAT_COMM *,             /* communicator */
	IN      DAT_IA_ADDRESS_PTR,     /* address */
	IN      DAT_EVD_HANDLE,         /* evd_handle */
	OUT     DAT_CSP_HANDLE * );     /* csp_handle           */

typedef DAT_RETURN (DAT_API *DAT_CSP_QUERY_FUNC) (
	IN      DAT_CSP_HANDLE,         /* csp_handle           */
	IN      DAT_CSP_PARAM_MASK,     /* csp_param_mask       */
	OUT     DAT_CSP_PARAM * );      /* csp_param            */

typedef DAT_RETURN (DAT_API *DAT_CSP_FREE_FUNC) (
	IN      DAT_CSP_HANDLE );       /* csp_handle           */

/* PZ functions */

typedef DAT_RETURN (DAT_API *DAT_PZ_CREATE_FUNC) (
	IN      DAT_IA_HANDLE,		/* ia_handle            */
	OUT     DAT_PZ_HANDLE * );	/* pz_handle            */

typedef DAT_RETURN (DAT_API *DAT_PZ_QUERY_FUNC) (
	IN      DAT_PZ_HANDLE,		/* pz_handle            */
	IN      DAT_PZ_PARAM_MASK,	/* pz_param_mask        */
	OUT     DAT_PZ_PARAM *);	/* pz_param             */

typedef DAT_RETURN (DAT_API *DAT_PZ_FREE_FUNC) (
	IN      DAT_PZ_HANDLE );	/* pz_handle            */

/* SRQ functions */

typedef DAT_RETURN (DAT_API *DAT_SRQ_CREATE_FUNC) (
	IN      DAT_IA_HANDLE,		/* ia_handle            */
	IN      DAT_PZ_HANDLE,		/* pz_handle            */
	IN      DAT_SRQ_ATTR *,		/* srq_attributes       */
	OUT     DAT_SRQ_HANDLE *);	/* srq_handle           */

typedef DAT_RETURN (DAT_API *DAT_SRQ_SET_LW_FUNC) (
	IN      DAT_SRQ_HANDLE,		/* srq_handle           */
	IN      DAT_COUNT );		/* srq_low_watermark    */

typedef DAT_RETURN (DAT_API *DAT_SRQ_FREE_FUNC) (
	IN      DAT_SRQ_HANDLE);	/* srq_handle           */

typedef DAT_RETURN (DAT_API *DAT_SRQ_QUERY_FUNC) (
	IN      DAT_SRQ_HANDLE,		/* srq_handle           */
	IN      DAT_SRQ_PARAM_MASK,	/* srq_param_mask       */
	OUT     DAT_SRQ_PARAM *);	/* srq_param            */

typedef DAT_RETURN (DAT_API *DAT_SRQ_RESIZE_FUNC) (
	IN      DAT_SRQ_HANDLE,		/* srq_handle           */
	IN      DAT_COUNT );		/* srq_queue_length     */

typedef DAT_RETURN (DAT_API *DAT_SRQ_POST_RECV_FUNC) (
	IN      DAT_SRQ_HANDLE,		/* srq_handle           */
	IN      DAT_COUNT,		/* num_segments         */
	IN      DAT_LMR_TRIPLET *,	/* local_iov            */
	IN      DAT_DTO_COOKIE );	/* user_cookie          */

typedef DAT_RETURN (DAT_API *DAT_IA_HA_RELATED_FUNC) (
	IN	DAT_IA_HANDLE, /* ia_handle            */
	IN const DAT_NAME_PTR, /* provider             */
	OUT	DAT_BOOLEAN *);		/* answer */

#ifdef DAT_EXTENSIONS	
#include <stdarg.h>
typedef DAT_RETURN (DAT_API *DAT_HANDLE_EXTENDEDOP_FUNC)(
	IN	DAT_HANDLE,		/* handle */
	IN	DAT_EXTENDED_OP,	/* extended op */
	IN	va_list);		/* argument list */
#endif /* DAT_EXTENSIONS */

#endif /* _DAT_REDIRECTION_H_ */
