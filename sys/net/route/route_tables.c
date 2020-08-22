/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1980, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
/************************************************************************
 * Note: In this file a 'fib' is a "forwarding information base"	*
 * Which is the new name for an in kernel routing (next hop) table.	*
 ***********************************************************************/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include "opt_route.h"

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/jail.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/sx.h>
#include <sys/domain.h>
#include <sys/sysproto.h>

#include <net/vnet.h>
#include <net/route.h>
#include <net/route/route_var.h>

/* Kernel config default option. */
#ifdef ROUTETABLES
#if ROUTETABLES <= 0
#error "ROUTETABLES defined too low"
#endif
#if ROUTETABLES > RT_MAXFIBS
#error "ROUTETABLES defined too big"
#endif
#define	RT_NUMFIBS	ROUTETABLES
#endif /* ROUTETABLES */
/* Initialize to default if not otherwise set. */
#ifndef	RT_NUMFIBS
#define	RT_NUMFIBS	1
#endif

static void grow_rtables(uint32_t num_fibs);

VNET_DEFINE_STATIC(struct sx, rtables_lock);
#define	V_rtables_lock		VNET(rtables_lock)
#define	RTABLES_LOCK()		sx_xlock(&V_rtables_lock)
#define	RTABLES_UNLOCK()	sx_xunlock(&V_rtables_lock)
#define	RTABLES_LOCK_INIT()	sx_init(&V_rtables_lock, "rtables lock")
#define	RTABLES_LOCK_ASSERT()	sx_assert(&V_rtables_lock, SA_LOCKED)

VNET_DEFINE_STATIC(struct rib_head **, rt_tables);
#define	V_rt_tables	VNET(rt_tables)

VNET_DEFINE(uint32_t, _rt_numfibs) = RT_NUMFIBS;

/*
 * Handler for net.my_fibnum.
 * Returns current fib of the process.
 */
static int
sysctl_my_fibnum(SYSCTL_HANDLER_ARGS)
{
        int fibnum;
        int error;

        fibnum = curthread->td_proc->p_fibnum;
        error = sysctl_handle_int(oidp, &fibnum, 0, req);
        return (error);
}
SYSCTL_PROC(_net, OID_AUTO, my_fibnum,
    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    &sysctl_my_fibnum, "I",
    "default FIB of caller");

static uint32_t
normalize_num_rtables(uint32_t num_rtables)
{

	if (num_rtables > RT_MAXFIBS)
		num_rtables = RT_MAXFIBS;
	else if (num_rtables == 0)
		num_rtables = 1;
	return (num_rtables);
}

/*
 * Sets the number of fibs in the current vnet.
 * Function does not allow shrinking number of rtables.
 */
static int
sysctl_fibs(SYSCTL_HANDLER_ARGS)
{
	uint32_t new_fibs;
	int error;

	RTABLES_LOCK();
	new_fibs = V_rt_numfibs;
	error = sysctl_handle_32(oidp, &new_fibs, 0, req);
	if (error == 0) {
		new_fibs = normalize_num_rtables(new_fibs);

		if (new_fibs < V_rt_numfibs)
			error = ENOTCAPABLE;
		if (new_fibs > V_rt_numfibs)
			grow_rtables(new_fibs);
	}
	RTABLES_UNLOCK();

	return (error);
}
SYSCTL_PROC(_net, OID_AUTO, fibs,
    CTLFLAG_VNET | CTLTYPE_U32 | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, NULL, 0,
    &sysctl_fibs, "IU",
    "set number of fibs");

/*
 * Sets fib of a current process.
 */
int
sys_setfib(struct thread *td, struct setfib_args *uap)
{
	int error = 0;

	CURVNET_SET(TD_TO_VNET(td));
	if (uap->fibnum >= 0 && uap->fibnum < V_rt_numfibs)
		td->td_proc->p_fibnum = uap->fibnum;
	else
		error = EINVAL;
	CURVNET_RESTORE();

	return (error);
}

/*
 * Grows up the number of routing tables in the current fib.
 * Function creates new index array for all rtables and allocates
 *  remaining routing tables.
 */
