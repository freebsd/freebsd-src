/*-
 * Copyright (c) 2021 The FreeBSD Foundation
 * Copyright (c) 2022 Bjoern A. Zeeb
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <linux/bitops.h>
#include <linux/list.h>
#include <linux/netdevice.h>

MALLOC_DEFINE(M_NETDEV, "lkpindev", "Linux KPI netdevice compat");

#define	NAPI_LOCK_INIT(_ndev)		\
    mtx_init(&(_ndev)->napi_mtx, "napi_mtx", NULL, MTX_DEF)
#define	NAPI_LOCK_DESTROY(_ndev)	mtx_destroy(&(_ndev)->napi_mtx)
#define	NAPI_LOCK_ASSERT(_ndev)		mtx_assert(&(_ndev)->napi_mtx, MA_OWNED)
#define	NAPI_LOCK(_ndev)		mtx_lock(&(_ndev)->napi_mtx)
#define	NAPI_UNLOCK(_ndev)		mtx_unlock(&(_ndev)->napi_mtx)

/* -------------------------------------------------------------------------- */

#define LKPI_NAPI_FLAGS \
        "\20\1DISABLE_PENDING\2IS_SCHEDULED\3LOST_RACE_TRY_AGAIN"

/* #define	NAPI_DEBUG */
#ifdef NAPI_DEBUG
static int debug_napi;
SYSCTL_INT(_compat_linuxkpi, OID_AUTO, debug_napi, CTLFLAG_RWTUN,
    &debug_napi, 0, "NAPI debug level");

#define	DNAPI_TODO		0x01
#define	DNAPI_IMPROVE		0x02
#define	DNAPI_TRACE		0x10
#define	DNAPI_TRACE_TASK	0x20
#define	DNAPI_DIRECT_DISPATCH	0x1000

#define	NAPI_TRACE(_n)		if (debug_napi & DNAPI_TRACE)		\
    printf("NAPI_TRACE %s:%d %u %p (%#jx %b)\n", __func__, __LINE__,	\
	(unsigned int)ticks, _n, (uintmax_t)(_n)->state,		\
	(int)(_n)->state, LKPI_NAPI_FLAGS)
#define	NAPI_TRACE2D(_n, _d)	if (debug_napi & DNAPI_TRACE)		\
    printf("NAPI_TRACE %s:%d %u %p (%#jx %b) %d\n", __func__, __LINE__, \
	(unsigned int)ticks, _n, (uintmax_t)(_n)->state,		\
	(int)(_n)->state, LKPI_NAPI_FLAGS, _d)
#define	NAPI_TRACE_TASK(_n, _p, _c) if (debug_napi & DNAPI_TRACE_TASK)	\
    printf("NAPI_TRACE %s:%d %u %p (%#jx %b) pending %d count %d "	\
	"rx_count %d\n", __func__, __LINE__,				\
	(unsigned int)ticks, _n, (uintmax_t)(_n)->state,		\
	(int)(_n)->state, LKPI_NAPI_FLAGS, _p, _c, (_n)->rx_count)
#define	NAPI_TODO()		if (debug_napi & DNAPI_TODO)		\
    printf("NAPI_TODO %s:%d %d\n", __func__, __LINE__, ticks)
#define	NAPI_IMPROVE()		if (debug_napi & DNAPI_IMPROVE)		\
    printf("NAPI_IMPROVE %s:%d %d\n", __func__, __LINE__, ticks)

#define	NAPI_DIRECT_DISPATCH()	((debug_napi & DNAPI_DIRECT_DISPATCH) != 0)
#else
#define	NAPI_TRACE(_n)			do { } while(0)
#define	NAPI_TRACE2D(_n, _d)		do { } while(0)
#define	NAPI_TRACE_TASK(_n, _p, _c)	do { } while(0)
#define	NAPI_TODO()			do { } while(0)
#define	NAPI_IMPROVE()			do { } while(0)

#define	NAPI_DIRECT_DISPATCH()		(0)
#endif

/* -------------------------------------------------------------------------- */

/*
 * Check if a poll is running or can run and and if the latter
 * make us as running.  That way we ensure that only one poll
 * can only ever run at the same time.  Returns true if no poll
 * was scheduled yet.
 */
bool
linuxkpi_napi_schedule_prep(struct napi_struct *napi)
{
	unsigned long old, new;

	NAPI_TRACE(napi);

	/* Can can only update/return if all flags agree. */
	do {
		old = READ_ONCE(napi->state);

		/* If we are stopping, cannot run again. */
		if ((old & BIT(LKPI_NAPI_FLAG_DISABLE_PENDING)) != 0) {
			NAPI_TRACE(napi);
			return (false);
		}

		new = old;
		/* We were already scheduled. Need to try again? */
		if ((old & BIT(LKPI_NAPI_FLAG_IS_SCHEDULED)) != 0)
			new |= BIT(LKPI_NAPI_FLAG_LOST_RACE_TRY_AGAIN);
		new |= BIT(LKPI_NAPI_FLAG_IS_SCHEDULED);

	} while (atomic_cmpset_acq_long(&napi->state, old, new) == 0);

	NAPI_TRACE(napi);
        return ((old & BIT(LKPI_NAPI_FLAG_IS_SCHEDULED)) == 0);
}

