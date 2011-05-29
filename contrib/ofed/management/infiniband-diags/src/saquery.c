/*
 * Copyright (c) 2006,2007 The Regents of the University of California.
 * Copyright (c) 2004-2008 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 *
 * Produced at Lawrence Livermore National Laboratory.
 * Written by Ira Weiny <weiny2@llnl.gov>.
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

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>

#define _GNU_SOURCE
#include <getopt.h>

#include <infiniband/mad.h>
#include <infiniband/opensm/osm_log.h>
#include <infiniband/vendor/osm_vendor_api.h>
#include <infiniband/vendor/osm_vendor_sa_api.h>
#include <infiniband/opensm/osm_mad_pool.h>
#include <infiniband/complib/cl_debug.h>
#include <infiniband/complib/cl_nodenamemap.h>

#include <netinet/in.h>

#include "ibdiag_common.h"

struct query_cmd {
	const char *name, *alias;
	ib_net16_t query_type;
	const char *usage;
	int (*handler) (const struct query_cmd * q, osm_bind_handle_t h,
			int argc, char *argv[]);
};

char *argv0 = "saquery";

static char *node_name_map_file = NULL;
static nn_map_t *node_name_map = NULL;
static ib_net64_t smkey = OSM_DEFAULT_SA_KEY;

/**
 * Declare some globals because I don't want this to be too complex.
 */
#define MAX_PORTS (8)
#define DEFAULT_SA_TIMEOUT_MS (1000)
osmv_query_res_t result;
osm_log_t log_osm;
osm_mad_pool_t mad_pool;
osm_vendor_t *vendor = NULL;
int osm_debug = 0;
uint32_t sa_timeout_ms = DEFAULT_SA_TIMEOUT_MS;
char *sa_hca_name = NULL;
uint32_t sa_port_num = 0;

enum {
	ALL,
	LID_ONLY,
	UNIQUE_LID_ONLY,
	GUID_ONLY,
	ALL_DESC,
	NAME_OF_LID,
	NAME_OF_GUID,
} node_print_desc = ALL;

char *requested_name = NULL;
ib_net16_t requested_lid = 0;
int requested_lid_flag = 0;
ib_net64_t requested_guid = 0;
int requested_guid_flag = 0;

static void format_buf(char *in, char *out, unsigned size)
{
	unsigned i;

	for (i = 0; i < size - 3 && *in; i++) {
		*out++ = *in;
		if (*in++ == '\n' && *in) {
			*out++ = '\t';
			*out++ = '\t';
		}
	}
	*out = '\0';
}

/**
 * Call back for the various record requests.
 */
static void query_res_cb(osmv_query_res_t * res)
{
	result = *res;
}

static void print_node_desc(ib_node_record_t * node_record)
{
	ib_node_info_t *p_ni = &(node_record->node_info);
	ib_node_desc_t *p_nd = &(node_record->node_desc);

	if (p_ni->node_type == IB_NODE_TYPE_CA) {
		printf("%6d  \"%s\"\n",
		       cl_ntoh16(node_record->lid),
		       clean_nodedesc((char *)p_nd->description));
	}
}

static void print_node_record(ib_node_record_t * node_record)
{
	ib_node_info_t *p_ni = NULL;
	ib_node_desc_t *p_nd = NULL;
	char *name;

	p_ni = &(node_record->node_info);
	p_nd = &(node_record->node_desc);

	switch (node_print_desc) {
	case LID_ONLY:
	case UNIQUE_LID_ONLY:
		printf("%d\n", cl_ntoh16(node_record->lid));
		return;
	case GUID_ONLY:
		printf("0x%016" PRIx64 "\n", cl_ntoh64(p_ni->port_guid));
		return;
	case NAME_OF_LID:
	case NAME_OF_GUID:
		name = remap_node_name(node_name_map,
				       cl_ntoh64(p_ni->node_guid),
				       (char *)p_nd->description);
		printf("%s\n", name);
		free(name);
		return;
	case ALL:
	default:
		break;
	}

	printf("NodeRecord dump:\n"
	       "\t\tlid.....................0x%X\n"
	       "\t\treserved................0x%X\n"
	       "\t\tbase_version............0x%X\n"
	       "\t\tclass_version...........0x%X\n"
	       "\t\tnode_type...............%s\n"
	       "\t\tnum_ports...............0x%X\n"
	       "\t\tsys_guid................0x%016" PRIx64 "\n"
	       "\t\tnode_guid...............0x%016" PRIx64 "\n"
	       "\t\tport_guid...............0x%016" PRIx64 "\n"
	       "\t\tpartition_cap...........0x%X\n"
	       "\t\tdevice_id...............0x%X\n"
	       "\t\trevision................0x%X\n"
	       "\t\tport_num................0x%X\n"
	       "\t\tvendor_id...............0x%X\n"
	       "\t\tNodeDescription.........%s\n"
	       "",
	       cl_ntoh16(node_record->lid),
	       cl_ntoh16(node_record->resv),
	       p_ni->base_version,
	       p_ni->class_version,
	       ib_get_node_type_str(p_ni->node_type),
	       p_ni->num_ports,
	       cl_ntoh64(p_ni->sys_guid),
	       cl_ntoh64(p_ni->node_guid),
	       cl_ntoh64(p_ni->port_guid),
	       cl_ntoh16(p_ni->partition_cap),
	       cl_ntoh16(p_ni->device_id),
	       cl_ntoh32(p_ni->revision),
	       ib_node_info_get_local_port_num(p_ni),
	       cl_ntoh32(ib_node_info_get_vendor_id(p_ni)),
	       clean_nodedesc((char *)node_record->node_desc.description)
	    );
}

static void dump_path_record(void *data)
{
	char gid_str[INET6_ADDRSTRLEN];
	char gid_str2[INET6_ADDRSTRLEN];
	ib_path_rec_t *p_pr = data;
	printf("PathRecord dump:\n"
	       "\t\tservice_id..............0x%016" PRIx64 "\n"
	       "\t\tdgid....................%s\n"
	       "\t\tsgid....................%s\n"
	       "\t\tdlid....................0x%X\n"
	       "\t\tslid....................0x%X\n"
	       "\t\thop_flow_raw............0x%X\n"
	       "\t\ttclass..................0x%X\n"
	       "\t\tnum_path_revers.........0x%X\n"
	       "\t\tpkey....................0x%X\n"
	       "\t\tqos_class...............0x%X\n"
	       "\t\tsl......................0x%X\n"
	       "\t\tmtu.....................0x%X\n"
	       "\t\trate....................0x%X\n"
	       "\t\tpkt_life................0x%X\n"
	       "\t\tpreference..............0x%X\n"
	       "\t\tresv2...................0x%X\n"
	       "\t\tresv3...................0x%X\n"
	       "",
	       cl_ntoh64(p_pr->service_id),
	       inet_ntop(AF_INET6, p_pr->dgid.raw, gid_str, sizeof gid_str),
	       inet_ntop(AF_INET6, p_pr->sgid.raw, gid_str2, sizeof gid_str2),
	       cl_ntoh16(p_pr->dlid),
	       cl_ntoh16(p_pr->slid),
	       cl_ntoh32(p_pr->hop_flow_raw),
	       p_pr->tclass,
	       p_pr->num_path,
	       cl_ntoh16(p_pr->pkey),
	       ib_path_rec_qos_class(p_pr),
	       ib_path_rec_sl(p_pr),
	       p_pr->mtu,
	       p_pr->rate,
	       p_pr->pkt_life,
	       p_pr->preference,
	       *(uint32_t *) & p_pr->resv2, *((uint16_t *) & p_pr->resv2 + 2)
	    );
}

