/*
 * Copyright (c) 2009 Intel Corporation.  All rights reserved.
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

/* 
 * Definitions common to all OpenIB providers, cma, scm, ucm 
 */

#ifndef _DAPL_IB_COMMON_H_
#define _DAPL_IB_COMMON_H_

#include <infiniband/verbs.h>

#ifdef DAT_EXTENSIONS
#include <dat2/dat_ib_extensions.h>
#endif

#ifndef __cplusplus
#define false 0
#define true  1
#endif /*__cplusplus */

/* Typedefs to map common DAPL provider types to IB verbs */
typedef	struct ibv_qp		*ib_qp_handle_t;
typedef	struct ibv_cq		*ib_cq_handle_t;
typedef	struct ibv_pd		*ib_pd_handle_t;
typedef	struct ibv_mr		*ib_mr_handle_t;
typedef	struct ibv_mw		*ib_mw_handle_t;
typedef	struct ibv_wc		ib_work_completion_t;
typedef struct ibv_ah		*ib_ah_handle_t;
typedef union  ibv_gid		*ib_gid_handle_t;

/* HCA context type maps to IB verbs  */
typedef	struct ibv_context	*ib_hca_handle_t;
typedef ib_hca_handle_t		dapl_ibal_ca_t;

/* QP info to exchange, wire protocol version for these CM's */
#define DCM_VER 6

/* CM private data areas, same for all operations */
#define        DCM_MAX_PDATA_SIZE      118

/*
 * UCM DAPL IB/QP address (lid, qp_num, gid) mapping to
 * DAT_IA_ADDRESS_PTR, DAT_SOCK_ADDR2 (28 bytes)
 * For applications, like MPI, that exchange IA_ADDRESS
 * across the fabric before connecting, it eliminates the
 * overhead of name and address resolution to the destination's
 * CM services. UCM provider uses the following for 
 * DAT_IA_ADDRESS. Note: family == AF_INET6 to insure proper
 * callee storage for address.
 */
union dcm_addr {
       DAT_SOCK_ADDR6          so;
       struct {
		uint16_t	family;  /* sin6_family */
		uint16_t	lid;     /* sin6_port */
		uint32_t	qpn;     /* sin6_flowinfo */
		uint8_t		gid[16]; /* sin6_addr */
		uint16_t	port;    /* sin6_scope_id */
		uint8_t		sl;
		uint8_t		qp_type;
       } ib;
};

/* 256 bytes total; default max_inline_send, min IB MTU size */
typedef struct _ib_cm_msg
{
	uint16_t		ver;
	uint16_t		op;
	uint16_t		sport; /* src cm port */
	uint16_t		dport; /* dst cm port */
	uint32_t		sqpn;  /* src cm qpn */
	uint32_t		dqpn;  /* dst cm qpn */
	uint16_t		p_size;
	uint8_t			resv[14];
	union dcm_addr		saddr;
	union dcm_addr		daddr;
	union dcm_addr		saddr_alt;
	union dcm_addr		daddr_alt;
	uint8_t			p_data[DCM_MAX_PDATA_SIZE];

} ib_cm_msg_t;

/* CM events */
typedef enum {
	IB_CME_CONNECTED,
	IB_CME_DISCONNECTED,
	IB_CME_DISCONNECTED_ON_LINK_DOWN,
	IB_CME_CONNECTION_REQUEST_PENDING,
	IB_CME_CONNECTION_REQUEST_PENDING_PRIVATE_DATA,
	IB_CME_CONNECTION_REQUEST_ACKED,
	IB_CME_DESTINATION_REJECT,
	IB_CME_DESTINATION_REJECT_PRIVATE_DATA,
	IB_CME_DESTINATION_UNREACHABLE,
	IB_CME_TOO_MANY_CONNECTION_REQUESTS,
	IB_CME_LOCAL_FAILURE,
	IB_CME_BROKEN,
	IB_CME_TIMEOUT
} ib_cm_events_t;

