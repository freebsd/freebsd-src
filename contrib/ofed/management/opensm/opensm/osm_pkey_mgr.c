/*
 * Copyright (c) 2004-2008 Voltaire, Inc. All rights reserved.
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
 * Implementation of the P_Key Manager (Partititon Manager).
 * This is part of the OpenSM.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <string.h>
#include <iba/ib_types.h>
#include <complib/cl_qmap.h>
#include <complib/cl_debug.h>
#include <opensm/osm_node.h>
#include <opensm/osm_switch.h>
#include <opensm/osm_partition.h>
#include <opensm/osm_opensm.h>

/**********************************************************************
 **********************************************************************/
/*
  The max number of pkey blocks for a physical port is located in
  a different place for switch external ports (SwitchInfo) and the
  rest of the ports (NodeInfo).
*/
static uint16_t
pkey_mgr_get_physp_max_blocks(IN const osm_subn_t * p_subn,
			      IN const osm_physp_t * p_physp)
{
	osm_node_t *p_node = osm_physp_get_node_ptr(p_physp);
	uint16_t num_pkeys = 0;

	if (!p_node->sw || (osm_physp_get_port_num(p_physp) == 0))
		num_pkeys = cl_ntoh16(p_node->node_info.partition_cap);
	else
		num_pkeys = cl_ntoh16(p_node->sw->switch_info.enforce_cap);
	return ((num_pkeys + 31) / 32);
}

/**********************************************************************
 **********************************************************************/
/*
 * Insert new pending pkey entry to the specific port pkey table
 * pending pkeys. New entries are inserted at the back.
 */
static void
pkey_mgr_process_physical_port(IN osm_log_t * p_log,
			       IN osm_sm_t * sm,
			       IN const ib_net16_t pkey,
			       IN osm_physp_t * p_physp)
{
	osm_node_t *p_node = osm_physp_get_node_ptr(p_physp);
	osm_pkey_tbl_t *p_pkey_tbl;
	ib_net16_t *p_orig_pkey;
	char *stat = NULL;
	osm_pending_pkey_t *p_pending;

	p_pkey_tbl = &p_physp->pkeys;
	p_pending = (osm_pending_pkey_t *) malloc(sizeof(osm_pending_pkey_t));
	if (!p_pending) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0502: "
			"Failed to allocate new pending pkey entry for node "
			"0x%016" PRIx64 " port %u\n",
			cl_ntoh64(osm_node_get_node_guid(p_node)),
			osm_physp_get_port_num(p_physp));
		return;
	}
	p_pending->pkey = pkey;
	p_orig_pkey = cl_map_get(&p_pkey_tbl->keys, ib_pkey_get_base(pkey));
	if (!p_orig_pkey) {
		p_pending->is_new = TRUE;
		cl_qlist_insert_tail(&p_pkey_tbl->pending,
				     (cl_list_item_t *) p_pending);
		stat = "inserted";
	} else {
		CL_ASSERT(ib_pkey_get_base(*p_orig_pkey) ==
			  ib_pkey_get_base(pkey));
		p_pending->is_new = FALSE;
		if (osm_pkey_tbl_get_block_and_idx(p_pkey_tbl, p_orig_pkey,
						   &p_pending->block,
						   &p_pending->index) !=
		    IB_SUCCESS) {
			OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0503: "
				"Failed to obtain P_Key 0x%04x block and index for node "
				"0x%016" PRIx64 " port %u\n",
				ib_pkey_get_base(pkey),
				cl_ntoh64(osm_node_get_node_guid(p_node)),
				osm_physp_get_port_num(p_physp));
			return;
		}
		cl_qlist_insert_head(&p_pkey_tbl->pending,
				     (cl_list_item_t *) p_pending);
		stat = "updated";
	}

	OSM_LOG(p_log, OSM_LOG_DEBUG,
		"pkey 0x%04x was %s for node 0x%016" PRIx64 " port %u\n",
		cl_ntoh16(pkey), stat,
		cl_ntoh64(osm_node_get_node_guid(p_node)),
		osm_physp_get_port_num(p_physp));
}