static void dump_class_port_info(void *data)
{
	char gid_str[INET6_ADDRSTRLEN];
	char gid_str2[INET6_ADDRSTRLEN];
	ib_class_port_info_t *class_port_info = data;

	printf("SA ClassPortInfo:\n"
	       "\t\tBase version.............%d\n"
	       "\t\tClass version............%d\n"
	       "\t\tCapability mask..........0x%04X\n"
	       "\t\tCapability mask 2........0x%08X\n"
	       "\t\tResponse time value......0x%02X\n"
	       "\t\tRedirect GID.............%s\n"
	       "\t\tRedirect TC/SL/FL........0x%08X\n"
	       "\t\tRedirect LID.............0x%04X\n"
	       "\t\tRedirect PKey............0x%04X\n"
	       "\t\tRedirect QP..............0x%08X\n"
	       "\t\tRedirect QKey............0x%08X\n"
	       "\t\tTrap GID.................%s\n"
	       "\t\tTrap TC/SL/FL............0x%08X\n"
	       "\t\tTrap LID.................0x%04X\n"
	       "\t\tTrap PKey................0x%04X\n"
	       "\t\tTrap HL/QP...............0x%08X\n"
	       "\t\tTrap QKey................0x%08X\n"
	       "",
	       class_port_info->base_ver,
	       class_port_info->class_ver,
	       cl_ntoh16(class_port_info->cap_mask),
	       ib_class_cap_mask2(class_port_info),
	       ib_class_resp_time_val(class_port_info),
	       inet_ntop(AF_INET6, &(class_port_info->redir_gid), gid_str,
			 sizeof gid_str),
	       cl_ntoh32(class_port_info->redir_tc_sl_fl),
	       cl_ntoh16(class_port_info->redir_lid),
	       cl_ntoh16(class_port_info->redir_pkey),
	       cl_ntoh32(class_port_info->redir_qp),
	       cl_ntoh32(class_port_info->redir_qkey),
	       inet_ntop(AF_INET6, &(class_port_info->trap_gid), gid_str2,
			 sizeof gid_str2),
	       cl_ntoh32(class_port_info->trap_tc_sl_fl),
	       cl_ntoh16(class_port_info->trap_lid),
	       cl_ntoh16(class_port_info->trap_pkey),
	       cl_ntoh32(class_port_info->trap_hop_qp),
	       cl_ntoh32(class_port_info->trap_qkey)
	    );
}

static void dump_portinfo_record(void *data)
{
	ib_portinfo_record_t *p_pir = data;
	const ib_port_info_t *const p_pi = &p_pir->port_info;

	printf("PortInfoRecord dump:\n"
	       "\t\tEndPortLid..............0x%X\n"
	       "\t\tPortNum.................0x%X\n"
	       "\t\tbase_lid................0x%X\n"
	       "\t\tmaster_sm_base_lid......0x%X\n"
	       "\t\tcapability_mask.........0x%X\n"
	       "",
	       cl_ntoh16(p_pir->lid),
	       p_pir->port_num,
	       cl_ntoh16(p_pi->base_lid),
	       cl_ntoh16(p_pi->master_sm_base_lid),
	       cl_ntoh32(p_pi->capability_mask)
	    );
}

static void dump_one_portinfo_record(void *data)
{
	char buf[2048], buf2[4096];
	ib_portinfo_record_t *pir = data;
	ib_port_info_t *pi = &pir->port_info;

	mad_dump_portinfo(buf, sizeof(buf), pi, sizeof(*pi));

	format_buf(buf, buf2, sizeof(buf2));

	printf("PortInfoRecord dump:\n"
		"\tRID:\n"
		"\t\tEndPortLid..............%u\n"
		"\t\tPortNum.................0x%x\n"
		"\t\tReserved................0x%x\n"
		"\tPortInfo dump:\n\t\t%s",
		cl_ntoh16(pir->lid), pir->port_num, pir->resv, buf2);
}

static void dump_multicast_group_record(void *data)
{
	char gid_str[INET6_ADDRSTRLEN];
	ib_member_rec_t *p_mcmr = data;
	uint8_t sl;
	ib_member_get_sl_flow_hop(p_mcmr->sl_flow_hop, &sl, NULL, NULL);
	printf("MCMemberRecord group dump:\n"
	       "\t\tMGID....................%s\n"
	       "\t\tMlid....................0x%X\n"
	       "\t\tMtu.....................0x%X\n"
	       "\t\tpkey....................0x%X\n"
	       "\t\tRate....................0x%X\n"
	       "\t\tSL......................0x%X\n"
	       "",
	       inet_ntop(AF_INET6, p_mcmr->mgid.raw, gid_str, sizeof gid_str),
	       cl_ntoh16(p_mcmr->mlid),
	       p_mcmr->mtu, cl_ntoh16(p_mcmr->pkey), p_mcmr->rate, sl);
}

static void dump_multicast_member_record(void *data)
{
	char gid_str[INET6_ADDRSTRLEN];
	char gid_str2[INET6_ADDRSTRLEN];
	ib_member_rec_t *p_mcmr = data;
	uint16_t mlid = cl_ntoh16(p_mcmr->mlid);
	int i = 0;
	char *node_name = "<unknown>";

	/* go through the node records searching for a port guid which matches
	 * this port gid interface id.
	 * This gives us a node name to print, if available.
	 */
	for (i = 0; i < result.result_cnt; i++) {
		ib_node_record_t *nr =
		    osmv_get_query_node_rec(result.p_result_madw, i);
		if (nr->node_info.port_guid ==
		    p_mcmr->port_gid.unicast.interface_id) {
			node_name =
			    clean_nodedesc((char *)nr->node_desc.description);
			break;
		}
	}

	if (requested_name) {
		if (strtol(requested_name, NULL, 0) == mlid) {
			printf("\t\tPortGid.................%s (%s)\n",
			       inet_ntop(AF_INET6, p_mcmr->port_gid.raw,
					 gid_str, sizeof gid_str), node_name);
		}
	} else {
		printf("MCMemberRecord member dump:\n"
		       "\t\tMGID....................%s\n"
		       "\t\tMlid....................0x%X\n"
		       "\t\tPortGid.................%s\n"
		       "\t\tScopeState..............0x%X\n"
		       "\t\tProxyJoin...............0x%X\n"
		       "\t\tNodeDescription.........%s\n"
		       "",
		       inet_ntop(AF_INET6, p_mcmr->mgid.raw, gid_str,
				 sizeof gid_str),
		       cl_ntoh16(p_mcmr->mlid),
		       inet_ntop(AF_INET6, p_mcmr->port_gid.raw,
				 gid_str2, sizeof gid_str2),
		       p_mcmr->scope_state, p_mcmr->proxy_join, node_name);
	}
}

static void dump_service_record(void *data)
{
	char gid_str[INET6_ADDRSTRLEN];
	char buf_service_key[35];
	char buf_service_name[65];
	ib_service_record_t *p_sr = data;

	sprintf(buf_service_key,
		"0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		p_sr->service_key[0], p_sr->service_key[1],
		p_sr->service_key[2], p_sr->service_key[3],
		p_sr->service_key[4], p_sr->service_key[5],
		p_sr->service_key[6], p_sr->service_key[7],
		p_sr->service_key[8], p_sr->service_key[9],
		p_sr->service_key[10], p_sr->service_key[11],
		p_sr->service_key[12], p_sr->service_key[13],
		p_sr->service_key[14], p_sr->service_key[15]);
	strncpy(buf_service_name, (char *)p_sr->service_name, 64);
	buf_service_name[64] = '\0';

	printf("ServiceRecord dump:\n"
	       "\t\tServiceID...............0x%016" PRIx64 "\n"
	       "\t\tServiceGID..............%s\n"
	       "\t\tServiceP_Key............0x%X\n"
	       "\t\tServiceLease............0x%X\n"
	       "\t\tServiceKey..............%s\n"
	       "\t\tServiceName.............%s\n"
	       "\t\tServiceData8.1..........0x%X\n"
	       "\t\tServiceData8.2..........0x%X\n"
	       "\t\tServiceData8.3..........0x%X\n"
	       "\t\tServiceData8.4..........0x%X\n"
	       "\t\tServiceData8.5..........0x%X\n"
	       "\t\tServiceData8.6..........0x%X\n"
	       "\t\tServiceData8.7..........0x%X\n"
	       "\t\tServiceData8.8..........0x%X\n"
	       "\t\tServiceData8.9..........0x%X\n"
	       "\t\tServiceData8.10.........0x%X\n"
	       "\t\tServiceData8.11.........0x%X\n"
	       "\t\tServiceData8.12.........0x%X\n"
	       "\t\tServiceData8.13.........0x%X\n"
	       "\t\tServiceData8.14.........0x%X\n"
	       "\t\tServiceData8.15.........0x%X\n"
	       "\t\tServiceData8.16.........0x%X\n"
	       "\t\tServiceData16.1.........0x%X\n"
	       "\t\tServiceData16.2.........0x%X\n"
	       "\t\tServiceData16.3.........0x%X\n"
	       "\t\tServiceData16.4.........0x%X\n"
	       "\t\tServiceData16.5.........0x%X\n"
	       "\t\tServiceData16.6.........0x%X\n"
	       "\t\tServiceData16.7.........0x%X\n"
	       "\t\tServiceData16.8.........0x%X\n"
	       "\t\tServiceData32.1.........0x%X\n"
	       "\t\tServiceData32.2.........0x%X\n"
	       "\t\tServiceData32.3.........0x%X\n"
	       "\t\tServiceData32.4.........0x%X\n"
	       "\t\tServiceData64.1.........0x%016" PRIx64 "\n"
	       "\t\tServiceData64.2.........0x%016" PRIx64 "\n"
	       "",
	       cl_ntoh64(p_sr->service_id),
	       inet_ntop(AF_INET6, p_sr->service_gid.raw, gid_str,
			 sizeof gid_str),
	       cl_ntoh16(p_sr->service_pkey),
	       cl_ntoh32(p_sr->service_lease),
	       buf_service_key,
	       buf_service_name,
	       p_sr->service_data8[0], p_sr->service_data8[1],
	       p_sr->service_data8[2], p_sr->service_data8[3],
	       p_sr->service_data8[4], p_sr->service_data8[5],
	       p_sr->service_data8[6], p_sr->service_data8[7],
	       p_sr->service_data8[8], p_sr->service_data8[9],
	       p_sr->service_data8[10], p_sr->service_data8[11],
	       p_sr->service_data8[12], p_sr->service_data8[13],
	       p_sr->service_data8[14], p_sr->service_data8[15],
	       cl_ntoh16(p_sr->service_data16[0]),
	       cl_ntoh16(p_sr->service_data16[1]),
	       cl_ntoh16(p_sr->service_data16[2]),
	       cl_ntoh16(p_sr->service_data16[3]),
	       cl_ntoh16(p_sr->service_data16[4]),
	       cl_ntoh16(p_sr->service_data16[5]),
	       cl_ntoh16(p_sr->service_data16[6]),
	       cl_ntoh16(p_sr->service_data16[7]),
	       cl_ntoh32(p_sr->service_data32[0]),
	       cl_ntoh32(p_sr->service_data32[1]),
	       cl_ntoh32(p_sr->service_data32[2]),
	       cl_ntoh32(p_sr->service_data32[3]),
	       cl_ntoh64(p_sr->service_data64[0]),
	       cl_ntoh64(p_sr->service_data64[1])
	    );
}

