/*-
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
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

static const char vpsid[] =
    "$Id: vps_restore.c 164 2013-06-10 12:46:17Z klaus $";

#include <sys/cdefs.h>

#include "opt_ddb.h"
#include "opt_ktrace.h"
#include "opt_global.h"
#include "opt_compat.h"
#include "opt_kstack_max_pages.h"

#ifdef VPS

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/refcount.h>
#include <sys/sched.h>
#include <sys/sockio.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/ttycom.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/resourcevar.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/syscallsubr.h>
#include <sys/mman.h>
#include <sys/sleepqueue.h>
#include <sys/filedesc.h>
#include <sys/mount.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/pipe.h>
#include <sys/tty.h>
#include <sys/syscall.h>
#include <sys/jail.h>
#include <sys/ktrace.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/loginclass.h>
#include <sys/vmmeter.h>
#include <sys/event.h>
#include <sys/eventvar.h>
#include <sys/stat.h>
#include <sys/kdb.h>

#include <net/if.h>
#include <net/radix.h>
#include <net/route.h>
#include <netinet/in.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_kern.h>
#include <vm/vm_pageout.h>

#include <machine/pcb.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <net/vnet.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_fsm.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include "vps_account.h"
#include "vps_user.h"
#include "vps.h"
#include "vps2.h"
#include <machine/vps_md.h>

#define _VPS_SNAPST_H_ALL
#include "vps_snapst.h"

#include "vps_libdump.h"

/* see vm/vm_glue.c */
#ifndef KSTACK_MAX_PAGES
#define KSTACK_MAX_PAGES 32
#endif


#define ERRMSG vps_snapst_pusherrormsg

#ifdef DIAGNOSTIC

#define DBGR if (debug_restore) printf

static int debug_restore = 1;
SYSCTL_INT(_debug, OID_AUTO, vps_restore_debug, CTLFLAG_RW,
    &debug_restore, 0, "");

#else

#define DBGR(x, ...)

#endif /* DIAGNOSTIC */

static int debug_restore_ktrace = 0;
SYSCTL_INT(_debug, OID_AUTO, vps_restore_ktrace, CTLFLAG_RW,
    &debug_restore_ktrace, 0, "");

int sys_posix_openpt_unit(struct thread *, struct posix_openpt_args *, int);
int ktrops(struct thread *,struct proc *,int,int,struct vnode *);

MALLOC_DEFINE(M_VPS_RESTORE, "vps_restore",
    "Virtual Private Systems Restore memory");

void vps_restore_return(struct thread *, struct trapframe *);
static struct prison *vps_restore_prison_lookup(
    struct vps_snapst_ctx *ctx, struct vps *vps, struct prison *old_pr);

static int vps_restore_mod_refcnt;

/*
 * * * * * Restore functions. * * * *
 */

static struct ucred *vps_restore_ucred_lookup(struct vps_snapst_ctx *ctx,
    struct vps *vps, void *orig_ptr);

VPSFUNC
static int
vps_restore_ucred(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct vps_restore_obj *ro;
	struct vps_dumpobj *o1;
	struct vps_dump_ucred *vdcr;
	struct ucred *ncr;
	gid_t *tmp_groups;
	int i;

	o1 = vdo_next(ctx);

	if (o1->type != VPS_DUMPOBJT_UCRED) {
		ERRMSG(ctx, "%s: o1=%p type=%d != VPS_DUMPOBJT_UCRED\n",
		    __func__, o1, o1->type);
		return (EINVAL);
	}
	vdcr = (struct vps_dump_ucred *)o1->data;

	KASSERT(vps != NULL, ("%s: vps == NULL\n", __func__));

	if ((ncr = vps_restore_ucred_lookup(ctx, vps, vdcr->cr_origptr)) !=
	    NULL) {
		/* debugging
		panic("%s: double restore, orig_ptr=%p !\n",
		    __func__, vdcr->cr_origptr);
		*/
		DBGR("%s: double restore, orig_ptr=%p !\n",
		    __func__, vdcr->cr_origptr);
		/* Already restored. */
		crfree(ncr);
		return (0);
	}

	ncr = crget();
	ncr->cr_vps = vps;
	vps_ref(ncr->cr_vps, ncr);

	/* Is re-set in fixup routine, after prisons are restored. */
	ncr->cr_prison = VPS_VPS(vps, prison0);
	prison_hold(ncr->cr_prison);

	ncr->cr_uid = vdcr->cr_uid;
	ncr->cr_ruid = vdcr->cr_ruid;
	ncr->cr_svuid = vdcr->cr_svuid;
	ncr->cr_rgid = vdcr->cr_rgid;
	ncr->cr_svgid = vdcr->cr_svgid;

	ncr->cr_loginclass = loginclass_find("default");
	ncr->cr_uidinfo = uifind(vdcr->cr_uid);
	ncr->cr_ruidinfo = uifind(vdcr->cr_ruid);

	if ((caddr_t)vdcr->cr_groups +
	     (sizeof(vdcr->cr_groups[0]) * vdcr->cr_ngroups) >
	     (caddr_t)o1 + o1->size) {
		ERRMSG(ctx, "%s: vdcr->cr_groups smaller than specified by "
		    "vdcr->cr_ngroups=%d\n", __func__, vdcr->cr_ngroups);
		return (EINVAL);
	}
	tmp_groups = malloc(sizeof(tmp_groups[0]) * vdcr->cr_ngroups,
	    M_TEMP, M_WAITOK);
	for (i = 0; i < vdcr->cr_ngroups; i++)
		tmp_groups[i] = vdcr->cr_groups[i];
	crsetgroups(ncr, vdcr->cr_ngroups, tmp_groups);
	free(tmp_groups, M_TEMP);

	ncr->cr_flags = vdcr->cr_flags;
	ncr->cr_pspare2[0] = NULL;
	ncr->cr_pspare2[1] = NULL;
	ncr->cr_label = NULL;
	memset(&ncr->cr_audit, 0, sizeof(ncr->cr_audit));

	ro = malloc(sizeof(*ro), M_VPS_RESTORE, M_WAITOK | M_ZERO);
	ro->type = VPS_DUMPOBJT_UCRED;
	ro->orig_ptr = vdcr->cr_origptr;
	ro->new_ptr = ncr;
	ro->spare[0] = vdcr->cr_prison;
	SLIST_INSERT_HEAD(&ctx->obj_list, ro, list);

	DBGR("%s: ncr=%p\n", __func__, ncr);

	return (0);
}

VPSFUNC
static int
vps_restore_ucred_all(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct vps_dumpobj *o1, *o2;
	int error = 0;
	int cnt = 0;

	o2 = ctx->curobj;
	o1 = ctx->rootobj;
	ctx->curobj = o1;
	do {
		if (vdo_typeofnext(ctx) != VPS_DUMPOBJT_UCRED) {
			o1 = vdo_next(ctx);
			continue;
		}

		if ((error = vps_restore_ucred(ctx, vps)))
			break;

		++cnt;

	} while (o1 != NULL);

	DBGR("%s: restored %d ucreds\n", __func__, cnt);

	/* Reset ctx to where it was. */
	ctx->curobj = o2;
	ctx->lastobj = o2;

	return (error);
}

VPSFUNC
__attribute__((unused))
static int
vps_restore_ucred_checkall(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct vps_dumpobj *o1, *o2;
	struct vps_dump_ucred *vdcr;
	struct ucred *ncr;
	int cnt = 0;

	o2 = ctx->curobj;
	o1 = ctx->rootobj;
	ctx->curobj = o1;
	do {
		if (vdo_typeofnext(ctx) != VPS_DUMPOBJT_UCRED) {
			o1 = vdo_next(ctx);
			continue;
		}

		o1 = vdo_next(ctx);
		vdcr = (struct vps_dump_ucred *)o1->data;
		ncr = vps_restore_ucred_lookup(ctx, vps, vdcr->cr_origptr);
		crfree(ncr);
		KASSERT(ncr != NULL, ("%s: ncr==NULL\n", __func__));
		/* There is still one extra reference that is kept during
		   the restore run. */
		if (ncr->cr_ref != vdcr->cr_ref + 1)
			DBGR("%s: ncr=%p orig=%p ncr->cr_ref=%d "
			    "vdcr->cr_ref=%d\n", __func__, ncr,
			    vdcr->cr_origptr, ncr->cr_ref - 1,
			    vdcr->cr_ref);

		++cnt;

	} while (o1 != NULL);

	DBGR("%s: checked %d ucreds\n", __func__, cnt);

	/* Reset ctx to where it was. */
	ctx->curobj = o2;
	ctx->lastobj = o2;

	return (0);
}

VPSFUNC
static struct ucred *
vps_restore_ucred_lookup(struct vps_snapst_ctx *ctx, struct vps *vps,
    void *orig_ptr)
{
	struct vps_restore_obj *ro;
	struct ucred *ncr;

	ncr = NULL;
	SLIST_FOREACH(ro, &ctx->obj_list, list)
		if (ro->type == VPS_DUMPOBJT_UCRED &&
		    ro->orig_ptr == orig_ptr) {
			ncr = ro->new_ptr;
			break;
		}

	if (ncr != NULL) {
		DBGR("%s: found ucred %p for %p, ncr->ref=%d\n",
		    __func__, ncr, orig_ptr, ncr->cr_ref+1);
		crhold(ncr);
		return (ncr);
	} else {
		DBGR("%s: no ucred found for %p\n", __func__, orig_ptr);
		return (NULL);
	}
}

VPSFUNC
static int
vps_restore_ucred_fixup(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct vps_restore_obj *ro;
	struct prison *pr2;
	struct ucred *cr;

	SLIST_FOREACH(ro, &ctx->obj_list, list) {
		if (ro->type != VPS_DUMPOBJT_UCRED)
			continue;

		cr = ro->new_ptr;
		pr2 = vps_restore_prison_lookup(ctx, vps, ro->spare[0]);
		KASSERT(pr2 != NULL, ("%s: prison not found for "
		    "orig_ptr %p\n", __func__, ro->spare[0]));

		DBGR("%s: cr=%p: %p -> %p\n", __func__, cr,
		    cr->cr_prison, pr2);

		prison_hold(pr2);
		prison_free(cr->cr_prison);
		cr->cr_prison = pr2;
	}

	return (0);
}

VPSFUNC
static int
vps_restore_vnet_route_one(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vnet *vnet, struct vps_dumpobj *o1, struct radix_node_head *rnh,
    int fibnum, int af)
{
	struct vps_dump_route *vdr;
	struct vps_dump_vnet_sockaddr *vds;
	struct rtentry *rt_entry;
	struct sockaddr *dst, *gateway, *netmask;
	size_t saddr_offset;
	int flags;
	int error = 0;

	vdr = (struct vps_dump_route *)o1->data;
	flags = vdr->rt_flags;

	dst = netmask = gateway = NULL;

	saddr_offset = offsetof(struct sockaddr, sa_data);

	DBGR("%s: rt_have_mask=%d rt_have_gateway=%d rt_have_ifa=%d\n",
	    __func__, vdr->rt_have_mask, vdr->rt_have_gateway,
	    vdr->rt_have_ifa);

	if (1) {
		dst = malloc(sizeof(struct sockaddr_storage),
		    M_TEMP, M_WAITOK|M_ZERO);
		vds = (struct vps_dump_vnet_sockaddr *)(vdr + 1);
		dst->sa_len = vds->sa_len;
		dst->sa_family = vds->sa_family;
		if (vds->sa_len - saddr_offset > 0 &&
		    vds->sa_len - saddr_offset <=
		    sizeof(struct sockaddr_storage))
			memcpy(dst->sa_data, vds->sa_data,
			    vds->sa_len - saddr_offset);
	}

	if (vdr->rt_have_mask == 1) {
		netmask = malloc(sizeof(struct sockaddr_storage),
		    M_TEMP, M_WAITOK|M_ZERO);
		vds = (struct vps_dump_vnet_sockaddr *)(vds + 1);
		netmask->sa_len = vds->sa_len;
		netmask->sa_family = vds->sa_family;
		if (vds->sa_len - saddr_offset > 0 &&
		    vds->sa_len - saddr_offset <=
		    sizeof(struct sockaddr_storage))
			memcpy(netmask->sa_data, vds->sa_data,
			    vds->sa_len - saddr_offset);
	}

	if (vdr->rt_have_gateway == 1 || vdr->rt_have_ifa == 1) {
		gateway = malloc(sizeof(struct sockaddr_storage),
		    M_TEMP, M_WAITOK|M_ZERO);
		/* Either rt->rt_gateway or rt->rt_ifa->ifa_addr */
		vds = (struct vps_dump_vnet_sockaddr *)(vds + 1);
		gateway->sa_len = vds->sa_len;
		gateway->sa_family = vds->sa_family;
		if (vds->sa_len - saddr_offset > 0 &&
		    vds->sa_len - saddr_offset <=
		    sizeof(struct sockaddr_storage))
			memcpy(gateway->sa_data, vds->sa_data,
			    vds->sa_len - saddr_offset);
	}

	CURVNET_SET_QUIET(vnet);

	error = rtrequest_fib(RTM_ADD, dst, gateway, netmask,
	    flags, &rt_entry, vdr->rt_fibnum);
	if (error)
		ERRMSG(ctx, "%s: rtrequest_fib: error=%d\n",
		    __func__, error);

	CURVNET_RESTORE();

	if (dst != NULL)
		free(dst, M_TEMP);
	if (netmask != NULL)
		free(netmask, M_TEMP);
	if (gateway != NULL)
		free(gateway, M_TEMP);

	return (error);
}

VPSFUNC
static int
vps_restore_vnet_route(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vnet *vnet)
{
	struct vps_dumpobj *o1, *o2;
	struct radix_node_head *rnh;
	int fibnum;
	int af;
	int error = 0;

	CURVNET_SET_QUIET(vnet);

	/* Freeing and reinitalizing routing tables to have them clean. */
	/* XXX We lose memory this way ... */
	vnet_route_uninit(NULL);
	vnet_route_init(NULL);

	while (vdo_typeofnext(ctx) == VPS_DUMPOBJT_VNET_ROUTETABLE) {

		o1 = vdo_next(ctx);

		fibnum = *(int *)(o1->data + (sizeof(int) * 0));
		af =     *(int *)(o1->data + (sizeof(int) * 1));

		DBGR("%s: fibnum=%d af=%d\n", __func__, fibnum, af);

		rnh = NULL;

		while (vdo_typeofnext(ctx) == VPS_DUMPOBJT_VNET_ROUTE) {

			o2 = vdo_next(ctx);
			if ((error = vps_restore_vnet_route_one(ctx, vps,
			    vnet, o2, rnh, fibnum, af)))
				goto out;
		}
	}

  out:
	CURVNET_RESTORE();

	return (error);
}

VPSFUNC
static int
vps_restore_iface_ifaddr(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct ifnet *ifp)
{
	struct vps_dumpobj *o2;
	struct vps_dump_vnet_ifaddr *vdifaddr;
	struct vps_dump_vnet_sockaddr *vdsaddr;
	struct vps_dump_vnet_inet6_lifetime *vdia6lt;
	struct thread *td;
	struct vnet *savevnet;
	struct in_aliasreq *in_alreq;
	struct in6_aliasreq *in6_alreq;
	size_t saddr_offset;
	int error = 0;
#ifdef DIAGNOSTIC
	char ip6buf[INET6_ADDRSTRLEN];
#endif

	td = curthread;
	curvnet = TD_TO_VNET(td);
	savevnet = curvnet;

	o2 = vdo_next(ctx);

	vdifaddr = (struct vps_dump_vnet_ifaddr *)o2->data;

	if (vdifaddr->have_addr == 0) {
		DBGR("%s: vdifaddr->have_addr == 0\n", __func__);
		return (0);
	}

	vdsaddr = (struct vps_dump_vnet_sockaddr *)
	    (vdifaddr + 1);

	switch (vdsaddr->sa_family) {
	case AF_LINK:
		DBGR("%s: AF_LINK: ignoring\n", __func__);
		break;

	case AF_INET:
		in_alreq = (struct in_aliasreq *)malloc(sizeof(*in_alreq),
		    M_TEMP, M_WAITOK | M_ZERO);
		memcpy(in_alreq->ifra_name, ifp->if_xname,
		    sizeof(in_alreq->ifra_name));

		saddr_offset = offsetof(struct sockaddr_in, sin_port);

		DBGR("%s: ifa: have_addr=%d have_dstaddr=%d "
		    "have_netmask=%d\n", __func__, vdifaddr->have_addr,
		    vdifaddr->have_dstaddr, vdifaddr->have_netmask);

		if (vdifaddr->have_addr &&
		    vdsaddr->sa_len > saddr_offset &&
		    vdsaddr->sa_len <= sizeof(struct sockaddr_in)) {
			in_alreq->ifra_addr.sin_family = vdsaddr->sa_family;
			in_alreq->ifra_addr.sin_len = vdsaddr->sa_len;
			memcpy(&in_alreq->ifra_addr.sin_port,
			    vdsaddr->sa_data,
			    vdsaddr->sa_len - saddr_offset);
			vdsaddr += 1;
		}
		if (vdifaddr->have_dstaddr &&
		    vdsaddr->sa_len > saddr_offset &&
		    vdsaddr->sa_len <= sizeof(struct sockaddr_in)) {
			in_alreq->ifra_dstaddr.sin_family =
			    vdsaddr->sa_family;
			in_alreq->ifra_dstaddr.sin_len =
			    vdsaddr->sa_len;
			memcpy(&in_alreq->ifra_dstaddr.sin_port,
			    vdsaddr->sa_data,
			    vdsaddr->sa_len - saddr_offset);
			vdsaddr += 1;
		}
		if (vdifaddr->have_netmask &&
		    vdsaddr->sa_len > saddr_offset &&
		    vdsaddr->sa_len <= sizeof(struct sockaddr_in)) {
			in_alreq->ifra_mask.sin_family = vdsaddr->sa_family;
			in_alreq->ifra_mask.sin_len = vdsaddr->sa_len;
			memcpy(&in_alreq->ifra_mask.sin_port,
			    vdsaddr->sa_data,
			    vdsaddr->sa_len - saddr_offset);
			vdsaddr += 1;
		}

		DBGR("%s: AF_INET: if_name=[%s] addr=[%08x] "
		    "dst=[%08x] mask=[%08x]\n",
		    __func__, in_alreq->ifra_name,
		    in_alreq->ifra_addr.sin_addr.s_addr,
		    in_alreq->ifra_dstaddr.sin_addr.s_addr,
		    in_alreq->ifra_mask.sin_addr.s_addr);

		curvnet = ifp->if_vnet;
		if ((error = in_control(NULL, SIOCAIFADDR,
		    (caddr_t)in_alreq, ifp, td))) {
			ERRMSG(ctx, "%s: in_control() error = %d\n",
			    __func__, error);
			free(in_alreq, M_TEMP);
			curvnet = savevnet;
			goto out;
		}
		if (ifp->if_pspare[2] != NULL)
			((void (*) (u_long cmd, caddr_t data,
			    struct ifnet *ifp, struct thread *td))
			    ifp->if_pspare[2])
			    (SIOCAIFADDR, (caddr_t)in_alreq, ifp, td);
		free(in_alreq, M_TEMP);
		curvnet = savevnet;

		break;

	case AF_INET6:

		saddr_offset = offsetof(struct sockaddr_in6, sin6_port);

		in6_alreq = (struct in6_aliasreq *)
		    malloc(sizeof(*in6_alreq), M_TEMP, M_WAITOK | M_ZERO);
		memcpy(in6_alreq->ifra_name, ifp->if_xname,
		    sizeof(in6_alreq->ifra_name));

		DBGR("%s: AF_INET6 in6_aliasreq @ %p\n\tifra_name=[%s]\n",
		    __func__, in6_alreq, in6_alreq->ifra_name);

		if (vdifaddr->have_addr && vdsaddr->sa_len ==
		    sizeof(struct sockaddr_in6)) {
			in6_alreq->ifra_addr.sin6_family =
			    vdsaddr->sa_family;
			in6_alreq->ifra_addr.sin6_len =
			    vdsaddr->sa_len;
			memcpy(&in6_alreq->ifra_addr.sin6_port,
			    vdsaddr->sa_data,
			    vdsaddr->sa_len - saddr_offset);
			vdsaddr += 1;

			DBGR("\tifra_addr: %s\n",
			    ip6_sprintf(ip6buf,
			    &in6_alreq->ifra_addr.sin6_addr));
		}
		if (vdifaddr->have_dstaddr && vdsaddr->sa_len ==
		    sizeof(struct sockaddr_in6)) {
			in6_alreq->ifra_dstaddr.sin6_family =
			    vdsaddr->sa_family;
			in6_alreq->ifra_dstaddr.sin6_len =
			    vdsaddr->sa_len;
			memcpy(&in6_alreq->ifra_dstaddr.sin6_port,
			    vdsaddr->sa_data,
			    vdsaddr->sa_len - saddr_offset);
			vdsaddr += 1;

			DBGR("\tifra_dstaddr: %s\n",
			    ip6_sprintf(ip6buf,
			    &in6_alreq->ifra_dstaddr.sin6_addr));
		}
		if (vdifaddr->have_netmask && vdsaddr->sa_len ==
		    sizeof(struct sockaddr_in6)) {
			in6_alreq->ifra_prefixmask.sin6_family =
			    vdsaddr->sa_family;
			in6_alreq->ifra_prefixmask.sin6_len =
			    vdsaddr->sa_len;
			memcpy(&in6_alreq->ifra_prefixmask.sin6_port,
			    vdsaddr->sa_data,
			    vdsaddr->sa_len - saddr_offset);
			vdsaddr += 1;

			DBGR("\tifra_prefixmask: %s\n",
			    ip6_sprintf(ip6buf,
			    &in6_alreq->ifra_prefixmask.sin6_addr));
		}

		if (1) {
			vdia6lt = (struct vps_dump_vnet_inet6_lifetime *)
			    vdsaddr;
			in6_alreq->ifra_lifetime.ia6t_expire =
				vdia6lt->ia6t_expire;
			in6_alreq->ifra_lifetime.ia6t_preferred =
				vdia6lt->ia6t_preferred;
			in6_alreq->ifra_lifetime.ia6t_vltime =
				vdia6lt->ia6t_vltime;
			in6_alreq->ifra_lifetime.ia6t_pltime =
				vdia6lt->ia6t_pltime;

			DBGR("\tia6_lifetime: ...\n");
		}

		/* Skipping link-local */
		if (in6_alreq->ifra_addr.sin6_addr.s6_addr8[0] == 0xfe &&
		    in6_alreq->ifra_addr.sin6_addr.s6_addr8[1] == 0x80) {
			free(in6_alreq, M_TEMP);
			goto out;
		}

		curvnet = ifp->if_vnet;
		if ((error = in6_control(NULL, SIOCAIFADDR_IN6,
				(caddr_t)in6_alreq, ifp, td))) {
			ERRMSG(ctx, "%s: in6_control() error = %d\n",
			    __func__, error);
			free(in6_alreq, M_TEMP);
			curvnet = savevnet;
			goto out;
		}
		if (ifp->if_pspare[2] != NULL)
			((void (*) (u_long cmd, caddr_t data,
				struct ifnet *ifp, struct thread *td))
				ifp->if_pspare[2])(SIOCAIFADDR_IN6,
				(caddr_t)in6_alreq, ifp, td);
		free(in6_alreq, M_TEMP);
		curvnet = savevnet;

		break;

	default:
		ERRMSG(ctx, "%s: unhandled address family %d\n",
		    __func__, vdsaddr->sa_family);
		error = EINVAL;
		goto out;
	}

  out:
	return (error);
}

