/*
 * Copyright (c) 2007 Intel Corporation.  All rights reserved.
 * 
 * This Software is licensed under one of the following licenses:
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
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 * 
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

/**********************************************************************
 *
 * HEADER: dat_ib_extensions.h
 *
 * PURPOSE: extensions to the DAT API for IB transport specific services 
 *	    NOTE: Prototyped IB extension support in openib-cma 1.2 provider.
 *	          Applications MUST recompile with new dat.h definitions
 *		  and include this file.
 *
 * Description: Header file for "uDAPL: User Direct Access Programming
 *		Library, Version: 2.0"
 *
 * Mapping rules:
 *      All global symbols are prepended with "DAT_" or "dat_"
 *      All DAT objects have an 'api' tag which, such as 'ep' or 'lmr'
 *      The method table is in the provider definition structure.
 *
 *
 **********************************************************************/
#ifndef _DAT_IB_EXTENSIONS_H_
#define _DAT_IB_EXTENSIONS_H_

/* 
 * Provider specific attribute strings for extension support 
 *	returned with dat_ia_query() and 
 *	DAT_PROVIDER_ATTR_MASK == DAT_PROVIDER_FIELD_PROVIDER_SPECIFIC_ATTR
 *
 *	DAT_NAMED_ATTR	name == extended operations and version, 
 *			version_value = version number of extension API
 */

/* 2.0.1 - Initial IB extension support, atomic and immed data
 *         dat_ib_post_fetch_and_add()
 *         dat_ib_post_cmp_and_swap()
 *         dat_ib_post_rdma_write_immed()
 *		
 * 2.0.2 - Add UD support, post send and remote_ah via connect events 
 *         dat_ib_post_send_ud()
 *
 * 2.0.3 - Add query/print counter support for IA, EP, and EVD's 
 *         dat_query_counters(), dat_print_counters()
 *
 * 2.0.4 - Add DAT_IB_UD_CONNECTION_REJECT_EVENT extended UD event
 * 2.0.5 - Add DAT_IB_UD extended UD connection error events
 *
 */
#define DAT_IB_EXTENSION_VERSION	205	/* 2.0.5 */
#define DAT_ATTR_COUNTERS		"DAT_COUNTERS"
#define DAT_IB_ATTR_FETCH_AND_ADD	"DAT_IB_FETCH_AND_ADD"
#define DAT_IB_ATTR_CMP_AND_SWAP	"DAT_IB_CMP_AND_SWAP"
#define DAT_IB_ATTR_IMMED_DATA		"DAT_IB_IMMED_DATA"
#define DAT_IB_ATTR_UD			"DAT_IB_UD"

/* 
 * Definition for extended EVENT numbers, DAT_IB_EXTENSION_BASE_RANGE
 * is used by these extensions as a starting point for extended event numbers 
 *
 * DAT_IB_DTO_EVENT - All extended data transfers - req/recv evd
 * DAT_IB_AH_EVENT - address handle resolution - connect evd
 */
typedef enum dat_ib_event_number
{
	DAT_IB_DTO_EVENT = DAT_IB_EXTENSION_RANGE_BASE,
	DAT_IB_UD_CONNECTION_REQUEST_EVENT,
	DAT_IB_UD_CONNECTION_EVENT_ESTABLISHED,
	DAT_IB_UD_CONNECTION_REJECT_EVENT,
	DAT_IB_UD_CONNECTION_ERROR_EVENT

} DAT_IB_EVENT_NUMBER;

/* 
 * Extension operations 
 */
typedef enum dat_ib_op
{
	DAT_IB_FETCH_AND_ADD_OP,
	DAT_IB_CMP_AND_SWAP_OP,
	DAT_IB_RDMA_WRITE_IMMED_OP,
	DAT_IB_UD_SEND_OP,
	DAT_QUERY_COUNTERS_OP,
	DAT_PRINT_COUNTERS_OP
	
} DAT_IB_OP;

