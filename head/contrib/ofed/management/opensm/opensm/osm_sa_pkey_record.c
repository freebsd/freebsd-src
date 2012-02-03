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

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <string.h>
#include <iba/ib_types.h>
#include <complib/cl_qmap.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_debug.h>
#include <complib/cl_qlist.h>
#include <vendor/osm_vendor_api.h>
#include <opensm/osm_port.h>
#include <opensm/osm_node.h>
#include <opensm/osm_helper.h>
#include <opensm/osm_pkey.h>
#include <opensm/osm_sa.h>

typedef struct osm_pkey_item {
	cl_list_item_t list_item;
	ib_pkey_table_record_t rec;
} osm_pkey_item_t;

typedef struct osm_pkey_search_ctxt {
	const ib_pkey_table_record_t *p_rcvd_rec;
	ib_net64_t comp_mask;
	uint16_t block_num;
	cl_qlist_t *p_list;
	osm_sa_t *sa;
	const osm_physp_t *p_req_physp;
} osm_pkey_search_ctxt_t;

/**********************************************************************
 **********************************************************************/
static void
__osm_sa_pkey_create(IN osm_sa_t * sa,
		     IN osm_physp_t * const p_physp,
		     IN osm_pkey_search_ctxt_t * const p_ctxt,
		     IN uint16_t block)
{
	osm_pkey_item_t *p_rec_item;
	uint16_t lid;
	ib_api_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(sa->p_log);

	p_rec_item = malloc(sizeof(*p_rec_item));
	if (p_rec_item == NULL) {
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 4602: "
			"rec_item alloc failed\n");
		status = IB_INSUFFICIENT_RESOURCES;
		goto Exit;
	}

	if (p_physp->p_node->node_info.node_type != IB_NODE_TYPE_SWITCH)
		lid = p_physp->port_info.base_lid;
	else
		lid = osm_node_get_base_lid(p_physp->p_node, 0);

	OSM_LOG(sa->p_log, OSM_LOG_DEBUG,
		"New P_Key table for: port 0x%016" PRIx64
		", lid %u, port %u Block:%u\n",
		cl_ntoh64(osm_physp_get_port_guid(p_physp)),
		cl_ntoh16(lid), osm_physp_get_port_num(p_physp), block);

	memset(p_rec_item, 0, sizeof(*p_rec_item));

	p_rec_item->rec.lid = lid;
	p_rec_item->rec.block_num = block;
	p_rec_item->rec.port_num = osm_physp_get_port_num(p_physp);
	p_rec_item->rec.pkey_tbl =
	    *(osm_pkey_tbl_block_get(osm_physp_get_pkey_tbl(p_physp), block));

	cl_qlist_insert_tail(p_ctxt->p_list, &p_rec_item->list_item);

Exit:
	OSM_LOG_EXIT(sa->p_log);
}

/**********************************************************************
 **********************************************************************/
static void
__osm_sa_pkey_check_physp(IN osm_sa_t * sa,
			  IN osm_physp_t * const p_physp,
			  osm_pkey_search_ctxt_t * const p_ctxt)
{
	ib_net64_t comp_mask = p_ctxt->comp_mask;
	uint16_t block, num_blocks;

	OSM_LOG_ENTER(sa->p_log);

	/* we got here with the phys port - all is left is to get the right block */
	if (comp_mask & IB_PKEY_COMPMASK_BLOCK) {
		__osm_sa_pkey_create(sa, p_physp, p_ctxt, p_ctxt->block_num);
	} else {
		num_blocks =
		    osm_pkey_tbl_get_num_blocks(osm_physp_get_pkey_tbl
						(p_physp));
		for (block = 0; block < num_blocks; block++) {
			__osm_sa_pkey_create(sa, p_physp, p_ctxt, block);
		}
	}

	OSM_LOG_EXIT(sa->p_log);
}

/**********************************************************************
 **********************************************************************/
