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
 * MODULE: dat_sr.c
 *
 * PURPOSE: static registry implementation
 *
 * $Id: dat_sr.c,v 1.17 2005/03/24 05:58:27 jlentini Exp $
 **********************************************************************/

#include "dat_sr.h"

#include "dat_dictionary.h"
#include "udat_sr_parser.h"

/*********************************************************************
 *                                                                   *
 * Global Variables                                                  *
 *                                                                   *
 *********************************************************************/

static DAT_OS_LOCK g_sr_lock;
static DAT_DICTIONARY *g_sr_dictionary = NULL;

/*********************************************************************
 *                                                                   *
 * External Functions                                                *
 *                                                                   *
 *********************************************************************/

//***********************************************************************
// Function: dat_sr_init
//***********************************************************************

DAT_RETURN dat_sr_init(void)
{
	DAT_RETURN status;

	status = dat_os_lock_init(&g_sr_lock);
	if (DAT_SUCCESS != status) {
		return status;
	}

	status = dat_dictionary_create(&g_sr_dictionary);
	if (DAT_SUCCESS != status) {
		return status;
	}

	/*
	 * Since DAT allows providers to be loaded by either the static
	 * registry or explicitly through OS dependent methods, do not
	 * return an error if no providers are loaded via the static registry.
	 */

	(void)dat_sr_load();

	return DAT_SUCCESS;
}

//***********************************************************************
// Function: dat_sr_fini
//***********************************************************************

extern DAT_RETURN dat_sr_fini(void)
{
	DAT_RETURN status;

	status = dat_os_lock_destroy(&g_sr_lock);
	if (DAT_SUCCESS != status) {
		return status;
	}

	status = dat_dictionary_destroy(g_sr_dictionary);
	if (DAT_SUCCESS != status) {
		return status;
	}

	return DAT_SUCCESS;
}

//***********************************************************************
// Function: dat_sr_insert
//***********************************************************************