/* Operation and state mappings */
typedef int ib_send_op_type_t;
typedef	struct	ibv_sge		ib_data_segment_t;
typedef enum	ibv_qp_state	ib_qp_state_t;
typedef	enum	ibv_event_type	ib_async_event_type;
typedef struct	ibv_async_event	ib_error_record_t;	

/* CQ notifications */
typedef enum
{
	IB_NOTIFY_ON_NEXT_COMP,
	IB_NOTIFY_ON_SOLIC_COMP

} ib_notification_type_t;

/* other mappings */
typedef int			ib_bool_t;
typedef union ibv_gid		GID;
typedef char			*IB_HCA_NAME;
typedef uint16_t		ib_hca_port_t;

/* Definitions */
#define IB_INVALID_HANDLE	NULL

/* inline send rdma threshold */
#define	INLINE_SEND_IWARP_DEFAULT	64
#define	INLINE_SEND_IB_DEFAULT		256

/* qkey for UD QP's */
#define DAT_UD_QKEY	0x78654321

/* RC timer - retry count defaults */
#define DCM_ACK_TIMER	16 /* 5 bits, 4.096us*2^ack_timer. 16== 268ms */
#define DCM_ACK_RETRY	7  /* 3 bits, 7 * 268ms = 1.8 seconds */
#define DCM_RNR_TIMER	12 /* 5 bits, 12 =.64ms, 28 =163ms, 31 =491ms */
#define DCM_RNR_RETRY	7  /* 3 bits, 7 == infinite */
#define DCM_IB_MTU	2048

/* Global routing defaults */
#define DCM_GLOBAL	0       /* global routing is disabled */
#define DCM_HOP_LIMIT	0xff
#define DCM_TCLASS	0

/* DAPL uCM timers, default queue sizes */
#define DCM_RETRY_CNT   10 
#define DCM_REP_TIME    800	/* reply timeout in m_secs */
#define DCM_RTU_TIME    400	/* rtu timeout in m_secs */
#define DCM_QP_SIZE     500     /* uCM tx, rx qp size */
#define DCM_CQ_SIZE     500     /* uCM cq size */

/* DTO OPs, ordered for DAPL ENUM definitions */
#define OP_RDMA_WRITE           IBV_WR_RDMA_WRITE
#define OP_RDMA_WRITE_IMM       IBV_WR_RDMA_WRITE_WITH_IMM
#define OP_SEND                 IBV_WR_SEND
#define OP_SEND_IMM             IBV_WR_SEND_WITH_IMM
#define OP_RDMA_READ            IBV_WR_RDMA_READ
#define OP_COMP_AND_SWAP        IBV_WR_ATOMIC_CMP_AND_SWP
#define OP_FETCH_AND_ADD        IBV_WR_ATOMIC_FETCH_AND_ADD
#define OP_RECEIVE              7   /* internal op */
#define OP_RECEIVE_IMM		8   /* rdma write with immed, internel op */
#define OP_RECEIVE_MSG_IMM	9   /* recv msg with immed, internel op */
#define OP_BIND_MW              10   /* internal op */
#define OP_SEND_UD              11  /* internal op */
#define OP_RECV_UD              12  /* internal op */
#define OP_INVALID		0xff

/* Definitions to map QP state */
#define IB_QP_STATE_RESET	IBV_QPS_RESET
#define IB_QP_STATE_INIT	IBV_QPS_INIT
#define IB_QP_STATE_RTR		IBV_QPS_RTR
#define IB_QP_STATE_RTS		IBV_QPS_RTS
#define IB_QP_STATE_SQD		IBV_QPS_SQD
#define IB_QP_STATE_SQE		IBV_QPS_SQE
#define IB_QP_STATE_ERROR	IBV_QPS_ERR

/* Definitions for ibverbs/mthca return codes, should be defined in verbs.h */
/* some are errno and some are -n values */

