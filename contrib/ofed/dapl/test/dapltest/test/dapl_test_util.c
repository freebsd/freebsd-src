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

#include "dapl_proto.h"

/* -----------------------------------------------------------
 * Gather info about default attributes
 */
DAT_BOOLEAN
DT_query(Per_Test_Data_t * pt_ptr,
	 DAT_IA_HANDLE ia_handle, DAT_EP_HANDLE ep_handle)
{
	char *module = "DT_query";
	DAT_EVD_HANDLE async_evd_hdl;	/* not used */
	DAT_EP_PARAM ep_params;
	DAT_RETURN ret;
	DT_Tdep_Print_Head *phead;

	phead = pt_ptr->Params.phead;

	/* Query the IA */
	ret = dat_ia_query(ia_handle,
			   &async_evd_hdl,
			   DAT_IA_ALL,
			   &pt_ptr->ia_attr,
			   DAT_PROVIDER_FIELD_ALL, &pt_ptr->provider_attr);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead, "%s: dat_ia_query error: %s\n",
				  module, DT_RetToString(ret));
		return (false);
	}

	/* Query the EP */
	ret = dat_ep_query(ep_handle, DAT_EP_FIELD_ALL, &ep_params);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead, "%s: dat_ep_query error: %s\n",
				  module, DT_RetToString(ret));
		return (false);
	}
	pt_ptr->ep_attr = ep_params.ep_attr;

	/*
	 * If debugging, print out some interesting attributes
	 */
	if (DT_dapltest_debug) {
		DAT_SOCK_ADDR6 *ip6_addr;
		struct sockaddr_in *ip_addr;

		DT_Tdep_PT_Printf(phead,
				  "*****  DAPL  Characteristics  *****\n");
		DT_Tdep_PT_Printf(phead,
				  "Provider: %s  Version %d.%d  DAPL %d.%d\n",
				  pt_ptr->provider_attr.provider_name,
				  pt_ptr->provider_attr.provider_version_major,
				  pt_ptr->provider_attr.provider_version_minor,
				  pt_ptr->provider_attr.dapl_version_major,
				  pt_ptr->provider_attr.dapl_version_minor);
		DT_Tdep_PT_Printf(phead, "Adapter: %s by %s Version %d.%d\n",
				  pt_ptr->ia_attr.adapter_name,
				  pt_ptr->ia_attr.vendor_name,
				  pt_ptr->ia_attr.hardware_version_major,
				  pt_ptr->ia_attr.hardware_version_minor);
		DT_Tdep_PT_Printf(phead, "Supporting:\n");
		DT_Tdep_PT_Printf(phead,
				  "\t%d EPs with %d DTOs and %d RDMA/RDs each\n",
				  pt_ptr->ia_attr.max_eps,
				  pt_ptr->ia_attr.max_dto_per_ep,
				  pt_ptr->ia_attr.max_rdma_read_per_ep);
		DT_Tdep_PT_Printf(phead,
				  "\t%d EVDs of up to %d entries "
				  " (default S/R size is %d/%d)\n",
				  pt_ptr->ia_attr.max_evds,
				  pt_ptr->ia_attr.max_evd_qlen,
				  pt_ptr->ep_attr.max_request_dtos,
				  pt_ptr->ep_attr.max_recv_dtos);
		DT_Tdep_PT_Printf(phead, "\tIOVs of up to %d elements\n",
				  pt_ptr->ia_attr.max_iov_segments_per_dto);
		DT_Tdep_PT_Printf(phead,
				  "\t%d LMRs (and %d RMRs) of up to 0x" F64x
				  " bytes\n", pt_ptr->ia_attr.max_lmrs,
				  pt_ptr->ia_attr.max_rmrs,
				  pt_ptr->ia_attr.max_lmr_block_size);
		DT_Tdep_PT_Printf(phead,
				  "\tMaximum MTU 0x" F64x " bytes, RDMA 0x" F64x
				  " bytes\n", pt_ptr->ia_attr.max_mtu_size,
				  pt_ptr->ia_attr.max_rdma_size);
		DT_Tdep_PT_Printf(phead,
				  "\tMaximum Private data size %d bytes\n",
				  pt_ptr->provider_attr.max_private_data_size);

		ip6_addr = (DAT_SOCK_ADDR6 *) pt_ptr->ia_attr.ia_address_ptr;
		if (ip6_addr->sin6_family == AF_INET6) {
			DT_Tdep_PT_Printf(phead,
					  "\tLocal IP address  %x:%x:%x:%x:%x:%x:%x:%x:\n",
					  ip6_addr->sin6_addr.s6_addr[0],
					  ip6_addr->sin6_addr.s6_addr[1],
					  ip6_addr->sin6_addr.s6_addr[2],
					  ip6_addr->sin6_addr.s6_addr[3],
					  ip6_addr->sin6_addr.s6_addr[4],
					  ip6_addr->sin6_addr.s6_addr[5],
					  ip6_addr->sin6_addr.s6_addr[6],
					  ip6_addr->sin6_addr.s6_addr[7]);
			DT_Tdep_PT_Printf(phead, "%x:%x:%x:%x:%x:%x:%x:%x\n",
					  ip6_addr->sin6_addr.s6_addr[8],
					  ip6_addr->sin6_addr.s6_addr[9],
					  ip6_addr->sin6_addr.s6_addr[10],
					  ip6_addr->sin6_addr.s6_addr[11],
					  ip6_addr->sin6_addr.s6_addr[12],
					  ip6_addr->sin6_addr.s6_addr[13],
					  ip6_addr->sin6_addr.s6_addr[14],
					  ip6_addr->sin6_addr.s6_addr[15]);
		} else if (ip6_addr->sin6_family == AF_INET)
		{
			ip_addr =
			    (struct sockaddr_in *)pt_ptr->ia_attr.
			    ia_address_ptr;

			DT_Tdep_PT_Printf(phead, "\tLocal IP address %s\n",
					  inet_ntoa(ip_addr->sin_addr));
		}

		DT_Tdep_PT_Printf(phead,
				  "***** ***** ***** ***** ***** *****\n");
	}

	return (true);
}

