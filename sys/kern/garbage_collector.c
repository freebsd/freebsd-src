/*-
 * * Copyright (c) 2012, by Adara Networks. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Adara Networks,nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/garbage_collector.h>


static struct mtx gc_mtx;
static struct garbage_list gc_list;
static struct callout gc_timer;
static uint8_t gc_running = 0;
static uint8_t gc_inited = 0;


#define GC_LOCK() mtx_lock(&gc_mtx)
#define GC_UNLOCK() mtx_unlock(&gc_mtx)


MALLOC_DEFINE(M_GARBAGE, "gc_temp_mem", "Space for garbage before deleting");

static void
garbage_init(void)
{
	TAILQ_INIT(&gc_list);
	mtx_init(&gc_mtx, "garbage_collector_lock", "gc_lock", MTX_DEF);
	gc_running = 0;
	gc_inited = 1;
	callout_init(&gc_timer, 1);
}


SYSINIT(garbage_collect,
    SI_SUB_PROTO_END,
    SI_ORDER_ANY, garbage_init, NULL);


static void
gc_time_out(void *notused)
{
	struct garbage *entry, *tentry;
	struct timeval now;
	struct garbage_list loc_gc_list;

	TAILQ_INIT(&loc_gc_list);
	GC_LOCK();
	if (callout_pending(&gc_timer)) {
		/* Callout has been rescheduled */
		GC_UNLOCK();
		return;
	}
	if (!callout_active(&gc_timer)) {
		/* The callout has been stopped */
		GC_UNLOCK();
		return;
	}
	callout_deactivate(&gc_timer);
	getmicrouptime(&now);
	gc_running = 0;
	TAILQ_FOREACH_SAFE(entry, &gc_list, next, tentry) {
		if (timevalcmp(&now, &entry->purge_time, >)) {
			/* Ok we can run the purge on this one */
			TAILQ_REMOVE(&gc_list, entry, next);
			TAILQ_INSERT_TAIL(&loc_gc_list, entry, next);
		} else {
			/* We will find no more */
			break;
		}
	}
	GC_UNLOCK();
	TAILQ_FOREACH_SAFE(entry, &loc_gc_list, next, tentry) {
		garbage_func gf;

		TAILQ_REMOVE(&loc_gc_list, entry, next);
		entry->purged_at = now; /* for debugging */
		gf = entry->gf;
		if (gf) {
			/* It should not be 0 */
#ifdef INVARIANTS
			if ((void *)gf != (void *)0xdeadc0dedeadc0de) {
				entry->gf = NULL;
				gf(entry->junk);
			} else {
				printf("gc found deleted entry dead-code\n");
			}
#else
			entry->gf = NULL;
			gf(entry->junk);
#endif
		} else {
			printf("gc finds NULL in gf:%p placed by f_line %s:%d?\n",
			       gf,
			       entry->func,
			       entry->line);
		}
	}
	GC_LOCK();
	if ((!TAILQ_EMPTY(&gc_list)) && (gc_running == 0)) {
		struct timeval nxttm;
		entry = TAILQ_FIRST(&gc_list);
		if (timevalcmp(&entry->purge_time, &now, >)) {
			nxttm = entry->purge_time;
			timevalsub(&nxttm, &now);
		} else {
			/* Huh? TSNH */
			nxttm.tv_sec = 0;
			nxttm.tv_usec = 0;	/* 1 tick I guess */
		}
		callout_reset(&gc_timer,
		    (TV_TO_TICKS(&nxttm) + 1),
		    gc_time_out, NULL);
		gc_running = 1;
	}
	GC_UNLOCK();
}

int
garbage_collect_add(struct garbage *m, garbage_func f,
		    void *gar, struct timeval *expire, const char *func,
		    int line)
{
	/* sanity */
	if (gc_inited == 0) {
		/* Sorry, to early in init process */
		return (EAGAIN);
	}
	if ((f == NULL) ||
	    (gar == NULL) ||
	    (expire == NULL)) {
		return (EINVAL);
	}
	if (m == NULL) {
		return(EINVAL);
	}
	GC_LOCK();
	if (m->on_gc_list) {
		if (m->gf == NULL) {
#ifdef INVARIANTS
			printf("gc finds NULL gf, caller func:%s:%d func:%s:%d\n",
			       func, line,
			       m->func, m->line);
#endif
			GC_UNLOCK();
			return (EINVAL);
		}
		/* Normal case -- extend the time */
		callout_reset(&gc_timer,
			      (TV_TO_TICKS(expire) + 1),
			      gc_time_out, NULL);
		GC_UNLOCK();
		return(0);
	}
	getmicrouptime(&m->purge_time);
	timevaladd(&m->purge_time, expire);
	m->gf = f;
	m->func = ((char *)(__uintptr_t)(const void *)(func));
	m->line = line;
	m->junk = gar;
	m->on_gc_list = 1;
	TAILQ_INSERT_TAIL(&gc_list, m, next);
	if (gc_running == 0) {
		gc_running = 1;
		callout_reset(&gc_timer,
			      (TV_TO_TICKS(expire) + 1),
			      gc_time_out, NULL);
	}
	GC_UNLOCK();
	return (0);
}

int
garbage_collect_init(struct garbage *m)
{
	memset(m, 0, sizeof(struct garbage));
	return (0);
}
