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
 * MODULE: dapl_ia_open.c
 *
 * PURPOSE: Interface Adapter management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.1 API, Chapter 6, section 2
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
 * LOCAL PROTOTYPES
 */
#ifdef IBHOSTS_NAMING
void dapli_assign_hca_ip_address(DAPL_HCA * hca_ptr, char *device_name);
#endif				/* IBHOSTS_NAMING */

void dapli_hca_cleanup(DAPL_HCA * hca_ptr, DAT_BOOLEAN dec_ref);

/*
 * dapl_ia_open
 *
 * DAPL Requirements Version xxx, 6.2.1.1
 *
 * Open a provider and return a handle. The handle enables the user
 * to invoke operations on this provider.
 *
 * The dat_ia_open  call is actually part of the DAT registration module.
 * That function maps the DAT_NAME parameter of dat_ia_open to a DAT_PROVIDER,
 * and calls this function.
 *
 * Input:
 *	provider
 *	async_evd_qlen
 *	async_evd_handle_ptr
 *
 * Output:
 *	async_evd_handle
 *	ia_handle
 *
 * Return Values:
 * 	DAT_SUCCESS
 * 	DAT_INSUFFICIENT_RESOURCES
 * 	DAT_INVALID_PARAMETER
 * 	DAT_INVALID_HANDLE
 * 	DAT_PROVIDER_NOT_FOUND	(returned by dat registry if necessary)
 */
DAT_RETURN DAT_API
dapl_ia_open(IN const DAT_NAME_PTR name,
	     IN DAT_COUNT async_evd_qlen,
	     INOUT DAT_EVD_HANDLE * async_evd_handle_ptr,
	     OUT DAT_IA_HANDLE * ia_handle_ptr)
{
	DAT_RETURN dat_status;
	DAT_PROVIDER *provider;
	DAPL_HCA *hca_ptr;
	DAPL_IA *ia_ptr;
	DAPL_EVD *evd_ptr;

	dat_status = DAT_SUCCESS;
	hca_ptr = NULL;
	ia_ptr = NULL;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_ia_open (%s, %d, %p, %p)\n",
		     name, async_evd_qlen, async_evd_handle_ptr, ia_handle_ptr);

	dat_status = dapl_provider_list_search(name, &provider);
	if (DAT_SUCCESS != dat_status) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG1);
		goto bail;
	}

	/* ia_handle_ptr and async_evd_handle_ptr cannot be NULL */
	if (ia_handle_ptr == NULL) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG4);
		goto bail;
	}
	if (async_evd_handle_ptr == NULL) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
		goto bail;
	}

	/* initialize the caller's OUT param */
	*ia_handle_ptr = DAT_HANDLE_NULL;

	/* get the hca_ptr */
	hca_ptr = (DAPL_HCA *) provider->extension;

	/*
	 * Open the HCA if it has not been done before.
	 */
	dapl_os_lock(&hca_ptr->lock);
	if (hca_ptr->ib_hca_handle == IB_INVALID_HANDLE) {
		/* register with the HW */
		dat_status = dapls_ib_open_hca(hca_ptr->name, hca_ptr);

		if (dat_status != DAT_SUCCESS) {
			dapl_dbg_log(DAPL_DBG_TYPE_ERR,
				     "dapls_ib_open_hca failed %x\n",
				     dat_status);
			dapl_os_unlock(&hca_ptr->lock);
			goto bail;
		}

		/*
		 * Obtain the IP address associated with this name and HCA.
		 */
#ifdef IBHOSTS_NAMING
		dapli_assign_hca_ip_address(hca_ptr, name);
#endif				/* IBHOSTS_NAMING */

		/*
		 * Obtain IA attributes from the HCA to limit certain
		 * operations.
		 * If using DAPL_ATS naming, ib_query_hca will also set the ip
		 * address.
		 */
		dat_status = dapls_ib_query_hca(hca_ptr,
						&hca_ptr->ia_attr,
						NULL, &hca_ptr->hca_address);
		if (dat_status != DAT_SUCCESS) {
			dapli_hca_cleanup(hca_ptr, DAT_FALSE);
			dapl_os_unlock(&hca_ptr->lock);
			goto bail;
		}
	}

	/* Take a reference on the hca_handle */
	dapl_os_atomic_inc(&hca_ptr->handle_ref_count);
	dapl_os_unlock(&hca_ptr->lock);

	/* Allocate and initialize ia structure */
	ia_ptr = dapl_ia_alloc(provider, hca_ptr);
	if (!ia_ptr) {
		dapl_os_lock(&hca_ptr->lock);
		dapli_hca_cleanup(hca_ptr, DAT_TRUE);
		dapl_os_unlock(&hca_ptr->lock);
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	/* we need an async EVD for this IA
	 * use the one passed in (if non-NULL) or create one
	 */

	evd_ptr = (DAPL_EVD *) * async_evd_handle_ptr;
	if (evd_ptr) {
		if (DAPL_BAD_HANDLE(evd_ptr, DAPL_MAGIC_EVD) ||
		    !(evd_ptr->evd_flags & DAT_EVD_ASYNC_FLAG)) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_HANDLE,
				      DAT_INVALID_HANDLE_EVD_ASYNC);
			goto bail;
		}

		/* InfiniBand allows only 1 asychronous event handler per HCA */
		/* (see InfiniBand Spec, release 1.1, vol I, section 11.5.2,  */
		/*  page 559).                                                */
		/*                                                            */
		/* We only need to make sure that this EVD's CQ belongs to    */
		/* the same HCA as is being opened.                           */

		if (evd_ptr->header.owner_ia->hca_ptr->ib_hca_handle !=
		    hca_ptr->ib_hca_handle) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_HANDLE,
				      DAT_INVALID_HANDLE_EVD_ASYNC);
			goto bail;
		}

		ia_ptr->cleanup_async_error_evd = DAT_FALSE;
		ia_ptr->async_error_evd = evd_ptr;
	} else {
		/* Verify we have >0 length, and let the provider check the size */
		if (async_evd_qlen <= 0) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG2);
			goto bail;
		}
		dat_status = dapls_evd_internal_create(ia_ptr, NULL,	/* CNO ptr */
						       async_evd_qlen,
						       DAT_EVD_ASYNC_FLAG,
						       &evd_ptr);
		if (dat_status != DAT_SUCCESS) {
			goto bail;
		}

		dapl_os_atomic_inc(&evd_ptr->evd_ref_count);

		dapl_os_lock(&hca_ptr->lock);
		if (hca_ptr->async_evd != (DAPL_EVD *) 0) {
#if 0
			/*
			 * The async EVD for this HCA has already been assigned.
			 * It's an error to try and assign another one.
			 *
			 * However, we need to somehow allow multiple IAs
			 * off of the same HCA.  The right way to do this
			 * is by dispatching events off the HCA to the appropriate
			 * IA, but we aren't there yet.  So for now we create
			 * the EVD but don't connect it to anything.
			 */
			dapl_os_atomic_dec(&evd_ptr->evd_ref_count);
			dapl_evd_free(evd_ptr);
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG4);
			goto bail;
