/*-
 * SPDX-License-Identifier: BSD-2-Clause OR GPL-2.0
 *
 * Copyright (c) 2005 Voltaire Inc.  All rights reserved.
 * Copyright (c) 2005 Intel Corporation.  All rights reserved.
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

#if !defined(IB_ADDR_H)
#define IB_ADDR_H

#include <linux/in.h>
#include <linux/in6.h>
#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <linux/socket.h>
#include <linux/if_vlan.h>
#include <net/ipv6.h>
#include <net/if_inet6.h>
#include <net/ip.h>
#include <rdma/ib_verbs.h>
#include <rdma/ib_pack.h>
#include <rdma/ib_addr_freebsd.h>

/* Linux netdevice.h but for working on an ifnet rather than a net_device. */
#define	dev_hold(d)	if_ref(d)
#define	dev_put(d)	if_rele(d)
#define	dev_net(d)	if_getvnet(d)
#define	net_eq(a,b)	((a) == (b))


struct rdma_addr_client {
	atomic_t refcount;
	struct completion comp;
};

union rdma_sockaddr {
	struct sockaddr         _sockaddr;
	struct sockaddr_in      _sockaddr_in;
	struct sockaddr_in6     _sockaddr_in6;
	struct sockaddr_storage _sockaddr_ss;
};

/**
 * rdma_addr_register_client - Register an address client.
 */
void rdma_addr_register_client(struct rdma_addr_client *client);

/**
 * rdma_addr_unregister_client - Deregister an address client.
 * @client: Client object to deregister.
 */
void rdma_addr_unregister_client(struct rdma_addr_client *client);

/**
 * struct rdma_dev_addr - Contains resolved RDMA hardware addresses
 * @src_dev_addr:	Source MAC address.
 * @dst_dev_addr:	Destination MAC address.
 * @broadcast:		Broadcast address of the device.
 * @dev_type:		The interface hardware type of the device.
 * @bound_dev_if:	An optional device interface index.
 * @transport:		The transport type used.
 * @net:		Network namespace containing the bound_dev_if net_dev.
 */
struct vnet;
struct rdma_dev_addr {
	unsigned char src_dev_addr[MAX_ADDR_LEN];
	unsigned char dst_dev_addr[MAX_ADDR_LEN];
	unsigned char broadcast[MAX_ADDR_LEN];
	unsigned short dev_type;
	int bound_dev_if;
	enum rdma_transport_type transport;
	struct vnet *net;
	enum rdma_network_type network;
	int hoplimit;
};

/**
 * rdma_translate_ip - Translate a local IP address to an RDMA hardware
 *   address.
 *
 * The dev_addr->net and dev_addr->bound_dev_if fields must be initialized.
 */
int rdma_translate_ip(const struct sockaddr *addr,
		      struct rdma_dev_addr *dev_addr);

/**
 * rdma_resolve_ip - Resolve source and destination IP addresses to
 *   RDMA hardware addresses.
 * @client: Address client associated with request.
 * @src_addr: An optional source address to use in the resolution.  If a
 *   source address is not provided, a usable address will be returned via
 *   the callback.
 * @dst_addr: The destination address to resolve.
 * @addr: A reference to a data location that will receive the resolved
 *   addresses.  The data location must remain valid until the callback has
 *   been invoked. The net field of the addr struct must be valid.
 * @timeout_ms: Amount of time to wait for the address resolution to complete.
 * @callback: Call invoked once address resolution has completed, timed out,
 *   or been canceled.  A status of 0 indicates success.
 * @context: User-specified context associated with the call.
 */
int rdma_resolve_ip(struct rdma_addr_client *client,
		    struct sockaddr *src_addr, struct sockaddr *dst_addr,
		    struct rdma_dev_addr *addr, int timeout_ms,
		    void (*callback)(int status, struct sockaddr *src_addr,
				     struct rdma_dev_addr *addr, void *context),
		    void *context);

int rdma_resolve_ip_route(struct sockaddr *src_addr,
			  const struct sockaddr *dst_addr,
			  struct rdma_dev_addr *addr);

void rdma_addr_cancel(struct rdma_dev_addr *addr);

int rdma_copy_addr(struct rdma_dev_addr *dev_addr, if_t dev,
	      const unsigned char *dst_dev_addr);

int rdma_addr_size(struct sockaddr *addr);
int rdma_addr_size_in6(struct sockaddr_in6 *addr);
int rdma_addr_size_kss(struct sockaddr_storage *addr);

