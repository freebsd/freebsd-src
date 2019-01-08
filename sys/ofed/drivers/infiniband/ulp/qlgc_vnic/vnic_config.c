/*
 * Copyright (c) 2006 QLogic, Inc.  All rights reserved.
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
 */

#include <linux/netdevice.h>
#include <linux/string.h>
#include <linux/utsname.h>
#include <linux/if_vlan.h>

#include "vnic_util.h"
#include "vnic_config.h"
#include "vnic_trailer.h"
#include "vnic_main.h"

u16 vnic_max_mtu = MAX_MTU;

static u32 default_no_path_timeout = DEFAULT_NO_PATH_TIMEOUT;
static u32 sa_path_rec_get_timeout = SA_PATH_REC_GET_TIMEOUT;
static u32 default_primary_reconnect_timeout =
				    DEFAULT_PRIMARY_RECONNECT_TIMEOUT;
static u32 default_primary_switch_timeout = DEFAULT_PRIMARY_SWITCH_TIMEOUT;
static int default_prefer_primary         = DEFAULT_PREFER_PRIMARY;

static int use_rx_csum = VNIC_USE_RX_CSUM;
static int use_tx_csum = VNIC_USE_TX_CSUM;

static u32 control_response_timeout = CONTROL_RSP_TIMEOUT;
static u32 completion_limit = DEFAULT_COMPLETION_LIMIT;

module_param(vnic_max_mtu, ushort, 0444);
MODULE_PARM_DESC(vnic_max_mtu, "Maximum MTU size (1500-9500). Default is 9500");

module_param(default_prefer_primary, bool, 0444);
MODULE_PARM_DESC(default_prefer_primary, "Determines if primary path is"
		 " preferred (1) or not (0). Defaults to 0");
module_param(use_rx_csum, bool, 0444);
MODULE_PARM_DESC(use_rx_csum, "Determines if RX checksum is done on VEx (1)"
		 " or not (0). Defaults to 1");
module_param(use_tx_csum, bool, 0444);
MODULE_PARM_DESC(use_tx_csum, "Determines if TX checksum is done on VEx (1)"
		 " or not (0). Defaults to 1");
module_param(default_no_path_timeout, uint, 0444);
MODULE_PARM_DESC(default_no_path_timeout, "Time to wait in milliseconds"
		 " before reconnecting to VEx after connection loss");
module_param(default_primary_reconnect_timeout, uint, 0444);
MODULE_PARM_DESC(default_primary_reconnect_timeout,  "Time to wait in"
		 " milliseconds before reconnecting the"
		 " primary path to VEx");
module_param(default_primary_switch_timeout, uint, 0444);
MODULE_PARM_DESC(default_primary_switch_timeout, "Time to wait before"
		 " switching back to primary path if"
		 " primary path is preferred");
module_param(sa_path_rec_get_timeout, uint, 0444);
MODULE_PARM_DESC(sa_path_rec_get_timeout, "Time out value in milliseconds"
		 " for SA path record get queries");

module_param(control_response_timeout, uint, 0444);
MODULE_PARM_DESC(control_response_timeout, "Time out value in milliseconds"
		 " to wait for response to control requests");

module_param(completion_limit, uint, 0444);
MODULE_PARM_DESC(completion_limit, "Maximum completions to process"
		" in a single completion callback invocation. Default is 100"
		" Minimum value is 10");

static void config_control_defaults(struct control_config *control_config,
				    struct path_param *params)
{
	int len;
	char *dot;
	u64 sid;

	sid = (SST_AGN << 56) | (SST_OUI << 32) | (CONTROL_PATH_ID << 8)
	      |	IOC_NUMBER(be64_to_cpu(params->ioc_guid));

