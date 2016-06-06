/*	$FreeBSD$ */
/*	$NetBSD: pfil.c,v 1.20 2001/11/12 23:49:46 lukem Exp $	*/

/*-
 * Copyright (c) 1996 Matthew R. Green
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/rmlock.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <sys/condvar.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/pfil.h>

static struct mtx pfil_global_lock;

MTX_SYSINIT(pfil_heads_lock, &pfil_global_lock, "pfil_head_list lock",
  MTX_DEF);

static struct packet_filter_hook *pfil_chain_get(int, struct pfil_head *);
static int pfil_chain_add(pfil_chain_t *, struct packet_filter_hook *, int);
static int pfil_chain_remove(pfil_chain_t *, pfil_func_t, void *);

LIST_HEAD(pfilheadhead, pfil_head);
VNET_DEFINE(struct pfilheadhead, pfil_head_list);
#define	V_pfil_head_list	VNET(pfil_head_list)
VNET_DEFINE(struct rmlock, pfil_lock);
#define	V_pfil_lock	VNET(pfil_lock)

/*
 * pfil_run_hooks() runs the specified packet filter hook chain.
 */
int
pfil_run_hooks(struct pfil_head *ph, struct mbuf **mp, struct ifnet *ifp,
    int dir, struct inpcb *inp)
{
	struct rm_priotracker rmpt;
	struct packet_filter_hook *pfh;
	struct mbuf *m = *mp;
	int rv = 0;

	PFIL_RLOCK(ph, &rmpt);
	KASSERT(ph->ph_nhooks >= 0, ("Pfil hook count dropped < 0"));
	for (pfh = pfil_chain_get(dir, ph); pfh != NULL;
	     pfh = TAILQ_NEXT(pfh, pfil_chain)) {
		if (pfh->pfil_func != NULL) {
			rv = (*pfh->pfil_func)(pfh->pfil_arg, &m, ifp, dir,
			    inp);
			if (rv != 0 || m == NULL)
				break;
		}
	}
	PFIL_RUNLOCK(ph, &rmpt);
	*mp = m;
	return (rv);
}

static struct packet_filter_hook *
pfil_chain_get(int dir, struct pfil_head *ph)
{

	if (dir == PFIL_IN)
		return (TAILQ_FIRST(&ph->ph_in));
	else if (dir == PFIL_OUT)
		return (TAILQ_FIRST(&ph->ph_out));
	else
		return (NULL);
}

/*
 * pfil_try_rlock() acquires rm reader lock for specified head
 * if this is immediately possible.
 */
int
pfil_try_rlock(struct pfil_head *ph, struct rm_priotracker *tracker)
{

	return (PFIL_TRY_RLOCK(ph, tracker));
}

/*
 * pfil_rlock() acquires rm reader lock for specified head.
 */
void
pfil_rlock(struct pfil_head *ph, struct rm_priotracker *tracker)
{

	PFIL_RLOCK(ph, tracker);
}

/*
 * pfil_runlock() releases reader lock for specified head.
 */
void
pfil_runlock(struct pfil_head *ph, struct rm_priotracker *tracker)
{

	PFIL_RUNLOCK(ph, tracker);
}

/*
 * pfil_wlock() acquires writer lock for specified head.
 */
void
pfil_wlock(struct pfil_head *ph)
{

	PFIL_WLOCK(ph);
}

/*
 * pfil_wunlock() releases writer lock for specified head.
 */
void
pfil_wunlock(struct pfil_head *ph)
{

	PFIL_WUNLOCK(ph);
}

/*
 * pfil_wowned() returns a non-zero value if the current thread owns
 * an exclusive lock.
 */
int
pfil_wowned(struct pfil_head *ph)
{

	return (PFIL_WOWNED(ph));
}

/*
 * pfil_head_register() registers a pfil_head with the packet filter hook
 * mechanism.
 */
