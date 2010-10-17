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

/**********************************************************************
 *
 * HEADER: dat_init.h
 *
 * PURPOSE: DAT registry global data
 *
 * $Id: dat_init.h,v 1.16 2005/03/24 05:58:27 jlentini Exp $
 **********************************************************************/

#ifndef _DAT_INIT_H_
#define _DAT_INIT_H_

#include "dat_osd.h"

/*********************************************************************
 *                                                                   *
 * Enumerations                                                      *
 *                                                                   *
 *********************************************************************/

typedef enum
{
    DAT_MODULE_STATE_UNINITIALIZED,
    DAT_MODULE_STATE_INITIALIZING,
    DAT_MODULE_STATE_INITIALIZED,
    DAT_MODULE_STATE_DEINITIALIZING,
    DAT_MODULE_STATE_DEINITIALIZED
} DAT_MODULE_STATE;

/*********************************************************************
 *                                                                   *
 * Function Prototypes                                               *
 *                                                                   *
 *********************************************************************/

DAT_MODULE_STATE
dat_module_get_state ( void ) ;

#if defined(_MSC_VER) || defined(_WIN64) || defined(_WIN32)
/* NT. MSC compiler, Win32/64 platform */
void
dat_init ( void );

void
dat_fini ( void );

#else /* GNU C */

void
dat_init ( void ) __attribute__ ((constructor));

void
dat_fini ( void ) __attribute__ ((destructor));
#endif

extern DAT_RETURN 
dats_handle_vector_init ( void );

extern DAT_IA_HANDLE
dats_set_ia_handle (
	IN  DAT_IA_HANDLE		ia_handle);

extern DAT_RETURN 
dats_get_ia_handle(
	IN	DAT_IA_HANDLE		handle,
	OUT	DAT_IA_HANDLE		*ia_handle_p);

extern DAT_BOOLEAN
dats_is_ia_handle (
	IN  DAT_HANDLE          	dat_handle);

extern DAT_RETURN 
dats_free_ia_handle(
	IN	DAT_IA_HANDLE		handle);

#endif