static void dump_inform_info_record(void *data)
{
	char gid_str[INET6_ADDRSTRLEN];
	char gid_str2[INET6_ADDRSTRLEN];
	ib_inform_info_record_t *p_iir = data;
	uint32_t qpn;
	uint8_t resp_time_val;

	ib_inform_info_get_qpn_resp_time(p_iir->inform_info.g_or_v.generic.
					 qpn_resp_time_val, &qpn,
					 &resp_time_val);

	if (p_iir->inform_info.is_generic) {
		printf("InformInfoRecord dump:\n"
		       "\t\tRID\n"
		       "\t\tSubscriberGID...........%s\n"
		       "\t\tSubscriberEnum..........0x%X\n"
		       "\t\tInformInfo dump:\n"
		       "\t\tgid.....................%s\n"
		       "\t\tlid_range_begin.........0x%X\n"
		       "\t\tlid_range_end...........0x%X\n"
		       "\t\tis_generic..............0x%X\n"
		       "\t\tsubscribe...............0x%X\n"
		       "\t\ttrap_type...............0x%X\n"
		       "\t\ttrap_num................%u\n"
		       "\t\tqpn.....................0x%06X\n"
		       "\t\tresp_time_val...........0x%X\n"
		       "\t\tnode_type...............0x%06X\n"
		       "",
		       inet_ntop(AF_INET6, p_iir->subscriber_gid.raw, gid_str,
				 sizeof gid_str),
		       cl_ntoh16(p_iir->subscriber_enum),
		       inet_ntop(AF_INET6, p_iir->inform_info.gid.raw, gid_str2,
				 sizeof gid_str2),
		       cl_ntoh16(p_iir->inform_info.lid_range_begin),
		       cl_ntoh16(p_iir->inform_info.lid_range_end),
		       p_iir->inform_info.is_generic,
		       p_iir->inform_info.subscribe,
		       cl_ntoh16(p_iir->inform_info.trap_type),
		       cl_ntoh16(p_iir->inform_info.g_or_v.generic.trap_num),
		       cl_ntoh32(qpn),
		       resp_time_val,
		       cl_ntoh32(ib_inform_info_get_prod_type
				 (&p_iir->inform_info))
		    );
	} else {
		printf("InformInfoRecord dump:\n"
		       "\t\tRID\n"
		       "\t\tSubscriberGID...........%s\n"
		       "\t\tSubscriberEnum..........0x%X\n"
		       "\t\tInformInfo dump:\n"
		       "\t\tgid.....................%s\n"
		       "\t\tlid_range_begin.........0x%X\n"
		       "\t\tlid_range_end...........0x%X\n"
		       "\t\tis_generic..............0x%X\n"
		       "\t\tsubscribe...............0x%X\n"
		       "\t\ttrap_type...............0x%X\n"
		       "\t\tdev_id..................0x%X\n"
		       "\t\tqpn.....................0x%06X\n"
		       "\t\tresp_time_val...........0x%X\n"
		       "\t\tvendor_id...............0x%06X\n"
		       "",
		       inet_ntop(AF_INET6, p_iir->subscriber_gid.raw, gid_str,
				 sizeof gid_str),
		       cl_ntoh16(p_iir->subscriber_enum),
		       inet_ntop(AF_INET6, p_iir->inform_info.gid.raw,
				 gid_str2, sizeof gid_str2),
		       cl_ntoh16(p_iir->inform_info.lid_range_begin),
		       cl_ntoh16(p_iir->inform_info.lid_range_end),
		       p_iir->inform_info.is_generic,
		       p_iir->inform_info.subscribe,
		       cl_ntoh16(p_iir->inform_info.trap_type),
		       cl_ntoh16(p_iir->inform_info.g_or_v.vend.dev_id),
		       cl_ntoh32(qpn),
		       resp_time_val,
		       cl_ntoh32(ib_inform_info_get_prod_type
				 (&p_iir->inform_info))
		    );
	}
}

static void dump_one_link_record(void *data)
{
	ib_link_record_t *lr = data;
	printf("LinkRecord dump:\n"
	       "\t\tFromLID....................%u\n"
	       "\t\tFromPort...................%u\n"
	       "\t\tToPort.....................%u\n"
	       "\t\tToLID......................%u\n",
	       cl_ntoh16(lr->from_lid), lr->from_port_num,
	       lr->to_port_num, cl_ntoh16(lr->to_lid));
}

static void dump_one_slvl_record(void *data)
{
	ib_slvl_table_record_t *slvl = data;
	ib_slvl_table_t *t = &slvl->slvl_tbl;
	printf("SL2VLTableRecord dump:\n"
	       "\t\tLID........................%u\n"
	       "\t\tInPort.....................%u\n"
	       "\t\tOutPort....................%u\n"
	       "\t\tSL: 0| 1| 2| 3| 4| 5| 6| 7| 8| 9|10|11|12|13|14|15|\n"
	       "\t\tVL:%2u|%2u|%2u|%2u|%2u|%2u|%2u|%2u|%2u|%2u|%2u|%2u|%2u"
	       "|%2u|%2u|%2u|\n",
	       cl_ntoh16(slvl->lid), slvl->in_port_num, slvl->out_port_num,
	       ib_slvl_table_get(t, 0), ib_slvl_table_get(t, 1),
	       ib_slvl_table_get(t, 2), ib_slvl_table_get(t, 3),
	       ib_slvl_table_get(t, 4), ib_slvl_table_get(t, 5),
	       ib_slvl_table_get(t, 6), ib_slvl_table_get(t, 7),
	       ib_slvl_table_get(t, 8), ib_slvl_table_get(t, 9),
	       ib_slvl_table_get(t, 10), ib_slvl_table_get(t, 11),
	       ib_slvl_table_get(t, 12), ib_slvl_table_get(t, 13),
	       ib_slvl_table_get(t, 14), ib_slvl_table_get(t, 15));
}

static void dump_one_vlarb_record(void *data)
{
	ib_vl_arb_table_record_t *vlarb = data;
	ib_vl_arb_element_t *e = vlarb->vl_arb_tbl.vl_entry;
	int i;
	printf("VLArbTableRecord dump:\n"
	       "\t\tLID........................%u\n"
	       "\t\tPort.......................%u\n"
	       "\t\tBlock......................%u\n",
	       cl_ntoh16(vlarb->lid), vlarb->port_num, vlarb->block_num);
	for (i = 0; i < 32; i += 16) {
		printf("\t\tVL    :%2u|%2u|%2u|%2u|%2u|%2u|%2u|%2u|"
		       "%2u|%2u|%2u|%2u|%2u|%2u|%2u|%2u|",
		       e[i + 0].vl, e[i + 1].vl, e[i + 2].vl, e[i + 3].vl,
		       e[i + 4].vl, e[i + 5].vl, e[i + 6].vl, e[i + 7].vl,
		       e[i + 8].vl, e[i + 9].vl, e[i + 10].vl, e[i + 11].vl,
		       e[i + 12].vl, e[i + 13].vl, e[i + 14].vl, e[i + 15].vl);
		printf("\n\t\tWeight:%2u|%2u|%2u|%2u|%2u|%2u|%2u|%2u|"
		       "%2u|%2u|%2u|%2u|%2u|%2u|%2u|%2u|",
		       e[i + 0].weight, e[i + 1].weight, e[i + 2].weight,
		       e[i + 3].weight, e[i + 4].weight, e[i + 5].weight,
		       e[i + 6].weight, e[i + 7].weight, e[i + 8].weight,
		       e[i + 9].weight, e[i + 10].weight, e[i + 11].weight,
		       e[i + 12].weight, e[i + 13].weight, e[i + 14].weight,
		       e[i + 15].weight);
		printf("\n");
	}
}

