/*
 * Copyright (c) 2002-2003, Network Appliance, Inc. All rights reserved.
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
 * MODULE: dapl_ia_util.c
 *
 * PURPOSE: Manage IA Info structure
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_hca_util.h"
#include "dapl_ia_util.h"
#include "dapl_sp_util.h"
#include "dapl_evd_util.h"
#include "dapl_cno_util.h"
#include "dapl_cr_util.h"
#include "dapl_adapter_util.h"

/* Internal prototype */
void dapli_ia_release_hca(DAPL_HCA * hca_ptr);

/*
 * dapl_ia_alloc
 *
 * alloc and initialize an IA INFO struct
 *
 * Input:
 * 	none
 *
 * Output:
 * 	ia_ptr
 *
 * Returns:
 * 	none
 *
 */
DAPL_IA *dapl_ia_alloc(DAT_PROVIDER * provider, DAPL_HCA * hca_ptr)
{
	DAPL_IA *ia_ptr;

	/* Allocate IA */
	ia_ptr = (DAPL_IA *) dapl_os_alloc(sizeof(DAPL_IA));
	if (ia_ptr == NULL) {
		return (NULL);
	}

	/* zero the structure */
	dapl_os_memzero(ia_ptr, sizeof(DAPL_IA));

#ifdef DAPL_COUNTERS
	/* Allocate counters */
	ia_ptr->cntrs =
	    dapl_os_alloc(sizeof(DAT_UINT64) * DCNT_IA_ALL_COUNTERS);
	if (ia_ptr->cntrs == NULL) {
		dapl_os_free(ia_ptr, sizeof(DAPL_IA));
		return (NULL);
	}
	dapl_os_memzero(ia_ptr->cntrs,
			sizeof(DAT_UINT64) * DCNT_IA_ALL_COUNTERS);
#endif				/* DAPL_COUNTERS */

	/*
	 * initialize the header
	 */
	ia_ptr->header.provider = provider;
	ia_ptr->header.magic = DAPL_MAGIC_IA;
	ia_ptr->header.handle_type = DAT_HANDLE_TYPE_IA;
	ia_ptr->header.owner_ia = ia_ptr;
	ia_ptr->header.user_context.as_64 = 0;
	ia_ptr->header.user_context.as_ptr = NULL;
	dapl_llist_init_entry(&ia_ptr->header.ia_list_entry);
	dapl_os_lock_init(&ia_ptr->header.lock);

	/*
	 * initialize the body
	 */
	ia_ptr->hca_ptr = hca_ptr;
	ia_ptr->async_error_evd = NULL;
	ia_ptr->cleanup_async_error_evd = DAT_FALSE;
	dapl_llist_init_entry(&ia_ptr->hca_ia_list_entry);
	dapl_llist_init_head(&ia_ptr->ep_list_head);
	dapl_llist_init_head(&ia_ptr->lmr_list_head);
	dapl_llist_init_head(&ia_ptr->rmr_list_head);
	dapl_llist_init_head(&ia_ptr->pz_list_head);
	dapl_llist_init_head(&ia_ptr->evd_list_head);
	dapl_llist_init_head(&ia_ptr->cno_list_head);
	dapl_llist_init_head(&ia_ptr->rsp_list_head);
	dapl_llist_init_head(&ia_ptr->psp_list_head);

	dapl_hca_link_ia(hca_ptr, ia_ptr);

	return (ia_ptr);
}

/*
 * dapl_ia_abrupt_close
 *
 * Performs an abrupt close of the IA
 *
 * Input:
 * 	ia_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	status
 *
 */