/**
 * ibv_get_device_name - Return kernel device name
 * ibv_get_device_guid - Return device's node GUID
 * ibv_open_device - Return ibv_context or NULL
 * ibv_close_device - Return 0, (errno?)
 * ibv_get_async_event - Return 0, -1 
 * ibv_alloc_pd - Return ibv_pd, NULL
 * ibv_dealloc_pd - Return 0, errno 
 * ibv_reg_mr - Return ibv_mr, NULL
 * ibv_dereg_mr - Return 0, errno
 * ibv_create_cq - Return ibv_cq, NULL
 * ibv_destroy_cq - Return 0, errno
 * ibv_get_cq_event - Return 0 & ibv_cq/context, int
 * ibv_poll_cq - Return n & ibv_wc, 0 ok, -1 empty, -2 error 
 * ibv_req_notify_cq - Return 0 (void?)
 * ibv_create_qp - Return ibv_qp, NULL
 * ibv_modify_qp - Return 0, errno
 * ibv_destroy_qp - Return 0, errno
 * ibv_post_send - Return 0, -1 & bad_wr
 * ibv_post_recv - Return 0, -1 & bad_wr 
 */

/* async handler for DTO, CQ, QP, and unafiliated */
typedef void (*ib_async_dto_handler_t)(
    IN    ib_hca_handle_t    ib_hca_handle,
    IN    ib_error_record_t  *err_code,
    IN    void               *context);

typedef void (*ib_async_cq_handler_t)(
    IN    ib_hca_handle_t    ib_hca_handle,
    IN    ib_cq_handle_t     ib_cq_handle,
    IN    ib_error_record_t  *err_code,
    IN    void               *context);

typedef void (*ib_async_qp_handler_t)(
    IN    ib_hca_handle_t    ib_hca_handle,
    IN    ib_qp_handle_t     ib_qp_handle,
    IN    ib_error_record_t  *err_code,
    IN    void               *context);

typedef void (*ib_async_handler_t)(
    IN    ib_hca_handle_t    ib_hca_handle,
    IN    ib_error_record_t  *err_code,
    IN    void               *context);

typedef enum
{
	IB_THREAD_INIT,
	IB_THREAD_CREATE,
	IB_THREAD_RUN,
	IB_THREAD_CANCEL,
	IB_THREAD_EXIT

} ib_thread_state_t;

typedef enum dapl_cm_op
{
	DCM_REQ = 1,
	DCM_REP,
	DCM_REJ_USER, /* user reject */
	DCM_REJ_CM,   /* cm reject, no SID */
	DCM_RTU,
	DCM_DREQ,
	DCM_DREP

} DAPL_CM_OP;

typedef enum dapl_cm_state
{
	DCM_INIT,
	DCM_LISTEN,
	DCM_CONN_PENDING,
	DCM_REP_PENDING,
	DCM_ACCEPTING,
	DCM_ACCEPTING_DATA,
	DCM_ACCEPTED,
	DCM_REJECTING,
	DCM_REJECTED,
	DCM_CONNECTED,
	DCM_RELEASED,
	DCM_DISC_PENDING,
	DCM_DISCONNECTED,
	DCM_DESTROY,
	DCM_RTU_PENDING,
	DCM_DISC_RECV

} DAPL_CM_STATE;

/* provider specfic fields for shared memory support */
typedef uint32_t ib_shm_transport_t;

/* prototypes */
int32_t	dapls_ib_init(void);
int32_t	dapls_ib_release(void);

/* util.c */
enum ibv_mtu dapl_ib_mtu(int mtu);
char *dapl_ib_mtu_str(enum ibv_mtu mtu);
DAT_RETURN getlocalipaddr(DAT_SOCK_ADDR *addr, int addr_len);