/**********************************************************************
 **********************************************************************/
static void
pkey_mgr_process_partition_table(osm_log_t * p_log, osm_sm_t * sm,
				 const osm_prtn_t * p_prtn,
				 const boolean_t full)
{
	const cl_map_t *p_tbl =
	    full ? &p_prtn->full_guid_tbl : &p_prtn->part_guid_tbl;
	cl_map_iterator_t i, i_next;
	ib_net16_t pkey = p_prtn->pkey;
	osm_physp_t *p_physp;

	if (full)
		pkey |= cl_hton16(0x8000);

	i_next = cl_map_head(p_tbl);
	while (i_next != cl_map_end(p_tbl)) {
		i = i_next;
		i_next = cl_map_next(i);
		p_physp = cl_map_obj(i);
		if (p_physp)
			pkey_mgr_process_physical_port(p_log, sm, pkey,
						       p_physp);
	}
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
pkey_mgr_update_pkey_entry(IN osm_sm_t * sm,
			   IN const osm_physp_t * p_physp,
			   IN const ib_pkey_table_t * block,
			   IN const uint16_t block_index)
{
	osm_madw_context_t context;
	osm_node_t *p_node = osm_physp_get_node_ptr(p_physp);
	uint32_t attr_mod;

	context.pkey_context.node_guid = osm_node_get_node_guid(p_node);
	context.pkey_context.port_guid = osm_physp_get_port_guid(p_physp);
	context.pkey_context.set_method = TRUE;
	attr_mod = block_index;
	if (osm_node_get_type(p_node) == IB_NODE_TYPE_SWITCH)
		attr_mod |= osm_physp_get_port_num(p_physp) << 16;
	return osm_req_set(sm, osm_physp_get_dr_path_ptr(p_physp),
			   (uint8_t *) block, sizeof(*block),
			   IB_MAD_ATTR_P_KEY_TABLE,
			   cl_hton32(attr_mod), CL_DISP_MSGID_NONE, &context);
}

/**********************************************************************
 **********************************************************************/
static boolean_t
pkey_mgr_enforce_partition(IN osm_log_t * p_log, osm_sm_t * sm,
			   IN osm_physp_t * p_physp, IN const boolean_t enforce)
{
	osm_madw_context_t context;
	uint8_t payload[IB_SMP_DATA_SIZE];
	ib_port_info_t *p_pi;
	ib_api_status_t status;

	p_pi = &p_physp->port_info;

	if ((p_pi->vl_enforce & 0xc) == (0xc) * (enforce == TRUE)) {
		OSM_LOG(p_log, OSM_LOG_DEBUG,
			"No need to update PortInfo for "
			"node 0x%016" PRIx64 " port %u\n",
			cl_ntoh64(osm_node_get_node_guid
				  (osm_physp_get_node_ptr(p_physp))),
			osm_physp_get_port_num(p_physp));
		return FALSE;
	}

	memset(payload, 0, IB_SMP_DATA_SIZE);
	memcpy(payload, p_pi, sizeof(ib_port_info_t));

	p_pi = (ib_port_info_t *) payload;
	if (enforce == TRUE)
		p_pi->vl_enforce |= 0xc;
	else
		p_pi->vl_enforce &= ~0xc;
	p_pi->state_info2 = 0;
	ib_port_info_set_port_state(p_pi, IB_LINK_NO_CHANGE);

	context.pi_context.node_guid =
	    osm_node_get_node_guid(osm_physp_get_node_ptr(p_physp));
	context.pi_context.port_guid = osm_physp_get_port_guid(p_physp);
	context.pi_context.set_method = TRUE;
	context.pi_context.light_sweep = FALSE;
	context.pi_context.active_transition = FALSE;

	status = osm_req_set(sm, osm_physp_get_dr_path_ptr(p_physp),
			     payload, sizeof(payload),
			     IB_MAD_ATTR_PORT_INFO,
			     cl_hton32(osm_physp_get_port_num(p_physp)),
			     CL_DISP_MSGID_NONE, &context);
	if (status != IB_SUCCESS) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0511: "
			"Failed to set PortInfo for "
			"node 0x%016" PRIx64 " port %u\n",
			cl_ntoh64(osm_node_get_node_guid
				  (osm_physp_get_node_ptr(p_physp))),
			osm_physp_get_port_num(p_physp));
		return FALSE;
	} else {
		OSM_LOG(p_log, OSM_LOG_DEBUG,
			"Set PortInfo for node 0x%016" PRIx64 " port %u\n",
			cl_ntoh64(osm_node_get_node_guid
				  (osm_physp_get_node_ptr(p_physp))),
			osm_physp_get_port_num(p_physp));
		return TRUE;
	}
}

