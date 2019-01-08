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
 * MODULE: dat_init.c
 *
 * PURPOSE: DAT registry implementation for uDAPL
 * Description: init and fini functions for DAT module.
 *
 * $Id: dat_init.c,v 1.18 2005/03/24 05:58:27 jlentini Exp $
 **********************************************************************/

#include <dat2/dat_platform_specific.h>
#include "dat_init.h"
#include "dat_dr.h"
#include "dat_osd.h"

#ifndef DAT_NO_STATIC_REGISTRY
#include "dat_sr.h"
#endif

/*********************************************************************
 *                                                                   *
 * Global Variables                                                  *
 *                                                                   *
 *********************************************************************/

/*
 * Ideally, the following two rules could be enforced:
 *
 * - The DAT Registry's initialization function is executed before that
 *   of any DAT Providers and hence all calls into the registry occur
 *   after the registry module is initialized.
 *
 * - The DAT Registry's deinitialization function is executed after that
 *   of any DAT Providers and hence all calls into the registry occur
 *   before the registry module is deinitialized.
 *
 * However, on many platforms few guarantees are provided regarding the
 * order in which module initialization and deinitialization functions
 * are invoked.
 *
 * To understand why these rules are difficult to enforce using only
 * features common to all platforms, consider the Linux platform. The order
 * in which Linux shared libraries are loaded into a process's address space
 * is undefined. When a DAT consumer explicitly links to DAT provider
 * libraries, the order in which library initialization and deinitialization
 * functions are invoked becomes important. In this scenario, a DAPL provider
 * may call dat_registry_add_provider() before the registry has been 
 * initialized.
 *
 * We assume that modules are loaded with a single thread. Given
 * this assumption, we can use a simple state variable to determine
 * the state of the DAT registry.
 */

static DAT_MODULE_STATE g_module_state = DAT_MODULE_STATE_UNINITIALIZED;

//***********************************************************************
// Function: dat_module_get_state
//***********************************************************************

DAT_MODULE_STATE dat_module_get_state(void)
{
	return g_module_state;
}

//***********************************************************************
// Function: dat_init
//***********************************************************************

void dat_init(void)
{
	if (DAT_MODULE_STATE_UNINITIALIZED == g_module_state) {
		/*
		 * update the module state flag immediately in case there
		 * is a recursive call to dat_init().
		 */
		g_module_state = DAT_MODULE_STATE_INITIALIZING;

		dat_os_dbg_init();

		dats_handle_vector_init();

		dat_os_dbg_print(DAT_OS_DBG_TYPE_GENERIC,
				 "DAT Registry: Started (dat_init)\n");

#ifndef DAT_NO_STATIC_REGISTRY
		dat_sr_init();
#endif
		dat_dr_init();

		g_module_state = DAT_MODULE_STATE_INITIALIZED;
	}
}

//***********************************************************************
// Function: dat_fini
//***********************************************************************

void dat_fini(void)
{
	if (DAT_MODULE_STATE_INITIALIZED == g_module_state) {
		g_module_state = DAT_MODULE_STATE_DEINITIALIZING;

		dat_dr_fini();
#ifndef DAT_NO_STATIC_REGISTRY
		dat_sr_fini();
#endif

		dat_os_dbg_print(DAT_OS_DBG_TYPE_GENERIC,
				 "DAT Registry: Stopped (dat_fini)\n");

		g_module_state = DAT_MODULE_STATE_DEINITIALIZED;
	}
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
