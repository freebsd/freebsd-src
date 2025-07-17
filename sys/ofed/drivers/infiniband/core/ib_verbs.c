/*-
 * SPDX-License-Identifier: BSD-2-Clause OR GPL-2.0
 *
 * Copyright (c) 2004 Mellanox Technologies Ltd.  All rights reserved.
 * Copyright (c) 2004 Infinicon Corporation.  All rights reserved.
 * Copyright (c) 2004 Intel Corporation.  All rights reserved.
 * Copyright (c) 2004 Topspin Corporation.  All rights reserved.
 * Copyright (c) 2004 Voltaire Corporation.  All rights reserved.
 * Copyright (c) 2005 Sun Microsystems, Inc. All rights reserved.
 * Copyright (c) 2005, 2006 Cisco Systems.  All rights reserved.
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

#include <sys/cdefs.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/wait.h>

#include <rdma/ib_verbs.h>
#include <rdma/ib_cache.h>
#include <rdma/ib_addr.h>

#include <netinet/ip.h>
#include <netinet/ip6.h>

#include <machine/in_cksum.h>

#include "core_priv.h"

static const char * const ib_events[] = {
	[IB_EVENT_CQ_ERR]		= "CQ error",
	[IB_EVENT_QP_FATAL]		= "QP fatal error",
	[IB_EVENT_QP_REQ_ERR]		= "QP request error",
	[IB_EVENT_QP_ACCESS_ERR]	= "QP access error",
	[IB_EVENT_COMM_EST]		= "communication established",
	[IB_EVENT_SQ_DRAINED]		= "send queue drained",
	[IB_EVENT_PATH_MIG]		= "path migration successful",
	[IB_EVENT_PATH_MIG_ERR]		= "path migration error",
	[IB_EVENT_DEVICE_FATAL]		= "device fatal error",
	[IB_EVENT_PORT_ACTIVE]		= "port active",
	[IB_EVENT_PORT_ERR]		= "port error",
	[IB_EVENT_LID_CHANGE]		= "LID change",
	[IB_EVENT_PKEY_CHANGE]		= "P_key change",
	[IB_EVENT_SM_CHANGE]		= "SM change",
	[IB_EVENT_SRQ_ERR]		= "SRQ error",
	[IB_EVENT_SRQ_LIMIT_REACHED]	= "SRQ limit reached",
	[IB_EVENT_QP_LAST_WQE_REACHED]	= "last WQE reached",
	[IB_EVENT_CLIENT_REREGISTER]	= "client reregister",
	[IB_EVENT_GID_CHANGE]		= "GID changed",
};

const char *__attribute_const__ ib_event_msg(enum ib_event_type event)
{
	size_t index = event;

	return (index < ARRAY_SIZE(ib_events) && ib_events[index]) ?
			ib_events[index] : "unrecognized event";
}
EXPORT_SYMBOL(ib_event_msg);

static const char * const wc_statuses[] = {
	[IB_WC_SUCCESS]			= "success",
	[IB_WC_LOC_LEN_ERR]		= "local length error",
	[IB_WC_LOC_QP_OP_ERR]		= "local QP operation error",
	[IB_WC_LOC_EEC_OP_ERR]		= "local EE context operation error",
	[IB_WC_LOC_PROT_ERR]		= "local protection error",
	[IB_WC_WR_FLUSH_ERR]		= "WR flushed",
	[IB_WC_MW_BIND_ERR]		= "memory management operation error",
	[IB_WC_BAD_RESP_ERR]		= "bad response error",
	[IB_WC_LOC_ACCESS_ERR]		= "local access error",
	[IB_WC_REM_INV_REQ_ERR]		= "invalid request error",
	[IB_WC_REM_ACCESS_ERR]		= "remote access error",
	[IB_WC_REM_OP_ERR]		= "remote operation error",
	[IB_WC_RETRY_EXC_ERR]		= "transport retry counter exceeded",
	[IB_WC_RNR_RETRY_EXC_ERR]	= "RNR retry counter exceeded",
	[IB_WC_LOC_RDD_VIOL_ERR]	= "local RDD violation error",
	[IB_WC_REM_INV_RD_REQ_ERR]	= "remote invalid RD request",
	[IB_WC_REM_ABORT_ERR]		= "operation aborted",
	[IB_WC_INV_EECN_ERR]		= "invalid EE context number",
	[IB_WC_INV_EEC_STATE_ERR]	= "invalid EE context state",
	[IB_WC_FATAL_ERR]		= "fatal error",
	[IB_WC_RESP_TIMEOUT_ERR]	= "response timeout error",
	[IB_WC_GENERAL_ERR]		= "general error",
};

const char *__attribute_const__ ib_wc_status_msg(enum ib_wc_status status)
{
	size_t index = status;

	return (index < ARRAY_SIZE(wc_statuses) && wc_statuses[index]) ?
			wc_statuses[index] : "unrecognized status";
}
EXPORT_SYMBOL(ib_wc_status_msg);

__attribute_const__ int ib_rate_to_mult(enum ib_rate rate)
{
	switch (rate) {
	case IB_RATE_2_5_GBPS: return   1;
	case IB_RATE_5_GBPS:   return   2;
	case IB_RATE_10_GBPS:  return   4;
	case IB_RATE_20_GBPS:  return   8;
	case IB_RATE_30_GBPS:  return  12;
	case IB_RATE_40_GBPS:  return  16;
	case IB_RATE_60_GBPS:  return  24;
	case IB_RATE_80_GBPS:  return  32;
	case IB_RATE_120_GBPS: return  48;
	case IB_RATE_14_GBPS:  return   6;
	case IB_RATE_56_GBPS:  return  22;
	case IB_RATE_112_GBPS: return  45;
	case IB_RATE_168_GBPS: return  67;
	case IB_RATE_25_GBPS:  return  10;
	case IB_RATE_100_GBPS: return  40;
	case IB_RATE_200_GBPS: return  80;
	case IB_RATE_300_GBPS: return 120;
	case IB_RATE_28_GBPS:  return  11;
	case IB_RATE_50_GBPS:  return  20;
	case IB_RATE_400_GBPS: return 160;
	case IB_RATE_600_GBPS: return 240;
	default:	       return  -1;
	}
}
EXPORT_SYMBOL(ib_rate_to_mult);

__attribute_const__ enum ib_rate mult_to_ib_rate(int mult)
{
	switch (mult) {
	case 1:   return IB_RATE_2_5_GBPS;
	case 2:   return IB_RATE_5_GBPS;
	case 4:   return IB_RATE_10_GBPS;
	case 8:   return IB_RATE_20_GBPS;
	case 12:  return IB_RATE_30_GBPS;
	case 16:  return IB_RATE_40_GBPS;
	case 24:  return IB_RATE_60_GBPS;
	case 32:  return IB_RATE_80_GBPS;
	case 48:  return IB_RATE_120_GBPS;
	case 6:   return IB_RATE_14_GBPS;
	case 22:  return IB_RATE_56_GBPS;
	case 45:  return IB_RATE_112_GBPS;
	case 67:  return IB_RATE_168_GBPS;
	case 10:  return IB_RATE_25_GBPS;
	case 40:  return IB_RATE_100_GBPS;
	case 80:  return IB_RATE_200_GBPS;
	case 120: return IB_RATE_300_GBPS;
	case 11:  return IB_RATE_28_GBPS;
	case 20:  return IB_RATE_50_GBPS;
	case 160: return IB_RATE_400_GBPS;
	case 240: return IB_RATE_600_GBPS;
	default:  return IB_RATE_PORT_CURRENT;
	}
}
EXPORT_SYMBOL(mult_to_ib_rate);

__attribute_const__ int ib_rate_to_mbps(enum ib_rate rate)
{
	switch (rate) {
	case IB_RATE_2_5_GBPS: return 2500;
	case IB_RATE_5_GBPS:   return 5000;
	case IB_RATE_10_GBPS:  return 10000;
	case IB_RATE_20_GBPS:  return 20000;
	case IB_RATE_30_GBPS:  return 30000;
	case IB_RATE_40_GBPS:  return 40000;
	case IB_RATE_60_GBPS:  return 60000;
	case IB_RATE_80_GBPS:  return 80000;
	case IB_RATE_120_GBPS: return 120000;
	case IB_RATE_14_GBPS:  return 14062;
	case IB_RATE_56_GBPS:  return 56250;
	case IB_RATE_112_GBPS: return 112500;
	case IB_RATE_168_GBPS: return 168750;
	case IB_RATE_25_GBPS:  return 25781;
	case IB_RATE_100_GBPS: return 103125;
	case IB_RATE_200_GBPS: return 206250;
	case IB_RATE_300_GBPS: return 309375;
	case IB_RATE_28_GBPS:  return 28125;
	case IB_RATE_50_GBPS:  return 53125;
	case IB_RATE_400_GBPS: return 425000;
	case IB_RATE_600_GBPS: return 637500;
	default:	       return -1;
	}
}
EXPORT_SYMBOL(ib_rate_to_mbps);

__attribute_const__ enum rdma_transport_type
rdma_node_get_transport(enum rdma_node_type node_type)
{
	switch (node_type) {
	case RDMA_NODE_IB_CA:
	case RDMA_NODE_IB_SWITCH:
	case RDMA_NODE_IB_ROUTER:
		return RDMA_TRANSPORT_IB;
	case RDMA_NODE_RNIC:
		return RDMA_TRANSPORT_IWARP;
	case RDMA_NODE_USNIC:
		return RDMA_TRANSPORT_USNIC;
	case RDMA_NODE_USNIC_UDP:
		return RDMA_TRANSPORT_USNIC_UDP;
	default:
		BUG();
		return 0;
	}
}
EXPORT_SYMBOL(rdma_node_get_transport);

enum rdma_link_layer rdma_port_get_link_layer(struct ib_device *device, u8 port_num)
{
	if (device->get_link_layer)
		return device->get_link_layer(device, port_num);

	switch (rdma_node_get_transport(device->node_type)) {
	case RDMA_TRANSPORT_IB:
		return IB_LINK_LAYER_INFINIBAND;
	case RDMA_TRANSPORT_IWARP:
	case RDMA_TRANSPORT_USNIC:
	case RDMA_TRANSPORT_USNIC_UDP:
		return IB_LINK_LAYER_ETHERNET;
	default:
		return IB_LINK_LAYER_UNSPECIFIED;
	}
}
EXPORT_SYMBOL(rdma_port_get_link_layer);

/* Protection domains */

/**
 * ib_alloc_pd - Allocates an unused protection domain.
 * @device: The device on which to allocate the protection domain.
 *
 * A protection domain object provides an association between QPs, shared
 * receive queues, address handles, memory regions, and memory windows.
 *
 * Every PD has a local_dma_lkey which can be used as the lkey value for local
 * memory operations.
 */
