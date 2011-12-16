/*
 * Copyright (c) 2006-2008 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
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
 * 	Implementation of Multicast Member testing flow..
 *
 */

#ifndef __WIN__
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <complib/cl_debug.h>
#include <complib/cl_map.h>
#include <complib/cl_list.h>
#include "osmtest.h"

/**********************************************************************
 **********************************************************************/

static void __osmt_print_all_multicast_records(IN osmtest_t * const p_osmt)
{
	uint32_t i;
	ib_api_status_t status;
	osmv_query_req_t req;
	osmv_user_query_t user;
	osmtest_req_context_t context;
	ib_member_rec_t *mcast_record;

	memset(&context, 0, sizeof(context));
	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));

	user.attr_id = IB_MAD_ATTR_MCMEMBER_RECORD;
	user.attr_offset = ib_get_attr_offset(sizeof(*mcast_record));

	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = 1;
	req.flags = OSM_SA_FLAGS_SYNC;
	context.p_osmt = p_osmt;
	req.query_context = &context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;

	/* UnTrusted (SMKey of 0)  - get the multicast groups */
	status = osmv_query_sa(p_osmt->h_bind, &req);

	if (status != IB_SUCCESS || context.result.status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02B5: "
			"Failed getting the multicast groups records - %s/%s\n",
			ib_get_err_str(status),
			ib_get_err_str(context.result.status));
		return;
	}

	osm_log(&p_osmt->log, OSM_LOG_INFO,
		"\n                    |------------------------------------------|"
		"\n                    |        Remaining Multicast Groups        |"
		"\n                    |------------------------------------------|\n");

	for (i = 0; i < context.result.result_cnt; i++) {
		mcast_record =
		    osmv_get_query_mc_rec(context.result.p_result_madw, i);
		osm_dump_mc_record(&p_osmt->log, mcast_record, OSM_LOG_INFO);
	}

	/* Trusted - now get the multicast group members */
	req.sm_key = OSM_DEFAULT_SM_KEY;
	status = osmv_query_sa(p_osmt->h_bind, &req);

	if (status != IB_SUCCESS || context.result.status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02B6: "
			"Failed getting the multicast group members records - %s/%s\n",
			ib_get_err_str(status),
			ib_get_err_str(context.result.status));
		return;
	}

	osm_log(&p_osmt->log, OSM_LOG_INFO,
		"\n                    |--------------------------------------------------|"
		"\n                    |        Remaining Multicast Group Members        |"
		"\n                    |--------------------------------------------------|\n");

	for (i = 0; i < context.result.result_cnt; i++) {
		mcast_record =
		    osmv_get_query_mc_rec(context.result.p_result_madw, i);
		osm_dump_mc_record(&p_osmt->log, mcast_record, OSM_LOG_INFO);
	}

}

/**********************************************************************
 **********************************************************************/

static cl_status_t
__match_mgids(IN const void *const p_object, IN void *context)
{
	ib_gid_t *p_mgid_context = (ib_gid_t *) context;
	ib_gid_t *p_mgid_list_item = (ib_gid_t *) p_object;
	int32_t count;

	count = memcmp(p_mgid_context, p_mgid_list_item, sizeof(ib_gid_t));
	if (count == 0)
		return CL_SUCCESS;
	else
		return CL_NOT_FOUND;
}

/**********************************************************************
 **********************************************************************/

ib_api_status_t osmt_query_mcast(IN osmtest_t * const p_osmt)
{
	ib_api_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;
	osmtest_req_context_t context;
	ib_member_rec_t *p_rec;
	uint32_t i, num_recs = 0;
	cl_list_t mgids_list;
	cl_list_t *p_mgids_list;
	cl_list_iterator_t p_mgids_res;
	cl_status_t cl_status;
	cl_map_item_t *p_item, *p_next_item;
	osmtest_mgrp_t *p_mgrp;

	OSM_LOG_ENTER(&p_osmt->log);

	/*
	 * Do a blocking query for all Multicast Records in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */

	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));

	context.p_osmt = p_osmt;
	user.attr_id = IB_MAD_ATTR_MCMEMBER_RECORD;
	user.attr_offset = ib_get_attr_offset(sizeof(ib_member_rec_t));

	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = &context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;

	status = osmv_query_sa(p_osmt->h_bind, &req);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0203: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = context.result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0264: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		if (status == IB_REMOTE_ERROR) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s.\n",
				ib_get_mad_status_str(osm_madw_get_mad_ptr
						      (context.result.
						       p_result_madw)));
		}
		goto Exit;
	}

	/* ok we have got something */
	/* First Delete the old MGID Table */
	p_next_item = cl_qmap_head(&p_osmt->exp_subn.mgrp_mlid_tbl);
	while (p_next_item != cl_qmap_end(&p_osmt->exp_subn.mgrp_mlid_tbl)) {
		p_item = p_next_item;
		p_next_item = cl_qmap_next(p_item);
		cl_qmap_remove_item(&p_osmt->exp_subn.mgrp_mlid_tbl, p_item);
		free(p_item);

	}

	cl_list_construct(&mgids_list);
	cl_list_init(&mgids_list, num_recs);
	p_mgids_list = &mgids_list;
	num_recs = context.result.result_cnt;
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Received %u records\n",
		num_recs);

	for (i = 0; i < num_recs; i++) {
		p_rec = osmv_get_query_result(context.result.p_result_madw, i);
		p_mgids_res =
		    cl_list_find_from_head(p_mgids_list, __match_mgids,
					   &(p_rec->mgid));
		/* If returns iterator other than end of list, same mgid exists already */
		if (p_mgids_res != cl_list_end(p_mgids_list)) {
			char gid_str[INET6_ADDRSTRLEN];
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0265: "
				"MCG MGIDs are the same - invalid MGID : %s\n",
				inet_ntop(AF_INET6, p_rec->mgid.raw, gid_str,
					  sizeof gid_str));
			status = IB_ERROR;
			goto Exit;

		}
		osm_dump_mc_record(&p_osmt->log, p_rec, OSM_LOG_VERBOSE);
		cl_status = cl_list_insert_head(p_mgids_list, &(p_rec->mgid));
		if (cl_status) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0205: "
				"Could not add MGID to cl_list\n");
			status = IB_ERROR;
			goto Exit;
		}
		p_mgrp = (osmtest_mgrp_t *) malloc(sizeof(*p_mgrp));
		if (!p_mgrp) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0204: "
				"Could not allocate new MCG\n");
			status = IB_ERROR;
			goto Exit;
		}
		memcpy(&p_mgrp->mcmember_rec, p_rec,
		       sizeof(p_mgrp->mcmember_rec));
		cl_qmap_insert(&p_osmt->exp_subn.mgrp_mlid_tbl,
			       cl_ntoh16(p_rec->mlid), &p_mgrp->map_item);
	}

Exit:
	if (context.result.p_result_madw != NULL) {
		osm_mad_pool_put(&p_osmt->mad_pool,
				 context.result.p_result_madw);
		context.result.p_result_madw = NULL;
	}

	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/

/* given a multicast request send and wait for response. */
ib_api_status_t
osmt_send_mcast_request(IN osmtest_t * const p_osmt,
			IN uint8_t is_set,
			IN ib_member_rec_t * p_mc_req,
			IN uint64_t comp_mask, OUT ib_sa_mad_t * p_res)
{
	osmtest_req_context_t context;
	ib_api_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;

	OSM_LOG_ENTER(&p_osmt->log);

	/*
	 * Do a blocking query for this record in the subnet.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));
	memset(&context, 0, sizeof(context));
	memset(p_res, 0, sizeof(ib_sa_mad_t));

	context.p_osmt = p_osmt;

	user.p_attr = p_mc_req;
	user.comp_mask = comp_mask;

	if (is_set == 1) {
		req.query_type = OSMV_QUERY_UD_MULTICAST_SET;
	} else if (is_set == 0) {
		req.query_type = OSMV_QUERY_UD_MULTICAST_DELETE;
	} else if (is_set == 0xee) {
		OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
			"Set USER DEFINED QUERY\n");
		req.query_type = OSMV_QUERY_USER_DEFINED;
		user.method = IB_MAD_METHOD_GET;
		user.attr_id = IB_MAD_ATTR_MCMEMBER_RECORD;
		user.attr_offset = ib_get_attr_offset(sizeof(ib_member_rec_t));
	} else if (is_set == 0xff) {
		OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
			"Set USER DEFINED QUERY\n");
		req.query_type = OSMV_QUERY_USER_DEFINED;
		user.method = IB_MAD_METHOD_SET;
		user.attr_id = IB_MAD_ATTR_MCMEMBER_RECORD;
		user.attr_offset = ib_get_attr_offset(sizeof(ib_member_rec_t));
	}

	/* TODO : Check the validity of all user fields in order to use
	   OSMV_QUERY_USER_DEFINED
	   p_user_query = ( osmv_user_query_t * ) p_query_req->p_query_input;
	   if (p_user_query->method) sa_mad_data.method = p_user_query->method;
	   sa_mad_data.attr_offset = p_user_query->attr_offset;
	   sa_mad_data.attr_id = p_user_query->attr_id;
	   sa_mad_data.comp_mask = p_user_query->comp_mask;
	   sa_mad_data.p_attr = p_user_query->p_attr;
	 */

	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = &context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;

	status = osmv_query_sa(p_osmt->h_bind, &req);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0206: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	/* ok it worked */
	memcpy(p_res,
	       osm_madw_get_mad_ptr(context.result.p_result_madw),
	       sizeof(ib_sa_mad_t));

	status = context.result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0224: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		if (status == IB_REMOTE_ERROR) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(osm_madw_get_mad_ptr
						      (context.result.
						       p_result_madw)));
		}
	}

Exit:
	/*
	 * Return the IB query MAD to the pool as necessary.
	 */
	if (context.result.p_result_madw != NULL) {
		osm_mad_pool_put(&p_osmt->mad_pool,
				 context.result.p_result_madw);
		context.result.p_result_madw = NULL;
	}

	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/

void
osmt_init_mc_query_rec(IN osmtest_t * const p_osmt,
		       IN OUT ib_member_rec_t * p_mc_req)
{
	/* use default values so we can change only what we want later */
	memset(p_mc_req, 0, sizeof(ib_member_rec_t));

	/* we leave the MGID to the user */
	memcpy(&p_mc_req->port_gid.unicast.interface_id,
	       &p_osmt->local_port.port_guid,
	       sizeof(p_osmt->local_port.port_guid)
	    );

	/*  use our own subnet prefix: */
	p_mc_req->port_gid.unicast.prefix = CL_HTON64(0xFE80000000000000ULL);

	/*  ib_net32_t  qkey; */
	/*  ib_net16_t  mlid; - we keep it zero for upper level to decide. */
	/*  uint8_t     mtu; - keep it zero means - anything you have please. */
	/*  uint8_t     tclass; can leave as zero for now (between subnets) */
	/*  ib_net16_t  pkey; leave as zero */
	p_mc_req->rate = IB_LINK_WIDTH_ACTIVE_4X;
	/*  uint8_t     pkt_life; zero means greater than zero ... */
	/*  ib_net32_t  sl_flow_hop; keep it all zeros */
	/*  we want to use a link local scope: 0x02 */
	p_mc_req->scope_state = ib_member_set_scope_state(0x02, 0);
}

