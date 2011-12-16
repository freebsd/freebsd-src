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

/* TODO : Check why we dont free the cl_qmap_items we store when reading DB */

/*
 * Abstract:
 *    Implementation of osmtest_t.
 *    This object represents the OSMTest Test object.
 *
 */

#ifdef __WIN__
#pragma warning(disable : 4996)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __WIN__
#include <complib/cl_timer.h>
#else
#include <strings.h>
#include <sys/time.h>
#endif
#include <complib/cl_debug.h>
#include "osmtest.h"

#ifndef __WIN__
#define strnicmp strncasecmp
#endif

#define POOL_MIN_ITEMS  64
#define GUID_ARRAY_SIZE 64

typedef struct _osmtest_sm_info_rec {
	ib_net64_t sm_guid;
	ib_net16_t lid;
	uint8_t priority;
	uint8_t sm_state;
} osmtest_sm_info_rec_t;

typedef struct _osmtest_inform_info {
	boolean_t subscribe;
	ib_net32_t qpn;
	ib_net16_t trap;
} osmtest_inform_info_t;

typedef struct _osmtest_inform_info_rec {
	ib_gid_t subscriber_gid;
	ib_net16_t subscriber_enum;
} osmtest_inform_info_rec_t;

typedef enum _osmtest_token_val {
	OSMTEST_TOKEN_COMMENT = 0,
	OSMTEST_TOKEN_END,
	OSMTEST_TOKEN_DEFINE_NODE,
	OSMTEST_TOKEN_DEFINE_PORT,
	OSMTEST_TOKEN_DEFINE_PATH,
	OSMTEST_TOKEN_DEFINE_LINK,
	OSMTEST_TOKEN_LID,
	OSMTEST_TOKEN_BASE_VERSION,
	OSMTEST_TOKEN_CLASS_VERSION,
	OSMTEST_TOKEN_NODE_TYPE,
	OSMTEST_TOKEN_NUM_PORTS,
	OSMTEST_TOKEN_SYS_GUID,
	OSMTEST_TOKEN_NODE_GUID,
	OSMTEST_TOKEN_PORT_GUID,
	OSMTEST_TOKEN_PARTITION_CAP,
	OSMTEST_TOKEN_DEVICE_ID,
	OSMTEST_TOKEN_REVISION,
	OSMTEST_TOKEN_PORT_NUM,
	OSMTEST_TOKEN_VENDOR_ID,
	OSMTEST_TOKEN_DGID,
	OSMTEST_TOKEN_SGID,
	OSMTEST_TOKEN_DLID,
	OSMTEST_TOKEN_SLID,
	OSMTEST_TOKEN_HOP_FLOW_RAW,
	OSMTEST_TOKEN_TCLASS,
	OSMTEST_TOKEN_NUM_PATH,
	OSMTEST_TOKEN_PKEY,
	OSMTEST_TOKEN_SL,
	OSMTEST_TOKEN_RATE,
	OSMTEST_TOKEN_PKT_LIFE,
	OSMTEST_TOKEN_PREFERENCE,
	OSMTEST_TOKEN_MKEY,
	OSMTEST_TOKEN_SUBN_PREF,
	OSMTEST_TOKEN_BASE_LID,
	OSMTEST_TOKEN_SM_BASE_LID,
	OSMTEST_TOKEN_CAP_MASK,
	OSMTEST_TOKEN_DIAG_CODE,
	OSMTEST_TOKEN_MKEY_LEASE_PER,
	OSMTEST_TOKEN_LOC_PORT_NUM,
	OSMTEST_TOKEN_LINK_WID_EN,
	OSMTEST_TOKEN_LINK_WID_SUP,
	OSMTEST_TOKEN_LINK_WID_ACT,
	OSMTEST_TOKEN_LINK_SPEED_SUP,
	OSMTEST_TOKEN_PORT_STATE,
	OSMTEST_TOKEN_STATE_INFO2,
	OSMTEST_TOKEN_MKEY_PROT_BITS,
	OSMTEST_TOKEN_LMC,
	OSMTEST_TOKEN_LINK_SPEED,
	OSMTEST_TOKEN_MTU_SMSL,
	OSMTEST_TOKEN_VL_CAP,
	OSMTEST_TOKEN_VL_HIGH_LIMIT,
	OSMTEST_TOKEN_VL_ARB_HIGH_CAP,
	OSMTEST_TOKEN_VL_ARB_LOW_CAP,
	OSMTEST_TOKEN_MTU_CAP,
	OSMTEST_TOKEN_VL_STALL_LIFE,
	OSMTEST_TOKEN_VL_ENFORCE,
	OSMTEST_TOKEN_MKEY_VIOL,
	OSMTEST_TOKEN_PKEY_VIOL,
	OSMTEST_TOKEN_QKEY_VIOL,
	OSMTEST_TOKEN_GUID_CAP,
	OSMTEST_TOKEN_SUBN_TIMEOUT,
	OSMTEST_TOKEN_RESP_TIME_VAL,
	OSMTEST_TOKEN_ERR_THRESHOLD,
	OSMTEST_TOKEN_MTU,
	OSMTEST_TOKEN_FROMLID,
	OSMTEST_TOKEN_FROMPORTNUM,
	OSMTEST_TOKEN_TOPORTNUM,
	OSMTEST_TOKEN_TOLID,
	OSMTEST_TOKEN_UNKNOWN
} osmtest_token_val_t;

typedef struct _osmtest_token {
	osmtest_token_val_t val;
	size_t str_size;
	const char *str;
} osmtest_token_t;

const osmtest_token_t token_array[] = {
	{OSMTEST_TOKEN_COMMENT, 1, "#"},
	{OSMTEST_TOKEN_END, 3, "END"},
	{OSMTEST_TOKEN_DEFINE_NODE, 11, "DEFINE_NODE"},
	{OSMTEST_TOKEN_DEFINE_PORT, 11, "DEFINE_PORT"},
	{OSMTEST_TOKEN_DEFINE_PATH, 11, "DEFINE_PATH"},
	{OSMTEST_TOKEN_DEFINE_LINK, 11, "DEFINE_LINK"},
	{OSMTEST_TOKEN_LID, 3, "LID"},
	{OSMTEST_TOKEN_BASE_VERSION, 12, "BASE_VERSION"},
	{OSMTEST_TOKEN_CLASS_VERSION, 13, "CLASS_VERSION"},
	{OSMTEST_TOKEN_NODE_TYPE, 9, "NODE_TYPE"},
	{OSMTEST_TOKEN_NUM_PORTS, 9, "NUM_PORTS"},
	{OSMTEST_TOKEN_SYS_GUID, 8, "SYS_GUID"},
	{OSMTEST_TOKEN_NODE_GUID, 9, "NODE_GUID"},
	{OSMTEST_TOKEN_PORT_GUID, 9, "PORT_GUID"},
	{OSMTEST_TOKEN_PARTITION_CAP, 13, "PARTITION_CAP"},
	{OSMTEST_TOKEN_DEVICE_ID, 9, "DEVICE_ID"},
	{OSMTEST_TOKEN_REVISION, 8, "REVISION"},
	{OSMTEST_TOKEN_PORT_NUM, 8, "PORT_NUM"},
	{OSMTEST_TOKEN_VENDOR_ID, 9, "VENDOR_ID"},
	{OSMTEST_TOKEN_DGID, 4, "DGID"},
	{OSMTEST_TOKEN_SGID, 4, "SGID"},
	{OSMTEST_TOKEN_DLID, 4, "DLID"},
	{OSMTEST_TOKEN_SLID, 4, "SLID"},
	{OSMTEST_TOKEN_HOP_FLOW_RAW, 12, "HOP_FLOW_RAW"},
	{OSMTEST_TOKEN_TCLASS, 6, "TCLASS"},
	{OSMTEST_TOKEN_NUM_PATH, 8, "NUM_PATH"},
	{OSMTEST_TOKEN_PKEY, 4, "PKEY"},
	{OSMTEST_TOKEN_SL, 2, "SL"},
	{OSMTEST_TOKEN_RATE, 4, "RATE"},
	{OSMTEST_TOKEN_PKT_LIFE, 8, "PKT_LIFE"},
	{OSMTEST_TOKEN_PREFERENCE, 10, "PREFERENCE"},
	{OSMTEST_TOKEN_MKEY, 4, "M_KEY"},
	{OSMTEST_TOKEN_SUBN_PREF, 13, "SUBNET_PREFIX"},
	{OSMTEST_TOKEN_BASE_LID, 8, "BASE_LID"},
	{OSMTEST_TOKEN_SM_BASE_LID, 18, "MASTER_SM_BASE_LID"},
	{OSMTEST_TOKEN_CAP_MASK, 15, "CAPABILITY_MASK"},
	{OSMTEST_TOKEN_DIAG_CODE, 9, "DIAG_CODE"},
	{OSMTEST_TOKEN_MKEY_LEASE_PER, 18, "m_key_lease_period"},
	{OSMTEST_TOKEN_LOC_PORT_NUM, 14, "local_port_num"},
	{OSMTEST_TOKEN_LINK_WID_EN, 18, "link_width_enabled"},
	{OSMTEST_TOKEN_LINK_WID_SUP, 20, "link_width_supported"},
	{OSMTEST_TOKEN_LINK_WID_ACT, 17, "link_width_active"},
	{OSMTEST_TOKEN_LINK_SPEED_SUP, 20, "link_speed_supported"},
	{OSMTEST_TOKEN_PORT_STATE, 10, "port_state"},
	{OSMTEST_TOKEN_STATE_INFO2, 10, "state_info2"},
	{OSMTEST_TOKEN_MKEY_PROT_BITS, 3, "mpb"},
	{OSMTEST_TOKEN_LMC, 3, "lmc"},
	{OSMTEST_TOKEN_LINK_SPEED, 10, "link_speed"},
	{OSMTEST_TOKEN_MTU_SMSL, 8, "mtu_smsl"},
	{OSMTEST_TOKEN_VL_CAP, 6, "vl_cap"},
	{OSMTEST_TOKEN_VL_HIGH_LIMIT, 13, "vl_high_limit"},
	{OSMTEST_TOKEN_VL_ARB_HIGH_CAP, 15, "vl_arb_high_cap"},
	{OSMTEST_TOKEN_VL_ARB_LOW_CAP, 14, "vl_arb_low_cap"},
	{OSMTEST_TOKEN_MTU_CAP, 7, "mtu_cap"},
	{OSMTEST_TOKEN_VL_STALL_LIFE, 13, "vl_stall_life"},
	{OSMTEST_TOKEN_VL_ENFORCE, 10, "vl_enforce"},
	{OSMTEST_TOKEN_MKEY_VIOL, 16, "m_key_violations"},
	{OSMTEST_TOKEN_PKEY_VIOL, 16, "p_key_violations"},
	{OSMTEST_TOKEN_QKEY_VIOL, 16, "q_key_violations"},
	{OSMTEST_TOKEN_GUID_CAP, 8, "guid_cap"},
	{OSMTEST_TOKEN_SUBN_TIMEOUT, 14, "subnet_timeout"},
	{OSMTEST_TOKEN_RESP_TIME_VAL, 15, "resp_time_value"},
	{OSMTEST_TOKEN_ERR_THRESHOLD, 15, "error_threshold"},
	{OSMTEST_TOKEN_MTU, 3, "MTU"},	/*  must be after the other mtu... tokens. */
	{OSMTEST_TOKEN_FROMLID, 8, "from_lid"},
	{OSMTEST_TOKEN_FROMPORTNUM, 13, "from_port_num"},
	{OSMTEST_TOKEN_TOPORTNUM, 11, "to_port_num"},
	{OSMTEST_TOKEN_TOLID, 6, "to_lid"},
	{OSMTEST_TOKEN_UNKNOWN, 0, ""}	/* must be last entry */
};

#define IB_MAD_STATUS_CLASS_MASK       (CL_HTON16(0xFF00))

static const char ib_mad_status_str_busy[] = "IB_MAD_STATUS_BUSY";
static const char ib_mad_status_str_redirect[] = "IB_MAD_STATUS_REDIRECT";
static const char ib_mad_status_str_unsup_class_ver[] =
    "IB_MAD_STATUS_UNSUP_CLASS_VER";
static const char ib_mad_status_str_unsup_method[] =
    "IB_MAD_STATUS_UNSUP_METHOD";
static const char ib_mad_status_str_unsup_method_attr[] =
    "IB_MAD_STATUS_UNSUP_METHOD_ATTR";
static const char ib_mad_status_str_invalid_field[] =
    "IB_MAD_STATUS_INVALID_FIELD";
static const char ib_mad_status_str_no_resources[] =
    "IB_SA_MAD_STATUS_NO_RESOURCES";
static const char ib_mad_status_str_req_invalid[] =
    "IB_SA_MAD_STATUS_REQ_INVALID";
static const char ib_mad_status_str_no_records[] =
    "IB_SA_MAD_STATUS_NO_RECORDS";
static const char ib_mad_status_str_too_many_records[] =
    "IB_SA_MAD_STATUS_TOO_MANY_RECORDS";
static const char ib_mad_status_str_invalid_gid[] =
    "IB_SA_MAD_STATUS_INVALID_GID";
static const char ib_mad_status_str_insuf_comps[] =
    "IB_SA_MAD_STATUS_INSUF_COMPS";
static const char generic_or_str[] = " | ";

/**********************************************************************
 **********************************************************************/
const char *ib_get_mad_status_str(IN const ib_mad_t * const p_mad)
{
	static char line[512];
	uint32_t offset = 0;
	ib_net16_t status;
	boolean_t first = TRUE;

	line[offset] = '\0';

	status = (ib_net16_t) (p_mad->status & IB_SMP_STATUS_MASK);

	if (status == 0) {
		strcat(&line[offset], "IB_SUCCESS");
		return (line);
	}

	if (status & IB_MAD_STATUS_BUSY) {
		strcat(&line[offset], ib_mad_status_str_busy);
		offset += sizeof(ib_mad_status_str_busy);
	}
	if (status & IB_MAD_STATUS_REDIRECT) {
		if (!first) {
			strcat(&line[offset], generic_or_str);
			offset += sizeof(generic_or_str) - 1;
		}
		first = FALSE;
		strcat(&line[offset], ib_mad_status_str_redirect);
		offset += sizeof(ib_mad_status_str_redirect) - 1;
	}
	if ((status & IB_MAD_STATUS_INVALID_FIELD) ==
	    IB_MAD_STATUS_UNSUP_CLASS_VER) {
		if (!first) {
			strcat(&line[offset], generic_or_str);
			offset += sizeof(generic_or_str) - 1;
		}
		first = FALSE;
		strcat(&line[offset], ib_mad_status_str_unsup_class_ver);
		offset += sizeof(ib_mad_status_str_unsup_class_ver) - 1;
	}
	if ((status & IB_MAD_STATUS_INVALID_FIELD) ==
	    IB_MAD_STATUS_UNSUP_METHOD) {
		if (!first) {
			strcat(&line[offset], generic_or_str);
			offset += sizeof(generic_or_str) - 1;
		}
		first = FALSE;
		strcat(&line[offset], ib_mad_status_str_unsup_method);
		offset += sizeof(ib_mad_status_str_unsup_method) - 1;
	}
	if ((status & IB_MAD_STATUS_INVALID_FIELD) ==
	    IB_MAD_STATUS_UNSUP_METHOD_ATTR) {
		if (!first) {
			strcat(&line[offset], generic_or_str);
			offset += sizeof(generic_or_str) - 1;
		}
		first = FALSE;
		strcat(&line[offset], ib_mad_status_str_unsup_method_attr);
		offset += sizeof(ib_mad_status_str_unsup_method_attr) - 1;
	}
	if ((status & IB_MAD_STATUS_INVALID_FIELD) ==
	    IB_MAD_STATUS_INVALID_FIELD) {
		if (!first) {
			strcat(&line[offset], generic_or_str);
			offset += sizeof(generic_or_str) - 1;
		}
		first = FALSE;
		strcat(&line[offset], ib_mad_status_str_invalid_field);
		offset += sizeof(ib_mad_status_str_invalid_field) - 1;
	}
	if ((status & IB_MAD_STATUS_CLASS_MASK) ==
	    IB_SA_MAD_STATUS_NO_RESOURCES) {
		if (!first) {
			strcat(&line[offset], generic_or_str);
			offset += sizeof(generic_or_str) - 1;
		}
		first = FALSE;
		strcat(&line[offset], ib_mad_status_str_no_resources);
		offset += sizeof(ib_mad_status_str_no_resources) - 1;
	}
	if ((status & IB_MAD_STATUS_CLASS_MASK) == IB_SA_MAD_STATUS_REQ_INVALID) {
		if (!first) {
			strcat(&line[offset], generic_or_str);
			offset += sizeof(generic_or_str) - 1;
		}
		first = FALSE;
		strcat(&line[offset], ib_mad_status_str_req_invalid);
		offset += sizeof(ib_mad_status_str_req_invalid) - 1;
	}
	if ((status & IB_MAD_STATUS_CLASS_MASK) == IB_SA_MAD_STATUS_NO_RECORDS) {
		if (!first) {
			strcat(&line[offset], generic_or_str);
			offset += sizeof(generic_or_str) - 1;
		}
		first = FALSE;
		strcat(&line[offset], ib_mad_status_str_no_records);
		offset += sizeof(ib_mad_status_str_no_records) - 1;
	}
	if ((status & IB_MAD_STATUS_CLASS_MASK) ==
	    IB_SA_MAD_STATUS_TOO_MANY_RECORDS) {
		if (!first) {
			strcat(&line[offset], generic_or_str);
			offset += sizeof(generic_or_str) - 1;
		}
		first = FALSE;
		strcat(&line[offset], ib_mad_status_str_too_many_records);
		offset += sizeof(ib_mad_status_str_too_many_records) - 1;
	}
	if ((status & IB_MAD_STATUS_CLASS_MASK) == IB_SA_MAD_STATUS_INVALID_GID) {
		if (!first) {
			strcat(&line[offset], generic_or_str);
			offset += sizeof(generic_or_str) - 1;
		}
		first = FALSE;
		strcat(&line[offset], ib_mad_status_str_invalid_gid);
		offset += sizeof(ib_mad_status_str_invalid_gid) - 1;
	}
	if ((status & IB_MAD_STATUS_CLASS_MASK) == IB_SA_MAD_STATUS_INSUF_COMPS) {
		if (!first) {
			strcat(&line[offset], generic_or_str);
			offset += sizeof(generic_or_str) - 1;
		}
		first = FALSE;
		strcat(&line[offset], ib_mad_status_str_insuf_comps);
		offset += sizeof(ib_mad_status_str_insuf_comps) - 1;
	}

	return (line);
}

/**********************************************************************
 **********************************************************************/
void subnet_construct(IN subnet_t * const p_subn)
{
	cl_qmap_init(&p_subn->link_tbl);
	cl_qmap_init(&p_subn->node_lid_tbl);
	cl_qmap_init(&p_subn->node_guid_tbl);
	cl_qmap_init(&p_subn->mgrp_mlid_tbl);

	/* NO WAY TO HAVE UNIQUE PORT BY LID OR GUID */
	/* cl_qmap_init( &p_subn->port_lid_tbl ); */
	/* cl_qmap_init( &p_subn->port_guid_tbl ); */

	/* port key is a lid and num pair */
	cl_qmap_init(&p_subn->port_key_tbl);
	cl_qmap_init(&p_subn->path_tbl);
}

/**********************************************************************
 **********************************************************************/
cl_status_t subnet_init(IN subnet_t * const p_subn)
{
	cl_status_t status = IB_SUCCESS;

	subnet_construct(p_subn);

	return (status);
}

/**********************************************************************
 **********************************************************************/
void osmtest_construct(IN osmtest_t * const p_osmt)
{
	memset(p_osmt, 0, sizeof(*p_osmt));
	osm_log_construct(&p_osmt->log);
	subnet_construct(&p_osmt->exp_subn);
}

/**********************************************************************
 **********************************************************************/
void osmtest_destroy(IN osmtest_t * const p_osmt)
{
	cl_map_item_t *p_item, *p_next_item;

	/* Currently there is a problem with IBAL exit flow - memory overrun,
	   so bypass vendor deletion - it will be cleaned by the Windows OS */
#ifndef __WIN__
	if (p_osmt->p_vendor)
		osm_vendor_delete(&p_osmt->p_vendor);
#endif

	cl_qpool_destroy(&p_osmt->port_pool);
	cl_qpool_destroy(&p_osmt->node_pool);

	/* destroy the qmap tables */
	p_next_item = cl_qmap_head(&p_osmt->exp_subn.link_tbl);
	while (p_next_item != cl_qmap_end(&p_osmt->exp_subn.link_tbl)) {
		p_item = p_next_item;
		p_next_item = cl_qmap_next(p_item);
		free(p_item);
	}
	p_next_item = cl_qmap_head(&p_osmt->exp_subn.mgrp_mlid_tbl);
	while (p_next_item != cl_qmap_end(&p_osmt->exp_subn.mgrp_mlid_tbl)) {
		p_item = p_next_item;
		p_next_item = cl_qmap_next(p_item);
		free(p_item);
	}
	p_next_item = cl_qmap_head(&p_osmt->exp_subn.node_guid_tbl);
	while (p_next_item != cl_qmap_end(&p_osmt->exp_subn.node_guid_tbl)) {
		p_item = p_next_item;
		p_next_item = cl_qmap_next(p_item);
		free(p_item);
	}

	p_next_item = cl_qmap_head(&p_osmt->exp_subn.node_lid_tbl);
	while (p_next_item != cl_qmap_end(&p_osmt->exp_subn.node_lid_tbl)) {
		p_item = p_next_item;
		p_next_item = cl_qmap_next(p_item);
		free(p_item);
	}

	p_next_item = cl_qmap_head(&p_osmt->exp_subn.path_tbl);
	while (p_next_item != cl_qmap_end(&p_osmt->exp_subn.path_tbl)) {
		p_item = p_next_item;
		p_next_item = cl_qmap_next(p_item);
		free(p_item);
	}
	p_next_item = cl_qmap_head(&p_osmt->exp_subn.port_key_tbl);
	while (p_next_item != cl_qmap_end(&p_osmt->exp_subn.port_key_tbl)) {
		p_item = p_next_item;
		p_next_item = cl_qmap_next(p_item);
		free(p_item);
	}

	osm_log_destroy(&p_osmt->log);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
osmtest_init(IN osmtest_t * const p_osmt,
	     IN const osmtest_opt_t * const p_opt,
	     IN const osm_log_level_t log_flags)
{
	ib_api_status_t status;

	/* Can't use log macros here, since we're initializing the log. */
	osmtest_construct(p_osmt);

	status = osm_log_init_v2(&p_osmt->log, p_opt->force_log_flush,
				 0x0001, p_opt->log_file, 0, TRUE);
	if (status != IB_SUCCESS)
		return (status);

	/* but we do not want any extra stuff here */
	osm_log_set_level(&p_osmt->log, log_flags);

	OSM_LOG(&p_osmt->log, OSM_LOG_FUNCS, "[\n");

	p_osmt->opt = *p_opt;

	status = cl_qpool_init(&p_osmt->node_pool, POOL_MIN_ITEMS, 0,
			       POOL_MIN_ITEMS, sizeof(node_t), NULL, NULL,
			       NULL);
	CL_ASSERT(status == CL_SUCCESS);

	status = cl_qpool_init(&p_osmt->port_pool, POOL_MIN_ITEMS, 0,
			       POOL_MIN_ITEMS, sizeof(port_t), NULL, NULL,
			       NULL);
	CL_ASSERT(status == CL_SUCCESS);

	p_osmt->p_vendor = osm_vendor_new(&p_osmt->log,
					  p_opt->transaction_timeout);

	if (p_osmt->p_vendor == NULL) {
		status = IB_INSUFFICIENT_RESOURCES;
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0001: "
			"Unable to allocate vendor object");
		status = IB_ERROR;
		goto Exit;
	}

	osm_mad_pool_construct(&p_osmt->mad_pool);
	status = osm_mad_pool_init(&p_osmt->mad_pool);
	if (status != IB_SUCCESS)
		goto Exit;

Exit:
	OSM_LOG(&p_osmt->log, OSM_LOG_FUNCS, "]\n");
	return (status);
}

/**********************************************************************
 **********************************************************************/
void osmtest_query_res_cb(IN osmv_query_res_t * p_rec)
{
	osmtest_req_context_t *const p_ctxt =
	    (osmtest_req_context_t *) p_rec->query_context;
	osmtest_t *const p_osmt = p_ctxt->p_osmt;

	OSM_LOG_ENTER(&p_osmt->log);

	p_ctxt->result = *p_rec;

	if (p_rec->status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0003: "
			"Error on query (%s)\n", ib_get_err_str(p_rec->status));
	}

	OSM_LOG_EXIT(&p_osmt->log);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
