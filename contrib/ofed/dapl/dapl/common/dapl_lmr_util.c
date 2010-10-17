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
 * MODULE: dapl_lmr_util.c
 *
 * PURPOSE: Memory management support routines
 * Description: Support routines for LMR functions
 *
 * $Id:$
 **********************************************************************/

#include "dapl_lmr_util.h"
#include "dapl_ia_util.h"

DAPL_LMR *dapl_lmr_alloc(IN DAPL_IA * ia,
			 IN DAT_MEM_TYPE mem_type,
			 IN DAT_REGION_DESCRIPTION region_desc,
			 IN DAT_VLEN length,
			 IN DAT_PZ_HANDLE pz_handle,
			 IN DAT_MEM_PRIV_FLAGS mem_priv)
{
	DAPL_LMR *lmr;

	/* Allocate LMR */
	lmr = (DAPL_LMR *) dapl_os_alloc(sizeof(DAPL_LMR));
	if (NULL == lmr) {
		return (NULL);
	}

	/* zero the structure */
	dapl_os_memzero(lmr, sizeof(DAPL_LMR));

	/*
	 * initialize the header
	 */
	lmr->header.provider = ia->header.provider;
	lmr->header.magic = DAPL_MAGIC_LMR;
	lmr->header.handle_type = DAT_HANDLE_TYPE_LMR;
	lmr->header.owner_ia = ia;
	lmr->header.user_context.as_64 = 0;
	lmr->header.user_context.as_ptr = NULL;
	dapl_llist_init_entry(&lmr->header.ia_list_entry);
	dapl_ia_link_lmr(ia, lmr);
	dapl_os_lock_init(&lmr->header.lock);

	/* 
	 * initialize the body 
	 */
	lmr->param.ia_handle = (DAT_IA_HANDLE) ia;
	lmr->param.mem_type = mem_type;
	lmr->param.region_desc = region_desc;
	lmr->param.length = length;
	lmr->param.pz_handle = pz_handle;
	lmr->param.mem_priv = mem_priv;
	dapl_os_atomic_set(&lmr->lmr_ref_count, 0);

	return (lmr);
}

void dapl_lmr_dealloc(IN DAPL_LMR * lmr)
{
	lmr->header.magic = DAPL_MAGIC_INVALID;	/* reset magic to prevent reuse */
	dapl_ia_unlink_lmr(lmr->header.owner_ia, lmr);
	dapl_os_lock_destroy(&lmr->header.lock);

	dapl_os_free((void *)lmr, sizeof(DAPL_LMR));
}
