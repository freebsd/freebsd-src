/*
 * Copyright (c) 2006-2008 Voltaire, Inc. All rights reserved.
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
 *    Implementation of osm_prtn_t.
 * This object represents an IBA partition.
 * This object is part of the opensm family of objects.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <complib/cl_debug.h>
#include <iba/ib_types.h>
#include <opensm/osm_opensm.h>
#include <opensm/osm_partition.h>
#include <opensm/osm_node.h>
#include <opensm/osm_sa.h>
#include <opensm/osm_multicast.h>

extern int osm_prtn_config_parse_file(osm_log_t * const p_log,
				      osm_subn_t * const p_subn,
				      const char *file_name);

static uint16_t global_pkey_counter;

osm_prtn_t *osm_prtn_new(IN const char *name, IN const uint16_t pkey)
{
	osm_prtn_t *p = malloc(sizeof(*p));
	if (!p)
		return NULL;

	memset(p, 0, sizeof(*p));
	p->pkey = pkey;
	p->sl = OSM_DEFAULT_SL;
	cl_map_construct(&p->full_guid_tbl);
	cl_map_init(&p->full_guid_tbl, 32);
	cl_map_construct(&p->part_guid_tbl);
	cl_map_init(&p->part_guid_tbl, 32);

	if (name && *name)
		strncpy(p->name, name, sizeof(p->name));
	else
		snprintf(p->name, sizeof(p->name), "%04x", cl_ntoh16(pkey));

	return p;
}

void osm_prtn_delete(IN OUT osm_prtn_t ** const pp_prtn)
{
	osm_prtn_t *p = *pp_prtn;

	cl_map_remove_all(&p->full_guid_tbl);
	cl_map_destroy(&p->full_guid_tbl);
	cl_map_remove_all(&p->part_guid_tbl);
	cl_map_destroy(&p->part_guid_tbl);
	free(p);
	*pp_prtn = NULL;
}

ib_api_status_t osm_prtn_add_port(osm_log_t * p_log, osm_subn_t * p_subn,
				  osm_prtn_t * p, ib_net64_t guid,
				  boolean_t full)
{
	ib_api_status_t status = IB_SUCCESS;
	cl_map_t *p_tbl;
	osm_port_t *p_port;
	osm_physp_t *p_physp;

	p_port = osm_get_port_by_guid(p_subn, guid);
	if (!p_port) {
		OSM_LOG(p_log, OSM_LOG_VERBOSE,
			"port 0x%" PRIx64 " not found\n", cl_ntoh64(guid));
		return status;
	}

	p_physp = p_port->p_physp;
	if (!p_physp) {
		OSM_LOG(p_log, OSM_LOG_VERBOSE,
			"no physical for port 0x%" PRIx64 "\n",
			cl_ntoh64(guid));
		return status;
	}

	if (cl_map_remove(&p->part_guid_tbl, guid) ||
	    cl_map_remove(&p->full_guid_tbl, guid)) {
		OSM_LOG(p_log, OSM_LOG_VERBOSE,
			"port 0x%" PRIx64 " already in "
			"partition \'%s\' (0x%04x). Will overwrite\n",
			cl_ntoh64(guid), p->name, cl_ntoh16(p->pkey));
	}

	p_tbl = (full == TRUE) ? &p->full_guid_tbl : &p->part_guid_tbl;

	if (cl_map_insert(p_tbl, guid, p_physp) == NULL)
		return IB_INSUFFICIENT_MEMORY;

	return status;
}

ib_api_status_t osm_prtn_add_all(osm_log_t * p_log, osm_subn_t * p_subn,
				 osm_prtn_t * p, boolean_t full)
{
	cl_qmap_t *p_port_tbl = &p_subn->port_guid_tbl;
	cl_map_item_t *p_item;
	osm_port_t *p_port;
	ib_api_status_t status = IB_SUCCESS;

	p_item = cl_qmap_head(p_port_tbl);
	while (p_item != cl_qmap_end(p_port_tbl)) {
		p_port = (osm_port_t *) p_item;
		p_item = cl_qmap_next(p_item);
		status = osm_prtn_add_port(p_log, p_subn, p,
					   osm_port_get_guid(p_port), full);
		if (status != IB_SUCCESS)
			goto _err;
	}

_err:
	return status;
}

static const ib_gid_t osm_ipoib_mgid = {
	{
	 0xff,			/*  multicast field */
	 0x12,			/*  non-permanent bit, link local scope */
	 0x40, 0x1b,		/*  IPv4 signature */
	 0xff, 0xff,		/*  16 bits of P_Key (to be filled in) */
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/*  48 bits of zeros */
	 0xff, 0xff, 0xff, 0xff,	/*  32 bit IPv4 broadcast address */
	 },
};