osmtest_get_all_recs(IN osmtest_t * const p_osmt,
		     IN ib_net16_t const attr_id,
		     IN size_t const attr_size,
		     IN OUT osmtest_req_context_t * const p_context)
{
	ib_api_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG, "Getting all %s records\n",
		ib_get_sa_attr_str(attr_id));

	/*
	 * Do a blocking query for all <attr_id> records in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));

	p_context->p_osmt = p_osmt;
	user.attr_id = attr_id;
	user.attr_offset = cl_ntoh16((uint16_t) (attr_size >> 3));

	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0004: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = p_context->result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0064: "
			"ib_query failed (%s)\n", ib_get_err_str(status));

		if (status == IB_REMOTE_ERROR) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(osm_madw_get_mad_ptr
						      (p_context->result.
						       p_result_madw)));
		}
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t osmtest_validate_sa_class_port_info(IN osmtest_t * const p_osmt)
{
	ib_api_status_t status = IB_SUCCESS;
	osmv_query_req_t req;
	ib_class_port_info_t *p_cpi;
	osmtest_req_context_t context;
	osmtest_req_context_t *p_context = &context;
	ib_sa_mad_t *p_resp_sa_madp;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Getting ClassPortInfo\n");

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));

	p_context->p_osmt = p_osmt;
	req.query_type = OSMV_QUERY_CLASS_PORT_INFO;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = 0;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0065: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = p_context->result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0070: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		if (status == IB_REMOTE_ERROR) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(osm_madw_get_mad_ptr
						      (p_context->result.
						       p_result_madw)));
		}
		goto Exit;
	}

	/* ok we got it so please print it out */
	p_resp_sa_madp =
	    (ib_sa_mad_t *) osm_madw_get_mad_ptr(context.result.p_result_madw);
	p_cpi =
	    (ib_class_port_info_t *) ib_sa_mad_get_payload_ptr(p_resp_sa_madp);

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"\n-----------------------------\n"
		"SA Class Port Info:\n"
		" base_ver:%u\n"
		" class_ver:%u\n"
		" cap_mask:0x%X\n"
		" cap_mask2:0x%X\n"
		" resp_time_val:0x%X\n"
		"-----------------------------\n",
		p_cpi->base_ver, p_cpi->class_ver, cl_ntoh16(p_cpi->cap_mask),
		ib_class_cap_mask2(p_cpi), ib_class_resp_time_val(p_cpi));

Exit:
#if 0
	if (context.result.p_result_madw != NULL) {
		osm_mad_pool_put(&p_osmt->mad_pool,
				 context.result.p_result_madw);
		context.result.p_result_madw = NULL;
	}
#endif

	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
osmtest_get_node_rec(IN osmtest_t * const p_osmt,
		     IN ib_net64_t const node_guid,
		     IN OUT osmtest_req_context_t * const p_context)
{
	ib_api_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;
	ib_node_record_t record;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Getting node record for 0x%016" PRIx64 "\n",
		cl_ntoh64(node_guid));

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));
	memset(&record, 0, sizeof(record));

	record.node_info.node_guid = node_guid;

	p_context->p_osmt = p_osmt;
	user.comp_mask = IB_NR_COMPMASK_NODEGUID;
	user.attr_id = IB_MAD_ATTR_NODE_RECORD;
	user.attr_offset = cl_ntoh16((uint16_t) (sizeof(record) >> 3));
	user.p_attr = &record;

	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0071: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = p_context->result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0072: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		if (status == IB_REMOTE_ERROR) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(osm_madw_get_mad_ptr
						      (p_context->result.
						       p_result_madw)));
		}
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 * Get a node record by node LID
 **********************************************************************/
ib_api_status_t
osmtest_get_node_rec_by_lid(IN osmtest_t * const p_osmt,
			    IN ib_net16_t const lid,
			    IN OUT osmtest_req_context_t * const p_context)
{
	ib_api_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;
	ib_node_record_t record;
	ib_mad_t *p_mad;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Getting node record for LID 0x%02X\n", cl_ntoh16(lid));

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));
	memset(&record, 0, sizeof(record));

	record.lid = lid;

	p_context->p_osmt = p_osmt;
	user.comp_mask = IB_NR_COMPMASK_LID;
	user.attr_id = IB_MAD_ATTR_NODE_RECORD;
	user.attr_offset = cl_ntoh16((uint16_t) (sizeof(record) >> 3));
	user.p_attr = &record;

	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0073: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = p_context->result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0074: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		if (status == IB_REMOTE_ERROR) {
			p_mad =
			    osm_madw_get_mad_ptr(p_context->result.
						 p_result_madw);
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(p_mad));

			status =
			    (ib_net16_t) (p_mad->status & IB_SMP_STATUS_MASK);
		}
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_get_path_rec_by_guid_pair(IN osmtest_t * const p_osmt,
				  IN ib_net64_t sguid,
				  IN ib_net64_t dguid,
				  IN osmtest_req_context_t * p_context)
{
	cl_status_t status = IB_SUCCESS;
	osmv_query_req_t req;
	osmv_guid_pair_t guid_pair;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&req, 0, sizeof(req));
	memset(p_context, 0, sizeof(*p_context));

	p_context->p_osmt = p_osmt;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;

	req.query_type = OSMV_QUERY_PATH_REC_BY_PORT_GUIDS;

	guid_pair.dest_guid = dguid;
	guid_pair.src_guid = sguid;

	req.p_query_input = &guid_pair;
	req.sm_key = 0;

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Query for path from 0x%" PRIx64 " to 0x%" PRIx64 "\n",
		sguid, dguid);

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0063: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = (*p_context).result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0066: "
			"ib_query failed (%s)\n", ib_get_err_str(status));

		if (status == IB_REMOTE_ERROR) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(osm_madw_get_mad_ptr
						      ((*p_context).result.
						       p_result_madw)));
		}
		goto Exit;
	}

Exit:

	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_get_path_rec_by_gid_pair(IN osmtest_t * const p_osmt,
				 IN ib_gid_t sgid,
				 IN ib_gid_t dgid,
				 IN osmtest_req_context_t * p_context)
{
	cl_status_t status = IB_SUCCESS;
	osmv_query_req_t req;
	osmv_gid_pair_t gid_pair;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&req, 0, sizeof(req));
	memset(p_context, 0, sizeof(*p_context));

	p_context->p_osmt = p_osmt;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;

	req.query_type = OSMV_QUERY_PATH_REC_BY_GIDS;

	gid_pair.dest_gid = dgid;
	gid_pair.src_gid = sgid;

	req.p_query_input = &gid_pair;
	req.sm_key = 0;

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Query for path from 0x%016" PRIx64 " 0x%016" PRIx64
		" to 0x%016" PRIx64 " 0x%016" PRIx64 "\n", sgid.unicast.prefix,
		sgid.unicast.interface_id, dgid.unicast.prefix,
		dgid.unicast.interface_id);

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 006A: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = (*p_context).result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 006B: "
			"ib_query failed (%s)\n", ib_get_err_str(status));

		if (status == IB_REMOTE_ERROR) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(osm_madw_get_mad_ptr
						      ((*p_context).result.
						       p_result_madw)));
		}
		goto Exit;
	}

Exit:

	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

#if defined (VENDOR_RMPP_SUPPORT) && defined (DUAL_SIDED_RMPP)
/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_get_multipath_rec(IN osmtest_t * const p_osmt,
			  IN osmv_multipath_req_t * p_request,
			  IN osmtest_req_context_t * p_context)
{
	cl_status_t status = IB_SUCCESS;
	osmv_query_req_t req;

	OSM_LOG_ENTER(&p_osmt->log);

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));

	p_context->p_osmt = p_osmt;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;

	req.query_type = OSMV_QUERY_MULTIPATH_REC;

	req.p_query_input = p_request;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0068: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = p_context->result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0069: "
			"ib_query failed (%s)\n", ib_get_err_str(status));

		if (status == IB_REMOTE_ERROR) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(osm_madw_get_mad_ptr
						      (p_context->result.
						       p_result_madw)));
		}
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}
#endif

/**********************************************************************
 **********************************************************************/
ib_api_status_t
osmtest_get_port_rec(IN osmtest_t * const p_osmt,
		     IN ib_net16_t const lid,
		     IN OUT osmtest_req_context_t * const p_context)
{
	ib_api_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;
	ib_portinfo_record_t record;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
		"Getting PortInfoRecord for port with LID 0x%X\n",
		cl_ntoh16(lid));

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));
	memset(&record, 0, sizeof(record));

	record.lid = lid;

	p_context->p_osmt = p_osmt;
	user.comp_mask = IB_PIR_COMPMASK_LID;
	user.attr_id = IB_MAD_ATTR_PORTINFO_RECORD;
	user.attr_offset = cl_ntoh16((uint16_t) (sizeof(record) >> 3));
	user.p_attr = &record;

	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0075: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = p_context->result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0076: "
			"ib_query failed (%s)\n", ib_get_err_str(status));

		if (status == IB_REMOTE_ERROR) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(osm_madw_get_mad_ptr
						      (p_context->result.
						       p_result_madw)));
		}
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
osmtest_get_port_rec_by_num(IN osmtest_t * const p_osmt,
			    IN ib_net16_t const lid,
			    IN uint8_t const port_num,
			    IN OUT osmtest_req_context_t * const p_context)
{
	ib_api_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;
	ib_portinfo_record_t record;
	ib_mad_t *p_mad;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
		"Getting PortInfoRecord for port with LID 0x%X Num:0x%X\n",
		cl_ntoh16(lid), port_num);

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));
	memset(&record, 0, sizeof(record));

	record.lid = lid;
	record.port_num = port_num;
	user.p_attr = &record;

	p_context->p_osmt = p_osmt;

	req.query_type = OSMV_QUERY_PORT_REC_BY_LID_AND_NUM;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0077: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = p_context->result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0078: "
			"ib_query failed (%s)\n", ib_get_err_str(status));

		if (status == IB_REMOTE_ERROR) {
			p_mad =
			    osm_madw_get_mad_ptr(p_context->result.
						 p_result_madw);
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(p_mad));
			status =
			    (ib_net16_t) (p_mad->status & IB_SMP_STATUS_MASK);
		}
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
osmtest_stress_port_recs_large(IN osmtest_t * const p_osmt,
			       OUT uint32_t * const p_num_recs,
			       OUT uint32_t * const p_num_queries)
{
	osmtest_req_context_t context;
	ib_portinfo_record_t *p_rec;
	uint32_t i;
	cl_status_t status;
	uint32_t num_recs = 0;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&context, 0, sizeof(context));
	/*
	 * Do a blocking query for all PortInfoRecords in the subnet.
	 */
	status = osmtest_get_all_recs(p_osmt, IB_MAD_ATTR_PORTINFO_RECORD,
				      sizeof(*p_rec), &context);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0006: "
			"osmtest_get_all_recs failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	/*
	 * Populate the database with the received records.
	 */
	num_recs = context.result.result_cnt;
	*p_num_recs += num_recs;
	++*p_num_queries;

	if (osm_log_is_active(&p_osmt->log, OSM_LOG_VERBOSE)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
			"Received %u records\n", num_recs);

		for (i = 0; i < num_recs; i++) {
			p_rec =
			    osmv_get_query_portinfo_rec(context.result.
							p_result_madw, i);
			osm_dump_portinfo_record(&p_osmt->log, p_rec,
						 OSM_LOG_VERBOSE);
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
ib_api_status_t
osmtest_stress_node_recs_large(IN osmtest_t * const p_osmt,
			       OUT uint32_t * const p_num_recs,
			       OUT uint32_t * const p_num_queries)
{
	osmtest_req_context_t context;
	ib_node_record_t *p_rec;
	uint32_t i;
	cl_status_t status;
	uint32_t num_recs = 0;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&context, 0, sizeof(context));

	/*
	 * Do a blocking query for all NodeRecords in the subnet.
	 */
	status = osmtest_get_all_recs(p_osmt, IB_MAD_ATTR_NODE_RECORD,
				      sizeof(*p_rec), &context);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0007: "
			"osmtest_get_all_recs failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	/*
	 * Populate the database with the received records.
	 */
	num_recs = context.result.result_cnt;
	*p_num_recs += num_recs;
	++*p_num_queries;

	if (osm_log_is_active(&p_osmt->log, OSM_LOG_VERBOSE)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
			"Received %u records\n", num_recs);

		for (i = 0; i < num_recs; i++) {
			p_rec =
			    osmv_get_query_node_rec(context.result.
						    p_result_madw, i);
			osm_dump_node_record(&p_osmt->log, p_rec,
					     OSM_LOG_VERBOSE);
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
ib_api_status_t
osmtest_stress_path_recs_large(IN osmtest_t * const p_osmt,
			       OUT uint32_t * const p_num_recs,
			       OUT uint32_t * const p_num_queries)
{
	osmtest_req_context_t context;
	ib_path_rec_t *p_rec;
	uint32_t i;
	cl_status_t status;
	uint32_t num_recs = 0;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&context, 0, sizeof(context));

	/*
	 * Do a blocking query for all PathRecords in the subnet.
	 */
	status = osmtest_get_all_recs(p_osmt, IB_MAD_ATTR_PATH_RECORD,
				      sizeof(*p_rec), &context);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0008: "
			"osmtest_get_all_recs failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	/*
	 * Populate the database with the received records.
	 */
	num_recs = context.result.result_cnt;
	*p_num_recs += num_recs;
	++*p_num_queries;

	if (osm_log_is_active(&p_osmt->log, OSM_LOG_VERBOSE)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
			"Received %u records\n", num_recs);

		for (i = 0; i < num_recs; i++) {
			p_rec =
			    osmv_get_query_path_rec(context.result.
						    p_result_madw, i);
			osm_dump_path_record(&p_osmt->log, p_rec,
					     OSM_LOG_VERBOSE);
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
ib_api_status_t
osmtest_stress_path_recs_by_guid(IN osmtest_t * const p_osmt,
				 OUT uint32_t * const p_num_recs,
				 OUT uint32_t * const p_num_queries)
{
	osmtest_req_context_t context;
	ib_path_rec_t *p_rec;
	uint32_t i;
	cl_status_t status = IB_SUCCESS;
	uint32_t num_recs = 0;
	node_t *p_src_node, *p_dst_node;
	cl_qmap_t *p_tbl;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&context, 0, sizeof(context));

	context.p_osmt = p_osmt;

	p_tbl = &p_osmt->exp_subn.node_guid_tbl;

	p_src_node = (node_t *) cl_qmap_head(p_tbl);

	/*
	 * Go over all nodes that exist in the subnet
	 * for each pair that are not switch nodes get the path record
	 */
	while (p_src_node != (node_t *) cl_qmap_end(p_tbl)) {
		p_dst_node = (node_t *) cl_qmap_head(p_tbl);

		while (p_dst_node != (node_t *) cl_qmap_end(p_tbl)) {
			/*
			 * Do a blocking query for CA to CA Path Record
			 */
			OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
				"Source : guid = 0x%" PRIx64 " type = %d"
				"Target : guid = 0x%" PRIx64 " type = %d\n",
				cl_ntoh64(p_src_node->rec.node_info.port_guid),
				p_src_node->rec.node_info.node_type,
				cl_ntoh64(p_dst_node->rec.node_info.port_guid),
				p_dst_node->rec.node_info.node_type);

			if (p_src_node->rec.node_info.node_type ==
			    IB_NODE_TYPE_CA
			    && p_dst_node->rec.node_info.node_type ==
			    IB_NODE_TYPE_CA) {
				status =
				    osmtest_get_path_rec_by_guid_pair(p_osmt,
								      p_src_node->
								      rec.
								      node_info.
								      port_guid,
								      p_dst_node->
								      rec.
								      node_info.
								      port_guid,
								      &context);

				/* In a case of TIMEOUT you still can try sending but cant count, maybe its a temporary issue */
				if (status != IB_SUCCESS) {
					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"ERR 0009: "
						"osmtest_get_path_rec_by_guid_pair failed (%s)\n",
						ib_get_err_str(status));
					if (status != IB_TIMEOUT)
						goto Exit;
				} else {
					/* we might have received several records */
					num_recs = context.result.result_cnt;
					/*
					 * Populate the database with the received records.
					 */
					*p_num_recs += num_recs;
					++*p_num_queries;
					OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
						"Received %u records\n", num_recs);
					/* Dont waste time if not VERBOSE and above */
					if (p_osmt->log.level & OSM_LOG_VERBOSE) {
						for (i = 0; i < num_recs; i++) {
							p_rec =
							    osmv_get_query_path_rec
							    (context.result.
							     p_result_madw, i);
							osm_dump_path_record
							    (&p_osmt->log,
							     p_rec,
							     OSM_LOG_VERBOSE);
						}
					}
				}
				if (context.result.p_result_madw != NULL) {
					osm_mad_pool_put(&p_osmt->mad_pool,
							 context.result.
							 p_result_madw);
					context.result.p_result_madw = NULL;
				}
			}
			/* next one please */
			p_dst_node =
			    (node_t *) cl_qmap_next(&p_dst_node->map_item);
		}

		p_src_node = (node_t *) cl_qmap_next(&p_src_node->map_item);
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
osmtest_stress_port_recs_small(IN osmtest_t * const p_osmt,
			       OUT uint32_t * const p_num_recs,
			       OUT uint32_t * const p_num_queries)
{
	osmtest_req_context_t context;
	ib_portinfo_record_t *p_rec;
	uint32_t i;
	cl_status_t status;
	uint32_t num_recs = 0;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&context, 0, sizeof(context));

	/*
	 * Do a blocking query for our own PortInfoRecord in the subnet.
	 */
	status = osmtest_get_port_rec(p_osmt,
				      cl_ntoh16(p_osmt->local_port.lid),
				      &context);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0010: "
			"osmtest_get_port_rec failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	/*
	 * Populate the database with the received records.
	 */
	num_recs = context.result.result_cnt;
	*p_num_recs += num_recs;
	++*p_num_queries;

	if (osm_log_is_active(&p_osmt->log, OSM_LOG_VERBOSE)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
			"Received %u records\n", num_recs);

		for (i = 0; i < num_recs; i++) {
			p_rec =
			    osmv_get_query_portinfo_rec(context.result.
							p_result_madw, i);
			osm_dump_portinfo_record(&p_osmt->log, p_rec,
						 OSM_LOG_VERBOSE);
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
ib_api_status_t
osmtest_get_local_port_lmc(IN osmtest_t * const p_osmt,
			   IN ib_net16_t lid, OUT uint8_t * const p_lmc)
{
	osmtest_req_context_t context;
	ib_portinfo_record_t *p_rec;
	uint32_t i;
	cl_status_t status;
	uint32_t num_recs = 0;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&context, 0, sizeof(context));

	/*
	 * Do a blocking query for our own PortInfoRecord in the subnet.
	 */
	status = osmtest_get_port_rec(p_osmt, cl_ntoh16(lid), &context);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 001A: "
			"osmtest_get_port_rec failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	num_recs = context.result.result_cnt;

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Received %u records\n", num_recs);

	for (i = 0; i < num_recs; i++) {
		p_rec =
		    osmv_get_query_portinfo_rec(context.result.p_result_madw,
						i);
		osm_dump_portinfo_record(&p_osmt->log, p_rec, OSM_LOG_VERBOSE);
		if (p_lmc) {
			*p_lmc = ib_port_info_get_lmc(&p_rec->port_info);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG, "LMC %d\n", *p_lmc);
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
 * Use a wrong SM_Key in a simple port query and report success if
 * failed.
 **********************************************************************/
ib_api_status_t osmtest_wrong_sm_key_ignored(IN osmtest_t * const p_osmt)
{
	ib_api_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;
	ib_portinfo_record_t record;
	osmtest_req_context_t context;
	osmtest_req_context_t *p_context = &context;
	uint8_t port_num = 1;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
		"Trying PortInfoRecord for port with LID 0x%X Num:0x%X\n",
		p_osmt->local_port.sm_lid, port_num);

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));
	memset(&record, 0, sizeof(record));

	record.lid = p_osmt->local_port.sm_lid;
	record.port_num = port_num;
	user.p_attr = &record;

	p_context->p_osmt = p_osmt;

	req.query_type = OSMV_QUERY_PORT_REC_BY_LID_AND_NUM;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.sm_key = 9999;
	context.result.p_result_madw = NULL;

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmv_query_sa(p_osmt->h_bind, &req);
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	/* since we use a wrong sm_key we should get a timeout */
	if (status != IB_TIMEOUT) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0011: "
			"Did not get a timeout but got (%s)\n",
			ib_get_err_str(status));
		if (status == IB_SUCCESS) {
			/* assign some error value to status, since IB_SUCCESS is a bad rc */
			status = IB_ERROR;
		}
		goto Exit;
	} else {
		status = IB_SUCCESS;
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
static ib_api_status_t
osmtest_write_port_info(IN osmtest_t * const p_osmt,
			IN FILE * fh,
			IN const ib_portinfo_record_t * const p_rec)
{
	int result;
	cl_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(&p_osmt->log);

	result = fprintf(fh,
			 "DEFINE_PORT\n"
			 "lid                     0x%X\n"
			 "port_num                0x%X\n"
			 "m_key                   0x%016" PRIx64 "\n"
			 "subnet_prefix           0x%016" PRIx64 "\n"
			 "base_lid                0x%X\n"
			 "master_sm_base_lid      0x%X\n"
			 "capability_mask         0x%X\n"
			 "diag_code               0x%X\n"
			 "m_key_lease_period      0x%X\n"
			 "local_port_num          0x%X\n"
			 "link_width_enabled      0x%X\n"
			 "link_width_supported    0x%X\n"
			 "link_width_active       0x%X\n"
			 "link_speed_supported    0x%X\n"
			 "port_state              %s\n"
			 "state_info2             0x%X\n"
			 "mpb                     0x%X\n"
			 "lmc                     0x%X\n"
			 "link_speed              0x%X\n"
			 "mtu_smsl                0x%X\n"
			 "vl_cap                  0x%X\n"
			 "vl_high_limit           0x%X\n"
			 "vl_arb_high_cap         0x%X\n"
			 "vl_arb_low_cap          0x%X\n"
			 "mtu_cap                 0x%X\n"
			 "vl_stall_life           0x%X\n"
			 "vl_enforce              0x%X\n"
			 "m_key_violations        0x%X\n"
			 "p_key_violations        0x%X\n"
			 "q_key_violations        0x%X\n"
			 "guid_cap                0x%X\n"
			 "subnet_timeout          0x%X\n"
			 "resp_time_value         0x%X\n"
			 "error_threshold         0x%X\n"
			 "END\n\n",
			 cl_ntoh16(p_rec->lid),
			 p_rec->port_num,
			 cl_ntoh64(p_rec->port_info.m_key),
			 cl_ntoh64(p_rec->port_info.subnet_prefix),
			 cl_ntoh16(p_rec->port_info.base_lid),
			 cl_ntoh16(p_rec->port_info.master_sm_base_lid),
			 cl_ntoh32(p_rec->port_info.capability_mask),
			 cl_ntoh16(p_rec->port_info.diag_code),
			 cl_ntoh16(p_rec->port_info.m_key_lease_period),
			 p_rec->port_info.local_port_num,
			 p_rec->port_info.link_width_enabled,
			 p_rec->port_info.link_width_supported,
			 p_rec->port_info.link_width_active,
			 ib_port_info_get_link_speed_sup(&p_rec->port_info),
			 ib_get_port_state_str(ib_port_info_get_port_state
					       (&p_rec->port_info)),
			 p_rec->port_info.state_info2,
			 ib_port_info_get_mpb(&p_rec->port_info),
			 ib_port_info_get_lmc(&p_rec->port_info),
			 p_rec->port_info.link_speed, p_rec->port_info.mtu_smsl,
			 p_rec->port_info.vl_cap,
			 p_rec->port_info.vl_high_limit,
			 p_rec->port_info.vl_arb_high_cap,
			 p_rec->port_info.vl_arb_low_cap,
			 p_rec->port_info.mtu_cap,
			 p_rec->port_info.vl_stall_life,
			 p_rec->port_info.vl_enforce,
			 cl_ntoh16(p_rec->port_info.m_key_violations),
			 cl_ntoh16(p_rec->port_info.p_key_violations),
			 cl_ntoh16(p_rec->port_info.q_key_violations),
			 p_rec->port_info.guid_cap,
			 ib_port_info_get_timeout(&p_rec->port_info),
			 p_rec->port_info.resp_time_value,
			 p_rec->port_info.error_threshold);

	if (result < 0) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0161: "
			"Write failed\n");
		status = IB_ERROR;
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_write_path_info(IN osmtest_t * const p_osmt,
			IN FILE * fh, IN const ib_path_rec_t * const p_rec)
{
	int result;
	cl_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(&p_osmt->log);

	result = fprintf(fh,
			 "DEFINE_PATH\n"
			 "dgid                    0x%016" PRIx64 " 0x%016"
			 PRIx64 "\nsgid                    0x%016" PRIx64
			 " 0x%016" PRIx64 "\ndlid                    0x%X\n"
			 "slid                    0x%X\n"
			 "# hop_flow_raw          0x%X\n"
			 "# tclass                0x%X\n"
			 "# num_path              0x%X\n"
			 "pkey                    0x%X\n"
			 "# sl                    0x%X\n"
			 "# qos_class             0x%X\n"
			 "# mtu                   0x%X\n"
			 "# rate                  0x%X\n"
			 "# pkt_life              0x%X\n"
			 "# preference            0x%X\n" "END\n\n",
			 cl_ntoh64(p_rec->dgid.unicast.prefix),
			 cl_ntoh64(p_rec->dgid.unicast.interface_id),
			 cl_ntoh64(p_rec->sgid.unicast.prefix),
			 cl_ntoh64(p_rec->sgid.unicast.interface_id),
			 cl_ntoh16(p_rec->dlid), cl_ntoh16(p_rec->slid),
			 cl_ntoh32(p_rec->hop_flow_raw), p_rec->tclass,
			 p_rec->num_path, cl_ntoh16(p_rec->pkey),
			 ib_path_rec_sl(p_rec), ib_path_rec_qos_class(p_rec),
			 p_rec->mtu, p_rec->rate, p_rec->pkt_life,
			 p_rec->preference);

	if (result < 0) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0162: "
			"Write failed\n");
		status = IB_ERROR;
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_write_node_info(IN osmtest_t * const p_osmt,
			IN FILE * fh, IN const ib_node_record_t * const p_rec)
{
	int result;
	cl_status_t status = IB_SUCCESS;
	char desc[IB_NODE_DESCRIPTION_SIZE + 1];

	OSM_LOG_ENTER(&p_osmt->log);

	memcpy(desc, p_rec->node_desc.description, IB_NODE_DESCRIPTION_SIZE);
	desc[IB_NODE_DESCRIPTION_SIZE] = '\0';

	result = fprintf(fh,
			 "DEFINE_NODE\n"
			 "lid                     0x%X\n"
			 "base_version            0x%X\n"
			 "class_version           0x%X\n"
			 "node_type               0x%X # (%s)\n"
			 "num_ports               0x%X\n"
			 "sys_guid                0x%016" PRIx64 "\n"
			 "node_guid               0x%016" PRIx64 "\n"
			 "port_guid               0x%016" PRIx64 "\n"
			 "partition_cap           0x%X\n"
			 "device_id               0x%X\n"
			 "revision                0x%X\n"
			 "# port_num              0x%X\n"
			 "# vendor_id             0x%X\n"
			 "# node_desc             %s\n"
			 "END\n\n",
			 cl_ntoh16(p_rec->lid),
			 p_rec->node_info.base_version,
			 p_rec->node_info.class_version,
			 p_rec->node_info.node_type,
			 ib_get_node_type_str(p_rec->node_info.node_type),
			 p_rec->node_info.num_ports,
			 cl_ntoh64(p_rec->node_info.sys_guid),
			 cl_ntoh64(p_rec->node_info.node_guid),
			 cl_ntoh64(p_rec->node_info.port_guid),
			 cl_ntoh16(p_rec->node_info.partition_cap),
			 cl_ntoh16(p_rec->node_info.device_id),
			 cl_ntoh32(p_rec->node_info.revision),
			 ib_node_info_get_local_port_num(&p_rec->node_info),
			 cl_ntoh32(ib_node_info_get_vendor_id
				   (&p_rec->node_info)), desc);

	if (result < 0) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0163: "
			"Write failed\n");
		status = IB_ERROR;
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_write_link(IN osmtest_t * const p_osmt,
		   IN FILE * fh, IN const ib_link_record_t * const p_rec)
{
	int result;
	cl_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(&p_osmt->log);

	result = fprintf(fh,
			 "DEFINE_LINK\n"
			 "from_lid                0x%X\n"
			 "from_port_num           0x%X\n"
			 "to_port_num             0x%X\n"
			 "to_lid                  0x%X\n"
			 "END\n\n",
			 cl_ntoh16(p_rec->from_lid),
			 p_rec->from_port_num,
			 p_rec->to_port_num, cl_ntoh16(p_rec->to_lid));

	if (result < 0) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0164: "
			"Write failed\n");
		status = IB_ERROR;
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_write_all_link_recs(IN osmtest_t * const p_osmt, IN FILE * fh)
{
	osmtest_req_context_t context;
	const ib_link_record_t *p_rec;
	uint32_t i;
	cl_status_t status;
	size_t num_recs;
	int result;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&context, 0, sizeof(context));

	/*
	 * Do a blocking query for all NodeRecords in the subnet.
	 */
	status = osmtest_get_all_recs(p_osmt, IB_MAD_ATTR_LINK_RECORD,
				      sizeof(*p_rec), &context);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0165: "
			"osmtest_get_all_recs failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	/*
	 * Write the received records out to the file.
	 */
	num_recs = context.result.result_cnt;

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Received %zu records\n", num_recs);

	result = fprintf(fh, "#\n" "# Link Records\n" "#\n");
	if (result < 0) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0166: "
			"Write failed\n");
		status = IB_ERROR;
		goto Exit;
	}

	for (i = 0; i < num_recs; i++) {
		p_rec =
		    (ib_link_record_t *) osmv_get_query_result(context.result.
							       p_result_madw,
							       i);

		osmtest_write_link(p_osmt, fh, p_rec);
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
static ib_api_status_t
osmtest_get_path_rec_by_lid_pair(IN osmtest_t * const p_osmt,
				 IN ib_net16_t slid,
				 IN ib_net16_t dlid,
				 IN osmtest_req_context_t * p_context)
{
	cl_status_t status = IB_SUCCESS;
	osmv_query_req_t req;
	osmv_lid_pair_t lid_pair;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&req, 0, sizeof(req));
	memset(p_context, 0, sizeof(*p_context));

	p_context->p_osmt = p_osmt;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;

	req.query_type = OSMV_QUERY_PATH_REC_BY_LIDS;

	lid_pair.dest_lid = dlid;
	lid_pair.src_lid = slid;

	req.p_query_input = &lid_pair;
	req.sm_key = 0;

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Query for path from 0x%X to 0x%X\n", slid, dlid);
	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0053: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = (*p_context).result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0067: "
			"ib_query failed (%s)\n", ib_get_err_str(status));

		if (status == IB_REMOTE_ERROR) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(osm_madw_get_mad_ptr
						      ((*p_context).result.
						       p_result_madw)));
		}
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

