/*
 * Copyright (c) 2006-2008 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2006 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*
 * Abstract:
 *    Implementation of service records testing flow..
 *    Top level is osmt_run_service_records_flow:
 *     osmt_register_service
 *     osmt_get_service_by_name
 *     osmt_get_all_services
 *     osmt_delete_service_by_name
 *
 */

#ifndef __WIN__
#include <unistd.h>
#else
#include <time.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <complib/cl_debug.h>
#include "osmtest.h"

/**********************************************************************
 **********************************************************************/

ib_api_status_t
osmt_register_service(IN osmtest_t * const p_osmt,
		      IN ib_net64_t service_id,
		      IN ib_net16_t service_pkey,
		      IN ib_net32_t service_lease,
		      IN uint8_t service_key_lsb, IN char *service_name)
{
	osmv_query_req_t req;
	osmv_user_query_t user;
	osmtest_req_context_t context;
	ib_service_record_t svc_rec;
	osm_log_t *p_log = &p_osmt->log;
	ib_api_status_t status;

	OSM_LOG_ENTER(p_log);

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Registering service: name: %s id: 0x%" PRIx64 "\n",
		service_name, cl_ntoh64(service_id));

	memset(&req, 0, sizeof(req));
	memset(&context, 0, sizeof(context));
	memset(&user, 0, sizeof(user));
	memset(&svc_rec, 0, sizeof(svc_rec));

	/* set the new service record fields */
	svc_rec.service_id = service_id;
	svc_rec.service_pkey = service_pkey;
	svc_rec.service_gid.unicast.prefix = 0;
	svc_rec.service_gid.unicast.interface_id = p_osmt->local_port.port_guid;
	svc_rec.service_lease = service_lease;
	memset(&svc_rec.service_key, 0, 16 * sizeof(uint8_t));
	svc_rec.service_key[0] = service_key_lsb;
	memset(svc_rec.service_name, 0, sizeof(svc_rec.service_name));
	memcpy(svc_rec.service_name, service_name,
	       (strlen(service_name) + 1) * sizeof(char));

	/* prepare the data used for this query */
	/*  sa_mad_data.method = IB_MAD_METHOD_SET; */
	/*  sa_mad_data.sm_key = 0; */

	context.p_osmt = p_osmt;
	req.query_context = &context;
	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.sm_key = 0;
	req.timeout_ms = p_osmt->opt.transaction_timeout;

	user.method = IB_MAD_METHOD_SET;
	user.attr_id = IB_MAD_ATTR_SERVICE_RECORD;
	if (ib_pkey_is_invalid(service_pkey)) {
		/* if given an invalid service_pkey - don't turn the PKEY compmask on */
		user.comp_mask = IB_SR_COMPMASK_SID |
		    IB_SR_COMPMASK_SGID |
		    IB_SR_COMPMASK_SLEASE |
		    IB_SR_COMPMASK_SKEY | IB_SR_COMPMASK_SNAME;
	} else {
		user.comp_mask = IB_SR_COMPMASK_SID |
		    IB_SR_COMPMASK_SGID |
		    IB_SR_COMPMASK_SPKEY |
		    IB_SR_COMPMASK_SLEASE |
		    IB_SR_COMPMASK_SKEY | IB_SR_COMPMASK_SNAME;
	}
	user.attr_offset = ib_get_attr_offset(sizeof(ib_service_record_t));
	user.p_attr = &svc_rec;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A01: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = context.result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A02: "
			"ib_query failed (%s)\n", ib_get_err_str(status));

		if (status == IB_REMOTE_ERROR) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(osm_madw_get_mad_ptr
						      (context.result.
						       p_result_madw)));
		}
		goto Exit;
	}

Exit:
	if (context.result.p_result_madw != NULL) {
		osm_mad_pool_put(&p_osmt->mad_pool,
				 context.result.p_result_madw);
		context.result.p_result_madw = NULL;
	}

	OSM_LOG_EXIT(&p_osmt->log);
	return status;
}

/**********************************************************************
 **********************************************************************/

ib_api_status_t
osmt_register_service_with_full_key(IN osmtest_t * const p_osmt,
				    IN ib_net64_t service_id,
				    IN ib_net16_t service_pkey,
				    IN ib_net32_t service_lease,
				    IN uint8_t * service_key,
				    IN char *service_name)
{
	osmv_query_req_t req;
	osmv_user_query_t user;
	osmtest_req_context_t context;
	ib_service_record_t svc_rec, *p_rec;
	osm_log_t *p_log = &p_osmt->log;
	ib_api_status_t status;
	uint8_t i, skey[16];

	OSM_LOG_ENTER(p_log);

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Registering service: name: %s id: 0x%" PRIx64 "\n",
		service_name, cl_ntoh64(service_id));

	memset(&req, 0, sizeof(req));
	memset(&context, 0, sizeof(context));
	memset(&user, 0, sizeof(user));
	memset(&svc_rec, 0, sizeof(svc_rec));

	/* set the new service record fields */
	svc_rec.service_id = service_id;
	svc_rec.service_pkey = service_pkey;
	svc_rec.service_gid.unicast.prefix = 0;
	svc_rec.service_gid.unicast.interface_id = p_osmt->local_port.port_guid;
	svc_rec.service_lease = service_lease;
	memset(&svc_rec.service_key, 0, 16 * sizeof(uint8_t));
	memcpy(svc_rec.service_key, service_key, 16 * sizeof(uint8_t));
	memset(svc_rec.service_name, 0, sizeof(svc_rec.service_name));
	memset(skey, 0, 16 * sizeof(uint8_t));
	memcpy(svc_rec.service_name, service_name,
	       (strlen(service_name) + 1) * sizeof(char));

	/* prepare the data used for this query */
	/*  sa_mad_data.method = IB_MAD_METHOD_SET; */
	/*  sa_mad_data.sm_key = 0; */

	context.p_osmt = p_osmt;
	req.query_context = &context;
	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.sm_key = 0;
	req.timeout_ms = p_osmt->opt.transaction_timeout;

	user.method = IB_MAD_METHOD_SET;
	user.attr_id = IB_MAD_ATTR_SERVICE_RECORD;
	if (ib_pkey_is_invalid(service_pkey)) {
		/* if given an invalid service_pkey - don't turn the PKEY compmask on */
		user.comp_mask = IB_SR_COMPMASK_SID |
		    IB_SR_COMPMASK_SGID |
		    IB_SR_COMPMASK_SLEASE |
		    IB_SR_COMPMASK_SKEY | IB_SR_COMPMASK_SNAME;
	} else {
		user.comp_mask = IB_SR_COMPMASK_SID |
		    IB_SR_COMPMASK_SGID |
		    IB_SR_COMPMASK_SPKEY |
		    IB_SR_COMPMASK_SLEASE |
		    IB_SR_COMPMASK_SKEY | IB_SR_COMPMASK_SNAME;
	}
	user.attr_offset = ib_get_attr_offset(sizeof(ib_service_record_t));
	user.p_attr = &svc_rec;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A03: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = context.result.status;
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A04: "
			"ib_query failed (%s)\n", ib_get_err_str(status));

		if (status == IB_REMOTE_ERROR) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(osm_madw_get_mad_ptr
						      (context.result.
						       p_result_madw)));
		}
		goto Exit;
	}

	/*  Check service key on context to see if match */
	p_rec = osmv_get_query_svc_rec(context.result.p_result_madw, 0);
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Comparing service key...\n" "return key is:\n");
	for (i = 0; i <= 15; i++) {
		OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
			"service_key sent[%u] = %u, service_key returned[%u] = %u\n",
			i, service_key[i], i, p_rec->service_key[i]);
	}
	/*  since c15-0.1.14 not supported all key association queries should bring in return zero in service key */
	if (memcmp(skey, p_rec->service_key, 16 * sizeof(uint8_t)) != 0) {
		status = IB_REMOTE_ERROR;
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A33: "
			"Data mismatch in service_key\n");
		goto Exit;
	}

