/*-
 * Copyright 1998 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * The kernel resource manager.  This code is responsible for keeping track
 * of hardware resources which are apportioned out to various drivers.
 * It does not actually assign those resources, and it is not expected
 * that end-device drivers will call into this code directly.  Rather,
 * the code which implements the buses that those devices are attached to,
 * and the code which manages CPU resources, will call this code, and the
 * end-device drivers will make upcalls to that code to actually perform
 * the allocation.
 *
 * There are two sorts of resources managed by this code.  The first is
 * the more familiar array (RMAN_ARRAY) type; resources in this class
 * consist of a sequence of individually-allocatable objects which have
 * been numbered in some well-defined order.  Most of the resources
 * are of this type, as it is the most familiar.  The second type is
 * called a gauge (RMAN_GAUGE), and models fungible resources (i.e.,
 * resources in which each instance is indistinguishable from every
 * other instance).  The principal anticipated application of gauges
 * is in the context of power consumption, where a bus may have a specific
 * power budget which all attached devices share.  RMAN_GAUGE is not
 * implemented yet.
 *
 * For array resources, we make one simplifying assumption: two clients
 * sharing the same resource must use the same range of indices.  That
 * is to say, sharing of overlapping-but-not-identical regions is not
 * permitted.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define __RMAN_RESOURCE_VISIBLE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/bus.h>		/* XXX debugging */
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

int     rman_debug = 0;
TUNABLE_INT("debug.rman_debug", &rman_debug);
SYSCTL_INT(_debug, OID_AUTO, rman_debug, CTLFLAG_RW,
    &rman_debug, 0, "rman debug");

#define DPRINTF(params) if (rman_debug) printf params

static MALLOC_DEFINE(M_RMAN, "rman", "Resource manager");

struct	rman_head rman_head;
static	struct mtx rman_mtx; /* mutex to protect rman_head */
static	int int_rman_activate_resource(struct rman *rm, struct resource_i *r,
				       struct resource_i **whohas);
static	int int_rman_deactivate_resource(struct resource_i *r);
static	int int_rman_release_resource(struct rman *rm, struct resource_i *r);

static __inline struct resource_i *
int_alloc_resource(int malloc_flag)
{
	struct resource_i *r;

	r = malloc(sizeof *r, M_RMAN, malloc_flag | M_ZERO);
	if (r != NULL) {
		r->r_r.__r_i = r;
	}
	return (r);
}

int
rman_init(struct rman *rm)
{
	static int once;

	if (once == 0) {
		once = 1;
		TAILQ_INIT(&rman_head);
		mtx_init(&rman_mtx, "rman head", NULL, MTX_DEF);
	}

	if (rm->rm_type == RMAN_UNINIT)
		panic("rman_init");
	if (rm->rm_type == RMAN_GAUGE)
		panic("implement RMAN_GAUGE");

	TAILQ_INIT(&rm->rm_list);
	rm->rm_mtx = malloc(sizeof *rm->rm_mtx, M_RMAN, M_NOWAIT | M_ZERO);
	if (rm->rm_mtx == 0)
		return ENOMEM;
	mtx_init(rm->rm_mtx, "rman", NULL, MTX_DEF);

	mtx_lock(&rman_mtx);
	TAILQ_INSERT_TAIL(&rman_head, rm, rm_link);
	mtx_unlock(&rman_mtx);
	return 0;
}

/*
 * NB: this interface is not robust against programming errors which
 * add multiple copies of the same region.
 */
int
rman_manage_region(struct rman *rm, u_long start, u_long end)
{
	struct resource_i *r, *s;

	DPRINTF(("rman_manage_region: <%s> request: start %#lx, end %#lx\n",
	    rm->rm_descr, start, end));
	r = int_alloc_resource(M_NOWAIT);
	if (r == 0)
		return ENOMEM;
	r->r_start = start;
	r->r_end = end;
	r->r_rm = rm;

	mtx_lock(rm->rm_mtx);
	for (s = TAILQ_FIRST(&rm->rm_list);	
	     s && s->r_end < r->r_start;
	     s = TAILQ_NEXT(s, r_link))
		;

	if (s == NULL) {
		TAILQ_INSERT_TAIL(&rm->rm_list, r, r_link);
	} else {
		TAILQ_INSERT_BEFORE(s, r, r_link);
	}

	mtx_unlock(rm->rm_mtx);
	return 0;
}

