/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Dmitry Chagin <dchagin@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/vnet.h>

#include <compat/linux/linux.h>
#include <compat/linux/linux_common.h>
#include <fs/pseudofs/pseudofs.h>

#include <compat/linsysfs/linsysfs.h>

struct pfs_node *net;
static eventhandler_tag if_arrival_tag, if_departure_tag;

static uint32_t net_latch_count = 0;
static struct mtx net_latch_mtx;
MTX_SYSINIT(net_latch_mtx, &net_latch_mtx, "lsfnet", MTX_DEF);

struct ifp_nodes_queue {
	TAILQ_ENTRY(ifp_nodes_queue) ifp_nodes_next;
	if_t ifp;
	struct vnet *vnet;
	struct pfs_node *pn;
};
TAILQ_HEAD(,ifp_nodes_queue) ifp_nodes_q;

static void
linsysfs_net_latch_hold(void)
{

	mtx_lock(&net_latch_mtx);
	if (net_latch_count++ > 0)
		mtx_sleep(&net_latch_count, &net_latch_mtx, PDROP, "lsfnet", 0);
	else
		mtx_unlock(&net_latch_mtx);
}

static void
linsysfs_net_latch_rele(void)
{

	mtx_lock(&net_latch_mtx);
	if (--net_latch_count > 0)
		wakeup_one(&net_latch_count);
	mtx_unlock(&net_latch_mtx);
}

static int
linsysfs_if_addr(PFS_FILL_ARGS)
{
	struct epoch_tracker et;
	struct l_sockaddr lsa;
	if_t ifp;
	int error;

	CURVNET_SET(TD_TO_VNET(td));
	NET_EPOCH_ENTER(et);
	ifp = ifname_linux_to_ifp(td, pn->pn_parent->pn_name);
	if (ifp != NULL && (error = linux_ifhwaddr(ifp, &lsa)) == 0)
		error = sbuf_printf(sb, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
		    lsa.sa_data[0], lsa.sa_data[1], lsa.sa_data[2],
		    lsa.sa_data[3], lsa.sa_data[4], lsa.sa_data[5]);
	else
		error = ENOENT;
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();
	return (error == -1 ? ERANGE : error);
}

static int
linsysfs_if_addrlen(PFS_FILL_ARGS)
{

	sbuf_printf(sb, "%d\n", LINUX_IFHWADDRLEN);
	return (0);
}

static int
linsysfs_if_flags(PFS_FILL_ARGS)
{
	struct epoch_tracker et;
	if_t ifp;
	int error;

	CURVNET_SET(TD_TO_VNET(td));
	NET_EPOCH_ENTER(et);
	ifp = ifname_linux_to_ifp(td, pn->pn_parent->pn_name);
	if (ifp != NULL)
		error = sbuf_printf(sb, "0x%x\n", linux_ifflags(ifp));
	else
		error = ENOENT;
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();
	return (error == -1 ? ERANGE : error);
}

static int
linsysfs_if_ifindex(PFS_FILL_ARGS)
{
	struct epoch_tracker et;
	if_t ifp;
	int error;

	CURVNET_SET(TD_TO_VNET(td));
	NET_EPOCH_ENTER(et);
	ifp = ifname_linux_to_ifp(td, pn->pn_parent->pn_name);
	if (ifp != NULL)
		error = sbuf_printf(sb, "%u\n", if_getindex(ifp));
	else
		error = ENOENT;
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();
	return (error == -1 ? ERANGE : error);
}

static int
linsysfs_if_mtu(PFS_FILL_ARGS)
{
	struct epoch_tracker et;
	if_t ifp;
	int error;

	CURVNET_SET(TD_TO_VNET(td));
	NET_EPOCH_ENTER(et);
	ifp = ifname_linux_to_ifp(td, pn->pn_parent->pn_name);
	if (ifp != NULL)
		error = sbuf_printf(sb, "%u\n", if_getmtu(ifp));
	else
		error = ENOENT;
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();
	return (error == -1 ? ERANGE : error);
}

static int
linsysfs_if_txq_len(PFS_FILL_ARGS)
{

	/* XXX */
	sbuf_printf(sb, "1000\n");
	return (0);
}

static int
linsysfs_if_type(PFS_FILL_ARGS)
{
	struct epoch_tracker et;
	struct l_sockaddr lsa;
	if_t ifp;
	int error;

	CURVNET_SET(TD_TO_VNET(td));
	NET_EPOCH_ENTER(et);
	ifp = ifname_linux_to_ifp(td, pn->pn_parent->pn_name);
	if (ifp != NULL && (error = linux_ifhwaddr(ifp, &lsa)) == 0)
		error = sbuf_printf(sb, "%d\n", lsa.sa_family);
	else
		error = ENOENT;
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();
	return (error == -1 ? ERANGE : error);
}

static int
linsysfs_if_visible(PFS_VIS_ARGS)
{
	struct ifp_nodes_queue *nq, *nq_tmp;
	struct epoch_tracker et;
	if_t ifp;
	int visible;

	visible = 0;
	CURVNET_SET(TD_TO_VNET(td));
	NET_EPOCH_ENTER(et);
	ifp = ifname_linux_to_ifp(td, pn->pn_name);
	if (ifp != NULL) {
		TAILQ_FOREACH_SAFE(nq, &ifp_nodes_q, ifp_nodes_next, nq_tmp) {
			if (nq->ifp == ifp && nq->vnet == curvnet) {
				visible = 1;
				break;
			}
		}
	}
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();
	return (visible);
}

