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
 * HEADER: dapl_sp_util.h
 *
 * PURPOSE: Utility defs & routines for the PSP & RSP data structure
 *
 * $Id:$
 *
 **********************************************************************/

#ifndef _DAPL_PSP_UTIL_H_
#define _DAPL_PSP_UTIL_H_

DAPL_SP *dapls_sp_alloc (
	IN DAPL_IA		*ia_ptr,
	IN DAT_BOOLEAN		is_psp );

void dapls_sp_free_sp (
	IN DAPL_SP		*sp_ptr );

void dapl_sp_link_cr (
	IN DAPL_SP		*sp_ptr,
	IN DAPL_CR		*cr_ptr );

DAPL_CR *dapl_sp_search_cr (
	IN DAPL_SP		*sp_ptr,
	IN  dp_ib_cm_handle_t   ib_cm_handle );

void dapl_sp_remove_cr (
	IN  DAPL_SP		*sp_ptr,
	IN  DAPL_CR		*cr_ptr );

void dapl_sp_remove_ep (
	IN  DAPL_EP		*ep_ptr );

#endif /* _DAPL_PSP_UTIL_H_ */