struct ib_pd *__ib_alloc_pd(struct ib_device *device, unsigned int flags,
		const char *caller)
{
	struct ib_pd *pd;
	int mr_access_flags = 0;
	int ret;

	pd = rdma_zalloc_drv_obj(device, ib_pd);
	if (!pd)
		return ERR_PTR(-ENOMEM);

	pd->device = device;
	pd->uobject = NULL;
	pd->__internal_mr = NULL;
	atomic_set(&pd->usecnt, 0);
	pd->flags = flags;

	ret = device->alloc_pd(pd, NULL);
	if (ret) {
		kfree(pd);
		return ERR_PTR(ret);
	}

	if (device->attrs.device_cap_flags & IB_DEVICE_LOCAL_DMA_LKEY)
		pd->local_dma_lkey = device->local_dma_lkey;
	else
		mr_access_flags |= IB_ACCESS_LOCAL_WRITE;

	if (flags & IB_PD_UNSAFE_GLOBAL_RKEY) {
		pr_warn("%s: enabling unsafe global rkey\n", caller);
		mr_access_flags |= IB_ACCESS_REMOTE_READ | IB_ACCESS_REMOTE_WRITE;
	}

	if (mr_access_flags) {
		struct ib_mr *mr;

		mr = pd->device->get_dma_mr(pd, mr_access_flags);
		if (IS_ERR(mr)) {
			ib_dealloc_pd(pd);
			return ERR_CAST(mr);
		}

		mr->device	= pd->device;
		mr->pd		= pd;
		mr->type        = IB_MR_TYPE_DMA;
		mr->uobject	= NULL;
		mr->need_inval	= false;

		pd->__internal_mr = mr;

		if (!(device->attrs.device_cap_flags & IB_DEVICE_LOCAL_DMA_LKEY))
			pd->local_dma_lkey = pd->__internal_mr->lkey;

		if (flags & IB_PD_UNSAFE_GLOBAL_RKEY)
			pd->unsafe_global_rkey = pd->__internal_mr->rkey;
	}

	return pd;
}
EXPORT_SYMBOL(__ib_alloc_pd);

/**
 * ib_dealloc_pd_user - Deallocates a protection domain.
 * @pd: The protection domain to deallocate.
 * @udata: Valid user data or NULL for kernel object
 *
 * It is an error to call this function while any resources in the pd still
 * exist.  The caller is responsible to synchronously destroy them and
 * guarantee no new allocations will happen.
 */
void ib_dealloc_pd_user(struct ib_pd *pd, struct ib_udata *udata)
{
	int ret;

	if (pd->__internal_mr) {
		ret = pd->device->dereg_mr(pd->__internal_mr, NULL);
		WARN_ON(ret);
		pd->__internal_mr = NULL;
	}

	/* uverbs manipulates usecnt with proper locking, while the kabi
	   requires the caller to guarantee we can't race here. */
	WARN_ON(atomic_read(&pd->usecnt));

	pd->device->dealloc_pd(pd, udata);
	kfree(pd);
}
EXPORT_SYMBOL(ib_dealloc_pd_user);

/* Address handles */

static struct ib_ah *_ib_create_ah(struct ib_pd *pd,
				     struct ib_ah_attr *ah_attr,
				     u32 flags,
				     struct ib_udata *udata)
{
	struct ib_device *device = pd->device;
	struct ib_ah *ah;
	int ret;

	might_sleep_if(flags & RDMA_CREATE_AH_SLEEPABLE);

	if (!device->create_ah)
		return ERR_PTR(-EOPNOTSUPP);

	ah = rdma_zalloc_drv_obj_gfp(
		device, ib_ah,
		(flags & RDMA_CREATE_AH_SLEEPABLE) ? GFP_KERNEL : GFP_ATOMIC);
	if (!ah)
		return ERR_PTR(-ENOMEM);

	ah->device = device;
	ah->pd = pd;

	ret = device->create_ah(ah, ah_attr, flags, udata);
	if (ret) {
		kfree(ah);
		return ERR_PTR(ret);
	}

	atomic_inc(&pd->usecnt);
	return ah;
}

/**
 * rdma_create_ah - Creates an address handle for the
 * given address vector.
 * @pd: The protection domain associated with the address handle.
 * @ah_attr: The attributes of the address vector.
 * @flags: Create address handle flags (see enum rdma_create_ah_flags).
 *
 * It returns 0 on success and returns appropriate error code on error.
 * The address handle is used to reference a local or global destination
 * in all UD QP post sends.
 */
struct ib_ah *ib_create_ah(struct ib_pd *pd, struct ib_ah_attr *ah_attr,
			   u32 flags)
{
	struct ib_ah *ah;

	ah = _ib_create_ah(pd, ah_attr, flags, NULL);

	return ah;
}
EXPORT_SYMBOL(ib_create_ah);

/**
 * ib_create_user_ah - Creates an address handle for the
 * given address vector.
 * It resolves destination mac address for ah attribute of RoCE type.
 * @pd: The protection domain associated with the address handle.
 * @ah_attr: The attributes of the address vector.
 * @udata: pointer to user's input output buffer information need by
 *         provider driver.
 *
 * It returns a valid address handle pointer on success and
 * returns appropriate error code on error.
 * The address handle is used to reference a local or global destination
 * in all UD QP post sends.
 */
struct ib_ah *ib_create_user_ah(struct ib_pd *pd,
				struct ib_ah_attr *ah_attr,
				struct ib_udata *udata)
{
	int err;

	if (rdma_protocol_roce(pd->device, ah_attr->port_num)) {
		err = ib_resolve_eth_dmac(pd->device, ah_attr);
		if (err)
			return ERR_PTR(err);
	}

	return _ib_create_ah(pd, ah_attr, RDMA_CREATE_AH_SLEEPABLE, udata);
}
EXPORT_SYMBOL(ib_create_user_ah);