/***********************************************************************
 * UD Multicast testing flow:
 * o15.0.1.3:
 * - Request new MCG with not enough components in comp_mask :
 *   ERR_INSUFFICIENT_COMPONENTS
 * o15.0.1.8:
 * - Request a join with irrelevant RATE and get a ERR_INVALID_REQ
 * o15.0.1.4:
 * - Create an MGID by asking for a join with MGID = 0
 *   providing P_Key, Q_Key, SL, FlowLabel, Tclass.
 * o15.0.1.5:
 * - Check the returned MGID is valid. (p 804)
 * o15.0.1.6:
 * - Create a new MCG with valid requested MGID.
 * - Try to create a new MCG with invalid MGID : get back ERR_REQ_INVALID
 * - Try again with MGID prefix = 0xA01B (maybe 0x1BA0 little or big ?)
 * - Try to create again the already created group: ERR_REQ_INVALID
 * o15.0.1.7 - implicitlly checked during the prev steps.
 * o15.0.1.9
 * - Create MCG with Invalid JoinState.FullMember != 1 : get ERR_REQ_INVALID
 * o15.0.1.10 - can't check on a single client .
 * o15.0.1.11:
 * - Try to join into a MGID that exists with JoinState=SendOnlyMember -
 *   see that it updates JoinState. What is the routing change?
 * - We can not check simple join since we have only one tester (for now)
 * o15.0.1.12:
 * - The last join should have a special treatment in the SA (sender only)
 *   but what is it ?
 * o15.0.1.13:
 * - Try joining with wrong rate - ERR_REQ_INVALID
 * o15.0.1.14:
 * - Try partial delete - actually updating the join state. check it.
 * - Register by InformInfo flow to receive trap 67 on MCG delete.
 * - Try full delete (JoinState and should be 0)
 * - Wait for trap 67.
 * - Try joining (not full mem) again to see the group was deleted.
 *   (should fail - o15.0.1.13)
 * o15.0.1.15:
 * - Try deletion of the IPoIB MCG and get: ERR_REQ_INVALID
 * o15.0.1.16:
 * - Try GetTable with PortGUID wildcarded and get back some groups.
 ***********************************************************************/

/* The following macro can be used only within the osmt_run_mcast_flow() function */
#define IS_IPOIB_MGID(p_mgid) \
           ( !memcmp(&osm_ipoib_good_mgid,    (p_mgid), sizeof(osm_ipoib_good_mgid)) || \
             !memcmp(&osm_ts_ipoib_good_mgid, (p_mgid), sizeof(osm_ts_ipoib_good_mgid)) )