/* qp.c */
DAT_RETURN dapls_modify_qp_ud(IN DAPL_HCA *hca, IN ib_qp_handle_t qp);
DAT_RETURN dapls_modify_qp_state(IN ib_qp_handle_t	qp_handle,
                                IN ib_qp_state_t	qp_state,
                                IN uint32_t		qpn,
                                IN uint16_t		lid,
                                IN ib_gid_handle_t	gid);
ib_ah_handle_t dapls_create_ah( IN DAPL_HCA		*hca,
				IN ib_pd_handle_t	pd,
				IN ib_qp_handle_t	qp,
				IN uint16_t		lid,
				IN ib_gid_handle_t	gid);

/* inline functions */
STATIC _INLINE_ IB_HCA_NAME dapl_ib_convert_name (IN char *name)
{
	/* use ascii; name of local device */
	return dapl_os_strdup(name);
}

STATIC _INLINE_ void dapl_ib_release_name (IN IB_HCA_NAME name)
{
	return;
}

/*
 *  Convert errno to DAT_RETURN values
 */
STATIC _INLINE_ DAT_RETURN 
dapl_convert_errno( IN int err, IN const char *str )
{
    if (!err)  return DAT_SUCCESS;
    	
#if DAPL_DBG
    if ((err != EAGAIN) && (err != ETIMEDOUT))
	dapl_dbg_log (DAPL_DBG_TYPE_ERR," %s %s\n", str, strerror(err));
#endif 

    switch( err )
    {
	case EOVERFLOW	: return DAT_LENGTH_ERROR;
	case EACCES	: return DAT_PRIVILEGES_VIOLATION;
	case EPERM	: return DAT_PROTECTION_VIOLATION;		  
	case EINVAL	: return DAT_INVALID_HANDLE;
    	case EISCONN	: return DAT_INVALID_STATE | DAT_INVALID_STATE_EP_CONNECTED;
    	case ECONNREFUSED : return DAT_INVALID_STATE | DAT_INVALID_STATE_EP_NOTREADY;
	case ETIMEDOUT	: return DAT_TIMEOUT_EXPIRED;
    	case ENETUNREACH: return DAT_INVALID_ADDRESS | DAT_INVALID_ADDRESS_UNREACHABLE;
    	case EADDRINUSE	: return DAT_CONN_QUAL_IN_USE;
    	case EALREADY	: return DAT_INVALID_STATE | DAT_INVALID_STATE_EP_ACTCONNPENDING;
	case ENOMEM	: return DAT_INSUFFICIENT_RESOURCES;
        case EAGAIN	: return DAT_QUEUE_EMPTY;
	case EINTR	: return DAT_INTERRUPTED_CALL;
    	case EAFNOSUPPORT : return DAT_INVALID_ADDRESS | DAT_INVALID_ADDRESS_MALFORMED;
    	case EFAULT	: 
	default		: return DAT_INTERNAL_ERROR;
    }
 }

STATIC _INLINE_ char * dapl_cm_state_str(IN int st)
{
	static char *state[] = {
		"CM_INIT",
		"CM_LISTEN",
		"CM_CONN_PENDING",
		"CM_REP_PENDING",
		"CM_ACCEPTING",
		"CM_ACCEPTING_DATA",
		"CM_ACCEPTED",
		"CM_REJECTING",
		"CM_REJECTED",
		"CM_CONNECTED",
		"CM_RELEASED",
		"CM_DISC_PENDING",
		"CM_DISCONNECTED",
		"CM_DESTROY",
		"CM_RTU_PENDING",
		"CM_DISC_RECV"
        };
        return ((st < 0 || st > 15) ? "Invalid CM state?" : state[st]);
}

STATIC _INLINE_ char * dapl_cm_op_str(IN int op)
{
	static char *ops[] = {
		"INVALID",
		"REQ",
		"REP",
		"REJ_USER",
		"REJ_CM",
		"RTU",
		"DREQ",
		"DREP",
	};
	return ((op < 1 || op > 7) ? "Invalid OP?" : ops[op]);
}

#endif /*  _DAPL_IB_COMMON_H_ */
