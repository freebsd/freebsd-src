/*-
 * Copyright (c) 2010 Lawrence Stewart <lstewart@freebsd.org>
 * Copyright (c) 2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Lawrence Stewart while studying at the Centre
 * for Advanced Internet Architectures, Swinburne University, made possible in
 * part by grants from the FreeBSD Foundation and Cisco University Research
 * Program Fund at Community Foundation Silicon Valley.
 *
 * Portions of this software were developed at the Centre for Advanced
 * Internet Architectures, Swinburne University of Technology, Melbourne,
 * Australia by Lawrence Stewart under sponsorship from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/kernel.h>
#include <sys/hhook.h>
#include <sys/khelp.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/module_khelp.h>
#include <sys/osd.h>
#include <sys/queue.h>
#include <sys/refcount.h>
#include <sys/systm.h>

#include <net/vnet.h>

struct hhook {
	hhook_func_t		hhk_func;
	struct helper		*hhk_helper;
	void			*hhk_udata;
	STAILQ_ENTRY(hhook)	hhk_next;
};

MALLOC_DECLARE(M_HHOOK);
MALLOC_DEFINE(M_HHOOK, "hhook", "Helper hooks are linked off hhook_head lists");

LIST_HEAD(hhookheadhead, hhook_head);
VNET_DEFINE(struct hhookheadhead, hhook_head_list);
#define	V_hhook_head_list VNET(hhook_head_list)

static struct mtx hhook_head_list_lock;
MTX_SYSINIT(hhookheadlistlock, &hhook_head_list_lock, "hhook_head list lock",
    MTX_DEF);

/* Private function prototypes. */
static void hhook_head_destroy(struct hhook_head *hhh);

#define	HHHLIST_LOCK() mtx_lock(&hhook_head_list_lock)
#define	HHHLIST_UNLOCK() mtx_unlock(&hhook_head_list_lock)
#define	HHHLIST_LOCK_ASSERT() mtx_assert(&hhook_head_list_lock, MA_OWNED)

#define	HHH_LOCK_INIT(hhh) rm_init(&(hhh)->hhh_lock, "hhook_head rm lock")
#define	HHH_LOCK_DESTROY(hhh) rm_destroy(&(hhh)->hhh_lock)
#define	HHH_WLOCK(hhh) rm_wlock(&(hhh)->hhh_lock)
#define	HHH_WUNLOCK(hhh) rm_wunlock(&(hhh)->hhh_lock)
#define	HHH_RLOCK(hhh, rmpt) rm_rlock(&(hhh)->hhh_lock, (rmpt))
#define	HHH_RUNLOCK(hhh, rmpt) rm_runlock(&(hhh)->hhh_lock, (rmpt))

/*
 * Run all helper hook functions for a given hook point.
 */
void
hhook_run_hooks(struct hhook_head *hhh, void *ctx_data, struct osd *hosd)
{
	struct hhook *hhk;
	void *hdata;
	struct rm_priotracker rmpt;

	KASSERT(hhh->hhh_refcount > 0, ("hhook_head %p refcount is 0", hhh));

	HHH_RLOCK(hhh, &rmpt);
	STAILQ_FOREACH(hhk, &hhh->hhh_hooks, hhk_next) {
		if (hhk->hhk_helper->h_flags & HELPER_NEEDS_OSD) {
			hdata = osd_get(OSD_KHELP, hosd, hhk->hhk_helper->h_id);
			if (hdata == NULL)
				continue;
		} else
			hdata = NULL;

		/*
		 * XXXLAS: We currently ignore the int returned by the hook,
		 * but will likely want to handle it in future to allow hhook to
		 * be used like pfil and effect changes at the hhook calling
		 * site e.g. we could define a new hook type of HHOOK_TYPE_PFIL
		 * and standardise what particular return values mean and set
		 * the context data to pass exactly the same information as pfil
		 * hooks currently receive, thus replicating pfil with hhook.
		 */
		hhk->hhk_func(hhh->hhh_type, hhh->hhh_id, hhk->hhk_udata,
		    ctx_data, hdata, hosd);
	}
	HHH_RUNLOCK(hhh, &rmpt);
}

/*
 * Register a new helper hook function with a helper hook point.
 */