int rdma_addr_find_l2_eth_by_grh(const union ib_gid *sgid,
				 const union ib_gid *dgid,
				 u8 *smac, if_t dev,
				 int *hoplimit);

static inline u16 ib_addr_get_pkey(struct rdma_dev_addr *dev_addr)
{
	return ((u16)dev_addr->broadcast[8] << 8) | (u16)dev_addr->broadcast[9];
}

static inline void ib_addr_set_pkey(struct rdma_dev_addr *dev_addr, u16 pkey)
{
	dev_addr->broadcast[8] = pkey >> 8;
	dev_addr->broadcast[9] = (unsigned char) pkey;
}

static inline void ib_addr_get_mgid(struct rdma_dev_addr *dev_addr,
				    union ib_gid *gid)
{
	memcpy(gid, dev_addr->broadcast + 4, sizeof *gid);
}

static inline int rdma_addr_gid_offset(struct rdma_dev_addr *dev_addr)
{
	return dev_addr->dev_type == ARPHRD_INFINIBAND ? 4 : 0;
}

static inline u16 rdma_vlan_dev_vlan_id(if_t dev)
{
	uint16_t tag;

	if (if_gettype(dev) == IFT_ETHER && if_getpcp(dev) != IFNET_PCP_NONE)
		return 0x0000;	/* prio-tagged traffic */
	if (VLAN_TAG(__DECONST(if_t, dev), &tag) != 0)
		return 0xffff;
	return tag;
}