/**********************************************************************
 **********************************************************************/
static boolean_t pkey_mgr_update_port(osm_log_t * p_log, osm_sm_t * sm,
				      const osm_port_t * const p_port)
{
	osm_physp_t *p_physp;
	osm_node_t *p_node;
	ib_pkey_table_t *block, *new_block;
	osm_pkey_tbl_t *p_pkey_tbl;
	uint16_t block_index;
	uint8_t pkey_index;
	uint16_t last_free_block_index = 0;
	uint8_t last_free_pkey_index = 0;
	uint16_t num_of_blocks;
	uint16_t max_num_of_blocks;
	ib_api_status_t status;
	boolean_t ret_val = FALSE;
	osm_pending_pkey_t *p_pending;
	boolean_t found;
	ib_pkey_table_t empty_block;

	memset(&empty_block, 0, sizeof(ib_pkey_table_t));

	p_physp = p_port->p_physp;
	if (!p_physp)
		return FALSE;

	p_node = osm_physp_get_node_ptr(p_physp);
	p_pkey_tbl = &p_physp->pkeys;
	num_of_blocks = osm_pkey_tbl_get_num_blocks(p_pkey_tbl);
	max_num_of_blocks =
	    pkey_mgr_get_physp_max_blocks(sm->p_subn, p_physp);
	if (p_pkey_tbl->max_blocks > max_num_of_blocks) {
		OSM_LOG(p_log, OSM_LOG_INFO,
			"Max number of blocks reduced from %u to %u "
			"for node 0x%016" PRIx64 " port %u\n",
			p_pkey_tbl->max_blocks, max_num_of_blocks,
			cl_ntoh64(osm_node_get_node_guid(p_node)),
			osm_physp_get_port_num(p_physp));
	}
	p_pkey_tbl->max_blocks = max_num_of_blocks;

	osm_pkey_tbl_init_new_blocks(p_pkey_tbl);
	p_pkey_tbl->used_blocks = 0;

	/*
	   process every pending pkey in order -
	   first must be "updated" last are "new"
	 */
	p_pending =
	    (osm_pending_pkey_t *) cl_qlist_remove_head(&p_pkey_tbl->pending);
	while (p_pending !=
	       (osm_pending_pkey_t *) cl_qlist_end(&p_pkey_tbl->pending)) {
		if (p_pending->is_new == FALSE) {
			block_index = p_pending->block;
			pkey_index = p_pending->index;
			found = TRUE;
		} else {
			found = osm_pkey_find_next_free_entry(p_pkey_tbl,
							      &last_free_block_index,
							      &last_free_pkey_index);
			if (!found) {
				OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0504: "
					"Failed to find empty space for new pkey 0x%04x "
					"for node 0x%016" PRIx64 " port %u\n",
					cl_ntoh16(p_pending->pkey),
					cl_ntoh64(osm_node_get_node_guid
						  (p_node)),
					osm_physp_get_port_num(p_physp));
			} else {
				block_index = last_free_block_index;
				pkey_index = last_free_pkey_index++;
			}
		}

		if (found) {
			if (IB_SUCCESS !=
			    osm_pkey_tbl_set_new_entry(p_pkey_tbl, block_index,
						       pkey_index,
						       p_pending->pkey)) {
				OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0505: "
					"Failed to set PKey 0x%04x in block %u idx %u "
					"for node 0x%016" PRIx64 " port %u\n",
					cl_ntoh16(p_pending->pkey), block_index,
					pkey_index,
					cl_ntoh64(osm_node_get_node_guid
						  (p_node)),
					osm_physp_get_port_num(p_physp));
			}
		}

		free(p_pending);
		p_pending =
		    (osm_pending_pkey_t *) cl_qlist_remove_head(&p_pkey_tbl->
								pending);
	}

	/* now look for changes and store */
	for (block_index = 0; block_index < num_of_blocks; block_index++) {
		block = osm_pkey_tbl_block_get(p_pkey_tbl, block_index);
		new_block = osm_pkey_tbl_new_block_get(p_pkey_tbl, block_index);
		if (!new_block)
			new_block = &empty_block;
		if (block && !memcmp(new_block, block, sizeof(*block)))
			continue;

		status =
		    pkey_mgr_update_pkey_entry(sm, p_physp, new_block,
					       block_index);
		if (status == IB_SUCCESS) {
			OSM_LOG(p_log, OSM_LOG_DEBUG,
				"Updated pkey table block %d for node 0x%016"
				PRIx64 " port %u\n", block_index,
				cl_ntoh64(osm_node_get_node_guid(p_node)),
				osm_physp_get_port_num(p_physp));
			ret_val = TRUE;
		} else {
			OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0506: "
				"pkey_mgr_update_pkey_entry() failed to update "
				"pkey table block %d for node 0x%016" PRIx64
				" port %u\n", block_index,
				cl_ntoh64(osm_node_get_node_guid(p_node)),
				osm_physp_get_port_num(p_physp));
		}
	}

	return ret_val;
}