int
hhook_add_hook(struct hhook_head *hhh, struct hookinfo *hki, uint32_t flags)
{
	struct hhook *hhk, *tmp;
	int error;

	error = 0;

	if (hhh == NULL)
		return (ENOENT);

	hhk = malloc(sizeof(struct hhook), M_HHOOK,
	    M_ZERO | ((flags & HHOOK_WAITOK) ? M_WAITOK : M_NOWAIT));

	if (hhk == NULL)
		return (ENOMEM);

	hhk->hhk_helper = hki->hook_helper;
	hhk->hhk_func = hki->hook_func;
	hhk->hhk_udata = hki->hook_udata;

	HHH_WLOCK(hhh);
	STAILQ_FOREACH(tmp, &hhh->hhh_hooks, hhk_next) {
		if (tmp->hhk_func == hki->hook_func &&
		    tmp->hhk_udata == hki->hook_udata) {
			/* The helper hook function is already registered. */
			error = EEXIST;
			break;
		}
	}

	if (!error) {
		STAILQ_INSERT_TAIL(&hhh->hhh_hooks, hhk, hhk_next);
		hhh->hhh_nhooks++;
	}
	else
		free(hhk, M_HHOOK);

	HHH_WUNLOCK(hhh);

	return (error);
}

/*
 * Lookup a helper hook point and register a new helper hook function with it.
 */
int
hhook_add_hook_lookup(struct hookinfo *hki, uint32_t flags)
{
	struct hhook_head *hhh;
	int error;

	hhh = hhook_head_get(hki->hook_type, hki->hook_id);

	if (hhh == NULL)
		return (ENOENT);

	error = hhook_add_hook(hhh, hki, flags);
	hhook_head_release(hhh);

	return (error);
}

/*
 * Remove a helper hook function from a helper hook point.
 */
int
hhook_remove_hook(struct hhook_head *hhh, struct hookinfo *hki)
{
	struct hhook *tmp;

	if (hhh == NULL)
		return (ENOENT);

	HHH_WLOCK(hhh);
	STAILQ_FOREACH(tmp, &hhh->hhh_hooks, hhk_next) {
		if (tmp->hhk_func == hki->hook_func &&
		    tmp->hhk_udata == hki->hook_udata) {
			STAILQ_REMOVE(&hhh->hhh_hooks, tmp, hhook, hhk_next);
			free(tmp, M_HHOOK);
			hhh->hhh_nhooks--;
			break;
		}
	}
	HHH_WUNLOCK(hhh);

	return (0);
}

/*
 * Lookup a helper hook point and remove a helper hook function from it.
 */
int
hhook_remove_hook_lookup(struct hookinfo *hki)
{
	struct hhook_head *hhh;

	hhh = hhook_head_get(hki->hook_type, hki->hook_id);

	if (hhh == NULL)
		return (ENOENT);

	hhook_remove_hook(hhh, hki);
	hhook_head_release(hhh);

	return (0);
}

/*
 * Register a new helper hook point.
 */
int
hhook_head_register(int32_t hhook_type, int32_t hhook_id, struct hhook_head **hhh,
    uint32_t flags)
{
	struct hhook_head *tmphhh;

	tmphhh = hhook_head_get(hhook_type, hhook_id);

	if (tmphhh != NULL) {
		/* Hook point previously registered. */
		hhook_head_release(tmphhh);
		return (EEXIST);
	}

	/* XXXLAS: Need to implement support for non-virtualised hooks. */
	if ((flags & HHOOK_HEADISINVNET) == 0) {
		printf("%s: only vnet-style virtualised hooks can be used\n",
		    __func__);
		return (EINVAL);
	}

	tmphhh = malloc(sizeof(struct hhook_head), M_HHOOK,
	    M_ZERO | ((flags & HHOOK_WAITOK) ? M_WAITOK : M_NOWAIT));

	if (tmphhh == NULL)
		return (ENOMEM);

	tmphhh->hhh_type = hhook_type;
	tmphhh->hhh_id = hhook_id;
	tmphhh->hhh_nhooks = 0;
	STAILQ_INIT(&tmphhh->hhh_hooks);
	HHH_LOCK_INIT(tmphhh);

	if (hhh != NULL)
		refcount_init(&tmphhh->hhh_refcount, 1);
	else
		refcount_init(&tmphhh->hhh_refcount, 0);

	if (flags & HHOOK_HEADISINVNET) {
		tmphhh->hhh_flags |= HHH_ISINVNET;
		HHHLIST_LOCK();
		LIST_INSERT_HEAD(&V_hhook_head_list, tmphhh, hhh_next);
		HHHLIST_UNLOCK();
	} else {
		/* XXXLAS: Add tmphhh to the non-virtualised list. */
	}

	*hhh = tmphhh;

	return (0);
}