int
rman_fini(struct rman *rm)
{
	struct resource_i *r;

	mtx_lock(rm->rm_mtx);
	TAILQ_FOREACH(r, &rm->rm_list, r_link) {
		if (r->r_flags & RF_ALLOCATED) {
			mtx_unlock(rm->rm_mtx);
			return EBUSY;
		}
	}

	/*
	 * There really should only be one of these if we are in this
	 * state and the code is working properly, but it can't hurt.
	 */
	while (!TAILQ_EMPTY(&rm->rm_list)) {
		r = TAILQ_FIRST(&rm->rm_list);
		TAILQ_REMOVE(&rm->rm_list, r, r_link);
		free(r, M_RMAN);
	}
	mtx_unlock(rm->rm_mtx);
	mtx_lock(&rman_mtx);
	TAILQ_REMOVE(&rman_head, rm, rm_link);
	mtx_unlock(&rman_mtx);
	mtx_destroy(rm->rm_mtx);
	free(rm->rm_mtx, M_RMAN);

	return 0;
}

struct resource *
rman_reserve_resource_bound(struct rman *rm, u_long start, u_long end,
		      u_long count, u_long bound,  u_int flags,
		      struct device *dev)
{
	u_int	want_activate;
	struct	resource_i *r, *s, *rv;
	u_long	rstart, rend, amask, bmask;

	rv = 0;

	DPRINTF(("rman_reserve_resource: <%s> request: [%#lx, %#lx], length "
	       "%#lx, flags %u, device %s\n", rm->rm_descr, start, end, count,
	       flags, dev == NULL ? "<null>" : device_get_nameunit(dev)));
	want_activate = (flags & RF_ACTIVE);
	flags &= ~RF_ACTIVE;

	mtx_lock(rm->rm_mtx);

	for (r = TAILQ_FIRST(&rm->rm_list); 
	     r && r->r_end < start;
	     r = TAILQ_NEXT(r, r_link))
		;

	if (r == NULL) {
		DPRINTF(("could not find a region\n"));
		goto out;
	}

