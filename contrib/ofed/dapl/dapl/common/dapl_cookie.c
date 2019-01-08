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
 * MODULE: dapl_cookie.c
 *
 * PURPOSE: Manage CQE cookie structures
 *
 * The DAPL spec requires that all a cookies passed to a posting operation 
 * be returned in the operation's corresponding completion. 
 * 
 * Implementing this feature is complicated by the user's ability to 
 * suppress event generation for specific operations. When these operations 
 * complete successfully, the provider does not have an easy way to 
 * deallocate resources devoted to storing context data for these operations.
 *
 * To support this feature, a pool of memory is allocated up front large 
 * enough to hold cookie data for the maximum number of operations possible 
 * on an endpoint. 
 *
 * Two pieces of information are maintained to manage cookie allocation:
 *
 * head index : index of next unallocated cookie 
 * tail index : index of last unallocated cookie
 *
 * Each cookie store its index in this memory pool.
 *
 * When an event is received, the index stored in the event's cookie will be 
 * used to update the tail. This will implicitly deallocate all of the cookies 
 * "between" the old tail and the new tail.
 *
 * The implementation relies on the following assumptions:
 *
 * - there can be only 1 thread in dat_ep_post_send(), dat_ep_post_rdma_write(),
 *   dat_ep_post_rdma_read(), or dat_rmr_bind() at a time, therefore 
 *   dapls_cb_get() does not need to be thread safe when manipulating
 *   request data structures.
 *
 * - there can be only 1 thread in dat_ep_post_recv(), therefore 
 *   dapls_cb_get() does not need to be thread safe when manipulating
 *   receive data structures.
 *
 * - there can be only 1 thread generating completions for a given EP's request 
 *   opeartions, therefore dapls_cb_put() does not need to be thread safe when 
 *   manipulating request data structures.
 *
 * - there can be only 1 thread generating completions for a given EP's receive 
 *   opeartions therefore dapls_cb_put() does not need to be thread safe when 
 *   manipulating receive data structures.
 *
 * - completions are delivered in order
 *
 * $Id:$
 **********************************************************************/

#include "dapl_cookie.h"
#include "dapl_ring_buffer_util.h"

/*********************************************************************
 *                                                                   *
 * Function Prototypes                                               *
 *                                                                   *
 *********************************************************************/

DAT_RETURN dapls_cb_get(DAPL_COOKIE_BUFFER * buffer, DAPL_COOKIE ** cookie_ptr);

DAT_RETURN dapls_cb_put(DAPL_COOKIE_BUFFER * buffer, DAPL_COOKIE * cookie);

/*********************************************************************
 *                                                                   *
 * Function Definitions                                              *
 *                                                                   *
 *********************************************************************/

/*
 * dapls_cb_create
 *
 * Given a DAPL_COOKIE_BUFFER, allocate and initialize memory for
 * the data structure.
 *
 * Input:
 *	buffer		pointer to DAPL_COOKIE_BUFFER
 *	ep		endpoint to associate with cookies
 *	size		number of elements to allocate & manage
 *
 * Output:
 *	none
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *
 */
DAT_RETURN
dapls_cb_create(IN DAPL_COOKIE_BUFFER * buffer,
		IN DAPL_EP * ep, IN DAT_COUNT size)
{
	DAT_COUNT i;

	/*
	 * allocate one additional entry so that the tail 
	 * can always point at an empty location
	 */
	size++;

	buffer->pool = dapl_os_alloc(size * sizeof(DAPL_COOKIE));
	if (NULL != buffer->pool) {
		buffer->pool_size = size;
		dapl_os_atomic_set(&buffer->head, 0);
		dapl_os_atomic_set(&buffer->tail, 0);

		for (i = 0; i < size; i++) {
			buffer->pool[i].index = i;
			buffer->pool[i].ep = ep;
		}

		return (DAT_SUCCESS);
	} else {
		return (DAT_INSUFFICIENT_RESOURCES);
	}
}

/*
 * dapls_cb_free
 *
 * Free the data structure
 *
 * Input:
 *	buffer		pointer to DAPL_COOKIE_BUFFER
 *
 * Output:
 *	none
 *
 * Returns:
 *	none
 *
 */
void dapls_cb_free(IN DAPL_COOKIE_BUFFER * buffer)
{
	if (NULL != buffer->pool) {
		dapl_os_free(buffer->pool,
			     buffer->pool_size * sizeof(DAPL_COOKIE));
		buffer->pool = NULL;
	}
}

/*
 * dapls_cb_get
 *
 * Remove an entry from the buffer
 *
 * Input:
 *	buffer		pointer to DAPL_COOKIE_BUFFER
 *
 * Output:
 *      cookie_ptr 	pointer to pointer to cookie
 *
 * Returns:
 *	DAT_SUCCESS
 * 	DAT_INVALID_PARAMETER
 *	DAT_INSUFFICIENT_RESOURCES
 *
 */