static void
hhook_head_destroy(struct hhook_head *hhh)
{
	struct hhook *tmp, *tmp2;

	HHHLIST_LOCK_ASSERT();

	LIST_REMOVE(hhh, hhh_next);
	HHH_WLOCK(hhh);
	STAILQ_FOREACH_SAFE(tmp, &hhh->hhh_hooks, hhk_next, tmp2)
		free(tmp, M_HHOOK);
	HHH_WUNLOCK(hhh);
	HHH_LOCK_DESTROY(hhh);
	free(hhh, M_HHOOK);
}

/*
 * Remove a helper hook point.
 */
int
hhook_head_deregister(struct hhook_head *hhh)
{
	int error;

	error = 0;

	HHHLIST_LOCK();
	if (hhh == NULL)
		error = ENOENT;
	else if (hhh->hhh_refcount > 1)
		error = EBUSY;
	else
		hhook_head_destroy(hhh);
	HHHLIST_UNLOCK();

	return (error);
}

/*
 * Remove a helper hook point via a hhook_head lookup.
 */
int
hhook_head_deregister_lookup(int32_t hhook_type, int32_t hhook_id)
{
	struct hhook_head *hhh;
	int error;

	error = 0;
	hhh = hhook_head_get(hhook_type, hhook_id);
	error = hhook_head_deregister(hhh);

	if (error == EBUSY)
		hhook_head_release(hhh);

	return (error);
}

/*
 * Lookup and return the hhook_head struct associated with the specified type
 * and id, or NULL if not found. If found, the hhook_head's refcount is bumped.
 */
struct hhook_head *
hhook_head_get(int32_t hhook_type, int32_t hhook_id)
{
	struct hhook_head *hhh;

	/* XXXLAS: Pick hhook_head_list based on hhook_head flags. */
	HHHLIST_LOCK();
	LIST_FOREACH(hhh, &V_hhook_head_list, hhh_next) {
		if (hhh->hhh_type == hhook_type && hhh->hhh_id == hhook_id) {
			refcount_acquire(&hhh->hhh_refcount);
			HHHLIST_UNLOCK();
			return (hhh);
		}
	}
	HHHLIST_UNLOCK();

	return (NULL);
}

void
hhook_head_release(struct hhook_head *hhh)
{

	refcount_release(&hhh->hhh_refcount);
}

/*
 * Check the hhook_head private flags and return the appropriate public
 * representation of the flag to the caller. The function is implemented in a
 * way that allows us to cope with other subsystems becoming virtualised in the
 * future.
 */
uint32_t
hhook_head_is_virtualised(struct hhook_head *hhh)
{
	uint32_t ret;

	if (hhh == NULL)
		return (0);

	if (hhh->hhh_flags & HHH_ISINVNET)
		ret = HHOOK_HEADISINVNET;

	return (ret);
}

uint32_t
hhook_head_is_virtualised_lookup(int32_t hook_type, int32_t hook_id)
{
	struct hhook_head *hhh;
	uint32_t ret;

	hhh = hhook_head_get(hook_type, hook_id);

	if (hhh == NULL)
		return (0);

	ret = hhook_head_is_virtualised(hhh);
	hhook_head_release(hhh);

	return (ret);
}

/*
 * Vnet created and being initialised.
 */
static void
hhook_vnet_init(const void *unused __unused)
{

	LIST_INIT(&V_hhook_head_list);
}

/*
 * Vnet being torn down and destroyed.
 */
static void
hhook_vnet_uninit(const void *unused __unused)
{
	struct hhook_head *hhh, *tmphhh;

	/*
	 * If subsystems which export helper hook points use the hhook KPI
	 * correctly, the loop below should have no work to do because the
	 * subsystem should have already called hhook_head_deregister().
	 */
	HHHLIST_LOCK();
	LIST_FOREACH_SAFE(hhh, &V_hhook_head_list, hhh_next, tmphhh) {
		printf("%s: hhook_head type=%d, id=%d cleanup required\n",
		    __func__, hhh->hhh_type, hhh->hhh_id);
		hhook_head_destroy(hhh);
	}
	HHHLIST_UNLOCK();
}


/*
 * When a vnet is created and being initialised, init the V_hhook_head_list.
 */
VNET_SYSINIT(hhook_vnet_init, SI_SUB_PROTO_BEGIN, SI_ORDER_FIRST,
    hhook_vnet_init, NULL);

/*
 * The hhook KPI provides a mechanism for subsystems which export helper hook
 * points to clean up on vnet tear down, but in case the KPI is misused,
 * provide a function to clean up and free memory for a vnet being destroyed.
 */
VNET_SYSUNINIT(hhook_vnet_uninit, SI_SUB_PROTO_BEGIN, SI_ORDER_FIRST,
    hhook_vnet_uninit, NULL);