/* -----------------------------------------------------------
 * Post a recv buffer
 */
DAT_BOOLEAN
DT_post_recv_buffer(DT_Tdep_Print_Head * phead,
		    DAT_EP_HANDLE ep_handle, Bpool * bp, int index, int size)
{
	unsigned char *buff = DT_Bpool_GetBuffer(bp, index);
	DAT_LMR_TRIPLET *iov = DT_Bpool_GetIOV(bp, index);
	DAT_LMR_CONTEXT lmr_c = DT_Bpool_GetLMR(bp, index);
	DAT_DTO_COOKIE cookie;
	DAT_RETURN ret;

	/*
	 * Prep the inputs
	 */
	iov->virtual_address = (DAT_VADDR) (uintptr_t) buff;
	iov->segment_length = size;
	iov->lmr_context = lmr_c;
	cookie.as_64 = (DAT_UINT64) 0UL;
	cookie.as_ptr = (DAT_PVOID) buff;

	DT_Tdep_PT_Debug(3,
			 (phead, "Post-Recv #%d [%p, %x]\n", index, buff,
			  size));

	/* Post the recv buffer */
	ret = dat_ep_post_recv(ep_handle,
			       1, iov, cookie, DAT_COMPLETION_DEFAULT_FLAG);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "Test Error: dat_ep_post_recv failed: %s\n",
				  DT_RetToString(ret));
		DT_Test_Error();
		return false;
	}
	return true;
}

/* -----------------------------------------------------------
 * Post a send buffer
 */