/*
 * HACK: Until TS resolves their noncompliant join compmask,
 * we have to pre-define the MGID
 */
static const ib_gid_t osm_ts_ipoib_mgid = {
	{
	 0xff,			/*  multicast field */
	 0x12,			/*  non-permanent bit, link local scope */
	 0x40, 0x1b,		/*  IPv4 signature */
	 0xff, 0xff,		/*  16 bits of P_Key (to be filled in) */
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/*  48 bits of zeros */
	 0x00, 0x00, 0x00, 0x01,	/*  32 bit IPv4 broadcast address */
	 },
};

ib_api_status_t osm_prtn_add_mcgroup(osm_log_t * p_log,
				     osm_subn_t * p_subn, osm_prtn_t * p,
				     uint8_t rate,
				     uint8_t mtu, uint8_t scope)
{
	ib_member_rec_t mc_rec;
	ib_net64_t comp_mask;
	ib_net16_t pkey;
	osm_mgrp_t *p_mgrp = NULL;
	osm_sa_t *p_sa = &p_subn->p_osm->sa;
	ib_api_status_t status = IB_SUCCESS;
	uint8_t hop_limit;

	pkey = p->pkey | cl_hton16(0x8000);
	if (!scope)
		scope = OSM_DEFAULT_MGRP_SCOPE;
	hop_limit = (scope == IB_MC_SCOPE_LINK_LOCAL) ? 0 : IB_HOPLIMIT_MAX;

	memset(&mc_rec, 0, sizeof(mc_rec));

	mc_rec.mgid = osm_ipoib_mgid;	/* ipv4 broadcast group */
	memcpy(&mc_rec.mgid.raw[4], &pkey, sizeof(pkey));

	mc_rec.qkey = CL_HTON32(0x0b1b);
	mc_rec.mtu = (mtu ? mtu : OSM_DEFAULT_MGRP_MTU) | (2 << 6);	/* 2048 Bytes */
	mc_rec.tclass = 0;
	mc_rec.pkey = pkey;
	mc_rec.rate = (rate ? rate : OSM_DEFAULT_MGRP_RATE) | (2 << 6);	/* 10Gb/sec */
	mc_rec.pkt_life = p_subn->opt.subnet_timeout;
	mc_rec.sl_flow_hop = ib_member_set_sl_flow_hop(p->sl, 0, hop_limit);
	/* Scope in MCMemberRecord (if present) needs to be consistent with MGID */
	mc_rec.scope_state = ib_member_set_scope_state(scope, IB_MC_REC_STATE_FULL_MEMBER);
	ib_mgid_set_scope(&mc_rec.mgid, scope);

	/* don't update rate, mtu */
	comp_mask = IB_MCR_COMPMASK_MTU | IB_MCR_COMPMASK_MTU_SEL |
	    IB_MCR_COMPMASK_RATE | IB_MCR_COMPMASK_RATE_SEL;
	status = osm_mcmr_rcv_find_or_create_new_mgrp(p_sa, comp_mask, &mc_rec,
						      &p_mgrp);
	if (!p_mgrp || status != IB_SUCCESS)
		OSM_LOG(p_log, OSM_LOG_ERROR,
			"Failed to create MC group with pkey 0x%04x\n",
			cl_ntoh16(pkey));
	if (p_mgrp) {
		p_mgrp->well_known = TRUE;
		p->mlid = p_mgrp->mlid;
	}

	/* workaround for TS */
	/* FIXME: remove this upon TS fixes */
	mc_rec.mgid = osm_ts_ipoib_mgid;
	memcpy(&mc_rec.mgid.raw[4], &pkey, sizeof(pkey));
	/* Scope in MCMemberRecord (if present) needs to be consistent with MGID */
	mc_rec.scope_state = ib_member_set_scope_state(scope, IB_MC_REC_STATE_FULL_MEMBER);
	ib_mgid_set_scope(&mc_rec.mgid, scope);

	status = osm_mcmr_rcv_find_or_create_new_mgrp(p_sa, comp_mask, &mc_rec,
						      &p_mgrp);
	if (p_mgrp) {
		p_mgrp->well_known = TRUE;
		if (!p->mlid)
			p->mlid = p_mgrp->mlid;
	}

	return status;
}

static uint16_t __generate_pkey(osm_subn_t * p_subn)
{
	uint16_t pkey;

	cl_qmap_t *m = &p_subn->prtn_pkey_tbl;
	while (global_pkey_counter < cl_ntoh16(IB_DEFAULT_PARTIAL_PKEY) - 1) {
		pkey = ++global_pkey_counter;
		pkey = cl_hton16(pkey);
		if (cl_qmap_get(m, pkey) == cl_qmap_end(m))
			return pkey;
	}
	return 0;
}