ib_api_status_t osmt_run_mcast_flow(IN osmtest_t * const p_osmt)
{
	char gid_str[INET6_ADDRSTRLEN];
	char gid_str2[INET6_ADDRSTRLEN];
	ib_api_status_t status;
	ib_member_rec_t mc_req_rec;
	ib_member_rec_t *p_mc_res;
	ib_sa_mad_t res_sa_mad;
	uint64_t comp_mask = 0;
	ib_net64_t remote_port_guid = 0x0;
	cl_qmap_t *p_mgrp_mlid_tbl;
	osmtest_mgrp_t *p_mgrp;
	ib_gid_t special_mgid, tmp_mgid, proxy_mgid;
	ib_net16_t invalid_mlid = 0x0;
	ib_net16_t max_mlid = cl_hton16(0xFFFE), tmp_mlid;
	boolean_t ReachedMlidLimit = FALSE;
	int start_cnt = 0, cnt, middle_cnt = 0, end_cnt = 0;
	int start_ipoib_cnt = 0, end_ipoib_cnt = 0;
	int mcg_outside_test_cnt = 0, fail_to_delete_mcg = 0;
	osmtest_req_context_t context;
	ib_node_record_t *p_rec;
	uint32_t num_recs = 0, i;
	uint8_t mtu_phys = 0, rate_phys = 0;
	cl_map_t test_created_mlids;	/* List of all mlids created in this test */
	ib_member_rec_t *p_recvd_rec;
	boolean_t got_error = FALSE;

	static ib_gid_t good_mgid = {
		{
		 0xFF, 0x12, 0xA0, 0x1C,
		 0xFE, 0x80, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00,
		 0x12, 0x34, 0x56, 0x78}
	};
	static ib_gid_t osm_ipoib_mgid = {
		{
		 0xff,		/* multicast field */
		 0x12,		/* scope */
		 0x40, 0x1b,	/* IPv4 signature */
		 0xff, 0xff,	/* 16 bits of P_Key (to be filled in) */
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 48 bits of zeros */
		 0xff, 0xff, 0xff, 0xee,	/* 32 bit IPv4 broadcast address */
		 },
	};
	static ib_gid_t osm_ts_ipoib_good_mgid = {
		{
		 0xff,		/* multicast field */
		 0x12,		/* non-permanent bit,scope */
		 0x40, 0x1b,	/* IPv4 signature */
		 0xff, 0xff,	/* 16 bits of P_Key (to be filled in) */
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 48 bits of zeros */
		 0x00, 0x00, 0x00, 0x01,	/* 32 bit IPv4 broadcast address */
		 },
	};
	static ib_gid_t osm_ipoib_good_mgid = {
		{
		 0xff,		/* multicast field */
		 0x12,		/* non-permanent bit,scope */
		 0x40, 0x1b,	/* IPv4 signature */
		 0xff, 0xff,	/* 16 bits of P_Key (to be filled in) */
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 48 bits of zeros */
		 0xff, 0xff, 0xff, 0xff,	/* 32 bit IPv4 broadcast address */
		 },
	};
	static ib_gid_t osm_link_local_mgid = {
		{
		 0xFF, 0x02, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x01},
	};

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO, "GetTable of all current MCGs...\n");
	status = osmt_query_mcast(p_osmt);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 2FF "
			"GetTable of all records has failed!\n");
		goto Exit;
	}

	/* Initialize the test_created_mgrps map */
	cl_map_construct(&test_created_mlids);
	cl_map_init(&test_created_mlids, 1000);

	p_mgrp_mlid_tbl = &p_osmt->exp_subn.mgrp_mlid_tbl;
	osmt_init_mc_query_rec(p_osmt, &mc_req_rec);

	p_mc_res = ib_sa_mad_get_payload_ptr(&res_sa_mad);

	/* Only when we are on single mode check flow - do the count comparison, otherwise skip */
	if (p_osmt->opt.mmode == 1 || p_osmt->opt.mmode == 3) {
		start_cnt = cl_qmap_count(p_mgrp_mlid_tbl);
		OSM_LOG(&p_osmt->log, OSM_LOG_INFO, "(start): "
			"Number of MC Records found in SA DB is %d\n",
			start_cnt);
	}

	/* This flow is being added due to bug discovered using SilverStorm stack -
	   The bug was initializing MCast with MTU & RATE min values that do
	   not match the subnet capability, even though that OpenSM
	   reponds with the correct value it does not store it in the MCG.
	   We want the check a join request to already existing group (ipoib)
	   without using MTU or RATE then getting response from OpenSM with
	   the correct values then join again with them and get IB_SUCCESS
	   all the way
	 */

	/* First validate IPoIB exist in the SA DB */
	p_mgrp = (osmtest_mgrp_t *) cl_qmap_head(p_mgrp_mlid_tbl);
	/* scan all available multicast groups in the DB and fill in the table */
	while (p_mgrp != (osmtest_mgrp_t *) cl_qmap_end(p_mgrp_mlid_tbl)) {
		/* search for ipoib mgid */
		if (IS_IPOIB_MGID(&p_mgrp->mcmember_rec.mgid)) {
			start_ipoib_cnt++;
		} else {
			OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
				"Non-IPoIB MC Groups exist: mgid=%s\n",
				inet_ntop(AF_INET6,
					  p_mgrp->mcmember_rec.mgid.raw,
					  gid_str, sizeof gid_str));
			mcg_outside_test_cnt++;
		}

		p_mgrp = (osmtest_mgrp_t *) cl_qmap_next(&p_mgrp->map_item);
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Found %d non-IPoIB MC Groups\n", mcg_outside_test_cnt);

	if (start_ipoib_cnt) {
		/* o15-0.2.4 - Check a join request to already created MCG */
		OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
			"Found IPoIB MC Group, so we run SilverStorm Bug Flow...\n");
		/* Try to join first like IPoIB of SilverStorm */
		memcpy(&mc_req_rec.mgid, &osm_ipoib_good_mgid,
		       sizeof(ib_gid_t));
		/* Request Join */
		ib_member_set_join_state(&mc_req_rec,
					 IB_MC_REC_STATE_FULL_MEMBER);
		comp_mask =
		    IB_MCR_COMPMASK_MGID | IB_MCR_COMPMASK_PORT_GID |
		    IB_MCR_COMPMASK_JOIN_STATE;

		status = osmt_send_mcast_request(p_osmt, 0xff,	/* User Defined query Set */
						 &mc_req_rec,
						 comp_mask, &res_sa_mad);

		OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
			"Joining an existing IPoIB multicast group\n");
		OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
			"Sent Join request with :\n\t\tport_gid=%s, mgid=%s\n"
			"\t\tjoin state= 0x%x, response is : %s\n",
			inet_ntop(AF_INET6, mc_req_rec.port_gid.raw,
				  gid_str, sizeof gid_str),
			inet_ntop(AF_INET6, mc_req_rec.mgid.raw,
				  gid_str2, sizeof gid_str2),
			(mc_req_rec.scope_state & 0x0F),
			ib_get_err_str(status));
		if (status != IB_SUCCESS) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02B3: "
				"Failed joining existing IPoIB MCGroup - got %s\n",
				ib_get_err_str(status));
			goto Exit;
		}
		/* Check MTU & Rate Value and resend with SA suggested values */
		p_mc_res = ib_sa_mad_get_payload_ptr(&res_sa_mad);

		/* Prepare the mc_req_rec for the rest of the flow */
		osmt_init_mc_query_rec(p_osmt, &mc_req_rec);
		/*
		   We simulate the same situation as in SilverStorm - a response with the
		   exact RATE & MTU as the SA responded with. Actually the query
		   has included some more fields but we know that problem was
		   genereated by the RATE
		 */
		OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
			"Received attributes of MCG : \n\t\tMTU=0x%02X, RATE=0x%02X\n",
			p_mc_res->mtu, p_mc_res->rate);

		mc_req_rec.mtu = p_mc_res->mtu;
		mc_req_rec.rate = p_mc_res->rate;
		/* Set feasible mtu & rate that will allow check the
		   exact statement of OpenSM */
		mtu_phys = p_mc_res->mtu;
		rate_phys = p_mc_res->rate;

		memcpy(&mc_req_rec.mgid, &osm_ipoib_good_mgid,
		       sizeof(ib_gid_t));
		/* Request Join */
		ib_member_set_join_state(&mc_req_rec,
					 IB_MC_REC_STATE_FULL_MEMBER);
		comp_mask =
		    IB_MCR_COMPMASK_MGID | IB_MCR_COMPMASK_PORT_GID |
		    IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_MTU_SEL |
		    IB_MCR_COMPMASK_MTU | IB_MCR_COMPMASK_RATE_SEL |
		    IB_MCR_COMPMASK_RATE;

		OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
			"Sending attributes of MCG : \n\t\tMTU=0x%02X, RATE=0x%02X\n",
			mc_req_rec.mtu, mc_req_rec.rate);
		status = osmt_send_mcast_request(p_osmt, 0xff,	/* User Defined query */
						 &mc_req_rec,
						 comp_mask, &res_sa_mad);
		OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
			"Sent Join request using response values, response is : %s\n",
			ib_get_err_str(status));
		if (status != IB_SUCCESS) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02EF: "
				"Query as Full Member of already existing "
				"ipoib group gid %s has failed\n",
				inet_ntop(AF_INET6, mc_req_rec.mgid.raw,
					  gid_str, sizeof gid_str));
			goto Exit;
		}
		/* We do not want to leave the MCG since its IPoIB */
	}

  /**************************************************************************/
	/* Check Get with invalid mlid */

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Get with invalid mlid...\n");
	/* Request Get */
	ib_member_set_join_state(&mc_req_rec, IB_MC_REC_STATE_FULL_MEMBER);
	mc_req_rec.mlid = invalid_mlid;
	comp_mask = IB_MCR_COMPMASK_MLID;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 0xee,	/* User Defined query Get */
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status == IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 2E0 "
			"SubnAdmGet with invalid mlid 0x%x succeeded\n",
			cl_ntoh16(mc_req_rec.mlid));
		status = IB_ERROR;
		goto Exit;
	}

	/* Prepare the mc_req_rec for the rest of the flow */
	osmt_init_mc_query_rec(p_osmt, &mc_req_rec);
  /**************************************************************************/
	/* Check Get with invalid port guid */

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Get with invalid port guid (0x0) but valid interface ID : 0x%"
		PRIx64 "...\n",
		cl_ntoh64(mc_req_rec.port_gid.unicast.interface_id));

	/* Request Get */
	ib_member_set_join_state(&mc_req_rec, IB_MC_REC_STATE_FULL_MEMBER);
	memset(&mc_req_rec.port_gid.unicast.interface_id, 0,
	       sizeof(ib_net64_t));
	comp_mask = IB_MCR_COMPMASK_GID;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 0xee,	/* User Defined query Get */
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status == IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 2E4 "
			"SubnAdmGet with invalid port guid succeeded\n");
		status = IB_ERROR;
		goto Exit;
	}

	/* Prepare the mc_req_rec for the rest of the flow */
	osmt_init_mc_query_rec(p_osmt, &mc_req_rec);
  /**************************************************************************/

	/* o15.0.1.3:  */
	/* - Request Join with insufficient comp_mask */

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Join with insufficient comp mask qkey & pkey (o15.0.1.3)...\n");

	/* no MGID */
	memset(&mc_req_rec.mgid, 0, sizeof(ib_gid_t));
	/* Request Join */
	ib_member_set_join_state(&mc_req_rec, IB_MC_REC_STATE_FULL_MEMBER);

	comp_mask = IB_MCR_COMPMASK_MGID | IB_MCR_COMPMASK_PORT_GID |
	    /* IB_MCR_COMPMASK_QKEY |  */
	    /* IB_MCR_COMPMASK_PKEY | intentionaly missed to raise the error */
	    IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_RATE_SEL | IB_MCR_COMPMASK_RATE;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status != IB_REMOTE_ERROR ||
	    ((ib_net16_t) (res_sa_mad.status & IB_SMP_STATUS_MASK)) !=
	    IB_SA_MAD_STATUS_INSUF_COMPS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02EE: "
			"Expectedd REMOTE ERROR IB_SA_MAD_STATUS_INSUF_COMPS got:%s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Join with insufficient comp mask - sl (15.0.1.3)...\n");

	/* no MGID */
	memset(&mc_req_rec.mgid, 0, sizeof(ib_gid_t));
	/* Request Join */
	ib_member_set_join_state(&mc_req_rec, IB_MC_REC_STATE_FULL_MEMBER);

	comp_mask =
	    IB_MCR_COMPMASK_MGID |
	    IB_MCR_COMPMASK_PORT_GID |
	    IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY |
	    /* IB_MCR_COMPMASK_SL |  */
	    IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_RATE_SEL | IB_MCR_COMPMASK_RATE;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status != IB_REMOTE_ERROR ||
	    ((ib_net16_t) (res_sa_mad.status & IB_SMP_STATUS_MASK)) !=
	    IB_SA_MAD_STATUS_INSUF_COMPS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02ED: "
			"Expectedd REMOTE ERROR IB_SA_MAD_STATUS_INSUF_COMPS got:%s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	osmt_init_mc_query_rec(p_osmt, &mc_req_rec);
	/* no MGID */
	memset(&mc_req_rec.mgid, 0, sizeof(ib_gid_t));

	mc_req_rec.mgid.raw[15] = 0x01;

	p_mc_res = ib_sa_mad_get_payload_ptr(&res_sa_mad);

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Join with insufficient comp mask - flow label (o15.0.1.3)...\n");

	/* Request Join */
	ib_member_set_join_state(&mc_req_rec, IB_MC_REC_STATE_FULL_MEMBER);

	comp_mask =
	    IB_MCR_COMPMASK_MGID |
	    IB_MCR_COMPMASK_PORT_GID |
	    IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL |
	    /* IB_MCR_COMPMASK_FLOW | intentionaly missed to raise the error */
	    IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_RATE_SEL | IB_MCR_COMPMASK_RATE;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status != IB_REMOTE_ERROR ||
	    ((ib_net16_t) (res_sa_mad.status & IB_SMP_STATUS_MASK)) !=
	    IB_SA_MAD_STATUS_INSUF_COMPS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02EC: "
			"Expected REMOTE ERROR IB_SA_MAD_STATUS_INSUF_COMPS got:%s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	osmt_init_mc_query_rec(p_osmt, &mc_req_rec);

	p_mc_res = ib_sa_mad_get_payload_ptr(&res_sa_mad);

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Join with insufficient comp mask - tclass (o15.0.1.3)...\n");

	/* Request Join */
	ib_member_set_join_state(&mc_req_rec, IB_MC_REC_STATE_FULL_MEMBER);

	comp_mask =
	    IB_MCR_COMPMASK_MGID |
	    IB_MCR_COMPMASK_PORT_GID |
	    IB_MCR_COMPMASK_QKEY |
	    IB_MCR_COMPMASK_PKEY |
	    IB_MCR_COMPMASK_SL |
	    IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE |
	    /* IB_MCR_COMPMASK_TCLASS |  Intentionally missed to raise an error */
	    IB_MCR_COMPMASK_RATE_SEL | IB_MCR_COMPMASK_RATE;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status != IB_REMOTE_ERROR ||
	    ((ib_net16_t) (res_sa_mad.status & IB_SMP_STATUS_MASK)) !=
	    IB_SA_MAD_STATUS_INSUF_COMPS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02EA: "
			"Expected REMOTE ERROR IB_SA_MAD_STATUS_INSUF_COMPS got:%s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	osmt_init_mc_query_rec(p_osmt, &mc_req_rec);

	p_mc_res = ib_sa_mad_get_payload_ptr(&res_sa_mad);

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Join with insufficient comp mask - tclass qkey (o15.0.1.3)...\n");

	/* no MGID */
	/* memset(&mc_req_rec.mgid, 0, sizeof(ib_gid_t)); */
	/* Request Join */
	ib_member_set_join_state(&mc_req_rec, IB_MC_REC_STATE_FULL_MEMBER);

	comp_mask = IB_MCR_COMPMASK_MGID | IB_MCR_COMPMASK_PORT_GID |
	    /* IB_MCR_COMPMASK_QKEY | intentionaly missed to raise the error */
	    IB_MCR_COMPMASK_PKEY |
	    IB_MCR_COMPMASK_SL |
	    IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE |
	    /* IB_MCR_COMPMASK_TCLASS |  intentionaly missed to raise the error */
	    IB_MCR_COMPMASK_RATE_SEL | IB_MCR_COMPMASK_RATE;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status != IB_REMOTE_ERROR ||
	    ((ib_net16_t) (res_sa_mad.status & IB_SMP_STATUS_MASK)) !=
	    IB_SA_MAD_STATUS_INSUF_COMPS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02E9: "
			"Expected REMOTE ERROR IB_SA_MAD_STATUS_INSUF_COMPS got:%s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* o15.0.1.8: */
	/* - Request join with irrelevant RATE : get a ERR_INSUFFICIENT_COMPONENTS */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Join with unrealistic rate (o15.0.1.8)...\n");

	/* impossible requested rate */
	mc_req_rec.rate =
	    IB_LINK_WIDTH_ACTIVE_12X | IB_PATH_SELECTOR_GREATER_THAN << 6;

	comp_mask = IB_MCR_COMPMASK_GID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_RATE_SEL | IB_MCR_COMPMASK_RATE;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status != IB_REMOTE_ERROR ||
	    res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0207: "
			"Expected REMOTE ERROR IB_SA_MAD_STATUS_REQ_INVALID got:%s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* Check Valid value which is unreasonable now */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Join with unrealistic rate 120GB (o15.0.1.8)...\n");

	/* impossible requested rate */
	mc_req_rec.rate =
	    IB_PATH_RECORD_RATE_120_GBS | IB_PATH_SELECTOR_GREATER_THAN << 6;

	comp_mask = IB_MCR_COMPMASK_GID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_RATE_SEL | IB_MCR_COMPMASK_RATE;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status != IB_REMOTE_ERROR ||
	    res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0208: "
			"Expected REMOTE ERROR IB_SA_MAD_STATUS_REQ_INVALID got:%s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* Check Valid value which is unreasonable now */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Join with less than min rate 2.5GB (o15.0.1.8)...\n");

	/* impossible requested rate */
	mc_req_rec.rate =
	    IB_PATH_RECORD_RATE_2_5_GBS | IB_PATH_SELECTOR_LESS_THAN << 6;

	comp_mask = IB_MCR_COMPMASK_GID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_RATE_SEL | IB_MCR_COMPMASK_RATE;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status != IB_REMOTE_ERROR ||
	    res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02AB: "
			"Expected REMOTE ERROR IB_SA_MAD_STATUS_REQ_INVALID got:%s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* Checking above max value of MTU which is impossible */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Join with unrealistic mtu : \n\t\tmore than 4096 -"
		" max (o15.0.1.8)...\n");

	/* impossible requested mtu */
	mc_req_rec.mtu = IB_MTU_LEN_4096 | IB_PATH_SELECTOR_GREATER_THAN << 6;

	comp_mask = IB_MCR_COMPMASK_GID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_MTU_SEL | IB_MCR_COMPMASK_MTU;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status != IB_REMOTE_ERROR ||
	    res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02AC: "
			"Expected REMOTE ERROR IB_SA_MAD_STATUS_REQ_INVALID got:%s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad))
		    );
		status = IB_ERROR;
		goto Exit;
	}

	/* Checking below min value of MTU which is impossible */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Join with unrealistic mtu : \n\t\tless than 256 -"
		" min (o15.0.1.8)...\n");

	/* impossible requested mtu */
	mc_req_rec.mtu = IB_MTU_LEN_256 | IB_PATH_SELECTOR_LESS_THAN << 6;

	comp_mask = IB_MCR_COMPMASK_GID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_MTU_SEL | IB_MCR_COMPMASK_MTU;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status != IB_REMOTE_ERROR ||
	    res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02AD: "
			"Expected REMOTE ERROR IB_SA_MAD_STATUS_REQ_INVALID got:%s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Join with unrealistic mtu (o15.0.1.8)...\n");

	/* impossible requested mtu */
	mc_req_rec.mtu = 0x6 | IB_PATH_SELECTOR_GREATER_THAN << 6;

	comp_mask = IB_MCR_COMPMASK_GID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_MTU_SEL | IB_MCR_COMPMASK_MTU;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status != IB_REMOTE_ERROR ||
	    res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02AE: "
			"Expected REMOTE ERROR IB_SA_MAD_STATUS_REQ_INVALID got:%s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}