Exit:
	if (context.result.p_result_madw != NULL) {
		osm_mad_pool_put(&p_osmt->mad_pool,
				 context.result.p_result_madw);
		context.result.p_result_madw = NULL;
	}

	OSM_LOG_EXIT(&p_osmt->log);
	return status;
}

/**********************************************************************
 **********************************************************************/

ib_api_status_t
osmt_register_service_with_data(IN osmtest_t * const p_osmt,
				IN ib_net64_t service_id,
				IN ib_net16_t service_pkey,
				IN ib_net32_t service_lease,
				IN uint8_t service_key_lsb,
				IN uint8_t * service_data8,
				IN ib_net16_t * service_data16,
				IN ib_net32_t * service_data32,
				IN ib_net64_t * service_data64,
				IN char *service_name)
{
	osmv_query_req_t req;
	osmv_user_query_t user;
	osmtest_req_context_t context;
	ib_service_record_t svc_rec, *p_rec;
	osm_log_t *p_log = &p_osmt->log;
	ib_api_status_t status;
	/*   ib_service_record_t* p_rec; */

	OSM_LOG_ENTER(p_log);

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Registering service: name: %s id: 0x%" PRIx64 "\n",
		service_name, cl_ntoh64(service_id));

	memset(&req, 0, sizeof(req));
	memset(&context, 0, sizeof(context));
	memset(&user, 0, sizeof(user));
	memset(&svc_rec, 0, sizeof(svc_rec));

	/* set the new service record fields */
	svc_rec.service_id = service_id;
	svc_rec.service_pkey = service_pkey;
	svc_rec.service_gid.unicast.prefix = 0;
	svc_rec.service_gid.unicast.interface_id = p_osmt->local_port.port_guid;
	svc_rec.service_lease = service_lease;
	memset(&svc_rec.service_key, 0, 16 * sizeof(uint8_t));
	svc_rec.service_key[0] = service_key_lsb;

	/*  Copy data to service_data arrays */
	memcpy(svc_rec.service_data8, service_data8, 16 * sizeof(uint8_t));
	memcpy(svc_rec.service_data16, service_data16, 8 * sizeof(ib_net16_t));
	memcpy(svc_rec.service_data32, service_data32, 4 * sizeof(ib_net32_t));
	memcpy(svc_rec.service_data64, service_data64, 2 * sizeof(ib_net64_t));

	memset(svc_rec.service_name, 0, sizeof(svc_rec.service_name));
	memcpy(svc_rec.service_name, service_name,
	       (strlen(service_name) + 1) * sizeof(char));

	/* prepare the data used for this query */
	/*  sa_mad_data.method = IB_MAD_METHOD_SET; */
	/*  sa_mad_data.sm_key = 0; */

	context.p_osmt = p_osmt;
	req.query_context = &context;
	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.sm_key = 0;
	req.timeout_ms = p_osmt->opt.transaction_timeout;

	user.method = IB_MAD_METHOD_SET;
	user.attr_id = IB_MAD_ATTR_SERVICE_RECORD;
	if (ib_pkey_is_invalid(service_pkey)) {
		/* if given an invalid service_pkey - don't turn the PKEY compmask on */
		user.comp_mask = IB_SR_COMPMASK_SID |
		    IB_SR_COMPMASK_SGID |
		    IB_SR_COMPMASK_SLEASE |
		    IB_SR_COMPMASK_SKEY |
		    IB_SR_COMPMASK_SNAME |
		    IB_SR_COMPMASK_SDATA8_0 |
		    IB_SR_COMPMASK_SDATA8_1 |
		    IB_SR_COMPMASK_SDATA16_0 |
		    IB_SR_COMPMASK_SDATA16_1 |
		    IB_SR_COMPMASK_SDATA32_0 |
		    IB_SR_COMPMASK_SDATA32_1 |
		    IB_SR_COMPMASK_SDATA64_0 | IB_SR_COMPMASK_SDATA64_1;
	} else {
		user.comp_mask = IB_SR_COMPMASK_SID |
		    IB_SR_COMPMASK_SGID |
		    IB_SR_COMPMASK_SPKEY |
		    IB_SR_COMPMASK_SLEASE |
		    IB_SR_COMPMASK_SKEY |
		    IB_SR_COMPMASK_SNAME |
		    IB_SR_COMPMASK_SDATA8_0 |
		    IB_SR_COMPMASK_SDATA8_1 |
		    IB_SR_COMPMASK_SDATA16_0 |
		    IB_SR_COMPMASK_SDATA16_1 |
		    IB_SR_COMPMASK_SDATA32_0 |
		    IB_SR_COMPMASK_SDATA32_1 |
		    IB_SR_COMPMASK_SDATA64_0 | IB_SR_COMPMASK_SDATA64_1;
	}
	user.attr_offset = ib_get_attr_offset(sizeof(ib_service_record_t));
	user.p_attr = &svc_rec;

	/*  Dump to Service Data b4 send */
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Dumping service data b4 send\n");
	osm_dump_service_record(&p_osmt->log, &svc_rec, OSM_LOG_VERBOSE);

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A05: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = context.result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A06: "
			"ib_query failed (%s)\n", ib_get_err_str(status));

		if (status == IB_REMOTE_ERROR) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(osm_madw_get_mad_ptr
						      (context.result.
						       p_result_madw)));
		}
		goto Exit;
	}

	/*  Check data on context to see if match */
	p_rec = osmv_get_query_svc_rec(context.result.p_result_madw, 0);
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Comparing service data...\n");
	if (memcmp(service_data8, p_rec->service_data8, 16 * sizeof(uint8_t)) !=
	    0
	    || memcmp(service_data16, p_rec->service_data16,
		      8 * sizeof(uint16_t)) != 0
	    || memcmp(service_data32, p_rec->service_data32,
		      4 * sizeof(uint32_t)) != 0
	    || memcmp(service_data64, p_rec->service_data64,
		      2 * sizeof(uint64_t)) != 0) {
		status = IB_REMOTE_ERROR;
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
			"Data mismatch in service_data8\n");
		goto Exit;
	}