DAT_BOOLEAN
DT_post_send_buffer(DT_Tdep_Print_Head * phead,
		    DAT_EP_HANDLE ep_handle, Bpool * bp, int index, int size)
{
	unsigned char *buff = DT_Bpool_GetBuffer(bp, index);
	DAT_LMR_TRIPLET *iov = DT_Bpool_GetIOV(bp, index);
	DAT_LMR_CONTEXT lmr_c = DT_Bpool_GetLMR(bp, index);
	DAT_DTO_COOKIE cookie;
	DAT_RETURN ret;

	/*
	 * Prep the inputs
	 */
	iov->virtual_address = (DAT_VADDR) (uintptr_t) buff;
	iov->segment_length = size;
	iov->lmr_context = lmr_c;
	cookie.as_64 = (DAT_UINT64) 0UL;
	cookie.as_ptr = (DAT_PVOID) buff;

	DT_Tdep_PT_Debug(3,
			 (phead, "Post-Send #%d [%p, %x]\n", index, buff,
			  size));

	/* Post the recv buffer */
	ret = dat_ep_post_send(ep_handle,
			       1, iov, cookie, DAT_COMPLETION_DEFAULT_FLAG);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "Test Error: dat_ep_post_send failed: %s\n",
				  DT_RetToString(ret));
		DT_Test_Error();
		return false;
	}
	return true;
}

/* -----------------------------------------------------------
 * Wait for a CR event, returning false on error.
 */
bool
DT_cr_event_wait(DT_Tdep_Print_Head * phead,
		 DAT_EVD_HANDLE evd_handle,
		 DAT_CR_ARRIVAL_EVENT_DATA * cr_stat_p)
{
	int err_cnt;

	err_cnt = 0;

	for (;;) {
		DAT_RETURN ret;
		DAT_EVENT event;

		ret =
		    DT_Tdep_evd_wait(evd_handle, DAT_TIMEOUT_INFINITE, &event);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: dapl_event_wait (CR) failed: %s\n",
					  DT_RetToString(ret));
			DT_Test_Error();
			/*
			 * If we get an error due to the client breaking the
			 * connection early or some transients, just ignore it
			 * and keep going. If we get a bunch of errors, bail
			 * out.
			 */
			/*      if ( err_cnt++ < 10 ) */
			/*      { */
			/*              continue; */
			/*      } */

			break;
		}

		if (event.event_number == DAT_CONNECTION_REQUEST_EVENT) {
			/*
			 * Pass back what we know, if requested.
			 */
			if (cr_stat_p) {
				*cr_stat_p =
				    event.event_data.cr_arrival_event_data;
			}
			return (true);
		}

		DT_Tdep_PT_Printf(phead,
				  "Warning: cr_event_wait swallowing %s event\n",
				  DT_EventToSTr(event.event_number));
	}

	return (false);
}

/* -----------------------------------------------------------
 * Wait for a connection event, returning false on error.
 */
bool
DT_conn_event_wait(DT_Tdep_Print_Head * phead,
		   DAT_EP_HANDLE ep_handle,
		   DAT_EVD_HANDLE evd_handle, DAT_EVENT_NUMBER * event_number)
{
	for (;;) {
		DAT_RETURN ret;
		DAT_EVENT event;

		ret =
		    DT_Tdep_evd_wait(evd_handle, DAT_TIMEOUT_INFINITE, &event);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: dapl_event_wait (CONN) failed: %s\n",
					  DT_RetToString(ret));
			DT_Test_Error();
			break;
		}
		*event_number = event.event_number;
		if (event.event_number == DAT_CONNECTION_EVENT_PEER_REJECTED
		    || event.event_number ==
		    DAT_CONNECTION_EVENT_NON_PEER_REJECTED
		    || event.event_number ==
		    DAT_CONNECTION_EVENT_ACCEPT_COMPLETION_ERROR
		    || event.event_number == DAT_CONNECTION_EVENT_DISCONNECTED
		    || event.event_number == DAT_CONNECTION_EVENT_BROKEN
		    || event.event_number == DAT_CONNECTION_EVENT_UNREACHABLE
		    || event.event_number == DAT_CONNECTION_EVENT_TIMED_OUT) {
			DT_Tdep_PT_Printf(phead,
					  "Warning: conn_event_wait %s\n",
					  DT_EventToSTr(event.event_number));
			break;
		}
		if (event.event_number == DAT_CONNECTION_EVENT_ESTABLISHED) {
			/*
			 * Could return DAT_CONNECTION_EVENT_DATA and verify:
			 *      event.event_data.connect_event_data.ep_handle
			 *      event.event_data.connect_event_data.private_data_size
			 *      event.event_data.connect_event_data.private_data
			 */
			return (true);
		}

		DT_Tdep_PT_Printf(phead,
				  "Warning: conn_event_wait swallowing %s event\n",
				  DT_EventToSTr(event.event_number));
	}

	return (false);
}

