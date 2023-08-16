/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (c) 2021 - 2022 Intel Corporation
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenFabrics.org BSD license below:
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *    - Redistributions of source code must retain the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer.
 *
 *    - Redistributions in binary form must reproduce the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer in the documentation and/or other materials
 *	provided with the distribution.
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

#include "osdep.h"
#include "ice_rdma.h"
#include "irdma_di_if.h"
#include "irdma_main.h"
#include <sys/gsb_crc32.h>
#include <netinet/in_fib.h>
#include <netinet6/in6_fib.h>
#include <net/route/nhop.h>
#include <net/if_llatbl.h>

/* additional QP debuging option. Keep false unless needed */
bool irdma_upload_context = false;

inline u32
irdma_rd32(struct irdma_dev_ctx *dev_ctx, u32 reg){

	KASSERT(reg < dev_ctx->mem_bus_space_size,
		("irdma: register offset %#jx too large (max is %#jx)",
		 (uintmax_t)reg, (uintmax_t)dev_ctx->mem_bus_space_size));

	return (bus_space_read_4(dev_ctx->mem_bus_space_tag,
				 dev_ctx->mem_bus_space_handle, reg));
}

inline void
irdma_wr32(struct irdma_dev_ctx *dev_ctx, u32 reg, u32 value)
{

	KASSERT(reg < dev_ctx->mem_bus_space_size,
		("irdma: register offset %#jx too large (max is %#jx)",
		 (uintmax_t)reg, (uintmax_t)dev_ctx->mem_bus_space_size));

	bus_space_write_4(dev_ctx->mem_bus_space_tag,
			  dev_ctx->mem_bus_space_handle, reg, value);
}

inline u64
irdma_rd64(struct irdma_dev_ctx *dev_ctx, u32 reg){

	KASSERT(reg < dev_ctx->mem_bus_space_size,
		("irdma: register offset %#jx too large (max is %#jx)",
		 (uintmax_t)reg, (uintmax_t)dev_ctx->mem_bus_space_size));

	return (bus_space_read_8(dev_ctx->mem_bus_space_tag,
				 dev_ctx->mem_bus_space_handle, reg));
}

inline void
irdma_wr64(struct irdma_dev_ctx *dev_ctx, u32 reg, u64 value)
{

	KASSERT(reg < dev_ctx->mem_bus_space_size,
		("irdma: register offset %#jx too large (max is %#jx)",
		 (uintmax_t)reg, (uintmax_t)dev_ctx->mem_bus_space_size));

	bus_space_write_8(dev_ctx->mem_bus_space_tag,
			  dev_ctx->mem_bus_space_handle, reg, value);

}

void
irdma_request_reset(struct irdma_pci_f *rf)
{
	struct ice_rdma_peer *peer = rf->peer_info;
	struct ice_rdma_request req = {0};

	req.type = ICE_RDMA_EVENT_RESET;

	printf("%s:%d requesting pf-reset\n", __func__, __LINE__);
	IRDMA_DI_REQ_HANDLER(peer, &req);
}

int
irdma_register_qset(struct irdma_sc_vsi *vsi, struct irdma_ws_node *tc_node)
{
	struct irdma_device *iwdev = vsi->back_vsi;
	struct ice_rdma_peer *peer = iwdev->rf->peer_info;
	struct ice_rdma_request req = {0};
	struct ice_rdma_qset_update *res = &req.res;

	req.type = ICE_RDMA_EVENT_QSET_REGISTER;
	res->cnt_req = 1;
	res->res_type = ICE_RDMA_QSET_ALLOC;
	res->qsets.qs_handle = tc_node->qs_handle;
	res->qsets.tc = tc_node->traffic_class;
	res->qsets.vsi_id = vsi->vsi_idx;

	IRDMA_DI_REQ_HANDLER(peer, &req);

	tc_node->l2_sched_node_id = res->qsets.teid;
	vsi->qos[tc_node->user_pri].l2_sched_node_id =
	    res->qsets.teid;

	return 0;
}