VPSFUNC
static int
vps_restore_vnet_iface(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vnet *vnet)
{
	struct vps_dumpobj *o1;
	struct vps_dump_vnet_ifnet *vdifnet;
	struct thread *td;
	struct vnet *savevnet;
	struct ifnet *nifnetp;
	char ifname[IFNAMSIZ];
	int if_dunit;
	int last_was_epair = 0;
	int error = 0;

	td = curthread;

	curvnet = TD_TO_VNET(td);
	savevnet = curvnet;
	DBGR("curvnet=%p savevnet=%p\n", curvnet, savevnet);

	nifnetp = NULL;

	while (vdo_typeofnext(ctx) == VPS_DUMPOBJT_VNET_IFACE) {

		o1 = vdo_next(ctx);

		vdifnet = (struct vps_dump_vnet_ifnet *)o1->data;
		DBGR("%s: vdifnet: if_xname=[%s] if_dname=[%s] "
		    "if_dunit=%d\n", __func__, vdifnet->if_xname,
		    vdifnet->if_dname, vdifnet->if_dunit);

		/*
		 * Skip "lo0" attach, because it is created on
		 * vps instance allocation.
		 */
		if (strcmp(vdifnet->if_xname, "lo0") == 0) {

			strcpy(ifname, "lo0");
			CURVNET_SET_QUIET(vnet);
			nifnetp = ifunit(ifname);
			CURVNET_RESTORE();

		} else {

			/* Restore interface. */

			/*
			 * XXX For non-cloned interfaces,
			 *     like hardware interfaces,
			 *     we need special treatment.
			 */

			if (last_was_epair == 1) {
				nifnetp = ((struct epair_softc *)
				    (nifnetp->if_softc))->oifp;
			} else {
				/* Cloned interfaces. */
				if_dunit = vdifnet->if_dunit;
				do {
					snprintf(ifname, IFNAMSIZ, "%s%d",
					    vdifnet->if_dname, if_dunit++);
					DBGR("%s: ifname=[%s]\n",
					    __func__, ifname);
					error = if_clone_create(ifname,
					    sizeof(ifname), NULL);

				} while (error == EEXIST);

				if (error) {
					ERRMSG(ctx, "%s: if_clone_create "
						"returned error = %d\n",
						__func__, error);
					goto out;
				}
				nifnetp = ifunit(ifname);
			}

			if (nifnetp == NULL) {
				ERRMSG(ctx, "%s: ifunit ([%s]) == NULL !\n",
				    __func__, ifname);
				error = EINVAL;
				goto out;
			}

			if (last_was_epair == 0) {
				/* Move interface into new vps instance. */
				if ((error = if_vmove_vps(td,
				    nifnetp->if_xname, 0, vps,
				    vdifnet->if_xname))) {
					ERRMSG(ctx, "%s: if_vps_vmove() "
					    "error = %d\n",
					    __func__, error);
					goto out;
				}
				if (strcmp(vdifnet->if_dname, "epair") == 0)
					last_was_epair = 1;
			} else
				last_was_epair = 0;
		}

		if (vdifnet->if_flags & IFF_UP) {
			curvnet = nifnetp->if_vnet;
			if_up(nifnetp);
			DBGR("%s: setting IFF_UP on [%s]\n",
			    __func__, nifnetp->if_xname);
			curvnet = savevnet;
		}

		while (vdo_typeofnext(ctx) == VPS_DUMPOBJT_VNET_ADDR) {

			error = vps_restore_iface_ifaddr(ctx, vps, nifnetp);
			if (error != 0)
				goto out;

			/* Next ifaddr. */
		}
	}

  out:
	return (error);
}

VPSFUNC
static int
vps_restore_vnet(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vnet **vnetp)
{
	struct vps_dump_vnet *vdvnet;
	struct vps_dumpobj *o1;
	struct vnet *nvnet;
	int error = 0;

	o1 = vdo_next(ctx);
	if (o1->type != VPS_DUMPOBJT_VNET) {
		DBGR("%s: o1=%p o1->type=%d\n",
		    __func__, o1, o1->type);
		error = EINVAL;
		goto out;
	}
	vdvnet = (struct vps_dump_vnet *)o1->data;
	DBGR("%s: orig_ptr=%p\n", __func__, vdvnet->orig_ptr);

	if (*vnetp != NULL) {
		nvnet = *vnetp;
		DBGR("%s: vnet=%p (existed)\n", __func__, nvnet);
	} else {
		nvnet = vnet_alloc();
		*vnetp = nvnet;
		DBGR("%s: vnet=%p (allocated)\n", __func__, nvnet);
	}

	if ((error = vps_restore_vnet_iface(ctx, vps, nvnet)))
		goto out;

	if ((error = vps_restore_vnet_route(ctx, vps, nvnet)))
		goto out;

  out:
	if (error)
		*vnetp = NULL;
	return (error);
}

VPSFUNC
static int
vps_restore_sysentvec(struct vps_snapst_ctx *ctx, struct vps *vps,
			struct proc *p)
{
	struct vps_dump_sysentvec *vds;
	struct vps_dumpobj *o1;
	struct sysentvec *sv;

	o1 = vdo_next(ctx);
	if (o1->type != VPS_DUMPOBJT_SYSENTVEC) {
		ERRMSG(ctx, "%s: wrong object type: %d\n",
		    __func__, o1->type);
		return (EINVAL);
	}
	vds = (struct vps_dump_sysentvec *)o1->data;

	if (vps_md_restore_sysentvec(vds->sv_type, &sv) != 0) {
		ERRMSG(ctx, "%s: unknown sysentvec type: %d\n",
		    __func__, vds->sv_type);
		return (EINVAL);
	}

	p->p_sysent = sv;

	return (0);
}

/* XXX */
int kqueue_register(struct kqueue *kq, struct kevent *kev,
    struct thread *td, int waitok);
int kqueue_acquire(struct file *fp, struct kqueue **kqp);
void kqueue_release(struct kqueue *kq, int locked);

VPSFUNC
static int
vps_restore_kqueue(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct proc *p)
{
	struct vps_dumpobj *o1, *o2;
	struct vps_dump_knote *vdkn;
	struct kevent *nkev;
	struct kqueue *kq;
	struct thread *td;
	struct file *fp;
	int error;
	int dfl;

	o1 = vdo_next(ctx);

	if (o1->type != VPS_DUMPOBJT_KQUEUE)
		return (EINVAL);

	DBGR("%s: \n", __func__);

	td = FIRST_THREAD_IN_PROC(p);

	if ((error = sys_kqueue(td, NULL))) {
		ERRMSG(ctx, "%s: sys_kqueue(): %d\n",
		    __func__, error);
		return (error);
	}
	fget(td, td->td_retval[0], 0, &fp);
	DBGR("%s: kqueue installed at fd %zd\n",
	    __func__, td->td_retval[0]);

	kq = NULL;
	if ((error = kqueue_acquire(fp, &kq)) != 0) {
		ERRMSG(ctx, "%s: kqueue_acquire(): error=%d\n",
		    __func__, error);
		goto out;
	}

	while (vdo_typeofnext(ctx) == VPS_DUMPOBJT_KNOTE) {

		o2 = vdo_next(ctx);

		vdkn = (struct vps_dump_knote *)o2->data;
		dfl = vdkn->kn_status;

		if (!vdkn->ke_filter)
			continue;

		nkev = malloc(sizeof(*nkev), M_TEMP, M_WAITOK);

		nkev->ident  = vdkn->ke_ident;
		nkev->filter = vdkn->ke_filter;
		nkev->flags  = vdkn->ke_flags;
		nkev->fflags = vdkn->ke_fflags;
		nkev->data   = vdkn->ke_data;
		nkev->udata  = vdkn->ke_udata;

		nkev->flags = EV_ADD;

		if (dfl & KN_ACTIVE)
			DBGR("KN_ACTIVE\n");
		if (dfl & KN_QUEUED)
			DBGR("KN_QUEUED\n");
		if (dfl & KN_DISABLED)
			nkev->flags |= EV_DISABLE;

		/* XXX ?! kevp->flags &= ~EV_SYSFLAGS; */

		DBGR("kevent: ident  = 0x%016zx\n", (size_t)nkev->ident);
		DBGR("kevent: filter = 0x%04hx\n", nkev->filter);
		DBGR("kevent: flags  = 0x%04hx\n", nkev->flags);
		DBGR("kevent: fflags = 0x%08x\n", nkev->fflags);
		DBGR("kevent: data   = 0x%016zx\n", (size_t)nkev->data);
		DBGR("kevent: udata  = 0x%016lx\n",
		    (long unsigned int)nkev->udata);

		error = kqueue_register(kq, nkev, td, 1);
		if (error) {
			ERRMSG(ctx, "%s: kqueue_register(): error=%d\n",
			    __func__, error);
			free(nkev, M_TEMP);
			goto out;
		}

		free(nkev, M_TEMP);

		/* XXX
		if (dfl & KN_ACTIVE)
			// set activate flag
		if (dfl & KN_QUEUED)
			//knote_enqueue()
		*/

	}

  out:
	if (kq != NULL)
		kqueue_release(kq, 0);
	fdrop(fp, td);

	return (error);
}

/*
 * This section is a little bit tricky because we have to deal
 * with pairs of pipes that have one end closed.
 */
VPSFUNC
static int
vps_restore_pipe(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct proc *p)
{
	struct vps_dumpobj *o1;
	struct vps_dump_pipe *vdp;
	struct vps_restore_obj *ro;
	struct file *fp;
	struct pipepair *npp;
	int filedes[2];
	int error = 0;

	o1 = vdo_next(ctx);

	if (o1->type != VPS_DUMPOBJT_PIPE)
		return (EINVAL);

	vdp = (struct vps_dump_pipe *)o1->data;

	DBGR("%s: vdp: pi_have_dumped_pipe=%d pi_localend=%p pi_pair=%p "
	    "pi_rpipe=%p pi_wpipe=%p\n",
	    __func__, vdp->pi_have_dumped_pipe, vdp->pi_localend,
	    vdp->pi_pair, vdp->pi_rpipe, vdp->pi_wpipe);

	if (vdp->pi_have_dumped_pipe != 0) {

		if ((error = kern_pipe(curthread, filedes))) {
			ERRMSG(ctx, "%s: kern_pipe() error: %d\n",
				__func__, error);
			goto out;
		}

		/*
		 * filedes[0] is the read endpoint,
		 * filedes[1] the write endpoint.
		 *
		 * We only keep the endpoint connected to the file
		 * descriptor to be restored and close the other endpoint.
		 *
		 * If the currently restored process has a reference to
		 * the second endpoint, it will be connected again later
		 * in another run.
		 */

		fget(curthread, filedes[0], 0, &fp);
		npp = (struct pipepair *)
		    ((struct pipe *)fp->f_data)->pipe_pair;
		fdrop(fp, curthread);

		/* Insert into restored objects list. */
		ro = malloc(sizeof(*ro), M_VPS_RESTORE, M_WAITOK | M_ZERO);
		ro->type = VPS_DUMPOBJT_PIPE;
		ro->orig_ptr = vdp->pi_pair;
		ro->new_ptr = npp;
		/* These references have to be released later. */
		fget(curthread, filedes[0], 0, &fp);
		ro->spare[0] = fp;
		fget(curthread, filedes[1], 0, &fp);
		ro->spare[1] = fp;
		SLIST_INSERT_HEAD(&ctx->obj_list, ro, list);

		if (vdp->pi_localend == vdp->pi_rpipe) {

			/* We want the read endpoint. */
			fget(curthread, filedes[0], 0, &fp);
			fp->f_data = &npp->pp_rpipe;
			fdrop(fp, curthread);

			/* Close the write endpoint. */
			fget(curthread, filedes[1], 0, &fp);
			fdclose(curthread->td_proc->p_fd, fp,
			    filedes[1], curthread);
			fdrop(fp, curthread);

			curthread->td_retval[0] = filedes[0];

		} else if (vdp->pi_localend == vdp->pi_wpipe) {

			/* We want the write endpoint. */
			fget(curthread, filedes[1], 0, &fp);
			fp->f_data = &npp->pp_wpipe;
			fdrop(fp, curthread);

			/* Close the read endpoint. */
			fget(curthread, filedes[0], 0, &fp);
			fdclose(curthread->td_proc->p_fd, fp,
			    filedes[0], curthread);
			fdrop(fp, curthread);

			curthread->td_retval[0] = filedes[1];

		} else {
			ERRMSG(ctx, "%s: vdp->pi_localend != vdp->pi_rpipe "
			    "&& vdp->pi_localend != vdp->pi_wpipe\n",
			    __func__);
			/* XXX Clean up. */
			error = EINVAL;
			goto out;
		}

	} else {

		fp = NULL;
		npp = NULL;
		SLIST_FOREACH(ro, &ctx->obj_list, list)
			if (ro->type == VPS_DUMPOBJT_PIPE &&
			    ro->orig_ptr == vdp->pi_pair)
				break;

		if (ro == NULL) {
			ERRMSG(ctx, "%s: pipe pair (old_ptr %p) "
				"which should be there was not found !\n",
				__func__, vdp->pi_pair);
			error = EINVAL;
			goto out;
		}

		npp = ro->new_ptr;

		if (vdp->pi_localend == vdp->pi_rpipe) {

			/* We want the read endpoint. */
			fp = (struct file *)ro->spare[0];

		} else if (vdp->pi_localend == vdp->pi_wpipe) {

			/* We want the write endpoint. */
			fp = (struct file *)ro->spare[1];

		} else {
			ERRMSG(ctx, "%s: vdp->pi_localend != vdp->pi_rpipe "
			    "&& vdp->pi_localend != vdp->pi_wpipe\n",
			    __func__);
			/* XXX Clean up. */
			error = EINVAL;
			goto out;
		}

		fhold(fp);

		FILEDESC_XLOCK (curthread->td_proc->p_fd);
		if ((error = fdalloc(curthread, 0, &filedes[0]))) {
			FILEDESC_XUNLOCK(curthread->td_proc->p_fd);
			ERRMSG(ctx, "%s: fdalloc() error: %d\n",
				__func__, error);
			goto out;
		}
		curthread->td_proc->p_fd->fd_ofiles[filedes[0]].fde_file
		    = fp;
		FILEDESC_XUNLOCK(curthread->td_proc->p_fd);

		curthread->td_retval[0] = filedes[0];
	}

  out:
	return (error);
}

VPSFUNC
static void
vps_restore_cleanup_pipe(struct vps_snapst_ctx *ctx, struct vps *vps,
	struct _vps_restore_obj_list *obj_list)
{
	struct vps_restore_obj *obj, *obj2;

	SLIST_FOREACH_SAFE(obj, obj_list, list, obj2) {
		if (obj->type != VPS_DUMPOBJT_PIPE)
			continue;
		if (obj->spare[0])
			fdrop((struct file *)obj->spare[0], curthread);
		if (obj->spare[1])
			fdrop((struct file *)obj->spare[1], curthread);
		/* Let the generic cleanup unlink and free the list item. */
	}
}

VPSFUNC
static void
vps_restore_cleanup_ucred(struct vps_snapst_ctx *ctx, struct vps *vps,
	struct _vps_restore_obj_list *obj_list)
{
	struct vps_restore_obj *obj, *obj2;

	SLIST_FOREACH_SAFE(obj, obj_list, list, obj2) {
		if (obj->type != VPS_DUMPOBJT_UCRED)
			continue;
		crfree(obj->new_ptr);
		/* Let the generic cleanup unlink and free the list item. */
	}
}

/*
 * XXX
 *
 * All the socket snapshot and restore code is still incomplete.
 * Most socket options are not supported,
 */

VPSFUNC
__attribute__((unused))
static void
vps_sbcheck(struct sockbuf *sb)
{
        struct mbuf *m;
        struct mbuf *n = 0;
        u_long len = 0, mbcnt = 0;

        SOCKBUF_LOCK_ASSERT(sb);

        for (m = sb->sb_mb; m; m = n) {
            n = m->m_nextpkt;
        	for (; m; m = m->m_next) {
			len += m->m_len;
			mbcnt += MSIZE;
			if (m->m_flags & M_EXT)
				/*XXX*/ /* pretty sure this is bogus */
				mbcnt += m->m_ext.ext_size;
		}
        }
        if (len != sb->sb_cc || mbcnt != sb->sb_mbcnt) {
		/*
                DBGR("cc %ld != %u || mbcnt %ld != %u\n", len, sb->sb_cc,
                    mbcnt, sb->sb_mbcnt);
                panic("sbcheck");
		*/
		/* debugging */
		printf("cc %ld != %u || mbcnt %ld != %u\n",
			len, sb->sb_cc, mbcnt, sb->sb_mbcnt);
		kdb_enter(KDB_WHY_BREAK, "VPS break to debugger");
        }
}

/*
 * Pointers m1, m2, m3, ... contain mbuf pointers of the system
 * where the snapshot was taken.
 * We compare them to the mbufs we restore and replace them with the new
 * pointer (XXX: , or NULL if not available).
 *
 * Since there *shouldn't* be any activity on the socket,
 * it *should* be safe to sleep for allocating mbufs.
 */
VPSFUNC
static int
vps_restore_mbufchain(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct mbuf **mptrs)
{
	struct vps_dump_mbufchain *vdmc;
	struct vps_dump_mbuf *vdmb;
	struct vps_dumpobj *o1;
	struct mbuf *nm, *lnm, *nm0;
	struct mbuf **mptrs2;
	int i, j;

	/* caller verified type */
	o1 = vdo_next(ctx);

	vdmc = (struct vps_dump_mbufchain *)o1->data;

	mptrs2 = malloc(sizeof(struct mbuf *) * vdmc->mc_mbcount,
	    M_TEMP, M_WAITOK);

	lnm = nm0 = NULL;
	vdmb = (struct vps_dump_mbuf *)(vdmc + 1);

	for (i = 0; i < vdmc->mc_mbcount; i++) {

		nm = m_get(M_WAITOK, vdmb->mb_type);
		nm->m_len = vdmb->mb_len;

		if (i == 0)
			nm0 = nm;

		mptrs2[i] = vdmb->mb_orig_ptr;

		if (vdmb->mb_have_dat==1) {

			memcpy(nm->m_dat, vdmb->mb_payload,
			    vdmb->mb_payload_size);
			nm->m_flags = vdmb->mb_flags;
			if (vdmb->mb_have_data==1)
				nm->m_data = nm->m_dat + vdmb->mb_data_off;

			/*
			vps_print_ascii(nm->m_dat, nm->m_len);
			*/

		} else if (vdmb->mb_have_ext==1) {

			m_cljget(nm, M_WAITOK, vdmb->mb_payload_size);
			memcpy(nm->m_ext.ext_buf, vdmb->mb_payload,
			    vdmb->mb_payload_size);
			nm->m_flags = vdmb->mb_flags;
			if (vdmb->mb_have_data==1)
				nm->m_data = nm->m_ext.ext_buf +
				    vdmb->mb_data_off;

			DBGR("%s: M_EXT m_ext.ext_size=%u "
			    "mb_payload_size=%u\n", __func__,
			    nm->m_ext.ext_size, vdmb->mb_payload_size);

			/*
			vps_print_ascii(nm->m_ext.ext_buf,
			    nm->m_ext.ext_size);
			*/

			/* checksum */
			if (1) {
				int sum = 0, i;

				for (i = 0; i < nm->m_ext.ext_size; i++)
					sum += (u_char)nm->m_ext.ext_buf[i];
				DBGR("%s: computed checksum=%08x, "
				    "original checksum=%08x\n",
				    __func__, sum, vdmb->mb_checksum);
				if (sum != vdmb->mb_checksum) {
					ERRMSG(ctx, "%s: checksum "
					    "failure !\n", __func__);
					return (-1);
				}
			}

		} else {
			ERRMSG(ctx, "%s: DON'T KNOW HOW TO HANDLE MBUF\n",
			    __func__);
			DBGR("%s: vdmb->mb_have_dat=%d "
			    "vdmb->mb_have_ext=%d\n",
			    __func__, vdmb->mb_have_dat,
			    vdmb->mb_have_ext);
			return (-1);
		}

		if (nm->m_flags & M_PKTHDR) {
			DBGR("%s: nm=%p M_PKTHDR\n", __func__, nm);
			nm->m_pkthdr.rcvif = NULL;
			nm->m_pkthdr.header = NULL;
			/* XXX
			if (dm->m_pkthdr.header != NULL)
				nm->m_pkthdr.header = (caddr_t)nm +
					((caddr_t)dm->m_pkthdr.header -
					(caddr_t)vdm->morigptr[i]);
			*/
			/*
			DBGR("%s: nm=%p dm_orig=%p header=%p rcvif=%p\n",
				__func__, nm, vdm->morigptr[i],
				dm->m_pkthdr.header, dm->m_pkthdr.rcvif);
			*/
			/* XXX */
			SLIST_INIT(&nm->m_pkthdr.tags);
		}

		if (lnm)
			lnm->m_next = nm;

		lnm = nm;

		DBGR("%s: nm=%p type=%d flags=%08x len=%d next=%p "
		    "nextpkt=%p\n", __func__, nm, nm->m_type,
		    nm->m_flags, nm->m_len, nm->m_next, nm->m_nextpkt);

		for (j = 0; mptrs[j]; j++)
			if (mptrs[j] == mptrs2[i]) {
				mptrs[j] = nm;
				DBGR("%s: replaced %p --> %p, i=%d j=%d\n",
				    __func__, mptrs2[i], nm, i, j);
				/* No 'break' because we may have the
				   same pointer twice. */
			}

		/* Next. */
		DBGR("%s: vdmb=%p vdmb->mb_payload=%p "
		    "vdmb->mb_payload_size=%u\n",
		    __func__, vdmb, vdmb->mb_payload,
		    vdmb->mb_payload_size);
		/* Always padded to meet 64 bit alignment ! */
		vdmb = (struct vps_dump_mbuf *)(vdmb->mb_payload +
			roundup(vdmb->mb_payload_size, 8));
	}

	DBGR("%s: restored %d mbufs\n", __func__, i);

	free(mptrs2, M_TEMP);

	if (nm0 && m_sanity(nm0, 1) != 1) {
		DBGR("%s: m_sanity: BAD mbuf(chain)\n", __func__);
	}

	return (i);
}