Exit:
	if (context.result.p_result_madw != NULL) {
		osm_mad_pool_put(&p_osmt->mad_pool,
				 context.result.p_result_madw);
		context.result.p_result_madw = NULL;
	}

	OSM_LOG_EXIT(&p_osmt->log);
	return status;
}

/**********************************************************************
 **********************************************************************/

ib_api_status_t
osmt_get_service_by_id_and_name(IN osmtest_t * const p_osmt,
				IN uint32_t rec_num,
				IN ib_net64_t sid,
				IN char *sr_name,
				OUT ib_service_record_t * p_out_rec)
{

	ib_api_status_t status = IB_SUCCESS;
	osmtest_req_context_t context;
	osmv_query_req_t req;
	ib_service_record_t svc_rec, *p_rec;
	uint32_t num_recs = 0;
	osmv_user_query_t user;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Getting service record: id: 0x%016" PRIx64
		" and name: %s\n", cl_ntoh64(sid), sr_name);

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&context, 0, sizeof(context));

	context.p_osmt = p_osmt;

	/* prepare the data used for this query */
	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = &context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.sm_key = 0;

	memset(&svc_rec, 0, sizeof(svc_rec));
	memset(&user, 0, sizeof(user));
	/* set the new service record fields */
	memset(svc_rec.service_name, 0, sizeof(svc_rec.service_name));
	memcpy(svc_rec.service_name, sr_name,
	       (strlen(sr_name) + 1) * sizeof(char));
	svc_rec.service_id = sid;
	req.p_query_input = &user;

	user.method = IB_MAD_METHOD_GET;
	user.attr_id = IB_MAD_ATTR_SERVICE_RECORD;
	user.comp_mask = IB_SR_COMPMASK_SID | IB_SR_COMPMASK_SNAME;
	user.attr_offset = ib_get_attr_offset(sizeof(ib_service_record_t));
	user.p_attr = &svc_rec;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A07: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = context.result.status;
	num_recs = context.result.result_cnt;

	if (status != IB_SUCCESS) {
		char mad_stat_err[256];

		/* If the failure is due to IB_SA_MAD_STATUS_NO_RECORDS and rec_num is 0,
		   then this is fine */
		if (status == IB_REMOTE_ERROR)
			strcpy(mad_stat_err,
			       ib_get_mad_status_str(osm_madw_get_mad_ptr
						     (context.result.
						      p_result_madw)));
		else
			strcpy(mad_stat_err, ib_get_err_str(status));
		if (status == IB_REMOTE_ERROR &&
		    !strcmp(mad_stat_err, "IB_SA_MAD_STATUS_NO_RECORDS") &&
		    rec_num == 0) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"IS EXPECTED ERROR ^^^^\n");
			status = IB_SUCCESS;
		} else {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A08: "
				"Query failed: %s (%s)\n",
				ib_get_err_str(status), mad_stat_err);
			goto Exit;
		}
	}

	if (rec_num && num_recs != rec_num) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
			"Unmatched number of records: expected: %d, received: %d\n",
			rec_num, num_recs);
		status = IB_REMOTE_ERROR;
		goto Exit;
	}

	p_rec = osmv_get_query_svc_rec(context.result.p_result_madw, 0);
	*p_out_rec = *p_rec;

	if (num_recs) {
		OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
			"Found service record: name: %s id: 0x%016" PRIx64 "\n",
			p_rec->service_name, cl_ntoh64(p_rec->service_id));

		osm_dump_service_record(&p_osmt->log, p_rec, OSM_LOG_DEBUG);
	}

Exit:
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Expected and found %d records\n", rec_num);

	if (context.result.p_result_madw != NULL) {
		osm_mad_pool_put(&p_osmt->mad_pool,
				 context.result.p_result_madw);
		context.result.p_result_madw = NULL;
	}

	OSM_LOG_EXIT(&p_osmt->log);
	return status;
}

/**********************************************************************
 **********************************************************************/