DAT_RETURN dapl_ia_abrupt_close(IN DAPL_IA * ia_ptr)
{
	DAT_RETURN dat_status;
	DAPL_EP *ep_ptr, *next_ep_ptr;
	DAPL_LMR *lmr_ptr, *next_lmr_ptr;
	DAPL_RMR *rmr_ptr, *next_rmr_ptr;
	DAPL_PZ *pz_ptr, *next_pz_ptr;
	DAPL_EVD *evd_ptr, *next_evd_ptr;
	DAPL_CNO *cno_ptr, *next_cno_ptr;
	DAPL_SP *sp_ptr, *next_sp_ptr;	/* for PSP and RSP queues */
	DAPL_CR *cr_ptr, *next_cr_ptr;
	DAPL_HCA *hca_ptr;

	dat_status = DAT_SUCCESS;

	/*
	 * clear all the data structures associated with the IA.
	 * this must be done in order (rmr,rsp) before (ep lmr psp) before
	 * (pz evd)
	 *
	 * Note that in all the following we can leave the loop either
	 * when we run out of entries, or when we get back to the head
	 * if we end up skipping an entry.
	 */

	rmr_ptr = (dapl_llist_is_empty(&ia_ptr->rmr_list_head)
		   ? NULL : dapl_llist_peek_head(&ia_ptr->rmr_list_head));
	while (rmr_ptr != NULL) {
		next_rmr_ptr = dapl_llist_next_entry(&ia_ptr->rmr_list_head,
						     &rmr_ptr->header.
						     ia_list_entry);
		dat_status = dapl_rmr_free(rmr_ptr);
		if (dat_status != DAT_SUCCESS) {
			dapl_dbg_log(DAPL_DBG_TYPE_WARN,
				     "ia_close(ABRUPT): rmr_free(%p) returns %x\n",
				     rmr_ptr, dat_status);
		}
		rmr_ptr = next_rmr_ptr;
	}

	sp_ptr = (dapl_llist_is_empty(&ia_ptr->rsp_list_head)
		  ? NULL : dapl_llist_peek_head(&ia_ptr->rsp_list_head));
	while (sp_ptr != NULL) {
		next_sp_ptr = dapl_llist_next_entry(&ia_ptr->rsp_list_head,
						    &sp_ptr->header.
						    ia_list_entry);
		dat_status = dapl_rsp_free(sp_ptr);
		if (dat_status != DAT_SUCCESS) {
			dapl_dbg_log(DAPL_DBG_TYPE_WARN,
				     "ia_close(ABRUPT): rsp_free(%p) returns %x\n",
				     sp_ptr, dat_status);
		}
		sp_ptr = next_sp_ptr;
	}

	ep_ptr = (dapl_llist_is_empty(&ia_ptr->ep_list_head)
		  ? NULL : dapl_llist_peek_head(&ia_ptr->ep_list_head));
	while (ep_ptr != NULL) {
		next_ep_ptr = dapl_llist_next_entry(&ia_ptr->ep_list_head,
						    &ep_ptr->header.
						    ia_list_entry);
		/*
		 * Issue a disconnect if the EP needs it
		 */
		if (ep_ptr->param.ep_state == DAT_EP_STATE_CONNECTED ||
		    ep_ptr->param.ep_state == DAT_EP_STATE_DISCONNECTED ||
		    ep_ptr->param.ep_state ==
		    DAT_EP_STATE_ACTIVE_CONNECTION_PENDING
		    || ep_ptr->param.ep_state == DAT_EP_STATE_COMPLETION_PENDING
		    || ep_ptr->param.ep_state ==
		    DAT_EP_STATE_DISCONNECT_PENDING) {
			dat_status =
			    dapl_ep_disconnect(ep_ptr, DAT_CLOSE_ABRUPT_FLAG);
			if (dat_status != DAT_SUCCESS) {
				dapl_dbg_log(DAPL_DBG_TYPE_WARN,
					     "ia_close(ABRUPT): ep_disconnect(%p) returns %x\n",
					     ep_ptr, dat_status);
			}
		}
		/* force the EP into error state to force flush all posted DTOs. */
		{
			DAT_EP_ATTR ep_attr;
			DAT_NAMED_ATTR ep_state;

			dapl_os_memzero(&ep_attr, sizeof(DAT_EP_ATTR));
			ep_state.name = (char *)IB_QP_STATE;
			ep_state.value = (char *)DAPL_QP_STATE_ERROR;
			ep_attr.ep_provider_specific_count = 1;
			ep_attr.ep_provider_specific = &ep_state;

			(void)dapls_ib_qp_modify(ia_ptr, ep_ptr, &ep_attr);
		}

		dat_status = dapl_ep_free(ep_ptr);
		if (dat_status != DAT_SUCCESS) {
			dapl_dbg_log(DAPL_DBG_TYPE_WARN,
				     "ia_close(ABRUPT): ep_free(%p) returns %x\n",
				     ep_ptr, dat_status);
		}
		ep_ptr = next_ep_ptr;
	}

	lmr_ptr = (dapl_llist_is_empty(&ia_ptr->lmr_list_head)
		   ? NULL : dapl_llist_peek_head(&ia_ptr->lmr_list_head));
	while (lmr_ptr != NULL) {
		next_lmr_ptr = dapl_llist_next_entry(&ia_ptr->lmr_list_head,
						     &lmr_ptr->header.
						     ia_list_entry);
		dat_status = dapl_lmr_free(lmr_ptr);
		if (dat_status != DAT_SUCCESS) {
			dapl_dbg_log(DAPL_DBG_TYPE_WARN,
				     "ia_close(ABRUPT): lmr_free(%p) returns %x\n",
				     lmr_ptr, dat_status);
		}
		lmr_ptr = next_lmr_ptr;
	}

	sp_ptr = (dapl_llist_is_empty(&ia_ptr->psp_list_head)
		  ? NULL : dapl_llist_peek_head(&ia_ptr->psp_list_head));
	while (sp_ptr != NULL) {
		/*
		 * Shut down the PSP so we get no further callbacks. There
		 * should be no competing threads after this.
		 */
		dat_status = dapls_ib_remove_conn_listener(ia_ptr, sp_ptr);
		if (dat_status != DAT_SUCCESS) {
			dapl_dbg_log(DAPL_DBG_TYPE_WARN,
				     "ia_close(ABRUPT): psp cannot remove listener, returns %x\n",
				     dat_status);
		}

		next_sp_ptr = dapl_llist_next_entry(&ia_ptr->psp_list_head,
						    &sp_ptr->header.
						    ia_list_entry);

		/* Remove CR's from this PSP and clean them up */
		cr_ptr = dapl_llist_is_empty(&sp_ptr->cr_list_head) ? NULL :
		    dapl_llist_peek_head(&sp_ptr->cr_list_head);
		while (cr_ptr != NULL) {
			next_cr_ptr =
			    dapl_llist_next_entry(&sp_ptr->cr_list_head,
						  &cr_ptr->header.
						  ia_list_entry);
			/* Remove the CR from the queue & cleanup */
			dapl_os_lock(&sp_ptr->header.lock);
			dapl_sp_remove_cr(sp_ptr, cr_ptr);
			dapl_os_unlock(&sp_ptr->header.lock);

			dapls_cr_free(cr_ptr);
			cr_ptr = next_cr_ptr;
		}

		dat_status = dapl_psp_free(sp_ptr);
		if (dat_status != DAT_SUCCESS) {
			dapl_dbg_log(DAPL_DBG_TYPE_WARN,
				     "ia_close(ABRUPT): psp_free(%p) returns %x\n",
				     sp_ptr, dat_status);
		}

		sp_ptr = next_sp_ptr;
	}

	pz_ptr = (dapl_llist_is_empty(&ia_ptr->pz_list_head)
		  ? NULL : dapl_llist_peek_head(&ia_ptr->pz_list_head));
	while (pz_ptr != NULL) {
		next_pz_ptr = dapl_llist_next_entry(&ia_ptr->pz_list_head,
						    &pz_ptr->header.
						    ia_list_entry);
		dat_status = dapl_pz_free(pz_ptr);
		if (dat_status != DAT_SUCCESS) {
			dapl_dbg_log(DAPL_DBG_TYPE_WARN,
				     "ia_close(ABRUPT): pz_free(%p) returns %x\n",
				     pz_ptr, dat_status);
		}
		pz_ptr = next_pz_ptr;
	}

	/*
	 * EVDs are tricky; we want to release all except for the async
	 * EVD.  That EVD needs to stick around until after we close the
	 * HCA, to accept any async events that occur.  So we cycle through
	 * the list with dapl_llist_next_entry instead of dapl_llist_is_empty.
	 */
	evd_ptr = (dapl_llist_is_empty(&ia_ptr->evd_list_head)
		   ? NULL : dapl_llist_peek_head(&ia_ptr->evd_list_head));
	while (evd_ptr != NULL) {
		next_evd_ptr = dapl_llist_next_entry(&ia_ptr->evd_list_head,
						     &evd_ptr->header.
						     ia_list_entry);
		if (evd_ptr == ia_ptr->async_error_evd) {
#if !defined(__KDAPL__)
			/* Don't delete the EVD, but break any CNO connections.  */
			dapl_evd_disable(evd_ptr);
			dapl_evd_modify_cno(evd_ptr, DAT_HANDLE_NULL);
#endif				/* __KDAPL__ */
		} else {
			/* it isn't the async EVD; delete it.  */
			dat_status = dapl_evd_free(evd_ptr);
			if (dat_status != DAT_SUCCESS) {
				dapl_dbg_log(DAPL_DBG_TYPE_WARN,
					     "ia_close(ABRUPT): evd_free(%p) returns %x\n",
					     evd_ptr, dat_status);
			}
		}
		evd_ptr = next_evd_ptr;
	}

	cno_ptr = (dapl_llist_is_empty(&ia_ptr->cno_list_head)
		   ? NULL : dapl_llist_peek_head(&ia_ptr->cno_list_head));
	while (cno_ptr != NULL) {
		next_cno_ptr = dapl_llist_next_entry(&ia_ptr->cno_list_head,
						     &cno_ptr->header.
						     ia_list_entry);
#if !defined(__KDAPL__)
		if (cno_ptr->cno_waiters > 0) {
			/* Notify the waiter the IA is going away: see uDAPL 1.1 spec,
			 * 6.3.2.3
			 */
			dapl_internal_cno_trigger(cno_ptr, NULL);
		}
		/* clean up the CNO */
		dat_status = dapl_cno_free(cno_ptr);
		if (dat_status != DAT_SUCCESS) {
			dapl_dbg_log(DAPL_DBG_TYPE_WARN,
				     "ia_close(ABRUPT): cno_free(%p) returns %x\n",
				     cno_ptr, dat_status);
		}
#endif				/* __KDAPL__ */
		cno_ptr = next_cno_ptr;
	}

	hca_ptr = ia_ptr->hca_ptr;

	/*
	 * Free the async EVD, shutting down callbacks from the HCA.
	 */
	if (ia_ptr->async_error_evd &&
	    (DAT_TRUE == ia_ptr->cleanup_async_error_evd)) {
		dat_status = dapls_ia_teardown_callbacks(ia_ptr);

		dapl_os_atomic_dec(&ia_ptr->async_error_evd->evd_ref_count);
		dat_status = dapl_evd_free(ia_ptr->async_error_evd);

		if (DAT_SUCCESS != dat_status) {
			dapl_dbg_log(DAPL_DBG_TYPE_WARN,
				     "ia_close(ABRUPT): evd_free(%p) returns %x\n",
				     ia_ptr->async_error_evd, dat_status);
		}

		ia_ptr->async_error_evd = NULL;
	}

	/*
	 * Release our reference on the hca_handle. If we are the last
	 * one, close it
	 */
	dapli_ia_release_hca(hca_ptr);

	dapls_ia_free(ia_ptr);

	return DAT_SUCCESS;	/* Abrupt close can't fail.  */
}