#ifdef VENDOR_RMPP_SUPPORT
/**********************************************************************
 * ASSUMES RMPP
 **********************************************************************/
static ib_api_status_t
osmtest_write_all_node_recs(IN osmtest_t * const p_osmt, IN FILE * fh)
{
	osmtest_req_context_t context;
	const ib_node_record_t *p_rec;
	uint32_t i;
	cl_status_t status;
	size_t num_recs;
	int result;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&context, 0, sizeof(context));

	/*
	 * Do a blocking query for all NodeRecords in the subnet.
	 */
	status = osmtest_get_all_recs(p_osmt, IB_MAD_ATTR_NODE_RECORD,
				      sizeof(*p_rec), &context);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0022: "
			"osmtest_get_all_recs failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	/*
	 * Write the received records out to the file.
	 */
	num_recs = context.result.result_cnt;

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Received %zu records\n", num_recs);

	result = fprintf(fh, "#\n" "# Node Records\n" "#\n");
	if (result < 0) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0023: "
			"Write failed\n");
		status = IB_ERROR;
		goto Exit;
	}

	for (i = 0; i < num_recs; i++) {
		p_rec =
		    osmv_get_query_node_rec(context.result.p_result_madw, i);
		osmtest_write_node_info(p_osmt, fh, p_rec);
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
 * ASSUMES RMPP
 **********************************************************************/
static ib_api_status_t
osmtest_write_all_port_recs(IN osmtest_t * const p_osmt, IN FILE * fh)
{
	osmtest_req_context_t context;
	const ib_portinfo_record_t *p_rec;
	uint32_t i;
	cl_status_t status;
	size_t num_recs;
	int result;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&context, 0, sizeof(context));

	/*
	 * Do a blocking query for all NodeRecords in the subnet.
	 */
	status = osmtest_get_all_recs(p_osmt, IB_MAD_ATTR_PORTINFO_RECORD,
				      sizeof(*p_rec), &context);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0167: "
			"osmtest_get_all_recs failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	/*
	 * Write the received records out to the file.
	 */
	num_recs = context.result.result_cnt;

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Received %zu records\n", num_recs);

	result = fprintf(fh, "#\n" "# PortInfo Records\n" "#\n");
	if (result < 0) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0024: "
			"Write failed\n");
		status = IB_ERROR;
		goto Exit;
	}

	for (i = 0; i < num_recs; i++) {
		p_rec =
		    osmv_get_query_portinfo_rec(context.result.p_result_madw,
						i);
		osmtest_write_port_info(p_osmt, fh, p_rec);
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
 * ASSUMES RMPP
 **********************************************************************/
static ib_api_status_t
osmtest_write_all_path_recs(IN osmtest_t * const p_osmt, IN FILE * fh)
{
	osmtest_req_context_t context;
	const ib_path_rec_t *p_rec;
	uint32_t i;
	cl_status_t status;
	size_t num_recs;
	int result;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&context, 0, sizeof(context));

	/*
	 * Do a blocking query for all PathRecords in the subnet.
	 */
	status = osmtest_get_all_recs(p_osmt, IB_MAD_ATTR_PATH_RECORD,
				      sizeof(*p_rec), &context);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0025: "
			"osmtest_get_all_recs failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	/*
	 * Write the received records out to the file.
	 */
	num_recs = context.result.result_cnt;

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Received %zu records\n", num_recs);

	result = fprintf(fh, "#\n" "# Path Records\n" "#\n");
	if (result < 0) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0026: "
			"Write failed\n");
		status = IB_ERROR;
		goto Exit;
	}

	for (i = 0; i < num_recs; i++) {
		p_rec =
		    osmv_get_query_path_rec(context.result.p_result_madw, i);
		osmtest_write_path_info(p_osmt, fh, p_rec);
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

#else
/*
 * NON RMPP BASED QUERY FOR ALL NODES: BASED ON THE MAX LID GIVEN BY THE USER
 */
static ib_api_status_t
osmtest_write_all_node_recs(IN osmtest_t * const p_osmt, IN FILE * fh)
{
	osmtest_req_context_t context;
	node_t *p_node;
	node_t *p_guid_node;
	const ib_node_record_t *p_rec;
	cl_status_t status = CL_SUCCESS;
	int result;
	uint16_t lid;

	OSM_LOG_ENTER(&p_osmt->log);

	result = fprintf(fh, "#\n" "# Node Records\n" "#\n");
	if (result < 0) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0027: "
			"Write failed\n");
		status = IB_ERROR;
		goto Exit;
	}

	/*
	 * Go over all LIDs in the range 1 to max_lid and do a
	 * NodeRecord query by that lid.
	 */
	for (lid = 1; lid <= p_osmt->max_lid; lid++) {
		/* prepare the query context */
		memset(&context, 0, sizeof(context));

		status =
		    osmtest_get_node_rec_by_lid(p_osmt, cl_ntoh16(lid),
						&context);
		if (status != IB_SUCCESS) {
			if (status != IB_SA_MAD_STATUS_NO_RECORDS) {
				OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG, "ERR 0028: "
					"failed to get node info for LID:0x%02X (%s)\n",
					cl_ntoh16(lid), ib_get_err_str(status));
				goto Exit;
			} else {
				OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG, "WRN 0121: "
					"failed to get node info for LID:0x%02X (%s)\n",
					cl_ntoh16(lid), ib_get_err_str(status));
				status = IB_SUCCESS;
			}
		} else {
			/* OK we got something */
			p_rec =
			    osmv_get_query_node_rec(context.result.
						    p_result_madw, 0);
			osmtest_write_node_info(p_osmt, fh, p_rec);

			/* create a subnet object */
			p_node = node_new();
			CL_ASSERT(p_node != NULL);

			/* copy the info to the subnet node object */
			p_node->rec = *p_rec;

			cl_qmap_insert(&p_osmt->exp_subn.node_lid_tbl,
				       p_node->rec.lid, &p_node->map_item);

			p_guid_node = node_new();
			CL_ASSERT(p_guid_node != NULL);

			*p_guid_node = *p_node;

			cl_qmap_insert(&p_osmt->exp_subn.node_guid_tbl,
				       p_guid_node->rec.node_info.node_guid,
				       &p_guid_node->map_item);

		}

		if (context.result.p_result_madw != NULL) {
			osm_mad_pool_put(&p_osmt->mad_pool,
					 context.result.p_result_madw);
			context.result.p_result_madw = NULL;
		}
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

/*
 * GET ALL PORT RECORDS IN THE FABRIC -
 * one by one by using the node info received
 */
static ib_api_status_t
osmtest_write_all_port_recs(IN osmtest_t * const p_osmt, IN FILE * fh)
{
	osmtest_req_context_t context;
	const ib_node_record_t *p_node_rec;
	const ib_portinfo_record_t *p_rec;
	uint8_t port_num;
	cl_status_t status = CL_SUCCESS;
	cl_qmap_t *p_tbl;
	node_t *p_node;
	port_t *p_port;
	int result;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&context, 0, sizeof(context));

	/* print header */
	result = fprintf(fh, "#\n" "# PortInfo Records\n" "#\n");
	if (result < 0) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0029: "
			"Write failed\n");
		status = IB_ERROR;
		goto Exit;
	}

	/* use the pre-explored set of nodes */
	p_tbl = &p_osmt->exp_subn.node_lid_tbl;
	p_node = (node_t *) cl_qmap_head(p_tbl);

	/*
	 * Go over all LIDs in the range 1 to max_lid and do a
	 * NodeRecord query by that lid.
	 */
	while (p_node != (node_t *) cl_qmap_end(p_tbl)) {

		p_node_rec = &(p_node->rec);

		/* go through all ports of the node: */
		for (port_num = 0; port_num <= p_node_rec->node_info.num_ports;
		     port_num++) {
			/* prepare the query context */
			memset(&context, 0, sizeof(context));

			status = osmtest_get_port_rec_by_num(p_osmt,
							     p_node_rec->lid,
							     port_num,
							     &context);
			if (status != IB_SUCCESS) {
				if (status != IB_SA_MAD_STATUS_NO_RECORDS) {
					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"WRN 0122: "
						"Error encountered getting port info for LID:0x%04X Num:0x%02X (%s)\n",
						p_node_rec->lid, port_num,
						ib_get_err_str(status));
					goto Exit;
				} else {
					OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
						"WRN 0123: "
						"failed to get port info for LID:0x%04X Num:0x%02X (%s)\n",
						p_node_rec->lid, port_num,
						ib_get_err_str(status));
					status = IB_SUCCESS;
				}
			} else {
				/* OK we got something */
				p_rec =
				    osmv_get_query_portinfo_rec(context.result.
								p_result_madw,
								0);
				osmtest_write_port_info(p_osmt, fh, p_rec);

				/* create a subnet object */
				p_port = port_new();
				CL_ASSERT(p_port != NULL);

				/* copy the info to the subnet node object */
				p_port->rec = *p_rec;

				cl_qmap_insert(&p_osmt->exp_subn.port_key_tbl,
					       port_gen_id(p_node_rec->lid,
							   port_num),
					       &p_port->map_item);
			}

			if (context.result.p_result_madw != NULL) {
				osm_mad_pool_put(&p_osmt->mad_pool,
						 context.result.p_result_madw);
				context.result.p_result_madw = NULL;
			}
		}
		p_node = (node_t *) cl_qmap_next(&p_node->map_item);
	}

	/* we must set the exist status to avoid abort of the over all algorith */

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
 * ASSUMES NO RMPP
 **********************************************************************/
static ib_api_status_t
osmtest_write_all_path_recs(IN osmtest_t * const p_osmt, IN FILE * fh)
{
	osmtest_req_context_t context;
	const ib_path_rec_t *p_rec;
	cl_status_t status = CL_SUCCESS;
	int num_recs, i;
	cl_qmap_t *p_tbl;
	node_t *p_src_node, *p_dst_node;
	ib_api_status_t got_status = IB_SUCCESS;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&context, 0, sizeof(context));

	/*
	 * Go over all nodes that exist in the subnet
	 * for each pair that are not switch nodes get the path record
	 */

	context.p_osmt = p_osmt;

	p_tbl = &p_osmt->exp_subn.node_lid_tbl;

	p_src_node = (node_t *) cl_qmap_head(p_tbl);

	while (p_src_node != (node_t *) cl_qmap_end(p_tbl)) {
		/* HACK we use capability_mask to know diff a CA node from switch node */
		/* if(p_src_node->rec.node_info.capability_mask  ) { */
		p_dst_node = (node_t *) cl_qmap_head(p_tbl);

		while (p_dst_node != (node_t *) cl_qmap_end(p_tbl)) {
			/* HACK we use capability_mask to know diff a CA node from switch node */
			/* if (p_dst_node->rec.node_info.capability_mask) { */

			/* query for it: */
			status = osmtest_get_path_rec_by_lid_pair(p_osmt,
								  p_src_node->
								  rec.lid,
								  p_dst_node->
								  rec.lid,
								  &context);

			if (status != IB_SUCCESS) {
				OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 012D: "
					"failed to get path info from LID:0x%X To LID:0x%X (%s)\n",
					p_src_node->rec.lid,
					p_dst_node->rec.lid,
					ib_get_err_str(status));
				/* remember the first error status */
				got_status =
				    (got_status ==
				     IB_SUCCESS) ? status : got_status;
			} else {
				/* we might have received several records */
				num_recs = context.result.result_cnt;
				for (i = 0; i < num_recs; i++) {
					p_rec =
					    osmv_get_query_path_rec(context.
								    result.
								    p_result_madw,
								    i);
					osmtest_write_path_info(p_osmt, fh,
								p_rec);
				}
			}
/*  } */

			if (context.result.p_result_madw != NULL) {
				osm_mad_pool_put(&p_osmt->mad_pool,
						 context.result.p_result_madw);
				context.result.p_result_madw = NULL;
			}

			/* next one please */
			p_dst_node =
			    (node_t *) cl_qmap_next(&p_dst_node->map_item);
		}
/* } */

		p_src_node = (node_t *) cl_qmap_next(&p_src_node->map_item);
	}

	if (got_status != IB_SUCCESS)
		status = got_status;

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