static void
__osm_sa_pkey_by_comp_mask(IN osm_sa_t * sa,
			   IN const osm_port_t * const p_port,
			   osm_pkey_search_ctxt_t * const p_ctxt)
{
	const ib_pkey_table_record_t *p_rcvd_rec;
	ib_net64_t comp_mask;
	osm_physp_t *p_physp;
	uint8_t port_num;
	uint8_t num_ports;
	const osm_physp_t *p_req_physp;

	OSM_LOG_ENTER(sa->p_log);

	p_rcvd_rec = p_ctxt->p_rcvd_rec;
	comp_mask = p_ctxt->comp_mask;
	port_num = p_rcvd_rec->port_num;
	p_req_physp = p_ctxt->p_req_physp;

	/* if this is a switch port we can search all ports
	   otherwise we must be looking on port 0 */
	if (p_port->p_node->node_info.node_type != IB_NODE_TYPE_SWITCH) {
		/* we put it in the comp mask and port num */
		port_num = p_port->p_physp->port_num;
		OSM_LOG(sa->p_log, OSM_LOG_DEBUG,
			"Using Physical Default Port Number: 0x%X (for End Node)\n",
			port_num);
		comp_mask |= IB_PKEY_COMPMASK_PORT;
	}

	if (comp_mask & IB_PKEY_COMPMASK_PORT) {
		if (port_num < osm_node_get_num_physp(p_port->p_node)) {
			p_physp =
			    osm_node_get_physp_ptr(p_port->p_node, port_num);
			/* Check that the p_physp is valid, and that is shares a pkey
			   with the p_req_physp. */
			if (p_physp &&
			    (osm_physp_share_pkey
			     (sa->p_log, p_req_physp, p_physp)))
				__osm_sa_pkey_check_physp(sa, p_physp,
							  p_ctxt);
		} else {
			OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 4603: "
				"Given Physical Port Number: 0x%X is out of range should be < 0x%X\n",
				port_num,
				osm_node_get_num_physp(p_port->p_node));
			goto Exit;
		}
	} else {
		num_ports = osm_node_get_num_physp(p_port->p_node);
		for (port_num = 0; port_num < num_ports; port_num++) {
			p_physp =
			    osm_node_get_physp_ptr(p_port->p_node, port_num);
			if (!p_physp)
				continue;

			/* if the requester and the p_physp don't share a pkey -
			   continue */
			if (!osm_physp_share_pkey
			    (sa->p_log, p_req_physp, p_physp))
				continue;

			__osm_sa_pkey_check_physp(sa, p_physp, p_ctxt);
		}
	}
Exit:
	OSM_LOG_EXIT(sa->p_log);
}

/**********************************************************************
 **********************************************************************/
static void
__osm_sa_pkey_by_comp_mask_cb(IN cl_map_item_t * const p_map_item,
			      IN void *context)
{
	const osm_port_t *const p_port = (osm_port_t *) p_map_item;
	osm_pkey_search_ctxt_t *const p_ctxt =
	    (osm_pkey_search_ctxt_t *) context;

	__osm_sa_pkey_by_comp_mask(p_ctxt->sa, p_port, p_ctxt);
}

/**********************************************************************
 **********************************************************************/
