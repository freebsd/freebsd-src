/*
 * Copyright (c) 2005 Voltaire Inc.  All rights reserved.
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 * Copyright (c) 1999-2005, Mellanox Technologies, Inc. All rights reserved.
 * Copyright (c) 2005 Intel Corporation.  All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a
 *    copy of which is available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/condvar.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/module.h>

#include <sys/lock.h>
#include <sys/condvar.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/route.h>

#include <net80211/ieee80211_freebsd.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <contrib/rdma/ib_addr.h>

struct addr_req {
	TAILQ_ENTRY(addr_req) entry;
	struct sockaddr src_addr;
	struct sockaddr dst_addr;
	struct rdma_dev_addr *addr;
	struct rdma_addr_client *client;
	void *context;
	void (*callback)(int status, struct sockaddr *src_addr,
			 struct rdma_dev_addr *addr, void *context);
	unsigned long timeout;
	int status;
};

static void process_req(void *ctx, int pending);

static struct mtx lock;

static TAILQ_HEAD(addr_req_list, addr_req) req_list;
static struct task addr_task;
static struct taskqueue *addr_taskq;
static struct callout addr_ch;
static eventhandler_tag route_event_tag;

static void addr_timeout(void *arg)
{
	taskqueue_enqueue(addr_taskq, &addr_task);
}

void rdma_addr_register_client(struct rdma_addr_client *client)
{
	mtx_init(&client->lock, "rdma_addr client lock", NULL, MTX_DUPOK|MTX_DEF);
	cv_init(&client->comp, "rdma_addr cv");
	client->refcount = 1;
}

static inline void put_client(struct rdma_addr_client *client)
{
	mtx_lock(&client->lock);
	if (--client->refcount == 0) {
		cv_broadcast(&client->comp);
	}
	mtx_unlock(&client->lock);
}

void rdma_addr_unregister_client(struct rdma_addr_client *client)
{
	put_client(client);
	mtx_lock(&client->lock);
	if (client->refcount) {
		cv_wait(&client->comp, &client->lock);
	}
	mtx_unlock(&client->lock);
}

int rdma_copy_addr(struct rdma_dev_addr *dev_addr, struct ifnet *dev,
		     const unsigned char *dst_dev_addr)
{
	dev_addr->dev_type = RDMA_NODE_RNIC;
	memset(dev_addr->src_dev_addr, 0, MAX_ADDR_LEN);
	memcpy(dev_addr->src_dev_addr, IF_LLADDR(dev), dev->if_addrlen);
	memcpy(dev_addr->broadcast, dev->if_broadcastaddr, MAX_ADDR_LEN);
	if (dst_dev_addr)
		memcpy(dev_addr->dst_dev_addr, dst_dev_addr, MAX_ADDR_LEN);
	return 0;
}

int rdma_translate_ip(struct sockaddr *addr, struct rdma_dev_addr *dev_addr)
{
	struct ifaddr *ifa;
	struct sockaddr_in *sin = (struct sockaddr_in *)addr;
	uint16_t port = sin->sin_port;
	int ret;
	
	sin->sin_port = 0;
	ifa = ifa_ifwithaddr(addr);
	sin->sin_port = port;
	if (!ifa)
		return (EADDRNOTAVAIL);
	ret = rdma_copy_addr(dev_addr, ifa->ifa_ifp, NULL);
	ifa_free(ifa);
	return (ret);
}

static void queue_req(struct addr_req *req)
{
	struct addr_req *tmp_req = NULL;
	
	mtx_lock(&lock);
	TAILQ_FOREACH_REVERSE(tmp_req, &req_list, addr_req_list, entry)
	    if (time_after_eq(req->timeout, tmp_req->timeout))
		    break;
	
	if (tmp_req)
		TAILQ_INSERT_AFTER(&req_list, tmp_req, req, entry);
	else
		TAILQ_INSERT_TAIL(&req_list, req, entry);
	
	if (TAILQ_FIRST(&req_list) == req)	
		callout_reset(&addr_ch, req->timeout - ticks, addr_timeout, NULL);
	mtx_unlock(&lock);
}

#ifdef needed
static void addr_send_arp(struct sockaddr_in *dst_in)
{
	struct route iproute;
	struct sockaddr_in *dst = (struct sockaddr_in *)&iproute.ro_dst;
	char dmac[ETHER_ADDR_LEN];
	struct llentry *lle;

	bzero(&iproute, sizeof iproute);
	*dst = *dst_in;

	rtalloc(&iproute);
	if (iproute.ro_rt == NULL)
		return;

	arpresolve(iproute.ro_rt->rt_ifp, iproute.ro_rt, NULL, 
		   rt_key(iproute.ro_rt), dmac, &lle);

	RTFREE(iproute.ro_rt);
}
#endif

static int addr_resolve_remote(struct sockaddr_in *src_in,
			       struct sockaddr_in *dst_in,
			       struct rdma_dev_addr *addr)
{
	int ret = 0;
	struct route iproute;
	struct sockaddr_in *dst = (struct sockaddr_in *)&iproute.ro_dst;
	char dmac[ETHER_ADDR_LEN];
	struct llentry *lle;

	bzero(&iproute, sizeof iproute);
	*dst = *dst_in;

	rtalloc(&iproute);
	if (iproute.ro_rt == NULL) {
		ret = EHOSTUNREACH;
		goto out;
	}

	/* If the device does ARP internally, return 'done' */
	if (iproute.ro_rt->rt_ifp->if_flags & IFF_NOARP) {
		rdma_copy_addr(addr, iproute.ro_rt->rt_ifp, NULL);
		goto put;
	}
 	ret = arpresolve(iproute.ro_rt->rt_ifp, iproute.ro_rt, NULL, 
		(struct sockaddr *)dst_in, dmac, &lle);
	if (ret) {
		goto put;
	}

	if (!src_in->sin_addr.s_addr) {
		src_in->sin_len = sizeof *src_in;
		src_in->sin_family = dst_in->sin_family;
		src_in->sin_addr.s_addr = ((struct sockaddr_in *)iproute.ro_rt->rt_ifa->ifa_addr)->sin_addr.s_addr;
	}

	ret = rdma_copy_addr(addr, iproute.ro_rt->rt_ifp, dmac);
