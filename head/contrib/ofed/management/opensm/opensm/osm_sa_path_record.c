/*
 * Copyright (c) 2004-2008 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2006 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 * Copyright (c) 2008 Xsigo Systems Inc.  All rights reserved.
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
 *    Implementation of osm_pr_rcv_t.
 * This object represents the PathRecord Receiver object.
 * This object is part of the opensm family of objects.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <string.h>
#include <arpa/inet.h>
#include <iba/ib_types.h>
#include <complib/cl_qmap.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_debug.h>
#include <complib/cl_qlist.h>
#include <vendor/osm_vendor_api.h>
#include <opensm/osm_base.h>
#include <opensm/osm_port.h>
#include <opensm/osm_node.h>
#include <opensm/osm_switch.h>
#include <opensm/osm_helper.h>
#include <opensm/osm_pkey.h>
#include <opensm/osm_multicast.h>
#include <opensm/osm_partition.h>
#include <opensm/osm_opensm.h>
#include <opensm/osm_qos_policy.h>
#include <opensm/osm_sa.h>
#include <opensm/osm_router.h>
#include <opensm/osm_prefix_route.h>

#include <sys/socket.h>

extern uint8_t osm_get_lash_sl(osm_opensm_t * p_osm,
			       const osm_port_t * p_src_port,
			       const osm_port_t * p_dst_port);

typedef struct osm_pr_item {
	cl_list_item_t list_item;
	ib_path_rec_t path_rec;
} osm_pr_item_t;

typedef struct osm_path_parms {
	ib_net16_t pkey;
	uint8_t mtu;
	uint8_t rate;
	uint8_t sl;
	uint8_t pkt_life;
	boolean_t reversible;
} osm_path_parms_t;

static const ib_gid_t zero_gid = { {0x00, 0x00, 0x00, 0x00,
				    0x00, 0x00, 0x00, 0x00,
				    0x00, 0x00, 0x00, 0x00,
				    0x00, 0x00, 0x00, 0x00},
};

/**********************************************************************
 **********************************************************************/
static inline boolean_t
__osm_sa_path_rec_is_tavor_port(IN const osm_port_t * const p_port)
{
	osm_node_t const *p_node;
	ib_net32_t vend_id;

	p_node = p_port->p_node;
	vend_id = ib_node_info_get_vendor_id(&p_node->node_info);

	return ((p_node->node_info.device_id == CL_HTON16(23108)) &&
		((vend_id == CL_HTON32(OSM_VENDOR_ID_MELLANOX)) ||
		 (vend_id == CL_HTON32(OSM_VENDOR_ID_TOPSPIN)) ||
		 (vend_id == CL_HTON32(OSM_VENDOR_ID_SILVERSTORM)) ||
		 (vend_id == CL_HTON32(OSM_VENDOR_ID_VOLTAIRE))));
}

/**********************************************************************
 **********************************************************************/
