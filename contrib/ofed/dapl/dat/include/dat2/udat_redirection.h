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
 * HEADER: udat_redirection.h
 *
 * PURPOSE: User DAT macro definitions
 *
 * Description: Macros to invoke DAPL functions from the dat_registry
 *
 * Mapping rules:
 *      All global symbols are prepended with DAT_ or dat_
 *      All DAT objects have an 'api' tag which, such as 'ep' or 'lmr'
 *      The method table is in the provider definition structure.
 *
 **********************************************************/
#ifndef _UDAT_REDIRECTION_H_
#define _UDAT_REDIRECTION_H_


#define DAT_LMR_CREATE(ia, mem_type, reg_desc, len, pz, priv, va_type, \
                       lmr, lmr_context, rmr_context, reg_len, reg_addr) \
         (*DAT_HANDLE_TO_PROVIDER(ia)->lmr_create_func)(\
                (ia),               \
                (mem_type),         \
                (reg_desc),         \
                (len),              \
                (pz),               \
                (priv),             \
		(va_type),	\
                (lmr),              \
                (lmr_context),      \
                (rmr_context),      \
                (reg_len),          \
                (reg_addr))

#define DAT_EVD_CREATE(ia, qlen, cno, flags, handle) \
        (*DAT_HANDLE_TO_PROVIDER(ia)->evd_create_func)(\
                (ia),               \
                (qlen),             \
                (cno),              \
                (flags),            \
                (handle))

#define DAT_EVD_ENABLE(evd) \
        (*DAT_HANDLE_TO_PROVIDER(evd)->evd_enable_func)(\
                (evd))

#define DAT_EVD_WAIT(evd, timeout, threshold, event, nmore) \
        (*DAT_HANDLE_TO_PROVIDER(evd)->evd_wait_func)(\
                (evd),              \
                (timeout),          \
                (threshold),        \
                (event),            \
                (nmore))

#define DAT_EVD_DISABLE(evd) \
        (*DAT_HANDLE_TO_PROVIDER(evd)->evd_disable_func)(\
                (evd))

#define DAT_EVD_SET_UNWAITABLE(evd) \
        (*DAT_HANDLE_TO_PROVIDER(evd)->evd_set_unwaitable_func)(\
                (evd))

#define DAT_EVD_CLEAR_UNWAITABLE(evd) \
        (*DAT_HANDLE_TO_PROVIDER(evd)->evd_clear_unwaitable_func)(\
                (evd))

#define DAT_EVD_MODIFY_CNO(evd, cno) \
        (*DAT_HANDLE_TO_PROVIDER(evd)->evd_modify_cno_func)(\
                (evd),              \
                (cno))

#define DAT_CNO_CREATE(ia, proxy, cno) \
        (*DAT_HANDLE_TO_PROVIDER(ia)->cno_create_func)(\
                (ia),               \
                (proxy),            \
                (cno))

#define DAT_CNO_FD_CREATE(ia, fd, cno) \
	(*DAT_HANDLE_TO_PROVIDER(ia)->cno_fd_create_func)(\
		(ia),               \
		(fd),            \
		(cno))

#define DAT_CNO_TRIGGER(cno, evd) \
	(*DAT_HANDLE_TO_PROVIDER(cno)->cno_trigger_func)(\
                (cno),              \
		(evd))


#define DAT_CNO_MODIFY_AGENT(cno, proxy) \
        (*DAT_HANDLE_TO_PROVIDER(cno)->cno_modify_agent_func)(\
                (cno),              \
                (proxy))

#define DAT_CNO_QUERY(cno, mask, param) \
        (*DAT_HANDLE_TO_PROVIDER(cno)->cno_query_func)(\
                (cno),              \
                (mask),             \
                (param))

#define DAT_CNO_FREE(cno) \
	(*DAT_HANDLE_TO_PROVIDER(cno)->cno_free_func)(\
		(cno))

#define DAT_CNO_WAIT(cno,timeout,evd) \
	(*DAT_HANDLE_TO_PROVIDER(cno)->cno_wait_func)(\
		(cno),              \
		(timeout),          \
		(evd))