VPSFUNC
static int
vps_restore_sockbuf(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct sockbuf *nsb)
{
	struct vps_dumpobj *o1;
	struct vps_dump_sockbuf *vdsb;
	struct mbuf *mptrs[5];
	int rc;
	int error = 0;

	o1 = vdo_next(ctx);

	if (o1->type != VPS_DUMPOBJT_SOCKBUF)
		return (-1);

	vdsb = (struct vps_dump_sockbuf *)o1->data;

	mptrs[0] = vdsb->sb_mb;
	mptrs[1] = vdsb->sb_mbtail;
	mptrs[2] = vdsb->sb_lastrecord;
	mptrs[3] = vdsb->sb_sndptr;
	mptrs[4] = NULL;

	DBGR("%s: mptrs[]: 0=%p 1=%p 2=%p 3=%p\n",
	    __func__, mptrs[0], mptrs[1], mptrs[2], mptrs[3]);

	if (vdo_typeofnext(ctx) == VPS_DUMPOBJT_MBUFCHAIN)
		rc = vps_restore_mbufchain(ctx, vps, mptrs);
	else
		rc = 0;

	if (rc != vdsb->sb_mcnt) {
		ERRMSG(ctx, "%s: vps_restore_mbufchain()=%d != "
		    "dsb->sb_mcnt=%d\n", __func__, rc, vdsb->sb_mcnt);
		return (EINVAL);
	}

	DBGR("%s: mptrs[]: 0=%p 1=%p 2=%p 3=%p\n",
	    __func__, mptrs[0], mptrs[1], mptrs[2], mptrs[3]);

	nsb->sb_mb =			mptrs[0];
	nsb->sb_mbtail =		mptrs[1];
	nsb->sb_lastrecord =	 	mptrs[2];
	nsb->sb_sndptr = 		mptrs[3];
	nsb->sb_state = 		vdsb->sb_state;
	nsb->sb_sndptroff = 		vdsb->sb_sndptroff;
	nsb->sb_cc = 			vdsb->sb_cc;
	nsb->sb_hiwat = 		vdsb->sb_hiwat;
	nsb->sb_mbcnt = 		vdsb->sb_mbcnt;
	nsb->sb_mcnt = 			vdsb->sb_mcnt;
	nsb->sb_ccnt = 			vdsb->sb_ccnt;
	nsb->sb_mbmax = 		vdsb->sb_mbmax;
	nsb->sb_ctl = 			vdsb->sb_ctl;
	nsb->sb_lowat = 		vdsb->sb_lowat;
	nsb->sb_hiwat = 		vdsb->sb_hiwat;
	nsb->sb_timeo = 		vdsb->sb_timeo;
	nsb->sb_flags = 		vdsb->sb_flags;
	/* XXX
	nsb->sb_upcall = 		vdsb->sb_upcall;
	nsb->sb_upcallarg = 		vdsb->sb_upcallarg;
	*/
	nsb->sb_upcall =		NULL;
	nsb->sb_upcallarg =		NULL;

	DBGR("%s: restored sockbuf=%p sb_cc=%u sb_mcnt=%u "
	    "sb_sndptroff=%u\n", __func__, nsb, nsb->sb_cc,
	    nsb->sb_mcnt, nsb->sb_sndptroff);

	vps_sbcheck(nsb);

	return (error);
}

VPSFUNC
static int
vps_restore_fixup_unixsockets(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct vps_restore_obj *obj1, *obj2;
	struct socket *srvso_old, *srvso_new, *cltso;
	struct unpcb *cltunp, *srvunp;
	int error = 0;

	SLIST_FOREACH(obj1, &ctx->obj_list, list) {
		if (obj1->type != VPS_DUMPOBJT_SOCKET_UNIX)
			continue;

		if (obj1->spare[0] != (void*)'c')
			/* not client socket */
			continue;

		cltso = (struct socket *)obj1->new_ptr;
		srvso_old = (struct socket *)obj1->spare[1];
		srvso_new = NULL;

		SLIST_FOREACH(obj2, &ctx->obj_list, list) {
			if (obj2->type != VPS_DUMPOBJT_SOCKET_UNIX)
				continue;
			if (obj2->orig_ptr == (void*)srvso_old) {
				srvso_new = obj2->new_ptr;
				break;
			}
		}

		if (srvso_new == NULL) {
			ERRMSG(ctx, "%s: srvso_new == NULL for "
			    "srvso_old=%p\n", __func__, srvso_old);
			error = EINVAL;
			break;
		}

		cltunp = sotounpcb((struct socket *)obj1->new_ptr);
		srvunp = sotounpcb((struct socket *)srvso_new);

		cltunp->unp_conn = srvunp;
		cltso->so_state |= SS_ISCONNECTED;

		if (cltso->so_type == SOCK_DGRAM) {
			LIST_INSERT_HEAD(&srvunp->unp_refs, cltunp,
			    unp_reflink);
		} else {
			srvunp->unp_conn = cltunp;
			srvso_new->so_state |= SS_ISCONNECTED;
		}

		DBGR("%s: cltso=%p srvso_new=%p\n",
		    __func__, cltso, srvso_new);

		/* Let the generic cleanup unlink and free the list item. */
	}

	return (error);
}

VPSFUNC
static int
vps_restore_socket(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct proc *p)
{
	struct vps_dumpobj *o1;
	struct vps_dump_socket *vds;
	struct vps_dump_inetpcb *vdinpcb;
	struct vps_dump_unixpcb *vdunpcb;
	struct vps_dump_udppcb *vdudpcb;
	struct vps_dump_tcppcb *vdtcpcb;
	struct vps_dump_vnet_sockaddr *vdsaddr;
	struct vps_restore_obj *ro;
	struct socket *nso, *nso2;
	struct inpcb *ninpcb;
	struct tcpcb *ntcpcb;
	struct unpcb *nunpcb;
	struct udpcb *nudpcb;
	struct sockaddr_un *saddr_un;
	struct socket_args sockargs; /* 3 * int */
	struct ucred *save_ucred, *ncr;
	struct filedesc *cfd;
	struct stat *statp;
	struct file *fp;
	caddr_t cpos;
	int fdidx, fdidx_save;
	int error;
	int i;

	nso = NULL;
	fdidx = -1;

	o1 = vdo_next(ctx);

	if (o1->type != VPS_DUMPOBJT_SOCKET)
		return (-1);

	/* XXX prison with own vnet ! */
	CURVNET_SET_QUIET(vps->vnet);
	/* We have to temporarily change our ucred too. */
	save_ucred = curthread->td_ucred;
	ncr = NULL;

	cpos = o1->data;
	vds = (struct vps_dump_socket *)cpos;
	cpos += sizeof(*vds);

	if (vdo_typeofnext(ctx) == VPS_DUMPOBJT_UCRED) {
		vdo_next(ctx);
		/* XXX don't put child objects in the middle of data ! */
		cpos = ctx->cpos;
	}
	ncr = vps_restore_ucred_lookup(ctx, vps, vds->so_cred);
	curthread->td_ucred = ncr;

	DBGR("%s: family=%d protocol=%d type=%d\n",
	    __func__, vds->so_family, vds->so_protocol, vds->so_type);

	sockargs.domain = vds->so_family;
	sockargs.type = vds->so_type;
	sockargs.protocol = vds->so_protocol;

	if ((error = sys_socket(curthread, &sockargs))) {
		ERRMSG(ctx, "%s: sys_socket() error: %d\n",
			__func__, error);
		goto out;
	}
	fdidx = curthread->td_retval[0];
	if ((error = getsock(curthread->td_proc->p_fd, fdidx, &fp, NULL))) {
		ERRMSG(ctx, "%s: getsock() error: %d\n",
			__func__, error);
		goto out;
	}
	nso = fp->f_data;
	sblock(&nso->so_rcv, SBL_WAIT | SBL_NOINTR);
	sblock(&nso->so_snd, SBL_WAIT | SBL_NOINTR);
	SOCKBUF_LOCK(&nso->so_rcv);
	SOCKBUF_LOCK(&nso->so_snd);
	nso->so_vnet = curvnet;
	nso->so_state = vds->so_state;
	if (vds->so_options & SO_ACCEPTCONN)
		nso->so_options |= SO_ACCEPTCONN;
	nso->so_qlimit = vds->so_qlimit;
	nso->so_qstate = vds->so_qstate;

	/* XXX restore all socket options at all levels ! */

	DBGR("%s: nso=%p dso->so_state = %08x\n",
	    __func__, nso, vds->so_state);
	DBGR("%s: nso->so_cred=%p nso->so_cred->cr_vps=%p "
	    "nso->so_vnet=%p\n", __func__, nso->so_cred,
	    nso->so_cred->cr_vps, nso->so_vnet);

	switch (vds->so_family) {
	case PF_UNIX:

		SOCKBUF_UNLOCK(&nso->so_snd);
		SOCKBUF_UNLOCK(&nso->so_rcv);

		vdunpcb = (struct vps_dump_unixpcb *)cpos;
		cpos += sizeof(*vdunpcb);

		nunpcb = (struct unpcb *)nso->so_pcb;

		if (vdunpcb->unp_have_addr) {
			vdsaddr = (struct vps_dump_vnet_sockaddr *)cpos;
			cpos += sizeof(*vdsaddr);

			saddr_un = malloc(sizeof(struct sockaddr_un),
			    M_TEMP, M_WAITOK | M_ZERO);
			saddr_un->sun_len = vdsaddr->sa_len;
			saddr_un->sun_family = vdsaddr->sa_family;
			memcpy(saddr_un->sun_path, vdsaddr->sa_data,
			    vdsaddr->sa_len);
			/* Make sure sun_path is null terminated. */
			saddr_un->sun_path[saddr_un->sun_len -
			    offsetof(struct sockaddr_un, sun_path[0])] =
			    '\0';
		} else
			saddr_un = NULL;

		if (vdunpcb->unp_have_conn==0 &&
		    vdunpcb->unp_have_vnode==1 &&
		    vdunpcb->unp_have_addr==1) {

			/* Socket was bound. */
			DBGR("%s: socket was bound: [%s]\n",
			    __func__, saddr_un->sun_path);

			/* Remove name from filesystem first. */
			statp = malloc(sizeof(*statp), M_TEMP, M_WAITOK);
			error = kern_stat(curthread, saddr_un->sun_path,
			    UIO_SYSSPACE, statp);
			if (error) {
				ERRMSG(ctx, "%s: kern_stat() error: "
				    "%d [%s]\n", __func__, error,
				    saddr_un->sun_path);
				free(statp, M_TEMP);
				free(saddr_un, M_TEMP);
				SOCKBUF_LOCK(&nso->so_rcv);
				SOCKBUF_LOCK(&nso->so_snd);
				goto out_unlock;
			}
			kern_unlink(curthread, saddr_un->sun_path,
			    UIO_SYSSPACE);

			error = sobind(nso, (struct sockaddr *)saddr_un,
			    curthread);
			if (error) {
				ERRMSG(ctx, "%s: sobind() error: %d [%s]\n",
				    __func__, error, saddr_un->sun_path);
				free(statp, M_TEMP);
				free(saddr_un, M_TEMP);
				SOCKBUF_LOCK(&nso->so_rcv);
				SOCKBUF_LOCK(&nso->so_snd);
				goto out_unlock;
			}

			/* Restore permissions. */
			error = kern_chown(curthread, saddr_un->sun_path,
			    UIO_SYSSPACE, statp->st_uid, statp->st_gid);
			if (error) {
				ERRMSG(ctx, "%s: kern_chown() error: "
				    "%d [%s]\n", __func__, error,
				    saddr_un->sun_path);
				free(statp, M_TEMP);
				free(saddr_un, M_TEMP);
				SOCKBUF_LOCK(&nso->so_rcv);
				SOCKBUF_LOCK(&nso->so_snd);
				goto out_unlock;
			}
			error = kern_chmod(curthread, saddr_un->sun_path,
			    UIO_SYSSPACE, statp->st_mode);
			if (error) {
				ERRMSG(ctx, "%s: kern_chmod() error: "
				    "%d [%s]\n", __func__, error,
				    saddr_un->sun_path);
				free(statp, M_TEMP);
				free(saddr_un, M_TEMP);
				SOCKBUF_LOCK(&nso->so_rcv);
				SOCKBUF_LOCK(&nso->so_snd);
				goto out_unlock;
			}
			free(statp, M_TEMP);

			if (vdunpcb->unp_flags & UNP_HAVEPCCACHED) {
				/* Socket was listening. */
				DBGR("%s: socket was listening\n",
				    __func__);
				error = solisten(nso, vds->so_qlimit,
				    curthread);
				if (error) {
					ERRMSG(ctx, "%s: solisten() error: "
					    "%d [%s]\n", __func__, error,
					    saddr_un->sun_path);
					free(saddr_un, M_TEMP);
					SOCKBUF_LOCK(&nso->so_rcv);
					SOCKBUF_LOCK(&nso->so_snd);
					goto out_unlock;
				}
			}

			/* insert into global list of unix sockets */
			ro = malloc(sizeof(*ro), M_VPS_RESTORE,
			    M_WAITOK | M_ZERO);
			ro->type = VPS_DUMPOBJT_SOCKET_UNIX;
			ro->orig_ptr = vdunpcb->unp_socket;
			ro->new_ptr = nso;
			ro->spare[0] = (void*)'s';
			ro->spare[1] = NULL;

			SLIST_INSERT_HEAD(&ctx->obj_list, ro, list);

		/*
		 * Both client and server sockets are put into a global list
		 * that is walked through at the end of vps restore.
		 * All references can be restored then.
		 */
		} else if (vdunpcb->unp_have_conn==1 &&
		    vdunpcb->unp_have_addr==1 &&
		    (vdunpcb->unp_flags & UNP_HAVEPC)) {
			DBGR("%s: connected; server\n", __func__);
			/* insert into global list of unix sockets */
			ro = malloc(sizeof(*ro), M_VPS_RESTORE,
			    M_WAITOK | M_ZERO);
			ro->type = VPS_DUMPOBJT_SOCKET_UNIX;
			ro->orig_ptr = vdunpcb->unp_socket;
			ro->new_ptr = nso;
			ro->spare[0] = (void*)'s';
			ro->spare[1] = NULL;

			SLIST_INSERT_HEAD(&ctx->obj_list, ro, list);

			DBGR("%s: server socket: orig_ptr=%p new_ptr=%p "
			    "unp_socket=%p\n", __func__, ro->orig_ptr,
			    ro->new_ptr, vdunpcb->unp_socket);

		} else if (vdunpcb->unp_have_conn) {
			DBGR("%s: connected; client\n", __func__);
			/* insert into global list of unix sockets */
			ro = malloc(sizeof(*ro), M_VPS_RESTORE,
			    M_WAITOK | M_ZERO);
			ro->type = VPS_DUMPOBJT_SOCKET_UNIX;
			ro->orig_ptr = vdunpcb->unp_socket;
			ro->new_ptr = nso;
			ro->spare[0] = (void*)'c';
			ro->spare[1] = (void*)vdunpcb->unp_conn_socket;

			SLIST_INSERT_HEAD(&ctx->obj_list, ro, list);

			DBGR("%s: client socket: orig_ptr=%p new_ptr=%p "
			    "unp_conn_socket=%p\n", __func__, ro->orig_ptr,
			    ro->new_ptr, vdunpcb->unp_conn_socket);

		} else {
			DBGR("%s: unknown socket state\n", __func__);
		}

		if (vdunpcb->unp_flags & UNP_HAVEPCCACHED) {
			nunpcb->unp_flags |= UNP_HAVEPCCACHED;
			nunpcb->unp_peercred.cr_uid =
			    vdunpcb->unp_peercred.cr_uid;
			nunpcb->unp_peercred.cr_ngroups =
			    vdunpcb->unp_peercred.cr_ngroups;
			for (i = 0; i < nunpcb->unp_peercred.cr_ngroups;
			    i++)
				nunpcb->unp_peercred.cr_groups[i] =
					vdunpcb->unp_peercred.cr_groups[i];
		}

		/* XXX not used at all ? */
		if (vdunpcb->unp_have_vnode==1 && vdo_typeofnext(ctx) ==
				VPS_DUMPOBJT_FILE_PATH) {
			DBGR("%s: unused vnode object !\n", __func__);
			vdo_next(ctx);
		}

		DBGR("%s: nso=%p so_state=%d unpcb->unp_conn=%p\n",
			__func__, nso, nso->so_state, nunpcb->unp_conn);

		free(saddr_un, M_TEMP);
		SOCKBUF_LOCK(&nso->so_rcv);
		SOCKBUF_LOCK(&nso->so_snd);
		break;

	case PF_INET:
	case PF_INET6:
		vdinpcb = (struct vps_dump_inetpcb *)cpos;
		cpos += sizeof(*vdinpcb);

		ninpcb = (struct inpcb *)nso->so_pcb;

		INP_INFO_WLOCK(ninpcb->inp_pcbinfo);
		INP_HASH_WLOCK(ninpcb->inp_pcbinfo);
		INP_WLOCK(ninpcb);

		ninpcb->inp_vnet = curvnet;

		/* Connection info (endpoints). */

		ninpcb->inp_inc.inc_flags = vdinpcb->inp_inc.inc_flags;
		ninpcb->inp_inc.inc_len = vdinpcb->inp_inc.inc_len;
		ninpcb->inp_inc.inc_fibnum = vdinpcb->inp_inc.inc_fibnum;
		ninpcb->inp_inc.inc_ie.ie_fport= vdinpcb->inp_inc.ie_fport;
		ninpcb->inp_inc.inc_ie.ie_lport= vdinpcb->inp_inc.ie_lport;
		if (vdinpcb->inp_vflag & INP_IPV6) {
			memcpy(&ninpcb->inp_inc.inc6_faddr,
			    vdinpcb->inp_inc.ie_ufaddr, 0x10);
			memcpy(&ninpcb->inp_inc.inc6_laddr,
			    vdinpcb->inp_inc.ie_uladdr, 0x10);
		} else {
			memcpy(&ninpcb->inp_inc.inc_faddr,
			    vdinpcb->inp_inc.ie_ufaddr, 0x4);
			memcpy(&ninpcb->inp_inc.inc_laddr,
			    vdinpcb->inp_inc.ie_uladdr, 0x4);
		}

		/*
		DBGR("%s: inc_flags=%08x inc_len=%d inc_fibnum=%d\n",
		    __func__,
		    vdinpcb->inp_inc.inc_flags,
		    vdinpcb->inp_inc.inc_len,
		    vdinpcb->inp_inc.inc_fibnum);
		DBGR("%s: inc_flags=%08x inc_len=%d inc_fibnum=%d\n",
		    __func__,
		    ninpcb->inp_inc.inc_flags,
		    ninpcb->inp_inc.inc_len,
		    ninpcb->inp_inc.inc_fibnum);
		DBGR("%s: inp_inc.ie_fport=%u inp_inc.ie_lport=%u\n",
		    __func__,
		    vdinpcb->inp_inc.ie_fport,
		    vdinpcb->inp_inc.ie_lport);
		DBGR("%s: inp_inc.ie_fport=%u inp_inc.ie_lport=%u\n",
		    __func__,
		    ninpcb->inp_inc.inc_ie.ie_fport,
		    ninpcb->inp_inc.inc_ie.ie_lport);
		DBGR("%s: inp_inc.inc6_faddr: %16D\n",
		    __func__, vdinpcb->inp_inc.ie_ufaddr, ":");
		DBGR("%s: inp_inc.inc6_laddr: %16D\n",
		    __func__, vdinpcb->inp_inc.ie_uladdr, ":");
		DBGR("%s: inp_inc.inc6_faddr: %16D\n",
		    __func__, &ninpcb->inp_inc.inc6_faddr, ":");
		DBGR("%s: inp_inc.inc6_laddr: %16D\n",
		    __func__, &ninpcb->inp_inc.inc6_laddr, ":");
		*/

		ninpcb->inp_vflag = vdinpcb->inp_vflag;
		ninpcb->inp_flags = vdinpcb->inp_flags;
		ninpcb->inp_flags2 = vdinpcb->inp_flags2;
		/* in_pcbinshash() has NOT been called */
		ninpcb->inp_flags &= ~INP_INHASHLIST;

		in_pcbinshash(ninpcb);

		DBGR("%s: ninpcb=%p inp_vflag=%02hhx inp_flags=%08x "
		    "inp_flags2=%08x\n",
		    __func__, ninpcb, ninpcb->inp_vflag,
		    ninpcb->inp_flags, ninpcb->inp_flags2);

		/* XXX IP options. */

		if (vdinpcb->inp_have_ppcb != 0) {

			KASSERT(ninpcb->inp_ppcb != NULL,
				("%s: ninpcb->inp_ppcb == NULL", __func__));
			if (ninpcb->inp_ppcb == NULL) {
				ERRMSG(ctx, "%s: ninpcb->inp_ppcb == "
				    "NULL\n", __func__);
				error = EINVAL;
				goto out_unlock;
			}

			/* inpcb->inp_ip_p seems to be 0 and only used for
			   raw ip and divert sockets! */

			switch (vds->so_protocol) {

			case IPPROTO_TCP:
				vdtcpcb = (struct vps_dump_tcppcb *)cpos;
				ntcpcb = (struct tcpcb *)ninpcb->inp_ppcb;

				INP_INFO_WLOCK(&V_tcbinfo);
				ntcpcb->t_state = vdtcpcb->t_state;
				ntcpcb->snd_una = vdtcpcb->snd_una;
				ntcpcb->snd_max = vdtcpcb->snd_max;
				ntcpcb->snd_nxt = vdtcpcb->snd_nxt;
				ntcpcb->snd_up = vdtcpcb->snd_up;
				ntcpcb->snd_wl1 = vdtcpcb->snd_wl1;
				ntcpcb->snd_wl2 = vdtcpcb->snd_wl2;
				ntcpcb->iss = vdtcpcb->iss;
				ntcpcb->irs = vdtcpcb->irs;
				ntcpcb->rcv_nxt = vdtcpcb->rcv_nxt;
				ntcpcb->rcv_adv = vdtcpcb->rcv_adv;
				ntcpcb->rcv_wnd = vdtcpcb->rcv_wnd;
				ntcpcb->rcv_up = vdtcpcb->rcv_up;
				ntcpcb->snd_wnd = vdtcpcb->snd_wnd;
				ntcpcb->snd_cwnd = vdtcpcb->snd_cwnd;
				ntcpcb->snd_ssthresh =
				    vdtcpcb->snd_ssthresh;
				INP_INFO_WUNLOCK(&V_tcbinfo);

				break;

			case IPPROTO_UDP:
				vdudpcb = (struct vps_dump_udppcb *)cpos;
				nudpcb = (struct udpcb *)ninpcb->inp_ppcb;

				if (vdudpcb->u_have_tun_func != 0) {
					ERRMSG(ctx, "%s: ucb->u_tun_func "
					    "!= NULL, unsupported\n",
					    __func__);
					error = EINVAL;
					goto out_unlock;
				}
				nudpcb->u_flags = vdudpcb->u_flags;
				nudpcb->u_tun_func = NULL;

				break;

			case IPPROTO_RAW:
			case IPPROTO_ICMP:
				/* Nothing to do. */
				break;

			default:
				ERRMSG(ctx, "%s: unhandled IPPROTO "
				    "%d / %d\n", __func__,
				    vds->so_protocol, vdinpcb->inp_ip_p);
				error = EINVAL;
				goto out_unlock;
				break;
			}
		}

		INP_WUNLOCK(ninpcb);
		INP_HASH_WUNLOCK(ninpcb->inp_pcbinfo);
		INP_INFO_WUNLOCK(ninpcb->inp_pcbinfo);

		break;

	default:
		ERRMSG(ctx, "%s: unhandled protocol family %d\n",
		    __func__, vds->so_family);
		error = EINVAL;
		goto out_unlock;
		break;
	}

	if (vdo_typeofnext(ctx) == VPS_DUMPOBJT_SOCKBUF)
		if ((error = vps_restore_sockbuf(ctx, vps, &nso->so_rcv)))
			goto out_unlock;
	if (vdo_typeofnext(ctx) == VPS_DUMPOBJT_SOCKBUF)
		if ((error = vps_restore_sockbuf(ctx, vps, &nso->so_snd)))
			goto out_unlock;

	/*
	 * On success, the only thing to return is the new fd index
	 * in curthread->td_retval[0].
	 */

  out_unlock:

	SOCKBUF_UNLOCK(&nso->so_snd);
	SOCKBUF_UNLOCK(&nso->so_rcv);
	sbunlock(&nso->so_snd);
	sbunlock(&nso->so_rcv);

	fdrop(fp, curthread);

  out:

	if (error) {
		/* XXX destroy socket. */
		ERRMSG(ctx, "%s: error = %d\n", __func__, error);
	}

	curthread->td_retval[0] = fdidx;
	curthread->td_ucred = save_ucred;
	if (ncr)
		crfree(ncr);
	CURVNET_RESTORE();

	/* Sockets that were on the accept queue of this socket. */
	if (vds->so_qlen > 0 || vds->so_incqlen > 0) {
		DBGR("%s: so_qlen=%d so_incqlen=%d\n",
			__func__, vds->so_qlen, vds->so_incqlen);

		fdidx_save = fdidx;
		cfd = curthread->td_proc->p_fd;

		for (i = 0; i < (vds->so_qlen + vds->so_incqlen); i++) {

			if ((error = vps_restore_socket(ctx, vps, p)))
				return (error);
			/*
			 * We have to remove it from the fdset and link it
			 * up to the listening socket.
			 */
			fdidx = (int)curthread->td_retval[0];
			DBGR("%s: open returned fd %d\n", __func__, fdidx);

			FILEDESC_XLOCK(cfd);
			fp = cfd->fd_ofiles[fdidx].fde_file;
			nso2 = (struct socket *)fp->f_data;
			cfd->fd_ofiles[fdidx].fde_file = NULL;
			cfd->fd_ofiles[fdidx].fde_flags = 0;
			fdunused(cfd, fdidx);
			FILEDESC_XUNLOCK(cfd);

			/*
			 * Have to do ugly things here ...
			 * Better allocate sockets using soalloc() rather
			 * than sys_socket().
			 *
			 */
			fp->f_ops = &badfileops;
			fdrop(fp, curthread);

			nso2->so_count = 0;

			if (i < vds->so_qlen) {
				TAILQ_INSERT_TAIL(&nso->so_comp, nso2,
				    so_list);
				nso->so_qlen++;
			} else {
				TAILQ_INSERT_TAIL(&nso->so_incomp, nso2,
				    so_list);
				nso->so_incqlen++;
			}

		}
		curthread->td_retval[0] = fdidx_save;
	}

	return (error);
}