void
irdma_unregister_qset(struct irdma_sc_vsi *vsi, struct irdma_ws_node *tc_node)
{
	struct irdma_device *iwdev = vsi->back_vsi;
	struct ice_rdma_peer *peer = iwdev->rf->peer_info;
	struct ice_rdma_request req = {0};
	struct ice_rdma_qset_update *res = &req.res;

	req.type = ICE_RDMA_EVENT_QSET_REGISTER;
	res->res_allocated = 1;
	res->res_type = ICE_RDMA_QSET_FREE;
	res->qsets.vsi_id = vsi->vsi_idx;
	res->qsets.teid = tc_node->l2_sched_node_id;
	res->qsets.qs_handle = tc_node->qs_handle;

	IRDMA_DI_REQ_HANDLER(peer, &req);
}

void *
hw_to_dev(struct irdma_hw *hw)
{
	struct irdma_pci_f *rf;

	rf = container_of(hw, struct irdma_pci_f, hw);
	return rf->pcidev;
}

void
irdma_free_hash_desc(void *desc)
{
	return;
}

int
irdma_init_hash_desc(void **desc)
{
	return 0;
}

int
irdma_ieq_check_mpacrc(void *desc,
		       void *addr, u32 len, u32 val)
{
	u32 crc = calculate_crc32c(0xffffffff, addr, len) ^ 0xffffffff;
	int ret_code = 0;

	if (crc != val) {
		irdma_pr_err("mpa crc check fail %x %x\n", crc, val);
		ret_code = -EINVAL;
	}
	printf("%s: result crc=%x value=%x\n", __func__, crc, val);
	return ret_code;
}

static u_int
irdma_add_ipv6_cb(void *arg, struct ifaddr *addr, u_int count __unused)
{
	struct irdma_device *iwdev = arg;
	struct sockaddr_in6 *sin6;
	u32 local_ipaddr6[4] = {};
	char ip6buf[INET6_ADDRSTRLEN];
	u8 *mac_addr;

	sin6 = (struct sockaddr_in6 *)addr->ifa_addr;

	irdma_copy_ip_ntohl(local_ipaddr6, (u32 *)&sin6->sin6_addr);

	mac_addr = if_getlladdr(addr->ifa_ifp);

	printf("%s:%d IP=%s, MAC=%02x:%02x:%02x:%02x:%02x:%02x\n",
	       __func__, __LINE__,
	       ip6_sprintf(ip6buf, &sin6->sin6_addr),
	       mac_addr[0], mac_addr[1], mac_addr[2],
	       mac_addr[3], mac_addr[4], mac_addr[5]);

	irdma_manage_arp_cache(iwdev->rf, mac_addr, local_ipaddr6,
			       IRDMA_ARP_ADD);
	return (0);
}

/**
 * irdma_add_ipv6_addr - add ipv6 address to the hw arp table
 * @iwdev: irdma device
 * @ifp: interface network device pointer
 */
static void
irdma_add_ipv6_addr(struct irdma_device *iwdev, struct ifnet *ifp)
{
	if_addr_rlock(ifp);
	if_foreach_addr_type(ifp, AF_INET6, irdma_add_ipv6_cb, iwdev);
	if_addr_runlock(ifp);
}

static u_int
irdma_add_ipv4_cb(void *arg, struct ifaddr *addr, u_int count __unused)
{
	struct irdma_device *iwdev = arg;
	struct sockaddr_in *sin;
	u32 ip_addr[4] = {};
	uint8_t *mac_addr;

	sin = (struct sockaddr_in *)addr->ifa_addr;

	ip_addr[0] = ntohl(sin->sin_addr.s_addr);

	mac_addr = if_getlladdr(addr->ifa_ifp);

	printf("%s:%d IP=%d.%d.%d.%d, MAC=%02x:%02x:%02x:%02x:%02x:%02x\n",
	       __func__, __LINE__,
	       ip_addr[0] >> 24,
	       (ip_addr[0] >> 16) & 0xFF,
	       (ip_addr[0] >> 8) & 0xFF,
	       ip_addr[0] & 0xFF,
	       mac_addr[0], mac_addr[1], mac_addr[2],
	       mac_addr[3], mac_addr[4], mac_addr[5]);

	irdma_manage_arp_cache(iwdev->rf, mac_addr, ip_addr,
			       IRDMA_ARP_ADD);
	return (0);
}

/**
 * irdma_add_ipv4_addr - add ipv4 address to the hw arp table
 * @iwdev: irdma device
 * @ifp: interface network device pointer
 */
static void
irdma_add_ipv4_addr(struct irdma_device *iwdev, struct ifnet *ifp)
{
	if_addr_rlock(ifp);
	if_foreach_addr_type(ifp, AF_INET, irdma_add_ipv4_cb, iwdev);
	if_addr_runlock(ifp);
}

