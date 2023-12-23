/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1991, 1993, 1995
 *	The Regents of the University of California.
 * Copyright (c) 2007-2009 Robert N. M. Watson
 * Copyright (c) 2010-2011 Juniper Networks, Inc.
 * Copyright (c) 2021-2022 Gleb Smirnoff <glebius@FreeBSD.org>
 * All rights reserved.
 *
 * Portions of this software were developed by Robert N. M. Watson under
 * contract to Juniper Networks, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "opt_ddb.h"
#include "opt_ipsec.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ratelimit.h"
#include "opt_route.h"
#include "opt_rss.h"

#include <sys/param.h>
#include <sys/hash.h>
#include <sys/systm.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/eventhandler.h>
#include <sys/domain.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/smp.h>
#include <sys/smr.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/refcount.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <vm/uma.h>
#include <vm/vm.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/if_types.h>
#include <net/if_llatbl.h>
#include <net/route.h>
#include <net/rss_config.h>
#include <net/vnet.h>

#if defined(INET) || defined(INET6)
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_pcb_var.h>
#include <netinet/tcp.h>
#ifdef INET
#include <netinet/in_var.h>
#include <netinet/in_fib.h>
#endif
#include <netinet/ip_var.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#endif /* INET6 */
#include <net/route/nhop.h>
#endif

#include <netipsec/ipsec_support.h>

#include <security/mac/mac_framework.h>

#define	INPCBLBGROUP_SIZMIN	8
#define	INPCBLBGROUP_SIZMAX	256

#define	INP_FREED	0x00000200	/* Went through in_pcbfree(). */
#define	INP_INLBGROUP	0x01000000	/* Inserted into inpcblbgroup. */

/*
 * These configure the range of local port addresses assigned to
 * "unspecified" outgoing connections/packets/whatever.
 */
VNET_DEFINE(int, ipport_lowfirstauto) = IPPORT_RESERVED - 1;	/* 1023 */
VNET_DEFINE(int, ipport_lowlastauto) = IPPORT_RESERVEDSTART;	/* 600 */
VNET_DEFINE(int, ipport_firstauto) = IPPORT_EPHEMERALFIRST;	/* 10000 */
VNET_DEFINE(int, ipport_lastauto) = IPPORT_EPHEMERALLAST;	/* 65535 */
VNET_DEFINE(int, ipport_hifirstauto) = IPPORT_HIFIRSTAUTO;	/* 49152 */
VNET_DEFINE(int, ipport_hilastauto) = IPPORT_HILASTAUTO;	/* 65535 */

/*
 * Reserved ports accessible only to root. There are significant
 * security considerations that must be accounted for when changing these,
 * but the security benefits can be great. Please be careful.
 */
VNET_DEFINE(int, ipport_reservedhigh) = IPPORT_RESERVED - 1;	/* 1023 */
VNET_DEFINE(int, ipport_reservedlow);

/* Enable random ephemeral port allocation by default. */
VNET_DEFINE(int, ipport_randomized) = 1;

#ifdef INET
static struct inpcb	*in_pcblookup_hash_locked(struct inpcbinfo *pcbinfo,
			    struct in_addr faddr, u_int fport_arg,
			    struct in_addr laddr, u_int lport_arg,
			    int lookupflags, uint8_t numa_domain);

#define RANGECHK(var, min, max) \
	if ((var) < (min)) { (var) = (min); } \
	else if ((var) > (max)) { (var) = (max); }

static int
sysctl_net_ipport_check(SYSCTL_HANDLER_ARGS)
{
	int error;

	error = sysctl_handle_int(oidp, arg1, arg2, req);
	if (error == 0) {
		RANGECHK(V_ipport_lowfirstauto, 1, IPPORT_RESERVED - 1);
		RANGECHK(V_ipport_lowlastauto, 1, IPPORT_RESERVED - 1);
		RANGECHK(V_ipport_firstauto, IPPORT_RESERVED, IPPORT_MAX);
		RANGECHK(V_ipport_lastauto, IPPORT_RESERVED, IPPORT_MAX);
		RANGECHK(V_ipport_hifirstauto, IPPORT_RESERVED, IPPORT_MAX);
		RANGECHK(V_ipport_hilastauto, IPPORT_RESERVED, IPPORT_MAX);
	}
	return (error);
}

#undef RANGECHK

static SYSCTL_NODE(_net_inet_ip, IPPROTO_IP, portrange,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "IP Ports");

SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, lowfirst,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &VNET_NAME(ipport_lowfirstauto), 0, &sysctl_net_ipport_check, "I",
    "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, lowlast,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &VNET_NAME(ipport_lowlastauto), 0, &sysctl_net_ipport_check, "I",
    "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, first,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &VNET_NAME(ipport_firstauto), 0, &sysctl_net_ipport_check, "I",
    "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, last,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &VNET_NAME(ipport_lastauto), 0, &sysctl_net_ipport_check, "I",
    "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, hifirst,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &VNET_NAME(ipport_hifirstauto), 0, &sysctl_net_ipport_check, "I",
    "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, hilast,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &VNET_NAME(ipport_hilastauto), 0, &sysctl_net_ipport_check, "I",
    "");
SYSCTL_INT(_net_inet_ip_portrange, OID_AUTO, reservedhigh,
	CTLFLAG_VNET | CTLFLAG_RW | CTLFLAG_SECURE,
	&VNET_NAME(ipport_reservedhigh), 0, "");
SYSCTL_INT(_net_inet_ip_portrange, OID_AUTO, reservedlow,
	CTLFLAG_RW|CTLFLAG_SECURE, &VNET_NAME(ipport_reservedlow), 0, "");
SYSCTL_INT(_net_inet_ip_portrange, OID_AUTO, randomized,
	CTLFLAG_VNET | CTLFLAG_RW,
	&VNET_NAME(ipport_randomized), 0, "Enable random port allocation");

#ifdef RATELIMIT
counter_u64_t rate_limit_new;
counter_u64_t rate_limit_chg;
counter_u64_t rate_limit_active;
counter_u64_t rate_limit_alloc_fail;
counter_u64_t rate_limit_set_ok;

static SYSCTL_NODE(_net_inet_ip, OID_AUTO, rl, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "IP Rate Limiting");
SYSCTL_COUNTER_U64(_net_inet_ip_rl, OID_AUTO, active, CTLFLAG_RD,
    &rate_limit_active, "Active rate limited connections");
SYSCTL_COUNTER_U64(_net_inet_ip_rl, OID_AUTO, alloc_fail, CTLFLAG_RD,
   &rate_limit_alloc_fail, "Rate limited connection failures");
SYSCTL_COUNTER_U64(_net_inet_ip_rl, OID_AUTO, set_ok, CTLFLAG_RD,
   &rate_limit_set_ok, "Rate limited setting succeeded");
SYSCTL_COUNTER_U64(_net_inet_ip_rl, OID_AUTO, newrl, CTLFLAG_RD,
   &rate_limit_new, "Total Rate limit new attempts");
SYSCTL_COUNTER_U64(_net_inet_ip_rl, OID_AUTO, chgrl, CTLFLAG_RD,
   &rate_limit_chg, "Total Rate limited change attempts");
#endif /* RATELIMIT */

#endif /* INET */

VNET_DEFINE(uint32_t, in_pcbhashseed);
static void
in_pcbhashseed_init(void)
{

	V_in_pcbhashseed = arc4random();
}
VNET_SYSINIT(in_pcbhashseed_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_FIRST,
    in_pcbhashseed_init, 0);

static void in_pcbremhash(struct inpcb *);

/*
 * in_pcb.c: manage the Protocol Control Blocks.
 *
 * NOTE: It is assumed that most of these functions will be called with
 * the pcbinfo lock held, and often, the inpcb lock held, as these utility
 * functions often modify hash chains or addresses in pcbs.
 */

static struct inpcblbgroup *
in_pcblbgroup_alloc(struct inpcblbgrouphead *hdr, struct ucred *cred,
    u_char vflag, uint16_t port, const union in_dependaddr *addr, int size,
    uint8_t numa_domain)
{
	struct inpcblbgroup *grp;
	size_t bytes;

	bytes = __offsetof(struct inpcblbgroup, il_inp[size]);
	grp = malloc(bytes, M_PCB, M_ZERO | M_NOWAIT);
	if (grp == NULL)
		return (NULL);
	grp->il_cred = crhold(cred);
	grp->il_vflag = vflag;
	grp->il_lport = port;
	grp->il_numa_domain = numa_domain;
	grp->il_dependladdr = *addr;
	grp->il_inpsiz = size;
	CK_LIST_INSERT_HEAD(hdr, grp, il_list);
	return (grp);
}

static void
in_pcblbgroup_free_deferred(epoch_context_t ctx)
{
	struct inpcblbgroup *grp;

	grp = __containerof(ctx, struct inpcblbgroup, il_epoch_ctx);
	crfree(grp->il_cred);
	free(grp, M_PCB);
}

static void
in_pcblbgroup_free(struct inpcblbgroup *grp)
{

	CK_LIST_REMOVE(grp, il_list);
	NET_EPOCH_CALL(in_pcblbgroup_free_deferred, &grp->il_epoch_ctx);
}

static struct inpcblbgroup *
in_pcblbgroup_resize(struct inpcblbgrouphead *hdr,
    struct inpcblbgroup *old_grp, int size)
{
	struct inpcblbgroup *grp;
	int i;

	grp = in_pcblbgroup_alloc(hdr, old_grp->il_cred, old_grp->il_vflag,
	    old_grp->il_lport, &old_grp->il_dependladdr, size,
	    old_grp->il_numa_domain);
	if (grp == NULL)
		return (NULL);

	KASSERT(old_grp->il_inpcnt < grp->il_inpsiz,
	    ("invalid new local group size %d and old local group count %d",
	     grp->il_inpsiz, old_grp->il_inpcnt));

	for (i = 0; i < old_grp->il_inpcnt; ++i)
		grp->il_inp[i] = old_grp->il_inp[i];
	grp->il_inpcnt = old_grp->il_inpcnt;
	in_pcblbgroup_free(old_grp);
	return (grp);
}

/*
 * PCB at index 'i' is removed from the group. Pull up the ones below il_inp[i]
 * and shrink group if possible.
 */
static void
in_pcblbgroup_reorder(struct inpcblbgrouphead *hdr, struct inpcblbgroup **grpp,
    int i)
{
	struct inpcblbgroup *grp, *new_grp;

	grp = *grpp;
	for (; i + 1 < grp->il_inpcnt; ++i)
		grp->il_inp[i] = grp->il_inp[i + 1];
	grp->il_inpcnt--;

	if (grp->il_inpsiz > INPCBLBGROUP_SIZMIN &&
	    grp->il_inpcnt <= grp->il_inpsiz / 4) {
		/* Shrink this group. */
		new_grp = in_pcblbgroup_resize(hdr, grp, grp->il_inpsiz / 2);
		if (new_grp != NULL)
			*grpp = new_grp;
	}
}

/*
 * Add PCB to load balance group for SO_REUSEPORT_LB option.
 */
static int
in_pcbinslbgrouphash(struct inpcb *inp, uint8_t numa_domain)
{
	const static struct timeval interval = { 60, 0 };
	static struct timeval lastprint;
	struct inpcbinfo *pcbinfo;
	struct inpcblbgrouphead *hdr;
	struct inpcblbgroup *grp;
	uint32_t idx;

	pcbinfo = inp->inp_pcbinfo;

	INP_WLOCK_ASSERT(inp);
	INP_HASH_WLOCK_ASSERT(pcbinfo);

#ifdef INET6
	/*
	 * Don't allow IPv4 mapped INET6 wild socket.
	 */
	if ((inp->inp_vflag & INP_IPV4) &&
	    inp->inp_laddr.s_addr == INADDR_ANY &&
	    INP_CHECK_SOCKAF(inp->inp_socket, AF_INET6)) {
		return (0);
	}
#endif

	idx = INP_PCBPORTHASH(inp->inp_lport, pcbinfo->ipi_lbgrouphashmask);
	hdr = &pcbinfo->ipi_lbgrouphashbase[idx];
	CK_LIST_FOREACH(grp, hdr, il_list) {
		if (grp->il_cred->cr_prison == inp->inp_cred->cr_prison &&
		    grp->il_vflag == inp->inp_vflag &&
		    grp->il_lport == inp->inp_lport &&
		    grp->il_numa_domain == numa_domain &&
		    memcmp(&grp->il_dependladdr,
		    &inp->inp_inc.inc_ie.ie_dependladdr,
		    sizeof(grp->il_dependladdr)) == 0) {
			break;
		}
	}
	if (grp == NULL) {
		/* Create new load balance group. */
		grp = in_pcblbgroup_alloc(hdr, inp->inp_cred, inp->inp_vflag,
		    inp->inp_lport, &inp->inp_inc.inc_ie.ie_dependladdr,
		    INPCBLBGROUP_SIZMIN, numa_domain);
		if (grp == NULL)
			return (ENOBUFS);
	} else if (grp->il_inpcnt == grp->il_inpsiz) {
		if (grp->il_inpsiz >= INPCBLBGROUP_SIZMAX) {
			if (ratecheck(&lastprint, &interval))
				printf("lb group port %d, limit reached\n",
				    ntohs(grp->il_lport));
			return (0);
		}

		/* Expand this local group. */
		grp = in_pcblbgroup_resize(hdr, grp, grp->il_inpsiz * 2);
		if (grp == NULL)
			return (ENOBUFS);
	}

	KASSERT(grp->il_inpcnt < grp->il_inpsiz,
	    ("invalid local group size %d and count %d", grp->il_inpsiz,
	    grp->il_inpcnt));

	grp->il_inp[grp->il_inpcnt] = inp;
	grp->il_inpcnt++;
	inp->inp_flags |= INP_INLBGROUP;
	return (0);
}

/*
 * Remove PCB from load balance group.
 */
static void
in_pcbremlbgrouphash(struct inpcb *inp)
{
	struct inpcbinfo *pcbinfo;
	struct inpcblbgrouphead *hdr;
	struct inpcblbgroup *grp;
	int i;

	pcbinfo = inp->inp_pcbinfo;

	INP_WLOCK_ASSERT(inp);
	MPASS(inp->inp_flags & INP_INLBGROUP);
	INP_HASH_WLOCK_ASSERT(pcbinfo);

	hdr = &pcbinfo->ipi_lbgrouphashbase[
	    INP_PCBPORTHASH(inp->inp_lport, pcbinfo->ipi_lbgrouphashmask)];
	CK_LIST_FOREACH(grp, hdr, il_list) {
		for (i = 0; i < grp->il_inpcnt; ++i) {
			if (grp->il_inp[i] != inp)
				continue;

			if (grp->il_inpcnt == 1) {
				/* We are the last, free this local group. */
				in_pcblbgroup_free(grp);
			} else {
				/* Pull up inpcbs, shrink group if possible. */
				in_pcblbgroup_reorder(hdr, &grp, i);
			}
			inp->inp_flags &= ~INP_INLBGROUP;
			return;
		}
	}
	KASSERT(0, ("%s: did not find %p", __func__, inp));
}