static int ib_get_header_version(const union rdma_network_hdr *hdr)
{
	const struct ip *ip4h = (const struct ip *)&hdr->roce4grh;
	struct ip ip4h_checked;
	const struct ip6_hdr *ip6h = (const struct ip6_hdr *)&hdr->ibgrh;

	/* If it's IPv6, the version must be 6, otherwise, the first
	 * 20 bytes (before the IPv4 header) are garbled.
	 */
	if ((ip6h->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION)
		return (ip4h->ip_v == 4) ? 4 : 0;
	/* version may be 6 or 4 because the first 20 bytes could be garbled */

	/* RoCE v2 requires no options, thus header length
	 * must be 5 words
	 */
	if (ip4h->ip_hl != 5)
		return 6;

	/* Verify checksum.
	 * We can't write on scattered buffers so we need to copy to
	 * temp buffer.
	 */
	memcpy(&ip4h_checked, ip4h, sizeof(ip4h_checked));
	ip4h_checked.ip_sum = 0;
#if defined(INET) || defined(INET6)
	ip4h_checked.ip_sum = in_cksum_hdr(&ip4h_checked);
#endif
	/* if IPv4 header checksum is OK, believe it */
	if (ip4h->ip_sum == ip4h_checked.ip_sum)
		return 4;
	return 6;
}

static enum rdma_network_type ib_get_net_type_by_grh(struct ib_device *device,
						     u8 port_num,
						     const struct ib_grh *grh)
{
	int grh_version;

	if (rdma_protocol_ib(device, port_num))
		return RDMA_NETWORK_IB;

	grh_version = ib_get_header_version((const union rdma_network_hdr *)grh);

	if (grh_version == 4)
		return RDMA_NETWORK_IPV4;

	if (grh->next_hdr == IPPROTO_UDP)
		return RDMA_NETWORK_IPV6;

	return RDMA_NETWORK_ROCE_V1;
}

struct find_gid_index_context {
	u16 vlan_id;
	enum ib_gid_type gid_type;
};


/*
 * This function will return true only if a inspected GID index
 * matches the request based on the GID type and VLAN configuration
 */
static bool find_gid_index(const union ib_gid *gid,
			   const struct ib_gid_attr *gid_attr,
			   void *context)
{
	u16 vlan_diff;
	struct find_gid_index_context *ctx =
		(struct find_gid_index_context *)context;

	if (ctx->gid_type != gid_attr->gid_type)
		return false;

	/*
	 * The following will verify:
	 * 1. VLAN ID matching for VLAN tagged requests.
	 * 2. prio-tagged/untagged to prio-tagged/untagged matching.
	 *
	 * This XOR is valid, since 0x0 < vlan_id < 0x0FFF.
	 */
	vlan_diff = rdma_vlan_dev_vlan_id(gid_attr->ndev) ^ ctx->vlan_id;

	return (vlan_diff == 0x0000 || vlan_diff == 0xFFFF);
}

static int get_sgid_index_from_eth(struct ib_device *device, u8 port_num,
				   u16 vlan_id, const union ib_gid *sgid,
				   enum ib_gid_type gid_type,
				   u16 *gid_index)
{
	struct find_gid_index_context context = {.vlan_id = vlan_id,
						 .gid_type = gid_type};

	return ib_find_gid_by_filter(device, sgid, port_num, find_gid_index,
				     &context, gid_index);
}

static int get_gids_from_rdma_hdr(const union rdma_network_hdr *hdr,
				  enum rdma_network_type net_type,
				  union ib_gid *sgid, union ib_gid *dgid)
{
	struct sockaddr_in  src_in;
	struct sockaddr_in  dst_in;
	__be32 src_saddr, dst_saddr;

	if (!sgid || !dgid)
		return -EINVAL;

	if (net_type == RDMA_NETWORK_IPV4) {
		memcpy(&src_in.sin_addr.s_addr,
		       &hdr->roce4grh.ip_src, 4);
		memcpy(&dst_in.sin_addr.s_addr,
		       &hdr->roce4grh.ip_dst, 4);
		src_saddr = src_in.sin_addr.s_addr;
		dst_saddr = dst_in.sin_addr.s_addr;
		ipv6_addr_set_v4mapped(src_saddr,
				       (struct in6_addr *)sgid);
		ipv6_addr_set_v4mapped(dst_saddr,
				       (struct in6_addr *)dgid);
		return 0;
	} else if (net_type == RDMA_NETWORK_IPV6 ||
		   net_type == RDMA_NETWORK_IB) {
		*dgid = hdr->ibgrh.dgid;
		*sgid = hdr->ibgrh.sgid;
		return 0;
	} else {
		return -EINVAL;
	}
}

int ib_init_ah_from_wc(struct ib_device *device, u8 port_num,
		       const struct ib_wc *wc, const struct ib_grh *grh,
		       struct ib_ah_attr *ah_attr)
{
	u32 flow_class;
	u16 gid_index = 0;
	int ret;
	enum rdma_network_type net_type = RDMA_NETWORK_IB;
	enum ib_gid_type gid_type = IB_GID_TYPE_IB;
	int hoplimit = 0xff;
	union ib_gid dgid;
	union ib_gid sgid;

	memset(ah_attr, 0, sizeof *ah_attr);
	if (rdma_cap_eth_ah(device, port_num)) {
		if (wc->wc_flags & IB_WC_WITH_NETWORK_HDR_TYPE)
			net_type = wc->network_hdr_type;
		else
			net_type = ib_get_net_type_by_grh(device, port_num, grh);
		gid_type = ib_network_to_gid_type(net_type);
	}
	ret = get_gids_from_rdma_hdr((const union rdma_network_hdr *)grh, net_type,
				     &sgid, &dgid);
	if (ret)
		return ret;

	if (rdma_protocol_roce(device, port_num)) {
		struct ib_gid_attr dgid_attr;
		const u16 vlan_id = (wc->wc_flags & IB_WC_WITH_VLAN) ?
				wc->vlan_id : 0xffff;

		if (!(wc->wc_flags & IB_WC_GRH))
			return -EPROTOTYPE;

		ret = get_sgid_index_from_eth(device, port_num, vlan_id,
					      &dgid, gid_type, &gid_index);
		if (ret)
			return ret;

		ret = ib_get_cached_gid(device, port_num, gid_index, &dgid, &dgid_attr);
		if (ret)
			return ret;

		if (dgid_attr.ndev == NULL)
			return -ENODEV;

		ret = rdma_addr_find_l2_eth_by_grh(&dgid, &sgid, ah_attr->dmac,
		    dgid_attr.ndev, &hoplimit);

		dev_put(dgid_attr.ndev);
		if (ret)
			return ret;
	}

	ah_attr->dlid = wc->slid;
	ah_attr->sl = wc->sl;
	ah_attr->src_path_bits = wc->dlid_path_bits;
	ah_attr->port_num = port_num;

	if (wc->wc_flags & IB_WC_GRH) {
		ah_attr->ah_flags = IB_AH_GRH;
		ah_attr->grh.dgid = sgid;

		if (!rdma_cap_eth_ah(device, port_num)) {
			if (dgid.global.interface_id != cpu_to_be64(IB_SA_WELL_KNOWN_GUID)) {
				ret = ib_find_cached_gid_by_port(device, &dgid,
								 IB_GID_TYPE_IB,
								 port_num, NULL,
								 &gid_index);
				if (ret)
					return ret;
			}
		}

		ah_attr->grh.sgid_index = (u8) gid_index;
		flow_class = be32_to_cpu(grh->version_tclass_flow);
		ah_attr->grh.flow_label = flow_class & 0xFFFFF;
		ah_attr->grh.hop_limit = hoplimit;
		ah_attr->grh.traffic_class = (flow_class >> 20) & 0xFF;
	}
	return 0;
}
EXPORT_SYMBOL(ib_init_ah_from_wc);

struct ib_ah *ib_create_ah_from_wc(struct ib_pd *pd, const struct ib_wc *wc,
				   const struct ib_grh *grh, u8 port_num)
{
	struct ib_ah_attr ah_attr;
	int ret;

	ret = ib_init_ah_from_wc(pd->device, port_num, wc, grh, &ah_attr);
	if (ret)
		return ERR_PTR(ret);

	return ib_create_ah(pd, &ah_attr, RDMA_CREATE_AH_SLEEPABLE);
}
EXPORT_SYMBOL(ib_create_ah_from_wc);

int ib_modify_ah(struct ib_ah *ah, struct ib_ah_attr *ah_attr)
{
	return ah->device->modify_ah ?
		ah->device->modify_ah(ah, ah_attr) :
		-ENOSYS;
}
EXPORT_SYMBOL(ib_modify_ah);

int ib_query_ah(struct ib_ah *ah, struct ib_ah_attr *ah_attr)
{
	return ah->device->query_ah ?
		ah->device->query_ah(ah, ah_attr) :
		-ENOSYS;
}
EXPORT_SYMBOL(ib_query_ah);

int ib_destroy_ah_user(struct ib_ah *ah, u32 flags, struct ib_udata *udata)
{
	struct ib_pd *pd;

	might_sleep_if(flags & RDMA_DESTROY_AH_SLEEPABLE);

	pd = ah->pd;
	ah->device->destroy_ah(ah, flags);
	atomic_dec(&pd->usecnt);

	kfree(ah);
	return 0;
}
EXPORT_SYMBOL(ib_destroy_ah_user);

/* Shared receive queues */

struct ib_srq *ib_create_srq(struct ib_pd *pd,
			     struct ib_srq_init_attr *srq_init_attr)
{
	struct ib_srq *srq;
	int ret;

	if (!pd->device->create_srq)
		return ERR_PTR(-EOPNOTSUPP);

	srq = rdma_zalloc_drv_obj(pd->device, ib_srq);
	if (!srq)
		return ERR_PTR(-ENOMEM);

	srq->device = pd->device;
	srq->pd = pd;
	srq->event_handler = srq_init_attr->event_handler;
	srq->srq_context = srq_init_attr->srq_context;
	srq->srq_type = srq_init_attr->srq_type;

	if (ib_srq_has_cq(srq->srq_type)) {
		srq->ext.cq = srq_init_attr->ext.cq;
		atomic_inc(&srq->ext.cq->usecnt);
	}
	if (srq->srq_type == IB_SRQT_XRC) {
		srq->ext.xrc.xrcd = srq_init_attr->ext.xrc.xrcd;
		atomic_inc(&srq->ext.xrc.xrcd->usecnt);
	}
	atomic_inc(&pd->usecnt);

	ret = pd->device->create_srq(srq, srq_init_attr, NULL);
	if (ret) {
		atomic_dec(&srq->pd->usecnt);
		if (srq->srq_type == IB_SRQT_XRC)
			atomic_dec(&srq->ext.xrc.xrcd->usecnt);
		if (ib_srq_has_cq(srq->srq_type))
			atomic_dec(&srq->ext.cq->usecnt);
		kfree(srq);
		return ERR_PTR(ret);
	}

	return srq;
}
EXPORT_SYMBOL(ib_create_srq);

int ib_modify_srq(struct ib_srq *srq,
		  struct ib_srq_attr *srq_attr,
		  enum ib_srq_attr_mask srq_attr_mask)
{
	return srq->device->modify_srq ?
		srq->device->modify_srq(srq, srq_attr, srq_attr_mask, NULL) :
		-ENOSYS;
}
EXPORT_SYMBOL(ib_modify_srq);

int ib_query_srq(struct ib_srq *srq,
		 struct ib_srq_attr *srq_attr)
{
	return srq->device->query_srq ?
		srq->device->query_srq(srq, srq_attr) : -ENOSYS;
}
EXPORT_SYMBOL(ib_query_srq);

int ib_destroy_srq_user(struct ib_srq *srq, struct ib_udata *udata)
{
	if (atomic_read(&srq->usecnt))
		return -EBUSY;

	srq->device->destroy_srq(srq, udata);

	atomic_dec(&srq->pd->usecnt);
	if (srq->srq_type == IB_SRQT_XRC)
		atomic_dec(&srq->ext.xrc.xrcd->usecnt);
	if (ib_srq_has_cq(srq->srq_type))
		atomic_dec(&srq->ext.cq->usecnt);
	kfree(srq);

	return 0;
}
EXPORT_SYMBOL(ib_destroy_srq_user);

/* Queue pairs */

static void __ib_shared_qp_event_handler(struct ib_event *event, void *context)
{
	struct ib_qp *qp = context;
	unsigned long flags;

	spin_lock_irqsave(&qp->device->event_handler_lock, flags);
	list_for_each_entry(event->element.qp, &qp->open_list, open_list)
		if (event->element.qp->event_handler)
			event->element.qp->event_handler(event, event->element.qp->qp_context);
	spin_unlock_irqrestore(&qp->device->event_handler_lock, flags);
}

static void __ib_insert_xrcd_qp(struct ib_xrcd *xrcd, struct ib_qp *qp)
{
	mutex_lock(&xrcd->tgt_qp_mutex);
	list_add(&qp->xrcd_list, &xrcd->tgt_qp_list);
	mutex_unlock(&xrcd->tgt_qp_mutex);
}

static struct ib_qp *__ib_open_qp(struct ib_qp *real_qp,
				  void (*event_handler)(struct ib_event *, void *),
				  void *qp_context)
{
	struct ib_qp *qp;
	unsigned long flags;

	qp = kzalloc(sizeof *qp, GFP_KERNEL);
	if (!qp)
		return ERR_PTR(-ENOMEM);

	qp->real_qp = real_qp;
	atomic_inc(&real_qp->usecnt);
	qp->device = real_qp->device;
	qp->event_handler = event_handler;
	qp->qp_context = qp_context;
	qp->qp_num = real_qp->qp_num;
	qp->qp_type = real_qp->qp_type;

	spin_lock_irqsave(&real_qp->device->event_handler_lock, flags);
	list_add(&qp->open_list, &real_qp->open_list);
	spin_unlock_irqrestore(&real_qp->device->event_handler_lock, flags);

	return qp;
}

struct ib_qp *ib_open_qp(struct ib_xrcd *xrcd,
			 struct ib_qp_open_attr *qp_open_attr)
{
	struct ib_qp *qp, *real_qp;

	if (qp_open_attr->qp_type != IB_QPT_XRC_TGT)
		return ERR_PTR(-EINVAL);

	qp = ERR_PTR(-EINVAL);
	mutex_lock(&xrcd->tgt_qp_mutex);
	list_for_each_entry(real_qp, &xrcd->tgt_qp_list, xrcd_list) {
		if (real_qp->qp_num == qp_open_attr->qp_num) {
			qp = __ib_open_qp(real_qp, qp_open_attr->event_handler,
					  qp_open_attr->qp_context);
			break;
		}
	}
	mutex_unlock(&xrcd->tgt_qp_mutex);
	return qp;
}
EXPORT_SYMBOL(ib_open_qp);

static struct ib_qp *ib_create_xrc_qp(struct ib_qp *qp,
		struct ib_qp_init_attr *qp_init_attr)
{
	struct ib_qp *real_qp = qp;

	qp->event_handler = __ib_shared_qp_event_handler;
	qp->qp_context = qp;
	qp->pd = NULL;
	qp->send_cq = qp->recv_cq = NULL;
	qp->srq = NULL;
	qp->xrcd = qp_init_attr->xrcd;
	atomic_inc(&qp_init_attr->xrcd->usecnt);
	INIT_LIST_HEAD(&qp->open_list);

	qp = __ib_open_qp(real_qp, qp_init_attr->event_handler,
			  qp_init_attr->qp_context);
	if (!IS_ERR(qp))
		__ib_insert_xrcd_qp(qp_init_attr->xrcd, real_qp);
	else
		real_qp->device->destroy_qp(real_qp, NULL);
	return qp;
}

struct ib_qp *ib_create_qp(struct ib_pd *pd,
			   struct ib_qp_init_attr *qp_init_attr)
{
	struct ib_device *device = pd ? pd->device : qp_init_attr->xrcd->device;
	struct ib_qp *qp;

	if (qp_init_attr->rwq_ind_tbl &&
	    (qp_init_attr->recv_cq ||
	    qp_init_attr->srq || qp_init_attr->cap.max_recv_wr ||
	    qp_init_attr->cap.max_recv_sge))
		return ERR_PTR(-EINVAL);

	qp = _ib_create_qp(device, pd, qp_init_attr, NULL, NULL);
	if (IS_ERR(qp))
		return qp;

	qp->device     = device;
	qp->real_qp    = qp;
	qp->uobject    = NULL;
	qp->qp_type    = qp_init_attr->qp_type;
	qp->rwq_ind_tbl = qp_init_attr->rwq_ind_tbl;

	atomic_set(&qp->usecnt, 0);
	spin_lock_init(&qp->mr_lock);

	if (qp_init_attr->qp_type == IB_QPT_XRC_TGT)
		return ib_create_xrc_qp(qp, qp_init_attr);

	qp->event_handler = qp_init_attr->event_handler;
	qp->qp_context = qp_init_attr->qp_context;
	if (qp_init_attr->qp_type == IB_QPT_XRC_INI) {
		qp->recv_cq = NULL;
		qp->srq = NULL;
	} else {
		qp->recv_cq = qp_init_attr->recv_cq;
		if (qp_init_attr->recv_cq)
			atomic_inc(&qp_init_attr->recv_cq->usecnt);
		qp->srq = qp_init_attr->srq;
		if (qp->srq)
			atomic_inc(&qp_init_attr->srq->usecnt);
	}

	qp->pd	    = pd;
	qp->send_cq = qp_init_attr->send_cq;
	qp->xrcd    = NULL;

	atomic_inc(&pd->usecnt);
	if (qp_init_attr->send_cq)
		atomic_inc(&qp_init_attr->send_cq->usecnt);
	if (qp_init_attr->rwq_ind_tbl)
		atomic_inc(&qp->rwq_ind_tbl->usecnt);

	/*
	 * Note: all hw drivers guarantee that max_send_sge is lower than
	 * the device RDMA WRITE SGE limit but not all hw drivers ensure that
	 * max_send_sge <= max_sge_rd.
	 */
	qp->max_write_sge = qp_init_attr->cap.max_send_sge;
	qp->max_read_sge = min_t(u32, qp_init_attr->cap.max_send_sge,
				 device->attrs.max_sge_rd);

	return qp;
}
EXPORT_SYMBOL(ib_create_qp);

static const struct {
	int			valid;
	enum ib_qp_attr_mask	req_param[IB_QPT_MAX];
	enum ib_qp_attr_mask	opt_param[IB_QPT_MAX];
} qp_state_table[IB_QPS_ERR + 1][IB_QPS_ERR + 1] = {
	[IB_QPS_RESET] = {
		[IB_QPS_RESET] = { .valid = 1 },
		[IB_QPS_INIT]  = {
			.valid = 1,
			.req_param = {
				[IB_QPT_UD]  = (IB_QP_PKEY_INDEX		|
						IB_QP_PORT			|
						IB_QP_QKEY),
				[IB_QPT_RAW_PACKET] = IB_QP_PORT,
				[IB_QPT_UC]  = (IB_QP_PKEY_INDEX		|
						IB_QP_PORT			|
						IB_QP_ACCESS_FLAGS),
				[IB_QPT_RC]  = (IB_QP_PKEY_INDEX		|
						IB_QP_PORT			|
						IB_QP_ACCESS_FLAGS),
				[IB_QPT_XRC_INI] = (IB_QP_PKEY_INDEX		|
						IB_QP_PORT			|
						IB_QP_ACCESS_FLAGS),
				[IB_QPT_XRC_TGT] = (IB_QP_PKEY_INDEX		|
						IB_QP_PORT			|
						IB_QP_ACCESS_FLAGS),
				[IB_QPT_SMI] = (IB_QP_PKEY_INDEX		|
						IB_QP_QKEY),
				[IB_QPT_GSI] = (IB_QP_PKEY_INDEX		|
						IB_QP_QKEY),
			}
		},
	},
	[IB_QPS_INIT]  = {
		[IB_QPS_RESET] = { .valid = 1 },
		[IB_QPS_ERR] =   { .valid = 1 },
		[IB_QPS_INIT]  = {
			.valid = 1,
			.opt_param = {
				[IB_QPT_UD]  = (IB_QP_PKEY_INDEX		|
						IB_QP_PORT			|
						IB_QP_QKEY),
				[IB_QPT_UC]  = (IB_QP_PKEY_INDEX		|
						IB_QP_PORT			|
						IB_QP_ACCESS_FLAGS),
				[IB_QPT_RC]  = (IB_QP_PKEY_INDEX		|
						IB_QP_PORT			|
						IB_QP_ACCESS_FLAGS),
				[IB_QPT_XRC_INI] = (IB_QP_PKEY_INDEX		|
						IB_QP_PORT			|
						IB_QP_ACCESS_FLAGS),
				[IB_QPT_XRC_TGT] = (IB_QP_PKEY_INDEX		|
						IB_QP_PORT			|
						IB_QP_ACCESS_FLAGS),
				[IB_QPT_SMI] = (IB_QP_PKEY_INDEX		|
						IB_QP_QKEY),
				[IB_QPT_GSI] = (IB_QP_PKEY_INDEX		|
						IB_QP_QKEY),
			}
		},
		[IB_QPS_RTR]   = {
			.valid = 1,
			.req_param = {
				[IB_QPT_UC]  = (IB_QP_AV			|
						IB_QP_PATH_MTU			|
						IB_QP_DEST_QPN			|
						IB_QP_RQ_PSN),
				[IB_QPT_RC]  = (IB_QP_AV			|
						IB_QP_PATH_MTU			|
						IB_QP_DEST_QPN			|
						IB_QP_RQ_PSN			|
						IB_QP_MAX_DEST_RD_ATOMIC	|
						IB_QP_MIN_RNR_TIMER),
				[IB_QPT_XRC_INI] = (IB_QP_AV			|
						IB_QP_PATH_MTU			|
						IB_QP_DEST_QPN			|
						IB_QP_RQ_PSN),
				[IB_QPT_XRC_TGT] = (IB_QP_AV			|
						IB_QP_PATH_MTU			|
						IB_QP_DEST_QPN			|
						IB_QP_RQ_PSN			|
						IB_QP_MAX_DEST_RD_ATOMIC	|
						IB_QP_MIN_RNR_TIMER),
			},
			.opt_param = {
				 [IB_QPT_UD]  = (IB_QP_PKEY_INDEX		|
						 IB_QP_QKEY),
				 [IB_QPT_UC]  = (IB_QP_ALT_PATH			|
						 IB_QP_ACCESS_FLAGS		|
						 IB_QP_PKEY_INDEX),
				 [IB_QPT_RC]  = (IB_QP_ALT_PATH			|
						 IB_QP_ACCESS_FLAGS		|
						 IB_QP_PKEY_INDEX),
				 [IB_QPT_XRC_INI] = (IB_QP_ALT_PATH		|
						 IB_QP_ACCESS_FLAGS		|
						 IB_QP_PKEY_INDEX),
				 [IB_QPT_XRC_TGT] = (IB_QP_ALT_PATH		|
						 IB_QP_ACCESS_FLAGS		|
						 IB_QP_PKEY_INDEX),
				 [IB_QPT_SMI] = (IB_QP_PKEY_INDEX		|
						 IB_QP_QKEY),
				 [IB_QPT_GSI] = (IB_QP_PKEY_INDEX		|
						 IB_QP_QKEY),
			 },
		},
	},
	[IB_QPS_RTR]   = {
		[IB_QPS_RESET] = { .valid = 1 },
		[IB_QPS_ERR] =   { .valid = 1 },
		[IB_QPS_RTS]   = {
			.valid = 1,
			.req_param = {
				[IB_QPT_UD]  = IB_QP_SQ_PSN,
				[IB_QPT_UC]  = IB_QP_SQ_PSN,
				[IB_QPT_RC]  = (IB_QP_TIMEOUT			|
						IB_QP_RETRY_CNT			|
						IB_QP_RNR_RETRY			|
						IB_QP_SQ_PSN			|
						IB_QP_MAX_QP_RD_ATOMIC),
				[IB_QPT_XRC_INI] = (IB_QP_TIMEOUT		|
						IB_QP_RETRY_CNT			|
						IB_QP_RNR_RETRY			|
						IB_QP_SQ_PSN			|
						IB_QP_MAX_QP_RD_ATOMIC),
				[IB_QPT_XRC_TGT] = (IB_QP_TIMEOUT		|
						IB_QP_SQ_PSN),
				[IB_QPT_SMI] = IB_QP_SQ_PSN,
				[IB_QPT_GSI] = IB_QP_SQ_PSN,
			},
			.opt_param = {
				 [IB_QPT_UD]  = (IB_QP_CUR_STATE		|
						 IB_QP_QKEY),
				 [IB_QPT_UC]  = (IB_QP_CUR_STATE		|
						 IB_QP_ALT_PATH			|
						 IB_QP_ACCESS_FLAGS		|
						 IB_QP_PATH_MIG_STATE),
				 [IB_QPT_RC]  = (IB_QP_CUR_STATE		|
						 IB_QP_ALT_PATH			|
						 IB_QP_ACCESS_FLAGS		|
						 IB_QP_MIN_RNR_TIMER		|
						 IB_QP_PATH_MIG_STATE),
				 [IB_QPT_XRC_INI] = (IB_QP_CUR_STATE		|
						 IB_QP_ALT_PATH			|
						 IB_QP_ACCESS_FLAGS		|
						 IB_QP_PATH_MIG_STATE),
				 [IB_QPT_XRC_TGT] = (IB_QP_CUR_STATE		|
						 IB_QP_ALT_PATH			|
						 IB_QP_ACCESS_FLAGS		|
						 IB_QP_MIN_RNR_TIMER		|
						 IB_QP_PATH_MIG_STATE),
				 [IB_QPT_SMI] = (IB_QP_CUR_STATE		|
						 IB_QP_QKEY),
				 [IB_QPT_GSI] = (IB_QP_CUR_STATE		|
						 IB_QP_QKEY),
				 [IB_QPT_RAW_PACKET] = IB_QP_RATE_LIMIT,
			 }
		}
	},
	[IB_QPS_RTS]   = {
		[IB_QPS_RESET] = { .valid = 1 },
		[IB_QPS_ERR] =   { .valid = 1 },
		[IB_QPS_RTS]   = {
			.valid = 1,
			.opt_param = {
				[IB_QPT_UD]  = (IB_QP_CUR_STATE			|
						IB_QP_QKEY),
				[IB_QPT_UC]  = (IB_QP_CUR_STATE			|
						IB_QP_ACCESS_FLAGS		|
						IB_QP_ALT_PATH			|
						IB_QP_PATH_MIG_STATE),
				[IB_QPT_RC]  = (IB_QP_CUR_STATE			|
						IB_QP_ACCESS_FLAGS		|
						IB_QP_ALT_PATH			|
						IB_QP_PATH_MIG_STATE		|
						IB_QP_MIN_RNR_TIMER),
				[IB_QPT_XRC_INI] = (IB_QP_CUR_STATE		|
						IB_QP_ACCESS_FLAGS		|
						IB_QP_ALT_PATH			|
						IB_QP_PATH_MIG_STATE),
				[IB_QPT_XRC_TGT] = (IB_QP_CUR_STATE		|
						IB_QP_ACCESS_FLAGS		|
						IB_QP_ALT_PATH			|
						IB_QP_PATH_MIG_STATE		|
						IB_QP_MIN_RNR_TIMER),
				[IB_QPT_SMI] = (IB_QP_CUR_STATE			|
						IB_QP_QKEY),
				[IB_QPT_GSI] = (IB_QP_CUR_STATE			|
						IB_QP_QKEY),
				[IB_QPT_RAW_PACKET] = IB_QP_RATE_LIMIT,
			}
		},
		[IB_QPS_SQD]   = {
			.valid = 1,
			.opt_param = {
				[IB_QPT_UD]  = IB_QP_EN_SQD_ASYNC_NOTIFY,
				[IB_QPT_UC]  = IB_QP_EN_SQD_ASYNC_NOTIFY,
				[IB_QPT_RC]  = IB_QP_EN_SQD_ASYNC_NOTIFY,
				[IB_QPT_XRC_INI] = IB_QP_EN_SQD_ASYNC_NOTIFY,
				[IB_QPT_XRC_TGT] = IB_QP_EN_SQD_ASYNC_NOTIFY, /* ??? */
				[IB_QPT_SMI] = IB_QP_EN_SQD_ASYNC_NOTIFY,
				[IB_QPT_GSI] = IB_QP_EN_SQD_ASYNC_NOTIFY
			}
		},
	},
	[IB_QPS_SQD]   = {
		[IB_QPS_RESET] = { .valid = 1 },
		[IB_QPS_ERR] =   { .valid = 1 },
		[IB_QPS_RTS]   = {
			.valid = 1,
			.opt_param = {
				[IB_QPT_UD]  = (IB_QP_CUR_STATE			|
						IB_QP_QKEY),
				[IB_QPT_UC]  = (IB_QP_CUR_STATE			|
						IB_QP_ALT_PATH			|
						IB_QP_ACCESS_FLAGS		|
						IB_QP_PATH_MIG_STATE),
				[IB_QPT_RC]  = (IB_QP_CUR_STATE			|
						IB_QP_ALT_PATH			|
						IB_QP_ACCESS_FLAGS		|
						IB_QP_MIN_RNR_TIMER		|
						IB_QP_PATH_MIG_STATE),
				[IB_QPT_XRC_INI] = (IB_QP_CUR_STATE		|
						IB_QP_ALT_PATH			|
						IB_QP_ACCESS_FLAGS		|
						IB_QP_PATH_MIG_STATE),
				[IB_QPT_XRC_TGT] = (IB_QP_CUR_STATE		|
						IB_QP_ALT_PATH			|
						IB_QP_ACCESS_FLAGS		|
						IB_QP_MIN_RNR_TIMER		|
						IB_QP_PATH_MIG_STATE),
				[IB_QPT_SMI] = (IB_QP_CUR_STATE			|
						IB_QP_QKEY),
				[IB_QPT_GSI] = (IB_QP_CUR_STATE			|
						IB_QP_QKEY),
			}
		},
		[IB_QPS_SQD]   = {
			.valid = 1,
			.opt_param = {
				[IB_QPT_UD]  = (IB_QP_PKEY_INDEX		|
						IB_QP_QKEY),
				[IB_QPT_UC]  = (IB_QP_AV			|
						IB_QP_ALT_PATH			|
						IB_QP_ACCESS_FLAGS		|
						IB_QP_PKEY_INDEX		|
						IB_QP_PATH_MIG_STATE),
				[IB_QPT_RC]  = (IB_QP_PORT			|
						IB_QP_AV			|
						IB_QP_TIMEOUT			|
						IB_QP_RETRY_CNT			|
						IB_QP_RNR_RETRY			|
						IB_QP_MAX_QP_RD_ATOMIC		|
						IB_QP_MAX_DEST_RD_ATOMIC	|
						IB_QP_ALT_PATH			|
						IB_QP_ACCESS_FLAGS		|
						IB_QP_PKEY_INDEX		|
						IB_QP_MIN_RNR_TIMER		|
						IB_QP_PATH_MIG_STATE),
				[IB_QPT_XRC_INI] = (IB_QP_PORT			|
						IB_QP_AV			|
						IB_QP_TIMEOUT			|
						IB_QP_RETRY_CNT			|
						IB_QP_RNR_RETRY			|
						IB_QP_MAX_QP_RD_ATOMIC		|
						IB_QP_ALT_PATH			|
						IB_QP_ACCESS_FLAGS		|
						IB_QP_PKEY_INDEX		|
						IB_QP_PATH_MIG_STATE),
				[IB_QPT_XRC_TGT] = (IB_QP_PORT			|
						IB_QP_AV			|
						IB_QP_TIMEOUT			|
						IB_QP_MAX_DEST_RD_ATOMIC	|
						IB_QP_ALT_PATH			|
						IB_QP_ACCESS_FLAGS		|
						IB_QP_PKEY_INDEX		|
						IB_QP_MIN_RNR_TIMER		|
						IB_QP_PATH_MIG_STATE),
				[IB_QPT_SMI] = (IB_QP_PKEY_INDEX		|
						IB_QP_QKEY),
				[IB_QPT_GSI] = (IB_QP_PKEY_INDEX		|
						IB_QP_QKEY),
			}
		}
	},
	[IB_QPS_SQE]   = {
		[IB_QPS_RESET] = { .valid = 1 },
		[IB_QPS_ERR] =   { .valid = 1 },
		[IB_QPS_RTS]   = {
			.valid = 1,
			.opt_param = {
				[IB_QPT_UD]  = (IB_QP_CUR_STATE			|
						IB_QP_QKEY),
				[IB_QPT_UC]  = (IB_QP_CUR_STATE			|
						IB_QP_ACCESS_FLAGS),
				[IB_QPT_SMI] = (IB_QP_CUR_STATE			|
						IB_QP_QKEY),
				[IB_QPT_GSI] = (IB_QP_CUR_STATE			|
						IB_QP_QKEY),
			}
		}
	},
	[IB_QPS_ERR] = {
		[IB_QPS_RESET] = { .valid = 1 },
		[IB_QPS_ERR] =   { .valid = 1 }
	}
};

bool ib_modify_qp_is_ok(enum ib_qp_state cur_state, enum ib_qp_state next_state,
			enum ib_qp_type type, enum ib_qp_attr_mask mask)
{
	enum ib_qp_attr_mask req_param, opt_param;

	if (mask & IB_QP_CUR_STATE  &&
	    cur_state != IB_QPS_RTR && cur_state != IB_QPS_RTS &&
	    cur_state != IB_QPS_SQD && cur_state != IB_QPS_SQE)
		return false;

	if (!qp_state_table[cur_state][next_state].valid)
		return false;

	req_param = qp_state_table[cur_state][next_state].req_param[type];
	opt_param = qp_state_table[cur_state][next_state].opt_param[type];

	if ((mask & req_param) != req_param)
		return false;

	if (mask & ~(req_param | opt_param | IB_QP_STATE))
		return false;

	return true;
}
EXPORT_SYMBOL(ib_modify_qp_is_ok);

int ib_resolve_eth_dmac(struct ib_device *device,
			struct ib_ah_attr *ah_attr)
{
	struct ib_gid_attr sgid_attr;
	union ib_gid sgid;
	int hop_limit;
	int ret;

	if (ah_attr->port_num < rdma_start_port(device) ||
	    ah_attr->port_num > rdma_end_port(device))
		return -EINVAL;

	if (!rdma_cap_eth_ah(device, ah_attr->port_num))
		return 0;

	if (rdma_is_multicast_addr((struct in6_addr *)ah_attr->grh.dgid.raw)) {
		if (ipv6_addr_v4mapped((struct in6_addr *)ah_attr->grh.dgid.raw)) {
			__be32 addr = 0;

			memcpy(&addr, ah_attr->grh.dgid.raw + 12, 4);
			ip_eth_mc_map(addr, (char *)ah_attr->dmac);
		} else {
			ipv6_eth_mc_map((struct in6_addr *)ah_attr->grh.dgid.raw,
					(char *)ah_attr->dmac);
		}
		return 0;
	}

	ret = ib_query_gid(device,
			   ah_attr->port_num,
			   ah_attr->grh.sgid_index,
			   &sgid, &sgid_attr);
	if (ret != 0)
		return (ret);
	if (!sgid_attr.ndev)
		return -ENXIO;

	ret = rdma_addr_find_l2_eth_by_grh(&sgid,
					   &ah_attr->grh.dgid,
					   ah_attr->dmac,
					   sgid_attr.ndev, &hop_limit);
	dev_put(sgid_attr.ndev);

	ah_attr->grh.hop_limit = hop_limit;
	return ret;
}
EXPORT_SYMBOL(ib_resolve_eth_dmac);

static bool is_qp_type_connected(const struct ib_qp *qp)
{
	return (qp->qp_type == IB_QPT_UC ||
		qp->qp_type == IB_QPT_RC ||
		qp->qp_type == IB_QPT_XRC_INI ||
		qp->qp_type == IB_QPT_XRC_TGT);
}

/**
 * IB core internal function to perform QP attributes modification.
 */
static int _ib_modify_qp(struct ib_qp *qp, struct ib_qp_attr *attr,
			 int attr_mask, struct ib_udata *udata)
{
	u8 port = attr_mask & IB_QP_PORT ? attr->port_num : qp->port;
	int ret;

	if (port < rdma_start_port(qp->device) ||
	    port > rdma_end_port(qp->device))
		return -EINVAL;

	if (attr_mask & IB_QP_ALT_PATH) {
		/*
		 * Today the core code can only handle alternate paths and APM
		 * for IB. Ban them in roce mode.
		 */
		if (!(rdma_protocol_ib(qp->device,
		      attr->alt_ah_attr.port_num) &&
		      rdma_protocol_ib(qp->device, port))) {
			ret = EINVAL;
			goto out;
		}
	}

	/*
	 * If the user provided the qp_attr then we have to resolve it. Kernel
	 * users have to provide already resolved rdma_ah_attr's
	 */
	if (udata && (attr_mask & IB_QP_AV) &&
	    rdma_protocol_roce(qp->device, port) &&
	    is_qp_type_connected(qp)) {
		ret = ib_resolve_eth_dmac(qp->device, &attr->ah_attr);
		if (ret)
			goto out;
	}

	if (rdma_ib_or_roce(qp->device, port)) {
		if (attr_mask & IB_QP_RQ_PSN && attr->rq_psn & ~0xffffff) {
			dev_warn(&qp->device->dev,
				 "%s rq_psn overflow, masking to 24 bits\n",
				 __func__);
			attr->rq_psn &= 0xffffff;
		}

		if (attr_mask & IB_QP_SQ_PSN && attr->sq_psn & ~0xffffff) {
			dev_warn(&qp->device->dev,
				 " %s sq_psn overflow, masking to 24 bits\n",
				 __func__);
			attr->sq_psn &= 0xffffff;
		}
	}

	ret = qp->device->modify_qp(qp, attr, attr_mask, udata);
	if (ret)
		goto out;

	if (attr_mask & IB_QP_PORT)
		qp->port = attr->port_num;
out:
	return ret;
}

/**
 * ib_modify_qp_with_udata - Modifies the attributes for the specified QP.
 * @ib_qp: The QP to modify.
 * @attr: On input, specifies the QP attributes to modify.  On output,
 *   the current values of selected QP attributes are returned.
 * @attr_mask: A bit-mask used to specify which attributes of the QP
 *   are being modified.
 * @udata: pointer to user's input output buffer information
 *   are being modified.
 * It returns 0 on success and returns appropriate error code on error.
 */
int ib_modify_qp_with_udata(struct ib_qp *ib_qp, struct ib_qp_attr *attr,
			    int attr_mask, struct ib_udata *udata)
{
	return _ib_modify_qp(ib_qp->real_qp, attr, attr_mask, udata);
}
EXPORT_SYMBOL(ib_modify_qp_with_udata);

int ib_modify_qp(struct ib_qp *qp,
		 struct ib_qp_attr *qp_attr,
		 int qp_attr_mask)
{
	if (qp_attr_mask & IB_QP_AV) {
		int ret;

		ret = ib_resolve_eth_dmac(qp->device, &qp_attr->ah_attr);
		if (ret)
			return ret;
	}

	return qp->device->modify_qp(qp->real_qp, qp_attr, qp_attr_mask, NULL);
}
EXPORT_SYMBOL(ib_modify_qp);

int ib_query_qp(struct ib_qp *qp,
		struct ib_qp_attr *qp_attr,
		int qp_attr_mask,
		struct ib_qp_init_attr *qp_init_attr)
{
	return qp->device->query_qp ?
		qp->device->query_qp(qp->real_qp, qp_attr, qp_attr_mask, qp_init_attr) :
		-ENOSYS;
}
EXPORT_SYMBOL(ib_query_qp);

int ib_close_qp(struct ib_qp *qp)
{
	struct ib_qp *real_qp;
	unsigned long flags;

	real_qp = qp->real_qp;
	if (real_qp == qp)
		return -EINVAL;

	spin_lock_irqsave(&real_qp->device->event_handler_lock, flags);
	list_del(&qp->open_list);
	spin_unlock_irqrestore(&real_qp->device->event_handler_lock, flags);

	atomic_dec(&real_qp->usecnt);
	kfree(qp);

	return 0;
}
EXPORT_SYMBOL(ib_close_qp);

static int __ib_destroy_shared_qp(struct ib_qp *qp)
{
	struct ib_xrcd *xrcd;
	struct ib_qp *real_qp;
	int ret;

	real_qp = qp->real_qp;
	xrcd = real_qp->xrcd;

	mutex_lock(&xrcd->tgt_qp_mutex);
	ib_close_qp(qp);
	if (atomic_read(&real_qp->usecnt) == 0)
		list_del(&real_qp->xrcd_list);
	else
		real_qp = NULL;
	mutex_unlock(&xrcd->tgt_qp_mutex);

	if (real_qp) {
		ret = ib_destroy_qp(real_qp);
		if (!ret)
			atomic_dec(&xrcd->usecnt);
		else
			__ib_insert_xrcd_qp(xrcd, real_qp);
	}

	return 0;
}

int ib_destroy_qp_user(struct ib_qp *qp, struct ib_udata *udata)
{
	struct ib_pd *pd;
	struct ib_cq *scq, *rcq;
	struct ib_srq *srq;
	struct ib_rwq_ind_table *ind_tbl;
	int ret;

	if (atomic_read(&qp->usecnt))
		return -EBUSY;

	if (qp->real_qp != qp)
		return __ib_destroy_shared_qp(qp);

	pd   = qp->pd;
	scq  = qp->send_cq;
	rcq  = qp->recv_cq;
	srq  = qp->srq;
	ind_tbl = qp->rwq_ind_tbl;

	ret = qp->device->destroy_qp(qp, udata);
	if (!ret) {
		if (pd)
			atomic_dec(&pd->usecnt);
		if (scq)
			atomic_dec(&scq->usecnt);
		if (rcq)
			atomic_dec(&rcq->usecnt);
		if (srq)
			atomic_dec(&srq->usecnt);
		if (ind_tbl)
			atomic_dec(&ind_tbl->usecnt);
	}

	return ret;
}
EXPORT_SYMBOL(ib_destroy_qp_user);

/* Completion queues */

struct ib_cq *__ib_create_cq(struct ib_device *device,
			     ib_comp_handler comp_handler,
			     void (*event_handler)(struct ib_event *, void *),
			     void *cq_context,
			     const struct ib_cq_init_attr *cq_attr,
			     const char *caller)
{
	struct ib_cq *cq;
	int ret;

	cq = rdma_zalloc_drv_obj(device, ib_cq);
	if (!cq)
		return ERR_PTR(-ENOMEM);

	cq->device = device;
	cq->uobject = NULL;
	cq->comp_handler = comp_handler;
	cq->event_handler = event_handler;
	cq->cq_context = cq_context;
	atomic_set(&cq->usecnt, 0);

	ret = device->create_cq(cq, cq_attr, NULL);
	if (ret) {
		kfree(cq);
		return ERR_PTR(ret);
	}

	return cq;
}
EXPORT_SYMBOL(__ib_create_cq);

int ib_modify_cq(struct ib_cq *cq, u16 cq_count, u16 cq_period)
{
	return cq->device->modify_cq ?
		cq->device->modify_cq(cq, cq_count, cq_period) : -ENOSYS;
}
EXPORT_SYMBOL(ib_modify_cq);

int ib_destroy_cq_user(struct ib_cq *cq, struct ib_udata *udata)
{
	if (atomic_read(&cq->usecnt))
		return -EBUSY;

	cq->device->destroy_cq(cq, udata);
	kfree(cq);
	return 0;
}
EXPORT_SYMBOL(ib_destroy_cq_user);

int ib_resize_cq(struct ib_cq *cq, int cqe)
{
	return cq->device->resize_cq ?
		cq->device->resize_cq(cq, cqe, NULL) : -ENOSYS;
}
EXPORT_SYMBOL(ib_resize_cq);

/* Memory regions */

int ib_dereg_mr_user(struct ib_mr *mr, struct ib_udata *udata)
{
	struct ib_pd *pd = mr->pd;
	struct ib_dm *dm = mr->dm;
	struct ib_sig_attrs *sig_attrs = mr->sig_attrs;
	int ret;

	ret = mr->device->dereg_mr(mr, udata);
	if (!ret) {
		atomic_dec(&pd->usecnt);
		if (dm)
			atomic_dec(&dm->usecnt);
		kfree(sig_attrs);
	}

	return ret;
}
EXPORT_SYMBOL(ib_dereg_mr_user);

/**
 * ib_alloc_mr_user() - Allocates a memory region
 * @pd:            protection domain associated with the region
 * @mr_type:       memory region type
 * @max_num_sg:    maximum sg entries available for registration.
 * @udata:	   user data or null for kernel objects
 *
 * Notes:
 * Memory registeration page/sg lists must not exceed max_num_sg.
 * For mr_type IB_MR_TYPE_MEM_REG, the total length cannot exceed
 * max_num_sg * used_page_size.
 *
 */
struct ib_mr *ib_alloc_mr_user(struct ib_pd *pd, enum ib_mr_type mr_type,
			       u32 max_num_sg, struct ib_udata *udata)
{
	struct ib_mr *mr;

	if (!pd->device->alloc_mr) {
		mr = ERR_PTR(-EOPNOTSUPP);
		goto out;
	}

	if (mr_type == IB_MR_TYPE_INTEGRITY) {
		WARN_ON_ONCE(1);
		mr = ERR_PTR(-EINVAL);
		goto out;
	}

	mr = pd->device->alloc_mr(pd, mr_type, max_num_sg, udata);
	if (!IS_ERR(mr)) {
		mr->device  = pd->device;
		mr->pd      = pd;
		mr->dm      = NULL;
		mr->uobject = NULL;
		atomic_inc(&pd->usecnt);
		mr->need_inval = false;
		mr->type = mr_type;
		mr->sig_attrs = NULL;
	}

out:
	return mr;
}
EXPORT_SYMBOL(ib_alloc_mr_user);

/* "Fast" memory regions */

struct ib_fmr *ib_alloc_fmr(struct ib_pd *pd,
			    int mr_access_flags,
			    struct ib_fmr_attr *fmr_attr)
{
	struct ib_fmr *fmr;

	if (!pd->device->alloc_fmr)
		return ERR_PTR(-ENOSYS);

	fmr = pd->device->alloc_fmr(pd, mr_access_flags, fmr_attr);
	if (!IS_ERR(fmr)) {
		fmr->device = pd->device;
		fmr->pd     = pd;
		atomic_inc(&pd->usecnt);
	}

	return fmr;
}
EXPORT_SYMBOL(ib_alloc_fmr);

int ib_unmap_fmr(struct list_head *fmr_list)
{
	struct ib_fmr *fmr;

	if (list_empty(fmr_list))
		return 0;

	fmr = list_entry(fmr_list->next, struct ib_fmr, list);
	return fmr->device->unmap_fmr(fmr_list);
}
EXPORT_SYMBOL(ib_unmap_fmr);

int ib_dealloc_fmr(struct ib_fmr *fmr)
{
	struct ib_pd *pd;
	int ret;

	pd = fmr->pd;
	ret = fmr->device->dealloc_fmr(fmr);
	if (!ret)
		atomic_dec(&pd->usecnt);

	return ret;
}
EXPORT_SYMBOL(ib_dealloc_fmr);

/* Multicast groups */

static bool is_valid_mcast_lid(struct ib_qp *qp, u16 lid)
{
	struct ib_qp_init_attr init_attr = {};
	struct ib_qp_attr attr = {};
	int num_eth_ports = 0;
	int port;

	/* If QP state >= init, it is assigned to a port and we can check this
	 * port only.
	 */
	if (!ib_query_qp(qp, &attr, IB_QP_STATE | IB_QP_PORT, &init_attr)) {
		if (attr.qp_state >= IB_QPS_INIT) {
			if (rdma_port_get_link_layer(qp->device, attr.port_num) !=
			    IB_LINK_LAYER_INFINIBAND)
				return true;
			goto lid_check;
		}
	}

	/* Can't get a quick answer, iterate over all ports */
	for (port = 0; port < qp->device->phys_port_cnt; port++)
		if (rdma_port_get_link_layer(qp->device, port) !=
		    IB_LINK_LAYER_INFINIBAND)
			num_eth_ports++;

	/* If we have at lease one Ethernet port, RoCE annex declares that
	 * multicast LID should be ignored. We can't tell at this step if the
	 * QP belongs to an IB or Ethernet port.
	 */
	if (num_eth_ports)
		return true;

	/* If all the ports are IB, we can check according to IB spec. */
lid_check:
	return !(lid < be16_to_cpu(IB_MULTICAST_LID_BASE) ||
		 lid == be16_to_cpu(IB_LID_PERMISSIVE));
}

int ib_attach_mcast(struct ib_qp *qp, union ib_gid *gid, u16 lid)
{
	int ret;

	if (!qp->device->attach_mcast)
		return -ENOSYS;

	if (!rdma_is_multicast_addr((struct in6_addr *)gid->raw) ||
	    qp->qp_type != IB_QPT_UD || !is_valid_mcast_lid(qp, lid))
		return -EINVAL;

	ret = qp->device->attach_mcast(qp, gid, lid);
	if (!ret)
		atomic_inc(&qp->usecnt);
	return ret;
}
EXPORT_SYMBOL(ib_attach_mcast);

int ib_detach_mcast(struct ib_qp *qp, union ib_gid *gid, u16 lid)
{
	int ret;

	if (!qp->device->detach_mcast)
		return -ENOSYS;

	if (!rdma_is_multicast_addr((struct in6_addr *)gid->raw) ||
	    qp->qp_type != IB_QPT_UD || !is_valid_mcast_lid(qp, lid))
		return -EINVAL;

	ret = qp->device->detach_mcast(qp, gid, lid);
	if (!ret)
		atomic_dec(&qp->usecnt);
	return ret;
}
EXPORT_SYMBOL(ib_detach_mcast);

struct ib_xrcd *__ib_alloc_xrcd(struct ib_device *device, const char *caller)
{
	struct ib_xrcd *xrcd;

	if (!device->alloc_xrcd)
		return ERR_PTR(-EOPNOTSUPP);

	xrcd = device->alloc_xrcd(device, NULL);
	if (!IS_ERR(xrcd)) {
		xrcd->device = device;
		xrcd->inode = NULL;
		atomic_set(&xrcd->usecnt, 0);
		mutex_init(&xrcd->tgt_qp_mutex);
		INIT_LIST_HEAD(&xrcd->tgt_qp_list);
	}

	return xrcd;
}
EXPORT_SYMBOL(__ib_alloc_xrcd);

int ib_dealloc_xrcd(struct ib_xrcd *xrcd, struct ib_udata *udata)
{
	struct ib_qp *qp;
	int ret;

	if (atomic_read(&xrcd->usecnt))
		return -EBUSY;

	while (!list_empty(&xrcd->tgt_qp_list)) {
		qp = list_entry(xrcd->tgt_qp_list.next, struct ib_qp, xrcd_list);
		ret = ib_destroy_qp(qp);
		if (ret)
			return ret;
	}
	mutex_destroy(&xrcd->tgt_qp_mutex);

	return xrcd->device->dealloc_xrcd(xrcd, udata);
}
EXPORT_SYMBOL(ib_dealloc_xrcd);

/**
 * ib_create_wq - Creates a WQ associated with the specified protection
 * domain.
 * @pd: The protection domain associated with the WQ.
 * @wq_init_attr: A list of initial attributes required to create the
 * WQ. If WQ creation succeeds, then the attributes are updated to
 * the actual capabilities of the created WQ.
 *
 * wq_init_attr->max_wr and wq_init_attr->max_sge determine
 * the requested size of the WQ, and set to the actual values allocated
 * on return.
 * If ib_create_wq() succeeds, then max_wr and max_sge will always be
 * at least as large as the requested values.
 */
struct ib_wq *ib_create_wq(struct ib_pd *pd,
			   struct ib_wq_init_attr *wq_attr)
{
	struct ib_wq *wq;

	if (!pd->device->create_wq)
		return ERR_PTR(-ENOSYS);

	wq = pd->device->create_wq(pd, wq_attr, NULL);
	if (!IS_ERR(wq)) {
		wq->event_handler = wq_attr->event_handler;
		wq->wq_context = wq_attr->wq_context;
		wq->wq_type = wq_attr->wq_type;
		wq->cq = wq_attr->cq;
		wq->device = pd->device;
		wq->pd = pd;
		wq->uobject = NULL;
		atomic_inc(&pd->usecnt);
		atomic_inc(&wq_attr->cq->usecnt);
		atomic_set(&wq->usecnt, 0);
	}
	return wq;
}
EXPORT_SYMBOL(ib_create_wq);

/**
 * ib_destroy_wq - Destroys the specified user WQ.
 * @wq: The WQ to destroy.
 * @udata: Valid user data
 */
int ib_destroy_wq(struct ib_wq *wq, struct ib_udata *udata)
{
	struct ib_cq *cq = wq->cq;
	struct ib_pd *pd = wq->pd;

	if (atomic_read(&wq->usecnt))
		return -EBUSY;

	wq->device->destroy_wq(wq, udata);
	atomic_dec(&pd->usecnt);
	atomic_dec(&cq->usecnt);

	return 0;
}
EXPORT_SYMBOL(ib_destroy_wq);

/**
 * ib_modify_wq - Modifies the specified WQ.
 * @wq: The WQ to modify.
 * @wq_attr: On input, specifies the WQ attributes to modify.
 * @wq_attr_mask: A bit-mask used to specify which attributes of the WQ
 *   are being modified.
 * On output, the current values of selected WQ attributes are returned.
 */
int ib_modify_wq(struct ib_wq *wq, struct ib_wq_attr *wq_attr,
		 u32 wq_attr_mask)
{
	int err;

	if (!wq->device->modify_wq)
		return -ENOSYS;

	err = wq->device->modify_wq(wq, wq_attr, wq_attr_mask, NULL);
	return err;
}
EXPORT_SYMBOL(ib_modify_wq);

/*
 * ib_create_rwq_ind_table - Creates a RQ Indirection Table.
 * @device: The device on which to create the rwq indirection table.
 * @ib_rwq_ind_table_init_attr: A list of initial attributes required to
 * create the Indirection Table.
 *
 * Note: The life time of ib_rwq_ind_table_init_attr->ind_tbl is not less
 *	than the created ib_rwq_ind_table object and the caller is responsible
 *	for its memory allocation/free.
 */
struct ib_rwq_ind_table *ib_create_rwq_ind_table(struct ib_device *device,
						 struct ib_rwq_ind_table_init_attr *init_attr)
{
	struct ib_rwq_ind_table *rwq_ind_table;
	int i;
	u32 table_size;

	if (!device->create_rwq_ind_table)
		return ERR_PTR(-ENOSYS);

	table_size = (1 << init_attr->log_ind_tbl_size);
	rwq_ind_table = device->create_rwq_ind_table(device,
				init_attr, NULL);
	if (IS_ERR(rwq_ind_table))
		return rwq_ind_table;

	rwq_ind_table->ind_tbl = init_attr->ind_tbl;
	rwq_ind_table->log_ind_tbl_size = init_attr->log_ind_tbl_size;
	rwq_ind_table->device = device;
	rwq_ind_table->uobject = NULL;
	atomic_set(&rwq_ind_table->usecnt, 0);

	for (i = 0; i < table_size; i++)
		atomic_inc(&rwq_ind_table->ind_tbl[i]->usecnt);

	return rwq_ind_table;
}
EXPORT_SYMBOL(ib_create_rwq_ind_table);

/*
 * ib_destroy_rwq_ind_table - Destroys the specified Indirection Table.
 * @wq_ind_table: The Indirection Table to destroy.
*/
int ib_destroy_rwq_ind_table(struct ib_rwq_ind_table *rwq_ind_table)
{
	int err, i;
	u32 table_size = (1 << rwq_ind_table->log_ind_tbl_size);
	struct ib_wq **ind_tbl = rwq_ind_table->ind_tbl;

	if (atomic_read(&rwq_ind_table->usecnt))
		return -EBUSY;

	err = rwq_ind_table->device->destroy_rwq_ind_table(rwq_ind_table);
	if (!err) {
		for (i = 0; i < table_size; i++)
			atomic_dec(&ind_tbl[i]->usecnt);
	}

	return err;
}
EXPORT_SYMBOL(ib_destroy_rwq_ind_table);

int ib_check_mr_status(struct ib_mr *mr, u32 check_mask,
		       struct ib_mr_status *mr_status)
{
	return mr->device->check_mr_status ?
		mr->device->check_mr_status(mr, check_mask, mr_status) : -ENOSYS;
}
EXPORT_SYMBOL(ib_check_mr_status);

int ib_set_vf_link_state(struct ib_device *device, int vf, u8 port,
			 int state)
{
	if (!device->set_vf_link_state)
		return -ENOSYS;

	return device->set_vf_link_state(device, vf, port, state);
}
EXPORT_SYMBOL(ib_set_vf_link_state);

int ib_get_vf_config(struct ib_device *device, int vf, u8 port,
		     struct ifla_vf_info *info)
{
	if (!device->get_vf_config)
		return -ENOSYS;

	return device->get_vf_config(device, vf, port, info);
}
EXPORT_SYMBOL(ib_get_vf_config);

int ib_get_vf_stats(struct ib_device *device, int vf, u8 port,
		    struct ifla_vf_stats *stats)
{
	if (!device->get_vf_stats)
		return -ENOSYS;

	return device->get_vf_stats(device, vf, port, stats);
}
EXPORT_SYMBOL(ib_get_vf_stats);

int ib_set_vf_guid(struct ib_device *device, int vf, u8 port, u64 guid,
		   int type)
{
	if (!device->set_vf_guid)
		return -ENOSYS;

	return device->set_vf_guid(device, vf, port, guid, type);
}
EXPORT_SYMBOL(ib_set_vf_guid);

/**
 * ib_map_mr_sg() - Map the largest prefix of a dma mapped SG list
 *     and set it the memory region.
 * @mr:            memory region
 * @sg:            dma mapped scatterlist
 * @sg_nents:      number of entries in sg
 * @sg_offset:     offset in bytes into sg
 * @page_size:     page vector desired page size
 *
 * Constraints:
 * - The first sg element is allowed to have an offset.
 * - Each sg element must either be aligned to page_size or virtually
 *   contiguous to the previous element. In case an sg element has a
 *   non-contiguous offset, the mapping prefix will not include it.
 * - The last sg element is allowed to have length less than page_size.
 * - If sg_nents total byte length exceeds the mr max_num_sge * page_size
 *   then only max_num_sg entries will be mapped.
 * - If the MR was allocated with type IB_MR_TYPE_SG_GAPS, none of these
 *   constraints holds and the page_size argument is ignored.
 *
 * Returns the number of sg elements that were mapped to the memory region.
 *
 * After this completes successfully, the  memory region
 * is ready for registration.
 */
int ib_map_mr_sg(struct ib_mr *mr, struct scatterlist *sg, int sg_nents,
		 unsigned int *sg_offset, unsigned int page_size)
{
	if (unlikely(!mr->device->map_mr_sg))
		return -ENOSYS;

	mr->page_size = page_size;

	return mr->device->map_mr_sg(mr, sg, sg_nents, sg_offset);
}
EXPORT_SYMBOL(ib_map_mr_sg);

/**
 * ib_sg_to_pages() - Convert the largest prefix of a sg list
 *     to a page vector
 * @mr:            memory region
 * @sgl:           dma mapped scatterlist
 * @sg_nents:      number of entries in sg
 * @sg_offset_p:   IN:  start offset in bytes into sg
 *                 OUT: offset in bytes for element n of the sg of the first
 *                      byte that has not been processed where n is the return
 *                      value of this function.
 * @set_page:      driver page assignment function pointer
 *
 * Core service helper for drivers to convert the largest
 * prefix of given sg list to a page vector. The sg list
 * prefix converted is the prefix that meet the requirements
 * of ib_map_mr_sg.
 *
 * Returns the number of sg elements that were assigned to
 * a page vector.
 */
int ib_sg_to_pages(struct ib_mr *mr, struct scatterlist *sgl, int sg_nents,
		unsigned int *sg_offset_p, int (*set_page)(struct ib_mr *, u64))
{
	struct scatterlist *sg;
	u64 last_end_dma_addr = 0;
	unsigned int sg_offset = sg_offset_p ? *sg_offset_p : 0;
	unsigned int last_page_off = 0;
	u64 page_mask = ~((u64)mr->page_size - 1);
	int i, ret;

	if (unlikely(sg_nents <= 0 || sg_offset > sg_dma_len(&sgl[0])))
		return -EINVAL;

	mr->iova = sg_dma_address(&sgl[0]) + sg_offset;
	mr->length = 0;

	for_each_sg(sgl, sg, sg_nents, i) {
		u64 dma_addr = sg_dma_address(sg) + sg_offset;
		u64 prev_addr = dma_addr;
		unsigned int dma_len = sg_dma_len(sg) - sg_offset;
		u64 end_dma_addr = dma_addr + dma_len;
		u64 page_addr = dma_addr & page_mask;

		/*
		 * For the second and later elements, check whether either the
		 * end of element i-1 or the start of element i is not aligned
		 * on a page boundary.
		 */
		if (i && (last_page_off != 0 || page_addr != dma_addr)) {
			/* Stop mapping if there is a gap. */
			if (last_end_dma_addr != dma_addr)
				break;

			/*
			 * Coalesce this element with the last. If it is small
			 * enough just update mr->length. Otherwise start
			 * mapping from the next page.
			 */
			goto next_page;
		}

		do {
			ret = set_page(mr, page_addr);
			if (unlikely(ret < 0)) {
				sg_offset = prev_addr - sg_dma_address(sg);
				mr->length += prev_addr - dma_addr;
				if (sg_offset_p)
					*sg_offset_p = sg_offset;
				return i || sg_offset ? i : ret;
			}
			prev_addr = page_addr;
next_page:
			page_addr += mr->page_size;
		} while (page_addr < end_dma_addr);

		mr->length += dma_len;
		last_end_dma_addr = end_dma_addr;
		last_page_off = end_dma_addr & ~page_mask;

		sg_offset = 0;
	}

	if (sg_offset_p)
		*sg_offset_p = 0;
	return i;
}
EXPORT_SYMBOL(ib_sg_to_pages);

struct ib_drain_cqe {
	struct ib_cqe cqe;
	struct completion done;
};

static void ib_drain_qp_done(struct ib_cq *cq, struct ib_wc *wc)
{
	struct ib_drain_cqe *cqe = container_of(wc->wr_cqe, struct ib_drain_cqe,
						cqe);

	complete(&cqe->done);
}

/*
 * Post a WR and block until its completion is reaped for the SQ.
 */
static void __ib_drain_sq(struct ib_qp *qp)
{
	struct ib_qp_attr attr = { .qp_state = IB_QPS_ERR };
	struct ib_drain_cqe sdrain;
	const struct ib_send_wr *bad_swr;
	struct ib_rdma_wr swr = {
		.wr = {
			.opcode	= IB_WR_RDMA_WRITE,
			.wr_cqe	= &sdrain.cqe,
		},
	};
	int ret;

	if (qp->send_cq->poll_ctx == IB_POLL_DIRECT) {
		WARN_ONCE(qp->send_cq->poll_ctx == IB_POLL_DIRECT,
			  "IB_POLL_DIRECT poll_ctx not supported for drain\n");
		return;
	}

	sdrain.cqe.done = ib_drain_qp_done;
	init_completion(&sdrain.done);

	ret = ib_modify_qp(qp, &attr, IB_QP_STATE);
	if (ret) {
		WARN_ONCE(ret, "failed to drain send queue: %d\n", ret);
		return;
	}

	ret = ib_post_send(qp, &swr.wr, &bad_swr);
	if (ret) {
		WARN_ONCE(ret, "failed to drain send queue: %d\n", ret);
		return;
	}

	wait_for_completion(&sdrain.done);
}

/*
 * Post a WR and block until its completion is reaped for the RQ.
 */
static void __ib_drain_rq(struct ib_qp *qp)
{
	struct ib_qp_attr attr = { .qp_state = IB_QPS_ERR };
	struct ib_drain_cqe rdrain;
	struct ib_recv_wr rwr = {};
	const struct ib_recv_wr *bad_rwr;
	int ret;

	if (qp->recv_cq->poll_ctx == IB_POLL_DIRECT) {
		WARN_ONCE(qp->recv_cq->poll_ctx == IB_POLL_DIRECT,
			  "IB_POLL_DIRECT poll_ctx not supported for drain\n");
		return;
	}

	rwr.wr_cqe = &rdrain.cqe;
	rdrain.cqe.done = ib_drain_qp_done;
	init_completion(&rdrain.done);

	ret = ib_modify_qp(qp, &attr, IB_QP_STATE);
	if (ret) {
		WARN_ONCE(ret, "failed to drain recv queue: %d\n", ret);
		return;
	}

	ret = ib_post_recv(qp, &rwr, &bad_rwr);
	if (ret) {
		WARN_ONCE(ret, "failed to drain recv queue: %d\n", ret);
		return;
	}

	wait_for_completion(&rdrain.done);
}

/**
 * ib_drain_sq() - Block until all SQ CQEs have been consumed by the
 *		   application.
 * @qp:            queue pair to drain
 *
 * If the device has a provider-specific drain function, then
 * call that.  Otherwise call the generic drain function
 * __ib_drain_sq().
 *
 * The caller must:
 *
 * ensure there is room in the CQ and SQ for the drain work request and
 * completion.
 *
 * allocate the CQ using ib_alloc_cq() and the CQ poll context cannot be
 * IB_POLL_DIRECT.
 *
 * ensure that there are no other contexts that are posting WRs concurrently.
 * Otherwise the drain is not guaranteed.
 */
void ib_drain_sq(struct ib_qp *qp)
{
	if (qp->device->drain_sq)
		qp->device->drain_sq(qp);
	else
		__ib_drain_sq(qp);
}
EXPORT_SYMBOL(ib_drain_sq);

/**
 * ib_drain_rq() - Block until all RQ CQEs have been consumed by the
 *		   application.
 * @qp:            queue pair to drain
 *
 * If the device has a provider-specific drain function, then
 * call that.  Otherwise call the generic drain function
 * __ib_drain_rq().
 *
 * The caller must:
 *
 * ensure there is room in the CQ and RQ for the drain work request and
 * completion.
 *
 * allocate the CQ using ib_alloc_cq() and the CQ poll context cannot be
 * IB_POLL_DIRECT.
 *
 * ensure that there are no other contexts that are posting WRs concurrently.
 * Otherwise the drain is not guaranteed.
 */
void ib_drain_rq(struct ib_qp *qp)
{
	if (qp->device->drain_rq)
		qp->device->drain_rq(qp);
	else
		__ib_drain_rq(qp);
}
EXPORT_SYMBOL(ib_drain_rq);

/**
 * ib_drain_qp() - Block until all CQEs have been consumed by the
 *		   application on both the RQ and SQ.
 * @qp:            queue pair to drain
 *
 * The caller must:
 *
 * ensure there is room in the CQ(s), SQ, and RQ for drain work requests
 * and completions.
 *
 * allocate the CQs using ib_alloc_cq() and the CQ poll context cannot be
 * IB_POLL_DIRECT.
 *
 * ensure that there are no other contexts that are posting WRs concurrently.
 * Otherwise the drain is not guaranteed.
 */
void ib_drain_qp(struct ib_qp *qp)
{
	ib_drain_sq(qp);
	if (!qp->srq)
		ib_drain_rq(qp);
}
EXPORT_SYMBOL(ib_drain_qp);