/**********************************************************************
 **********************************************************************/
static boolean_t
pkey_mgr_update_peer_port(osm_log_t * p_log, osm_sm_t * sm,
			  const osm_subn_t * p_subn,
			  const osm_port_t * const p_port, boolean_t enforce)
{
	osm_physp_t *p_physp, *peer;
	osm_node_t *p_node;
	ib_pkey_table_t *block, *peer_block;
	const osm_pkey_tbl_t *p_pkey_tbl;
	osm_pkey_tbl_t *p_peer_pkey_tbl;
	uint16_t block_index;
	uint16_t num_of_blocks;
	uint16_t peer_max_blocks;
	ib_api_status_t status = IB_SUCCESS;
	boolean_t ret_val = FALSE;
	boolean_t port_info_set = FALSE;
	ib_pkey_table_t empty_block;

	memset(&empty_block, 0, sizeof(ib_pkey_table_t));

	p_physp = p_port->p_physp;
	if (!p_physp)
		return FALSE;
	peer = osm_physp_get_remote(p_physp);
	if (!peer)
		return FALSE;
	p_node = osm_physp_get_node_ptr(peer);
	if (!p_node->sw || !p_node->sw->switch_info.enforce_cap)
		return FALSE;

	p_pkey_tbl = osm_physp_get_pkey_tbl(p_physp);
	p_peer_pkey_tbl = &peer->pkeys;
	num_of_blocks = osm_pkey_tbl_get_num_blocks(p_pkey_tbl);
	peer_max_blocks = pkey_mgr_get_physp_max_blocks(p_subn, peer);
	if (peer_max_blocks < p_pkey_tbl->used_blocks) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0508: "
			"Not enough pkey entries (%u < %u) on switch 0x%016"
			PRIx64 " port %u. Clearing Enforcement bit\n",
			peer_max_blocks, num_of_blocks,
			cl_ntoh64(osm_node_get_node_guid(p_node)),
			osm_physp_get_port_num(peer));
		enforce = FALSE;
	}

	if (pkey_mgr_enforce_partition(p_log, sm, peer, enforce))
		port_info_set = TRUE;

	if (enforce == FALSE)
		return port_info_set;

	p_peer_pkey_tbl->used_blocks = p_pkey_tbl->used_blocks;
	for (block_index = 0; block_index < p_pkey_tbl->used_blocks;
	     block_index++) {
		block = osm_pkey_tbl_new_block_get(p_pkey_tbl, block_index);
		if (!block)
			block = &empty_block;

		peer_block =
		    osm_pkey_tbl_block_get(p_peer_pkey_tbl, block_index);
		if (!peer_block
		    || memcmp(peer_block, block, sizeof(*peer_block))) {
			status =
			    pkey_mgr_update_pkey_entry(sm, peer, block,
						       block_index);
			if (status == IB_SUCCESS)
				ret_val = TRUE;
			else
				OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0509: "
					"pkey_mgr_update_pkey_entry() failed to update "
					"pkey table block %d for node 0x%016"
					PRIx64 " port %u\n", block_index,
					cl_ntoh64(osm_node_get_node_guid
						  (p_node)),
					osm_physp_get_port_num(peer));
		}
	}

	if (ret_val)
		OSM_LOG(p_log, OSM_LOG_DEBUG,
			"Pkey table was updated for node 0x%016" PRIx64
			" port %u\n",
			cl_ntoh64(osm_node_get_node_guid(p_node)),
			osm_physp_get_port_num(peer));

	if (port_info_set)
		return TRUE;
	return ret_val;
}

