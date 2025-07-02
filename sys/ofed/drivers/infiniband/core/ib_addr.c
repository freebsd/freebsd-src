/*-
 * SPDX-License-Identifier: BSD-2-Clause OR GPL-2.0
 *
 * Copyright (c) 2005 Voltaire Inc.  All rights reserved.
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 * Copyright (c) 1999-2019, Mellanox Technologies, Inc. All rights reserved.
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

#include <sys/cdefs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <net/if_llatbl.h>
#include <net/route.h>
#include <net/route/nhop.h>
#include <net/netevent.h>
#include <rdma/ib_addr.h>
#include <rdma/ib.h>

#include <netinet/in_fib.h>
#include <netinet/if_ether.h>
#include <netinet/ip_var.h>
#include <netinet6/scope6_var.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/in6_fib.h>

#include "core_priv.h"

struct addr_req {
	struct list_head list;
	struct sockaddr_storage src_addr;
	struct sockaddr_storage dst_addr;
	struct rdma_dev_addr *addr;
	struct rdma_addr_client *client;
	void *context;
	void (*callback)(int status, struct sockaddr *src_addr,
			 struct rdma_dev_addr *addr, void *context);
	unsigned long timeout;
	int status;
};

static void process_req(struct work_struct *work);

static DEFINE_MUTEX(lock);
static LIST_HEAD(req_list);
static DECLARE_DELAYED_WORK(work, process_req);
static struct workqueue_struct *addr_wq;

int rdma_addr_size(struct sockaddr *addr)
{
	switch (addr->sa_family) {
	case AF_INET:
		return sizeof(struct sockaddr_in);
	case AF_INET6:
		return sizeof(struct sockaddr_in6);
	case AF_IB:
		return sizeof(struct sockaddr_ib);
	default:
		return 0;
	}
}
EXPORT_SYMBOL(rdma_addr_size);

int rdma_addr_size_in6(struct sockaddr_in6 *addr)
{
	int ret = rdma_addr_size((struct sockaddr *) addr);

	return ret <= sizeof(*addr) ? ret : 0;
}
EXPORT_SYMBOL(rdma_addr_size_in6);

int rdma_addr_size_kss(struct sockaddr_storage *addr)
{
	int ret = rdma_addr_size((struct sockaddr *) addr);

	return ret <= sizeof(*addr) ? ret : 0;
}
EXPORT_SYMBOL(rdma_addr_size_kss);

static struct rdma_addr_client self;

void rdma_addr_register_client(struct rdma_addr_client *client)
{
	atomic_set(&client->refcount, 1);
	init_completion(&client->comp);
}
EXPORT_SYMBOL(rdma_addr_register_client);

static inline void put_client(struct rdma_addr_client *client)
{
	if (atomic_dec_and_test(&client->refcount))
		complete(&client->comp);
}

void rdma_addr_unregister_client(struct rdma_addr_client *client)
{
	put_client(client);
	wait_for_completion(&client->comp);
}
EXPORT_SYMBOL(rdma_addr_unregister_client);

static inline void
rdma_copy_addr_sub(u8 *dst, const u8 *src, unsigned min, unsigned max)
{
	if (min > max)
		min = max;
	memcpy(dst, src, min);
	memset(dst + min, 0, max - min);
}

int rdma_copy_addr(struct rdma_dev_addr *dev_addr, if_t dev,
		     const unsigned char *dst_dev_addr)
{
	int dev_type = if_gettype(dev);

	/* check for loopback device */
	if (if_getflags(dev) & IFF_LOOPBACK) {
		dev_addr->dev_type = ARPHRD_ETHER;
		memset(dev_addr->src_dev_addr, 0, MAX_ADDR_LEN);
		memset(dev_addr->broadcast, 0, MAX_ADDR_LEN);
		memset(dev_addr->dst_dev_addr, 0, MAX_ADDR_LEN);
		dev_addr->bound_dev_if = if_getindex(dev);
		return (0);
	} else if (dev_type == IFT_INFINIBAND)
		dev_addr->dev_type = ARPHRD_INFINIBAND;
	else if (dev_type == IFT_ETHER || dev_type == IFT_L2VLAN)
		dev_addr->dev_type = ARPHRD_ETHER;
	else
		dev_addr->dev_type = 0;
	rdma_copy_addr_sub(dev_addr->src_dev_addr, if_getlladdr(dev),
			   if_getaddrlen(dev), MAX_ADDR_LEN);
	rdma_copy_addr_sub(dev_addr->broadcast, if_getbroadcastaddr(dev),
			   if_getaddrlen(dev), MAX_ADDR_LEN);
	if (dst_dev_addr != NULL) {
		rdma_copy_addr_sub(dev_addr->dst_dev_addr, dst_dev_addr,
				   if_getaddrlen(dev), MAX_ADDR_LEN);
	}
	dev_addr->bound_dev_if = if_getindex(dev);
	return 0;
}
EXPORT_SYMBOL(rdma_copy_addr);