/**
 * irdma_add_ip - add ip addresses
 * @iwdev: irdma device
 *
 * Add ipv4/ipv6 addresses to the arp cache
 */
void
irdma_add_ip(struct irdma_device *iwdev)
{
	struct ifnet *ifp = iwdev->netdev;
	struct ifnet *ifv;
	int i;

	irdma_add_ipv4_addr(iwdev, ifp);
	irdma_add_ipv6_addr(iwdev, ifp);
	for (i = 0; if_getvlantrunk(ifp) != NULL && i < VLAN_N_VID; ++i) {
		ifv = VLAN_DEVAT(ifp, i);
		if (!ifv)
			continue;
		irdma_add_ipv4_addr(iwdev, ifv);
		irdma_add_ipv6_addr(iwdev, ifv);
	}
}

static void
irdma_ifaddrevent_handler(void *arg, struct ifnet *ifp, struct ifaddr *ifa, int event)
{
	struct irdma_pci_f *rf = arg;
	struct ifnet *ifv = NULL;
	struct sockaddr_in *sin;
	struct epoch_tracker et;
	int arp_index = 0, i = 0;
	u32 ip[4] = {};

	if (!ifa || !ifa->ifa_addr || !ifp)
		return;
	if (rf->iwdev->netdev != ifp) {
		for (i = 0; if_getvlantrunk(rf->iwdev->netdev) != NULL && i < VLAN_N_VID; ++i) {
			NET_EPOCH_ENTER(et);
			ifv = VLAN_DEVAT(rf->iwdev->netdev, i);
			NET_EPOCH_EXIT(et);
			if (ifv == ifp)
				break;
		}
		if (ifv != ifp)
			return;
	}
	sin = (struct sockaddr_in *)ifa->ifa_addr;

	switch (event) {
	case IFADDR_EVENT_ADD:
		if (sin->sin_family == AF_INET)
			irdma_add_ipv4_addr(rf->iwdev, ifp);
		else if (sin->sin_family == AF_INET6)
			irdma_add_ipv6_addr(rf->iwdev, ifp);
		break;
	case IFADDR_EVENT_DEL:
		if (sin->sin_family == AF_INET) {
			ip[0] = ntohl(sin->sin_addr.s_addr);
		} else if (sin->sin_family == AF_INET6) {
			irdma_copy_ip_ntohl(ip, (u32 *)&((struct sockaddr_in6 *)sin)->sin6_addr);
		} else {
			break;
		}
		for_each_set_bit(arp_index, rf->allocated_arps, rf->arp_table_size) {
			if (!memcmp(rf->arp_table[arp_index].ip_addr, ip, sizeof(ip))) {
				irdma_manage_arp_cache(rf, rf->arp_table[arp_index].mac_addr,
						       rf->arp_table[arp_index].ip_addr,
						       IRDMA_ARP_DELETE);
			}
		}
		break;
	default:
		break;
	}
}

void
irdma_reg_ipaddr_event_cb(struct irdma_pci_f *rf)
{
	rf->irdma_ifaddr_event = EVENTHANDLER_REGISTER(ifaddr_event_ext,
						       irdma_ifaddrevent_handler,
						       rf,
						       EVENTHANDLER_PRI_ANY);
}

void
irdma_dereg_ipaddr_event_cb(struct irdma_pci_f *rf)
{
	EVENTHANDLER_DEREGISTER(ifaddr_event_ext, rf->irdma_ifaddr_event);
}

static int
irdma_get_route_ifp(struct sockaddr *dst_sin, struct ifnet *netdev,
		    struct ifnet **ifp, struct sockaddr **nexthop, bool *gateway)
{
	struct nhop_object *nh;

	if (dst_sin->sa_family == AF_INET6)
		nh = fib6_lookup(RT_DEFAULT_FIB, &((struct sockaddr_in6 *)dst_sin)->sin6_addr, 0, NHR_NONE, 0);
	else
		nh = fib4_lookup(RT_DEFAULT_FIB, ((struct sockaddr_in *)dst_sin)->sin_addr, 0, NHR_NONE, 0);
	if (!nh || (nh->nh_ifp != netdev &&
		    rdma_vlan_dev_real_dev(nh->nh_ifp) != netdev))
		goto rt_not_found;
	*gateway = (nh->nh_flags & NHF_GATEWAY) ? true : false;
	*nexthop = (*gateway) ? &nh->gw_sa : dst_sin;
	*ifp = nh->nh_ifp;

