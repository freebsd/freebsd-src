/*-
 * Copyright (c) 2004-2009 University of Zagreb
 * Copyright (c) 2006-2009 FreeBSD Foundation
 *
 * This software was developed by the University of Zagreb and the
 * FreeBSD Foundation under sponsorship by the Stichting NLnet and the
 * FreeBSD Foundation.
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

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sx.h>
#include <sys/vimage.h>
#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <net/if.h>
#include <net/route.h>
#include <net/vnet.h>

MALLOC_DEFINE(M_VNET, "vnet", "network stack control block");

struct rwlock		vnet_rwlock;
struct sx		vnet_sxlock;

#define	VNET_LIST_WLOCK() do {						\
	sx_xlock(&vnet_sxlock);						\
	rw_wlock(&vnet_rwlock);						\
} while (0)

#define	VNET_LIST_WUNLOCK() do {					\
	rw_wunlock(&vnet_rwlock);					\
	sx_xunlock(&vnet_sxlock);					\
} while (0)

struct vnet_list_head vnet_head;
struct vnet *vnet0;

/*
 * Move an ifnet to or from another vnet, specified by the jail id.
 */
int
vi_if_move(struct thread *td, struct ifnet *ifp, char *ifname, int jid)
{
	struct ifnet *t_ifp;
	struct prison *pr;
	struct vnet *new_vnet;
	int error;

	sx_slock(&allprison_lock);
	pr = prison_find_child(td->td_ucred->cr_prison, jid);
	sx_sunlock(&allprison_lock);
	if (pr == NULL)
		return (ENXIO);
	prison_hold_locked(pr);
	mtx_unlock(&pr->pr_mtx);
	if (ifp != NULL) {
		/* SIOCSIFVNET */
		new_vnet = pr->pr_vnet;
	} else {
		/* SIOCSIFRVNET */
		new_vnet = TD_TO_VNET(td);
		CURVNET_SET(pr->pr_vnet);
		ifp = ifunit(ifname);
		CURVNET_RESTORE();
		if (ifp == NULL) {
			prison_free(pr);
			return (ENXIO);
		}
	}

	error = 0;
	if (new_vnet != ifp->if_vnet) {
		/*
		 * Check for naming clashes in target vnet.  Not locked so races
		 * are possible.
		 */
		CURVNET_SET_QUIET(new_vnet);
		t_ifp = ifunit(ifname);
		CURVNET_RESTORE();
		if (t_ifp != NULL)
			error = EEXIST;
		else {
			/* Detach from curvnet and attach to new_vnet. */
			if_vmove(ifp, new_vnet);

			/* Report the new if_xname back to the userland */
			sprintf(ifname, "%s", ifp->if_xname);
		}
	}
	prison_free(pr);
	return (error);
}

struct vnet *
vnet_alloc(void)
{
	struct vnet *vnet;

	vnet = malloc(sizeof(struct vnet), M_VNET, M_WAITOK | M_ZERO);
	vnet->vnet_magic_n = VNET_MAGIC_N;
	vnet_data_init(vnet);

	/* Initialize / attach vnet module instances. */
	CURVNET_SET_QUIET(vnet);

	sx_xlock(&vnet_sxlock);
	vnet_sysinit();
	CURVNET_RESTORE();

	rw_wlock(&vnet_rwlock);
	LIST_INSERT_HEAD(&vnet_head, vnet, vnet_le);
	VNET_LIST_WUNLOCK();

	return (vnet);
}

void
vnet_destroy(struct vnet *vnet)
{
	struct ifnet *ifp, *nifp;

	KASSERT(vnet->vnet_sockcnt == 0,
	    ("%s: vnet still has sockets", __func__));

	VNET_LIST_WLOCK();
	LIST_REMOVE(vnet, vnet_le);
	rw_wunlock(&vnet_rwlock);

	CURVNET_SET_QUIET(vnet);

	/* Return all inherited interfaces to their parent vnets. */
	TAILQ_FOREACH_SAFE(ifp, &V_ifnet, if_link, nifp) {
		if (ifp->if_home_vnet != ifp->if_vnet)
			if_vmove(ifp, ifp->if_home_vnet);
	}

	vnet_sysuninit();
	sx_xunlock(&vnet_sxlock);

	CURVNET_RESTORE();

	/* Hopefully, we are OK to free the vnet container itself. */
	vnet_data_destroy(vnet);
	vnet->vnet_magic_n = 0xdeadbeef;
	free(vnet, M_VNET);
}

void
vnet_foreach(void (*vnet_foreach_fn)(struct vnet *, void *), void *arg)
{
	struct vnet *vnet;

	VNET_LIST_RLOCK();
	LIST_FOREACH(vnet, &vnet_head, vnet_le)
		vnet_foreach_fn(vnet, arg);
	VNET_LIST_RUNLOCK();
}

static void
vnet_init_prelink(void *arg)
{

	rw_init(&vnet_rwlock, "vnet_rwlock");
	sx_init(&vnet_sxlock, "vnet_sxlock");
	LIST_INIT(&vnet_head);
}
SYSINIT(vnet_init_prelink, SI_SUB_VNET_PRELINK, SI_ORDER_FIRST,
    vnet_init_prelink, NULL);

static void
vnet0_init(void *arg)
{

	/*
	 * We MUST clear curvnet in vi_init_done() before going SMP,
	 * otherwise CURVNET_SET() macros would scream about unnecessary
	 * curvnet recursions.
	 */
	curvnet = prison0.pr_vnet = vnet0 = vnet_alloc();
}
SYSINIT(vnet0_init, SI_SUB_VNET, SI_ORDER_FIRST, vnet0_init, NULL);

static void
vnet_init_done(void *unused)
{

	curvnet = NULL;
}

SYSINIT(vnet_init_done, SI_SUB_VNET_DONE, SI_ORDER_FIRST, vnet_init_done,
    NULL);

#ifdef DDB
DB_SHOW_COMMAND(vnets, db_show_vnets)
{
	VNET_ITERATOR_DECL(vnet_iter);

#if SIZE_MAX == UINT32_MAX /* 32-bit arch */
	db_printf("      vnet ifs socks\n");
#else /* 64-bit arch, most probaly... */
	db_printf("              vnet ifs socks\n");
#endif
	VNET_FOREACH(vnet_iter) {
		db_printf("%p %3d %5d\n", vnet_iter, vnet_iter->vnet_ifcnt,
		    vnet_iter->vnet_sockcnt);
	}
}
#endif