ib_api_status_t
osmt_get_service_by_id(IN osmtest_t * const p_osmt,
		       IN uint32_t rec_num,
		       IN ib_net64_t sid, OUT ib_service_record_t * p_out_rec)
{

	ib_api_status_t status = IB_SUCCESS;
	osmtest_req_context_t context;
	osmv_query_req_t req;
	ib_service_record_t svc_rec, *p_rec;
	uint32_t num_recs = 0;
	osmv_user_query_t user;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Getting service record: id: 0x%016" PRIx64 "\n",
		cl_ntoh64(sid));

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&context, 0, sizeof(context));

	context.p_osmt = p_osmt;

	/* prepare the data used for this query */
	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = &context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.sm_key = 0;

	memset(&svc_rec, 0, sizeof(svc_rec));
	memset(&user, 0, sizeof(user));
	/* set the new service record fields */
	svc_rec.service_id = sid;
	req.p_query_input = &user;

	user.method = IB_MAD_METHOD_GET;
	user.attr_id = IB_MAD_ATTR_SERVICE_RECORD;
	user.comp_mask = IB_SR_COMPMASK_SID;
	user.attr_offset = ib_get_attr_offset(sizeof(ib_service_record_t));
	user.p_attr = &svc_rec;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A09: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = context.result.status;
	num_recs = context.result.result_cnt;

	if (status != IB_SUCCESS) {
		char mad_stat_err[256];

		/* If the failure is due to IB_SA_MAD_STATUS_NO_RECORDS and rec_num is 0,
		   then this is fine */
		if (status == IB_REMOTE_ERROR)
			strcpy(mad_stat_err,
			       ib_get_mad_status_str(osm_madw_get_mad_ptr
						     (context.result.
						      p_result_madw)));
		else
			strcpy(mad_stat_err, ib_get_err_str(status));

		if (status == IB_REMOTE_ERROR &&
		    !strcmp(mad_stat_err, "IB_SA_MAD_STATUS_NO_RECORDS") &&
		    rec_num == 0) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"IS EXPECTED ERROR ^^^^\n");
			status = IB_SUCCESS;
		} else {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A0A: "
				"Query failed: %s (%s)\n",
				ib_get_err_str(status), mad_stat_err);
			goto Exit;
		}
	}

	if (rec_num && num_recs != rec_num) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A0B: "
			"Unmatched number of records: expected: %d received: %d\n",
			rec_num, num_recs);
		status = IB_REMOTE_ERROR;
		goto Exit;
	}

	p_rec = osmv_get_query_svc_rec(context.result.p_result_madw, 0);
	*p_out_rec = *p_rec;

	if (num_recs) {
		OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
			"Found service record: name: %s id: 0x%016" PRIx64 "\n",
			p_rec->service_name, cl_ntoh64(p_rec->service_id));

		osm_dump_service_record(&p_osmt->log, p_rec, OSM_LOG_DEBUG);
	}

Exit:
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Expected and found %d records\n", rec_num);

	if (context.result.p_result_madw != NULL) {
		osm_mad_pool_put(&p_osmt->mad_pool,
				 context.result.p_result_madw);
		context.result.p_result_madw = NULL;
	}

	OSM_LOG_EXIT(&p_osmt->log);
	return status;
}

/**********************************************************************
 **********************************************************************/

ib_api_status_t
osmt_get_service_by_name_and_key(IN osmtest_t * const p_osmt,
				 IN char *sr_name,
				 IN uint32_t rec_num,
				 IN uint8_t * skey,
				 OUT ib_service_record_t * p_out_rec)
{

	ib_api_status_t status = IB_SUCCESS;
	osmtest_req_context_t context;
	osmv_query_req_t req;
	ib_service_record_t svc_rec, *p_rec;
	uint32_t num_recs = 0, i;
	osmv_user_query_t user;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Getting service record: name: %s and key: "
		"0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
		sr_name, skey[0], skey[1], skey[2], skey[3], skey[4], skey[5],
		skey[6], skey[7], skey[8], skey[9], skey[10], skey[11],
		skey[12], skey[13], skey[14], skey[15]);

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&context, 0, sizeof(context));

	context.p_osmt = p_osmt;

	/* prepare the data used for this query */
	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = &context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.sm_key = 0;

	memset(&svc_rec, 0, sizeof(svc_rec));
	memset(&user, 0, sizeof(user));
	/* set the new service record fields */
	memset(svc_rec.service_name, 0, sizeof(svc_rec.service_name));
	memcpy(svc_rec.service_name, sr_name,
	       (strlen(sr_name) + 1) * sizeof(char));
	for (i = 0; i <= 15; i++)
		svc_rec.service_key[i] = skey[i];

	req.p_query_input = &user;

	user.method = IB_MAD_METHOD_GET;
	user.attr_id = IB_MAD_ATTR_SERVICE_RECORD;
	user.comp_mask = IB_SR_COMPMASK_SNAME | IB_SR_COMPMASK_SKEY;
	user.attr_offset = ib_get_attr_offset(sizeof(ib_service_record_t));
	user.p_attr = &svc_rec;
	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A0C: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = context.result.status;
	num_recs = context.result.result_cnt;

	if (status != IB_SUCCESS) {
		char mad_stat_err[256];

		/* If the failure is due to IB_SA_MAD_STATUS_NO_RECORDS and rec_num is 0,
		   then this is fine */
		if (status == IB_REMOTE_ERROR)
			strcpy(mad_stat_err,
			       ib_get_mad_status_str(osm_madw_get_mad_ptr
						     (context.result.
						      p_result_madw)));
		else
			strcpy(mad_stat_err, ib_get_err_str(status));

		if (status == IB_REMOTE_ERROR &&
		    !strcmp(mad_stat_err, "IB_SA_MAD_STATUS_NO_RECORDS") &&
		    rec_num == 0) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"IS EXPECTED ERROR ^^^^\n");
			status = IB_SUCCESS;
		} else {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A0D: "
				"Query failed:%s (%s)\n",
				ib_get_err_str(status), mad_stat_err);
			goto Exit;
		}
	}

	if (rec_num && num_recs != rec_num) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
			"Unmatched number of records: expected: %d, received: %d\n",
			rec_num, num_recs);
		status = IB_REMOTE_ERROR;
		goto Exit;
	}

	p_rec = osmv_get_query_svc_rec(context.result.p_result_madw, 0);
	*p_out_rec = *p_rec;

	if (num_recs) {
		OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
			"Found service record: name: %s id: 0x%016" PRIx64 "\n",
			sr_name, cl_ntoh64(p_rec->service_id));

		osm_dump_service_record(&p_osmt->log, p_rec, OSM_LOG_DEBUG);
	}