/***************************************************************
 * FUNCTION PROTOTYPES
 *
 * User DAT function call definitions,
 *
 ****************************************************************/

typedef DAT_RETURN (DAT_API *DAT_LMR_CREATE_FUNC) (
	IN      DAT_IA_HANDLE,		/* ia_handle            */
	IN      DAT_MEM_TYPE,		/* mem_type             */
	IN      DAT_REGION_DESCRIPTION, /* region_description   */
	IN      DAT_VLEN,		/* length               */
	IN      DAT_PZ_HANDLE,		/* pz_handle            */
	IN      DAT_MEM_PRIV_FLAGS,	/* privileges           */
	IN	DAT_VA_TYPE,		/* va_type		*/
	OUT     DAT_LMR_HANDLE *,	/* lmr_handle           */
	OUT     DAT_LMR_CONTEXT *,	/* lmr_context          */
	OUT     DAT_RMR_CONTEXT *,	/* rmr_context          */
	OUT     DAT_VLEN *,		/* registered_length    */
	OUT     DAT_VADDR * );		/* registered_address   */

typedef DAT_RETURN (DAT_API *DAT_LMR_QUERY_FUNC) (
	IN      DAT_LMR_HANDLE,		/* lmr_handle           */
	IN      DAT_LMR_PARAM_MASK,	/* lmr_param_mask       */
	OUT     DAT_LMR_PARAM *);	/* lmr_param            */

/* Event functions */

typedef DAT_RETURN (DAT_API *DAT_EVD_CREATE_FUNC) (
	IN      DAT_IA_HANDLE,		/* ia_handle            */
	IN      DAT_COUNT,		/* evd_min_qlen         */
	IN      DAT_CNO_HANDLE,		/* cno_handle           */
	IN      DAT_EVD_FLAGS,		/* evd_flags            */
	OUT     DAT_EVD_HANDLE * );	/* evd_handle           */

typedef DAT_RETURN (DAT_API *DAT_EVD_MODIFY_CNO_FUNC) (
	IN      DAT_EVD_HANDLE,		/* evd_handle           */
	IN      DAT_CNO_HANDLE);	/* cno_handle           */

typedef DAT_RETURN (DAT_API *DAT_CNO_CREATE_FUNC) (
	IN 	DAT_IA_HANDLE,		/* ia_handle            */
	IN 	DAT_OS_WAIT_PROXY_AGENT,/* agent                */
	OUT 	DAT_CNO_HANDLE *);	/* cno_handle           */

typedef DAT_RETURN (DAT_API *DAT_CNO_FD_CREATE_FUNC) (
	IN 	DAT_IA_HANDLE,		/* ia_handle            */
	OUT	DAT_FD *,		/* file_descriptor	*/
	OUT 	DAT_CNO_HANDLE *);	/* cno_handle           */

typedef DAT_RETURN (DAT_API *DAT_CNO_TRIGGER_FUNC) (
	IN      DAT_CNO_HANDLE,          /* cno_handle          */
	OUT     DAT_EVD_HANDLE *);	/* trigger */

typedef DAT_RETURN (DAT_API *DAT_CNO_MODIFY_AGENT_FUNC) (
	IN 	DAT_CNO_HANDLE,		 /* cno_handle          */
	IN 	DAT_OS_WAIT_PROXY_AGENT);/* agent               */

typedef DAT_RETURN (DAT_API *DAT_CNO_QUERY_FUNC) (
	IN      DAT_CNO_HANDLE,		/* cno_handle           */
	IN      DAT_CNO_PARAM_MASK,	/* cno_param_mask       */
	OUT     DAT_CNO_PARAM * );	/* cno_param            */

typedef DAT_RETURN (DAT_API *DAT_CNO_FREE_FUNC) (
	IN DAT_CNO_HANDLE);		/* cno_handle           */

typedef DAT_RETURN (DAT_API *DAT_CNO_WAIT_FUNC) (
	IN  	DAT_CNO_HANDLE,		/* cno_handle           */
	IN  	DAT_TIMEOUT,		/* timeout              */
	OUT 	DAT_EVD_HANDLE *);	/* evd_handle           */

