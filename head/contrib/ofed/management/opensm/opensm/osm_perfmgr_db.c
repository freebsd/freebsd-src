/*
 * Copyright (c) 2008 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2007 The Regents of the University of California.
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

#ifdef ENABLE_OSM_PERF_MGR

#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <dlfcn.h>
#include <sys/stat.h>

#include <opensm/osm_perfmgr_db.h>
#include <opensm/osm_perfmgr.h>
#include <opensm/osm_opensm.h>

/** =========================================================================
 */
perfmgr_db_t *perfmgr_db_construct(osm_perfmgr_t *perfmgr)
{
	perfmgr_db_t *db = malloc(sizeof(*db));
	if (!db)
		return (NULL);

	cl_qmap_init(&(db->pc_data));
	cl_plock_construct(&(db->lock));
	cl_plock_init(&(db->lock));
	db->perfmgr = perfmgr;
	return ((void *)db);
}

/** =========================================================================
 */
void perfmgr_db_destroy(perfmgr_db_t * db)
{
	if (db) {
		cl_plock_destroy(&(db->lock));
		free(db);
	}
}

/**********************************************************************
 * Internal call db->lock should be held when calling
 **********************************************************************/
static inline _db_node_t *_get(perfmgr_db_t * db, uint64_t guid)
{
	cl_map_item_t *rc = cl_qmap_get(&(db->pc_data), guid);
	const cl_map_item_t *end = cl_qmap_end(&(db->pc_data));

	if (rc == end)
		return (NULL);
	return ((_db_node_t *) rc);
}

static inline perfmgr_db_err_t bad_node_port(_db_node_t * node, uint8_t port)
{
	if (!node)
		return (PERFMGR_EVENT_DB_GUIDNOTFOUND);
	if (port == 0 || port >= node->num_ports)
		return (PERFMGR_EVENT_DB_PORTNOTFOUND);
	return (PERFMGR_EVENT_DB_SUCCESS);
}

/** =========================================================================
 */
static _db_node_t *__malloc_node(uint64_t guid, uint8_t num_ports, char *name)
{
	int i = 0;
	time_t cur_time = 0;
	_db_node_t *rc = malloc(sizeof(*rc));
	if (!rc)
		return (NULL);

	rc->ports = calloc(num_ports, sizeof(_db_port_t));
	if (!rc->ports)
		goto free_rc;
	rc->num_ports = num_ports;
	rc->node_guid = guid;

	cur_time = time(NULL);
	for (i = 0; i < num_ports; i++) {
		rc->ports[i].last_reset = cur_time;
		rc->ports[i].err_previous.time = cur_time;
		rc->ports[i].dc_previous.time = cur_time;
	}
	snprintf(rc->node_name, NODE_NAME_SIZE, "%s", name);

	return (rc);

free_rc:
	free(rc);
	return (NULL);
}

/** =========================================================================
 */
static void __free_node(_db_node_t * node)
{
	if (!node)
		return;
	if (node->ports)
		free(node->ports);
	free(node);
}

/* insert nodes to the database */
static perfmgr_db_err_t __insert(perfmgr_db_t * db, _db_node_t * node)
{
	cl_map_item_t *rc = cl_qmap_insert(&(db->pc_data), node->node_guid,
					   (cl_map_item_t *) node);

	if ((void *)rc != (void *)node)
		return (PERFMGR_EVENT_DB_FAIL);
	return (PERFMGR_EVENT_DB_SUCCESS);
}

/**********************************************************************
 **********************************************************************/
perfmgr_db_err_t
perfmgr_db_create_entry(perfmgr_db_t * db, uint64_t guid,
			uint8_t num_ports, char *name)
{
	perfmgr_db_err_t rc = PERFMGR_EVENT_DB_SUCCESS;

	cl_plock_excl_acquire(&(db->lock));
	if (!_get(db, guid)) {
		_db_node_t *pc_node = __malloc_node(guid, num_ports, name);
		if (!pc_node) {
			rc = PERFMGR_EVENT_DB_NOMEM;
			goto Exit;
		}
		if (__insert(db, pc_node)) {
			__free_node(pc_node);
			rc = PERFMGR_EVENT_DB_FAIL;
			goto Exit;
		}
	}
Exit:
	cl_plock_release(&(db->lock));
	return (rc);
}