Exit:
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Expected and found %d records\n", rec_num);

	if (context.result.p_result_madw != NULL) {
		osm_mad_pool_put(&p_osmt->mad_pool,
				 context.result.p_result_madw);
		context.result.p_result_madw = NULL;
	}

	OSM_LOG_EXIT(&p_osmt->log);
	return status;
}

/**********************************************************************
 **********************************************************************/

ib_api_status_t
osmt_get_service_by_name(IN osmtest_t * const p_osmt,
			 IN char *sr_name,
			 IN uint32_t rec_num,
			 OUT ib_service_record_t * p_out_rec)
{

	ib_api_status_t status = IB_SUCCESS;
	osmtest_req_context_t context;
	osmv_query_req_t req;
	ib_service_record_t *p_rec;
	ib_svc_name_t service_name;
	uint32_t num_recs = 0;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Getting service record: name: %s\n", sr_name);

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&context, 0, sizeof(context));

	context.p_osmt = p_osmt;

	/* prepare the data used for this query */
	req.query_type = OSMV_QUERY_SVC_REC_BY_NAME;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = &context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.sm_key = 0;

	memset(service_name, 0, sizeof(service_name));
	memcpy(service_name, sr_name, (strlen(sr_name) + 1) * sizeof(char));
	req.p_query_input = service_name;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A0E: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = context.result.status;
	num_recs = context.result.result_cnt;

	if (status != IB_SUCCESS) {
		char mad_stat_err[256];

		/*  If the failure is due to IB_SA_MAD_STATUS_NO_RECORDS and rec_num is 0,
		   then this is fine */
		if (status == IB_REMOTE_ERROR)
			strcpy(mad_stat_err,
			       ib_get_mad_status_str(osm_madw_get_mad_ptr
						     (context.result.
						      p_result_madw)));
		else
			strcpy(mad_stat_err, ib_get_err_str(status));

		if (status == IB_REMOTE_ERROR &&
		    !strcmp(mad_stat_err, "IB_SA_MAD_STATUS_NO_RECORDS") &&
		    rec_num == 0) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"IS EXPECTED ERROR ^^^^\n");
			status = IB_SUCCESS;
		} else {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A0F: "
				"Query failed: %s (%s)\n",
				ib_get_err_str(status), mad_stat_err);
			goto Exit;
		}
	}

	if (rec_num && num_recs != rec_num) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A10: "
			"Unmatched number of records: expected: %d, received: %d\n",
			rec_num, num_recs);
		status = IB_REMOTE_ERROR;
		goto Exit;
	}

	p_rec = osmv_get_query_svc_rec(context.result.p_result_madw, 0);
	*p_out_rec = *p_rec;

	if (num_recs) {
		OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
			"Found service record: name: %s id: 0x%016" PRIx64 "\n",
			sr_name, cl_ntoh64(p_rec->service_id));

		osm_dump_service_record(&p_osmt->log, p_rec, OSM_LOG_DEBUG);
	}

Exit:
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Expected and found %d records\n", rec_num);

	if (context.result.p_result_madw != NULL) {
		osm_mad_pool_put(&p_osmt->mad_pool,
				 context.result.p_result_madw);
		context.result.p_result_madw = NULL;
	}

	OSM_LOG_EXIT(&p_osmt->log);
	return status;
}

/**********************************************************************
 **********************************************************************/

#ifdef VENDOR_RMPP_SUPPORT
ib_api_status_t
osmt_get_all_services_and_check_names(IN osmtest_t * const p_osmt,
				      IN ib_svc_name_t *
				      const p_valid_service_names_arr,
				      IN uint8_t num_of_valid_names,
				      OUT uint32_t * num_services)
{
	ib_api_status_t status = IB_SUCCESS;
	osmtest_req_context_t context;
	osmv_query_req_t req;
	ib_service_record_t *p_rec;
	uint32_t num_recs = 0, i, j;
	uint8_t *p_checked_names;

	OSM_LOG_ENTER(&p_osmt->log);

	/* Prepare tracker for the checked names */
	p_checked_names =
	    (uint8_t *) malloc(sizeof(uint8_t) * num_of_valid_names);
	for (j = 0; j < num_of_valid_names; j++) {
		p_checked_names[j] = 0;
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Getting all service records\n");

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&context, 0, sizeof(context));

	context.p_osmt = p_osmt;

	req.query_type = OSMV_QUERY_ALL_SVC_RECS;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = &context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A12: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = context.result.status;

	if (status != IB_SUCCESS) {
		if (status != IB_INVALID_PARAMETER) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A13: "
				"ib_query failed (%s)\n", ib_get_err_str(status));
		}
		if (status == IB_REMOTE_ERROR) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(osm_madw_get_mad_ptr
						      (context.result.
						       p_result_madw)));
		}
		goto Exit;
	}

	num_recs = context.result.result_cnt;
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Received %u records\n", num_recs);

	for (i = 0; i < num_recs; i++) {
		p_rec = osmv_get_query_svc_rec(context.result.p_result_madw, i);
		OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
			"Found service record: name: %s id: 0x%016" PRIx64 "\n",
			p_rec->service_name, cl_ntoh64(p_rec->service_id));
		osm_dump_service_record(&p_osmt->log, p_rec, OSM_LOG_VERBOSE);
		for (j = 0; j < num_of_valid_names; j++) {
			/* If the service names exist in the record, mark it as checked (1) */
			OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
				"-I- Comparing source name : >%s<, with record name : >%s<, idx : %d\n",
				p_valid_service_names_arr[j],
				p_rec->service_name, p_checked_names[j]);
			if (strcmp
			    ((char *)p_valid_service_names_arr[j],
			     (char *)p_rec->service_name) == 0) {
				OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
					"-I- The service %s is valid\n",
					p_valid_service_names_arr[j]);
				p_checked_names[j] = 1;
				break;
			}
		}
	}
	/* Check that all service names have been identified */
	for (j = 0; j < num_of_valid_names; j++)
		if (p_checked_names[j] == 0) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A14: "
				"Missing valid service: name: %s\n",
				p_valid_service_names_arr[j]);
			status = IB_ERROR;
			goto Exit;
		}
	*num_services = num_recs;

