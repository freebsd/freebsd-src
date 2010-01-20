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
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <sys/condvar.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/pfil.h>

static struct mtx pfil_global_lock;

MTX_SYSINIT(pfil_heads_lock, &pfil_global_lock, "pfil_head_list lock", MTX_DEF);

static int pfil_list_add(pfil_list_t *, struct packet_filter_hook *, int);

static int pfil_list_remove(pfil_list_t *,
    int (*)(void *, struct mbuf **, struct ifnet *, int, struct inpcb *), void *);

LIST_HEAD(, pfil_head) pfil_head_list =
    LIST_HEAD_INITIALIZER(&pfil_head_list);

static __inline void
PFIL_RLOCK(struct pfil_head *ph)
{
	mtx_lock(&ph->ph_mtx);
	ph->ph_busy_count++;
	mtx_unlock(&ph->ph_mtx);
}

static __inline void
PFIL_RUNLOCK(struct pfil_head *ph)
{
	mtx_lock(&ph->ph_mtx);
	ph->ph_busy_count--;
	if (ph->ph_busy_count == 0 && ph->ph_want_write)
		cv_signal(&ph->ph_cv);
	mtx_unlock(&ph->ph_mtx);
}

static __inline void
PFIL_WLOCK(struct pfil_head *ph)
{
	mtx_lock(&ph->ph_mtx);
	ph->ph_want_write = 1;
	while (ph->ph_busy_count > 0)
		cv_wait(&ph->ph_cv, &ph->ph_mtx);
}

static __inline int
PFIL_TRY_WLOCK(struct pfil_head *ph)
{
	mtx_lock(&ph->ph_mtx);
	ph->ph_want_write = 1;
	if (ph->ph_busy_count > 0) {
		ph->ph_want_write = 0;
		mtx_unlock(&ph->ph_mtx);
		return EBUSY;
	}
	return 0;
}

static __inline void
PFIL_WUNLOCK(struct pfil_head *ph)
{
	ph->ph_want_write = 0;
	cv_signal(&ph->ph_cv);
	mtx_unlock(&ph->ph_mtx);
}

#define PFIL_LIST_LOCK() mtx_lock(&pfil_global_lock)
#define PFIL_LIST_UNLOCK() mtx_unlock(&pfil_global_lock)

/*
 * pfil_run_hooks() runs the specified packet filter hooks.
 */
int
pfil_run_hooks(struct pfil_head *ph, struct mbuf **mp, struct ifnet *ifp,
    int dir, struct inpcb *inp)
{
	struct packet_filter_hook *pfh;
	struct mbuf *m = *mp;
	int rv = 0;

	if (ph->ph_busy_count == -1)
		return (0);
	/*
	 * Prevent packet filtering from starving the modification of
	 * the packet filters. We would prefer a reader/writer locking
	 * mechanism with guaranteed ordering, though.
	 */
	if (ph->ph_want_write) {
		m_freem(*mp);
		*mp = NULL;
		return (ENOBUFS);
	}

	PFIL_RLOCK(ph);
	for (pfh = pfil_hook_get(dir, ph); pfh != NULL;
	     pfh = TAILQ_NEXT(pfh, pfil_link)) {
		if (pfh->pfil_func != NULL) {
			rv = (*pfh->pfil_func)(pfh->pfil_arg, &m, ifp, dir, inp);
			if (rv != 0 || m == NULL)
				break;
		}
	}
	PFIL_RUNLOCK(ph);
	
	*mp = m;
	return (rv);
}

/*
 * pfil_head_register() registers a pfil_head with the packet filter
 * hook mechanism.
 */
int
pfil_head_register(struct pfil_head *ph)
{
	struct pfil_head *lph;

	PFIL_LIST_LOCK();
	LIST_FOREACH(lph, &pfil_head_list, ph_list)
		if (ph->ph_type == lph->ph_type &&
		    ph->ph_un.phu_val == lph->ph_un.phu_val) {
			PFIL_LIST_UNLOCK();
			return EEXIST;
		}
	PFIL_LIST_UNLOCK();

	if (mtx_initialized(&ph->ph_mtx)) {	/* should not happen */
		KASSERT((0), ("%s: allready initialized!", __func__));
		return EBUSY;
	} else {
		ph->ph_busy_count = -1;
		ph->ph_want_write = 1;
		mtx_init(&ph->ph_mtx, "pfil_head_mtx", NULL, MTX_DEF);
		cv_init(&ph->ph_cv, "pfil_head_cv");
		mtx_lock(&ph->ph_mtx);			/* XXX: race? */
	}

	TAILQ_INIT(&ph->ph_in);
	TAILQ_INIT(&ph->ph_out);

	PFIL_LIST_LOCK();
	LIST_INSERT_HEAD(&pfil_head_list, ph, ph_list);
	PFIL_LIST_UNLOCK();
	
	PFIL_WUNLOCK(ph);
	
	return (0);
}

/*
 * pfil_head_unregister() removes a pfil_head from the packet filter
 * hook mechanism.
 */
