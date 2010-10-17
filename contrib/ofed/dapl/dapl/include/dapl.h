/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
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
 * MODULE: dapl.h
 *
 * PURPOSE: defines common data structures for the DAPL reference implemenation
 *
 * Description: This file describes the working data structures used within
 *              DAPL RI.
 *
 *
 * $Id: dapl.h 1317 2005-04-25 17:29:42Z jlentini $
 **********************************************************************/

#ifndef _DAPL_H_
#define _DAPL_H_

#if defined(__KERNEL__)
#include <dat2/kdat.h>
#else
#include <dat2/udat.h>
#endif	/* defined(__KERNEL__) */
#include <dat2/dat_registry.h>
#include "dapl_osd.h"
#include "dapl_debug.h"



/*********************************************************************
 *                                                                   *
 * Enumerations                                                      *
 *                                                                   *
 *********************************************************************/

typedef enum dapl_magic
{
    /* magic number values for verification & debug */
    DAPL_MAGIC_IA	= 0xCafeF00d,
    DAPL_MAGIC_EVD	= 0xFeedFace,
    DAPL_MAGIC_EP	= 0xDeadBabe,
    DAPL_MAGIC_LMR	= 0xBeefCafe,
    DAPL_MAGIC_RMR      = 0xABadCafe,
    DAPL_MAGIC_PZ	= 0xDeafBeef,
    DAPL_MAGIC_PSP	= 0xBeadeD0c,
    DAPL_MAGIC_RSP	= 0xFab4Feed,
    DAPL_MAGIC_SRQ	= 0xC001Babe,
    DAPL_MAGIC_CR	= 0xBe12Cee1,
    DAPL_MAGIC_CR_DESTROYED = 0xB12bDead,
    DAPL_MAGIC_CNO	= 0xDeadF00d,
    DAPL_MAGIC_INVALID  = 0xFFFFFFFF
} DAPL_MAGIC;

typedef enum dapl_evd_state
{
    DAPL_EVD_STATE_TERMINAL,
    DAPL_EVD_STATE_INITIAL,
    DAPL_EVD_STATE_OPEN,
    DAPL_EVD_STATE_WAITED,
    DAPL_EVD_STATE_DEAD	= 0xDEAD
} DAPL_EVD_STATE;

typedef enum dapl_evd_completion
{
    DAPL_EVD_STATE_INIT,
    DAPL_EVD_STATE_SOLICITED_WAIT,
    DAPL_EVD_STATE_THRESHOLD,
    DAPL_EVD_STATE_UNSIGNALLED
} DAPL_EVD_COMPLETION;

typedef enum dapl_cno_state
{
    DAPL_CNO_STATE_UNTRIGGERED,
    DAPL_CNO_STATE_TRIGGERED,
    DAPL_CNO_STATE_DEAD = 0xDeadFeed,
} DAPL_CNO_STATE;

typedef enum dapl_qp_state
{
    DAPL_QP_STATE_UNCONNECTED,
    DAPL_QP_STATE_RESERVED,
    DAPL_QP_STATE_PASSIVE_CONNECTION_PENDING,
    DAPL_QP_STATE_ACTIVE_CONNECTION_PENDING,
    DAPL_QP_STATE_TENTATIVE_CONNECTION_PENDING,
    DAPL_QP_STATE_CONNECTED,
    DAPL_QP_STATE_DISCONNECT_PENDING,
    DAPL_QP_STATE_ERROR,
    DAPL_QP_STATE_NOT_REUSABLE,
    DAPL_QP_STATE_FREE
} DAPL_QP_STATE;


/*********************************************************************
 *                                                                   *
 * Constants                                                         *
 *                                                                   *
 *********************************************************************/

/*
 * number of HCAs allowed
 */
#define DAPL_MAX_HCA_COUNT		4

/*
 * Configures the RMR bind evd restriction
 */

#define DAPL_RMR_BIND_EVD_RESTRICTION 	DAT_RMR_EVD_SAME_AS_REQUEST_EVD

/*
 * special qp_state indicating the EP does not have a QP attached yet
 */
#define DAPL_QP_STATE_UNATTACHED 	0xFFF0

#define DAPL_MAX_PRIVATE_DATA_SIZE 	256

/*********************************************************************
 *                                                                   *
 * Macros                                                            *
 *                                                                   *
 *********************************************************************/

#if defined (sun) || defined(__sun) || defined(_sun_) || defined (__solaris__) 
#define DAPL_BAD_PTR(a) ((unsigned long)(a) & 3)
#elif defined(__linux__)
#define DAPL_BAD_PTR(a) ((unsigned long)(a) & 3)
#elif defined(_WIN64)
#define DAPL_BAD_PTR(a) ((unsigned long)((DAT_UINT64)(a)) & 3)
#elif defined(_WIN32)
#define DAPL_BAD_PTR(a) ((unsigned long)((DAT_UINT64)(a)) & 3)
#endif

/*
 * Simple macro to verify a handle is bad. Conditions:
 * - pointer is NULL
 * - pointer is not word aligned
 * - pointer's magic number is wrong
 */

#define DAPL_BAD_HANDLE(h, magicNum) (				\
	    ((h) == NULL) ||					\
	    DAPL_BAD_PTR(h) ||					\
	    (((DAPL_HEADER *)(h))->magic != (magicNum)))

#define DAPL_MIN(a, b)        ((a < b) ? (a) : (b))
#define DAPL_MAX(a, b)        ((a > b) ? (a) : (b))

#if NDEBUG > 0
#define DEBUG_IS_BAD_HANDLE(h, magicNum) (DAPL_BAD_HANDLE(h, magicNum))
#else
#define DEBUG_IS_BAD_HANDLE(h, magicNum) (0)
#endif

#define DAT_ERROR(Type, SubType) ((DAT_RETURN)(DAT_CLASS_ERROR | Type | SubType))

/*********************************************************************
 *                                                                   *
 * Typedefs                                                          *
 *                                                                   *
 *********************************************************************/

typedef struct dapl_llist_entry		DAPL_LLIST_ENTRY;
typedef DAPL_LLIST_ENTRY *		DAPL_LLIST_HEAD;
typedef struct dapl_ring_buffer		DAPL_RING_BUFFER;
typedef struct dapl_cookie_buffer	DAPL_COOKIE_BUFFER;

typedef struct dapl_hash_table          DAPL_HASH_TABLE;
typedef struct dapl_hash_table *        DAPL_HASH_TABLEP;
typedef DAT_UINT64                      DAPL_HASH_KEY;
typedef void *                          DAPL_HASH_DATA;

typedef struct dapl_hca			DAPL_HCA;