	amask = (1ul << RF_ALIGNMENT(flags)) - 1;
	/* If bound is 0, bmask will also be 0 */
	bmask = ~(bound - 1);
	/*
	 * First try to find an acceptable totally-unshared region.
	 */
	for (s = r; s; s = TAILQ_NEXT(s, r_link)) {
		DPRINTF(("considering [%#lx, %#lx]\n", s->r_start, s->r_end));
		if (s->r_start + count - 1 > end) {
			DPRINTF(("s->r_start (%#lx) + count - 1> end (%#lx)\n",
			    s->r_start, end));
			break;
		}
		if (s->r_flags & RF_ALLOCATED) {
			DPRINTF(("region is allocated\n"));
			continue;
		}
		rstart = ulmax(s->r_start, start);
		/*
		 * Try to find a region by adjusting to boundary and alignment
		 * until both conditions are satisfied. This is not an optimal
		 * algorithm, but in most cases it isn't really bad, either.
		 */
		do {
			rstart = (rstart + amask) & ~amask;
			if (((rstart ^ (rstart + count - 1)) & bmask) != 0)
				rstart += bound - (rstart & ~bmask);
		} while ((rstart & amask) != 0 && rstart < end &&
		    rstart < s->r_end);
		rend = ulmin(s->r_end, ulmax(rstart + count - 1, end));
		if (rstart > rend) {
			DPRINTF(("adjusted start exceeds end\n"));
			continue;
		}
		DPRINTF(("truncated region: [%#lx, %#lx]; size %#lx (requested %#lx)\n",
		       rstart, rend, (rend - rstart + 1), count));

		if ((rend - rstart + 1) >= count) {
			DPRINTF(("candidate region: [%#lx, %#lx], size %#lx\n",
			       rstart, rend, (rend - rstart + 1)));
			if ((s->r_end - s->r_start + 1) == count) {
				DPRINTF(("candidate region is entire chunk\n"));
				rv = s;
				rv->r_flags |= RF_ALLOCATED | flags;
				rv->r_dev = dev;
				goto out;
			}

			/*
			 * If s->r_start < rstart and
			 *    s->r_end > rstart + count - 1, then
			 * we need to split the region into three pieces
			 * (the middle one will get returned to the user).
			 * Otherwise, we are allocating at either the
			 * beginning or the end of s, so we only need to
			 * split it in two.  The first case requires
			 * two new allocations; the second requires but one.
			 */
			rv = int_alloc_resource(M_NOWAIT);
			if (rv == 0)
				goto out;
			rv->r_start = rstart;
			rv->r_end = rstart + count - 1;
			rv->r_flags = flags | RF_ALLOCATED;
			rv->r_dev = dev;
			rv->r_rm = rm;
			
			if (s->r_start < rv->r_start && s->r_end > rv->r_end) {
				DPRINTF(("splitting region in three parts: "
				       "[%#lx, %#lx]; [%#lx, %#lx]; [%#lx, %#lx]\n",
				       s->r_start, rv->r_start - 1,
				       rv->r_start, rv->r_end,
				       rv->r_end + 1, s->r_end));
				/*
				 * We are allocating in the middle.
				 */
				r = int_alloc_resource(M_NOWAIT);
				if (r == 0) {
					free(rv, M_RMAN);
					rv = 0;
					goto out;
				}
				r->r_start = rv->r_end + 1;
				r->r_end = s->r_end;
				r->r_flags = s->r_flags;
				r->r_rm = rm;
				s->r_end = rv->r_start - 1;
				TAILQ_INSERT_AFTER(&rm->rm_list, s, rv,
						     r_link);
				TAILQ_INSERT_AFTER(&rm->rm_list, rv, r,
						     r_link);
			} else if (s->r_start == rv->r_start) {
				DPRINTF(("allocating from the beginning\n"));
				/*
				 * We are allocating at the beginning.
				 */
				s->r_start = rv->r_end + 1;
				TAILQ_INSERT_BEFORE(s, rv, r_link);
			} else {
				DPRINTF(("allocating at the end\n"));
				/*
				 * We are allocating at the end.
				 */
				s->r_end = rv->r_start - 1;
				TAILQ_INSERT_AFTER(&rm->rm_list, s, rv,
						     r_link);
			}
			goto out;
		}
	}

	/*
	 * Now find an acceptable shared region, if the client's requirements
	 * allow sharing.  By our implementation restriction, a candidate
	 * region must match exactly by both size and sharing type in order
	 * to be considered compatible with the client's request.  (The
	 * former restriction could probably be lifted without too much
	 * additional work, but this does not seem warranted.)
	 */
	DPRINTF(("no unshared regions found\n"));
	if ((flags & (RF_SHAREABLE | RF_TIMESHARE)) == 0)
		goto out;

	for (s = r; s; s = TAILQ_NEXT(s, r_link)) {
		if (s->r_start > end)
			break;
		if ((s->r_flags & flags) != flags)
			continue;
		rstart = ulmax(s->r_start, start);
		rend = ulmin(s->r_end, ulmax(start + count - 1, end));
		if (s->r_start >= start && s->r_end <= end
		    && (s->r_end - s->r_start + 1) == count &&
		    (s->r_start & amask) == 0 &&
		    ((s->r_start ^ s->r_end) & bmask) == 0) {
			rv = int_alloc_resource(M_NOWAIT);
			if (rv == 0)
				goto out;
			rv->r_start = s->r_start;
			rv->r_end = s->r_end;
			rv->r_flags = s->r_flags & 
				(RF_ALLOCATED | RF_SHAREABLE | RF_TIMESHARE);
			rv->r_dev = dev;
			rv->r_rm = rm;
			if (s->r_sharehead == 0) {
				s->r_sharehead = malloc(sizeof *s->r_sharehead,
						M_RMAN, M_NOWAIT | M_ZERO);
				if (s->r_sharehead == 0) {
					free(rv, M_RMAN);
					rv = 0;
					goto out;
				}
				LIST_INIT(s->r_sharehead);
				LIST_INSERT_HEAD(s->r_sharehead, s, 
						 r_sharelink);
				s->r_flags |= RF_FIRSTSHARE;
			}
			rv->r_sharehead = s->r_sharehead;
			LIST_INSERT_HEAD(s->r_sharehead, rv, r_sharelink);
			goto out;
		}
	}

