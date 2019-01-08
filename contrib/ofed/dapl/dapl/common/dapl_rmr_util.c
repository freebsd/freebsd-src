/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a 
 *    copy of which is in the file LICENSE3.txt in the root directory. The 
 *    license is also available from the Open Source Initiative, see
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

#include "dapl_rmr_util.h"
#include "dapl_ia_util.h"

DAPL_RMR *dapl_rmr_alloc(IN DAPL_PZ * pz)
{
	DAPL_RMR *rmr;

	/* Allocate LMR */
	rmr = (DAPL_RMR *) dapl_os_alloc(sizeof(DAPL_RMR));
	if (NULL == rmr) {
		return (NULL);
	}

	/* zero the structure */
	dapl_os_memzero(rmr, sizeof(DAPL_RMR));

	/*
	 * initialize the header
	 */
	rmr->header.provider = pz->header.provider;
	rmr->header.magic = DAPL_MAGIC_RMR;
	rmr->header.handle_type = DAT_HANDLE_TYPE_RMR;
	rmr->header.owner_ia = pz->header.owner_ia;
	rmr->header.user_context.as_64 = 0;
	rmr->header.user_context.as_ptr = 0;
	dapl_llist_init_entry(&rmr->header.ia_list_entry);
	dapl_ia_link_rmr(rmr->header.owner_ia, rmr);
	dapl_os_lock_init(&rmr->header.lock);

	/* 
	 * initialize the body 
	 */
	rmr->param.ia_handle = (DAT_IA_HANDLE) pz->header.owner_ia;
	rmr->param.pz_handle = (DAT_PZ_HANDLE) pz;
	rmr->param.lmr_triplet.lmr_context = 0;
	rmr->param.lmr_triplet.virtual_address = 0;
	rmr->param.lmr_triplet.segment_length = 0;

	rmr->param.mem_priv = 0;
	rmr->pz = pz;
	rmr->lmr = NULL;

	return (rmr);
}

void dapl_rmr_dealloc(IN DAPL_RMR * rmr)
{
	rmr->header.magic = DAPL_MAGIC_INVALID;	/* reset magic to prevent reuse */

	dapl_ia_unlink_rmr(rmr->header.owner_ia, rmr);
	dapl_os_lock_destroy(&rmr->header.lock);

	dapl_os_free((void *)rmr, sizeof(DAPL_RMR));
}
