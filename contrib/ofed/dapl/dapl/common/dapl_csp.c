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
 * MODULE: dapl_csp.c
 *
 * PURPOSE: Connection management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 2.0 API, Chapter 6, section 4
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_sp_util.h"
#include "dapl_ia_util.h"
#include "dapl_adapter_util.h"

/*
 * dapl_psp_create, dapl_csp_query, dapl_csp_free
 *
 * uDAPL: User Direct Access Program Library Version 2.0, 6.4.4.2
 *
 * The Common Service Point is transport-independent analog of the Public
 * Service Point. It allows the Consumer to listen on socket-equivalent for
 * requests for connections arriving on a specified IP port instead of
 * transport-dependent Connection Qualifier. An IA Address follows the
 * platform conventions and provides among others the IP port to listen on.
 * An IP port of the Common Service Point advertisement is supported by
 * existing Ethernet infrastructure or DAT Name Service.
 *
 * Input:
 * 	ia_handle
 * 	comm_id
 * 	address
 * 	evd_handle
 *      csp_handle
 *
 * Output:
 * 	csp_handle
 *
 * Returns:
 * 	DAT_SUCCESS
 * 	DAT_INSUFFICIENT_RESOURCES
 * 	DAT_INVALID_PARAMETER
 * 	DAT_CONN_QUAL_IN_USE
 * 	DAT_MODEL_NOT_SUPPORTED
 */
DAT_RETURN DAT_API dapl_csp_create(IN DAT_IA_HANDLE ia_handle,	/* ia_handle      */
				   IN DAT_COMM * comm,	/* communicator   */
				   IN DAT_IA_ADDRESS_PTR addr,	/* address        */
				   IN DAT_EVD_HANDLE evd_handle,	/* evd_handle     */
				   OUT DAT_CSP_HANDLE * csp_handle)
{				/* csp_handle     */
	return DAT_MODEL_NOT_SUPPORTED;
}

DAT_RETURN DAT_API dapl_csp_query(IN DAT_CSP_HANDLE csp_handle,	/* csp_handle     */
				  IN DAT_CSP_PARAM_MASK param_mask,	/* csp_param_mask */
				  OUT DAT_CSP_PARAM * param)
{				/* csp_param      */
	return DAT_MODEL_NOT_SUPPORTED;
}

DAT_RETURN DAT_API dapl_csp_free(IN DAT_CSP_HANDLE csp_handle)
{				/* csp_handle     */
	return DAT_MODEL_NOT_SUPPORTED;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  c-brace-offset: -4
 *  tab-width: 8
 * End:
 */