void osm_pkey_rec_rcv_process(IN void *ctx, IN void *data)
{
	osm_sa_t *sa = ctx;
	osm_madw_t *p_madw = data;
	const ib_sa_mad_t *p_rcvd_mad;
	const ib_pkey_table_record_t *p_rcvd_rec;
	const osm_port_t *p_port = NULL;
	const ib_pkey_table_t *p_pkey;
	cl_qlist_t rec_list;
	osm_pkey_search_ctxt_t context;
	ib_api_status_t status = IB_SUCCESS;
	ib_net64_t comp_mask;
	osm_physp_t *p_req_physp;

	CL_ASSERT(sa);

	OSM_LOG_ENTER(sa->p_log);

	CL_ASSERT(p_madw);

	p_rcvd_mad = osm_madw_get_sa_mad_ptr(p_madw);
	p_rcvd_rec =
	    (ib_pkey_table_record_t *) ib_sa_mad_get_payload_ptr(p_rcvd_mad);
	comp_mask = p_rcvd_mad->comp_mask;

	CL_ASSERT(p_rcvd_mad->attr_id == IB_MAD_ATTR_PKEY_TBL_RECORD);

	/* we only support SubnAdmGet and SubnAdmGetTable methods */
	if (p_rcvd_mad->method != IB_MAD_METHOD_GET &&
	    p_rcvd_mad->method != IB_MAD_METHOD_GETTABLE) {
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 4605: "
			"Unsupported Method (%s)\n",
			ib_get_sa_method_str(p_rcvd_mad->method));
		osm_sa_send_error(sa, p_madw, IB_MAD_STATUS_UNSUP_METHOD_ATTR);
		goto Exit;
	}

	/*
	   p922 - P_KeyTableRecords shall only be provided in response
	   to trusted requests.
	   Check that the requester is a trusted one.
	 */
	if (p_rcvd_mad->sm_key != sa->p_subn->opt.sa_key) {
		/* This is not a trusted requester! */
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 4608: "
			"Request from non-trusted requester: "
			"Given SM_Key:0x%016" PRIx64 "\n",
			cl_ntoh64(p_rcvd_mad->sm_key));
		osm_sa_send_error(sa, p_madw, IB_SA_MAD_STATUS_REQ_INVALID);
		goto Exit;
	}

	/* update the requester physical port. */
	p_req_physp = osm_get_physp_by_mad_addr(sa->p_log, sa->p_subn,
						osm_madw_get_mad_addr_ptr
						(p_madw));
	if (p_req_physp == NULL) {
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 4604: "
			"Cannot find requester physical port\n");
		goto Exit;
	}

	p_pkey = (ib_pkey_table_t *) ib_sa_mad_get_payload_ptr(p_rcvd_mad);

	cl_qlist_init(&rec_list);

	context.p_rcvd_rec = p_rcvd_rec;
	context.p_list = &rec_list;
	context.comp_mask = p_rcvd_mad->comp_mask;
	context.sa = sa;
	context.block_num = p_rcvd_rec->block_num;
	context.p_req_physp = p_req_physp;

	OSM_LOG(sa->p_log, OSM_LOG_DEBUG,
		"Got Query Lid:%u(%02X), Block:0x%02X(%02X), Port:0x%02X(%02X)\n",
		cl_ntoh16(p_rcvd_rec->lid),
		(comp_mask & IB_PKEY_COMPMASK_LID) != 0, p_rcvd_rec->port_num,
		(comp_mask & IB_PKEY_COMPMASK_PORT) != 0, p_rcvd_rec->block_num,
		(comp_mask & IB_PKEY_COMPMASK_BLOCK) != 0);

	cl_plock_acquire(sa->p_lock);

	/*
	   If the user specified a LID, it obviously narrows our
	   work load, since we don't have to search every port
	 */
	if (comp_mask & IB_PKEY_COMPMASK_LID) {
		status = osm_get_port_by_base_lid(sa->p_subn, p_rcvd_rec->lid,
						  &p_port);
		if (status != IB_SUCCESS || p_port == NULL) {
			status = IB_NOT_FOUND;
			OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 460B: "
				"No port found with LID %u\n",
				cl_ntoh16(p_rcvd_rec->lid));
		}
	}

	if (status == IB_SUCCESS) {
		/* if we got a unique port - no need for a port search */
		if (p_port)
			/* this does the loop on all the port phys ports */
			__osm_sa_pkey_by_comp_mask(sa, p_port, &context);
		else
			cl_qmap_apply_func(&sa->p_subn->port_guid_tbl,
					   __osm_sa_pkey_by_comp_mask_cb,
					   &context);
	}

	cl_plock_release(sa->p_lock);

	osm_sa_respond(sa, p_madw, sizeof(ib_pkey_table_record_t), &rec_list);

Exit:
	OSM_LOG_EXIT(sa->p_log);
}