typedef struct dapl_header		DAPL_HEADER;

typedef struct dapl_ia			DAPL_IA;
typedef struct dapl_cno			DAPL_CNO;
typedef struct dapl_evd         	DAPL_EVD;
typedef struct dapl_ep 	        	DAPL_EP;
typedef struct dapl_srq	        	DAPL_SRQ;
typedef struct dapl_pz			DAPL_PZ;
typedef struct dapl_lmr			DAPL_LMR;
typedef struct dapl_rmr			DAPL_RMR;
typedef struct dapl_sp			DAPL_SP;
typedef struct dapl_cr			DAPL_CR;

typedef struct dapl_cookie		DAPL_COOKIE;
typedef struct dapl_dto_cookie		DAPL_DTO_COOKIE;
typedef struct dapl_rmr_cookie		DAPL_RMR_COOKIE;

typedef struct dapl_private		DAPL_PRIVATE;



/*********************************************************************
 *                                                                   *
 * Structures                                                        *
 *                                                                   *
 *********************************************************************/

struct dapl_llist_entry
{
    struct dapl_llist_entry	*flink;
    struct dapl_llist_entry	*blink;
    void			*data;
    DAPL_LLIST_HEAD		*list_head; /* for consistency checking */
};

struct dapl_ring_buffer
{
    void		**base;		/* base of element array */
    DAT_COUNT		lim;		/* mask, number of entries - 1 */
    DAPL_ATOMIC		head;		/* head pointer index */
    DAPL_ATOMIC		tail;		/* tail pointer index */
};

struct dapl_cookie_buffer
{
    DAPL_COOKIE		*pool;
    DAT_COUNT		pool_size;
    DAPL_ATOMIC		head;
    DAPL_ATOMIC		tail;
};

#ifdef	IBAPI
#include "dapl_ibapi_util.h"
#elif VAPI
#include "dapl_vapi_util.h"
#elif __OPENIB__
#include "dapl_openib_util.h"
#include "dapl_openib_cm.h"
#elif DUMMY
#include "dapl_dummy_util.h"
#elif OPENIB
#include "dapl_ib_util.h"
#else /* windows - IBAL and/or IBAL+Sock_CM */
#include "dapl_ibal_util.h"
#endif

struct dapl_hca
{
    DAPL_OS_LOCK	lock;
    DAPL_LLIST_HEAD	ia_list_head;	   /* list of all open IAs */
    DAPL_ATOMIC		handle_ref_count;  /* count of ia_opens on handle */
    DAPL_EVD		*async_evd;
    DAPL_EVD		*async_error_evd;
    DAT_SOCK_ADDR6	hca_address;	   /* local address of HCA*/
    char	 	*name;		   /* provider name */
    ib_hca_handle_t	ib_hca_handle;
    unsigned long       port_num;	   /* physical port number */
    ib_hca_transport_t  ib_trans;	   /* Values specific transport API */
    /* Memory Subsystem Support */
    DAPL_HASH_TABLE 	*lmr_hash_table;
    /* Limits & useful HCA attributes */
    DAT_IA_ATTR		ia_attr;
};

/* DAPL Objects always have the following header */
struct dapl_header
{
    DAT_PROVIDER	*provider;	/* required by DAT - must be first */
    DAPL_MAGIC		magic;		/* magic number for verification */
    DAT_HANDLE_TYPE	handle_type;	/* struct type */
    DAPL_IA		*owner_ia;	/* ia which owns this stuct */
    DAPL_LLIST_ENTRY	ia_list_entry;	/* link entry on ia struct */
    DAT_CONTEXT		user_context;	/* user context - opaque to DAPL */
    DAPL_OS_LOCK	lock;		/* lock - in header for easier macros */
};

/* DAPL_IA maps to DAT_IA_HANDLE */
struct dapl_ia
{
    DAPL_HEADER		header;
    DAPL_HCA		*hca_ptr;
    DAPL_EVD		*async_error_evd;
    DAT_BOOLEAN		cleanup_async_error_evd;

    DAPL_LLIST_ENTRY	hca_ia_list_entry;	/* HCAs list of IAs */
    DAPL_LLIST_HEAD	ep_list_head;		/* EP queue */
    DAPL_LLIST_HEAD	lmr_list_head;		/* LMR queue */
    DAPL_LLIST_HEAD	rmr_list_head;		/* RMR queue */
    DAPL_LLIST_HEAD	pz_list_head;		/* PZ queue */
    DAPL_LLIST_HEAD	evd_list_head;		/* EVD queue */
    DAPL_LLIST_HEAD	cno_list_head;		/* CNO queue */
    DAPL_LLIST_HEAD	psp_list_head;		/* PSP queue */
    DAPL_LLIST_HEAD	rsp_list_head;		/* RSP queue */
    DAPL_LLIST_HEAD	srq_list_head;		/* SRQ queue */
#ifdef DAPL_COUNTERS
    void		*cntrs;
#endif
};

/* DAPL_CNO maps to DAT_CNO_HANDLE */
struct dapl_cno
{
    DAPL_HEADER	header;

    /* A CNO cannot be freed while it is referenced elsewhere.  */
    DAPL_ATOMIC			cno_ref_count;
    DAPL_CNO_STATE		cno_state;

    DAT_COUNT			cno_waiters;
    DAPL_EVD			*cno_evd_triggered;
#if defined(__KERNEL__)
    DAT_UPCALL_OBJECT		cno_upcall;
    DAT_UPCALL_POLICY		cno_upcall_policy;
#else
    DAT_OS_WAIT_PROXY_AGENT	cno_wait_agent;
#endif	/* defined(__KERNEL__) */

    DAPL_OS_WAIT_OBJECT		cno_wait_object;
};

/* DAPL_EVD maps to DAT_EVD_HANDLE */
struct dapl_evd
{
    DAPL_HEADER		header;

    DAPL_EVD_STATE	evd_state;
    DAT_EVD_FLAGS	evd_flags;
    DAT_BOOLEAN		evd_enabled; /* For attached CNO.  */
    DAT_BOOLEAN		evd_waitable; /* EVD state.  */

    /* Derived from evd_flags; see dapls_evd_internal_create.  */
    DAT_BOOLEAN		evd_producer_locking_needed;

    /* Every EVD has a CQ unless it is a SOFTWARE_EVENT only EVD */
    ib_cq_handle_t	ib_cq_handle;

    /* An Event Dispatcher cannot be freed while
     * it is referenced elsewhere.
     */
    DAPL_ATOMIC		evd_ref_count;

    /* Set if there has been a catastrophic overflow */
    DAT_BOOLEAN		catastrophic_overflow;