osm_prtn_t *osm_prtn_find_by_name(osm_subn_t * p_subn, const char *name)
{
	cl_map_item_t *p_next;
	osm_prtn_t *p;

	p_next = cl_qmap_head(&p_subn->prtn_pkey_tbl);
	while (p_next != cl_qmap_end(&p_subn->prtn_pkey_tbl)) {
		p = (osm_prtn_t *) p_next;
		p_next = cl_qmap_next(&p->map_item);
		if (!strncmp(p->name, name, sizeof(p->name)))
			return p;
	}

	return NULL;
}

osm_prtn_t *osm_prtn_make_new(osm_log_t * p_log, osm_subn_t * p_subn,
			      const char *name, uint16_t pkey)
{
	osm_prtn_t *p = NULL, *p_check;

	pkey &= cl_hton16((uint16_t) ~ 0x8000);

	if (!pkey) {
		if (name && (p = osm_prtn_find_by_name(p_subn, name)))
			return p;
		if (!(pkey = __generate_pkey(p_subn)))
			return NULL;
	}

	p = osm_prtn_new(name, pkey);
	if (!p) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "Unable to create"
			" partition \'%s\' (0x%04x)\n", name, cl_ntoh16(pkey));
		return NULL;
	}

	p_check = (osm_prtn_t *) cl_qmap_insert(&p_subn->prtn_pkey_tbl,
						p->pkey, &p->map_item);
	if (p != p_check) {
		OSM_LOG(p_log, OSM_LOG_VERBOSE, "Duplicated partition"
			" definition: \'%s\' (0x%04x) prev name \'%s\'"
			".  Will use it\n",
			name, cl_ntoh16(pkey), p_check->name);
		osm_prtn_delete(&p);
		p = p_check;
	}

	return p;
}

static ib_api_status_t osm_prtn_make_default(osm_log_t * const p_log,
					     osm_subn_t * const p_subn,
					     boolean_t no_config)
{
	ib_api_status_t status = IB_UNKNOWN_ERROR;
	osm_prtn_t *p;

	p = osm_prtn_make_new(p_log, p_subn, "Default",
			      IB_DEFAULT_PARTIAL_PKEY);
	if (!p)
		goto _err;
	status = osm_prtn_add_all(p_log, p_subn, p, no_config);
	if (status != IB_SUCCESS)
		goto _err;
	cl_map_remove(&p->part_guid_tbl, p_subn->sm_port_guid);
	status =
	    osm_prtn_add_port(p_log, p_subn, p, p_subn->sm_port_guid, TRUE);

	if (no_config)
		osm_prtn_add_mcgroup(p_log, p_subn, p, 0, 0, 0);

_err:
	return status;
}

ib_api_status_t osm_prtn_make_partitions(osm_log_t * const p_log,
					 osm_subn_t * const p_subn)
{
	struct stat statbuf;
	const char *file_name;
	boolean_t is_config = TRUE;
	ib_api_status_t status = IB_SUCCESS;
	cl_map_item_t *p_next;
	osm_prtn_t *p;

	file_name = p_subn->opt.partition_config_file ?
	    p_subn->opt.partition_config_file : OSM_DEFAULT_PARTITION_CONFIG_FILE;
	if (stat(file_name, &statbuf))
		is_config = FALSE;

	/* clean up current port maps */
	p_next = cl_qmap_head(&p_subn->prtn_pkey_tbl);
	while (p_next != cl_qmap_end(&p_subn->prtn_pkey_tbl)) {
		p = (osm_prtn_t *) p_next;
		p_next = cl_qmap_next(&p->map_item);
		cl_map_remove_all(&p->part_guid_tbl);
		cl_map_remove_all(&p->full_guid_tbl);
	}

	global_pkey_counter = 0;

	status = osm_prtn_make_default(p_log, p_subn, !is_config);
	if (status != IB_SUCCESS)
		goto _err;

	if (is_config && osm_prtn_config_parse_file(p_log, p_subn, file_name)) {
		OSM_LOG(p_log, OSM_LOG_VERBOSE, "Partition configuration "
			"was not fully processed\n");
	}

	/* and now clean up empty partitions */
	p_next = cl_qmap_head(&p_subn->prtn_pkey_tbl);
	while (p_next != cl_qmap_end(&p_subn->prtn_pkey_tbl)) {
		p = (osm_prtn_t *) p_next;
		p_next = cl_qmap_next(&p->map_item);
		if (cl_map_count(&p->part_guid_tbl) == 0 &&
		    cl_map_count(&p->full_guid_tbl) == 0) {
			cl_qmap_remove_item(&p_subn->prtn_pkey_tbl,
					    (cl_map_item_t *) p);
			osm_prtn_delete(&p);
		}
	}

_err:
	return status;
}
