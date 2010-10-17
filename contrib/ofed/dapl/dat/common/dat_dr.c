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
 * MODULE: dat_dr.c
 *
 * PURPOSE: dynamic registry implementation
 *
 * $Id: dat_dr.c,v 1.17 2005/03/24 05:58:27 jlentini Exp $
 **********************************************************************/

#include <dat2/dat_platform_specific.h>
#include "dat_dr.h"
#include "dat_dictionary.h"

/*********************************************************************
 *                                                                   *
 * Global Variables                                                  *
 *                                                                   *
 *********************************************************************/

static DAT_OS_LOCK g_dr_lock;
static DAT_DICTIONARY *g_dr_dictionary = NULL;

/*********************************************************************
 *                                                                   *
 * External Functions                                                *
 *                                                                   *
 *********************************************************************/

//***********************************************************************
// Function: dat_dr_init
//***********************************************************************

DAT_RETURN dat_dr_init(void)
{
	DAT_RETURN status;

	status = dat_os_lock_init(&g_dr_lock);
	if (DAT_SUCCESS != status) {
		return status;
	}

	status = dat_dictionary_create(&g_dr_dictionary);
	if (DAT_SUCCESS != status) {
		return status;
	}

	return DAT_SUCCESS;
}

//***********************************************************************
// Function: dat_dr_fini
//***********************************************************************

DAT_RETURN dat_dr_fini(void)
{
	DAT_RETURN status;

	status = dat_os_lock_destroy(&g_dr_lock);
	if (DAT_SUCCESS != status) {
		return status;
	}

	status = dat_dictionary_destroy(g_dr_dictionary);
	if (DAT_SUCCESS != status) {
		return status;
	}

	return DAT_SUCCESS;
}

//***********************************************************************
// Function: dat_dr_insert
//***********************************************************************