#endif

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_create_inventory_file(IN osmtest_t * const p_osmt)
{
	FILE *fh;
	ib_api_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(&p_osmt->log);

	fh = fopen(p_osmt->opt.file_name, "w");
	if (fh == NULL) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0079: "
			"Unable to open inventory file (%s)\n",
			p_osmt->opt.file_name);
		status = IB_ERROR;
		goto Exit;
	}

	/* HACK: the order is important: nodes ports paths */
	status = osmtest_write_all_node_recs(p_osmt, fh);
	if (status != IB_SUCCESS)
		goto Exit;

	status = osmtest_write_all_port_recs(p_osmt, fh);
	if (status != IB_SUCCESS)
		goto Exit;

	if (!p_osmt->opt.ignore_path_records) {
		status = osmtest_write_all_path_recs(p_osmt, fh);
		if (status != IB_SUCCESS)
			goto Exit;
	}

	status = osmtest_write_all_link_recs(p_osmt, fh);
	if (status != IB_SUCCESS)
		goto Exit;

	fclose(fh);

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t osmtest_stress_large_rmpp_pr(IN osmtest_t * const p_osmt)
{
	ib_api_status_t status = IB_SUCCESS;
	uint64_t num_recs = 0;
	uint64_t num_queries = 0;
	uint32_t delta_recs;
	uint32_t delta_queries;
	uint32_t print_freq = 0;
	struct timeval start_tv, end_tv;
	long sec_diff, usec_diff;
	float ratio;

	OSM_LOG_ENTER(&p_osmt->log);
	gettimeofday(&start_tv, NULL);
	printf("-I- Start time is : %09ld:%06ld [sec:usec]\n", start_tv.tv_sec,
	       (long)start_tv.tv_usec);

	while (num_queries < STRESS_LARGE_PR_RMPP_THR) {
		delta_recs = 0;
		delta_queries = 0;

		status = osmtest_stress_path_recs_by_guid(p_osmt, &delta_recs,
							  &delta_queries);
		if (status != IB_SUCCESS)
			goto Exit;

		num_recs += delta_recs;
		num_queries += delta_queries;

		print_freq += delta_recs;
		if (print_freq > 10000) {
			gettimeofday(&end_tv, NULL);
			if (end_tv.tv_usec > start_tv.tv_usec) {
				sec_diff = end_tv.tv_sec - start_tv.tv_sec;
				usec_diff = end_tv.tv_usec - start_tv.tv_usec;
			} else {
				sec_diff = end_tv.tv_sec - start_tv.tv_sec - 1;
				usec_diff =
				    1000000 - (start_tv.tv_usec -
					       end_tv.tv_usec);
			}
			printf("-I- End time is : %09ld:%06ld [sec:usec]\n",
			       end_tv.tv_sec, (long)end_tv.tv_usec);
			printf("-I- Querying %" PRId64
			       " Path Record queries CA to CA (rmpp)\n\ttook %04ld:%06ld [sec:usec]\n",
			       num_queries, sec_diff, usec_diff);
			if (num_recs == 0)
				ratio = 0;
			else
				ratio = ((float)num_queries / (float)num_recs);
			printf("-I- Queries to Record Ratio is %" PRIu64
			       " records, %" PRIu64 " queries : %.2f \n",
			       num_recs, num_queries, ratio);
			print_freq = 0;
		}
	}

Exit:
	gettimeofday(&end_tv, NULL);
	printf("-I- End time is : %09ld:%06ld [sec:usec]\n",
	       end_tv.tv_sec, (long)end_tv.tv_usec);
	if (end_tv.tv_usec > start_tv.tv_usec) {
		sec_diff = end_tv.tv_sec - start_tv.tv_sec;
		usec_diff = end_tv.tv_usec - start_tv.tv_usec;
	} else {
		sec_diff = end_tv.tv_sec - start_tv.tv_sec - 1;
		usec_diff = 1000000 - (start_tv.tv_usec - end_tv.tv_usec);
	}

	printf("-I- Querying %" PRId64
	       " Path Record queries (rmpp) took %04ld:%06ld [sec:usec]\n",
	       num_queries, sec_diff, usec_diff);

	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t osmtest_stress_large_rmpp(IN osmtest_t * const p_osmt)
{
	ib_api_status_t status = IB_SUCCESS;
	uint64_t num_recs = 0;
	uint64_t num_queries = 0;
	uint32_t delta_recs;
	uint32_t delta_queries;
	uint32_t print_freq = 0;
	struct timeval start_tv, end_tv;
	long sec_diff, usec_diff;

	OSM_LOG_ENTER(&p_osmt->log);
	gettimeofday(&start_tv, NULL);
	printf("-I- Start time is : %09ld:%06ld [sec:usec]\n", start_tv.tv_sec,
	       (long)start_tv.tv_usec);

	while (num_queries < STRESS_LARGE_RMPP_THR) {
		delta_recs = 0;
		delta_queries = 0;

		status = osmtest_stress_node_recs_large(p_osmt, &delta_recs,
							&delta_queries);
		if (status != IB_SUCCESS)
			goto Exit;

		status = osmtest_stress_path_recs_large(p_osmt, &delta_recs,
							&delta_queries);
		if (status != IB_SUCCESS)
			goto Exit;

		status = osmtest_stress_port_recs_large(p_osmt, &delta_recs,
							&delta_queries);
		if (status != IB_SUCCESS)
			goto Exit;

		num_recs += delta_recs;
		num_queries += delta_queries;

		print_freq += delta_recs;

		if (print_freq > 100000) {
			gettimeofday(&end_tv, NULL);
			if (end_tv.tv_usec > start_tv.tv_usec) {
				sec_diff = end_tv.tv_sec - start_tv.tv_sec;
				usec_diff = end_tv.tv_usec - start_tv.tv_usec;
			} else {
				sec_diff = end_tv.tv_sec - start_tv.tv_sec - 1;
				usec_diff =
				    1000000 - (start_tv.tv_usec -
					       end_tv.tv_usec);
			}
			printf("-I- End time is : %09ld:%06ld [sec:usec]\n",
			       end_tv.tv_sec, (long)end_tv.tv_usec);
			printf("-I- Querying %" PRId64
			       " large mixed queries (rmpp) took %04ld:%06ld [sec:usec]\n",
			       num_queries, sec_diff, usec_diff);
			printf("%" PRIu64 " records, %" PRIu64 " queries\n",
			       num_recs, num_queries);
			print_freq = 0;
		}
	}

Exit:
	gettimeofday(&end_tv, NULL);
	printf("-I- End time is : %09ld:%06ld [sec:usec]\n",
	       end_tv.tv_sec, (long)end_tv.tv_usec);
	if (end_tv.tv_usec > start_tv.tv_usec) {
		sec_diff = end_tv.tv_sec - start_tv.tv_sec;
		usec_diff = end_tv.tv_usec - start_tv.tv_usec;
	} else {
		sec_diff = end_tv.tv_sec - start_tv.tv_sec - 1;
		usec_diff = 1000000 - (start_tv.tv_usec - end_tv.tv_usec);
	}

	printf("-I- Querying %" PRId64
	       " large mixed queries (rmpp) took %04ld:%06ld [sec:usec]\n",
	       num_queries, sec_diff, usec_diff);

	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t osmtest_stress_small_rmpp(IN osmtest_t * const p_osmt)
{
	ib_api_status_t status = IB_SUCCESS;
	uint64_t num_recs = 0;
	uint64_t num_queries = 0;
	uint32_t delta_recs;
	uint32_t delta_queries;
	uint32_t print_freq = 0;
	int num_timeouts = 0;
	struct timeval start_tv, end_tv;
	long sec_diff, usec_diff;

	OSM_LOG_ENTER(&p_osmt->log);
	gettimeofday(&start_tv, NULL);
	printf("-I- Start time is : %09ld:%06ld [sec:usec]\n",
	       start_tv.tv_sec, (long)start_tv.tv_usec);

	while ((num_queries < STRESS_SMALL_RMPP_THR) && (num_timeouts < 100)) {
		delta_recs = 0;
		delta_queries = 0;

		status = osmtest_stress_port_recs_small(p_osmt, &delta_recs,
							&delta_queries);
		if (status != IB_SUCCESS)
			goto Exit;

		num_recs += delta_recs;
		num_queries += delta_queries;

		print_freq += delta_recs;
		if (print_freq > 5000) {
			gettimeofday(&end_tv, NULL);
			printf("%" PRIu64 " records, %" PRIu64 " queries\n",
			       num_recs, num_queries);
			if (end_tv.tv_usec > start_tv.tv_usec) {
				sec_diff = end_tv.tv_sec - start_tv.tv_sec;
				usec_diff = end_tv.tv_usec - start_tv.tv_usec;
			} else {
				sec_diff = end_tv.tv_sec - start_tv.tv_sec - 1;
				usec_diff =
				    1000000 - (start_tv.tv_usec -
					       end_tv.tv_usec);
			}
			printf("-I- End time is : %09ld:%06ld [sec:usec]\n",
			       end_tv.tv_sec, (long)end_tv.tv_usec);
			printf("-I- Querying %" PRId64
			       " port_info queries (single mad) took %04ld:%06ld [sec:usec]\n",
			       num_queries, sec_diff, usec_diff);
			print_freq = 0;
		}
	}

Exit:
	gettimeofday(&end_tv, NULL);
	printf("-I- End time is : %09ld:%06ld [sec:usec]\n",
	       end_tv.tv_sec, (long)end_tv.tv_usec);
	if (end_tv.tv_usec > start_tv.tv_usec) {
		sec_diff = end_tv.tv_sec - start_tv.tv_sec;
		usec_diff = end_tv.tv_usec - start_tv.tv_usec;
	} else {
		sec_diff = end_tv.tv_sec - start_tv.tv_sec - 1;
		usec_diff = 1000000 - (start_tv.tv_usec - end_tv.tv_usec);
	}

	printf("-I- Querying %" PRId64
	       " port_info queries (single mad) took %04ld:%06ld [sec:usec]\n",
	       num_queries, sec_diff, usec_diff);
	if (num_timeouts > 50) {
		status = IB_TIMEOUT;
	}
	/* Exit: */
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static void
osmtest_prepare_db_generic(IN osmtest_t * const p_osmt,
			   IN cl_qmap_t * const p_tbl)
{
	generic_t *p_generic;

	OSM_LOG_ENTER(&p_osmt->log);

	p_generic = (generic_t *) cl_qmap_head(p_tbl);

	while (p_generic != (generic_t *) cl_qmap_end(p_tbl)) {
		p_generic->count = 0;
		p_generic = (generic_t *) cl_qmap_next(&p_generic->map_item);
	}

	OSM_LOG_EXIT(&p_osmt->log);
}

/**********************************************************************
 **********************************************************************/
static void osmtest_prepare_db(IN osmtest_t * const p_osmt)
{
	OSM_LOG_ENTER(&p_osmt->log);

	osmtest_prepare_db_generic(p_osmt, &p_osmt->exp_subn.node_lid_tbl);
	osmtest_prepare_db_generic(p_osmt, &p_osmt->exp_subn.path_tbl);

	OSM_LOG_EXIT(&p_osmt->log);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t osmtest_check_missing_nodes(IN osmtest_t * const p_osmt)
{
	const node_t *p_node;
	cl_status_t status = IB_SUCCESS;
	cl_qmap_t *p_tbl;

	OSM_LOG_ENTER(&p_osmt->log);

	p_tbl = &p_osmt->exp_subn.node_lid_tbl;

	p_node = (node_t *) cl_qmap_head(p_tbl);

	while (p_node != (node_t *) cl_qmap_end(p_tbl)) {
		if (p_node->count == 0) {
			/*
			 * This node was not reported by the SA
			 */
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0080: "
				"Missing node 0x%016" PRIx64 "\n",
				cl_ntoh64(p_node->rec.node_info.node_guid));
			status = IB_ERROR;
		}

		p_node = (node_t *) cl_qmap_next(&p_node->map_item);
	}

	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t osmtest_check_missing_ports(IN osmtest_t * const p_osmt)
{
	const port_t *p_port;
	cl_status_t status = IB_SUCCESS;
	cl_qmap_t *p_tbl;

	OSM_LOG_ENTER(&p_osmt->log);

	p_tbl = &p_osmt->exp_subn.port_key_tbl;

	p_port = (port_t *) cl_qmap_head(p_tbl);

	while (p_port != (port_t *) cl_qmap_end(p_tbl)) {
		if (p_port->count == 0) {
			/*
			 * This port was not reported by the SA
			 */
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0081: "
				"Missing port LID:0x%X Num:0x%X\n",
				cl_ntoh16(p_port->rec.lid),
				p_port->rec.port_num);
			status = IB_ERROR;
		}

		p_port = (port_t *) cl_qmap_next(&p_port->map_item);
	}

	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t osmtest_check_missing_paths(IN osmtest_t * const p_osmt)
{
	const path_t *p_path;
	cl_status_t status = IB_SUCCESS;
	cl_qmap_t *p_tbl;

	OSM_LOG_ENTER(&p_osmt->log);

	p_tbl = &p_osmt->exp_subn.path_tbl;

	p_path = (path_t *) cl_qmap_head(p_tbl);

	while (p_path != (path_t *) cl_qmap_end(p_tbl)) {
		if (p_path->count == 0) {
			/*
			 * This path was not reported by the SA
			 */
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0051: "
				"SA did not return path SLID 0x%X to DLID 0x%X\n",
				cl_ntoh16(p_path->rec.slid),
				cl_ntoh16(p_path->rec.dlid));
			status = IB_ERROR;
			goto Exit;
		}

		p_path = (path_t *) cl_qmap_next(&p_path->map_item);
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
inline uint32_t osmtest_path_rec_key_get(IN const ib_path_rec_t * const p_rec)
{
	return (p_rec->dlid << 16 | p_rec->slid);
}

/**********************************************************************
 **********************************************************************/
static boolean_t
osmtest_path_rec_kay_is_valid(IN osmtest_t * const p_osmt,
			      IN const path_t * const p_path)
{
	if ((p_path->comp.dlid == 0) || (p_path->comp.slid == 0)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0168: "
			"SLID and DLID must be specified for defined paths\n");
		return (FALSE);
	}

	return (TRUE);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_validate_path_data(IN osmtest_t * const p_osmt,
			   IN path_t * const p_path,
			   IN const ib_path_rec_t * const p_rec)
{
	cl_status_t status = IB_SUCCESS;
	uint8_t lmc = 0;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
		"Checking path SLID 0x%X to DLID 0x%X\n",
		cl_ntoh16(p_rec->slid), cl_ntoh16(p_rec->dlid));

	status =
	    osmtest_get_local_port_lmc(p_osmt, p_osmt->local_port.lid, &lmc);
	if (status != IB_SUCCESS)
		goto Exit;

	/* HACK: Assume uniform LMC across endports in the subnet */
	/* This is the only LMC mode which OpenSM currently supports */
	/* In absence of this assumption, validation of this is much more complicated */
	if (lmc == 0) {
		/*
		 * Has this record already been returned?
		 */
		if (p_path->count != 0) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0056: "
				"Already received path SLID 0x%X to DLID 0x%X\n",
				cl_ntoh16(p_rec->slid), cl_ntoh16(p_rec->dlid));
			status = IB_ERROR;
			goto Exit;
		}
	} else {
		/* Also, this doesn't detect fewer than the correct number of paths being returned */
		if (p_path->count >= (uint32_t) (1 << (2 * lmc))) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0052: "
				"Already received path SLID 0x%X to DLID 0x%X count %d LMC %d\n",
				cl_ntoh16(p_rec->slid), cl_ntoh16(p_rec->dlid),
				p_path->count, lmc);
			status = IB_ERROR;
			goto Exit;
		}
	}

	++p_path->count;

	/*
	 * Check the fields the user wants checked.
	 */
	if ((p_path->comp.dgid.unicast.interface_id &
	     p_path->rec.dgid.unicast.interface_id) !=
	    (p_path->comp.dgid.unicast.interface_id &
	     p_rec->dgid.unicast.interface_id)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0169: "
			"DGID mismatch on path SLID 0x%X to DLID 0x%X\n"
			"\t\t\t\tExpected 0x%016" PRIx64 " 0x%016" PRIx64 "\n"
			"\t\t\t\tReceived 0x%016" PRIx64 " 0x%016" PRIx64 "\n",
			cl_ntoh16(p_path->rec.slid),
			cl_ntoh16(p_path->rec.dlid),
			cl_ntoh64(p_path->rec.dgid.unicast.prefix),
			cl_ntoh64(p_path->rec.dgid.unicast.interface_id),
			cl_ntoh64(p_rec->dgid.unicast.prefix),
			cl_ntoh64(p_rec->dgid.unicast.interface_id));
		status = IB_ERROR;
		goto Exit;
	}

	/*
	 * Check the fields the user wants checked.
	 */
	if ((p_path->comp.sgid.unicast.interface_id &
	     p_path->rec.sgid.unicast.interface_id) !=
	    (p_path->comp.sgid.unicast.interface_id &
	     p_rec->sgid.unicast.interface_id)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0057: "
			"SGID mismatch on path SLID 0x%X to DLID 0x%X\n"
			"\t\t\t\tExpected 0x%016" PRIx64 " 0x%016" PRIx64 ",\n"
			"\t\t\t\tReceived 0x%016" PRIx64 " 0x%016" PRIx64 ".\n",
			cl_ntoh16(p_path->rec.slid),
			cl_ntoh16(p_path->rec.dlid),
			cl_ntoh64(p_path->rec.sgid.unicast.prefix),
			cl_ntoh64(p_path->rec.sgid.unicast.interface_id),
			cl_ntoh64(p_rec->sgid.unicast.prefix),
			cl_ntoh64(p_rec->sgid.unicast.interface_id));
		status = IB_ERROR;
		goto Exit;
	}

	/*
	 * Compare the fields the user wishes to validate.
	 */
	if ((p_path->comp.pkey & p_path->rec.pkey) !=
	    (p_path->comp.pkey & p_rec->pkey)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0012: "
			"PKEY mismatch on path SLID 0x%X to DLID 0x%X\n"
			"\t\t\t\tExpected 0x%X, received 0x%X\n",
			cl_ntoh16(p_path->rec.slid),
			cl_ntoh16(p_path->rec.dlid),
			cl_ntoh16(p_path->rec.pkey), cl_ntoh16(p_rec->pkey));
		status = IB_ERROR;
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_validate_node_data(IN osmtest_t * const p_osmt,
			   IN node_t * const p_node,
			   IN const ib_node_record_t * const p_rec)
{
	cl_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
		"Checking node 0x%016" PRIx64 ", LID 0x%X\n",
		cl_ntoh64(p_rec->node_info.node_guid), cl_ntoh16(p_rec->lid));

	/*
	 * Has this record already been returned?
	 */
	if (p_node->count != 0) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0013: "
			"Already received node 0x%016" PRIx64 "\n",
			cl_ntoh64(p_node->rec.node_info.node_guid));
		status = IB_ERROR;
		goto Exit;
	}

	++p_node->count;

	/*
	 * Compare the fields the user wishes to validate.
	 */
	if ((p_node->comp.lid & p_node->rec.lid) !=
	    (p_node->comp.lid & p_rec->lid)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0014: "
			"Field mismatch node 0x%016" PRIx64 ", LID 0x%X\n"
			"\t\t\t\tExpected LID 0x%X, received 0x%X\n",
			cl_ntoh64(p_rec->node_info.node_guid),
			cl_ntoh16(p_rec->lid), p_node->rec.lid, p_rec->lid);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_node->comp.node_info.base_version &
	     p_node->rec.node_info.base_version) !=
	    (p_node->comp.node_info.base_version &
	     p_rec->node_info.base_version)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0015: "
			"Field mismatch node 0x%016" PRIx64 ", LID 0x%X\n"
			"\t\t\t\tExpected base_version 0x%X, received 0x%X\n",
			cl_ntoh64(p_rec->node_info.node_guid),
			cl_ntoh16(p_rec->lid),
			p_node->rec.node_info.base_version,
			p_rec->node_info.base_version);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_node->comp.node_info.class_version &
	     p_node->rec.node_info.class_version) !=
	    (p_node->comp.node_info.class_version &
	     p_rec->node_info.class_version)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0016: "
			"Field mismatch node 0x%016" PRIx64 ", LID 0x%X\n"
			"\t\t\t\tExpected class_version 0x%X, received 0x%X\n",
			cl_ntoh64(p_rec->node_info.node_guid),
			cl_ntoh16(p_rec->lid),
			p_node->rec.node_info.class_version,
			p_rec->node_info.class_version);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_node->comp.node_info.node_type &
	     p_node->rec.node_info.node_type) !=
	    (p_node->comp.node_info.node_type & p_rec->node_info.node_type)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0017: "
			"Field mismatch node 0x%016" PRIx64 ", LID 0x%X\n"
			"\t\t\t\tExpected node_type 0x%X, received 0x%X\n",
			cl_ntoh64(p_rec->node_info.node_guid),
			cl_ntoh16(p_rec->lid),
			p_node->rec.node_info.node_type,
			p_rec->node_info.node_type);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_node->comp.node_info.sys_guid &
	     p_node->rec.node_info.sys_guid) !=
	    (p_node->comp.node_info.sys_guid & p_rec->node_info.sys_guid)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0018: "
			"Field mismatch node 0x%016" PRIx64 ", LID 0x%X\n"
			"\t\t\t\tExpected sys_guid 0x%016" PRIx64
			", received 0x%016" PRIx64 "\n",
			cl_ntoh64(p_rec->node_info.node_guid),
			cl_ntoh16(p_rec->lid),
			cl_ntoh64(p_node->rec.node_info.sys_guid),
			cl_ntoh64(p_rec->node_info.sys_guid));
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_node->comp.node_info.node_guid &
	     p_node->rec.node_info.node_guid) !=
	    (p_node->comp.node_info.node_guid & p_rec->node_info.node_guid)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0019: "
			"Field mismatch node 0x%016" PRIx64 ", LID 0x%X\n"
			"\t\t\t\tExpected node_guid 0x%016" PRIx64
			", received 0x%016" PRIx64 "\n",
			cl_ntoh64(p_rec->node_info.node_guid),
			cl_ntoh16(p_rec->lid),
			cl_ntoh64(p_node->rec.node_info.node_guid),
			cl_ntoh64(p_rec->node_info.node_guid));
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_node->comp.node_info.port_guid &
	     p_node->rec.node_info.port_guid) !=
	    (p_node->comp.node_info.port_guid & p_rec->node_info.port_guid)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0031: "
			"Field mismatch node 0x%016" PRIx64 ", LID 0x%X\n"
			"\t\t\t\tExpected port_guid 0x%016" PRIx64
			", received 0x%016" PRIx64 "\n",
			cl_ntoh64(p_rec->node_info.node_guid),
			cl_ntoh16(p_rec->lid),
			cl_ntoh64(p_node->rec.node_info.port_guid),
			cl_ntoh64(p_rec->node_info.port_guid));
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_node->comp.node_info.partition_cap &
	     p_node->rec.node_info.partition_cap) !=
	    (p_node->comp.node_info.partition_cap &
	     p_rec->node_info.partition_cap)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0032: "
			"Field mismatch node 0x%016" PRIx64 ", LID 0x%X\n"
			"\t\t\t\tExpected partition_cap 0x%X, received 0x%X\n",
			cl_ntoh64(p_rec->node_info.node_guid),
			cl_ntoh16(p_rec->lid),
			cl_ntoh16(p_node->rec.node_info.partition_cap),
			cl_ntoh16(p_rec->node_info.partition_cap));
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_node->comp.node_info.device_id &
	     p_node->rec.node_info.device_id) !=
	    (p_node->comp.node_info.device_id & p_rec->node_info.device_id)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0033: "
			"Field mismatch node 0x%016" PRIx64 ", LID 0x%X\n"
			"\t\t\t\tExpected device_id 0x%X, received 0x%X\n",
			cl_ntoh64(p_rec->node_info.node_guid),
			cl_ntoh16(p_rec->lid),
			cl_ntoh16(p_node->rec.node_info.device_id),
			cl_ntoh16(p_rec->node_info.device_id));
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_node->comp.node_info.revision &
	     p_node->rec.node_info.revision) !=
	    (p_node->comp.node_info.revision & p_rec->node_info.revision)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0034: "
			"Field mismatch node 0x%016" PRIx64 ", LID 0x%X\n"
			"\t\t\t\tExpected revision 0x%X, received 0x%X\n",
			cl_ntoh64(p_rec->node_info.node_guid),
			cl_ntoh16(p_rec->lid),
			cl_ntoh32(p_node->rec.node_info.revision),
			cl_ntoh32(p_rec->node_info.revision));
		status = IB_ERROR;
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_validate_node_rec(IN osmtest_t * const p_osmt,
			  IN const ib_node_record_t * const p_rec)
{
	cl_status_t status = IB_SUCCESS;
	node_t *p_node;
	const cl_qmap_t *p_tbl;

	OSM_LOG_ENTER(&p_osmt->log);

	/*
	 * Find proper node record in the database.
	 */
	p_tbl = &p_osmt->exp_subn.node_lid_tbl;
	p_node = (node_t *) cl_qmap_get(p_tbl, p_rec->lid);
	if (p_node == (node_t *) cl_qmap_end(p_tbl)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0035: "
			"Unexpected node 0x%016" PRIx64 ", LID 0x%X\n",
			cl_ntoh64(p_rec->node_info.node_guid),
			cl_ntoh16(p_rec->lid));
		status = IB_ERROR;
		goto Exit;
	}

	status = osmtest_validate_node_data(p_osmt, p_node, p_rec);

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_validate_port_data(IN osmtest_t * const p_osmt,
			   IN port_t * const p_port,
			   IN const ib_portinfo_record_t * const p_rec)
{
	cl_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
		"Checking port LID 0x%X, Num 0x%X\n",
		cl_ntoh16(p_rec->lid), p_rec->port_num);

	/*
	 * Has this record already been returned?
	 */
	if (p_port->count != 0) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0036: "
			"Already received port LID 0x%X, Num 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num);
		status = IB_ERROR;
		goto Exit;
	}

	++p_port->count;

	/*
	 * Compare the fields the user wishes to validate.
	 */
	if ((p_port->comp.lid & p_port->rec.lid) !=
	    (p_port->comp.lid & p_rec->lid)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0037: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected LID 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.lid, p_rec->lid);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_num & p_port->rec.port_num) !=
	    (p_port->comp.port_num & p_rec->port_num)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0038: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected port_num 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_num, p_rec->port_num);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.m_key & p_port->rec.port_info.m_key) !=
	    (p_port->comp.port_info.m_key & p_rec->port_info.m_key)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0039: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected m_key 0x%016" PRIx64
			", received 0x%016" PRIx64 "\n", cl_ntoh16(p_rec->lid),
			p_rec->port_num, p_port->rec.port_info.m_key,
			p_rec->port_info.m_key);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.subnet_prefix & p_port->rec.port_info.
	     subnet_prefix) !=
	    (p_port->comp.port_info.subnet_prefix & p_rec->port_info.
	     subnet_prefix)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0040: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected subnet_prefix 0x%016" PRIx64
			", received 0x%016" PRIx64 "\n", cl_ntoh16(p_rec->lid),
			p_rec->port_num, p_port->rec.port_info.subnet_prefix,
			p_rec->port_info.subnet_prefix);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.base_lid & p_port->rec.port_info.
	     base_lid) !=
	    (p_port->comp.port_info.base_lid & p_rec->port_info.base_lid)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0041: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected base_lid 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.base_lid,
			p_rec->port_info.base_lid);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.master_sm_base_lid & p_port->rec.port_info.
	     master_sm_base_lid) !=
	    (p_port->comp.port_info.master_sm_base_lid & p_rec->port_info.
	     master_sm_base_lid)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0042: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected master_sm_base_lid 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.master_sm_base_lid,
			p_rec->port_info.master_sm_base_lid);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.capability_mask & p_port->rec.port_info.
	     capability_mask) !=
	    (p_port->comp.port_info.capability_mask & p_rec->port_info.
	     capability_mask)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0043: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected capability_mask 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			cl_ntoh32(p_port->rec.port_info.capability_mask),
			cl_ntoh32(p_rec->port_info.capability_mask));
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.diag_code & p_port->rec.port_info.
	     diag_code) !=
	    (p_port->comp.port_info.diag_code & p_rec->port_info.diag_code)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0044: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected diag_code 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.diag_code,
			p_rec->port_info.diag_code);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.m_key_lease_period & p_port->rec.port_info.
	     m_key_lease_period) !=
	    (p_port->comp.port_info.m_key_lease_period & p_rec->port_info.
	     m_key_lease_period)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0045: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected m_key_lease_period 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.m_key_lease_period,
			p_rec->port_info.m_key_lease_period);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.local_port_num & p_port->rec.port_info.
	     local_port_num) !=
	    (p_port->comp.port_info.local_port_num & p_rec->port_info.
	     local_port_num)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0046: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected local_port_num 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.local_port_num,
			p_rec->port_info.local_port_num);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.link_width_enabled & p_port->rec.port_info.
	     link_width_enabled) !=
	    (p_port->comp.port_info.link_width_enabled & p_rec->port_info.
	     link_width_enabled)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0047: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected link_width_enabled 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.link_width_enabled,
			p_rec->port_info.link_width_enabled);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.link_width_supported & p_port->rec.
	     port_info.link_width_supported) !=
	    (p_port->comp.port_info.link_width_supported & p_rec->port_info.
	     link_width_supported)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0048: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected link_width_supported 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.link_width_supported,
			p_rec->port_info.link_width_supported);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.link_width_active & p_port->rec.port_info.
	     link_width_active) !=
	    (p_port->comp.port_info.link_width_active & p_rec->port_info.
	     link_width_active)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0049: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected link_width_active 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.link_width_active,
			p_rec->port_info.link_width_active);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.link_speed & p_port->rec.port_info.
	     link_speed) !=
	    (p_port->comp.port_info.link_speed & p_rec->port_info.link_speed)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0054: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected link_speed 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.link_speed,
			p_rec->port_info.link_speed);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.state_info1 & p_port->rec.port_info.
	     state_info1) !=
	    (p_port->comp.port_info.state_info1 & p_rec->port_info.
	     state_info1)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0055: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected state_info1 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.state_info1,
			p_rec->port_info.state_info1);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.state_info2 & p_port->rec.port_info.
	     state_info2) !=
	    (p_port->comp.port_info.state_info2 & p_rec->port_info.
	     state_info2)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0058: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected state_info2 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.state_info2,
			p_rec->port_info.state_info2);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.mkey_lmc & p_port->rec.port_info.
	     mkey_lmc) !=
	    (p_port->comp.port_info.mkey_lmc & p_rec->port_info.mkey_lmc)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0059: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected mkey_lmc 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.mkey_lmc,
			p_rec->port_info.mkey_lmc);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.link_speed & p_port->rec.port_info.
	     link_speed) !=
	    (p_port->comp.port_info.link_speed & p_rec->port_info.link_speed)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0060: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected link_speed 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.link_speed,
			p_rec->port_info.link_speed);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.mtu_smsl & p_port->rec.port_info.
	     mtu_smsl) !=
	    (p_port->comp.port_info.mtu_smsl & p_rec->port_info.mtu_smsl)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0061: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected mtu_smsl 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.mtu_smsl,
			p_rec->port_info.mtu_smsl);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.vl_cap & p_port->rec.port_info.vl_cap) !=
	    (p_port->comp.port_info.vl_cap & p_rec->port_info.vl_cap)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0062: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected vl_cap 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.vl_cap, p_rec->port_info.vl_cap);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.vl_high_limit & p_port->rec.port_info.
	     vl_high_limit) !=
	    (p_port->comp.port_info.vl_high_limit & p_rec->port_info.
	     vl_high_limit)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0082: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected vl_high_limit 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.vl_high_limit,
			p_rec->port_info.vl_high_limit);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.vl_arb_high_cap & p_port->rec.port_info.
	     vl_arb_high_cap) !=
	    (p_port->comp.port_info.vl_arb_high_cap & p_rec->port_info.
	     vl_arb_high_cap)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0083: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected vl_arb_high_cap 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.vl_arb_high_cap,
			p_rec->port_info.vl_arb_high_cap);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.vl_arb_low_cap & p_port->rec.port_info.
	     vl_arb_low_cap) !=
	    (p_port->comp.port_info.vl_arb_low_cap & p_rec->port_info.
	     vl_arb_low_cap)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0084: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected vl_arb_low_cap 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.vl_arb_low_cap,
			p_rec->port_info.vl_arb_low_cap);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.mtu_cap & p_port->rec.port_info.mtu_cap) !=
	    (p_port->comp.port_info.mtu_cap & p_rec->port_info.mtu_cap)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0085: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected mtu_cap 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.mtu_cap,
			p_rec->port_info.mtu_cap);
		status = IB_ERROR;
		goto Exit;
	}
