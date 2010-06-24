/*
 * Copyright (c) 2004-2008 Voltaire, Inc. All rights reserved.
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
 *    Implementation of osm_mcm_port_t.
 * This object represents the membership of a port in a multicast group.
 * This object is part of the OpenSM family of objects.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>
#include <opensm/osm_mcm_port.h>

/**********************************************************************
 **********************************************************************/
osm_mcm_port_t *osm_mcm_port_new(IN const ib_gid_t * const p_port_gid,
				 IN const uint8_t scope_state,
				 IN const boolean_t proxy_join)
{
	osm_mcm_port_t *p_mcm;

	p_mcm = malloc(sizeof(*p_mcm));
	if (p_mcm) {
		memset(p_mcm, 0, sizeof(*p_mcm));
		p_mcm->port_gid = *p_port_gid;
		p_mcm->scope_state = scope_state;
		p_mcm->proxy_join = proxy_join;
	}

	return (p_mcm);
}

/**********************************************************************
 **********************************************************************/
void osm_mcm_port_delete(IN osm_mcm_port_t * const p_mcm)
{
	CL_ASSERT(p_mcm);
	free(p_mcm);
}
