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
 *    Implementation of OpenSM QoS infrastructure primitives
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>

#include <iba/ib_types.h>
#include <complib/cl_qmap.h>
#include <complib/cl_debug.h>
#include <opensm/osm_opensm.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_qos_policy.h>

struct qos_config {
	uint8_t max_vls;
	uint8_t vl_high_limit;
	ib_vl_arb_table_t vlarb_high[2];
	ib_vl_arb_table_t vlarb_low[2];
	ib_slvl_table_t sl2vl;
};

static void qos_build_config(struct qos_config *cfg,
			     osm_qos_options_t * opt, osm_qos_options_t * dflt);

/*
 * QoS primitives
 */
static ib_api_status_t vlarb_update_table_block(osm_sm_t * sm,
						osm_physp_t * p,
						uint8_t port_num,
						unsigned force_update,
						const ib_vl_arb_table_t *
						table_block,
						unsigned block_length,
						unsigned block_num)
{
	ib_vl_arb_table_t block;
	osm_madw_context_t context;
	uint32_t attr_mod;
	unsigned vl_mask, i;

	vl_mask = (1 << (ib_port_info_get_op_vls(&p->port_info) - 1)) - 1;

	memset(&block, 0, sizeof(block));
	memcpy(&block, table_block, block_length * sizeof(block.vl_entry[0]));
	for (i = 0; i < block_length; i++)
		block.vl_entry[i].vl &= vl_mask;

	if (!force_update &&
	    !memcmp(&p->vl_arb[block_num], &block,
		    block_length * sizeof(block.vl_entry[0])))
		return IB_SUCCESS;

	context.vla_context.node_guid =
	    osm_node_get_node_guid(osm_physp_get_node_ptr(p));
	context.vla_context.port_guid = osm_physp_get_port_guid(p);
	context.vla_context.set_method = TRUE;
	attr_mod = ((block_num + 1) << 16) | port_num;

	return osm_req_set(sm, osm_physp_get_dr_path_ptr(p),
			   (uint8_t *) & block, sizeof(block),
			   IB_MAD_ATTR_VL_ARBITRATION,
			   cl_hton32(attr_mod), CL_DISP_MSGID_NONE, &context);
}

static ib_api_status_t vlarb_update(osm_sm_t * sm,
				    osm_physp_t * p, uint8_t port_num,
				    unsigned force_update,
				    const struct qos_config *qcfg)
{
	ib_api_status_t status = IB_SUCCESS;
	ib_port_info_t *p_pi = &p->port_info;
	unsigned len;

	if (p_pi->vl_arb_low_cap > 0) {
		len = p_pi->vl_arb_low_cap < IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK ?
		    p_pi->vl_arb_low_cap : IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK;
		if ((status = vlarb_update_table_block(sm, p, port_num,
						       force_update,
						       &qcfg->vlarb_low[0],
						       len, 0)) != IB_SUCCESS)
			return status;
	}
	if (p_pi->vl_arb_low_cap > IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK) {
		len = p_pi->vl_arb_low_cap % IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK;
		if ((status = vlarb_update_table_block(sm, p, port_num,
						       force_update,
						       &qcfg->vlarb_low[1],
						       len, 1)) != IB_SUCCESS)
			return status;
	}
	if (p_pi->vl_arb_high_cap > 0) {
		len = p_pi->vl_arb_high_cap < IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK ?
		    p_pi->vl_arb_high_cap : IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK;
		if ((status = vlarb_update_table_block(sm, p, port_num,
						       force_update,
						       &qcfg->vlarb_high[0],
						       len, 2)) != IB_SUCCESS)
			return status;
	}
	if (p_pi->vl_arb_high_cap > IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK) {
		len = p_pi->vl_arb_high_cap % IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK;
		if ((status = vlarb_update_table_block(sm, p, port_num,
						       force_update,
						       &qcfg->vlarb_high[1],
						       len, 3)) != IB_SUCCESS)
			return status;
	}

	return status;
}

