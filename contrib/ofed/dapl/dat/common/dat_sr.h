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
 * HEADER: dat_sr.h
 *
 * PURPOSE: static registry (SR) inteface declarations
 *
 * $Id: dat_sr.h,v 1.12 2005/03/24 05:58:28 jlentini Exp $
 **********************************************************************/

#ifndef _DAT_SR_H_
#define _DAT_SR_H_


#include <dat2/udat.h>
#include <dat2/dat_registry.h>

#include "dat_osd.h"

/*********************************************************************
 *                                                                   *
 * Strucutres                                                        *
 *                                                                   *
 *********************************************************************/

typedef struct DAT_SR_ENTRY
{
    DAT_PROVIDER_INFO 		info;
    char * 			lib_path;
    char * 			ia_params;
    DAT_OS_LIBRARY_HANDLE 	lib_handle;
    DAT_PROVIDER_INIT_FUNC 	init_func;
    DAT_PROVIDER_FINI_FUNC	fini_func;
    DAT_COUNT 			ref_count;
    struct DAT_SR_ENTRY         *next;
} DAT_SR_ENTRY;


/*********************************************************************
 *                                                                   *
 * Function Declarations                                             *
 *                                                                   *
 *********************************************************************/

extern DAT_RETURN
dat_sr_init ( void );

extern DAT_RETURN
dat_sr_fini ( void );

extern DAT_RETURN
dat_sr_insert (
    IN  const DAT_PROVIDER_INFO *info,
    IN  DAT_SR_ENTRY 		*entry );

extern DAT_RETURN
dat_sr_size (
    OUT DAT_COUNT               *size);

extern DAT_RETURN
dat_sr_list (
    IN  DAT_COUNT               max_to_return,
    OUT DAT_COUNT               *entries_returned,
    OUT DAT_PROVIDER_INFO       * (dat_provider_list[]) );

extern DAT_RETURN
dat_sr_provider_open (
    IN  const DAT_PROVIDER_INFO *info );

extern DAT_RETURN
dat_sr_provider_close (
    IN  const DAT_PROVIDER_INFO *info );


#endif