Exit:
	if (context.result.p_result_madw != NULL) {
		osm_mad_pool_put(&p_osmt->mad_pool,
				 context.result.p_result_madw);
		context.result.p_result_madw = NULL;
	}

	OSM_LOG_EXIT(&p_osmt->log);
	return status;
}
#endif

/**********************************************************************
 **********************************************************************/

ib_api_status_t
osmt_delete_service_by_name(IN osmtest_t * const p_osmt,
			    IN uint8_t IsServiceExist,
			    IN char *sr_name, IN uint32_t rec_num)
{
	osmv_query_req_t req;
	osmv_user_query_t user;
	osmtest_req_context_t context;
	ib_service_record_t svc_rec;
	ib_api_status_t status;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Trying to Delete service name: %s\n", sr_name);

	memset(&svc_rec, 0, sizeof(svc_rec));

	status = osmt_get_service_by_name(p_osmt, sr_name, rec_num, &svc_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A15: "
			"Failed to get service: name: %s\n", sr_name);
		goto ExitNoDel;
	}

	memset(&req, 0, sizeof(req));
	memset(&context, 0, sizeof(context));
	memset(&user, 0, sizeof(user));

	/* set the new service record fields */
	memset(svc_rec.service_name, 0, sizeof(svc_rec.service_name));
	memcpy(svc_rec.service_name, sr_name,
	       (strlen(sr_name) + 1) * sizeof(char));

	/* prepare the data used for this query */
	context.p_osmt = p_osmt;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.query_context = &context;
	req.query_type = OSMV_QUERY_USER_DEFINED;	/*  basically a don't care here */
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.sm_key = 0;

	user.method = IB_MAD_METHOD_DELETE;
	user.attr_id = IB_MAD_ATTR_SERVICE_RECORD;
	user.comp_mask = IB_SR_COMPMASK_SNAME;
	user.attr_offset = ib_get_attr_offset(sizeof(ib_service_record_t));
	user.p_attr = &svc_rec;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A16: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = context.result.status;
	if (IsServiceExist) {
		/* If IsServiceExist = 1 then we should succeed here */
		if (status != IB_SUCCESS) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A17: "
				"ib_query failed (%s)\n",
				ib_get_err_str(status));

			if (status == IB_REMOTE_ERROR) {
				OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
					"ERR 4A18: Remote error = %s\n",
					ib_get_mad_status_str
					(osm_madw_get_mad_ptr
					 (context.result.p_result_madw)));
			}
		}
	} else {
		/* If IsServiceExist = 0 then we should fail here */
		if (status == IB_SUCCESS) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A19: "
				"Succeeded to delete service: %s which "
				"shouldn't exist", sr_name);
			status = IB_ERROR;
		} else {
			/* The deletion should have failed, since the service_name
			   shouldn't exist. */
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"IS EXPECTED ERROR ^^^^\n");
			OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
				"Failed to delete service_name: %s\n", sr_name);
			status = IB_SUCCESS;
		}
	}

Exit:
	if (context.result.p_result_madw != NULL) {
		osm_mad_pool_put(&p_osmt->mad_pool,
				 context.result.p_result_madw);
		context.result.p_result_madw = NULL;
	}

ExitNoDel:
	OSM_LOG_EXIT(&p_osmt->log);
	return status;
}

/**********************************************************************
 **********************************************************************/

/*
 * Run a complete service records flow:
 * - register a service
 * - register a service (with a lease period)
 * - get a service by name
 * - get all services / must be 2
 * - delete a service
 * - get all services / must be 1
 * - wait for the lease to expire
 * - get all services / must be 0
 * - get / set service by data
 */
