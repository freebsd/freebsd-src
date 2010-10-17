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
 * MODULE: dapl_ring_buffer_util.c
 *
 * PURPOSE: Ring buffer management
 * Description: Support and management functions for ring buffers
 *
 * $Id:$
 **********************************************************************/

#include "dapl_ring_buffer_util.h"

/*
 * dapls_rbuf_alloc
 *
 * Given a DAPL_RING_BUFFER, initialize it and provide memory for
 * the ringbuf itself. A passed in size will be adjusted to the next
 * largest power of two number to simplify management.
 *
 * Input:
 *	rbuf		pointer to DAPL_RING_BUFFER
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
DAT_RETURN dapls_rbuf_alloc(INOUT DAPL_RING_BUFFER * rbuf, IN DAT_COUNT size)
{
	unsigned int rsize;	/* real size */

	/* The circular buffer must be allocated one too large.
	 * This eliminates any need for a distinct counter, as
	 * having the two pointers equal always means "empty" -- never "full"
	 */
	size++;

	/* Put size on a power of 2 boundary */
	rsize = 1;
	while ((DAT_COUNT) rsize < size) {
		rsize <<= 1;
	}

	rbuf->base = (void *)dapl_os_alloc(rsize * sizeof(void *));
	if (rbuf->base != NULL) {
		rbuf->lim = rsize - 1;
		dapl_os_atomic_set(&rbuf->head, 0);
		dapl_os_atomic_set(&rbuf->tail, 0);
	} else {
		return DAT_INSUFFICIENT_RESOURCES | DAT_RESOURCE_MEMORY;
	}

	return DAT_SUCCESS;
}

/*
 * dapls_rbuf_realloc
 *
 * Resizes a DAPL_RING_BUFFER. This function is not thread safe;
 * adding or removing elements from a ring buffer while resizing 
 * will have indeterminate results.
 *
 * Input:
 *	rbuf		pointer to DAPL_RING_BUFFER
 *	size		number of elements to allocate & manage
 *
 * Output:
 *	none
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INVALID_STATE
 *	DAT_INSUFFICIENT_RESOURCES
 *
 */
DAT_RETURN dapls_rbuf_realloc(INOUT DAPL_RING_BUFFER * rbuf, IN DAT_COUNT size)
{
	DAPL_RING_BUFFER new_rbuf;
	void *entry;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;

	/* decreasing the size or retaining the old size is not allowed */
	if (size <= rbuf->lim + 1) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG2);
		goto bail;
	}

	/*
	 * !This is NOT ATOMIC!
	 * Simple algorithm: Allocate a new ring buffer, take everything
	 * out of the old one and put it in the new one, and release the 
	 * old base buffer.
	 */
	dat_status = dapls_rbuf_alloc(&new_rbuf, size);
	if (dat_status != DAT_SUCCESS) {
		goto bail;
	}

	while ((entry = dapls_rbuf_remove(rbuf)) != NULL) {
		/* We know entries will fit so ignore the return code */
		(void)dapls_rbuf_add(&new_rbuf, entry);
	}

	/* release the old base buffer */
	dapl_os_free(rbuf->base, (rbuf->lim + 1) * sizeof(void *));

	*rbuf = new_rbuf;

      bail:
	return dat_status;
}

/*
 * dapls_rbuf_destroy
 *
 * Release the buffer and reset pointers to a DAPL_RING_BUFFER
 *
 * Input:
 *	rbuf		pointer to DAPL_RING_BUFFER
 *
 * Output:
 *	none
 *
 * Returns:
 *	none
 *
 */
void dapls_rbuf_destroy(IN DAPL_RING_BUFFER * rbuf)
{
	if ((NULL == rbuf) || (NULL == rbuf->base)) {
		return;
	}

	dapl_os_free(rbuf->base, (rbuf->lim + 1) * sizeof(void *));
	rbuf->base = NULL;
	rbuf->lim = 0;

	return;
}

/*
 * dapls_rbuf_add
 *
 * Add an entry to the ring buffer
 *
 * Input:
 *	rbuf		pointer to DAPL_RING_BUFFER
 *	entry		entry to add
 *
 * Output:
 *	none
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES         (queue full)
 *
 */
DAT_RETURN dapls_rbuf_add(IN DAPL_RING_BUFFER * rbuf, IN void *entry)
{
	int pos;
	int val;

	while (((dapl_os_atomic_read(&rbuf->head) + 1) & rbuf->lim) !=
	       (dapl_os_atomic_read(&rbuf->tail) & rbuf->lim)) {
		pos = dapl_os_atomic_read(&rbuf->head);
		val = dapl_os_atomic_assign(&rbuf->head, pos, pos + 1);
		if (val == pos) {
			pos = (pos + 1) & rbuf->lim;	/* verify in range */
			rbuf->base[pos] = entry;
			return DAT_SUCCESS;
		}
	}

	return DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);

}

/*
 * dapls_rbuf_remove
 *
 * Remove an entry from the ring buffer
 *
 * Input:
 *	rbuf		pointer to DAPL_RING_BUFFER
 *
 * Output:
 *	entry		entry removed from the ring buffer
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_QUEUE_EMPTY
   
 */
void *dapls_rbuf_remove(IN DAPL_RING_BUFFER * rbuf)
{
	int pos;
	int val;

	while (dapl_os_atomic_read(&rbuf->head) !=
	       dapl_os_atomic_read(&rbuf->tail)) {
		pos = dapl_os_atomic_read(&rbuf->tail);
		val = dapl_os_atomic_assign(&rbuf->tail, pos, pos + 1);
		if (val == pos) {
			pos = (pos + 1) & rbuf->lim;	/* verify in range */

			return (rbuf->base[pos]);
		}
	}

	return NULL;

}

/*
 * dapli_rbuf_count
 *
 * Return the number of entries in use in the ring buffer
 *
 * Input:
 *      rbuf            pointer to DAPL_RING_BUFFER
 *
 *
 * Output:
 *	none
 *
 * Returns:
 *	count of entries
 *
 */
DAT_COUNT dapls_rbuf_count(IN DAPL_RING_BUFFER * rbuf)
{
	DAT_COUNT count;
	int head;
	int tail;

	head = dapl_os_atomic_read(&rbuf->head) & rbuf->lim;
	tail = dapl_os_atomic_read(&rbuf->tail) & rbuf->lim;
	if (head > tail) {
		count = head - tail;
	} else {
		/* add 1 to lim as it is a mask, number of entries - 1 */
		count = (rbuf->lim + 1 - tail + head) & rbuf->lim;
	}

	return count;
}

/*
 * dapls_rbuf_adjust
 *
 * Adjusts the addresses of all elements stored in the 
 * ring buffer by a constant. This is useful for situations 
 * in which the memory area for the elements being stored 
 * has been reallocated (see dapl_evd_realloc() and helper 
 * functions).
 *
 * Input:
 *	rbuf		pointer to DAPL_RING_BUFFER
 *	offset		offset to adjust elemnt addresss by,
 *                      used for addresses of type void *
 * Output:
 *	none
 *
 * Returns:
 *	none
 */
void dapls_rbuf_adjust(IN DAPL_RING_BUFFER * rbuf, IN intptr_t offset)
{
	int pos;

	pos = dapl_os_atomic_read(&rbuf->head);
	while (pos != dapl_os_atomic_read(&rbuf->tail)) {
		rbuf->base[pos] = (void *)((char *)rbuf->base[pos] + offset);
		pos = (pos + 1) & rbuf->lim;	/* verify in range */
	}
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