	return 0;

rt_not_found:
	pr_err("irdma: route not found\n");
	return -ENETUNREACH;
}

/**
 * irdma_get_dst_mac - get destination mac address
 * @cm_node: connection's node
 * @dst_sin: destination address information
 * @dst_mac: mac address array to return
 */
int
irdma_get_dst_mac(struct irdma_cm_node *cm_node, struct sockaddr *dst_sin, u8 *dst_mac)
{
	struct ifnet *netdev = cm_node->iwdev->netdev;
#ifdef VIMAGE
	struct rdma_cm_id *rdma_id = (struct rdma_cm_id *)cm_node->cm_id->context;
	struct vnet *vnet = rdma_id->route.addr.dev_addr.net;
#endif
	struct ifnet *ifp;
	struct llentry *lle;
	struct sockaddr *nexthop;
	struct epoch_tracker et;
	int err;
	bool gateway;

	NET_EPOCH_ENTER(et);
	CURVNET_SET_QUIET(vnet);
	err = irdma_get_route_ifp(dst_sin, netdev, &ifp, &nexthop, &gateway);
	if (err)
		goto get_route_fail;

	if (dst_sin->sa_family == AF_INET) {
		err = arpresolve(ifp, gateway, NULL, nexthop, dst_mac, NULL, &lle);
	} else if (dst_sin->sa_family == AF_INET6) {
		err = nd6_resolve(ifp, LLE_SF(AF_INET6, gateway), NULL, nexthop,
				  dst_mac, NULL, &lle);
	} else {
		err = -EPROTONOSUPPORT;
	}

get_route_fail:
	CURVNET_RESTORE();
	NET_EPOCH_EXIT(et);
	if (err) {
		pr_err("failed to resolve neighbor address (err=%d)\n",
		       err);
		return -ENETUNREACH;
	}

	return 0;
}

/**
 * irdma_addr_resolve_neigh - resolve neighbor address
 * @cm_node: connection's node
 * @dst_ip: remote ip address
 * @arpindex: if there is an arp entry
 */
int
irdma_addr_resolve_neigh(struct irdma_cm_node *cm_node,
			 u32 dst_ip, int arpindex)
{
	struct irdma_device *iwdev = cm_node->iwdev;
	struct sockaddr_in dst_sin = {};
	int err;
	u32 ip[4] = {};
	u8 dst_mac[MAX_ADDR_LEN];

	dst_sin.sin_len = sizeof(dst_sin);
	dst_sin.sin_family = AF_INET;
	dst_sin.sin_port = 0;
	dst_sin.sin_addr.s_addr = htonl(dst_ip);

	err = irdma_get_dst_mac(cm_node, (struct sockaddr *)&dst_sin, dst_mac);
	if (err)
		return arpindex;

	ip[0] = dst_ip;

	return irdma_add_arp(iwdev->rf, ip, dst_mac);
}

/**
 * irdma_addr_resolve_neigh_ipv6 - resolve neighbor ipv6 address
 * @cm_node: connection's node
 * @dest: remote ip address
 * @arpindex: if there is an arp entry
 */
int
irdma_addr_resolve_neigh_ipv6(struct irdma_cm_node *cm_node,
			      u32 *dest, int arpindex)
{
	struct irdma_device *iwdev = cm_node->iwdev;
	struct sockaddr_in6 dst_addr = {};
	int err;
	u8 dst_mac[MAX_ADDR_LEN];

	dst_addr.sin6_family = AF_INET6;
	dst_addr.sin6_len = sizeof(dst_addr);
	dst_addr.sin6_scope_id = if_getindex(iwdev->netdev);

	irdma_copy_ip_htonl(dst_addr.sin6_addr.__u6_addr.__u6_addr32, dest);
	err = irdma_get_dst_mac(cm_node, (struct sockaddr *)&dst_addr, dst_mac);
	if (err)
		return arpindex;

	return irdma_add_arp(iwdev->rf, dest, dst_mac);
}