VPSFUNC
static int
vps_restore_pathtovnode(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vnode **vnp)
{
	struct vps_dumpobj *o1;
	struct vps_dump_filepath *vdfp;
	struct nameidata nd;
	int error;

	o1 = vdo_next(ctx);

	if (o1->type != VPS_DUMPOBJT_FILE_PATH)
		return (EINVAL);

	vdfp = (struct vps_dump_filepath *)o1->data;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE,
		vdfp->fp_path, curthread);
	error = namei(&nd);
	if (error)
		return (error);

	/* XXX VREF() ?! */
	*vnp = nd.ni_vp;

	VOP_UNLOCK(nd.ni_vp, 0);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	if (strcmp(vdfp->fp_path, "/VPSRELINKED_") == 0)
		/* XXX oldinum -> 0 ? */
		kern_unlinkat(curthread, AT_FDCWD, vdfp->fp_path,
		    UIO_SYSSPACE, 0);

	return (0);
}

/* EXPERIMENTAL - nfs doesn't support vfs_vget() :-( */
VPSFUNC
static int
vps_restore_inodenumtovnode(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vnode **vnp)
{
	struct vps_dump_fileinodenum *vdfi;
	struct vps_dumpobj *o1;
	struct mount *mp;
	struct vnode *vp;
	char *vpsroot;
	char *mnton;
	int len;
	int error;

	o1 = vdo_next(ctx);

	if (o1->type != VPS_DUMPOBJT_FILE_INODENUM)
		return (EINVAL);

	vdfi = (struct vps_dump_fileinodenum *)o1->data;

	DBGR("%s: fsid=%llu fileid=%d\n",
	     __func__, (unsigned long long)vdfi->fsid, vdfi->fileid);

	DBGR("%s: vps's rootpath=[%s] vnode=%p\n",
	     __func__, vps->_rootpath, vps->_rootvnode);

	vpsroot = strdup(vps->_rootpath, M_TEMP);
	if (vpsroot[strlen(vpsroot) - 1] == '/')
		vpsroot[strlen(vpsroot) - 1] = '\0';
	len = strlen(vpsroot);

	mtx_lock(&mountlist_mtx);
	TAILQ_FOREACH(mp, &mountlist, mnt_list) {
		mnton = mp->mnt_stat.f_mntonname;
		if (!(strncmp(vpsroot, mnton, len) == 0 &&
		    (mnton[len] == '\0' || mnton[len] == '/')))
			continue;

		if (mp->mnt_stat.f_fsid.val[0] == vdfi->fsid)
			break;
	}
	mtx_unlock(&mountlist_mtx);

	free(vpsroot, M_TEMP);

	if (mp == NULL) {
		ERRMSG(ctx, "%s: no mount found for fsid [%16x]\n",
		    __func__, vdfi->fsid);
		return (ENOENT);
	} else
		DBGR("%s: got mount=%p for fsid\n", __func__, mp);

	error = VFS_VGET(mp, vdfi->fileid, LK_SHARED | LK_RETRY, &vp);
	if (error != 0) {
		ERRMSG(ctx, "%s: VFS_VGET() error=%d\n",
		    __func__, error);
		return (error);
	}

	*vnp = vp;

	vref(vp);
	VOP_UNLOCK(vp, 0);

	return (0);
}

VPSFUNC
static int
vps_restore_vnode(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vnode **vnp)
{
	int error;

	if (vdo_typeofnext(ctx) == VPS_DUMPOBJT_FILE_INODENUM)
		error = vps_restore_inodenumtovnode(ctx, vps, vnp);
	else if (vdo_typeofnext(ctx) == VPS_DUMPOBJT_FILE_PATH)
		error = vps_restore_pathtovnode(ctx, vps, vnp);
	else {
		ERRMSG(ctx, "%s: vdo_typeofnext(ctx)=%d\n",
		    __func__, vdo_typeofnext(ctx));
		return (EINVAL);
	}

	return (error);
}

VPSFUNC
static int
vps_restore_file_vnode(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct proc *p, struct thread *curtd, struct vps_dump_file *vdf)
{
	struct vps_dumpobj *o1;
	struct vps_dump_filepath *vdfp;
	int oflags;
	int error = 0;

	o1 = vdo_next(ctx);

	if (o1->type != VPS_DUMPOBJT_FILE_PATH) {
		ERRMSG(ctx, "%s: DTYPE_VNODE without path information "
			"-> skipping !\n", __func__);
		error = EINVAL;
		goto out;
	}
	vdfp = (struct vps_dump_filepath *)o1->data;

	oflags = OFLAGS(vdf->f_flag);

	/*
	 * We have to open the file in our current thread's context,
	 * because e.g. devfs relies on td == curthread.
	 * Afterwards we simply move the reference into the new proc's fd.
	 */
	if ((error = kern_openat(curtd, AT_FDCWD /* XXX */, vdfp->fp_path,
				UIO_SYSSPACE, oflags, 0))) {
		ERRMSG(ctx, "%s: open error: [%s] %d\n",
			__func__, vdfp->fp_path, error);
#ifdef DIAGNOSTIC
		/* XXX debugging */
		if (error == ENOENT && !strcmp(vdfp->fp_path, "/dev/null"))
			kdb_enter(KDB_WHY_BREAK, "VPS break to debugger");
		error = 0;
#else
		error = EINVAL;
		goto out;
#endif
	}

	if (strncmp(vdfp->fp_path, "/VPSRELINKED_", 13) == 0) {
		DBGR("%s: unlinking [%s]\n", __func__, vdfp->fp_path);
		/* XXX oldinum -> 0 ? */
		kern_unlinkat(curtd, AT_FDCWD, vdfp->fp_path,
		    UIO_SYSSPACE, 0);
	}

	/* Returning new fd index in curthread->td_retval[0]. */

  out:
	return (error);
}

VPSFUNC
static int
vps_restore_file_pts(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct proc *p, struct thread *curtd, struct vps_dump_file *vdf)
{
	struct vps_dumpobj *o1, *o2;
	struct vps_restore_obj *ro;
	struct vps_dump_filepath *vdfp;
	struct filedesc *fdp;
	struct file *fp;
	struct tty *ttyp;
	struct vps_dump_pts *vdp;
	struct pts_softc *psc;
	struct termios *termiosp;
	int oflags;
	int fdidx;
	int found;
	int i;
	int error = 0;

	o1 = vdo_next(ctx);
	if (o1->type != VPS_DUMPOBJT_PTS) {
		ERRMSG(ctx, "%s: DTYPE_PTS without VPS_DUMPOBJT_PTS\n",
		    __func__);
		error = EINVAL;
		goto out;
	}

	vdp = (struct vps_dump_pts *)o1->data;

	if (vdo_typeofnext(ctx) == VPS_DUMPOBJT_UCRED)
		vdo_next(ctx);

	o2 = vdo_next(ctx);
	if (o2->type != VPS_DUMPOBJT_FILE_PATH) {
		ERRMSG(ctx, "%s: DTYPE_PTS without "
		    "VPS_DUMPOBJT_FILE_PATH\n", __func__);
		error = EINVAL;
		goto out;
	}

	vdfp = (struct vps_dump_filepath *)o2->data;

	oflags = OFLAGS(vdf->f_flag);

	fdp = curtd->td_proc->p_fd;
	FILEDESC_XLOCK(fdp);

	found = 0;
	for (fdidx = 0; fdidx < fdp->fd_nfiles; fdidx++) {
		fp = fget_locked(fdp, fdidx);
		if (fp && fp->f_type == DTYPE_PTS) {
			if (strncmp(vdfp->fp_path,
			    tty_devname((struct tty *)fp->f_data),
			    vdfp->fp_size) == 0) {
				found = 1;
				curtd->td_retval[0] = fdidx;
				break;
			}
		}
	}

	FILEDESC_XUNLOCK(fdp);

	if (found == 0) {
		ERRMSG(ctx, "%s: pts [%s] not found !\n",
		    __func__, vdfp->fp_path);
		error = EINVAL;
		goto out;
	}

	ttyp = (struct tty *)fp->f_data;
	termiosp = malloc(sizeof(*termiosp), M_TEMP, M_WAITOK);
	termiosp->c_iflag = vdp->pt_termios.c_iflag;
	termiosp->c_oflag = vdp->pt_termios.c_oflag;
	termiosp->c_cflag = vdp->pt_termios.c_cflag;
	termiosp->c_lflag = vdp->pt_termios.c_lflag;
	termiosp->c_ispeed = vdp->pt_termios.c_ispeed;
	termiosp->c_ospeed = vdp->pt_termios.c_ospeed;
	for (i = 0; i < NCCS; i++)
		termiosp->c_cc[i] = vdp->pt_termios.c_cc[i];
	error = kern_ioctl(curtd, /* fd */ curtd->td_retval[0],
				TIOCSETAW, (caddr_t)termiosp);
	free(termiosp, M_TEMP);
	if (error)
		ERRMSG(ctx, "%s: ttydev_ioctl() error=%d\n",
		    __func__, error);
	ttyp->t_pgrp = (void*)(size_t)vdp->pt_pgrp_id; /* ID */
	psc = tty_softc(ttyp);
	psc->pts_flags = vdp->pt_flags;

	if (vdp->pt_cred != NULL) {
		if (psc->pts_cred != NULL)
			crfree(psc->pts_cred);
		psc->pts_cred = vps_restore_ucred_lookup(ctx, vps,
		    vdp->pt_cred);
	}

	/* Insert into restored objects list. */
	ro = malloc(sizeof(*ro), M_VPS_RESTORE, M_WAITOK | M_ZERO);
	ro->type = VPS_DUMPOBJT_PTS;
	ro->new_ptr = ttyp;
	SLIST_INSERT_HEAD(&ctx->obj_list, ro, list);

  out:
	return (error);
}

VPSFUNC
static int
vps_restore_file_kqueue(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct proc *p, struct thread *curtd, struct vps_dump_file *vdf)
{
	struct filedesc *nfd, *cfd;
	struct file *nfp;
	char tmpflags;
	int idx;
	int error = 0;

	if ((error = vps_restore_kqueue(ctx, vps, p))) {
		ERRMSG(ctx, "%s: vps_restore_kqueue() error: %d\n",
			__func__, error);
		goto out;
	}
	/* The file was restored in newproc so move it to curproc. */

	idx = (int)FIRST_THREAD_IN_PROC(p)->td_retval[0];

	nfd = p->p_fd;
	cfd = curtd->td_proc->p_fd;

	FILEDESC_XLOCK(nfd);
	nfp = nfd->fd_ofiles[idx].fde_file;
	fhold(nfp);
	tmpflags = nfd->fd_ofiles[idx].fde_flags;
	nfd->fd_ofiles[idx].fde_file = NULL;
	nfd->fd_ofiles[idx].fde_flags = 0;
	fdunused(nfd, idx);
	FILEDESC_XUNLOCK(nfd);

	FILEDESC_XLOCK(cfd);
	/* fdalloc() calls fdused() for the new descriptor. */
	if ((error = fdalloc(curtd, 0, &idx))) {
		ERRMSG(ctx, "%s: fdalloc(): %d\n", __func__, error);
		fdrop(nfp, curtd);
		FILEDESC_XUNLOCK(cfd);
		goto out;
	}
	cfd->fd_ofiles[idx].fde_file = nfp;
	cfd->fd_ofiles[idx].fde_flags = tmpflags;
	fdrop(nfp, curtd);
	FILEDESC_XUNLOCK(cfd);
	curtd->td_retval[0] = idx;

  out:
	return (error);
}

VPSFUNC
static int
vps_restore_file(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct proc *p)
{
	struct vps_dumpobj *o1;
	struct vps_dump_file *vdf;
	struct vps_restore_obj *ro;
	struct filedesc *cfd;
	struct file *nfp;
	struct vps *save_vps;
	struct ucred *save_ucred, *ncr;
	struct vnode *save_rdir, *save_cdir;
	struct thread *curtd;
	char tmpflags;
	int idx;
	int error = 0;

	curtd = curthread;
	save_vps = curtd->td_vps;
	save_ucred = curtd->td_ucred;
	save_rdir = curtd->td_proc->p_fd->fd_rdir;
	save_cdir = curtd->td_proc->p_fd->fd_cdir;
	curtd->td_vps = vps;
	curtd->td_proc->p_fd->fd_rdir = p->p_fd->fd_rdir;
	curtd->td_proc->p_fd->fd_cdir = p->p_fd->fd_cdir;

	ncr = NULL;

	/* caller verified type */
	o1 = vdo_next(ctx);

	/*
	DBGR("%s: o1=%p: VPS_DUMPOBJT_FILE size=%d\n",
	    __func__, o1, o1->size);
	*/

	vdf = (struct vps_dump_file *)o1->data;
	ncr = NULL;

	DBGR("%s: index= origidx= origptr=%p type=%d flag=%08x offset=%d\n",
	    __func__, vdf->orig_ptr, vdf->f_type, vdf->f_flag,
	    (int)vdf->f_offset);

	/* Lookup in list of restored file objects. */
	SLIST_FOREACH(ro, &ctx->obj_list, list)
		if (ro->type == VPS_DUMPOBJT_FILE && ro->orig_ptr ==
		    vdf->orig_ptr)
			break;

	if (ro != NULL) {

		DBGR("%s: found in restored obj list: orig_ptr=%p "
		    "new_ptr=%p\n", __func__, ro->orig_ptr, ro->new_ptr);

		/* skip over child objects if exist */
		while (vdo_nextischild(ctx, o1))
			vdo_next(ctx);

		error = 0;
		goto out;
	}

	if (vdo_typeofnext(ctx) == VPS_DUMPOBJT_UCRED) {
		vdo_next(ctx);
	}
	ncr = vps_restore_ucred_lookup(ctx, vps, vdf->f_cred);
	save_ucred = curtd->td_ucred;
	curtd->td_ucred = ncr;

	switch (vdf->f_type) {

	case DTYPE_VNODE:
		/* Returns new fd index in curtd->td_retval[0]. */
		if ((error = vps_restore_file_vnode(ctx, vps, p, curtd,
		    vdf))) {
			ERRMSG(ctx, "%s: vps_restore_file_vnode() "
			    "error: %d\n", __func__, error);
			goto out;
		}
		break;

	case DTYPE_PTS:
		/* Returns new fd index in curtd->td_retval[0]. */
		if ((error = vps_restore_file_pts(ctx, vps, p, curtd,
		    vdf))) {
			ERRMSG(ctx, "%s: vps_restore_file_pts() "
			    "error: %d\n", __func__, error);
			goto out;
		}
		break;

        case DTYPE_SOCKET:
		/* Returns new fd index in curtd->td_retval[0]. */
		if ((error = vps_restore_socket(ctx, vps, p))) {
			ERRMSG(ctx, "%s: vps_restore_socket() error: %d\n",
				__func__, error);
			goto out;
		}
		break;

	case DTYPE_PIPE:
		/* Returns new fd index in curtd->td_retval[0]. */
		if ((error = vps_restore_pipe(ctx, vps, p))) {
			ERRMSG(ctx, "%s: vps_restore_pipe() error: %d\n",
				__func__, error);
			goto out;
		}
		break;

	case DTYPE_KQUEUE:
		/* Returns new fd index in curtd->td_retval[0]. */
		if ((error = vps_restore_file_kqueue(ctx, vps, p, curtd,
		    vdf))) {
			ERRMSG(ctx, "%s: vps_restore_file_kqueue() "
			    "error: %d\n", __func__, error);
			goto out;
		}
		break;

	default:
		ERRMSG(ctx, "%s: unhandled file type %d\n",
			__func__, vdf->f_type);
		error = ENOTSUP;
		goto out;
		break;
	}

	idx = (int)curtd->td_retval[0];
	DBGR("%s: open returned fd %d\n", __func__, idx);

	cfd = curtd->td_proc->p_fd;

	FILEDESC_XLOCK(cfd);
	nfp = cfd->fd_ofiles[idx].fde_file;
	fhold(nfp);
	tmpflags = cfd->fd_ofiles[idx].fde_flags;
	cfd->fd_ofiles[idx].fde_file = NULL;
	cfd->fd_ofiles[idx].fde_flags = 0;
	fdunused(cfd, idx);
	FILEDESC_XUNLOCK(cfd);

	fhold(nfp);
	nfp->f_offset = vdf->f_offset;
	fdrop(nfp, curtd);

	/* Restore f_flag XXX */
	if (vdf->f_flag & FNONBLOCK)
		nfp->f_flag |= FNONBLOCK;

	/* Insert into restored objects list. */
	ro = malloc(sizeof(*ro), M_VPS_RESTORE, M_WAITOK | M_ZERO);
	ro->type = VPS_DUMPOBJT_FILE;
	ro->orig_ptr = vdf->orig_ptr;
	/* Having an extra reference now for list. */
	ro->new_ptr = nfp;
	SLIST_INSERT_HEAD(&ctx->obj_list, ro, list);
	fdrop(nfp, curtd);

  out:
	curtd->td_vps = save_vps;
	curtd->td_proc->p_fd->fd_rdir = save_rdir;
	curtd->td_proc->p_fd->fd_cdir = save_cdir;

	if (ncr) {
		curtd->td_ucred = save_ucred;
		crfree(ncr);
		ncr = NULL;
	}
	return (error);
}

VPSFUNC
static int
vps_restore_fdset_linkup(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vps_dump_filedesc *vdfd, struct proc *p, char is_final)
{
	struct vps_restore_obj *ro;
	struct filedesc *nfd;
	struct file *nfp;
	int i;
	int error = 0;

	nfd = p->p_fd;

	/* Now all files should exist, so link them into fdset. */
	for (i = 0; i < vdfd->fd_nfiles; i++) {

		DBGR("%s: vdfd->fd_entries[%d].fp = %p\n",
			__func__, i, vdfd->fd_entries[i].fp);
		if (vdfd->fd_entries[i].fp == NULL)
			continue;

		/* Look if already restored in a previous run. */
		if (i <= nfd->fd_nfiles &&
		    nfd->fd_ofiles[i].fde_file != NULL)
			continue;

                /* Lookup in list of restored file objects. */
		SLIST_FOREACH(ro, &ctx->obj_list, list)
			if (ro->type == VPS_DUMPOBJT_FILE &&
			    ro->orig_ptr == vdfd->fd_entries[i].fp)
				break;

		/* Only return error if this is the final run. */
		if (is_final != 0 && ro == NULL) {
			ERRMSG(ctx, "%s: can't find file fp=%p\n",
			    __func__, vdfd->fd_entries[i].fp);
			error = EINVAL;
			goto out;
		} else if (ro == NULL) {
			continue;
		}
		nfp = (struct file *)ro->new_ptr;
		fhold(nfp);

		FILEDESC_XLOCK(nfd);
		if (i >= nfd->fd_nfiles)
			fdgrowtable(nfd, i);
		nfd->fd_ofiles[i].fde_file = nfp;
		nfd->fd_ofiles[i].fde_flags = vdfd->fd_entries[i].flags;
		nfd->fd_ofiles[i].fde_rights = vdfd->fd_entries[i].rights;
		fdused(nfd, i);
		FILEDESC_XUNLOCK(nfd);

		DBGR("%s: linked up fp: idx=%d new=%p orig=%p\n",
		    __func__, i, ro->new_ptr, ro->orig_ptr);
	}

  out:
	return (error);
}