	/*
	 * We couldn't find anything.
	 */
out:
	/*
	 * If the user specified RF_ACTIVE in the initial flags,
	 * which is reflected in `want_activate', we attempt to atomically
	 * activate the resource.  If this fails, we release the resource
	 * and indicate overall failure.  (This behavior probably doesn't
	 * make sense for RF_TIMESHARE-type resources.)
	 */
	if (rv && want_activate) {
		struct resource_i *whohas;
		if (int_rman_activate_resource(rm, rv, &whohas)) {
			int_rman_release_resource(rm, rv);
			rv = 0;
		}
	}
			
	mtx_unlock(rm->rm_mtx);
	return (&rv->r_r);
}

struct resource *
rman_reserve_resource(struct rman *rm, u_long start, u_long end, u_long count,
		      u_int flags, struct device *dev)
{

	return (rman_reserve_resource_bound(rm, start, end, count, 0, flags,
	    dev));
}

static int
int_rman_activate_resource(struct rman *rm, struct resource_i *r,
			   struct resource_i **whohas)
{
	struct resource_i *s;
	int ok;

	/*
	 * If we are not timesharing, then there is nothing much to do.
	 * If we already have the resource, then there is nothing at all to do.
	 * If we are not on a sharing list with anybody else, then there is
	 * little to do.
	 */
	if ((r->r_flags & RF_TIMESHARE) == 0
	    || (r->r_flags & RF_ACTIVE) != 0
	    || r->r_sharehead == 0) {
		r->r_flags |= RF_ACTIVE;
		return 0;
	}

	ok = 1;
	for (s = LIST_FIRST(r->r_sharehead); s && ok;
	     s = LIST_NEXT(s, r_sharelink)) {
		if ((s->r_flags & RF_ACTIVE) != 0) {
			ok = 0;
			*whohas = s;
		}
	}
	if (ok) {
		r->r_flags |= RF_ACTIVE;
		return 0;
	}
	return EBUSY;
}

int
rman_activate_resource(struct resource *re)
{
	int rv;
	struct resource_i *r, *whohas;
	struct rman *rm;

	r = re->__r_i;
	rm = r->r_rm;
	mtx_lock(rm->rm_mtx);
	rv = int_rman_activate_resource(rm, r, &whohas);
	mtx_unlock(rm->rm_mtx);
	return rv;
}

int
rman_await_resource(struct resource *re, int pri, int timo)
{
	int	rv;
	struct	resource_i *r, *whohas;
	struct	rman *rm;

	r = re->__r_i;
	rm = r->r_rm;
	mtx_lock(rm->rm_mtx);
	for (;;) {
		rv = int_rman_activate_resource(rm, r, &whohas);
		if (rv != EBUSY)
			return (rv);	/* returns with mutex held */

		if (r->r_sharehead == 0)
			panic("rman_await_resource");
		whohas->r_flags |= RF_WANTED;
		rv = msleep(r->r_sharehead, rm->rm_mtx, pri, "rmwait", timo);
		if (rv) {
			mtx_unlock(rm->rm_mtx);
			return (rv);
		}
	}
}

static int
int_rman_deactivate_resource(struct resource_i *r)
{

	r->r_flags &= ~RF_ACTIVE;
	if (r->r_flags & RF_WANTED) {
		r->r_flags &= ~RF_WANTED;
		wakeup(r->r_sharehead);
	}
	return 0;
}

int
rman_deactivate_resource(struct resource *r)
{
	struct	rman *rm;

	rm = r->__r_i->r_rm;
	mtx_lock(rm->rm_mtx);
	int_rman_deactivate_resource(r->__r_i);
	mtx_unlock(rm->rm_mtx);
	return 0;
}