/**********************************************************************
 * Dump a reading vs the previous reading to stdout
 **********************************************************************/
static inline void
debug_dump_err_reading(perfmgr_db_t * db, uint64_t guid, uint8_t port_num,
		       _db_port_t * port, perfmgr_db_err_reading_t * cur)
{
	osm_log_t *log = db->perfmgr->log;

	if (!osm_log_is_active(log, OSM_LOG_DEBUG))
		return;		/* optimize this a bit */

	osm_log(log, OSM_LOG_DEBUG,
		"GUID 0x%" PRIx64 " Port %u:\n", guid, port_num);
	osm_log(log, OSM_LOG_DEBUG,
		"sym %" PRIu64 " <-- %" PRIu64 " (%" PRIu64 ")\n",
		cur->symbol_err_cnt, port->err_previous.symbol_err_cnt,
		port->err_total.symbol_err_cnt);
	osm_log(log, OSM_LOG_DEBUG,
		"ler %" PRIu64 " <-- %" PRIu64 " (%" PRIu64 ")\n",
		cur->link_err_recover, port->err_previous.link_err_recover,
		port->err_total.link_err_recover);
	osm_log(log, OSM_LOG_DEBUG,
		"ld %" PRIu64 " <-- %" PRIu64 " (%" PRIu64 ")\n",
		cur->link_downed, port->err_previous.link_downed,
		port->err_total.link_downed);
	osm_log(log, OSM_LOG_DEBUG,
		"re %" PRIu64 " <-- %" PRIu64 " (%" PRIu64 ")\n", cur->rcv_err,
		port->err_previous.rcv_err, port->err_total.rcv_err);
	osm_log(log, OSM_LOG_DEBUG,
		"rrp %" PRIu64 " <-- %" PRIu64 " (%" PRIu64 ")\n",
		cur->rcv_rem_phys_err, port->err_previous.rcv_rem_phys_err,
		port->err_total.rcv_rem_phys_err);
	osm_log(log, OSM_LOG_DEBUG,
		"rsr %" PRIu64 " <-- %" PRIu64 " (%" PRIu64 ")\n",
		cur->rcv_switch_relay_err,
		port->err_previous.rcv_switch_relay_err,
		port->err_total.rcv_switch_relay_err);
	osm_log(log, OSM_LOG_DEBUG,
		"xd %" PRIu64 " <-- %" PRIu64 " (%" PRIu64 ")\n",
		cur->xmit_discards, port->err_previous.xmit_discards,
		port->err_total.xmit_discards);
	osm_log(log, OSM_LOG_DEBUG,
		"xce %" PRIu64 " <-- %" PRIu64 " (%" PRIu64 ")\n",
		cur->xmit_constraint_err,
		port->err_previous.xmit_constraint_err,
		port->err_total.xmit_constraint_err);
	osm_log(log, OSM_LOG_DEBUG,
		"rce %" PRIu64 " <-- %" PRIu64 " (%" PRIu64 ")\n",
		cur->rcv_constraint_err, port->err_previous.rcv_constraint_err,
		port->err_total.rcv_constraint_err);
	osm_log(log, OSM_LOG_DEBUG,
		"li %" PRIu64 " <-- %" PRIu64 " (%" PRIu64 ")\n",
		cur->link_integrity, port->err_previous.link_integrity,
		port->err_total.link_integrity);
	osm_log(log, OSM_LOG_DEBUG,
		"bo %" PRIu64 " <-- %" PRIu64 " (%" PRIu64 ")\n",
		cur->buffer_overrun, port->err_previous.buffer_overrun,
		port->err_total.buffer_overrun);
	osm_log(log, OSM_LOG_DEBUG,
		"vld %" PRIu64 " <-- %" PRIu64 " (%" PRIu64 ")\n",
		cur->vl15_dropped, port->err_previous.vl15_dropped,
		port->err_total.vl15_dropped);
}

/**********************************************************************
 * perfmgr_db_err_reading_t functions
 **********************************************************************/