ib_api_status_t osmt_run_service_records_flow(IN osmtest_t * const p_osmt)
{
	ib_service_record_t srv_rec;
	ib_api_status_t status;
	uint8_t instance, i;
	uint8_t service_data8[16], service_key[16];
	ib_net16_t service_data16[8];
	ib_net32_t service_data32[4];
	ib_net64_t service_data64[2];
	uint64_t pid = getpid();
	uint64_t id[7];
	/* We use up to seven service names - we use the extra for bad flow */
	ib_svc_name_t service_name[7];
#ifdef VENDOR_RMPP_SUPPORT
	/* This array contain only the valid names after registering vs SM */
	ib_svc_name_t service_valid_names[3];
	uint32_t num_recs = 0;
#endif

	OSM_LOG_ENTER(&p_osmt->log);

	/* Init Service names */
	for (i = 0; i < 7; i++) {
#ifdef __WIN__
		uint64_t rand_val = rand() - (uint64_t) i;
#else
		uint64_t rand_val = random() - (uint64_t) i;
#endif
		id[i] = abs((int)(pid - rand_val));
		/* Just to be unique any place on any host */
		sprintf((char *)(service_name[i]),
			"osmt.srvc.%" PRIu64 ".%" PRIu64, rand_val, pid);
		/*printf("-I- Service Name is : %s, ID is : 0x%" PRIx64 "\n",service_name[i],id[i]); */
	}

	status = osmt_register_service(p_osmt, cl_ntoh64(id[0]),	/*  IN ib_net64_t      service_id, */
				       IB_DEFAULT_PKEY,	/*  IN ib_net16_t      service_pkey, */
				       0xFFFFFFFF,	/*  IN ib_net32_t      service_lease, */
				       11,	/*  IN uint8_t         service_key_lsb, */
				       (char *)service_name[0]	/*  IN char            *service_name */
	    );
	if (status != IB_SUCCESS) {
		goto Exit;
	}

	status = osmt_register_service(p_osmt, cl_ntoh64(id[1]),	/*  IN ib_net64_t      service_id, */
				       IB_DEFAULT_PKEY,	/*  IN ib_net16_t      service_pkey, */
				       cl_hton32(0x00000004),	/*  IN ib_net32_t     service_lease, */
				       11,	/*  IN uint8_t         service_key_lsb, */
				       (char *)service_name[1]	/*  IN char            *service_name */
	    );
	if (status != IB_SUCCESS) {
		goto Exit;
	}

	status = osmt_register_service(p_osmt, cl_ntoh64(id[2]),	/*  IN ib_net64_t      service_id, */
				       0,	/*  IN ib_net16_t      service_pkey, */
				       0xFFFFFFFF,	/*  IN ib_net32_t      service_lease, */
				       11,	/* Remove Service Record IN uint8_t service_key_lsb, */
				       (char *)service_name[2]	/*  IN char            *service_name */
	    );

	if (status != IB_SUCCESS) {
		goto Exit;
	}

	/*  Generate 2 instances of service record with consecutive data */
	for (instance = 0; instance < 2; instance++) {
		/*  First, clear all arrays */
		memset(service_data8, 0, 16 * sizeof(uint8_t));
		memset(service_data16, 0, 8 * sizeof(uint16_t));
		memset(service_data32, 0, 4 * sizeof(uint32_t));
		memset(service_data64, 0, 2 * sizeof(uint64_t));
		service_data8[instance] = instance + 1;
		service_data16[instance] = cl_hton16(instance + 2);
		service_data32[instance] = cl_hton32(instance + 3);
		service_data64[instance] = cl_hton64(instance + 4);
		status = osmt_register_service_with_data(p_osmt, cl_ntoh64(id[3]),	/*  IN ib_net64_t      service_id, */
							 IB_DEFAULT_PKEY,	/*  IN ib_net16_t      service_pkey, */
							 cl_ntoh32(10),	/*  IN ib_net32_t      service_lease, */
							 12,	/*  IN uint8_t         service_key_lsb, */
							 service_data8, service_data16, service_data32, service_data64,	/* service data structures */
							 (char *)service_name[3]	/*  IN char            *service_name */
		    );

		if (status != IB_SUCCESS) {
			goto Exit;
		}

	}

	/*  Trying to create service with zero key */
	memset(service_key, 0, 16 * sizeof(uint8_t));
	status = osmt_register_service_with_full_key(p_osmt, cl_ntoh64(id[5]),	/*  IN ib_net64_t      service_id, */
						     0,	/*  IN ib_net16_t      service_pkey, */
						     0xFFFFFFFF,	/*  IN ib_net32_t      service_lease, */
						     service_key,	/*  full service_key, */
						     (char *)service_name[5]	/*  IN char            *service_name */
	    );

	if (status != IB_SUCCESS) {
		goto Exit;
	}

	/*  Now update it with Unique key and different service name */
	for (i = 0; i <= 15; i++) {
		service_key[i] = i + 1;
	}
	status = osmt_register_service_with_full_key(p_osmt, cl_ntoh64(id[5]),	/*  IN ib_net64_t      service_id, */
						     0,	/*  IN ib_net16_t      service_pkey, */
						     0xFFFFFFFF,	/*  IN ib_net32_t      service_lease, */
						     service_key,	/* full service_key, */
						     (char *)service_name[6]	/*  IN char            *service_name */
	    );
	if (status != IB_SUCCESS) {
		goto Exit;
	}

	/* Let OpenSM handle it */
	usleep(100);

	/* Make sure service_name[0] exists */
	status = osmt_get_service_by_name(p_osmt,
					  (char *)service_name[0], 1, &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A1A: "
			"Fail to find service: name: %s\n",
			(char *)service_name[0]);
		status = IB_ERROR;
		goto Exit;
	}

	/* Make sure service_name[1] exists */
	status = osmt_get_service_by_name(p_osmt,
					  (char *)service_name[1], 1, &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A1B: "
			"Fail to find service: name: %s\n",
			(char *)service_name[1]);
		status = IB_ERROR;
		goto Exit;
	}

	/* Make sure service_name[2] exists */
	status = osmt_get_service_by_name(p_osmt,
					  (char *)service_name[2], 1, &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A1C: "
			"Fail to find service: name: %s\n",
			(char *)service_name[2]);
		status = IB_ERROR;
		goto Exit;
	}

	/* Make sure service_name[3] exists. */
	/* After 10 seconds the service should not exist: service_lease = 10 */
	status = osmt_get_service_by_name(p_osmt,
					  (char *)service_name[3], 1, &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A1D: "
			"Fail to find service: name: %s\n",
			(char *)service_name[3]);
		status = IB_ERROR;
		goto Exit;
	}

	sleep(10);

	status = osmt_get_service_by_name(p_osmt,
					  (char *)service_name[3], 0, &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A1E: "
			"Found service: name: %s that should have been "
			"deleted due to service lease expiring\n",
			(char *)service_name[3]);
		status = IB_ERROR;
		goto Exit;
	}

	/*  Check that for service: id[5] only one record exists */
	status = osmt_get_service_by_id(p_osmt, 1, cl_ntoh64(id[5]), &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A1F: "
			"Found number of records != 1 for "
			"service: id: 0x%016" PRIx64 "\n", id[5]);
		status = IB_ERROR;
		goto Exit;
	}

	/*  Bad Flow of Get with invalid Service ID: id[6] */
	status = osmt_get_service_by_id(p_osmt, 0, cl_ntoh64(id[6]), &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A20: "
			"Found service: id: 0x%016" PRIx64 " "
			"that is invalid\n", id[6]);
		status = IB_ERROR;
		goto Exit;
	}

	/*  Check by both id and service name: id[0], service_name[0] */
	status = osmt_get_service_by_id_and_name(p_osmt, 1, cl_ntoh64(id[0]),
						 (char *)service_name[0],
						 &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A21: "
			"Fail to find service: id: 0x%016" PRIx64 " "
			"name: %s\n", id[0], (char *)service_name[0]);
		status = IB_ERROR;
		goto Exit;
	}

	/*  Check by both id and service name: id[5], service_name[6] */
	status = osmt_get_service_by_id_and_name(p_osmt, 1, cl_ntoh64(id[5]),
						 (char *)service_name[6],
						 &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A22: "
			"Fail to find service: id: 0x%016" PRIx64 " "
			"name: %s\n", id[5], (char *)service_name[6]);
		status = IB_ERROR;
		goto Exit;
	}

	/* Bad Flow of Get with invalid name(service_name[3]) and valid ID(id[0]) */
	status = osmt_get_service_by_id_and_name(p_osmt, 0, cl_ntoh64(id[0]),
						 (char *)service_name[3],
						 &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A23: "
			"Found service: id: 0x%016" PRIx64
			"name: %s which is an invalid service\n",
			id[0], (char *)service_name[3]);
		status = IB_ERROR;
		goto Exit;
	}

	/*  Bad Flow of Get with unmatched name(service_name[5]) and id(id[3]) (both valid) */
	status = osmt_get_service_by_id_and_name(p_osmt, 0, cl_ntoh64(id[3]),
						 (char *)service_name[5],
						 &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A24: "
			"Found service: id: 0x%016" PRIx64
			"name: %s which is an invalid service\n",
			id[3], (char *)service_name[5]);
		status = IB_ERROR;
		goto Exit;
	}

	/* Bad Flow of Get with service name that doesn't exist (service_name[4]) */
	status = osmt_get_service_by_name(p_osmt,
					  (char *)service_name[4], 0, &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A25: "
			"Found service: name: %s that shouldn't exist\n",
			(char *)service_name[4]);
		status = IB_ERROR;
		goto Exit;
	}

	/*  Bad Flow : Check that getting service_name[5] brings no records since another service
	   has been updated with the same ID (service_name[6] */
	status = osmt_get_service_by_name(p_osmt,
					  (char *)service_name[5], 0, &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A26: "
			"Found service: name: %s which is an "
			"invalid service\n", (char *)service_name[5]);
		status = IB_ERROR;
		goto Exit;
	}

	/*  Check that getting service_name[6] by name ONLY is valid,
	   since we do not support key&name association, also trusted queries */
	status = osmt_get_service_by_name(p_osmt,
					  (char *)service_name[6], 1, &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A27: "
			"Fail to find service: name: %s\n",
			(char *)service_name[6]);
		status = IB_ERROR;
		goto Exit;
	}

	/*  Test Service Key */
	memset(service_key, 0, 16 * sizeof(uint8_t));

	/* Check for service_name[5] with service_key=0 - the service shouldn't
	   exist with this name. */
	status = osmt_get_service_by_name_and_key(p_osmt,
						  (char *)service_name[5],
						  0, service_key, &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A28: "
			"Found service: name: %s key:0 which is an "
			"invalid service (wrong name)\n",
			(char *)service_name[5]);
		status = IB_ERROR;
		goto Exit;
	}

	/* Check for service_name[6] with service_key=0 - the service should
	   exist with different key. */
	status = osmt_get_service_by_name_and_key(p_osmt,
						  (char *)service_name[6],
						  0, service_key, &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A29: "
			"Found service: name: %s key: 0 which is an "
			"invalid service (wrong service_key)\n",
			(char *)service_name[6]);
		status = IB_ERROR;
		goto Exit;
	}

	/* check for service_name[6] with the correct service_key */
	for (i = 0; i <= 15; i++)
		service_key[i] = i + 1;
	status = osmt_get_service_by_name_and_key(p_osmt,
						  (char *)service_name[6],
						  1, service_key, &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A2A: "
			"Fail to find service: name: %s with "
			"correct service key\n", (char *)service_name[6]);
		status = IB_ERROR;
		goto Exit;
	}