static int
int_rman_release_resource(struct rman *rm, struct resource_i *r)
{
	struct	resource_i *s, *t;

	if (r->r_flags & RF_ACTIVE)
		int_rman_deactivate_resource(r);

	/*
	 * Check for a sharing list first.  If there is one, then we don't
	 * have to think as hard.
	 */
	if (r->r_sharehead) {
		/*
		 * If a sharing list exists, then we know there are at
		 * least two sharers.
		 *
		 * If we are in the main circleq, appoint someone else.
		 */
		LIST_REMOVE(r, r_sharelink);
		s = LIST_FIRST(r->r_sharehead);
		if (r->r_flags & RF_FIRSTSHARE) {
			s->r_flags |= RF_FIRSTSHARE;
			TAILQ_INSERT_BEFORE(r, s, r_link);
			TAILQ_REMOVE(&rm->rm_list, r, r_link);
		}

		/*
		 * Make sure that the sharing list goes away completely
		 * if the resource is no longer being shared at all.
		 */
		if (LIST_NEXT(s, r_sharelink) == 0) {
			free(s->r_sharehead, M_RMAN);
			s->r_sharehead = 0;
			s->r_flags &= ~RF_FIRSTSHARE;
		}
		goto out;
	}

	/*
	 * Look at the adjacent resources in the list and see if our
	 * segment can be merged with any of them.  If either of the
	 * resources is allocated or is not exactly adjacent then they
	 * cannot be merged with our segment.
	 */
	s = TAILQ_PREV(r, resource_head, r_link);
	if (s != NULL && ((s->r_flags & RF_ALLOCATED) != 0 ||
	    s->r_end + 1 != r->r_start))
		s = NULL;
	t = TAILQ_NEXT(r, r_link);
	if (t != NULL && ((t->r_flags & RF_ALLOCATED) != 0 ||
	    r->r_end + 1 != t->r_start))
		t = NULL;

	if (s != NULL && t != NULL) {
		/*
		 * Merge all three segments.
		 */
		s->r_end = t->r_end;
		TAILQ_REMOVE(&rm->rm_list, r, r_link);
		TAILQ_REMOVE(&rm->rm_list, t, r_link);
		free(t, M_RMAN);
	} else if (s != NULL) {
		/*
		 * Merge previous segment with ours.
		 */
		s->r_end = r->r_end;
		TAILQ_REMOVE(&rm->rm_list, r, r_link);
	} else if (t != NULL) {
		/*
		 * Merge next segment with ours.
		 */
		t->r_start = r->r_start;
		TAILQ_REMOVE(&rm->rm_list, r, r_link);
	} else {
		/*
		 * At this point, we know there is nothing we
		 * can potentially merge with, because on each
		 * side, there is either nothing there or what is
		 * there is still allocated.  In that case, we don't
		 * want to remove r from the list; we simply want to
		 * change it to an unallocated region and return
		 * without freeing anything.
		 */
		r->r_flags &= ~RF_ALLOCATED;
		return 0;
	}

out:
	free(r, M_RMAN);
	return 0;
}

int
rman_release_resource(struct resource *re)
{
	int	rv;
	struct	resource_i *r;
	struct	rman *rm;

	r = re->__r_i;
	rm = r->r_rm;
	mtx_lock(rm->rm_mtx);
	rv = int_rman_release_resource(rm, r);
	mtx_unlock(rm->rm_mtx);
	return (rv);
}

uint32_t
rman_make_alignment_flags(uint32_t size)
{
	int	i;

	/*
	 * Find the hightest bit set, and add one if more than one bit
	 * set.  We're effectively computing the ceil(log2(size)) here.
	 */
	for (i = 31; i > 0; i--)
		if ((1 << i) & size)
			break;
	if (~(1 << i) & size)
		i++;

	return(RF_ALIGNMENT_LOG2(i));
}

u_long
rman_get_start(struct resource *r)
{
	return (r->__r_i->r_start);
}

u_long
rman_get_end(struct resource *r)
{
	return (r->__r_i->r_end);
}

u_long
rman_get_size(struct resource *r)
{
	return (r->__r_i->r_end - r->__r_i->r_start + 1);
}

u_int
rman_get_flags(struct resource *r)
{
	return (r->__r_i->r_flags);
}