/*
 * dapl_ia_graceful_close
 *
 * Performs an graceful close of the IA
 *
 * Input:
 * 	ia_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	status
 *
 */

DAT_RETURN dapl_ia_graceful_close(IN DAPL_IA * ia_ptr)
{
	DAT_RETURN dat_status;
	DAT_RETURN cur_dat_status;
	DAPL_EVD *evd_ptr;
	DAPL_LLIST_ENTRY *entry;
	DAPL_HCA *hca_ptr;

	dat_status = DAT_SUCCESS;

	if (!dapl_llist_is_empty(&ia_ptr->rmr_list_head) ||
	    !dapl_llist_is_empty(&ia_ptr->rsp_list_head) ||
	    !dapl_llist_is_empty(&ia_ptr->ep_list_head) ||
	    !dapl_llist_is_empty(&ia_ptr->lmr_list_head) ||
	    !dapl_llist_is_empty(&ia_ptr->psp_list_head) ||
	    !dapl_llist_is_empty(&ia_ptr->pz_list_head)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE, DAT_INVALID_STATE_IA_IN_USE);
		goto bail;
	}

	/* if the async evd does not need to be cleaned up */
	/* (ie. it was not created by dapl_ia_open)        */
	/*  then the evd list should be empty              */
	if (DAT_FALSE == ia_ptr->cleanup_async_error_evd) {
		if (!dapl_llist_is_empty(&ia_ptr->evd_list_head)) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_STATE,
				      DAT_INVALID_STATE_IA_IN_USE);
			goto bail;
		}
	}
	/* else the async evd should be the only evd in    */
	/* the list.                                        */
	else {
		evd_ptr = (dapl_llist_is_empty(&ia_ptr->evd_list_head)
			   ? NULL : dapl_llist_peek_head(&ia_ptr->
							 evd_list_head));

		if (evd_ptr != NULL &&
		    !(evd_ptr->evd_flags & DAT_EVD_ASYNC_FLAG)) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_STATE,
				      DAT_INVALID_STATE_IA_IN_USE);
			goto bail;
		}

		entry = ia_ptr->evd_list_head;

		/* if the async evd is not the only element in the list */
		if (entry->blink != entry->flink) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_STATE,
				      DAT_INVALID_STATE_IA_IN_USE);
			goto bail;
		}

		/*
		 * If the async evd has a non-unary ref count (i.e. it's in
		 * use by someone besides us.
		 */
		if (dapl_os_atomic_read(&evd_ptr->evd_ref_count) != 1) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_STATE,
				      DAT_INVALID_STATE_IA_IN_USE);
			goto bail;
		}
	}

	/*
	 * We've validated the call; now we can start the teardown.
	 * Because we're in the IA close routine, we're safe from races with DAPL
	 * consumers on this IA (operate/destroy races are disallowed in
	 * DAPL).
	 */
	hca_ptr = ia_ptr->hca_ptr;

	/* Tear down the async EVD if needed, first shutting down callbacks.  */
	if (ia_ptr->async_error_evd &&
	    (DAT_TRUE == ia_ptr->cleanup_async_error_evd)) {
		cur_dat_status = dapls_ia_teardown_callbacks(ia_ptr);
		if (DAT_SUCCESS != cur_dat_status) {
			dat_status = cur_dat_status;
		}
		dapl_os_atomic_dec(&ia_ptr->async_error_evd->evd_ref_count);
		cur_dat_status = dapl_evd_free(ia_ptr->async_error_evd);
		if (DAT_SUCCESS != cur_dat_status) {
			dat_status = cur_dat_status;
		}

		ia_ptr->async_error_evd = NULL;
	}

	dapli_ia_release_hca(hca_ptr);

	dapls_ia_free(ia_ptr);

      bail:
	return dat_status;
}