typedef DAT_RETURN (DAT_API *DAT_EVD_ENABLE_FUNC) (
	IN      DAT_EVD_HANDLE);	/* evd_handle           */

typedef DAT_RETURN (DAT_API *DAT_EVD_WAIT_FUNC) (
	IN  	DAT_EVD_HANDLE,		/* evd_handle           */
	IN  	DAT_TIMEOUT,		/* Timeout              */
	IN  	DAT_COUNT,		/* Threshold            */
	OUT 	DAT_EVENT *,		/* event                */
	OUT 	DAT_COUNT * );		/* N more events        */

typedef DAT_RETURN (DAT_API *DAT_EVD_DISABLE_FUNC) (
	IN      DAT_EVD_HANDLE);	/* evd_handle           */

typedef DAT_RETURN (DAT_API *DAT_EVD_SET_UNWAITABLE_FUNC) (
	IN      DAT_EVD_HANDLE);        /* evd_handle           */

typedef DAT_RETURN (DAT_API *DAT_EVD_CLEAR_UNWAITABLE_FUNC) (
	IN DAT_EVD_HANDLE);		/* evd_handle           */


#include <dat2/dat_redirection.h>

struct dat_provider
{
    const char *                        device_name;
    DAT_PVOID                           extension;

    DAT_IA_OPEN_FUNC                    ia_open_func;
    DAT_IA_QUERY_FUNC                   ia_query_func;
    DAT_IA_CLOSE_FUNC                   ia_close_func;

    DAT_SET_CONSUMER_CONTEXT_FUNC       set_consumer_context_func;
    DAT_GET_CONSUMER_CONTEXT_FUNC       get_consumer_context_func;
    DAT_GET_HANDLE_TYPE_FUNC            get_handle_type_func;

    DAT_CNO_CREATE_FUNC                 cno_create_func;	/* udat only */
    DAT_CNO_MODIFY_AGENT_FUNC           cno_modify_agent_func;	/* udat only */
    DAT_CNO_QUERY_FUNC                  cno_query_func;		/* udat only */
    DAT_CNO_FREE_FUNC                   cno_free_func;		/* udat only */
    DAT_CNO_WAIT_FUNC                   cno_wait_func;		/* udat only */

    DAT_CR_QUERY_FUNC                   cr_query_func;
    DAT_CR_ACCEPT_FUNC                  cr_accept_func;
    DAT_CR_REJECT_FUNC                  cr_reject_func;
    DAT_CR_HANDOFF_FUNC                 cr_handoff_func;

    DAT_EVD_CREATE_FUNC                 evd_create_func;
    DAT_EVD_QUERY_FUNC                  evd_query_func;

    DAT_EVD_MODIFY_CNO_FUNC             evd_modify_cno_func;	/* udat only */
    DAT_EVD_ENABLE_FUNC                 evd_enable_func;	/* udat only */
    DAT_EVD_DISABLE_FUNC                evd_disable_func;	/* udat only */
    DAT_EVD_WAIT_FUNC                   evd_wait_func;		/* udat only */

    DAT_EVD_RESIZE_FUNC                 evd_resize_func;
    DAT_EVD_POST_SE_FUNC                evd_post_se_func;
    DAT_EVD_DEQUEUE_FUNC                evd_dequeue_func;
    DAT_EVD_FREE_FUNC                   evd_free_func;

    DAT_EP_CREATE_FUNC                  ep_create_func;
    DAT_EP_QUERY_FUNC                   ep_query_func;
    DAT_EP_MODIFY_FUNC                  ep_modify_func;
    DAT_EP_CONNECT_FUNC                 ep_connect_func;
    DAT_EP_DUP_CONNECT_FUNC             ep_dup_connect_func;
    DAT_EP_DISCONNECT_FUNC              ep_disconnect_func;
    DAT_EP_POST_SEND_FUNC               ep_post_send_func;
    DAT_EP_POST_RECV_FUNC               ep_post_recv_func;
    DAT_EP_POST_RDMA_READ_FUNC          ep_post_rdma_read_func;
    DAT_EP_POST_RDMA_WRITE_FUNC         ep_post_rdma_write_func;
    DAT_EP_GET_STATUS_FUNC              ep_get_status_func;
    DAT_EP_FREE_FUNC                    ep_free_func;