#if 0
	/* this is a dynamic attribute */
	if ((p_port->comp.port_info.vl_stall_life & p_port->rec.port_info.
	     vl_stall_life) !=
	    (p_port->comp.port_info.vl_stall_life & p_rec->port_info.
	     vl_stall_life)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 012F: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected vl_stall_life 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.vl_stall_life,
			p_rec->port_info.vl_stall_life);
		status = IB_ERROR;
		goto Exit;
	}
#endif

	if ((p_port->comp.port_info.vl_enforce & p_port->rec.port_info.
	     vl_enforce) !=
	    (p_port->comp.port_info.vl_enforce & p_rec->port_info.vl_enforce)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0086: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected vl_enforce 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.vl_enforce,
			p_rec->port_info.vl_enforce);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.m_key_violations & p_port->rec.port_info.
	     m_key_violations) !=
	    (p_port->comp.port_info.m_key_violations & p_rec->port_info.
	     m_key_violations)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0087: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected m_key_violations 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			cl_ntoh16(p_port->rec.port_info.m_key_violations),
			cl_ntoh16(p_rec->port_info.m_key_violations));
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.p_key_violations & p_port->rec.port_info.
	     p_key_violations) !=
	    (p_port->comp.port_info.p_key_violations & p_rec->port_info.
	     p_key_violations)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0088: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected p_key_violations 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			cl_ntoh16(p_port->rec.port_info.p_key_violations),
			cl_ntoh16(p_rec->port_info.p_key_violations));
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.q_key_violations & p_port->rec.port_info.
	     q_key_violations) !=
	    (p_port->comp.port_info.q_key_violations & p_rec->port_info.
	     q_key_violations)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0089: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected q_key_violations 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			cl_ntoh16(p_port->rec.port_info.q_key_violations),
			cl_ntoh16(p_rec->port_info.q_key_violations));
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.guid_cap & p_port->rec.port_info.
	     guid_cap) !=
	    (p_port->comp.port_info.guid_cap & p_rec->port_info.guid_cap)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0090: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected guid_cap 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.guid_cap,
			p_rec->port_info.guid_cap);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.subnet_timeout & p_port->rec.port_info.
	     subnet_timeout) !=
	    (p_port->comp.port_info.subnet_timeout & p_rec->port_info.
	     subnet_timeout)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0091: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected subnet_timeout 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			ib_port_info_get_timeout(&p_port->rec.port_info),
			ib_port_info_get_timeout(&p_rec->port_info));
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.resp_time_value & p_port->rec.port_info.
	     resp_time_value) !=
	    (p_port->comp.port_info.resp_time_value & p_rec->port_info.
	     resp_time_value)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0092: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected resp_time_value 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.resp_time_value,
			p_rec->port_info.resp_time_value);
		status = IB_ERROR;
		goto Exit;
	}

	if ((p_port->comp.port_info.error_threshold & p_port->rec.port_info.
	     error_threshold) !=
	    (p_port->comp.port_info.error_threshold & p_rec->port_info.
	     error_threshold)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0093: "
			"Field mismatch port LID 0x%X Num:0x%X\n"
			"\t\t\t\tExpected error_threshold 0x%X, received 0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num,
			p_port->rec.port_info.error_threshold,
			p_rec->port_info.error_threshold);
		status = IB_ERROR;
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_validate_port_rec(IN osmtest_t * const p_osmt,
			  IN const ib_portinfo_record_t * const p_rec)
{
	cl_status_t status = IB_SUCCESS;
	port_t *p_port;
	const cl_qmap_t *p_tbl;

	OSM_LOG_ENTER(&p_osmt->log);

	/*
	 * Find proper port record in the database.
	 * (we use by guid - since lid is not unique)
	 */
	p_tbl = &p_osmt->exp_subn.port_key_tbl;
	p_port =
	    (port_t *) cl_qmap_get(p_tbl,
				   port_gen_id(p_rec->lid, p_rec->port_num));
	if (p_port == (port_t *) cl_qmap_end(p_tbl)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0094: "
			"Unexpected port LID 0x%X, Num:0x%X\n",
			cl_ntoh16(p_rec->lid), p_rec->port_num);
		status = IB_ERROR;
		goto Exit;
	}

	status = osmtest_validate_port_data(p_osmt, p_port, p_rec);

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_validate_path_rec(IN osmtest_t * const p_osmt,
			  IN const ib_path_rec_t * const p_rec)
{
	cl_status_t status = IB_SUCCESS;
	path_t *p_path;
	const cl_qmap_t *p_tbl;

	OSM_LOG_ENTER(&p_osmt->log);

	/*
	 * Find proper path record in the database.
	 */
	p_tbl = &p_osmt->exp_subn.path_tbl;
	p_path = (path_t *) cl_qmap_get(p_tbl, osmtest_path_rec_key_get(p_rec));
	if (p_path == (path_t *) cl_qmap_end(p_tbl)) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0095: "
			"Unexpected path SLID 0x%X to DLID 0x%X\n",
			cl_ntoh16(p_rec->slid), cl_ntoh16(p_rec->dlid));
		status = IB_ERROR;
		goto Exit;
	}

	status = osmtest_validate_path_data(p_osmt, p_path, p_rec);

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

#ifdef VENDOR_RMPP_SUPPORT
ib_net64_t portguid = 0;

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_validate_all_node_recs(IN osmtest_t * const p_osmt)
{
	osmtest_req_context_t context;
	const ib_node_record_t *p_rec;
	uint32_t i;
	cl_status_t status;
	size_t num_recs;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&context, 0, sizeof(context));

	/*
	 * Do a blocking query for all NodeRecords in the subnet.
	 */
	status = osmtest_get_all_recs(p_osmt, IB_MAD_ATTR_NODE_RECORD,
				      sizeof(*p_rec), &context);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0096: "
			"osmtest_get_all_recs failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	num_recs = context.result.result_cnt;

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Received %zu records\n",
		num_recs);

	/*
	 * Compare the received records to the database.
	 */
	osmtest_prepare_db(p_osmt);

	for (i = 0; i < num_recs; i++) {
		p_rec =
		    osmv_get_query_node_rec(context.result.p_result_madw, i);

		status = osmtest_validate_node_rec(p_osmt, p_rec);
		if (status != IB_SUCCESS) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0097: "
				"osmtest_valid_node_rec failed (%s)\n",
				ib_get_err_str(status));
			goto Exit;
		}
		if (!portguid)
			portguid = p_rec->node_info.port_guid;
	}

	status = osmtest_check_missing_nodes(p_osmt);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0098: "
			"osmtest_check_missing_nodes failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
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
static ib_api_status_t
osmtest_validate_all_guidinfo_recs(IN osmtest_t * const p_osmt)
{
	osmtest_req_context_t context;
	const ib_guidinfo_record_t *p_rec;
	cl_status_t status;
	size_t num_recs;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&context, 0, sizeof(context));

	/*
	 * Do a blocking query for all GuidInfoRecords in the subnet.
	 */
	status = osmtest_get_all_recs(p_osmt, IB_MAD_ATTR_GUIDINFO_RECORD,
				      sizeof(*p_rec), &context);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0099: "
			"osmtest_get_all_recs failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	num_recs = context.result.result_cnt;

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Received %zu records\n",
		num_recs);

	/* No validation as yet */

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
static ib_api_status_t
osmtest_validate_all_path_recs(IN osmtest_t * const p_osmt)
{
	osmtest_req_context_t context;
	const ib_path_rec_t *p_rec;
	uint32_t i;
	cl_status_t status;
	size_t num_recs;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&context, 0, sizeof(context));

	/*
	 * Do a blocking query for all PathRecords in the subnet.
	 */
	status = osmtest_get_all_recs(p_osmt, IB_MAD_ATTR_PATH_RECORD,
				      sizeof(*p_rec), &context);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 009A: "
			"osmtest_get_all_recs failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	num_recs = context.result.result_cnt;

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "Received %zu records\n",
		num_recs);

	/*
	 * Compare the received records to the database.
	 */
	osmtest_prepare_db(p_osmt);

	for (i = 0; i < num_recs; i++) {
		p_rec =
		    osmv_get_query_path_rec(context.result.p_result_madw, i);

		status = osmtest_validate_path_rec(p_osmt, p_rec);
		if (status != IB_SUCCESS) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0100: "
				"osmtest_validate_path_rec failed (%s)\n",
				ib_get_err_str(status));
			goto Exit;
		}
	}

	status = osmtest_check_missing_paths(p_osmt);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0101: "
			"osmtest_check_missing_paths failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
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
 * Get link record by LID
 **********************************************************************/
ib_api_status_t
osmtest_get_link_rec_by_lid(IN osmtest_t * const p_osmt,
			    IN ib_net16_t const from_lid,
			    IN ib_net16_t const to_lid,
			    IN OUT osmtest_req_context_t * const p_context)
{
	ib_api_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;
	ib_link_record_t record;
	ib_mad_t *p_mad;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Getting link record from LID 0x%02X to LID 0x%02X\n",
		cl_ntoh16(from_lid), cl_ntoh16(to_lid));

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));
	memset(&record, 0, sizeof(record));

	record.from_lid = from_lid;
	record.to_lid = to_lid;
	p_context->p_osmt = p_osmt;
	if (from_lid)
		user.comp_mask |= IB_LR_COMPMASK_FROM_LID;
	if (to_lid)
		user.comp_mask |= IB_LR_COMPMASK_TO_LID;
	user.attr_id = IB_MAD_ATTR_LINK_RECORD;
	user.attr_offset = cl_ntoh16((uint16_t) (sizeof(record) >> 3));
	user.p_attr = &record;

	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 007A: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = p_context->result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 007B: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		if (status == IB_REMOTE_ERROR) {
			p_mad =
			    osm_madw_get_mad_ptr(p_context->result.
						 p_result_madw);
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"osmtest_get_link_rec_by_lid: "
				"Remote error = %s\n",
				ib_get_mad_status_str(p_mad));

			status =
			    (ib_net16_t) (p_mad->status & IB_SMP_STATUS_MASK);
		}
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 * Get GUIDInfo record by LID
 **********************************************************************/
ib_api_status_t
osmtest_get_guidinfo_rec_by_lid(IN osmtest_t * const p_osmt,
				IN ib_net16_t const lid,
				IN OUT osmtest_req_context_t * const p_context)
{
	ib_api_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;
	ib_guidinfo_record_t record;
	ib_mad_t *p_mad;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Getting GUIDInfo record for LID 0x%02X\n", cl_ntoh16(lid));

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));
	memset(&record, 0, sizeof(record));

	record.lid = lid;
	p_context->p_osmt = p_osmt;
	user.comp_mask = IB_GIR_COMPMASK_LID;
	user.attr_id = IB_MAD_ATTR_GUIDINFO_RECORD;
	user.attr_offset = cl_ntoh16((uint16_t) (sizeof(record) >> 3));
	user.p_attr = &record;

	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;

	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 007C: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = p_context->result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 007D: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		if (status == IB_REMOTE_ERROR) {
			p_mad =
			    osm_madw_get_mad_ptr(p_context->result.
						 p_result_madw);
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(p_mad));

			status =
			    (ib_net16_t) (p_mad->status & IB_SMP_STATUS_MASK);
		}
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 * Get PKeyTable record by LID
 **********************************************************************/
ib_api_status_t
osmtest_get_pkeytbl_rec_by_lid(IN osmtest_t * const p_osmt,
			       IN ib_net16_t const lid,
			       IN ib_net64_t const sm_key,
			       IN OUT osmtest_req_context_t * const p_context)
{
	ib_api_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;
	ib_pkey_table_record_t record;
	ib_mad_t *p_mad;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Getting PKeyTable record for LID 0x%02X\n", cl_ntoh16(lid));

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));
	memset(&record, 0, sizeof(record));

	record.lid = lid;
	p_context->p_osmt = p_osmt;
	user.comp_mask = IB_PKEY_COMPMASK_LID;
	user.attr_id = IB_MAD_ATTR_PKEY_TBL_RECORD;
	user.attr_offset = cl_ntoh16((uint16_t) (sizeof(record) >> 3));
	user.p_attr = &record;

	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;

	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.sm_key = sm_key;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 007E: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = p_context->result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 007F: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		if (status == IB_REMOTE_ERROR) {
			p_mad =
			    osm_madw_get_mad_ptr(p_context->result.
						 p_result_madw);
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(p_mad));

			status =
			    (ib_net16_t) (p_mad->status & IB_SMP_STATUS_MASK);
		}
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 * Get SwitchInfo record by LID
 **********************************************************************/
ib_api_status_t
osmtest_get_sw_info_rec_by_lid(IN osmtest_t * const p_osmt,
			       IN ib_net16_t const lid,
			       IN OUT osmtest_req_context_t * const p_context)
{
	ib_api_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;
	ib_switch_info_record_t record;
	ib_mad_t *p_mad;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Getting SwitchInfo record for LID 0x%02X\n", cl_ntoh16(lid));

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));
	memset(&record, 0, sizeof(record));

	record.lid = lid;
	p_context->p_osmt = p_osmt;
	if (lid)
		user.comp_mask = IB_SWIR_COMPMASK_LID;
	user.attr_id = IB_MAD_ATTR_SWITCH_INFO_RECORD;
	user.attr_offset = cl_ntoh16((uint16_t) (sizeof(record) >> 3));
	user.p_attr = &record;

	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;

	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 006C: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = p_context->result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 006D: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		if (status == IB_REMOTE_ERROR) {
			p_mad =
			    osm_madw_get_mad_ptr(p_context->result.
						 p_result_madw);
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(p_mad));

			status =
			    (ib_net16_t) (p_mad->status & IB_SMP_STATUS_MASK);
		}
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 * Get LFT record by LID
 **********************************************************************/
ib_api_status_t
osmtest_get_lft_rec_by_lid(IN osmtest_t * const p_osmt,
			   IN ib_net16_t const lid,
			   IN OUT osmtest_req_context_t * const p_context)
{
	ib_api_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;
	ib_lft_record_t record;
	ib_mad_t *p_mad;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Getting LFT record for LID 0x%02X\n", cl_ntoh16(lid));

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));
	memset(&record, 0, sizeof(record));

	record.lid = lid;
	p_context->p_osmt = p_osmt;
	if (lid)
		user.comp_mask = IB_LFTR_COMPMASK_LID;
	user.attr_id = IB_MAD_ATTR_LFT_RECORD;
	user.attr_offset = cl_ntoh16((uint16_t) (sizeof(record) >> 3));
	user.p_attr = &record;

	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;

	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 008A: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = p_context->result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 008B: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		if (status == IB_REMOTE_ERROR) {
			p_mad =
			    osm_madw_get_mad_ptr(p_context->result.
						 p_result_madw);
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(p_mad));

			status =
			    (ib_net16_t) (p_mad->status & IB_SMP_STATUS_MASK);
		}
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 * Get MFT record by LID
 **********************************************************************/
