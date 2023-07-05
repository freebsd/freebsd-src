/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012-2021 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: Navdeep Parhar <np@FreeBSD.org>
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

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/types.h>
#include <sys/ck.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/rmlock.h>
#include <sys/sbuf.h>
#include <sys/socket.h>
#include <sys/taskqueue.h>
#include <net/if.h>
#include <net/if_var.h>
#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet6/scope6_var.h>

#include "common/common.h"
#include "t4_clip.h"

/*
 * Code to deal with the Compressed Local IPv6 (CLIP) table in the ASIC.
 *
 * The driver maintains a global CLIP database (clip_db) of IPv6 addresses and a
 * per-adapter CLIP table (sc->clip_table) with entries that point to an IPv6 in
 * the clip_db.  All access is protected by a single global lock (clip_db_lock).
 * The correct lock order is clip lock before synchronized op.
 *
 * By default (hw.cxgbe.clip_db_auto=1) all local IPv6 addresses are added to
 * the db.  Addresses are also added on-demand when the driver allocates an
 * entry for a filter, TOE tid, etc.  krn_ref counts the number of times an
 * address appears in the system.  adp_ref counts the number of adapters that
 * have that address in their CLIP table.  If both are 0 then the entry is
 * evicted from the db.  Consumers of the CLIP table entry (filters, TOE tids)
 * are tracked in ce->refcount.  Driver ioctls let external consumers add/remove
 * addresses from the CLIP table.
 */

#if defined(INET6)
struct clip_db_entry {
	LIST_ENTRY(clip_db_entry) link;	/* clip_db hash linkage */
	struct in6_addr lip;
	u_int krn_ref;	/* # of times this IP6 appears in list of all IP6 */
	u_int adp_ref;	/* # of adapters with this IP6 in their CLIP */
	u_int tmp_ref;	/* Used only during refresh */
};

struct clip_entry {
	LIST_ENTRY(clip_entry) link;	/* clip_table hash linkage */
	TAILQ_ENTRY(clip_entry) plink;	/* clip_pending linkage */
	struct clip_db_entry *cde;
	int16_t clip_idx;		/* index in the hw table */
	bool pending;			/* in clip_pending list */
	int refcount;
};

static eventhandler_tag ifaddr_evhandler;
static struct mtx clip_db_lock;
static LIST_HEAD(, clip_db_entry) *clip_db;
static u_long clip_db_mask;
static int clip_db_gen;
static struct task clip_db_task;

static int add_lip(struct adapter *, struct in6_addr *, int16_t *);
static int del_lip(struct adapter *, struct in6_addr *);
static void t4_clip_db_task(void *, int);
static void t4_clip_task(void *, int);
static void update_clip_db(void);
static int update_sw_clip_table(struct adapter *);
static int update_hw_clip_table(struct adapter *);
static void update_clip_table(struct adapter *, void *);
static int sysctl_clip_db(SYSCTL_HANDLER_ARGS);
static int sysctl_clip_db_auto(SYSCTL_HANDLER_ARGS);
static struct clip_db_entry *lookup_clip_db_entry(struct in6_addr *, bool);
static struct clip_entry *lookup_clip_entry(struct adapter *, struct in6_addr *,
    bool);

SYSCTL_PROC(_hw_cxgbe, OID_AUTO, clip_db, CTLTYPE_STRING | CTLFLAG_RD |
    CTLFLAG_SKIP | CTLFLAG_MPSAFE, NULL, 0, sysctl_clip_db, "A",
    "CLIP database");

int t4_clip_db_auto = 1;
SYSCTL_PROC(_hw_cxgbe, OID_AUTO, clip_db_auto, CTLTYPE_INT | CTLFLAG_RWTUN |
    CTLFLAG_MPSAFE, NULL, 0, sysctl_clip_db_auto, "I",
    "Add local IPs to CLIP db automatically (0 = no, 1 = yes)");

static inline uint32_t
clip_hashfn(struct in6_addr *addr)
{
	return (fnv_32_buf(addr, sizeof(*addr), FNV1_32_INIT) & clip_db_mask);
}