#endif
			dapl_os_unlock(&hca_ptr->lock);
		} else {
			hca_ptr->async_evd = evd_ptr;
			dapl_os_unlock(&hca_ptr->lock);

			/* Register the handlers associated with the async EVD.  */
			dat_status = dapls_ia_setup_callbacks(ia_ptr, evd_ptr);
			if (dat_status != DAT_SUCCESS) {
				/* Assign the EVD so it gets cleaned up */
				ia_ptr->cleanup_async_error_evd = DAT_TRUE;
				ia_ptr->async_error_evd = evd_ptr;
				goto bail;
			}
		}

		ia_ptr->cleanup_async_error_evd = DAT_TRUE;
		ia_ptr->async_error_evd = evd_ptr;
	}

	dat_status = DAT_SUCCESS;
	*ia_handle_ptr = ia_ptr;
	*async_evd_handle_ptr = evd_ptr;

      bail:
	if (dat_status != DAT_SUCCESS) {
		if (ia_ptr) {
			/* This will release the async EVD if needed.  */
			dapl_ia_close(ia_ptr, DAT_CLOSE_ABRUPT_FLAG);
		}
	}

	dapl_dbg_log(DAPL_DBG_TYPE_RTN,
		     "dapl_ia_open () returns 0x%x\n", dat_status);

	return dat_status;
}

/*
 * dapli_hca_cleanup
 *
 * Clean up partially allocated HCA stuff. Strictly to make cleanup
 * simple.
 */
void dapli_hca_cleanup(DAPL_HCA * hca_ptr, DAT_BOOLEAN dec_ref)
{
	dapls_ib_close_hca(hca_ptr);
	hca_ptr->ib_hca_handle = IB_INVALID_HANDLE;
	if (dec_ref == DAT_TRUE) {
		dapl_os_atomic_dec(&hca_ptr->handle_ref_count);
	}
}

#ifdef IBHOSTS_NAMING