perfmgr_db_err_t
perfmgr_db_add_err_reading(perfmgr_db_t * db, uint64_t guid,
			   uint8_t port, perfmgr_db_err_reading_t * reading)
{
	_db_port_t *p_port = NULL;
	_db_node_t *node = NULL;
	perfmgr_db_err_reading_t *previous = NULL;
	perfmgr_db_err_t rc = PERFMGR_EVENT_DB_SUCCESS;
	osm_epi_pe_event_t epi_pe_data;

	cl_plock_excl_acquire(&(db->lock));
	node = _get(db, guid);
	if ((rc = bad_node_port(node, port)) != PERFMGR_EVENT_DB_SUCCESS)
		goto Exit;

	p_port = &(node->ports[port]);
	previous = &(node->ports[port].err_previous);

	debug_dump_err_reading(db, guid, port, p_port, reading);

	epi_pe_data.time_diff_s = (reading->time - previous->time);
	osm_epi_create_port_id(&(epi_pe_data.port_id), guid, port,
			       node->node_name);

	/* calculate changes from previous reading */
	epi_pe_data.symbol_err_cnt =
	    (reading->symbol_err_cnt - previous->symbol_err_cnt);
	p_port->err_total.symbol_err_cnt += epi_pe_data.symbol_err_cnt;
	epi_pe_data.link_err_recover =
	    (reading->link_err_recover - previous->link_err_recover);
	p_port->err_total.link_err_recover += epi_pe_data.link_err_recover;
	epi_pe_data.link_downed =
	    (reading->link_downed - previous->link_downed);
	p_port->err_total.link_downed += epi_pe_data.link_downed;
	epi_pe_data.rcv_err = (reading->rcv_err - previous->rcv_err);
	p_port->err_total.rcv_err += epi_pe_data.rcv_err;
	epi_pe_data.rcv_rem_phys_err =
	    (reading->rcv_rem_phys_err - previous->rcv_rem_phys_err);
	p_port->err_total.rcv_rem_phys_err += epi_pe_data.rcv_rem_phys_err;
	epi_pe_data.rcv_switch_relay_err =
	    (reading->rcv_switch_relay_err - previous->rcv_switch_relay_err);
	p_port->err_total.rcv_switch_relay_err +=
	    epi_pe_data.rcv_switch_relay_err;
	epi_pe_data.xmit_discards =
	    (reading->xmit_discards - previous->xmit_discards);
	p_port->err_total.xmit_discards += epi_pe_data.xmit_discards;
	epi_pe_data.xmit_constraint_err =
	    (reading->xmit_constraint_err - previous->xmit_constraint_err);
	p_port->err_total.xmit_constraint_err +=
	    epi_pe_data.xmit_constraint_err;
	epi_pe_data.rcv_constraint_err =
	    (reading->rcv_constraint_err - previous->rcv_constraint_err);
	p_port->err_total.rcv_constraint_err += epi_pe_data.rcv_constraint_err;
	epi_pe_data.link_integrity =
	    (reading->link_integrity - previous->link_integrity);
	p_port->err_total.link_integrity += epi_pe_data.link_integrity;
	epi_pe_data.buffer_overrun =
	    (reading->buffer_overrun - previous->buffer_overrun);
	p_port->err_total.buffer_overrun += epi_pe_data.buffer_overrun;
	epi_pe_data.vl15_dropped =
	    (reading->vl15_dropped - previous->vl15_dropped);
	p_port->err_total.vl15_dropped += epi_pe_data.vl15_dropped;

	p_port->err_previous = *reading;

	osm_opensm_report_event(db->perfmgr->osm, OSM_EVENT_ID_PORT_ERRORS,
				&epi_pe_data);

Exit:
	cl_plock_release(&(db->lock));
	return (rc);
}

perfmgr_db_err_t perfmgr_db_get_prev_err(perfmgr_db_t * db, uint64_t guid,
					 uint8_t port,
					 perfmgr_db_err_reading_t * reading)
{
	_db_node_t *node = NULL;
	perfmgr_db_err_t rc = PERFMGR_EVENT_DB_SUCCESS;

	cl_plock_acquire(&(db->lock));

	node = _get(db, guid);
	if ((rc = bad_node_port(node, port)) != PERFMGR_EVENT_DB_SUCCESS)
		goto Exit;

	*reading = node->ports[port].err_previous;

Exit:
	cl_plock_release(&(db->lock));
	return (rc);
}