/*
 * Release a reference on the HCA handle. If it is 0, close the
 * handle. Manipulate under lock to prevent races with threads trying to
 * open the HCA.
 */
void dapli_ia_release_hca(DAPL_HCA * hca_ptr)
{
	dapl_os_lock(&hca_ptr->lock);
	dapl_os_atomic_dec(&hca_ptr->handle_ref_count);
	if (dapl_os_atomic_read(&hca_ptr->handle_ref_count) == 0) {
		dapls_ib_close_hca(hca_ptr);
		hca_ptr->ib_hca_handle = IB_INVALID_HANDLE;
		hca_ptr->async_evd = NULL;
	}
	dapl_os_unlock(&hca_ptr->lock);
}

/*
 * dapls_ia_free
 *
 * free an IA INFO struct
 *
 * Input:
 * 	ia_ptr
 *
 * Output:
 * 	one
 *
 * Returns:
 * 	none
 *
 */
void dapls_ia_free(DAPL_IA * ia_ptr)
{
	dapl_os_assert(ia_ptr->header.magic == DAPL_MAGIC_IA);

	dapl_os_assert(ia_ptr->async_error_evd == NULL);
	dapl_os_assert(dapl_llist_is_empty(&ia_ptr->lmr_list_head));
	dapl_os_assert(dapl_llist_is_empty(&ia_ptr->rmr_list_head));
	dapl_os_assert(dapl_llist_is_empty(&ia_ptr->ep_list_head));
	dapl_os_assert(dapl_llist_is_empty(&ia_ptr->evd_list_head));
	dapl_os_assert(dapl_llist_is_empty(&ia_ptr->cno_list_head));
	dapl_os_assert(dapl_llist_is_empty(&ia_ptr->psp_list_head));
	dapl_os_assert(dapl_llist_is_empty(&ia_ptr->rsp_list_head));

	/*
	 * deinitialize the header
	 */
	dapl_hca_unlink_ia(ia_ptr->hca_ptr, ia_ptr);
	ia_ptr->header.magic = DAPL_MAGIC_INVALID;	/* reset magic to prevent reuse */
	dapl_os_lock_destroy(&ia_ptr->header.lock);

#ifdef DAPL_COUNTERS
	dapl_os_free(ia_ptr->cntrs, sizeof(DAT_UINT64) * DCNT_IA_ALL_COUNTERS);
#endif				/* DAPL_COUNTERS */

	dapl_os_free(ia_ptr, sizeof(DAPL_IA));
}