int
in_pcblbgroup_numa(struct inpcb *inp, int arg)
{
	struct inpcbinfo *pcbinfo;
	struct inpcblbgrouphead *hdr;
	struct inpcblbgroup *grp;
	int err, i;
	uint8_t numa_domain;

	switch (arg) {
	case TCP_REUSPORT_LB_NUMA_NODOM:
		numa_domain = M_NODOM;
		break;
	case TCP_REUSPORT_LB_NUMA_CURDOM:
		numa_domain = PCPU_GET(domain);
		break;
	default:
		if (arg < 0 || arg >= vm_ndomains)
			return (EINVAL);
		numa_domain = arg;
	}

	err = 0;
	pcbinfo = inp->inp_pcbinfo;
	INP_WLOCK_ASSERT(inp);
	INP_HASH_WLOCK(pcbinfo);
	hdr = &pcbinfo->ipi_lbgrouphashbase[
	    INP_PCBPORTHASH(inp->inp_lport, pcbinfo->ipi_lbgrouphashmask)];
	CK_LIST_FOREACH(grp, hdr, il_list) {
		for (i = 0; i < grp->il_inpcnt; ++i) {
			if (grp->il_inp[i] != inp)
				continue;

			if (grp->il_numa_domain == numa_domain) {
				goto abort_with_hash_wlock;
			}

			/* Remove it from the old group. */
			in_pcbremlbgrouphash(inp);

			/* Add it to the new group based on numa domain. */
			in_pcbinslbgrouphash(inp, numa_domain);
			goto abort_with_hash_wlock;
		}
	}
	err = ENOENT;
abort_with_hash_wlock:
	INP_HASH_WUNLOCK(pcbinfo);
	return (err);
}

/* Make sure it is safe to use hashinit(9) on CK_LIST. */
CTASSERT(sizeof(struct inpcbhead) == sizeof(LIST_HEAD(, inpcb)));

/*
 * Initialize an inpcbinfo - a per-VNET instance of connections db.
 */
void
in_pcbinfo_init(struct inpcbinfo *pcbinfo, struct inpcbstorage *pcbstor,
    u_int hash_nelements, u_int porthash_nelements)
{

	mtx_init(&pcbinfo->ipi_lock, pcbstor->ips_infolock_name, NULL, MTX_DEF);
	mtx_init(&pcbinfo->ipi_hash_lock, pcbstor->ips_hashlock_name,
	    NULL, MTX_DEF);
#ifdef VIMAGE
	pcbinfo->ipi_vnet = curvnet;
#endif
	CK_LIST_INIT(&pcbinfo->ipi_listhead);
	pcbinfo->ipi_count = 0;
	pcbinfo->ipi_hash_exact = hashinit(hash_nelements, M_PCB,
	    &pcbinfo->ipi_hashmask);
	pcbinfo->ipi_hash_wild = hashinit(hash_nelements, M_PCB,
	    &pcbinfo->ipi_hashmask);
	porthash_nelements = imin(porthash_nelements, IPPORT_MAX + 1);
	pcbinfo->ipi_porthashbase = hashinit(porthash_nelements, M_PCB,
	    &pcbinfo->ipi_porthashmask);
	pcbinfo->ipi_lbgrouphashbase = hashinit(porthash_nelements, M_PCB,
	    &pcbinfo->ipi_lbgrouphashmask);
	pcbinfo->ipi_zone = pcbstor->ips_zone;
	pcbinfo->ipi_portzone = pcbstor->ips_portzone;
	pcbinfo->ipi_smr = uma_zone_get_smr(pcbinfo->ipi_zone);
}

/*
 * Destroy an inpcbinfo.
 */
void
in_pcbinfo_destroy(struct inpcbinfo *pcbinfo)
{

	KASSERT(pcbinfo->ipi_count == 0,
	    ("%s: ipi_count = %u", __func__, pcbinfo->ipi_count));

	hashdestroy(pcbinfo->ipi_hash_exact, M_PCB, pcbinfo->ipi_hashmask);
	hashdestroy(pcbinfo->ipi_hash_wild, M_PCB, pcbinfo->ipi_hashmask);
	hashdestroy(pcbinfo->ipi_porthashbase, M_PCB,
	    pcbinfo->ipi_porthashmask);
	hashdestroy(pcbinfo->ipi_lbgrouphashbase, M_PCB,
	    pcbinfo->ipi_lbgrouphashmask);
	mtx_destroy(&pcbinfo->ipi_hash_lock);
	mtx_destroy(&pcbinfo->ipi_lock);
}

/*
 * Initialize a pcbstorage - per protocol zones to allocate inpcbs.
 */
static void inpcb_fini(void *, int);
void
in_pcbstorage_init(void *arg)
{
	struct inpcbstorage *pcbstor = arg;

	pcbstor->ips_zone = uma_zcreate(pcbstor->ips_zone_name,
	    pcbstor->ips_size, NULL, NULL, pcbstor->ips_pcbinit,
	    inpcb_fini, UMA_ALIGN_CACHE, UMA_ZONE_SMR);
	pcbstor->ips_portzone = uma_zcreate(pcbstor->ips_portzone_name,
	    sizeof(struct inpcbport), NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	uma_zone_set_smr(pcbstor->ips_portzone,
	    uma_zone_get_smr(pcbstor->ips_zone));
}

/*
 * Destroy a pcbstorage - used by unloadable protocols.
 */
void
in_pcbstorage_destroy(void *arg)
{
	struct inpcbstorage *pcbstor = arg;

	uma_zdestroy(pcbstor->ips_zone);
	uma_zdestroy(pcbstor->ips_portzone);
}

/*
 * Allocate a PCB and associate it with the socket.
 * On success return with the PCB locked.
 */
int
in_pcballoc(struct socket *so, struct inpcbinfo *pcbinfo)
{
	struct inpcb *inp;
#if defined(IPSEC) || defined(IPSEC_SUPPORT) || defined(MAC)
	int error;
#endif

	inp = uma_zalloc_smr(pcbinfo->ipi_zone, M_NOWAIT);
	if (inp == NULL)
		return (ENOBUFS);
	bzero(&inp->inp_start_zero, inp_zero_size);
#ifdef NUMA
	inp->inp_numa_domain = M_NODOM;
#endif
	inp->inp_pcbinfo = pcbinfo;
	inp->inp_socket = so;
	inp->inp_cred = crhold(so->so_cred);
	inp->inp_inc.inc_fibnum = so->so_fibnum;
#ifdef MAC
	error = mac_inpcb_init(inp, M_NOWAIT);
	if (error != 0)
		goto out;
	mac_inpcb_create(so, inp);
#endif
#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	error = ipsec_init_pcbpolicy(inp);
	if (error != 0) {
#ifdef MAC
		mac_inpcb_destroy(inp);
#endif
		goto out;
	}
#endif /*IPSEC*/
#ifdef INET6
	if (INP_SOCKAF(so) == AF_INET6) {
		inp->inp_vflag |= INP_IPV6PROTO | INP_IPV6;
		if (V_ip6_v6only)
			inp->inp_flags |= IN6P_IPV6_V6ONLY;
#ifdef INET
		else
			inp->inp_vflag |= INP_IPV4;
#endif
		if (V_ip6_auto_flowlabel)
			inp->inp_flags |= IN6P_AUTOFLOWLABEL;
		inp->in6p_hops = -1;	/* use kernel default */
	}
#endif
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
		inp->inp_vflag |= INP_IPV4;
#endif
	inp->inp_smr = SMR_SEQ_INVALID;

	/*
	 * Routes in inpcb's can cache L2 as well; they are guaranteed
	 * to be cleaned up.
	 */
	inp->inp_route.ro_flags = RT_LLE_CACHE;
	refcount_init(&inp->inp_refcount, 1);   /* Reference from socket. */
	INP_WLOCK(inp);
	INP_INFO_WLOCK(pcbinfo);
	pcbinfo->ipi_count++;
	inp->inp_gencnt = ++pcbinfo->ipi_gencnt;
	CK_LIST_INSERT_HEAD(&pcbinfo->ipi_listhead, inp, inp_list);
	INP_INFO_WUNLOCK(pcbinfo);
	so->so_pcb = inp;

	return (0);

#if defined(IPSEC) || defined(IPSEC_SUPPORT) || defined(MAC)
out:
	uma_zfree_smr(pcbinfo->ipi_zone, inp);
	return (error);
#endif
}

#ifdef INET
int
in_pcbbind(struct inpcb *inp, struct sockaddr_in *sin, struct ucred *cred)
{
	int anonport, error;

	KASSERT(sin == NULL || sin->sin_family == AF_INET,
	    ("%s: invalid address family for %p", __func__, sin));
	KASSERT(sin == NULL || sin->sin_len == sizeof(struct sockaddr_in),
	    ("%s: invalid address length for %p", __func__, sin));
	INP_WLOCK_ASSERT(inp);
	INP_HASH_WLOCK_ASSERT(inp->inp_pcbinfo);

	if (inp->inp_lport != 0 || inp->inp_laddr.s_addr != INADDR_ANY)
		return (EINVAL);
	anonport = sin == NULL || sin->sin_port == 0;
	error = in_pcbbind_setup(inp, sin, &inp->inp_laddr.s_addr,
	    &inp->inp_lport, cred);
	if (error)
		return (error);
	if (in_pcbinshash(inp) != 0) {
		inp->inp_laddr.s_addr = INADDR_ANY;
		inp->inp_lport = 0;
		return (EAGAIN);
	}
	if (anonport)
		inp->inp_flags |= INP_ANONPORT;
	return (0);
}
#endif

#if defined(INET) || defined(INET6)
/*
 * Assign a local port like in_pcb_lport(), but also used with connect()
 * and a foreign address and port.  If fsa is non-NULL, choose a local port
 * that is unused with those, otherwise one that is completely unused.
 * lsa can be NULL for IPv6.
 */
int
in_pcb_lport_dest(struct inpcb *inp, struct sockaddr *lsa, u_short *lportp,
    struct sockaddr *fsa, u_short fport, struct ucred *cred, int lookupflags)
{
	struct inpcbinfo *pcbinfo;
	struct inpcb *tmpinp;
	unsigned short *lastport;
	int count, error;
	u_short aux, first, last, lport;
#ifdef INET
	struct in_addr laddr, faddr;
#endif
#ifdef INET6
	struct in6_addr *laddr6, *faddr6;
#endif

	pcbinfo = inp->inp_pcbinfo;

	/*
	 * Because no actual state changes occur here, a global write lock on
	 * the pcbinfo isn't required.
	 */
	INP_LOCK_ASSERT(inp);
	INP_HASH_LOCK_ASSERT(pcbinfo);

	if (inp->inp_flags & INP_HIGHPORT) {
		first = V_ipport_hifirstauto;	/* sysctl */
		last  = V_ipport_hilastauto;
		lastport = &pcbinfo->ipi_lasthi;
	} else if (inp->inp_flags & INP_LOWPORT) {
		error = priv_check_cred(cred, PRIV_NETINET_RESERVEDPORT);
		if (error)
			return (error);
		first = V_ipport_lowfirstauto;	/* 1023 */
		last  = V_ipport_lowlastauto;	/* 600 */
		lastport = &pcbinfo->ipi_lastlow;
	} else {
		first = V_ipport_firstauto;	/* sysctl */
		last  = V_ipport_lastauto;
		lastport = &pcbinfo->ipi_lastport;
	}

	/*
	 * Instead of having two loops further down counting up or down
	 * make sure that first is always <= last and go with only one
	 * code path implementing all logic.
	 */
	if (first > last) {
		aux = first;
		first = last;
		last = aux;
	}

#ifdef INET
	laddr.s_addr = INADDR_ANY;	/* used by INET6+INET below too */
	if ((inp->inp_vflag & (INP_IPV4|INP_IPV6)) == INP_IPV4) {
		if (lsa != NULL)
			laddr = ((struct sockaddr_in *)lsa)->sin_addr;
		if (fsa != NULL)
			faddr = ((struct sockaddr_in *)fsa)->sin_addr;
	}
#endif
#ifdef INET6
	laddr6 = NULL;
	if ((inp->inp_vflag & INP_IPV6) != 0) {
		if (lsa != NULL)
			laddr6 = &((struct sockaddr_in6 *)lsa)->sin6_addr;
		if (fsa != NULL)
			faddr6 = &((struct sockaddr_in6 *)fsa)->sin6_addr;
	}
#endif

	tmpinp = NULL;
	lport = *lportp;

	if (V_ipport_randomized)
		*lastport = first + (arc4random() % (last - first));

	count = last - first;

	do {
		if (count-- < 0)	/* completely used? */
			return (EADDRNOTAVAIL);
		++*lastport;
		if (*lastport < first || *lastport > last)
			*lastport = first;
		lport = htons(*lastport);

		if (fsa != NULL) {
#ifdef INET
			if (lsa->sa_family == AF_INET) {
				tmpinp = in_pcblookup_hash_locked(pcbinfo,
				    faddr, fport, laddr, lport, lookupflags,
				    M_NODOM);
			}
#endif
#ifdef INET6
			if (lsa->sa_family == AF_INET6) {
				tmpinp = in6_pcblookup_hash_locked(pcbinfo,
				    faddr6, fport, laddr6, lport, lookupflags,
				    M_NODOM);
			}
#endif
		} else {
#ifdef INET6
			if ((inp->inp_vflag & INP_IPV6) != 0) {
				tmpinp = in6_pcblookup_local(pcbinfo,
				    &inp->in6p_laddr, lport, lookupflags, cred);
#ifdef INET
				if (tmpinp == NULL &&
				    (inp->inp_vflag & INP_IPV4))
					tmpinp = in_pcblookup_local(pcbinfo,
					    laddr, lport, lookupflags, cred);
#endif
			}
#endif
#if defined(INET) && defined(INET6)
			else
#endif
#ifdef INET
				tmpinp = in_pcblookup_local(pcbinfo, laddr,
				    lport, lookupflags, cred);
#endif
		}
	} while (tmpinp != NULL);

	*lportp = lport;

	return (0);
}

/*
 * Select a local port (number) to use.
 */
int
in_pcb_lport(struct inpcb *inp, struct in_addr *laddrp, u_short *lportp,
    struct ucred *cred, int lookupflags)
{
	struct sockaddr_in laddr;

	if (laddrp) {
		bzero(&laddr, sizeof(laddr));
		laddr.sin_family = AF_INET;
		laddr.sin_addr = *laddrp;
	}
	return (in_pcb_lport_dest(inp, laddrp ? (struct sockaddr *) &laddr :
	    NULL, lportp, NULL, 0, cred, lookupflags));
}
#endif /* INET || INET6 */

#ifdef INET
/*
 * Set up a bind operation on a PCB, performing port allocation
 * as required, but do not actually modify the PCB. Callers can
 * either complete the bind by setting inp_laddr/inp_lport and
 * calling in_pcbinshash(), or they can just use the resulting
 * port and address to authorise the sending of a once-off packet.
 *
 * On error, the values of *laddrp and *lportp are not changed.
 */
int
in_pcbbind_setup(struct inpcb *inp, struct sockaddr_in *sin, in_addr_t *laddrp,
    u_short *lportp, struct ucred *cred)
{
	struct socket *so = inp->inp_socket;
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;
	struct in_addr laddr;
	u_short lport = 0;
	int lookupflags = 0, reuseport = (so->so_options & SO_REUSEPORT);
	int error;

	/*
	 * XXX: Maybe we could let SO_REUSEPORT_LB set SO_REUSEPORT bit here
	 * so that we don't have to add to the (already messy) code below.
	 */
	int reuseport_lb = (so->so_options & SO_REUSEPORT_LB);

	/*
	 * No state changes, so read locks are sufficient here.
	 */
	INP_LOCK_ASSERT(inp);
	INP_HASH_LOCK_ASSERT(pcbinfo);

	laddr.s_addr = *laddrp;
	if (sin != NULL && laddr.s_addr != INADDR_ANY)
		return (EINVAL);
	if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT|SO_REUSEPORT_LB)) == 0)
		lookupflags = INPLOOKUP_WILDCARD;
	if (sin == NULL) {
		if ((error = prison_local_ip4(cred, &laddr)) != 0)
			return (error);
	} else {
		KASSERT(sin->sin_family == AF_INET,
		    ("%s: invalid family for address %p", __func__, sin));
		KASSERT(sin->sin_len == sizeof(*sin),
		    ("%s: invalid length for address %p", __func__, sin));

		error = prison_local_ip4(cred, &sin->sin_addr);
		if (error)
			return (error);
		if (sin->sin_port != *lportp) {
			/* Don't allow the port to change. */
			if (*lportp != 0)
				return (EINVAL);
			lport = sin->sin_port;
		}
		/* NB: lport is left as 0 if the port isn't being changed. */
		if (IN_MULTICAST(ntohl(sin->sin_addr.s_addr))) {
			/*
			 * Treat SO_REUSEADDR as SO_REUSEPORT for multicast;
			 * allow complete duplication of binding if
			 * SO_REUSEPORT is set, or if SO_REUSEADDR is set
			 * and a multicast address is bound on both
			 * new and duplicated sockets.
			 */
			if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) != 0)
				reuseport = SO_REUSEADDR|SO_REUSEPORT;
			/*
			 * XXX: How to deal with SO_REUSEPORT_LB here?
			 * Treat same as SO_REUSEPORT for now.
			 */
			if ((so->so_options &
			    (SO_REUSEADDR|SO_REUSEPORT_LB)) != 0)
				reuseport_lb = SO_REUSEADDR|SO_REUSEPORT_LB;
		} else if (sin->sin_addr.s_addr != INADDR_ANY) {
			sin->sin_port = 0;		/* yech... */
			bzero(&sin->sin_zero, sizeof(sin->sin_zero));
			/*
			 * Is the address a local IP address?
			 * If INP_BINDANY is set, then the socket may be bound
			 * to any endpoint address, local or not.
			 */
			if ((inp->inp_flags & INP_BINDANY) == 0 &&
			    ifa_ifwithaddr_check((struct sockaddr *)sin) == 0)
				return (EADDRNOTAVAIL);
		}
		laddr = sin->sin_addr;
		if (lport) {
			struct inpcb *t;

			/* GROSS */
			if (ntohs(lport) <= V_ipport_reservedhigh &&
			    ntohs(lport) >= V_ipport_reservedlow &&
			    priv_check_cred(cred, PRIV_NETINET_RESERVEDPORT))
				return (EACCES);
			if (!IN_MULTICAST(ntohl(sin->sin_addr.s_addr)) &&
			    priv_check_cred(inp->inp_cred, PRIV_NETINET_REUSEPORT) != 0) {
				t = in_pcblookup_local(pcbinfo, sin->sin_addr,
				    lport, INPLOOKUP_WILDCARD, cred);
	/*
	 * XXX
	 * This entire block sorely needs a rewrite.
	 */
				if (t != NULL &&
				    (so->so_type != SOCK_STREAM ||
				     ntohl(t->inp_faddr.s_addr) == INADDR_ANY) &&
				    (ntohl(sin->sin_addr.s_addr) != INADDR_ANY ||
				     ntohl(t->inp_laddr.s_addr) != INADDR_ANY ||
				     (t->inp_socket->so_options & SO_REUSEPORT) ||
				     (t->inp_socket->so_options & SO_REUSEPORT_LB) == 0) &&
				    (inp->inp_cred->cr_uid !=
				     t->inp_cred->cr_uid))
					return (EADDRINUSE);
			}
			t = in_pcblookup_local(pcbinfo, sin->sin_addr,
			    lport, lookupflags, cred);
			if (t != NULL && (reuseport & t->inp_socket->so_options) == 0 &&
			    (reuseport_lb & t->inp_socket->so_options) == 0) {
#ifdef INET6
				if (ntohl(sin->sin_addr.s_addr) !=
				    INADDR_ANY ||
				    ntohl(t->inp_laddr.s_addr) !=
				    INADDR_ANY ||
				    (inp->inp_vflag & INP_IPV6PROTO) == 0 ||
				    (t->inp_vflag & INP_IPV6PROTO) == 0)
#endif
						return (EADDRINUSE);
			}
		}
	}
	if (*lportp != 0)
		lport = *lportp;
	if (lport == 0) {
		error = in_pcb_lport(inp, &laddr, &lport, cred, lookupflags);
		if (error != 0)
			return (error);
	}
	*laddrp = laddr.s_addr;
	*lportp = lport;
	return (0);
}