perfmgr_db_err_t
perfmgr_db_clear_prev_err(perfmgr_db_t * db, uint64_t guid, uint8_t port)
{
	_db_node_t *node = NULL;
	perfmgr_db_err_reading_t *previous = NULL;
	perfmgr_db_err_t rc = PERFMGR_EVENT_DB_SUCCESS;

	cl_plock_excl_acquire(&(db->lock));
	node = _get(db, guid);
	if ((rc = bad_node_port(node, port)) != PERFMGR_EVENT_DB_SUCCESS)
		goto Exit;

	previous = &(node->ports[port].err_previous);

	memset(previous, 0, sizeof(*previous));
	node->ports[port].err_previous.time = time(NULL);

Exit:
	cl_plock_release(&(db->lock));
	return (rc);
}

static inline void
debug_dump_dc_reading(perfmgr_db_t * db, uint64_t guid, uint8_t port_num,
		      _db_port_t * port, perfmgr_db_data_cnt_reading_t * cur)
{
	osm_log_t *log = db->perfmgr->log;
	if (!osm_log_is_active(log, OSM_LOG_DEBUG))
		return;		/* optimize this a big */

	osm_log(log, OSM_LOG_DEBUG,
		"xd %" PRIu64 " <-- %" PRIu64 " (%" PRIu64 ")\n",
		cur->xmit_data, port->dc_previous.xmit_data,
		port->dc_total.xmit_data);
	osm_log(log, OSM_LOG_DEBUG,
		"rd %" PRIu64 " <-- %" PRIu64 " (%" PRIu64 ")\n", cur->rcv_data,
		port->dc_previous.rcv_data, port->dc_total.rcv_data);
	osm_log(log, OSM_LOG_DEBUG,
		"xp %" PRIu64 " <-- %" PRIu64 " (%" PRIu64 ")\n",
		cur->xmit_pkts, port->dc_previous.xmit_pkts,
		port->dc_total.xmit_pkts);
	osm_log(log, OSM_LOG_DEBUG,
		"rp %" PRIu64 " <-- %" PRIu64 " (%" PRIu64 ")\n", cur->rcv_pkts,
		port->dc_previous.rcv_pkts, port->dc_total.rcv_pkts);
}

/**********************************************************************
 * perfmgr_db_data_cnt_reading_t functions
 **********************************************************************/
perfmgr_db_err_t
perfmgr_db_add_dc_reading(perfmgr_db_t * db, uint64_t guid,
			  uint8_t port, perfmgr_db_data_cnt_reading_t * reading)
{
	_db_port_t *p_port = NULL;
	_db_node_t *node = NULL;
	perfmgr_db_data_cnt_reading_t *previous = NULL;
	perfmgr_db_err_t rc = PERFMGR_EVENT_DB_SUCCESS;
	osm_epi_dc_event_t epi_dc_data;

	cl_plock_excl_acquire(&(db->lock));
	node = _get(db, guid);
	if ((rc = bad_node_port(node, port)) != PERFMGR_EVENT_DB_SUCCESS)
		goto Exit;

	p_port = &(node->ports[port]);
	previous = &(node->ports[port].dc_previous);

	debug_dump_dc_reading(db, guid, port, p_port, reading);

	epi_dc_data.time_diff_s = (reading->time - previous->time);
	osm_epi_create_port_id(&(epi_dc_data.port_id), guid, port,
			       node->node_name);

	/* calculate changes from previous reading */
	epi_dc_data.xmit_data = (reading->xmit_data - previous->xmit_data);
	p_port->dc_total.xmit_data += epi_dc_data.xmit_data;
	epi_dc_data.rcv_data = (reading->rcv_data - previous->rcv_data);
	p_port->dc_total.rcv_data += epi_dc_data.rcv_data;
	epi_dc_data.xmit_pkts = (reading->xmit_pkts - previous->xmit_pkts);
	p_port->dc_total.xmit_pkts += epi_dc_data.xmit_pkts;
	epi_dc_data.rcv_pkts = (reading->rcv_pkts - previous->rcv_pkts);
	p_port->dc_total.rcv_pkts += epi_dc_data.rcv_pkts;
	epi_dc_data.unicast_xmit_pkts =
	    (reading->unicast_xmit_pkts - previous->unicast_xmit_pkts);
	p_port->dc_total.unicast_xmit_pkts += epi_dc_data.unicast_xmit_pkts;
	epi_dc_data.unicast_rcv_pkts =
	    (reading->unicast_rcv_pkts - previous->unicast_rcv_pkts);
	p_port->dc_total.unicast_rcv_pkts += epi_dc_data.unicast_rcv_pkts;
	epi_dc_data.multicast_xmit_pkts =
	    (reading->multicast_xmit_pkts - previous->multicast_xmit_pkts);
	p_port->dc_total.multicast_xmit_pkts += epi_dc_data.multicast_xmit_pkts;
	epi_dc_data.multicast_rcv_pkts =
	    (reading->multicast_rcv_pkts - previous->multicast_rcv_pkts);
	p_port->dc_total.multicast_rcv_pkts += epi_dc_data.multicast_rcv_pkts;

	p_port->dc_previous = *reading;

	osm_opensm_report_event(db->perfmgr->osm,
				OSM_EVENT_ID_PORT_DATA_COUNTERS, &epi_dc_data);

Exit:
	cl_plock_release(&(db->lock));
	return (rc);
}