/*
 * dapl_ia_link_ep
 *
 * Add an ep to the IA structure
 *
 * Input:
 *	ia_ptr
 *	ep_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_ia_link_ep(IN DAPL_IA * ia_ptr, IN DAPL_EP * ep_ptr)
{
	dapl_os_lock(&ia_ptr->header.lock);
	dapl_llist_add_head(&ia_ptr->ep_list_head,
			    &ep_ptr->header.ia_list_entry, ep_ptr);
	dapl_os_unlock(&ia_ptr->header.lock);
}

/*
 * dapl_ia_unlink_ep
 *
 * Remove an ep from the ia info structure
 *
 * Input:
 *	ia_ptr
 *	ep_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_ia_unlink_ep(IN DAPL_IA * ia_ptr, IN DAPL_EP * ep_ptr)
{
	dapl_os_lock(&ia_ptr->header.lock);
	dapl_llist_remove_entry(&ia_ptr->ep_list_head,
				&ep_ptr->header.ia_list_entry);
	dapl_os_unlock(&ia_ptr->header.lock);
}

/*
 * dapl_ia_link_srq
 *
 * Add an srq to the IA structure
 *
 * Input:
 *	ia_ptr
 *	srq_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_ia_link_srq(IN DAPL_IA * ia_ptr, IN DAPL_SRQ * srq_ptr)
{
	dapl_os_lock(&ia_ptr->header.lock);
	dapl_llist_add_head(&ia_ptr->srq_list_head,
			    &srq_ptr->header.ia_list_entry, srq_ptr);
	dapl_os_unlock(&ia_ptr->header.lock);
}

/*
 * dapl_ia_unlink_srq
 *
 * Remove an srq from the ia info structure
 *
 * Input:
 *	ia_ptr
 *	srq_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_ia_unlink_srq(IN DAPL_IA * ia_ptr, IN DAPL_SRQ * srq_ptr)
{
	dapl_os_lock(&ia_ptr->header.lock);
	dapl_llist_remove_entry(&ia_ptr->srq_list_head,
				&srq_ptr->header.ia_list_entry);
	dapl_os_unlock(&ia_ptr->header.lock);
}

/*
 * dapl_ia_link_lmr
 *
 * Add an lmr to the IA structure
 *
 * Input:
 *	ia_ptr
 *	lmr_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_ia_link_lmr(IN DAPL_IA * ia_ptr, IN DAPL_LMR * lmr_ptr)
{
	dapl_os_lock(&ia_ptr->header.lock);
	dapl_llist_add_head(&ia_ptr->lmr_list_head,
			    &lmr_ptr->header.ia_list_entry, lmr_ptr);
	dapl_os_unlock(&ia_ptr->header.lock);
}

/*
 * dapl_ia_unlink_lmr
 *
 * Remove an lmr from the ia info structure
 *
 * Input:
 *	ia_ptr
 *	lmr_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_ia_unlink_lmr(IN DAPL_IA * ia_ptr, IN DAPL_LMR * lmr_ptr)
{
	dapl_os_lock(&ia_ptr->header.lock);
	dapl_llist_remove_entry(&ia_ptr->lmr_list_head,
				&lmr_ptr->header.ia_list_entry);
	dapl_os_unlock(&ia_ptr->header.lock);
}

/*
 * dapl_ia_link_rmr
 *
 * Add an rmr to the IA structure
 *
 * Input:
 *	ia_ptr
 *	rmr_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_ia_link_rmr(IN DAPL_IA * ia_ptr, IN DAPL_RMR * rmr_ptr)
{
	dapl_os_lock(&ia_ptr->header.lock);
	dapl_llist_add_head(&ia_ptr->rmr_list_head,
			    &rmr_ptr->header.ia_list_entry, rmr_ptr);
	dapl_os_unlock(&ia_ptr->header.lock);
}

/*
 * dapl_ia_unlink_rmr
 *
 * Remove an rmr from the ia info structure
 *
 * Input:
 *	ia_ptr
 *	rmr_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_ia_unlink_rmr(IN DAPL_IA * ia_ptr, IN DAPL_RMR * rmr_ptr)
{
	dapl_os_lock(&ia_ptr->header.lock);
	dapl_llist_remove_entry(&ia_ptr->rmr_list_head,
				&rmr_ptr->header.ia_list_entry);
	dapl_os_unlock(&ia_ptr->header.lock);
}

/*
 * dapl_ia_link_pz
 *
 * Add an pz to the IA structure
 *
 * Input:
 *	ia_ptr
 *	pz_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_ia_link_pz(IN DAPL_IA * ia_ptr, IN DAPL_PZ * pz_ptr)
{
	dapl_os_lock(&ia_ptr->header.lock);
	dapl_llist_add_head(&ia_ptr->pz_list_head,
			    &pz_ptr->header.ia_list_entry, pz_ptr);
	dapl_os_unlock(&ia_ptr->header.lock);
}

/*
 * dapl_ia_unlink_pz
 *
 * Remove an pz from the ia info structure
 *
 * Input:
 *	ia_ptr
 *	pz_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_ia_unlink_pz(IN DAPL_IA * ia_ptr, IN DAPL_PZ * pz_ptr)
{
	dapl_os_lock(&ia_ptr->header.lock);
	dapl_llist_remove_entry(&ia_ptr->pz_list_head,
				&pz_ptr->header.ia_list_entry);
	dapl_os_unlock(&ia_ptr->header.lock);
}

/*
 * dapl_ia_link_evd
 *
 * Add an evd to the IA structure
 *
 * Input:
 *	ia_ptr
 *	evd_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_ia_link_evd(IN DAPL_IA * ia_ptr, IN DAPL_EVD * evd_ptr)
{
	dapl_os_lock(&ia_ptr->header.lock);
	dapl_llist_add_head(&ia_ptr->evd_list_head,
			    &evd_ptr->header.ia_list_entry, evd_ptr);
	dapl_os_unlock(&ia_ptr->header.lock);
}

/*
 * dapl_ia_unlink_evd
 *
 * Remove an evd from the ia info structure
 *
 * Input:
 *	ia_ptr
 *	evd_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_ia_unlink_evd(IN DAPL_IA * ia_ptr, IN DAPL_EVD * evd_ptr)
{
	dapl_os_lock(&ia_ptr->header.lock);
	dapl_llist_remove_entry(&ia_ptr->evd_list_head,
				&evd_ptr->header.ia_list_entry);
	dapl_os_unlock(&ia_ptr->header.lock);
}

/*
 * dapl_ia_link_cno
 *
 * Add an cno to the IA structure
 *
 * Input:
 *	ia_ptr
 *	cno_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_ia_link_cno(IN DAPL_IA * ia_ptr, IN DAPL_CNO * cno_ptr)
{
	dapl_os_lock(&ia_ptr->header.lock);
	dapl_llist_add_head(&ia_ptr->cno_list_head,
			    &cno_ptr->header.ia_list_entry, cno_ptr);
	dapl_os_unlock(&ia_ptr->header.lock);
}

/*
 * dapl_ia_unlink_cno
 *
 * Remove an cno from the ia info structure
 *
 * Input:
 *	ia_ptr
 *	cno_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_ia_unlink_cno(IN DAPL_IA * ia_ptr, IN DAPL_CNO * cno_ptr)
{
	dapl_os_lock(&ia_ptr->header.lock);
	dapl_llist_remove_entry(&ia_ptr->cno_list_head,
				&cno_ptr->header.ia_list_entry);
	dapl_os_unlock(&ia_ptr->header.lock);
}

/*
 * dapl_ia_link_psp
 *
 * Add an psp to the IA structure
 *
 * Input:
 *	ia_ptr
 *	sp_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_ia_link_psp(IN DAPL_IA * ia_ptr, IN DAPL_SP * sp_ptr)
{
	dapl_os_lock(&ia_ptr->header.lock);
	dapl_llist_add_head(&ia_ptr->psp_list_head,
			    &sp_ptr->header.ia_list_entry, sp_ptr);
	dapl_os_unlock(&ia_ptr->header.lock);
}

/*
 * daps_ia_unlink_sp
 *
 * Remove an sp from the appropriate ia rsp or psp queue
 *
 * Input:
 *	ia_ptr
 *	sp_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapls_ia_unlink_sp(IN DAPL_IA * ia_ptr, IN DAPL_SP * sp_ptr)
{
	DAPL_LLIST_HEAD *list_head;

	if (sp_ptr->header.handle_type == DAT_HANDLE_TYPE_PSP) {
		list_head = &ia_ptr->psp_list_head;
	} else {
		dapl_os_assert(sp_ptr->header.handle_type ==
			       DAT_HANDLE_TYPE_RSP);
		list_head = &ia_ptr->rsp_list_head;
	}

	dapl_os_lock(&ia_ptr->header.lock);
	dapl_llist_remove_entry(list_head, &sp_ptr->header.ia_list_entry);
	dapl_os_unlock(&ia_ptr->header.lock);
}

/*
 * dapls_ia_sp_search
 *
 * Find an RSP or PSP on the IA list with a matching conn_qual value
 *
 * Input:
 *	ia_ptr
 *	sp_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
DAPL_SP *dapls_ia_sp_search(IN DAPL_IA * ia_ptr,
			    IN DAT_CONN_QUAL conn_qual, IN DAT_BOOLEAN is_psp)
{
	DAPL_SP *sp_ptr;
	DAPL_LLIST_HEAD *list_head;

	if (is_psp) {
		list_head = &ia_ptr->psp_list_head;
	} else {
		list_head = &ia_ptr->rsp_list_head;
	}

	dapl_os_lock(&ia_ptr->header.lock);

	sp_ptr = (dapl_llist_is_empty(list_head) ? NULL :
		  dapl_llist_peek_head(list_head));

	while (sp_ptr != NULL) {
		if (sp_ptr->conn_qual == conn_qual) {
			break;
		}
		sp_ptr = dapl_llist_next_entry(list_head,
					       &sp_ptr->header.ia_list_entry);
	}

	dapl_os_unlock(&ia_ptr->header.lock);

	return sp_ptr;
}

/*
 * dapl_ia_link_rsp
 *
 * Add an rsp to the IA structure
 *
 * Input:
 *	ia_ptr
 *	sp_ptr
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 *
 */