    /* the actual events */
    DAT_COUNT		qlen;
    DAT_EVENT		*events;
    DAPL_RING_BUFFER	free_event_queue;
    DAPL_RING_BUFFER	pending_event_queue;

    /* CQ Completions are not placed into 'deferred_events'
     ** rather they are simply left on the Completion Queue
     ** and the fact that there was a notification is flagged.
     */
    DAT_BOOLEAN		cq_notified;
    DAPL_OS_TICKS	cq_notified_when;

    DAT_COUNT		cno_active_count;
    DAPL_CNO		*cno_ptr;

    DAPL_OS_WAIT_OBJECT	wait_object;

    DAT_COUNT		threshold;
    DAPL_EVD_COMPLETION	completion_type;

#ifdef DAPL_COUNTERS
    void		*cntrs;
#endif
};

/* DAPL_PRIVATE used to pass private data in a connection */
struct dapl_private
{
#ifdef IBHOSTS_NAMING
    DAT_SOCK_ADDR6		hca_address;	/* local address of HCA*/
#endif /* IBHOSTS_NAMING */
    unsigned char		private_data[DAPL_MAX_PRIVATE_DATA_SIZE];
};

/* uDAPL timer entry, used to queue timeouts */
struct dapl_timer_entry
{
    DAPL_LLIST_ENTRY		list_entry;	/* link entry on ia struct */
    DAPL_OS_TIMEVAL		expires;
    void			(*function) (uintptr_t);
    void			*data;
};

#ifdef DAPL_DBG_IO_TRC

#define DBG_IO_TRC_QLEN   32		/* length of trace buffer        */
#define DBG_IO_TRC_IOV 3		/* iov elements we keep track of */

struct io_buf_track
{
    Ib_send_op_type 		op_type;
    DAPL_COOKIE			*cookie;
    DAT_LMR_TRIPLET		iov[DBG_IO_TRC_IOV];
    DAT_RMR_TRIPLET		remote_iov;
    unsigned int		done;	/* count to track completion ordering */
    int				status;
    void			*wqe;
};

#endif /* DAPL_DBG_IO_TRC */

/* DAPL_EP maps to DAT_EP_HANDLE */
struct dapl_ep
{
    DAPL_HEADER			header;
    /* What the DAT Consumer asked for */
    DAT_EP_PARAM		param;

    /* The RC Queue Pair (IBM OS API) */
    ib_qp_handle_t		qp_handle;
    unsigned int		qpn;	/* qp number */
    ib_qp_state_t		qp_state;

    /* communications manager handle (IBM OS API) */
    dp_ib_cm_handle_t		cm_handle;

    /* store the remote IA address here, reference from the param
     * struct which only has a pointer, no storage
     */
    DAT_SOCK_ADDR6		remote_ia_address;

    /* For passive connections we maintain a back pointer to the CR */
    void *			cr_ptr;

    /* pointer to connection timer, if set */
    struct dapl_timer_entry	*cxn_timer;

    /* private data container */
    DAPL_PRIVATE		private;

    /* DTO data */
    DAPL_ATOMIC			req_count;
    DAPL_ATOMIC			recv_count;

    DAPL_COOKIE_BUFFER	req_buffer;
    DAPL_COOKIE_BUFFER	recv_buffer;

#ifdef DAPL_DBG_IO_TRC
    int			ibt_dumped;
    struct io_buf_track *ibt_base;
    DAPL_RING_BUFFER	ibt_queue;
#endif /* DAPL_DBG_IO_TRC */
#if defined(_WIN32) || defined(_WIN64)
    DAT_BOOLEAN         recv_discreq;
    DAT_BOOLEAN         sent_discreq;
    dp_ib_cm_handle_t   ibal_cm_handle;
#endif
#ifdef DAPL_COUNTERS
    void		*cntrs;
#endif
};

/* DAPL_SRQ maps to DAT_SRQ_HANDLE */
struct dapl_srq
{
    DAPL_HEADER		header;
    DAT_SRQ_PARAM	param;
    DAPL_ATOMIC		srq_ref_count;
    DAPL_COOKIE_BUFFER	recv_buffer;
    DAPL_ATOMIC		recv_count;
};

/* DAPL_PZ maps to DAT_PZ_HANDLE */
struct dapl_pz
{
    DAPL_HEADER		header;
    ib_pd_handle_t	pd_handle;
    DAPL_ATOMIC		pz_ref_count;
};

/* DAPL_LMR maps to DAT_LMR_HANDLE */
struct dapl_lmr
{
    DAPL_HEADER		header;
    DAT_LMR_PARAM	param;
    ib_mr_handle_t	mr_handle;
    DAPL_ATOMIC		lmr_ref_count;
#if !defined(__KDAPL__)
    char		shmid[DAT_LMR_COOKIE_SIZE]; /* shared memory ID */
    ib_shm_transport_t	ib_trans; 	/* provider specific data */
#endif /* !__KDAPL__ */
};

/* DAPL_RMR maps to DAT_RMR_HANDLE */
struct dapl_rmr
{
    DAPL_HEADER		header;
    DAT_RMR_PARAM	param;
    DAPL_EP             *ep;
    DAPL_PZ             *pz;
    DAPL_LMR            *lmr;
    ib_mw_handle_t	mw_handle;
};

/* SP types, indicating the state and queue */
typedef enum dapl_sp_state
{
    DAPL_SP_STATE_FREE,
    DAPL_SP_STATE_PSP_LISTENING,
    DAPL_SP_STATE_PSP_PENDING,
    DAPL_SP_STATE_RSP_LISTENING,
    DAPL_SP_STATE_RSP_PENDING
} DAPL_SP_STATE;

/* DAPL_SP maps to DAT_PSP_HANDLE and DAT_RSP_HANDLE */
struct dapl_sp
{
    DAPL_HEADER		header;
    DAPL_SP_STATE	state;		/* type and queue of the SP */

    /* PSP/RSP PARAM fields */
    DAT_CONN_QUAL       conn_qual;
    DAT_EVD_HANDLE      evd_handle;
    DAT_PSP_FLAGS       psp_flags;
    DAT_EP_HANDLE       ep_handle;

     /* maintenence fields */
    DAT_BOOLEAN		listening;	/* PSP is registered & active */
    ib_cm_srvc_handle_t	cm_srvc_handle; /* Used by Mellanox CM */
    DAPL_LLIST_HEAD	cr_list_head;	/* CR pending queue */
    DAT_COUNT		cr_list_count;	/* count of CRs on queue */
#if defined(_VENDOR_IBAL_)
    DAPL_OS_WAIT_OBJECT wait_object;    /* cancel & destroy. */
#endif
};