perfmgr_db_err_t perfmgr_db_get_prev_dc(perfmgr_db_t * db, uint64_t guid,
					uint8_t port,
					perfmgr_db_data_cnt_reading_t * reading)
{
	_db_node_t *node = NULL;
	perfmgr_db_err_t rc = PERFMGR_EVENT_DB_SUCCESS;

	cl_plock_acquire(&(db->lock));

	node = _get(db, guid);
	if ((rc = bad_node_port(node, port)) != PERFMGR_EVENT_DB_SUCCESS)
		goto Exit;

	*reading = node->ports[port].dc_previous;

Exit:
	cl_plock_release(&(db->lock));
	return (rc);
}

perfmgr_db_err_t
perfmgr_db_clear_prev_dc(perfmgr_db_t * db, uint64_t guid, uint8_t port)
{
	_db_node_t *node = NULL;
	perfmgr_db_data_cnt_reading_t *previous = NULL;
	perfmgr_db_err_t rc = PERFMGR_EVENT_DB_SUCCESS;

	cl_plock_excl_acquire(&(db->lock));
	node = _get(db, guid);
	if ((rc = bad_node_port(node, port)) != PERFMGR_EVENT_DB_SUCCESS)
		goto Exit;

	previous = &(node->ports[port].dc_previous);

	memset(previous, 0, sizeof(*previous));
	node->ports[port].dc_previous.time = time(NULL);

Exit:
	cl_plock_release(&(db->lock));
	return (rc);
}

static void __clear_counters(cl_map_item_t * const p_map_item, void *context)
{
	_db_node_t *node = (_db_node_t *) p_map_item;
	int i = 0;
	time_t ts = time(NULL);

	for (i = 0; i < node->num_ports; i++) {
		node->ports[i].err_total.symbol_err_cnt = 0;
		node->ports[i].err_total.link_err_recover = 0;
		node->ports[i].err_total.link_downed = 0;
		node->ports[i].err_total.rcv_err = 0;
		node->ports[i].err_total.rcv_rem_phys_err = 0;
		node->ports[i].err_total.rcv_switch_relay_err = 0;
		node->ports[i].err_total.xmit_discards = 0;
		node->ports[i].err_total.xmit_constraint_err = 0;
		node->ports[i].err_total.rcv_constraint_err = 0;
		node->ports[i].err_total.link_integrity = 0;
		node->ports[i].err_total.buffer_overrun = 0;
		node->ports[i].err_total.vl15_dropped = 0;
		node->ports[i].err_total.time = ts;

		node->ports[i].dc_total.xmit_data = 0;
		node->ports[i].dc_total.rcv_data = 0;
		node->ports[i].dc_total.xmit_pkts = 0;
		node->ports[i].dc_total.rcv_pkts = 0;
		node->ports[i].dc_total.unicast_xmit_pkts = 0;
		node->ports[i].dc_total.unicast_rcv_pkts = 0;
		node->ports[i].dc_total.multicast_xmit_pkts = 0;
		node->ports[i].dc_total.multicast_rcv_pkts = 0;
		node->ports[i].dc_total.time = ts;

		node->ports[i].last_reset = ts;
	}
}