DAT_RETURN
dat_dr_insert(IN const DAT_PROVIDER_INFO * info, IN DAT_DR_ENTRY * entry)
{
	DAT_RETURN status;
	DAT_DICTIONARY_ENTRY dict_entry = NULL;
	DAT_DR_ENTRY *data;

	data = dat_os_alloc(sizeof(DAT_DR_ENTRY));
	if (NULL == data) {
		status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	*data = *entry;

	status = dat_dictionary_entry_create(&dict_entry);
	if (DAT_SUCCESS != status) {
		goto bail;
	}

	dat_os_lock(&g_dr_lock);

	status = dat_dictionary_insert(g_dr_dictionary,
				       dict_entry,
				       info, (DAT_DICTIONARY_DATA *) data);

	dat_os_unlock(&g_dr_lock);

      bail:
	if (DAT_SUCCESS != status) {
		if (NULL != data) {
			dat_os_free(data, sizeof(DAT_DR_ENTRY));
		}

		if (NULL != dict_entry) {
			(void)dat_dictionary_entry_destroy(dict_entry);
		}
	}

	return status;
}

//***********************************************************************
// Function: dat_dr_remove
//***********************************************************************

DAT_RETURN dat_dr_remove(IN const DAT_PROVIDER_INFO * info)
{
	DAT_DICTIONARY_ENTRY dict_entry;
	DAT_RETURN status;
	DAT_DICTIONARY_DATA data;

	dict_entry = NULL;
	dat_os_lock(&g_dr_lock);

	status = dat_dictionary_search(g_dr_dictionary, info, &data);

	if (DAT_SUCCESS != status) {
		/* return status from dat_dictionary_search() */
		goto bail;
	}

	if (0 != ((DAT_DR_ENTRY *) data)->ref_count) {
		status = DAT_ERROR(DAT_PROVIDER_IN_USE, 0);
		goto bail;
	}

	status = dat_dictionary_remove(g_dr_dictionary,
				       &dict_entry, info, &data);
	if (DAT_SUCCESS != status) {
		/* return status from dat_dictionary_remove() */
		goto bail;
	}

	dat_os_free(data, sizeof(DAT_DR_ENTRY));

      bail:
	dat_os_unlock(&g_dr_lock);

	if (NULL != dict_entry) {
		(void)dat_dictionary_entry_destroy(dict_entry);
	}

	return status;
}

//***********************************************************************
// Function: dat_dr_provider_open
//***********************************************************************

DAT_RETURN
dat_dr_provider_open(IN const DAT_PROVIDER_INFO * info,
		     OUT DAT_IA_OPEN_FUNC * p_ia_open_func)
{
	DAT_RETURN status;
	DAT_DICTIONARY_DATA data;

	dat_os_lock(&g_dr_lock);
	status = dat_dictionary_search(g_dr_dictionary, info, &data);
	dat_os_unlock(&g_dr_lock);

	if (DAT_SUCCESS == status) {
		((DAT_DR_ENTRY *) data)->ref_count++;
		*p_ia_open_func = ((DAT_DR_ENTRY *) data)->ia_open_func;
	}

	return status;
}

//***********************************************************************
// Function: dat_dr_provider_close
//***********************************************************************

DAT_RETURN dat_dr_provider_close(IN const DAT_PROVIDER_INFO * info)
{
	DAT_RETURN status;
	DAT_DICTIONARY_DATA data;

	dat_os_lock(&g_dr_lock);
	status = dat_dictionary_search(g_dr_dictionary, info, &data);
	dat_os_unlock(&g_dr_lock);

	if (DAT_SUCCESS == status) {
		((DAT_DR_ENTRY *) data)->ref_count--;
	}

	return status;
}

//***********************************************************************
// Function: dat_dr_size
//***********************************************************************

DAT_RETURN dat_dr_size(OUT DAT_COUNT * size)
{
	return dat_dictionary_size(g_dr_dictionary, size);
}

//***********************************************************************
// Function: dat_dr_list
//***********************************************************************

DAT_RETURN
dat_dr_list(IN DAT_COUNT max_to_return,
	    OUT DAT_COUNT * entries_returned,
	    OUT DAT_PROVIDER_INFO * (dat_provider_list[]))
{
	DAT_DR_ENTRY **array;
	DAT_COUNT array_size;
	DAT_COUNT i;
	DAT_RETURN status;

	array = NULL;
	status = DAT_SUCCESS;

	/* The dictionary size may increase between the call to      */
	/* dat_dictionary_size() and dat_dictionary_enumerate().     */
	/* Therefore we loop until a successful enumeration is made. */
	*entries_returned = 0;
	for (;;) {
		status = dat_dictionary_size(g_dr_dictionary, &array_size);
		if (status != DAT_SUCCESS) {
			goto bail;
		}

		if (array_size == 0) {
			status = DAT_SUCCESS;
			goto bail;
		}

		array = dat_os_alloc(array_size * sizeof(DAT_DR_ENTRY *));
		if (array == NULL) {
			status =
			    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES,
				      DAT_RESOURCE_MEMORY);
			goto bail;
		}

		dat_os_lock(&g_dr_lock);

		status = dat_dictionary_enumerate(g_dr_dictionary,
						  (DAT_DICTIONARY_DATA *) array,
						  array_size);

		dat_os_unlock(&g_dr_lock);

		if (DAT_SUCCESS == status) {
			break;
		} else {
			dat_os_free(array, array_size * sizeof(DAT_DR_ENTRY *));
			array = NULL;
			continue;
		}
	}

	for (i = 0; (i < max_to_return) && (i < array_size); i++) {
		if (NULL == dat_provider_list[i]) {
			status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
			goto bail;
		}

		*dat_provider_list[i] = array[i]->info;
	}

	*entries_returned = i;

      bail:
	if (NULL != array) {
		dat_os_free(array, array_size * sizeof(DAT_DR_ENTRY *));
	}

	return status;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
