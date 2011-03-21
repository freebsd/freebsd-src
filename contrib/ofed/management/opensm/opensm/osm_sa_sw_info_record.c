/*
 * Copyright (c) 2004-2008 Voltaire, Inc. All rights reserved.
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
 *    Implementation of osm_sir_rcv_t.
 * This object represents the SwitchInfo Receiver object.
 * This object is part of the opensm family of objects.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <string.h>
#include <iba/ib_types.h>
#include <complib/cl_debug.h>
#include <complib/cl_qlist.h>
#include <vendor/osm_vendor_api.h>
#include <opensm/osm_node.h>
#include <opensm/osm_helper.h>
#include <opensm/osm_pkey.h>
#include <opensm/osm_sa.h>

typedef struct osm_sir_item {
	cl_list_item_t list_item;
	ib_switch_info_record_t rec;
} osm_sir_item_t;

typedef struct osm_sir_search_ctxt {
	const ib_switch_info_record_t *p_rcvd_rec;
	ib_net64_t comp_mask;
	cl_qlist_t *p_list;
	osm_sa_t *sa;
	const osm_physp_t *p_req_physp;
} osm_sir_search_ctxt_t;

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
__osm_sir_rcv_new_sir(IN osm_sa_t * sa,
		      IN const osm_switch_t * const p_sw,
		      IN cl_qlist_t * const p_list, IN ib_net16_t const lid)
{
	osm_sir_item_t *p_rec_item;
	ib_api_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(sa->p_log);

	p_rec_item = malloc(sizeof(*p_rec_item));
	if (p_rec_item == NULL) {
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 5308: "
			"rec_item alloc failed\n");
		status = IB_INSUFFICIENT_RESOURCES;
		goto Exit;
	}

	OSM_LOG(sa->p_log, OSM_LOG_DEBUG,
		"New SwitchInfoRecord: lid %u\n", cl_ntoh16(lid));

	memset(p_rec_item, 0, sizeof(*p_rec_item));

	p_rec_item->rec.lid = lid;
	p_rec_item->rec.switch_info = p_sw->switch_info;

	cl_qlist_insert_tail(p_list, &p_rec_item->list_item);

Exit:
	OSM_LOG_EXIT(sa->p_log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static void
__osm_sir_rcv_create_sir(IN osm_sa_t * sa,
			 IN const osm_switch_t * const p_sw,
			 IN cl_qlist_t * const p_list,
			 IN ib_net16_t const match_lid,
			 IN const osm_physp_t * const p_req_physp)
{
	osm_port_t *p_port;
	const osm_physp_t *p_physp;
	uint16_t match_lid_ho;
	ib_net16_t min_lid_ho;
	ib_net16_t max_lid_ho;

	OSM_LOG_ENTER(sa->p_log);

	OSM_LOG(sa->p_log, OSM_LOG_DEBUG,
		"Looking for SwitchInfoRecord with LID: %u\n",
		cl_ntoh16(match_lid));

	/* In switches, the port guid is the node guid. */
	p_port =
	    osm_get_port_by_guid(sa->p_subn, p_sw->p_node->node_info.port_guid);
	if (!p_port) {
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 530A: "
			"Failed to find Port by Node Guid:0x%016" PRIx64
			"\n", cl_ntoh64(p_sw->p_node->node_info.node_guid));
		goto Exit;
	}

	/* check that the requester physp and the current physp are under
	   the same partition. */
	p_physp = p_port->p_physp;
	if (!p_physp) {
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 530B: "
			"Failed to find default physical Port by Node Guid:0x%016"
			PRIx64 "\n",
			cl_ntoh64(p_sw->p_node->node_info.node_guid));
		goto Exit;
	}
	if (!osm_physp_share_pkey(sa->p_log, p_req_physp, p_physp))
		goto Exit;

	/* get the port 0 of the switch */
	osm_port_get_lid_range_ho(p_port, &min_lid_ho, &max_lid_ho);

	match_lid_ho = cl_ntoh16(match_lid);
	if (match_lid_ho) {
		/*
		   We validate that the lid belongs to this switch.
		 */
		OSM_LOG(sa->p_log, OSM_LOG_DEBUG,
			"Comparing LID: %u <= %u <= %u\n",
			min_lid_ho, match_lid_ho, max_lid_ho);

		if (match_lid_ho < min_lid_ho || match_lid_ho > max_lid_ho)
			goto Exit;

	}

	__osm_sir_rcv_new_sir(sa, p_sw, p_list, osm_port_get_base_lid(p_port));