/*
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int
in_pcbconnect(struct inpcb *inp, struct sockaddr_in *sin, struct ucred *cred,
    bool rehash __unused)
{
	u_short lport, fport;
	in_addr_t laddr, faddr;
	int anonport, error;

	INP_WLOCK_ASSERT(inp);
	INP_HASH_WLOCK_ASSERT(inp->inp_pcbinfo);
	KASSERT(in_nullhost(inp->inp_faddr),
	    ("%s: inp is already connected", __func__));

	lport = inp->inp_lport;
	laddr = inp->inp_laddr.s_addr;
	anonport = (lport == 0);
	error = in_pcbconnect_setup(inp, sin, &laddr, &lport, &faddr, &fport,
	    cred);
	if (error)
		return (error);

	inp->inp_faddr.s_addr = faddr;
	inp->inp_fport = fport;

	/* Do the initial binding of the local address if required. */
	if (inp->inp_laddr.s_addr == INADDR_ANY && inp->inp_lport == 0) {
		inp->inp_lport = lport;
		inp->inp_laddr.s_addr = laddr;
		if (in_pcbinshash(inp) != 0) {
			inp->inp_laddr.s_addr = inp->inp_faddr.s_addr =
			    INADDR_ANY;
			inp->inp_lport = inp->inp_fport = 0;
			return (EAGAIN);
		}
	} else {
		inp->inp_lport = lport;
		inp->inp_laddr.s_addr = laddr;
		if ((inp->inp_flags & INP_INHASHLIST) != 0)
			in_pcbrehash(inp);
		else
			in_pcbinshash(inp);
	}

	if (anonport)
		inp->inp_flags |= INP_ANONPORT;
	return (0);
}

/*
 * Do proper source address selection on an unbound socket in case
 * of connect. Take jails into account as well.
 */
int
in_pcbladdr(struct inpcb *inp, struct in_addr *faddr, struct in_addr *laddr,
    struct ucred *cred)
{
	struct ifaddr *ifa;
	struct sockaddr *sa;
	struct sockaddr_in *sin, dst;
	struct nhop_object *nh;
	int error;

	NET_EPOCH_ASSERT();
	KASSERT(laddr != NULL, ("%s: laddr NULL", __func__));

	/*
	 * Bypass source address selection and use the primary jail IP
	 * if requested.
	 */
	if (!prison_saddrsel_ip4(cred, laddr))
		return (0);

	error = 0;

	nh = NULL;
	bzero(&dst, sizeof(dst));
	sin = &dst;
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(struct sockaddr_in);
	sin->sin_addr.s_addr = faddr->s_addr;

	/*
	 * If route is known our src addr is taken from the i/f,
	 * else punt.
	 *
	 * Find out route to destination.
	 */
	if ((inp->inp_socket->so_options & SO_DONTROUTE) == 0)
		nh = fib4_lookup(inp->inp_inc.inc_fibnum, *faddr,
		    0, NHR_NONE, 0);

	/*
	 * If we found a route, use the address corresponding to
	 * the outgoing interface.
	 *
	 * Otherwise assume faddr is reachable on a directly connected
	 * network and try to find a corresponding interface to take
	 * the source address from.
	 */
	if (nh == NULL || nh->nh_ifp == NULL) {
		struct in_ifaddr *ia;
		struct ifnet *ifp;

		ia = ifatoia(ifa_ifwithdstaddr((struct sockaddr *)sin,
					inp->inp_socket->so_fibnum));
		if (ia == NULL) {
			ia = ifatoia(ifa_ifwithnet((struct sockaddr *)sin, 0,
						inp->inp_socket->so_fibnum));
		}
		if (ia == NULL) {
			error = ENETUNREACH;
			goto done;
		}

		if (!prison_flag(cred, PR_IP4)) {
			laddr->s_addr = ia->ia_addr.sin_addr.s_addr;
			goto done;
		}

		ifp = ia->ia_ifp;
		ia = NULL;
		CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			sa = ifa->ifa_addr;
			if (sa->sa_family != AF_INET)
				continue;
			sin = (struct sockaddr_in *)sa;
			if (prison_check_ip4(cred, &sin->sin_addr) == 0) {
				ia = (struct in_ifaddr *)ifa;
				break;
			}
		}
		if (ia != NULL) {
			laddr->s_addr = ia->ia_addr.sin_addr.s_addr;
			goto done;
		}

		/* 3. As a last resort return the 'default' jail address. */
		error = prison_get_ip4(cred, laddr);
		goto done;
	}

	/*
	 * If the outgoing interface on the route found is not
	 * a loopback interface, use the address from that interface.
	 * In case of jails do those three steps:
	 * 1. check if the interface address belongs to the jail. If so use it.
	 * 2. check if we have any address on the outgoing interface
	 *    belonging to this jail. If so use it.
	 * 3. as a last resort return the 'default' jail address.
	 */
	if ((nh->nh_ifp->if_flags & IFF_LOOPBACK) == 0) {
		struct in_ifaddr *ia;
		struct ifnet *ifp;

		/* If not jailed, use the default returned. */
		if (!prison_flag(cred, PR_IP4)) {
			ia = (struct in_ifaddr *)nh->nh_ifa;
			laddr->s_addr = ia->ia_addr.sin_addr.s_addr;
			goto done;
		}

		/* Jailed. */
		/* 1. Check if the iface address belongs to the jail. */
		sin = (struct sockaddr_in *)nh->nh_ifa->ifa_addr;
		if (prison_check_ip4(cred, &sin->sin_addr) == 0) {
			ia = (struct in_ifaddr *)nh->nh_ifa;
			laddr->s_addr = ia->ia_addr.sin_addr.s_addr;
			goto done;
		}

		/*
		 * 2. Check if we have any address on the outgoing interface
		 *    belonging to this jail.
		 */
		ia = NULL;
		ifp = nh->nh_ifp;
		CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			sa = ifa->ifa_addr;
			if (sa->sa_family != AF_INET)
				continue;
			sin = (struct sockaddr_in *)sa;
			if (prison_check_ip4(cred, &sin->sin_addr) == 0) {
				ia = (struct in_ifaddr *)ifa;
				break;
			}
		}
		if (ia != NULL) {
			laddr->s_addr = ia->ia_addr.sin_addr.s_addr;
			goto done;
		}

		/* 3. As a last resort return the 'default' jail address. */
		error = prison_get_ip4(cred, laddr);
		goto done;
	}

	/*
	 * The outgoing interface is marked with 'loopback net', so a route
	 * to ourselves is here.
	 * Try to find the interface of the destination address and then
	 * take the address from there. That interface is not necessarily
	 * a loopback interface.
	 * In case of jails, check that it is an address of the jail
	 * and if we cannot find, fall back to the 'default' jail address.
	 */
	if ((nh->nh_ifp->if_flags & IFF_LOOPBACK) != 0) {
		struct in_ifaddr *ia;

		ia = ifatoia(ifa_ifwithdstaddr(sintosa(&dst),
					inp->inp_socket->so_fibnum));
		if (ia == NULL)
			ia = ifatoia(ifa_ifwithnet(sintosa(&dst), 0,
						inp->inp_socket->so_fibnum));
		if (ia == NULL)
			ia = ifatoia(ifa_ifwithaddr(sintosa(&dst)));

		if (!prison_flag(cred, PR_IP4)) {
			if (ia == NULL) {
				error = ENETUNREACH;
				goto done;
			}
			laddr->s_addr = ia->ia_addr.sin_addr.s_addr;
			goto done;
		}

		/* Jailed. */
		if (ia != NULL) {
			struct ifnet *ifp;

			ifp = ia->ia_ifp;
			ia = NULL;
			CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
				sa = ifa->ifa_addr;
				if (sa->sa_family != AF_INET)
					continue;
				sin = (struct sockaddr_in *)sa;
				if (prison_check_ip4(cred,
				    &sin->sin_addr) == 0) {
					ia = (struct in_ifaddr *)ifa;
					break;
				}
			}
			if (ia != NULL) {
				laddr->s_addr = ia->ia_addr.sin_addr.s_addr;
				goto done;
			}
		}

		/* 3. As a last resort return the 'default' jail address. */
		error = prison_get_ip4(cred, laddr);
		goto done;
	}

done:
	if (error == 0 && laddr->s_addr == INADDR_ANY)
		return (EHOSTUNREACH);
	return (error);
}

/*
 * Set up for a connect from a socket to the specified address.
 * On entry, *laddrp and *lportp should contain the current local
 * address and port for the PCB; these are updated to the values
 * that should be placed in inp_laddr and inp_lport to complete
 * the connect.
 *
 * On success, *faddrp and *fportp will be set to the remote address
 * and port. These are not updated in the error case.
 */
int
in_pcbconnect_setup(struct inpcb *inp, struct sockaddr_in *sin,
    in_addr_t *laddrp, u_short *lportp, in_addr_t *faddrp, u_short *fportp,
    struct ucred *cred)
{
	struct in_ifaddr *ia;
	struct in_addr laddr, faddr;
	u_short lport, fport;
	int error;

	KASSERT(sin->sin_family == AF_INET,
	    ("%s: invalid address family for %p", __func__, sin));
	KASSERT(sin->sin_len == sizeof(*sin),
	    ("%s: invalid address length for %p", __func__, sin));

	/*
	 * Because a global state change doesn't actually occur here, a read
	 * lock is sufficient.
	 */
	NET_EPOCH_ASSERT();
	INP_LOCK_ASSERT(inp);
	INP_HASH_LOCK_ASSERT(inp->inp_pcbinfo);