static inline int rdma_ip2gid(const struct sockaddr *addr, union ib_gid *gid)
{
	switch (addr->sa_family) {
	case AF_INET:
		ipv6_addr_set_v4mapped(((const struct sockaddr_in *)
					addr)->sin_addr.s_addr,
				       (struct in6_addr *)gid);
		break;
	case AF_INET6:
		memcpy(gid->raw, &((const struct sockaddr_in6 *)addr)->sin6_addr, 16);
		/* make sure scope ID gets zeroed inside GID */
		if (IN6_IS_SCOPE_LINKLOCAL((struct in6_addr *)gid->raw) ||
		    IN6_IS_ADDR_MC_INTFACELOCAL((struct in6_addr *)gid->raw)) {
			gid->raw[2] = 0;
			gid->raw[3] = 0;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/* Important - sockaddr should be a union of sockaddr_in and sockaddr_in6 */
static inline void rdma_gid2ip(struct sockaddr *out, const union ib_gid *gid)
{
	if (ipv6_addr_v4mapped((const struct in6_addr *)gid)) {
		struct sockaddr_in *out_in = (struct sockaddr_in *)out;
		memset(out_in, 0, sizeof(*out_in));
		out_in->sin_len = sizeof(*out_in);
		out_in->sin_family = AF_INET;
		memcpy(&out_in->sin_addr.s_addr, gid->raw + 12, 4);
	} else {
		struct sockaddr_in6 *out_in = (struct sockaddr_in6 *)out;
		memset(out_in, 0, sizeof(*out_in));
		out_in->sin6_len = sizeof(*out_in);
		out_in->sin6_family = AF_INET6;
		memcpy(&out_in->sin6_addr.s6_addr, gid->raw, 16);
	}
}

static u_int
_iboe_addr_get_sgid_ia_cb(void *arg, struct ifaddr *ifa, u_int count __unused)
{
	ipv6_addr_set_v4mapped(((struct sockaddr_in *)
			       ifa->ifa_addr)->sin_addr.s_addr,
			       (struct in6_addr *)arg);
	return (0);
}

static inline void iboe_addr_get_sgid(struct rdma_dev_addr *dev_addr,
				      union ib_gid *gid)
{
	if_t dev;

#ifdef VIMAGE
	if (dev_addr->net == NULL)
		return;
#endif
	dev = dev_get_by_index(dev_addr->net, dev_addr->bound_dev_if);
	if (dev) {
		if_foreach_addr_type(dev, AF_INET,
				     _iboe_addr_get_sgid_ia_cb, gid);
		dev_put(dev);
	}
}

static inline void rdma_addr_get_sgid(struct rdma_dev_addr *dev_addr, union ib_gid *gid)
{
	if (dev_addr->transport == RDMA_TRANSPORT_IB &&
	    dev_addr->dev_type != ARPHRD_INFINIBAND)
		iboe_addr_get_sgid(dev_addr, gid);
	else
		memcpy(gid, dev_addr->src_dev_addr +
		       rdma_addr_gid_offset(dev_addr), sizeof *gid);
}

static inline void rdma_addr_set_sgid(struct rdma_dev_addr *dev_addr, union ib_gid *gid)
{
	memcpy(dev_addr->src_dev_addr + rdma_addr_gid_offset(dev_addr), gid, sizeof *gid);
}

static inline void rdma_addr_get_dgid(struct rdma_dev_addr *dev_addr, union ib_gid *gid)
{
	memcpy(gid, dev_addr->dst_dev_addr + rdma_addr_gid_offset(dev_addr), sizeof *gid);
}

static inline void rdma_addr_set_dgid(struct rdma_dev_addr *dev_addr, union ib_gid *gid)
{
	memcpy(dev_addr->dst_dev_addr + rdma_addr_gid_offset(dev_addr), gid, sizeof *gid);
}

static inline enum ib_mtu iboe_get_mtu(int mtu)
{
	/*
	 * reduce IB headers from effective IBoE MTU. 28 stands for
	 * atomic header which is the biggest possible header after BTH
	 */
	mtu = mtu - IB_GRH_BYTES - IB_BTH_BYTES - 28;

	if (mtu >= ib_mtu_enum_to_int(IB_MTU_4096))
		return IB_MTU_4096;
	else if (mtu >= ib_mtu_enum_to_int(IB_MTU_2048))
		return IB_MTU_2048;
	else if (mtu >= ib_mtu_enum_to_int(IB_MTU_1024))
		return IB_MTU_1024;
	else if (mtu >= ib_mtu_enum_to_int(IB_MTU_512))
		return IB_MTU_512;
	else if (mtu >= ib_mtu_enum_to_int(IB_MTU_256))
		return IB_MTU_256;
	else
		return 0;
}

static inline int iboe_get_rate(if_t dev)
{
	uint64_t baudrate = if_getbaudrate(dev);
#ifdef if_baudrate_pf
	int exp;
	for (exp = dev->if_baudrate_pf; exp > 0; exp--)
		baudrate *= 10;
#endif
	if (baudrate >= IF_Gbps(40))
		return IB_RATE_40_GBPS;
	else if (baudrate >= IF_Gbps(30))
		return IB_RATE_30_GBPS;
	else if (baudrate >= IF_Gbps(20))
		return IB_RATE_20_GBPS;
	else if (baudrate >= IF_Gbps(10))
		return IB_RATE_10_GBPS;
	else
		return IB_RATE_PORT_CURRENT;
}

static inline int rdma_link_local_addr(struct in6_addr *addr)
{
	if (addr->s6_addr32[0] == htonl(0xfe800000) &&
	    addr->s6_addr32[1] == 0)
		return 1;

	return 0;
}

static inline void rdma_get_ll_mac(struct in6_addr *addr, u8 *mac)
{
	memcpy(mac, &addr->s6_addr[8], 3);
	memcpy(mac + 3, &addr->s6_addr[13], 3);
	mac[0] ^= 2;
}

static inline int rdma_is_multicast_addr(struct in6_addr *addr)
{
	__be32 ipv4_addr;

	if (addr->s6_addr[0] == 0xff)
		return 1;

	ipv4_addr = addr->s6_addr32[3];
	return (ipv6_addr_v4mapped(addr) && ipv4_is_multicast(ipv4_addr));
}

static inline void rdma_get_mcast_mac(struct in6_addr *addr, u8 *mac)
{
	int i;

	mac[0] = 0x33;
	mac[1] = 0x33;
	for (i = 2; i < 6; ++i)
		mac[i] = addr->s6_addr[i + 10];
}

static inline u16 rdma_get_vlan_id(union ib_gid *dgid)
{
	u16 vid;

	vid = dgid->raw[11] << 8 | dgid->raw[12];
	return vid < 0x1000 ? vid : 0xffff;
}

static inline if_t rdma_vlan_dev_real_dev(if_t dev)
{
	struct epoch_tracker et;

	NET_EPOCH_ENTER(et);
	if (if_gettype(dev) != IFT_ETHER || if_getpcp(dev) == IFNET_PCP_NONE)
		dev = VLAN_TRUNKDEV(dev);	/* non prio-tagged traffic */
	NET_EPOCH_EXIT(et);
	return (dev);
}

#endif /* IB_ADDR_H */
