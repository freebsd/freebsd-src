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

#ifndef VNIC_CONFIG_H_INCLUDED
#define VNIC_CONFIG_H_INCLUDED

#include <rdma/ib_verbs.h>
#include <linux/types.h>
#include <linux/if.h>

#include "vnic_control.h"
#include "vnic_ib.h"

#define SST_AGN         0x10ULL
#define SST_OUI         0x00066AULL

enum {
	CONTROL_PATH_ID = 0x0,
	DATA_PATH_ID    = 0x1
};

#define IOC_NUMBER(GUID)        (((GUID) >> 32) & 0xFF)

enum {
	VNIC_CLASS_SUBCLASS	= 0x2000066A,
	VNIC_PROTOCOL		= 0,
	VNIC_PROT_VERSION	= 1
};

enum {
	MIN_MTU	= 1500,	/* minimum negotiated MTU size */
	MAX_MTU	= 9500	/* jumbo frame */
};

/*
 * TODO: tune the pool parameter values
 */
enum {
	MIN_ADDRESS_ENTRIES = 16,
	MAX_ADDRESS_ENTRIES = 64
};

enum {
	HOST_RECV_POOL_ENTRIES	= 512,
	MIN_HOST_POOL_SZ	= 64,
	MIN_EIOC_POOL_SZ	= 64,
	MAX_EIOC_POOL_SZ	= 256,
	MIN_HOST_UPDATE_SZ	= 8,
	MAX_HOST_UPDATE_SZ	= 32,
	MIN_EIOC_UPDATE_SZ	= 8,
	MAX_EIOC_UPDATE_SZ	= 32,
	NOTIFY_BUNDLE_SZ	= 32
};

enum {
	MIN_HOST_KICK_TIMEOUT = 10,	/* in usec */
	MAX_HOST_KICK_TIMEOUT = 100	/* in usec */
};

enum {
	MIN_HOST_KICK_ENTRIES = 1,
	MAX_HOST_KICK_ENTRIES = 128
};

enum {
	MIN_HOST_KICK_BYTES = 0,
	MAX_HOST_KICK_BYTES = 5000
};

enum {
	DEFAULT_NO_PATH_TIMEOUT			= 10000,
	DEFAULT_PRIMARY_CONNECT_TIMEOUT		= 10000,
	DEFAULT_PRIMARY_RECONNECT_TIMEOUT	= 10000,
	DEFAULT_PRIMARY_SWITCH_TIMEOUT		= 10000
};

enum {
	VIPORT_STATS_INTERVAL		= 500,	/* .5 sec */
	VIPORT_HEARTBEAT_INTERVAL	= 1000,	/* 1 second */
	VIPORT_HEARTBEAT_TIMEOUT	= 64000	/* 64 sec */
};

enum {
	/* 5 sec increased for EVIC support for large number of
	 * host connections
	 */
	CONTROL_RSP_TIMEOUT		= 5000,
	MIN_CONTROL_RSP_TIMEOUT		= 1000,	/* 1  sec */
	MAX_CONTROL_RSP_TIMEOUT		= 60000	/* 60 sec */
};

/* Maximum number of completions to be processed
 * during a single completion callback invocation
 */
enum {
	DEFAULT_COMPLETION_LIMIT 	= 100,
	MIN_COMPLETION_LIMIT		= 10
};

/* infiniband connection parameters */
enum {
	RETRY_COUNT		= 3,
	MIN_RNR_TIMER		= 22,	/* 20 ms */
	DEFAULT_PKEY		= 0	/* pkey table index */
};

enum {
	SA_PATH_REC_GET_TIMEOUT	= 1000,	/* 1000 ms */
	MIN_SA_TIMEOUT		= 100,	/* 100 ms */
	MAX_SA_TIMEOUT		= 20000	/* 20s */
};

#define MAX_PARAM_VALUE                 0x40000000
#define VNIC_USE_RX_CSUM		1
#define VNIC_USE_TX_CSUM		1
#define	DEFAULT_PREFER_PRIMARY		0

/* As per IBTA specification, IOCString Maximum length can be 512 bits. */
#define MAX_IOC_STRING_LEN 		(512/8)

struct path_param {
	__be64			ioc_guid;
	u8			ioc_string[MAX_IOC_STRING_LEN+1];
	u8			port;
	u8			instance;
	struct ib_device	*ibdev;
	struct vnic_ib_port	*ibport;
	char			name[IFNAMSIZ];
	u8			dgid[16];
	__be16			pkey;
	int			rx_csum;
	int			tx_csum;
	int			heartbeat;
	int			ib_multicast;
};

struct vnic_ib_config {
	__be64				service_id;
	struct vnic_connection_data	conn_data;
	u32				retry_count;
	u32				rnr_retry_count;
	u8				min_rnr_timer;
	u32				num_sends;
	u32				num_recvs;
	u32				recv_scatter;	/* 1 */
	u32				send_gather;	/* 1 or 2 */
	u32				completion_limit;
};

struct control_config {
	struct vnic_ib_config	ib_config;
	u32			num_recvs;
	u8			vnic_instance;
	u16			max_address_entries;
	u16			min_address_entries;
	u32			rsp_timeout;
	u32			ib_multicast;
};

struct data_config {
	struct vnic_ib_config		ib_config;
	u64				path_id;
	u32				num_recvs;
	u32				host_recv_pool_entries;
	struct vnic_recv_pool_config	host_min;
	struct vnic_recv_pool_config	host_max;
	struct vnic_recv_pool_config	eioc_min;
	struct vnic_recv_pool_config	eioc_max;
	u32				notify_bundle;
};

struct viport_config {
	struct viport			*viport;
	struct control_config		control_config;
	struct data_config		data_config;
	struct vnic_ib_path_info	path_info;
	u32				sa_path_rec_get_timeout;
	struct ib_device		*ibdev;
	u32				port;
	unsigned long			stats_interval;
	u32				hb_interval;
	u32				hb_timeout;
	__be64				ioc_guid;
	u8				ioc_string[MAX_IOC_STRING_LEN+1];
	size_t				path_idx;
};

/*
 * primary_connect_timeout   - if the secondary connects first,
 *                             how long do we give the primary?
 * primary_reconnect_timeout - same as above, but used when recovering
 *                             from the case where both paths fail
 * primary_switch_timeout -    how long do we wait before switching to the
 *                             primary when it comes back?
 */
struct vnic_config {
	struct vnic	*vnic;
	char		name[IFNAMSIZ];
	unsigned long	no_path_timeout;
	u32 		primary_connect_timeout;
	u32		primary_reconnect_timeout;
	u32		primary_switch_timeout;
	int		prefer_primary;
	int		use_rx_csum;
	int		use_tx_csum;
};

int config_start(void);
struct viport_config *config_alloc_viport(struct path_param *params);
struct vnic_config   *config_alloc_vnic(void);
char *config_viport_name(struct viport_config *config);

#endif	/* VNIC_CONFIG_H_INCLUDED */