static inline struct clip_db_entry *
alloc_clip_db_entry(struct in6_addr *in6)
{
	struct clip_db_entry *cde;

	cde = malloc(sizeof(*cde), M_CXGBE, M_NOWAIT | M_ZERO);
	if (__predict_true(cde != NULL))
		memcpy(&cde->lip, in6, sizeof(cde->lip));

	return (cde);
}

static inline struct clip_entry *
alloc_clip_entry(struct clip_db_entry *cde)
{
	struct clip_entry *ce;

	mtx_assert(&clip_db_lock, MA_OWNED);

	ce = malloc(sizeof(*ce), M_CXGBE, M_NOWAIT | M_ZERO);
	if (__predict_true(ce != NULL)) {
		ce->cde = cde;
		cde->adp_ref++;
		ce->clip_idx = -1;
	}

	return (ce);
}

/*
 * Look up the IP6 address in the CLIP db.  If add is set then an entry for the
 * IP6 will be added to the db.
 */
static struct clip_db_entry *
lookup_clip_db_entry(struct in6_addr *in6, bool add)
{
	struct clip_db_entry *cde;
	const int bucket = clip_hashfn(in6);

	mtx_assert(&clip_db_lock, MA_OWNED);

	LIST_FOREACH(cde, &clip_db[bucket], link) {
		if (IN6_ARE_ADDR_EQUAL(&cde->lip, in6))
			return (cde);
	}

	/* Not found.  Create a new entry if requested. */
	if (add) {
		cde = alloc_clip_db_entry(in6);
		if (cde != NULL)
			LIST_INSERT_HEAD(&clip_db[bucket], cde, link);
	}

	return (cde);
}

/*
 * Look up the IP6 address in the CLIP db.  If add is set then an entry for the
 * IP6 will be added to the db.
 */
static struct clip_entry *
lookup_clip_entry(struct adapter *sc, struct in6_addr *in6, bool add)
{
	struct clip_db_entry *cde;
	struct clip_entry *ce;
	const int bucket = clip_hashfn(in6);

	mtx_assert(&clip_db_lock, MA_OWNED);

	cde = lookup_clip_db_entry(in6, add);
	if (cde == NULL)
		return (NULL);

	LIST_FOREACH(ce, &sc->clip_table[bucket], link) {
		if (ce->cde == cde)
			return (ce);
	}

	/* Not found.  Create a new entry if requested. */
	if (add) {
		ce = alloc_clip_entry(cde);
		if (ce != NULL) {
			LIST_INSERT_HEAD(&sc->clip_table[bucket], ce, link);
			TAILQ_INSERT_TAIL(&sc->clip_pending, ce, plink);
			ce->pending = true;
		}
	}

	return (ce);
}

static int
add_lip(struct adapter *sc, struct in6_addr *lip, int16_t *idx)
{
	struct fw_clip_cmd c;
	int rc;

	ASSERT_SYNCHRONIZED_OP(sc);

	memset(&c, 0, sizeof(c));
	c.op_to_write = htonl(V_FW_CMD_OP(FW_CLIP_CMD) | F_FW_CMD_REQUEST |
	    F_FW_CMD_WRITE);
	c.alloc_to_len16 = htonl(F_FW_CLIP_CMD_ALLOC | FW_LEN16(c));
	c.ip_hi = *(uint64_t *)&lip->s6_addr[0];
	c.ip_lo = *(uint64_t *)&lip->s6_addr[8];

	rc = -t4_wr_mbox_ns(sc, sc->mbox, &c, sizeof(c), &c);
	if (rc == 0 && idx != NULL)
		*idx = G_FW_CLIP_CMD_INDEX(ntohl(c.alloc_to_len16));
	return (rc);
}

