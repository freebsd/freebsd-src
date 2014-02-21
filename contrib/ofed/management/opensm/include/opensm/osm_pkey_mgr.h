/*
 * Copyright (c) 2006 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
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
 * 	Prototype for osm_pkey_mgr_process() function
 *	This is part of the OpenSM family of objects.
 */

#ifndef _OSM_PKEY_MGR_H_
#define _OSM_PKEY_MGR_H_

#include <opensm/osm_base.h>
#include <opensm/osm_opensm.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/****f* OpenSM: P_Key Manager/osm_pkey_mgr_process
* NAME
*	osm_pkey_mgr_process
*
* DESCRIPTION
*	This function enforces the pkey rules on the SM DB.
*
* SYNOPSIS
*/
osm_signal_t osm_pkey_mgr_process(IN osm_opensm_t * p_osm);
/*
* PARAMETERS
*	p_osm
*		[in] Pointer to an osm_opensm_t object.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

END_C_DECLS
#endif				/* _OSM_PKEY_MGR_H_ */