int
irdma_resolve_neigh_lpb_chk(struct irdma_device *iwdev, struct irdma_cm_node *cm_node,
			    struct irdma_cm_info *cm_info)
{
#ifdef VIMAGE
	struct rdma_cm_id *rdma_id = (struct rdma_cm_id *)cm_node->cm_id->context;
	struct vnet *vnet = rdma_id->route.addr.dev_addr.net;
#endif
	int arpindex;
	int oldarpindex;
	bool is_lpb = false;

	CURVNET_SET_QUIET(vnet);
	is_lpb = cm_node->ipv4 ?
	    irdma_ipv4_is_lpb(cm_node->loc_addr[0], cm_node->rem_addr[0]) :
	    irdma_ipv6_is_lpb(cm_node->loc_addr, cm_node->rem_addr);
	CURVNET_RESTORE();
	if (is_lpb) {
		cm_node->do_lpb = true;
		arpindex = irdma_arp_table(iwdev->rf, cm_node->rem_addr,
					   NULL,
					   IRDMA_ARP_RESOLVE);
	} else {
		oldarpindex = irdma_arp_table(iwdev->rf, cm_node->rem_addr,
					      NULL,
					      IRDMA_ARP_RESOLVE);
		if (cm_node->ipv4)
			arpindex = irdma_addr_resolve_neigh(cm_node,
							    cm_info->rem_addr[0],
							    oldarpindex);
		else
			arpindex = irdma_addr_resolve_neigh_ipv6(cm_node,
								 cm_info->rem_addr,
								 oldarpindex);
	}
	return arpindex;
}

/**
 * irdma_add_handler - add a handler to the list
 * @hdl: handler to be added to the handler list
 */
void
irdma_add_handler(struct irdma_handler *hdl)
{
	unsigned long flags;

	spin_lock_irqsave(&irdma_handler_lock, flags);
	list_add(&hdl->list, &irdma_handlers);
	spin_unlock_irqrestore(&irdma_handler_lock, flags);
}

/**
 * irdma_del_handler - delete a handler from the list
 * @hdl: handler to be deleted from the handler list
 */
void
irdma_del_handler(struct irdma_handler *hdl)
{
	unsigned long flags;

	spin_lock_irqsave(&irdma_handler_lock, flags);
	list_del(&hdl->list);
	spin_unlock_irqrestore(&irdma_handler_lock, flags);
}

/**
 * irdma_set_rf_user_cfg_params - apply user configurable settings
 * @rf: RDMA PCI function
 */
void
irdma_set_rf_user_cfg_params(struct irdma_pci_f *rf)
{
	int en_rem_endpoint_trk = 0;
	int limits_sel = 4;

	rf->en_rem_endpoint_trk = en_rem_endpoint_trk;
	rf->limits_sel = limits_sel;
	rf->rst_to = IRDMA_RST_TIMEOUT_HZ;
	/* Enable DCQCN algorithm by default */
	rf->dcqcn_ena = true;
}

/**
 * irdma_sysctl_dcqcn_update - handle dcqcn_ena sysctl update
 * @arg1: pointer to rf
 * @arg2: unused
 * @oidp: sysctl oid structure
 * @req: sysctl request pointer
 */
static int
irdma_sysctl_dcqcn_update(SYSCTL_HANDLER_ARGS)
{
	struct irdma_pci_f *rf = (struct irdma_pci_f *)arg1;
	int ret;
	u8 dcqcn_ena = rf->dcqcn_ena;

	ret = sysctl_handle_8(oidp, &dcqcn_ena, 0, req);
	if ((ret) || (req->newptr == NULL))
		return ret;
	if (dcqcn_ena == 0)
		rf->dcqcn_ena = false;
	else
		rf->dcqcn_ena = true;

	return 0;
}

/**
 * irdma_dcqcn_tunables_init - create tunables for dcqcn settings
 * @rf: RDMA PCI function
 *
 * Create DCQCN related sysctls for the driver.
 * dcqcn_ena is writeable settings and applicable to next QP creation or
 * context setting.
 * all other settings are of RDTUN type (read on driver load) and are
 * applicable only to CQP creation.
 */