static int
del_lip(struct adapter *sc, struct in6_addr *lip)
{
	struct fw_clip_cmd c;

	ASSERT_SYNCHRONIZED_OP(sc);

	memset(&c, 0, sizeof(c));
	c.op_to_write = htonl(V_FW_CMD_OP(FW_CLIP_CMD) | F_FW_CMD_REQUEST |
	    F_FW_CMD_READ);
	c.alloc_to_len16 = htonl(F_FW_CLIP_CMD_FREE | FW_LEN16(c));
	c.ip_hi = *(uint64_t *)&lip->s6_addr[0];
	c.ip_lo = *(uint64_t *)&lip->s6_addr[8];

	return (-t4_wr_mbox_ns(sc, sc->mbox, &c, sizeof(c), &c));
}
#endif

struct clip_entry *
t4_get_clip_entry(struct adapter *sc, struct in6_addr *in6, bool add)
{
#ifdef INET6
	struct clip_entry *ce;
	bool schedule = false;

	mtx_lock(&clip_db_lock);
	ce = lookup_clip_entry(sc, in6, add);
	if (ce != NULL) {
		MPASS(ce->cde->adp_ref > 0);
		if (++ce->refcount == 1 && ce->pending && ce->clip_idx != -1) {
			/*
			 * Valid entry that was waiting to be deleted.  It is in
			 * use now so take it off the pending list.
			 */
			TAILQ_REMOVE(&sc->clip_pending, ce, plink);
			ce->pending = false;
		}
		if (ce->clip_idx == -1 && update_hw_clip_table(sc) != 0)
			schedule = true;
	}
	mtx_unlock(&clip_db_lock);
	if (schedule)
		taskqueue_enqueue_timeout(taskqueue_thread, &sc->clip_task, 0);

	return (ce);
#else
	return (NULL);
#endif
}

void
t4_hold_clip_entry(struct adapter *sc, struct clip_entry *ce)
{
#ifdef INET6
	MPASS(ce != NULL);
	MPASS(ce->cde->adp_ref > 0);

	mtx_lock(&clip_db_lock);
	MPASS(ce->refcount > 0); /* Caller should already have a reference */
	ce->refcount++;
	mtx_unlock(&clip_db_lock);
#endif
}

#ifdef INET6
static void
release_clip_entry_locked(struct adapter *sc, struct clip_entry *ce)
{
	struct clip_db_entry *cde;

	mtx_assert(&clip_db_lock, MA_OWNED);
	MPASS(ce->refcount > 0);
	cde = ce->cde;
	MPASS(cde->adp_ref > 0);
	if (--ce->refcount == 0 && cde->krn_ref == 0) {
		if (ce->clip_idx == -1) {
			/* Was never written to the hardware. */
			MPASS(ce->pending);
			TAILQ_REMOVE(&sc->clip_pending, ce, plink);
			LIST_REMOVE(ce, link);
			free(ce, M_CXGBE);
			if (--cde->adp_ref == 0) {
				LIST_REMOVE(cde, link);
				free(cde, M_CXGBE);
			}
		} else {
			/*
			 * Valid entry is now unused, add to the pending list
			 * for deletion.  Its refcount was 1 on entry so it
			 * can't already be pending.
			 */
			MPASS(!ce->pending);
			TAILQ_INSERT_HEAD(&sc->clip_pending, ce, plink);
			ce->pending = true;
		}
	}
}
#endif

void
t4_release_clip_entry(struct adapter *sc, struct clip_entry *ce)
{
#ifdef INET6
	MPASS(ce != NULL);

	mtx_lock(&clip_db_lock);
	release_clip_entry_locked(sc, ce);
	/*
	 * This isn't a manual release via the ioctl.  No need to update the
	 * hw right now even if the release resulted in the entry being queued
	 * for deletion.
	 */
	mtx_unlock(&clip_db_lock);
#endif
}

int
t4_release_clip_addr(struct adapter *sc, struct in6_addr *in6)
{
	int rc = ENOTSUP;
#ifdef INET6
	struct clip_entry *ce;
	bool schedule = false;

	mtx_lock(&clip_db_lock);
	ce = lookup_clip_entry(sc, in6, false);
	if (ce == NULL)
		rc = ENOENT;
	else if (ce->refcount == 0)
		rc = EIO;
	else {
		release_clip_entry_locked(sc, ce);
		if (update_hw_clip_table(sc) != 0)
			schedule = true;
		rc = 0;
	}
	mtx_unlock(&clip_db_lock);
	if (schedule)
		taskqueue_enqueue_timeout(taskqueue_thread, &sc->clip_task, 0);
#endif
	return (rc);
}