ib_api_status_t
osmtest_get_mft_rec_by_lid(IN osmtest_t * const p_osmt,
			   IN ib_net16_t const lid,
			   IN OUT osmtest_req_context_t * const p_context)
{
	ib_api_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;
	ib_mft_record_t record;
	ib_mad_t *p_mad;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Getting MFT record for LID 0x%02X\n", cl_ntoh16(lid));

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));
	memset(&record, 0, sizeof(record));

	record.lid = lid;
	p_context->p_osmt = p_osmt;
	if (lid)
		user.comp_mask = IB_MFTR_COMPMASK_LID;
	user.attr_id = IB_MAD_ATTR_MFT_RECORD;
	user.attr_offset = cl_ntoh16((uint16_t) (sizeof(record) >> 3));
	user.p_attr = &record;

	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;

	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 009B: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = p_context->result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 009C: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		if (status == IB_REMOTE_ERROR) {
			p_mad =
			    osm_madw_get_mad_ptr(p_context->result.
						 p_result_madw);
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(p_mad));

			status =
			    (ib_net16_t) (p_mad->status & IB_SMP_STATUS_MASK);
		}
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_sminfo_record_request(IN osmtest_t * const p_osmt,
			      IN uint8_t method,
			      IN void *p_options,
			      IN OUT osmtest_req_context_t * const p_context)
{
	ib_api_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;
	ib_sminfo_record_t record;
	ib_mad_t *p_mad;
	osmtest_sm_info_rec_t *p_sm_info_opt;

	OSM_LOG_ENTER(&p_osmt->log);

	/*
	 * Do a blocking query for these records in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));
	memset(&record, 0, sizeof(record));

	p_context->p_osmt = p_osmt;
	user.attr_id = IB_MAD_ATTR_SMINFO_RECORD;
	user.attr_offset = cl_ntoh16((uint16_t) (sizeof(record) >> 3));
	p_sm_info_opt = p_options;
	if (p_sm_info_opt->sm_guid != 0) {
		record.sm_info.guid = p_sm_info_opt->sm_guid;
		user.comp_mask |= IB_SMIR_COMPMASK_GUID;
	}
	if (p_sm_info_opt->lid != 0) {
		record.lid = p_sm_info_opt->lid;
		user.comp_mask |= IB_SMIR_COMPMASK_LID;
	}
	if (p_sm_info_opt->priority != 0) {
		record.sm_info.pri_state =
		    (p_sm_info_opt->priority & 0x0F) << 4;
		user.comp_mask |= IB_SMIR_COMPMASK_PRIORITY;
	}
	if (p_sm_info_opt->sm_state != 0) {
		record.sm_info.pri_state |= p_sm_info_opt->sm_state & 0x0F;
		user.comp_mask |= IB_SMIR_COMPMASK_SMSTATE;
	}

	user.method = method;
	user.p_attr = &record;

	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;

	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 008C: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = p_context->result.status;

	if (status != IB_SUCCESS) {
		if (status != IB_INVALID_PARAMETER) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 008D: "
				"ib_query failed (%s)\n",
				ib_get_err_str(status));
		}
		if (status == IB_REMOTE_ERROR) {
			p_mad =
			    osm_madw_get_mad_ptr(p_context->result.
						 p_result_madw);
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(p_mad));

			status =
			    (ib_net16_t) (p_mad->status & IB_SMP_STATUS_MASK);
		}
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_informinfo_request(IN osmtest_t * const p_osmt,
			   IN ib_net16_t attr_id,
			   IN uint8_t method,
			   IN void *p_options,
			   IN OUT osmtest_req_context_t * const p_context)
{
	ib_api_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;
	ib_inform_info_t rec;
	ib_inform_info_record_t record;
	ib_mad_t *p_mad;
	osmtest_inform_info_t *p_inform_info_opt;
	osmtest_inform_info_rec_t *p_inform_info_rec_opt;

	OSM_LOG_ENTER(&p_osmt->log);

	/*
	 * Do a blocking query for these records in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));
	memset(&rec, 0, sizeof(rec));
	memset(&record, 0, sizeof(record));

	p_context->p_osmt = p_osmt;
	user.attr_id = attr_id;
	if (attr_id == IB_MAD_ATTR_INFORM_INFO_RECORD) {
		user.attr_offset = cl_ntoh16((uint16_t) (sizeof(record) >> 3));
		p_inform_info_rec_opt = p_options;
		if (p_inform_info_rec_opt->subscriber_gid.unicast.prefix != 0 &&
		    p_inform_info_rec_opt->subscriber_gid.unicast.
		    interface_id != 0) {
			record.subscriber_gid =
			    p_inform_info_rec_opt->subscriber_gid;
			user.comp_mask = IB_IIR_COMPMASK_SUBSCRIBERGID;
		}
		record.subscriber_enum =
		    cl_hton16(p_inform_info_rec_opt->subscriber_enum);
		user.comp_mask |= IB_IIR_COMPMASK_ENUM;
		user.p_attr = &record;
	} else {
		user.attr_offset = cl_ntoh16((uint16_t) (sizeof(rec) >> 3));
		/* comp mask bits below are for InformInfoRecord rather than InformInfo */
		/* as currently no comp mask bits defined for InformInfo!!! */
		user.comp_mask = IB_IIR_COMPMASK_SUBSCRIBE;
		p_inform_info_opt = p_options;
		rec.subscribe = (uint8_t) p_inform_info_opt->subscribe;
		if (p_inform_info_opt->qpn) {
			rec.g_or_v.generic.qpn_resp_time_val =
			    cl_hton32(p_inform_info_opt->qpn << 8);
			user.comp_mask |= IB_IIR_COMPMASK_QPN;
		}
		if (p_inform_info_opt->trap) {
			rec.g_or_v.generic.trap_num =
			    cl_hton16(p_inform_info_opt->trap);
			user.comp_mask |= IB_IIR_COMPMASK_TRAPNUMB;
		}
		user.p_attr = &rec;
	}
	user.method = method;

	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;

	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = p_context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 008E: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = p_context->result.status;

	if (status != IB_SUCCESS) {
		if (status != IB_INVALID_PARAMETER) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 008F: "
				"ib_query failed (%s)\n",
				ib_get_err_str(status));
		}
		if (status == IB_REMOTE_ERROR) {
			p_mad =
			    osm_madw_get_mad_ptr(p_context->result.
						 p_result_madw);
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				"Remote error = %s\n",
				ib_get_mad_status_str(p_mad));

			status =
			    (ib_net16_t) (p_mad->status & IB_SMP_STATUS_MASK);
		}
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}
#endif

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_validate_single_path_rec_lid_pair(IN osmtest_t * const p_osmt,
					  IN path_t * const p_path)
{
	osmtest_req_context_t context;
	const ib_path_rec_t *p_rec;
	cl_status_t status = IB_SUCCESS;
	size_t num_recs;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&context, 0, sizeof(context));

	status = osmtest_get_path_rec_by_lid_pair(p_osmt,
						  p_path->rec.slid,
						  p_path->rec.dlid, &context);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0102: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	num_recs = context.result.result_cnt;
	if (num_recs != 1) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0103: "
			"Too many records. Expected 1, received %zu\n",
			num_recs);

		status = IB_ERROR;
	} else {
		p_rec =
		    osmv_get_query_path_rec(context.result.p_result_madw, 0);

		status = osmtest_validate_path_data(p_osmt, p_path, p_rec);
		if (status != IB_SUCCESS) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0104: "
				"osmtest_validate_path_data failed (%s)\n",
				ib_get_err_str(status));
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
static ib_api_status_t
osmtest_validate_single_node_rec_lid(IN osmtest_t * const p_osmt,
				     IN ib_net16_t const lid,
				     IN node_t * const p_node)
{
	cl_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;
	ib_node_record_t record;

	osmtest_req_context_t context;
	const ib_node_record_t *p_rec;
	int num_recs, i;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
		"Getting NodeRecord for node with LID 0x%X\n", cl_ntoh16(lid));

	memset(&context, 0, sizeof(context));
	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));
	memset(&record, 0, sizeof(record));

	record.lid = lid;

	context.p_osmt = p_osmt;
	user.comp_mask = IB_NR_COMPMASK_LID;
	user.attr_id = IB_MAD_ATTR_NODE_RECORD;
	user.attr_offset = cl_ntoh16((uint16_t) (sizeof(record) >> 3));
	user.p_attr = &record;

	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = &context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0105: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = context.result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0106: "
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

	num_recs = context.result.result_cnt;
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Received %d nodes\n", num_recs);

	for (i = 0; i < num_recs; i++) {
		p_rec =
		    osmv_get_query_node_rec(context.result.p_result_madw, i);

		status = osmtest_validate_node_rec(p_osmt, p_rec);
		if (status != IB_SUCCESS) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0107: "
				"osmtest_validate_node_data failed (%s)\n",
				ib_get_err_str(status));
			goto Exit;
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
static ib_api_status_t
osmtest_validate_single_port_rec_lid(IN osmtest_t * const p_osmt,
				     IN port_t * const p_port)
{
	osmtest_req_context_t context;

	const ib_portinfo_record_t *p_rec;
	cl_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&context, 0, sizeof(context));

	context.p_osmt = p_osmt;
	osmtest_get_port_rec_by_num(p_osmt,
				    p_port->rec.lid,
				    p_port->rec.port_num, &context);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0108: "
			"ib_query failed (%s)\n", ib_get_err_str(status));

		goto Exit;
	}

	/* we should have got exactly one port */
	p_rec = osmv_get_query_portinfo_rec(context.result.p_result_madw, 0);
	status = osmtest_validate_port_rec(p_osmt, p_rec);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0109: "
			"osmtest_validate_port_data failed (%s)\n",
			ib_get_err_str(status));
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
static ib_api_status_t
osmtest_validate_single_path_rec_guid_pair(IN osmtest_t * const p_osmt,
					   IN const osmv_guid_pair_t *
					   const p_pair)
{
	osmtest_req_context_t context;
	const ib_path_rec_t *p_rec;
	cl_status_t status = IB_SUCCESS;
	size_t num_recs;
	osmv_query_req_t req;
	uint32_t i;
	boolean_t got_error = FALSE;

	OSM_LOG_ENTER(&p_osmt->log);

	memset(&req, 0, sizeof(req));
	memset(&context, 0, sizeof(context));

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
		"\n\t\t\t\tChecking src 0x%016" PRIx64
		" to dest 0x%016" PRIx64 "\n",
		cl_ntoh64(p_pair->src_guid), cl_ntoh64(p_pair->dest_guid));

	context.p_osmt = p_osmt;

	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = &context;
	req.pfn_query_cb = osmtest_query_res_cb;

	req.query_type = OSMV_QUERY_PATH_REC_BY_PORT_GUIDS;
	req.p_query_input = p_pair;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0110: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = context.result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0111: "
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

	num_recs = context.result.result_cnt;
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "%zu records\n", num_recs);

	for (i = 0; i < num_recs; i++) {
		p_rec =
		    osmv_get_query_path_rec(context.result.p_result_madw, i);

		/*
		 * Make sure the GUID values are correct
		 */
		if (p_rec->dgid.unicast.interface_id != p_pair->dest_guid) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0112: "
				"Destination GUID mismatch\n"
				"\t\t\t\texpected 0x%016" PRIx64
				", received 0x%016" PRIx64 "\n",
				cl_ntoh64(p_pair->dest_guid),
				cl_ntoh64(p_rec->dgid.unicast.interface_id));
			got_error = TRUE;
		}

		if (p_rec->sgid.unicast.interface_id != p_pair->src_guid) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0113: "
				"Source GUID mismatch\n"
				"\t\t\t\texpected 0x%016" PRIx64
				", received 0x%016" PRIx64 ".\n",
				cl_ntoh64(p_pair->src_guid),
				cl_ntoh64(p_rec->sgid.unicast.interface_id));
			got_error = TRUE;
		}

		status = osmtest_validate_path_rec(p_osmt, p_rec);
		if (status != IB_SUCCESS) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0114: "
				"osmtest_validate_path_rec failed (%s)\n",
				ib_get_err_str(status));
			got_error = TRUE;
		}
		if (got_error || (status != IB_SUCCESS)) {
			osm_dump_path_record(&p_osmt->log, p_rec,
					     OSM_LOG_VERBOSE);
			if (status == IB_SUCCESS)
				status = IB_ERROR;
			goto Exit;
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
static ib_api_status_t
osmtest_validate_single_path_recs(IN osmtest_t * const p_osmt)
{
	path_t *p_path;
	cl_status_t status = IB_SUCCESS;
	const cl_qmap_t *p_path_tbl;
/* We skip node to node path record validation since it might contains
   NONEXISTENT PATHS, i.e. when using UPDN */
	osmv_guid_pair_t guid_pair;
	uint16_t cnt;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Validating individual path record queries\n");
	p_path_tbl = &p_osmt->exp_subn.path_tbl;

	osmtest_prepare_db(p_osmt);

	/*
	 * Walk the list of all path records, and ask for each one
	 * specifically.  Make sure we get it.
	 */
	cnt = 0;
	p_path = (path_t *) cl_qmap_head(p_path_tbl);
	while (p_path != (path_t *) cl_qmap_end(p_path_tbl)) {
		status =
		    osmtest_validate_single_path_rec_lid_pair(p_osmt, p_path);
		if (status != IB_SUCCESS)
			goto Exit;
		cnt++;
		p_path = (path_t *) cl_qmap_next(&p_path->map_item);
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Total of %u path records validated using LID based query\n",
		cnt);

	status = osmtest_check_missing_paths(p_osmt);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0115: "
			"osmtest_check_missing_paths failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	/*
	 * Do the whole thing again with port GUID pairs.
	 * Note that multiple path records may be returned
	 * for each guid pair if LMC > 0.
	 */
	osmtest_prepare_db(p_osmt);
	cnt = 0;
	p_path = (path_t *) cl_qmap_head(p_path_tbl);
	while (p_path != (path_t *) cl_qmap_end(p_path_tbl)) {
		guid_pair.src_guid = p_path->rec.sgid.unicast.interface_id;
		guid_pair.dest_guid = p_path->rec.dgid.unicast.interface_id;
		status = osmtest_validate_single_path_rec_guid_pair(p_osmt,
								    &guid_pair);
		if (status != IB_SUCCESS)
			goto Exit;
		cnt++;
		p_path = (path_t *) cl_qmap_next(&p_path->map_item);
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Total of %u path records validated using GUID based query\n",
		cnt);

	status = osmtest_check_missing_paths(p_osmt);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0116: "
			"osmtest_check_missing_paths failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_validate_single_node_recs(IN osmtest_t * const p_osmt)
{
	node_t *p_node;
	cl_status_t status = IB_SUCCESS;
	const cl_qmap_t *p_node_lid_tbl;
	uint16_t cnt = 0;

	OSM_LOG_ENTER(&p_osmt->log);

	p_node_lid_tbl = &p_osmt->exp_subn.node_lid_tbl;

	osmtest_prepare_db(p_osmt);

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Validating individual node record queries\n");

	/*
	 * Walk the list of all node records, and ask for each one
	 * specifically.  Make sure we get it.
	 */
	p_node = (node_t *) cl_qmap_head(p_node_lid_tbl);
	while (p_node != (node_t *) cl_qmap_end(p_node_lid_tbl)) {
		status = osmtest_validate_single_node_rec_lid(p_osmt,
							      (ib_net16_t)
							      cl_qmap_key((cl_map_item_t *) p_node), p_node);
		if (status != IB_SUCCESS) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 011A: "
				"osmtest_validate_single_node_rec_lid (%s)\n",
				ib_get_err_str(status));
			goto Exit;
		}
		cnt++;
		p_node = (node_t *) cl_qmap_next(&p_node->map_item);
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Total of %u node records validated\n", cnt);

	status = osmtest_check_missing_nodes(p_osmt);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0117: "
			"osmtest_check_missing_nodes (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_validate_single_port_recs(IN osmtest_t * const p_osmt)
{
	port_t *p_port;
	cl_status_t status = IB_SUCCESS;
	const cl_qmap_t *p_port_key_tbl;
	uint16_t cnt = 0;

	OSM_LOG_ENTER(&p_osmt->log);

	p_port_key_tbl = &p_osmt->exp_subn.port_key_tbl;

	osmtest_prepare_db(p_osmt);

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Validating individual port record queries\n");

	/*
	 * Walk the list of all port records, and ask for each one
	 * specifically.  Make sure we get it.
	 */
	p_port = (port_t *) cl_qmap_head(p_port_key_tbl);
	while (p_port != (port_t *) cl_qmap_end(p_port_key_tbl)) {
		status = osmtest_validate_single_port_rec_lid(p_osmt, p_port);
		if (status != IB_SUCCESS) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 011B: "
				"osmtest_validate_single_port_rec_lid (%s)\n",
				ib_get_err_str(status));
			goto Exit;
		}
		cnt++;
		p_port = (port_t *) cl_qmap_next(&p_port->map_item);
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Total of %u port records validated\n", cnt);

	status = osmtest_check_missing_ports(p_osmt);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0118: "
			"osmtest_check_missing_paths failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t osmtest_validate_against_db(IN osmtest_t * const p_osmt)
{
	ib_api_status_t status = IB_SUCCESS;
	ib_gid_t portgid, mgid;
	osmtest_sm_info_rec_t sm_info_rec_opt;
	osmtest_inform_info_t inform_info_opt;
	osmtest_inform_info_rec_t inform_info_rec_opt;
#ifdef VENDOR_RMPP_SUPPORT
	ib_net64_t sm_key;
	ib_net16_t test_lid;
	uint8_t lmc;
	osmtest_req_context_t context;
#ifdef DUAL_SIDED_RMPP
	osmv_multipath_req_t request;
#endif
	uint8_t i;
#endif

	OSM_LOG_ENTER(&p_osmt->log);

#ifdef VENDOR_RMPP_SUPPORT
	status = osmtest_validate_all_node_recs(p_osmt);
	if (status != IB_SUCCESS)
		goto Exit;
#endif

	status = osmtest_validate_single_node_recs(p_osmt);
	if (status != IB_SUCCESS)
		goto Exit;

	/* Exercise SA PathRecord multicast destination code */
	memset(&context, 0, sizeof(context));
	ib_gid_set_default(&portgid, portguid);
	/* Set IPoIB broadcast MGID */
	mgid.unicast.prefix = CL_HTON64(0xff12401bffff0000ULL);
	mgid.unicast.interface_id = CL_HTON64(0x00000000ffffffffULL);
	/* Can't check status as don't know whether port is running IPoIB */
	osmtest_get_path_rec_by_gid_pair(p_osmt, portgid, mgid, &context);

	/* Other link local unicast PathRecord */
	memset(&context, 0, sizeof(context));
	ib_gid_set_default(&portgid, portguid);
	ib_gid_set_default(&mgid, portguid);
	mgid.raw[7] = 0xff;	/* not default GID prefix */
	/* Can't check status as don't know whether ??? */
	osmtest_get_path_rec_by_gid_pair(p_osmt, portgid, mgid, &context);

	/* Off subnet (site local) unicast PathRecord */
	memset(&context, 0, sizeof(context));
	ib_gid_set_default(&portgid, portguid);
	ib_gid_set_default(&mgid, portguid);
	mgid.raw[1] = 0xc0;	/* site local */
	/* Can't check status as don't know whether ??? */
	osmtest_get_path_rec_by_gid_pair(p_osmt, portgid, mgid, &context);

	/* More than link local scope multicast PathRecord */
	memset(&context, 0, sizeof(context));
	ib_gid_set_default(&portgid, portguid);
	/* Set IPoIB broadcast MGID */
	mgid.unicast.prefix = CL_HTON64(0xff15401bffff0000ULL);	/* site local */
	mgid.unicast.interface_id = CL_HTON64(0x00000000ffffffffULL);
	/* Can't check status as don't know whether port is running IPoIB */
	osmtest_get_path_rec_by_gid_pair(p_osmt, portgid, mgid, &context);

#if defined (VENDOR_RMPP_SUPPORT) && defined (DUAL_SIDED_RMPP)
	memset(&context, 0, sizeof(context));
	memset(&request, 0, sizeof(request));
	request.comp_mask =
	    IB_MPR_COMPMASK_SGIDCOUNT | IB_MPR_COMPMASK_DGIDCOUNT;
	request.sgid_count = 1;
	request.dgid_count = 1;
	ib_gid_set_default(&request.gids[0], portguid);
	ib_gid_set_default(&request.gids[1], portguid);
	status = osmtest_get_multipath_rec(p_osmt, &request, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	memset(&context, 0, sizeof(context));
	memset(&request, 0, sizeof(request));

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmtest_get_multipath_rec(p_osmt, &request, &context);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
			"Got error %s\n", ib_get_err_str(status));
	}
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status == IB_SUCCESS) {
		status = IB_ERROR;
		goto Exit;
	}

	memset(&context, 0, sizeof(context));
	memset(&request, 0, sizeof(request));
	request.comp_mask = IB_MPR_COMPMASK_SGIDCOUNT;
	request.sgid_count = 1;
	ib_gid_set_default(&request.gids[0], portguid);

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmtest_get_multipath_rec(p_osmt, &request, &context);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
			"Got error %s\n", ib_get_err_str(status));
	}
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status == IB_SUCCESS) {
		status = IB_ERROR;
		goto Exit;
	}

	memset(&context, 0, sizeof(context));
	memset(&request, 0, sizeof(request));
	request.comp_mask =
	    IB_MPR_COMPMASK_SGIDCOUNT | IB_MPR_COMPMASK_DGIDCOUNT;
	request.sgid_count = 1;
	request.dgid_count = 1;
	ib_gid_set_default(&request.gids[0], portguid);
	/* Set IPoIB broadcast MGID as DGID */
	request.gids[1].unicast.prefix = CL_HTON64(0xff12401bffff0000ULL);
	request.gids[1].unicast.interface_id = CL_HTON64(0x00000000ffffffffULL);

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmtest_get_multipath_rec(p_osmt, &request, &context);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
			"Got error %s\n", ib_get_err_str(status));
	}
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status == IB_SUCCESS) {
		status = IB_ERROR;
		goto Exit;
	}

	memset(&context, 0, sizeof(context));
	request.comp_mask =
	    IB_MPR_COMPMASK_SGIDCOUNT | IB_MPR_COMPMASK_DGIDCOUNT;
	request.sgid_count = 1;
	request.dgid_count = 1;
	/* Set IPoIB broadcast MGID as SGID */
	request.gids[0].unicast.prefix = CL_HTON64(0xff12401bffff0000ULL);
	request.gids[0].unicast.interface_id = CL_HTON64(0x00000000ffffffffULL);
	ib_gid_set_default(&request.gids[1], portguid);

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmtest_get_multipath_rec(p_osmt, &request, &context);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
			"Got error %s\n", ib_get_err_str(status));
	}
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status == IB_SUCCESS) {
		status = IB_ERROR;
		goto Exit;
	}

	memset(&context, 0, sizeof(context));
	memset(&request, 0, sizeof(request));
	request.comp_mask =
	    IB_MPR_COMPMASK_SGIDCOUNT | IB_MPR_COMPMASK_DGIDCOUNT |
	    IB_MPR_COMPMASK_NUMBPATH;
	request.sgid_count = 2;
	request.dgid_count = 2;
	request.num_path = 2;
	ib_gid_set_default(&request.gids[0], portguid);
	ib_gid_set_default(&request.gids[1], portguid);
	ib_gid_set_default(&request.gids[2], portguid);
	ib_gid_set_default(&request.gids[3], portguid);
	status = osmtest_get_multipath_rec(p_osmt, &request, &context);
	if (status != IB_SUCCESS)
		goto Exit;
#endif