put:
	RTFREE(iproute.ro_rt);
out:
	return ret;
}

static void process_req(void *ctx, int pending)
{
	struct addr_req *req, *tmp_req;
	struct sockaddr_in *src_in, *dst_in;
	TAILQ_HEAD(, addr_req) done_list;

	TAILQ_INIT(&done_list);

	mtx_lock(&lock);
	TAILQ_FOREACH_SAFE(req, &req_list, entry, tmp_req) {
		if (req->status == EWOULDBLOCK) {
			src_in = (struct sockaddr_in *) &req->src_addr;
			dst_in = (struct sockaddr_in *) &req->dst_addr;
			req->status = addr_resolve_remote(src_in, dst_in,
							  req->addr);
			if (req->status && time_after_eq(ticks, req->timeout))
				req->status = ETIMEDOUT;
			else if (req->status == EWOULDBLOCK)
				continue;
		}
		TAILQ_REMOVE(&req_list, req, entry);
		TAILQ_INSERT_TAIL(&done_list, req, entry);
	}

	if (!TAILQ_EMPTY(&req_list)) {
		req = TAILQ_FIRST(&req_list);
		callout_reset(&addr_ch, req->timeout - ticks, addr_timeout, 
			NULL);
	}
	mtx_unlock(&lock);

	TAILQ_FOREACH_SAFE(req, &done_list, entry, tmp_req) {
		TAILQ_REMOVE(&done_list, req, entry);
		req->callback(req->status, &req->src_addr, req->addr,
			      req->context);
		put_client(req->client);
		free(req, M_DEVBUF);
	}
}

