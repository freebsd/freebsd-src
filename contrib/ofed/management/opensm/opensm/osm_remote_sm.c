/*
 * Copyright (c) 2004-2006 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*
 * Abstract:
 *    Implementation of osm_sm_t.
 * This object represents the remote SM object.
 * This object is part of the opensm family of objects.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <string.h>
#include <opensm/osm_remote_sm.h>

/**********************************************************************
 **********************************************************************/
void osm_remote_sm_construct(IN osm_remote_sm_t * const p_sm)
{
	memset(p_sm, 0, sizeof(*p_sm));
}

/**********************************************************************
 **********************************************************************/
void osm_remote_sm_destroy(IN osm_remote_sm_t * const p_sm)
{
	memset(p_sm, 0, sizeof(*p_sm));
}

/**********************************************************************
 **********************************************************************/
void
osm_remote_sm_init(IN osm_remote_sm_t * const p_sm,
		   IN const osm_port_t * const p_port,
		   IN const ib_sm_info_t * const p_smi)
{
	CL_ASSERT(p_sm);
	CL_ASSERT(p_port);

	osm_remote_sm_construct(p_sm);

	p_sm->p_port = p_port;
	p_sm->smi = *p_smi;
	return;
}