	control_config->ib_config.service_id = cpu_to_be64(sid);
	control_config->ib_config.conn_data.path_id = 0;
	control_config->ib_config.conn_data.vnic_instance = params->instance;
	control_config->ib_config.conn_data.path_num = 0;
	control_config->ib_config.conn_data.features_supported =
			__constant_cpu_to_be32((u32) (VNIC_FEAT_IGNORE_VLAN |
						      VNIC_FEAT_RDMA_IMMED));
	dot = strchr(init_utsname()->nodename, '.');

	if (dot)
		len = dot - init_utsname()->nodename;
	else
		len = strlen(init_utsname()->nodename);

	if (len > VNIC_MAX_NODENAME_LEN)
		len = VNIC_MAX_NODENAME_LEN;

	memcpy(control_config->ib_config.conn_data.nodename,
			init_utsname()->nodename, len);

	if (params->ib_multicast == 1)
		control_config->ib_multicast = 1;
	else if (params->ib_multicast == 0)
		control_config->ib_multicast = 0;
	else {
		/* parameter is not set - enable it by default */
		control_config->ib_multicast = 1;
		CONFIG_ERROR ("IOCGUID=%llx INSTANCE=%d IB_MULTICAST defaulted"
			      " to TRUE\n", be64_to_cpu(params->ioc_guid),
			      (char)params->instance);
	}

	if (control_config->ib_multicast)
		control_config->ib_config.conn_data.features_supported |=
			__constant_cpu_to_be32(VNIC_FEAT_INBOUND_IB_MC);

	control_config->ib_config.retry_count = RETRY_COUNT;
	control_config->ib_config.rnr_retry_count = RETRY_COUNT;
	control_config->ib_config.min_rnr_timer = MIN_RNR_TIMER;

	/* These values are not configurable*/
	control_config->ib_config.num_recvs    = 5;
	control_config->ib_config.num_sends    = 1;
	control_config->ib_config.recv_scatter = 1;
	control_config->ib_config.send_gather  = 1;
	control_config->ib_config.completion_limit = completion_limit;

	control_config->num_recvs = control_config->ib_config.num_recvs;

	control_config->vnic_instance = params->instance;
	control_config->max_address_entries = MAX_ADDRESS_ENTRIES;
	control_config->min_address_entries = MIN_ADDRESS_ENTRIES;
	control_config->rsp_timeout = msecs_to_jiffies(control_response_timeout);
}

static void config_data_defaults(struct data_config *data_config,
				 struct path_param *params)
{
	u64 sid;

	sid = (SST_AGN << 56) | (SST_OUI << 32) | (DATA_PATH_ID << 8)
	      |	IOC_NUMBER(be64_to_cpu(params->ioc_guid));

	data_config->ib_config.service_id = cpu_to_be64(sid);
	data_config->ib_config.conn_data.path_id = jiffies; /* random */
	data_config->ib_config.conn_data.vnic_instance = params->instance;
	data_config->ib_config.conn_data.path_num = 0;

	data_config->ib_config.retry_count = RETRY_COUNT;
	data_config->ib_config.rnr_retry_count = RETRY_COUNT;
	data_config->ib_config.min_rnr_timer = MIN_RNR_TIMER;

	/*
	 * NOTE: the num_recvs size assumes that the EIOC could
	 * RDMA enough packets to fill all of the host recv
	 * pool entries, plus send a kick message after each
	 * packet, plus RDMA new buffers for the size of
	 * the EIOC recv buffer pool, plus send kick messages
	 * after each min_host_update_sz of new buffers all
	 * before the host can even pull off the first completed
	 * receive off the completion queue, and repost the
	 * receive. NOT LIKELY!
	 */
	data_config->ib_config.num_recvs = HOST_RECV_POOL_ENTRIES +
	    (MAX_EIOC_POOL_SZ / MIN_HOST_UPDATE_SZ);

	data_config->ib_config.num_sends = (2 * NOTIFY_BUNDLE_SZ) +
	    (HOST_RECV_POOL_ENTRIES / MIN_EIOC_UPDATE_SZ) + 1;

	data_config->ib_config.recv_scatter = 1; /* not configurable */
	data_config->ib_config.send_gather = 2;	 /* not configurable */
	data_config->ib_config.completion_limit = completion_limit;