	if (sin->sin_port == 0)
		return (EADDRNOTAVAIL);
	laddr.s_addr = *laddrp;
	lport = *lportp;
	faddr = sin->sin_addr;
	fport = sin->sin_port;
#ifdef ROUTE_MPATH
	if (CALC_FLOWID_OUTBOUND) {
		uint32_t hash_val, hash_type;

		hash_val = fib4_calc_software_hash(laddr, faddr, 0, fport,
		    inp->inp_socket->so_proto->pr_protocol, &hash_type);

		inp->inp_flowid = hash_val;
		inp->inp_flowtype = hash_type;
	}
#endif
	if (!CK_STAILQ_EMPTY(&V_in_ifaddrhead)) {
		/*
		 * If the destination address is INADDR_ANY,
		 * use the primary local address.
		 * If the supplied address is INADDR_BROADCAST,
		 * and the primary interface supports broadcast,
		 * choose the broadcast address for that interface.
		 */
		if (faddr.s_addr == INADDR_ANY) {
			faddr =
			    IA_SIN(CK_STAILQ_FIRST(&V_in_ifaddrhead))->sin_addr;
			if ((error = prison_get_ip4(cred, &faddr)) != 0)
				return (error);
		} else if (faddr.s_addr == (u_long)INADDR_BROADCAST) {
			if (CK_STAILQ_FIRST(&V_in_ifaddrhead)->ia_ifp->if_flags &
			    IFF_BROADCAST)
				faddr = satosin(&CK_STAILQ_FIRST(
				    &V_in_ifaddrhead)->ia_broadaddr)->sin_addr;
		}
	}
	if (laddr.s_addr == INADDR_ANY) {
		error = in_pcbladdr(inp, &faddr, &laddr, cred);
		/*
		 * If the destination address is multicast and an outgoing
		 * interface has been set as a multicast option, prefer the
		 * address of that interface as our source address.
		 */
		if (IN_MULTICAST(ntohl(faddr.s_addr)) &&
		    inp->inp_moptions != NULL) {
			struct ip_moptions *imo;
			struct ifnet *ifp;

			imo = inp->inp_moptions;
			if (imo->imo_multicast_ifp != NULL) {
				ifp = imo->imo_multicast_ifp;
				CK_STAILQ_FOREACH(ia, &V_in_ifaddrhead, ia_link) {
					if (ia->ia_ifp == ifp &&
					    prison_check_ip4(cred,
					    &ia->ia_addr.sin_addr) == 0)
						break;
				}
				if (ia == NULL)
					error = EADDRNOTAVAIL;
				else {
					laddr = ia->ia_addr.sin_addr;
					error = 0;
				}
			}
		}
		if (error)
			return (error);
	}

	if (lport != 0) {
		if (in_pcblookup_hash_locked(inp->inp_pcbinfo, faddr,
		    fport, laddr, lport, 0, M_NODOM) != NULL)
			return (EADDRINUSE);
	} else {
		struct sockaddr_in lsin, fsin;

		bzero(&lsin, sizeof(lsin));
		bzero(&fsin, sizeof(fsin));
		lsin.sin_family = AF_INET;
		lsin.sin_addr = laddr;
		fsin.sin_family = AF_INET;
		fsin.sin_addr = faddr;
		error = in_pcb_lport_dest(inp, (struct sockaddr *) &lsin,
		    &lport, (struct sockaddr *)& fsin, fport, cred,
		    INPLOOKUP_WILDCARD);
		if (error)
			return (error);
	}
	*laddrp = laddr.s_addr;
	*lportp = lport;
	*faddrp = faddr.s_addr;
	*fportp = fport;
	return (0);
}

void
in_pcbdisconnect(struct inpcb *inp)
{

	INP_WLOCK_ASSERT(inp);
	INP_HASH_WLOCK_ASSERT(inp->inp_pcbinfo);
	KASSERT(inp->inp_smr == SMR_SEQ_INVALID,
	    ("%s: inp %p was already disconnected", __func__, inp));

	in_pcbremhash_locked(inp);

	/* See the comment in in_pcbinshash(). */
	inp->inp_smr = smr_advance(inp->inp_pcbinfo->ipi_smr);
	inp->inp_laddr.s_addr = INADDR_ANY;
	inp->inp_faddr.s_addr = INADDR_ANY;
	inp->inp_fport = 0;
}
#endif /* INET */

/*
 * in_pcbdetach() is responsibe for disassociating a socket from an inpcb.
 * For most protocols, this will be invoked immediately prior to calling
 * in_pcbfree().  However, with TCP the inpcb may significantly outlive the
 * socket, in which case in_pcbfree() is deferred.
 */
void
in_pcbdetach(struct inpcb *inp)
{

	KASSERT(inp->inp_socket != NULL, ("%s: inp_socket == NULL", __func__));

#ifdef RATELIMIT
	if (inp->inp_snd_tag != NULL)
		in_pcbdetach_txrtlmt(inp);
#endif
	inp->inp_socket->so_pcb = NULL;
	inp->inp_socket = NULL;
}

/*
 * inpcb hash lookups are protected by SMR section.
 *
 * Once desired pcb has been found, switching from SMR section to a pcb
 * lock is performed with inp_smr_lock(). We can not use INP_(W|R)LOCK
 * here because SMR is a critical section.
 * In 99%+ cases inp_smr_lock() would obtain the lock immediately.
 */
void
inp_lock(struct inpcb *inp, const inp_lookup_t lock)
{

	lock == INPLOOKUP_RLOCKPCB ?
	    rw_rlock(&inp->inp_lock) : rw_wlock(&inp->inp_lock);
}

void
inp_unlock(struct inpcb *inp, const inp_lookup_t lock)
{

	lock == INPLOOKUP_RLOCKPCB ?
	    rw_runlock(&inp->inp_lock) : rw_wunlock(&inp->inp_lock);
}

int
inp_trylock(struct inpcb *inp, const inp_lookup_t lock)
{

	return (lock == INPLOOKUP_RLOCKPCB ?
	    rw_try_rlock(&inp->inp_lock) : rw_try_wlock(&inp->inp_lock));
}

static inline bool
_inp_smr_lock(struct inpcb *inp, const inp_lookup_t lock, const int ignflags)
{

	MPASS(lock == INPLOOKUP_RLOCKPCB || lock == INPLOOKUP_WLOCKPCB);
	SMR_ASSERT_ENTERED(inp->inp_pcbinfo->ipi_smr);

	if (__predict_true(inp_trylock(inp, lock))) {
		if (__predict_false(inp->inp_flags & ignflags)) {
			smr_exit(inp->inp_pcbinfo->ipi_smr);
			inp_unlock(inp, lock);
			return (false);
		}
		smr_exit(inp->inp_pcbinfo->ipi_smr);
		return (true);
	}

	if (__predict_true(refcount_acquire_if_not_zero(&inp->inp_refcount))) {
		smr_exit(inp->inp_pcbinfo->ipi_smr);
		inp_lock(inp, lock);
		if (__predict_false(in_pcbrele(inp, lock)))
			return (false);
		/*
		 * inp acquired through refcount & lock for sure didn't went
		 * through uma_zfree().  However, it may have already went
		 * through in_pcbfree() and has another reference, that
		 * prevented its release by our in_pcbrele().
		 */
		if (__predict_false(inp->inp_flags & ignflags)) {
			inp_unlock(inp, lock);
			return (false);
		}
		return (true);
	} else {
		smr_exit(inp->inp_pcbinfo->ipi_smr);
		return (false);
	}
}

bool
inp_smr_lock(struct inpcb *inp, const inp_lookup_t lock)
{

	/*
	 * in_pcblookup() family of functions ignore not only freed entries,
	 * that may be found due to lockless access to the hash, but dropped
	 * entries, too.
	 */
	return (_inp_smr_lock(inp, lock, INP_FREED | INP_DROPPED));
}

/*
 * inp_next() - inpcb hash/list traversal iterator
 *
 * Requires initialized struct inpcb_iterator for context.
 * The structure can be initialized with INP_ITERATOR() or INP_ALL_ITERATOR().
 *
 * - Iterator can have either write-lock or read-lock semantics, that can not
 *   be changed later.
 * - Iterator can iterate either over all pcbs list (INP_ALL_LIST), or through
 *   a single hash slot.  Note: only rip_input() does the latter.
 * - Iterator may have optional bool matching function.  The matching function
 *   will be executed for each inpcb in the SMR context, so it can not acquire
 *   locks and can safely access only immutable fields of inpcb.
 *
 * A fresh initialized iterator has NULL inpcb in its context and that
 * means that inp_next() call would return the very first inpcb on the list
 * locked with desired semantic.  In all following calls the context pointer
 * shall hold the current inpcb pointer.  The KPI user is not supposed to
 * unlock the current inpcb!  Upon end of traversal inp_next() will return NULL
 * and write NULL to its context.  After end of traversal an iterator can be
 * reused.
 *
 * List traversals have the following features/constraints:
 * - New entries won't be seen, as they are always added to the head of a list.
 * - Removed entries won't stop traversal as long as they are not added to
 *   a different list. This is violated by in_pcbrehash().
 */
#define	II_LIST_FIRST(ipi, hash)					\
		(((hash) == INP_ALL_LIST) ?				\
		    CK_LIST_FIRST(&(ipi)->ipi_listhead) :		\
		    CK_LIST_FIRST(&(ipi)->ipi_hash_exact[(hash)]))
#define	II_LIST_NEXT(inp, hash)						\
		(((hash) == INP_ALL_LIST) ?				\
		    CK_LIST_NEXT((inp), inp_list) :			\
		    CK_LIST_NEXT((inp), inp_hash_exact))
#define	II_LOCK_ASSERT(inp, lock)					\
		rw_assert(&(inp)->inp_lock,				\
		    (lock) == INPLOOKUP_RLOCKPCB ?  RA_RLOCKED : RA_WLOCKED )
struct inpcb *
inp_next(struct inpcb_iterator *ii)
{
	const struct inpcbinfo *ipi = ii->ipi;
	inp_match_t *match = ii->match;
	void *ctx = ii->ctx;
	inp_lookup_t lock = ii->lock;
	int hash = ii->hash;
	struct inpcb *inp;

	if (ii->inp == NULL) {		/* First call. */
		smr_enter(ipi->ipi_smr);
		/* This is unrolled CK_LIST_FOREACH(). */
		for (inp = II_LIST_FIRST(ipi, hash);
		    inp != NULL;
		    inp = II_LIST_NEXT(inp, hash)) {
			if (match != NULL && (match)(inp, ctx) == false)
				continue;
			if (__predict_true(_inp_smr_lock(inp, lock, INP_FREED)))
				break;
			else {
				smr_enter(ipi->ipi_smr);
				MPASS(inp != II_LIST_FIRST(ipi, hash));
				inp = II_LIST_FIRST(ipi, hash);
				if (inp == NULL)
					break;
			}
		}

		if (inp == NULL)
			smr_exit(ipi->ipi_smr);
		else
			ii->inp = inp;

		return (inp);
	}

	/* Not a first call. */
	smr_enter(ipi->ipi_smr);
restart:
	inp = ii->inp;
	II_LOCK_ASSERT(inp, lock);
next:
	inp = II_LIST_NEXT(inp, hash);
	if (inp == NULL) {
		smr_exit(ipi->ipi_smr);
		goto found;
	}

	if (match != NULL && (match)(inp, ctx) == false)
		goto next;

	if (__predict_true(inp_trylock(inp, lock))) {
		if (__predict_false(inp->inp_flags & INP_FREED)) {
			/*
			 * Entries are never inserted in middle of a list, thus
			 * as long as we are in SMR, we can continue traversal.
			 * Jump to 'restart' should yield in the same result,
			 * but could produce unnecessary looping.  Could this
			 * looping be unbound?
			 */
			inp_unlock(inp, lock);
			goto next;
		} else {
			smr_exit(ipi->ipi_smr);
			goto found;
		}
	}

	/*
	 * Can't obtain lock immediately, thus going hard.  Once we exit the
	 * SMR section we can no longer jump to 'next', and our only stable
	 * anchoring point is ii->inp, which we keep locked for this case, so
	 * we jump to 'restart'.
	 */
	if (__predict_true(refcount_acquire_if_not_zero(&inp->inp_refcount))) {
		smr_exit(ipi->ipi_smr);
		inp_lock(inp, lock);
		if (__predict_false(in_pcbrele(inp, lock))) {
			smr_enter(ipi->ipi_smr);
			goto restart;
		}
		/*
		 * See comment in inp_smr_lock().
		 */
		if (__predict_false(inp->inp_flags & INP_FREED)) {
			inp_unlock(inp, lock);
			smr_enter(ipi->ipi_smr);
			goto restart;
		}
	} else
		goto next;

found:
	inp_unlock(ii->inp, lock);
	ii->inp = inp;

	return (ii->inp);
}

/*
 * in_pcbref() bumps the reference count on an inpcb in order to maintain
 * stability of an inpcb pointer despite the inpcb lock being released or
 * SMR section exited.
 *
 * To free a reference later in_pcbrele_(r|w)locked() must be performed.
 */
void
in_pcbref(struct inpcb *inp)
{
	u_int old __diagused;

	old = refcount_acquire(&inp->inp_refcount);
	KASSERT(old > 0, ("%s: refcount 0", __func__));
}

/*
 * Drop a refcount on an inpcb elevated using in_pcbref(), potentially
 * freeing the pcb, if the reference was very last.
 */
bool
in_pcbrele_rlocked(struct inpcb *inp)
{

	INP_RLOCK_ASSERT(inp);

	if (!refcount_release(&inp->inp_refcount))
		return (false);

	MPASS(inp->inp_flags & INP_FREED);
	MPASS(inp->inp_socket == NULL);
	crfree(inp->inp_cred);
#ifdef INVARIANTS
	inp->inp_cred = NULL;
#endif
	INP_RUNLOCK(inp);
	uma_zfree_smr(inp->inp_pcbinfo->ipi_zone, inp);
	return (true);
}

bool
in_pcbrele_wlocked(struct inpcb *inp)
{

	INP_WLOCK_ASSERT(inp);

	if (!refcount_release(&inp->inp_refcount))
		return (false);

	MPASS(inp->inp_flags & INP_FREED);
	MPASS(inp->inp_socket == NULL);
	crfree(inp->inp_cred);
#ifdef INVARIANTS
	inp->inp_cred = NULL;
#endif
	INP_WUNLOCK(inp);
	uma_zfree_smr(inp->inp_pcbinfo->ipi_zone, inp);
	return (true);
}

bool
in_pcbrele(struct inpcb *inp, const inp_lookup_t lock)
{

	return (lock == INPLOOKUP_RLOCKPCB ?
	    in_pcbrele_rlocked(inp) : in_pcbrele_wlocked(inp));
}