int rdma_translate_ip(const struct sockaddr *addr,
		      struct rdma_dev_addr *dev_addr)
{
	if_t dev;
	int ret;

	if (dev_addr->bound_dev_if) {
		dev = dev_get_by_index(dev_addr->net, dev_addr->bound_dev_if);
	} else switch (addr->sa_family) {
#ifdef INET
	case AF_INET:
		dev = ip_ifp_find(dev_addr->net,
			((const struct sockaddr_in *)addr)->sin_addr.s_addr);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		dev = ip6_ifp_find(dev_addr->net,
			((const struct sockaddr_in6 *)addr)->sin6_addr, 0);
		break;
#endif
	default:
		dev = NULL;
		break;
	}

	if (dev != NULL) {
		/* disallow connections through 127.0.0.1 itself */
		if (if_getflags(dev) & IFF_LOOPBACK)
			ret = -EINVAL;
		else
			ret = rdma_copy_addr(dev_addr, dev, NULL);
		dev_put(dev);
	} else {
		ret = -ENODEV;
	}
	return ret;
}
EXPORT_SYMBOL(rdma_translate_ip);

static void set_timeout(unsigned long time)
{
	unsigned long delay;

	delay = time - jiffies;
	if (delay <= 0)
		delay = 1;
	else if (delay > hz)
		delay = hz;

	mod_delayed_work(addr_wq, &work, delay);
}

static void queue_req(struct addr_req *req)
{
	struct addr_req *temp_req;

	mutex_lock(&lock);
	list_for_each_entry_reverse(temp_req, &req_list, list) {
		if (time_after_eq(req->timeout, temp_req->timeout))
			break;
	}

	list_add(&req->list, &temp_req->list);

	if (req_list.next == &req->list)
		set_timeout(req->timeout);
	mutex_unlock(&lock);
}

#if defined(INET) || defined(INET6)
static int addr_resolve_multi(u8 *edst, if_t ifp, struct sockaddr *dst_in)
{
	struct sockaddr *llsa;
	struct sockaddr_dl sdl;
	int error;

	sdl.sdl_len = sizeof(sdl);
	llsa = (struct sockaddr *)&sdl;

	error = if_resolvemulti(ifp, &llsa, dst_in);
	if (error == 0) {
		rdma_copy_addr_sub(edst, LLADDR((struct sockaddr_dl *)llsa),
		    if_getaddrlen(ifp), MAX_ADDR_LEN);
	}
	return (error);
}
#endif