static int
linsysfs_net_addif(if_t ifp, void *arg)
{
	struct ifp_nodes_queue *nq, *nq_tmp;
	struct pfs_node *nic, *dir = arg;
	char ifname[LINUX_IFNAMSIZ];
	struct epoch_tracker et;
	int ret __diagused;

	NET_EPOCH_ENTER(et);
	ret = ifname_bsd_to_linux_ifp(ifp, ifname, sizeof(ifname));
	NET_EPOCH_EXIT(et);
	KASSERT(ret > 0, ("Interface (%s) is not converted", if_name(ifp)));

	nic = pfs_find_node(dir, ifname);
	if (nic == NULL) {
		nic = pfs_create_dir(dir, ifname, NULL, linsysfs_if_visible,
		    NULL, 0);
		pfs_create_file(nic, "address", &linsysfs_if_addr,
		    NULL, NULL, NULL, PFS_RD);
		pfs_create_file(nic, "addr_len", &linsysfs_if_addrlen,
		    NULL, NULL, NULL, PFS_RD);
		pfs_create_file(nic, "flags", &linsysfs_if_flags,
		    NULL, NULL, NULL, PFS_RD);
		pfs_create_file(nic, "ifindex", &linsysfs_if_ifindex,
		    NULL, NULL, NULL, PFS_RD);
		pfs_create_file(nic, "mtu", &linsysfs_if_mtu,
		    NULL, NULL, NULL, PFS_RD);
		pfs_create_file(nic, "tx_queue_len", &linsysfs_if_txq_len,
		    NULL, NULL, NULL, PFS_RD);
		pfs_create_file(nic, "type", &linsysfs_if_type,
		NULL, NULL, NULL, PFS_RD);
	}
	/*
	 * There is a small window between registering the if_arrival
	 * eventhandler and creating a list of interfaces.
	 */
	TAILQ_FOREACH_SAFE(nq, &ifp_nodes_q, ifp_nodes_next, nq_tmp) {
		if (nq->ifp == ifp && nq->vnet == curvnet)
			return (0);
	}
	nq = malloc(sizeof(*nq), M_LINSYSFS, M_WAITOK);
	nq->pn = nic;
	nq->ifp = ifp;
	nq->vnet = curvnet;
	TAILQ_INSERT_TAIL(&ifp_nodes_q, nq, ifp_nodes_next);
	return (0);
}

static void
linsysfs_net_delif(if_t ifp)
{
	struct ifp_nodes_queue *nq, *nq_tmp;
	struct pfs_node *pn;

	pn = NULL;
	TAILQ_FOREACH_SAFE(nq, &ifp_nodes_q, ifp_nodes_next, nq_tmp) {
		if (nq->ifp == ifp && nq->vnet == curvnet) {
			TAILQ_REMOVE(&ifp_nodes_q, nq, ifp_nodes_next);
			pn = nq->pn;
			free(nq, M_LINSYSFS);
			break;
		}
	}
	if (pn == NULL)
		return;
	TAILQ_FOREACH_SAFE(nq, &ifp_nodes_q, ifp_nodes_next, nq_tmp) {
		if (nq->pn == pn)
			return;
	}
	pfs_destroy(pn);
}

static void
linsysfs_if_arrival(void *arg __unused, if_t ifp)
{

	linsysfs_net_latch_hold();
	(void)linsysfs_net_addif(ifp, net);
	linsysfs_net_latch_rele();
}

static void
linsysfs_if_departure(void *arg __unused, if_t ifp)
{

	linsysfs_net_latch_hold();
	linsysfs_net_delif(ifp);
	linsysfs_net_latch_rele();
}

void
linsysfs_net_init(void)
{
	VNET_ITERATOR_DECL(vnet_iter);

	MPASS(net != NULL);
	TAILQ_INIT(&ifp_nodes_q);

	if_arrival_tag = EVENTHANDLER_REGISTER(ifnet_arrival_event,
	    linsysfs_if_arrival, NULL, EVENTHANDLER_PRI_ANY);
	if_departure_tag = EVENTHANDLER_REGISTER(ifnet_departure_event,
	    linsysfs_if_departure, NULL, EVENTHANDLER_PRI_ANY);

	linsysfs_net_latch_hold();
	VNET_LIST_RLOCK();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		if_foreach_sleep(NULL, NULL, linsysfs_net_addif, net);
		CURVNET_RESTORE();
	}
	VNET_LIST_RUNLOCK();
	linsysfs_net_latch_rele();
}

void
linsysfs_net_uninit(void)
{
	struct ifp_nodes_queue *nq, *nq_tmp;

	EVENTHANDLER_DEREGISTER(ifnet_arrival_event, if_arrival_tag);
	EVENTHANDLER_DEREGISTER(ifnet_departure_event, if_departure_tag);

	linsysfs_net_latch_hold();
	TAILQ_FOREACH_SAFE(nq, &ifp_nodes_q, ifp_nodes_next, nq_tmp) {
		TAILQ_REMOVE(&ifp_nodes_q, nq, ifp_nodes_next);
		free(nq, M_LINSYSFS);
	}
	linsysfs_net_latch_rele();
}