static void dump_one_pkey_tbl_record(void *data)
{
	ib_pkey_table_record_t *pktr = data;
	ib_net16_t *p = pktr->pkey_tbl.pkey_entry;
	int i;
	printf("PKeyTableRecord dump:\n"
	       "\t\tLID........................%u\n"
	       "\t\tPort.......................%u\n"
	       "\t\tBlock......................%u\n"
	       "\t\tPKey Table:\n",
	       cl_ntoh16(pktr->lid), pktr->port_num, pktr->block_num);
	for (i = 0; i < 32; i += 8)
		printf("\t\t0x%04x 0x%04x 0x%04x 0x%04x"
		       " 0x%04x 0x%04x 0x%04x 0x%04x\n",
		       cl_ntoh16(p[i + 0]), cl_ntoh16(p[i + 1]),
		       cl_ntoh16(p[i + 2]), cl_ntoh16(p[i + 3]),
		       cl_ntoh16(p[i + 4]), cl_ntoh16(p[i + 5]),
		       cl_ntoh16(p[i + 6]), cl_ntoh16(p[i + 7]));
	printf("\n");
}

static void dump_one_lft_record(void *data)
{
	ib_lft_record_t *lftr = data;
	unsigned block = cl_ntoh16(lftr->block_num);
	int i;
	printf("LFT Record dump:\n"
	       "\t\tLID........................%u\n"
	       "\t\tBlock......................%u\n"
	       "\t\tLFT:\n\t\tLID\tPort Number\n", cl_ntoh16(lftr->lid), block);
	for (i = 0; i < 64; i++)
		printf("\t\t%u\t%u\n", block * 64 + i, lftr->lft[i]);
	printf("\n");
}

static void dump_one_mft_record(void *data)
{
	ib_mft_record_t *mftr = data;
	unsigned position = cl_ntoh16(mftr->position_block_num) >> 12;
	unsigned block = cl_ntoh16(mftr->position_block_num) &
	    IB_MCAST_BLOCK_ID_MASK_HO;
	int i;
	printf("MFT Record dump:\n"
	       "\t\tLID........................%u\n"
	       "\t\tPosition...................%u\n"
	       "\t\tBlock......................%u\n"
	       "\t\tMFT:\n\t\tMLID\tPort Mask\n",
	       cl_ntoh16(mftr->lid), position, block);
	for (i = 0; i < IB_MCAST_BLOCK_SIZE; i++)
		printf("\t\t0x%x\t0x%x\n",
		       IB_LID_MCAST_START_HO + block * 64 + i,
		       cl_ntoh16(mftr->mft[i]));
	printf("\n");
}
static void dump_results(osmv_query_res_t * r, void (*dump_func) (void *))
{
	int i;
	for (i = 0; i < r->result_cnt; i++) {
		void *data = osmv_get_query_result(r->p_result_madw, i);
		dump_func(data);
	}
}

static void return_mad(void)
{
	/*
	 * Return the IB query MAD to the pool as necessary.
	 */
	if (result.p_result_madw != NULL) {
		osm_mad_pool_put(&mad_pool, result.p_result_madw);
		result.p_result_madw = NULL;
	}
}

/**
 * Get any record(s)
 */
static ib_api_status_t
get_any_records(osm_bind_handle_t h,
		ib_net16_t attr_id, ib_net32_t attr_mod, ib_net64_t comp_mask,
		void *attr, ib_net16_t attr_offset, ib_net64_t sm_key)
{
	ib_api_status_t status;
	osmv_query_req_t req;
	osmv_user_query_t user;

	memset(&req, 0, sizeof(req));
	memset(&user, 0, sizeof(user));

	user.attr_id = attr_id;
	user.attr_offset = attr_offset;
	user.attr_mod = attr_mod;
	user.comp_mask = comp_mask;
	user.p_attr = attr;

	req.query_type = OSMV_QUERY_USER_DEFINED;
	req.timeout_ms = sa_timeout_ms;
	req.retry_cnt = 1;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = NULL;
	req.pfn_query_cb = query_res_cb;
	req.p_query_input = &user;
	req.sm_key = sm_key;

	if ((status = osmv_query_sa(h, &req)) != IB_SUCCESS) {
		fprintf(stderr, "Query SA failed: %s\n",
			ib_get_err_str(status));
		return status;
	}

	if (result.status != IB_SUCCESS) {
		fprintf(stderr, "Query result returned: %s\n",
			ib_get_err_str(result.status));
		return result.status;
	}

	return status;
}

/**
 * Get all the records available for requested query type.
 */
static ib_api_status_t
get_all_records(osm_bind_handle_t h,
		ib_net16_t query_id, ib_net16_t attr_offset, int trusted)
{
	return get_any_records(h, query_id, 0, 0, NULL, attr_offset,
			       trusted ? smkey : 0);
}

/**
 * return the lid from the node descriptor (name) supplied
 */
static ib_api_status_t
get_lid_from_name(osm_bind_handle_t h, const char *name, ib_net16_t * lid)
{
	int i = 0;
	ib_node_record_t *node_record = NULL;
	ib_node_info_t *p_ni = NULL;
	ib_net16_t attr_offset = ib_get_attr_offset(sizeof(*node_record));
	ib_api_status_t status;

	status = get_all_records(h, IB_MAD_ATTR_NODE_RECORD, attr_offset, 0);
	if (status != IB_SUCCESS)
		return (status);

	for (i = 0; i < result.result_cnt; i++) {
		node_record = osmv_get_query_node_rec(result.p_result_madw, i);
		p_ni = &(node_record->node_info);
		if (name
		    && strncmp(name, (char *)node_record->node_desc.description,
			       sizeof(node_record->node_desc.description)) ==
		    0) {
			*lid = cl_ntoh16(node_record->lid);
			break;
		}
	}
	return_mad();
	return (status);
}

static ib_net16_t get_lid(osm_bind_handle_t h, const char *name)
{
	ib_net16_t rc_lid = 0;

	if (!name)
		return (0);
	if (isalpha(name[0]))
		assert(get_lid_from_name(h, name, &rc_lid) == IB_SUCCESS);
	else
		rc_lid = atoi(name);
	if (rc_lid == 0)
		fprintf(stderr, "Failed to find lid for \"%s\"\n", name);
	return (rc_lid);
}

static int parse_lid_and_ports(osm_bind_handle_t h,
			       char *str, int *lid, int *port1, int *port2)
{
	char *p, *e;

	if (port1)
		*port1 = -1;
	if (port2)
		*port2 = -1;

	p = strchr(str, '/');
	if (p)
		*p = '\0';
	if (lid)
		*lid = get_lid(h, str);

	if (!p)
		return 0;
	str = p + 1;
	p = strchr(str, '/');
	if (p)
		*p = '\0';
	if (port1) {
		*port1 = strtoul(str, &e, 0);
		if (e == str)
			*port1 = -1;
	}

	if (!p)
		return 0;
	str = p + 1;
	if (port2) {
		*port2 = strtoul(str, &e, 0);
		if (e == str)
			*port2 = -1;
	}

	return 0;
}

/*
 * Get the portinfo records available with IsSM or IsSMdisabled CapabilityMask bit on.
 */
static ib_api_status_t
get_issm_records(osm_bind_handle_t h, ib_net32_t capability_mask)
{
	ib_portinfo_record_t attr;

	memset(&attr, 0, sizeof(attr));
	attr.port_info.capability_mask = capability_mask;

	return get_any_records(h, IB_MAD_ATTR_PORTINFO_RECORD,
			       cl_hton32(1 << 31), IB_PIR_COMPMASK_CAPMASK,
			       &attr,
			       ib_get_attr_offset(sizeof(ib_portinfo_record_t)),
			       0);
}