	data_config->num_recvs = data_config->ib_config.num_recvs;
	data_config->path_id = data_config->ib_config.conn_data.path_id;


	data_config->host_recv_pool_entries = HOST_RECV_POOL_ENTRIES;

	data_config->host_min.size_recv_pool_entry =
			cpu_to_be32(BUFFER_SIZE(VLAN_ETH_HLEN + MIN_MTU));
	data_config->host_max.size_recv_pool_entry =
			cpu_to_be32(BUFFER_SIZE(VLAN_ETH_HLEN + vnic_max_mtu));
	data_config->eioc_min.size_recv_pool_entry =
			cpu_to_be32(BUFFER_SIZE(VLAN_ETH_HLEN + MIN_MTU));
	data_config->eioc_max.size_recv_pool_entry =
			__constant_cpu_to_be32(MAX_PARAM_VALUE);

	data_config->host_min.num_recv_pool_entries =
				__constant_cpu_to_be32(MIN_HOST_POOL_SZ);
	data_config->host_max.num_recv_pool_entries =
				__constant_cpu_to_be32(MAX_PARAM_VALUE);
	data_config->eioc_min.num_recv_pool_entries =
				__constant_cpu_to_be32(MIN_EIOC_POOL_SZ);
	data_config->eioc_max.num_recv_pool_entries =
				__constant_cpu_to_be32(MAX_EIOC_POOL_SZ);

	data_config->host_min.timeout_before_kick =
			__constant_cpu_to_be32(MIN_HOST_KICK_TIMEOUT);
	data_config->host_max.timeout_before_kick =
			__constant_cpu_to_be32(MAX_HOST_KICK_TIMEOUT);
	data_config->eioc_min.timeout_before_kick = 0;
	data_config->eioc_max.timeout_before_kick =
			__constant_cpu_to_be32(MAX_PARAM_VALUE);

	data_config->host_min.num_recv_pool_entries_before_kick =
			__constant_cpu_to_be32(MIN_HOST_KICK_ENTRIES);
	data_config->host_max.num_recv_pool_entries_before_kick =
			__constant_cpu_to_be32(MAX_HOST_KICK_ENTRIES);
	data_config->eioc_min.num_recv_pool_entries_before_kick = 0;
	data_config->eioc_max.num_recv_pool_entries_before_kick =
				__constant_cpu_to_be32(MAX_PARAM_VALUE);

	data_config->host_min.num_recv_pool_bytes_before_kick =
			__constant_cpu_to_be32(MIN_HOST_KICK_BYTES);
	data_config->host_max.num_recv_pool_bytes_before_kick =
			__constant_cpu_to_be32(MAX_HOST_KICK_BYTES);
	data_config->eioc_min.num_recv_pool_bytes_before_kick = 0;
	data_config->eioc_max.num_recv_pool_bytes_before_kick =
				__constant_cpu_to_be32(MAX_PARAM_VALUE);

	data_config->host_min.free_recv_pool_entries_per_update =
				__constant_cpu_to_be32(MIN_HOST_UPDATE_SZ);
	data_config->host_max.free_recv_pool_entries_per_update =
				__constant_cpu_to_be32(MAX_HOST_UPDATE_SZ);
	data_config->eioc_min.free_recv_pool_entries_per_update =
				__constant_cpu_to_be32(MIN_EIOC_UPDATE_SZ);
	data_config->eioc_max.free_recv_pool_entries_per_update =
				__constant_cpu_to_be32(MAX_EIOC_UPDATE_SZ);

	data_config->notify_bundle = NOTIFY_BUNDLE_SZ;
}

static void config_path_info_defaults(struct viport_config *config,
				      struct path_param *params)
{
	int i;
	ib_query_gid(config->ibdev, config->port, 0,
			  &config->path_info.path.sgid);
	for (i = 0; i < 16; i++)
		config->path_info.path.dgid.raw[i] = params->dgid[i];