/*
 * The DAT_IB_EXT_TYPE enum specifies the type of extension operation that just
 * completed. All IB extended completion types both, DTO and NON-DTO, are 
 * reported in the extended operation type with the single DAT_IB_DTO_EVENT type. 
 * The specific extended DTO operation is reported with a DAT_IB_DTOS type in the 
 * operation field of the base DAT_EVENT structure. All other extended events are 
 * identified by unique DAT_IB_EVENT_NUMBER types.
 */
typedef enum dat_ib_ext_type
{
	DAT_IB_FETCH_AND_ADD,		// 0
	DAT_IB_CMP_AND_SWAP,		// 1
	DAT_IB_RDMA_WRITE_IMMED,	// 2
	DAT_IB_RDMA_WRITE_IMMED_DATA,	// 3
	DAT_IB_RECV_IMMED_DATA,		// 4
	DAT_IB_UD_CONNECT_REQUEST,	// 5
	DAT_IB_UD_REMOTE_AH,		// 6
	DAT_IB_UD_PASSIVE_REMOTE_AH,	// 7
	DAT_IB_UD_SEND,			// 8
	DAT_IB_UD_RECV,			// 9
	DAT_IB_UD_CONNECT_REJECT,	// 10
	DAT_IB_UD_CONNECT_ERROR,	// 11

} DAT_IB_EXT_TYPE;

/* 
 * Extension event status
 */
typedef enum dat_ib_status
{
	DAT_OP_SUCCESS = DAT_SUCCESS,
	DAT_IB_OP_ERR,

} DAT_IB_STATUS;


/* 
 * Definitions for additional extension type RETURN codes above
 * standard DAT types. Included with standard DAT_TYPE_STATUS 
 * bits using a DAT_EXTENSION BASE as a starting point.
 */
typedef enum dat_ib_return
{
	DAT_IB_ERR = DAT_EXTENSION_BASE,

} DAT_IB_RETURN;

/* 
 * Definition for extended IB DTO operations, DAT_DTO_EXTENSION_BASE
 * is used by DAT extensions as a starting point of extension DTOs 
 */
typedef enum dat_ib_dtos
{
	DAT_IB_DTO_RDMA_WRITE_IMMED = DAT_DTO_EXTENSION_BASE,
	DAT_IB_DTO_RECV_IMMED,
	DAT_IB_DTO_FETCH_ADD,
	DAT_IB_DTO_CMP_SWAP,
	DAT_IB_DTO_RECV_MSG_IMMED,
	DAT_IB_DTO_SEND_UD,
	DAT_IB_DTO_RECV_UD,
	DAT_IB_DTO_RECV_UD_IMMED,	

} DAT_IB_DTOS;

/* 
 * Definitions for additional extension handle types beyond 
 * standard DAT handle. New Bit definitions MUST start at 
 * DAT_HANDLE_TYPE_EXTENSION_BASE
 */
typedef enum dat_ib_handle_type
{
    DAT_IB_HANDLE_TYPE_EXT = DAT_HANDLE_TYPE_EXTENSION_BASE,

} DAT_IB_HANDLE_TYPE;

/*
 * The DAT_IB_EVD_EXTENSION_FLAGS enum specifies the EVD extension flags that
 * do not map directly to existing DAT_EVD_FLAGS. This new EVD flag has been 
 * added to identify an extended EVD that does not fit the existing stream 
 * types.
 */
typedef enum dat_ib_evd_extension_flags
{
	DAT_IB_EVD_EXTENSION_FLAG = DAT_EVD_EXTENSION_BASE,

} DAT_IB_EVD_EXTENSION_FLAGS;

/* 
 * Definition for memory privilege extension flags.
 * New privileges required for new atomic DTO type extensions.
 * New Bit definitions MUST start at DAT_MEM_PRIV_EXTENSION
 */