/* DAPL_CR maps to DAT_CR_HANDLE */
struct dapl_cr
{
    DAPL_HEADER		header;

    /* for convenience the data is kept as a DAT_CR_PARAM.
     * however, the "local_endpoint" field is always NULL
     * so this wastes a pointer. This is probably ok to
     * simplify code, espedially dat_cr_query.
     */
    DAT_CR_PARAM	param;
    /* IB specific fields */
    dp_ib_cm_handle_t	ib_cm_handle;

    DAT_SOCK_ADDR6	remote_ia_address;
    /* Assuming that the maximum private data size is small.
     * If it gets large, use of a pointer may be appropriate.
     */
    unsigned char	private_data[DAPL_MAX_PRIVATE_DATA_SIZE];
    /*
     * Need to be able to associate the CR back to the PSP for
     * dapl_cr_reject.
     */
    DAPL_SP		*sp_ptr;
};

typedef enum dapl_dto_type
{
    DAPL_DTO_TYPE_SEND,
    DAPL_DTO_TYPE_RECV,
    DAPL_DTO_TYPE_RDMA_WRITE,
    DAPL_DTO_TYPE_RDMA_READ,
#ifdef DAT_EXTENSIONS
    DAPL_DTO_TYPE_EXTENSION,
#endif
} DAPL_DTO_TYPE;

typedef enum dapl_cookie_type
{
    DAPL_COOKIE_TYPE_NULL,
    DAPL_COOKIE_TYPE_DTO,
    DAPL_COOKIE_TYPE_RMR,
} DAPL_COOKIE_TYPE;

/* DAPL_DTO_COOKIE used as context for DTO WQEs */
struct dapl_dto_cookie
{
    DAPL_DTO_TYPE		type;
    DAT_DTO_COOKIE		cookie;
    DAT_COUNT			size;	/* used for SEND and RDMA write */
};

/* DAPL_RMR_COOKIE used as context for bind WQEs */
struct dapl_rmr_cookie
{
    DAPL_RMR			*rmr;
    DAT_RMR_COOKIE              cookie;
};

/* DAPL_COOKIE used as context for WQEs */
struct dapl_cookie
{
    DAPL_COOKIE_TYPE    	type; /* Must be first, to define struct.  */
    DAPL_EP			*ep;
    DAT_COUNT 			index;
    union
    {
	DAPL_DTO_COOKIE 	dto;
	DAPL_RMR_COOKIE		rmr;
    } val;
};

/*
 * Private Data operations. Used to obtain the size of the private
 * data from the provider layer.
 */
typedef enum dapl_private_data_op
{
    DAPL_PDATA_CONN_REQ  = 0,		/* connect request    */
    DAPL_PDATA_CONN_REP  = 1,		/* connect reply      */
    DAPL_PDATA_CONN_REJ  = 2,		/* connect reject     */
    DAPL_PDATA_CONN_DREQ = 3,		/* disconnect request */
    DAPL_PDATA_CONN_DREP = 4,		/* disconnect reply   */
} DAPL_PDATA_OP;


/*
 * Generic HCA name field
 */
#define DAPL_HCA_NAME_MAX_LEN 260
typedef char DAPL_HCA_NAME[DAPL_HCA_NAME_MAX_LEN+1];

#ifdef IBHOSTS_NAMING

/*
 * Simple mapping table to match IP addresses to GIDs. Loaded
 * by dapl_init.
 */
typedef struct _dapl_gid_map_table
{
    uint32_t		ip_address;
    ib_gid_t		gid;
} DAPL_GID_MAP;

#endif /* IBHOSTS_NAMING */

/*
 * IBTA defined reason for reject message: See IBTA 1.1 specification,
 * 12.6.7.2 REJECTION REASON section.
 */
#define IB_CM_REJ_REASON_CONSUMER_REJ            0x001C


#if defined(DAPL_DBG_IO_TRC)
/*********************************************************************
 *                                                                   *
 * Debug I/O tracing support prototypes                              *
 *                                                                   *
 *********************************************************************/
/*
 * I/O tracing support
 */
void dapls_io_trc_alloc (
    DAPL_EP			*ep_ptr);

void dapls_io_trc_update_completion (
    DAPL_EP			*ep_ptr,
    DAPL_COOKIE			*cookie,
    ib_uint32_t			ib_status );

void dapls_io_trc_dump (
    DAPL_EP			*ep_ptr,
    ib_work_completion_t	*cqe_ptr,
    ib_uint32_t			ib_status);

#else /* DAPL_DBG_IO_TRC */

#define dapls_io_trc_alloc(a)
#define dapls_io_trc_update_completion(a, b, c)
#define dapls_io_trc_dump(a, b, c)

#endif /* DAPL_DBG_IO_TRC */


/*********************************************************************
 *                                                                   *
 * Function Prototypes                                               *
 *                                                                   *
 *********************************************************************/

typedef void (*DAPL_CONNECTION_STATE_HANDLER) (
	IN	DAPL_EP *,
	IN	ib_cm_events_t,
	IN	const void *,
	OUT	DAT_EVENT *);

/*
 * DAT Mandated functions
 */

extern DAT_RETURN DAT_API dapl_ia_open (
	IN	const DAT_NAME_PTR,	/* name */
	IN	DAT_COUNT,		/* asynch_evd_qlen */
	INOUT	DAT_EVD_HANDLE *,	/* asynch_evd_handle */
	OUT	DAT_IA_HANDLE *);	/* ia_handle */

extern DAT_RETURN DAT_API dapl_ia_close (
	IN	DAT_IA_HANDLE,		/* ia_handle */
	IN	DAT_CLOSE_FLAGS );	/* ia_flags */


extern DAT_RETURN DAT_API dapl_ia_query (
	IN	DAT_IA_HANDLE,		/* ia handle */
	OUT	DAT_EVD_HANDLE *,	/* async_evd_handle */
	IN	DAT_IA_ATTR_MASK,	/* ia_params_mask */
	OUT	DAT_IA_ATTR *,		/* ia_params */
	IN	DAT_PROVIDER_ATTR_MASK, /* provider_params_mask */
	OUT	DAT_PROVIDER_ATTR * );	/* provider_params */


/* helper functions */

extern DAT_RETURN DAT_API dapl_set_consumer_context (
	IN	DAT_HANDLE,			/* dat handle */
	IN	DAT_CONTEXT);			/* context */

extern DAT_RETURN DAT_API dapl_get_consumer_context (
	IN	DAT_HANDLE,			/* dat handle */
	OUT	DAT_CONTEXT * );		/* context */

extern DAT_RETURN DAT_API dapl_get_handle_type (
	IN	DAT_HANDLE,
	OUT	DAT_HANDLE_TYPE * );