static ib_api_status_t print_node_records(osm_bind_handle_t h)
{
	int i = 0;
	ib_node_record_t *node_record = NULL;
	ib_net16_t attr_offset = ib_get_attr_offset(sizeof(*node_record));
	ib_api_status_t status;

	status = get_all_records(h, IB_MAD_ATTR_NODE_RECORD, attr_offset, 0);
	if (status != IB_SUCCESS)
		return (status);

	if (node_print_desc == ALL_DESC) {
		printf("   LID \"name\"\n");
		printf("================\n");
	}
	for (i = 0; i < result.result_cnt; i++) {
		node_record = osmv_get_query_node_rec(result.p_result_madw, i);
		if (node_print_desc == ALL_DESC) {
			print_node_desc(node_record);
		} else if (node_print_desc == NAME_OF_LID) {
			if (requested_lid == cl_ntoh16(node_record->lid)) {
				print_node_record(node_record);
			}
		} else if (node_print_desc == NAME_OF_GUID) {
			ib_node_info_t *p_ni = &(node_record->node_info);

			if (requested_guid == cl_ntoh64(p_ni->port_guid)) {
				print_node_record(node_record);
			}
		} else {
			if (!requested_name ||
			    (strncmp(requested_name,
				     (char *)node_record->node_desc.description,
				     sizeof(node_record->node_desc.
					    description)) == 0)) {
				print_node_record(node_record);
				if (node_print_desc == UNIQUE_LID_ONLY) {
					return_mad();
					exit(0);
				}
			}
		}
	}
	return_mad();
	return (status);
}

static ib_api_status_t
get_print_path_rec_lid(osm_bind_handle_t h,
		       ib_net16_t src_lid, ib_net16_t dst_lid)
{
	osmv_query_req_t req;
	osmv_lid_pair_t lid_pair;
	ib_api_status_t status;

	lid_pair.src_lid = cl_hton16(src_lid);
	lid_pair.dest_lid = cl_hton16(dst_lid);

	memset(&req, 0, sizeof(req));

	req.query_type = OSMV_QUERY_PATH_REC_BY_LIDS;
	req.timeout_ms = sa_timeout_ms;
	req.retry_cnt = 1;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = NULL;
	req.pfn_query_cb = query_res_cb;
	req.p_query_input = (void *)&lid_pair;
	req.sm_key = 0;

	if ((status = osmv_query_sa(h, &req)) != IB_SUCCESS) {
		fprintf(stderr, "ERROR: Query SA failed: %s\n",
			ib_get_err_str(status));
		return (status);
	}
	if (result.status != IB_SUCCESS) {
		fprintf(stderr, "ERROR: Query result returned: %s\n",
			ib_get_err_str(result.status));
		return (result.status);
	}
	status = result.status;
	dump_results(&result, dump_path_record);
	return_mad();
	return (status);
}

static ib_api_status_t
get_print_path_rec_gid(osm_bind_handle_t h,
		       const ib_gid_t * src_gid, const ib_gid_t * dst_gid)
{
	osmv_query_req_t req;
	osmv_gid_pair_t gid_pair;
	ib_api_status_t status;

	gid_pair.src_gid = *src_gid;
	gid_pair.dest_gid = *dst_gid;

	memset(&req, 0, sizeof(req));

	req.query_type = OSMV_QUERY_PATH_REC_BY_GIDS;
	req.timeout_ms = sa_timeout_ms;
	req.retry_cnt = 1;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = NULL;
	req.pfn_query_cb = query_res_cb;
	req.p_query_input = (void *)&gid_pair;
	req.sm_key = 0;

	if ((status = osmv_query_sa(h, &req)) != IB_SUCCESS) {
		fprintf(stderr, "ERROR: Query SA failed: %s\n",
			ib_get_err_str(status));
		return (status);
	}
	if (result.status != IB_SUCCESS) {
		fprintf(stderr, "ERROR: Query result returned: %s\n",
			ib_get_err_str(result.status));
		return (result.status);
	}
	status = result.status;
	dump_results(&result, dump_path_record);
	return_mad();
	return (status);
}

static ib_api_status_t get_print_class_port_info(osm_bind_handle_t h)
{
	osmv_query_req_t req;
	ib_api_status_t status;

	memset(&req, 0, sizeof(req));

	req.query_type = OSMV_QUERY_CLASS_PORT_INFO;
	req.timeout_ms = sa_timeout_ms;
	req.retry_cnt = 1;
	req.flags = OSM_SA_FLAGS_SYNC;
	req.query_context = NULL;
	req.pfn_query_cb = query_res_cb;
	req.p_query_input = NULL;
	req.sm_key = 0;

	if ((status = osmv_query_sa(h, &req)) != IB_SUCCESS) {
		fprintf(stderr, "ERROR: Query SA failed: %s\n",
			ib_get_err_str(status));
		return (status);
	}
	if (result.status != IB_SUCCESS) {
		fprintf(stderr, "ERROR: Query result returned: %s\n",
			ib_get_err_str(result.status));
		return (result.status);
	}
	status = result.status;
	dump_results(&result, dump_class_port_info);
	return_mad();
	return (status);
}

static int query_path_records(const struct query_cmd *q,
			      osm_bind_handle_t h, int argc, char *argv[])
{
	ib_net16_t attr_offset = ib_get_attr_offset(sizeof(ib_path_rec_t));
	ib_api_status_t status;

	status = get_all_records(h, IB_MAD_ATTR_PATH_RECORD, attr_offset, 0);
	if (status != IB_SUCCESS)
		return (status);

	dump_results(&result, dump_path_record);
	return_mad();
	return (status);
}

static ib_api_status_t print_issm_records(osm_bind_handle_t h)
{
	ib_api_status_t status;

	/* First, get IsSM records */
	status = get_issm_records(h, IB_PORT_CAP_IS_SM);
	if (status != IB_SUCCESS)
		return (status);

	printf("IsSM ports\n");
	dump_results(&result, dump_portinfo_record);
	return_mad();

	/* Now, get IsSMdisabled records */
	status = get_issm_records(h, IB_PORT_CAP_SM_DISAB);
	if (status != IB_SUCCESS)
		return (status);

	printf("\nIsSMdisabled ports\n");
	dump_results(&result, dump_portinfo_record);
	return_mad();

	return (status);
}

static ib_api_status_t print_multicast_member_records(osm_bind_handle_t h)
{
	osmv_query_res_t mc_group_result;
	ib_api_status_t status;

	status = get_all_records(h, IB_MAD_ATTR_MCMEMBER_RECORD,
				 ib_get_attr_offset(sizeof(ib_member_rec_t)),
				 1);
	if (status != IB_SUCCESS)
		return (status);

	mc_group_result = result;

	status = get_all_records(h, IB_MAD_ATTR_NODE_RECORD,
				 ib_get_attr_offset(sizeof(ib_node_record_t)),
				 0);
	if (status != IB_SUCCESS)
		goto return_mc;

	dump_results(&mc_group_result, dump_multicast_member_record);
	return_mad();

return_mc:
	/* return_mad for the mc_group_result */
	if (mc_group_result.p_result_madw != NULL) {
		osm_mad_pool_put(&mad_pool, mc_group_result.p_result_madw);
		mc_group_result.p_result_madw = NULL;
	}

	return (status);
}

static ib_api_status_t print_multicast_group_records(osm_bind_handle_t h)
{
	ib_api_status_t status;

	status = get_all_records(h, IB_MAD_ATTR_MCMEMBER_RECORD,
				 ib_get_attr_offset(sizeof(ib_member_rec_t)),
				 0);
	if (status != IB_SUCCESS)
		return (status);

	dump_results(&result, dump_multicast_group_record);
	return_mad();
	return (status);
}

static int query_class_port_info(const struct query_cmd *q,
				 osm_bind_handle_t h, int argc, char *argv[])
{
	return get_print_class_port_info(h);
}

static int query_node_records(const struct query_cmd *q,
			      osm_bind_handle_t h, int argc, char *argv[])
{
	return print_node_records(h);
}

static int query_portinfo_records(const struct query_cmd *q,
				  osm_bind_handle_t h, int argc, char *argv[])
{
	ib_portinfo_record_t pir;
	ib_net64_t comp_mask = 0;
	int lid = 0, port = -1;
	ib_api_status_t status;

	if (argc > 0)
		parse_lid_and_ports(h, argv[0], &lid, &port, NULL);

	memset(&pir, 0, sizeof(pir));

	if (lid > 0) {
		pir.lid = cl_hton16(lid);
		comp_mask |= IB_PIR_COMPMASK_LID;
	}
	if (port >= 0) {
		pir.port_num = cl_hton16(port);
		comp_mask |= IB_PIR_COMPMASK_PORTNUM;
	}

	status = get_any_records(h, IB_MAD_ATTR_PORTINFO_RECORD, 0,
				 comp_mask, &pir,
				 ib_get_attr_offset(sizeof(pir)), 0);

	if (status != IB_SUCCESS)
		return status;

	dump_results(&result, dump_one_portinfo_record);
	return_mad();

	return 0;
}

