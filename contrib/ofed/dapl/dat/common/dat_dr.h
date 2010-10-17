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
 * HEADER: dat_dr.h
 *
 * PURPOSE: dynamic registry interface declarations
 *
 * $Id: dat_dr.h,v 1.12 2005/03/24 05:58:27 jlentini Exp $
 **********************************************************************/

#ifndef __DAT_DR_H__
#define __DAT_DR_H__


#include "dat_osd.h"
#include <dat2/dat_registry.h> /* Provider API function prototypes */


/*********************************************************************
 *                                                                   *
 * Strucutres                                                        *
 *                                                                   *
 *********************************************************************/

typedef struct
{
    DAT_COUNT 			ref_count;
    DAT_IA_OPEN_FUNC 		ia_open_func;
    DAT_PROVIDER_INFO 		info;
} DAT_DR_ENTRY;


/*********************************************************************
 *                                                                   *
 * Function Declarations                                             *
 *                                                                   *
 *********************************************************************/

extern DAT_RETURN
dat_dr_init ( void );

extern DAT_RETURN
dat_dr_fini ( void );

extern DAT_RETURN
dat_dr_insert (
    IN  const DAT_PROVIDER_INFO *info,
    IN  DAT_DR_ENTRY 		*entry );

extern DAT_RETURN
dat_dr_remove (
    IN  const DAT_PROVIDER_INFO *info );


extern DAT_RETURN
dat_dr_provider_open (
    IN  const DAT_PROVIDER_INFO *info,
    OUT DAT_IA_OPEN_FUNC	*p_ia_open_func );

extern DAT_RETURN
dat_dr_provider_close (
    IN  const DAT_PROVIDER_INFO *info);

extern DAT_RETURN
dat_dr_size (
    OUT DAT_COUNT               *size);

extern DAT_RETURN
dat_dr_list (
    IN  DAT_COUNT               max_to_return,
    OUT DAT_COUNT               *entries_returned,
    OUT DAT_PROVIDER_INFO       * (dat_provider_list[]) );

#endif