#ifdef INET6
void
t4_init_clip_table(struct adapter *sc)
{
	TAILQ_INIT(&sc->clip_pending);
	TIMEOUT_TASK_INIT(taskqueue_thread, &sc->clip_task, 0, t4_clip_task, sc);
	sc->clip_gen = -1;
	sc->clip_table = hashinit(CLIP_HASH_SIZE, M_CXGBE, &sc->clip_mask);

	/* Both the hashes must use the same bucket for the same key. */
	if (sc->clip_table != NULL)
		MPASS(sc->clip_mask == clip_db_mask);
	/*
	 * Don't bother forcing an update of the clip table when the
	 * adapter is initialized.  Before an interface can be used it
	 * must be assigned an address which will trigger the event
	 * handler to update the table.
	 */
}

/*
 * Returns true if any additions or deletions were made to the CLIP DB.
 */
static void
update_clip_db(void)
{
	VNET_ITERATOR_DECL(vnet_iter);
	struct rm_priotracker in6_ifa_tracker;
	struct in6_addr *in6, tin6;
	struct in6_ifaddr *ia;
	struct clip_db_entry *cde, *cde_tmp;
	int i, addel;

	VNET_LIST_RLOCK();
	IN6_IFADDR_RLOCK(&in6_ifa_tracker);
	mtx_lock(&clip_db_lock);
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET_QUIET(vnet_iter);
		CK_STAILQ_FOREACH(ia, &V_in6_ifaddrhead, ia_link) {
			if (if_getflags(ia->ia_ifp) & IFF_LOOPBACK)
				continue;
			in6 = &ia->ia_addr.sin6_addr;
			KASSERT(!IN6_IS_ADDR_MULTICAST(in6),
			    ("%s: mcast address in in6_ifaddr list", __func__));
			if (IN6_IS_ADDR_LOOPBACK(in6))
				continue;

			if (IN6_IS_SCOPE_EMBED(in6)) {
				tin6 = *in6;
				in6 = &tin6;
				in6_clearscope(in6);
			}
			cde = lookup_clip_db_entry(in6, true);
			if (cde == NULL)
				continue;
			cde->tmp_ref++;
		}
		CURVNET_RESTORE();
	}

	addel = 0;
	for (i = 0; i <= clip_db_mask; i++) {
		LIST_FOREACH_SAFE(cde, &clip_db[i], link, cde_tmp) {
			if (cde->krn_ref == 0 && cde->tmp_ref > 0) {
				addel++;	/* IP6 addr added. */
			} else if (cde->krn_ref > 0 && cde->tmp_ref == 0) {
				if (cde->adp_ref == 0) {
					LIST_REMOVE(cde, link);
					free(cde, M_CXGBE);
					continue;
				}
				addel++;	/* IP6 addr deleted. */
			}
			cde->krn_ref = cde->tmp_ref;
			cde->tmp_ref = 0;
		}
	}
	if (addel > 0)
		clip_db_gen++;
	mtx_unlock(&clip_db_lock);
	IN6_IFADDR_RUNLOCK(&in6_ifa_tracker);
	VNET_LIST_RUNLOCK();

}

/*
 * Update the CLIP db and then update the CLIP tables on all the adapters.
 */
static void
t4_clip_db_task(void *arg, int count)
{
	update_clip_db();
	t4_iterate(update_clip_table, NULL);
}

/*
 * Refresh the sw CLIP table for this adapter from the global CLIP db.  Entries
 * that need to be added or deleted from the hardware CLIP table are placed on a
 * pending list but the hardware is not touched.  The pending list is something
 * reasonable even if this fails so it's ok to apply that to the hardware.
 */