static void
lkpi___napi_schedule_dd(struct napi_struct *napi)
{
	unsigned long old, new;
	int rc;

	rc = 0;
again:
	NAPI_TRACE2D(napi, rc);
	if (napi->poll != NULL)
		rc = napi->poll(napi, napi->budget);
	napi->rx_count += rc;

	/* Check if interrupts are still disabled, more work to do. */
	/* Bandaid for now. */
	if (rc >= napi->budget)
		goto again;

	/* Bandaid for now. */
	if (test_bit(LKPI_NAPI_FLAG_LOST_RACE_TRY_AGAIN, &napi->state))
		goto again;

	do {
		new = old = READ_ONCE(napi->state);
		clear_bit(LKPI_NAPI_FLAG_LOST_RACE_TRY_AGAIN, &new);
		clear_bit(LKPI_NAPI_FLAG_IS_SCHEDULED, &new);
	} while (atomic_cmpset_acq_long(&napi->state, old, new) == 0);

	NAPI_TRACE2D(napi, rc);
}

void
linuxkpi___napi_schedule(struct napi_struct *napi)
{
	int rc;

	NAPI_TRACE(napi);
	if (test_bit(LKPI_NAPI_FLAG_SHUTDOWN, &napi->state)) {
		clear_bit(LKPI_NAPI_FLAG_LOST_RACE_TRY_AGAIN, &napi->state);
		clear_bit(LKPI_NAPI_FLAG_IS_SCHEDULED, &napi->state);
		NAPI_TRACE(napi);
		return;
	}

	if (NAPI_DIRECT_DISPATCH()) {
		lkpi___napi_schedule_dd(napi);
	} else {
		rc = taskqueue_enqueue(napi->dev->napi_tq, &napi->napi_task);
		NAPI_TRACE2D(napi, rc);
		if (rc != 0) {
			/* Should we assert EPIPE? */
			return;
		}
	}
}

void
linuxkpi_napi_schedule(struct napi_struct *napi)
{

	NAPI_TRACE(napi);

	/*
	 * iwlwifi calls this sequence instead of napi_schedule()
	 * to be able to test the prep result.
	 */
	if (napi_schedule_prep(napi))
		__napi_schedule(napi);
}

void
linuxkpi_napi_reschedule(struct napi_struct *napi)
{

	NAPI_TRACE(napi);

	/* Not sure what is different to napi_schedule yet. */
	if (napi_schedule_prep(napi))
		__napi_schedule(napi);
}

bool
linuxkpi_napi_complete_done(struct napi_struct *napi, int ret)
{
	unsigned long old, new;

	NAPI_TRACE(napi);
	if (NAPI_DIRECT_DISPATCH())
		return (true);

	do {
		new = old = READ_ONCE(napi->state);

		/*
		 * If we lost a race before, we need to re-schedule.
		 * Leave IS_SCHEDULED set essentially doing "_prep".
		 */
		if (!test_bit(LKPI_NAPI_FLAG_LOST_RACE_TRY_AGAIN, &old))
			clear_bit(LKPI_NAPI_FLAG_IS_SCHEDULED, &new);
		clear_bit(LKPI_NAPI_FLAG_LOST_RACE_TRY_AGAIN, &new);
	} while (atomic_cmpset_acq_long(&napi->state, old, new) == 0);

	NAPI_TRACE(napi);

	/* Someone tried to schedule while poll was running. Re-sched. */
	if (test_bit(LKPI_NAPI_FLAG_LOST_RACE_TRY_AGAIN, &old)) {
		__napi_schedule(napi);
		return (false);
	}

	return (true);
}

bool
linuxkpi_napi_complete(struct napi_struct *napi)
{

	NAPI_TRACE(napi);
	return (napi_complete_done(napi, 0));
}

void
linuxkpi_napi_disable(struct napi_struct *napi)
{
	NAPI_TRACE(napi);
	set_bit(LKPI_NAPI_FLAG_DISABLE_PENDING, &napi->state);
	while (test_bit(LKPI_NAPI_FLAG_IS_SCHEDULED, &napi->state))
		pause_sbt("napidslp", SBT_1MS, 0, C_HARDCLOCK);
	clear_bit(LKPI_NAPI_FLAG_DISABLE_PENDING, &napi->state);
}

void
linuxkpi_napi_enable(struct napi_struct *napi)
{

	NAPI_TRACE(napi);
	KASSERT(!test_bit(LKPI_NAPI_FLAG_IS_SCHEDULED, &napi->state),
	    ("%s: enabling napi %p already scheduled\n", __func__, napi));
	mb();
	/* Let us be scheduled. */
	clear_bit(LKPI_NAPI_FLAG_IS_SCHEDULED, &napi->state);
}

