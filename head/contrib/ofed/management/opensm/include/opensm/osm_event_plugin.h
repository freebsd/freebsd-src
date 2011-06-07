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

#ifndef _OSM_EVENT_PLUGIN_H_
#define _OSM_EVENT_PLUGIN_H_

#include <time.h>
#include <iba/ib_types.h>
#include <complib/cl_qlist.h>
#include <opensm/osm_config.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/****h* OpenSM Event plugin interface
* DESCRIPTION
*       Database interface to record subnet events
*
*       Implementations of this object _MUST_ be thread safe.
*
* AUTHOR
*	Ira Weiny, LLNL
*
*********/

#define OSM_EPI_NODE_NAME_LEN (128)

struct osm_opensm;
/** =========================================================================
 * Event types
 */
typedef enum {
	OSM_EVENT_ID_PORT_ERRORS = 0,
	OSM_EVENT_ID_PORT_DATA_COUNTERS,
	OSM_EVENT_ID_PORT_SELECT,
	OSM_EVENT_ID_TRAP,
	OSM_EVENT_ID_SUBNET_UP,
	OSM_EVENT_ID_MAX
} osm_epi_event_id_t;

typedef struct osm_epi_port_id {
	uint64_t node_guid;
	uint8_t port_num;
	char node_name[OSM_EPI_NODE_NAME_LEN];
} osm_epi_port_id_t;

/** =========================================================================
 * Port error event
 * OSM_EVENT_ID_PORT_COUNTER
 * This is a difference from the last reading.  NOT an absolute reading.
 */
typedef struct osm_epi_pe_event {
	osm_epi_port_id_t port_id;
	uint64_t symbol_err_cnt;
	uint64_t link_err_recover;
	uint64_t link_downed;
	uint64_t rcv_err;
	uint64_t rcv_rem_phys_err;
	uint64_t rcv_switch_relay_err;
	uint64_t xmit_discards;
	uint64_t xmit_constraint_err;
	uint64_t rcv_constraint_err;
	uint64_t link_integrity;
	uint64_t buffer_overrun;
	uint64_t vl15_dropped;
	time_t time_diff_s;
} osm_epi_pe_event_t;

/** =========================================================================
 * Port data counter event
 * This is a difference from the last reading.  NOT an absolute reading.
 */
typedef struct osm_epi_dc_event {
	osm_epi_port_id_t port_id;
	uint64_t xmit_data;
	uint64_t rcv_data;
	uint64_t xmit_pkts;
	uint64_t rcv_pkts;
	uint64_t unicast_xmit_pkts;
	uint64_t unicast_rcv_pkts;
	uint64_t multicast_xmit_pkts;
	uint64_t multicast_rcv_pkts;
	time_t time_diff_s;
} osm_epi_dc_event_t;

/** =========================================================================
 * Port select event
 * This is a difference from the last reading.  NOT an absolute reading.
 */
typedef struct osm_api_ps_event {
	osm_epi_port_id_t port_id;
	uint64_t xmit_wait;
	time_t time_diff_s;
} osm_epi_ps_event_t;

/** =========================================================================
 * Trap events
 */
typedef struct osm_epi_trap_event {
	osm_epi_port_id_t port_id;
	uint8_t type;
	uint32_t prod_type;
	uint16_t trap_num;
	uint16_t issuer_lid;
	time_t time;
} osm_epi_trap_event_t;

/** =========================================================================
 * Plugin creators should allocate an object of this type
 *    (named OSM_EVENT_PLUGIN_IMPL_NAME)
 * The version should be set to OSM_EVENT_PLUGIN_INTERFACE_VER
 */
#define OSM_EVENT_PLUGIN_IMPL_NAME "osm_event_plugin"
#define OSM_ORIG_EVENT_PLUGIN_INTERFACE_VER 1
#define OSM_EVENT_PLUGIN_INTERFACE_VER 2
typedef struct osm_event_plugin {
	const char *osm_version;
	void *(*create) (struct osm_opensm *osm);
	void (*delete) (void *plugin_data);
	void (*report) (void *plugin_data,
			osm_epi_event_id_t event_id, void *event_data);
} osm_event_plugin_t;

/** =========================================================================
 * The plugin structure should be considered opaque
 */
typedef struct osm_epi_plugin {
	cl_list_item_t list;
	void *handle;
	osm_event_plugin_t *impl;
	void *plugin_data;
	char *plugin_name;
} osm_epi_plugin_t;

/**
 * functions
 */
osm_epi_plugin_t *osm_epi_construct(struct osm_opensm *osm, char *plugin_name);
void osm_epi_destroy(osm_epi_plugin_t * plugin);

/** =========================================================================
 * Helper functions
 */
static inline void
osm_epi_create_port_id(osm_epi_port_id_t * port_id, uint64_t node_guid,
		       uint8_t port_num, char *node_name)
{
	port_id->node_guid = node_guid;
	port_id->port_num = port_num;
	strncpy(port_id->node_name, node_name, OSM_EPI_NODE_NAME_LEN);
	port_id->node_name[OSM_EPI_NODE_NAME_LEN - 1] = '\0';
}

END_C_DECLS
#endif				/* _OSM_EVENT_PLUGIN_H_ */