static boolean_t
__osm_sa_path_rec_apply_tavor_mtu_limit(IN const ib_path_rec_t * const p_pr,
					IN const osm_port_t * const p_src_port,
					IN const osm_port_t * const p_dest_port,
					IN const ib_net64_t comp_mask)
{
	uint8_t required_mtu;

	/* only if at least one of the ports is a Tavor device */
	if (!__osm_sa_path_rec_is_tavor_port(p_src_port) &&
	    !__osm_sa_path_rec_is_tavor_port(p_dest_port))
		return (FALSE);

	/*
	   we can apply the patch if either:
	   1. No MTU required
	   2. Required MTU <
	   3. Required MTU = 1K or 512 or 256
	   4. Required MTU > 256 or 512
	 */
	required_mtu = ib_path_rec_mtu(p_pr);
	if ((comp_mask & IB_PR_COMPMASK_MTUSELEC) &&
	    (comp_mask & IB_PR_COMPMASK_MTU)) {
		switch (ib_path_rec_mtu_sel(p_pr)) {
		case 0:	/* must be greater than */
		case 2:	/* exact match */
			if (IB_MTU_LEN_1024 < required_mtu)
				return (FALSE);
			break;

		case 1:	/* must be less than */
			/* can't be disqualified by this one */
			break;

		case 3:	/* largest available */
			/* the ULP intentionally requested */
			/* the largest MTU possible */
			return (FALSE);
			break;

		default:
			/* if we're here, there's a bug in ib_path_rec_mtu_sel() */
			CL_ASSERT(FALSE);
			break;
		}
	}

	return (TRUE);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
__osm_pr_rcv_get_path_parms(IN osm_sa_t * sa,
			    IN const ib_path_rec_t * const p_pr,
			    IN const osm_port_t * const p_src_port,
			    IN const osm_port_t * const p_dest_port,
			    IN const uint16_t dest_lid_ho,
			    IN const ib_net64_t comp_mask,
			    OUT osm_path_parms_t * const p_parms)
{
	const osm_node_t *p_node;
	const osm_physp_t *p_physp;
	const osm_physp_t *p_src_physp;
	const osm_physp_t *p_dest_physp;
	const osm_prtn_t *p_prtn = NULL;
	osm_opensm_t *p_osm;
	const ib_port_info_t *p_pi;
	ib_api_status_t status = IB_SUCCESS;
	ib_net16_t pkey;
	uint8_t mtu;
	uint8_t rate;
	uint8_t pkt_life;
	uint8_t required_mtu;
	uint8_t required_rate;
	uint8_t required_pkt_life;
	uint8_t sl;
	uint8_t in_port_num;
	ib_net16_t dest_lid;
	uint8_t i;
	ib_slvl_table_t *p_slvl_tbl = NULL;
	osm_qos_level_t *p_qos_level = NULL;
	uint16_t valid_sl_mask = 0xffff;
	int is_lash;

	OSM_LOG_ENTER(sa->p_log);

	dest_lid = cl_hton16(dest_lid_ho);

	p_dest_physp = p_dest_port->p_physp;
	p_physp = p_src_port->p_physp;
	p_src_physp = p_physp;
	p_pi = &p_physp->port_info;
	p_osm = sa->p_subn->p_osm;

	mtu = ib_port_info_get_mtu_cap(p_pi);
	rate = ib_port_info_compute_rate(p_pi);

	/*
	   Mellanox Tavor device performance is better using 1K MTU.
	   If required MTU and MTU selector are such that 1K is OK
	   and at least one end of the path is Tavor we override the
	   port MTU with 1K.
	 */
	if (sa->p_subn->opt.enable_quirks &&
	    __osm_sa_path_rec_apply_tavor_mtu_limit(p_pr, p_src_port,
						    p_dest_port, comp_mask))
		if (mtu > IB_MTU_LEN_1024) {
			mtu = IB_MTU_LEN_1024;
			OSM_LOG(sa->p_log, OSM_LOG_DEBUG,
				"Optimized Path MTU to 1K for Mellanox Tavor device\n");
		}

	/*
	   Walk the subnet object from source to destination,
	   tracking the most restrictive rate and mtu values along the way...

	   If source port node is a switch, then p_physp should
	   point to the port that routes the destination lid
	 */

	p_node = osm_physp_get_node_ptr(p_physp);

	if (p_node->sw) {
		/*
		 * Source node is a switch.
		 * Make sure that p_physp points to the out port of the
		 * switch that routes to the destination lid (dest_lid_ho)
		 */
		p_physp = osm_switch_get_route_by_lid(p_node->sw, dest_lid);
		if (p_physp == 0) {
			OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F02: "
				"Cannot find routing to LID %u from switch for GUID 0x%016"
				PRIx64 "\n", dest_lid_ho,
				cl_ntoh64(osm_node_get_node_guid(p_node)));
			status = IB_NOT_FOUND;
			goto Exit;
		}
	}

	if (sa->p_subn->opt.qos) {

		/*
		 * Whether this node is switch or CA, the IN port for
		 * the sl2vl table is 0, because this is a source node.
		 */
		p_slvl_tbl = osm_physp_get_slvl_tbl(p_physp, 0);

		/* update valid SLs that still exist on this route */
		for (i = 0; i < IB_MAX_NUM_VLS; i++) {
			if (valid_sl_mask & (1 << i) &&
			    ib_slvl_table_get(p_slvl_tbl, i) == IB_DROP_VL)
				valid_sl_mask &= ~(1 << i);
		}
		if (!valid_sl_mask) {
			OSM_LOG(sa->p_log, OSM_LOG_DEBUG,
				"All the SLs lead to VL15 on this path\n");
			status = IB_NOT_FOUND;
			goto Exit;
		}
	}

	/*
	 * Same as above
	 */
	p_node = osm_physp_get_node_ptr(p_dest_physp);

	if (p_node->sw) {
		/*
		 * if destination is switch, we want p_dest_physp to point to port 0
		 */
		p_dest_physp = osm_switch_get_route_by_lid(p_node->sw, dest_lid);

		if (p_dest_physp == 0) {
			OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F03: "
				"Cannot find routing to LID %u from switch for GUID 0x%016"
				PRIx64 "\n", dest_lid_ho,
				cl_ntoh64(osm_node_get_node_guid(p_node)));
			status = IB_NOT_FOUND;
			goto Exit;
		}

	}

	/*
	 * Now go through the path step by step
	 */

	while (p_physp != p_dest_physp) {

		p_node = osm_physp_get_node_ptr(p_physp);
		p_physp = osm_physp_get_remote(p_physp);

		if (p_physp == 0) {
			OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F05: "
				"Cannot find remote phys port when routing to LID %u from node GUID 0x%016"
				PRIx64 "\n", dest_lid_ho,
				cl_ntoh64(osm_node_get_node_guid(p_node)));
			status = IB_ERROR;
			goto Exit;
		}

		in_port_num = osm_physp_get_port_num(p_physp);

		/*
		   This is point to point case (no switch in between)
		 */
		if (p_physp == p_dest_physp)
			break;

		p_node = osm_physp_get_node_ptr(p_physp);

		if (!p_node->sw) {
			/*
			   There is some sort of problem in the subnet object!
			   If this isn't a switch, we should have reached
			   the destination by now!
			 */
			OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F06: "
				"Internal error, bad path\n");
			status = IB_ERROR;
			goto Exit;
		}

		/*
		   Check parameters for the ingress port in this switch.
		 */
		p_pi = &p_physp->port_info;

		if (mtu > ib_port_info_get_mtu_cap(p_pi))
			mtu = ib_port_info_get_mtu_cap(p_pi);

		if (rate > ib_port_info_compute_rate(p_pi))
			rate = ib_port_info_compute_rate(p_pi);

		/*
		   Continue with the egress port on this switch.
		 */
		p_physp = osm_switch_get_route_by_lid(p_node->sw, dest_lid);
		if (p_physp == 0) {
			OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F07: "
				"Dead end on path to LID %u from switch for GUID 0x%016"
				PRIx64 "\n", dest_lid_ho,
				cl_ntoh64(osm_node_get_node_guid(p_node)));
			status = IB_ERROR;
			goto Exit;
		}

		p_pi = &p_physp->port_info;

		if (mtu > ib_port_info_get_mtu_cap(p_pi))
			mtu = ib_port_info_get_mtu_cap(p_pi);

		if (rate > ib_port_info_compute_rate(p_pi))
			rate = ib_port_info_compute_rate(p_pi);

		if (sa->p_subn->opt.qos) {
			/*
			 * Check SL2VL table of the switch and update valid SLs
			 */
			p_slvl_tbl = osm_physp_get_slvl_tbl(p_physp, in_port_num);
			for (i = 0; i < IB_MAX_NUM_VLS; i++) {
				if (valid_sl_mask & (1 << i) &&
				    ib_slvl_table_get(p_slvl_tbl, i) == IB_DROP_VL)
					valid_sl_mask &= ~(1 << i);
			}
			if (!valid_sl_mask) {
				OSM_LOG(sa->p_log, OSM_LOG_DEBUG, "All the SLs "
					"lead to VL15 on this path\n");
				status = IB_NOT_FOUND;
				goto Exit;
			}
		}
	}

	/*
	   p_physp now points to the destination
	 */
	p_pi = &p_physp->port_info;

	if (mtu > ib_port_info_get_mtu_cap(p_pi))
		mtu = ib_port_info_get_mtu_cap(p_pi);

	if (rate > ib_port_info_compute_rate(p_pi))
		rate = ib_port_info_compute_rate(p_pi);

	OSM_LOG(sa->p_log, OSM_LOG_DEBUG,
		"Path min MTU = %u, min rate = %u\n", mtu, rate);

	/*
	 * Get QoS Level object according to the path request
	 * and adjust path parameters according to QoS settings
	 */
	if (sa->p_subn->opt.qos &&
	    sa->p_subn->p_qos_policy &&
	    (p_qos_level =
	     osm_qos_policy_get_qos_level_by_pr(sa->p_subn->p_qos_policy,
						p_pr, p_src_physp, p_dest_physp,
						comp_mask))) {
		OSM_LOG(sa->p_log, OSM_LOG_DEBUG,
			"PathRecord request matches QoS Level '%s' (%s)\n",
			p_qos_level->name, p_qos_level->use ?
			p_qos_level->use : "no description");

		if (p_qos_level->mtu_limit_set
		    && (mtu > p_qos_level->mtu_limit))
			mtu = p_qos_level->mtu_limit;

		if (p_qos_level->rate_limit_set
		    && (rate > p_qos_level->rate_limit))
			rate = p_qos_level->rate_limit;

		if (p_qos_level->sl_set) {
			sl = p_qos_level->sl;
			if (!(valid_sl_mask & (1 << sl))) {
				status = IB_NOT_FOUND;
				goto Exit;
			}
		}
	}

	/*
	 * Set packet lifetime.
	 * According to spec definition IBA 1.2 Table 205
	 * PacketLifeTime description, for loopback paths,
	 * packetLifeTime shall be zero.
	 */
	if (p_src_port == p_dest_port)
		pkt_life = 0;
	else if (p_qos_level && p_qos_level->pkt_life_set)
		pkt_life = p_qos_level->pkt_life;
	else
		pkt_life = sa->p_subn->opt.subnet_timeout;

	/*
	   Determine if these values meet the user criteria
	   and adjust appropriately
	 */

	/* we silently ignore cases where only the MTU selector is defined */
	if ((comp_mask & IB_PR_COMPMASK_MTUSELEC) &&
	    (comp_mask & IB_PR_COMPMASK_MTU)) {
		required_mtu = ib_path_rec_mtu(p_pr);
		switch (ib_path_rec_mtu_sel(p_pr)) {
		case 0:	/* must be greater than */
			if (mtu <= required_mtu)
				status = IB_NOT_FOUND;
			break;

		case 1:	/* must be less than */
			if (mtu >= required_mtu) {
				/* adjust to use the highest mtu
				   lower then the required one */
				if (required_mtu > 1)
					mtu = required_mtu - 1;
				else
					status = IB_NOT_FOUND;
			}
			break;

		case 2:	/* exact match */
			if (mtu < required_mtu)
				status = IB_NOT_FOUND;
			else
				mtu = required_mtu;
			break;

		case 3:	/* largest available */
			/* can't be disqualified by this one */
			break;

		default:
			/* if we're here, there's a bug in ib_path_rec_mtu_sel() */
			CL_ASSERT(FALSE);
			status = IB_ERROR;
			break;
		}
	}
	if (status != IB_SUCCESS)
		goto Exit;

	/* we silently ignore cases where only the Rate selector is defined */
	if ((comp_mask & IB_PR_COMPMASK_RATESELEC) &&
	    (comp_mask & IB_PR_COMPMASK_RATE)) {
		required_rate = ib_path_rec_rate(p_pr);
		switch (ib_path_rec_rate_sel(p_pr)) {
		case 0:	/* must be greater than */
			if (rate <= required_rate)
				status = IB_NOT_FOUND;
			break;

		case 1:	/* must be less than */
			if (rate >= required_rate) {
				/* adjust the rate to use the highest rate
				   lower then the required one */
				if (required_rate > 2)
					rate = required_rate - 1;
				else
					status = IB_NOT_FOUND;
			}
			break;

		case 2:	/* exact match */
			if (rate < required_rate)
				status = IB_NOT_FOUND;
			else
				rate = required_rate;
			break;

		case 3:	/* largest available */
			/* can't be disqualified by this one */
			break;

		default:
			/* if we're here, there's a bug in ib_path_rec_mtu_sel() */
			CL_ASSERT(FALSE);
			status = IB_ERROR;
			break;
		}
	}
	if (status != IB_SUCCESS)
		goto Exit;

	/* we silently ignore cases where only the PktLife selector is defined */
	if ((comp_mask & IB_PR_COMPMASK_PKTLIFETIMESELEC) &&
	    (comp_mask & IB_PR_COMPMASK_PKTLIFETIME)) {
		required_pkt_life = ib_path_rec_pkt_life(p_pr);
		switch (ib_path_rec_pkt_life_sel(p_pr)) {
		case 0:	/* must be greater than */
			if (pkt_life <= required_pkt_life)
				status = IB_NOT_FOUND;
			break;

		case 1:	/* must be less than */
			if (pkt_life >= required_pkt_life) {
				/* adjust the lifetime to use the highest possible
				   lower then the required one */
				if (required_pkt_life > 1)
					pkt_life = required_pkt_life - 1;
				else
					status = IB_NOT_FOUND;
			}
			break;

		case 2:	/* exact match */
			if (pkt_life < required_pkt_life)
				status = IB_NOT_FOUND;
			else
				pkt_life = required_pkt_life;
			break;

		case 3:	/* smallest available */
			/* can't be disqualified by this one */
			break;

		default:
			/* if we're here, there's a bug in ib_path_rec_pkt_life_sel() */
			CL_ASSERT(FALSE);
			status = IB_ERROR;
			break;
		}
	}

	if (status != IB_SUCCESS)
		goto Exit;

	/*
	 * set Pkey for this path record request
	 */

	if ((comp_mask & IB_PR_COMPMASK_RAWTRAFFIC) &&
	    (cl_ntoh32(p_pr->hop_flow_raw) & (1 << 31)))
		pkey = osm_physp_find_common_pkey(p_src_physp, p_dest_physp);

	else if (comp_mask & IB_PR_COMPMASK_PKEY) {
		/*
		 * PR request has a specific pkey:
		 * Check that source and destination share this pkey.
		 * If QoS level has pkeys, check that this pkey exists
		 * in the QoS level pkeys.
		 * PR returned pkey is the requested pkey.
		 */
		pkey = p_pr->pkey;
		if (!osm_physp_share_this_pkey(p_src_physp, p_dest_physp, pkey)) {
			OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F1A: "
				"Ports 0x%016" PRIx64 " 0x%016" PRIx64
				" do not share specified PKey 0x%04x\n",
				cl_ntoh64(osm_physp_get_port_guid(p_src_physp)),
				cl_ntoh64(osm_physp_get_port_guid(p_dest_physp)),
				cl_ntoh16(pkey));
			status = IB_NOT_FOUND;
			goto Exit;
		}
		if (p_qos_level && p_qos_level->pkey_range_len &&
		    !osm_qos_level_has_pkey(p_qos_level, pkey)) {
			OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F1D: "
				"Ports do not share PKeys defined by QoS level\n");
			status = IB_NOT_FOUND;
			goto Exit;
		}

	} else if (p_qos_level && p_qos_level->pkey_range_len) {
		/*
		 * PR request doesn't have a specific pkey, but QoS level
		 * has pkeys - get shared pkey from QoS level pkeys
		 */
		pkey = osm_qos_level_get_shared_pkey(p_qos_level,
						     p_src_physp, p_dest_physp);
		if (!pkey) {
			OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F1E: "
				"Ports 0x%016" PRIx64 " 0x%016" PRIx64
				" do not share PKeys defined by QoS level\n",
				cl_ntoh64(osm_physp_get_port_guid(p_src_physp)),
				cl_ntoh64(osm_physp_get_port_guid(p_dest_physp)));
			status = IB_NOT_FOUND;
			goto Exit;
		}
	} else {
		/*
		 * Neither PR request nor QoS level have pkey.
		 * Just get any shared pkey.
		 */
		pkey = osm_physp_find_common_pkey(p_src_physp, p_dest_physp);
		if (!pkey) {
			OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F1B: "
				"Ports 0x%016" PRIx64 " 0x%016" PRIx64
				" do not have any shared PKeys\n",
				cl_ntoh64(osm_physp_get_port_guid(p_src_physp)),
				cl_ntoh64(osm_physp_get_port_guid(p_dest_physp)));
			status = IB_NOT_FOUND;
			goto Exit;
		}
	}

	if (pkey) {
		p_prtn =
		    (osm_prtn_t *) cl_qmap_get(&sa->p_subn->prtn_pkey_tbl,
					       pkey & cl_hton16((uint16_t) ~
								0x8000));
		if (p_prtn ==
		    (osm_prtn_t *) cl_qmap_end(&sa->p_subn->prtn_pkey_tbl))
			p_prtn = NULL;
	}

	/*
	 * Set PathRecord SL.
	 */

	is_lash = (p_osm->routing_engine_used == OSM_ROUTING_ENGINE_TYPE_LASH);

	if (comp_mask & IB_PR_COMPMASK_SL) {
		/*
		 * Specific SL was requested
		 */
		sl = ib_path_rec_sl(p_pr);

		if (p_qos_level && p_qos_level->sl_set
		    && (p_qos_level->sl != sl)) {
			OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F1F: "
				"QoS constaraints: required PathRecord SL (%u) "
				"doesn't match QoS policy SL (%u)\n", sl,
				p_qos_level->sl);
			status = IB_NOT_FOUND;
			goto Exit;
		}

		if (is_lash
		    && osm_get_lash_sl(p_osm, p_src_port, p_dest_port) != sl) {
			OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F23: "
				"Required PathRecord SL (%u) doesn't "
				"match LASH SL\n", sl);
			status = IB_NOT_FOUND;
			goto Exit;
		}

	} else if (is_lash) {
		/*
		 * No specific SL in PathRecord request.
		 * If it's LASH routing - use its SL.
		 * slid and dest_lid are stored in network in lash.
		 */
		sl = osm_get_lash_sl(p_osm, p_src_port, p_dest_port);
	} else if (p_qos_level && p_qos_level->sl_set) {
		/*
		 * No specific SL was requested, and we're not in
		 * LASH routing, but there is an SL in QoS level.
		 */
		sl = p_qos_level->sl;

		if (pkey && p_prtn && p_prtn->sl != p_qos_level->sl)
			OSM_LOG(sa->p_log, OSM_LOG_DEBUG,
				"QoS level SL (%u) overrides partition SL (%u)\n",
				p_qos_level->sl, p_prtn->sl);

	} else if (pkey) {
		/*
		 * No specific SL in request or in QoS level - use partition SL
		 */
		if (!p_prtn) {
			sl = OSM_DEFAULT_SL;
			/* this may be possible when pkey tables are created somehow in
			   previous runs or things are going wrong here */
			OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F1C: "
				"No partition found for PKey 0x%04x - using default SL %d\n",
				cl_ntoh16(pkey), sl);
		} else
			sl = p_prtn->sl;
	} else if (sa->p_subn->opt.qos) {
		if (valid_sl_mask & (1 << OSM_DEFAULT_SL))
			sl = OSM_DEFAULT_SL;
		else {
			for (i = 0; i < IB_MAX_NUM_VLS; i++)
				if (valid_sl_mask & (1 << i))
					break;
			sl = i;
		}
	} else
		sl = OSM_DEFAULT_SL;

	if (sa->p_subn->opt.qos && !(valid_sl_mask & (1 << sl))) {
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F24: "
			"Selected SL (%u) leads to VL15\n", sl);
		status = IB_NOT_FOUND;
		goto Exit;
	}

	/* reset pkey when raw traffic */
	if (comp_mask & IB_PR_COMPMASK_RAWTRAFFIC &&
	    cl_ntoh32(p_pr->hop_flow_raw) & (1 << 31))
		pkey = 0;

	p_parms->mtu = mtu;
	p_parms->rate = rate;
	p_parms->pkt_life = pkt_life;
	p_parms->pkey = pkey;
	p_parms->sl = sl;

	OSM_LOG(sa->p_log, OSM_LOG_DEBUG, "Path params: mtu = %u, rate = %u,"
		" packet lifetime = %u, pkey = 0x%04X, sl = %u\n",
		mtu, rate, pkt_life, cl_ntoh16(pkey), sl);