static int query_mcmember_records(const struct query_cmd *q,
				   osm_bind_handle_t h, int argc, char *argv[])
{
	return print_multicast_member_records(h);
}

static int query_service_records(const struct query_cmd *q,
				 osm_bind_handle_t h, int argc, char *argv[])
{
	ib_net16_t attr_offset =
	    ib_get_attr_offset(sizeof(ib_service_record_t));
	ib_api_status_t status;

	status = get_all_records(h, IB_MAD_ATTR_SERVICE_RECORD, attr_offset, 0);
	if (status != IB_SUCCESS)
		return (status);

	dump_results(&result, dump_service_record);
	return_mad();
	return (status);
}

static int query_informinfo_records(const struct query_cmd *q,
				    osm_bind_handle_t h, int argc, char *argv[])
{
	ib_net16_t attr_offset =
	    ib_get_attr_offset(sizeof(ib_inform_info_record_t));
	ib_api_status_t status;

	status =
	    get_all_records(h, IB_MAD_ATTR_INFORM_INFO_RECORD, attr_offset, 0);
	if (status != IB_SUCCESS)
		return (status);

	dump_results(&result, dump_inform_info_record);
	return_mad();
	return (status);
}

static int query_link_records(const struct query_cmd *q,
			      osm_bind_handle_t h, int argc, char *argv[])
{
	ib_link_record_t lr;
	ib_net64_t comp_mask = 0;
	int from_lid = 0, to_lid = 0, from_port = -1, to_port = -1;
	ib_api_status_t status;

	if (argc > 0)
		parse_lid_and_ports(h, argv[0], &from_lid, &from_port, NULL);

	if (argc > 1)
		parse_lid_and_ports(h, argv[1], &to_lid, &to_port, NULL);

	memset(&lr, 0, sizeof(lr));

	if (from_lid > 0) {
		lr.from_lid = cl_hton16(from_lid);
		comp_mask |= IB_LR_COMPMASK_FROM_LID;
	}
	if (from_port >= 0) {
		lr.from_port_num = from_port;
		comp_mask |= IB_LR_COMPMASK_FROM_PORT;
	}
	if (to_lid > 0) {
		lr.to_lid = cl_hton16(to_lid);
		comp_mask |= IB_LR_COMPMASK_TO_LID;
	}
	if (to_port >= 0) {
		lr.to_port_num = to_port;
		comp_mask |= IB_LR_COMPMASK_TO_PORT;
	}

	status = get_any_records(h, IB_MAD_ATTR_LINK_RECORD, 0,
				 comp_mask, &lr,
				 ib_get_attr_offset(sizeof(lr)), 0);
	if (status != IB_SUCCESS)
		return status;

	dump_results(&result, dump_one_link_record);
	return_mad();
	return status;
}

static int query_sl2vl_records(const struct query_cmd *q,
			       osm_bind_handle_t h, int argc, char *argv[])
{
	ib_slvl_table_record_t slvl;
	ib_net64_t comp_mask = 0;
	int lid = 0, in_port = -1, out_port = -1;
	ib_api_status_t status;

	if (argc > 0)
		parse_lid_and_ports(h, argv[0], &lid, &in_port, &out_port);

	memset(&slvl, 0, sizeof(slvl));

	if (lid > 0) {
		slvl.lid = cl_hton16(lid);
		comp_mask |= IB_SLVL_COMPMASK_LID;
	}
	if (in_port >= 0) {
		slvl.in_port_num = in_port;
		comp_mask |= IB_SLVL_COMPMASK_IN_PORT;
	}
	if (out_port >= 0) {
		slvl.out_port_num = out_port;
		comp_mask |= IB_SLVL_COMPMASK_OUT_PORT;
	}

	status = get_any_records(h, IB_MAD_ATTR_SLVL_RECORD, 0,
				 comp_mask, &slvl,
				 ib_get_attr_offset(sizeof(slvl)), 0);
	if (status != IB_SUCCESS)
		return status;

	dump_results(&result, dump_one_slvl_record);
	return_mad();
	return status;
}

static int query_vlarb_records(const struct query_cmd *q,
			       osm_bind_handle_t h, int argc, char *argv[])
{
	ib_vl_arb_table_record_t vlarb;
	ib_net64_t comp_mask = 0;
	int lid = 0, port = -1, block = -1;
	ib_api_status_t status;

	if (argc > 0)
		parse_lid_and_ports(h, argv[0], &lid, &port, &block);

	memset(&vlarb, 0, sizeof(vlarb));

	if (lid > 0) {
		vlarb.lid = cl_hton16(lid);
		comp_mask |= IB_VLA_COMPMASK_LID;
	}
	if (port >= 0) {
		vlarb.port_num = port;
		comp_mask |= IB_VLA_COMPMASK_OUT_PORT;
	}
	if (block >= 0) {
		vlarb.block_num = block;
		comp_mask |= IB_VLA_COMPMASK_BLOCK;
	}

	status = get_any_records(h, IB_MAD_ATTR_VLARB_RECORD, 0,
				 comp_mask, &vlarb,
				 ib_get_attr_offset(sizeof(vlarb)), 0);
	if (status != IB_SUCCESS)
		return status;

	dump_results(&result, dump_one_vlarb_record);
	return_mad();
	return status;
}

static int query_pkey_tbl_records(const struct query_cmd *q,
				  osm_bind_handle_t h, int argc, char *argv[])
{
	ib_pkey_table_record_t pktr;
	ib_net64_t comp_mask = 0;
	int lid = 0, port = -1, block = -1;
	ib_api_status_t status;

	if (argc > 0)
		parse_lid_and_ports(h, argv[0], &lid, &port, &block);

	memset(&pktr, 0, sizeof(pktr));

	if (lid > 0) {
		pktr.lid = cl_hton16(lid);
		comp_mask |= IB_PKEY_COMPMASK_LID;
	}
	if (port >= 0) {
		pktr.port_num = port;
		comp_mask |= IB_PKEY_COMPMASK_PORT;
	}
	if (block >= 0) {
		pktr.block_num = block;
		comp_mask |= IB_PKEY_COMPMASK_BLOCK;
	}

	status = get_any_records(h, IB_MAD_ATTR_PKEY_TBL_RECORD, 0,
				 comp_mask, &pktr,
				 ib_get_attr_offset(sizeof(pktr)), smkey);
	if (status != IB_SUCCESS)
		return status;

	dump_results(&result, dump_one_pkey_tbl_record);
	return_mad();
	return status;
}

static int query_lft_records(const struct query_cmd *q,
			     osm_bind_handle_t h, int argc, char *argv[])
{
	ib_lft_record_t lftr;
	ib_net64_t comp_mask = 0;
	int lid = 0, block = -1;
	ib_api_status_t status;

	if (argc > 0)
		parse_lid_and_ports(h, argv[0], &lid, &block, NULL);

	memset(&lftr, 0, sizeof(lftr));

	if (lid > 0) {
		lftr.lid = cl_hton16(lid);
		comp_mask |= IB_LFTR_COMPMASK_LID;
	}
	if (block >= 0) {
		lftr.block_num = cl_hton16(block);
		comp_mask |= IB_LFTR_COMPMASK_BLOCK;
	}

	status = get_any_records(h, IB_MAD_ATTR_LFT_RECORD, 0,
				 comp_mask, &lftr,
				 ib_get_attr_offset(sizeof(lftr)), 0);
	if (status != IB_SUCCESS)
		return status;

	dump_results(&result, dump_one_lft_record);
	return_mad();
	return status;
}

static int query_mft_records(const struct query_cmd *q,
			     osm_bind_handle_t h, int argc, char *argv[])
{
	ib_mft_record_t mftr;
	ib_net64_t comp_mask = 0;
	int lid = 0, block = -1, position = -1;
	ib_api_status_t status;

	if (argc > 0)
		parse_lid_and_ports(h, argv[0], &lid, &position, &block);

	memset(&mftr, 0, sizeof(mftr));

	if (lid > 0) {
		mftr.lid = cl_hton16(lid);
		comp_mask |= IB_MFTR_COMPMASK_LID;
	}
	if (position >= 0) {
		mftr.position_block_num = cl_hton16(position << 12);
		comp_mask |= IB_MFTR_COMPMASK_POSITION;
	}
	if (block >= 0) {
		mftr.position_block_num |=
		    cl_hton16(block & IB_MCAST_BLOCK_ID_MASK_HO);
		comp_mask |= IB_MFTR_COMPMASK_BLOCK;
	}

	status = get_any_records(h, IB_MAD_ATTR_MFT_RECORD, 0,
				 comp_mask, &mftr,
				 ib_get_attr_offset(sizeof(mftr)), 0);
	if (status != IB_SUCCESS)
		return status;

	dump_results(&result, dump_one_mft_record);
	return_mad();
	return status;
}