static void
grow_rtables(uint32_t num_tables)
{
	struct domain *dom;
	struct rib_head **prnh;
	struct rib_head **new_rt_tables, **old_rt_tables;
	int family;

	RTABLES_LOCK_ASSERT();

	KASSERT(num_tables >= V_rt_numfibs, ("num_tables(%u) < rt_numfibs(%u)\n",
				num_tables, V_rt_numfibs));

	new_rt_tables = mallocarray(num_tables * (AF_MAX + 1), sizeof(void *),
	    M_RTABLE, M_WAITOK | M_ZERO);

	/*
	 * Current rt_tables layout:
	 * fib0[af0, af1, af2, .., AF_MAX]fib1[af0, af1, af2, .., Af_MAX]..
	 * this allows to copy existing tables data by using memcpy()
	 */
	if (V_rt_tables != NULL)
		memcpy(new_rt_tables, V_rt_tables,
		    V_rt_numfibs * (AF_MAX + 1) * sizeof(void *));

	/* Populate the remainders */
	for (dom = domains; dom; dom = dom->dom_next) {
		if (dom->dom_rtattach == NULL)
			continue;
		family = dom->dom_family;
		for (int i = 0; i < num_tables; i++) {
			prnh = &new_rt_tables[i * (AF_MAX + 1) + family];
			if (*prnh != NULL)
				continue;
			*prnh = dom->dom_rtattach(i);
			if (*prnh == NULL)
				log(LOG_ERR, "unable to create routing tables for domain %d\n",
				    dom->dom_family);
		}
	}

	/*
	 * Update rtables pointer.
	 * Ensure all writes to new_rt_tables has been completed before
	 *  switching pointer.
	 */
	atomic_thread_fence_rel();
	old_rt_tables = V_rt_tables;
	V_rt_tables = new_rt_tables;

	/* Wait till all cpus see new pointers */
	atomic_thread_fence_rel();
	epoch_wait_preempt(net_epoch_preempt);

	/* Finally, set number of fibs to a new value */
	V_rt_numfibs = num_tables;

	if (old_rt_tables != NULL)
		free(old_rt_tables, M_RTABLE);
}

static void
vnet_rtables_init(const void *unused __unused)
{
	int num_rtables_base;

	if (IS_DEFAULT_VNET(curvnet)) {
		num_rtables_base = RT_NUMFIBS;
		TUNABLE_INT_FETCH("net.fibs", &num_rtables_base);
		V_rt_numfibs = normalize_num_rtables(num_rtables_base);
	} else
		V_rt_numfibs = 1;

	vnet_rtzone_init();

	RTABLES_LOCK_INIT();

	RTABLES_LOCK();
	grow_rtables(V_rt_numfibs);
	RTABLES_UNLOCK();
}
VNET_SYSINIT(vnet_rtables_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_FOURTH,
    vnet_rtables_init, 0);

#ifdef VIMAGE
static void
rtables_destroy(const void *unused __unused)
{
	struct rib_head *rnh;
	struct domain *dom;
	int family;

	RTABLES_LOCK();
	for (dom = domains; dom; dom = dom->dom_next) {
		if (dom->dom_rtdetach == NULL)
			continue;
		family = dom->dom_family;
		for (int i = 0; i < V_rt_numfibs; i++) {
			rnh = rt_tables_get_rnh(i, family);
			dom->dom_rtdetach(rnh);
		}
	}
	RTABLES_UNLOCK();

	/*
	 * dom_rtdetach calls rt_table_destroy(), which
	 *  schedules deletion for all rtentries, nexthops and control
	 *  structures. Wait for the destruction callbacks to fire.
	 * Note that this should result in freeing all rtentries, but
	 *  nexthops deletions will be scheduled for the next epoch run
	 *  and will be completed after vnet teardown.
	 */
	epoch_drain_callbacks(net_epoch_preempt);

	free(V_rt_tables, M_RTABLE);
	vnet_rtzone_destroy();
}
VNET_SYSUNINIT(rtables_destroy, SI_SUB_PROTO_DOMAIN, SI_ORDER_FIRST,
    rtables_destroy, 0);
#endif

static inline struct rib_head *
rt_tables_get_rnh_ptr(uint32_t table, sa_family_t family)
{
	struct rib_head **prnh;

	KASSERT(table < V_rt_numfibs,
	    ("%s: table out of bounds (%d < %d)", __func__, table,
	     V_rt_numfibs));
	KASSERT(family < (AF_MAX + 1),
	    ("%s: fam out of bounds (%d < %d)", __func__, family, AF_MAX + 1));

	/* rnh is [fib=0][af=0]. */
	prnh = V_rt_tables;
	/* Get the offset to the requested table and fam. */
	prnh += table * (AF_MAX + 1) + family;

	return (*prnh);
}

struct rib_head *
rt_tables_get_rnh(uint32_t table, sa_family_t family)
{

	return (rt_tables_get_rnh_ptr(table, family));
}

u_int
rt_tables_get_gen(uint32_t table, sa_family_t family)
{
	struct rib_head *rnh;

	rnh = rt_tables_get_rnh_ptr(table, family);
	KASSERT(rnh != NULL, ("%s: NULL rib_head pointer table %d family %d",
	    __func__, table, family));
	return (rnh->rnh_gen);
}

