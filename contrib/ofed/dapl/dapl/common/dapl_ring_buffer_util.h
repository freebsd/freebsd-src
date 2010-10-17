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
 * HEADER: dapl_ring_buffer_util.h
 *
 * PURPOSE: Utility defs & routines for the ring buffer data structure
 *
 * $Id:$
 *
 **********************************************************************/

#ifndef _DAPL_RING_BUFFER_H_
#define _DAPL_RING_BUFFER_H_

#include "dapl.h"

/*
 * Prototypes
 */
DAT_RETURN dapls_rbuf_alloc (
        DAPL_RING_BUFFER		*rbuf,
	DAT_COUNT			 size );

DAT_RETURN dapls_rbuf_realloc (
        DAPL_RING_BUFFER		*rbuf,
	DAT_COUNT			 size );

void dapls_rbuf_destroy (
	DAPL_RING_BUFFER		*rbuf);

DAT_RETURN dapls_rbuf_add (
	DAPL_RING_BUFFER		*rbuf,
	void				*entry);

void * dapls_rbuf_remove (
	DAPL_RING_BUFFER		*rbuf);

DAT_COUNT dapls_rbuf_count (
	DAPL_RING_BUFFER		*rbuf );

void dapls_rbuf_adjust (
	IN  DAPL_RING_BUFFER		*rbuf,
	IN  intptr_t			offset);


/*
 * Simple functions
 */
#define dapls_rbuf_empty(rbuf)	(rbuf->head == rbuf->tail)


#endif /* _DAPL_RING_BUFFER_H_ */