VPSFUNC
static int
vps_restore_fdset(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct proc *p, struct filedesc *orig_fdp)
{
	struct vps_dumpobj *o1;
	struct vps_dumpobj *o2;
	struct vps_dumpobj *o3;
	struct vps_dump_filedesc *vdfd;
	struct vps_restore_obj *ro;
	struct filedesc *nfd, *cfd;
	struct vps *save_vps;
	struct ucred *save_ucred;
	struct vnode *save_rdir;
	struct vnode *save_cdir;
	int error = 0;

	if (vdo_typeofnext(ctx) != VPS_DUMPOBJT_FDSET) {
		/* Lookup in list of restored file objects. */
		SLIST_FOREACH(ro, &ctx->obj_list, list)
			if (ro->type == VPS_DUMPOBJT_FDSET &&
			    ro->orig_ptr == orig_fdp)
				break;
		if (ro == NULL) {
			ERRMSG(ctx, "%s: fdset orig_ptr=%p not found !\n",
				__func__, orig_fdp);
			return (EINVAL);
		}
		p->p_fd = fdshare(ro->new_ptr);
		DBGR("%s: linked shared fdset %p (orig %p) to proc %p/%d\n",
		    __func__, p->p_fd, orig_fdp, p, p->p_pid);

		return (0);
	}

	/* verified type */
	o1 = vdo_next(ctx);

	/*
	DBGR("%s: o1=%p: VPS_DUMPOBJT_FDSET size=%d\n",
	    __func__, o1, o1->size);
	*/

	vdfd = (struct vps_dump_filedesc *)o1->data;

	DBGR("%s: fdset has %d entries\n", __func__, vdfd->fd_nfiles);

	p->p_fd = nfd = fdinit(NULL);
	cfd = curthread->td_proc->p_fd;

	if (vdfd->fd_have_cdir != 0) {
		if ((error = vps_restore_vnode(ctx, vps,
		    &p->p_fd->fd_cdir)))
			return (error);
	}
	if (vdfd->fd_have_rdir != 0) {
		if ((error = vps_restore_vnode(ctx, vps,
		    &p->p_fd->fd_rdir)))
			return (error);
	}
	if (vdfd->fd_have_jdir != 0) {
		if ((error = vps_restore_vnode(ctx, vps,
		    &p->p_fd->fd_jdir)))
			return (error);
	}

	save_vps = curthread->td_vps;
	save_ucred = curthread->td_ucred;
	save_rdir = curthread->td_proc->p_fd->fd_rdir;
	save_cdir = curthread->td_proc->p_fd->fd_cdir;
	curthread->td_vps = vps;
	curthread->td_proc->p_fd->fd_rdir = p->p_fd->fd_rdir;
	curthread->td_proc->p_fd->fd_cdir = p->p_fd->fd_cdir;

	/* 
	 * First only restore file objects with priority >= 0,
	 * then the ones with priority < 0.
	 * This is necessary because kqueue has to be restored
	 * after all other file descriptors.
	 */

	o2 = vdo_getcur(ctx);

	while (vdo_typeofnext(ctx) == VPS_DUMPOBJT_FILE) {
		if (vdo_peek(ctx)->prio < 0) {
			o3 = vdo_next(ctx);
			while (vdo_nextischild(ctx, o3))
				vdo_next(ctx);
			continue;
		}
		if ((error = vps_restore_file(ctx, vps, p)))
			goto out;
	}

	if ((error = vps_restore_fdset_linkup(ctx, vps, vdfd, p,
	    0 /* not final run */)) != 0)
		goto out;

	vdo_setcur(ctx, o2);

	while (vdo_typeofnext(ctx) == VPS_DUMPOBJT_FILE) {
		if (vdo_peek(ctx)->prio >= 0) {
			o3 = vdo_next(ctx);
			while (vdo_nextischild(ctx, o3))
				vdo_next(ctx);
			continue;
		}
		if ((error = vps_restore_file(ctx, vps, p)))
			goto out;
	}

	if ((error = vps_restore_fdset_linkup(ctx, vps, vdfd, p,
	    1 /* final run */)) != 0)
		goto out;

	curthread->td_vps = save_vps;
	curthread->td_proc->p_fd->fd_rdir = save_rdir;
	curthread->td_proc->p_fd->fd_cdir = save_cdir;

	/* Insert into restored objects list. */
	ro = malloc(sizeof(*ro), M_VPS_RESTORE, M_WAITOK | M_ZERO);
	ro->type = VPS_DUMPOBJT_FDSET;
	ro->orig_ptr = vdfd->fd_orig_ptr;
	ro->new_ptr = nfd;
	SLIST_INSERT_HEAD(&ctx->obj_list, ro, list);

	DBGR("%s: restored fdset orig=%p new=%p\n",
	    __func__, vdfd->fd_orig_ptr, nfd);

  out:
	return (error);
}

VPSFUNC
static void
vps_restore_cleanup_fdset(struct vps_snapst_ctx *ctx, struct vps *vps,
	struct _vps_restore_obj_list *obj_list)
{
	struct vps_restore_obj *obj, *obj2;

	SLIST_FOREACH_SAFE(obj, obj_list, list, obj2) {
		if (obj->type != VPS_DUMPOBJT_FILE)
			continue;
		if (obj->new_ptr)
			fdrop((struct file *)obj->new_ptr, curthread);
		/* Let the generic cleanup unlink and free the list item. */
	}
}

VPSFUNC
static int
vps_restore_pargs(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct proc *p)
{
	struct vps_dumpobj *o1;
	struct vps_dump_pargs *vdp;
	int error = 0;

	/* caller verified type */
	o1 = vdo_next(ctx);

	vdp = (struct vps_dump_pargs *)o1->data;

	p->p_args = pargs_alloc(vdp->ar_length);
	memcpy(p->p_args->ar_args, vdp->ar_args, vdp->ar_length);

	DBGR("%s: len=%d [%s]\n", __func__,
	    vdp->ar_length, p->p_args->ar_args);

	return (error);
}

/*
 * Return one logical memory page named by index, from the userspace dump.
 */
VPSFUNC
static struct vm_page *
vps_restore_getuserpage(struct vps_snapst_ctx *ctx, int idx, int test)
{
	vm_map_t map;
	vm_object_t obj;
	vm_map_entry_t entry;
	vm_pindex_t index;
	vm_prot_t prot;
	vm_page_t m;
	boolean_t wired;
	vm_offset_t vaddr;

	vaddr = (vm_offset_t)(ctx->userpagesaddr + (idx << PAGE_SHIFT));

	map = &curthread->td_proc->p_vmspace->vm_map;

  retry:
	if ((vm_map_lookup(&map, vaddr, VM_PROT_READ, &entry, &obj,
	    &index, &prot, &wired))) {
		DBGR("%s: vm_map_lookup(): error\n", __func__);
		m = NULL;
		goto out;
	}

	VM_OBJECT_WLOCK(obj);

	m = vm_page_lookup(obj, index);

	vm_map_lookup_done(map, entry);

	/*DBGR("%s: userpage idx=%d at %p\n", __func__, idx, mem);*/

	if (m == NULL) {
		/* Try to page in. */

		/* Note: unlocks the object if it has to sleep */
		m = vm_page_grab(obj, index,
		    VM_ALLOC_NORMAL | VM_ALLOC_RETRY);

		if (m == NULL)
			panic("%s: vm_page_alloc() == NULL", __func__);

		if ((vm_pager_get_pages(obj, &m, 1, 0)) != VM_PAGER_OK) {
			vm_page_lock(m);
			vm_page_free(m);
			vm_page_unlock(m);
			VM_OBJECT_WUNLOCK(obj);
			m = NULL;
			goto out;
		}

		m = vm_page_lookup(obj, index);

		vm_page_wakeup(m);
	}

	/*
	 * The pageout daemon might have already decided to swap out
	 * this very page.
	 */
	if (m->oflags & VPO_SWAPINPROG || m->busy > 0) {
		DBGR("%s: m->oflags & VPO_SWAPINPROG || m->busy > 0   "
			"--> vm_page_sleep()\n", __func__);
		vm_page_sleep(m, "swpinp");
		VM_OBJECT_WUNLOCK(obj);
		goto retry;
	}

	vm_page_busy(m);

	VM_OBJECT_WUNLOCK(obj);

  out:
	KASSERT( !(test == 0 && m == NULL),
	    ("vps_restore_getuserpage: unable to retrieve page, "
	    "idx=%d/offset=%d", idx, idx << PAGE_SHIFT));

	return (m);
}

VPSFUNC
static int
vps_restore_vmobject(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vm_object **nvo_out)
{
	struct vps_dumpobj *o1, *o2;
	struct vps_dump_vmobject *vdvmo;
	struct vps_dump_vmpages *vdvmp;
	struct vps_dump_filepath *vdfp;
	struct vps_restore_obj *vbo;
	struct vm_object *nvo, *vo2;
	struct vm_page *m;
	struct ucred *ncr;
	struct nameidata nd;
	int error = 0;

	/*
	DBGR("%s: o=%p: type=%d size=%d\n",
	    __func__, o, o->type, o->size);
	*/

	*nvo_out = NULL;

	/* caller verified type */
	o1 = vdo_next(ctx);

	vdvmo = (struct vps_dump_vmobject *)o1->data;

	DBGR("%s: old obj=%p: size=%d flags=%04x type=%02x cred=%p "
	    "origptr=%p\n",
	    __func__, vdvmo, (int)vdvmo->size, vdvmo->flags,
	    vdvmo->type, vdvmo->cred, vdvmo->orig_ptr);

	if (vdvmo->type == OBJT_VNODE && vdvmo->have_vnode) {
		o2 = vdo_next(ctx);
		if (o2->type != VPS_DUMPOBJT_FILE_PATH) {
			ERRMSG(ctx, "%s: wrong object, expected "
			    "VPS_DUMPOBJT_FILE_PATH\n", __func__);
			error = EINVAL;
			goto out;
		}
		vdfp = (struct vps_dump_filepath *)o2->data;

		NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE,
			vdfp->fp_path, curthread);
		if ((error = namei(&nd))) {
			ERRMSG(ctx, "%s: namei([%s]): error = %d\n",
			    __func__, vdfp->fp_path, error);
			goto out;
		}
		if ((error = VOP_OPEN(nd.ni_vp, FREAD, curthread->td_ucred,
			curthread, NULL))) {
			ERRMSG(ctx, "%s: VOP_OPEN(...): error = %d\n",
			    __func__, error);
			VOP_UNLOCK(nd.ni_vp, 0);
			NDFREE(&nd, NDF_ONLY_PNBUF);
			goto out;
		}
		nvo = nd.ni_vp->v_object;
		vm_object_reference(nvo);
		vrele(nd.ni_vp);
		VOP_UNLOCK(nd.ni_vp, 0);
		NDFREE(&nd, NDF_ONLY_PNBUF);

		KASSERT(nvo->cred == NULL,
		    ("%s: nvo=%p ->cred=%p\n",
		    __func__, nvo, nvo->cred));

		DBGR("%s: path [%s] got vnode %p v_object %p\n",
		    __func__, vdfp->fp_path, nd.ni_vp, nvo);

	} else if (vdvmo->type == OBJT_DEFAULT ||
	    vdvmo->type == OBJT_SWAP) {

		nvo = vm_object_allocate(OBJT_DEFAULT, vdvmo->size);

	} else if (vdvmo->type == OBJT_PHYS &&
		   vdvmo->is_sharedpageobj != 0) {

		DBGR("%s: shared_page_obj\n", __func__);

		nvo = shared_page_obj;
		vm_object_reference(nvo);

	} else if (vdvmo->type == OBJT_PHYS) {

		DBGR("%s: OBJT_PHYS\n", __func__);
		nvo = vm_object_allocate(OBJT_PHYS, vdvmo->size);

	} else {
		/*
		 * XXX We don't support that yet.
		 */

		ERRMSG(ctx, "%s: unsupported vm object: vdvmo=%p type=%d\n",
		    __func__, vdvmo, vdvmo->type);
		/* XXX missing the sibling list here */
		error = EINVAL;
		goto out;
	}

	if (vdvmo->cred != NULL && vdo_typeofnext(ctx) ==
	    VPS_DUMPOBJT_UCRED) {
		vdo_next(ctx);
	}
	if (vdo_typeofnext(ctx) == VPS_DUMPOBJT_UCRED &&
	    vdvmo->cred == NULL)
		DBGR("%s: have ucred but vdvmo->cred == NULL!!!\n",
		    __func__);

	if (nvo != shared_page_obj) {
		nvo->flags = vdvmo->flags;
		nvo->charge = vdvmo->charge;
		KASSERT(nvo->cred == NULL,
		    ("%s: nvo->cred = %p\n", __func__, nvo->cred));
		/*DBGR("%s: charge=%lu\n", __func__, nvo->charge);*/
		if (vdvmo->cred != NULL) {
			ncr = vps_restore_ucred_lookup(ctx, vps,
			    vdvmo->cred);
			KASSERT(ncr != NULL,
			    ("%s: ucred not found\n", __func__));
			nvo->cred = ncr;
			swap_reserve_by_cred(nvo->charge, nvo->cred);
		}
	}

	if (vdo_typeofnext(ctx) == VPS_DUMPOBJT_VMPAGE) {

		struct vps_dump_vmpageref *vdvmpr;
		int i;

		/*
		DBGR("%s: o2=%p: type=VMPAGE size=%d (pindex=%08x)\n",
		    __func__, o2, o2->size, *((int*)o2->data) );
		*/

		o2 = vdo_next(ctx);

		vdvmp = (struct vps_dump_vmpages *)o2->data;
		vdvmpr = (struct vps_dump_vmpageref *)(vdvmp + 1);

		DBGR("%s: vdvmp=%p count=%u vdvmpr=%p\n",
		    __func__, vdvmp, (u_int)vdvmp->count, vdvmpr);

		for (i = 0; i < vdvmp->count; i++) {

			m = vps_restore_getuserpage(ctx,
			    ctx->userpagesidx++, 0);

			vo2 = m->object;
			VM_OBJECT_WLOCK(nvo);
			VM_OBJECT_WLOCK(vo2);
			vm_page_lock(m);

			pmap_remove_all(m);

			KASSERT((m->oflags & VPO_SWAPINPROG) == 0,
			    ("%s: m=%p oflags 0x%x & VPO_SWAPINPROG\n",
			    __func__, m, m->oflags));

			KASSERT(vdvmpr->pr_vmobject == vdvmo->orig_ptr,
			    ("%s: object mismatch ! "
			    "(vdvmpr->pr_vmobject=%p)\n",
			    __func__, vdvmpr->pr_vmobject));

			vm_page_rename(m, nvo, vdvmpr->pr_pindex);

			if (vdvmo->type == OBJT_PHYS)
				vm_page_wire(m);

			vm_page_unlock(m);

			VM_OBJECT_WUNLOCK(vo2);
			VM_OBJECT_WUNLOCK(nvo);

			/* Next mem page. */
			vdvmpr++;
		}
	}

	if (vdvmo->backing_object) {
		/*
		 * We search in our list for the referenced backing object.
		 * If it's not there, something went wrong badly.
		 */

		SLIST_FOREACH(vbo, &ctx->obj_list, list)
			if (vbo->type == VPS_DUMPOBJT_VMOBJECT &&
			    vbo->orig_ptr == vdvmo->backing_object) {
				nvo->backing_object = vbo->new_ptr;
				break;
			}

		if (nvo->backing_object == NULL) {
			ERRMSG(ctx, "%s: backing object not found "
			    "(orig_ptr=%p)\n",
			    __func__, vdvmo->backing_object);
			error = EINVAL;
			goto out;
		}

		vm_object_reference(nvo->backing_object);
		LIST_INSERT_HEAD(&nvo->backing_object->shadow_head,
			nvo, shadow_list);
		nvo->backing_object->shadow_count++;
		nvo->backing_object->generation++;

		nvo->backing_object_offset = vdvmo->backing_object_offset;

		DBGR("%s: found backing object %p for object %p\n",
		    __func__, nvo->backing_object, nvo);
	}

	/*
	 * Add this object to our list of vm (backing) objects.
	 *
	 * Note: the reference that we have now is released
	 * when this list is cleaned up.
	 */
	vbo = malloc(sizeof(*vbo), M_VPS_RESTORE, M_WAITOK);
	vbo->type = VPS_DUMPOBJT_VMOBJECT;
	vbo->orig_ptr = vdvmo->orig_ptr;
	vbo->new_ptr = nvo;

	SLIST_INSERT_HEAD(&ctx->obj_list, vbo, list);

	DBGR("%s: object=%p put in list of vm objects, orig_ptr=%p\n",
	    __func__, nvo, vbo->orig_ptr);

	KASSERT(!(nvo->type != 2 && nvo != shared_page_obj &&
	    nvo->ref_count != 1),
	    ("%s: nvo=%p ->ref_count = %d\n",
	    __func__, nvo, nvo->ref_count));

	*nvo_out = nvo;
	/*DBGR("%s: *nvo_out=%p nvo=%p\n", __func__, *nvo_out, nvo);*/

  out:
	return (error);
}

VPSFUNC
static int
vps_restore_vmspace(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct proc *p, struct vmspace *orig_vmspace)
{
	struct vps_dumpobj *o1, *o2;
	struct vps_restore_obj *ro;
	struct vps_dump_vmspace *vdvms;
	struct vps_dump_vmmapentry *vdvme;
	struct vps_restore_obj *vbo;
	struct vmspace *ns;
	struct vm_map_entry *nme;
	struct vm_object *nvo;
	struct ucred *ncr;
	int cow;
	int error = 0;

	ncr = NULL;

	if (vdo_typeofnext(ctx) != VPS_DUMPOBJT_VMSPACE) {
		/* Lookup in list of restored file objects. */
		SLIST_FOREACH(ro, &ctx->obj_list, list)
			if (ro->type == VPS_DUMPOBJT_VMSPACE &&
			    ro->orig_ptr == orig_vmspace)
				break;
		if (ro == NULL) {
			ERRMSG(ctx, "%s: vmspace orig_ptr=%p not found !\n",
			    __func__, orig_vmspace);
			return (EINVAL);
		}
		p->p_vmspace = ro->new_ptr;
		atomic_add_int(&p->p_vmspace->vm_refcnt, 1);

		DBGR("%s: linked shared vmspace %p (orig %p) "
		    "to proc %p/%d\n",
		    __func__, p->p_vmspace, orig_vmspace, p, p->p_pid);

		return (0);
	}

	/* verified type. */
	o1 = vdo_next(ctx);

	vdvms = (struct vps_dump_vmspace *)o1->data;

	ns = vmspace_alloc(vdvms->vm_map.minoffset,
	     vdvms->vm_map.maxoffset);

	DBGR("%s: map=%p\n", __func__, &ns->vm_map);

	/* o1 --> vmspace */
	/* o2 --> map entry */

	while (vdo_typeofnext(ctx) == VPS_DUMPOBJT_VMMAPENTRY) {

		o2 = vdo_next(ctx);
		vdvme = (struct vps_dump_vmmapentry *)o2->data;

		nvo = NULL;

		DBGR("%s: vm_map_entry=%p: start=%p end=%p orig_obj=%p\n",
		    __func__, vdvme, PTRFROM64(vdvme->start),
		    PTRFROM64(vdvme->end), vdvme->map_object);

		if (vdvme->map_object == NULL) {

			/* No VM objects, next map entry follows. */

			DBGR("%s: no object\n", __func__);

			/* Move on to next map entry. */

		} else {

			/* Look if vm object is already restored
			   (shared memory). */
			SLIST_FOREACH(vbo, &ctx->obj_list, list)
				if (vbo->orig_ptr == vdvme->map_object) {
					nvo = vbo->new_ptr;
					break;
				}

			if (nvo) {
				/* Move on to next map entry. */
				DBGR("%s: found vm_object (shared memory): "
				    "orig=%p nvo=%p\n",
				    __func__, vdvme->map_object, nvo);
			}

		}

		if (vdvme->cred != NULL) {
			if (vdo_typeofnext(ctx) == VPS_DUMPOBJT_UCRED)
				vdo_next(ctx);
			ncr = vps_restore_ucred_lookup(ctx, vps,
			    vdvme->cred);
		} else
			ncr = NULL;
		DBGR("%s: ncr=%p\n", __func__, ncr);

		while (vdo_typeofnext(ctx) == VPS_DUMPOBJT_VMOBJECT) {
			if ((error = vps_restore_vmobject(ctx, vps, &nvo)))
				goto out;
		}

		/* This is the last vm object for this map entry,
		   so insert the entry. */
		cow = 0;

		DBGR("%s: entry (%016zx-%016zx, size=%016zx, prot=%02x, "
		    "max_prot=%02x, object=%p, offset=%016zx) eflags=%08x; "
		    "nvo=%p\n",
		    __func__, (size_t)vdvme->start, (size_t)vdvme->end,
		    (size_t)(vdvme->end - vdvme->start),
		    vdvme->protection, vdvme->max_protection,
		    vdvme->map_object, (size_t)vdvme->offset,
		    vdvme->eflags, nvo);

		if (nvo != NULL)
			vm_object_reference(nvo);

		vm_map_lock(&ns->vm_map);
		if ((error = vm_map_insert(&ns->vm_map, nvo, vdvme->offset,
		    vdvme->start, vdvme->end, vdvme->protection,
		    vdvme->max_protection, cow))) {
			ERRMSG(ctx, "%s: vm_map_insert(): error %d\n",
			    __func__, error);
			error = EINVAL;
			if (nvo)
				vm_object_deallocate(nvo);
			vm_map_unlock(&ns->vm_map);
			goto out;
		}

		vm_map_lookup_entry(&ns->vm_map, vdvme->start, &nme);
		nme->inheritance = vdvme->inheritance;
		if (vdvme->eflags & MAP_STACK_GROWS_DOWN)
			nme->eflags |= MAP_STACK_GROWS_DOWN;
		if (vdvme->eflags & MAP_STACK_GROWS_UP)
			nme->eflags |= MAP_STACK_GROWS_UP;
		if (vdvme->eflags & MAP_ENTRY_COW)
			nme->eflags |= MAP_ENTRY_COW;
		if (vdvme->eflags & MAP_ENTRY_NEEDS_COPY)
			nme->eflags |= MAP_ENTRY_NEEDS_COPY;
		if (vdvme->eflags & MAP_ENTRY_NOCOREDUMP)
			nme->eflags |= MAP_ENTRY_NOCOREDUMP;
		/* XXX audit this value */
		if (vdvme->avail_ssize > 0)
			nme->avail_ssize = vdvme->avail_ssize;

		if (nme->cred != NULL) {
			swap_release_by_cred(nme->end - nme->start,
			    nme->cred);
			crfree(nme->cred);
			nme->cred = NULL;
		}
		if (ncr != NULL) {
			nme->cred = crhold(ncr);
			swap_reserve_by_cred(nme->end - nme->start,
			    nme->cred);
		}

		vm_map_unlock(&ns->vm_map);

		if (ncr != NULL) {
			crfree(ncr);
			ncr = NULL;
		}

		/* Next map entry. */
	}


	ns->vm_tsize = vdvms->vm_tsize;
	ns->vm_dsize = vdvms->vm_dsize;
	ns->vm_ssize = vdvms->vm_ssize;

	/* Insert into restored objects list. */
	ro = malloc(sizeof(*ro), M_VPS_RESTORE, M_WAITOK | M_ZERO);
	ro->type = VPS_DUMPOBJT_VMSPACE;
	ro->orig_ptr = vdvms->vm_orig_ptr;
	ro->new_ptr = ns;
	SLIST_INSERT_HEAD(&ctx->obj_list, ro, list);

	DBGR("%s: restored vmspace orig=%p new=%p\n",
	    __func__, vdvms->vm_orig_ptr, ns);

	p->p_vmspace = ns;

  out:
	if (ncr != NULL) {
		crfree(ncr);
		ncr = NULL;
	}

	if (error) {
		ERRMSG(ctx, "%s: error = %d\n", __func__, error);
	}

	return (error);
}

VPSFUNC
static void
vps_restore_cleanup_vmspace(struct vps_snapst_ctx *ctx, struct vps *vps,
	struct _vps_restore_obj_list *obj_list)
{
	struct vps_restore_obj *obj, *obj2;