/**********************************************************************
 * Clear all the counters from the db
 **********************************************************************/
void perfmgr_db_clear_counters(perfmgr_db_t * db)
{
	cl_plock_excl_acquire(&(db->lock));
	cl_qmap_apply_func(&(db->pc_data), __clear_counters, (void *)db);
	cl_plock_release(&(db->lock));
#if 0
	if (db->db_impl->clear_counters)
		db->db_impl->clear_counters(db->db_data);
#endif
}

/**********************************************************************
 * Output a tab delimited output of the port counters
 **********************************************************************/
static void __dump_node_mr(_db_node_t * node, FILE * fp)
{
	int i = 0;

	fprintf(fp, "\nName\tGUID\tPort\tLast Reset\t"
		"%s\t%s\t"
		"%s\t%s\t%s\t%s\t%s\t%s\t%s\t"
		"%s\t%s\t%s\t%s\t%s\t%s\t%s\t"
		"%s\t%s\t%s\t%s\n",
		"symbol_err_cnt",
		"link_err_recover",
		"link_downed",
		"rcv_err",
		"rcv_rem_phys_err",
		"rcv_switch_relay_err",
		"xmit_discards",
		"xmit_constraint_err",
		"rcv_constraint_err",
		"link_int_err",
		"buf_overrun_err",
		"vl15_dropped",
		"xmit_data",
		"rcv_data",
		"xmit_pkts",
		"rcv_pkts",
		"unicast_xmit_pkts",
		"unicast_rcv_pkts",
		"multicast_xmit_pkts", "multicast_rcv_pkts");
	for (i = 1; i < node->num_ports; i++) {
		char *since = ctime(&(node->ports[i].last_reset));
		since[strlen(since) - 1] = '\0';	/* remove \n */

		fprintf(fp,
			"%s\t0x%" PRIx64 "\t%d\t%s\t%" PRIu64 "\t%" PRIu64 "\t"
			"%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t"
			"%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t" "%" PRIu64
			"\t%" PRIu64 "\t%" PRIu64 "\t" "%" PRIu64 "\t%" PRIu64
			"\t%" PRIu64 "\t%" PRIu64 "\t" "%" PRIu64 "\t%" PRIu64
			"\t%" PRIu64 "\t%" PRIu64 "\n", node->node_name,
			node->node_guid, i, since,
			node->ports[i].err_total.symbol_err_cnt,
			node->ports[i].err_total.link_err_recover,
			node->ports[i].err_total.link_downed,
			node->ports[i].err_total.rcv_err,
			node->ports[i].err_total.rcv_rem_phys_err,
			node->ports[i].err_total.rcv_switch_relay_err,
			node->ports[i].err_total.xmit_discards,
			node->ports[i].err_total.xmit_constraint_err,
			node->ports[i].err_total.rcv_constraint_err,
			node->ports[i].err_total.link_integrity,
			node->ports[i].err_total.buffer_overrun,
			node->ports[i].err_total.vl15_dropped,
			node->ports[i].dc_total.xmit_data,
			node->ports[i].dc_total.rcv_data,
			node->ports[i].dc_total.xmit_pkts,
			node->ports[i].dc_total.rcv_pkts,
			node->ports[i].dc_total.unicast_xmit_pkts,
			node->ports[i].dc_total.unicast_rcv_pkts,
			node->ports[i].dc_total.multicast_xmit_pkts,
			node->ports[i].dc_total.multicast_rcv_pkts);
	}
}

/**********************************************************************
 * Output a human readable output of the port counters
 **********************************************************************/