void dapl_ia_link_rsp(IN DAPL_IA * ia_ptr, IN DAPL_SP * sp_ptr)
{
	dapl_os_lock(&ia_ptr->header.lock);
	dapl_llist_add_head(&ia_ptr->rsp_list_head,
			    &sp_ptr->header.ia_list_entry, sp_ptr);
	dapl_os_unlock(&ia_ptr->header.lock);
}

DAT_RETURN
dapls_ia_setup_callbacks(IN DAPL_IA * ia_ptr, IN DAPL_EVD * async_evd_ptr)
{
	DAT_RETURN dat_status = DAT_SUCCESS;

	/* unaffiliated handler */
	dat_status =
	    dapls_ib_setup_async_callback(ia_ptr,
					  DAPL_ASYNC_UNAFILIATED,
					  NULL,
					  (ib_async_handler_t)
					  dapl_evd_un_async_error_callback,
					  async_evd_ptr);

	if (dat_status != DAT_SUCCESS) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "ib_set_un_async_error_eh failed %d\n",
			     dat_status);
		goto bail;
	}

	/* affiliated cq handler */
	dat_status = dapls_ib_setup_async_callback(ia_ptr,
						   DAPL_ASYNC_CQ_ERROR,
						   NULL,
						   (ib_async_handler_t)
						   dapl_evd_cq_async_error_callback,
						   async_evd_ptr);

	if (dat_status != DAT_SUCCESS) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "ib_set_cq_async_error_eh failed %d\n",
			     dat_status);
		goto bail;
	}

	/* affiliated qp handler */
	dat_status = dapls_ib_setup_async_callback(ia_ptr,
						   DAPL_ASYNC_QP_ERROR,
						   NULL,
						   (ib_async_handler_t)
						   dapl_evd_qp_async_error_callback,
						   ia_ptr);
	if (dat_status != DAT_SUCCESS) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "ib_set_qp_async_error_eh failed %d\n",
			     dat_status);
		goto bail;
	}

      bail:
	return dat_status;
}

