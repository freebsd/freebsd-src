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
 * HEADER: dapl_name_service.h
 *
 * PURPOSE: Utility defs & routines supporting name services
 *
 * $Id:$
 *
 **********************************************************************/

#ifndef _DAPL_NAME_SERVICE_H_
#define _DAPL_NAME_SERVICE_H_

#include "dapl.h"

/*
 * Prototypes for name service routines
 */

DAT_RETURN dapls_ns_init (void);

#ifdef IBHOSTS_NAMING
DAT_RETURN dapls_ns_lookup_address (
	IN  DAPL_IA		*ia_ptr,
	IN  DAT_IA_ADDRESS_PTR	remote_ia_address,
	OUT ib_gid_t		*gid);
#else

/* routine is a no op if not using local name services */
#define dapls_ns_lookup_address(ia,ria,gid) DAT_SUCCESS

#endif /* IBHOSTS_NAMING */

#endif /* _DAPL_NAME_SERVICE_H_ */