char *dapli_get_adapter_num(char *device_name);

void dapli_setup_dummy_addr(IN DAPL_HCA * hca_ptr, IN char *hca_name);
/*
 * dapli_assign_hca_ip_address
 *
 * Obtain the IP address of the passed in name, which represents a
 * port on the hca. There are three methods here to obtain the
 * appropriate IP address, each with their own shortcoming:
 * 1) IPOIB_NAMING. Requires the implementation of the IPoIB
 *    interface defined in include/dapl/ipoib_names.h. This is
 *    not the recommended interface as IPoIB is limited at
 *    the point we need to obtain an IP address on the
 *    passive side of a connection. The code supporting this
 *    implementation has been removed.
 *
 * 2) IBHOSTS. An entry exists in DNS and in the /etc/dapl/ibhosts
 *    file. The immediate drawback here is that we must dictate
 *    how to name the interface, which is a stated DAPL non-goal.
 *    In the broader perspective, this method requires us to xmit
 *    the IP address in the private data of a connection, which has
 *    other fun problems. This is the default method and is known to
 *    work, but it has problems.
 *
 * 3) Obtain the IP address from the driver, which has registered
 *    the address with the SA for retrieval.
 *
 *
 * Input:
 *	hca_ptr			Pointer to HCA structure
 *	device_name		Name of device as reported by the provider
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	char * to string number
 */
void dapli_assign_hca_ip_address(DAPL_HCA * hca_ptr, char *device_name)
{
	char *adapter_num;
#define NAMELEN	128
	struct addrinfo *addr;
	char hostname[NAMELEN];
	char *str;
	int rc;

	/*
	 * Obtain the IP address of the adapter. This is a simple
	 * scheme that creates a name that must appear available to
	 * DNS, e.g. it must be in the local site DNS or in the local
	 * /etc/hosts file, etc.
	 *
	 *  <hostname>-ib<index>
	 *
	 * This scheme obviously doesn't work with adapters from
	 * multiple vendors, but will suffice in common installations.
	 */

	rc = gethostname(hostname, NAMELEN);

	/* guarantee NUL termination if hostname gets truncated */
	hostname[NAMELEN - 1] = '\0';

	/*
	 * Strip off domain info if it exists (e.g. mynode.mydomain.com)
	 */
	for (str = hostname; *str && *str != '.';) {
		str++;
	}
	if (*str == '.') {
		*str = '\0';
	}
	strcat(hostname, "-ib");
	adapter_num = dapli_get_adapter_num(device_name);
	strcat(hostname, adapter_num);

	rc = dapls_osd_getaddrinfo(hostname, &addr);

	if (rc != 0) {
		/* Not registered in DNS, provide a dummy value */
		dapli_setup_dummy_addr(hca_ptr, hostname);
	} else {
		hca_ptr->hca_address = *((DAT_SOCK_ADDR6 *) addr->ai_addr);
		dapls_osd_freeaddrinfo(addr);
	}
}

/*
 * dapli_stup_dummy_addr
 *
 * Set up a dummy local address for the HCA. Things are not going
 * to work too well if this happens.
 * We call this routine if:
 *  - remote host adapter name is not in DNS
 *  - IPoIB implementation is not correctly set up
 *  - Similar nonsense.
 *
 * Input:
 *      hca_ptr
 *	rhost_name		Name of remote adapter
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	none
 */
void dapli_setup_dummy_addr(IN DAPL_HCA * hca_ptr, IN char *rhost_name)
{
	struct sockaddr_in *si;

	/* Not registered in DNS, provide a dummy value */
	dapl_dbg_log(DAPL_DBG_TYPE_WARN,
		     "WARNING: <%s> not registered in DNS, using dummy IP value\n",
		     rhost_name);
	si = (struct sockaddr_in *)&hca_ptr->hca_address;
	hca_ptr->hca_address.sin6_family = AF_INET;
	si->sin_addr.s_addr = 0x01020304;
}

/*
 * dapls_get_adapter_num
 *
 * Given a device name, return a string of the device number
 *
 * Input:
 *	device_name		Name of device as reported by the provider
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	char * to string number
 */
char *dapli_get_adapter_num(char *device_name)
{
	static char *zero = "0";
	char *p;

	/*
	 * Optimisticaly simple algorithm: the device number appears at
	 * the end of the device name string. Device that do not end
	 * in a number are by default "0".
	 */

	for (p = device_name; *p; p++) {
		if (isdigit(*p)) {
			return p;
		}
	}

	return zero;
}
#endif				/* IBHOSTS_NAMING */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