#ifdef VENDOR_RMPP_SUPPORT
	/* These ar the only service_names which are valid */
	memcpy(&service_valid_names[0], &service_name[0], sizeof(uint8_t) * 64);
	memcpy(&service_valid_names[1], &service_name[2], sizeof(uint8_t) * 64);
	memcpy(&service_valid_names[2], &service_name[6], sizeof(uint8_t) * 64);

	status =
	    osmt_get_all_services_and_check_names(p_osmt, service_valid_names,
						  3, &num_recs);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A2B: "
			"Fail to find all services that should exist\n");
		status = IB_ERROR;
		goto Exit;
	}
#endif

	/* Delete service_name[0] */
	status = osmt_delete_service_by_name(p_osmt, 1,
					     (char *)service_name[0], 1);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A2C: "
			"Fail to delete service: name: %s\n",
			(char *)service_name[0]);
		status = IB_ERROR;
		goto Exit;
	}

	/* Make sure deletion of service_name[0] succeeded */
	status = osmt_get_service_by_name(p_osmt,
					  (char *)service_name[0], 0, &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A2D: "
			"Found service: name: %s that was deleted\n",
			(char *)service_name[0]);
		status = IB_ERROR;
		goto Exit;
	}

	/* Make sure service_name[1] doesn't exist (expired service lease) */
	status = osmt_get_service_by_name(p_osmt,
					  (char *)service_name[1], 0, &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A2E: "
			"Found service: name: %s that should have expired\n",
			(char *)service_name[1]);
		status = IB_ERROR;
		goto Exit;
	}

	/* Make sure service_name[2] exists */
	status = osmt_get_service_by_name(p_osmt,
					  (char *)service_name[2], 1, &srv_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A2F: "
			"Fail to find service: name: %s\n",
			(char *)service_name[2]);
		status = IB_ERROR;
		goto Exit;
	}

	/*  Bad Flow - try to delete non-existent service_name[5] */
	status = osmt_delete_service_by_name(p_osmt, 0,
					     (char *)service_name[5], 0);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A30: "
			"Succeed to delete non-existent service: name: %s\n",
			(char *)service_name[5]);
		status = IB_ERROR;
		goto Exit;
	}

	/* Delete service_name[2] */
	status = osmt_delete_service_by_name(p_osmt, 1,
					     (char *)service_name[2], 1);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A31: "
			"Fail to delete service: name: %s\n",
			(char *)service_name[2]);
		status = IB_ERROR;
		goto Exit;
	}

	/* Delete service_name[6] */
	status = osmt_delete_service_by_name(p_osmt, 1,
					     (char *)service_name[6], 1);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 4A32: "
			"Failed to delete service name: %s\n",
			(char *)service_name[6]);
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return status;
}