typedef enum dat_ib_mem_priv_flags
{
	DAT_IB_MEM_PRIV_REMOTE_ATOMIC = DAT_MEM_PRIV_EXTENSION_BASE,
	
} DAT_IB_MEM_PRIV_FLAGS;

/* 
 * Definition for IB address handle, unreliable datagram.
 */
typedef struct dat_ib_addr_handle
{
    struct ibv_ah	*ah;
    DAT_UINT32	        qpn;
    DAT_SOCK_ADDR6	ia_addr;
   
} DAT_IB_ADDR_HANDLE;

/* 
 * Definitions for extended event data:
 *	When dat_event->event_number >= DAT_IB_EXTENSION_BASE_RANGE
 *	then dat_event->extension_data == DAT_IB_EXT_EVENT_DATA type
 *	and ((DAT_IB_EXT_EVENT_DATA*)dat_event->extension_data)->type
 *	specifies extension data values. 
 * NOTE: DAT_IB_EXT_EVENT_DATA cannot exceed 64 bytes as defined by 
 *	 "DAT_UINT64 extension_data[8]" in DAT_EVENT (dat.h)
 */
typedef struct dat_ib_immed_data 
{
    DAT_UINT32			data;

} DAT_IB_IMMED_DATA;

/* 
 * Definitions for extended event data:
 *	When dat_event->event_number >= DAT_IB_EXTENSION_BASE_RANGE
 *	then dat_event->extension_data == DAT_EXTENSION_EVENT_DATA type
 *	and ((DAT_EXTENSION_EVENT_DATA*)dat_event->extension_data)->type
 *	specifies extension data values. 
 * NOTE: DAT_EXTENSION_EVENT_DATA cannot exceed 64 bytes as defined by 
 *	 "DAT_UINT64 extension_data[8]" in DAT_EVENT (dat.h)
 *
 *  Provide UD address handles via extended connect establishment. 
 *  ia_addr provided with extended conn events for reference.
 */
typedef struct dat_ib_extension_event_data
{
    DAT_IB_EXT_TYPE	type;
    DAT_IB_STATUS	status;
    union {
		DAT_IB_IMMED_DATA	immed;
    } val;
    DAT_IB_ADDR_HANDLE	remote_ah; 

} DAT_IB_EXTENSION_EVENT_DATA;

/* 
 * Definitions for additional extension handle types beyond 
 * standard DAT handle. New Bit definitions MUST start at 
 * DAT_HANDLE_TYPE_EXTENSION_BASE
 */
typedef enum dat_ib_service_type
{
    DAT_IB_SERVICE_TYPE_UD   = DAT_SERVICE_TYPE_EXTENSION_BASE,

} DAT_IB_SERVICE_TYPE;

/*
 * Definitions for 64-bit IA Counters
 */
typedef enum dat_ia_counters
{
	DCNT_IA_PZ_CREATE,
	DCNT_IA_PZ_FREE,
	DCNT_IA_LMR_CREATE,
	DCNT_IA_LMR_FREE,
	DCNT_IA_RMR_CREATE,
	DCNT_IA_RMR_FREE,
	DCNT_IA_PSP_CREATE,
	DCNT_IA_PSP_CREATE_ANY,
	DCNT_IA_PSP_FREE,
	DCNT_IA_RSP_CREATE,
	DCNT_IA_RSP_FREE,
	DCNT_IA_EVD_CREATE,
	DCNT_IA_EVD_FREE,
	DCNT_IA_EP_CREATE,
	DCNT_IA_EP_FREE,
	DCNT_IA_SRQ_CREATE,
	DCNT_IA_SRQ_FREE,
	DCNT_IA_SP_CR,
	DCNT_IA_SP_CR_ACCEPTED,
	DCNT_IA_SP_CR_REJECTED,
	DCNT_IA_MEM_ALLOC,
	DCNT_IA_MEM_ALLOC_DATA,
	DCNT_IA_MEM_FREE,
	DCNT_IA_ASYNC_ERROR,
	DCNT_IA_ASYNC_QP_ERROR,
	DCNT_IA_ASYNC_CQ_ERROR,
	DCNT_IA_ALL_COUNTERS,  /* MUST be last */

} DAT_IA_COUNTERS;