static int
update_sw_clip_table(struct adapter *sc)
{
	struct clip_db_entry *cde;
	struct clip_entry *ce, *ce_temp;
	int i;
	bool found;

	mtx_assert(&clip_db_lock, MA_OWNED);

	/*
	 * We are about to rebuild the pending list from scratch.  Deletions are
	 * placed before additions because that's how we want to submit them to
	 * the hardware.
	 */
	TAILQ_INIT(&sc->clip_pending);

	/*
	 * Walk the sw CLIP table first.  We want to reset every entry's pending
	 * status as we're rebuilding the pending list.
	 */
	for (i = 0; i <= clip_db_mask; i++) {
		LIST_FOREACH_SAFE(ce, &sc->clip_table[i], link, ce_temp) {
			cde = ce->cde;
			MPASS(cde->adp_ref > 0);
			if (ce->refcount != 0 || cde->krn_ref != 0) {
				/*
				 * Entry should stay in the CLIP.
				 */

				if (ce->clip_idx != -1) {
					ce->pending = false;
				} else {
					/* Was never added, carry forward. */
					MPASS(ce->pending);
					TAILQ_INSERT_TAIL(&sc->clip_pending, ce,
					    plink);
				}
				continue;
			}

			/*
			 * Entry should be removed from the CLIP.
			 */

			if (ce->clip_idx != -1) {
				ce->pending = true;
				TAILQ_INSERT_HEAD(&sc->clip_pending, ce, plink);
			} else {
				/* Was never added, free right now. */
				MPASS(ce->pending);
				LIST_REMOVE(ce, link);
				free(ce, M_CXGBE);
				if (--cde->adp_ref == 0) {
					LIST_REMOVE(cde, link);
					free(cde, M_CXGBE);
				}
			}
		}
	}

	for (i = 0; i <= clip_db_mask; i++) {
		LIST_FOREACH(cde, &clip_db[i], link) {
			if (cde->krn_ref == 0)
				continue;

			found = false;
			LIST_FOREACH(ce, &sc->clip_table[i], link) {
				if (ce->cde == cde) {
					found = true;
					break;
				}
			}
			if (found)
				continue;
			ce = alloc_clip_entry(cde);
			if (ce == NULL)
				return (ENOMEM);
			LIST_INSERT_HEAD(&sc->clip_table[i], ce, link);
			TAILQ_INSERT_TAIL(&sc->clip_pending, ce, plink);
			ce->pending = true;
		}
	}

	sc->clip_gen = clip_db_gen;
	return (0);
}

static int
update_hw_clip_table(struct adapter *sc)
{
	struct clip_db_entry *cde;
	struct clip_entry *ce;
	int rc;
	char ip[INET6_ADDRSTRLEN];

	mtx_assert(&clip_db_lock, MA_OWNED);
	rc = begin_synchronized_op(sc, NULL, HOLD_LOCK, "t4clip");
	if (rc != 0)
		return (rc);
	if (hw_off_limits(sc))
		goto done;	/* with rc = 0, we don't want to reschedule. */
	while (!TAILQ_EMPTY(&sc->clip_pending)) {
		ce = TAILQ_FIRST(&sc->clip_pending);
		MPASS(ce->pending);
		cde = ce->cde;
		MPASS(cde->adp_ref > 0);

		if (ce->clip_idx == -1) {
			/*
			 * Entry was queued for addition to the HW CLIP.
			 */

			if (ce->refcount == 0 && cde->krn_ref == 0) {
				/* No need to add to HW CLIP. */
				TAILQ_REMOVE(&sc->clip_pending, ce, plink);
				LIST_REMOVE(ce, link);
				free(ce, M_CXGBE);
				if (--cde->adp_ref == 0) {
					LIST_REMOVE(cde, link);
					free(cde, M_CXGBE);
				}
			} else {
				/* Add to the HW CLIP. */
				rc = add_lip(sc, &cde->lip, &ce->clip_idx);
				if (rc == FW_ENOMEM) {
					/* CLIP full, no point in retrying. */
					rc = 0;
					goto done;
				}
				if (rc != 0) {
					inet_ntop(AF_INET6, &cde->lip, &ip[0],
					    sizeof(ip));
					CH_ERR(sc, "add_lip(%s) failed: %d\n",
					    ip, rc);
					goto done;
				}
				MPASS(ce->clip_idx != -1);
				TAILQ_REMOVE(&sc->clip_pending, ce, plink);
				ce->pending = false;
			}
		} else {
			/*
			 * Entry was queued for deletion from the HW CLIP.
			 */

			if (ce->refcount == 0 && cde->krn_ref == 0) {
				/*
				 * Delete from the HW CLIP.  Delete should never
				 * fail so we always log an error.  But if the
				 * failure is that the entry wasn't found in the
				 * CLIP then we carry on as if it was deleted.
				 */
				rc = del_lip(sc, &cde->lip);
				if (rc != 0)
					CH_ERR(sc, "del_lip(%s) failed: %d\n",
					    ip, rc);
				if (rc == FW_EPROTO)
					rc = 0;
				if (rc != 0)
					goto done;

				TAILQ_REMOVE(&sc->clip_pending, ce, plink);
				LIST_REMOVE(ce, link);
				free(ce, M_CXGBE);
				if (--cde->adp_ref == 0) {
					LIST_REMOVE(cde, link);
					free(cde, M_CXGBE);
				}
			} else {
				/* No need to delete from HW CLIP. */
				TAILQ_REMOVE(&sc->clip_pending, ce, plink);
				ce->pending = false;
			}
		}
	}
done:
	end_synchronized_op(sc, LOCK_HELD);
	return (rc);
}