static osm_bind_handle_t get_bind_handle(void)
{
	uint32_t i = 0;
	uint64_t port_guid = (uint64_t) - 1;
	osm_bind_handle_t h;
	ib_api_status_t status;
	ib_port_attr_t attr_array[MAX_PORTS];
	uint32_t num_ports = MAX_PORTS;
	uint32_t ca_name_index = 0;

	complib_init();

	osm_log_construct(&log_osm);
	if ((status = osm_log_init_v2(&log_osm, TRUE, 0x0001, NULL,
				      0, TRUE)) != IB_SUCCESS) {
		fprintf(stderr, "Failed to init osm_log: %s\n",
			ib_get_err_str(status));
		exit(-1);
	}
	osm_log_set_level(&log_osm, OSM_LOG_NONE);
	if (osm_debug)
		osm_log_set_level(&log_osm, OSM_LOG_DEFAULT_LEVEL);

	vendor = osm_vendor_new(&log_osm, sa_timeout_ms);
	osm_mad_pool_construct(&mad_pool);
	if ((status = osm_mad_pool_init(&mad_pool)) != IB_SUCCESS) {
		fprintf(stderr, "Failed to init mad pool: %s\n",
			ib_get_err_str(status));
		exit(-1);
	}

	if ((status =
	     osm_vendor_get_all_port_attr(vendor, attr_array,
					  &num_ports)) != IB_SUCCESS) {
		fprintf(stderr, "Failed to get port attributes: %s\n",
			ib_get_err_str(status));
		exit(-1);
	}

	for (i = 0; i < num_ports; i++) {
		if (i > 1 && cl_ntoh64(attr_array[i].port_guid)
		    != (cl_ntoh64(attr_array[i - 1].port_guid) + 1))
			ca_name_index++;
		if (sa_port_num && sa_port_num != attr_array[i].port_num)
			continue;
		if (sa_hca_name
		    && strcmp(sa_hca_name,
			      vendor->ca_names[ca_name_index]) != 0)
			continue;
		if (attr_array[i].link_state == IB_LINK_ACTIVE) {
			port_guid = attr_array[i].port_guid;
			break;
		}
	}

	if (port_guid == (uint64_t) - 1) {
		fprintf(stderr,
			"Failed to find active port, check port status with \"ibstat\"\n");
		exit(-1);
	}

	h = osmv_bind_sa(vendor, &mad_pool, port_guid);

	if (h == OSM_BIND_INVALID_HANDLE) {
		fprintf(stderr, "Failed to bind to SA\n");
		exit(-1);
	}
	return h;
}

static void clean_up(void)
{
	osm_mad_pool_destroy(&mad_pool);
	osm_vendor_delete(&vendor);
}

static const struct query_cmd query_cmds[] = {
	{"ClassPortInfo", "CPI", IB_MAD_ATTR_CLASS_PORT_INFO,
	 NULL, query_class_port_info},
	{"NodeRecord", "NR", IB_MAD_ATTR_NODE_RECORD,
	 NULL, query_node_records},
	{"PortInfoRecord", "PIR", IB_MAD_ATTR_PORTINFO_RECORD,
	 "[[lid]/[port]]", query_portinfo_records},
	{"SL2VLTableRecord", "SL2VL", IB_MAD_ATTR_SLVL_RECORD,
	 "[[lid]/[in_port]/[out_port]]", query_sl2vl_records},
	{"PKeyTableRecord", "PKTR", IB_MAD_ATTR_PKEY_TBL_RECORD,
	 "[[lid]/[port]/[block]]", query_pkey_tbl_records},
	{"VLArbitrationTableRecord", "VLAR", IB_MAD_ATTR_VLARB_RECORD,
	 "[[lid]/[port]/[block]]", query_vlarb_records},
	{"InformInfoRecord", "IIR", IB_MAD_ATTR_INFORM_INFO_RECORD,
	 NULL, query_informinfo_records},
	{"LinkRecord", "LR", IB_MAD_ATTR_LINK_RECORD,
	 "[[from_lid]/[from_port]] [[to_lid]/[to_port]]", query_link_records},
	{"ServiceRecord", "SR", IB_MAD_ATTR_SERVICE_RECORD,
	 NULL, query_service_records},
	{"PathRecord", "PR", IB_MAD_ATTR_PATH_RECORD,
	 NULL, query_path_records},
	{"MCMemberRecord", "MCMR", IB_MAD_ATTR_MCMEMBER_RECORD,
	 NULL, query_mcmember_records},
	{"LFTRecord", "LFTR", IB_MAD_ATTR_LFT_RECORD,
	 "[[lid]/[block]]", query_lft_records},
	{"MFTRecord", "MFTR", IB_MAD_ATTR_MFT_RECORD,
	 "[[mlid]/[position]/[block]]", query_mft_records},
	{0}
};

static const struct query_cmd *find_query(const char *name)
{
	const struct query_cmd *q;
	unsigned len = strlen(name);

	for (q = query_cmds; q->name; q++)
		if (!strncasecmp(name, q->name, len) ||
		    (q->alias && !strncasecmp(name, q->alias, len)))
			return q;

	return NULL;
}

static const struct query_cmd *find_query_by_type(ib_net16_t type)
{
	const struct query_cmd *q;

	for (q = query_cmds; q->name; q++)
		if (q->query_type == type)
			return q;

	return NULL;
}

static void usage(void)
{
	const struct query_cmd *q;

	fprintf(stderr, "Usage: %s [-h -d -p -N] [--list | -D] [-S -I -L -l -G"
		" -O -U -c -s -g -m --src-to-dst <src:dst> --sgid-to-dgid <src-dst> "
		"-C <ca_name> -P <ca_port> -t(imeout) <msec>] [query-name] [<name> | <lid> | <guid>]\n",
		argv0);
	fprintf(stderr, "   Queries node records by default\n");
	fprintf(stderr, "   -d enable debugging\n");
	fprintf(stderr, "   -p get PathRecord info\n");
	fprintf(stderr, "   -N get NodeRecord info\n");
	fprintf(stderr, "   --list | -D the node desc of the CA's\n");
	fprintf(stderr, "   -S get ServiceRecord info\n");
	fprintf(stderr, "   -I get InformInfoRecord (subscription) info\n");
	fprintf(stderr, "   -L return the Lids of the name specified\n");
	fprintf(stderr, "   -l return the unique Lid of the name specified\n");
	fprintf(stderr, "   -G return the Guids of the name specified\n");
	fprintf(stderr, "   -O return name for the Lid specified\n");
	fprintf(stderr, "   -U return name for the Guid specified\n");
	fprintf(stderr, "   -c get the SA's class port info\n");
	fprintf(stderr, "   -s return the PortInfoRecords with isSM or "
		"isSMdisabled capability mask bit on\n");
	fprintf(stderr, "   -g get multicast group info\n");
	fprintf(stderr, "   -m get multicast member info\n");
	fprintf(stderr, "      (if multicast group specified, list member GIDs"
		" only for group specified\n");
	fprintf(stderr, "      specified, for example 'saquery -m 0xC000')\n");
	fprintf(stderr, "   -x get LinkRecord info\n");
	fprintf(stderr, "   --src-to-dst get a PathRecord for <src:dst>\n"
		"                where src and dst are either node "
		"names or LIDs\n");
	fprintf(stderr, "   --sgid-to-dgid get a PathRecord for <sgid-dgid>\n"
		"                where sgid and dgid are addresses in "
		"IPv6 format\n");
	fprintf(stderr, "   -C <ca_name> specify the SA query HCA\n");
	fprintf(stderr, "   -P <ca_port> specify the SA query port\n");
	fprintf(stderr, "   --smkey <val> specify SM_Key value for the query."
		" If non-numeric value \n"
		"                 (like 'x') is specified then "
		"saquery will prompt for a value\n");
	fprintf(stderr, "   -t | --timeout <msec> specify the SA query "
		"response timeout (default %u msec)\n", DEFAULT_SA_TIMEOUT_MS);
	fprintf(stderr,
		"   --node-name-map <node-name-map> specify a node name map\n");
	fprintf(stderr, "\n   Supported query names (and aliases):\n");
	for (q = query_cmds; q->name; q++)
		fprintf(stderr, "      %s (%s) %s\n", q->name,
			q->alias ? q->alias : "", q->usage ? q->usage : "");
	fprintf(stderr, "\n");

	exit(-1);
}