/* -----------------------------------------------------------
 * Wait for a disconnection event, returning false on error.
 */
bool
DT_disco_event_wait(DT_Tdep_Print_Head * phead,
		    DAT_EVD_HANDLE evd_handle, DAT_EP_HANDLE * ep_handle)
{
	for (;;) {
		DAT_RETURN ret;
		DAT_EVENT event;

		ret =
		    DT_Tdep_evd_wait(evd_handle, DAT_TIMEOUT_INFINITE, &event);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: dapl_event_wait (DISCONN) failed: %s\n",
					  DT_RetToString(ret));
			DT_Test_Error();
			break;
		}
		if (event.event_number == DAT_CONNECTION_EVENT_PEER_REJECTED
		    || event.event_number ==
		    DAT_CONNECTION_EVENT_NON_PEER_REJECTED
		    || event.event_number ==
		    DAT_CONNECTION_EVENT_ACCEPT_COMPLETION_ERROR
		    || event.event_number == DAT_CONNECTION_EVENT_BROKEN
		    || event.event_number == DAT_CONNECTION_EVENT_UNREACHABLE
		    || event.event_number == DAT_CONNECTION_EVENT_TIMED_OUT) {
			DT_Tdep_PT_Printf(phead,
					  "Warning: disconn_event_wait %s\n",
					  DT_EventToSTr(event.event_number));
			break;
		}

		if (event.event_number == DAT_CONNECTION_EVENT_DISCONNECTED) {
			if (ep_handle != NULL) {
				*ep_handle =
				    event.event_data.connect_event_data.
				    ep_handle;
			}
			return (true);
		}

		DT_Tdep_PT_Printf(phead,
				  "Warning: conn_event_wait swallowing %s event\n",
				  DT_EventToSTr(event.event_number));
	}

	return (false);
}

/* -----------------------------------------------------------
 * Reap a DTO event using a wait or polling, returning false on error.
 */
bool
DT_dto_event_reap(DT_Tdep_Print_Head * phead,
		  DAT_EVD_HANDLE evd_handle,
		  bool poll, DAT_DTO_COMPLETION_EVENT_DATA * dto_statusp)
{
	if (poll) {
		return DT_dto_event_poll(phead, evd_handle, dto_statusp);
	} else {
		return DT_dto_event_wait(phead, evd_handle, dto_statusp);
	}
}

/* -----------------------------------------------------------
 * Poll for a DTO event, returning false on error.
 */
bool
DT_dto_event_poll(DT_Tdep_Print_Head * phead,
		  DAT_EVD_HANDLE evd_handle,
		  DAT_DTO_COMPLETION_EVENT_DATA * dto_statusp)
{
	for (;;DT_Mdep_yield()) {
		DAT_RETURN ret;
		DAT_EVENT event;

		ret = DT_Tdep_evd_dequeue(evd_handle, &event);

		if (DAT_GET_TYPE(ret) == DAT_QUEUE_EMPTY) {
			continue;
		}

		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: dapl_event_wait (DTO) failed: %s\n",
					  DT_RetToString(ret));
			DT_Test_Error();
			break;
		}

		if (event.event_number == DAT_DTO_COMPLETION_EVENT) {
			/*
			 * Pass back all the useful bits if requested:
			 *      ep_handle,  user_cookie.as_ptr
			 *      status,     transfered_length
			 */
			if (dto_statusp) {
				*dto_statusp =
				    event.event_data.dto_completion_event_data;
			}

			return (true);
		}

		DT_Tdep_PT_Printf(phead,
				  "Warning: dto_event_poll swallowing %s event\n",
				  DT_EventToSTr(event.event_number));
	}

	return (false);
}