/*
 * Unconditionally schedule an inpcb to be freed by decrementing its
 * reference count, which should occur only after the inpcb has been detached
 * from its socket.  If another thread holds a temporary reference (acquired
 * using in_pcbref()) then the free is deferred until that reference is
 * released using in_pcbrele_(r|w)locked(), but the inpcb is still unlocked.
 *  Almost all work, including removal from global lists, is done in this
 * context, where the pcbinfo lock is held.
 */
void
in_pcbfree(struct inpcb *inp)
{
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;
#ifdef INET
	struct ip_moptions *imo;
#endif
#ifdef INET6
	struct ip6_moptions *im6o;
#endif

	INP_WLOCK_ASSERT(inp);
	KASSERT(inp->inp_socket == NULL, ("%s: inp_socket != NULL", __func__));
	KASSERT((inp->inp_flags & INP_FREED) == 0,
	    ("%s: called twice for pcb %p", __func__, inp));

	inp->inp_flags |= INP_FREED;
	INP_INFO_WLOCK(pcbinfo);
	inp->inp_gencnt = ++pcbinfo->ipi_gencnt;
	pcbinfo->ipi_count--;
	CK_LIST_REMOVE(inp, inp_list);
	INP_INFO_WUNLOCK(pcbinfo);

	if (inp->inp_flags & INP_INHASHLIST)
		in_pcbremhash(inp);

	RO_INVALIDATE_CACHE(&inp->inp_route);
#ifdef MAC
	mac_inpcb_destroy(inp);
#endif
#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	if (inp->inp_sp != NULL)
		ipsec_delete_pcbpolicy(inp);
#endif
#ifdef INET
	if (inp->inp_options)
		(void)m_free(inp->inp_options);
	imo = inp->inp_moptions;
#endif
#ifdef INET6
	if (inp->inp_vflag & INP_IPV6PROTO) {
		ip6_freepcbopts(inp->in6p_outputopts);
		im6o = inp->in6p_moptions;
	} else
		im6o = NULL;
#endif

	if (__predict_false(in_pcbrele_wlocked(inp) == false)) {
		INP_WUNLOCK(inp);
	}
#ifdef INET6
	ip6_freemoptions(im6o);
#endif
#ifdef INET
	inp_freemoptions(imo);
#endif
}

/*
 * Different protocols initialize their inpcbs differently - giving
 * different name to the lock.  But they all are disposed the same.
 */
static void
inpcb_fini(void *mem, int size)
{
	struct inpcb *inp = mem;

	INP_LOCK_DESTROY(inp);
}

/*
 * in_pcbdrop() removes an inpcb from hashed lists, releasing its address and
 * port reservation, and preventing it from being returned by inpcb lookups.
 *
 * It is used by TCP to mark an inpcb as unused and avoid future packet
 * delivery or event notification when a socket remains open but TCP has
 * closed.  This might occur as a result of a shutdown()-initiated TCP close
 * or a RST on the wire, and allows the port binding to be reused while still
 * maintaining the invariant that so_pcb always points to a valid inpcb until
 * in_pcbdetach().
 *
 * XXXRW: Possibly in_pcbdrop() should also prevent future notifications by
 * in_pcbpurgeif0()?
 */
void
in_pcbdrop(struct inpcb *inp)
{

	INP_WLOCK_ASSERT(inp);
#ifdef INVARIANTS
	if (inp->inp_socket != NULL && inp->inp_ppcb != NULL)
		MPASS(inp->inp_refcount > 1);
#endif

	inp->inp_flags |= INP_DROPPED;
	if (inp->inp_flags & INP_INHASHLIST)
		in_pcbremhash(inp);
}

#ifdef INET
/*
 * Common routines to return the socket addresses associated with inpcbs.
 */
int
in_getsockaddr(struct socket *so, struct sockaddr *sa)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("in_getsockaddr: inp == NULL"));

	*(struct sockaddr_in *)sa = (struct sockaddr_in ){
		.sin_len = sizeof(struct sockaddr_in),
		.sin_family = AF_INET,
		.sin_port = inp->inp_lport,
		.sin_addr = inp->inp_laddr,
	};

	return (0);
}

int
in_getpeeraddr(struct socket *so, struct sockaddr *sa)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("in_getpeeraddr: inp == NULL"));

	*(struct sockaddr_in *)sa = (struct sockaddr_in ){
		.sin_len = sizeof(struct sockaddr_in),
		.sin_family = AF_INET,
		.sin_port = inp->inp_fport,
		.sin_addr = inp->inp_faddr,
	};

	return (0);
}

static bool
inp_v4_multi_match(const struct inpcb *inp, void *v __unused)
{

	if ((inp->inp_vflag & INP_IPV4) && inp->inp_moptions != NULL)
		return (true);
	else
		return (false);
}

void
in_pcbpurgeif0(struct inpcbinfo *pcbinfo, struct ifnet *ifp)
{
	struct inpcb_iterator inpi = INP_ITERATOR(pcbinfo, INPLOOKUP_WLOCKPCB,
	    inp_v4_multi_match, NULL);
	struct inpcb *inp;
	struct in_multi *inm;
	struct in_mfilter *imf;
	struct ip_moptions *imo;

	IN_MULTI_LOCK_ASSERT();

	while ((inp = inp_next(&inpi)) != NULL) {
		INP_WLOCK_ASSERT(inp);

		imo = inp->inp_moptions;
		/*
		 * Unselect the outgoing interface if it is being
		 * detached.
		 */
		if (imo->imo_multicast_ifp == ifp)
			imo->imo_multicast_ifp = NULL;

		/*
		 * Drop multicast group membership if we joined
		 * through the interface being detached.
		 *
		 * XXX This can all be deferred to an epoch_call
		 */
restart:
		IP_MFILTER_FOREACH(imf, &imo->imo_head) {
			if ((inm = imf->imf_inm) == NULL)
				continue;
			if (inm->inm_ifp != ifp)
				continue;
			ip_mfilter_remove(&imo->imo_head, imf);
			in_leavegroup_locked(inm, NULL);
			ip_mfilter_free(imf);
			goto restart;
		}
	}
}

/*
 * Lookup a PCB based on the local address and port.  Caller must hold the
 * hash lock.  No inpcb locks or references are acquired.
 */
#define INP_LOOKUP_MAPPED_PCB_COST	3
struct inpcb *
in_pcblookup_local(struct inpcbinfo *pcbinfo, struct in_addr laddr,
    u_short lport, int lookupflags, struct ucred *cred)
{
	struct inpcb *inp;
#ifdef INET6
	int matchwild = 3 + INP_LOOKUP_MAPPED_PCB_COST;
#else
	int matchwild = 3;
#endif
	int wildcard;

	KASSERT((lookupflags & ~(INPLOOKUP_WILDCARD)) == 0,
	    ("%s: invalid lookup flags %d", __func__, lookupflags));
	INP_HASH_LOCK_ASSERT(pcbinfo);

	if ((lookupflags & INPLOOKUP_WILDCARD) == 0) {
		struct inpcbhead *head;
		/*
		 * Look for an unconnected (wildcard foreign addr) PCB that
		 * matches the local address and port we're looking for.
		 */
		head = &pcbinfo->ipi_hash_wild[INP_PCBHASH_WILD(lport,
		    pcbinfo->ipi_hashmask)];
		CK_LIST_FOREACH(inp, head, inp_hash_wild) {
#ifdef INET6
			/* XXX inp locking */
			if ((inp->inp_vflag & INP_IPV4) == 0)
				continue;
#endif
			if (inp->inp_faddr.s_addr == INADDR_ANY &&
			    inp->inp_laddr.s_addr == laddr.s_addr &&
			    inp->inp_lport == lport) {
				/*
				 * Found?
				 */
				if (prison_equal_ip4(cred->cr_prison,
				    inp->inp_cred->cr_prison))
					return (inp);
			}
		}
		/*
		 * Not found.
		 */
		return (NULL);
	} else {
		struct inpcbporthead *porthash;
		struct inpcbport *phd;
		struct inpcb *match = NULL;
		/*
		 * Best fit PCB lookup.
		 *
		 * First see if this local port is in use by looking on the
		 * port hash list.
		 */
		porthash = &pcbinfo->ipi_porthashbase[INP_PCBPORTHASH(lport,
		    pcbinfo->ipi_porthashmask)];
		CK_LIST_FOREACH(phd, porthash, phd_hash) {
			if (phd->phd_port == lport)
				break;
		}
		if (phd != NULL) {
			/*
			 * Port is in use by one or more PCBs. Look for best
			 * fit.
			 */
			CK_LIST_FOREACH(inp, &phd->phd_pcblist, inp_portlist) {
				wildcard = 0;
				if (!prison_equal_ip4(inp->inp_cred->cr_prison,
				    cred->cr_prison))
					continue;
#ifdef INET6
				/* XXX inp locking */
				if ((inp->inp_vflag & INP_IPV4) == 0)
					continue;
				/*
				 * We never select the PCB that has
				 * INP_IPV6 flag and is bound to :: if
				 * we have another PCB which is bound
				 * to 0.0.0.0.  If a PCB has the
				 * INP_IPV6 flag, then we set its cost
				 * higher than IPv4 only PCBs.
				 *
				 * Note that the case only happens
				 * when a socket is bound to ::, under
				 * the condition that the use of the
				 * mapped address is allowed.
				 */
				if ((inp->inp_vflag & INP_IPV6) != 0)
					wildcard += INP_LOOKUP_MAPPED_PCB_COST;
#endif
				if (inp->inp_faddr.s_addr != INADDR_ANY)
					wildcard++;
				if (inp->inp_laddr.s_addr != INADDR_ANY) {
					if (laddr.s_addr == INADDR_ANY)
						wildcard++;
					else if (inp->inp_laddr.s_addr != laddr.s_addr)
						continue;
				} else {
					if (laddr.s_addr != INADDR_ANY)
						wildcard++;
				}
				if (wildcard < matchwild) {
					match = inp;
					matchwild = wildcard;
					if (matchwild == 0)
						break;
				}
			}
		}
		return (match);
	}
}
#undef INP_LOOKUP_MAPPED_PCB_COST

static bool
in_pcblookup_lb_numa_match(const struct inpcblbgroup *grp, int domain)
{
	return (domain == M_NODOM || domain == grp->il_numa_domain);
}

static struct inpcb *
in_pcblookup_lbgroup(const struct inpcbinfo *pcbinfo,
    const struct in_addr *faddr, uint16_t fport, const struct in_addr *laddr,
    uint16_t lport, int domain)
{
	const struct inpcblbgrouphead *hdr;
	struct inpcblbgroup *grp;
	struct inpcblbgroup *jail_exact, *jail_wild, *local_exact, *local_wild;

	INP_HASH_LOCK_ASSERT(pcbinfo);

	hdr = &pcbinfo->ipi_lbgrouphashbase[
	    INP_PCBPORTHASH(lport, pcbinfo->ipi_lbgrouphashmask)];

	/*
	 * Search for an LB group match based on the following criteria:
	 * - prefer jailed groups to non-jailed groups
	 * - prefer exact source address matches to wildcard matches
	 * - prefer groups bound to the specified NUMA domain
	 */
	jail_exact = jail_wild = local_exact = local_wild = NULL;
	CK_LIST_FOREACH(grp, hdr, il_list) {
		bool injail;

#ifdef INET6
		if (!(grp->il_vflag & INP_IPV4))
			continue;
#endif
		if (grp->il_lport != lport)
			continue;

		injail = prison_flag(grp->il_cred, PR_IP4) != 0;
		if (injail && prison_check_ip4_locked(grp->il_cred->cr_prison,
		    laddr) != 0)
			continue;

		if (grp->il_laddr.s_addr == laddr->s_addr) {
			if (injail) {
				jail_exact = grp;
				if (in_pcblookup_lb_numa_match(grp, domain))
					/* This is a perfect match. */
					goto out;
			} else if (local_exact == NULL ||
			    in_pcblookup_lb_numa_match(grp, domain)) {
				local_exact = grp;
			}
		} else if (grp->il_laddr.s_addr == INADDR_ANY) {
			if (injail) {
				if (jail_wild == NULL ||
				    in_pcblookup_lb_numa_match(grp, domain))
					jail_wild = grp;
			} else if (local_wild == NULL ||
			    in_pcblookup_lb_numa_match(grp, domain)) {
				local_wild = grp;
			}
		}
	}

	if (jail_exact != NULL)
		grp = jail_exact;
	else if (jail_wild != NULL)
		grp = jail_wild;
	else if (local_exact != NULL)
		grp = local_exact;
	else
		grp = local_wild;
	if (grp == NULL)
		return (NULL);
out:
	return (grp->il_inp[INP_PCBLBGROUP_PKTHASH(faddr, lport, fport) %
	    grp->il_inpcnt]);
}

static bool
in_pcblookup_exact_match(const struct inpcb *inp, struct in_addr faddr,
    u_short fport, struct in_addr laddr, u_short lport)
{
#ifdef INET6
	/* XXX inp locking */
	if ((inp->inp_vflag & INP_IPV4) == 0)
		return (false);
#endif
	if (inp->inp_faddr.s_addr == faddr.s_addr &&
	    inp->inp_laddr.s_addr == laddr.s_addr &&
	    inp->inp_fport == fport &&
	    inp->inp_lport == lport)
		return (true);
	return (false);
}

static struct inpcb *
in_pcblookup_hash_exact(struct inpcbinfo *pcbinfo, struct in_addr faddr,
    u_short fport, struct in_addr laddr, u_short lport)
{
	struct inpcbhead *head;
	struct inpcb *inp;

	INP_HASH_LOCK_ASSERT(pcbinfo);

	head = &pcbinfo->ipi_hash_exact[INP_PCBHASH(&faddr, lport, fport,
	    pcbinfo->ipi_hashmask)];
	CK_LIST_FOREACH(inp, head, inp_hash_exact) {
		if (in_pcblookup_exact_match(inp, faddr, fport, laddr, lport))
			return (inp);
	}
	return (NULL);
}

typedef enum {
	INPLOOKUP_MATCH_NONE = 0,
	INPLOOKUP_MATCH_WILD = 1,
	INPLOOKUP_MATCH_LADDR = 2,
} inp_lookup_match_t;

static inp_lookup_match_t
in_pcblookup_wild_match(const struct inpcb *inp, struct in_addr laddr,
    u_short lport)
{
#ifdef INET6
	/* XXX inp locking */
	if ((inp->inp_vflag & INP_IPV4) == 0)
		return (INPLOOKUP_MATCH_NONE);
#endif
	if (inp->inp_faddr.s_addr != INADDR_ANY || inp->inp_lport != lport)
		return (INPLOOKUP_MATCH_NONE);
	if (inp->inp_laddr.s_addr == INADDR_ANY)
		return (INPLOOKUP_MATCH_WILD);
	if (inp->inp_laddr.s_addr == laddr.s_addr)
		return (INPLOOKUP_MATCH_LADDR);
	return (INPLOOKUP_MATCH_NONE);
}

#define	INP_LOOKUP_AGAIN	((struct inpcb *)(uintptr_t)-1)