	config->path_info.path.pkey = params->pkey;
	config->path_info.path.numb_path = 1;
	config->sa_path_rec_get_timeout = sa_path_rec_get_timeout;

}

static void config_viport_defaults(struct viport_config *config,
				      struct path_param *params)
{
	config->ibdev = params->ibdev;
	config->port = params->port;
	config->ioc_guid = params->ioc_guid;
	config->stats_interval = msecs_to_jiffies(VIPORT_STATS_INTERVAL);
	config->hb_interval = msecs_to_jiffies(VIPORT_HEARTBEAT_INTERVAL);
	config->hb_timeout = VIPORT_HEARTBEAT_TIMEOUT * 1000;
				/*hb_timeout needs to be in usec*/
	strcpy(config->ioc_string, params->ioc_string);
	config_path_info_defaults(config, params);

	config_control_defaults(&config->control_config, params);
	config_data_defaults(&config->data_config, params);
	config->path_info.path.service_id = config->control_config.ib_config.service_id;
}

static void config_vnic_defaults(struct vnic_config *config)
{
	config->no_path_timeout = msecs_to_jiffies(default_no_path_timeout);
	config->primary_connect_timeout =
	    msecs_to_jiffies(DEFAULT_PRIMARY_CONNECT_TIMEOUT);
	config->primary_reconnect_timeout =
	    msecs_to_jiffies(default_primary_reconnect_timeout);
	config->primary_switch_timeout =
	    msecs_to_jiffies(default_primary_switch_timeout);
	config->prefer_primary = default_prefer_primary;
	config->use_rx_csum = use_rx_csum;
	config->use_tx_csum = use_tx_csum;
}

struct viport_config *config_alloc_viport(struct path_param *params)
{
	struct viport_config *config;

	config = kzalloc(sizeof *config, GFP_KERNEL);
	if (!config) {
		CONFIG_ERROR("could not allocate memory for"
			     " struct viport_config\n");
		return NULL;
	}

	config_viport_defaults(config, params);

	return config;
}

struct vnic_config *config_alloc_vnic(void)
{
	struct vnic_config *config;

	config = kzalloc(sizeof *config, GFP_KERNEL);
	if (!config) {
		CONFIG_ERROR("couldn't allocate memory for"
			     " struct vnic_config\n");

		return NULL;
	}

	config_vnic_defaults(config);
	return config;
}

char *config_viport_name(struct viport_config *config)
{
	/* function only called by one thread, can return a static string */
	static char str[64];

	sprintf(str, "GUID %llx instance %d",
		be64_to_cpu(config->ioc_guid),
		config->control_config.vnic_instance);
	return str;
}

int config_start(void)
{
	vnic_max_mtu = min_t(u16, vnic_max_mtu, MAX_MTU);
	vnic_max_mtu = max_t(u16, vnic_max_mtu, MIN_MTU);

	sa_path_rec_get_timeout = min_t(u32, sa_path_rec_get_timeout,
					MAX_SA_TIMEOUT);
	sa_path_rec_get_timeout = max_t(u32, sa_path_rec_get_timeout,
					MIN_SA_TIMEOUT);

	control_response_timeout = min_t(u32, control_response_timeout,
					 MAX_CONTROL_RSP_TIMEOUT);

	control_response_timeout = max_t(u32, control_response_timeout,
					 MIN_CONTROL_RSP_TIMEOUT);

	completion_limit	 = max_t(u32, completion_limit,
					 MIN_COMPLETION_LIMIT);

	if (!default_no_path_timeout)
		default_no_path_timeout = DEFAULT_NO_PATH_TIMEOUT;

	if (!default_primary_reconnect_timeout)
		default_primary_reconnect_timeout =
					 DEFAULT_PRIMARY_RECONNECT_TIMEOUT;

	if (!default_primary_switch_timeout)
		default_primary_switch_timeout =
					DEFAULT_PRIMARY_SWITCH_TIMEOUT;

	return 0;

}