/* -----------------------------------------------------------
 * Wait for a DTO event, returning false on error.
 */
bool
DT_dto_event_wait(DT_Tdep_Print_Head * phead,
		  DAT_EVD_HANDLE evd_handle,
		  DAT_DTO_COMPLETION_EVENT_DATA * dto_statusp)
{
	for (;;) {
		DAT_RETURN ret;
		DAT_EVENT event;

		ret =
		    DT_Tdep_evd_wait(evd_handle, DAT_TIMEOUT_INFINITE, &event);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: dapl_event_wait (DTO) failed: %s\n",
					  DT_RetToString(ret));
			DT_Test_Error();
			break;
		}

		if (event.event_number == DAT_DTO_COMPLETION_EVENT) {
			/*
			 * Pass back all the useful bits if requested:
			 *      ep_handle,  user_cookie.as_ptr
			 *      status,     transfered_length
			 */
			if (dto_statusp) {
				*dto_statusp =
				    event.event_data.dto_completion_event_data;
			}
			return (true);
		}

		DT_Tdep_PT_Printf(phead,
				  "Warning: dto_event_wait swallowing %s event\n",
				  DT_EventToSTr(event.event_number));
	}

	return (false);
}

/* -----------------------------------------------------------
 * Wait for a RMR event, returning false on error.
 */
bool
DT_rmr_event_wait(DT_Tdep_Print_Head * phead,
		  DAT_EVD_HANDLE evd_handle,
		  DAT_RMR_BIND_COMPLETION_EVENT_DATA * rmr_statusp)
{
	for (;;) {
		DAT_RETURN ret;
		DAT_EVENT event;

		ret =
		    DT_Tdep_evd_wait(evd_handle, DAT_TIMEOUT_INFINITE, &event);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: dapl_event_wait (RMR) failed: %s\n",
					  DT_RetToString(ret));
			DT_Test_Error();
			break;
		}

		if (event.event_number == DAT_RMR_BIND_COMPLETION_EVENT) {
			/*
			 * Pass back all the useful bits if requested:
			 *      rmr_handle,  user_cookie, status
			 */
			if (rmr_statusp) {
				*rmr_statusp =
				    event.event_data.rmr_completion_event_data;
			}
			return (true);
		}

		DT_Tdep_PT_Printf(phead,
				  "Warning: rmr_event_wait swallowing %s event\n",
				  DT_EventToSTr(event.event_number));
	}

	return (false);
}

/* -----------------------------------------------------------
 * Check a DTO and print some debug info if anything is amiss.
 */
bool
DT_dto_check(DT_Tdep_Print_Head * phead,
	     DAT_DTO_COMPLETION_EVENT_DATA * dto_p,
	     DAT_EP_HANDLE ep_expected,
	     DAT_COUNT len_expected,
	     DAT_DTO_COOKIE cookie_expected, char *message)
{
	if (((ep_expected != NULL) && (dto_p->ep_handle != ep_expected))
	    || dto_p->transfered_length != len_expected
	    || dto_p->user_cookie.as_64 != cookie_expected.as_64
	    || dto_p->status != DAT_DTO_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "Test Error: %s-reaping DTO problem, status = %s\n",
				  message,
				  (dto_p->status ==
				   DAT_DTO_SUCCESS ? "OK" : (dto_p->status ==
							     DAT_DTO_FAILURE ?
							     "FAILURE" :
							     "LengthError")));
		DT_Test_Error();
		if ((ep_expected != NULL) && (dto_p->ep_handle != ep_expected)) {
			DT_Tdep_PT_Printf(phead,
					  "\tEndPoint mismatch (got %p wanted %p)\n",
					  dto_p->ep_handle, ep_expected);
		}
		if (dto_p->transfered_length != len_expected) {
			DT_Tdep_PT_Printf(phead,
					  "\tLength mismatch (xfer 0x" F64x
					  " wanted 0x%x)\n",
					  dto_p->transfered_length,
					  len_expected);
		}
		if (dto_p->user_cookie.as_64 != cookie_expected.as_64) {
			DT_Tdep_PT_Printf(phead,
					  "\tCookie mismatch (got " F64x
					  " wanted " F64x ")\n",
					  dto_p->user_cookie.as_64,
					  cookie_expected.as_64);
		}
		return (false);
	}

	return (true);
}

