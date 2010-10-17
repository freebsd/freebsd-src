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
 * HEADER: dapl_hca_util.h
 *
 * PURPOSE: Utility defs & routines for the HCA data structure
 *
 * $Id:$
 **********************************************************************/

#ifndef _DAPL_HCA_UTIL_H_
#define _DAPL_HCA_UTIL_H_

#include "dapl.h"

DAPL_HCA *
dapl_hca_alloc ( char 	*name,
                 char 	*port ) ;

void
dapl_hca_free ( DAPL_HCA	*hca_ptr ) ;

void
dapl_hca_link_ia (
	IN DAPL_HCA 	*hca_ptr,
	IN DAPL_IA	*ia_info ) ;

void
dapl_hca_unlink_ia (
	IN DAPL_HCA 	*hca_ptr,
	IN DAPL_IA	*ia_info ) ;


#endif
