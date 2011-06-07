/*
 * Copyright (c) 2004-2007 Voltaire, Inc. All rights reserved.
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
 * 	Declaration of osm_mcm_info_t.
 *	This object represents a Multicast Forwarding Information object.
 *	This object is part of the OpenSM family of objects.
 */

#ifndef _OSM_MCM_INFO_H_
#define _OSM_MCM_INFO_H_

#include <string.h>
#include <iba/ib_types.h>
#include <complib/cl_qlist.h>
#include <opensm/osm_base.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/****s* OpenSM: Multicast Member Info/osm_mcm_info_t
* NAME
*	osm_mcm_info_t
*
* DESCRIPTION
*	Multicast Membership Info object.
*	This object contains information about a node's membership
*	in a particular multicast group.
*
*	This object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_mcm_info {
	cl_list_item_t list_item;
	ib_net16_t mlid;
} osm_mcm_info_t;
/*
* FIELDS
*	list_item
*		Linkage structure for cl_qlist.  MUST BE FIRST MEMBER!
*
*	mlid
*		MLID of this multicast group.
*
* SEE ALSO
*********/

/****f* OpenSM: Multicast Member Info/osm_mcm_info_new
* NAME
*	osm_mcm_info_new
*
* DESCRIPTION
*	Returns an initialized a Multicast Member Info object for use.
*
* SYNOPSIS
*/
osm_mcm_info_t *osm_mcm_info_new(IN const ib_net16_t mlid);
/*
* PARAMETERS
*	mlid
*		[in] MLID value for this multicast group.
*
* RETURN VALUES
*	Pointer to an initialized tree node.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Multicast Member Info/osm_mcm_info_delete
* NAME
*	osm_mcm_info_delete
*
* DESCRIPTION
*	Destroys and deallocates the specified object.
*
* SYNOPSIS
*/
void osm_mcm_info_delete(IN osm_mcm_info_t * const p_mcm);
/*
* PARAMETERS
*	p_mcm
*		Pointer to the object to destroy.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

END_C_DECLS
#endif				/* _OSM_MCM_INFO_H_ */