void
linuxkpi_napi_synchronize(struct napi_struct *napi)
{
	NAPI_TRACE(napi);
#if defined(SMP)
	/* Check & sleep while a napi is scheduled. */
	while (test_bit(LKPI_NAPI_FLAG_IS_SCHEDULED, &napi->state))
		pause_sbt("napisslp", SBT_1MS, 0, C_HARDCLOCK);
#else
	mb();
#endif
}

/* -------------------------------------------------------------------------- */

static void
lkpi_napi_task(void *ctx, int pending)
{
	struct napi_struct *napi;
	int count;

	KASSERT(ctx != NULL, ("%s: napi %p, pending %d\n",
	    __func__, ctx, pending));
	napi = ctx;
	KASSERT(napi->poll != NULL, ("%s: napi %p poll is NULL\n",
	    __func__, napi));

	NAPI_TRACE_TASK(napi, pending, napi->budget);
	count = napi->poll(napi, napi->budget);
	napi->rx_count += count;
	NAPI_TRACE_TASK(napi, pending, count);

	/*
	 * We must not check against count < pending here.  There are situations
	 * when a driver may "poll" and we may not have any work to do and that
	 * would make us re-schedule ourseless for ever.
	 */
	if (count >= napi->budget) {
		/*
		 * Have to re-schedule ourselves.  napi_complete() was not run
		 * in this case which means we are still SCHEDULED.
		 * In order to queue another task we have to directly call
		 * __napi_schedule() without _prep() in the way.
		 */
		__napi_schedule(napi);
	}
}

/* -------------------------------------------------------------------------- */

void
linuxkpi_netif_napi_add(struct net_device *ndev, struct napi_struct *napi,
    int(*napi_poll)(struct napi_struct *, int))
{

	napi->dev = ndev;
	napi->poll = napi_poll;
	napi->budget = NAPI_POLL_WEIGHT;

	INIT_LIST_HEAD(&napi->rx_list);
	napi->rx_count = 0;

	TASK_INIT(&napi->napi_task, 0, lkpi_napi_task, napi);

	NAPI_LOCK(ndev);
	TAILQ_INSERT_TAIL(&ndev->napi_head, napi, entry);
	NAPI_UNLOCK(ndev);

	/* Anything else to do on the ndev? */
	clear_bit(LKPI_NAPI_FLAG_SHUTDOWN, &napi->state);
}

static void
lkpi_netif_napi_del_locked(struct napi_struct *napi)
{
	struct net_device *ndev;

	ndev = napi->dev;
	NAPI_LOCK_ASSERT(ndev);

	set_bit(LKPI_NAPI_FLAG_SHUTDOWN, &napi->state);
	TAILQ_REMOVE(&ndev->napi_head, napi, entry);
	while (taskqueue_cancel(ndev->napi_tq, &napi->napi_task, NULL) != 0)
		taskqueue_drain(ndev->napi_tq, &napi->napi_task);
}

void
linuxkpi_netif_napi_del(struct napi_struct *napi)
{
	struct net_device *ndev;

	ndev = napi->dev;
	NAPI_LOCK(ndev);
	lkpi_netif_napi_del_locked(napi);
	NAPI_UNLOCK(ndev);
}

/* -------------------------------------------------------------------------- */

void
linuxkpi_init_dummy_netdev(struct net_device *ndev)
{

	memset(ndev, 0, sizeof(*ndev));

	ndev->reg_state = NETREG_DUMMY;
	NAPI_LOCK_INIT(ndev);
	TAILQ_INIT(&ndev->napi_head);
	/* Anything else? */

	ndev->napi_tq = taskqueue_create("tq_ndev_napi", M_WAITOK,
	    taskqueue_thread_enqueue, &ndev->napi_tq);
	/* One thread for now. */
	(void) taskqueue_start_threads(&ndev->napi_tq, 1, PWAIT,
	    "ndev napi taskq");
}

struct net_device *
linuxkpi_alloc_netdev(size_t len, const char *name, uint32_t flags,
    void(*setup_func)(struct net_device *))
{
	struct net_device *ndev;

	ndev = malloc(sizeof(*ndev) + len, M_NETDEV, M_NOWAIT);
	if (ndev == NULL)
		return (ndev);

	/* Always first as it zeros! */
	linuxkpi_init_dummy_netdev(ndev);

	strlcpy(ndev->name, name, sizeof(*ndev->name));

	/* This needs extending as we support more. */

	setup_func(ndev);

	return (ndev);
}

void
linuxkpi_free_netdev(struct net_device *ndev)
{
	struct napi_struct *napi, *temp;

	NAPI_LOCK(ndev);
	TAILQ_FOREACH_SAFE(napi, &ndev->napi_head, entry, temp) {
		lkpi_netif_napi_del_locked(napi);
	}
	NAPI_UNLOCK(ndev);

	taskqueue_free(ndev->napi_tq);
	ndev->napi_tq = NULL;
	NAPI_LOCK_DESTROY(ndev);

	/* This needs extending as we support more. */

	free(ndev, M_NETDEV);
}