void
irdma_dcqcn_tunables_init(struct irdma_pci_f *rf)
{
	struct sysctl_oid_list *irdma_sysctl_oid_list;

	irdma_sysctl_oid_list = SYSCTL_CHILDREN(rf->tun_info.irdma_sysctl_tree);

	SYSCTL_ADD_PROC(&rf->tun_info.irdma_sysctl_ctx, irdma_sysctl_oid_list,
			OID_AUTO, "dcqcn_enable", CTLFLAG_RW | CTLTYPE_U8, rf, 0,
			irdma_sysctl_dcqcn_update, "A",
			"enables DCQCN algorithm for RoCEv2 on all ports, default=true");

	SYSCTL_ADD_U8(&rf->tun_info.irdma_sysctl_ctx, irdma_sysctl_oid_list,
		      OID_AUTO, "dcqcn_cc_cfg_valid", CTLFLAG_RDTUN,
		      &rf->dcqcn_params.cc_cfg_valid, 0,
		      "set DCQCN parameters to be valid, default=false");

	rf->dcqcn_params.min_dec_factor = 1;
	SYSCTL_ADD_U8(&rf->tun_info.irdma_sysctl_ctx, irdma_sysctl_oid_list,
		      OID_AUTO, "dcqcn_min_dec_factor", CTLFLAG_RDTUN,
		      &rf->dcqcn_params.min_dec_factor, 0,
		    "set minimum percentage factor by which tx rate can be changed for CNP, Range: 1-100, default=1");

	SYSCTL_ADD_U8(&rf->tun_info.irdma_sysctl_ctx, irdma_sysctl_oid_list,
		      OID_AUTO, "dcqcn_min_rate_MBps", CTLFLAG_RDTUN,
		      &rf->dcqcn_params.min_rate, 0,
		      "set minimum rate limit value, in MBits per second, default=0");

	rf->dcqcn_params.dcqcn_f = 5;
	SYSCTL_ADD_U8(&rf->tun_info.irdma_sysctl_ctx, irdma_sysctl_oid_list,
		      OID_AUTO, "dcqcn_F", CTLFLAG_RDTUN, &rf->dcqcn_params.dcqcn_f, 0,
		      "set number of times to stay in each stage of bandwidth recovery, default=5");

	rf->dcqcn_params.dcqcn_t = 0x37;
	SYSCTL_ADD_U16(&rf->tun_info.irdma_sysctl_ctx, irdma_sysctl_oid_list,
		       OID_AUTO, "dcqcn_T", CTLFLAG_RDTUN, &rf->dcqcn_params.dcqcn_t, 0,
		       "set number of usecs that should elapse before increasing the CWND in DCQCN mode, default=0x37");

	rf->dcqcn_params.dcqcn_b = 0x249f0;
	SYSCTL_ADD_U32(&rf->tun_info.irdma_sysctl_ctx, irdma_sysctl_oid_list,
		       OID_AUTO, "dcqcn_B", CTLFLAG_RDTUN, &rf->dcqcn_params.dcqcn_b, 0,
		       "set number of MSS to add to the congestion window in additive increase mode, default=0x249f0");

	rf->dcqcn_params.rai_factor = 1;
	SYSCTL_ADD_U16(&rf->tun_info.irdma_sysctl_ctx, irdma_sysctl_oid_list,
		       OID_AUTO, "dcqcn_rai_factor", CTLFLAG_RDTUN,
		       &rf->dcqcn_params.rai_factor, 0,
		       "set number of MSS to add to the congestion window in additive increase mode, default=1");

	rf->dcqcn_params.hai_factor = 5;
	SYSCTL_ADD_U16(&rf->tun_info.irdma_sysctl_ctx, irdma_sysctl_oid_list,
		       OID_AUTO, "dcqcn_hai_factor", CTLFLAG_RDTUN,
		       &rf->dcqcn_params.hai_factor, 0,
		       "set number of MSS to add to the congestion window in hyperactive increase mode, default=5");

	rf->dcqcn_params.rreduce_mperiod = 50;
	SYSCTL_ADD_U32(&rf->tun_info.irdma_sysctl_ctx, irdma_sysctl_oid_list,
		       OID_AUTO, "dcqcn_rreduce_mperiod", CTLFLAG_RDTUN,
		       &rf->dcqcn_params.rreduce_mperiod, 0,
		       "set minimum time between 2 consecutive rate reductions for a single flow, default=50");
}

/**
 * irdma_dmamap_cb - callback for bus_dmamap_load
 */
static void
irdma_dmamap_cb(void *arg, bus_dma_segment_t * segs, int nseg, int error)
{
	if (error)
		return;
	*(bus_addr_t *) arg = segs->ds_addr;
	return;
}

