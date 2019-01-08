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
 * HEADER: dat_dictionary.h
 *
 * PURPOSE: dictionary data structure
 *
 * $Id: dat_dictionary.h,v 1.10 2005/03/24 05:58:27 jlentini Exp $
 **********************************************************************/

#ifndef _DAT_DICTIONARY_H_
#define _DAT_DICTIONARY_H_


#include "dat_osd.h"


/*********************************************************************
 *                                                                   *
 * Typedefs                                                          *
 *                                                                   *
 *********************************************************************/

typedef struct DAT_DICTIONARY   DAT_DICTIONARY;
typedef void *                  DAT_DICTIONARY_DATA;
typedef void *                  DAT_DICTIONARY_ENTRY;


/*********************************************************************
 *                                                                   *
 * Function Prototypes                                               *
 *                                                                   *
 *********************************************************************/

extern DAT_RETURN
dat_dictionary_create (
    OUT DAT_DICTIONARY **pp_dictionary);

extern DAT_RETURN
dat_dictionary_destroy (
    IN  DAT_DICTIONARY *p_dictionary);

extern DAT_RETURN
dat_dictionary_size (
    IN  DAT_DICTIONARY *p_dictionary,
    OUT DAT_COUNT *p_size);

extern DAT_RETURN
dat_dictionary_entry_create (
    OUT DAT_DICTIONARY_ENTRY *p_entry);

extern DAT_RETURN
dat_dictionary_entry_destroy (
    IN  DAT_DICTIONARY_ENTRY entry);

extern DAT_RETURN
dat_dictionary_insert (
    IN  DAT_DICTIONARY *p_dictionary,
    IN  DAT_DICTIONARY_ENTRY entry,
    IN  const DAT_PROVIDER_INFO *key,
    IN  DAT_DICTIONARY_DATA data);

extern DAT_RETURN
dat_dictionary_search (
    IN  DAT_DICTIONARY *p_dictionary,
    IN  const DAT_PROVIDER_INFO *key,
    OUT DAT_DICTIONARY_DATA *p_data);

extern DAT_RETURN
dat_dictionary_enumerate (
    IN  DAT_DICTIONARY *p_dictionary,
    IN  DAT_DICTIONARY_DATA array[],
    IN  DAT_COUNT array_size);


extern DAT_RETURN
dat_dictionary_remove (
    IN  DAT_DICTIONARY *p_dictionary,
    IN  DAT_DICTIONARY_ENTRY *p_entry,
    IN  const DAT_PROVIDER_INFO *key,
    OUT DAT_DICTIONARY_DATA *p_data);

#endif