static struct inpcb *
in_pcblookup_hash_wild_smr(struct inpcbinfo *pcbinfo, struct in_addr faddr,
    u_short fport, struct in_addr laddr, u_short lport,
    const inp_lookup_t lockflags)
{
	struct inpcbhead *head;
	struct inpcb *inp;

	KASSERT(SMR_ENTERED(pcbinfo->ipi_smr),
	    ("%s: not in SMR read section", __func__));

	head = &pcbinfo->ipi_hash_wild[INP_PCBHASH_WILD(lport,
	    pcbinfo->ipi_hashmask)];
	CK_LIST_FOREACH(inp, head, inp_hash_wild) {
		inp_lookup_match_t match;

		match = in_pcblookup_wild_match(inp, laddr, lport);
		if (match == INPLOOKUP_MATCH_NONE)
			continue;

		if (__predict_true(inp_smr_lock(inp, lockflags))) {
			match = in_pcblookup_wild_match(inp, laddr, lport);
			if (match != INPLOOKUP_MATCH_NONE &&
			    prison_check_ip4_locked(inp->inp_cred->cr_prison,
			    &laddr) == 0)
				return (inp);
			inp_unlock(inp, lockflags);
		}

		/*
		 * The matching socket disappeared out from under us.  Fall back
		 * to a serialized lookup.
		 */
		return (INP_LOOKUP_AGAIN);
	}
	return (NULL);
}

static struct inpcb *
in_pcblookup_hash_wild_locked(struct inpcbinfo *pcbinfo, struct in_addr faddr,
    u_short fport, struct in_addr laddr, u_short lport)
{
	struct inpcbhead *head;
	struct inpcb *inp, *local_wild, *local_exact, *jail_wild;
#ifdef INET6
	struct inpcb *local_wild_mapped;
#endif

	INP_HASH_LOCK_ASSERT(pcbinfo);

	/*
	 * Order of socket selection - we always prefer jails.
	 *      1. jailed, non-wild.
	 *      2. jailed, wild.
	 *      3. non-jailed, non-wild.
	 *      4. non-jailed, wild.
	 */
	head = &pcbinfo->ipi_hash_wild[INP_PCBHASH_WILD(lport,
	    pcbinfo->ipi_hashmask)];
	local_wild = local_exact = jail_wild = NULL;
#ifdef INET6
	local_wild_mapped = NULL;
#endif
	CK_LIST_FOREACH(inp, head, inp_hash_wild) {
		inp_lookup_match_t match;
		bool injail;

		match = in_pcblookup_wild_match(inp, laddr, lport);
		if (match == INPLOOKUP_MATCH_NONE)
			continue;

		injail = prison_flag(inp->inp_cred, PR_IP4) != 0;
		if (injail) {
			if (prison_check_ip4_locked(inp->inp_cred->cr_prison,
			    &laddr) != 0)
				continue;
		} else {
			if (local_exact != NULL)
				continue;
		}

		if (match == INPLOOKUP_MATCH_LADDR) {
			if (injail)
				return (inp);
			local_exact = inp;
		} else {
#ifdef INET6
			/* XXX inp locking, NULL check */
			if (inp->inp_vflag & INP_IPV6PROTO)
				local_wild_mapped = inp;
			else
#endif
				if (injail)
					jail_wild = inp;
				else
					local_wild = inp;
		}
	}
	if (jail_wild != NULL)
		return (jail_wild);
	if (local_exact != NULL)
		return (local_exact);
	if (local_wild != NULL)
		return (local_wild);
#ifdef INET6
	if (local_wild_mapped != NULL)
		return (local_wild_mapped);
#endif
	return (NULL);
}

/*
 * Lookup PCB in hash list, using pcbinfo tables.  This variation assumes
 * that the caller has either locked the hash list, which usually happens
 * for bind(2) operations, or is in SMR section, which happens when sorting
 * out incoming packets.
 */
static struct inpcb *
in_pcblookup_hash_locked(struct inpcbinfo *pcbinfo, struct in_addr faddr,
    u_int fport_arg, struct in_addr laddr, u_int lport_arg, int lookupflags,
    uint8_t numa_domain)
{
	struct inpcb *inp;
	const u_short fport = fport_arg, lport = lport_arg;

	KASSERT((lookupflags & ~INPLOOKUP_WILDCARD) == 0,
	    ("%s: invalid lookup flags %d", __func__, lookupflags));
	KASSERT(faddr.s_addr != INADDR_ANY,
	    ("%s: invalid foreign address", __func__));
	KASSERT(laddr.s_addr != INADDR_ANY,
	    ("%s: invalid local address", __func__));
	INP_HASH_WLOCK_ASSERT(pcbinfo);

	inp = in_pcblookup_hash_exact(pcbinfo, faddr, fport, laddr, lport);
	if (inp != NULL)
		return (inp);

	if ((lookupflags & INPLOOKUP_WILDCARD) != 0) {
		inp = in_pcblookup_lbgroup(pcbinfo, &faddr, fport,
		    &laddr, lport, numa_domain);
		if (inp == NULL) {
			inp = in_pcblookup_hash_wild_locked(pcbinfo, faddr,
			    fport, laddr, lport);
		}
	}

	return (inp);
}

static struct inpcb *
in_pcblookup_hash(struct inpcbinfo *pcbinfo, struct in_addr faddr,
    u_int fport, struct in_addr laddr, u_int lport, int lookupflags,
    uint8_t numa_domain)
{
	struct inpcb *inp;
	const inp_lookup_t lockflags = lookupflags & INPLOOKUP_LOCKMASK;

	KASSERT((lookupflags & (INPLOOKUP_RLOCKPCB | INPLOOKUP_WLOCKPCB)) != 0,
	    ("%s: LOCKPCB not set", __func__));

	INP_HASH_WLOCK(pcbinfo);
	inp = in_pcblookup_hash_locked(pcbinfo, faddr, fport, laddr, lport,
	    lookupflags & ~INPLOOKUP_LOCKMASK, numa_domain);
	if (inp != NULL && !inp_trylock(inp, lockflags)) {
		in_pcbref(inp);
		INP_HASH_WUNLOCK(pcbinfo);
		inp_lock(inp, lockflags);
		if (in_pcbrele(inp, lockflags))
			/* XXX-MJ or retry until we get a negative match? */
			inp = NULL;
	} else {
		INP_HASH_WUNLOCK(pcbinfo);
	}
	return (inp);
}

static struct inpcb *
in_pcblookup_hash_smr(struct inpcbinfo *pcbinfo, struct in_addr faddr,
    u_int fport_arg, struct in_addr laddr, u_int lport_arg, int lookupflags,
    uint8_t numa_domain)
{
	struct inpcb *inp;
	const inp_lookup_t lockflags = lookupflags & INPLOOKUP_LOCKMASK;
	const u_short fport = fport_arg, lport = lport_arg;

	KASSERT((lookupflags & ~INPLOOKUP_MASK) == 0,
	    ("%s: invalid lookup flags %d", __func__, lookupflags));
	KASSERT((lookupflags & (INPLOOKUP_RLOCKPCB | INPLOOKUP_WLOCKPCB)) != 0,
	    ("%s: LOCKPCB not set", __func__));

	smr_enter(pcbinfo->ipi_smr);
	inp = in_pcblookup_hash_exact(pcbinfo, faddr, fport, laddr, lport);
	if (inp != NULL) {
		if (__predict_true(inp_smr_lock(inp, lockflags))) {
			/*
			 * Revalidate the 4-tuple, the socket could have been
			 * disconnected.
			 */
			if (__predict_true(in_pcblookup_exact_match(inp,
			    faddr, fport, laddr, lport)))
				return (inp);
			inp_unlock(inp, lockflags);
		}

		/*
		 * We failed to lock the inpcb, or its connection state changed
		 * out from under us.  Fall back to a precise search.
		 */
		return (in_pcblookup_hash(pcbinfo, faddr, fport, laddr, lport,
		    lookupflags, numa_domain));
	}

	if ((lookupflags & INPLOOKUP_WILDCARD) != 0) {
		inp = in_pcblookup_lbgroup(pcbinfo, &faddr, fport,
		    &laddr, lport, numa_domain);
		if (inp != NULL) {
			if (__predict_true(inp_smr_lock(inp, lockflags))) {
				if (__predict_true(in_pcblookup_wild_match(inp,
				    laddr, lport) != INPLOOKUP_MATCH_NONE))
					return (inp);
				inp_unlock(inp, lockflags);
			}
			inp = INP_LOOKUP_AGAIN;
		} else {
			inp = in_pcblookup_hash_wild_smr(pcbinfo, faddr, fport,
			    laddr, lport, lockflags);
		}
		if (inp == INP_LOOKUP_AGAIN) {
			return (in_pcblookup_hash(pcbinfo, faddr, fport, laddr,
			    lport, lookupflags, numa_domain));
		}
	}

	if (inp == NULL)
		smr_exit(pcbinfo->ipi_smr);

	return (inp);
}

/*
 * Public inpcb lookup routines, accepting a 4-tuple, and optionally, an mbuf
 * from which a pre-calculated hash value may be extracted.
 */
struct inpcb *
in_pcblookup(struct inpcbinfo *pcbinfo, struct in_addr faddr, u_int fport,
    struct in_addr laddr, u_int lport, int lookupflags,
    struct ifnet *ifp __unused)
{
	return (in_pcblookup_hash_smr(pcbinfo, faddr, fport, laddr, lport,
	    lookupflags, M_NODOM));
}

struct inpcb *
in_pcblookup_mbuf(struct inpcbinfo *pcbinfo, struct in_addr faddr,
    u_int fport, struct in_addr laddr, u_int lport, int lookupflags,
    struct ifnet *ifp __unused, struct mbuf *m)
{
	return (in_pcblookup_hash_smr(pcbinfo, faddr, fport, laddr, lport,
	    lookupflags, m->m_pkthdr.numa_domain));
}
#endif /* INET */

static bool
in_pcbjailed(const struct inpcb *inp, unsigned int flag)
{
	return (prison_flag(inp->inp_cred, flag) != 0);
}

/*
 * Insert the PCB into a hash chain using ordering rules which ensure that
 * in_pcblookup_hash_wild_*() always encounter the highest-ranking PCB first.
 *
 * Specifically, keep jailed PCBs in front of non-jailed PCBs, and keep PCBs
 * with exact local addresses ahead of wildcard PCBs.  Unbound v4-mapped v6 PCBs
 * always appear last no matter whether they are jailed.
 */
static void
_in_pcbinshash_wild(struct inpcbhead *pcbhash, struct inpcb *inp)
{
	struct inpcb *last;
	bool bound, injail;

	INP_LOCK_ASSERT(inp);
	INP_HASH_WLOCK_ASSERT(inp->inp_pcbinfo);

	last = NULL;
	bound = inp->inp_laddr.s_addr != INADDR_ANY;
	if (!bound && (inp->inp_vflag & INP_IPV6PROTO) != 0) {
		CK_LIST_FOREACH(last, pcbhash, inp_hash_wild) {
			if (CK_LIST_NEXT(last, inp_hash_wild) == NULL) {
				CK_LIST_INSERT_AFTER(last, inp, inp_hash_wild);
				return;
			}
		}
		CK_LIST_INSERT_HEAD(pcbhash, inp, inp_hash_wild);
		return;
	}

	injail = in_pcbjailed(inp, PR_IP4);
	if (!injail) {
		CK_LIST_FOREACH(last, pcbhash, inp_hash_wild) {
			if (!in_pcbjailed(last, PR_IP4))
				break;
			if (CK_LIST_NEXT(last, inp_hash_wild) == NULL) {
				CK_LIST_INSERT_AFTER(last, inp, inp_hash_wild);
				return;
			}
		}
	} else if (!CK_LIST_EMPTY(pcbhash) &&
	    !in_pcbjailed(CK_LIST_FIRST(pcbhash), PR_IP4)) {
		CK_LIST_INSERT_HEAD(pcbhash, inp, inp_hash_wild);
		return;
	}
	if (!bound) {
		CK_LIST_FOREACH_FROM(last, pcbhash, inp_hash_wild) {
			if (last->inp_laddr.s_addr == INADDR_ANY)
				break;
			if (CK_LIST_NEXT(last, inp_hash_wild) == NULL) {
				CK_LIST_INSERT_AFTER(last, inp, inp_hash_wild);
				return;
			}
		}
	}
	if (last == NULL)
		CK_LIST_INSERT_HEAD(pcbhash, inp, inp_hash_wild);
	else
		CK_LIST_INSERT_BEFORE(last, inp, inp_hash_wild);
}

#ifdef INET6
/*
 * See the comment above _in_pcbinshash_wild().
 */
static void
_in6_pcbinshash_wild(struct inpcbhead *pcbhash, struct inpcb *inp)
{
	struct inpcb *last;
	bool bound, injail;

	INP_LOCK_ASSERT(inp);
	INP_HASH_WLOCK_ASSERT(inp->inp_pcbinfo);

	last = NULL;
	bound = !IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr);
	injail = in_pcbjailed(inp, PR_IP6);
	if (!injail) {
		CK_LIST_FOREACH(last, pcbhash, inp_hash_wild) {
			if (!in_pcbjailed(last, PR_IP6))
				break;
			if (CK_LIST_NEXT(last, inp_hash_wild) == NULL) {
				CK_LIST_INSERT_AFTER(last, inp, inp_hash_wild);
				return;
			}
		}
	} else if (!CK_LIST_EMPTY(pcbhash) &&
	    !in_pcbjailed(CK_LIST_FIRST(pcbhash), PR_IP6)) {
		CK_LIST_INSERT_HEAD(pcbhash, inp, inp_hash_wild);
		return;
	}
	if (!bound) {
		CK_LIST_FOREACH_FROM(last, pcbhash, inp_hash_wild) {
			if (IN6_IS_ADDR_UNSPECIFIED(&last->in6p_laddr))
				break;
			if (CK_LIST_NEXT(last, inp_hash_wild) == NULL) {
				CK_LIST_INSERT_AFTER(last, inp, inp_hash_wild);
				return;
			}
		}
	}
	if (last == NULL)
		CK_LIST_INSERT_HEAD(pcbhash, inp, inp_hash_wild);
	else
		CK_LIST_INSERT_BEFORE(last, inp, inp_hash_wild);
}
#endif

/*
 * Insert PCB onto various hash lists.
 */
int
in_pcbinshash(struct inpcb *inp)
{
	struct inpcbhead *pcbhash;
	struct inpcbporthead *pcbporthash;
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;
	struct inpcbport *phd;
	uint32_t hash;
	bool connected;

	INP_WLOCK_ASSERT(inp);
	INP_HASH_WLOCK_ASSERT(pcbinfo);
	KASSERT((inp->inp_flags & INP_INHASHLIST) == 0,
	    ("in_pcbinshash: INP_INHASHLIST"));

#ifdef INET6
	if (inp->inp_vflag & INP_IPV6) {
		hash = INP6_PCBHASH(&inp->in6p_faddr, inp->inp_lport,
		    inp->inp_fport, pcbinfo->ipi_hashmask);
		connected = !IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr);
	} else