int rdma_resolve_ip(struct rdma_addr_client *client,
		    struct sockaddr *src_addr, struct sockaddr *dst_addr,
		    struct rdma_dev_addr *addr, int timeout_ms,
		    void (*callback)(int status, struct sockaddr *src_addr,
				     struct rdma_dev_addr *addr, void *context),
		    void *context)
{
	struct sockaddr_in *src_in, *dst_in;
	struct addr_req *req;
	int ret = 0;

	req = malloc(sizeof *req, M_DEVBUF, M_NOWAIT);
	if (!req)
		return (ENOMEM);
	memset(req, 0, sizeof *req);

	if (src_addr)
		memcpy(&req->src_addr, src_addr, ip_addr_size(src_addr));
	memcpy(&req->dst_addr, dst_addr, ip_addr_size(dst_addr));
	req->addr = addr;
	req->callback = callback;
	req->context = context;
	req->client = client;
	mtx_lock(&client->lock);
	client->refcount++;
	mtx_unlock(&client->lock);

	src_in = (struct sockaddr_in *) &req->src_addr;
	dst_in = (struct sockaddr_in *) &req->dst_addr;

	req->status = addr_resolve_remote(src_in, dst_in, addr);

	switch (req->status) {
	case 0:
		req->timeout = ticks;
		queue_req(req);
		break;
	case EWOULDBLOCK:
		req->timeout = msecs_to_ticks(timeout_ms) + ticks;
		queue_req(req);
#ifdef needed
		addr_send_arp(dst_in);
#endif
		break;
	default:
		ret = req->status;
		mtx_lock(&client->lock);
		client->refcount--;
		mtx_unlock(&client->lock);
		free(req, M_DEVBUF);
		break;
	}
	return ret;
}

void rdma_addr_cancel(struct rdma_dev_addr *addr)
{
	struct addr_req *req, *tmp_req;

	mtx_lock(&lock);
	TAILQ_FOREACH_SAFE(req, &req_list, entry, tmp_req) {
		if (req->addr == addr) {
			req->status = ECANCELED;
			req->timeout = ticks;
			TAILQ_REMOVE(&req_list, req, entry);
			TAILQ_INSERT_HEAD(&req_list, req, entry);
			callout_reset(&addr_ch, req->timeout - ticks, addr_timeout, NULL);
			break;
		}
	}
	mtx_unlock(&lock);
}

static void
route_event_arp_update(void *unused, struct rtentry *rt0, uint8_t *enaddr,
	struct sockaddr *sa)
{
		callout_stop(&addr_ch);
		taskqueue_enqueue(addr_taskq, &addr_task);
}

static int addr_init(void)
{
	TAILQ_INIT(&req_list);
	mtx_init(&lock, "rdma_addr req_list lock", NULL, MTX_DEF);

	addr_taskq = taskqueue_create("rdma_addr_taskq", M_NOWAIT,
		taskqueue_thread_enqueue, &addr_taskq);
        if (addr_taskq == NULL) {
                printf("failed to allocate rdma_addr taskqueue\n");
                return (ENOMEM);
        }
        taskqueue_start_threads(&addr_taskq, 1, PI_NET, "rdma_addr taskq");
        TASK_INIT(&addr_task, 0, process_req, NULL);

	callout_init(&addr_ch, TRUE);

	route_event_tag = EVENTHANDLER_REGISTER(route_arp_update_event, 
		route_event_arp_update, NULL, EVENTHANDLER_PRI_ANY);

	return 0;
}

static void addr_cleanup(void)
{
	EVENTHANDLER_DEREGISTER(route_event_arp_update, route_event_tag);
	callout_stop(&addr_ch);
	taskqueue_drain(addr_taskq, &addr_task);
	taskqueue_free(addr_taskq);
}

static int 
addr_load(module_t mod, int cmd, void *arg)
{
        int err = 0;

        switch (cmd) {
        case MOD_LOAD:
                printf("Loading rdma_addr.\n");

                addr_init();
                break;
        case MOD_QUIESCE:
                break;
        case MOD_UNLOAD:
                printf("Unloading rdma_addr.\n");
		addr_cleanup();
                break;
        case MOD_SHUTDOWN:
                break;
        default:
                err = EOPNOTSUPP;
                break;
        }

        return (err);
}

static moduledata_t mod_data = {
	"rdma_addr",
	addr_load,
	0
};

MODULE_VERSION(rdma_addr, 1);
DECLARE_MODULE(rdma_addr, mod_data, SI_SUB_EXEC, SI_ORDER_ANY);