/*
 * Definitions for 64-bit EP Counters
 */
typedef enum dat_ep_counters
{
	DCNT_EP_CONNECT,
	DCNT_EP_DISCONNECT,
	DCNT_EP_POST_SEND,
	DCNT_EP_POST_SEND_DATA,
	DCNT_EP_POST_SEND_UD,
	DCNT_EP_POST_SEND_UD_DATA,
	DCNT_EP_POST_RECV,
	DCNT_EP_POST_RECV_DATA,
	DCNT_EP_POST_WRITE,
	DCNT_EP_POST_WRITE_DATA,
	DCNT_EP_POST_WRITE_IMM,
	DCNT_EP_POST_WRITE_IMM_DATA,
	DCNT_EP_POST_READ,
	DCNT_EP_POST_READ_DATA,
	DCNT_EP_POST_CMP_SWAP,
	DCNT_EP_POST_FETCH_ADD,
	DCNT_EP_RECV,
	DCNT_EP_RECV_DATA,
	DCNT_EP_RECV_UD,
	DCNT_EP_RECV_UD_DATA,
	DCNT_EP_RECV_IMM,
	DCNT_EP_RECV_IMM_DATA,
	DCNT_EP_RECV_RDMA_IMM,
	DCNT_EP_RECV_RDMA_IMM_DATA,
	DCNT_EP_ALL_COUNTERS,  /* MUST be last */

} DAT_EP_COUNTERS;

/*
 * Definitions for 64-bit EVD Counters
 */
typedef enum dat_evd_counters
{
	DCNT_EVD_WAIT,
	DCNT_EVD_WAIT_BLOCKED,
	DCNT_EVD_WAIT_NOTIFY,
	DCNT_EVD_DEQUEUE,
	DCNT_EVD_DEQUEUE_FOUND,
	DCNT_EVD_DEQUEUE_NOT_FOUND,
	DCNT_EVD_DEQUEUE_POLL,
	DCNT_EVD_DEQUEUE_POLL_FOUND,
	DCNT_EVD_CONN_CALLBACK,
	DCNT_EVD_DTO_CALLBACK,
	DCNT_EVD_ALL_COUNTERS,  /* MUST be last */

} DAT_EVD_COUNTERS;

/* Extended RETURN and EVENT STATUS string helper functions */

/* DAT_EXT_RETURN error to string */
static __inline__ DAT_RETURN DAT_API
dat_strerror_extension (
    IN  DAT_IB_RETURN 		value,
    OUT const char 		**message )
{
	switch( DAT_GET_TYPE(value) ) {
	case DAT_IB_ERR:
		*message = "DAT_IB_ERR";
		return DAT_SUCCESS;
	default:
		/* standard DAT return type */
		return(dat_strerror(value, message, NULL));
	}
}

/* DAT_EXT_STATUS error to string */
static __inline__ DAT_RETURN DAT_API
dat_strerror_ext_status (
    IN  DAT_IB_STATUS 	value,
    OUT const char 	**message )
{
	switch(value) {
	case 0:
		*message = " ";
		return DAT_SUCCESS;
	case DAT_IB_OP_ERR:
		*message = "DAT_IB_OP_ERR";
		return DAT_SUCCESS;
	default:
		*message = "unknown extension status";
		return DAT_INVALID_PARAMETER;
	}
}

/* 
 * Extended IB transport specific APIs
 *  redirection via DAT extension function
 */

/*
 * This asynchronous call is modeled after the InfiniBand atomic 
 * Fetch and Add operation. The add_value is added to the 64 bit 
 * value stored at the remote memory location specified in remote_iov
 * and the result is stored in the local_iov.  
 */