DAT_RETURN dapls_ia_teardown_callbacks(IN DAPL_IA * ia_ptr)
{
	DAT_RETURN dat_status = DAT_SUCCESS;

	/* unaffiliated handler */
	dat_status =
	    dapls_ib_setup_async_callback(ia_ptr,
					  DAPL_ASYNC_UNAFILIATED,
					  NULL, (ib_async_handler_t) 0, NULL);

	if (dat_status != DAT_SUCCESS) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "ib_set_un_async_error_eh failed %d\n",
			     dat_status);
		goto bail;
	}

	/* affiliated cq handler */
	dat_status = dapls_ib_setup_async_callback(ia_ptr,
						   DAPL_ASYNC_CQ_ERROR,
						   NULL,
						   (ib_async_handler_t) 0,
						   NULL);

	if (dat_status != DAT_SUCCESS) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "ib_set_cq_async_error_eh failed %d\n",
			     dat_status);
		goto bail;
	}

	/* affiliated qp handler */
	dat_status = dapls_ib_setup_async_callback(ia_ptr,
						   DAPL_ASYNC_QP_ERROR,
						   NULL,
						   (ib_async_handler_t) 0,
						   NULL);
	if (dat_status != DAT_SUCCESS) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "ib_set_qp_async_error_eh failed %d\n",
			     dat_status);
		goto bail;
	}

      bail:
	return dat_status;
}