static void
update_clip_table(struct adapter *sc, void *arg __unused)
{
	bool reschedule;

	if (sc->clip_table == NULL)
		return;

	reschedule = false;
	mtx_lock(&clip_db_lock);
	if (sc->clip_gen != clip_db_gen && update_sw_clip_table(sc) != 0)
		reschedule = true;
	if (!TAILQ_EMPTY(&sc->clip_pending) && update_hw_clip_table(sc) != 0)
		reschedule = true;
	mtx_unlock(&clip_db_lock);
	if (reschedule)
		taskqueue_enqueue_timeout(taskqueue_thread, &sc->clip_task,
		    -hz / 4);
}

/*
 * Update the CLIP table of the specified adapter.
 */
static void
t4_clip_task(void *sc, int count)
{
	update_clip_table(sc, NULL);
}

void
t4_destroy_clip_table(struct adapter *sc)
{
	struct clip_entry *ce, *ce_temp;
	int i;

	mtx_lock(&clip_db_lock);
	if (sc->clip_table == NULL)
		goto done;		/* CLIP was never initialized. */
	for (i = 0; i <= sc->clip_mask; i++) {
		LIST_FOREACH_SAFE(ce, &sc->clip_table[i], link, ce_temp) {
			MPASS(ce->refcount == 0);
			MPASS(ce->cde->adp_ref > 0);
#if 0
			del_lip(sc, &ce->lip);
#endif
			LIST_REMOVE(ce, link);
			if (--ce->cde->adp_ref == 0 && ce->cde->krn_ref == 0) {
				LIST_REMOVE(ce->cde, link);
				free(ce->cde, M_CXGBE);
			}
			free(ce, M_CXGBE);
		}
	}
	hashdestroy(sc->clip_table, M_CXGBE, sc->clip_mask);
	sc->clip_table = NULL;
done:
	mtx_unlock(&clip_db_lock);
}

static void
t4_ifaddr_event(void *arg __unused, if_t ifp, struct ifaddr *ifa,
    int event)
{
	struct in6_addr *in6;

	if (t4_clip_db_auto == 0)
		return;		/* Automatic updates not allowed. */
	if (ifa->ifa_addr->sa_family != AF_INET6)
		return;
	if (if_getflags(ifp) & IFF_LOOPBACK)
		return;
	in6 = &((struct in6_ifaddr *)ifa)->ia_addr.sin6_addr;
	if (IN6_IS_ADDR_LOOPBACK(in6) || IN6_IS_ADDR_MULTICAST(in6))
		return;

	taskqueue_enqueue(taskqueue_thread, &clip_db_task);
}