#if 0
	/* Currently PacketLifeTime isn't checked in opensm */
	/* Check PacketLifeTime as 0 */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Create with unrealistic packet life value less than 0 (o15.0.1.8)...\n");

	/* impossible requested packet life */
	mc_req_rec.pkt_life = 0 | IB_PATH_SELECTOR_LESS_THAN << 6;

	comp_mask = IB_MCR_COMPMASK_GID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_LIFE | IB_MCR_COMPMASK_LIFE_SEL;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status != IB_REMOTE_ERROR ||
	    res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02AF: "
			"Expected REMOTE ERROR IB_SA_MAD_STATUS_REQ_INVALID got:%s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}
#endif

	/* o15.0.1.4:  */
	/* - Create an MGID by asking for a join with MGID = 0 */
	/*   providing P_Key, Q_Key, SL, FlowLabel, Tclass. */

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Create given MGID=0 skip service level (o15.0.1.4)...\n");

	osmt_init_mc_query_rec(p_osmt, &mc_req_rec);

	p_mc_res = ib_sa_mad_get_payload_ptr(&res_sa_mad);

	/* no MGID */
	memset(&mc_req_rec.mgid, 0, sizeof(ib_gid_t));
	/* Request Join */
	ib_member_set_join_state(&mc_req_rec, IB_MC_REC_STATE_FULL_MEMBER);

	comp_mask =
	    IB_MCR_COMPMASK_MGID |
	    IB_MCR_COMPMASK_PORT_GID |
	    IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY |
	    /* IB_MCR_COMPMASK_SL | Intentionally missed */
	    IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_RATE_SEL | IB_MCR_COMPMASK_RATE;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status != IB_REMOTE_ERROR ||
	    ((ib_net16_t) (res_sa_mad.status & IB_SMP_STATUS_MASK)) !=
	    IB_SA_MAD_STATUS_INSUF_COMPS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02A8: "
			"Expected REMOTE ERROR IB_SA_MAD_STATUS_INSUF_COMPS got:%s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* Check that no same MCG in the SMDB */
	status = osmt_query_mcast(p_osmt);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02AA: "
			"Could not get all MC Records in subnet, got:%s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	/* Only when we are on single mode check flow - do the count comparison, otherwise skip */
	if (p_osmt->opt.mmode == 1 || p_osmt->opt.mmode == 3) {
		middle_cnt = cl_qmap_count(&p_osmt->exp_subn.mgrp_mlid_tbl);
		OSM_LOG(&p_osmt->log, OSM_LOG_INFO, "(post false create): "
			"Number of MC Records found in SA DB is %d\n",
			middle_cnt);
		if (middle_cnt != start_cnt) {
			OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
				"Got different number of records stored in SA DB (before any creation)\n"
				"Instead of %d got %d\n", start_cnt,
				middle_cnt);
		}
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Create given MGID=0 skip Qkey and Pkey (o15.0.1.4)...\n");

	osmt_init_mc_query_rec(p_osmt, &mc_req_rec);

	p_mc_res = ib_sa_mad_get_payload_ptr(&res_sa_mad);

	/* no MGID */
	memset(&mc_req_rec.mgid, 0, sizeof(ib_gid_t));
	/* Request Join */
	ib_member_set_join_state(&mc_req_rec, IB_MC_REC_STATE_FULL_MEMBER);

	comp_mask = IB_MCR_COMPMASK_MGID | IB_MCR_COMPMASK_PORT_GID |
	    /* IB_MCR_COMPMASK_QKEY | */
	    /* IB_MCR_COMPMASK_PKEY | Intentionally missed */
	    IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_RATE_SEL | IB_MCR_COMPMASK_RATE;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status != IB_REMOTE_ERROR ||
	    ((ib_net16_t) (res_sa_mad.status & IB_SMP_STATUS_MASK)) !=
	    IB_SA_MAD_STATUS_INSUF_COMPS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02A7: "
			"Expected REMOTE ERROR IB_SA_MAD_STATUS_INSUF_COMPS got:%s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* Bad Query o15.0.1.4 */

	status = osmt_query_mcast(p_osmt);

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Create given MGID=0 skip TClass (o15.0.1.4)...\n");

	osmt_init_mc_query_rec(p_osmt, &mc_req_rec);

	p_mc_res = ib_sa_mad_get_payload_ptr(&res_sa_mad);

	/* no MGID */
	memset(&mc_req_rec.mgid, 0, sizeof(ib_gid_t));
	/* Request Join */
	ib_member_set_join_state(&mc_req_rec, IB_MC_REC_STATE_FULL_MEMBER);

	comp_mask =
	    IB_MCR_COMPMASK_MGID |
	    IB_MCR_COMPMASK_PORT_GID |
	    IB_MCR_COMPMASK_QKEY |
	    IB_MCR_COMPMASK_PKEY |
	    IB_MCR_COMPMASK_SL |
	    IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE |
	    /* IB_MCR_COMPMASK_TCLASS |  Intentionally missed */
	    /* all above are required */
	    IB_MCR_COMPMASK_RATE_SEL | IB_MCR_COMPMASK_RATE;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status != IB_REMOTE_ERROR ||
	    ((ib_net16_t) (res_sa_mad.status & IB_SMP_STATUS_MASK)) !=
	    IB_SA_MAD_STATUS_INSUF_COMPS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02A6: "
			"Expected REMOTE ERROR IB_SA_MAD_STATUS_INSUF_COMPS got:%s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Create given MGID=0 valid Set several options :\n\t\t"
		"First above min RATE, Second less than max RATE\n\t\t"
		"Third above min MTU, Second less than max MTU\n\t\t"
		"Fifth exact MTU & RATE feasible, Sixth exact RATE feasible\n\t\t"
		"Seventh exact MTU feasible (o15.0.1.4)...\n");

	/* Good Flow - mgid is 0 while giving all required fields for join : P_Key, Q_Key, SL, FlowLabel, Tclass */

	mc_req_rec.rate =
	    IB_LINK_WIDTH_ACTIVE_1X | IB_PATH_SELECTOR_GREATER_THAN << 6;

	comp_mask = IB_MCR_COMPMASK_MGID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_RATE_SEL | IB_MCR_COMPMASK_RATE;

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02A5: "
			"Failed to create MCG for MGID=0 with higher than minimum RATE - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	/* Save the mlid created in test_created_mlids map */
	p_recvd_rec =
	    (ib_member_rec_t *) ib_sa_mad_get_payload_ptr(&res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "created MGID:%s MLID:0x%04X\n",
		inet_ntop(AF_INET6, p_recvd_rec->mgid.raw, gid_str,
			  sizeof gid_str), cl_ntoh16(p_recvd_rec->mlid));
	cl_map_insert(&test_created_mlids, cl_ntoh16(p_recvd_rec->mlid),
		      p_recvd_rec);

	/* Good Flow - mgid is 0 while giving all required fields for join : P_Key, Q_Key, SL, FlowLabel, Tclass */

	mc_req_rec.rate =
	    IB_LINK_WIDTH_ACTIVE_12X | IB_PATH_SELECTOR_LESS_THAN << 6;

	comp_mask = IB_MCR_COMPMASK_MGID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_RATE_SEL | IB_MCR_COMPMASK_RATE;

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0211: "
			"Failed to create MCG for MGID=0 with less than highest RATE - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	/* Save the mlid created in test_created_mlids map */
	p_recvd_rec =
	    (ib_member_rec_t *) ib_sa_mad_get_payload_ptr(&res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Created MGID:%s MLID:0x%04X\n",
		inet_ntop(AF_INET6, p_recvd_rec->mgid.raw, gid_str,
			  sizeof gid_str), cl_ntoh16(p_recvd_rec->mlid));
	cl_map_insert(&test_created_mlids, cl_ntoh16(p_recvd_rec->mlid),
		      p_recvd_rec);

	/* Good Flow - mgid is 0 while giving all required fields for join : P_Key, Q_Key, SL, FlowLabel, Tclass */

	mc_req_rec.mtu = IB_MTU_LEN_4096 | IB_PATH_SELECTOR_LESS_THAN << 6;

	comp_mask = IB_MCR_COMPMASK_MGID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_MTU_SEL | IB_MCR_COMPMASK_MTU;

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0238: "
			"Failed to create MCG for MGID=0 with less than highest MTU - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	/* Save the mlid created in test_created_mlids map */
	p_recvd_rec =
	    (ib_member_rec_t *) ib_sa_mad_get_payload_ptr(&res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Created MGID:%s MLID:0x%04X\n",
		inet_ntop(AF_INET6, p_recvd_rec->mgid.raw, gid_str,
			  sizeof gid_str), cl_ntoh16(p_recvd_rec->mlid));
	cl_map_insert(&test_created_mlids, cl_ntoh16(p_recvd_rec->mlid),
		      p_recvd_rec);

	/* Good Flow - mgid is 0 while giving all required fields for join : P_Key, Q_Key, SL, FlowLabel, Tclass */
	mc_req_rec.mtu = IB_MTU_LEN_256 | IB_PATH_SELECTOR_GREATER_THAN << 6;

	comp_mask = IB_MCR_COMPMASK_MGID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_MTU_SEL | IB_MCR_COMPMASK_MTU;

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0239: "
			"Failed to create MCG for MGID=0 with higher than lowest MTU - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	/* Save the mlid created in test_created_mlids map */
	p_recvd_rec =
	    (ib_member_rec_t *) ib_sa_mad_get_payload_ptr(&res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Created MGID:%s MLID:0x%04X\n",
		inet_ntop(AF_INET6, p_recvd_rec->mgid.raw, gid_str,
			  sizeof gid_str), cl_ntoh16(p_recvd_rec->mlid));
	cl_map_insert(&test_created_mlids, cl_ntoh16(p_recvd_rec->mlid),
		      p_recvd_rec);

	/* Good Flow - mgid is 0 while giving all required fields for join : P_Key, Q_Key, SL, FlowLabel, Tclass */
	/* Using Exact feasible MTU & RATE */

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Using Exact feasible MTU & RATE: "
		"MTU = 0x%02X, RATE = 0x%02X\n", mtu_phys, rate_phys);

	mc_req_rec.mtu = mtu_phys;
	mc_req_rec.rate = rate_phys;

	comp_mask = IB_MCR_COMPMASK_MGID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_MTU_SEL |
	    IB_MCR_COMPMASK_MTU |
	    IB_MCR_COMPMASK_RATE_SEL | IB_MCR_COMPMASK_RATE;

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0240: "
			"Failed to create MCG for MGID=0 with exact MTU & RATE - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	/* Save the mlid created in test_created_mlids map */
	p_recvd_rec =
	    (ib_member_rec_t *) ib_sa_mad_get_payload_ptr(&res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Created MGID:%s MLID:0x%04X\n",
		inet_ntop(AF_INET6, p_recvd_rec->mgid.raw, gid_str,
			  sizeof gid_str), cl_ntoh16(p_recvd_rec->mlid));
	cl_map_insert(&test_created_mlids, cl_ntoh16(p_recvd_rec->mlid),
		      p_recvd_rec);

	/* Good Flow - mgid is 0 while giving all required fields for join : P_Key, Q_Key, SL, FlowLabel, Tclass */
	/* Using Exact feasible RATE */

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Using Exact feasible RATE: 0x%02X\n", rate_phys);

	mc_req_rec.rate = rate_phys;

	comp_mask = IB_MCR_COMPMASK_MGID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_RATE_SEL | IB_MCR_COMPMASK_RATE;

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0241: "
			"Failed to create MCG for MGID=0 with exact RATE - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	/* Save the mlid created in test_created_mlids map */
	p_recvd_rec =
	    (ib_member_rec_t *) ib_sa_mad_get_payload_ptr(&res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Created MGID:%s MLID:0x%04X\n",
		inet_ntop(AF_INET6, p_recvd_rec->mgid.raw, gid_str,
			  sizeof gid_str), cl_ntoh16(p_recvd_rec->mlid));
	cl_map_insert(&test_created_mlids, cl_ntoh16(p_recvd_rec->mlid),
		      p_recvd_rec);

	/* Good Flow - mgid is 0 while giving all required fields for join : P_Key, Q_Key, SL, FlowLabel, Tclass */
	/* Using Exact feasible MTU */

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Using Exact feasible MTU: 0x%02X\n", mtu_phys);

	mc_req_rec.mtu = mtu_phys;

	comp_mask = IB_MCR_COMPMASK_MGID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_MTU_SEL | IB_MCR_COMPMASK_MTU;

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0242: "
			"Failed to create MCG for MGID=0 with exact MTU - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	/* Save the mlid created in test_created_mlids map */
	p_recvd_rec =
	    (ib_member_rec_t *) ib_sa_mad_get_payload_ptr(&res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Created MGID:%s MLID:0x%04X\n",
		inet_ntop(AF_INET6, p_recvd_rec->mgid.raw, gid_str,
			  sizeof gid_str), cl_ntoh16(p_recvd_rec->mlid));
	cl_map_insert(&test_created_mlids, cl_ntoh16(p_recvd_rec->mlid),
		      p_recvd_rec);

	/* o15.0.1.5: */
	/* - Check the returned MGID is valid. (p 804) */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Validating resulting MGID (o15.0.1.5)...\n");
	/* prefix 0xFF1 Scope 0xA01B */
	/* Since we did not directly specified SCOPE in comp mask
	   we should get the comp mask that is link-local scope */
	if ((p_mc_res->mgid.multicast.header[0] != 0xFF) ||
	    (p_mc_res->mgid.multicast.header[1] != 0x12) ||
	    (p_mc_res->mgid.multicast.raw_group_id[0] != 0xA0) ||
	    (p_mc_res->mgid.multicast.raw_group_id[1] != 0x1B)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0209: "
			"Validating MGID failed. MGID:%s\n",
			inet_ntop(AF_INET6, p_mc_res->mgid.raw, gid_str,
				  sizeof gid_str));
		status = IB_ERROR;
		goto Exit;
	}

	/* Good Flow - mgid is 0 while giving all required fields for join : P_Key, Q_Key, SL, FlowLabel, Tclass */
	/* Using feasible GREATER_THAN 0 packet lifitime */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Create given MGID=0 (o15.0.1.4)...\n");

	status = osmt_query_mcast(p_osmt);

	osmt_init_mc_query_rec(p_osmt, &mc_req_rec);

	p_mc_res = ib_sa_mad_get_payload_ptr(&res_sa_mad);

	/* no MGID */
	memset(&mc_req_rec.mgid, 0, sizeof(ib_gid_t));
	/* Request Join */
	ib_member_set_join_state(&mc_req_rec, IB_MC_REC_STATE_FULL_MEMBER);

	mc_req_rec.pkt_life = 0 | IB_PATH_SELECTOR_GREATER_THAN << 6;

	comp_mask = IB_MCR_COMPMASK_MGID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_LIFE | IB_MCR_COMPMASK_LIFE_SEL;

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0210: "
			"Failed to create MCG for MGID=0 - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	/* Save the mlid created in test_created_mlids map */
	p_recvd_rec =
	    (ib_member_rec_t *) ib_sa_mad_get_payload_ptr(&res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Created MGID:%s MLID:0x%04X\n",
		inet_ntop(AF_INET6, p_recvd_rec->mgid.raw, gid_str,
			  sizeof gid_str), cl_ntoh16(p_recvd_rec->mlid));
	cl_map_insert(&test_created_mlids, cl_ntoh16(p_recvd_rec->mlid),
		      p_recvd_rec);

	/* o15.0.1.6: */
	/* - Create a new MCG with valid requested MGID. */
	osmt_init_mc_query_rec(p_osmt, &mc_req_rec);
	mc_req_rec.mgid = good_mgid;

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Create given valid MGID=%s (o15.0.1.6)...\n",
		inet_ntop(AF_INET6, mc_req_rec.mgid.raw, gid_str,
			  sizeof gid_str));

	/* Before creation, need to check that this group doesn't exist */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Verifying that MCGroup with this MGID doesn't exist by trying to Join it (o15.0.1.13)...\n");

	ib_member_set_join_state(&mc_req_rec, IB_MC_REC_STATE_NON_MEMBER);

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,	/* join */
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if ((status != IB_REMOTE_ERROR) ||
	    (res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0301: "
			"Tried joining group that shouldn't have existed - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* Set State to full member to allow group creation */
	ib_member_set_join_state(&mc_req_rec, IB_MC_REC_STATE_FULL_MEMBER);

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Now creating group with given valid MGID=%s (o15.0.1.6)...\n",
		inet_ntop(AF_INET6, mc_req_rec.mgid.raw, gid_str,
			  sizeof gid_str));

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0211: "
			"Failed to create MCG for MGID=%s (o15.0.1.6) - got %s/%s\n",
			inet_ntop(AF_INET6, good_mgid.raw, gid_str,
				  sizeof gid_str), ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	/* Save the mlid created in test_created_mlids map */
	p_recvd_rec =
	    (ib_member_rec_t *) ib_sa_mad_get_payload_ptr(&res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Created MGID:%s MLID:0x%04X\n",
		inet_ntop(AF_INET6, p_recvd_rec->mgid.raw, gid_str,
			  sizeof gid_str), cl_ntoh16(p_recvd_rec->mlid));
	cl_map_insert(&test_created_mlids, cl_ntoh16(p_recvd_rec->mlid),
		      p_recvd_rec);

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Validating resulting MGID (o15.0.1.6)...\n");
	/* prefix 0xFF1 Scope 0xA01B */
	if ((p_mc_res->mgid.multicast.header[0] != 0xFF) || (p_mc_res->mgid.multicast.header[1] != 0x12) ||	/* HACK hardcoded scope = 0x02 */
	    (p_mc_res->mgid.multicast.raw_group_id[0] != 0xA0) ||
	    (p_mc_res->mgid.multicast.raw_group_id[1] != 0x1C)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0212: "
			"Validating MGID failed. MGID:%s\n",
			inet_ntop(AF_INET6, p_mc_res->mgid.raw, gid_str,
				  sizeof gid_str));
		status = IB_ERROR;
		goto Exit;
	}

	/* - Try to create a new MCG with invalid MGID : get back ERR_REQ_INVALID */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking BAD MGID=0xFA..... (o15.0.1.6)...\n");

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");

	mc_req_rec.mgid.raw[0] = 0xFA;
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if ((status != IB_REMOTE_ERROR) ||
	    (res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0213: "
			"Failed to recognize MGID error for MGID=0xFA - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* - Try again with MGID prefix = 0xA01B (maybe 0x1BA0 little or big ?) */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking BAD MGID=0xFF12A01B..... with link-local scope (o15.0.1.6)...\n");

	mc_req_rec.mgid.raw[0] = 0xFF;
	mc_req_rec.mgid.raw[3] = 0x1B;
	comp_mask = comp_mask | IB_MCR_COMPMASK_SCOPE;
	mc_req_rec.scope_state = mc_req_rec.scope_state & 0x2F;	/* local scope */
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if ((status != IB_REMOTE_ERROR) ||
	    (res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0214: "
			"Failed to recognize MGID error for A01B with link-local bit (status %s) (rem status %s)\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* Change the mgid prefix - get back ERR_REQ_INVALID */

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking BAD MGID PREFIX=0xEF... (o15.0.1.6)...\n");

	mc_req_rec.mgid = good_mgid;

	mc_req_rec.mgid.raw[0] = 0xEF;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if ((status != IB_REMOTE_ERROR) ||
	    (res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0215: "
			"Failed to recognize MGID PREFIX error for MGID=0xEF - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* Change the scope to reserved - get back VALID REQ */

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking local scope with full member \n\t\tand valid mgid %s"
		"  ... (o15.0.1.6)...\n",
		inet_ntop(AF_INET6, mc_req_rec.mgid.raw, gid_str,
			  sizeof gid_str));

	mc_req_rec.mgid = good_mgid;

	mc_req_rec.mgid.raw[1] = 0x1F;

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0216: "
			"Failed to create MCG for MGID=%s - got %s/%s\n",
			inet_ntop(AF_INET6, good_mgid.raw, gid_str,
				  sizeof gid_str), ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	/* Save the mlid created in test_created_mlids map */
	p_recvd_rec =
	    (ib_member_rec_t *) ib_sa_mad_get_payload_ptr(&res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Created MGID:%s MLID:0x%04X\n",
		inet_ntop(AF_INET6, p_recvd_rec->mgid.raw, gid_str,
			  sizeof gid_str), cl_ntoh16(p_recvd_rec->mlid));
	cl_map_insert(&test_created_mlids, cl_ntoh16(p_recvd_rec->mlid),
		      p_recvd_rec);

	/* Change the flags to invalid value 0x2 - get back INVALID REQ */

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking invalid flags=0xFF 22  ... (o15.0.1.6)...\n");

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");

	mc_req_rec.mgid = good_mgid;

	mc_req_rec.mgid.raw[1] = 0x22;

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if ((status != IB_REMOTE_ERROR) ||
	    (res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0217: "
			"Failed to recognize create with invalid flags value 0x2 - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* Change the MGID to link local MGID  - get back VALID REQ */

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking link local MGID 0xFF02:0:0:0:0:0:0:1 (o15.0.1.6)...\n");

	mc_req_rec.mgid = osm_link_local_mgid;

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0218: "
			"Failed to create MCG for MGID=0xFF02:0:0:0:0:0:0:1 - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	/* Save the mlid created in test_created_mlids map */
	p_recvd_rec =
	    (ib_member_rec_t *) ib_sa_mad_get_payload_ptr(&res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Created MGID:%s MLID:0x%04X\n",
		inet_ntop(AF_INET6, p_recvd_rec->mgid.raw, gid_str,
			  sizeof gid_str), cl_ntoh16(p_recvd_rec->mlid));
	cl_map_insert(&test_created_mlids, cl_ntoh16(p_recvd_rec->mlid),
		      p_recvd_rec);

	/* o15.0.1.7 - implicitlly checked during the prev steps. */
	/* o15.0.1.8 - implicitlly checked during the prev steps. */

	/* o15.0.1.9 */
	/* - Create MCG with Invalid JoinState.FullMember != 1 : get ERR_REQ_INVALID */

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking new MGID with invalid join state (o15.0.1.9)...\n");

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");

	mc_req_rec.mgid = good_mgid;
	mc_req_rec.mgid.raw[12] = 0xFF;
	mc_req_rec.scope_state = 0x22;	/* link-local scope, non-member state */

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if ((status != IB_REMOTE_ERROR) ||
	    (res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0219: "
			"Failed to recognize create with JoinState != FullMember - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* Lets try a valid join scope state */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking new MGID with valid join state (o15.0.1.9)...\n");

	mc_req_rec.mgid = good_mgid;
	mc_req_rec.scope_state = 0x23;	/* link-local scope, non member and full member */

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0220: "
			"Failed to create MCG with valid join state 0x3 - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	/* Save the mlid created in test_created_mlids map */
	p_recvd_rec =
	    (ib_member_rec_t *) ib_sa_mad_get_payload_ptr(&res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Created MGID:%s MLID:0x%04X\n",
		inet_ntop(AF_INET6, p_recvd_rec->mgid.raw, gid_str,
			  sizeof gid_str), cl_ntoh16(p_recvd_rec->mlid));
	cl_map_insert(&test_created_mlids, cl_ntoh16(p_recvd_rec->mlid),
		      p_recvd_rec);

	/* Lets try another invalid join scope state */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking new MGID with invalid join state (o15.0.1.9)...\n");

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");

	/* We have created a new MCG so now we need different mgid when cresting group otherwise it will be counted as join request . */
	mc_req_rec.mgid = good_mgid;
	mc_req_rec.mgid.raw[12] = 0xFC;

	mc_req_rec.scope_state = 0x24;	/* link-local scope, send only member */

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if ((status != IB_REMOTE_ERROR) ||
	    (res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0221: "
			"Failed to recognize create with JoinState != FullMember - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* Lets try another valid join scope state */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking new MGID creation with valid join state (o15.0.2.3)...\n");

	mc_req_rec.mgid = good_mgid;
	mc_req_rec.mgid.raw[12] = 0xFB;
	memcpy(&special_mgid, &mc_req_rec.mgid, sizeof(ib_gid_t));
	mc_req_rec.scope_state = 0x2F;	/* link-local scope, Full member with all other bits turned on */

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0222: "
			"Failed to create MCG with valid join state 0xF - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	/* Save the mlid created in test_created_mlids map */
	p_recvd_rec =
	    (ib_member_rec_t *) ib_sa_mad_get_payload_ptr(&res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Created MGID:%s MLID:0x%04X\n",
		inet_ntop(AF_INET6, p_recvd_rec->mgid.raw, gid_str,
			  sizeof gid_str), cl_ntoh16(p_recvd_rec->mlid));
	cl_map_insert(&test_created_mlids, cl_ntoh16(p_recvd_rec->mlid),
		      p_recvd_rec);

	/* o15.0.1.10 - can't check on a single client .-- obsolete -
	   checked by SilverStorm bug o15-0.2.4, never the less recheck */
	/* o15-0.2.4 - Check a join request to already created MCG */
	p_mc_res = ib_sa_mad_get_payload_ptr(&res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO, "Check o15-0.2.4 statement...\n");
	/* Try to join */
	memcpy(&mc_req_rec.mgid, &p_mc_res->mgid, sizeof(ib_gid_t));
	/* Request Join */
	ib_member_set_join_state(&mc_req_rec, IB_MC_REC_STATE_NON_MEMBER);
	comp_mask =
	    IB_MCR_COMPMASK_MGID |
	    IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_JOIN_STATE;

	status = osmt_send_mcast_request(p_osmt, 0x1,	/* SubnAdmSet */
					 &mc_req_rec, comp_mask, &res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02CC: "
			"Failed to join MCG with valid req, returned status = %s\n",
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	p_mc_res = ib_sa_mad_get_payload_ptr(&res_sa_mad);
	if ((p_mc_res->scope_state & 0x7) != 0x7) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02D0: "
			"Validating JoinState update failed. "
			"Expected 0x27 got 0x%02X\n",
			p_mc_res->scope_state);
		status = IB_ERROR;
		goto Exit;
	}

	/* o15.0.1.11: */
	/* - Try to join into a MGID that exists with JoinState=SendOnlyMember -  */
	/*   see that it updates JoinState. What is the routing change? */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Retry of existing MGID - See JoinState update (o15.0.1.11)...\n");

	mc_req_rec.mgid = good_mgid;

	/* first, make sure  that the group exists */
	mc_req_rec.scope_state = 0x21;
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02CD: "
			"Failed to create/join as full member - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	mc_req_rec.scope_state = 0x22;	/* link-local scope, non-member */
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02D1: "
			"Failed to update existing MGID - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Validating Join State update with NonMember (o15.0.1.11)...\n");

	if (p_mc_res->scope_state != 0x23) {	/* scope is LSB */
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02CE: "
			"Validating JoinState update failed. Expected 0x23 got: 0x%02X\n",
			p_mc_res->scope_state);
		status = IB_ERROR;
		goto Exit;
	}

	/* Try delete current join state then update it with another value  */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking JoinState update request should return 0x22 (o15.0.1.11)...\n");

	mc_req_rec.rate =
	    IB_LINK_WIDTH_ACTIVE_1X | IB_PATH_SELECTOR_GREATER_THAN << 6;
	mc_req_rec.mgid = good_mgid;

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Partially delete JoinState (o15.0.1.14)...\n");

	/* link-local scope, both non-member bits,
	   so we should not be able to delete) */
	mc_req_rec.scope_state = 0x26;
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 0,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status != IB_REMOTE_ERROR) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02CF: "
			"Expected to fail partially update JoinState, "
			"but got %s\n",
			ib_get_err_str(status));
		status = IB_ERROR;
		goto Exit;
	}

	/* link-local scope, NonMember bit, the FullMember bit should stay */
	mc_req_rec.scope_state = 0x22;
	status = osmt_send_mcast_request(p_osmt, 0,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02D3: "
			"Failed to partially update JoinState : %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	p_mc_res = ib_sa_mad_get_payload_ptr(&res_sa_mad);
	if (p_mc_res->scope_state != 0x21) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02D4: "
			"Failed to partially update JoinState : "
			"JoinState = 0x%02X, expected 0x%02X\n",
			p_mc_res->scope_state, 0x21);
		status = IB_ERROR;
		goto Exit;
	}

	/* So far successfully delete state - Now change it */
	mc_req_rec.mgid = good_mgid;
	mc_req_rec.scope_state = 0x24;	/* link-local scope, send only  member */

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02C0: "
			"Failed to update existing MCG - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Validating Join State update with Send Only Member (o15.0.1.11)...\n");

	if (p_mc_res->scope_state != 0x25) {	/* scope is MSB */
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02C1: "
			"Validating JoinState update failed. Expected 0x25 got: 0x%02X\n",
			p_mc_res->scope_state);
		status = IB_ERROR;
		goto Exit;
	}
	/* Now try to update value of join state */
	mc_req_rec.scope_state = 0x21;	/* link-local scope, full member */

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02C2: "
			"Failed to update existing MGID - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Validating Join State update with Full Member\n\t\t"
		"to an existing 0x5 state MCG (o15.0.1.11)...\n");

	if (p_mc_res->scope_state != 0x25) {	/* scope is LSB */
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02C3: "
			"Validating JoinState update failed. Expected 0x25 got: 0x%02X\n",
			p_mc_res->scope_state);
		status = IB_ERROR;
		goto Exit;
	}

	/* Now try to update value of join state */
	mc_req_rec.scope_state = 0x22;	/* link-local scope,non member */

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02C4: "
			"Failed to update existing MGID - got %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Validating Join State update with Non Member\n\t\t"
		"to an existing 0x5 state MCG (o15.0.1.11)...\n");

	if (p_mc_res->scope_state != 0x27) {	/* scope is LSB */
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02C5: "
			"Validating JoinState update failed. Expected 0x27 got: 0x%02X\n",
			p_mc_res->scope_state);
		status = IB_ERROR;
		goto Exit;
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"DEBUG - Current scope_state value : 0x%02X...\n",
		p_mc_res->scope_state);

	/* - We can not check simple join since we have only one tester (for now) */

	/* o15.0.1.12: Not Supported */
	/* - The SendOnlyNonMem join should have a special treatment in the
	   SA but what is it ? */

	/* o15.0.1.13: */
	/* - Try joining with rate that does not exist in any MCG -
	   ERR_REQ_INVALID */

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking BAD RATE when connecting to existing MGID (o15.0.1.13)...\n");
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");

	mc_req_rec.mgid = good_mgid;
	mc_req_rec.rate =
	    IB_LINK_WIDTH_ACTIVE_1X | IB_PATH_SELECTOR_LESS_THAN << 6;
	comp_mask = IB_MCR_COMPMASK_GID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_RATE_SEL | IB_MCR_COMPMASK_RATE;

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if ((status != IB_REMOTE_ERROR) ||
	    (res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02C6: "
			"Failed to catch BAD RATE joining an exiting MGID: %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* Try MTU that does not exist in any MCG */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking BAD MTU (higher them max) when connecting to "
		"existing MGID (o15.0.1.13)...\n");
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");

	mc_req_rec.mgid = osm_ipoib_mgid;
	mc_req_rec.mtu = IB_MTU_LEN_4096 | IB_PATH_SELECTOR_GREATER_THAN << 6;
	comp_mask = IB_MCR_COMPMASK_GID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_MTU_SEL | IB_MCR_COMPMASK_MTU;

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if ((status != IB_REMOTE_ERROR) ||
	    (res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02C7: "
			"Failed to catch BAD RATE (higher them max) joining an exiting MGID: %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* Try another MTU that does not exist in any MCG */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking BAD MTU (less than min) when connecting "
		"to existing MGID (o15.0.1.13)...\n");
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");

	mc_req_rec.mgid = osm_ipoib_mgid;
	mc_req_rec.mtu = IB_MTU_LEN_256 | IB_PATH_SELECTOR_LESS_THAN << 6;
	comp_mask = IB_MCR_COMPMASK_GID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_MTU_SEL | IB_MCR_COMPMASK_MTU;

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if ((status != IB_REMOTE_ERROR) ||
	    (res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02C8: "
			"Failed to catch BAD RATE (less them min) joining an exiting MGID: %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* o15.0.1.14: */
	/* - Try partial delete - actually updating the join state. check it. */

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking partial JoinState delete request - removing NonMember (o15.0.1.14)...\n");

	mc_req_rec.rate =
	    IB_LINK_WIDTH_ACTIVE_1X | IB_PATH_SELECTOR_GREATER_THAN << 6;
	mc_req_rec.mgid = good_mgid;
	comp_mask = IB_MCR_COMPMASK_GID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_RATE_SEL | IB_MCR_COMPMASK_RATE;
	/* link-local scope, non member (so we should not be able to delete) */
	/* but the NonMember bit should be gone */
	mc_req_rec.scope_state = 0x22;

	status = osmt_send_mcast_request(p_osmt, 0,
					 &mc_req_rec, comp_mask, &res_sa_mad);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02C9: "
			"Fail to partially update JoinState during delete: %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Validating Join State removal of Non Member bit (o15.0.1.14)...\n");
	if (p_mc_res->scope_state != 0x25) {	/* scope is MSB - now only the full member & send only member have left */
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02CA: "
			"Validating JoinState update failed. Expected 0x25 got: 0x%02X\n",
			p_mc_res->scope_state);
		status = IB_ERROR;
		goto Exit;
	}

	/* Now use the same scope_state and delete all JoinState - leave multicast group since state is 0x0 */

	mc_req_rec.scope_state = 0x25;
	status = osmt_send_mcast_request(p_osmt, 0,
					 &mc_req_rec, comp_mask, &res_sa_mad);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02CB: "
			"Failed to update JoinState during delete: %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Validating Join State update remove (o15.0.1.14)...\n");

	if (p_mc_res->scope_state != 0x25) {	/* scope is MSB - now only 0x0 so port is removed from MCG */
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02BF: "
			"Validating JoinState update failed. Expected 0x25 got: 0x%02X\n",
			p_mc_res->scope_state);
		status = IB_ERROR;
		goto Exit;
	}

	/* - Try joining (not full mem) again to see the group was deleted. (should fail) */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Delete by trying to Join deleted group (o15.0.1.13)...\n");

	mc_req_rec.scope_state = 0x22;	/* use non member - so if no group fail */

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,	/* join */
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status != IB_REMOTE_ERROR) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02BC: "
			"Succeeded Joining Deleted Group: %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* - Try deletion of the IPoIB MCG and get: ERR_REQ_INVALID */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking BAD Delete of Mgid membership (no prev join) (o15.0.1.15)...\n");
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");

	mc_req_rec.mgid = osm_ipoib_mgid;
	mc_req_rec.rate =
	    IB_LINK_WIDTH_ACTIVE_1X | IB_PATH_SELECTOR_GREATER_THAN << 6;
	mc_req_rec.scope_state = 0x21;	/* delete full member */

	status = osmt_send_mcast_request(p_osmt, 0,	/* delete flag */
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if ((status != IB_REMOTE_ERROR) ||
	    (res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02BD: "
			"Failed to catch BAD delete from IPoIB: %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* Prepare another MCG for the following tests : */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Create given MGID=%s\n\t\t(o15.0.1.4)...\n",
		inet_ntop(AF_INET6, osm_ipoib_mgid.raw, gid_str,
			  sizeof gid_str));

	mc_req_rec.mgid = good_mgid;
	mc_req_rec.mgid.raw[12] = 0xAA;
	mc_req_rec.pkt_life = 0 | IB_PATH_SELECTOR_GREATER_THAN << 6;
	mc_req_rec.scope_state = 0x21;	/* Full memeber */
	comp_mask = IB_MCR_COMPMASK_GID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_LIFE | IB_MCR_COMPMASK_LIFE_SEL;

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02BE: "
			"Failed to create MCG for %s - got %s/%s\n",
			inet_ntop(AF_INET6, good_mgid.raw, gid_str,
				  sizeof gid_str), ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		goto Exit;
	}

	/* - Try delete with valid join state */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Full Delete of a group (o15.0.1.14)...\n");
	mc_req_rec.scope_state = 0x21;	/* the FullMember is the current JoinState */
	status = osmt_send_mcast_request(p_osmt, 0,
					 &mc_req_rec, comp_mask, &res_sa_mad);

	if (status != IB_SUCCESS) {
		goto Exit;
	}

	/* o15.0.1.15: */
	/* - Try deletion of the IPoIB MCG and get: ERR_REQ_INVALID */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking BAD Delete of IPoIB membership (no prev join) (o15.0.1.15)...\n");
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");

	mc_req_rec.mgid = osm_ipoib_mgid;
	mc_req_rec.rate =
	    IB_LINK_WIDTH_ACTIVE_1X | IB_PATH_SELECTOR_GREATER_THAN << 6;
	mc_req_rec.scope_state = 0x21;	/* delete full member */

	status = osmt_send_mcast_request(p_osmt, 0,	/* delete flag */
					 &mc_req_rec, comp_mask, &res_sa_mad);

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if ((status != IB_REMOTE_ERROR) ||
	    (res_sa_mad.status != IB_SA_MAD_STATUS_REQ_INVALID)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0223: "
			"Failed to catch BAD delete from IPoIB: %s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

  /**************************************************************************/
	/* Checking join with invalid MTU */
	osmt_init_mc_query_rec(p_osmt, &mc_req_rec);
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Checking Join with unrealistic mtu : \n"
		"\t\tFirst create new MCG than try to join it \n"
		"\t\twith unrealistic MTU greater than 4096 (o15.0.1.8)...\n");

	/* First create new mgrp */
	ib_member_set_join_state(&mc_req_rec, IB_MC_REC_STATE_FULL_MEMBER);
	mc_req_rec.mtu = IB_MTU_LEN_1024 | IB_PATH_SELECTOR_EXACTLY << 6;
	memset(&mc_req_rec.mgid, 0, sizeof(ib_gid_t));
	comp_mask = IB_MCR_COMPMASK_MGID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
	    IB_MCR_COMPMASK_MTU_SEL | IB_MCR_COMPMASK_MTU;

	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	p_mc_res = ib_sa_mad_get_payload_ptr(&res_sa_mad);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02EB: "
			"Failed to create new mgrp\n");
		goto Exit;
	}
	memcpy(&tmp_mgid, &p_mc_res->mgid, sizeof(ib_gid_t));
	osm_dump_mc_record(&p_osmt->log, p_mc_res, OSM_LOG_INFO);
	/* tmp_mtu = p_mc_res->mtu & 0x3F; */

	/* impossible requested mtu always greater than exist in MCG */
	mc_req_rec.mtu = IB_MTU_LEN_4096 | IB_PATH_SELECTOR_GREATER_THAN << 6;
	memcpy(&mc_req_rec.mgid, &tmp_mgid, sizeof(ib_gid_t));
	ib_member_set_join_state(&mc_req_rec, IB_MC_REC_STATE_FULL_MEMBER);
	comp_mask =
	    IB_MCR_COMPMASK_GID |
	    IB_MCR_COMPMASK_PORT_GID |
	    IB_MCR_COMPMASK_JOIN_STATE |
	    IB_MCR_COMPMASK_MTU_SEL | IB_MCR_COMPMASK_MTU;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmt_send_mcast_request(p_osmt, 1,
					 &mc_req_rec, comp_mask, &res_sa_mad);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status == IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02E4: "
			"Expected REMOTE ERROR got:%s/%s\n",
			ib_get_err_str(status),
			ib_get_mad_status_str((ib_mad_t *) (&res_sa_mad)));
		status = IB_ERROR;
		goto Exit;
	}

	/* - Try GetTable with PortGUID wildcarded and get back some groups. */
	status = osmt_query_mcast(p_osmt);
	cnt = cl_qmap_count(&p_osmt->exp_subn.mgrp_mlid_tbl);
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO, "(Before checking Max MCG creation): "
		"Number of MC Records found in SA DB is %d\n", cnt);

  /**************************************************************************/
	/* Checking join on behalf of remote port gid */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO, "Checking Proxy Join...\n");
	osmt_init_mc_query_rec(p_osmt, &mc_req_rec);
	memset(&context, 0, sizeof(context));

	/*
	 * Do a blocking query for all NodeRecords in the subnet.
	 */
	status = osmtest_get_all_recs(p_osmt, IB_MAD_ATTR_NODE_RECORD,
				      sizeof(*p_rec), &context);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02E5: "
			"osmtest_get_all_recs failed on getting all node records(%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	/*
	 * Populate the database with the received records.
	 */
	num_recs = context.result.result_cnt;
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Received %u records\n", num_recs);

	for (i = 0; i < num_recs; i++) {
		p_rec =
		    osmv_get_query_node_rec(context.result.p_result_madw, i);
		if (p_rec->node_info.port_guid != p_osmt->local_port.port_guid
		    && p_rec->node_info.node_type == IB_NODE_TYPE_CA) {
			OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
				"remote port_guid = 0x%" PRIx64 "\n",
				cl_ntoh64(p_rec->node_info.port_guid));

			remote_port_guid = p_rec->node_info.port_guid;
			i = num_recs;
			break;
		}
	}

	if (remote_port_guid != 0x0) {
		ib_member_set_join_state(&mc_req_rec,
					 IB_MC_REC_STATE_FULL_MEMBER);
		memset(&mc_req_rec.mgid, 0, sizeof(ib_gid_t));
		mc_req_rec.port_gid.unicast.interface_id = remote_port_guid;
		comp_mask = IB_MCR_COMPMASK_MGID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS;	/* all above are required */

		status = osmt_send_mcast_request(p_osmt, 1,
						 &mc_req_rec,
						 comp_mask, &res_sa_mad);

		if (status != IB_SUCCESS) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02B4: "
				"Could not join on behalf of remote port 0x%016"
				PRIx64 " remote status: %s\n",
				cl_ntoh64(remote_port_guid),
				ib_get_mad_status_str((ib_mad_t
						       *) (&res_sa_mad)));
			status = IB_ERROR;
			goto Exit;
		}

		p_mc_res = ib_sa_mad_get_payload_ptr(&res_sa_mad);
		memcpy(&proxy_mgid, &p_mc_res->mgid, sizeof(ib_gid_t));

		/* First try a bad deletion then good one */

		OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
			"Trying deletion of remote port with local port guid\n");

		osmt_init_mc_query_rec(p_osmt, &mc_req_rec);
		ib_member_set_join_state(&mc_req_rec,
					 IB_MC_REC_STATE_FULL_MEMBER);
		comp_mask =
		    IB_MCR_COMPMASK_MGID | IB_MCR_COMPMASK_PORT_GID |
		    IB_MCR_COMPMASK_JOIN_STATE;

		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");

		status = osmt_send_mcast_request(p_osmt, 0,	/* delete flag */
						 &mc_req_rec,
						 comp_mask, &res_sa_mad);

		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

		if (status == IB_SUCCESS) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02A9: "
				"Successful deletion of remote port guid with local one MGID : "
				"%s, Got : %s/%s\n",
				inet_ntop(AF_INET6,
					p_mgrp->mcmember_rec.mgid.raw,
					gid_str, sizeof gid_str),
				ib_get_err_str(status),
				ib_get_mad_status_str((ib_mad_t
						       *) (&res_sa_mad)));
			status = IB_ERROR;
			goto Exit;
		}

		OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
			"Trying deletion of remote port with the right port guid\n");

		osmt_init_mc_query_rec(p_osmt, &mc_req_rec);
		ib_member_set_join_state(&mc_req_rec,
					 IB_MC_REC_STATE_FULL_MEMBER);
		mc_req_rec.mgid = proxy_mgid;
		mc_req_rec.port_gid.unicast.interface_id = remote_port_guid;
		comp_mask =
		    IB_MCR_COMPMASK_MGID |
		    IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_JOIN_STATE;
		status = osmt_send_mcast_request(p_osmt, 0,	/* delete flag */
						 &mc_req_rec,
						 comp_mask, &res_sa_mad);
		if (status != IB_SUCCESS) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02B0: "
				"Failed to delete mgid with remote port guid MGID : "
				"%s, Got : %s/%s\n",
				inet_ntop(AF_INET6,
					p_mgrp->mcmember_rec.mgid.raw,
					gid_str, sizeof gid_str),
				ib_get_err_str(status),
				ib_get_mad_status_str((ib_mad_t
						       *) (&res_sa_mad)));
			goto Exit;
		}
	} else {
		OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
			"Could not check proxy join since could not found remote port, different from local port\n");
	}

	/* prepare init for next check */
	osmt_init_mc_query_rec(p_osmt, &mc_req_rec);

  /**************************************************************************/
	if (p_osmt->opt.mmode > 2) {
		/* Check invalid Join with max mlid which is more than the
		   Mellanox switches support 0xC000+0x1000 = 0xd000 */
		OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
			"Checking Creation of Maximum avaliable Groups (MulticastFDBCap)...\n");
		tmp_mlid = cl_ntoh16(max_mlid) - cnt;

		while (tmp_mlid > 0 && !ReachedMlidLimit) {
			uint16_t cur_mlid = 0;

			/* Request Set */
			ib_member_set_join_state(&mc_req_rec,
						 IB_MC_REC_STATE_FULL_MEMBER);
			/* Good Flow - mgid is 0 while giving all required fields for
			   join : P_Key, Q_Key, SL, FlowLabel, Tclass */

			mc_req_rec.rate =
			    IB_LINK_WIDTH_ACTIVE_1X |
			    IB_PATH_SELECTOR_GREATER_THAN << 6;
			mc_req_rec.mlid = max_mlid;
			memset(&mc_req_rec.mgid, 0, sizeof(ib_gid_t));
			comp_mask = IB_MCR_COMPMASK_MGID | IB_MCR_COMPMASK_PORT_GID | IB_MCR_COMPMASK_QKEY | IB_MCR_COMPMASK_PKEY | IB_MCR_COMPMASK_SL | IB_MCR_COMPMASK_FLOW | IB_MCR_COMPMASK_JOIN_STATE | IB_MCR_COMPMASK_TCLASS |	/* all above are required */
			    IB_MCR_COMPMASK_MLID;
			status = osmt_send_mcast_request(p_osmt, 1,
							 &mc_req_rec,
							 comp_mask,
							 &res_sa_mad);

			p_mc_res = ib_sa_mad_get_payload_ptr(&res_sa_mad);
			if (status != IB_SUCCESS) {

				if (cur_mlid > cl_ntoh16(max_mlid)) {

					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"ERR 2E1 "
						"Successful Join with greater mlid than switches support (MulticastFDBCap) 0x%04X\n",
						cur_mlid);
					status = IB_ERROR;
					osm_dump_mc_record(&p_osmt->log,
							   p_mc_res,
							   OSM_LOG_VERBOSE);
					goto Exit;
				} else
				    if ((res_sa_mad.
					 status & IB_SMP_STATUS_MASK) ==
					IB_SA_MAD_STATUS_NO_RESOURCES) {
					/* You can quitly exit the loop since no available mlid in SA DB
					   i.e. reached the maximum valiad avalable mlid */
					ReachedMlidLimit = TRUE;
				}
			} else {
				cur_mlid = cl_ntoh16(p_mc_res->mlid);
				/* Save the mlid created in test_created_mlids map */
				p_recvd_rec =
				    (ib_member_rec_t *)
				    ib_sa_mad_get_payload_ptr(&res_sa_mad);
				OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
					"Created MGID:%s MLID:0x%04X\n",
					inet_ntop(AF_INET6,
						  p_recvd_rec->mgid.raw,
						  gid_str, sizeof gid_str),
					cl_ntoh16(p_recvd_rec->mlid));
				cl_map_insert(&test_created_mlids,
					      cl_ntoh16(p_recvd_rec->mlid),
					      p_recvd_rec);
			}
			tmp_mlid--;
		}
	}

	/* Prepare the mc_req_rec for the rest of the flow */
	osmt_init_mc_query_rec(p_osmt, &mc_req_rec);

  /**************************************************************************/
	/* o15.0.1.16: */
	/* - Try GetTable with PortGUID wildcarded and get back some groups. */

	status = osmt_query_mcast(p_osmt);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02B1: "
			"Failed to query multicast groups: %s\n",
			ib_get_err_str(status));
		goto Exit;
	}

	cnt = cl_qmap_count(&p_osmt->exp_subn.mgrp_mlid_tbl);
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO, "(Before Deletion of all MCG): "
		"Number of MC Records found in SA DB is %d\n", cnt);

	/* Delete all MCG that are not of IPoIB */
	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Cleanup all MCG that are not IPoIB...\n");

	p_mgrp_mlid_tbl = &p_osmt->exp_subn.mgrp_mlid_tbl;
	p_mgrp = (osmtest_mgrp_t *) cl_qmap_head(p_mgrp_mlid_tbl);
	/* scan all available multicast groups in the DB and fill in the table */
	while (p_mgrp != (osmtest_mgrp_t *) cl_qmap_end(p_mgrp_mlid_tbl)) {
		/* Only if different from IPoIB Mgid try to delete */
		if (!IS_IPOIB_MGID(&p_mgrp->mcmember_rec.mgid)) {
			osmt_init_mc_query_rec(p_osmt, &mc_req_rec);
			mc_req_rec.mgid = p_mgrp->mcmember_rec.mgid;

			/* o15-0.1.4 - need to specify the oppsite state for a valid delete */
			if (!memcmp
			    (&special_mgid, &p_mgrp->mcmember_rec.mgid,
			     sizeof(special_mgid))) {
				mc_req_rec.scope_state = 0x2F;
			} else {
				mc_req_rec.scope_state = 0x21;
			}
			comp_mask =
			    IB_MCR_COMPMASK_MGID |
			    IB_MCR_COMPMASK_PORT_GID |
			    IB_MCR_COMPMASK_JOIN_STATE;

			OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
				"Sending request to delete MGID : %s"
				", scope_state : 0x%02X\n",
				inet_ntop(AF_INET6, mc_req_rec.mgid.raw,
					  gid_str, sizeof gid_str),
				mc_req_rec.scope_state);
			status = osmt_send_mcast_request(p_osmt, 0,	/* delete flag */
							 &mc_req_rec,
							 comp_mask,
							 &res_sa_mad);
			if (status != IB_SUCCESS) {
				OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
					"ERR 02FF: Failed to delete MGID : %s"
					" ,\n\t\t it is not our MCG, Status : %s/%s\n",
					inet_ntop(AF_INET6,
						  p_mgrp->mcmember_rec.mgid.raw,
						  gid_str, sizeof gid_str),
					ib_get_err_str(status),
					ib_get_mad_status_str((ib_mad_t *)
							      (&res_sa_mad)));
				fail_to_delete_mcg++;
			}
		} else {
			end_ipoib_cnt++;
		}
		p_mgrp = (osmtest_mgrp_t *) cl_qmap_next(&p_mgrp->map_item);
	}

	status = osmt_query_mcast(p_osmt);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02B2 "
			"GetTable of all records has failed - got %s\n",
			ib_get_err_str(status));
		goto Exit;
	}

	/* If we are in single mode check flow - need to make sure all the multicast groups
	   that are left are not ones created during the flow.
	 */
	if (p_osmt->opt.mmode == 1 || p_osmt->opt.mmode == 3) {
		end_cnt = cl_qmap_count(&p_osmt->exp_subn.mgrp_mlid_tbl);

		OSM_LOG(&p_osmt->log, OSM_LOG_INFO, "Status of MC Records in SA DB during the test flow:\n" "  Beginning of test\n" "       Unrelated to the test: %d\n" "       IPoIB MC Records     : %d\n" "       Total                : %d\n" "  End of test\n" "       Failed to delete     : %d\n" "       IPoIB MC Records     : %d\n" "       Total                : %d\n", mcg_outside_test_cnt,	/* Non-IPoIB that existed at the beginning */
			start_ipoib_cnt,	/* IPoIB records */
			start_cnt,	/* Total: IPoIB and MC Records unrelated to the test */
			fail_to_delete_mcg,	/* Failed to delete at the end */
			end_ipoib_cnt,	/* IPoIB records */
			end_cnt);	/* Total MC Records at the end */

		/* when we compare num of MCG we should consider an outside source which create other MCGs */
		if ((end_cnt - fail_to_delete_mcg - end_ipoib_cnt) !=
		    (start_cnt - mcg_outside_test_cnt - start_ipoib_cnt)) {
			OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
				"Got different number of non-IPoIB records stored in SA DB\n\t\t"
				"at Start got %d, at End got %d (IPoIB groups only)\n",
				(start_cnt - mcg_outside_test_cnt -
				 start_ipoib_cnt),
				(end_cnt - fail_to_delete_mcg - end_ipoib_cnt));
		}

		p_mgrp_mlid_tbl = &p_osmt->exp_subn.mgrp_mlid_tbl;
		p_mgrp = (osmtest_mgrp_t *) cl_qmap_head(p_mgrp_mlid_tbl);
		while (p_mgrp !=
		       (osmtest_mgrp_t *) cl_qmap_end(p_mgrp_mlid_tbl)) {
			uint16_t mlid =
			    (uint16_t) cl_qmap_key((cl_map_item_t *) p_mgrp);

			OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
				"Found MLID:0x%04X\n", mlid);
			/* Check if the mlid is in the test_created_mlids. If TRUE, then we
			   didn't delete a MCgroup that was created in this flow. */
			if (cl_map_get(&test_created_mlids, mlid) != NULL) {
				/* This means that we still have an mgrp that we created!! */
				OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 02FE: "
					"Wasn't able to erase mgrp with MGID:%s"
					" MLID:0x%04X\n",
					inet_ntop(AF_INET6,
						  p_mgrp->mcmember_rec.mgid.raw,
						  gid_str, sizeof gid_str),
					mlid);
				got_error = TRUE;
			} else {
				OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
					"Still exists %s MGID:%s\n",
					(IS_IPOIB_MGID
					 (&p_mgrp->mcmember_rec.
					  mgid)) ? "IPoIB" : "non-IPoIB",
					inet_ntop(AF_INET6,
						p_mgrp->mcmember_rec.mgid.raw,
						gid_str, sizeof gid_str));
			}
			p_mgrp =
			    (osmtest_mgrp_t *) cl_qmap_next(&p_mgrp->map_item);
		}

		if (got_error) {
			__osmt_print_all_multicast_records(p_osmt);
			status = IB_ERROR;
		}
	}
Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return status;
}