int
pfil_head_register(struct pfil_head *ph)
{
	struct pfil_head *lph;

	PFIL_HEADLIST_LOCK();
	LIST_FOREACH(lph, &V_pfil_head_list, ph_list) {
		if (ph->ph_type == lph->ph_type &&
		    ph->ph_un.phu_val == lph->ph_un.phu_val) {
			PFIL_HEADLIST_UNLOCK();
			return (EEXIST);
		}
	}
	PFIL_LOCK_INIT(ph);
	ph->ph_nhooks = 0;
	TAILQ_INIT(&ph->ph_in);
	TAILQ_INIT(&ph->ph_out);
	LIST_INSERT_HEAD(&V_pfil_head_list, ph, ph_list);
	PFIL_HEADLIST_UNLOCK();
	return (0);
}

/*
 * pfil_head_unregister() removes a pfil_head from the packet filter hook
 * mechanism.  The producer of the hook promises that all outstanding
 * invocations of the hook have completed before it unregisters the hook.
 */
int
pfil_head_unregister(struct pfil_head *ph)
{
	struct packet_filter_hook *pfh, *pfnext;
		
	PFIL_HEADLIST_LOCK();
	LIST_REMOVE(ph, ph_list);
	PFIL_HEADLIST_UNLOCK();
	TAILQ_FOREACH_SAFE(pfh, &ph->ph_in, pfil_chain, pfnext)
		free(pfh, M_IFADDR);
	TAILQ_FOREACH_SAFE(pfh, &ph->ph_out, pfil_chain, pfnext)
		free(pfh, M_IFADDR);
	PFIL_LOCK_DESTROY(ph);
	return (0);
}

/*
 * pfil_head_get() returns the pfil_head for a given key/dlt.
 */
struct pfil_head *
pfil_head_get(int type, u_long val)
{
	struct pfil_head *ph;

	PFIL_HEADLIST_LOCK();
	LIST_FOREACH(ph, &V_pfil_head_list, ph_list)
		if (ph->ph_type == type && ph->ph_un.phu_val == val)
			break;
	PFIL_HEADLIST_UNLOCK();
	return (ph);
}

/*
 * pfil_add_hook() adds a function to the packet filter hook.  the
 * flags are:
 *	PFIL_IN		call me on incoming packets
 *	PFIL_OUT	call me on outgoing packets
 *	PFIL_ALL	call me on all of the above
 *	PFIL_WAITOK	OK to call malloc with M_WAITOK.
 */
int
pfil_add_hook(pfil_func_t func, void *arg, int flags, struct pfil_head *ph)
{
	struct packet_filter_hook *pfh1 = NULL;
	struct packet_filter_hook *pfh2 = NULL;
	int err;

	if (flags & PFIL_IN) {
		pfh1 = (struct packet_filter_hook *)malloc(sizeof(*pfh1), 
		    M_IFADDR, (flags & PFIL_WAITOK) ? M_WAITOK : M_NOWAIT);
		if (pfh1 == NULL) {
			err = ENOMEM;
			goto error;
		}
	}
	if (flags & PFIL_OUT) {
		pfh2 = (struct packet_filter_hook *)malloc(sizeof(*pfh1),
		    M_IFADDR, (flags & PFIL_WAITOK) ? M_WAITOK : M_NOWAIT);
		if (pfh2 == NULL) {
			err = ENOMEM;
			goto error;
		}
	}
	PFIL_WLOCK(ph);
	if (flags & PFIL_IN) {
		pfh1->pfil_func = func;
		pfh1->pfil_arg = arg;
		err = pfil_chain_add(&ph->ph_in, pfh1, flags & ~PFIL_OUT);
		if (err)
			goto locked_error;
		ph->ph_nhooks++;
	}
	if (flags & PFIL_OUT) {
		pfh2->pfil_func = func;
		pfh2->pfil_arg = arg;
		err = pfil_chain_add(&ph->ph_out, pfh2, flags & ~PFIL_IN);
		if (err) {
			if (flags & PFIL_IN)
				pfil_chain_remove(&ph->ph_in, func, arg);
			goto locked_error;
		}
		ph->ph_nhooks++;
	}
	PFIL_WUNLOCK(ph);
	return (0);
locked_error:
	PFIL_WUNLOCK(ph);
error:
	if (pfh1 != NULL)
		free(pfh1, M_IFADDR);
	if (pfh2 != NULL)
		free(pfh2, M_IFADDR);
	return (err);
}