#endif
	{
		hash = INP_PCBHASH(&inp->inp_faddr, inp->inp_lport,
		    inp->inp_fport, pcbinfo->ipi_hashmask);
		connected = !in_nullhost(inp->inp_faddr);
	}

	if (connected)
		pcbhash = &pcbinfo->ipi_hash_exact[hash];
	else
		pcbhash = &pcbinfo->ipi_hash_wild[hash];

	pcbporthash = &pcbinfo->ipi_porthashbase[
	    INP_PCBPORTHASH(inp->inp_lport, pcbinfo->ipi_porthashmask)];

	/*
	 * Add entry to load balance group.
	 * Only do this if SO_REUSEPORT_LB is set.
	 */
	if ((inp->inp_socket->so_options & SO_REUSEPORT_LB) != 0) {
		int error = in_pcbinslbgrouphash(inp, M_NODOM);
		if (error != 0)
			return (error);
	}

	/*
	 * Go through port list and look for a head for this lport.
	 */
	CK_LIST_FOREACH(phd, pcbporthash, phd_hash) {
		if (phd->phd_port == inp->inp_lport)
			break;
	}

	/*
	 * If none exists, malloc one and tack it on.
	 */
	if (phd == NULL) {
		phd = uma_zalloc_smr(pcbinfo->ipi_portzone, M_NOWAIT);
		if (phd == NULL) {
			if ((inp->inp_flags & INP_INLBGROUP) != 0)
				in_pcbremlbgrouphash(inp);
			return (ENOMEM);
		}
		phd->phd_port = inp->inp_lport;
		CK_LIST_INIT(&phd->phd_pcblist);
		CK_LIST_INSERT_HEAD(pcbporthash, phd, phd_hash);
	}
	inp->inp_phd = phd;
	CK_LIST_INSERT_HEAD(&phd->phd_pcblist, inp, inp_portlist);

	/*
	 * The PCB may have been disconnected in the past.  Before we can safely
	 * make it visible in the hash table, we must wait for all readers which
	 * may be traversing this PCB to finish.
	 */
	if (inp->inp_smr != SMR_SEQ_INVALID) {
		smr_wait(pcbinfo->ipi_smr, inp->inp_smr);
		inp->inp_smr = SMR_SEQ_INVALID;
	}

	if (connected)
		CK_LIST_INSERT_HEAD(pcbhash, inp, inp_hash_exact);
	else {
#ifdef INET6
		if ((inp->inp_vflag & INP_IPV6) != 0)
			_in6_pcbinshash_wild(pcbhash, inp);
		else
#endif
			_in_pcbinshash_wild(pcbhash, inp);
	}
	inp->inp_flags |= INP_INHASHLIST;

	return (0);
}

void
in_pcbremhash_locked(struct inpcb *inp)
{
	struct inpcbport *phd = inp->inp_phd;

	INP_WLOCK_ASSERT(inp);
	INP_HASH_WLOCK_ASSERT(inp->inp_pcbinfo);
	MPASS(inp->inp_flags & INP_INHASHLIST);

	if ((inp->inp_flags & INP_INLBGROUP) != 0)
		in_pcbremlbgrouphash(inp);
#ifdef INET6
	if (inp->inp_vflag & INP_IPV6) {
		if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr))
			CK_LIST_REMOVE(inp, inp_hash_wild);
		else
			CK_LIST_REMOVE(inp, inp_hash_exact);
	} else
#endif
	{
		if (in_nullhost(inp->inp_faddr))
			CK_LIST_REMOVE(inp, inp_hash_wild);
		else
			CK_LIST_REMOVE(inp, inp_hash_exact);
	}
	CK_LIST_REMOVE(inp, inp_portlist);
	if (CK_LIST_FIRST(&phd->phd_pcblist) == NULL) {
		CK_LIST_REMOVE(phd, phd_hash);
		uma_zfree_smr(inp->inp_pcbinfo->ipi_portzone, phd);
	}
	inp->inp_flags &= ~INP_INHASHLIST;
}

static void
in_pcbremhash(struct inpcb *inp)
{
	INP_HASH_WLOCK(inp->inp_pcbinfo);
	in_pcbremhash_locked(inp);
	INP_HASH_WUNLOCK(inp->inp_pcbinfo);
}

/*
 * Move PCB to the proper hash bucket when { faddr, fport } have  been
 * changed. NOTE: This does not handle the case of the lport changing (the
 * hashed port list would have to be updated as well), so the lport must
 * not change after in_pcbinshash() has been called.
 */
void
in_pcbrehash(struct inpcb *inp)
{
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;
	struct inpcbhead *head;
	uint32_t hash;
	bool connected;

	INP_WLOCK_ASSERT(inp);
	INP_HASH_WLOCK_ASSERT(pcbinfo);
	KASSERT(inp->inp_flags & INP_INHASHLIST,
	    ("%s: !INP_INHASHLIST", __func__));
	KASSERT(inp->inp_smr == SMR_SEQ_INVALID,
	    ("%s: inp was disconnected", __func__));

#ifdef INET6
	if (inp->inp_vflag & INP_IPV6) {
		hash = INP6_PCBHASH(&inp->in6p_faddr, inp->inp_lport,
		    inp->inp_fport, pcbinfo->ipi_hashmask);
		connected = !IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr);
	} else
#endif
	{
		hash = INP_PCBHASH(&inp->inp_faddr, inp->inp_lport,
		    inp->inp_fport, pcbinfo->ipi_hashmask);
		connected = !in_nullhost(inp->inp_faddr);
	}

	/*
	 * When rehashing, the caller must ensure that either the new or the old
	 * foreign address was unspecified.
	 */
	if (connected)
		CK_LIST_REMOVE(inp, inp_hash_wild);
	else
		CK_LIST_REMOVE(inp, inp_hash_exact);

	if (connected) {
		head = &pcbinfo->ipi_hash_exact[hash];
		CK_LIST_INSERT_HEAD(head, inp, inp_hash_exact);
	} else {
		head = &pcbinfo->ipi_hash_wild[hash];
		CK_LIST_INSERT_HEAD(head, inp, inp_hash_wild);
	}
}

/*
 * Check for alternatives when higher level complains
 * about service problems.  For now, invalidate cached
 * routing information.  If the route was created dynamically
 * (by a redirect), time to try a default gateway again.
 */
void
in_losing(struct inpcb *inp)
{

	RO_INVALIDATE_CACHE(&inp->inp_route);
	return;
}

/*
 * A set label operation has occurred at the socket layer, propagate the
 * label change into the in_pcb for the socket.
 */
void
in_pcbsosetlabel(struct socket *so)
{
#ifdef MAC
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("in_pcbsosetlabel: so->so_pcb == NULL"));

	INP_WLOCK(inp);
	SOCK_LOCK(so);
	mac_inpcb_sosetlabel(so, inp);
	SOCK_UNLOCK(so);
	INP_WUNLOCK(inp);
#endif
}

void
inp_wlock(struct inpcb *inp)
{

	INP_WLOCK(inp);
}

void
inp_wunlock(struct inpcb *inp)
{

	INP_WUNLOCK(inp);
}

void
inp_rlock(struct inpcb *inp)
{

	INP_RLOCK(inp);
}

void
inp_runlock(struct inpcb *inp)
{

	INP_RUNLOCK(inp);
}

#ifdef INVARIANT_SUPPORT
void
inp_lock_assert(struct inpcb *inp)
{

	INP_WLOCK_ASSERT(inp);
}

void
inp_unlock_assert(struct inpcb *inp)
{

	INP_UNLOCK_ASSERT(inp);
}
#endif

void
inp_apply_all(struct inpcbinfo *pcbinfo,
    void (*func)(struct inpcb *, void *), void *arg)
{
	struct inpcb_iterator inpi = INP_ALL_ITERATOR(pcbinfo,
	    INPLOOKUP_WLOCKPCB);
	struct inpcb *inp;

	while ((inp = inp_next(&inpi)) != NULL)
		func(inp, arg);
}

struct socket *
inp_inpcbtosocket(struct inpcb *inp)
{

	INP_WLOCK_ASSERT(inp);
	return (inp->inp_socket);
}

struct tcpcb *
inp_inpcbtotcpcb(struct inpcb *inp)
{

	INP_WLOCK_ASSERT(inp);
	return ((struct tcpcb *)inp->inp_ppcb);
}

int
inp_ip_tos_get(const struct inpcb *inp)
{

	return (inp->inp_ip_tos);
}

void
inp_ip_tos_set(struct inpcb *inp, int val)
{

	inp->inp_ip_tos = val;
}

void
inp_4tuple_get(struct inpcb *inp, uint32_t *laddr, uint16_t *lp,
    uint32_t *faddr, uint16_t *fp)
{

	INP_LOCK_ASSERT(inp);
	*laddr = inp->inp_laddr.s_addr;
	*faddr = inp->inp_faddr.s_addr;
	*lp = inp->inp_lport;
	*fp = inp->inp_fport;
}

struct inpcb *
so_sotoinpcb(struct socket *so)
{

	return (sotoinpcb(so));
}

/*
 * Create an external-format (``xinpcb'') structure using the information in
 * the kernel-format in_pcb structure pointed to by inp.  This is done to
 * reduce the spew of irrelevant information over this interface, to isolate
 * user code from changes in the kernel structure, and potentially to provide
 * information-hiding if we decide that some of this information should be
 * hidden from users.
 */
void
in_pcbtoxinpcb(const struct inpcb *inp, struct xinpcb *xi)
{

	bzero(xi, sizeof(*xi));
	xi->xi_len = sizeof(struct xinpcb);
	if (inp->inp_socket)
		sotoxsocket(inp->inp_socket, &xi->xi_socket);
	bcopy(&inp->inp_inc, &xi->inp_inc, sizeof(struct in_conninfo));
	xi->inp_gencnt = inp->inp_gencnt;
	xi->inp_ppcb = (uintptr_t)inp->inp_ppcb;
	xi->inp_flow = inp->inp_flow;
	xi->inp_flowid = inp->inp_flowid;
	xi->inp_flowtype = inp->inp_flowtype;
	xi->inp_flags = inp->inp_flags;
	xi->inp_flags2 = inp->inp_flags2;
	xi->in6p_cksum = inp->in6p_cksum;
	xi->in6p_hops = inp->in6p_hops;
	xi->inp_ip_tos = inp->inp_ip_tos;
	xi->inp_vflag = inp->inp_vflag;
	xi->inp_ip_ttl = inp->inp_ip_ttl;
	xi->inp_ip_p = inp->inp_ip_p;
	xi->inp_ip_minttl = inp->inp_ip_minttl;
}

int
sysctl_setsockopt(SYSCTL_HANDLER_ARGS, struct inpcbinfo *pcbinfo,
    int (*ctloutput_set)(struct inpcb *, struct sockopt *))
{
	struct sockopt sopt;
	struct inpcb_iterator inpi = INP_ALL_ITERATOR(pcbinfo,
	    INPLOOKUP_WLOCKPCB);
	struct inpcb *inp;
	struct sockopt_parameters *params;
	struct socket *so;
	int error;
	char buf[1024];

	if (req->oldptr != NULL || req->oldlen != 0)
		return (EINVAL);
	if (req->newptr == NULL)
		return (EPERM);
	if (req->newlen > sizeof(buf))
		return (ENOMEM);
	error = SYSCTL_IN(req, buf, req->newlen);
	if (error != 0)
		return (error);
	if (req->newlen < sizeof(struct sockopt_parameters))
		return (EINVAL);
	params = (struct sockopt_parameters *)buf;
	sopt.sopt_level = params->sop_level;
	sopt.sopt_name = params->sop_optname;
	sopt.sopt_dir = SOPT_SET;
	sopt.sopt_val = params->sop_optval;
	sopt.sopt_valsize = req->newlen - sizeof(struct sockopt_parameters);
	sopt.sopt_td = NULL;
#ifdef INET6
	if (params->sop_inc.inc_flags & INC_ISIPV6) {
		if (IN6_IS_SCOPE_LINKLOCAL(&params->sop_inc.inc6_laddr))
			params->sop_inc.inc6_laddr.s6_addr16[1] =
			    htons(params->sop_inc.inc6_zoneid & 0xffff);
		if (IN6_IS_SCOPE_LINKLOCAL(&params->sop_inc.inc6_faddr))
			params->sop_inc.inc6_faddr.s6_addr16[1] =
			    htons(params->sop_inc.inc6_zoneid & 0xffff);
	}
#endif
	if (params->sop_inc.inc_lport != htons(0)) {
		if (params->sop_inc.inc_fport == htons(0))
			inpi.hash = INP_PCBHASH_WILD(params->sop_inc.inc_lport,
			    pcbinfo->ipi_hashmask);
		else
#ifdef INET6
			if (params->sop_inc.inc_flags & INC_ISIPV6)
				inpi.hash = INP6_PCBHASH(
				    &params->sop_inc.inc6_faddr,
				    params->sop_inc.inc_lport,
				    params->sop_inc.inc_fport,
				    pcbinfo->ipi_hashmask);
			else
#endif
				inpi.hash = INP_PCBHASH(
				    &params->sop_inc.inc_faddr,
				    params->sop_inc.inc_lport,
				    params->sop_inc.inc_fport,
				    pcbinfo->ipi_hashmask);
	}
	while ((inp = inp_next(&inpi)) != NULL)
		if (inp->inp_gencnt == params->sop_id) {
			if (inp->inp_flags & INP_DROPPED) {
				INP_WUNLOCK(inp);
				return (ECONNRESET);
			}
			so = inp->inp_socket;
			KASSERT(so != NULL, ("inp_socket == NULL"));
			soref(so);
			if (params->sop_level == SOL_SOCKET) {
				INP_WUNLOCK(inp);
				error = sosetopt(so, &sopt);
			} else
				error = (*ctloutput_set)(inp, &sopt);
			sorele(so);
			break;
		}
	if (inp == NULL)
		error = ESRCH;
	return (error);
}

#ifdef DDB
static void
db_print_indent(int indent)
{
	int i;

	for (i = 0; i < indent; i++)
		db_printf(" ");
}

static void
db_print_inconninfo(struct in_conninfo *inc, const char *name, int indent)
{
	char faddr_str[48], laddr_str[48];

	db_print_indent(indent);
	db_printf("%s at %p\n", name, inc);

	indent += 2;

#ifdef INET6
	if (inc->inc_flags & INC_ISIPV6) {
		/* IPv6. */
		ip6_sprintf(laddr_str, &inc->inc6_laddr);
		ip6_sprintf(faddr_str, &inc->inc6_faddr);
	} else
#endif
	{
		/* IPv4. */
		inet_ntoa_r(inc->inc_laddr, laddr_str);
		inet_ntoa_r(inc->inc_faddr, faddr_str);
	}
	db_print_indent(indent);
	db_printf("inc_laddr %s   inc_lport %u\n", laddr_str,
	    ntohs(inc->inc_lport));
	db_print_indent(indent);
	db_printf("inc_faddr %s   inc_fport %u\n", faddr_str,
	    ntohs(inc->inc_fport));
}