#ifdef INET
static int addr4_resolve(struct sockaddr_in *src_in,
			 const struct sockaddr_in *dst_in,
			 struct rdma_dev_addr *addr,
			 u8 *edst,
			 if_t *ifpp)
{
	enum {
		ADDR_VALID = 0,
		ADDR_SRC_ANY = 1,
		ADDR_DST_ANY = 2,
	};
	struct sockaddr_in dst_tmp = *dst_in;
	in_port_t src_port;
	struct sockaddr *saddr = NULL;
	struct nhop_object *nh;
	if_t ifp;
	int error;
	int type;

	NET_EPOCH_ASSERT();

	/* set VNET, if any */
	CURVNET_SET(addr->net);

	/* set default TTL limit */
	addr->hoplimit = V_ip_defttl;

	type = ADDR_VALID;
	if (src_in->sin_addr.s_addr == INADDR_ANY)
		type |= ADDR_SRC_ANY;
	if (dst_tmp.sin_addr.s_addr == INADDR_ANY)
		type |= ADDR_DST_ANY;

	/*
	 * Make sure the socket address length field is set.
	 */
	dst_tmp.sin_len = sizeof(dst_tmp);

	/* Step 1 - lookup destination route if any */
	switch (type) {
	case ADDR_VALID:
	case ADDR_SRC_ANY:
		/* regular destination route lookup */
		nh = fib4_lookup(RT_DEFAULT_FIB, dst_tmp.sin_addr,0,NHR_NONE,0);
		if (nh == NULL) {
			error = EHOSTUNREACH;
			goto done;
		}
		break;
	default:
		error = ENETUNREACH;
		goto done;
	}

	/* Step 2 - find outgoing network interface */
	switch (type) {
	case ADDR_VALID:
		/* get source interface */
		if (addr->bound_dev_if != 0) {
			ifp = dev_get_by_index(addr->net, addr->bound_dev_if);
		} else {
			ifp = ip_ifp_find(addr->net, src_in->sin_addr.s_addr);
		}

		/* check source interface */
		if (ifp == NULL) {
			error = ENETUNREACH;
			goto done;
		} else if (if_getflags(ifp) & IFF_LOOPBACK) {
			/*
			 * Source address cannot be a loopback device.
			 */
			error = EHOSTUNREACH;
			goto error_put_ifp;
		} else if (if_getflags(nh->nh_ifp) & IFF_LOOPBACK) {
			if (memcmp(&src_in->sin_addr, &dst_in->sin_addr,
			    sizeof(src_in->sin_addr))) {
				/*
				 * Destination is loopback, but source
				 * and destination address is not the
				 * same.
				 */
				error = EHOSTUNREACH;
				goto error_put_ifp;
			}
			/* get destination network interface from route */
			dev_put(ifp);
			ifp = nh->nh_ifp;
			dev_hold(ifp);
		} else if (ifp != nh->nh_ifp) {
			/*
			 * Source and destination interfaces are
			 * different.
			 */
			error = ENETUNREACH;
			goto error_put_ifp;
		}
		break;
	case ADDR_SRC_ANY:
		/* check for loopback device */
		if (if_getflags(nh->nh_ifp) & IFF_LOOPBACK)
			saddr = (struct sockaddr *)&dst_tmp;
		else
			saddr = nh->nh_ifa->ifa_addr;

		/* get destination network interface from route */
		ifp = nh->nh_ifp;
		dev_hold(ifp);
		break;
	default:
		break;
	}

	/*
	 * Step 3 - resolve destination MAC address
	 */
	if (dst_tmp.sin_addr.s_addr == INADDR_BROADCAST) {
		rdma_copy_addr_sub(edst, if_getbroadcastaddr(ifp),
		    if_getaddrlen(ifp), MAX_ADDR_LEN);
		error = 0;
	} else if (IN_MULTICAST(ntohl(dst_tmp.sin_addr.s_addr))) {
		bool is_gw = (nh->nh_flags & NHF_GATEWAY) != 0;
		error = addr_resolve_multi(edst, ifp, (struct sockaddr *)&dst_tmp);
		if (error != 0)
			goto error_put_ifp;
		else if (is_gw)
			addr->network = RDMA_NETWORK_IPV4;
	} else if (if_getflags(ifp) & IFF_LOOPBACK) {
		memset(edst, 0, MAX_ADDR_LEN);
		error = 0;
	} else {
		bool is_gw = (nh->nh_flags & NHF_GATEWAY) != 0;
		memset(edst, 0, MAX_ADDR_LEN);
#ifdef INET6
		if (is_gw && nh->gw_sa.sa_family == AF_INET6)
			error = nd6_resolve(ifp, LLE_SF(AF_INET, is_gw), NULL,
			    &nh->gw_sa, edst, NULL, NULL);
		else
#endif
			error = arpresolve(ifp, is_gw, NULL, is_gw ?
			    &nh->gw_sa : (const struct sockaddr *)&dst_tmp,
			    edst, NULL, NULL);

		if (error != 0)
			goto error_put_ifp;
		else if (is_gw)
			addr->network = RDMA_NETWORK_IPV4;
	}

	/*
	 * Step 4 - update source address, if any
	 */
	if (saddr != NULL) {
		src_port = src_in->sin_port;
		memcpy(src_in, saddr, rdma_addr_size(saddr));
		src_in->sin_port = src_port;	/* preserve port number */
	}

	*ifpp = ifp;

	goto done;

error_put_ifp:
	dev_put(ifp);
done:
	CURVNET_RESTORE();

	if (error == EWOULDBLOCK || error == EAGAIN)
		error = ENODATA;
	return (-error);
}
#else
static int addr4_resolve(struct sockaddr_in *src_in,
			 const struct sockaddr_in *dst_in,
			 struct rdma_dev_addr *addr,
			 u8 *edst,
			 if_t *ifpp)
{
	return -EADDRNOTAVAIL;
}
#endif

