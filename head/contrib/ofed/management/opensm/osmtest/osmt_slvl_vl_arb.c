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
 *    Implementation of SLtoVL and VL Arbitration testing flow..
 *    Top level is osmt_run_slvl_and_vlarb_records_flow:
 *     osmt_query_all_ports_vl_arb
 *     osmt_query_all_ports_slvl_map
 *
 */

#ifndef __WIN__
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <complib/cl_debug.h>
#include "osmtest.h"

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osmtest_write_vl_arb_table(IN osmtest_t * const p_osmt,
			   IN FILE * fh,
			   IN const ib_vl_arb_table_record_t * const p_rec)
{
	int result, i;
	cl_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(&p_osmt->log);

	result = fprintf(fh,
			 "VL_ARBITRATION_TABLE\n"
			 "lid                     0x%X\n"
			 "port_num                0x%X\n"
			 "block                   0x%X\n",
			 cl_ntoh16(p_rec->lid),
			 p_rec->port_num, p_rec->block_num);

	fprintf(fh, "       ");
	for (i = 0; i < 32; i++)
		fprintf(fh, "| %-2u ", i);
	fprintf(fh, "|\nVL:    ");

	for (i = 0; i < 32; i++)
		fprintf(fh, "|0x%02X", p_rec->vl_arb_tbl.vl_entry[i].vl);
	fprintf(fh, "|\nWEIGHT:");

	for (i = 0; i < 32; i++)
		fprintf(fh, "|0x%02X", p_rec->vl_arb_tbl.vl_entry[i].weight);
	fprintf(fh, "|\nEND\n\n");

	/*  Exit: */
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 * GET A SINGLE PORT INFO BY NODE LID AND PORT NUMBER
 **********************************************************************/
ib_api_status_t
osmt_query_vl_arb(IN osmtest_t * const p_osmt,
		  IN ib_net16_t const lid,
		  IN uint8_t const port_num,
		  IN uint8_t const block_num, IN FILE * fh)
{
	osmtest_req_context_t context;
	ib_api_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;
	ib_vl_arb_table_record_t record, *p_rec;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
		"Getting VL_Arbitration Table for port with LID 0x%X Num:0x%X\n",
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
	memset(&context, 0, sizeof(context));

	context.p_osmt = p_osmt;

	record.lid = lid;
	record.port_num = port_num;
	record.block_num = block_num;
	user.p_attr = &record;

	req.query_type = OSMV_QUERY_VLARB_BY_LID_PORT_BLOCK;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = &context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0405: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = context.result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0466: "
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

	/* ok it worked */
	p_rec = osmv_get_query_result(context.result.p_result_madw, 0);
	if (fh) {
		osmtest_write_vl_arb_table(p_osmt, fh, p_rec);
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

static ib_api_status_t
osmt_query_all_ports_vl_arb(IN osmtest_t * const p_osmt, IN FILE * fh)
{
	cl_status_t status = CL_SUCCESS;
	cl_qmap_t *p_tbl;
	port_t *p_src_port;
	uint8_t block, anyErr = 0;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Obtaining ALL Ports VL Arbitration Tables\n");

	/*
	 * Go over all ports that exist in the subnet
	 * get the relevant VLarbs
	 */

	p_tbl = &p_osmt->exp_subn.port_key_tbl;

	p_src_port = (port_t *) cl_qmap_head(p_tbl);

	while (p_src_port != (port_t *) cl_qmap_end(p_tbl)) {

		/* HACK we use capability_mask to know diff a CA port from switch port */
		if (p_src_port->rec.port_info.capability_mask) {
			/* this is an hca port */
			for (block = 1; block <= 4; block++) {
				/*  NOTE to comply we must set port number to 0 and the SA should figure it out */
				/*  since it is a CA port */
				status =
				    osmt_query_vl_arb(p_osmt,
						      p_src_port->rec.lid, 0,
						      block, fh);
				if (status != IB_SUCCESS) {
					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"ERR 0467: "
						"Failed to get Lid:0x%X Port:0x%X (%s)\n",
						cl_ntoh16(p_src_port->rec.lid),
						0, ib_get_err_str(status));
					anyErr = 1;
				}
			}
		} else {
			/* this is a switch port */
			for (block = 1; block <= 4; block++) {
				status =
				    osmt_query_vl_arb(p_osmt,
						      p_src_port->rec.lid,
						      p_src_port->rec.port_num,
						      block, fh);
				if (status != IB_SUCCESS) {
					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"ERR 0468: "
						"Failed to get Lid:0x%X Port:0x%X (%s)\n",
						cl_ntoh16(p_src_port->rec.lid),
						p_src_port->rec.port_num,
						ib_get_err_str(status));
					anyErr = 1;
				}
			}
		}

		p_src_port = (port_t *) cl_qmap_next(&p_src_port->map_item);
	}

	OSM_LOG_EXIT(&p_osmt->log);
	if (anyErr) {
		status = IB_ERROR;
	}
	return (status);
}

/*******************************************************************************
 SLtoVL
*******************************************************************************/
static ib_api_status_t
osmtest_write_slvl_map_table(IN osmtest_t * const p_osmt,
			     IN FILE * fh,
			     IN const ib_slvl_table_record_t * const p_rec)
{
	int result, i;
	cl_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(&p_osmt->log);

	result = fprintf(fh,
			 "SLtoVL_MAP_TABLE\n"
			 "lid                     0x%X\n"
			 "in_port_num             0x%X\n"
			 "out_port_num            0x%X\n",
			 cl_ntoh16(p_rec->lid),
			 p_rec->in_port_num, p_rec->out_port_num);

	fprintf(fh, "SL:");
	for (i = 0; i < 16; i++)
		fprintf(fh, "| %-2u  ", i);
	fprintf(fh, "|\nVL:");

	for (i = 0; i < 16; i++)
		fprintf(fh, "| 0x%01X ",
			ib_slvl_table_get(&p_rec->slvl_tbl, (uint8_t) i));
	fprintf(fh, "|\nEND\n\n");

	/*  Exit: */
	OSM_LOG_EXIT(&p_osmt->log);
	return (status);
}

/**********************************************************************
 * GET A SINGLE PORT INFO BY NODE LID AND PORT NUMBER
 **********************************************************************/
ib_api_status_t
osmt_query_slvl_map(IN osmtest_t * const p_osmt,
		    IN ib_net16_t const lid,
		    IN uint8_t const out_port_num,
		    IN uint8_t const in_port_num, IN FILE * fh)
{
	osmtest_req_context_t context;
	ib_api_status_t status = IB_SUCCESS;
	osmv_user_query_t user;
	osmv_query_req_t req;
	ib_slvl_table_record_t record, *p_rec;

	OSM_LOG_ENTER(&p_osmt->log);

	OSM_LOG(&p_osmt->log, OSM_LOG_DEBUG,
		"Getting SLtoVL Map Table for out-port with LID 0x%X Num:0x%X from In-Port:0x%X\n",
		cl_ntoh16(lid), out_port_num, in_port_num);

	/*
	 * Do a blocking query for this record in the subnet.
	 * The result is returned in the result field of the caller's
	 * context structure.
	 *
	 * The query structures are locals.
	 */
	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));
	memset(&context, 0, sizeof(context));

	context.p_osmt = p_osmt;

	record.lid = lid;
	record.in_port_num = in_port_num;
	record.out_port_num = out_port_num;
	user.p_attr = &record;

	req.query_type = OSMV_QUERY_SLVL_BY_LID_AND_PORTS;
	req.timeout_ms = p_osmt->opt.transaction_timeout;
	req.retry_cnt = p_osmt->opt.retry_count;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = &context;
	req.pfn_query_cb = osmtest_query_res_cb;
	req.p_query_input = &user;
	req.sm_key = 0;

	status = osmv_query_sa(p_osmt->h_bind, &req);

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0469: "
			"ib_query failed (%s)\n", ib_get_err_str(status));
		goto Exit;
	}

	status = context.result.status;

	if (status != IB_SUCCESS) {
		OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0470: "
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

	/* ok it worked */
	p_rec = osmv_get_query_result(context.result.p_result_madw, 0);
	if (fh) {
		osmtest_write_slvl_map_table(p_osmt, fh, p_rec);
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

static ib_api_status_t
osmt_query_all_ports_slvl_map(IN osmtest_t * const p_osmt, IN FILE * fh)
{
	cl_status_t status = CL_SUCCESS;
	cl_qmap_t *p_tbl;
	port_t *p_src_port;
	uint8_t in_port, anyErr = 0, num_ports;
	node_t *p_node;
	const cl_qmap_t *p_node_tbl;

	OSM_LOG_ENTER(&p_osmt->log);

	/*
	 * Go over all ports that exist in the subnet
	 * get the relevant SLtoVLs
	 */

	OSM_LOG(&p_osmt->log, OSM_LOG_VERBOSE,
		"Obtaining ALL Ports (to other ports) SLtoVL Maps\n");

	p_tbl = &p_osmt->exp_subn.port_key_tbl;
	p_node_tbl = &p_osmt->exp_subn.node_lid_tbl;

	p_src_port = (port_t *) cl_qmap_head(p_tbl);

	while (p_src_port != (port_t *) cl_qmap_end(p_tbl)) {

		/* HACK we use capability_mask to know diff a CA port from switch port */
		if (p_src_port->rec.port_info.capability_mask) {
			/* this is an hca port */
			/*  NOTE to comply we must set port number to 0 and the SA should figure it out */
			/*  since it is a CA port */
			status =
			    osmt_query_slvl_map(p_osmt, p_src_port->rec.lid, 0,
						0, fh);
			if (status != IB_SUCCESS) {
				OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0471: "
					"Failed to get Lid:0x%X In-Port:0x%X Out-Port:0x%X(%s)\n",
					cl_ntoh16(p_src_port->rec.lid), 0, 0,
					ib_get_err_str(status));
				anyErr = 1;
			}
		} else {
			/* this is a switch port */
			/* get the node */
			p_node =
			    (node_t *) cl_qmap_get(p_node_tbl,
						   p_src_port->rec.lid);
			if (p_node == (node_t *) cl_qmap_end(p_node_tbl)) {
				OSM_LOG(&p_osmt->log, OSM_LOG_ERROR, "ERR 0472: "
					"Failed to get Node by Lid:0x%X\n",
					p_src_port->rec.lid);
				goto Exit;
			}

			num_ports = p_node->rec.node_info.num_ports;

			for (in_port = 1; in_port <= num_ports; in_port++) {
				status =
				    osmt_query_slvl_map(p_osmt,
							p_src_port->rec.lid,
							p_src_port->rec.
							port_num, in_port, fh);
				if (status != IB_SUCCESS) {
					OSM_LOG(&p_osmt->log, OSM_LOG_ERROR,
						"ERR 0473: "
						"Failed to get Lid:0x%X In-Port:0x%X Out-Port:0x%X (%s)\n",
						cl_ntoh16(p_src_port->rec.lid),
						p_src_port->rec.port_num,
						in_port,
						ib_get_err_str(status));
					anyErr = 1;
				}
			}
		}

		p_src_port = (port_t *) cl_qmap_next(&p_src_port->map_item);
	}

Exit:
	OSM_LOG_EXIT(&p_osmt->log);
	if (anyErr) {
		status = IB_ERROR;
	}
	return (status);
}

/*
 * Run a vl arbitration queries and sl2vl maps queries flow:
 * Good flow:
 * - for each physical port on the network - obtain the VL Arb
 * - for each CA physical port obtain its SLtoVL Map
 * - for each SW physical port (out) obtain the SLtoVL Map to each other port
 * BAD flow:
 * - Try get with multiple results
 * - Try gettable
 * - Try providing non existing port
 */
ib_api_status_t
osmt_run_slvl_and_vlarb_records_flow(IN osmtest_t * const p_osmt)
{
	ib_api_status_t status;
	FILE *fh;
	ib_net16_t test_lid;
	uint8_t lmc;

	OSM_LOG_ENTER(&p_osmt->log);

	fh = fopen("qos.txt", "w");

	/* go over all ports in the subnet */
	status = osmt_query_all_ports_vl_arb(p_osmt, fh);
	if (status != IB_SUCCESS) {
		goto Exit;
	}

	status = osmt_query_all_ports_slvl_map(p_osmt, fh);
	if (status != IB_SUCCESS) {
		goto Exit;
	}

	/* If LMC > 0, test non base LID SA QoS Record requests */
	status =
	    osmtest_get_local_port_lmc(p_osmt, p_osmt->local_port.lid, &lmc);
	if (status != IB_SUCCESS)
		goto Exit;

	if (lmc != 0) {
		test_lid = cl_ntoh16(p_osmt->local_port.lid + 1);

		status = osmt_query_vl_arb(p_osmt, test_lid, 0, 1, NULL);
		if (status != IB_SUCCESS)
			goto Exit;

		status = osmt_query_slvl_map(p_osmt, test_lid, 0, 0, NULL);
		if (status != IB_SUCCESS)
			goto Exit;
	}

Exit:
	fclose(fh);
	OSM_LOG_EXIT(&p_osmt->log);
	return status;
}