Exit:
	OSM_LOG_EXIT(sa->p_log);
}

/**********************************************************************
 **********************************************************************/
static void
__osm_sir_rcv_by_comp_mask(IN cl_map_item_t * const p_map_item,
			   IN void *context)
{
	const osm_sir_search_ctxt_t *const p_ctxt =
	    (osm_sir_search_ctxt_t *) context;
	const osm_switch_t *const p_sw = (osm_switch_t *) p_map_item;
	const ib_switch_info_record_t *const p_rcvd_rec = p_ctxt->p_rcvd_rec;
	const osm_physp_t *const p_req_physp = p_ctxt->p_req_physp;
	osm_sa_t *sa = p_ctxt->sa;
	ib_net64_t const comp_mask = p_ctxt->comp_mask;
	ib_net16_t match_lid = 0;

	OSM_LOG_ENTER(p_ctxt->sa->p_log);

	osm_dump_switch_info(p_ctxt->sa->p_log,
			     &p_sw->switch_info, OSM_LOG_VERBOSE);

	if (comp_mask & IB_SWIR_COMPMASK_LID) {
		match_lid = p_rcvd_rec->lid;
		if (!match_lid)
			goto Exit;
	}

	__osm_sir_rcv_create_sir(sa, p_sw, p_ctxt->p_list,
				 match_lid, p_req_physp);

Exit:
	OSM_LOG_EXIT(p_ctxt->sa->p_log);
}

/**********************************************************************
 **********************************************************************/
void osm_sir_rcv_process(IN void *ctx, IN void *data)
{
	osm_sa_t *sa = ctx;
	osm_madw_t *p_madw = data;
	const ib_sa_mad_t *sad_mad;
	const ib_switch_info_record_t *p_rcvd_rec;
	cl_qlist_t rec_list;
	osm_sir_search_ctxt_t context;
	osm_physp_t *p_req_physp;

	CL_ASSERT(sa);

	OSM_LOG_ENTER(sa->p_log);

	CL_ASSERT(p_madw);

	sad_mad = osm_madw_get_sa_mad_ptr(p_madw);
	p_rcvd_rec =
	    (ib_switch_info_record_t *) ib_sa_mad_get_payload_ptr(sad_mad);

	CL_ASSERT(sad_mad->attr_id == IB_MAD_ATTR_SWITCH_INFO_RECORD);

	/* we only support SubnAdmGet and SubnAdmGetTable methods */
	if (sad_mad->method != IB_MAD_METHOD_GET &&
	    sad_mad->method != IB_MAD_METHOD_GETTABLE) {
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 5305: "
			"Unsupported Method (%s)\n",
			ib_get_sa_method_str(sad_mad->method));
		osm_sa_send_error(sa, p_madw, IB_MAD_STATUS_UNSUP_METHOD_ATTR);
		goto Exit;
	}

	/* update the requester physical port. */
	p_req_physp = osm_get_physp_by_mad_addr(sa->p_log, sa->p_subn,
						osm_madw_get_mad_addr_ptr
						(p_madw));
	if (p_req_physp == NULL) {
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 5304: "
			"Cannot find requester physical port\n");
		goto Exit;
	}

	if (osm_log_is_active(sa->p_log, OSM_LOG_DEBUG))
		osm_dump_switch_info_record(sa->p_log, p_rcvd_rec,
					    OSM_LOG_DEBUG);

	cl_qlist_init(&rec_list);

	context.p_rcvd_rec = p_rcvd_rec;
	context.p_list = &rec_list;
	context.comp_mask = sad_mad->comp_mask;
	context.sa = sa;
	context.p_req_physp = p_req_physp;

	cl_plock_acquire(sa->p_lock);

	/* Go over all switches */
	cl_qmap_apply_func(&sa->p_subn->sw_guid_tbl,
			   __osm_sir_rcv_by_comp_mask, &context);

	cl_plock_release(sa->p_lock);

	osm_sa_respond(sa, p_madw, sizeof(ib_switch_info_record_t), &rec_list);

Exit:
	OSM_LOG_EXIT(sa->p_log);
}