#ifdef INET6
static int addr6_resolve(struct sockaddr_in6 *src_in,
			 const struct sockaddr_in6 *dst_in,
			 struct rdma_dev_addr *addr,
			 u8 *edst,
			 if_t *ifpp)
{
	enum {
		ADDR_VALID = 0,
		ADDR_SRC_ANY = 1,
		ADDR_DST_ANY = 2,
	};
	struct sockaddr_in6 dst_tmp = *dst_in;
	in_port_t src_port;
	struct sockaddr *saddr = NULL;
	struct nhop_object *nh;
	if_t ifp;
	int error;
	int type;

	NET_EPOCH_ASSERT();

	/* set VNET, if any */
	CURVNET_SET(addr->net);

	/* set default TTL limit */
	addr->hoplimit = V_ip_defttl;

	type = ADDR_VALID;
	if (ipv6_addr_any(&src_in->sin6_addr))
		type |= ADDR_SRC_ANY;
	if (ipv6_addr_any(&dst_tmp.sin6_addr))
		type |= ADDR_DST_ANY;

	/*
	 * Make sure the socket address length field is set.
	 */
	dst_tmp.sin6_len = sizeof(dst_tmp);

	/*
	 * Make sure the scope ID gets embedded, else nd6_resolve() will
	 * not find the record.
	 */
	dst_tmp.sin6_scope_id = addr->bound_dev_if;
	sa6_embedscope(&dst_tmp, 0);

	/* Step 1 - lookup destination route if any */
	switch (type) {
	case ADDR_VALID:
		/* sanity check for IPv4 addresses */
		if (ipv6_addr_v4mapped(&src_in->sin6_addr) !=
		    ipv6_addr_v4mapped(&dst_tmp.sin6_addr)) {
			error = EAFNOSUPPORT;
			goto done;
		}
		/* FALLTHROUGH */
	case ADDR_SRC_ANY:
		/* regular destination route lookup */
		nh = fib6_lookup(RT_DEFAULT_FIB, &dst_in->sin6_addr,
		    addr->bound_dev_if, NHR_NONE, 0);
		if (nh == NULL) {
			error = EHOSTUNREACH;
			goto done;
		}
		break;
	default:
		error = ENETUNREACH;
		goto done;
	}

	/* Step 2 - find outgoing network interface */
	switch (type) {
	case ADDR_VALID:
		/* get source interface */
		if (addr->bound_dev_if != 0) {
			ifp = dev_get_by_index(addr->net, addr->bound_dev_if);
		} else {
			ifp = ip6_ifp_find(addr->net, src_in->sin6_addr, 0);
		}

		/* check source interface */
		if (ifp == NULL) {
			error = ENETUNREACH;
			goto done;
		} else if (if_getflags(ifp) & IFF_LOOPBACK) {
			/*
			 * Source address cannot be a loopback device.
			 */
			error = EHOSTUNREACH;
			goto error_put_ifp;
		} else if (if_getflags(nh->nh_ifp) & IFF_LOOPBACK) {
			if (memcmp(&src_in->sin6_addr, &dst_in->sin6_addr,
			    sizeof(src_in->sin6_addr))) {
				/*
				 * Destination is loopback, but source
				 * and destination address is not the
				 * same.
				 */
				error = EHOSTUNREACH;
				goto error_put_ifp;
			}
			/* get destination network interface from route */
			dev_put(ifp);
			ifp = nh->nh_ifp;
			dev_hold(ifp);
		} else if (ifp != nh->nh_ifp) {
			/*
			 * Source and destination interfaces are
			 * different.
			 */
			error = ENETUNREACH;
			goto error_put_ifp;
		}
		break;
	case ADDR_SRC_ANY:
		/* check for loopback device */
		if (if_getflags(nh->nh_ifp) & IFF_LOOPBACK)
			saddr = (struct sockaddr *)&dst_tmp;
		else
			saddr = nh->nh_ifa->ifa_addr;

		/* get destination network interface from route */
		ifp = nh->nh_ifp;
		dev_hold(ifp);
		break;
	default:
		break;
	}

	/*
	 * Step 3 - resolve destination MAC address
	 */
	if (IN6_IS_ADDR_MULTICAST(&dst_tmp.sin6_addr)) {
		bool is_gw = (nh->nh_flags & NHF_GATEWAY) != 0;
		error = addr_resolve_multi(edst, ifp,
		    (struct sockaddr *)&dst_tmp);
		if (error != 0)
			goto error_put_ifp;
		else if (is_gw)
			addr->network = RDMA_NETWORK_IPV6;
	} else if (if_getflags(nh->nh_ifp) & IFF_LOOPBACK) {
		memset(edst, 0, MAX_ADDR_LEN);
		error = 0;
	} else {
		bool is_gw = (nh->nh_flags & NHF_GATEWAY) != 0;
		memset(edst, 0, MAX_ADDR_LEN);
		error = nd6_resolve(ifp, LLE_SF(AF_INET6, is_gw), NULL,
		    is_gw ? &nh->gw_sa : (const struct sockaddr *)&dst_tmp,
		    edst, NULL, NULL);
		if (error != 0)
			goto error_put_ifp;
		else if (is_gw)
			addr->network = RDMA_NETWORK_IPV6;
	}

	/*
	 * Step 4 - update source address, if any
	 */
	if (saddr != NULL) {
		src_port = src_in->sin6_port;
		memcpy(src_in, saddr, rdma_addr_size(saddr));
		src_in->sin6_port = src_port;	/* preserve port number */
	}

	*ifpp = ifp;

	goto done;

error_put_ifp:
	dev_put(ifp);
done:
	CURVNET_RESTORE();

	if (error == EWOULDBLOCK || error == EAGAIN)
		error = ENODATA;
	return (-error);
}
#else
static int addr6_resolve(struct sockaddr_in6 *src_in,
			 const struct sockaddr_in6 *dst_in,
			 struct rdma_dev_addr *addr,
			 u8 *edst,
			 if_t *ifpp)
{
	return -EADDRNOTAVAIL;
}
#endif

