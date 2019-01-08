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
 * MODULE: dapl_ep_dup_connect.c 
 *
 * PURPOSE: Endpoint management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 5
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_ep_util.h"
#include "dapl_adapter_util.h"
#include "dapl_timer_util.h"

/*
 * dapl_ep_dup_connect
 *
 * DAPL Requirements Version xxx, 6.5.8
 *
 * Requst that a connection be established between the local Endpoint
 * and a remote Endpoint. The remote Endpoint is identified by the
 * dup_ep.
 *
 * Input:
 *	ep_handle
 *	ep_dup_handle
 *	conn_qual
 *	timeout
 *	private_data_size
 *	private_data
 *	qos
 *
 * Output:
 *	none
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INVALID_PARAMETER
 *	DAT_INVALID_STATE
 *	DAT_MODEL_NOT_SUPPORTED
 */
DAT_RETURN DAT_API
dapl_ep_dup_connect(IN DAT_EP_HANDLE ep_handle,
		    IN DAT_EP_HANDLE ep_dup_handle,
		    IN DAT_TIMEOUT timeout,
		    IN DAT_COUNT private_data_size,
		    IN const DAT_PVOID private_data, IN DAT_QOS qos)
{
	DAPL_EP *ep_dup_ptr;
	DAT_RETURN dat_status;
	DAT_IA_ADDRESS_PTR remote_ia_address_ptr;
	DAT_CONN_QUAL remote_conn_qual;

	ep_dup_ptr = (DAPL_EP *) ep_dup_handle;

	/*
	 * Verify the dup handle, which must be connected. All other
	 * parameters will be verified by dapl_ep_connect
	 */
	if (DAPL_BAD_HANDLE(ep_dup_handle, DAPL_MAGIC_EP)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
		goto bail;
	}

	/* Can't do a connection in 0 time, reject outright */
	if (timeout == 0) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
		goto bail;
	}

	/* Check both the EP state and the QP state: if we don't have a QP
	 *  there is a problem.  Do this under a lock and pull out
	 * the connection parameters for atomicity.
	 */
	dapl_os_lock(&ep_dup_ptr->header.lock);
	if (ep_dup_ptr->param.ep_state != DAT_EP_STATE_CONNECTED) {
		dapl_os_unlock(&ep_dup_ptr->header.lock);
		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE,
			      dapls_ep_state_subtype(ep_dup_ptr));
		goto bail;
	}
	remote_ia_address_ptr = ep_dup_ptr->param.remote_ia_address_ptr;
	remote_conn_qual = ep_dup_ptr->param.remote_port_qual;
	dapl_os_unlock(&ep_dup_ptr->header.lock);

	dat_status = dapl_ep_connect(ep_handle,
				     remote_ia_address_ptr,
				     remote_conn_qual,
				     timeout,
				     private_data_size,
				     private_data,
				     qos, DAT_CONNECT_DEFAULT_FLAG);
      bail:
	return dat_status;
}