static void
db_print_inpflags(int inp_flags)
{
	int comma;

	comma = 0;
	if (inp_flags & INP_RECVOPTS) {
		db_printf("%sINP_RECVOPTS", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_RECVRETOPTS) {
		db_printf("%sINP_RECVRETOPTS", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_RECVDSTADDR) {
		db_printf("%sINP_RECVDSTADDR", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_ORIGDSTADDR) {
		db_printf("%sINP_ORIGDSTADDR", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_HDRINCL) {
		db_printf("%sINP_HDRINCL", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_HIGHPORT) {
		db_printf("%sINP_HIGHPORT", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_LOWPORT) {
		db_printf("%sINP_LOWPORT", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_ANONPORT) {
		db_printf("%sINP_ANONPORT", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_RECVIF) {
		db_printf("%sINP_RECVIF", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_MTUDISC) {
		db_printf("%sINP_MTUDISC", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_RECVTTL) {
		db_printf("%sINP_RECVTTL", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_DONTFRAG) {
		db_printf("%sINP_DONTFRAG", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_RECVTOS) {
		db_printf("%sINP_RECVTOS", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_IPV6_V6ONLY) {
		db_printf("%sIN6P_IPV6_V6ONLY", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_PKTINFO) {
		db_printf("%sIN6P_PKTINFO", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_HOPLIMIT) {
		db_printf("%sIN6P_HOPLIMIT", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_HOPOPTS) {
		db_printf("%sIN6P_HOPOPTS", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_DSTOPTS) {
		db_printf("%sIN6P_DSTOPTS", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_RTHDR) {
		db_printf("%sIN6P_RTHDR", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_RTHDRDSTOPTS) {
		db_printf("%sIN6P_RTHDRDSTOPTS", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_TCLASS) {
		db_printf("%sIN6P_TCLASS", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_AUTOFLOWLABEL) {
		db_printf("%sIN6P_AUTOFLOWLABEL", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_ONESBCAST) {
		db_printf("%sINP_ONESBCAST", comma ? ", " : "");
		comma  = 1;
	}
	if (inp_flags & INP_DROPPED) {
		db_printf("%sINP_DROPPED", comma ? ", " : "");
		comma  = 1;
	}
	if (inp_flags & INP_SOCKREF) {
		db_printf("%sINP_SOCKREF", comma ? ", " : "");
		comma  = 1;
	}
	if (inp_flags & IN6P_RFC2292) {
		db_printf("%sIN6P_RFC2292", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_MTU) {
		db_printf("IN6P_MTU%s", comma ? ", " : "");
		comma = 1;
	}
}

static void
db_print_inpvflag(u_char inp_vflag)
{
	int comma;

	comma = 0;
	if (inp_vflag & INP_IPV4) {
		db_printf("%sINP_IPV4", comma ? ", " : "");
		comma  = 1;
	}
	if (inp_vflag & INP_IPV6) {
		db_printf("%sINP_IPV6", comma ? ", " : "");
		comma  = 1;
	}
	if (inp_vflag & INP_IPV6PROTO) {
		db_printf("%sINP_IPV6PROTO", comma ? ", " : "");
		comma  = 1;
	}
}

static void
db_print_inpcb(struct inpcb *inp, const char *name, int indent)
{

	db_print_indent(indent);
	db_printf("%s at %p\n", name, inp);

	indent += 2;

	db_print_indent(indent);
	db_printf("inp_flow: 0x%x\n", inp->inp_flow);

	db_print_inconninfo(&inp->inp_inc, "inp_conninfo", indent);

	db_print_indent(indent);
	db_printf("inp_ppcb: %p   inp_pcbinfo: %p   inp_socket: %p\n",
	    inp->inp_ppcb, inp->inp_pcbinfo, inp->inp_socket);

	db_print_indent(indent);
	db_printf("inp_label: %p   inp_flags: 0x%x (",
	   inp->inp_label, inp->inp_flags);
	db_print_inpflags(inp->inp_flags);
	db_printf(")\n");

	db_print_indent(indent);
	db_printf("inp_sp: %p   inp_vflag: 0x%x (", inp->inp_sp,
	    inp->inp_vflag);
	db_print_inpvflag(inp->inp_vflag);
	db_printf(")\n");

	db_print_indent(indent);
	db_printf("inp_ip_ttl: %d   inp_ip_p: %d   inp_ip_minttl: %d\n",
	    inp->inp_ip_ttl, inp->inp_ip_p, inp->inp_ip_minttl);

	db_print_indent(indent);
#ifdef INET6
	if (inp->inp_vflag & INP_IPV6) {
		db_printf("in6p_options: %p   in6p_outputopts: %p   "
		    "in6p_moptions: %p\n", inp->in6p_options,
		    inp->in6p_outputopts, inp->in6p_moptions);
		db_printf("in6p_icmp6filt: %p   in6p_cksum %d   "
		    "in6p_hops %u\n", inp->in6p_icmp6filt, inp->in6p_cksum,
		    inp->in6p_hops);
	} else
#endif
	{
		db_printf("inp_ip_tos: %d   inp_ip_options: %p   "
		    "inp_ip_moptions: %p\n", inp->inp_ip_tos,
		    inp->inp_options, inp->inp_moptions);
	}

	db_print_indent(indent);
	db_printf("inp_phd: %p   inp_gencnt: %ju\n", inp->inp_phd,
	    (uintmax_t)inp->inp_gencnt);
}

DB_SHOW_COMMAND(inpcb, db_show_inpcb)
{
	struct inpcb *inp;

	if (!have_addr) {
		db_printf("usage: show inpcb <addr>\n");
		return;
	}
	inp = (struct inpcb *)addr;

	db_print_inpcb(inp, "inpcb", 0);
}
#endif /* DDB */

#ifdef RATELIMIT
/*
 * Modify TX rate limit based on the existing "inp->inp_snd_tag",
 * if any.
 */
int
in_pcbmodify_txrtlmt(struct inpcb *inp, uint32_t max_pacing_rate)
{
	union if_snd_tag_modify_params params = {
		.rate_limit.max_rate = max_pacing_rate,
		.rate_limit.flags = M_NOWAIT,
	};
	struct m_snd_tag *mst;
	int error;

	mst = inp->inp_snd_tag;
	if (mst == NULL)
		return (EINVAL);

	if (mst->sw->snd_tag_modify == NULL) {
		error = EOPNOTSUPP;
	} else {
		error = mst->sw->snd_tag_modify(mst, &params);
	}
	return (error);
}

/*
 * Query existing TX rate limit based on the existing
 * "inp->inp_snd_tag", if any.
 */
int
in_pcbquery_txrtlmt(struct inpcb *inp, uint32_t *p_max_pacing_rate)
{
	union if_snd_tag_query_params params = { };
	struct m_snd_tag *mst;
	int error;

	mst = inp->inp_snd_tag;
	if (mst == NULL)
		return (EINVAL);

	if (mst->sw->snd_tag_query == NULL) {
		error = EOPNOTSUPP;
	} else {
		error = mst->sw->snd_tag_query(mst, &params);
		if (error == 0 && p_max_pacing_rate != NULL)
			*p_max_pacing_rate = params.rate_limit.max_rate;
	}
	return (error);
}

/*
 * Query existing TX queue level based on the existing
 * "inp->inp_snd_tag", if any.
 */
int
in_pcbquery_txrlevel(struct inpcb *inp, uint32_t *p_txqueue_level)
{
	union if_snd_tag_query_params params = { };
	struct m_snd_tag *mst;
	int error;

	mst = inp->inp_snd_tag;
	if (mst == NULL)
		return (EINVAL);

	if (mst->sw->snd_tag_query == NULL)
		return (EOPNOTSUPP);

	error = mst->sw->snd_tag_query(mst, &params);
	if (error == 0 && p_txqueue_level != NULL)
		*p_txqueue_level = params.rate_limit.queue_level;
	return (error);
}

/*
 * Allocate a new TX rate limit send tag from the network interface
 * given by the "ifp" argument and save it in "inp->inp_snd_tag":
 */
int
in_pcbattach_txrtlmt(struct inpcb *inp, struct ifnet *ifp,
    uint32_t flowtype, uint32_t flowid, uint32_t max_pacing_rate, struct m_snd_tag **st)

{
	union if_snd_tag_alloc_params params = {
		.rate_limit.hdr.type = (max_pacing_rate == -1U) ?
		    IF_SND_TAG_TYPE_UNLIMITED : IF_SND_TAG_TYPE_RATE_LIMIT,
		.rate_limit.hdr.flowid = flowid,
		.rate_limit.hdr.flowtype = flowtype,
		.rate_limit.hdr.numa_domain = inp->inp_numa_domain,
		.rate_limit.max_rate = max_pacing_rate,
		.rate_limit.flags = M_NOWAIT,
	};
	int error;

	INP_WLOCK_ASSERT(inp);

	/*
	 * If there is already a send tag, or the INP is being torn
	 * down, allocating a new send tag is not allowed. Else send
	 * tags may leak.
	 */
	if (*st != NULL || (inp->inp_flags & INP_DROPPED) != 0)
		return (EINVAL);

	error = m_snd_tag_alloc(ifp, &params, st);
#ifdef INET
	if (error == 0) {
		counter_u64_add(rate_limit_set_ok, 1);
		counter_u64_add(rate_limit_active, 1);
	} else if (error != EOPNOTSUPP)
		  counter_u64_add(rate_limit_alloc_fail, 1);
#endif
	return (error);
}

void
in_pcbdetach_tag(struct m_snd_tag *mst)
{

	m_snd_tag_rele(mst);
#ifdef INET
	counter_u64_add(rate_limit_active, -1);
#endif
}

/*
 * Free an existing TX rate limit tag based on the "inp->inp_snd_tag",
 * if any:
 */
void
in_pcbdetach_txrtlmt(struct inpcb *inp)
{
	struct m_snd_tag *mst;

	INP_WLOCK_ASSERT(inp);

	mst = inp->inp_snd_tag;
	inp->inp_snd_tag = NULL;

	if (mst == NULL)
		return;

	m_snd_tag_rele(mst);
#ifdef INET
	counter_u64_add(rate_limit_active, -1);
#endif
}

int
in_pcboutput_txrtlmt_locked(struct inpcb *inp, struct ifnet *ifp, struct mbuf *mb, uint32_t max_pacing_rate)
{
	int error;

	/*
	 * If the existing send tag is for the wrong interface due to
	 * a route change, first drop the existing tag.  Set the
	 * CHANGED flag so that we will keep trying to allocate a new
	 * tag if we fail to allocate one this time.
	 */
	if (inp->inp_snd_tag != NULL && inp->inp_snd_tag->ifp != ifp) {
		in_pcbdetach_txrtlmt(inp);
		inp->inp_flags2 |= INP_RATE_LIMIT_CHANGED;
	}

	/*
	 * NOTE: When attaching to a network interface a reference is
	 * made to ensure the network interface doesn't go away until
	 * all ratelimit connections are gone. The network interface
	 * pointers compared below represent valid network interfaces,
	 * except when comparing towards NULL.
	 */
	if (max_pacing_rate == 0 && inp->inp_snd_tag == NULL) {
		error = 0;
	} else if (!(ifp->if_capenable & IFCAP_TXRTLMT)) {
		if (inp->inp_snd_tag != NULL)
			in_pcbdetach_txrtlmt(inp);
		error = 0;
	} else if (inp->inp_snd_tag == NULL) {
		/*
		 * In order to utilize packet pacing with RSS, we need
		 * to wait until there is a valid RSS hash before we
		 * can proceed:
		 */
		if (M_HASHTYPE_GET(mb) == M_HASHTYPE_NONE) {
			error = EAGAIN;
		} else {
			error = in_pcbattach_txrtlmt(inp, ifp, M_HASHTYPE_GET(mb),
			    mb->m_pkthdr.flowid, max_pacing_rate, &inp->inp_snd_tag);
		}
	} else {
		error = in_pcbmodify_txrtlmt(inp, max_pacing_rate);
	}
	if (error == 0 || error == EOPNOTSUPP)
		inp->inp_flags2 &= ~INP_RATE_LIMIT_CHANGED;

	return (error);
}

/*
 * This function should be called when the INP_RATE_LIMIT_CHANGED flag
 * is set in the fast path and will attach/detach/modify the TX rate
 * limit send tag based on the socket's so_max_pacing_rate value.
 */
void
in_pcboutput_txrtlmt(struct inpcb *inp, struct ifnet *ifp, struct mbuf *mb)
{
	struct socket *socket;
	uint32_t max_pacing_rate;
	bool did_upgrade;

	if (inp == NULL)
		return;

	socket = inp->inp_socket;
	if (socket == NULL)
		return;

	if (!INP_WLOCKED(inp)) {
		/*
		 * NOTE: If the write locking fails, we need to bail
		 * out and use the non-ratelimited ring for the
		 * transmit until there is a new chance to get the
		 * write lock.
		 */
		if (!INP_TRY_UPGRADE(inp))
			return;
		did_upgrade = 1;
	} else {
		did_upgrade = 0;
	}

	/*
	 * NOTE: The so_max_pacing_rate value is read unlocked,
	 * because atomic updates are not required since the variable
	 * is checked at every mbuf we send. It is assumed that the
	 * variable read itself will be atomic.
	 */
	max_pacing_rate = socket->so_max_pacing_rate;

	in_pcboutput_txrtlmt_locked(inp, ifp, mb, max_pacing_rate);

	if (did_upgrade)
		INP_DOWNGRADE(inp);
}

/*
 * Track route changes for TX rate limiting.
 */
void
in_pcboutput_eagain(struct inpcb *inp)
{
	bool did_upgrade;

	if (inp == NULL)
		return;

	if (inp->inp_snd_tag == NULL)
		return;

	if (!INP_WLOCKED(inp)) {
		/*
		 * NOTE: If the write locking fails, we need to bail
		 * out and use the non-ratelimited ring for the
		 * transmit until there is a new chance to get the
		 * write lock.
		 */
		if (!INP_TRY_UPGRADE(inp))
			return;
		did_upgrade = 1;
	} else {
		did_upgrade = 0;
	}

	/* detach rate limiting */
	in_pcbdetach_txrtlmt(inp);

	/* make sure new mbuf send tag allocation is made */
	inp->inp_flags2 |= INP_RATE_LIMIT_CHANGED;

	if (did_upgrade)
		INP_DOWNGRADE(inp);
}

#ifdef INET
static void
rl_init(void *st)
{
	rate_limit_new = counter_u64_alloc(M_WAITOK);
	rate_limit_chg = counter_u64_alloc(M_WAITOK);
	rate_limit_active = counter_u64_alloc(M_WAITOK);
	rate_limit_alloc_fail = counter_u64_alloc(M_WAITOK);
	rate_limit_set_ok = counter_u64_alloc(M_WAITOK);
}

SYSINIT(rl, SI_SUB_PROTO_DOMAININIT, SI_ORDER_ANY, rl_init, NULL);
#endif
#endif /* RATELIMIT */