	SLIST_FOREACH_SAFE(obj, obj_list, list, obj2) {
		if (obj->type != VPS_DUMPOBJT_VMOBJECT)
			continue;
		if (obj->new_ptr)
			vm_object_deallocate(obj->new_ptr);
		/* Let the generic cleanup unlink and free the list item. */
	}
}

VPSFUNC
static int
vps_restore_thread_savefpu(struct vps_snapst_ctx *ctx, struct vps *vps,
	struct thread *td)
{

	return (vps_md_restore_thread_savefpu(ctx, vps, td));
}

VPSFUNC
static int
vps_restore_thread(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct proc *p)
{
	struct vps_dumpobj *o1;
	struct vps_dump_thread *vdtd;
	struct thread *ntd;
#if defined(VPS_ARCH_I386) || defined(VPS_ARCH_AMD64)
	void *pcb_save;
#endif
	int kstack_pages;
	int error = 0;
	int i;

	/* caller verified type */
	o1 = vdo_next(ctx);

	vdtd = (struct vps_dump_thread *)o1->data;

	DBGR("%s: old thread: tid=%d\n", __func__, vdtd->td_tid);

	kstack_pages = vdtd->td_kstack_pages;
	if (kstack_pages > KSTACK_MAX_PAGES) {
		ERRMSG(ctx, "%s: requested %d pages for kstack but system "
		    "maximum is %d\n",
		    __func__, kstack_pages, KSTACK_MAX_PAGES);
		error = EINVAL;
		goto out;
	}

	if ((ntd = thread_alloc(kstack_pages)) == NULL) {
		error = ENOMEM;
		goto out;
	}

	tidhash_add(ntd);

	memset(&ntd->td_startzero, 0,
	    __rangeof(struct thread, td_startzero, td_endzero));
	memset(&ntd->td_rux, 0, sizeof(ntd->td_rux));

	ntd->td_rqindex = vdtd->td_rqindex;
	ntd->td_base_pri = vdtd->td_base_pri;
	ntd->td_priority = vdtd->td_priority;
	ntd->td_pri_class = vdtd->td_pri_class;
	ntd->td_user_pri = vdtd->td_user_pri;
	ntd->td_base_user_pri = vdtd->td_base_user_pri;

	ntd->td_sigstk.ss_sp = PTRFROM64(vdtd->td_sigstk.ss_sp);
	ntd->td_sigstk.ss_size = vdtd->td_sigstk.ss_size;
	ntd->td_sigstk.ss_flags = vdtd->td_sigstk.ss_flags;
	ntd->td_xsig = vdtd->td_xsig;
	ntd->td_dbgflags = vdtd->td_dbgflags;
	for (i = 0; i < _SIG_WORDS; i++) {
		ntd->td_sigmask.__bits[i] = vdtd->td_sigmask[i];
		ntd->td_oldsigmask.__bits[i] = vdtd->td_oldsigmask[i];
	}

#if defined(VPS_ARCH_I386) || defined(VPS_ARCH_AMD64)
	/* Remember because it will be overwritten. */
	pcb_save = ntd->td_pcb->pcb_save;
#endif

	/* Restore kernel stack (this includes the PCB). */
	memcpy((char *)ntd->td_kstack, vdtd->td_kstack,
	    kstack_pages * PAGE_SIZE);

#if defined(VPS_ARCH_I386) || defined(VPS_ARCH_AMD64)
	ntd->td_pcb->pcb_save = pcb_save;
#endif

	/*
	 * XXX
	 * There are some registers/values restored along with the PCB
	 * that have to be audited !
	 */

	/*
	vps_md_print_thread(ntd);
	*/

	if (vdo_typeofnext(ctx) == VPS_DUMPOBJT_SAVEFPU) {
		if ((error = vps_restore_thread_savefpu(ctx, vps, ntd)))
			goto out;
	}

	error = vps_md_restore_thread(vdtd, ntd, p);
	if (error != 0)
		goto out;

	ntd->td_proc = p;
	ntd->td_ucred = crhold(p->p_ucred);
	ntd->td_vps = vps;
	ntd->td_vps_acc = vps->vps_acc;

	thread_link(ntd, p);

	/* not yet */
	if (vdo_typeofnext(ctx) == VPS_DUMPOBJT_UMTX)
		vdo_next(ctx);

	DBGR("%s: created thread=%p tid=%d\n",
	    __func__, ntd, ntd->td_tid);

  out:
	return (error);
}

VPSFUNC
static int
vps_restore_proc_one(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct vps_dumpobj *o1;
	struct vps_dump_proc *vdp;
	struct ucred *save_ucred;
	struct proc *np;
	struct vps *save_vps;
	struct thread *ntd;
	struct nameidata nd;
	caddr_t cpos;
	int error = 0;
	int i;

	/* calling functions make sure the next object is of type _PROC */
	o1 = vdo_next(ctx);

	DBGR("%s: o1=%p\n", __func__, o1);

	save_ucred = curthread->td_ucred;

	vdp = (struct vps_dump_proc *)o1->data;
	cpos = (caddr_t)(vdp + 1);

	/*
	DBGR("%s: dtd->td_tid=%d dtd->td_ucred=%p dp->p_ucred=%p\n",
	    __func__, dtd->td_tid, dtd->td_ucred, dp->p_ucred);
	*/

	/*
	 * We get a struct proc that already contains loads
	 * of resources, e.g. a thread ...
	 * So release them all first ...
	 * XXX p_stats
	 * XXX p_ksi
	 */

	np = uma_zalloc(proc_zone, M_WAITOK);
	if ((ntd = FIRST_THREAD_IN_PROC(np)) != NULL) {
		PROC_LOCK(np);
		thread_unlink(ntd);
		PROC_UNLOCK(np);
		thread_free(ntd);
	}

	bzero(&np->p_startzero, __rangeof(struct proc,
	    p_startzero, p_endzero));

	/* assemble proc */
	np->p_magic = P_MAGIC;
	np->p_pid = vdp->p_pid;
	np->p_swtick = vdp->p_swtick; /* XXX apply delta */

	/* sigacts */

	/* XXX checks:
	vdp->p_sigacts.ps_maxsig == _SIG_MAXSIG
	vdp->p_sigacts.ps_sigwords == _SIG_WORDS
	*/

	np->p_sigacts = malloc(sizeof(struct sigacts), M_SUBPROC,
	    M_WAITOK | M_ZERO);
	np->p_sigacts->ps_refcnt = 1;
	mtx_init(&np->p_sigacts->ps_mtx, "sigacts", NULL, MTX_DEF);

	np->p_sigacts->ps_flag = vdp->p_sigacts.ps_flag;
	for (i = 0; i < _SIG_MAXSIG; i++) {
		np->p_sigacts->ps_sigact[i] =
		    PTRFROM64(vdp->p_sigacts.ps_sigact[i]);
		np->p_sigacts->ps_catchmask[i].__bits[0] =
		    vdp->p_sigacts.ps_catchmask[i][0];
		np->p_sigacts->ps_catchmask[i].__bits[1] =
		    vdp->p_sigacts.ps_catchmask[i][1];
		np->p_sigacts->ps_catchmask[i].__bits[2] =
		    vdp->p_sigacts.ps_catchmask[i][2];
		np->p_sigacts->ps_catchmask[i].__bits[3] =
		    vdp->p_sigacts.ps_catchmask[i][3];
	}
	for (i = 0; i < _SIG_WORDS; i++) {
		np->p_sigacts->ps_sigonstack.__bits[i] =
		    vdp->p_sigacts.ps_sigonstack[i];
		np->p_sigacts->ps_sigintr.__bits[i] =
		    vdp->p_sigacts.ps_sigintr[i];
		np->p_sigacts->ps_sigreset.__bits[i] =
		    vdp->p_sigacts.ps_sigreset[i];
		np->p_sigacts->ps_signodefer.__bits[i] =
		    vdp->p_sigacts.ps_signodefer[i];
		np->p_sigacts->ps_siginfo.__bits[i] =
		    vdp->p_sigacts.ps_siginfo[i];
		np->p_sigacts->ps_sigignore.__bits[i] =
		    vdp->p_sigacts.ps_sigignore[i];
		np->p_sigacts->ps_sigcatch.__bits[i] =
		    vdp->p_sigacts.ps_sigcatch[i];
		np->p_sigacts->ps_freebsd4.__bits[i] =
		   vdp->p_sigacts.ps_freebsd4[i];
		np->p_sigacts->ps_osigset.__bits[i] =
		   vdp->p_sigacts.ps_osigset[i];
		np->p_sigacts->ps_usertramp.__bits[i] =
		   vdp->p_sigacts.ps_usertramp[i];
	}

	/* plimit */
	/* XXX check: vdp->p_limit.pl_nlimits == RLIM_NLIMITS */

	np->p_limit = lim_alloc();

	for (i = 0; i < RLIM_NLIMITS; i++) {
		np->p_limit->pl_rlimit[i].rlim_cur =
		    vdp->p_limit.pl_rlimit[i].rlim_cur;
		np->p_limit->pl_rlimit[i].rlim_max =
		    vdp->p_limit.pl_rlimit[i].rlim_max;
	}

	/* --- */
	np->p_cpulimit = vdp->p_cpulimit;

	knlist_init(&np->p_klist, &np->p_mtx, NULL, NULL, NULL, NULL);
	STAILQ_INIT(&np->p_ktr);
	strlcpy(np->p_comm, vdp->p_comm, sizeof(np->p_comm));

	np->p_flag = vdp->p_flag;
	np->p_stops = vdp->p_stops;
	np->p_oppid = vdp->p_oppid;
	np->p_xstat = vdp->p_xstat;
	np->p_sigparent = vdp->p_sigparent;
	/* XXX */
	np->p_stype = vdp->p_stype;
	np->p_step = vdp->p_step;
	np->p_args = NULL;

	DBGR("%s: pid=%d p_flag=%08x p_oppid=%d\n",
	    __func__, np->p_pid, np->p_flag, np->p_oppid);

	/* --> from proc_linkup() */
	sigqueue_init(&np->p_sigqueue, np);
	np->p_ksi = ksiginfo_alloc(1);
	np->p_ksi->ksi_flags = KSI_EXT | KSI_INS;
	LIST_INIT(&np->p_mqnotifier);
	np->p_numthreads = 0;

	/* ucred */
	if (vdo_typeofnext(ctx) == VPS_DUMPOBJT_UCRED)
		vdo_next(ctx);
	np->p_ucred = vps_restore_ucred_lookup(ctx, vps, vdp->p_ucred);
	KASSERT(np->p_ucred != NULL,
	    ("%s: np->p_ucred == NULL\n", __func__));
	(void)chgproccnt(np->p_ucred->cr_ruidinfo, 1, 0);
	curthread->td_ucred = np->p_ucred;
	prison_proc_hold(np->p_ucred->cr_prison);

	/* sysentvec */
	if ((error = vps_restore_sysentvec(ctx, vps, np)))
		goto out;

	/* ktrace */
	if (vdp->p_have_tracevp) {
		if ((error = vps_restore_vnode(ctx, vps,
		    &np->p_tracevp)))
			goto out;
		/*
		DBGR("%s: p_tracevp: path [%s] got vnode %p\n",
		    __func__, o2->data, np->p_tracevp);
		*/

		/* XXX - could be different than p->p_ucred */
		if (vdp->p_tracecred != NULL)
			np->p_tracecred = crhold(np->p_ucred);

		np->p_traceflag = vdp->p_traceflag;
	}

	callout_init(&np->p_itcallout, CALLOUT_MPSAFE);

	/* textvp */
	if (vdp->p_have_textvp) {
		if ((error = vps_restore_vnode(ctx, vps,
		    &np->p_textvp)))
			goto out;
		DBGR("%s: p_textvp: path [...] got vnode %p\n",
		    __func__, np->p_textvp);
	}

	/* vmspace */
	if ((error = vps_restore_vmspace(ctx, vps, np, vdp->p_vmspace)))
		goto out;

	TAILQ_INIT(&np->p_threads);
	while (vdo_typeofnext(ctx) == VPS_DUMPOBJT_THREAD) {
		if ((error = vps_restore_thread(ctx, vps, np)))
			goto out;
		vps_account(vps, VPS_ACC_THREADS, VPS_ACC_ALLOC, 1);
	}
	/* XXX lookup by id */
	/*
	if (vdp->p_xthread_id != 0)
		np->p_xthread = FIRST_THREAD_IN_PROC(np);
	*/

	vps_account(vps, VPS_ACC_PROCS, VPS_ACC_ALLOC, 1);

	if ((error = vps_restore_fdset(ctx, vps, np, vdp->p_fd)))
		goto out;

	while (vdo_nextischild(ctx, o1)) {

		switch (vdo_typeofnext(ctx)) {
		case VPS_DUMPOBJT_PARGS:
			if ((error = vps_restore_pargs(ctx, vps, np)))
				goto out;
			break;
		case VPS_DUMPOBJT_SYSVSEM_PROC:
			if (vps_func->sem_restore_proc && (error =
			    vps_func->sem_restore_proc(ctx, vps, np)))
				goto out;
			break;
		case VPS_DUMPOBJT_SYSVSHM_PROC:
			if (vps_func->shm_restore_proc && (error =
			    vps_func->shm_restore_proc(ctx, vps, np)))
				goto out;
			break;
		case VPS_DUMPOBJT_SYSVMSG_PROC:
			if (vps_func->msg_restore_proc && (error =
			    vps_func->msg_restore_proc(ctx, vps, np)))
				goto out;
			break;
		default:
			DBGR("%s: unknown type=%d\n",
			    __func__, vdo_typeofnext(ctx));
			break;
		}

	}

	/* proc tree */
	save_vps = curthread->td_vps;
	curthread->td_vps = vps;

	sx_xlock(&VPS_VPS(vps, allproc_lock));
	sx_xlock(&VPS_VPS(vps, proctree_lock));

	LIST_INSERT_HEAD(&VPS_VPS(vps, allproc), np, p_list);
	LIST_INSERT_HEAD(&VPS_VPS(vps, pidhashtbl)[(np->p_pid) &
		VPS_VPS(vps, pidhash)], np, p_hash);
	VPS_VPS(vps, nprocs)++;

	/* These are ids rather than pointers. */
	np->p_pptr = (struct proc *)((size_t)vdp->p_pptr_id);
	np->p_peers = (struct proc *)((size_t)vdp->p_peers_id);
	np->p_leader = (struct proc *)((size_t)vdp->p_leader_id);
	np->p_pgrp = (struct pgrp *)((size_t)vdp->p_pgrp_id);

	sx_xunlock(&VPS_VPS(vps, proctree_lock));
	sx_xunlock(&VPS_VPS(vps, allproc_lock));

	if (TAILQ_EMPTY(&np->p_threads)) {
		ERRMSG(ctx, "%s: process has no threads !\n", __func__);
		error = 0;
		goto out;
	}
	np->p_state = vdp->p_state;

	/* Add the extra lock that was set in vps_suspend(). */
	np->p_lock++;

	PROC_LOCK(np);
	FOREACH_THREAD_IN_PROC(np, ntd) {

		/* Sets scheduler lock and cpuset, besides some
		   other stuff. */
		thread_lock(curthread);
		sched_fork_thread(curthread, ntd);
		thread_unlock(curthread);
		np->p_suspcount++;
		TD_SET_SUSPENDED(ntd);
	}

	PROC_UNLOCK(np);

	/* Enable ktrace for debugging. */
	if (debug_restore_ktrace) {
		int error1, flags;
		struct vnode *vp;

		NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_SYSSPACE,
			"/tmp/ktrace-all.out", curthread);
		flags = FREAD | FWRITE | O_NOFOLLOW;
		error1 = vn_open(&nd, &flags, 0, NULL);
		if (error1 == 0) {
			NDFREE(&nd, NDF_ONLY_PNBUF);
			vp = nd.ni_vp;
			VOP_UNLOCK(vp, 0);
			/* ktrops() unlocks proc */
			PROC_LOCK(np);
			curthread->td_pflags |= TDP_INKTRACE;
			ktrops(curthread, np, KTROP_SET,
			    0xffffffff & ~KTRFAC_CSW, vp);
			curthread->td_pflags &= ~TDP_INKTRACE;
			(void)vn_close(vp, FWRITE, curthread->td_ucred,
			    curthread);
		} else
			ERRMSG(ctx, "%s: ktrace / vn_open error: %d\n",
			    __func__, error1);
	}

  out:
	curthread->td_ucred = save_ucred;

	if (error)
		ERRMSG(ctx, "%s: error = %d\n", __func__, error);
	return (error);
}

VPSFUNC
static int
vps_restore_proc_session(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct vps_dumpobj *o1;
	struct vps_dump_session *vdsess;
	struct vps_dump_filepath *vdfp;
	struct session *nsess;
	struct vps_restore_obj *ro;
	struct nameidata nd;
	int error = 0;

	o1 = vdo_next(ctx);

	vdsess = (struct vps_dump_session *)o1->data;
	nsess = malloc(sizeof(*nsess), M_SESSION, M_WAITOK | M_ZERO);
	mtx_init(&nsess->s_mtx, "session", NULL, MTX_DEF | MTX_DUPOK);
	nsess->s_count = vdsess->s_count;
	/* This is a pid, fixup later. */
	nsess->s_ttyvp = NULL;
	nsess->s_ttyp = NULL;
	nsess->s_sid = vdsess->s_sid;
	memcpy(nsess->s_login, vdsess->s_login, sizeof(nsess->s_login));

	if (vdsess->s_have_ttyvp && vdo_typeofnext(ctx) ==
	    VPS_DUMPOBJT_FILE_PATH) {

		o1 = vdo_next(ctx);
		vdfp = (struct vps_dump_filepath *)o1->data;
		if ((strncmp(vdfp->fp_path, "/dev/pts/", 9)) == 0) {
			struct posix_openpt_args args;
			int unit;

			args.flags = O_RDWR;
			if ((sscanf(vdfp->fp_path, "/dev/pts/%d", &unit))
			    != 1) {
				ERRMSG(ctx, "%s: unable to find unit "
				    "number in pts name [%s]\n",
				    __func__, vdfp->fp_path);
				error = EINVAL;
				goto out;
			}
			DBGR("%s: pts unit number=%d\n", __func__, unit);
			if ((error = sys_posix_openpt_unit(curthread, &args,
					unit))) {
				ERRMSG(ctx, "%s: sys_posix_openpt_unit() "
				    "error: %d\n", __func__, error);
				goto out;
			}
			/* is used later in vps_restore_fdset */
		}
		NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF,
		    UIO_SYSSPACE, vdfp->fp_path, curthread);
		if ((error = namei(&nd))) {
			ERRMSG(ctx, "%s: namei([%s]): error = %d\n",
			    __func__, vdfp->fp_path, error);
			goto out;
		}
		/* XXX VREF() ?! */
		nsess->s_ttyvp = nd.ni_vp;
		VOP_UNLOCK(nd.ni_vp, 0);
		NDFREE(&nd, NDF_ONLY_PNBUF);
		DBGR("%s: path [%s] got vnode %p\n",
		    __func__, vdfp->fp_path, nd.ni_vp);

		if ( ! (nsess->s_ttyvp->v_type == VCHR &&
		    nsess->s_ttyvp->v_rdev)) {
			ERRMSG(ctx, "%s: not a device !\n", __func__);
			error = EINVAL;
			goto out;
		}

		nsess->s_ttyp = nsess->s_ttyvp->v_rdev->si_drv1;

	}

	ro = malloc(sizeof(*ro), M_VPS_RESTORE, M_WAITOK | M_ZERO);

	ro->type = VPS_DUMPOBJT_SESSION;
	ro->new_ptr = nsess;
	ro->orig_id = nsess->s_sid;
	ro->spare[0] = (void *)(size_t)vdsess->s_leader_id;

	SLIST_INSERT_HEAD(&ctx->obj_list, ro, list);

	DBGR("%s: restored session %p/%d ttyvp=%p ttyp=%p\n",
	    __func__, nsess, nsess->s_sid,
	    nsess->s_ttyvp, nsess->s_ttyp);

  out:
	return (error);
}