#define dat_ib_post_fetch_and_add(ep, add_val, lbuf, cookie, rbuf, flgs) \
	     dat_extension_op(  ep, \
				DAT_IB_FETCH_AND_ADD_OP, \
				(add_val), \
				(lbuf), \
				(cookie), \
				(rbuf), \
				(flgs))
				
/*
 * This asynchronous call is modeled after the InfiniBand atomic 
 * Compare and Swap operation. The cmp_value is compared to the 64 bit 
 * value stored at the remote memory location specified in remote_iov.  
 * If the two values are equal, the 64 bit swap_value is stored in 
 * the remote memory location.  In all cases, the original 64 bit 
 * value stored in the remote memory location is copied to the local_iov.
 */
#define dat_ib_post_cmp_and_swap(ep, cmp_val, swap_val, lbuf, cookie, rbuf, flgs) \
	     dat_extension_op(  ep, \
				DAT_IB_CMP_AND_SWAP_OP, \
				(cmp_val), \
				(swap_val), \
				(lbuf), \
				(cookie), \
				(rbuf), \
				(flgs))

/* 
 * RDMA Write with IMMEDIATE:
 *
 * This asynchronous call is modeled after the InfiniBand rdma write with  
 * immediate data operation. Event completion for the request completes as an 
 * DAT_EXTENSION with extension type set to DAT_DTO_EXTENSION_IMMED_DATA.
 * Event completion on the remote endpoint completes as receive DTO operation
 * type of DAT_EXTENSION with operation set to DAT_DTO_EXTENSION_IMMED_DATA.
 * The immediate data will be provided in the extented DTO event data structure.
 *
 * Note to Consumers: the immediate data will consume a receive
 * buffer at the Data Sink. 
 *
 * Other extension flags:
 *	n/a
 */
#define dat_ib_post_rdma_write_immed(ep, size, lbuf, cookie, rbuf, idata, flgs) \
	     dat_extension_op(  ep, \
				DAT_IB_RDMA_WRITE_IMMED_OP, \
				(size), \
				(lbuf), \
				(cookie), \
				(rbuf), \
				(idata), \
				(flgs))

/* 
 * Unreliable datagram: msg send 
 *
 * This asynchronous call is modeled after the InfiniBand UD message send
 * Event completion for the request completes as an 
 * DAT_EXTENSION with extension type set to DAT_DTO_EXTENSION_UD_SEND.
 * Event completion on the remote endpoint completes as receive DTO operation
 * type of DAT_EXTENSION with operation set to DAT_DTO_EXTENSION_UD_RECV.
 *
 * Other extension flags:
 *	n/a
 */
#define dat_ib_post_send_ud(ep, segments, lbuf, ah_ptr, cookie, flgs) \
	     dat_extension_op(  ep, \
				DAT_IB_UD_SEND_OP, \
				(segments), \
				(lbuf), \
				(ah_ptr), \
				(cookie), \
				(flgs))


/* 
 * Query counter(s):  
 * Provide IA, EP, or EVD and call will return appropriate counters
 * 	DAT_HANDLE dat_handle, enum cntr, *DAT_UINT64 p_cntrs_out, int reset
 *
 * use _ALL_COUNTERS to query all
 */
#define dat_query_counters(dat_handle, cntr, p_cntrs_out, reset) \
	     dat_extension_op(  dat_handle, \
				DAT_QUERY_COUNTERS_OP, \
				(cntr), \
				(p_cntrs_out), \
				(reset))
/* 
 * Print counter(s):  
 * Provide IA, EP, or EVD and call will print appropriate counters
 * 	DAT_HANDLE dat_handle, int cntr, int reset
 * 
 * use _ALL_COUNTERS to print all
 */
#define dat_print_counters(dat_handle, cntr, reset) \
	     dat_extension_op(  dat_handle, \
				DAT_PRINT_COUNTERS_OP, \
				(cntr), \
				(reset))

#endif /* _DAT_IB_EXTENSIONS_H_ */