static ib_api_status_t sl2vl_update_table(osm_sm_t * sm,
					  osm_physp_t * p, uint8_t in_port,
					  uint8_t out_port,
					  unsigned force_update,
					  const ib_slvl_table_t * sl2vl_table)
{
	osm_madw_context_t context;
	ib_slvl_table_t tbl, *p_tbl;
	osm_node_t *p_node = osm_physp_get_node_ptr(p);
	uint32_t attr_mod;
	unsigned vl_mask;
	uint8_t vl1, vl2;
	int i;

	vl_mask = (1 << (ib_port_info_get_op_vls(&p->port_info) - 1)) - 1;

	for (i = 0; i < IB_MAX_NUM_VLS / 2; i++) {
		vl1 = sl2vl_table->raw_vl_by_sl[i] >> 4;
		vl2 = sl2vl_table->raw_vl_by_sl[i] & 0xf;
		if (vl1 != 15)
			vl1 &= vl_mask;
		if (vl2 != 15)
			vl2 &= vl_mask;
		tbl.raw_vl_by_sl[i] = (vl1 << 4) | vl2;
	}

	if (!force_update && (p_tbl = osm_physp_get_slvl_tbl(p, in_port)) &&
	    !memcmp(p_tbl, &tbl, sizeof(tbl)))
		return IB_SUCCESS;

	context.slvl_context.node_guid = osm_node_get_node_guid(p_node);
	context.slvl_context.port_guid = osm_physp_get_port_guid(p);
	context.slvl_context.set_method = TRUE;
	attr_mod = in_port << 8 | out_port;
	return osm_req_set(sm, osm_physp_get_dr_path_ptr(p),
			   (uint8_t *) & tbl, sizeof(tbl),
			   IB_MAD_ATTR_SLVL_TABLE,
			   cl_hton32(attr_mod), CL_DISP_MSGID_NONE, &context);
}

static ib_api_status_t sl2vl_update(osm_sm_t * sm, osm_port_t * p_port,
				    osm_physp_t * p, uint8_t port_num,
				    unsigned force_update,
				    const struct qos_config *qcfg)
{
	ib_api_status_t status;
	uint8_t i, num_ports;
	osm_physp_t *p_physp;

	if (osm_node_get_type(osm_physp_get_node_ptr(p)) == IB_NODE_TYPE_SWITCH) {
		if (ib_port_info_get_vl_cap(&p->port_info) == 1) {
			/* Check port 0's capability mask */
			p_physp = p_port->p_physp;
			if (!
			    (p_physp->port_info.
			     capability_mask & IB_PORT_CAP_HAS_SL_MAP))
				return IB_SUCCESS;
		}
		num_ports = osm_node_get_num_physp(osm_physp_get_node_ptr(p));
	} else {
		if (!(p->port_info.capability_mask & IB_PORT_CAP_HAS_SL_MAP))
			return IB_SUCCESS;
		num_ports = 1;
	}

	for (i = 0; i < num_ports; i++) {
		status =
		    sl2vl_update_table(sm, p, i, port_num,
				       force_update, &qcfg->sl2vl);
		if (status != IB_SUCCESS)
			return status;
	}

	return IB_SUCCESS;
}

static ib_api_status_t qos_physp_setup(osm_log_t * p_log, osm_sm_t * sm,
				       osm_port_t * p_port, osm_physp_t * p,
				       uint8_t port_num,
				       unsigned force_update,
				       const struct qos_config *qcfg)
{
	ib_api_status_t status;

	/* OpVLs should be ok at this moment - just use it */

	/* setup VL high limit on the physp later to be updated by link mgr */
	p->vl_high_limit = qcfg->vl_high_limit;

	/* setup VLArbitration */
	status = vlarb_update(sm, p, port_num, force_update, qcfg);
	if (status != IB_SUCCESS) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 6202 : "
			"failed to update VLArbitration tables "
			"for port %" PRIx64 " #%d\n",
			cl_ntoh64(p->port_guid), port_num);
		return status;
	}

	/* setup SL2VL tables */
	status = sl2vl_update(sm, p_port, p, port_num, force_update, qcfg);
	if (status != IB_SUCCESS) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 6203 : "
			"failed to update SL2VLMapping tables "
			"for port %" PRIx64 " #%d\n",
			cl_ntoh64(p->port_guid), port_num);
		return status;
	}

	return IB_SUCCESS;
}