/* CNO functions */

#if !defined(__KERNEL__)
extern DAT_RETURN DAT_API dapl_cno_create (
	IN	DAT_IA_HANDLE,			/* ia_handle */
	IN	DAT_OS_WAIT_PROXY_AGENT,	/* agent */
	OUT	DAT_CNO_HANDLE *);		/* cno_handle */

extern DAT_RETURN DAT_API dapl_cno_modify_agent (
	IN	DAT_CNO_HANDLE,			/* cno_handle */
	IN	DAT_OS_WAIT_PROXY_AGENT);	/* agent */

extern DAT_RETURN DAT_API dapl_cno_query (
	IN	DAT_CNO_HANDLE,		/* cno_handle */
	IN	DAT_CNO_PARAM_MASK,	/* cno_param_mask */
	OUT	DAT_CNO_PARAM * );	/* cno_param */

extern DAT_RETURN DAT_API dapl_cno_free (
	IN	DAT_CNO_HANDLE);	/* cno_handle */

extern DAT_RETURN DAT_API dapl_cno_wait (
	IN	DAT_CNO_HANDLE,		/* cno_handle */
	IN	DAT_TIMEOUT,		/* timeout */
	OUT	DAT_EVD_HANDLE *);	/* evd_handle */

extern DAT_RETURN DAT_API dapl_cno_fd_create (
	IN 	DAT_IA_HANDLE,		/* ia_handle            */
	OUT	DAT_FD *,		/* file_descriptor	*/
	OUT 	DAT_CNO_HANDLE *);	/* cno_handle           */

extern DAT_RETURN DAT_API dapl_cno_trigger (
	IN	DAT_CNO_HANDLE,		/* cno_handle */
	OUT	DAT_EVD_HANDLE *);	/* evd_handle */

#endif	/* !defined(__KERNEL__) */

/* CR Functions */

extern DAT_RETURN DAT_API dapl_cr_query (
	IN	DAT_CR_HANDLE,		/* cr_handle */
	IN	DAT_CR_PARAM_MASK,	/* cr_args_mask */
	OUT	DAT_CR_PARAM * );	/* cwr_args */

extern DAT_RETURN DAT_API dapl_cr_accept (
	IN	DAT_CR_HANDLE,		/* cr_handle */
	IN	DAT_EP_HANDLE,		/* ep_handle */
	IN	DAT_COUNT,		/* private_data_size */
	IN	const DAT_PVOID );	/* private_data */

extern DAT_RETURN DAT_API dapl_cr_reject (
	IN      DAT_CR_HANDLE, 		/* cr_handle            */
	IN	DAT_COUNT,		/* private_data_size	*/
	IN	const DAT_PVOID );      /* private_data         */

extern DAT_RETURN DAT_API dapl_cr_handoff (
	IN DAT_CR_HANDLE,		/* cr_handle */
	IN DAT_CONN_QUAL);		/* handoff */

/* EVD Functions */

#if defined(__KERNEL__)
extern DAT_RETURN DAT_API dapl_ia_memtype_hint (
	IN    DAT_IA_HANDLE,		/* ia_handle */
	IN    DAT_MEM_TYPE,		/* mem_type */
	IN    DAT_VLEN,			/* length */
	IN    DAT_MEM_OPT,		/* mem_optimization */
	OUT   DAT_VLEN *,		/* suggested_length */
	OUT   DAT_VADDR	*);		/* suggested_alignment */

extern DAT_RETURN DAT_API dapl_evd_kcreate (
	IN	DAT_IA_HANDLE,		/* ia_handle */
	IN	DAT_COUNT,		/* evd_min_qlen */
	IN	DAT_UPCALL_POLICY,	/* upcall_policy */
	IN	const DAT_UPCALL_OBJECT *, /* upcall */
	IN	DAT_EVD_FLAGS,		/* evd_flags */
	OUT	DAT_EVD_HANDLE * );	/* evd_handle */

extern DAT_RETURN DAT_API dapl_evd_kquery (
	IN	DAT_EVD_HANDLE,		/* evd_handle */
	IN	DAT_EVD_PARAM_MASK,	/* evd_args_mask */
	OUT	DAT_EVD_PARAM * );	/* evd_args */

#else
extern DAT_RETURN DAT_API dapl_evd_create (
	IN	DAT_IA_HANDLE,		/* ia_handle */
	IN	DAT_COUNT,		/* evd_min_qlen */
	IN	DAT_CNO_HANDLE,		/* cno_handle */
	IN	DAT_EVD_FLAGS,		/* evd_flags */
	OUT	DAT_EVD_HANDLE * );	/* evd_handle */

extern DAT_RETURN DAT_API dapl_evd_query (
	IN	DAT_EVD_HANDLE,		/* evd_handle */
	IN	DAT_EVD_PARAM_MASK,	/* evd_args_mask */
	OUT	DAT_EVD_PARAM * );	/* evd_args */
#endif	/* defined(__KERNEL__) */

#if defined(__KERNEL__)
extern DAT_RETURN DAT_API dapl_evd_modify_upcall (
	IN	DAT_EVD_HANDLE,		/* evd_handle */
	IN	DAT_UPCALL_POLICY,	/* upcall_policy */
	IN	const DAT_UPCALL_OBJECT * ); /* upcall */

#else

extern DAT_RETURN DAT_API dapl_evd_modify_cno (
	IN	DAT_EVD_HANDLE,		/* evd_handle */
	IN	DAT_CNO_HANDLE);	/* cno_handle */
#endif

extern DAT_RETURN DAT_API dapl_evd_enable (
	IN	DAT_EVD_HANDLE);	/* evd_handle */

extern DAT_RETURN DAT_API dapl_evd_disable (
	IN	DAT_EVD_HANDLE);	/* evd_handle */

#if !defined(__KERNEL__)
extern DAT_RETURN DAT_API dapl_evd_wait (
	IN	DAT_EVD_HANDLE,		/* evd_handle */
	IN	DAT_TIMEOUT,		/* timeout */
	IN	DAT_COUNT,		/* threshold */
	OUT	DAT_EVENT *,		/* event */
	OUT	DAT_COUNT *);		/* nmore */
#endif	/* !defined(__KERNEL__) */

extern DAT_RETURN DAT_API dapl_evd_resize (
	IN	DAT_EVD_HANDLE,		/* evd_handle */
	IN	DAT_COUNT );		/* evd_qlen */

extern DAT_RETURN DAT_API dapl_evd_post_se (
	DAT_EVD_HANDLE,			/* evd_handle */
	const DAT_EVENT * );		/* event */

