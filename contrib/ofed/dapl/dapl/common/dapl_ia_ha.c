/*
 * Copyright (c) 2007 Intel Corporation.  All rights reserved.
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
 * MODULE: dapl_ia_ha.c
 *
 * PURPOSE: Interface Adapter High Availability - optional feature
 * Description: Described in DAPL 2.0 API, Chapter 5, section 9
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_provider.h"
#include "dapl_evd_util.h"
#include "dapl_hca_util.h"
#include "dapl_ia_util.h"
#include "dapl_adapter_util.h"

/*
 * dapl_ia_ha
 *
 * DAPL Requirements Version xxx, 5.9
 *
 * Queries for provider HA support
 *
 * Input:
 *	ia_handle
 *	provider name
 *
 * Output:
 *	answer
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_MODEL_NOT_SUPPORTED
 */

DAT_RETURN DAT_API dapl_ia_ha(IN DAT_IA_HANDLE ia_handle,	/* ia_handle */
			      IN const DAT_NAME_PTR provider,	/* provider  */
			      OUT DAT_BOOLEAN * answer)
{				/* answer    */
	return DAT_MODEL_NOT_SUPPORTED;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