int
sysctl_clip(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	struct clip_entry *ce;
	struct sbuf *sb;
	int i, rc, header = 0;
	char ip[INET6_ADDRSTRLEN];

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return (rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 4096, req);
	if (sb == NULL)
		return (ENOMEM);

	mtx_lock(&clip_db_lock);
	for (i = 0; i <= sc->clip_mask; i++) {
		LIST_FOREACH(ce, &sc->clip_table[i], link) {
			if (header == 0) {
				sbuf_printf(sb, "%-4s %-4s %s", "Indx", "Refs",
				    "IP address");
				header = 1;
			}
			inet_ntop(AF_INET6, &ce->cde->lip, &ip[0], sizeof(ip));
			if (ce->clip_idx == -1) {
				sbuf_printf(sb, "\n%-4s %-4d %s", "-",
				    ce->refcount, ip);
			} else {
				sbuf_printf(sb, "\n%-4d %-4d %s", ce->clip_idx,
				    ce->refcount, ip);
			}
		}
	}
	mtx_unlock(&clip_db_lock);

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

static int
sysctl_clip_db(SYSCTL_HANDLER_ARGS)
{
	struct clip_db_entry *cde;
	struct sbuf *sb;
	int i, rc, header = 0;
	char ip[INET6_ADDRSTRLEN];

	rc = sysctl_wire_old_buffer(req, 0);
	if (rc != 0)
		return (rc);

	sb = sbuf_new_for_sysctl(NULL, NULL, 4096, req);
	if (sb == NULL)
		return (ENOMEM);

	mtx_lock(&clip_db_lock);
	for (i = 0; i <= clip_db_mask; i++) {
		LIST_FOREACH(cde, &clip_db[i], link) {
			MPASS(cde->tmp_ref == 0);
			if (header == 0) {
				sbuf_printf(sb, "%-4s %-4s %s", "Kref", "Aref",
				    "IP address");
				header = 1;
			}
			inet_ntop(AF_INET6, &cde->lip, &ip[0], sizeof(ip));
			sbuf_printf(sb, "\n%-4d %-4d %s", cde->krn_ref,
			    cde->adp_ref, ip);
		}
	}
	mtx_unlock(&clip_db_lock);

	rc = sbuf_finish(sb);
	sbuf_delete(sb);

	return (rc);
}

static int
sysctl_clip_db_auto(SYSCTL_HANDLER_ARGS)
{
	int rc, val;

	val = t4_clip_db_auto;
	rc = sysctl_handle_int(oidp, &val, 0, req);
	if (rc != 0 || req->newptr == NULL)
		return (rc);

	if (val == 0 || val == 1)
		t4_clip_db_auto = val;
	else {
		/*
		 * Writing a value other than 0 or 1 forces a one-time update of
		 * the clip_db directly in the sysctl and not in some taskqueue.
		 */
		t4_clip_db_task(NULL, 0);
	}

	return (0);
}

void
t4_clip_modload(void)
{
	mtx_init(&clip_db_lock, "clip_db", NULL, MTX_DEF);
	clip_db = hashinit(CLIP_HASH_SIZE, M_CXGBE, &clip_db_mask);
	TASK_INIT(&clip_db_task, 0, t4_clip_db_task, NULL);
	ifaddr_evhandler = EVENTHANDLER_REGISTER(ifaddr_event_ext,
	    t4_ifaddr_event, NULL, EVENTHANDLER_PRI_ANY);
}

void
t4_clip_modunload(void)
{
	struct clip_db_entry *cde;
	int i;

	EVENTHANDLER_DEREGISTER(ifaddr_event_ext, ifaddr_evhandler);
	taskqueue_drain(taskqueue_thread, &clip_db_task);
	mtx_lock(&clip_db_lock);
	for (i = 0; i <= clip_db_mask; i++) {
		while ((cde = LIST_FIRST(&clip_db[i])) != NULL) {
			MPASS(cde->tmp_ref == 0);
			MPASS(cde->adp_ref == 0);
			LIST_REMOVE(cde, link);
			free(cde, M_CXGBE);
		}
	}
	mtx_unlock(&clip_db_lock);
	hashdestroy(clip_db, M_CXGBE, clip_db_mask);
	mtx_destroy(&clip_db_lock);
}
#endif