Exit:
	OSM_LOG_EXIT(sa->p_log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static void
__osm_pr_rcv_build_pr(IN osm_sa_t * sa,
		      IN const osm_port_t * const p_src_port,
		      IN const osm_port_t * const p_dest_port,
		      IN const ib_gid_t * const p_dgid,
		      IN const uint16_t src_lid_ho,
		      IN const uint16_t dest_lid_ho,
		      IN const uint8_t preference,
		      IN const osm_path_parms_t * const p_parms,
		      OUT ib_path_rec_t * const p_pr)
{
	const osm_physp_t *p_src_physp;
	const osm_physp_t *p_dest_physp;
	boolean_t is_nonzero_gid = 0;

	OSM_LOG_ENTER(sa->p_log);

	p_src_physp = p_src_port->p_physp;

	if (p_dgid) {
		if (memcmp(p_dgid, &zero_gid, sizeof(*p_dgid)))
			is_nonzero_gid = 1;
	}

	if (is_nonzero_gid)
		p_pr->dgid = *p_dgid;
	else {
		p_dest_physp = p_dest_port->p_physp;

		p_pr->dgid.unicast.prefix =
		    osm_physp_get_subnet_prefix(p_dest_physp);
		p_pr->dgid.unicast.interface_id =
		    osm_physp_get_port_guid(p_dest_physp);
	}

	p_pr->sgid.unicast.prefix = osm_physp_get_subnet_prefix(p_src_physp);
	p_pr->sgid.unicast.interface_id = osm_physp_get_port_guid(p_src_physp);

	p_pr->dlid = cl_hton16(dest_lid_ho);
	p_pr->slid = cl_hton16(src_lid_ho);

	p_pr->hop_flow_raw &= cl_hton32(1 << 31);

	/* Only set HopLimit if going through a router */
	if (is_nonzero_gid)
		p_pr->hop_flow_raw |= cl_hton32(IB_HOPLIMIT_MAX);

	p_pr->pkey = p_parms->pkey;
	ib_path_rec_set_sl(p_pr, p_parms->sl);
	ib_path_rec_set_qos_class(p_pr, 0);
	p_pr->mtu = (uint8_t) (p_parms->mtu | 0x80);
	p_pr->rate = (uint8_t) (p_parms->rate | 0x80);

	/* According to 1.2 spec definition Table 205 PacketLifeTime description,
	   for loopback paths, packetLifeTime shall be zero. */
	if (p_src_port == p_dest_port)
		p_pr->pkt_life = 0x80;	/* loopback */
	else
		p_pr->pkt_life = (uint8_t) (p_parms->pkt_life | 0x80);

	p_pr->preference = preference;

	/* always return num_path = 0 so this is only the reversible component */
	if (p_parms->reversible)
		p_pr->num_path = 0x80;

	OSM_LOG_EXIT(sa->p_log);
}

/**********************************************************************
 **********************************************************************/
static osm_pr_item_t *
__osm_pr_rcv_get_lid_pair_path(IN osm_sa_t * sa,
			       IN const ib_path_rec_t * const p_pr,
			       IN const osm_port_t * const p_src_port,
			       IN const osm_port_t * const p_dest_port,
			       IN const ib_gid_t * const p_dgid,
			       IN const uint16_t src_lid_ho,
			       IN const uint16_t dest_lid_ho,
			       IN const ib_net64_t comp_mask,
			       IN const uint8_t preference)
{
	osm_path_parms_t path_parms;
	osm_path_parms_t rev_path_parms;
	osm_pr_item_t *p_pr_item;
	ib_api_status_t status, rev_path_status;

	OSM_LOG_ENTER(sa->p_log);

	OSM_LOG(sa->p_log, OSM_LOG_DEBUG, "Src LID %u, Dest LID %u\n",
		src_lid_ho, dest_lid_ho);

	p_pr_item = malloc(sizeof(*p_pr_item));
	if (p_pr_item == NULL) {
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F01: "
			"Unable to allocate path record\n");
		goto Exit;
	}
	memset(p_pr_item, 0, sizeof(*p_pr_item));

	status = __osm_pr_rcv_get_path_parms(sa, p_pr, p_src_port,
					     p_dest_port, dest_lid_ho,
					     comp_mask, &path_parms);

	if (status != IB_SUCCESS) {
		free(p_pr_item);
		p_pr_item = NULL;
		goto Exit;
	}

	/* now try the reversible path */
	rev_path_status = __osm_pr_rcv_get_path_parms(sa, p_pr, p_dest_port,
						      p_src_port, src_lid_ho,
						      comp_mask,
						      &rev_path_parms);
	path_parms.reversible = (rev_path_status == IB_SUCCESS);

	/* did we get a Reversible Path compmask ? */
	/*
	   NOTE that if the reversible component = 0, it is a don't care
	   rather then requiring non-reversible paths ...
	   see Vol1 Ver1.2 p900 l16
	 */
	if (comp_mask & IB_PR_COMPMASK_REVERSIBLE) {
		if ((!path_parms.reversible && (p_pr->num_path & 0x80))) {
			OSM_LOG(sa->p_log, OSM_LOG_DEBUG,
				"Requested reversible path but failed to get one\n");

			free(p_pr_item);
			p_pr_item = NULL;
			goto Exit;
		}
	}

	__osm_pr_rcv_build_pr(sa, p_src_port, p_dest_port, p_dgid,
			      src_lid_ho, dest_lid_ho, preference, &path_parms,
			      &p_pr_item->path_rec);

Exit:
	OSM_LOG_EXIT(sa->p_log);
	return (p_pr_item);
}

