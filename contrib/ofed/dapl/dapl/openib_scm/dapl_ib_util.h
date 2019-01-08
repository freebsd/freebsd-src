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

#ifndef _DAPL_IB_UTIL_H_
#define _DAPL_IB_UTIL_H_
#define _OPENIB_SCM_ 

#include <infiniband/verbs.h>
#include "openib_osd.h"
#include "dapl_ib_common.h"

struct ib_cm_handle
{ 
	struct dapl_llist_entry	entry;
	DAPL_OS_LOCK		lock;
	int			state;
	DAPL_SOCKET		socket;
	struct dapl_hca		*hca;
	struct dapl_sp		*sp;	
	struct dapl_ep 		*ep;
	ib_cm_msg_t		msg;
	struct ibv_ah		*ah;
	DAT_SOCK_ADDR6          addr; 
};

typedef struct ib_cm_handle	*dp_ib_cm_handle_t;
typedef dp_ib_cm_handle_t	ib_cm_srvc_handle_t;

/* Definitions */
#define IB_INVALID_HANDLE	NULL

/* inline send rdma threshold */
#define	INLINE_SEND_DEFAULT	200

/* RC timer - retry count defaults */
#define SCM_ACK_TIMER 16 /* 5 bits, 4.096us*2^ack_timer. 16== 268ms */
#define SCM_ACK_RETRY 7  /* 3 bits, 7 * 268ms = 1.8 seconds */
#define SCM_RNR_TIMER 12 /* 5 bits, 12 =.64ms, 28 =163ms, 31 =491ms */
#define SCM_RNR_RETRY 7  /* 3 bits, 7 == infinite */
#define SCM_IB_MTU    2048

/* Global routing defaults */
#define SCM_GLOBAL	0	/* global routing is disabled */
#define SCM_HOP_LIMIT	0xff
#define SCM_TCLASS	0

/* ib_hca_transport_t, specific to this implementation */
typedef struct _ib_hca_transport
{ 
	struct dapl_llist_entry	entry;
	int			destroy;
	union ibv_gid		gid;
	struct	ibv_device	*ib_dev;
	struct	ibv_context	*ib_ctx;
	ib_cq_handle_t		ib_cq_empty;
	DAPL_OS_LOCK		cq_lock;	
	int			max_inline_send;
	ib_thread_state_t       cq_state;
	DAPL_OS_THREAD		cq_thread;
	struct ibv_comp_channel *ib_cq;
	int			cr_state;
	DAPL_OS_THREAD		thread;
	DAPL_OS_LOCK		lock;	
	struct dapl_llist_entry	*list;	
	ib_async_handler_t	async_unafiliated;
	void			*async_un_ctx;
	ib_async_cq_handler_t	async_cq_error;
	ib_async_dto_handler_t	async_cq;
	ib_async_qp_handler_t	async_qp_error;
	int			rd_atom_in;
	int			rd_atom_out;
	uint16_t		lid;
	uint8_t			ack_timer;
	uint8_t			ack_retry;
	uint8_t			rnr_timer;
	uint8_t			rnr_retry;
	uint8_t			global;
	uint8_t			hop_limit;
	uint8_t			tclass;
	uint8_t			mtu;
	DAT_NAMED_ATTR		named_attr;
	DAPL_SOCKET		scm[2];
} ib_hca_transport_t;

/* prototypes */
void cr_thread(void *arg);
int dapli_cq_thread_init(struct dapl_hca *hca_ptr);
void dapli_cq_thread_destroy(struct dapl_hca *hca_ptr);
void dapli_async_event_cb(struct _ib_hca_transport *tp);
void dapli_cq_event_cb(struct _ib_hca_transport *tp);
DAT_RETURN dapli_socket_disconnect(dp_ib_cm_handle_t cm_ptr);
dp_ib_cm_handle_t dapls_ib_cm_create(DAPL_EP *ep);
void dapls_ib_cm_free(dp_ib_cm_handle_t cm, DAPL_EP *ep);
void dapls_print_cm_list(IN DAPL_IA *ia_ptr);

#endif /*  _DAPL_IB_UTIL_H_ */