osm_signal_t osm_qos_setup(osm_opensm_t * p_osm)
{
	struct qos_config ca_config, sw0_config, swe_config, rtr_config;
	struct qos_config *cfg;
	cl_qmap_t *p_tbl;
	cl_map_item_t *p_next;
	osm_port_t *p_port;
	uint32_t num_physp;
	osm_physp_t *p_physp;
	osm_node_t *p_node;
	ib_api_status_t status;
	unsigned force_update;
	uint8_t i;

	if (!p_osm->subn.opt.qos)
		return OSM_SIGNAL_DONE;

	OSM_LOG_ENTER(&p_osm->log);

	qos_build_config(&ca_config, &p_osm->subn.opt.qos_ca_options,
			 &p_osm->subn.opt.qos_options);
	qos_build_config(&sw0_config, &p_osm->subn.opt.qos_sw0_options,
			 &p_osm->subn.opt.qos_options);
	qos_build_config(&swe_config, &p_osm->subn.opt.qos_swe_options,
			 &p_osm->subn.opt.qos_options);
	qos_build_config(&rtr_config, &p_osm->subn.opt.qos_rtr_options,
			 &p_osm->subn.opt.qos_options);

	cl_plock_excl_acquire(&p_osm->lock);

	/* read QoS policy config file */
	osm_qos_parse_policy_file(&p_osm->subn);

	p_tbl = &p_osm->subn.port_guid_tbl;
	p_next = cl_qmap_head(p_tbl);
	while (p_next != cl_qmap_end(p_tbl)) {
		p_port = (osm_port_t *) p_next;
		p_next = cl_qmap_next(p_next);

		p_node = p_port->p_node;
		if (p_node->sw) {
			num_physp = osm_node_get_num_physp(p_node);
			for (i = 1; i < num_physp; i++) {
				p_physp = osm_node_get_physp_ptr(p_node, i);
				if (!p_physp)
					continue;
				force_update = p_physp->need_update ||
				    p_osm->subn.need_update;
				status =
				    qos_physp_setup(&p_osm->log, &p_osm->sm,
						    p_port, p_physp, i,
						    force_update, &swe_config);
			}
			/* skip base port 0 */
			if (!ib_switch_info_is_enhanced_port0
			    (&p_node->sw->switch_info))
				continue;

			cfg = &sw0_config;
		} else if (osm_node_get_type(p_node) == IB_NODE_TYPE_ROUTER)
			cfg = &rtr_config;
		else
			cfg = &ca_config;

		p_physp = p_port->p_physp;
		if (!p_physp)
			continue;

		force_update = p_physp->need_update || p_osm->subn.need_update;
		status = qos_physp_setup(&p_osm->log, &p_osm->sm,
					 p_port, p_physp, 0, force_update, cfg);
	}

	cl_plock_release(&p_osm->lock);
	OSM_LOG_EXIT(&p_osm->log);

	return OSM_SIGNAL_DONE;
}

/*
 *  QoS config stuff
 */
static int parse_one_unsigned(char *str, char delim, unsigned *val)
{
	char *end;
	*val = strtoul(str, &end, 0);
	if (*end)
		end++;
	return (int)(end - str);
}

static int parse_vlarb_entry(char *str, ib_vl_arb_element_t * e)
{
	unsigned val;
	char *p = str;
	p += parse_one_unsigned(p, ':', &val);
	e->vl = val % 15;
	p += parse_one_unsigned(p, ',', &val);
	e->weight = (uint8_t) val;
	return (int)(p - str);
}

static int parse_sl2vl_entry(char *str, uint8_t * raw)
{
	unsigned val1, val2;
	char *p = str;
	p += parse_one_unsigned(p, ',', &val1);
	p += parse_one_unsigned(p, ',', &val2);
	*raw = (val1 << 4) | (val2 & 0xf);
	return (int)(p - str);
}

static void qos_build_config(struct qos_config *cfg,
			     osm_qos_options_t * opt, osm_qos_options_t * dflt)
{
	int i;
	char *p;

	memset(cfg, 0, sizeof(*cfg));

	cfg->max_vls = opt->max_vls > 0 ? opt->max_vls : dflt->max_vls;

	if (opt->high_limit >= 0)
		cfg->vl_high_limit = (uint8_t) opt->high_limit;
	else
		cfg->vl_high_limit = (uint8_t) dflt->high_limit;

	p = opt->vlarb_high ? opt->vlarb_high : dflt->vlarb_high;
	for (i = 0; i < 2 * IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK; i++) {
		p += parse_vlarb_entry(p,
				       &cfg->vlarb_high[i /
							IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK].
				       vl_entry[i %
						IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK]);
	}

	p = opt->vlarb_low ? opt->vlarb_low : dflt->vlarb_low;
	for (i = 0; i < 2 * IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK; i++) {
		p += parse_vlarb_entry(p,
				       &cfg->vlarb_low[i /
						       IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK].
				       vl_entry[i %
						IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK]);
	}

	p = opt->sl2vl ? opt->sl2vl : dflt->sl2vl;
	for (i = 0; i < IB_MAX_NUM_VLS / 2; i++)
		p += parse_sl2vl_entry(p, &cfg->sl2vl.raw_vl_by_sl[i]);

}