int
pfil_head_unregister(struct pfil_head *ph)
{
	struct packet_filter_hook *pfh, *pfnext;
		
	PFIL_LIST_LOCK();
	/* 
	 * LIST_REMOVE is safe for unlocked pfil_heads in ph_list.
	 * No need to WLOCK all of them.
	 */
	LIST_REMOVE(ph, ph_list);
	PFIL_LIST_UNLOCK();

	PFIL_WLOCK(ph);			/* XXX: may sleep (cv_wait)! */
	
	TAILQ_FOREACH_SAFE(pfh, &ph->ph_in, pfil_link, pfnext)
		free(pfh, M_IFADDR);
	TAILQ_FOREACH_SAFE(pfh, &ph->ph_out, pfil_link, pfnext)
		free(pfh, M_IFADDR);
	cv_destroy(&ph->ph_cv);
	mtx_destroy(&ph->ph_mtx);
	
	return (0);
}

/*
 * pfil_head_get() returns the pfil_head for a given key/dlt.
 */
struct pfil_head *
pfil_head_get(int type, u_long val)
{
	struct pfil_head *ph;

	PFIL_LIST_LOCK();
	LIST_FOREACH(ph, &pfil_head_list, ph_list)
		if (ph->ph_type == type && ph->ph_un.phu_val == val)
			break;
	PFIL_LIST_UNLOCK();
	
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
pfil_add_hook(int (*func)(void *, struct mbuf **, struct ifnet *, int, struct inpcb *),
    void *arg, int flags, struct pfil_head *ph)
{
	struct packet_filter_hook *pfh1 = NULL;
	struct packet_filter_hook *pfh2 = NULL;
	int err;

	/* Get memory */
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

	/* Lock */
	if (flags & PFIL_WAITOK)
		PFIL_WLOCK(ph);
	else {
		err = PFIL_TRY_WLOCK(ph);
		if (err)
			goto error;
	}

	/* Add */
	if (flags & PFIL_IN) {
		pfh1->pfil_func = func;
		pfh1->pfil_arg = arg;
		err = pfil_list_add(&ph->ph_in, pfh1, flags & ~PFIL_OUT);
		if (err)
			goto done;
	}
	if (flags & PFIL_OUT) {
		pfh2->pfil_func = func;
		pfh2->pfil_arg = arg;
		err = pfil_list_add(&ph->ph_out, pfh2, flags & ~PFIL_IN);
		if (err) {
			if (flags & PFIL_IN)
				pfil_list_remove(&ph->ph_in, func, arg);
			goto done;
		}
	}

	ph->ph_busy_count = 0;
	PFIL_WUNLOCK(ph);

	return 0;
done:
	PFIL_WUNLOCK(ph);
error:
	if (pfh1 != NULL)
		free(pfh1, M_IFADDR);
	if (pfh2 != NULL)
		free(pfh2, M_IFADDR);
	return err;
}

/*
 * pfil_remove_hook removes a specific function from the packet filter
 * hook list.
 */
int
pfil_remove_hook(int (*func)(void *, struct mbuf **, struct ifnet *, int, struct inpcb *),
    void *arg, int flags, struct pfil_head *ph)
{
	int err = 0;

	if (flags & PFIL_WAITOK)
		PFIL_WLOCK(ph);
	else {
		err = PFIL_TRY_WLOCK(ph);
		if (err)
			return err;
	}

	if (flags & PFIL_IN)
		err = pfil_list_remove(&ph->ph_in, func, arg);
	if ((err == 0) && (flags & PFIL_OUT))
		err = pfil_list_remove(&ph->ph_out, func, arg);

	if (TAILQ_EMPTY(&ph->ph_in) && TAILQ_EMPTY(&ph->ph_out))
		ph->ph_busy_count = -1;

	PFIL_WUNLOCK(ph);
	
	return err;
}

static int
pfil_list_add(pfil_list_t *list, struct packet_filter_hook *pfh1, int flags)
{
	struct packet_filter_hook *pfh;

	/*
	 * First make sure the hook is not already there.
	 */
	TAILQ_FOREACH(pfh, list, pfil_link)
		if (pfh->pfil_func == pfh1->pfil_func &&
		    pfh->pfil_arg == pfh1->pfil_arg)
			return EEXIST;
	/*
	 * insert the input list in reverse order of the output list
	 * so that the same path is followed in or out of the kernel.
	 */
	if (flags & PFIL_IN)
		TAILQ_INSERT_HEAD(list, pfh1, pfil_link);
	else
		TAILQ_INSERT_TAIL(list, pfh1, pfil_link);

	return 0;
}

/*
 * pfil_list_remove is an internal function that takes a function off the
 * specified list.
 */
static int
pfil_list_remove(pfil_list_t *list,
    int (*func)(void *, struct mbuf **, struct ifnet *, int, struct inpcb *), void *arg)
{
	struct packet_filter_hook *pfh;

	TAILQ_FOREACH(pfh, list, pfil_link)
		if (pfh->pfil_func == func && pfh->pfil_arg == arg) {
			TAILQ_REMOVE(list, pfh, pfil_link);
			free(pfh, M_IFADDR);
			return 0;
		}
	return ENOENT;
}