VPSFUNC
static int
vps_restore_proc(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct vps_dumpobj *o1;
	struct vps_dump_pgrp *vdpg;
	struct pgrp *npg, *pg;
	struct session *nsess;
	struct proc *p;
	struct vps_restore_obj *ro, *ro1;
	struct vps *save_vps;
	struct ucred *save_ucred;
	struct vnode *saverootvnode;
	struct tty *ttyp;
	int found;
	int error = 0;

	/* XXX See comment in vps_restore_mounts(). */
	save_vps = curthread->td_vps;
	save_ucred = curthread->td_ucred;

	curthread->td_vps = vps;
	curthread->td_ucred = ctx->vps_ucred;

	/*
	 * Temporarily change our root directory to the one of the vps
	 * instance to be restored.
	 * Makes namei() lookups much easier.
	 */
	FILEDESC_XLOCK(curthread->td_proc->p_fd);
	VREF(vps->_rootvnode);
	saverootvnode = curthread->td_proc->p_fd->fd_rdir;
	curthread->td_proc->p_fd->fd_rdir = vps->_rootvnode;
	FILEDESC_XUNLOCK(curthread->td_proc->p_fd);


	while (vdo_typeofnext(ctx) == VPS_DUMPOBJT_PGRP) {

		o1 = vdo_next(ctx);

		ro = malloc(sizeof(*ro), M_VPS_RESTORE, M_WAITOK | M_ZERO);

		vdpg = (struct vps_dump_pgrp *)o1->data;
		npg = malloc(sizeof(*npg), M_PGRP, M_WAITOK | M_ZERO);
		mtx_init(&npg->pg_mtx, "process group", NULL,
		    MTX_DEF | MTX_DUPOK);
		LIST_INIT(&npg->pg_members);
		npg->pg_id = vdpg->pg_id;
		npg->pg_jobc = vdpg->pg_jobc;
		/* This is the session id, fixup later. */
		npg->pg_session = NULL;
		/* XXX Restore this stuff too. */
		SLIST_INIT(&npg->pg_sigiolst);

		ro->type = VPS_DUMPOBJT_PGRP;
		ro->new_ptr = npg;
		ro->orig_id = npg->pg_id;
		ro->spare[0] = (void *)(size_t)vdpg->pg_session_id;

		SLIST_INSERT_HEAD(&ctx->obj_list, ro, list);

		DBGR("%s: restored pgrp %p/%d\n",
		    __func__, npg, npg->pg_id);

		if (vdo_typeofnext(ctx) == VPS_DUMPOBJT_SESSION) {
			error = vps_restore_proc_session(ctx, vps);
			if (error != 0)
				goto out;

		}
	}

	while (vdo_typeofnext(ctx) == VPS_DUMPOBJT_PROC) {

		if ((error = vps_restore_proc_one(ctx, vps)))
			goto out;

	}

	/* Now fixup stuff like the proc tree. */
	curthread->td_vps = vps;

	/* Fix up sessions. */
	SLIST_FOREACH(ro, &ctx->obj_list, list)
		if (ro->type == VPS_DUMPOBJT_SESSION) {
			nsess = (struct session *)ro->new_ptr;
			if ((nsess->s_leader = pfind((size_t)ro->spare[0])))
				PROC_UNLOCK(nsess->s_leader);
			DBGR("%s: fixed up session=%p s_leader=%p/%d\n",
			    __func__, nsess, nsess->s_leader,
			    nsess->s_leader ? nsess->s_leader->p_pid : -1);
			/* XXX check if every pointer is fixed up now. */
		}

	/* For every pgrp, lookup session. */
	SLIST_FOREACH(ro, &ctx->obj_list, list)
		if (ro->type == VPS_DUMPOBJT_PGRP) {
			pg = ro->new_ptr;
			found = 0;
			SLIST_FOREACH(ro1, &ctx->obj_list, list)
				if (ro1->type == VPS_DUMPOBJT_SESSION &&
				    ro1->orig_id == (size_t)ro->spare[0]) {
					DBGR("%s: found session %p/%d for "
					    "pgrp %p/%d\n", __func__,
					    ro1->new_ptr, ro1->orig_id,
					    pg, pg->pg_id);
					pg->pg_session = ro1->new_ptr;
					found = 1;
					/* XXX check if every pointer is
					   fixed up now. */
				}
			KASSERT(found == 1, ("%s: no session found for "
			    "pgrp=%p/%d\n", __func__, pg, pg->pg_id));
			LIST_INSERT_HEAD(&VPS_VPS(vps, pgrphashtbl)
			    [pg->pg_id & VPS_VPS(vps, pgrphash)],
			    pg, pg_hash);
			DBGR("%s: inserted pgrp %p/%d\n",
			    __func__, pg, pg->pg_id);
		}

	/* Fix up ttys. */
	sx_slock(&VPS_VPS(vps, proctree_lock));
	SLIST_FOREACH(ro, &ctx->obj_list, list) {
		struct tty *tp;
		if (ro->type != VPS_DUMPOBJT_PTS)
			continue;
		tp = ro->new_ptr;
		DBGR("%s: tp=%p tp->t_pgrp = %zu\n",
		    __func__, tp, (intptr_t)tp->t_pgrp);
		tp->t_pgrp = pgfind((intptr_t)tp->t_pgrp);
		if (tp->t_pgrp)
			PGRP_UNLOCK(tp->t_pgrp);
		DBGR("%s: tp=%p tp->t_pgrp = %p\n",
		    __func__, tp, tp->t_pgrp);
	}
	sx_sunlock(&VPS_VPS(vps, proctree_lock));

	/* Fix up unix domain sockets. */
	if ((error = vps_restore_fixup_unixsockets(ctx, vps)))
		goto out;

	/* Traverse V_allproc, lookup pgrp for each one. */
	sx_xlock(&VPS_VPS(vps, proctree_lock));
	LIST_FOREACH(p, &VPS_VPS(vps, allproc), p_list) {
		if ((pg = pgfind((intptr_t) p->p_pgrp)))
			PGRP_UNLOCK(pg);
		DBGR("%s: pgfind: pg=%p/%zu\n",
		    __func__, pg, (intptr_t)p->p_pgrp);
		/* XXX check NULL */
		//PROC_LOCK(p);
		p->p_pgrp = pg;
		LIST_INSERT_HEAD(&pg->pg_members, p, p_pglist);
		/* XXX not needed fixjobc(p, p->p_pgrp, 1); */
		if ((p->p_peers = pfind((intptr_t)p->p_peers)))
			PROC_UNLOCK(p->p_peers);
		if ((p->p_leader = pfind((intptr_t)p->p_leader)))
			PROC_UNLOCK (p->p_leader);
		if ((p->p_pptr = pfind((intptr_t)p->p_pptr)))
			PROC_UNLOCK(p->p_pptr);
		if (p->p_pptr)
			LIST_INSERT_HEAD(&p->p_pptr->p_children, p,
			    p_sibling);
		if (p->p_pgrp->pg_session && SESS_LEADER(p) &&
		    p->p_session->s_ttyp) {
			ttyp = p->p_session->s_ttyp;
			tty_lock(ttyp);
			ttyp->t_session = p->p_session;
			ttyp->t_sessioncnt++;
			tty_unlock(ttyp);
			DBGR("%s: set controlling session %p for ttyp %p\n",
			    __func__, p->p_session, p->p_session->s_ttyp);
		}
		//PROC_UNLOCK(p);
		DBGR("%s: fixed up proc=%p/%d p->p_pgrp=%p p->p_session=%p "
		    "p->p_pptr=%p p->p_stype=%08x\n",
		    __func__, p, p->p_pid, p->p_pgrp,
		    p->p_session, p->p_pptr, p->p_stype);
	}

	if ((VPS_VPS(vps, initpgrp) = pgfind((intptr_t)VPS_VPS(vps,
	    initpgrp))))
		PGRP_UNLOCK(VPS_VPS(vps, initpgrp));
	/* XXX pfind() locks allproc_lock again ! */
	if ((VPS_VPS(vps, initproc) = pfind((intptr_t)VPS_VPS(vps,
	    initproc))))
		PROC_UNLOCK(VPS_VPS(vps, initproc));

	DBGR("%s: V_initpgrp=%p V_initproc=%p\n",
	    __func__, VPS_VPS(vps, initpgrp), VPS_VPS(vps, initproc));

	/* debug */
	if (1) {
		if (PTRTO64(VPS_VPS(vps, initpgrp)) < 0x1000 ||
		    PTRTO64(VPS_VPS(vps, initproc)) < 0x1000) {
			ERRMSG(ctx, "%s: STOP\n", __func__);
			return (EINVAL);
		}
	}

	sx_xunlock(&VPS_VPS(vps, proctree_lock));

  out:
	FILEDESC_XLOCK(curthread->td_proc->p_fd);
	curthread->td_proc->p_fd->fd_rdir = saverootvnode;
	vrele(vps->_rootvnode);
	FILEDESC_XUNLOCK(curthread->td_proc->p_fd);

	curthread->td_vps = save_vps;
	curthread->td_ucred = save_ucred;

	if (error)
		ERRMSG(ctx, "%s: error = %d\n", __func__, error);
	return (error);
}

VPSFUNC
static int
vps_restore_arg(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct vps_dumpobj *o1;
	struct vps_dump_arg *vda;
	struct vps_dump_arg_ip4 *vda4;
	struct vps_dump_arg_ip6 *vda6;
	struct vps_dump_accounting *vdacc;
	size_t privsetsize_round;
	caddr_t cpos;
	int len;
	int i;

	o1 = vdo_next(ctx);

	if (o1->type != VPS_DUMPOBJT_ARG) {
		ERRMSG(ctx, "%s: VPS_DUMPOBJT_ARG missing !\n", __func__);
		return (-1);
	}

	vda = (struct vps_dump_arg *)o1->data;
	cpos = o1->data + sizeof(*vda);

	if (vda->privset_size != PRIV_SET_SIZE) {
		ERRMSG(ctx, "%s: vda->privset_size (%d) != "
		    "PRIV_SET_SIZE (%d)\n",
		    __func__, vda->privset_size, PRIV_SET_SIZE);
		return (EINVAL);
	}

	/*
	 * XXX We must check here if the current vps is allowed
	 *     the privs and networks etc. that we are about to restore.
	 *     (In case of nested vps instances.)
	 */
	privsetsize_round = roundup(vda->privset_size, 8);
	len = vda->privset_size > PRIV_SET_SIZE ? PRIV_SET_SIZE :
	    vda->privset_size;

	memcpy(vps->priv_allow_set, cpos, len);
	cpos += privsetsize_round;

	memcpy(vps->priv_impl_set, cpos, len);
	cpos += privsetsize_round;

	if (vda->ip4net_cnt > 0) {

		len = sizeof(struct vps_arg_ip4) * vda->ip4net_cnt;
		vps->vps_ip4 = malloc(len, M_VPS_CORE, M_WAITOK);

		for (i = 0; i < vda->ip4net_cnt; i++) {
			vda4 = (struct vps_dump_arg_ip4 *)cpos;
			memcpy(&vps->vps_ip4[i].addr, vda4->a4_addr, 0x4);
			memcpy(&vps->vps_ip4[i].mask, vda4->a4_mask, 0x4);
			cpos = (caddr_t)(vda4 + 1);
		}

		vps->vps_ip4_cnt = vda->ip4net_cnt;
	}

	if (vda->ip6net_cnt > 0) {

		len = sizeof(struct vps_arg_ip6) * vda->ip6net_cnt;
		vps->vps_ip6 = malloc(len, M_VPS_CORE, M_WAITOK);

		for (i = 0; i < vda->ip6net_cnt; i++) {
			vda6 = (struct vps_dump_arg_ip6 *)cpos;
			memcpy(&vps->vps_ip6[i].addr, vda6->a6_addr, 0x10);
			vps->vps_ip6[i].plen = vda6->a6_plen;
			cpos = (caddr_t)(vda6 + 1);
		}

		vps->vps_ip6_cnt = vda->ip6net_cnt;
	}


	if (vda->have_accounting) {

		vdacc = (struct vps_dump_accounting *)cpos;

#define FILL_ACCVAL(x)  					\
        vps->vps_acc->x.soft = vdacc->x.soft;			\
        vps->vps_acc->x.hard = vdacc->x.hard;			\
        vps->vps_acc->x.hits_soft = vdacc->x.hits_soft;		\
        vps->vps_acc->x.hits_hard = vdacc->x.hits_hard

		FILL_ACCVAL(virt);
		FILL_ACCVAL(phys);
		FILL_ACCVAL(kmem);
		FILL_ACCVAL(kernel);
		FILL_ACCVAL(buffer);
		FILL_ACCVAL(pctcpu);
		FILL_ACCVAL(blockio);
		FILL_ACCVAL(threads);
		FILL_ACCVAL(procs);

#undef FILL_ACCVAL
	}

	return (0);
}

VPSFUNC
static int
vps_restore_mounts(struct vps_snapst_ctx *ctx, struct vps *vps,
    char *rootfspath)
{
	struct vps_dump_mount_opt *dvmopt;
	struct vps_dump_mount *dvm;
	struct vps_dumpobj *o1;
	struct vps *savevps;
	struct ucred *saveucred, *ncr;
	struct vnode *save_rdir, *save_cdir;
	struct mntarg *ma;
	int errmsg_len;
	char *fspath;
	char *errmsg;
	int error = 0;
	int i;

	ncr = NULL;

	if (vps != NULL) {
		savevps = curthread->td_vps;
		saveucred = curthread->td_ucred;
		save_rdir = curthread->td_proc->p_fd->fd_rdir;
		save_cdir = curthread->td_proc->p_fd->fd_cdir;
	} else {
		savevps = NULL;
		saveucred = NULL;
		save_rdir = save_cdir = NULL;
	}

	errmsg_len = 0xf0;
	errmsg = malloc(errmsg_len, M_TEMP, M_WAITOK);

	while (vdo_typeofnext(ctx) == VPS_DUMPOBJT_MOUNT) {

		o1 = vdo_next(ctx);

		dvm = (struct vps_dump_mount *)o1->data;

		ncr = NULL;

		/* Only do root fs mount now. */
		if (vps == NULL && strcmp(dvm->mnton, rootfspath))
			continue;

		/* Do all mounts now except root fs. */
		if (vps != NULL && !strcmp(dvm->mnton, rootfspath))
			continue;

		ma = NULL;

		if (dvm->optcnt * sizeof(*dvmopt) >
		    (o1->size - sizeof(*dvm))) {
			ERRMSG(ctx, "%s: dvm->optcnt=%d seems invalid !\n",
			    __func__, dvm->optcnt);
			error = EINVAL;
			goto out;
		}
		dvmopt = (struct vps_dump_mount_opt *)(dvm+1);
		for (i = 0; i < dvm->optcnt; i++) {
			DBGR("%s: opt name=[%s] value=%p len=%u\n",
			    __func__, dvmopt->name, dvmopt->value,
			    dvmopt->len);

			if (!strcmp(dvm->fstype, "nfs")) {
				/*
				if (!strcmp(dvmopt->name, "addr") ||
				    !strcmp(dvmopt->name, "fh") ||
				    !strcmp(dvmopt->name, "hostname"))
				*/
				if (1)
					ma = mount_arg(ma, dvmopt->name,
					    dvmopt->value, dvmopt->len);
			}
			dvmopt += 1;
		}

		if (vdo_typeofnext(ctx) == VPS_DUMPOBJT_UCRED) {
			vdo_next(ctx);
		}

		DBGR("dvm=%p mntfrom=[%s] mnton=[%s] fstype=[%s] flags=%zx "
		    "vpsmount=%d\n", dvm, dvm->mntfrom, dvm->mnton,
		    dvm->fstype, (size_t)dvm->flags, dvm->vpsmount);

		if (vps != NULL && dvm->vpsmount) {
			curthread->td_vps = vps;
			curthread->td_proc->p_fd->fd_rdir = vps->_rootvnode;
			curthread->td_proc->p_fd->fd_cdir = vps->_rootvnode;

			/* Note that ucred gets duplicated in
			   vfs_mount_alloc(). */

			if (dvm->mnt_cred != NULL) {
				ncr = vps_restore_ucred_lookup(ctx, vps,
				    dvm->mnt_cred);
				if (ncr == NULL) {
					ERRMSG(ctx, "%s: ucred not found "
					    "!\n", __func__);
					error = EINVAL;
					goto out;
				}
				curthread->td_ucred = ncr;
			}
		}

		if (dvm->vpsmount)
			/* dvm->mnton is always absolute, so we have to
			   strip it. */
			fspath = dvm->mnton + strlen(rootfspath);
		else
			fspath = dvm->mnton;
		/*DBGR("%s: fspath=[%s]\n", __func__, fspath);*/

		ma = mount_arg(ma, "fstype", dvm->fstype, -1);
		ma = mount_arg(ma, "fspath", fspath, -1);
		if (!strcmp(dvm->fstype, "nullfs") ||
		    !strcmp(dvm->fstype, "vpsfs"))
			ma = mount_arg(ma, "target", dvm->mntfrom, -1);
		ma = mount_arg(ma, "from", dvm->mntfrom, -1);
		ma = mount_arg(ma, "errmsg", errmsg, errmsg_len);
		memset(errmsg, 0, errmsg_len);

		error = kernel_mount(ma, dvm->flags);
		if (error) {
			ERRMSG(ctx, "%s: kernel_mount() error: %d [%s]\n",
			    __func__, error, errmsg);
			goto out;
		}

		if (vps != NULL && dvm->vpsmount) {
			curthread->td_vps = savevps;
			curthread->td_ucred = saveucred;
			curthread->td_proc->p_fd->fd_rdir = save_rdir;
			curthread->td_proc->p_fd->fd_cdir = save_cdir;

			if (ncr != NULL)
				crfree(ncr);
		}
	}

  out:
	free(errmsg, M_TEMP);

	return (error);
}

VPSFUNC
static void
vps_restore_prison_fixup(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct vps_restore_obj *obj1;
	struct prison *pr;

	SLIST_FOREACH(obj1, &ctx->obj_list, list) {
		if (obj1->type != VPS_DUMPOBJT_PRISON)
			continue;
		pr = (struct prison *)obj1->new_ptr;

		/* Clear the extra references that we had while
		   restoring. */
		prison_lock(pr);
		pr->pr_ref--;
		pr->pr_uref--;
		prison_unlock(pr);
	}
}

VPSFUNC
static struct prison *
vps_restore_prison_lookup(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct prison *old_pr)
{
	struct vps_restore_obj *obj1;
	struct prison *new_pr;

	new_pr = NULL;
	SLIST_FOREACH(obj1, &ctx->obj_list, list) {
		if (obj1->type != VPS_DUMPOBJT_PRISON)
			continue;
		if (obj1->orig_ptr == old_pr) {
			new_pr = obj1->new_ptr;
			DBGR("%s: found new prison ptr: orig=%p new=%p\n",
			    __func__, obj1->orig_ptr, obj1->new_ptr);
			break;
		}
	}
	KASSERT(new_pr != NULL,
	    ("%s: old_pr=%p new_pr==NULL\n", __func__, old_pr));

	return (new_pr);
}

VPSFUNC
static int
vps_restore_prison_one(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct prison *npr, *ppr, *tpr;
	struct vps_dumpobj *o1;
	struct vps_dump_prison *vdpr;
	struct vps_restore_obj *ro;
	caddr_t cpos;
	int error = 0;
	int i;

	o1 = vdo_next(ctx);

	KASSERT(o1->type == VPS_DUMPOBJT_PRISON,
		("%s: o1=%p o1->type = %d\n", __func__, o1, o1->type));

	vdpr = (struct vps_dump_prison *)o1->data;

	DBGR("%s: orig_ptr=%p id=%d name=[%s]\n",
	    __func__, vdpr->pr_origptr, vdpr->pr_id, vdpr->pr_name);

	if (vdpr->pr_parent != NULL) {

		npr = malloc(sizeof(*npr), M_PRISON, M_WAITOK|M_ZERO);
		LIST_INIT(&npr->pr_children);
		mtx_init(&npr->pr_mtx, "jail mutex", NULL,
		    MTX_DEF | MTX_DUPOK);
		npr->pr_id = vdpr->pr_id;
		strlcpy(npr->pr_name, vdpr->pr_name, sizeof(npr->pr_name));
		/*
		 * We copy this from dumped one, but root vnode must be
		 * restored from old vnode, since the path might have
		 * been moved !
		 */
		strlcpy(npr->pr_path, vdpr->pr_path, sizeof(vdpr->pr_path));

		if ((error = vps_restore_vnode(ctx, vps,
		    &npr->pr_root)))
			return (error);

		npr->pr_parent = vps_restore_prison_lookup(ctx, vps,
		    vdpr->pr_parent);
		ppr = npr->pr_parent;

		ppr->pr_ref++;
		ppr->pr_uref++;

		if (vdpr->pr_flags & PR_PERSIST) {
			npr->pr_flags |= PR_PERSIST;
			npr->pr_ref++;
			npr->pr_uref++;
		}

		/* We free them after processes are attached. */
		npr->pr_ref++;
		npr->pr_uref++;

		/* XXX */
		cpuset_create_root(ppr, &npr->pr_cpuset);

		/* XXX */
		npr->pr_flags = vdpr->pr_flags;

		/* XXX check against parent's values */
		npr->pr_securelevel = vdpr->pr_securelevel;
		npr->pr_childmax = vdpr->pr_childmax;

		npr->pr_allow = vdpr->pr_allow;
		npr->pr_enforce_statfs = vdpr->pr_enforce_statfs;

		npr->pr_ip4s = vdpr->pr_ip4s;
		npr->pr_ip6s = vdpr->pr_ip6s;

		npr->pr_ip4 = malloc(sizeof(npr->pr_ip4[0]) * npr->pr_ip4s,
				M_PRISON, M_WAITOK);
		npr->pr_ip6 = malloc(sizeof(npr->pr_ip6[0]) * npr->pr_ip6s,
				M_PRISON, M_WAITOK);

		cpos = vdpr->pr_ipdata;

		/*
		vps_print_ascii(vdpr->pr_ipdata,
		    roundup(vdpr->pr_ip4s * 0x4, 8) +
		    vdpr->pr_ip6s * 0x10);
		*/

		for (i = 0; i < vdpr->pr_ip4s; i++) {
			memcpy(&npr->pr_ip4[i], cpos, 0x4);
			cpos += 0x4;
		}
		cpos = (caddr_t)roundup((size_t)cpos, 8);

		for (i = 0; i < vdpr->pr_ip6s; i++) {
			memcpy(&npr->pr_ip6[i], cpos, 0x10);
			cpos += 0x10;
		}

		if (vdpr->pr_flags & PR_VNET) {
			npr->pr_flags |= PR_VNET;
			if ((error = vps_restore_vnet(ctx, vps,
			    &npr->pr_vnet))) {
				ERRMSG(ctx, "%s: vps_restore_vnet(): %d\n",
				    __func__, error);
				return (error);
			}
			DBGR("%s: PR_VNET: npr->pr_vnet = %p\n",
			    __func__, npr->pr_vnet);
		} else {
			npr->pr_vnet = ppr->pr_vnet;
			DBGR("%s: inherit vnet: npr->pr_vnet = %p\n",
			    __func__, npr->pr_vnet);
		}

		TAILQ_FOREACH(tpr, &VPS_VPS(vps, allprison), pr_list)
			if (tpr->pr_id >= npr->pr_id) {
				TAILQ_INSERT_BEFORE(tpr, npr, pr_list);
				break;
			}
		if (tpr == NULL)
			TAILQ_INSERT_TAIL(&VPS_VPS(vps, allprison), npr,
			    pr_list);
		LIST_INSERT_HEAD(&ppr->pr_children, npr, pr_sibling);
			for (tpr = ppr; tpr != NULL; tpr = tpr->pr_parent)
				tpr->pr_childcount++;

	} else {
		/* this is dumped prison0 */

		npr = VPS_VPS(vps, prison0);

		/* Skip over filepath object. */
		vdo_next(ctx);
	}

	ro = malloc(sizeof(*ro), M_VPS_RESTORE, M_WAITOK|M_ZERO);
	ro->type = VPS_DUMPOBJT_PRISON;
	ro->orig_ptr = vdpr->pr_origptr;
	ro->new_ptr = npr;

	SLIST_INSERT_HEAD(&ctx->obj_list, ro, list);

	return (error);
}

VPSFUNC
static int
vps_restore_prison(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	int error = 0;

	while (vdo_typeofnext(ctx) == VPS_DUMPOBJT_PRISON) {

		if ((error = vps_restore_prison_one(ctx, vps)))
			goto out;

	}

  out:
	return (error);
}

/*
 * Restore vps instance.
 */

VPSFUNC
static int
vps_restore_vps(struct vps_snapst_ctx *ctx, const char *vps_name,
    struct vps **vps_in)
{
	struct vps_param vps_pr;
	struct vps_dumpobj *o1;
	struct vps_dump_vps *vdi;
	struct vps *vps;
	int nexttype;
	int error = 0;

	memset(&vps_pr, 0, sizeof(vps_pr));

	*vps_in = NULL;

	o1 = vdo_next(ctx);
	if (o1->type != VPS_DUMPOBJT_VPS) {
		ERRMSG(ctx, "%s: wrong object type: %p type=%d\n",
		    __func__, o1, o1->type);
		error = EINVAL;
		goto out;
	}
	vdi = (struct vps_dump_vps *)o1->data;

	// XXX dvps->vps_name check string termination
	// XXX dvps->_rootpath check string termination

	/* Name */
	if (vps_name[0])
		strlcpy(vps_pr.name, vps_name, sizeof(vps_pr.name));
	else
		strlcpy(vps_pr.name, vdi->vps_name, sizeof(vps_pr.name));

	/* Filesystem root */
	strlcpy(vps_pr.fsroot, vdi->rootpath, sizeof(vps_pr.fsroot));

	/* Restore vfs root mount. */
	if ((error = vps_restore_mounts(ctx, NULL, vps_pr.fsroot))) {
		goto out;
	}

	/*
	 * Actually allocating a vps.
	 *
	 * XXX Doing this first and then mounting the root filesystem
	 *     would make things much easier.
	 */
	if ((vps = vps_alloc(curthread->td_vps, &vps_pr, vps_pr.name,
	    &error)) == NULL) {
		goto out;
	}
	*vps_in = vps;