static int addr_resolve_neigh(if_t dev,
			      const struct sockaddr *dst_in,
			      u8 *edst,
			      struct rdma_dev_addr *addr)
{
	if (if_getflags(dev) & IFF_LOOPBACK) {
		int ret;

		/*
		 * Binding to a loopback device is not allowed. Make
		 * sure the destination device address is global by
		 * clearing the bound device interface:
		 */
		if (addr->bound_dev_if == if_getindex(dev))
			addr->bound_dev_if = 0;

		ret = rdma_translate_ip(dst_in, addr);
		if (ret == 0) {
			memcpy(addr->dst_dev_addr, addr->src_dev_addr,
			       MAX_ADDR_LEN);
		}
		return ret;
	}

	/* If the device doesn't do ARP internally */
	if (!(if_getflags(dev) & IFF_NOARP))
		return rdma_copy_addr(addr, dev, edst);

	return rdma_copy_addr(addr, dev, NULL);
}

static int addr_resolve(struct sockaddr *src_in,
			const struct sockaddr *dst_in,
			struct rdma_dev_addr *addr)
{
	struct epoch_tracker et;
	if_t ndev = NULL;
	u8 edst[MAX_ADDR_LEN];
	int ret;

	if (dst_in->sa_family != src_in->sa_family)
		return -EINVAL;