extern DAT_RETURN DAT_API dapl_evd_dequeue (
	IN	DAT_EVD_HANDLE,		/* evd_handle */
	OUT	DAT_EVENT * );		/* event */

extern DAT_RETURN DAT_API dapl_evd_free (
	IN	DAT_EVD_HANDLE );

extern DAT_RETURN DAT_API
dapl_evd_set_unwaitable (
	IN	DAT_EVD_HANDLE	evd_handle );

extern DAT_RETURN DAT_API
dapl_evd_clear_unwaitable (
	IN	DAT_EVD_HANDLE	evd_handle );

/* EP functions */

extern DAT_RETURN DAT_API dapl_ep_create (
	IN	DAT_IA_HANDLE,		/* ia_handle */
	IN	DAT_PZ_HANDLE,		/* pz_handle */
	IN	DAT_EVD_HANDLE,		/* in_dto_completion_evd_handle */
	IN	DAT_EVD_HANDLE,		/* out_dto_completion_evd_handle */
	IN	DAT_EVD_HANDLE,		/* connect_evd_handle */
	IN	const DAT_EP_ATTR *,	/* ep_parameters */
	OUT	DAT_EP_HANDLE * );	/* ep_handle */

extern DAT_RETURN DAT_API dapl_ep_query (
	IN	DAT_EP_HANDLE,		/* ep_handle */
	IN	DAT_EP_PARAM_MASK,	/* ep_args_mask */
	OUT	DAT_EP_PARAM * );	/* ep_args */

extern DAT_RETURN DAT_API dapl_ep_modify (
	IN	DAT_EP_HANDLE,		/* ep_handle */
	IN	DAT_EP_PARAM_MASK,	/* ep_args_mask */
	IN	const DAT_EP_PARAM * ); /* ep_args */

extern DAT_RETURN DAT_API dapl_ep_connect (
	IN	DAT_EP_HANDLE,		/* ep_handle */
	IN	DAT_IA_ADDRESS_PTR,	/* remote_ia_address */
	IN	DAT_CONN_QUAL,		/* remote_conn_qual */
	IN	DAT_TIMEOUT,		/* timeout */
	IN	DAT_COUNT,		/* private_data_size */
	IN	const DAT_PVOID,	/* private_data  */
	IN	DAT_QOS,		/* quality_of_service */
	IN	DAT_CONNECT_FLAGS );	/* connect_flags */

extern DAT_RETURN DAT_API dapl_ep_common_connect (
	IN      DAT_EP_HANDLE ep,		/* ep_handle            */
	IN      DAT_IA_ADDRESS_PTR remote_addr,	/* remote_ia_address    */
	IN      DAT_TIMEOUT timeout,		/* timeout              */
	IN      DAT_COUNT pdata_size,		/* private_data_size    */
	IN      const DAT_PVOID pdata	);	/* private_data         */

extern DAT_RETURN DAT_API dapl_ep_dup_connect (
	IN	DAT_EP_HANDLE,		/* ep_handle */
	IN	DAT_EP_HANDLE,		/* ep_dup_handle */
	IN	DAT_TIMEOUT,		/* timeout*/
	IN	DAT_COUNT,		/* private_data_size */
	IN	const DAT_PVOID,	/* private_data */
	IN	DAT_QOS);		/* quality_of_service */

extern DAT_RETURN DAT_API dapl_ep_disconnect (
	IN	DAT_EP_HANDLE,		/* ep_handle */
	IN	DAT_CLOSE_FLAGS );	/* close_flags */

extern DAT_RETURN DAT_API dapl_ep_post_send (
	IN	DAT_EP_HANDLE,		/* ep_handle */
	IN	DAT_COUNT,		/* num_segments */
	IN	DAT_LMR_TRIPLET *,	/* local_iov */
	IN	DAT_DTO_COOKIE,		/* user_cookie */
	IN	DAT_COMPLETION_FLAGS ); /* completion_flags */

extern DAT_RETURN DAT_API dapl_ep_post_recv (
	IN	DAT_EP_HANDLE,		/* ep_handle */
	IN	DAT_COUNT,		/* num_segments */
	IN	DAT_LMR_TRIPLET *,	/* local_iov */
	IN	DAT_DTO_COOKIE,		/* user_cookie */
	IN	DAT_COMPLETION_FLAGS ); /* completion_flags */

extern DAT_RETURN DAT_API dapl_ep_post_rdma_read (
	IN	DAT_EP_HANDLE,		 /* ep_handle */
	IN	DAT_COUNT,		 /* num_segments */
	IN	DAT_LMR_TRIPLET *,	 /* local_iov */
	IN	DAT_DTO_COOKIE,		 /* user_cookie */
	IN	const DAT_RMR_TRIPLET *, /* remote_iov */
	IN	DAT_COMPLETION_FLAGS );	 /* completion_flags */

extern DAT_RETURN DAT_API dapl_ep_post_rdma_read_to_rmr (
	IN      DAT_EP_HANDLE,	        /* ep_handle            */
	IN      const DAT_RMR_TRIPLET *,/* local_iov            */
	IN      DAT_DTO_COOKIE,		/* user_cookie          */
	IN      const DAT_RMR_TRIPLET *,/* remote_iov           */
	IN      DAT_COMPLETION_FLAGS);	/* completion_flags     */

extern DAT_RETURN DAT_API dapl_ep_post_rdma_write (
	IN	DAT_EP_HANDLE,		 /* ep_handle */
	IN	DAT_COUNT,		 /* num_segments */
	IN	DAT_LMR_TRIPLET *,	 /* local_iov */
	IN	DAT_DTO_COOKIE,		 /* user_cookie */
	IN	const DAT_RMR_TRIPLET *, /* remote_iov */
	IN	DAT_COMPLETION_FLAGS );	 /* completion_flags */

extern DAT_RETURN DAT_API dapl_ep_post_send_with_invalidate (
	IN      DAT_EP_HANDLE,          /* ep_handle            */
	IN      DAT_COUNT,              /* num_segments         */
	IN      DAT_LMR_TRIPLET *,      /* local_iov            */
	IN      DAT_DTO_COOKIE,         /* user_cookie          */
	IN      DAT_COMPLETION_FLAGS,   /* completion_flags     */
	IN      DAT_BOOLEAN,            /* invalidate_flag      */
	IN      DAT_RMR_CONTEXT);      /* RMR context          */

extern DAT_RETURN DAT_API dapl_ep_get_status (
	IN	DAT_EP_HANDLE,		/* ep_handle */
	OUT	DAT_EP_STATE *,		/* ep_state */
	OUT	DAT_BOOLEAN *,		/* in_dto_idle */
	OUT	DAT_BOOLEAN * );	/* out_dto_idle */