#ifdef VENDOR_RMPP_SUPPORT
	/* GUIDInfoRecords */
	status = osmtest_validate_all_guidinfo_recs(p_osmt);
	if (status != IB_SUCCESS)
		goto Exit;

	/* If LMC > 0, test non base LID SA PortInfoRecord request */
	status =
	    osmtest_get_local_port_lmc(p_osmt, p_osmt->local_port.lid, &lmc);
	if (status != IB_SUCCESS)
		goto Exit;

	if (lmc != 0) {
		status =
		    osmtest_get_local_port_lmc(p_osmt,
					       p_osmt->local_port.lid + 1,
					       NULL);
		if (status != IB_SUCCESS)
			goto Exit;
	}

	status = osmtest_get_local_port_lmc(p_osmt, 0xffff, NULL);
	if (status != IB_SUCCESS)
		goto Exit;

	test_lid = cl_ntoh16(p_osmt->local_port.lid);

	/* More GUIDInfo Record tests */
	memset(&context, 0, sizeof(context));
	status = osmtest_get_guidinfo_rec_by_lid(p_osmt, test_lid, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	memset(&context, 0, sizeof(context));
	status = osmtest_get_guidinfo_rec_by_lid(p_osmt, 0xffff, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* Some PKeyTable Record tests */
	sm_key = OSM_DEFAULT_SM_KEY;
	memset(&context, 0, sizeof(context));
	status =
	    osmtest_get_pkeytbl_rec_by_lid(p_osmt, test_lid, sm_key, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	memset(&context, 0, sizeof(context));

	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_START "\n");
	status = osmtest_get_pkeytbl_rec_by_lid(p_osmt, test_lid, 0, &context);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
			"Got error %s\n", ib_get_err_str(status));
	}
	OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, EXPECTING_ERRORS_END "\n");

	if (status == IB_SUCCESS) {
		status = IB_ERROR;
		goto Exit;
	}

	memset(&context, 0, sizeof(context));
	status =
	    osmtest_get_pkeytbl_rec_by_lid(p_osmt, 0xffff, sm_key, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* SwitchInfo Record tests */
	memset(&context, 0, sizeof(context));
	status = osmtest_get_sw_info_rec_by_lid(p_osmt, 0, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	memset(&context, 0, sizeof(context));
	status = osmtest_get_sw_info_rec_by_lid(p_osmt, test_lid, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* LFT Record tests */
	memset(&context, 0, sizeof(context));
	status = osmtest_get_lft_rec_by_lid(p_osmt, 0, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	memset(&context, 0, sizeof(context));
	status = osmtest_get_lft_rec_by_lid(p_osmt, test_lid, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* MFT Record tests */
	memset(&context, 0, sizeof(context));
	status = osmtest_get_mft_rec_by_lid(p_osmt, 0, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	memset(&context, 0, sizeof(context));
	status = osmtest_get_mft_rec_by_lid(p_osmt, test_lid, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* Some LinkRecord tests */
	/* FromLID */
	memset(&context, 0, sizeof(context));
	status = osmtest_get_link_rec_by_lid(p_osmt, test_lid, 0, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* ToLID */
	memset(&context, 0, sizeof(context));
	status = osmtest_get_link_rec_by_lid(p_osmt, 0, test_lid, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* FromLID & ToLID */
	memset(&context, 0, sizeof(context));
	status =
	    osmtest_get_link_rec_by_lid(p_osmt, test_lid, test_lid, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* NodeRecord test */
	memset(&context, 0, sizeof(context));
	status = osmtest_get_node_rec_by_lid(p_osmt, 0xffff, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* SMInfoRecord tests */
	memset(&sm_info_rec_opt, 0, sizeof(sm_info_rec_opt));
	memset(&context, 0, sizeof(context));
	status = osmtest_sminfo_record_request(p_osmt, IB_MAD_METHOD_SET,
					       &sm_info_rec_opt, &context);
	if (status == IB_SUCCESS) {
		status = IB_ERROR;
		goto Exit;
	} else {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "IS EXPECTED ERROR ^^^^\n");
	}

	memset(&sm_info_rec_opt, 0, sizeof(sm_info_rec_opt));
	memset(&context, 0, sizeof(context));
	status = osmtest_sminfo_record_request(p_osmt, IB_MAD_METHOD_GETTABLE,
					       &sm_info_rec_opt, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	memset(&sm_info_rec_opt, 0, sizeof(sm_info_rec_opt));
	sm_info_rec_opt.lid = test_lid;	/* local LID */
	memset(&context, 0, sizeof(context));
	status = osmtest_sminfo_record_request(p_osmt, IB_MAD_METHOD_GETTABLE,
					       &sm_info_rec_opt, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	if (portguid != 0) {
		memset(&sm_info_rec_opt, 0, sizeof(sm_info_rec_opt));
		sm_info_rec_opt.sm_guid = portguid;	/* local GUID */
		memset(&context, 0, sizeof(context));
		status =
		    osmtest_sminfo_record_request(p_osmt,
						  IB_MAD_METHOD_GETTABLE,
						  &sm_info_rec_opt, &context);
		if (status != IB_SUCCESS)
			goto Exit;
	}

	for (i = 1; i < 16; i++) {
		memset(&sm_info_rec_opt, 0, sizeof(sm_info_rec_opt));
		sm_info_rec_opt.priority = i;
		memset(&context, 0, sizeof(context));
		status =
		    osmtest_sminfo_record_request(p_osmt,
						  IB_MAD_METHOD_GETTABLE,
						  &sm_info_rec_opt, &context);
		if (status != IB_SUCCESS)
			goto Exit;
	}

	for (i = 1; i < 4; i++) {
		memset(&sm_info_rec_opt, 0, sizeof(sm_info_rec_opt));
		sm_info_rec_opt.sm_state = i;
		memset(&context, 0, sizeof(context));
		status =
		    osmtest_sminfo_record_request(p_osmt,
						  IB_MAD_METHOD_GETTABLE,
						  &sm_info_rec_opt, &context);
		if (status != IB_SUCCESS)
			goto Exit;
	}

	/* InformInfoRecord tests */
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "InformInfoRecord "
		"Sending a BAD - Set Unsubscribe request\n");
	memset(&inform_info_opt, 0, sizeof(inform_info_opt));
	memset(&inform_info_rec_opt, 0, sizeof(inform_info_rec_opt));
	memset(&context, 0, sizeof(context));
	status =
	    osmtest_informinfo_request(p_osmt, IB_MAD_ATTR_INFORM_INFO_RECORD,
				       IB_MAD_METHOD_SET, &inform_info_rec_opt,
				       &context);
	if (status == IB_SUCCESS) {
		status = IB_ERROR;
		goto Exit;
	} else {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "InformInfoRecord "
			"IS EXPECTED ERROR ^^^^\n");
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "InformInfoRecord "
		"Sending a Good - Empty GetTable request\n");
	memset(&context, 0, sizeof(context));
	status =
	    osmtest_informinfo_request(p_osmt, IB_MAD_ATTR_INFORM_INFO_RECORD,
				       IB_MAD_METHOD_GETTABLE,
				       &inform_info_rec_opt, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* InformInfo tests */
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "InformInfo "
		"Sending a BAD - Empty Get request "
		"(should fail with NO_RECORDS)\n");
	memset(&context, 0, sizeof(context));
	status = osmtest_informinfo_request(p_osmt, IB_MAD_ATTR_INFORM_INFO,
					    IB_MAD_METHOD_GET, &inform_info_opt,
					    &context);
	if (status == IB_SUCCESS) {
		status = IB_ERROR;
		goto Exit;
	} else {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "InformInfo "
			"IS EXPECTED ERROR ^^^^\n");
	}

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "InformInfo "
		"Sending a BAD - Set Unsubscribe request\n");
	memset(&context, 0, sizeof(context));
	status = osmtest_informinfo_request(p_osmt, IB_MAD_ATTR_INFORM_INFO,
					    IB_MAD_METHOD_SET, &inform_info_opt,
					    &context);
	if (status == IB_SUCCESS) {
		status = IB_ERROR;
		goto Exit;
	} else {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "InformInfo UnSubscribe "
			"IS EXPECTED ERROR ^^^^\n");
	}

	/* Now subscribe */
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "InformInfo "
		"Sending a Good - Set Subscribe request\n");
	inform_info_opt.subscribe = TRUE;
	memset(&context, 0, sizeof(context));
	status = osmtest_informinfo_request(p_osmt, IB_MAD_ATTR_INFORM_INFO,
					    IB_MAD_METHOD_SET, &inform_info_opt,
					    &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* Now unsubscribe (QPN needs to be 1 to work) */
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "InformInfo "
		"Sending a Good - Set Unsubscribe request\n");
	inform_info_opt.subscribe = FALSE;
	inform_info_opt.qpn = 1;
	memset(&context, 0, sizeof(context));
	status = osmtest_informinfo_request(p_osmt, IB_MAD_ATTR_INFORM_INFO,
					    IB_MAD_METHOD_SET, &inform_info_opt,
					    &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* Now subscribe again */
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "InformInfo "
		"Sending a Good - Set Subscribe request\n");
	inform_info_opt.subscribe = TRUE;
	inform_info_opt.qpn = 1;
	memset(&context, 0, sizeof(context));
	status = osmtest_informinfo_request(p_osmt, IB_MAD_ATTR_INFORM_INFO,
					    IB_MAD_METHOD_SET, &inform_info_opt,
					    &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* Subscribe over existing subscription */
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "InformInfo "
		"Sending a Good - Set Subscribe (again) request\n");
	inform_info_opt.qpn = 0;
	memset(&context, 0, sizeof(context));
	status = osmtest_informinfo_request(p_osmt, IB_MAD_ATTR_INFORM_INFO,
					    IB_MAD_METHOD_SET, &inform_info_opt,
					    &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* More InformInfoRecord tests */
	/* RID lookup (with currently invalid enum) */
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "InformInfoRecord "
		"Sending a Good - GetTable by GID\n");
	ib_gid_set_default(&inform_info_rec_opt.subscriber_gid,
			   p_osmt->local_port.port_guid);
	inform_info_rec_opt.subscriber_enum = 1;
	memset(&context, 0, sizeof(context));
	status =
	    osmtest_informinfo_request(p_osmt, IB_MAD_ATTR_INFORM_INFO_RECORD,
				       IB_MAD_METHOD_GETTABLE,
				       &inform_info_rec_opt, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* Enum lookup */
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "InformInfoRecord "
		"Sending a Good - GetTable (subscriber_enum == 0) request\n");
	inform_info_rec_opt.subscriber_enum = 0;
	memset(&context, 0, sizeof(context));
	status =
	    osmtest_informinfo_request(p_osmt, IB_MAD_ATTR_INFORM_INFO_RECORD,
				       IB_MAD_METHOD_GETTABLE,
				       &inform_info_rec_opt, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* Get all InformInfoRecords */
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "InformInfoRecord "
		"Sending a Good - GetTable (ALL records) request\n");
	memset(&inform_info_rec_opt, 0, sizeof(inform_info_rec_opt));
	memset(&context, 0, sizeof(context));
	status =
	    osmtest_informinfo_request(p_osmt, IB_MAD_ATTR_INFORM_INFO_RECORD,
				       IB_MAD_METHOD_GETTABLE,
				       &inform_info_rec_opt, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* Another subscription */
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "InformInfo "
		"Sending another Good - Set Subscribe (again) request\n");
	inform_info_opt.qpn = 0;
	inform_info_opt.trap = 0x1234;
	memset(&context, 0, sizeof(context));
	status = osmtest_informinfo_request(p_osmt, IB_MAD_ATTR_INFORM_INFO,
					    IB_MAD_METHOD_SET, &inform_info_opt,
					    &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* Get all InformInfoRecords again */
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "InformInfoRecord "
		"Sending a Good - GetTable (ALL records) request\n");
	memset(&inform_info_rec_opt, 0, sizeof(inform_info_rec_opt));
	memset(&context, 0, sizeof(context));
	status =
	    osmtest_informinfo_request(p_osmt, IB_MAD_ATTR_INFORM_INFO_RECORD,
				       IB_MAD_METHOD_GETTABLE,
				       &inform_info_rec_opt, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* Cleanup subscriptions before further testing */
	/* Does order of deletion matter ? Test this !!! */
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "InformInfo "
		"Sending a Good - Set (cleanup) request\n");
	inform_info_opt.subscribe = FALSE;
	inform_info_opt.qpn = 1;
	memset(&context, 0, sizeof(context));
	status = osmtest_informinfo_request(p_osmt, IB_MAD_ATTR_INFORM_INFO,
					    IB_MAD_METHOD_SET,
					    &inform_info_opt, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* Get all InformInfoRecords again */
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "InformInfoRecord "
		"Sending a Good - GetTable (ALL records) request\n");
	memset(&inform_info_rec_opt, 0, sizeof(inform_info_rec_opt));
	memset(&context, 0, sizeof(context));
	status =
	    osmtest_informinfo_request(p_osmt, IB_MAD_ATTR_INFORM_INFO_RECORD,
				       IB_MAD_METHOD_GETTABLE,
				       &inform_info_rec_opt, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "InformInfo"
		"Sending a Good - Set (cleanup) request\n");
	inform_info_opt.subscribe = FALSE;
	inform_info_opt.qpn = 1;
	inform_info_opt.trap = 0;
	memset(&context, 0, sizeof(context));
	status = osmtest_informinfo_request(p_osmt, IB_MAD_ATTR_INFORM_INFO,
					    IB_MAD_METHOD_SET,
					    &inform_info_opt, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	/* Get all InformInfoRecords a final time */
	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE, "InformInfoRecord "
		"Sending a Good - GetTable (ALL records) request\n");
	memset(&inform_info_rec_opt, 0, sizeof(inform_info_rec_opt));
	memset(&context, 0, sizeof(context));
	status =
	    osmtest_informinfo_request(p_osmt, IB_MAD_ATTR_INFORM_INFO_RECORD,
				       IB_MAD_METHOD_GETTABLE,
				       &inform_info_rec_opt, &context);
	if (status != IB_SUCCESS)
		goto Exit;

	if (lmc != 0) {
		test_lid = cl_ntoh16(p_osmt->local_port.lid + 1);

		/* Another GUIDInfo Record test */
		memset(&context, 0, sizeof(context));
		status =
		    osmtest_get_guidinfo_rec_by_lid(p_osmt, test_lid, &context);
		if (status != IB_SUCCESS)
			goto Exit;

		/* Another PKeyTable Record test */
		memset(&context, 0, sizeof(context));
		status =
		    osmtest_get_pkeytbl_rec_by_lid(p_osmt, test_lid, sm_key,
						   &context);
		if (status != IB_SUCCESS)
			goto Exit;

		/* Another SwitchInfo Record test */
		memset(&context, 0, sizeof(context));
		status =
		    osmtest_get_sw_info_rec_by_lid(p_osmt, test_lid, &context);
		if (status != IB_SUCCESS)
			goto Exit;

		/* Another LFT Record test */
		memset(&context, 0, sizeof(context));
		status = osmtest_get_lft_rec_by_lid(p_osmt, test_lid, &context);
		if (status != IB_SUCCESS)
			goto Exit;

		/* Another MFT Record test */
		memset(&context, 0, sizeof(context));
		status = osmtest_get_mft_rec_by_lid(p_osmt, test_lid, &context);
		if (status != IB_SUCCESS)
			goto Exit;

		/* More LinkRecord tests */
		/* FromLID */
		memset(&context, 0, sizeof(context));
		status =
		    osmtest_get_link_rec_by_lid(p_osmt, test_lid, 0, &context);
		if (status != IB_SUCCESS)
			goto Exit;

		/* ToLID */
		memset(&context, 0, sizeof(context));
		status =
		    osmtest_get_link_rec_by_lid(p_osmt, 0, test_lid, &context);
		if (status != IB_SUCCESS)
			goto Exit;

		/* Another NodeRecord test */
		memset(&context, 0, sizeof(context));
		status =
		    osmtest_get_node_rec_by_lid(p_osmt, test_lid, &context);
		if (status != IB_SUCCESS)
			goto Exit;
	}

	/* PathRecords */
	if (!p_osmt->opt.ignore_path_records) {
		status = osmtest_validate_all_path_recs(p_osmt);
		if (status != IB_SUCCESS)
			goto Exit;

		if (lmc != 0) {
			memset(&context, 0, sizeof(context));
			status =
			    osmtest_get_path_rec_by_lid_pair(p_osmt, test_lid,
							     test_lid,
							     &context);
			if (status != IB_SUCCESS)
				goto Exit;

			memset(&context, 0, sizeof(context));
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				EXPECTING_ERRORS_START "\n");
			status =
			    osmtest_get_path_rec_by_lid_pair(p_osmt, 0xffff,
							     0xffff, &context);
			if (status != IB_SUCCESS) {
				OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
					"Got error %s\n",
					ib_get_err_str(status));
			}
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				EXPECTING_ERRORS_END "\n");

			if (status == IB_SUCCESS) {
				status = IB_ERROR;
				goto Exit;
			}

			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				EXPECTING_ERRORS_START "\n");

			status =
			    osmtest_get_path_rec_by_lid_pair(p_osmt, test_lid,
							     0xffff, &context);
			if (status != IB_SUCCESS) {
				OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
					"Got error %s\n",
					ib_get_err_str(status));
			}
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
				EXPECTING_ERRORS_END "\n");

			if (status == IB_SUCCESS) {
				status = IB_ERROR;
				goto Exit;
			}
		}
	}
#endif

	status = osmtest_validate_single_port_recs(p_osmt);
	if (status != IB_SUCCESS)
		goto Exit;

	if (!p_osmt->opt.ignore_path_records) {
		status = osmtest_validate_single_path_recs(p_osmt);
		if (status != IB_SUCCESS)
			goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static const osmtest_token_t *str_get_token(IN char *const p_str)
{
	const osmtest_token_t *p_tok;
	uint32_t index = 0;

	p_tok = &token_array[index];

	while (p_tok->val != OSMTEST_TOKEN_UNKNOWN) {
		if (strnicmp(p_str, p_tok->str, p_tok->str_size) == 0)
			return (p_tok);

		p_tok = &token_array[++index];
	}

	return (NULL);
}

/**********************************************************************
   Returns true if not whitespace character encountered before EOL.
**********************************************************************/
static boolean_t
str_skip_white(IN char line[], IN OUT uint32_t * const p_offset)
{
	while (((line[*p_offset] == '\t') ||
		(line[*p_offset] == ' ')) &&
	       (line[*p_offset] != '\n') && (line[*p_offset] != '\0')) {
		++*p_offset;
	}

	if ((line[*p_offset] == '\n') || (line[*p_offset] == '\0'))
		return (FALSE);
	else
		return (TRUE);
}

/**********************************************************************
   Returns true if not whitespace character encountered before EOL.
**********************************************************************/
static void str_skip_token(IN char line[], IN OUT uint32_t * const p_offset)
{
	while ((line[*p_offset] != '\t') &&
	       (line[*p_offset] != ' ') && (line[*p_offset] != '\0')) {
		++*p_offset;
	}
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_parse_node(IN osmtest_t * const p_osmt,
		   IN FILE * const fh, IN OUT uint32_t * const p_line_num)
{
	ib_api_status_t status = IB_SUCCESS;
	uint32_t offset;
	char line[OSMTEST_MAX_LINE_LEN];
	boolean_t done = FALSE;
	node_t *p_node;
	node_t *p_guid_node;
	const osmtest_token_t *p_tok;

	OSM_LOG_ENTER(&p_osmt->log);

	p_node = node_new();
	CL_ASSERT(p_node != NULL);

	/*
	 * Parse the inventory file and create the database.
	 */
	while (!done) {
		if (fgets(line, OSMTEST_MAX_LINE_LEN, fh) == NULL) {
			/*
			 * End of file in the middle of a definition.
			 */
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0119: "
				"Unexpected end of file\n");
			status = IB_ERROR;
			goto Exit;
		}

		++*p_line_num;

		/*
		 * Skip whitespace
		 */
		offset = 0;
		if (!str_skip_white(line, &offset))
			continue;	/* whole line was whitespace */

		p_tok = str_get_token(&line[offset]);
		if (p_tok == NULL) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0120: "
				"Ignoring line %u with unknown token: %s\n",
				*p_line_num, &line[offset]);
			continue;
		}

		OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
			"Found '%s' (line %u)\n", p_tok->str, *p_line_num);

		str_skip_token(line, &offset);

		switch (p_tok->val) {
		case OSMTEST_TOKEN_COMMENT:
			break;

		case OSMTEST_TOKEN_LID:
			p_node->comp.lid = 0xFFFF;
			p_node->rec.lid =
			    cl_hton16((uint16_t)
				      strtoul(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG, "lid = 0x%X\n",
				cl_ntoh16(p_node->rec.lid));
			break;

		case OSMTEST_TOKEN_BASE_VERSION:
			p_node->comp.node_info.base_version = 0xFF;
			p_node->rec.node_info.base_version =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"base_version = 0x%X\n",
				p_node->rec.node_info.base_version);
			break;

		case OSMTEST_TOKEN_CLASS_VERSION:
			p_node->comp.node_info.class_version = 0xFF;
			p_node->rec.node_info.class_version =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"class_version = 0x%X\n",
				p_node->rec.node_info.class_version);
			break;

		case OSMTEST_TOKEN_NODE_TYPE:
			p_node->comp.node_info.node_type = 0xFF;
			p_node->rec.node_info.node_type =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"node_type = 0x%X\n",
				p_node->rec.node_info.node_type);
			break;

		case OSMTEST_TOKEN_NUM_PORTS:
			p_node->comp.node_info.num_ports = 0xFF;
			p_node->rec.node_info.num_ports =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"num_ports = 0x%X\n",
				p_node->rec.node_info.num_ports);
			break;

		case OSMTEST_TOKEN_SYS_GUID:
			p_node->comp.node_info.sys_guid = 0xFFFFFFFFFFFFFFFFULL;
			p_node->rec.node_info.sys_guid =
			    cl_hton64(strtoull(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"sys_guid = 0x%016" PRIx64 "\n",
				cl_ntoh64(p_node->rec.node_info.sys_guid));
			break;

		case OSMTEST_TOKEN_NODE_GUID:
			p_node->comp.node_info.node_guid =
			    0xFFFFFFFFFFFFFFFFULL;
			p_node->rec.node_info.node_guid =
			    cl_hton64(strtoull(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"node_guid = 0x%016" PRIx64 "\n",
				cl_ntoh64(p_node->rec.node_info.node_guid));
			break;

		case OSMTEST_TOKEN_PORT_GUID:
			p_node->comp.node_info.port_guid =
			    0xFFFFFFFFFFFFFFFFULL;
			p_node->rec.node_info.port_guid =
			    cl_hton64(strtoull(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"port_guid = 0x%016" PRIx64 "\n",
				cl_ntoh64(p_node->rec.node_info.port_guid));
			break;

		case OSMTEST_TOKEN_PARTITION_CAP:
			p_node->comp.node_info.partition_cap = 0xFFFF;
			p_node->rec.node_info.partition_cap =
			    cl_hton16((uint16_t)
				      strtoul(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"partition_cap = 0x%X\n",
				cl_ntoh16(p_node->rec.node_info.partition_cap));
			break;

		case OSMTEST_TOKEN_DEVICE_ID:
			p_node->comp.node_info.device_id = 0xFFFF;
			p_node->rec.node_info.device_id = cl_hton16((uint16_t)
								    strtoul
								    (&line
								     [offset],
								     NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"device_id = 0x%X\n",
				cl_ntoh16(p_node->rec.node_info.device_id));
			break;

		case OSMTEST_TOKEN_REVISION:
			p_node->comp.node_info.revision = 0xFFFFFFFF;
			p_node->rec.node_info.revision =
			    cl_hton32(strtoul(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"revision = 0x%X\n",
				cl_ntoh32(p_node->rec.node_info.revision));
			break;

		case OSMTEST_TOKEN_PORT_NUM:
			p_node->comp.node_info.port_num_vendor_id |=
			    IB_NODE_INFO_PORT_NUM_MASK;
			p_node->rec.node_info.port_num_vendor_id |=
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"local_port_num = 0x%X\n",
				ib_node_info_get_local_port_num
				(&p_node->rec.node_info));
			break;

		case OSMTEST_TOKEN_VENDOR_ID:
			p_node->comp.node_info.port_num_vendor_id |=
			    IB_NODE_INFO_VEND_ID_MASK;
			p_node->rec.node_info.port_num_vendor_id |=
			    cl_hton32(strtoul(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"vendor_id = 0x%X\n",
				cl_ntoh32(ib_node_info_get_vendor_id
					  (&p_node->rec.node_info)));
			break;

		case OSMTEST_TOKEN_END:
			done = TRUE;
			break;

		default:
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0121: "
				"Ignoring line %u with unknown token: %s\n",
				*p_line_num, &line[offset]);

			break;
		}
	}

	/*
	 * Make sure the user specified enough information, then
	 * add this object to the database.
	 */
	if (p_node->comp.lid == 0) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0122: "
			"LID must be specified for defined nodes\n");
		node_delete(p_node);
		goto Exit;
	}

	cl_qmap_insert(&p_osmt->exp_subn.node_lid_tbl,
		       p_node->rec.lid, &p_node->map_item);

	p_guid_node = node_new();
	CL_ASSERT(p_node != NULL);

	*p_guid_node = *p_node;

	cl_qmap_insert(&p_osmt->exp_subn.node_guid_tbl,
		       p_guid_node->rec.node_info.node_guid,
		       &p_guid_node->map_item);

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_parse_port(IN osmtest_t * const p_osmt,
		   IN FILE * const fh, IN OUT uint32_t * const p_line_num)
{
	ib_api_status_t status = IB_SUCCESS;
	uint32_t offset;
	char line[OSMTEST_MAX_LINE_LEN];
	boolean_t done = FALSE;
	port_t *p_port;
	const osmtest_token_t *p_tok;

	OSM_LOG_ENTER(&p_osmt->log);

	p_port = port_new();
	CL_ASSERT(p_port != NULL);

	/*
	 * Parse the inventory file and create the database.
	 */
	while (!done) {
		if (fgets(line, OSMTEST_MAX_LINE_LEN, fh) == NULL) {
			/*
			 * End of file in the middle of a definition.
			 */
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0123: "
				"Unexpected end of file\n");
			status = IB_ERROR;
			goto Exit;
		}

		++*p_line_num;

		/*
		 * Skip whitespace
		 */
		offset = 0;
		if (!str_skip_white(line, &offset))
			continue;	/* whole line was whitespace */

		p_tok = str_get_token(&line[offset]);
		if (p_tok == NULL) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0124: "
				"Ignoring line %u with unknown token: %s\n",
				*p_line_num, &line[offset]);
			continue;
		}

		OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
			"Found '%s' (line %u)\n", p_tok->str, *p_line_num);

		str_skip_token(line, &offset);

		switch (p_tok->val) {
		case OSMTEST_TOKEN_COMMENT:
			break;

		case OSMTEST_TOKEN_LID:
			p_port->comp.lid = 0xFFFF;
			p_port->rec.lid =
			    cl_hton16((uint16_t)
				      strtoul(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG, "lid = 0x%X\n",
				cl_ntoh16(p_port->rec.lid));
			break;

		case OSMTEST_TOKEN_PORT_NUM:
			p_port->comp.port_num = 0xFF;
			p_port->rec.port_num =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"port_num = 0x%u\n", p_port->rec.port_num);
			break;

		case OSMTEST_TOKEN_MKEY:
			p_port->comp.port_info.m_key = 0xFFFFFFFFFFFFFFFFULL;
			p_port->rec.port_info.m_key =
			    cl_hton64(strtoull(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"m_key = 0x%016" PRIx64 "\n",
				cl_ntoh64(p_port->rec.port_info.m_key));
			break;

		case OSMTEST_TOKEN_SUBN_PREF:
			p_port->comp.port_info.subnet_prefix =
			    0xFFFFFFFFFFFFFFFFULL;
			p_port->rec.port_info.subnet_prefix =
			    cl_hton64(strtoull(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"subnet_prefix = 0x%016" PRIx64 "\n",
				cl_ntoh64(p_port->rec.port_info.subnet_prefix));
			break;

		case OSMTEST_TOKEN_BASE_LID:
			p_port->comp.port_info.base_lid = 0xFFFF;
			p_port->rec.port_info.base_lid =
			    cl_hton16((uint16_t)
				      strtoul(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"base_lid = 0x%X\n",
				cl_ntoh16(p_port->rec.port_info.base_lid));
			break;

		case OSMTEST_TOKEN_SM_BASE_LID:
			p_port->comp.port_info.master_sm_base_lid = 0xFFFF;
			p_port->rec.port_info.master_sm_base_lid =
			    cl_hton16((uint16_t)
				      strtoul(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"master_sm_base_lid = 0x%X\n",
				cl_ntoh16(p_port->rec.port_info.master_sm_base_lid));
			break;

		case OSMTEST_TOKEN_CAP_MASK:
			p_port->comp.port_info.capability_mask = 0xFFFFFFFF;
			p_port->rec.port_info.capability_mask =
			    cl_hton32((uint32_t)
				      strtoul(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"capability_mask = 0x%X\n",
				cl_ntoh32(p_port->rec.port_info.capability_mask));
			break;

		case OSMTEST_TOKEN_DIAG_CODE:
			p_port->comp.port_info.diag_code = 0xFFFF;
			p_port->rec.port_info.diag_code =
			    cl_hton16((uint16_t)
				      strtoul(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"diag_code = 0x%X\n",
				cl_ntoh16(p_port->rec.port_info.diag_code));
			break;

		case OSMTEST_TOKEN_MKEY_LEASE_PER:
			p_port->comp.port_info.m_key_lease_period = 0xFFFF;
			p_port->rec.port_info.m_key_lease_period =
			    cl_hton16((uint16_t)
				      strtoul(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"m_key_lease_period = 0x%X\n",
				cl_ntoh16(p_port->rec.port_info.m_key_lease_period));
			break;

		case OSMTEST_TOKEN_LOC_PORT_NUM:
			p_port->comp.port_info.local_port_num = 0xFF;
			p_port->rec.port_info.local_port_num =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"local_port_num = 0x%u\n",
				p_port->rec.port_info.local_port_num);
			break;

		case OSMTEST_TOKEN_LINK_WID_EN:
			p_port->comp.port_info.link_width_enabled = 0xFF;
			p_port->rec.port_info.link_width_enabled =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"link_width_enabled = 0x%u\n",
				p_port->rec.port_info.link_width_enabled);
			break;

		case OSMTEST_TOKEN_LINK_WID_SUP:
			p_port->comp.port_info.link_width_supported = 0xFF;
			p_port->rec.port_info.link_width_supported =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"link_width_supported = 0x%u\n",
				p_port->rec.port_info.link_width_supported);
			break;

		case OSMTEST_TOKEN_LINK_WID_ACT:
			p_port->comp.port_info.link_width_active = 0xFF;
			p_port->rec.port_info.link_width_active =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"link_width_active = 0x%u\n",
				p_port->rec.port_info.link_width_active);
			break;

		case OSMTEST_TOKEN_LINK_SPEED_SUP:
			p_port->comp.port_info.state_info1 = 0xFF;
			ib_port_info_set_link_speed_sup((uint8_t)
							strtoul(&line[offset],
								NULL, 0),
							&p_port->rec.port_info);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"link_speed_supported = 0x%u\n",
				ib_port_info_get_link_speed_sup(&p_port->rec.port_info));
			break;

		case OSMTEST_TOKEN_PORT_STATE:
			str_skip_white(line, &offset);
			p_port->comp.port_info.state_info1 = 0xFF;
			ib_port_info_set_port_state(&p_port->rec.port_info,
						    ib_get_port_state_from_str
						    (&line[offset]));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"port_state = 0x%u\n",
				ib_port_info_get_port_state(&p_port->rec.port_info));
			break;

		case OSMTEST_TOKEN_STATE_INFO2:
			p_port->comp.port_info.state_info2 = 0xFF;
			p_port->rec.port_info.state_info2 =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"state_info2 = 0x%u\n",
				p_port->rec.port_info.state_info2);
			break;

		case OSMTEST_TOKEN_MKEY_PROT_BITS:
			p_port->comp.port_info.mkey_lmc = 0xFF;
			ib_port_info_set_mpb(&p_port->rec.port_info,
					     (uint8_t) strtoul(&line[offset],
							       NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG, "mpb = 0x%u\n",
				ib_port_info_get_mpb(&p_port->rec.port_info));
			break;

		case OSMTEST_TOKEN_LMC:
			p_port->comp.port_info.mkey_lmc = 0xFF;
			ib_port_info_set_lmc(&p_port->rec.port_info,
					     (uint8_t) strtoul(&line[offset],
							       NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG, "lmc = 0x%u\n",
				ib_port_info_get_lmc(&p_port->rec.port_info));
			break;

		case OSMTEST_TOKEN_LINK_SPEED:
			p_port->comp.port_info.link_speed = 0xFF;
			p_port->rec.port_info.link_speed =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"link_speed = 0x%u\n",
				p_port->rec.port_info.link_speed);
			break;

		case OSMTEST_TOKEN_MTU_SMSL:
			p_port->comp.port_info.mtu_smsl = 0xFF;
			p_port->rec.port_info.mtu_smsl =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"mtu_smsl = 0x%u\n",
				p_port->rec.port_info.mtu_smsl);
			break;

		case OSMTEST_TOKEN_VL_CAP:
			p_port->comp.port_info.vl_cap = 0xFF;
			p_port->rec.port_info.vl_cap =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG, "vl_cap = 0x%u\n",
				p_port->rec.port_info.vl_cap);
			break;

		case OSMTEST_TOKEN_VL_HIGH_LIMIT:
			p_port->comp.port_info.vl_high_limit = 0xFF;
			p_port->rec.port_info.vl_high_limit =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"vl_high_limit = 0x%u\n",
				p_port->rec.port_info.vl_high_limit);
			break;

		case OSMTEST_TOKEN_VL_ARB_HIGH_CAP:
			p_port->comp.port_info.vl_arb_high_cap = 0xFF;
			p_port->rec.port_info.vl_arb_high_cap =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"vl_arb_high_cap = 0x%u\n",
				p_port->rec.port_info.vl_arb_high_cap);
			break;

		case OSMTEST_TOKEN_VL_ARB_LOW_CAP:
			p_port->comp.port_info.vl_arb_low_cap = 0xFF;
			p_port->rec.port_info.vl_arb_low_cap =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"vl_arb_low_cap = 0x%u\n",
				p_port->rec.port_info.vl_arb_low_cap);
			break;

		case OSMTEST_TOKEN_MTU_CAP:
			p_port->comp.port_info.mtu_cap = 0xFF;
			p_port->rec.port_info.mtu_cap =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG, "mtu_cap = 0x%u\n",
				p_port->rec.port_info.mtu_cap);
			break;

		case OSMTEST_TOKEN_VL_STALL_LIFE:
			p_port->comp.port_info.vl_stall_life = 0xFF;
			p_port->rec.port_info.vl_stall_life =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"vl_stall_life = 0x%u\n",
				p_port->rec.port_info.vl_stall_life);
			break;

		case OSMTEST_TOKEN_VL_ENFORCE:
			p_port->comp.port_info.vl_enforce = 0xFF;
			p_port->rec.port_info.vl_enforce =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"vl_enforce = 0x%u\n",
				p_port->rec.port_info.vl_enforce);
			break;

		case OSMTEST_TOKEN_MKEY_VIOL:
			p_port->comp.port_info.m_key_violations = 0xFFFF;
			p_port->rec.port_info.m_key_violations =
			    cl_hton16((uint16_t)
				      strtoul(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"m_key_violations = 0x%X\n",
				cl_ntoh16(p_port->rec.port_info.m_key_violations));
			break;

		case OSMTEST_TOKEN_PKEY_VIOL:
			p_port->comp.port_info.p_key_violations = 0xFFFF;
			p_port->rec.port_info.p_key_violations =
			    cl_hton16((uint16_t)
				      strtoul(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"p_key_violations = 0x%X\n",
				cl_ntoh16(p_port->rec.port_info.p_key_violations));
			break;

		case OSMTEST_TOKEN_QKEY_VIOL:
			p_port->comp.port_info.q_key_violations = 0xFFFF;
			p_port->rec.port_info.q_key_violations =
			    cl_hton16((uint16_t)
				      strtoul(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"q_key_violations = 0x%X\n",
				cl_ntoh16(p_port->rec.port_info.q_key_violations));
			break;

		case OSMTEST_TOKEN_GUID_CAP:
			p_port->comp.port_info.guid_cap = 0xFF;
			p_port->rec.port_info.guid_cap =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"guid_cap = 0x%u\n",
				p_port->rec.port_info.guid_cap);
			break;

		case OSMTEST_TOKEN_SUBN_TIMEOUT:
			p_port->comp.port_info.subnet_timeout = 0x1F;
			p_port->rec.port_info.subnet_timeout =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"subnet_timeout = 0x%u\n",
				ib_port_info_get_timeout(&p_port->rec.port_info));
			break;

		case OSMTEST_TOKEN_RESP_TIME_VAL:
			p_port->comp.port_info.resp_time_value = 0xFF;
			p_port->rec.port_info.resp_time_value =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"resp_time_value = 0x%u\n",
				p_port->rec.port_info.resp_time_value);
			break;

		case OSMTEST_TOKEN_ERR_THRESHOLD:
			p_port->comp.port_info.error_threshold = 0xFF;
			p_port->rec.port_info.error_threshold =
			    (uint8_t) strtoul(&line[offset], NULL, 0);
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"error_threshold = 0x%u\n",
				p_port->rec.port_info.error_threshold);
			break;

		case OSMTEST_TOKEN_END:
			done = TRUE;
			break;

		default:
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0125: "
				"Ignoring line %u with unknown token: %s\n",
				*p_line_num, &line[offset]);
			break;
		}
	}

	/*
	 * Make sure the user specified enough information, then
	 * add this object to the database.
	 */
	if (p_port->comp.lid == 0) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0126: "
			"LID must be specified for defined ports\n");
		port_delete(p_port);
		status = IB_ERROR;
		goto Exit;
	}

	cl_qmap_insert(&p_osmt->exp_subn.port_key_tbl,
		       port_gen_id(p_port->rec.lid, p_port->rec.port_num),
		       &p_port->map_item);

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_parse_path(IN osmtest_t * const p_osmt,
		   IN FILE * const fh, IN OUT uint32_t * const p_line_num)
{
	ib_api_status_t status = IB_SUCCESS;
	uint32_t offset;
	char line[OSMTEST_MAX_LINE_LEN];
	boolean_t done = FALSE;
	path_t *p_path;
	const osmtest_token_t *p_tok;
	boolean_t got_error = FALSE;

	OSM_LOG_ENTER(&p_osmt->log);

	p_path = path_new();
	CL_ASSERT(p_path != NULL);

	/*
	 * Parse the inventory file and create the database.
	 */
	while (!done) {
		if (fgets(line, OSMTEST_MAX_LINE_LEN, fh) == NULL) {
			/*
			 * End of file in the middle of a definition.
			 */
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0127: "
				"Unexpected end of file\n");
			status = IB_ERROR;
			goto Exit;
		}

		++*p_line_num;

		/*
		 * Skip whitespace
		 */
		offset = 0;
		if (!str_skip_white(line, &offset))
			continue;	/* whole line was whitespace */

		p_tok = str_get_token(&line[offset]);
		if (p_tok == NULL) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0128: "
				"Ignoring line %u with unknown token: %s\n",
				*p_line_num, &line[offset]);
			got_error = TRUE;
			continue;
		}

		OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
			"Found '%s' (line %u)\n", p_tok->str, *p_line_num);

		str_skip_token(line, &offset);

		switch (p_tok->val) {
		case OSMTEST_TOKEN_COMMENT:
			break;

		case OSMTEST_TOKEN_DGID:
			p_path->comp.dgid.unicast.prefix =
			    0xFFFFFFFFFFFFFFFFULL;
			p_path->comp.dgid.unicast.interface_id =
			    0xFFFFFFFFFFFFFFFFULL;

			str_skip_white(line, &offset);
			p_path->rec.dgid.unicast.prefix =
			    cl_hton64(strtoull(&line[offset], NULL, 0));
			str_skip_token(line, &offset);
			p_path->rec.dgid.unicast.interface_id =
			    cl_hton64(strtoull(&line[offset], NULL, 0));

			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"dgid = 0x%016" PRIx64 " 0x%016" PRIx64 "\n",
				cl_ntoh64(p_path->rec.dgid.unicast.prefix),
				cl_ntoh64(p_path->rec.dgid.unicast.interface_id));
			break;

		case OSMTEST_TOKEN_SGID:
			p_path->comp.sgid.unicast.prefix =
			    0xFFFFFFFFFFFFFFFFULL;
			p_path->comp.sgid.unicast.interface_id =
			    0xFFFFFFFFFFFFFFFFULL;

			str_skip_white(line, &offset);
			p_path->rec.sgid.unicast.prefix =
			    cl_hton64(strtoull(&line[offset], NULL, 0));
			str_skip_token(line, &offset);
			p_path->rec.sgid.unicast.interface_id =
			    cl_hton64(strtoull(&line[offset], NULL, 0));

			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
				"sgid = 0x%016" PRIx64 " 0x%016" PRIx64 "\n",
				cl_ntoh64(p_path->rec.sgid.unicast.prefix),
				cl_ntoh64(p_path->rec.sgid.unicast.interface_id));
			break;

		case OSMTEST_TOKEN_DLID:
			p_path->comp.dlid = 0xFFFF;
			p_path->rec.dlid =
			    cl_hton16((uint16_t)
				      strtoul(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG, "dlid = 0x%X\n",
				cl_ntoh16(p_path->rec.dlid));
			break;

		case OSMTEST_TOKEN_SLID:
			p_path->comp.slid = 0xFFFF;
			p_path->rec.slid =
			    cl_hton16((uint16_t)
				      strtoul(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG, "slid = 0x%X\n",
				cl_ntoh16(p_path->rec.slid));
			break;

		case OSMTEST_TOKEN_PKEY:
			p_path->comp.pkey = 0xFFFF;
			p_path->rec.pkey =
			    cl_hton16((uint16_t)
				      strtoul(&line[offset], NULL, 0));
			OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG, "pkey = 0x%X\n",
				cl_ntoh16(p_path->rec.pkey));
			break;

		case OSMTEST_TOKEN_END:
			done = TRUE;
			break;

		default:
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0129: "
				"Ignoring line %u with unknown token: %s\n",
				*p_line_num, &line[offset]);
			got_error = TRUE;
			break;
		}
	}

	if (got_error) {
		status = IB_ERROR;
		goto Exit;
	}
	/*
	 * Make sure the user specified enough information, then
	 * add this object to the database.
	 */
	if (osmtest_path_rec_kay_is_valid(p_osmt, p_path) == FALSE) {
		path_delete(p_path);
		status = IB_ERROR;
		goto Exit;
	}

	cl_qmap_insert(&p_osmt->exp_subn.path_tbl,
		       osmtest_path_rec_key_get(&p_path->rec),
		       &p_path->map_item);

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_parse_link(IN osmtest_t * const p_osmt,
		   IN FILE * const fh, IN OUT uint32_t * const p_line_num)
{
	ib_api_status_t status = IB_SUCCESS;
	uint32_t offset;
	char line[OSMTEST_MAX_LINE_LEN];
	boolean_t done = FALSE;
	const osmtest_token_t *p_tok;
	boolean_t got_error = FALSE;

	OSM_LOG_ENTER(&p_osmt->log);

	/*
	 * Parse the inventory file and create the database.
	 */
	while (!done) {
		if (fgets(line, OSMTEST_MAX_LINE_LEN, fh) == NULL) {
			/*
			 * End of file in the middle of a definition.
			 */
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 012A: "
				"Unexpected end of file\n");
			status = IB_ERROR;
			goto Exit;
		}

		++*p_line_num;

		/*
		 * Skip whitespace
		 */
		offset = 0;
		if (!str_skip_white(line, &offset))
			continue;	/* whole line was whitespace */

		p_tok = str_get_token(&line[offset]);
		if (p_tok == NULL) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 012B: "
				"Ignoring line %u with unknown token: %s\n",
				*p_line_num, &line[offset]);
			got_error = TRUE;
			continue;
		}

		OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
			"Found '%s' (line %u)\n", p_tok->str, *p_line_num);

		str_skip_token(line, &offset);

		switch (p_tok->val) {
		case OSMTEST_TOKEN_FROMLID:
		case OSMTEST_TOKEN_FROMPORTNUM:
		case OSMTEST_TOKEN_TOPORTNUM:
		case OSMTEST_TOKEN_TOLID:
			/* For now */
			break;

		case OSMTEST_TOKEN_END:
			done = TRUE;
			break;

		default:
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 012C: "
				"Ignoring line %u with unknown token: %s\n",
				*p_line_num, &line[offset]);
			got_error = TRUE;
			break;
		}
	}

	if (got_error)
		status = IB_ERROR;

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t osmtest_create_db(IN osmtest_t * const p_osmt)
{
	FILE *fh;
	ib_api_status_t status = IB_SUCCESS;
	uint32_t offset;
	char line[OSMTEST_MAX_LINE_LEN];
	uint32_t line_num = 0;
	const osmtest_token_t *p_tok;
	boolean_t got_error = FALSE;

	OSM_LOG_ENTER(&p_osmt->log);

	fh = fopen(p_osmt->opt.file_name, "r");
	if (fh == NULL) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0130: "
			"Unable to open inventory file (%s)\n",
			p_osmt->opt.file_name);
		status = IB_ERROR;
		goto Exit;
	}

	/*
	 * Parse the inventory file and create the database.
	 */
	while (fgets(line, OSMTEST_MAX_LINE_LEN, fh) != NULL) {
		line_num++;

		/*
		 * Skip whitespace
		 */
		offset = 0;
		if (!str_skip_white(line, &offset))
			continue;	/* whole line was whitespace */

		p_tok = str_get_token(&line[offset]);
		if (p_tok == NULL) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0131: "
				"Ignoring line %u: %s\n", line_num,
				&line[offset]);
			got_error = TRUE;
			continue;
		}

		OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
			"Found '%s' (line %u)\n", p_tok->str, line_num);

		switch (p_tok->val) {
		case OSMTEST_TOKEN_COMMENT:
			break;

		case OSMTEST_TOKEN_DEFINE_NODE:
			status = osmtest_parse_node(p_osmt, fh, &line_num);
			break;

		case OSMTEST_TOKEN_DEFINE_PORT:
			status = osmtest_parse_port(p_osmt, fh, &line_num);
			break;

		case OSMTEST_TOKEN_DEFINE_PATH:
			status = osmtest_parse_path(p_osmt, fh, &line_num);
			break;

		case OSMTEST_TOKEN_DEFINE_LINK:
			status = osmtest_parse_link(p_osmt, fh, &line_num);
			break;

		default:
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0132: "
				"Ignoring line %u: %s\n", line_num,
				&line[offset]);
			got_error = TRUE;
			break;
		}

		if (got_error)
			status = IB_ERROR;

		if (status != IB_SUCCESS) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0133: "
				"Bad status received during parsing (%s)\n",
				ib_get_err_str(status));
			fclose(fh);
			goto Exit;
		}
	}

	fclose(fh);

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
   Returns the index in the local port attribute array for the
   user's selection.