/**********************************************************************
 **********************************************************************/
static void
__osm_pr_rcv_get_port_pair_paths(IN osm_sa_t * sa,
				 IN const osm_madw_t * const p_madw,
				 IN const osm_port_t * const p_req_port,
				 IN const osm_port_t * const p_src_port,
				 IN const osm_port_t * const p_dest_port,
				 IN const ib_gid_t * const p_dgid,
				 IN const ib_net64_t comp_mask,
				 IN cl_qlist_t * const p_list)
{
	const ib_path_rec_t *p_pr;
	const ib_sa_mad_t *p_sa_mad;
	osm_pr_item_t *p_pr_item;
	uint16_t src_lid_min_ho;
	uint16_t src_lid_max_ho;
	uint16_t dest_lid_min_ho;
	uint16_t dest_lid_max_ho;
	uint16_t src_lid_ho;
	uint16_t dest_lid_ho;
	uint32_t path_num;
	uint8_t preference;
	uintn_t iterations;
	uintn_t src_offset;
	uintn_t dest_offset;

	OSM_LOG_ENTER(sa->p_log);

	OSM_LOG(sa->p_log, OSM_LOG_DEBUG,
		"Src port 0x%016" PRIx64 ", Dst port 0x%016" PRIx64 "\n",
		cl_ntoh64(osm_port_get_guid(p_src_port)),
		cl_ntoh64(osm_port_get_guid(p_dest_port)));

	/* Check that the req_port, src_port and dest_port all share a
	   pkey. The check is done on the default physical port of the ports. */
	if (osm_port_share_pkey(sa->p_log, p_req_port, p_src_port) == FALSE
	    || osm_port_share_pkey(sa->p_log, p_req_port,
				   p_dest_port) == FALSE
	    || osm_port_share_pkey(sa->p_log, p_src_port,
				   p_dest_port) == FALSE)
		/* One of the pairs doesn't share a pkey so the path is disqualified. */
		goto Exit;

	p_sa_mad = osm_madw_get_sa_mad_ptr(p_madw);
	p_pr = (ib_path_rec_t *) ib_sa_mad_get_payload_ptr(p_sa_mad);

	/*
	   We shouldn't be here if the paths are disqualified in some way...
	   Thus, we assume every possible connection is valid.

	   We desire to return high-quality paths first.
	   In OpenSM, higher quality means least overlap with other paths.
	   This is acheived in practice by returning paths with
	   different LID value on each end, which means these
	   paths are more redundant that paths with the same LID repeated
	   on one side.  For example, in OpenSM the paths between two
	   endpoints with LMC = 1 might be as follows:

	   Port A, LID 1 <-> Port B, LID 3
	   Port A, LID 1 <-> Port B, LID 4
	   Port A, LID 2 <-> Port B, LID 3
	   Port A, LID 2 <-> Port B, LID 4

	   The OpenSM unicast routing algorithms attempt to disperse each path
	   to as varied a physical path as is reasonable.  1<->3 and 1<->4 have
	   more physical overlap (hence less redundancy) than 1<->3 and 2<->4.

	   OpenSM ranks paths in three preference groups:

	   Preference Value    Description
	   ----------------    -------------------------------------------
	   0             Redundant in both directions with other
	   pref value = 0 paths

	   1             Redundant in one direction with other
	   pref value = 0 and pref value = 1 paths

	   2             Not redundant in either direction with
	   other paths

	   3-FF          Unused

	   SA clients don't need to know these details, only that the lower
	   preference paths are preferred, as stated in the spec.  The paths
	   may not actually be physically redundant depending on the topology
	   of the subnet, but the point of LMC > 0 is to offer redundancy,
	   so it is assumed that the subnet is physically appropriate for the
	   specified LMC value.  A more advanced implementation would inspect for
	   physical redundancy, but I'm not going to bother with that now.
	 */

	/*
	   Refine our search if the client specified end-point LIDs
	 */
	if (comp_mask & IB_PR_COMPMASK_DLID) {
		dest_lid_min_ho = cl_ntoh16(p_pr->dlid);
		dest_lid_max_ho = cl_ntoh16(p_pr->dlid);
	} else
		osm_port_get_lid_range_ho(p_dest_port, &dest_lid_min_ho,
					  &dest_lid_max_ho);

	if (comp_mask & IB_PR_COMPMASK_SLID) {
		src_lid_min_ho = cl_ntoh16(p_pr->slid);
		src_lid_max_ho = cl_ntoh16(p_pr->slid);
	} else
		osm_port_get_lid_range_ho(p_src_port, &src_lid_min_ho,
					  &src_lid_max_ho);

	if (src_lid_min_ho == 0) {
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F20:"
			"Obtained source LID of 0. No such LID possible\n");
		goto Exit;
	}

	if (dest_lid_min_ho == 0) {
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F21:"
			"Obtained destination LID of 0. No such LID possible\n");
		goto Exit;
	}

	OSM_LOG(sa->p_log, OSM_LOG_DEBUG,
		"Src LIDs [%u-%u], Dest LIDs [%u-%u]\n",
		src_lid_min_ho, src_lid_max_ho,
		dest_lid_min_ho, dest_lid_max_ho);

	src_lid_ho = src_lid_min_ho;
	dest_lid_ho = dest_lid_min_ho;

	/*
	   Preferred paths come first in OpenSM
	 */
	preference = 0;
	path_num = 0;

	/* If SubnAdmGet, assume NumbPaths 1 (1.2 erratum) */
	if (p_sa_mad->method != IB_MAD_METHOD_GET)
		if (comp_mask & IB_PR_COMPMASK_NUMBPATH)
			iterations = ib_path_rec_num_path(p_pr);
		else
			iterations = (uintn_t) (-1);
	else
		iterations = 1;

	while (path_num < iterations) {
		/*
		   These paths are "fully redundant"
		 */

		p_pr_item = __osm_pr_rcv_get_lid_pair_path(sa, p_pr,
							   p_src_port,
							   p_dest_port, p_dgid,
							   src_lid_ho,
							   dest_lid_ho,
							   comp_mask,
							   preference);

		if (p_pr_item) {
			cl_qlist_insert_tail(p_list, &p_pr_item->list_item);
			++path_num;
		}

		if (++src_lid_ho > src_lid_max_ho)
			break;

		if (++dest_lid_ho > dest_lid_max_ho)
			break;
	}

	/*
	   Check if we've accumulated all the paths that the user cares to see
	 */
	if (path_num == iterations)
		goto Exit;

	/*
	   Don't bother reporting preference 1 paths for now.
	   It's more trouble than it's worth and can only occur
	   if ports have different LMC values, which isn't supported
	   by OpenSM right now anyway.
	 */
	preference = 2;
	src_lid_ho = src_lid_min_ho;
	dest_lid_ho = dest_lid_min_ho;
	src_offset = 0;
	dest_offset = 0;

	/*
	   Iterate over the remaining paths
	 */
	while (path_num < iterations) {
		dest_offset++;
		dest_lid_ho++;

		if (dest_lid_ho > dest_lid_max_ho) {
			src_offset++;
			src_lid_ho++;

			if (src_lid_ho > src_lid_max_ho)
				break;	/* done */

			dest_offset = 0;
			dest_lid_ho = dest_lid_min_ho;
		}

		/*
		   These paths are "fully non-redundant" with paths already
		   identified above and consequently not of much value.

		   Don't return paths we already identified above, as indicated
		   by the offset values being equal.
		 */
		if (src_offset == dest_offset)
			continue;	/* already reported */

		p_pr_item = __osm_pr_rcv_get_lid_pair_path(sa, p_pr,
							   p_src_port,
							   p_dest_port, p_dgid,
							   src_lid_ho,
							   dest_lid_ho,
							   comp_mask,
							   preference);

		if (p_pr_item) {
			cl_qlist_insert_tail(p_list, &p_pr_item->list_item);
			++path_num;
		}
	}