void
rman_set_virtual(struct resource *r, void *v)
{
	r->__r_i->r_virtual = v;
}

void *
rman_get_virtual(struct resource *r)
{
	return (r->__r_i->r_virtual);
}

void
rman_set_bustag(struct resource *r, bus_space_tag_t t)
{
	r->r_bustag = t;
}

bus_space_tag_t
rman_get_bustag(struct resource *r)
{
	return (r->r_bustag);
}

void
rman_set_bushandle(struct resource *r, bus_space_handle_t h)
{
	r->r_bushandle = h;
}

bus_space_handle_t
rman_get_bushandle(struct resource *r)
{
	return (r->r_bushandle);
}

void
rman_set_rid(struct resource *r, int rid)
{
	r->__r_i->r_rid = rid;
}

void
rman_set_start(struct resource *r, u_long start)
{
	r->__r_i->r_start = start;
}

void
rman_set_end(struct resource *r, u_long end)
{
	r->__r_i->r_end = end;
}

int
rman_get_rid(struct resource *r)
{
	return (r->__r_i->r_rid);
}

struct device *
rman_get_device(struct resource *r)
{
	return (r->__r_i->r_dev);
}

void
rman_set_device(struct resource *r, struct device *dev)
{
	r->__r_i->r_dev = dev;
}

int
rman_is_region_manager(struct resource *r, struct rman *rm)
{

	return (r->__r_i->r_rm == rm);
}

/*
 * Sysctl interface for scanning the resource lists.
 *
 * We take two input parameters; the index into the list of resource
 * managers, and the resource offset into the list.
 */
static int
sysctl_rman(SYSCTL_HANDLER_ARGS)
{
	int			*name = (int *)arg1;
	u_int			namelen = arg2;
	int			rman_idx, res_idx;
	struct rman		*rm;
	struct resource_i	*res;
	struct u_rman		urm;
	struct u_resource	ures;
	int			error;

	if (namelen != 3)
		return (EINVAL);

	if (bus_data_generation_check(name[0]))
		return (EINVAL);
	rman_idx = name[1];
	res_idx = name[2];

	/*
	 * Find the indexed resource manager
	 */
	TAILQ_FOREACH(rm, &rman_head, rm_link) {
		if (rman_idx-- == 0)
			break;
	}
	if (rm == NULL)
		return (ENOENT);

	/*
	 * If the resource index is -1, we want details on the
	 * resource manager.
	 */
	if (res_idx == -1) {
		bzero(&urm, sizeof(urm));
		urm.rm_handle = (uintptr_t)rm;
		strlcpy(urm.rm_descr, rm->rm_descr, RM_TEXTLEN);
		urm.rm_start = rm->rm_start;
		urm.rm_size = rm->rm_end - rm->rm_start + 1;
		urm.rm_type = rm->rm_type;

		error = SYSCTL_OUT(req, &urm, sizeof(urm));
		return (error);
	}

	/*
	 * Find the indexed resource and return it.
	 */
	TAILQ_FOREACH(res, &rm->rm_list, r_link) {
		if (res_idx-- == 0) {
			bzero(&ures, sizeof(ures));
			ures.r_handle = (uintptr_t)res;
			ures.r_parent = (uintptr_t)res->r_rm;
			ures.r_device = (uintptr_t)res->r_dev;
			if (res->r_dev != NULL) {
				if (device_get_name(res->r_dev) != NULL) {
					snprintf(ures.r_devname, RM_TEXTLEN,
					    "%s%d",
					    device_get_name(res->r_dev),
					    device_get_unit(res->r_dev));
				} else {
					strlcpy(ures.r_devname, "nomatch",
					    RM_TEXTLEN);
				}
			} else {
				ures.r_devname[0] = '\0';
			}
			ures.r_start = res->r_start;
			ures.r_size = res->r_end - res->r_start + 1;
			ures.r_flags = res->r_flags;

			error = SYSCTL_OUT(req, &ures, sizeof(ures));
			return (error);
		}
	}
	return (ENOENT);
}

SYSCTL_NODE(_hw_bus, OID_AUTO, rman, CTLFLAG_RD, sysctl_rman,
    "kernel resource manager");

