/*-
 * Copyright (c) 2010 Lawrence Stewart <lstewart@freebsd.org>
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University, by Lawrence Stewart,
 * made possible in part by a grant from the FreeBSD Foundation.
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
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/rmlock.h>
#include <sys/systm.h>

#include <net/vnet.h>

#include <netinet/helper.h>
#include <netinet/hhooks.h>

#define	RLOCK_HHOOK_HEAD	0x01
#define	WLOCK_HHOOK_HEAD	0x02

MALLOC_DECLARE(M_HHOOK);
MALLOC_DEFINE(M_HHOOK, "helper hook related memory", "Blah");

struct hhook {
	hhook_func_t h_func;
	void	*h_udata;
	struct helper *h_helper;
        STAILQ_ENTRY(hhook) h_next;
};

typedef	STAILQ_HEAD(hhook_list, hhook) hhook_list_t;

struct hhook_head {
	int	hh_type;
	int	hh_id;
	int	hh_nhooks;
	hhook_list_t	hh_hooks;
	struct rmlock	hh_lock;
	LIST_ENTRY(hhook_head) hh_next;
};

LIST_HEAD(hhookheadhead, hhook_head);
VNET_DEFINE(struct hhookheadhead, hhook_head_list);
#define	V_hhook_head_list	VNET(hhook_head_list)

static struct mtx hhook_head_list_lock;
MTX_SYSINIT(hhookheadlistlock, &hhook_head_list_lock, "hhook_head list lock",
    MTX_DEF);

static struct	hhook_head *	get_hhook_head(int hhook_type, int hhook_id,
    struct rm_priotracker* rmpt, int flags);


/*
 * Public KPI functions
 */
int
register_hhook_head(int hhook_type, int hhook_id, int flags)
{
	struct hhook_head *hh;

	HHOOK_HEAD_LIST_LOCK();
	hh = get_hhook_head(hhook_type, hhook_id, NULL, 0);

	if (hh != NULL)
		return (EEXIST);

	hh = malloc(sizeof(struct hhook_head), M_HHOOK,
	    M_ZERO | ((flags & HHOOK_WAITOK) ? M_WAITOK : M_NOWAIT));

	if (hh == NULL)
		return (ENOMEM);

	printf("About to register hhook_head %p with type: %d and id: %d\n", hh,
	hhook_type, hhook_id);

	hh->hh_type = hhook_type;
	hh->hh_id = hhook_id;
	hh->hh_nhooks = 0;
	STAILQ_INIT(&hh->hh_hooks);
	HHOOK_HEAD_LOCK_INIT(hh);

	LIST_INSERT_HEAD(&V_hhook_head_list, hh, hh_next);
	HHOOK_HEAD_LIST_UNLOCK();
	return (0);
}

int
deregister_hhook_head(int hhook_type, int hhook_id)
{
	struct hhook_head *hh;
	struct hhook *tmp, *tmp2;
	int error = 0;

	HHOOK_HEAD_LIST_LOCK();
	hh = get_hhook_head(hhook_type, hhook_id, NULL, WLOCK_HHOOK_HEAD);

	if (hh == NULL)
		error = ENOENT;
	else {
		LIST_REMOVE(hh, hh_next);

		STAILQ_FOREACH_SAFE(tmp, &hh->hh_hooks, h_next, tmp2) {
			free(tmp, M_HHOOK);
		}

		HHOOK_HEAD_WUNLOCK(hh);
		HHOOK_HEAD_LOCK_DESTROY(hh);
		free(hh, M_HHOOK);
	}

	HHOOK_HEAD_LIST_UNLOCK();
	return (error);
}

int
register_hhook(int hhook_type, int hhook_id, struct helper *helper,
    hhook_func_t hook, void *udata, int flags)
{
	struct hhook *h, *tmp;
	struct hhook_head *hh;
	int error = 0;

	h = malloc(sizeof(struct hhook), M_HHOOK,
	    M_ZERO | ((flags & HHOOK_WAITOK) ? M_WAITOK : M_NOWAIT));

	if (h == NULL)
		return (ENOMEM);

	h->h_helper = helper;
	h->h_func = hook;
	h->h_udata = udata;

	hh = get_hhook_head(hhook_type, hhook_id, NULL, WLOCK_HHOOK_HEAD);

	if (hh == NULL) {
		free(h, M_HHOOK);
		return (ENOENT);
	}

	STAILQ_FOREACH(tmp, &hh->hh_hooks, h_next) {
		if (tmp->h_func == hook && tmp->h_udata == udata) {
			error = EEXIST;
			break;
		}
	}

	if (!error) {
		STAILQ_INSERT_TAIL(&hh->hh_hooks, h, h_next);
		hh->hh_nhooks++;
	}
	else
		free(h, M_HHOOK);

	HHOOK_HEAD_WUNLOCK(hh);

	return (error);
}