	NET_EPOCH_ENTER(et);
	switch (src_in->sa_family) {
	case AF_INET:
		ret = addr4_resolve((struct sockaddr_in *)src_in,
				    (const struct sockaddr_in *)dst_in,
				    addr, edst, &ndev);
		break;
	case AF_INET6:
		ret = addr6_resolve((struct sockaddr_in6 *)src_in,
				    (const struct sockaddr_in6 *)dst_in, addr,
				    edst, &ndev);
		break;
	default:
		ret = -EADDRNOTAVAIL;
		break;
	}
	NET_EPOCH_EXIT(et);

	/* check for error */
	if (ret != 0)
		return ret;

	/* store MAC addresses and check for loopback */
	ret = addr_resolve_neigh(ndev, dst_in, edst, addr);

	/* set belonging VNET, if any */
	addr->net = dev_net(ndev);
	dev_put(ndev);

	return ret;
}

static void process_req(struct work_struct *work)
{
	struct addr_req *req, *temp_req;
	struct sockaddr *src_in, *dst_in;
	struct list_head done_list;

	INIT_LIST_HEAD(&done_list);

	mutex_lock(&lock);
	list_for_each_entry_safe(req, temp_req, &req_list, list) {
		if (req->status == -ENODATA) {
			src_in = (struct sockaddr *) &req->src_addr;
			dst_in = (struct sockaddr *) &req->dst_addr;
			req->status = addr_resolve(src_in, dst_in, req->addr);
			if (req->status && time_after_eq(jiffies, req->timeout))
				req->status = -ETIMEDOUT;
			else if (req->status == -ENODATA)
				continue;
		}
		list_move_tail(&req->list, &done_list);
	}

	if (!list_empty(&req_list)) {
		req = list_entry(req_list.next, struct addr_req, list);
		set_timeout(req->timeout);
	}
	mutex_unlock(&lock);

	list_for_each_entry_safe(req, temp_req, &done_list, list) {
		list_del(&req->list);
		req->callback(req->status, (struct sockaddr *) &req->src_addr,
			req->addr, req->context);
		put_client(req->client);
		kfree(req);
	}
}