/* -----------------------------------------------------------
 * Check an RMR Bind  and print some debug info if anything is amiss.
 */
bool
DT_rmr_check(DT_Tdep_Print_Head * phead,
	     DAT_RMR_BIND_COMPLETION_EVENT_DATA * rmr_p,
	     DAT_RMR_HANDLE rmr_expected,
	     DAT_PVOID cookie_expected, char *message)
{
	if (rmr_p->rmr_handle != rmr_expected
	    || rmr_p->user_cookie.as_ptr != cookie_expected
	    || rmr_p->status != DAT_RMR_BIND_SUCCESS) {

		DT_Tdep_PT_Printf(phead,
				  "Test Error: %s RMR bind problem, status = %s\n",
				  message,
				  (rmr_p->status ==
				   DAT_RMR_BIND_SUCCESS ? "OK" : "FAILURE"));
		DT_Test_Error();
		if (rmr_p->rmr_handle != rmr_expected) {
			DT_Tdep_PT_Printf(phead,
					  "\tRMR handle mismatch (got 0x%p wanted 0x%p)\n",
					  rmr_p->rmr_handle, rmr_expected);
		}
		if (rmr_p->user_cookie.as_ptr != cookie_expected) {
			DT_Tdep_PT_Printf(phead,
					  "\tCookie mismatch (got %p wanted %p)\n",
					  rmr_p->user_cookie.as_ptr,
					  cookie_expected);
		}
		return (false);
	}

	return (true);
}

/* -----------------------------------------------------------
 * Check a CR and print some debug info if anything is amiss.
 */
bool
DT_cr_check(DT_Tdep_Print_Head * phead,
	    DAT_CR_ARRIVAL_EVENT_DATA * cr_stat_p,
	    DAT_PSP_HANDLE psp_handle_expected,
	    DAT_CONN_QUAL port_expected,
	    DAT_CR_HANDLE * cr_handlep, char *message)
{
	DAT_RETURN ret;

	if (cr_handlep) {
		*cr_handlep = (DAT_CR_HANDLE) 0;
	}

	if (cr_stat_p->conn_qual != port_expected ||
	    (psp_handle_expected &&
	     cr_stat_p->sp_handle.psp_handle != psp_handle_expected)) {

		DT_Tdep_PT_Printf(phead, "Test Error: %s CR data problem\n",
				  message);
		DT_Test_Error();
		if (cr_stat_p->conn_qual != port_expected) {
			DT_Tdep_PT_Printf(phead, "\tCR conn_qual mismatch "
					  " (got 0x" F64x " wanted 0x" F64x
					  ")\n", cr_stat_p->conn_qual,
					  port_expected);
		}
		if (psp_handle_expected &&
		    cr_stat_p->sp_handle.psp_handle != psp_handle_expected) {
			DT_Tdep_PT_Printf(phead,
					  "\tPSP mismatch (got 0x%p wanted 0x%p)\n",
					  cr_stat_p->sp_handle.psp_handle,
					  psp_handle_expected);
		}
		if (!cr_stat_p->cr_handle) {
			DT_Tdep_PT_Printf(phead, "\tGot NULL cr_handle\n");
		} else {
			ret = dat_cr_reject(cr_stat_p->cr_handle, 0, NULL);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "\tdat_cr_reject error: %s\n",
						  DT_RetToString(ret));
			}
		}
		return (false);
	}

	if (cr_handlep) {
		*cr_handlep = cr_stat_p->cr_handle;
	}
	return (true);
}