static void __dump_node_hr(_db_node_t * node, FILE * fp)
{
	int i = 0;

	fprintf(fp, "\n");
	for (i = 1; i < node->num_ports; i++) {
		char *since = ctime(&(node->ports[i].last_reset));
		since[strlen(since) - 1] = '\0';	/* remove \n */

		fprintf(fp, "\"%s\" 0x%" PRIx64 " port %d (Since %s)\n"
			"     symbol_err_cnt       : %" PRIu64 "\n"
			"     link_err_recover     : %" PRIu64 "\n"
			"     link_downed          : %" PRIu64 "\n"
			"     rcv_err              : %" PRIu64 "\n"
			"     rcv_rem_phys_err     : %" PRIu64 "\n"
			"     rcv_switch_relay_err : %" PRIu64 "\n"
			"     xmit_discards        : %" PRIu64 "\n"
			"     xmit_constraint_err  : %" PRIu64 "\n"
			"     rcv_constraint_err   : %" PRIu64 "\n"
			"     link_integrity_err   : %" PRIu64 "\n"
			"     buf_overrun_err      : %" PRIu64 "\n"
			"     vl15_dropped         : %" PRIu64 "\n"
			"     xmit_data            : %" PRIu64 "\n"
			"     rcv_data             : %" PRIu64 "\n"
			"     xmit_pkts            : %" PRIu64 "\n"
			"     rcv_pkts             : %" PRIu64 "\n"
			"     unicast_xmit_pkts    : %" PRIu64 "\n"
			"     unicast_rcv_pkts     : %" PRIu64 "\n"
			"     multicast_xmit_pkts  : %" PRIu64 "\n"
			"     multicast_rcv_pkts   : %" PRIu64 "\n",
			node->node_name,
			node->node_guid,
			i,
			since,
			node->ports[i].err_total.symbol_err_cnt,
			node->ports[i].err_total.link_err_recover,
			node->ports[i].err_total.link_downed,
			node->ports[i].err_total.rcv_err,
			node->ports[i].err_total.rcv_rem_phys_err,
			node->ports[i].err_total.rcv_switch_relay_err,
			node->ports[i].err_total.xmit_discards,
			node->ports[i].err_total.xmit_constraint_err,
			node->ports[i].err_total.rcv_constraint_err,
			node->ports[i].err_total.link_integrity,
			node->ports[i].err_total.buffer_overrun,
			node->ports[i].err_total.vl15_dropped,
			node->ports[i].dc_total.xmit_data,
			node->ports[i].dc_total.rcv_data,
			node->ports[i].dc_total.xmit_pkts,
			node->ports[i].dc_total.rcv_pkts,
			node->ports[i].dc_total.unicast_xmit_pkts,
			node->ports[i].dc_total.unicast_rcv_pkts,
			node->ports[i].dc_total.multicast_xmit_pkts,
			node->ports[i].dc_total.multicast_rcv_pkts);
	}
}

/* Define a context for the __db_dump callback */
typedef struct {
	FILE *fp;
	perfmgr_db_dump_t dump_type;
} dump_context_t;

/**********************************************************************
 **********************************************************************/
static void __db_dump(cl_map_item_t * const p_map_item, void *context)
{
	_db_node_t *node = (_db_node_t *) p_map_item;
	dump_context_t *c = (dump_context_t *) context;
	FILE *fp = c->fp;

	switch (c->dump_type) {
	case PERFMGR_EVENT_DB_DUMP_MR:
		__dump_node_mr(node, fp);
		break;
	case PERFMGR_EVENT_DB_DUMP_HR:
	default:
		__dump_node_hr(node, fp);
		break;
	}
}

/**********************************************************************
 * print node data to fp
 **********************************************************************/
void
perfmgr_db_print_by_name(perfmgr_db_t * db, char *nodename, FILE *fp)
{
	cl_map_item_t *item = NULL;
	_db_node_t *node = NULL;

	cl_plock_acquire(&(db->lock));

	/* find the node */
	item = cl_qmap_head(&(db->pc_data));
	while (item != cl_qmap_end(&(db->pc_data))) {
		node = (_db_node_t *)item;
		if (strcmp(node->node_name, nodename) == 0) {
			__dump_node_hr(node, fp);
			goto done;
		}
		item = cl_qmap_next(item);
	}

	fprintf(fp, "Node %s not found...\n", nodename);
done:
	cl_plock_release(&(db->lock));
}

/**********************************************************************
 * print node data to fp
 **********************************************************************/