DAT_RETURN
dapls_cb_get(IN DAPL_COOKIE_BUFFER * buffer, OUT DAPL_COOKIE ** cookie_ptr)
{
	DAT_RETURN dat_status;
	DAT_COUNT new_head;

	dapl_os_assert(NULL != cookie_ptr);

	new_head = (dapl_os_atomic_read(&buffer->head) + 1) % buffer->pool_size;

	if (new_head == dapl_os_atomic_read(&buffer->tail)) {
		dat_status = DAT_INSUFFICIENT_RESOURCES;
		goto bail;
	} else {
		dapl_os_atomic_set(&buffer->head, new_head);
		*cookie_ptr = &buffer->pool[dapl_os_atomic_read(&buffer->head)];
		dat_status = DAT_SUCCESS;
	}
      bail:
	return dat_status;
}

/*
 * dapls_cb_put
 *
 * Add entry(s) to the buffer
 *
 * Input:
 *	buffer		pointer to DAPL_COOKIE_BUFFER
 *      cookie 		pointer to cookie
 *
 * Output:
 *	entry		entry removed from the ring buffer
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INSUFFICIENT_EMPTY
 *
 */
DAT_RETURN dapls_cb_put(IN DAPL_COOKIE_BUFFER * buffer, IN DAPL_COOKIE * cookie)
{
	dapl_os_atomic_set(&buffer->tail, cookie->index);

	return DAT_SUCCESS;
}

/*
 * dapls_cb_pending
 *
 * snapshot of active entries on cookie ring buffer 
 *
 * Input:
 *	buffer		pointer to DAPL_COOKIE_BUFFER
 *
 * Returns:
 *	DAT_COUNT	number of active/pending cookies
 *
 */
extern DAT_COUNT dapls_cb_pending(DAPL_COOKIE_BUFFER * buffer)
{
	DAT_COUNT head, tail;

	head = dapl_os_atomic_read(&buffer->head);
	tail = dapl_os_atomic_read(&buffer->tail);

	if (head == tail)
		return 0;
	else if (head > tail)
		return (head - tail);
	else
		return ((buffer->pool_size - tail) + head);
}

/*
 * dapls_rmr_cookie_alloc
 *
 * Allocate an RMR Bind cookie
 *
 * Input:
 *	buffer		pointer to DAPL_COOKIE_BUFFER
 *      rmr 		rmr to associate with the cookie
 *      user_cookie     user's cookie data 
 *
 * Output:
 *	cookie_ptr	pointer to pointer to allocated cookie
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INSUFFICIENT_EMPTY
 *
 */
DAT_RETURN
dapls_rmr_cookie_alloc(IN DAPL_COOKIE_BUFFER * buffer,
		       IN DAPL_RMR * rmr,
		       IN DAT_RMR_COOKIE user_cookie,
		       OUT DAPL_COOKIE ** cookie_ptr)
{
	DAPL_COOKIE *cookie;
	DAT_RETURN dat_status;

	if (DAT_SUCCESS != dapls_cb_get(buffer, &cookie)) {
		*cookie_ptr = NULL;
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	dat_status = DAT_SUCCESS;
	cookie->type = DAPL_COOKIE_TYPE_RMR;
	cookie->val.rmr.rmr = rmr;
	cookie->val.rmr.cookie = user_cookie;

	*cookie_ptr = cookie;

      bail:
	return dat_status;
}

/*
 * dapls_dto_cookie_alloc
 *
 * Allocate a DTO cookie
 *
 * Input:
 *	buffer		pointer to DAPL_COOKIE_BUFFER
 * 	type 		DTO type
 *      user_cookie     user's cookie data 
 *
 * Output:
 *	cookie_ptr	pointer to pointer to allocated cookie
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INSUFFICIENT_EMPTY
 *
 */
DAT_RETURN
dapls_dto_cookie_alloc(IN DAPL_COOKIE_BUFFER * buffer,
		       IN DAPL_DTO_TYPE type,
		       IN DAT_DTO_COOKIE user_cookie,
		       OUT DAPL_COOKIE ** cookie_ptr)
{
	DAPL_COOKIE *cookie;

	if (DAT_SUCCESS != dapls_cb_get(buffer, &cookie)) {
		*cookie_ptr = NULL;
		return DAT_ERROR(DAT_INSUFFICIENT_RESOURCES,
				 DAT_RESOURCE_MEMORY);
	}

	cookie->type = DAPL_COOKIE_TYPE_DTO;
	cookie->val.dto.type = type;
	cookie->val.dto.cookie = user_cookie;
	cookie->val.dto.size = 0;

	*cookie_ptr = cookie;
	return DAT_SUCCESS;
}

void
dapls_cookie_dealloc(IN DAPL_COOKIE_BUFFER * buffer, IN DAPL_COOKIE * cookie)
{
	dapls_cb_put(buffer, cookie);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