Exit:
	OSM_LOG_EXIT(sa->p_log);
}

/**********************************************************************
 **********************************************************************/
static ib_net16_t
__osm_pr_rcv_get_end_points(IN osm_sa_t * sa,
			    IN const osm_madw_t * const p_madw,
			    OUT const osm_port_t ** const pp_src_port,
			    OUT const osm_port_t ** const pp_dest_port,
			    OUT ib_gid_t * const p_dgid)
{
	const ib_path_rec_t *p_pr;
	const ib_sa_mad_t *p_sa_mad;
	ib_net64_t comp_mask;
	ib_net64_t dest_guid;
	ib_api_status_t status;
	ib_net16_t sa_status = IB_SA_MAD_STATUS_SUCCESS;
	osm_router_t *p_rtr;
	osm_port_t *p_rtr_port;

	OSM_LOG_ENTER(sa->p_log);

	/*
	   Determine what fields are valid and then get a pointer
	   to the source and destination port objects, if possible.
	 */

	p_sa_mad = osm_madw_get_sa_mad_ptr(p_madw);
	p_pr = (ib_path_rec_t *) ib_sa_mad_get_payload_ptr(p_sa_mad);

	comp_mask = p_sa_mad->comp_mask;

	/*
	   Check a few easy disqualifying cases up front before getting
	   into the endpoints.
	 */

	if (comp_mask & IB_PR_COMPMASK_SGID) {
		if (!ib_gid_is_link_local(&p_pr->sgid)) {
			if (ib_gid_get_subnet_prefix(&p_pr->sgid) !=
			    sa->p_subn->opt.subnet_prefix) {
				/*
				   This 'error' is the client's fault (bad gid)
				   so don't enter it as an error in our own log.
				   Return an error response to the client.
				 */
				OSM_LOG(sa->p_log, OSM_LOG_VERBOSE,
					"Non local SGID subnet prefix 0x%016"
					PRIx64 "\n",
					cl_ntoh64(p_pr->sgid.unicast.prefix));

				sa_status = IB_SA_MAD_STATUS_INVALID_GID;
				goto Exit;
			}
		}

		*pp_src_port = osm_get_port_by_guid(sa->p_subn,
						    p_pr->sgid.unicast.
						    interface_id);
		if (!*pp_src_port) {
			/*
			   This 'error' is the client's fault (bad gid) so
			   don't enter it as an error in our own log.
			   Return an error response to the client.
			 */
			OSM_LOG(sa->p_log, OSM_LOG_VERBOSE,
				"No source port with GUID 0x%016" PRIx64 "\n",
				cl_ntoh64(p_pr->sgid.unicast.interface_id));

			sa_status = IB_SA_MAD_STATUS_INVALID_GID;
			goto Exit;
		}
	} else {
		*pp_src_port = 0;
		if (comp_mask & IB_PR_COMPMASK_SLID) {
			status = cl_ptr_vector_at(&sa->p_subn->port_lid_tbl,
						  cl_ntoh16(p_pr->slid),
						  (void **)pp_src_port);

			if ((status != CL_SUCCESS) || (*pp_src_port == NULL)) {
				/*
				   This 'error' is the client's fault (bad lid) so
				   don't enter it as an error in our own log.
				   Return an error response to the client.
				 */
				OSM_LOG(sa->p_log, OSM_LOG_VERBOSE,
					"No source port with LID %u\n",
					cl_ntoh16(p_pr->slid));

				sa_status = IB_SA_MAD_STATUS_NO_RECORDS;
				goto Exit;
			}
		}
	}

	if (p_dgid)
		memset(p_dgid, 0, sizeof(*p_dgid));

	if (comp_mask & IB_PR_COMPMASK_DGID) {
		dest_guid = p_pr->dgid.unicast.interface_id;
		if (!ib_gid_is_link_local(&p_pr->dgid)) {
			if (!ib_gid_is_multicast(&p_pr->dgid) &&
			    ib_gid_get_subnet_prefix(&p_pr->dgid) !=
			    sa->p_subn->opt.subnet_prefix) {
				OSM_LOG(sa->p_log, OSM_LOG_VERBOSE,
					"Non local DGID subnet prefix 0x%016"
					PRIx64 "\n",
					cl_ntoh64(p_pr->dgid.unicast.prefix));

				/* Find the router port that is configured to
				   handle this prefix, if any */
				osm_prefix_route_t *route = NULL;
				osm_prefix_route_t *r = (osm_prefix_route_t *)
					cl_qlist_head(&sa->p_subn->prefix_routes_list);

				while (r != (osm_prefix_route_t *)
				       cl_qlist_end(&sa->p_subn->prefix_routes_list))
				{
					if (r->prefix == p_pr->dgid.unicast.prefix ||
					    r->prefix == 0)
					{
						route = r;
						break;
					}
					r = (osm_prefix_route_t *) cl_qlist_next(&r->list_item);
				}

				if (!route) {
					/*
					  This 'error' is the client's fault (bad gid) so
					  don't enter it as an error in our own log.
					  Return an error response to the client.
					*/
					sa_status = IB_SA_MAD_STATUS_INVALID_GID;
					goto Exit;
				} else if (route->guid == 0) {
					/* first router */
					p_rtr = (osm_router_t *)
						cl_qmap_head(&sa->
							     p_subn->
							     rtr_guid_tbl);
				} else {
					p_rtr = (osm_router_t *)
						cl_qmap_get(&sa->
							    p_subn->
							    rtr_guid_tbl,
							    route->guid);
				}

				if (p_rtr ==
				    (osm_router_t *) cl_qmap_end(&sa->
								 p_subn->
								 rtr_guid_tbl))
				{
					OSM_LOG(sa->p_log, OSM_LOG_ERROR,
						"ERR 1F22: "
						"Off subnet DGID but router not found\n");
					sa_status =
					    IB_SA_MAD_STATUS_INVALID_GID;
					goto Exit;
				}

				p_rtr_port = osm_router_get_port_ptr(p_rtr);
				dest_guid = osm_port_get_guid(p_rtr_port);
				if (p_dgid)
					*p_dgid = p_pr->dgid;
			}
		}

		*pp_dest_port = osm_get_port_by_guid(sa->p_subn, dest_guid);
		if (!*pp_dest_port) {
			/*
			   This 'error' is the client's fault (bad gid) so
			   don't enter it as an error in our own log.
			   Return an error response to the client.
			 */
			OSM_LOG(sa->p_log, OSM_LOG_VERBOSE,
				"No dest port with GUID 0x%016" PRIx64 "\n",
				cl_ntoh64(dest_guid));

			sa_status = IB_SA_MAD_STATUS_INVALID_GID;
			goto Exit;
		}
	} else {
		*pp_dest_port = 0;
		if (comp_mask & IB_PR_COMPMASK_DLID) {
			status = cl_ptr_vector_at(&sa->p_subn->port_lid_tbl,
						  cl_ntoh16(p_pr->dlid),
						  (void **)pp_dest_port);

			if ((status != CL_SUCCESS) || (*pp_dest_port == NULL)) {
				/*
				   This 'error' is the client's fault (bad lid)
				   so don't enter it as an error in our own log.
				   Return an error response to the client.
				 */
				OSM_LOG(sa->p_log, OSM_LOG_VERBOSE,
					"No dest port with LID %u\n",
					cl_ntoh16(p_pr->dlid));

				sa_status = IB_SA_MAD_STATUS_NO_RECORDS;
				goto Exit;
			}
		}
	}

Exit:
	OSM_LOG_EXIT(sa->p_log);
	return (sa_status);
}