int rdma_resolve_ip(struct rdma_addr_client *client,
		    struct sockaddr *src_addr, struct sockaddr *dst_addr,
		    struct rdma_dev_addr *addr, int timeout_ms,
		    void (*callback)(int status, struct sockaddr *src_addr,
				     struct rdma_dev_addr *addr, void *context),
		    void *context)
{
	struct sockaddr *src_in, *dst_in;
	struct addr_req *req;
	int ret = 0;

	req = kzalloc(sizeof *req, GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	src_in = (struct sockaddr *) &req->src_addr;
	dst_in = (struct sockaddr *) &req->dst_addr;

	if (src_addr) {
		if (src_addr->sa_family != dst_addr->sa_family) {
			ret = -EINVAL;
			goto err;
		}

		memcpy(src_in, src_addr, rdma_addr_size(src_addr));
	} else {
		src_in->sa_family = dst_addr->sa_family;
	}

	memcpy(dst_in, dst_addr, rdma_addr_size(dst_addr));
	req->addr = addr;
	req->callback = callback;
	req->context = context;
	req->client = client;
	atomic_inc(&client->refcount);

	req->status = addr_resolve(src_in, dst_in, addr);
	switch (req->status) {
	case 0:
		req->timeout = jiffies;
		queue_req(req);
		break;
	case -ENODATA:
		req->timeout = msecs_to_jiffies(timeout_ms) + jiffies;
		queue_req(req);
		break;
	default:
		ret = req->status;
		atomic_dec(&client->refcount);
		goto err;
	}
	return ret;
err:
	kfree(req);
	return ret;
}
EXPORT_SYMBOL(rdma_resolve_ip);

int rdma_resolve_ip_route(struct sockaddr *src_addr,
			  const struct sockaddr *dst_addr,
			  struct rdma_dev_addr *addr)
{
	struct sockaddr_storage ssrc_addr = {};
	struct sockaddr *src_in = (struct sockaddr *)&ssrc_addr;

	if (src_addr) {
		if (src_addr->sa_family != dst_addr->sa_family)
			return -EINVAL;

		memcpy(src_in, src_addr, rdma_addr_size(src_addr));
	} else {
		src_in->sa_family = dst_addr->sa_family;
	}

	return addr_resolve(src_in, dst_addr, addr);
}
EXPORT_SYMBOL(rdma_resolve_ip_route);

void rdma_addr_cancel(struct rdma_dev_addr *addr)
{
	struct addr_req *req, *temp_req;

	mutex_lock(&lock);
	list_for_each_entry_safe(req, temp_req, &req_list, list) {
		if (req->addr == addr) {
			req->status = -ECANCELED;
			req->timeout = jiffies;
			list_move(&req->list, &req_list);
			set_timeout(req->timeout);
			break;
		}
	}
	mutex_unlock(&lock);
}
EXPORT_SYMBOL(rdma_addr_cancel);

struct resolve_cb_context {
	struct rdma_dev_addr *addr;
	struct completion comp;
	int status;
};

static void resolve_cb(int status, struct sockaddr *src_addr,
	     struct rdma_dev_addr *addr, void *context)
{
	if (!status)
		memcpy(((struct resolve_cb_context *)context)->addr,
		       addr, sizeof(struct rdma_dev_addr));
	((struct resolve_cb_context *)context)->status = status;
	complete(&((struct resolve_cb_context *)context)->comp);
}

int rdma_addr_find_l2_eth_by_grh(const union ib_gid *sgid,
				 const union ib_gid *dgid,
				 u8 *dmac, if_t dev,
				 int *hoplimit)
{
	int ret = 0;
	struct rdma_dev_addr dev_addr;
	struct resolve_cb_context ctx;

	union rdma_sockaddr sgid_addr, dgid_addr;

	rdma_gid2ip(&sgid_addr._sockaddr, sgid);
	rdma_gid2ip(&dgid_addr._sockaddr, dgid);

	memset(&dev_addr, 0, sizeof(dev_addr));

	dev_addr.bound_dev_if = if_getindex(dev);
	dev_addr.net = dev_net(dev);

	ctx.addr = &dev_addr;
	init_completion(&ctx.comp);
	ret = rdma_resolve_ip(&self, &sgid_addr._sockaddr, &dgid_addr._sockaddr,
			&dev_addr, 1000, resolve_cb, &ctx);
	if (ret)
		return ret;

	wait_for_completion(&ctx.comp);

	ret = ctx.status;
	if (ret)
		return ret;

	memcpy(dmac, dev_addr.dst_dev_addr, ETH_ALEN);
	if (hoplimit)
		*hoplimit = dev_addr.hoplimit;
	return ret;
}
EXPORT_SYMBOL(rdma_addr_find_l2_eth_by_grh);

int addr_init(void)
{
	addr_wq = alloc_workqueue("ib_addr", WQ_MEM_RECLAIM, 0);
	if (!addr_wq)
		return -ENOMEM;

	rdma_addr_register_client(&self);

	return 0;
}

void addr_cleanup(void)
{
	rdma_addr_unregister_client(&self);
	destroy_workqueue(addr_wq);
}
