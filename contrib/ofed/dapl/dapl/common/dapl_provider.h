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
 * HEADER: dapl_provider.h
 *
 * PURPOSE: Provider function table
 * Description: DAT Interfaces to this provider
 *
 * $Id:$
 **********************************************************************/

#ifndef _DAPL_PROVIDER_H_
#define _DAPL_PROVIDER_H_

#include "dapl.h"


/*********************************************************************
 *                                                                   *
 * Structures                                                        *
 *                                                                   *
 *********************************************************************/

typedef struct DAPL_PROVIDER_LIST_NODE
{
    char                		name[DAT_NAME_MAX_LENGTH];
    DAT_PROVIDER 			data;
    struct DAPL_PROVIDER_LIST_NODE 	*next;
    struct DAPL_PROVIDER_LIST_NODE 	*prev;
} DAPL_PROVIDER_LIST_NODE;


typedef struct DAPL_PROVIDER_LIST
{
    DAPL_PROVIDER_LIST_NODE 		*head;
    DAPL_PROVIDER_LIST_NODE 		*tail;
    DAT_COUNT           		size;
} DAPL_PROVIDER_LIST;


/*********************************************************************
 *                                                                   *
 * Global Data                                                       *
 *                                                                   *
 *********************************************************************/

extern DAPL_PROVIDER_LIST 	g_dapl_provider_list;
extern DAT_PROVIDER 		g_dapl_provider_template;
extern int 			g_dapl_loopback_connection;


/*********************************************************************
 *                                                                   *
 * Function Prototypes                                               *
 *                                                                   *
 *********************************************************************/

extern DAT_RETURN
dapl_provider_list_create( void );

extern DAT_RETURN
dapl_provider_list_destroy( void );

extern DAT_COUNT
dapl_provider_list_size( void );

extern DAT_RETURN
dapl_provider_list_insert(
    IN  const char *name,
    OUT DAT_PROVIDER **p_data );

extern DAT_RETURN
dapl_provider_list_search(
    IN  const char *name,
    OUT DAT_PROVIDER **p_data );

extern DAT_RETURN
dapl_provider_list_remove(
    IN  const char *name );


#endif /* _DAPL_PROVIDER_H_ */