/**********************************************************************
 **********************************************************************/
static void
__osm_pr_rcv_process_world(IN osm_sa_t * sa,
			   IN const osm_madw_t * const p_madw,
			   IN const osm_port_t * const requester_port,
			   IN const ib_gid_t * const p_dgid,
			   IN const ib_net64_t comp_mask,
			   IN cl_qlist_t * const p_list)
{
	const cl_qmap_t *p_tbl;
	const osm_port_t *p_dest_port;
	const osm_port_t *p_src_port;

	OSM_LOG_ENTER(sa->p_log);

	/*
	   Iterate the entire port space over itself.
	   A path record from a port to itself is legit, so no
	   need for a special case there.

	   We compute both A -> B and B -> A, since we don't have
	   any check to determine the reversability of the paths.
	 */
	p_tbl = &sa->p_subn->port_guid_tbl;

	p_dest_port = (osm_port_t *) cl_qmap_head(p_tbl);
	while (p_dest_port != (osm_port_t *) cl_qmap_end(p_tbl)) {
		p_src_port = (osm_port_t *) cl_qmap_head(p_tbl);
		while (p_src_port != (osm_port_t *) cl_qmap_end(p_tbl)) {
			__osm_pr_rcv_get_port_pair_paths(sa, p_madw,
							 requester_port,
							 p_src_port,
							 p_dest_port, p_dgid,
							 comp_mask, p_list);

			p_src_port =
			    (osm_port_t *) cl_qmap_next(&p_src_port->map_item);
		}

		p_dest_port =
		    (osm_port_t *) cl_qmap_next(&p_dest_port->map_item);
	}

	OSM_LOG_EXIT(sa->p_log);
}