extern DAT_RETURN DAT_API dapl_ep_free (
	IN	DAT_EP_HANDLE);		/* ep_handle */

extern DAT_RETURN DAT_API dapl_ep_reset (
	IN	DAT_EP_HANDLE);		/* ep_handle */

extern DAT_RETURN DAT_API dapl_ep_create_with_srq (
        IN      DAT_IA_HANDLE,          /* ia_handle            */
        IN      DAT_PZ_HANDLE,          /* pz_handle            */
        IN      DAT_EVD_HANDLE,         /* recv_evd_handle      */
        IN      DAT_EVD_HANDLE,         /* request_evd_handle   */
        IN      DAT_EVD_HANDLE,         /* connect_evd_handle   */
        IN      DAT_SRQ_HANDLE,         /* srq_handle           */
        IN      const DAT_EP_ATTR *,    /* ep_attributes        */
        OUT     DAT_EP_HANDLE *);       /* ep_handle            */

extern DAT_RETURN DAT_API dapl_ep_recv_query (
        IN      DAT_EP_HANDLE,          /* ep_handle            */
        OUT     DAT_COUNT *,            /* nbufs_allocated      */
        OUT     DAT_COUNT *);           /* bufs_alloc_span      */

extern DAT_RETURN DAT_API dapl_ep_set_watermark (
        IN      DAT_EP_HANDLE,          /* ep_handle            */
        IN      DAT_COUNT,              /* soft_high_watermark  */
        IN      DAT_COUNT);             /* hard_high_watermark  */

/* LMR functions */

#if defined(__KERNEL__)
extern DAT_RETURN DAT_API dapl_lmr_kcreate (
	IN	DAT_IA_HANDLE,		/* ia_handle */
	IN	DAT_MEM_TYPE,		/* mem_type */
	IN	DAT_REGION_DESCRIPTION, /* region_description */
	IN	DAT_VLEN,		/* length */
	IN	DAT_PZ_HANDLE,		/* pz_handle */
	IN	DAT_MEM_PRIV_FLAGS,	/* privileges */
	IN	DAT_VA_TYPE,		/* va_type */
	IN	DAT_MEM_OPT,		/* optimization */
	OUT	DAT_LMR_HANDLE *,	/* lmr_handle */
	OUT	DAT_LMR_CONTEXT *,	/* lmr_context */
	OUT     DAT_RMR_CONTEXT *,	/* rmr_context          */
	OUT	DAT_VLEN *,		/* registered_length */
	OUT	DAT_VADDR * );		/* registered_address */
#else
extern DAT_RETURN DAT_API dapl_lmr_create (
	IN	DAT_IA_HANDLE,		/* ia_handle */
	IN	DAT_MEM_TYPE,		/* mem_type */
	IN	DAT_REGION_DESCRIPTION, /* region_description */
	IN	DAT_VLEN,		/* length */
	IN	DAT_PZ_HANDLE,		/* pz_handle */
	IN	DAT_MEM_PRIV_FLAGS,	/* privileges */
	IN	DAT_VA_TYPE,		/* va_type */
	OUT	DAT_LMR_HANDLE *,	/* lmr_handle */
	OUT	DAT_LMR_CONTEXT *,	/* lmr_context */
	OUT     DAT_RMR_CONTEXT *,	/* rmr_context          */
	OUT	DAT_VLEN *,		/* registered_length */
	OUT	DAT_VADDR * );		/* registered_address */
#endif	/* defined(__KERNEL__) */

extern DAT_RETURN DAT_API dapl_lmr_query (
	IN	DAT_LMR_HANDLE,
	IN	DAT_LMR_PARAM_MASK,
	OUT	DAT_LMR_PARAM *);

extern DAT_RETURN DAT_API dapl_lmr_free (
	IN	DAT_LMR_HANDLE);

extern DAT_RETURN DAT_API dapl_lmr_sync_rdma_read (
	IN      DAT_IA_HANDLE,          /* ia_handle            */
	IN      const DAT_LMR_TRIPLET *, /* local_segments      */
	IN      DAT_VLEN);              /* num_segments         */

extern DAT_RETURN DAT_API dapl_lmr_sync_rdma_write (
	IN      DAT_IA_HANDLE,          /* ia_handle            */
	IN      const DAT_LMR_TRIPLET *, /* local_segments      */
	IN      DAT_VLEN);              /* num_segments         */

/* RMR Functions */

extern DAT_RETURN DAT_API dapl_rmr_create (
	IN	DAT_PZ_HANDLE,		/* pz_handle */
	OUT	DAT_RMR_HANDLE *);	/* rmr_handle */

extern DAT_RETURN DAT_API dapl_rmr_create_for_ep (
	IN      DAT_PZ_HANDLE pz_handle,	/* pz_handle    */
	OUT     DAT_RMR_HANDLE *rmr_handle);	/* rmr_handle   */

extern DAT_RETURN DAT_API dapl_rmr_query (
	IN	DAT_RMR_HANDLE,		/* rmr_handle */
	IN	DAT_RMR_PARAM_MASK,	/* rmr_args_mask */
	OUT	DAT_RMR_PARAM *);	/* rmr_args */

extern DAT_RETURN DAT_API dapl_rmr_bind (
	IN	DAT_RMR_HANDLE,		 /* rmr_handle */
	IN	DAT_LMR_HANDLE,		 /* lmr_handle */
	IN	const DAT_LMR_TRIPLET *, /* lmr_triplet */
	IN	DAT_MEM_PRIV_FLAGS,	 /* mem_priv */
	IN	DAT_VA_TYPE,		 /* va_type */
	IN	DAT_EP_HANDLE,		 /* ep_handle */
	IN	DAT_RMR_COOKIE,		 /* user_cookie */
	IN	DAT_COMPLETION_FLAGS,	 /* completion_flags */
	INOUT	DAT_RMR_CONTEXT * );	 /* context */

extern DAT_RETURN DAT_API dapl_rmr_free (
	IN	DAT_RMR_HANDLE);

/* PSP Functions */

extern DAT_RETURN DAT_API dapl_psp_create (
	IN	DAT_IA_HANDLE,		/* ia_handle */
	IN	DAT_CONN_QUAL,		/* conn_qual */
	IN	DAT_EVD_HANDLE,		/* evd_handle */
	IN	DAT_PSP_FLAGS,		/* psp_flags */
	OUT	DAT_PSP_HANDLE * );	/* psp_handle */