/**
 * irdma_allocate_dma_mem - allocate dma memory
 * @hw: pointer to hw structure
 * @mem: structure holding memory information
 * @size: requested size
 * @alignment: requested alignment
 */
void *
irdma_allocate_dma_mem(struct irdma_hw *hw, struct irdma_dma_mem *mem,
		       u64 size, u32 alignment)
{
	struct irdma_dev_ctx *dev_ctx = (struct irdma_dev_ctx *)hw->dev_context;
	device_t dev = dev_ctx->dev;
	void *va;
	int ret;

	ret = bus_dma_tag_create(bus_get_dma_tag(dev),	/* parent */
				 alignment, 0,	/* alignment, bounds */
				 BUS_SPACE_MAXADDR,	/* lowaddr */
				 BUS_SPACE_MAXADDR,	/* highaddr */
				 NULL, NULL,	/* filter, filterarg */
				 size,	/* maxsize */
				 1,	/* nsegments */
				 size,	/* maxsegsize */
				 BUS_DMA_ALLOCNOW,	/* flags */
				 NULL,	/* lockfunc */
				 NULL,	/* lockfuncarg */
				 &mem->tag);
	if (ret != 0) {
		device_printf(dev, "%s: bus_dma_tag_create failed, error %u\n",
			      __func__, ret);
		goto fail_0;
	}
	ret = bus_dmamem_alloc(mem->tag, (void **)&va,
			       BUS_DMA_NOWAIT | BUS_DMA_ZERO, &mem->map);
	if (ret != 0) {
		device_printf(dev, "%s: bus_dmamem_alloc failed, error %u\n",
			      __func__, ret);
		goto fail_1;
	}
	ret = bus_dmamap_load(mem->tag, mem->map, va, size,
			      irdma_dmamap_cb, &mem->pa, BUS_DMA_NOWAIT);
	if (ret != 0) {
		device_printf(dev, "%s: bus_dmamap_load failed, error %u\n",
			      __func__, ret);
		goto fail_2;
	}
	mem->nseg = 1;
	mem->size = size;
	bus_dmamap_sync(mem->tag, mem->map,
			BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return va;
fail_2:
	bus_dmamem_free(mem->tag, va, mem->map);
fail_1:
	bus_dma_tag_destroy(mem->tag);
fail_0:
	mem->map = NULL;
	mem->tag = NULL;

	return NULL;
}

/**
 * irdma_free_dma_mem - Memory free helper fn
 * @hw: pointer to hw structure
 * @mem: ptr to mem struct to free
 */
int
irdma_free_dma_mem(struct irdma_hw *hw, struct irdma_dma_mem *mem)
{
	if (!mem)
		return -EINVAL;
	bus_dmamap_sync(mem->tag, mem->map,
			BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(mem->tag, mem->map);
	if (!mem->va)
		return -ENOMEM;
	bus_dmamem_free(mem->tag, mem->va, mem->map);
	bus_dma_tag_destroy(mem->tag);

	mem->va = NULL;

	return 0;
}

inline void
irdma_prm_rem_bitmapmem(struct irdma_hw *hw, struct irdma_chunk *chunk)
{
	kfree(chunk->bitmapmem.va);
}

void
irdma_cleanup_dead_qps(struct irdma_sc_vsi *vsi)
{
	struct irdma_sc_qp *qp = NULL;
	struct irdma_qp *iwqp;
	struct irdma_pci_f *rf;
	u8 i;

	for (i = 0; i < IRDMA_MAX_USER_PRIORITY; i++) {
		qp = irdma_get_qp_from_list(&vsi->qos[i].qplist, qp);
		while (qp) {
			if (qp->qp_uk.qp_type == IRDMA_QP_TYPE_UDA) {
				qp = irdma_get_qp_from_list(&vsi->qos[i].qplist, qp);
				continue;
			}
			iwqp = qp->qp_uk.back_qp;
			rf = iwqp->iwdev->rf;
			irdma_free_dma_mem(rf->sc_dev.hw, &iwqp->q2_ctx_mem);
			irdma_free_dma_mem(rf->sc_dev.hw, &iwqp->kqp.dma_mem);

			kfree(iwqp->kqp.sq_wrid_mem);
			kfree(iwqp->kqp.rq_wrid_mem);
			qp = irdma_get_qp_from_list(&vsi->qos[i].qplist, qp);
			kfree(iwqp);
		}
	}
}