/**********************************************************************
 **********************************************************************/
static void
__osm_pr_rcv_process_half(IN osm_sa_t * sa,
			  IN const osm_madw_t * const p_madw,
			  IN const osm_port_t * const requester_port,
			  IN const osm_port_t * const p_src_port,
			  IN const osm_port_t * const p_dest_port,
			  IN const ib_gid_t * const p_dgid,
			  IN const ib_net64_t comp_mask,
			  IN cl_qlist_t * const p_list)
{
	const cl_qmap_t *p_tbl;
	const osm_port_t *p_port;

	OSM_LOG_ENTER(sa->p_log);

	/*
	   Iterate over every port, looking for matches...
	   A path record from a port to itself is legit, so no
	   need to special case that one.
	 */
	p_tbl = &sa->p_subn->port_guid_tbl;

	if (p_src_port) {
		/*
		   The src port if fixed, so iterate over destination ports.
		 */
		p_port = (osm_port_t *) cl_qmap_head(p_tbl);
		while (p_port != (osm_port_t *) cl_qmap_end(p_tbl)) {
			__osm_pr_rcv_get_port_pair_paths(sa, p_madw,
							 requester_port,
							 p_src_port, p_port,
							 p_dgid, comp_mask,
							 p_list);
			p_port = (osm_port_t *) cl_qmap_next(&p_port->map_item);
		}
	} else {
		/*
		   The dest port if fixed, so iterate over source ports.
		 */
		p_port = (osm_port_t *) cl_qmap_head(p_tbl);
		while (p_port != (osm_port_t *) cl_qmap_end(p_tbl)) {
			__osm_pr_rcv_get_port_pair_paths(sa, p_madw,
							 requester_port, p_port,
							 p_dest_port, p_dgid,
							 comp_mask, p_list);
			p_port = (osm_port_t *) cl_qmap_next(&p_port->map_item);
		}
	}

	OSM_LOG_EXIT(sa->p_log);
}

/**********************************************************************
 **********************************************************************/
static void
__osm_pr_rcv_process_pair(IN osm_sa_t * sa,
			  IN const osm_madw_t * const p_madw,
			  IN const osm_port_t * const requester_port,
			  IN const osm_port_t * const p_src_port,
			  IN const osm_port_t * const p_dest_port,
			  IN const ib_gid_t * const p_dgid,
			  IN const ib_net64_t comp_mask,
			  IN cl_qlist_t * const p_list)
{
	OSM_LOG_ENTER(sa->p_log);

	__osm_pr_rcv_get_port_pair_paths(sa, p_madw, requester_port,
					 p_src_port, p_dest_port, p_dgid,
					 comp_mask, p_list);

	OSM_LOG_EXIT(sa->p_log);
}

/**********************************************************************
 **********************************************************************/
static osm_mgrp_t *pr_get_mgrp(IN osm_sa_t * sa,
			       IN const osm_madw_t * const p_madw)
{
	ib_path_rec_t *p_pr;
	const ib_sa_mad_t *p_sa_mad;
	ib_net64_t comp_mask;
	osm_mgrp_t *mgrp = NULL;

	p_sa_mad = osm_madw_get_sa_mad_ptr(p_madw);
	p_pr = (ib_path_rec_t *) ib_sa_mad_get_payload_ptr(p_sa_mad);

	comp_mask = p_sa_mad->comp_mask;

	if ((comp_mask & IB_PR_COMPMASK_DGID) &&
	    !(mgrp = osm_get_mgrp_by_mgid(sa, &p_pr->dgid))) {
		char gid_str[INET6_ADDRSTRLEN];
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F09: "
			"No MC group found for PathRecord destination GID %s\n",
			inet_ntop(AF_INET6, p_pr->dgid.raw, gid_str,
				  sizeof gid_str));
		goto Exit;
	}

	if (comp_mask & IB_PR_COMPMASK_DLID) {
		if (mgrp) {
			/* check that the MLID in the MC group is */
			/* the same as the DLID in the PathRecord */
			if (mgrp->mlid != p_pr->dlid) {
				/* Note: perhaps this might be better indicated as an invalid request */
				OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F10: "
					"MC group MLID 0x%x does not match "
					"PathRecord destination LID 0x%x\n",
					mgrp->mlid, p_pr->dlid);
				mgrp = NULL;
				goto Exit;
			}
		} else if (!(mgrp = osm_get_mgrp_by_mlid(sa->p_subn, p_pr->dlid)))
			OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F11: "
				"No MC group found for PathRecord "
				"destination LID 0x%x\n", p_pr->dlid);
	}