int
deregister_hhook(int hhook_type, int hhook_id, hhook_func_t hook, void *udata,
    int flags)
{
	struct hhook *tmp;
	struct hhook_head *hh;

	hh = get_hhook_head(hhook_type, hhook_id, NULL, WLOCK_HHOOK_HEAD);

	if (hh == NULL)
		return (ENOENT);

	STAILQ_FOREACH(tmp, &hh->hh_hooks, h_next) {
		if (tmp->h_func == hook && tmp->h_udata == udata) {
			STAILQ_REMOVE(&hh->hh_hooks, tmp, hhook, h_next);
			free(tmp, M_HHOOK);
			hh->hh_nhooks--;
			break;
		}
	}

	HHOOK_HEAD_WUNLOCK(hh);
	return (0);
}

void
run_hhooks(int hhook_type, int hhook_id, void *ctx_data,
    struct helper_dblock *dblocks, int n_dblocks)
{
	struct hhook_head *hh;
	struct hhook *tmp;
	struct rm_priotracker rmpt;
	int i = 0;
	void *dblock = NULL;

	hh = get_hhook_head(hhook_type, hhook_id, &rmpt, RLOCK_HHOOK_HEAD);

	if (hh == NULL)
		return;

	STAILQ_FOREACH(tmp, &hh->hh_hooks, h_next) {
		printf("Running hook %p for helper %d\n", tmp,
		tmp->h_helper->id);
		if (tmp->h_helper->flags & HELPER_NEEDS_DBLOCK) {
			if (n_dblocks == 0
			    || i >= n_dblocks
			    || tmp->h_helper->id != dblocks[i].id)
				continue;
			dblock = dblocks[i].block;
			i++;
		}
		tmp->h_func(tmp->h_udata, ctx_data, dblock);
		dblock = NULL;
	}

	HHOOK_HEAD_RUNLOCK(hh, &rmpt);
}


/*
 * Private KPI functions
 */
static struct hhook_head *
get_hhook_head(int hhook_type, int hhook_id, struct rm_priotracker *rmpt,
    int flags)
{
	struct hhook_head *tmp, *ret = NULL;

	/*KASSERT(HHOOK_HEAD_LIST_LOCK_ASSERT(), ("hhook_head_list_lock not
	 * locked"));*/

	LIST_FOREACH(tmp, &V_hhook_head_list, hh_next) {
		if (tmp->hh_type == hhook_type && tmp->hh_id == hhook_id) {
			ret = tmp;
			if (flags & RLOCK_HHOOK_HEAD)
				HHOOK_HEAD_RLOCK(ret, rmpt);
			else if (flags & WLOCK_HHOOK_HEAD)
				HHOOK_HEAD_WLOCK(ret);
			break;
		}
	}

	return (ret);
}

static int
vnet_hhook_init(const void *unused)
{

	LIST_INIT(&V_hhook_head_list);
	return (0);
}

static int
vnet_hhook_uninit(const void *unused)
{

	return (0);
}

#define	HHOOK_SYSINIT_ORDER	SI_SUB_PROTO_BEGIN
#define	HHOOK_MODEVENT_ORDER	(SI_ORDER_FIRST) 
#define	HHOOK_VNET_ORDER	(HHOOK_MODEVENT_ORDER + 2) 

VNET_SYSINIT(vnet_hhook_init, HHOOK_SYSINIT_ORDER, HHOOK_VNET_ORDER,
    vnet_hhook_init, NULL);
 
VNET_SYSUNINIT(vnet_hhook_uninit, HHOOK_SYSINIT_ORDER, HHOOK_VNET_ORDER,
    vnet_hhook_uninit, NULL);