    DAT_LMR_CREATE_FUNC                 lmr_create_func;
    DAT_LMR_QUERY_FUNC                  lmr_query_func;

    DAT_LMR_FREE_FUNC                   lmr_free_func;

    DAT_RMR_CREATE_FUNC                 rmr_create_func;
    DAT_RMR_QUERY_FUNC                  rmr_query_func;
    DAT_RMR_BIND_FUNC                   rmr_bind_func;
    DAT_RMR_FREE_FUNC                   rmr_free_func;

    DAT_PSP_CREATE_FUNC                 psp_create_func;
    DAT_PSP_QUERY_FUNC                  psp_query_func;
    DAT_PSP_FREE_FUNC                   psp_free_func;

    DAT_RSP_CREATE_FUNC                 rsp_create_func;
    DAT_RSP_QUERY_FUNC                  rsp_query_func;
    DAT_RSP_FREE_FUNC                   rsp_free_func;

    DAT_PZ_CREATE_FUNC                  pz_create_func;
    DAT_PZ_QUERY_FUNC                   pz_query_func;
    DAT_PZ_FREE_FUNC                    pz_free_func;

    /* dat-1.1 */
    DAT_PSP_CREATE_ANY_FUNC             psp_create_any_func;   /* dat-1.1    */
    DAT_EP_RESET_FUNC                   ep_reset_func;         /* dat-1.1    */

    /* udat-1.1 */
    DAT_EVD_SET_UNWAITABLE_FUNC         evd_set_unwaitable_func;   /* udat-1.1 */
    DAT_EVD_CLEAR_UNWAITABLE_FUNC       evd_clear_unwaitable_func; /* udat-1.1 */
    /* dat-1.2 */
    DAT_LMR_SYNC_RDMA_READ_FUNC         lmr_sync_rdma_read_func;
    DAT_LMR_SYNC_RDMA_WRITE_FUNC        lmr_sync_rdma_write_func;

    DAT_EP_CREATE_WITH_SRQ_FUNC         ep_create_with_srq_func;
    DAT_EP_RECV_QUERY_FUNC              ep_recv_query_func;
    DAT_EP_SET_WATERMARK_FUNC           ep_set_watermark_func;
    DAT_SRQ_CREATE_FUNC                 srq_create_func;
    DAT_SRQ_FREE_FUNC                   srq_free_func;
    DAT_SRQ_POST_RECV_FUNC              srq_post_recv_func;
    DAT_SRQ_QUERY_FUNC                  srq_query_func;
    DAT_SRQ_RESIZE_FUNC                 srq_resize_func;
    DAT_SRQ_SET_LW_FUNC                 srq_set_lw_func;

	/* DAT 2.0 functions */
    DAT_CSP_CREATE_FUNC                 csp_create_func;
    DAT_CSP_QUERY_FUNC                  csp_query_func;
    DAT_CSP_FREE_FUNC                   csp_free_func;
	
    DAT_EP_COMMON_CONNECT_FUNC	        ep_common_connect_func;

    DAT_RMR_CREATE_FOR_EP_FUNC	        rmr_create_for_ep_func;
    DAT_EP_POST_SEND_WITH_INVALIDATE_FUNC ep_post_send_with_invalidate_func;
    DAT_EP_POST_RDMA_READ_TO_RMR_FUNC   ep_post_rdma_read_to_rmr_func;

    DAT_CNO_FD_CREATE_FUNC		cno_fd_create_func;
    DAT_CNO_TRIGGER_FUNC		cno_trigger_func;

    DAT_IA_HA_RELATED_FUNC		ia_ha_related_func;

#ifdef DAT_EXTENSIONS
    DAT_HANDLE_EXTENDEDOP_FUNC		handle_extendedop_func;
#endif /* DAT_EXTENSIONS */
};

#endif /* _UDAT_REDIRECTION_H_ */
