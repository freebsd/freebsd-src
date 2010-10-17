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
 * HEADER: dapl_ia_util.h
 *
 * PURPOSE: Utility defs & routines for the IA data structure
 *
 * $Id:$
 **********************************************************************/

#ifndef _DAPL_IA_UTIL_H_
#define _DAPL_IA_UTIL_H_

#include "dapl.h"

DAPL_IA *
dapl_ia_alloc ( 
	DAT_PROVIDER	*provider,
	DAPL_HCA	*hca_ptr) ;

DAT_RETURN 
dapl_ia_abrupt_close (
	IN DAPL_IA 	*ia_ptr ) ;

DAT_RETURN 
dapl_ia_graceful_close (
    	IN DAPL_IA 	*ia_ptr ) ;

void
dapls_ia_free ( DAPL_IA *ia_ptr ) ;

void
dapl_ia_link_ep (
	IN DAPL_IA 	*ia_ptr,
	IN DAPL_EP	*ep_info ) ;

void
dapl_ia_unlink_ep (
	IN DAPL_IA 	*ia_ptr,
	IN DAPL_EP	*ep_info ) ;

void
dapl_ia_link_srq (
	IN DAPL_IA 	*ia_ptr,
	IN DAPL_SRQ	*srq_ptr ) ;

void
dapl_ia_unlink_srq (
	IN DAPL_IA 	*ia_ptr,
	IN DAPL_SRQ	*srq_ptr ) ;

void
dapl_ia_link_lmr (
	IN DAPL_IA 	*ia_ptr,
	IN DAPL_LMR	*lmr_info ) ;

void
dapl_ia_unlink_lmr (
	IN DAPL_IA 	*ia_ptr,
	IN DAPL_LMR	*lmr_info ) ;

void
dapl_ia_link_rmr (
	IN DAPL_IA 	*ia_ptr,
	IN DAPL_RMR	*rmr_info ) ;

void
dapl_ia_unlink_rmr (
	IN DAPL_IA 	*ia_ptr,
	IN DAPL_RMR	*rmr_info ) ;

void
dapl_ia_link_pz (
	IN DAPL_IA 	*ia_ptr,
	IN DAPL_PZ	*pz_info ) ;

void
dapl_ia_unlink_pz (
	IN DAPL_IA 	*ia_ptr,
	IN DAPL_PZ	*pz_info ) ;

void
dapl_ia_link_evd (
	IN DAPL_IA 	*ia_ptr,
	IN DAPL_EVD	*evd_info ) ;

void
dapl_ia_unlink_evd (
	IN DAPL_IA 	*ia_ptr,
	IN DAPL_EVD	*evd_info ) ;

void
dapl_ia_link_cno (
	IN DAPL_IA 	*ia_ptr,
	IN DAPL_CNO	*cno_info ) ;

void
dapl_ia_unlink_cno (
	IN DAPL_IA 	*ia_ptr,
	IN DAPL_CNO	*cno_info ) ;

void
dapl_ia_link_psp (
	IN DAPL_IA 	*ia_ptr,
	IN DAPL_SP	*sp_info ) ;

void
dapls_ia_unlink_sp (
	IN DAPL_IA 	*ia_ptr,
	IN DAPL_SP	*sp_info ) ;

void
dapl_ia_link_rsp (
	IN DAPL_IA 	*ia_ptr,
	IN DAPL_SP	*sp_info ) ;

DAPL_SP *
dapls_ia_sp_search (
	IN	DAPL_IA		   *ia_ptr,
	IN	DAT_CONN_QUAL	   conn_qual,
	IN	DAT_BOOLEAN	   is_psp ) ;

DAT_RETURN
dapls_ia_setup_callbacks (
    IN	DAPL_IA		*ia_ptr,
    IN	DAPL_EVD	*async_evd_ptr );

DAT_RETURN
dapls_ia_teardown_callbacks (
    IN	DAPL_IA		*ia_ptr );

#endif