Exit:
	return mgrp;
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
__osm_pr_match_mgrp_attributes(IN osm_sa_t * sa,
			       IN const osm_madw_t * const p_madw,
			       IN const osm_mgrp_t * const p_mgrp)
{
	const ib_path_rec_t *p_pr;
	const ib_sa_mad_t *p_sa_mad;
	ib_net64_t comp_mask;
	ib_api_status_t status = IB_ERROR;
	uint32_t flow_label;
	uint8_t sl;
	uint8_t hop_limit;

	OSM_LOG_ENTER(sa->p_log);

	p_sa_mad = osm_madw_get_sa_mad_ptr(p_madw);
	p_pr = (ib_path_rec_t *) ib_sa_mad_get_payload_ptr(p_sa_mad);

	comp_mask = p_sa_mad->comp_mask;

	/* If SGID and/or SLID specified, should validate as member of MC group */
	/* Also, MTU, rate, packet lifetime, and raw traffic requested are not currently checked */
	if (comp_mask & IB_PR_COMPMASK_PKEY) {
		if (p_pr->pkey != p_mgrp->mcmember_rec.pkey)
			goto Exit;
	}

	ib_member_get_sl_flow_hop(p_mgrp->mcmember_rec.sl_flow_hop,
				  &sl, &flow_label, &hop_limit);

	if (comp_mask & IB_PR_COMPMASK_SL) {
		if (ib_path_rec_sl(p_pr) != sl)
			goto Exit;
	}

	/* If SubnAdmGet, assume NumbPaths of 1 (1.2 erratum) */
	if ((comp_mask & IB_PR_COMPMASK_NUMBPATH) &&
	    (p_sa_mad->method != IB_MAD_METHOD_GET)) {
		if (ib_path_rec_num_path(p_pr) == 0)
			goto Exit;
	}

	if (comp_mask & IB_PR_COMPMASK_FLOWLABEL) {
		if (ib_path_rec_flow_lbl(p_pr) != flow_label)
			goto Exit;
	}

	if (comp_mask & IB_PR_COMPMASK_HOPLIMIT) {
		if (ib_path_rec_hop_limit(p_pr) != hop_limit)
			goto Exit;
	}

	if (comp_mask & IB_PR_COMPMASK_TCLASS) {
		if (p_pr->tclass != p_mgrp->mcmember_rec.tclass)
			goto Exit;
	}

	status = IB_SUCCESS;

Exit:
	OSM_LOG_EXIT(sa->p_log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static int
__osm_pr_rcv_check_mcast_dest(IN osm_sa_t * sa,
			      IN const osm_madw_t * const p_madw)
{
	const ib_path_rec_t *p_pr;
	const ib_sa_mad_t *p_sa_mad;
	ib_net64_t comp_mask;
	int is_multicast = 0;

	OSM_LOG_ENTER(sa->p_log);

	p_sa_mad = osm_madw_get_sa_mad_ptr(p_madw);
	p_pr = (ib_path_rec_t *) ib_sa_mad_get_payload_ptr(p_sa_mad);

	comp_mask = p_sa_mad->comp_mask;

	if (comp_mask & IB_PR_COMPMASK_DGID) {
		is_multicast = ib_gid_is_multicast(&p_pr->dgid);
		if (!is_multicast)
			goto Exit;
	}

	if (comp_mask & IB_PR_COMPMASK_DLID) {
		if (cl_ntoh16(p_pr->dlid) >= IB_LID_MCAST_START_HO &&
		    cl_ntoh16(p_pr->dlid) <= IB_LID_MCAST_END_HO)
			is_multicast = 1;
		else if (is_multicast) {
			OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F12: "
				"PathRecord request indicates MGID but not MLID\n");
			is_multicast = -1;
		}
	}

Exit:
	OSM_LOG_EXIT(sa->p_log);
	return (is_multicast);
}

/**********************************************************************
 **********************************************************************/
void osm_pr_rcv_process(IN void *context, IN void *data)
{
	osm_sa_t *sa = context;
	osm_madw_t *p_madw = data;
	const ib_path_rec_t *p_pr;
	const ib_sa_mad_t *p_sa_mad;
	const osm_port_t *p_src_port;
	const osm_port_t *p_dest_port;
	cl_qlist_t pr_list;
	ib_gid_t dgid;
	ib_net16_t sa_status;
	osm_port_t *requester_port;
	int ret;

	OSM_LOG_ENTER(sa->p_log);

	CL_ASSERT(p_madw);

	p_sa_mad = osm_madw_get_sa_mad_ptr(p_madw);
	p_pr = (ib_path_rec_t *) ib_sa_mad_get_payload_ptr(p_sa_mad);

	CL_ASSERT(p_sa_mad->attr_id == IB_MAD_ATTR_PATH_RECORD);

	/* we only support SubnAdmGet and SubnAdmGetTable methods */
	if (p_sa_mad->method != IB_MAD_METHOD_GET &&
	    p_sa_mad->method != IB_MAD_METHOD_GETTABLE) {
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F17: "
			"Unsupported Method (%s)\n",
			ib_get_sa_method_str(p_sa_mad->method));
		osm_sa_send_error(sa, p_madw, IB_MAD_STATUS_UNSUP_METHOD_ATTR);
		goto Exit;
	}

	/* update the requester physical port. */
	requester_port = osm_get_port_by_mad_addr(sa->p_log, sa->p_subn,
						  osm_madw_get_mad_addr_ptr
						  (p_madw));
	if (requester_port == NULL) {
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F16: "
			"Cannot find requester physical port\n");
		goto Exit;
	}

	if (osm_log_is_active(sa->p_log, OSM_LOG_DEBUG))
		osm_dump_path_record(sa->p_log, p_pr, OSM_LOG_DEBUG);

	cl_qlist_init(&pr_list);

	/*
	   Most SA functions (including this one) are read-only on the
	   subnet object, so we grab the lock non-exclusively.
	 */
	cl_plock_acquire(sa->p_lock);

	/* Handle multicast destinations separately */
	if ((ret = __osm_pr_rcv_check_mcast_dest(sa, p_madw)) < 0) {
		/* Multicast DGID with unicast DLID */
		cl_plock_release(sa->p_lock);
		osm_sa_send_error(sa, p_madw, IB_MAD_STATUS_INVALID_FIELD);
		goto Exit;
	}

	if (ret > 0)
		goto McastDest;

	OSM_LOG(sa->p_log, OSM_LOG_DEBUG, "Unicast destination requested\n");

	sa_status = __osm_pr_rcv_get_end_points(sa, p_madw,
						&p_src_port, &p_dest_port,
						&dgid);

	if (sa_status == IB_SA_MAD_STATUS_SUCCESS) {
		/*
		   What happens next depends on the type of endpoint information
		   that was specified....
		 */
		if (p_src_port) {
			if (p_dest_port)
				__osm_pr_rcv_process_pair(sa, p_madw,
							  requester_port,
							  p_src_port,
							  p_dest_port, &dgid,
							  p_sa_mad->comp_mask,
							  &pr_list);
			else
				__osm_pr_rcv_process_half(sa, p_madw,
							  requester_port,
							  p_src_port, NULL,
							  &dgid,
							  p_sa_mad->comp_mask,
							  &pr_list);
		} else {
			if (p_dest_port)
				__osm_pr_rcv_process_half(sa, p_madw,
							  requester_port, NULL,
							  p_dest_port, &dgid,
							  p_sa_mad->comp_mask,
							  &pr_list);
			else
				/*
				   Katie, bar the door!
				 */
				__osm_pr_rcv_process_world(sa, p_madw,
							   requester_port,
							   &dgid,
							   p_sa_mad->comp_mask,
							   &pr_list);
		}
	}
	goto Unlock;

McastDest:
	OSM_LOG(sa->p_log, OSM_LOG_DEBUG, "Multicast destination requested\n");
	{
		osm_mgrp_t *p_mgrp = NULL;
		ib_api_status_t status;
		osm_pr_item_t *p_pr_item;
		uint32_t flow_label;
		uint8_t sl;
		uint8_t hop_limit;

		/* First, get the MC info */
		p_mgrp = pr_get_mgrp(sa, p_madw);

		if (!p_mgrp)
			goto Unlock;

		/* Make sure the rest of the PathRecord matches the MC group attributes */
		status = __osm_pr_match_mgrp_attributes(sa, p_madw, p_mgrp);
		if (status != IB_SUCCESS) {
			OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F19: "
				"MC group attributes don't match PathRecord request\n");
			goto Unlock;
		}

		p_pr_item = malloc(sizeof(*p_pr_item));
		if (p_pr_item == NULL) {
			OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 1F18: "
				"Unable to allocate path record for MC group\n");
			goto Unlock;
		}
		memset(p_pr_item, 0, sizeof(*p_pr_item));

		/* Copy PathRecord request into response */
		p_sa_mad = osm_madw_get_sa_mad_ptr(p_madw);
		p_pr = (ib_path_rec_t *)
		    ib_sa_mad_get_payload_ptr(p_sa_mad);
		p_pr_item->path_rec = *p_pr;

		/* Now, use the MC info to cruft up the PathRecord response */
		p_pr_item->path_rec.dgid = p_mgrp->mcmember_rec.mgid;
		p_pr_item->path_rec.dlid = p_mgrp->mcmember_rec.mlid;
		p_pr_item->path_rec.tclass = p_mgrp->mcmember_rec.tclass;
		p_pr_item->path_rec.num_path = 1;
		p_pr_item->path_rec.pkey = p_mgrp->mcmember_rec.pkey;

		/* MTU, rate, and packet lifetime should be exactly */
		p_pr_item->path_rec.mtu = (2 << 6) | p_mgrp->mcmember_rec.mtu;
		p_pr_item->path_rec.rate = (2 << 6) | p_mgrp->mcmember_rec.rate;
		p_pr_item->path_rec.pkt_life =
		    (2 << 6) | p_mgrp->mcmember_rec.pkt_life;

		/* SL, Hop Limit, and Flow Label */
		ib_member_get_sl_flow_hop(p_mgrp->mcmember_rec.sl_flow_hop,
					  &sl, &flow_label, &hop_limit);
		ib_path_rec_set_sl(&p_pr_item->path_rec, sl);
		ib_path_rec_set_qos_class(&p_pr_item->path_rec, 0);

		/* HopLimit is not yet set in non link local MC groups */
		/* If it were, this would not be needed */
		if (ib_mgid_get_scope(&p_mgrp->mcmember_rec.mgid) != IB_MC_SCOPE_LINK_LOCAL)
			hop_limit = IB_HOPLIMIT_MAX;

		p_pr_item->path_rec.hop_flow_raw =
			cl_hton32(hop_limit) | (flow_label << 8);

		cl_qlist_insert_tail(&pr_list, &p_pr_item->list_item);
	}

Unlock:
	cl_plock_release(sa->p_lock);

	/* Now, (finally) respond to the PathRecord request */
	osm_sa_respond(sa, p_madw, sizeof(ib_path_rec_t), &pr_list);

Exit:
	OSM_LOG_EXIT(sa->p_log);
}