**********************************************************************/
static uint32_t
osmtest_get_user_port(IN osmtest_t * const p_osmt,
		      IN const ib_port_attr_t p_attr_array[],
		      IN uint32_t const num_ports)
{
	uint32_t i;
	uint32_t choice = 0;
	boolean_t done_flag = FALSE;

	OSM_LOG_ENTER(&p_osmt->log);

	/*
	 * User needs prompting for the local port GUID with which
	 * to bind.
	 */

	while (done_flag == FALSE) {
		printf("\nChoose a local port number with which to bind:\n\n");
		for (i = 0; i < num_ports; i++) {
			/*
			 * Print the index + 1 since by convention, port numbers
			 * start with 1 on host channel adapters.
			 */

			printf("\t%u: GUID = 0x%8" PRIx64
			       ", lid = 0x%04X, state = %s\n", i + 1,
			       cl_ntoh64(p_attr_array[i].port_guid),
			       p_attr_array[i].lid,
			       ib_get_port_state_str(p_attr_array[i].
						     link_state));
		}

		printf("\nEnter choice (1-%u): ", i);
		scanf("%u", &choice);
		if (choice > num_ports)
			printf("\nError: Lame choice!\n");
		else
			done_flag = TRUE;

	}
	printf("\n");
	OSM_LOG_EXIT(&p_osmt->log);
	return (choice - 1);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
osmtest_bind(IN osmtest_t * p_osmt,
	     IN uint16_t max_lid, IN ib_net64_t guid OPTIONAL)
{
	uint32_t port_index;
	ib_api_status_t status;
	uint32_t num_ports = GUID_ARRAY_SIZE;
	ib_port_attr_t attr_array[GUID_ARRAY_SIZE];

	OSM_LOG_ENTER(&p_osmt->log);

	/*
	 * Call the transport layer for a list of local port
	 * GUID values.
	 */
	status = osm_vendor_get_all_port_attr(p_osmt->p_vendor,
					      attr_array, &num_ports);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0134: "
			"Failure getting local port attributes (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	if (guid == 0) {
		/*
		 * User needs prompting for the local port GUID with which
		 * to bind.
		 */
		port_index =
		    osmtest_get_user_port(p_osmt, attr_array, num_ports);

		if (num_ports == 0) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0135: "
				"No local ports.  Unable to proceed\n");
			goto Exit;
		}
		guid = attr_array[port_index].port_guid;
	} else {
		for (port_index = 0; port_index < num_ports; port_index++) {
			if (attr_array[port_index].port_guid == guid)
				break;
		}

		if (port_index == num_ports) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0136: "
				"No local port with guid 0x%016" PRIx64 "\n",
				cl_ntoh64(guid));
			status = IB_NOT_FOUND;
			goto Exit;
		}
	}

	/*
	 * Copy the port info for the selected port.
	 */
	memcpy(&p_osmt->local_port, &attr_array[port_index],
	       sizeof(p_osmt->local_port));

	/* bind to the SA */
	OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
		"Using port with SM LID:0x%04X\n", p_osmt->local_port.sm_lid);
	p_osmt->max_lid = max_lid;

	p_osmt->h_bind =
	    osmv_bind_sa(p_osmt->p_vendor, &p_osmt->mad_pool, guid);

	if (p_osmt->h_bind == OSM_BIND_INVALID_HANDLE) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0137: "
			"Unable to bind to SA\n");
		status = IB_ERROR;
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t osmtest_run(IN osmtest_t * const p_osmt)
{
	ib_api_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(&p_osmt->log);

	status = osmtest_validate_sa_class_port_info(p_osmt);
	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0138: "
			"Could not obtain SA ClassPortInfo (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	if (p_osmt->opt.flow == OSMT_FLOW_CREATE_INVENTORY) {
		/*
		 * Creating an inventory file with all nodes, ports and paths
		 */
		status = osmtest_create_inventory_file(p_osmt);
		if (status != IB_SUCCESS) {
			OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0139: "
				"Inventory file create failed (%s)\n",
				ib_get_err_str(status));
			goto Exit;
		}
	} else {
		if (p_osmt->opt.flow == OSMT_FLOW_STRESS_SA) {
			/*
			 * Stress SA - flood the SA with queries
			 */
			switch (p_osmt->opt.stress) {
			case 0:
			case 1:	/* small response SA query stress */
				status = osmtest_stress_small_rmpp(p_osmt);
				if (status != IB_SUCCESS) {
					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"ERR 0140: "
						"Small RMPP stress test failed (%s)\n",
						ib_get_err_str(status));
					goto Exit;
				}
				break;
			case 2:	/* large response SA query stress */
				status = osmtest_stress_large_rmpp(p_osmt);
				if (status != IB_SUCCESS) {
					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"ERR 0141: "
						"Large RMPP stress test failed (%s)\n",
						ib_get_err_str(status));
					goto Exit;
				}
				break;
			case 3:	/* large response Path Record SA query stress */
				status = osmtest_create_db(p_osmt);
				if (status != IB_SUCCESS) {
					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"ERR 0142: "
						"Database creation failed (%s)\n",
						ib_get_err_str(status));
					goto Exit;
				}

				status = osmtest_stress_large_rmpp_pr(p_osmt);
				if (status != IB_SUCCESS) {
					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"ERR 0143: "
						"Large RMPP stress test failed (%s)\n",
						ib_get_err_str(status));
					goto Exit;
				}
				break;
			default:
				OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
					"ERR 0144: "
					"Unknown stress test value %u\n",
					p_osmt->opt.stress);
				break;
			}
		} else {

			/*
			 * Run normal validation tests.
			 */
			if (p_osmt->opt.flow == OSMT_FLOW_ALL ||
			    p_osmt->opt.flow == OSMT_FLOW_VALIDATE_INVENTORY) {
				/*
				 * Only validate the given inventory file
				 */
				status = osmtest_create_db(p_osmt);
				if (status != IB_SUCCESS) {
					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"ERR 0145: "
						"Database creation failed (%s)\n",
						ib_get_err_str(status));
					goto Exit;
				}

				status = osmtest_validate_against_db(p_osmt);
				if (status != IB_SUCCESS) {
					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"ERR 0146: "
						"SA validation database failure (%s)\n",
						ib_get_err_str(status));
					goto Exit;
				}
			}

			if (p_osmt->opt.flow == OSMT_FLOW_ALL) {
				status = osmtest_wrong_sm_key_ignored(p_osmt);
				if (status != IB_SUCCESS) {
					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"ERR 0147: "
						"Try wrong SM_Key failed (%s)\n",
						ib_get_err_str(status));
					goto Exit;
				}
			}

			if (p_osmt->opt.flow == OSMT_FLOW_ALL ||
			    p_osmt->opt.flow == OSMT_FLOW_SERVICE_REGISTRATION)
			{
				/*
				 * run service registration, deregistration, and lease test
				 */
				status = osmt_run_service_records_flow(p_osmt);
				if (status != IB_SUCCESS) {
					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"ERR 0148: "
						"Service Flow failed (%s)\n",
						ib_get_err_str(status));
					goto Exit;
				}
			}

			if (p_osmt->opt.flow == OSMT_FLOW_ALL ||
			    p_osmt->opt.flow == OSMT_FLOW_EVENT_FORWARDING) {
				/*
				 * Run event forwarding test
				 */
#ifdef OSM_VENDOR_INTF_MTL
				status = osmt_run_inform_info_flow(p_osmt);

				if (status != IB_SUCCESS) {
					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"ERR 0149: "
						"Inform Info Flow failed: (%s)\n",
						ib_get_err_str(status));
					goto Exit;
				}
#else
				OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
					"The event forwarding flow "
					"is not implemented yet!\n");
				status = IB_SUCCESS;
				goto Exit;
#endif
			}

			if (p_osmt->opt.flow == OSMT_FLOW_QOS) {
				/*
				 * QoS info: dump VLArb and SLtoVL tables.
				 * Since it generates a huge file, we run it only
				 * if explicitly required to
				 */
				status = osmtest_create_db(p_osmt);
				if (status != IB_SUCCESS) {
					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"ERR 014A: "
						"Database creation failed (%s)\n",
						ib_get_err_str(status));
					goto Exit;
				}

				status =
				    osmt_run_slvl_and_vlarb_records_flow
				    (p_osmt);
				if (status != IB_SUCCESS) {
					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"ERR 0150: "
						"Failed to get SLtoVL and VL Arbitration Tables (%s)\n",
						ib_get_err_str(status));
					goto Exit;
				}
			}

			if (p_osmt->opt.flow == OSMT_FLOW_TRAP) {
				/*
				 * Run trap 64/65 flow (this flow requires running of external tool)
				 */
#ifdef OSM_VENDOR_INTF_MTL
				status = osmt_run_trap64_65_flow(p_osmt);
				if (status != IB_SUCCESS) {
					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"ERR 0151: "
						"Trap 64/65 Flow failed: (%s)\n",
						ib_get_err_str(status));
					goto Exit;
				}
#else
				OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
					"The event forwarding flow "
					"is not implemented yet!\n");
				status = IB_SUCCESS;
				goto Exit;
#endif
			}

			if (p_osmt->opt.flow == OSMT_FLOW_ALL ||
			    p_osmt->opt.flow == OSMT_FLOW_MULTICAST) {
				/*
				 * Multicast flow
				 */
				status = osmt_run_mcast_flow(p_osmt);
				if (status != IB_SUCCESS) {
					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"ERR 0152: "
						"Multicast Flow failed: (%s)\n",
						ib_get_err_str(status));
					goto Exit;
				}
			}

			OSM_LOG(&p_osmt->log, OSM_LOG_INFO,
				"\n\n***************** ALL TESTS PASS *****************\n\n");

		}
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}