void
perfmgr_db_print_by_guid(perfmgr_db_t * db, uint64_t nodeguid, FILE *fp)
{
	cl_map_item_t *node = NULL;

	cl_plock_acquire(&(db->lock));

	node = cl_qmap_get(&(db->pc_data), nodeguid);
	if (node != cl_qmap_end(&(db->pc_data)))
		__dump_node_hr((_db_node_t *)node, fp);
	else
		fprintf(fp, "Node %"PRIx64" not found...\n", nodeguid);

	cl_plock_release(&(db->lock));
}

/**********************************************************************
 * dump the data to the file "file"
 **********************************************************************/
perfmgr_db_err_t
perfmgr_db_dump(perfmgr_db_t * db, char *file, perfmgr_db_dump_t dump_type)
{
	dump_context_t context;

	context.fp = fopen(file, "w+");
	if (!context.fp)
		return (PERFMGR_EVENT_DB_FAIL);
	context.dump_type = dump_type;

	cl_plock_acquire(&(db->lock));
	cl_qmap_apply_func(&(db->pc_data), __db_dump, (void *)&context);
	cl_plock_release(&(db->lock));
	fclose(context.fp);
	return (PERFMGR_EVENT_DB_SUCCESS);
}

/**********************************************************************
 * Fill in the various DB objects from their wire counter parts
 **********************************************************************/
void
perfmgr_db_fill_err_read(ib_port_counters_t * wire_read,
			 perfmgr_db_err_reading_t * reading)
{
	reading->symbol_err_cnt = cl_ntoh16(wire_read->symbol_err_cnt);
	reading->link_err_recover = cl_ntoh16(wire_read->link_err_recover);
	reading->link_downed = wire_read->link_downed;
	reading->rcv_err = wire_read->rcv_err;
	reading->rcv_rem_phys_err = cl_ntoh16(wire_read->rcv_rem_phys_err);
	reading->rcv_switch_relay_err =
	    cl_ntoh16(wire_read->rcv_switch_relay_err);
	reading->xmit_discards = cl_ntoh16(wire_read->xmit_discards);
	reading->xmit_constraint_err =
	    cl_ntoh16(wire_read->xmit_constraint_err);
	reading->rcv_constraint_err = wire_read->rcv_constraint_err;
	reading->link_integrity =
	    PC_LINK_INT(wire_read->link_int_buffer_overrun);
	reading->buffer_overrun =
	    PC_BUF_OVERRUN(wire_read->link_int_buffer_overrun);
	reading->vl15_dropped = cl_ntoh16(wire_read->vl15_dropped);
	reading->time = time(NULL);
}

void
perfmgr_db_fill_data_cnt_read_pc(ib_port_counters_t * wire_read,
				 perfmgr_db_data_cnt_reading_t * reading)
{
	reading->xmit_data = cl_ntoh32(wire_read->xmit_data);
	reading->rcv_data = cl_ntoh32(wire_read->rcv_data);
	reading->xmit_pkts = cl_ntoh32(wire_read->xmit_pkts);
	reading->rcv_pkts = cl_ntoh32(wire_read->rcv_pkts);
	reading->unicast_xmit_pkts = 0;
	reading->unicast_rcv_pkts = 0;
	reading->multicast_xmit_pkts = 0;
	reading->multicast_rcv_pkts = 0;
	reading->time = time(NULL);
}

void
perfmgr_db_fill_data_cnt_read_epc(ib_port_counters_ext_t * wire_read,
				  perfmgr_db_data_cnt_reading_t * reading)
{
	reading->xmit_data = cl_ntoh64(wire_read->xmit_data);
	reading->rcv_data = cl_ntoh64(wire_read->rcv_data);
	reading->xmit_pkts = cl_ntoh64(wire_read->xmit_pkts);
	reading->rcv_pkts = cl_ntoh64(wire_read->rcv_pkts);
	reading->unicast_xmit_pkts = cl_ntoh64(wire_read->unicast_xmit_pkts);
	reading->unicast_rcv_pkts = cl_ntoh64(wire_read->unicast_rcv_pkts);
	reading->multicast_xmit_pkts =
	    cl_ntoh64(wire_read->multicast_xmit_pkts);
	reading->multicast_rcv_pkts = cl_ntoh64(wire_read->multicast_rcv_pkts);
	reading->time = time(NULL);
}
#endif				/* ENABLE_OSM_PERF_MGR */