/**********************************************************************
 **********************************************************************/
osm_signal_t osm_pkey_mgr_process(IN osm_opensm_t * p_osm)
{
	cl_qmap_t *p_tbl;
	cl_map_item_t *p_next;
	osm_prtn_t *p_prtn;
	osm_port_t *p_port;
	osm_signal_t signal = OSM_SIGNAL_DONE;

	CL_ASSERT(p_osm);

	OSM_LOG_ENTER(&p_osm->log);

	CL_PLOCK_EXCL_ACQUIRE(&p_osm->lock);

	if (osm_prtn_make_partitions(&p_osm->log, &p_osm->subn) != IB_SUCCESS) {
		OSM_LOG(&p_osm->log, OSM_LOG_ERROR, "ERR 0510: "
			"osm_prtn_make_partitions() failed\n");
		goto _err;
	}

	/* populate the pending pkey entries by scanning all partitions */
	p_tbl = &p_osm->subn.prtn_pkey_tbl;
	p_next = cl_qmap_head(p_tbl);
	while (p_next != cl_qmap_end(p_tbl)) {
		p_prtn = (osm_prtn_t *) p_next;
		p_next = cl_qmap_next(p_next);
		pkey_mgr_process_partition_table(&p_osm->log, &p_osm->sm,
						 p_prtn, FALSE);
		pkey_mgr_process_partition_table(&p_osm->log, &p_osm->sm,
						 p_prtn, TRUE);
	}

	/* calculate and set new pkey tables */
	p_tbl = &p_osm->subn.port_guid_tbl;
	p_next = cl_qmap_head(p_tbl);
	while (p_next != cl_qmap_end(p_tbl)) {
		p_port = (osm_port_t *) p_next;
		p_next = cl_qmap_next(p_next);
		if (pkey_mgr_update_port(&p_osm->log, &p_osm->sm, p_port))
			signal = OSM_SIGNAL_DONE_PENDING;
		if ((osm_node_get_type(p_port->p_node) != IB_NODE_TYPE_SWITCH)
		    && pkey_mgr_update_peer_port(&p_osm->log, &p_osm->sm,
						 &p_osm->subn, p_port,
						 !p_osm->subn.opt.
						 no_partition_enforcement))
			signal = OSM_SIGNAL_DONE_PENDING;
	}

_err:
	CL_PLOCK_RELEASE(&p_osm->lock);
	OSM_LOG_EXIT(&p_osm->log);
	return (signal);
}