/*
 * pfil_remove_hook removes a specific function from the packet filter hook
 * chain.
 */
int
pfil_remove_hook(pfil_func_t func, void *arg, int flags, struct pfil_head *ph)
{
	int err = 0;

	PFIL_WLOCK(ph);
	if (flags & PFIL_IN) {
		err = pfil_chain_remove(&ph->ph_in, func, arg);
		if (err == 0)
			ph->ph_nhooks--;
	}
	if ((err == 0) && (flags & PFIL_OUT)) {
		err = pfil_chain_remove(&ph->ph_out, func, arg);
		if (err == 0)
			ph->ph_nhooks--;
	}
	PFIL_WUNLOCK(ph);
	return (err);
}

/*
 * Internal: Add a new pfil hook into a hook chain.
 */
static int
pfil_chain_add(pfil_chain_t *chain, struct packet_filter_hook *pfh1, int flags)
{
	struct packet_filter_hook *pfh;

	/*
	 * First make sure the hook is not already there.
	 */
	TAILQ_FOREACH(pfh, chain, pfil_chain)
		if (pfh->pfil_func == pfh1->pfil_func &&
		    pfh->pfil_arg == pfh1->pfil_arg)
			return (EEXIST);

	/*
	 * Insert the input list in reverse order of the output list so that
	 * the same path is followed in or out of the kernel.
	 */
	if (flags & PFIL_IN)
		TAILQ_INSERT_HEAD(chain, pfh1, pfil_chain);
	else
		TAILQ_INSERT_TAIL(chain, pfh1, pfil_chain);
	return (0);
}

/*
 * Internal: Remove a pfil hook from a hook chain.
 */
static int
pfil_chain_remove(pfil_chain_t *chain, pfil_func_t func, void *arg)
{
	struct packet_filter_hook *pfh;

	TAILQ_FOREACH(pfh, chain, pfil_chain)
		if (pfh->pfil_func == func && pfh->pfil_arg == arg) {
			TAILQ_REMOVE(chain, pfh, pfil_chain);
			free(pfh, M_IFADDR);
			return (0);
		}
	return (ENOENT);
}

/*
 * Stuff that must be initialized for every instance (including the first of
 * course).
 */
static void
vnet_pfil_init(const void *unused __unused)
{

	LIST_INIT(&V_pfil_head_list);
	PFIL_LOCK_INIT_REAL(&V_pfil_lock, "shared");
}

/*
 * Called for the removal of each instance.
 */
static void
vnet_pfil_uninit(const void *unused __unused)
{

	KASSERT(LIST_EMPTY(&V_pfil_head_list),
	    ("%s: pfil_head_list %p not empty", __func__, &V_pfil_head_list));
	PFIL_LOCK_DESTROY_REAL(&V_pfil_lock);
}

/* Define startup order. */
#define	PFIL_SYSINIT_ORDER	SI_SUB_PROTO_BEGIN
#define	PFIL_MODEVENT_ORDER	(SI_ORDER_FIRST) /* On boot slot in here. */
#define	PFIL_VNET_ORDER		(PFIL_MODEVENT_ORDER + 2) /* Later still. */

/*
 * Starting up.
 *
 * VNET_SYSINIT is called for each existing vnet and each new vnet.
 */
VNET_SYSINIT(vnet_pfil_init, PFIL_SYSINIT_ORDER, PFIL_VNET_ORDER,
    vnet_pfil_init, NULL);
 
/*
 * Closing up shop.  These are done in REVERSE ORDER.  Not called on reboot.
 *
 * VNET_SYSUNINIT is called for each exiting vnet as it exits.
 */
VNET_SYSUNINIT(vnet_pfil_uninit, PFIL_SYSINIT_ORDER, PFIL_VNET_ORDER,
    vnet_pfil_uninit, NULL);