enum saquery_command {
	SAQUERY_CMD_QUERY,
	SAQUERY_CMD_NODE_RECORD,
	SAQUERY_CMD_PATH_RECORD,
	SAQUERY_CMD_CLASS_PORT_INFO,
	SAQUERY_CMD_ISSM,
	SAQUERY_CMD_MCGROUPS,
	SAQUERY_CMD_MCMEMBERS,
};

int main(int argc, char **argv)
{
	int ch = 0;
	osm_bind_handle_t h;
	enum saquery_command command = SAQUERY_CMD_QUERY;
	const struct query_cmd *q = NULL;
	char *src = NULL, *dst = NULL;
	char *sgid = NULL, *dgid = NULL;
	ib_net16_t query_type = 0;
	ib_net16_t src_lid, dst_lid;
	ib_api_status_t status;

	static char const str_opts[] = "pVNDLlGOUcSIsgmxdhP:C:t:";
	static const struct option long_opts[] = {
		{"p", 0, 0, 'p'},
		{"Version", 0, 0, 'V'},
		{"N", 0, 0, 'N'},
		{"L", 0, 0, 'L'},
		{"l", 0, 0, 'l'},
		{"G", 0, 0, 'G'},
		{"O", 0, 0, 'O'},
		{"U", 0, 0, 'U'},
		{"s", 0, 0, 's'},
		{"g", 0, 0, 'g'},
		{"m", 0, 0, 'm'},
		{"x", 0, 0, 'x'},
		{"d", 0, 0, 'd'},
		{"c", 0, 0, 'c'},
		{"S", 0, 0, 'S'},
		{"I", 0, 0, 'I'},
		{"P", 1, 0, 'P'},
		{"C", 1, 0, 'C'},
		{"help", 0, 0, 'h'},
		{"list", 0, 0, 'D'},
		{"src-to-dst", 1, 0, 1},
		{"sgid-to-dgid", 1, 0, 2},
		{"timeout", 1, 0, 't'},
		{"node-name-map", 1, 0, 3},
		{"smkey", 1, 0, 4},
		{}
	};

	argv0 = argv[0];

	while ((ch = getopt_long(argc, argv, str_opts, long_opts, NULL)) != -1) {
		switch (ch) {
		case 1:
			{
				char *opt = strdup(optarg);
				char *ch = strchr(opt, ':');
				if (!ch) {
					fprintf(stderr,
						"ERROR: --src-to-dst <node>:<node>\n");
					usage();
				}
				*ch++ = '\0';
				if (*opt)
					src = strdup(opt);
				if (*ch)
					dst = strdup(ch);
				free(opt);
				command = SAQUERY_CMD_PATH_RECORD;
				break;
			}
		case 2:
			{
				char *opt = strdup(optarg);
				char *tok1 = strtok(opt, "-");
				char *tok2 = strtok(NULL, "\0");

				if (tok1 && tok2) {
					sgid = strdup(tok1);
					dgid = strdup(tok2);
				} else {
					fprintf(stderr,
						"ERROR: --sgid-to-dgid <GID>-<GID>\n");
					usage();
				}
				free(opt);
				command = SAQUERY_CMD_PATH_RECORD;
				break;
			}
		case 3:
			node_name_map_file = strdup(optarg);
			break;
		case 4:
			if (!isxdigit(*optarg) &&
			    !(optarg = getpass("SM_Key: "))) {
				fprintf(stderr, "cannot get SM_Key\n");
				usage();
			}
			smkey = cl_hton64(strtoull(optarg, NULL, 0));
			break;
		case 'p':
			command = SAQUERY_CMD_PATH_RECORD;
			break;
		case 'V':
			fprintf(stderr, "%s %s\n", argv0, get_build_version());
			exit(-1);
		case 'D':
			node_print_desc = ALL_DESC;
			break;
		case 'c':
			command = SAQUERY_CMD_CLASS_PORT_INFO;
			break;
		case 'S':
			query_type = IB_MAD_ATTR_SERVICE_RECORD;
			break;
		case 'I':
			query_type = IB_MAD_ATTR_INFORM_INFO_RECORD;
			break;
		case 'N':
			command = SAQUERY_CMD_NODE_RECORD;
			break;
		case 'L':
			node_print_desc = LID_ONLY;
			break;
		case 'l':
			node_print_desc = UNIQUE_LID_ONLY;
			break;
		case 'G':
			node_print_desc = GUID_ONLY;
			break;
		case 'O':
			node_print_desc = NAME_OF_LID;
			break;
		case 'U':
			node_print_desc = NAME_OF_GUID;
			break;
		case 's':
			command = SAQUERY_CMD_ISSM;
			break;
		case 'g':
			command = SAQUERY_CMD_MCGROUPS;
			break;
		case 'm':
			command = SAQUERY_CMD_MCMEMBERS;
			break;
		case 'x':
			query_type = IB_MAD_ATTR_LINK_RECORD;
			break;
		case 'd':
			osm_debug = 1;
			break;
		case 'C':
			sa_hca_name = optarg;
			break;
		case 'P':
			sa_port_num = strtoul(optarg, NULL, 0);
			break;
		case 't':
			sa_timeout_ms = strtoul(optarg, NULL, 0);
			break;
		case 'h':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (!query_type) {
		if (!argc || !(q = find_query(argv[0])))
			query_type = IB_MAD_ATTR_NODE_RECORD;
		else {
			query_type = q->query_type;
			argc--;
			argv++;
		}
	}

	if (argc) {
		if (node_print_desc == NAME_OF_LID) {
			requested_lid = (ib_net16_t) strtoul(argv[0], NULL, 0);
			requested_lid_flag++;
		} else if (node_print_desc == NAME_OF_GUID) {
			requested_guid = (ib_net64_t) strtoul(argv[0], NULL, 0);
			requested_guid_flag++;
		} else {
			requested_name = argv[0];
		}
	}

	if ((node_print_desc == LID_ONLY ||
	     node_print_desc == UNIQUE_LID_ONLY ||
	     node_print_desc == GUID_ONLY) && !requested_name) {
		fprintf(stderr, "ERROR: name not specified\n");
		usage();
	}

	if (node_print_desc == NAME_OF_LID && !requested_lid_flag) {
		fprintf(stderr, "ERROR: lid not specified\n");
		usage();
	}

	if (node_print_desc == NAME_OF_GUID && !requested_guid_flag) {
		fprintf(stderr, "ERROR: guid not specified\n");
		usage();
	}

	/* Note: lid cannot be 0; see infiniband spec 4.1.3 */
	if (node_print_desc == NAME_OF_LID && !requested_lid) {
		fprintf(stderr, "ERROR: lid invalid\n");
		usage();
	}

	h = get_bind_handle();
	node_name_map = open_node_name_map(node_name_map_file);

	switch (command) {
	case SAQUERY_CMD_NODE_RECORD:
		status = print_node_records(h);
		break;
	case SAQUERY_CMD_PATH_RECORD:
		if (src && dst) {
			src_lid = get_lid(h, src);
			dst_lid = get_lid(h, dst);
			printf("Path record for %s -> %s\n", src, dst);
			if (src_lid == 0 || dst_lid == 0) {
				status = IB_UNKNOWN_ERROR;
			} else {
				status =
				    get_print_path_rec_lid(h, src_lid, dst_lid);
			}
		} else if (sgid && dgid) {
			struct in6_addr src_addr, dst_addr;

			if (inet_pton(AF_INET6, sgid, &src_addr) <= 0) {
				fprintf(stderr, "invalid src gid: %s\n", sgid);
				exit(-1);
			}
			if (inet_pton(AF_INET6, dgid, &dst_addr) <= 0) {
				fprintf(stderr, "invalid dst gid: %s\n", dgid);
				exit(-1);
			}
			status = get_print_path_rec_gid(h,
					(ib_gid_t *) & src_addr.s6_addr,
					(ib_gid_t *) & dst_addr.s6_addr);
		} else {
			status = query_path_records(q, h, 0, NULL);
		}
		break;
	case SAQUERY_CMD_CLASS_PORT_INFO:
		status = get_print_class_port_info(h);
		break;
	case SAQUERY_CMD_ISSM:
		status = print_issm_records(h);
		break;
	case SAQUERY_CMD_MCGROUPS:
		status = print_multicast_group_records(h);
		break;
	case SAQUERY_CMD_MCMEMBERS:
		status = print_multicast_member_records(h);
		break;
	default:
		if ((!q && !(q = find_query_by_type(query_type)))
		    || !q->handler) {
			fprintf(stderr, "Unknown query type %d\n",
				ntohs(query_type));
			status = IB_UNKNOWN_ERROR;
		} else
			status = q->handler(q, h, argc, argv);
		break;
	}

	if (src)
		free(src);
	if (dst)
		free(dst);
	clean_up();
	close_node_name_map(node_name_map);
	return (status);
}
