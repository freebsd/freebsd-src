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
 * MODULE: dapl_init.c
 *
 * PURPOSE: Interface Adapter management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 2
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include <dat2/dat_registry.h>	/* Provider API function prototypes */
#include "dapl_hca_util.h"
#include "dapl_init.h"
#include "dapl_provider.h"
#include "dapl_mr_util.h"
#include "dapl_osd.h"
#include "dapl_adapter_util.h"
#include "dapl_name_service.h"
#include "dapl_timer_util.h"
#include "dapl_vendor.h"

/*
 * dapl_init
 *
 * initialize this provider
 * includes initialization of all global variables
 * as well as registering all supported IAs with the dat registry
 *
 * This function needs to be called once when the provider is loaded.
 *
 * Input:
 *	none
 *
 * Output:
 *	none
 *
 * Return Values:
 */
void dapl_init(void)
{
	DAT_RETURN dat_status;

	/* set up debug type */
	g_dapl_dbg_type = dapl_os_get_env_val("DAPL_DBG_TYPE",
					      DAPL_DBG_TYPE_ERR);
	/* set up debug destination */
	g_dapl_dbg_dest = dapl_os_get_env_val("DAPL_DBG_DEST",
					      DAPL_DBG_DEST_STDOUT);

	/* open log file on first logging call if necessary */
	if (g_dapl_dbg_dest & DAPL_DBG_DEST_SYSLOG)
		openlog("libdapl", LOG_ODELAY | LOG_PID | LOG_CONS, LOG_USER);

	dapl_log(DAPL_DBG_TYPE_UTIL, "dapl_init: dbg_type=0x%x,dbg_dest=0x%x\n",
		 g_dapl_dbg_type, g_dapl_dbg_dest);

	/* See if the user is on a loopback setup */
	g_dapl_loopback_connection = dapl_os_get_env_bool("DAPL_LOOPBACK");

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, "DAPL: %s Setting Loopback\n",
		     g_dapl_loopback_connection ? "" : "NOT");

	/* initialize verbs library */
	dapls_ib_init();

	/* initialize the timer */
	dapls_timer_init();

	/* Set up name services */
	dat_status = dapls_ns_init();
	if (DAT_SUCCESS != dat_status) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "dapls_ns_init failed %d\n", dat_status);
		goto bail;
	}

	/* initialize the provider list */
	dat_status = dapl_provider_list_create();

	if (DAT_SUCCESS != dat_status) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "dapl_provider_list_create failed %d\n",
			     dat_status);
		goto bail;
	}

	return;

      bail:
	dapl_dbg_log(DAPL_DBG_TYPE_ERR, "ERROR: dapl_init failed\n");
	return;
}

/*
 * dapl_fini
 *
 * finalize this provider
 * includes freeing of all global variables
 * as well as deregistering all supported IAs from the dat registry
 *
 * This function needs to be called once when the provider is loaded.
 *
 * Input:
 *	none
 *
 * Output:
 *	none
 *
 * Return Values:
 */
void dapl_fini(void)
{
	DAT_RETURN dat_status;

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, "DAPL: ENTER (dapl_fini)\n");

	dat_status = dapl_provider_list_destroy();
	if (DAT_SUCCESS != dat_status) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "dapl_provider_list_destroy failed %d\n",
			     dat_status);
	}

	dapls_ib_release();
	dapls_timer_release();

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, "DAPL: Exit (dapl_fini)\n");

	if (g_dapl_dbg_dest & DAPL_DBG_DEST_SYSLOG)
		closelog();

	return;
}

/*
 *
 * This function is called by the registry to initialize a provider
 *
 * The instance data string is expected to have the following form:
 *
 * <hca name> <port number>
 *
 */
void DAT_API
DAT_PROVIDER_INIT_FUNC_NAME(IN const DAT_PROVIDER_INFO * provider_info,
			    IN const char *instance_data)
{
	DAT_PROVIDER *provider;
	DAPL_HCA *hca_ptr;
	DAT_RETURN dat_status;
	char *data;
	char *name;
	char *port;
	unsigned int len;
	unsigned int i;

	data = NULL;
	provider = NULL;
	hca_ptr = NULL;

#if defined(_WIN32) || defined(_WIN64)
	/* initialize DAPL library here as when called from DLL context in DLLmain()
	 * the IB (ibal) call hangs.
	 */
	dapl_init();
#endif

	dat_status =
	    dapl_provider_list_insert(provider_info->ia_name, &provider);
	if (DAT_SUCCESS != dat_status) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "dat_provider_list_insert failed: %x\n",
			     dat_status);
		goto bail;
	}

	data = dapl_os_strdup(instance_data);
	if (NULL == data) {
		goto bail;
	}

	len = dapl_os_strlen(data);

	for (i = 0; i < len; i++) {
		if (' ' == data[i]) {
			data[i] = '\0';
			break;
		}
	}

	/* if the instance data did not have a valid format */
	if (i == len) {
		goto bail;
	}

	name = data;
	port = data + (i + 1);

	hca_ptr = dapl_hca_alloc(name, port);
	if (NULL == hca_ptr) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "%s() dapl_hca_alloc failed?\n");
		goto bail;
	}

	provider->extension = hca_ptr;
	dat_status = dat_registry_add_provider(provider, provider_info);
	if (DAT_SUCCESS != dat_status) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "dat_registry_add_provider failed: %x\n",
			     dat_status);
	}

      bail:
	if (NULL != data) {
		dapl_os_free(data, len + 1);
	}

	if (DAT_SUCCESS != dat_status) {
		if (NULL != provider) {
			(void)dapl_provider_list_remove(provider_info->ia_name);
		}

		if (NULL != hca_ptr) {
			dapl_hca_free(hca_ptr);
		}
	}
}

/*
 *
 * This function is called by the registry to de-initialize a provider
 *
 */
void DAT_API
DAT_PROVIDER_FINI_FUNC_NAME(IN const DAT_PROVIDER_INFO * provider_info)
{
	DAT_PROVIDER *provider;
	DAT_RETURN dat_status;

	dat_status =
	    dapl_provider_list_search(provider_info->ia_name, &provider);
	if (DAT_SUCCESS != dat_status) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "dat_registry_add_provider failed: %x\n",
			     dat_status);
		return;
	}

	dat_status = dat_registry_remove_provider(provider, provider_info);
	if (DAT_SUCCESS != dat_status) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "dat_registry_add_provider failed: %x\n",
			     dat_status);
	}

	/*
	 * free HCA memory
	 */
	dapl_hca_free(provider->extension);

	(void)dapl_provider_list_remove(provider_info->ia_name);

#if defined(_WIN32) || defined(_WIN64)
	/* cleanup DAPL library - relocated here from OSD DLL context as the IBAL
	 * calls hung in the DLL context?
	 */
	dapl_fini();
#endif
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