	vps->vps_status = VPS_ST_SUSPENDED;

	/*
	 * Get a generic ucred we can use for various vps system things.
	 */
	ctx->vps_ucred = crdup(curthread->td_ucred);
	vps_deref(ctx->vps_ucred->cr_vps, ctx->vps_ucred);
	ctx->vps_ucred->cr_vps = vps;
	vps_ref(ctx->vps_ucred->cr_vps, ctx->vps_ucred);
	prison_free(ctx->vps_ucred->cr_prison);
	ctx->vps_ucred->cr_prison = VPS_VPS(vps, prison0);
	prison_hold(ctx->vps_ucred->cr_prison);
	DBGR("%s: ctx->vps_ucred = %p\n", __func__, ctx->vps_ucred);

	/* Restore all ucreds at once. */
	if ((error = vps_restore_ucred_all(ctx, vps))) {
		goto out;
	}

	/* Restore all the remaining vfs mounts. */
	ctx->curobj = o1;
	if ((error = vps_restore_mounts(ctx, vps, vps_pr.fsroot))) {
		goto out;
	}

	if ((error = vps_restore_arg(ctx, vps))) {
		goto out;
	}

	/* Restore network related stuff. */
	if ((error = vps_restore_vnet(ctx, vps, &vps->vnet))) {
		goto out;
	}

	/* Restore prisons (jails). */
	if ((error = vps_restore_prison(ctx, vps))) {
		goto out;
	}

	/* Link prisons back into ucreds */
	if ((error = vps_restore_ucred_fixup(ctx, vps))) {
		goto out;
	}

	while ((nexttype = vdo_typeofnext(ctx)) != VPS_DUMPOBJT_PGRP) {
		switch (nexttype) {
		case VPS_DUMPOBJT_SYSVSEM_VPS:
			if (vps_func->sem_restore_vps)
				error = vps_func->sem_restore_vps(ctx, vps);
			else
				error = EOPNOTSUPP;
			break;
		case VPS_DUMPOBJT_SYSVSHM_VPS:
			if (vps_func->shm_restore_vps)
				error = vps_func->shm_restore_vps(ctx, vps);
			else
				error = EOPNOTSUPP;
			break;
		case VPS_DUMPOBJT_SYSVMSG_VPS:
			if (vps_func->msg_restore_vps)
				error = vps_func->msg_restore_vps(ctx, vps);
			else
				error = EOPNOTSUPP;
			break;
		default:
			/* Just ignore and skip. */
			vdo_next(ctx);
			break;
		}
		if (error)
			goto out;
	}

	VPS_VPS(vps, initproc) = (struct proc *)
	    ((long)vdi->initproc_id); /* ID */
	VPS_VPS(vps, initpgrp) = (struct pgrp *)
	    ((long)vdi->initpgrp_id); /* ID */
	strlcpy(VPS_VPS(vps, hostname), vdi->hostname,
	    sizeof(VPS_VPS(vps, hostname)));

	VPS_VPS(vps, boottimebin).sec = vdi->boottime.tv_sec;
	VPS_VPS(vps, boottimebin).frac = 0;
	VPS_VPS(vps, boottime).tv_sec = vdi->boottime.tv_sec;
	VPS_VPS(vps, boottime).tv_usec = vdi->boottime.tv_usec;
	VPS_VPS(vps, lastpid) = vdi->lastpid;

	vps->restore_count = vdi->restore_count + 1;

	/*
	 * Restore processes.
	 */
	if ((error = vps_restore_proc(ctx, vps))) {
		goto out;
	}
	VPS_VPS(vps, lastpid) = vdi->lastpid;

	/*
	 * Various fixup routines.
	 */

	vps_restore_prison_fixup(ctx, vps);

	if (vps_func->sem_restore_fixup &&
	    (error = vps_func->sem_restore_fixup(ctx, vps)))
		goto out;

	if (vps_func->shm_restore_fixup &&
	    (error = vps_func->shm_restore_fixup(ctx, vps)))
		goto out;

	if (vps_func->msg_restore_fixup &&
	    (error = vps_func->msg_restore_fixup(ctx, vps)))
		goto out;

  out:
	return (error);
}

/*
 * Go through the dump image and link every dump object into this list.
 */

#if 0
VPSFUNC
static int
vps_restore_fixup_objlist(struct vps_snapst_ctx *ctx,
    struct vps_dumpheader *dumphdr, struct vps_dumpobj *o)
{
	int error = 0;
	int cnt = 0;

	DBGR("%s: \n", __func__);

	SLIST_INIT(&vdi->dumpobj_list);
	ctx->dumpobj_list = &vdi->dumpobj_list;

	while ( ( ((caddr_t)o) + sizeof(*o) < ctx->data + ctx->dsize) &&
			( ((caddr_t)o) > ctx->data) &&
				o->type && o->size) {

		/*
		DBGR("%s: o=%p o->type=%d o->size=%d\n",
		    __func__, o, o->type, o->size);
		*/

		SLIST_INSERT_HEAD(ctx->dumpobj_list, o, list);

		++cnt;

		/*
		 * If the end-of-snapshot record is missing, the range check
		 * breaks the loop.
		 */
		if (o->type == VPS_DUMPOBJT_END) {
			/* Reached end of snapshot. */
			break;
		}

		/* Next object. */
		o = (struct vps_dumpobj *)(o->data + o->subsize);
	}

	DBGR("%s: got %d elements\n", __func__, cnt);

	return (error);
}
#endif /* 0 */

/*
 * The user supplies an userspace address, where the snapshot
 * dump is located.
 * The first part of this dump contains the kernel memory parts,
 * which will be copied or newly assembled anyway.
 *
 * The second (usually much larger) part is the userspace memory
 * of the dumped processes. The pages will be moved into the
 * new processes vmspaces without copying.
 */

VPSFUNC
static int
vps_restore_copyin(struct vps_snapst_ctx *ctx, struct vps_arg_snapst *va)
{
	struct vps_dumpheader *dumphdr;
	struct vps_dumpobj *o1;
	vm_offset_t kvaddr;
	u_int checksum1, checksum2;
	vm_page_t m;
	int npages;
	int i;
	int error = 0;

	DBGR("%s: \n", __func__);

	ctx->data = NULL;
	ctx->vmobj = NULL;

	/* Snapshot must be page-aligned! */
	if ((void *)trunc_page((unsigned long)va->database) !=
	    va->database) {
		ERRMSG(ctx, "%s: dump must be page aligned but is not: "
		    "%p\n", __func__, va->database);
		error = EFAULT;
		goto fail;
	}

	if (vm_page_count_min()) {
		ERRMSG(ctx, "%s: low on memory: v_free_min=%u > "
		    "(v_free_count=%u + v_cache_count=%u)\n",
		    __func__, cnt.v_free_min, cnt.v_free_count,
		    cnt.v_cache_count);
		ERRMSG(ctx, "%s: cnt.v_inactive_count=%u\n",
		    __func__, cnt.v_inactive_count);
		error = ENOMEM;
		goto fail;
	}
	DBGR("%s: v_free_min=%u v_free_count=%u + v_cache_count=%u\n",
	    __func__, cnt.v_free_min, cnt.v_free_count, cnt.v_cache_count);

	ctx->data = malloc(4 << PAGE_SHIFT, M_VPS_RESTORE,
	    M_WAITOK | M_ZERO);

	/*
	 * First copy only the first page in, to look how much kernel
	 * memory is needed.
	 */
	if ((error = copyin(va->database, ctx->data, PAGE_SIZE)))
		goto fail;
	dumphdr = (struct vps_dumpheader *)ctx->data;

	if (vld_checkheader(dumphdr)) {
		ERRMSG(ctx, "%s: dump is invalid\n", __func__);
		vld_printheader(dumphdr);
		error = EINVAL;
		goto fail;
	}
#ifdef DIAGNOSTIC
	if (debug_restore)
		vld_printheader(dumphdr);
#endif

	if (vps_md_restore_checkarch(dumphdr->ptrsize,
	    dumphdr->byteorder) != 0) {
		ERRMSG(ctx, "%s: wrong architecture: ptrsize=%x "
		    "byteorder=%x !\n",
		    __func__, dumphdr->ptrsize, dumphdr->byteorder);
		error = EINVAL;
		goto fail;
	}

	if (dumphdr->version != VPS_DUMPH_VERSION) {
		ERRMSG(ctx, "%s: unsupported dump version %08x, "
		    "want %08x\n",
		    __func__, dumphdr->version, VPS_DUMPH_VERSION);
		error = EINVAL;
		goto fail;
	}

	/*
	 * Since there is swap not all has to fit in memory,
	 * but try to avoid total memory exhaustion.
	 */
#ifndef INVARIANTS
	if (1 && (dumphdr->nuserpages + dumphdr->nsyspages) / 2 >
	    (cnt.v_free_count + cnt.v_cache_count + cnt.v_inactive_count)) {
		DBGR("%s: (dumphdr->nuserpages=%u + dumphdr->nsyspages=%u) "
		     "/ 2 > \n"
		     "          (v_free_count=%u + v_cache_count=%u + "
		     "v_inactive_count=%u)\n",
		    __func__, dumphdr->nuserpages, dumphdr->nsyspages,
		    cnt.v_free_count, cnt.v_cache_count,
		    cnt.v_inactive_count);
		ERRMSG(ctx, "%s: low on memory, not restoring "
			"VPS instance (pid=%u)\n",
			__func__, curthread->td_proc->p_pid);
		error = ENOMEM;
		goto fail;
	}
#endif

	ctx->nuserpages = dumphdr->nuserpages;
	ctx->nsyspages = dumphdr->nsyspages;
	ctx->dsize = dumphdr->nsyspages << PAGE_SHIFT;
	ctx->userpagesaddr = ((caddr_t)va->database) + ctx->dsize;
	ctx->userpagesidx = 0;
	SLIST_INIT(&ctx->obj_list);

	free(ctx->data, M_VPS_RESTORE);

	ctx->vmobj = vm_object_allocate(OBJT_DEFAULT, ctx->nsyspages);
	if (ctx->vmobj == NULL) {
		error = ENOMEM;
		goto fail;
	}

	vm_map_lock(kernel_map);
	/* Find free space in kernel virtual address space. */
	if (vm_map_findspace(kernel_map, vm_map_min(kernel_map),
		ctx->nsyspages << PAGE_SHIFT, &kvaddr) != KERN_SUCCESS) {
		vm_map_unlock(kernel_map);
		vm_object_deallocate(ctx->vmobj);
		ctx->vmobj = NULL;
		ERRMSG(ctx, "%s: vm_map_findspace failed\n", __func__);
		error = ENOMEM;
		goto fail;
	}

	if (vm_map_insert(kernel_map, ctx->vmobj,
	    kvaddr - VM_MIN_KERNEL_ADDRESS,
	    kvaddr, kvaddr + (ctx->vmobj->size << PAGE_SHIFT),
	    VM_PROT_ALL, VM_PROT_ALL, 0) != KERN_SUCCESS) {
		vm_map_unlock(kernel_map);
		vm_object_deallocate(ctx->vmobj);
		ctx->vmobj = NULL;
		ERRMSG(ctx, "%s: vm_map_insert failed\n", __func__);
		error = ENOMEM;
		goto fail;
	}
	vm_map_unlock(kernel_map);

	VM_OBJECT_WLOCK(ctx->vmobj);
	for (i = 0; i < ctx->vmobj->size; i++) {
		do {
			m = vm_page_alloc(ctx->vmobj, i,
			    VM_ALLOC_NORMAL|VM_ALLOC_WIRED);
			if (m == NULL) {
				/* Assume that object will not be
				   modified. */
				VM_OBJECT_WUNLOCK(ctx->vmobj);
				vm_waitpfault();
				VM_OBJECT_WLOCK(ctx->vmobj);
			}
		} while (m == NULL);
		pmap_qenter(kvaddr + (i << PAGE_SHIFT), &m, 1);
		vm_page_wakeup(m);
	}
	VM_OBJECT_WUNLOCK(ctx->vmobj);

	DBGR("%s: mapped syspages vm object %p at %zx - %zx\n",
	    __func__, ctx->vmobj, (size_t)kvaddr,
	    (size_t)(kvaddr + (ctx->vmobj->size << PAGE_SHIFT)));

	ctx->data = ctx->cpos = (void *)kvaddr;

	DBGR("%s: ctx->data=%p ctx->dsize=%zx\n",
	    __func__, ctx->data, (size_t)ctx->dsize);

	if ((error = copyin(va->database, ctx->data, ctx->dsize)))
		goto fail;

	dumphdr = (struct vps_dumpheader *)ctx->data;
	ctx->dumphdr = dumphdr;

	checksum1 = dumphdr->checksum;
	dumphdr->checksum = 0;
	checksum2 = vps_cksum(ctx->data, ctx->dsize);
	if (checksum1 != checksum2) {
		ERRMSG(ctx, "%s: CHECKSUM mismatch: snapshot info: "
		    "%08x calculated: %08x\n",
		    __func__, checksum1, checksum2);
		error = EINVAL;
		goto fail;
	} else
		DBGR("%s: CHECKSUM is valid !\n", __func__);
	dumphdr->checksum = checksum1;

	o1 = (struct vps_dumpobj *)(dumphdr + 1);
	ctx->rootobj = o1;
	ctx->relative = 1;
	ctx->elements = -1;

	if (vdo_checktree(ctx)) {
		ERRMSG(ctx, "%s: dump tree is invalid !\n", __func__);
		vdo_printtree(ctx);
		return (EINVAL);
	}
	vdo_makeabsolute(ctx);
	DBGR("%s: dump tree is valid and contains %d elements\n",
	    __func__, ctx->elements);

	/* XXX maybe link together wil lists/tailqs ... */

	vm_map_protect(kernel_map, kvaddr, kvaddr +
	    (ctx->vmobj->size << PAGE_SHIFT),
	    VM_PROT_READ, 0);

	DBGR("%s: set map entry to readonly\n", __func__);

	/* Check if the user really has the memory containing
	   the userpages. */
	if ((m = vps_restore_getuserpage(ctx, ctx->nuserpages - 1,
	    1)) == NULL) {
		ERRMSG(ctx, "%s: user supplied memory range "
		    "inaccessible !\n", __func__);
		error = EFAULT;
		goto fail;
	}
	VM_OBJECT_WLOCK(m->object);
	vm_page_wakeup(m);
	VM_OBJECT_WUNLOCK(m->object);

	return (0);

  fail:

	if (ctx->data != NULL && ctx->vmobj == NULL) {
		free(ctx->data, M_VPS_RESTORE);

	} else if (ctx->vmobj != NULL) {
		npages = ctx->vmobj->size;

		pmap_qremove(kvaddr, npages);
		VM_OBJECT_WLOCK(ctx->vmobj);
		while (npages) {
			npages--;
			m = TAILQ_LAST(&ctx->vmobj->memq, pglist);
			vm_page_lock(m);
			vm_page_unwire(m, 0);
			vm_page_free(m);
			vm_page_unlock(m);
		}
		VM_OBJECT_WUNLOCK(ctx->vmobj);

		/* This also destroys the vm object. */
		(void)vm_map_remove(kernel_map, kvaddr, kvaddr +
		    (ctx->vmobj->size << PAGE_SHIFT));
	}

	return (error);
}

VPSFUNC
int
vps_restore(struct vps_dev_ctx *dev_ctx, struct vps_arg_snapst *va)
{
	struct vps_snapst_ctx *ctx;
	struct vps_dumpheader *dumphdr;
	struct vps *vps, *vps_save;
	struct vps_dumpobj *o1;
	time_t starttime;
	void *ptr;
	int error = 0;

#ifdef DIAGNOSTIC
	vps_snapst_print_errormsgs = debug_restore;
#endif

	starttime = time_second;

	vps_restore_mod_refcnt++;

	DBGR("%s: \n", __func__);

	vps = NULL;
	vps_save = curthread->td_vps;

	ctx = malloc(sizeof(*ctx), M_VPS_RESTORE, M_WAITOK|M_ZERO);
	LIST_INIT(&ctx->errormsgs);

	if (vps_func->vps_dumpobj_create == NULL) {
		ERRMSG(ctx, "%s: vps_libdump module not loaded\n",
		    __func__);
		error = EOPNOTSUPP;
		goto out;
	}

#ifdef INVARIANTS
	/* XXX debugging: if more than 3 dead vpses, stop */
	{
		struct vps *vps2;
		int dead_cnt = 0;

		LIST_FOREACH(vps2, &vps_head, vps_all)
			if (vps2->vps_status == VPS_ST_DEAD)
				dead_cnt++;

		if (dead_cnt > 3) {
			printf("%s: more than 3 dead vps instances\n",
			    __func__);
			kdb_enter(KDB_WHY_BREAK, "VPS break to debugger");
		}
	}
#endif

	if ((error = vps_restore_copyin(ctx, va)))
		goto out;

	ctx->cpos = ctx->data;
	dumphdr = ctx->dumphdr;
	o1 = ctx->rootobj;

	if (o1->type != VPS_DUMPOBJT_ROOT) {
		ERRMSG(ctx, "%s: missing root object\n", __func__);
		error = EINVAL;
		goto out;
	}

	o1 = vdo_next(ctx);
	if (o1->type != VPS_DUMPOBJT_SYSINFO) {
		ERRMSG(ctx, "%s: missing sysinfo object\n", __func__);
		error = EINVAL;
		goto out;
	}
	ctx->old_sysinfo = (struct vps_dump_sysinfo *)o1->data;

	if ((error = vps_restore_vps(ctx, va->vps_name, &vps))) {
		goto out;
	}

	/*
	 * Free resources.
	 */

	vps_restore_cleanup_vmspace(ctx, vps, &ctx->obj_list);
	vps_restore_cleanup_pipe(ctx, vps, &ctx->obj_list);
	vps_restore_cleanup_fdset(ctx, vps, &ctx->obj_list);
#ifdef INVARIANTS
	vps_restore_ucred_checkall(ctx, vps);
#endif
	vps_restore_cleanup_ucred(ctx, vps, &ctx->obj_list);

	while ( ! SLIST_EMPTY(&ctx->obj_list)) {
		ptr = SLIST_FIRST(&ctx->obj_list);
		SLIST_REMOVE_HEAD(&ctx->obj_list, list);
		free(ptr, M_VPS_RESTORE);
	}
	if (ctx->vps_ucred) {
		DBGR("%s: ctx->vps_ucred->cr_ref=%d\n",
		    __func__, ctx->vps_ucred->cr_ref);
		crfree(ctx->vps_ucred);
	}

  out:

	if (ctx->vmobj != NULL) {
		vm_offset_t kvaddr;
		vm_page_t m;
		int npages;

		kvaddr = (vm_offset_t)ctx->data;
		npages = ctx->vmobj->size;

		pmap_qremove(kvaddr, npages);
		VM_OBJECT_WLOCK(ctx->vmobj);
		while (npages) {
			npages--;
			m = TAILQ_LAST(&ctx->vmobj->memq, pglist);
			/* On early error condition we might have less
			   than npages */
			if (m == NULL)
				break;
			vm_page_lock(m);
			vm_page_unwire(m, 0);
			vm_page_free(m);
			vm_page_unlock(m);
		}
		VM_OBJECT_WUNLOCK(ctx->vmobj);

		/* This also destroys the vm object. */
		(void)vm_map_remove(kernel_map, kvaddr, kvaddr +
			(ctx->vmobj->size << PAGE_SHIFT));
	}

	if (1) {
		struct vps_snapst_errmsg *msg;
		char *buf, *bufpos;
		size_t buflen;
		int error2;

		buflen = 0;

		LIST_FOREACH(msg, &ctx->errormsgs, list) {
			printf("%s\n", msg->str);
			buflen += strlen(msg->str);
		}

		buf = bufpos = malloc(buflen + 1, M_TEMP, M_WAITOK);
		LIST_FOREACH(msg, &ctx->errormsgs, list) {
			memcpy(bufpos, msg->str, strlen(msg->str));
			bufpos += strlen(msg->str);
		}
		*(bufpos) = '\0';
		bufpos += 1;

		DBGR("%s: va->msgbase=%p va->msglen=%zu\n",
		    __func__, va->msgbase, va->msglen);
		if (va->msgbase != NULL && va->msglen > 0) {
			if (va->msglen < (bufpos - buf))
				printf("%s: warning: user-supplied buffer "
				    "too small for error messages\n",
				    __func__);
			if ((error2 = copyout(buf, va->msgbase,
			    min(va->msglen, (bufpos - buf)))))
				printf("%s: error messages copyout=%d\n",
				     __func__, error2);
		}
		free(buf, M_TEMP);

		while (!LIST_EMPTY(&ctx->errormsgs)) {
			msg = LIST_FIRST(&ctx->errormsgs);
			LIST_REMOVE(msg, list);
			free(msg, M_TEMP);
		}
	}

	free(ctx, M_VPS_RESTORE);

	if (error) {
		/* XXX Perform a proper cleanup ! */
		if (vps != NULL) {
			vps->vps_status = VPS_ST_DEAD;
			snprintf(vps->vps_name, sizeof(vps->vps_name),
			    "dead_%p", vps);
		}
		DBGR("%s: error = %d\n", __func__, error);
	}

	curvnet = NULL;
	curthread->td_vps = vps_save;
	KASSERT(curthread->td_vps == curthread->td_ucred->cr_vps,
	    ("%s: curthread->td_vps=%p curthread->td_ucred->cr_vps=%p\n",
	    __func__, curthread->td_vps, curthread->td_ucred->cr_vps));

	vps_restore_mod_refcnt--;

	DBGR("%s: total time: %lld seconds\n",
	    __func__, (long long int)(time_second - starttime));

	return (error);
}

/* XXX This code should be protected by module refcount as well. */
void
vps_restore_return(struct thread *td, struct trapframe *frame)
{

	cpu_set_syscall_retval(td, td->td_errno);

	userret(td, frame);

#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT))
		ktrstruct("VPS", "VPS RESTORED", 13);
#endif
	mtx_assert(&Giant, MA_NOTOWNED);
}

static int
vps_restore_modevent(module_t mod, int type, void *data)
{
	int error;

	error = 0;

	switch (type) {
	case MOD_LOAD:
	   vps_restore_mod_refcnt = 0;
	   vps_func->vps_restore = vps_restore;
	   vps_func->vps_restore_ucred = vps_restore_ucred;
	   vps_func->vps_restore_ucred_lookup = vps_restore_ucred_lookup;
	   vps_func->vps_restore_return = vps_restore_return;
	   break;
	case MOD_UNLOAD:
	   if (vps_restore_mod_refcnt > 0)
		return (EBUSY);
	   vps_func->vps_restore = NULL;
	   vps_func->vps_restore_ucred = NULL;
	   vps_func->vps_restore_ucred_lookup = NULL;
	   vps_func->vps_restore_return = NULL;
	   break;
	default:
	   error = EOPNOTSUPP;
	   break;
	}
	return (error);
}

static moduledata_t vps_restore_mod = {
	"vps_restore",
	vps_restore_modevent,
	0
};

DECLARE_MODULE(vps_restore, vps_restore_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);

#endif /* VPS */

/* EOF */