extern DAT_RETURN DAT_API dapl_psp_create_any (
	IN	DAT_IA_HANDLE,		/* ia_handle */
	OUT	DAT_CONN_QUAL *,	/* conn_qual */
	IN	DAT_EVD_HANDLE,		/* evd_handle */
	IN	DAT_PSP_FLAGS,		/* psp_flags */
	OUT	DAT_PSP_HANDLE *);	/* psp_handle */

extern DAT_RETURN DAT_API dapl_psp_query (
	IN	DAT_PSP_HANDLE,
	IN	DAT_PSP_PARAM_MASK,
	OUT	DAT_PSP_PARAM * );

extern DAT_RETURN DAT_API dapl_psp_free (
	IN	DAT_PSP_HANDLE );	/* psp_handle */

/* RSP Functions */

extern DAT_RETURN DAT_API dapl_rsp_create (
	IN	DAT_IA_HANDLE,		/* ia_handle */
	IN	DAT_CONN_QUAL,		/* conn_qual */
	IN	DAT_EP_HANDLE,		/* ep_handle */
	IN	DAT_EVD_HANDLE,		/* evd_handle */
	OUT	DAT_RSP_HANDLE * );	/* rsp_handle */

extern DAT_RETURN DAT_API dapl_rsp_query (
	IN	DAT_RSP_HANDLE,
	IN	DAT_RSP_PARAM_MASK,
	OUT	DAT_RSP_PARAM * );

extern DAT_RETURN DAT_API dapl_rsp_free (
	IN	DAT_RSP_HANDLE );	/* rsp_handle */

/* PZ Functions */

extern DAT_RETURN DAT_API dapl_pz_create (
	IN	DAT_IA_HANDLE,		/* ia_handle */
	OUT	DAT_PZ_HANDLE * );	/* pz_handle */

extern DAT_RETURN DAT_API dapl_pz_query (
	IN	DAT_PZ_HANDLE,		/* pz_handle */
	IN	DAT_PZ_PARAM_MASK,	/* pz_args_mask */
	OUT	DAT_PZ_PARAM * );	/* pz_args */

extern DAT_RETURN DAT_API dapl_pz_free (
	IN	DAT_PZ_HANDLE );	/* pz_handle */

/* SRQ functions */

extern DAT_RETURN DAT_API dapl_srq_create (
        IN      DAT_IA_HANDLE,          /* ia_handle            */
        IN      DAT_PZ_HANDLE,          /* pz_handle            */
        IN      DAT_SRQ_ATTR *,         /* srq_attr             */
        OUT     DAT_SRQ_HANDLE *);      /* srq_handle           */

extern DAT_RETURN DAT_API dapl_srq_free (
	IN      DAT_SRQ_HANDLE);        /* srq_handle           */

extern DAT_RETURN DAT_API dapl_srq_post_recv (
	IN      DAT_SRQ_HANDLE,         /* srq_handle           */
	IN      DAT_COUNT,              /* num_segments         */
	IN      DAT_LMR_TRIPLET *,      /* local_iov            */
	IN      DAT_DTO_COOKIE);        /* user_cookie          */

extern DAT_RETURN DAT_API dapl_srq_query (
	IN      DAT_SRQ_HANDLE,         /* srq_handle           */
	IN      DAT_SRQ_PARAM_MASK,     /* srq_param_mask       */
	OUT     DAT_SRQ_PARAM *);       /* srq_param            */

extern DAT_RETURN DAT_API dapl_srq_resize (
	IN      DAT_SRQ_HANDLE,         /* srq_handle           */
	IN      DAT_COUNT);             /* srq_max_recv_dto     */

extern DAT_RETURN DAT_API dapl_srq_set_lw (
	IN      DAT_SRQ_HANDLE,         /* srq_handle           */
	IN      DAT_COUNT);             /* low_watermark        */

/* CSP functions */
extern DAT_RETURN DAT_API dapl_csp_create (
	IN      DAT_IA_HANDLE,          /* ia_handle      */
	IN      DAT_COMM *,             /* communicator   */
	IN      DAT_IA_ADDRESS_PTR,     /* address        */
	IN      DAT_EVD_HANDLE,         /* evd_handle     */
	OUT     DAT_CSP_HANDLE *);      /* csp_handle     */

extern DAT_RETURN DAT_API dapl_csp_query (
	IN      DAT_CSP_HANDLE,         /* csp_handle     */
	IN      DAT_CSP_PARAM_MASK,     /* csp_param_mask */
	OUT     DAT_CSP_PARAM *);       /* csp_param      */

extern DAT_RETURN DAT_API dapl_csp_free (
	IN      DAT_CSP_HANDLE);        /* csp_handle     */

/* HA functions */
DAT_RETURN DAT_API dapl_ia_ha (
	IN	 DAT_IA_HANDLE,         /* ia_handle */
	IN const DAT_NAME_PTR,          /* provider  */
	OUT	 DAT_BOOLEAN *);        /* answer    */

#ifdef DAT_EXTENSIONS
#include <stdarg.h>
extern DAT_RETURN DAT_API dapl_extensions (
	IN	DAT_HANDLE,		/* handle */
	IN	DAT_EXTENDED_OP,	/* extended op */
	IN	va_list);		/* argument list */
#endif

/*
 * DAPL internal utility function prototpyes
 */

extern void dapl_llist_init_head (
    DAPL_LLIST_HEAD * 	head);

extern void dapl_llist_init_entry (
    DAPL_LLIST_ENTRY * 	entry);

extern DAT_BOOLEAN dapl_llist_is_empty (
    DAPL_LLIST_HEAD * 	head);

extern void dapl_llist_add_head (
    DAPL_LLIST_HEAD * 	head,
    DAPL_LLIST_ENTRY * 	entry,
    void * 		data);

extern void dapl_llist_add_tail (
    DAPL_LLIST_HEAD *   head,
    DAPL_LLIST_ENTRY *  entry,
    void * 		data);

extern void dapl_llist_add_entry (
    DAPL_LLIST_HEAD * head,
    DAPL_LLIST_ENTRY * entry,
    DAPL_LLIST_ENTRY * new_entry,
    void * data);

extern void * dapl_llist_remove_head (
    DAPL_LLIST_HEAD *	head);

extern void * dapl_llist_remove_tail (
    DAPL_LLIST_HEAD *	head);

extern void * dapl_llist_remove_entry (
    DAPL_LLIST_HEAD *	head,
    DAPL_LLIST_ENTRY *	entry);

extern void * dapl_llist_peek_head (
    DAPL_LLIST_HEAD *	head);

extern void * dapl_llist_next_entry (
    IN    DAPL_LLIST_HEAD 	*head,
    IN    DAPL_LLIST_ENTRY 	*cur_ent);

extern void dapl_llist_debug_print_list (
    DAPL_LLIST_HEAD *	head);


#endif