extern DAT_RETURN
dat_sr_insert(IN const DAT_PROVIDER_INFO * info, IN DAT_SR_ENTRY * entry)
{
	DAT_RETURN status;
	DAT_SR_ENTRY *data;
	DAT_OS_SIZE lib_path_size;
	DAT_OS_SIZE lib_path_len;
	DAT_OS_SIZE ia_params_size;
	DAT_OS_SIZE ia_params_len;
	DAT_DICTIONARY_ENTRY dict_entry;
	DAT_DICTIONARY_DATA prev_data;

	if (NULL == (data = dat_os_alloc(sizeof(DAT_SR_ENTRY)))) {
		status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	dat_os_memset(data, '\0', sizeof(DAT_SR_ENTRY));

	lib_path_len = strlen(entry->lib_path);
	lib_path_size = (lib_path_len + 1) * sizeof(char);

	if (NULL == (data->lib_path = dat_os_alloc(lib_path_size))) {
		status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	dat_os_strncpy(data->lib_path, entry->lib_path, lib_path_len);
	data->lib_path[lib_path_len] = '\0';

	ia_params_len = strlen(entry->ia_params);
	ia_params_size = (ia_params_len + 1) * sizeof(char);

	if (NULL == (data->ia_params = dat_os_alloc(ia_params_size))) {
		status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	dat_os_strncpy(data->ia_params, entry->ia_params, ia_params_len);
	data->ia_params[ia_params_len] = '\0';

	data->info = entry->info;
	data->lib_handle = entry->lib_handle;
	data->ref_count = entry->ref_count;
	data->next = NULL;

	dict_entry = NULL;
	status = dat_dictionary_entry_create(&dict_entry);
	if (DAT_SUCCESS != status) {
		goto bail;
	}

	dat_os_lock(&g_sr_lock);

	status = dat_dictionary_search(g_sr_dictionary, info, &prev_data);
	if (DAT_SUCCESS == status) {
		/* We already have a dictionary entry, so we don't need a new one.
		 * This means there are multiple duplicate names in dat.conf,
		 * but presumably they have different paths. Simply link the
		 * new entry at the end of the chain of like-named entries.
		 */
		(void)dat_dictionary_entry_destroy(dict_entry);
		dict_entry = NULL;

		/* Find the next available slot in this chain */
		while (NULL != ((DAT_SR_ENTRY *) prev_data)->next) {
			prev_data = ((DAT_SR_ENTRY *) prev_data)->next;
		}
		dat_os_assert(NULL != prev_data);
		((DAT_SR_ENTRY *) prev_data)->next = data;
	} else {
		status = dat_dictionary_insert(g_sr_dictionary,
					       dict_entry,
					       info,
					       (DAT_DICTIONARY_DATA *) data);
	}

	dat_os_unlock(&g_sr_lock);

      bail:
	if (DAT_SUCCESS != status) {
		if (NULL != data) {
			if (NULL != data->lib_path) {
				dat_os_free(data->lib_path, lib_path_size);
			}

			if (NULL != data->ia_params) {
				dat_os_free(data->ia_params, ia_params_size);
			}

			dat_os_free(data, sizeof(DAT_SR_ENTRY));
		}

		if (NULL != dict_entry) {
			(void)dat_dictionary_entry_destroy(dict_entry);
		}
	}

	return status;
}

//***********************************************************************
// Function: dat_sr_size
//***********************************************************************

extern DAT_RETURN dat_sr_size(OUT DAT_COUNT * size)
{
	return dat_dictionary_size(g_sr_dictionary, size);
}

//***********************************************************************
// Function: dat_sr_list
//***********************************************************************

extern DAT_RETURN
dat_sr_list(IN DAT_COUNT max_to_return,
	    OUT DAT_COUNT * entries_returned,
	    OUT DAT_PROVIDER_INFO * (dat_provider_list[]))
{
	DAT_SR_ENTRY **array;
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
		status = dat_dictionary_size(g_sr_dictionary, &array_size);
		if (DAT_SUCCESS != status) {
			goto bail;
		}

		if (array_size == 0) {
			status = DAT_SUCCESS;
			goto bail;
		}

		array = dat_os_alloc(array_size * sizeof(DAT_SR_ENTRY *));
		if (array == NULL) {
			status =
			    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES,
				      DAT_RESOURCE_MEMORY);
			goto bail;
		}

		dat_os_lock(&g_sr_lock);

		status = dat_dictionary_enumerate(g_sr_dictionary,
						  (DAT_DICTIONARY_DATA *) array,
						  array_size);

		dat_os_unlock(&g_sr_lock);

		if (DAT_SUCCESS == status) {
			break;
		} else {
			dat_os_free(array, array_size * sizeof(DAT_SR_ENTRY *));
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
		dat_os_free(array, array_size * sizeof(DAT_SR_ENTRY *));
	}

	return status;
}

//***********************************************************************
// Function: dat_sr_provider_open
//***********************************************************************

extern DAT_RETURN dat_sr_provider_open(IN const DAT_PROVIDER_INFO * info)
{
	DAT_RETURN status;
	DAT_SR_ENTRY *data;
	DAT_DICTIONARY_DATA dict_data;

	dat_os_lock(&g_sr_lock);

	status = dat_dictionary_search(g_sr_dictionary, info, &dict_data);

	if (DAT_SUCCESS == status) {
		data = (DAT_SR_ENTRY *) dict_data;
		while (data != NULL) {
			if (0 == data->ref_count) {
				/*
				 * Try to open the path. If it fails, try the next
				 * path in the chain. Only the first successful library
				 * open matters, the others will be unused.
				 */
				dat_os_dbg_print(DAT_OS_DBG_TYPE_SR,
						 "DAT Registry: IA %s, trying to load library %s\n",
						 data->info.ia_name,
						 data->lib_path);

				status = dat_os_library_load(data->lib_path,
							     &data->lib_handle);

				if (status == DAT_SUCCESS) {
#ifdef DAT_DBG
					dat_os_dbg_print(DAT_OS_DBG_TYPE_SR,
							 "DAT2 Registry: IA %s, loaded library %s\n",
							 data->info.ia_name,
							 data->lib_path);
#endif
					data->ref_count++;
					data->init_func =
					    dat_os_library_sym(data->lib_handle,
							       DAT_PROVIDER_INIT_FUNC_STR);
					data->fini_func =
					    dat_os_library_sym(data->lib_handle,
							       DAT_PROVIDER_FINI_FUNC_STR);
					/* Warning: DAT and DAPL libraries not ext compatible */
#ifdef DAT_EXTENSIONS
					{
						void *fncptr;

						fncptr =
						    dat_os_library_sym(data->
								       lib_handle,
								       "dapl_extensions");

						if ((dat_os_library_error() !=
						     NULL)
						    || (fncptr == NULL)) {
							dat_os_dbg_print
							    (DAT_OS_DBG_TYPE_SR,
							     "DAT Registry: WARNING: library %s, "
							     "extended DAT expected extended uDAPL: %s\n",
							     data->lib_path,
							     strerror(errno));
						}
					}
#endif
					if (NULL != data->init_func) {
						(*data->init_func) (&data->info,
								    data->
								    ia_params);
					} else {
						dat_os_dbg_print
						    (DAT_OS_DBG_TYPE_SR,
						     "DAT Registry: Cannot find library init func (%s)\n",
						     DAT_PROVIDER_INIT_FUNC_STR);
					}

					/* exit after we find the first valid entry */
					break;
				} else {
					dat_os_dbg_print(DAT_OS_DBG_TYPE_SR,
							 "DAT Registry: static registry unable to "
							 "load library %s\n",
							 data->lib_path);
				}
			} else {
				data->ref_count++;
				break;
			}
			data = data->next;
		}
	}

	dat_os_unlock(&g_sr_lock);

	return status;
}

//***********************************************************************
// Function: dat_sr_provider_close
//***********************************************************************

extern DAT_RETURN dat_sr_provider_close(IN const DAT_PROVIDER_INFO * info)
{
	DAT_RETURN status;
	DAT_SR_ENTRY *data;
	DAT_DICTIONARY_DATA dict_data;

	dat_os_lock(&g_sr_lock);

	status = dat_dictionary_search(g_sr_dictionary, info, &dict_data);

	if (DAT_SUCCESS == status) {
		data = (DAT_SR_ENTRY *) dict_data;
		while (data != NULL) {
			if (1 == data->ref_count) {
				dat_os_dbg_print(DAT_OS_DBG_TYPE_SR,
						 "DAT Registry: IA %s, unloading library %s\n",
						 data->info.ia_name,
						 data->lib_path);

				if (NULL != data->fini_func) {
					(*data->fini_func) (&data->info);
				}

				status =
				    dat_os_library_unload(data->lib_handle);
				if (status == DAT_SUCCESS) {
					data->ref_count--;
				}
				break;
			} else if (data->ref_count > 0) {
				data->ref_count--;
				break;
			}
			data = data->next;
		}
	}

	dat_os_unlock(&g_sr_lock);

	return status;
}
