/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Alexander V. Chernikov
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
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_route.h"

#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/sbuf.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/queue.h>
#include <net/vnet.h>

#include <net/if.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif

#include <net/route.h>
#include <net/route/nhop.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <net/route/fib_algo.h>

#include <machine/stdarg.h>

/*
 * Fib lookup framework.
 *
 * This framework enables accelerated longest-prefix-match lookups for the
 *  routing tables by adding the ability to dynamically attach/detach lookup
 *  algorithms implementation to/from the datapath.
 *
 * flm - fib lookup modules - implementation of particular lookup algorithm
 * fd - fib data - instance of an flm bound to specific routing table
 *
 * This file provides main framework functionality.
 *
 * The following are the features provided by the framework
 *
 * 1) nexhops abstraction -> provides transparent referencing, indexing
 *   and efficient idx->ptr mappings for nexthop and nexthop groups.
 * 2) Routing table synchronisation
 * 3) dataplane attachment points
 * 4) automatic algorithm selection based on the provided preference.
 *
 *
 * DATAPATH
 * For each supported address family, there is a an allocated array of fib_dp
 *  structures, indexed by fib number. Each array entry contains callback function
 *  and its argument. This function will be called with a family-specific lookup key,
 *  scope and provided argument. This array gets re-created every time when new algo
 *  instance gets created. Please take a look at the replace_rtables_family() function
 *  for more details.
 *
 */

SYSCTL_DECL(_net_route);
SYSCTL_NODE(_net_route, OID_AUTO, algo, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Fib algorithm lookups");

#ifdef INET6
VNET_DEFINE_STATIC(bool, algo_fixed_inet6) = false;
#define	V_algo_fixed_inet6	VNET(algo_fixed_inet6)
SYSCTL_NODE(_net_route_algo, OID_AUTO, inet6, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "IPv6 longest prefix match lookups");
#endif
#ifdef INET
VNET_DEFINE_STATIC(bool, algo_fixed_inet) = false;
#define	V_algo_fixed_inet	VNET(algo_fixed_inet)
SYSCTL_NODE(_net_route_algo, OID_AUTO, inet, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "IPv4 longest prefix match lookups");
#endif

struct nhop_ref_table {
	uint32_t		count;
	int32_t			refcnt[0];
};

/*
 * Data structure for the fib lookup instance tied to the particular rib.
 */
struct fib_data {
	uint32_t		number_nhops;	/* current # of nhops */
	uint8_t			hit_nhops;	/* true if out of nhop limit */
	uint8_t			init_done;	/* true if init is competed */
	uint32_t		fd_dead:1;	/* Scheduled for deletion */
	uint32_t		fd_linked:1;	/* true if linked */
	uint32_t		fd_need_rebuild:1;	/* true if rebuild scheduled */
	uint32_t		fd_force_eval:1;/* true if rebuild scheduled */
	uint8_t			fd_family;	/* family */
	uint32_t		fd_fibnum;	/* fibnum */
	uint32_t		fd_failed_rebuilds;	/* stat: failed rebuilds */
	uint32_t		fd_algo_mask;	/* bitmask for algo data */
	struct callout		fd_callout;	/* rebuild callout */
	void			*fd_algo_data;	/* algorithm data */
	struct nhop_object	**nh_idx;	/* nhop idx->ptr array */
	struct nhop_ref_table	*nh_ref_table;	/* array with # of nhop references */
	struct rib_head		*fd_rh;		/* RIB table we're attached to */
	struct rib_subscription	*fd_rs;		/* storing table subscription */
	struct fib_dp		fd_dp;		/* fib datapath data */
	struct vnet		*fd_vnet;	/* vnet fib belongs to */
	struct epoch_context	fd_epoch_ctx;	/* epoch context for deletion */
	struct fib_lookup_module	*fd_flm;/* pointer to the lookup module */
	uint32_t		fd_num_changes;	/* number of changes since last callout */
	TAILQ_ENTRY(fib_data)	entries;	/* list of all fds in vnet */
};

static void rebuild_fd_callout(void *_data);
static void destroy_fd_instance_epoch(epoch_context_t ctx);
static enum flm_op_result attach_datapath(struct fib_data *fd);
static bool is_idx_free(struct fib_data *fd, uint32_t index);
static void set_algo_fixed(struct rib_head *rh);
static bool is_algo_fixed(struct rib_head *rh);

static uint32_t fib_ref_nhop(struct fib_data *fd, struct nhop_object *nh);
static void fib_unref_nhop(struct fib_data *fd, struct nhop_object *nh);

static struct fib_lookup_module *fib_check_best_algo(struct rib_head *rh,
    struct fib_lookup_module *orig_flm);
static void fib_unref_algo(struct fib_lookup_module *flm);
static bool flm_error_check(const struct fib_lookup_module *flm, uint32_t fibnum);

struct mtx fib_mtx;
#define	FIB_MOD_LOCK()		mtx_lock(&fib_mtx)
#define	FIB_MOD_UNLOCK()	mtx_unlock(&fib_mtx)
#define	FIB_MOD_LOCK_ASSERT()	mtx_assert(&fib_mtx, MA_OWNED)

MTX_SYSINIT(fib_mtx, &fib_mtx, "algo list mutex", MTX_DEF);

/* Algorithm has to be this percent better than the current to switch */
#define	BEST_DIFF_PERCENT	(5 * 256 / 100)
/* Schedule algo re-evaluation X seconds after a change */
#define	ALGO_EVAL_DELAY_MS	30000
/* Force algo re-evaluation after X changes */
#define	ALGO_EVAL_NUM_ROUTES	100
/* Try to setup algorithm X times */
#define	FIB_MAX_TRIES		32
/* Max amount of supported nexthops */
#define	FIB_MAX_NHOPS		262144
#define	FIB_CALLOUT_DELAY_MS	50

/* Debug */
static int flm_debug_level = LOG_NOTICE;
SYSCTL_INT(_net_route_algo, OID_AUTO, debug_level, CTLFLAG_RW | CTLFLAG_RWTUN,
    &flm_debug_level, 0, "debuglevel");
#define	FLM_MAX_DEBUG_LEVEL	LOG_DEBUG

#define	_PASS_MSG(_l)	(flm_debug_level >= (_l))
#define	ALGO_PRINTF(_fmt, ...)	printf("[fib_algo] %s: " _fmt "\n", __func__, ##__VA_ARGS__)
#define	_ALGO_PRINTF(_fib, _fam, _aname, _func, _fmt, ...) \
    printf("[fib_algo] %s.%u (%s) %s: " _fmt "\n",\
    print_family(_fam), _fib, _aname, _func, ## __VA_ARGS__)
#define	_RH_PRINTF(_fib, _fam, _func, _fmt, ...) \
    printf("[fib_algo] %s.%u %s: " _fmt "\n", print_family(_fam), _fib, _func, ## __VA_ARGS__)
#define	RH_PRINTF(_l, _rh, _fmt, ...)	if (_PASS_MSG(_l)) {	\
    _RH_PRINTF(_rh->rib_fibnum, _rh->rib_family, __func__, _fmt, ## __VA_ARGS__);\
}
#define	FD_PRINTF(_l, _fd, _fmt, ...)	FD_PRINTF_##_l(_l, _fd, _fmt, ## __VA_ARGS__)
#define	_FD_PRINTF(_l, _fd, _fmt, ...)	if (_PASS_MSG(_l)) {		\
    _ALGO_PRINTF(_fd->fd_fibnum, _fd->fd_family, _fd->fd_flm->flm_name,	\
    __func__, _fmt, ## __VA_ARGS__);					\
}
#if FLM_MAX_DEBUG_LEVEL>=LOG_DEBUG
#define	FD_PRINTF_LOG_DEBUG	_FD_PRINTF
#else
#define	FD_PRINTF_LOG_DEBUG()
#endif
#if FLM_MAX_DEBUG_LEVEL>=LOG_INFO
#define	FD_PRINTF_LOG_INFO	_FD_PRINTF
#else
#define	FD_PRINTF_LOG_INFO()
#endif
#define	FD_PRINTF_LOG_NOTICE	_FD_PRINTF
#define	FD_PRINTF_LOG_ERR	_FD_PRINTF
#define	FD_PRINTF_LOG_WARNING	_FD_PRINTF


/* List of all registered lookup algorithms */
static TAILQ_HEAD(, fib_lookup_module) all_algo_list = TAILQ_HEAD_INITIALIZER(all_algo_list);

/* List of all fib lookup instances in the vnet */
VNET_DEFINE_STATIC(TAILQ_HEAD(fib_data_head, fib_data), fib_data_list);
#define	V_fib_data_list	VNET(fib_data_list)

/* Datastructure for storing non-transient fib lookup module failures */
struct fib_error {
	int				fe_family;
	uint32_t			fe_fibnum;	/* failed rtable */
	struct fib_lookup_module	*fe_flm;	/* failed module */
	TAILQ_ENTRY(fib_error)		entries;/* list of all errored entries */
};
VNET_DEFINE_STATIC(TAILQ_HEAD(fib_error_head, fib_error), fib_error_list);
#define	V_fib_error_list VNET(fib_error_list)

/* Per-family array of fibnum -> {func, arg} mappings used in datapath */
struct fib_dp_header {
	struct epoch_context	fdh_epoch_ctx;
	uint32_t		fdh_num_tables;
	struct fib_dp		fdh_idx[0];
};

/*
 * Tries to add new non-transient algorithm error to the list of
 *  errors.
 * Returns true on success.
 */
static bool
flm_error_add(struct fib_lookup_module *flm, uint32_t fibnum)
{
	struct fib_error *fe;

	fe = malloc(sizeof(struct fib_error), M_TEMP, M_NOWAIT | M_ZERO);
	if (fe == NULL)
		return (false);
	fe->fe_flm = flm;
	fe->fe_family = flm->flm_family;
	fe->fe_fibnum = fibnum;

	FIB_MOD_LOCK();
	/* Avoid duplicates by checking if error already exists first */
	if (flm_error_check(flm, fibnum)) {
		FIB_MOD_UNLOCK();
		free(fe, M_TEMP);
		return (true);
	}
	TAILQ_INSERT_HEAD(&V_fib_error_list, fe, entries);
	FIB_MOD_UNLOCK();

	return (true);
}

/*
 * True if non-transient error has been registered for @flm in @fibnum.
 */
static bool
flm_error_check(const struct fib_lookup_module *flm, uint32_t fibnum)
{
	const struct fib_error *fe;

	TAILQ_FOREACH(fe, &V_fib_error_list, entries) {
		if ((fe->fe_flm == flm) && (fe->fe_fibnum == fibnum))
			return (true);
	}

	return (false);
}

/*
 * Clear all errors of algo specified by @flm.
 */
static void
fib_error_clear_flm(struct fib_lookup_module *flm)
{
	struct fib_error *fe, *fe_tmp;

	FIB_MOD_LOCK_ASSERT();

	TAILQ_FOREACH_SAFE(fe, &V_fib_error_list, entries, fe_tmp) {
		if (fe->fe_flm == flm) {
			TAILQ_REMOVE(&V_fib_error_list, fe, entries);
			free(fe, M_TEMP);
		}
	}
}

/*
 * Clears all errors in current VNET.
 */
static void
fib_error_clear()
{
	struct fib_error *fe, *fe_tmp;

	FIB_MOD_LOCK_ASSERT();

	TAILQ_FOREACH_SAFE(fe, &V_fib_error_list, entries, fe_tmp) {
		TAILQ_REMOVE(&V_fib_error_list, fe, entries);
		free(fe, M_TEMP);
	}
}

static const char *
print_op_result(enum flm_op_result result)
{
	switch (result) {
	case FLM_SUCCESS:
		return "success";
	case FLM_REBUILD:
		return "rebuild";
	case FLM_ERROR:
		return "error";
	}

	return "unknown";
}

static const char *
print_family(int family)
{

	if (family == AF_INET)
		return ("inet");
	else if (family == AF_INET6)
		return ("inet6");
	else
		return ("unknown");
}

/*
 * Debug function used by lookup algorithms.
 * Outputs message denoted by @fmt, prepended by "[fib_algo] inetX.Y (algo) "
 */
void
fib_printf(int level, struct fib_data *fd, const char *func, char *fmt, ...)
{
	char buf[128];
	va_list ap;

	if (level > flm_debug_level)
		return;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	_ALGO_PRINTF(fd->fd_fibnum, fd->fd_family, fd->fd_flm->flm_name,
	    func, "%s", buf);
}

/*
 * Outputs list of algorithms supported by the provided address family.
 */
static int
print_algos_sysctl(struct sysctl_req *req, int family)
{
	struct fib_lookup_module *flm;
	struct sbuf sbuf;
	int error, count = 0;

	error = sysctl_wire_old_buffer(req, 0);
	if (error == 0) {
		sbuf_new_for_sysctl(&sbuf, NULL, 512, req);
		TAILQ_FOREACH(flm, &all_algo_list, entries) {
			if (flm->flm_family == family) {
				if (count++ > 0)
					sbuf_cat(&sbuf, ", ");
				sbuf_cat(&sbuf, flm->flm_name);
			}
		}
		error = sbuf_finish(&sbuf);
		sbuf_delete(&sbuf);
	}
	return (error);
}

#ifdef INET6
static int
print_algos_sysctl_inet6(SYSCTL_HANDLER_ARGS)
{

	return (print_algos_sysctl(req, AF_INET6));
}
SYSCTL_PROC(_net_route_algo_inet6, OID_AUTO, algo_list,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    print_algos_sysctl_inet6, "A", "List of IPv6 lookup algorithms");
#endif

#ifdef INET
static int
print_algos_sysctl_inet(SYSCTL_HANDLER_ARGS)
{

	return (print_algos_sysctl(req, AF_INET));
}
SYSCTL_PROC(_net_route_algo_inet, OID_AUTO, algo_list,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    print_algos_sysctl_inet, "A", "List of IPv4 lookup algorithms");
#endif

/*
 * Calculate delay between repeated failures.
 * Returns current delay in milliseconds.
 */
static uint32_t
callout_calc_delay_ms(struct fib_data *fd)
{
	uint32_t shift;

	if (fd->fd_failed_rebuilds > 10)
		shift = 10;
	else
		shift = fd->fd_failed_rebuilds;

	return ((1 << shift) * FIB_CALLOUT_DELAY_MS);
}

static void
schedule_callout(struct fib_data *fd, int delay_ms)
{

	callout_reset_sbt(&fd->fd_callout, 0, SBT_1MS * delay_ms,
	    rebuild_fd_callout, fd, 0);
}

static void
schedule_fd_rebuild(struct fib_data *fd)
{

	FIB_MOD_LOCK();
	if (!fd->fd_need_rebuild) {
		fd->fd_need_rebuild = true;

		/*
		 * Potentially re-schedules pending callout
		 *  initiated by schedule_algo_eval.
		 */
		FD_PRINTF(LOG_INFO, fd, "Scheduling rebuilt");
		schedule_callout(fd, callout_calc_delay_ms(fd));
	}
	FIB_MOD_UNLOCK();
}

static void
schedule_algo_eval(struct fib_data *fd)
{

	if (fd->fd_num_changes++ == 0) {
		/* Start callout to consider switch */
		FIB_MOD_LOCK();
		if (!callout_pending(&fd->fd_callout))
			schedule_callout(fd, ALGO_EVAL_DELAY_MS);
		FIB_MOD_UNLOCK();
	} else if (fd->fd_num_changes > ALGO_EVAL_NUM_ROUTES && !fd->fd_force_eval) {
		/* Reset callout to exec immediately */
		FIB_MOD_LOCK();
		if (!fd->fd_need_rebuild) {
			fd->fd_force_eval = true;
			schedule_callout(fd, 1);
		}
		FIB_MOD_UNLOCK();
	}
}

/*
 * Rib subscription handler. Checks if the algorithm is ready to
 *  receive updates, handles nexthop refcounting and passes change
 *  data to the algorithm callback.
 */
static void
handle_rtable_change_cb(struct rib_head *rnh, struct rib_cmd_info *rc,
    void *_data)
{
	struct fib_data *fd = (struct fib_data *)_data;
	enum flm_op_result result;

	RIB_WLOCK_ASSERT(rnh);

	/*
	 * There is a small gap between subscribing for route changes
	 *  and initiating rtable dump. Avoid receiving route changes
	 *  prior to finishing rtable dump by checking `init_done`.
	 */
	if (!fd->init_done)
		return;
	/*
	 * If algo requested rebuild, stop sending updates by default.
	 * This simplifies nexthop refcount handling logic.
	 */
	if (fd->fd_need_rebuild)
		return;

	/* Consider scheduling algorithm re-evaluation */
	schedule_algo_eval(fd);

	/*
	 * Maintain guarantee that every nexthop returned by the dataplane
	 *  lookup has > 0 refcount, so can be safely referenced within current
	 *  epoch.
	 */
	if (rc->rc_nh_new != NULL) {
		if (fib_ref_nhop(fd, rc->rc_nh_new) == 0) {
			/* ran out of indexes */
			schedule_fd_rebuild(fd);
			return;
		}
	}

	result = fd->fd_flm->flm_change_rib_item_cb(rnh, rc, fd->fd_algo_data);

	switch (result) {
	case FLM_SUCCESS:
		/* Unref old nexthop on success */
		if (rc->rc_nh_old != NULL)
			fib_unref_nhop(fd, rc->rc_nh_old);
		break;
	case FLM_REBUILD:

		/*
		 * Algo is not able to apply the update.
		 * Schedule algo rebuild.
		 */
		schedule_fd_rebuild(fd);
		break;
	case FLM_ERROR:

		/*
		 * Algo reported a non-recoverable error.
		 * Record the error and schedule rebuild, which will
		 *  trigger best algo selection.
		 */
		FD_PRINTF(LOG_ERR, fd, "algo reported non-recoverable error");
		if (!flm_error_add(fd->fd_flm, fd->fd_fibnum))
			FD_PRINTF(LOG_ERR, fd, "failed to ban algo");
		schedule_fd_rebuild(fd);
	}
}

static void
estimate_nhop_scale(const struct fib_data *old_fd, struct fib_data *fd)
{

	if (old_fd == NULL) {
		// TODO: read from rtable
		fd->number_nhops = 16;
		return;
	}

	if (old_fd->hit_nhops && old_fd->number_nhops < FIB_MAX_NHOPS)
		fd->number_nhops = 2 * old_fd->number_nhops;
	else
		fd->number_nhops = old_fd->number_nhops;
}

struct walk_cbdata {
	struct fib_data		*fd;
	flm_dump_t		*func;
	enum flm_op_result	result;
};

/*
 * Handler called after all rtenties have been dumped.
 * Performs post-dump framework checks and calls
 * algo:flm_dump_end_cb().
 *
 * Updates walk_cbdata result.
 */
static void
sync_algo_end_cb(struct rib_head *rnh, enum rib_walk_hook stage, void *_data)
{
	struct walk_cbdata *w = (struct walk_cbdata *)_data;
	struct fib_data *fd = w->fd;

	RIB_WLOCK_ASSERT(w->fd->fd_rh);

	if (rnh->rib_dying) {
		w->result = FLM_ERROR;
		return;
	}

	if (fd->hit_nhops) {
		FD_PRINTF(LOG_INFO, fd, "ran out of nexthops at %u nhops",
		    fd->nh_ref_table->count);
		if (w->result == FLM_SUCCESS)
			w->result = FLM_REBUILD;
		return;
	}

	if (stage != RIB_WALK_HOOK_POST || w->result != FLM_SUCCESS)
		return;

	/* Post-dump hook, dump successful */
	w->result = fd->fd_flm->flm_dump_end_cb(fd->fd_algo_data, &fd->fd_dp);

	if (w->result == FLM_SUCCESS) {
		/* Mark init as done to allow routing updates */
		fd->init_done = 1;
	}
}

/*
 * Callback for each entry in rib.
 * Calls algo:flm_dump_rib_item_cb func as a part of initial
 *  route table synchronisation.
 */
static int
sync_algo_cb(struct rtentry *rt, void *_data)
{
	struct walk_cbdata *w = (struct walk_cbdata *)_data;

	RIB_WLOCK_ASSERT(w->fd->fd_rh);

	if (w->result == FLM_SUCCESS && w->func) {

		/*
		 * Reference nexthops to maintain guarantee that
		 *  each nexthop returned by datapath has > 0 references
		 *  and can be safely referenced within current epoch.
		 */
		struct nhop_object *nh = rt_get_raw_nhop(rt);
		if (fib_ref_nhop(w->fd, nh) != 0)
			w->result = w->func(rt, w->fd->fd_algo_data);
		else
			w->result = FLM_REBUILD;
	}

	return (0);
}

/*
 * Dump all routing table state to the algo instance.
 */
static enum flm_op_result
sync_algo(struct fib_data *fd)
{
	struct walk_cbdata w = {
		.fd = fd,
		.func = fd->fd_flm->flm_dump_rib_item_cb,
		.result = FLM_SUCCESS,
	};

	rib_walk_ext_internal(fd->fd_rh, true, sync_algo_cb, sync_algo_end_cb, &w);

	FD_PRINTF(LOG_INFO, fd, "initial dump completed, result: %s",
	    print_op_result(w.result));

	return (w.result);
}

/*
 * Schedules epoch-backed @fd instance deletion.
 * * Unlinks @fd from the list of active algo instances.
 * * Removes rib subscription.
 * * Stops callout.
 * * Schedules actual deletion.
 *
 * Assume @fd is already unlinked from the datapath.
 */
static int
schedule_destroy_fd_instance(struct fib_data *fd, bool in_callout)
{
	bool is_dead;

	NET_EPOCH_ASSERT();

	FIB_MOD_LOCK();
	is_dead = fd->fd_dead;
	if (!is_dead)
		fd->fd_dead = true;
	if (fd->fd_linked) {
		TAILQ_REMOVE(&V_fib_data_list, fd, entries);
		fd->fd_linked = false;
	}
	FIB_MOD_UNLOCK();
	if (is_dead)
		return (0);

	FD_PRINTF(LOG_INFO, fd, "DETACH");

	if (fd->fd_rs != NULL)
		rib_unsibscribe(fd->fd_rs);

	/*
	 * After rib_unsubscribe() no _new_ handle_rtable_change_cb() calls
	 * will be executed, hence no _new_ callout schedules will happen.
	 *
	 * There can be 2 possible scenarious here:
	 * 1) we're running inside a callout when we're deleting ourselves
	 *  due to migration to a newer fd
	 * 2) we're running from rt_table_destroy() and callout is scheduled
	 *  for execution OR is executing
	 *
	 * For (2) we need to wait for the callout termination, as the routing table
	 * will be destroyed after this function returns.
	 * For (1) we cannot call drain, but can ensure that this is the last invocation.
	 */

	if (in_callout)
		callout_stop(&fd->fd_callout);
	else
		callout_drain(&fd->fd_callout);

	epoch_call(net_epoch_preempt, destroy_fd_instance_epoch,
	    &fd->fd_epoch_ctx);

	return (0);
}

/*
 * Wipe all fd instances from the list matching rib specified by @rh.
 * If @keep_first is set, remove all but the first record.
 */
static void
fib_cleanup_algo(struct rib_head *rh, bool keep_first, bool in_callout)
{
	struct fib_data_head tmp_head = TAILQ_HEAD_INITIALIZER(tmp_head);
	struct fib_data *fd, *fd_tmp;
	struct epoch_tracker et;

	FIB_MOD_LOCK();
	TAILQ_FOREACH_SAFE(fd, &V_fib_data_list, entries, fd_tmp) {
		if (fd->fd_rh == rh) {
			if (keep_first) {
				keep_first = false;
				continue;
			}
			TAILQ_REMOVE(&V_fib_data_list, fd, entries);
			fd->fd_linked = false;
			TAILQ_INSERT_TAIL(&tmp_head, fd, entries);
		}
	}
	FIB_MOD_UNLOCK();

	/* Pass 2: remove each entry */
	NET_EPOCH_ENTER(et);
	TAILQ_FOREACH_SAFE(fd, &tmp_head, entries, fd_tmp) {
		schedule_destroy_fd_instance(fd, in_callout);
	}
	NET_EPOCH_EXIT(et);
}

void
fib_destroy_rib(struct rib_head *rh)
{

	/*
	 * rnh has `is_dying` flag set, so setup of new fd's will fail at
	 *  sync_algo() stage, preventing new entries to be added to the list
	 *  of active algos. Remove all existing entries for the particular rib.
	 */
	fib_cleanup_algo(rh, false, false);
}

/*
 * Finalises fd destruction by freeing all fd resources.
 */
static void
destroy_fd_instance(struct fib_data *fd)
{

	FD_PRINTF(LOG_INFO, fd, "destroy fd %p", fd);

	/* Call destroy callback first */
	if (fd->fd_algo_data != NULL)
		fd->fd_flm->flm_destroy_cb(fd->fd_algo_data);

	/* Nhop table */
	if ((fd->nh_idx != NULL) && (fd->nh_ref_table != NULL)) {
		for (int i = 0; i < fd->number_nhops; i++) {
			if (!is_idx_free(fd, i)) {
				FD_PRINTF(LOG_DEBUG, fd, " FREE nhop %d %p",
				    i, fd->nh_idx[i]);
				nhop_free_any(fd->nh_idx[i]);
			}
		}
		free(fd->nh_idx, M_RTABLE);
	}
	if (fd->nh_ref_table != NULL)
		free(fd->nh_ref_table, M_RTABLE);

	fib_unref_algo(fd->fd_flm);

	free(fd, M_RTABLE);
}

/*
 * Epoch callback indicating fd is safe to destroy
 */
static void
destroy_fd_instance_epoch(epoch_context_t ctx)
{
	struct fib_data *fd;

	fd = __containerof(ctx, struct fib_data, fd_epoch_ctx);

	destroy_fd_instance(fd);
}

/*
 * Tries to setup fd instance.
 * - Allocates fd/nhop table
 * - Runs algo:flm_init_cb algo init
 * - Subscribes fd to the rib
 * - Runs rtable dump
 * - Adds instance to the list of active instances.
 *
 * Returns: operation result. Fills in @pfd with resulting fd on success.
 *
 */
static enum flm_op_result
try_setup_fd_instance(struct fib_lookup_module *flm, struct rib_head *rh,
    struct fib_data *old_fd, struct fib_data **pfd)
{
	struct fib_data *fd;
	size_t size;
	enum flm_op_result result;

	/* Allocate */
	fd = malloc(sizeof(struct fib_data), M_RTABLE, M_NOWAIT | M_ZERO);
	if (fd == NULL)  {
		*pfd = NULL;
		return (FLM_REBUILD);
	}
	*pfd = fd;

	estimate_nhop_scale(old_fd, fd);

	fd->fd_rh = rh;
	fd->fd_family = rh->rib_family;
	fd->fd_fibnum = rh->rib_fibnum;
	callout_init(&fd->fd_callout, 1);
	fd->fd_vnet = curvnet;
	fd->fd_flm = flm;

	FIB_MOD_LOCK();
	flm->flm_refcount++;
	FIB_MOD_UNLOCK();

	/* Allocate nhidx -> nhop_ptr table */
	size = fd->number_nhops * sizeof(void *);
	fd->nh_idx = malloc(size, M_RTABLE, M_NOWAIT | M_ZERO);
	if (fd->nh_idx == NULL) {
		FD_PRINTF(LOG_INFO, fd, "Unable to allocate nhop table idx (sz:%zu)", size);
		return (FLM_REBUILD);
	}

	/* Allocate nhop index refcount table */
	size = sizeof(struct nhop_ref_table);
	size += fd->number_nhops * sizeof(uint32_t);
	fd->nh_ref_table = malloc(size, M_RTABLE, M_NOWAIT | M_ZERO);
	if (fd->nh_ref_table == NULL) {
		FD_PRINTF(LOG_INFO, fd, "Unable to allocate nhop refcount table (sz:%zu)", size);
		return (FLM_REBUILD);
	}
	FD_PRINTF(LOG_DEBUG, fd, "Allocated %u nhop indexes", fd->number_nhops);

	/* Okay, we're ready for algo init */
	void *old_algo_data = (old_fd != NULL) ? old_fd->fd_algo_data : NULL;
	result = flm->flm_init_cb(fd->fd_fibnum, fd, old_algo_data, &fd->fd_algo_data);
	if (result != FLM_SUCCESS)
		return (result);

	/* Try to subscribe */
	if (flm->flm_change_rib_item_cb != NULL) {
		fd->fd_rs = rib_subscribe_internal(fd->fd_rh,
		    handle_rtable_change_cb, fd, RIB_NOTIFY_IMMEDIATE, 0);
		if (fd->fd_rs == NULL)
			return (FLM_REBUILD);
	}

	/* Dump */
	result = sync_algo(fd);
	if (result != FLM_SUCCESS)
		return (result);
	FD_PRINTF(LOG_INFO, fd, "DUMP completed successfully.");

	FIB_MOD_LOCK();
	/*
	 * Insert fd in the beginning of a list, to maintain invariant
	 *  that first matching entry for the AF/fib is always the active
	 *  one.
	 */
	TAILQ_INSERT_HEAD(&V_fib_data_list, fd, entries);
	fd->fd_linked = true;
	FIB_MOD_UNLOCK();

	return (FLM_SUCCESS);
}

/*
 * Sets up algo @flm for table @rh and links it to the datapath.
 *
 */
static enum flm_op_result
setup_fd_instance(struct fib_lookup_module *flm, struct rib_head *rh,
    struct fib_data *orig_fd, struct fib_data **pfd, bool attach)
{
	struct fib_data *prev_fd, *new_fd;
	struct epoch_tracker et;
	enum flm_op_result result;

	prev_fd = orig_fd;
	new_fd = NULL;
	for (int i = 0; i < FIB_MAX_TRIES; i++) {
		NET_EPOCH_ENTER(et);
		result = try_setup_fd_instance(flm, rh, prev_fd, &new_fd);

		if ((result == FLM_SUCCESS) && attach)
			result = attach_datapath(new_fd);

		if ((prev_fd != NULL) && (prev_fd != orig_fd)) {
			schedule_destroy_fd_instance(prev_fd, false);
			prev_fd = NULL;
		}
		NET_EPOCH_EXIT(et);

		RH_PRINTF(LOG_INFO, rh, "try %d: fib algo result: %s", i,
		    print_op_result(result));

		if (result == FLM_REBUILD) {
			prev_fd = new_fd;
			new_fd = NULL;
			continue;
		}

		break;
	}

	if (result != FLM_SUCCESS) {
		/* update failure count */
		FIB_MOD_LOCK();
		if (orig_fd != NULL)
			orig_fd->fd_failed_rebuilds++;
		FIB_MOD_UNLOCK();

		/* Ban algo on non-recoverable error */
		if (result == FLM_ERROR)
			flm_error_add(flm, rh->rib_fibnum);

		NET_EPOCH_ENTER(et);
		if ((prev_fd != NULL) && (prev_fd != orig_fd))
			schedule_destroy_fd_instance(prev_fd, false);
		if (new_fd != NULL) {
			schedule_destroy_fd_instance(new_fd, false);
			new_fd = NULL;
		}
		NET_EPOCH_EXIT(et);
	}

	*pfd = new_fd;
	return (result);
}

/*
 * Callout for all scheduled fd-related work.
 * - Checks if the current algo is still the best algo
 * - Creates a new instance of an algo for af/fib if desired.
 */
static void
rebuild_fd_callout(void *_data)
{
	struct fib_data *fd, *fd_new, *fd_tmp;
	struct fib_lookup_module *flm_new = NULL;
	struct epoch_tracker et;
	enum flm_op_result result;
	bool need_rebuild = false;

	fd = (struct fib_data *)_data;

	FIB_MOD_LOCK();
	need_rebuild = fd->fd_need_rebuild;
	fd->fd_need_rebuild = false;
	fd->fd_force_eval = false;
	fd->fd_num_changes = 0;
	FIB_MOD_UNLOCK();

	CURVNET_SET(fd->fd_vnet);

	/* First, check if we're still OK to use this algo */
	if (!is_algo_fixed(fd->fd_rh))
		flm_new = fib_check_best_algo(fd->fd_rh, fd->fd_flm);
	if ((flm_new == NULL) && (!need_rebuild)) {
		/* Keep existing algo, no need to rebuild. */
		CURVNET_RESTORE();
		return;
	}

	if (flm_new == NULL) {
		flm_new = fd->fd_flm;
		fd_tmp = fd;
	} else {
		fd_tmp = NULL;
		FD_PRINTF(LOG_NOTICE, fd, "switching algo to %s", flm_new->flm_name);
	}
	result = setup_fd_instance(flm_new, fd->fd_rh, fd_tmp, &fd_new, true);
	if (fd_tmp == NULL) {
		/* fd_new represents new algo */
		fib_unref_algo(flm_new);
	}
	if (result != FLM_SUCCESS) {
		FD_PRINTF(LOG_NOTICE, fd, "table rebuild failed");
		CURVNET_RESTORE();
		return;
	}
	FD_PRINTF(LOG_INFO, fd_new, "switched to new instance");

	/* Remove old instance removal */
	if (fd != NULL) {
		NET_EPOCH_ENTER(et);
		schedule_destroy_fd_instance(fd, true);
		NET_EPOCH_EXIT(et);
	}

	CURVNET_RESTORE();
}

/*
 * Finds algo by name/family.
 * Returns referenced algo or NULL.
 */
static struct fib_lookup_module *
fib_find_algo(const char *algo_name, int family)
{
	struct fib_lookup_module *flm;

	FIB_MOD_LOCK();
	TAILQ_FOREACH(flm, &all_algo_list, entries) {
		if ((strcmp(flm->flm_name, algo_name) == 0) &&
		    (family == flm->flm_family)) {
			flm->flm_refcount++;
			FIB_MOD_UNLOCK();
			return (flm);
		}
	}
	FIB_MOD_UNLOCK();

	return (NULL);
}

static void
fib_unref_algo(struct fib_lookup_module *flm)
{

	FIB_MOD_LOCK();
	flm->flm_refcount--;
	FIB_MOD_UNLOCK();
}

static int
set_fib_algo(uint32_t fibnum, int family, struct sysctl_oid *oidp, struct sysctl_req *req)
{
	struct fib_lookup_module *flm = NULL;
	struct fib_data *fd = NULL;
	char old_algo_name[32], algo_name[32];
	struct rib_head *rh = NULL;
	enum flm_op_result result;
	int error;

	/* Fetch current algo/rib for af/family */
	FIB_MOD_LOCK();
	TAILQ_FOREACH(fd, &V_fib_data_list, entries) {
		if ((fd->fd_family == family) && (fd->fd_fibnum == fibnum))
			break;
	}
	if (fd == NULL) {
		FIB_MOD_UNLOCK();
		return (ENOENT);
	}
	rh = fd->fd_rh;
	strlcpy(old_algo_name, fd->fd_flm->flm_name,
	    sizeof(old_algo_name));
	FIB_MOD_UNLOCK();

	strlcpy(algo_name, old_algo_name, sizeof(algo_name));
	error = sysctl_handle_string(oidp, algo_name, sizeof(algo_name), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (strcmp(algo_name, old_algo_name) == 0)
		return (0);

	/* New algorithm name is different */
	flm = fib_find_algo(algo_name, family);
	if (flm == NULL) {
		RH_PRINTF(LOG_INFO, rh, "unable to find algo %s", algo_name);
		return (ESRCH);
	}

	fd = NULL;
	result = setup_fd_instance(flm, rh, NULL, &fd, true);
	fib_unref_algo(flm);
	if (result != FLM_SUCCESS)
		return (EINVAL);

	/* Disable automated jumping between algos */
	FIB_MOD_LOCK();
	set_algo_fixed(rh);
	FIB_MOD_UNLOCK();
	/* Remove old instance(s) */
	fib_cleanup_algo(rh, true, false);

	/* Drain cb so user can unload the module after userret if so desired */
	epoch_drain_callbacks(net_epoch_preempt);

	return (0);
}

#ifdef INET
static int
set_algo_inet_sysctl_handler(SYSCTL_HANDLER_ARGS)
{

	return (set_fib_algo(RT_DEFAULT_FIB, AF_INET, oidp, req));
}
SYSCTL_PROC(_net_route_algo_inet, OID_AUTO, algo,
    CTLFLAG_VNET | CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0,
    set_algo_inet_sysctl_handler, "A", "Set IPv4 lookup algo");
#endif

#ifdef INET6
static int
set_algo_inet6_sysctl_handler(SYSCTL_HANDLER_ARGS)
{

	return (set_fib_algo(RT_DEFAULT_FIB, AF_INET6, oidp, req));
}
SYSCTL_PROC(_net_route_algo_inet6, OID_AUTO, algo,
    CTLFLAG_VNET | CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0,
    set_algo_inet6_sysctl_handler, "A", "Set IPv6 lookup algo");
#endif

static void
destroy_fdh_epoch(epoch_context_t ctx)
{
	struct fib_dp_header *fdh;

	fdh = __containerof(ctx, struct fib_dp_header, fdh_epoch_ctx);
	free(fdh, M_RTABLE);
}

static struct fib_dp_header *
alloc_fib_dp_array(uint32_t num_tables, bool waitok)
{
	size_t sz;
	struct fib_dp_header *fdh;

	sz = sizeof(struct fib_dp_header);
	sz += sizeof(struct fib_dp) * num_tables;
	fdh = malloc(sz, M_RTABLE, (waitok ? M_WAITOK : M_NOWAIT) | M_ZERO);
	if (fdh != NULL)
		fdh->fdh_num_tables = num_tables;
	return (fdh);
}

static struct fib_dp_header *
get_fib_dp_header(struct fib_dp *dp)
{

	return (__containerof((void *)dp, struct fib_dp_header, fdh_idx));
}

/*
 * Replace per-family index pool @pdp with a new one which
 * contains updated callback/algo data from @fd.
 * Returns 0 on success.
 */
static enum flm_op_result
replace_rtables_family(struct fib_dp **pdp, struct fib_data *fd)
{
	struct fib_dp_header *new_fdh, *old_fdh;

	NET_EPOCH_ASSERT();

	FD_PRINTF(LOG_DEBUG, fd, "[vnet %p] replace with f:%p arg:%p",
	    curvnet, fd->fd_dp.f, fd->fd_dp.arg);

	FIB_MOD_LOCK();
	old_fdh = get_fib_dp_header(*pdp);
	new_fdh = alloc_fib_dp_array(old_fdh->fdh_num_tables, false);
	FD_PRINTF(LOG_DEBUG, fd, "OLD FDH: %p NEW FDH: %p", old_fdh, new_fdh);
	if (new_fdh == NULL) {
		FIB_MOD_UNLOCK();
		FD_PRINTF(LOG_WARNING, fd, "error attaching datapath");
		return (FLM_REBUILD);
	}

	memcpy(&new_fdh->fdh_idx[0], &old_fdh->fdh_idx[0],
	    old_fdh->fdh_num_tables * sizeof(struct fib_dp));
	/* Update relevant data structure for @fd */
	new_fdh->fdh_idx[fd->fd_fibnum] = fd->fd_dp;

	/* Ensure memcpy() writes have completed */
	atomic_thread_fence_rel();
	/* Set new datapath pointer */
	*pdp = &new_fdh->fdh_idx[0];
	FIB_MOD_UNLOCK();
	FD_PRINTF(LOG_DEBUG, fd, "update %p -> %p", old_fdh, new_fdh);

	epoch_call(net_epoch_preempt, destroy_fdh_epoch,
	    &old_fdh->fdh_epoch_ctx);

	return (FLM_SUCCESS);
}

static struct fib_dp **
get_family_dp_ptr(int family)
{
	switch (family) {
	case AF_INET:
		return (&V_inet_dp);
	case AF_INET6:
		return (&V_inet6_dp);
	}
	return (NULL);
}

/*
 * Make datapath use fib instance @fd
 */
static enum flm_op_result
attach_datapath(struct fib_data *fd)
{
	struct fib_dp **pdp;

	pdp = get_family_dp_ptr(fd->fd_family);
	return (replace_rtables_family(pdp, fd));
}

/*
 * Grow datapath pointers array.
 * Called from sysctl handler on growing number of routing tables.
 */
static void
grow_rtables_family(struct fib_dp **pdp, uint32_t new_num_tables)
{
	struct fib_dp_header *new_fdh, *old_fdh = NULL;

	new_fdh = alloc_fib_dp_array(new_num_tables, true);

	FIB_MOD_LOCK();
	if (*pdp != NULL) {
		old_fdh = get_fib_dp_header(*pdp);
		memcpy(&new_fdh->fdh_idx[0], &old_fdh->fdh_idx[0],
		    old_fdh->fdh_num_tables * sizeof(struct fib_dp));
	}

	/* Wait till all writes completed */
	atomic_thread_fence_rel();

	*pdp = &new_fdh->fdh_idx[0];
	FIB_MOD_UNLOCK();

	if (old_fdh != NULL)
		epoch_call(net_epoch_preempt, destroy_fdh_epoch,
		    &old_fdh->fdh_epoch_ctx);
}

/*
 * Grows per-AF arrays of datapath pointers for each supported family.
 * Called from fibs resize sysctl handler.
 */
void
fib_grow_rtables(uint32_t new_num_tables)
{

#ifdef INET
	grow_rtables_family(get_family_dp_ptr(AF_INET), new_num_tables);
#endif
#ifdef INET6
	grow_rtables_family(get_family_dp_ptr(AF_INET6), new_num_tables);
#endif
}

void
fib_get_rtable_info(struct rib_head *rh, struct rib_rtable_info *rinfo)
{

	bzero(rinfo, sizeof(struct rib_rtable_info));
	rinfo->num_prefixes = rh->rnh_prefixes;
	rinfo->num_nhops = nhops_get_count(rh);
#ifdef ROUTE_MPATH
	rinfo->num_nhgrp = nhgrp_get_count(rh);
#endif
}

/*
 * Accessor to get rib instance @fd is attached to.
 */
struct rib_head *
fib_get_rh(struct fib_data *fd)
{

	return (fd->fd_rh);
}

/*
 * Accessor to export idx->nhop array
 */
struct nhop_object **
fib_get_nhop_array(struct fib_data *fd)
{

	return (fd->nh_idx);
}

static uint32_t
get_nhop_idx(struct nhop_object *nh)
{
#ifdef ROUTE_MPATH
	if (NH_IS_NHGRP(nh))
		return (nhgrp_get_idx((struct nhgrp_object *)nh) * 2 - 1);
	else
		return (nhop_get_idx(nh) * 2);
#else
	return (nhop_get_idx(nh));
#endif
}

uint32_t
fib_get_nhop_idx(struct fib_data *fd, struct nhop_object *nh)
{

	return (get_nhop_idx(nh));
}

static bool
is_idx_free(struct fib_data *fd, uint32_t index)
{

	return (fd->nh_ref_table->refcnt[index] == 0);
}

static uint32_t
fib_ref_nhop(struct fib_data *fd, struct nhop_object *nh)
{
	uint32_t idx = get_nhop_idx(nh);

	if (idx >= fd->number_nhops) {
		fd->hit_nhops = 1;
		return (0);
	}

	if (is_idx_free(fd, idx)) {
		nhop_ref_any(nh);
		fd->nh_idx[idx] = nh;
		fd->nh_ref_table->count++;
		FD_PRINTF(LOG_DEBUG, fd, " REF nhop %u %p", idx, fd->nh_idx[idx]);
	}
	fd->nh_ref_table->refcnt[idx]++;

	return (idx);
}

struct nhop_release_data {
	struct nhop_object	*nh;
	struct epoch_context	ctx;
};

static void
release_nhop_epoch(epoch_context_t ctx)
{
	struct nhop_release_data *nrd;

	nrd = __containerof(ctx, struct nhop_release_data, ctx);
	nhop_free_any(nrd->nh);
	free(nrd, M_TEMP);
}

/*
 * Delays nexthop refcount release.
 * Datapath may have the datastructures not updated yet, so the old
 *  nexthop may still be returned till the end of current epoch. Delay
 *  refcount removal, as we may be removing the last instance, which will
 *  trigger nexthop deletion, rendering returned nexthop invalid.
 */
static void
fib_schedule_release_nhop(struct fib_data *fd, struct nhop_object *nh)
{
	struct nhop_release_data *nrd;

	nrd = malloc(sizeof(struct nhop_release_data), M_TEMP, M_NOWAIT | M_ZERO);
	if (nrd != NULL) {
		nrd->nh = nh;
		epoch_call(net_epoch_preempt, release_nhop_epoch, &nrd->ctx);
	} else {
		/*
		 * Unable to allocate memory. Leak nexthop to maintain guarantee
		 *  that each nhop can be referenced.
		 */
		FD_PRINTF(LOG_ERR, fd, "unable to schedule nhop %p deletion", nh);
	}
}

static void
fib_unref_nhop(struct fib_data *fd, struct nhop_object *nh)
{
	uint32_t idx = get_nhop_idx(nh);

	KASSERT((idx < fd->number_nhops), ("invalid nhop index"));
	KASSERT((nh == fd->nh_idx[idx]), ("index table contains whong nh"));

	fd->nh_ref_table->refcnt[idx]--;
	if (fd->nh_ref_table->refcnt[idx] == 0) {
		FD_PRINTF(LOG_DEBUG, fd, " FREE nhop %d %p", idx, fd->nh_idx[idx]);
		fib_schedule_release_nhop(fd, fd->nh_idx[idx]);
	}
}

static void
set_algo_fixed(struct rib_head *rh)
{
	switch (rh->rib_family) {
#ifdef INET
	case AF_INET:
		V_algo_fixed_inet = true;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		V_algo_fixed_inet6 = true;
		break;
#endif
	}
}

static bool
is_algo_fixed(struct rib_head *rh)
{

	switch (rh->rib_family) {
#ifdef INET
	case AF_INET:
		return (V_algo_fixed_inet);
#endif
#ifdef INET6
	case AF_INET6:
		return (V_algo_fixed_inet6);
#endif
	}
	return (false);
}

/*
 * Runs the check on what would be the best algo for rib @rh, assuming
 *  that the current algo is the one specified by @orig_flm. Note that
 *  it can be NULL for initial selection.
 *
 * Returns referenced new algo or NULL if the current one is the best.
 */
static struct fib_lookup_module *
fib_check_best_algo(struct rib_head *rh, struct fib_lookup_module *orig_flm)
{
	uint8_t preference, curr_preference = 0, best_preference = 0;
	struct fib_lookup_module *flm, *best_flm = NULL;
	struct rib_rtable_info rinfo;
	int candidate_algos = 0;

	fib_get_rtable_info(rh, &rinfo);

	FIB_MOD_LOCK();
	TAILQ_FOREACH(flm, &all_algo_list, entries) {
		if (flm->flm_family != rh->rib_family)
			continue;
		candidate_algos++;
		preference = flm->flm_get_pref(&rinfo);
		if (preference > best_preference) {
			if (!flm_error_check(flm, rh->rib_fibnum)) {
				best_preference = preference;
				best_flm = flm;
			}
		}
		if (flm == orig_flm)
			curr_preference = preference;
	}
	if ((best_flm != NULL) && (curr_preference + BEST_DIFF_PERCENT < best_preference))
		best_flm->flm_refcount++;
	else
		best_flm = NULL;
	FIB_MOD_UNLOCK();

	RH_PRINTF(LOG_DEBUG, rh, "candidate_algos: %d, curr: %s(%d) result: %s(%d)",
	    candidate_algos, orig_flm ? orig_flm->flm_name : "NULL", curr_preference,
	    best_flm ? best_flm->flm_name : (orig_flm ? orig_flm->flm_name : "NULL"),
	    best_preference);

	return (best_flm);
}

/*
 * Called when new route table is created.
 * Selects, allocates and attaches fib algo for the table.
 */
int
fib_select_algo_initial(struct rib_head *rh)
{
	struct fib_lookup_module *flm;
	struct fib_data *fd = NULL;
	enum flm_op_result result;
	int error = 0;

	flm = fib_check_best_algo(rh, NULL);
	if (flm == NULL) {
		RH_PRINTF(LOG_CRIT, rh, "no algo selected");
		return (ENOENT);
	}
	RH_PRINTF(LOG_INFO, rh, "selected algo %s", flm->flm_name);

	result = setup_fd_instance(flm, rh, NULL, &fd, false);
	RH_PRINTF(LOG_DEBUG, rh, "result=%d fd=%p", result, fd);
	if (result == FLM_SUCCESS) {

		/*
		 * Attach datapath directly to avoid multiple reallocations
		 * during fib growth
		 */
		struct fib_dp_header *fdp;
		struct fib_dp **pdp;

		pdp = get_family_dp_ptr(rh->rib_family);
		if (pdp != NULL) {
			fdp = get_fib_dp_header(*pdp);
			fdp->fdh_idx[fd->fd_fibnum] = fd->fd_dp;
			FD_PRINTF(LOG_INFO, fd, "datapath attached");
		}
	} else {
		error = EINVAL;
		RH_PRINTF(LOG_CRIT, rh, "unable to setup algo %s", flm->flm_name);
	}

	fib_unref_algo(flm);

	return (error);
}

/*
 * Registers fib lookup module within the subsystem.
 */
int
fib_module_register(struct fib_lookup_module *flm)
{

	FIB_MOD_LOCK();
	ALGO_PRINTF("attaching %s to %s", flm->flm_name,
	    print_family(flm->flm_family));
	TAILQ_INSERT_TAIL(&all_algo_list, flm, entries);
	FIB_MOD_UNLOCK();

	return (0);
}

/*
 * Tries to unregister fib lookup module.
 *
 * Returns 0 on success, EBUSY if module is still used
 *  by some of the tables.
 */
int
fib_module_unregister(struct fib_lookup_module *flm)
{

	FIB_MOD_LOCK();
	if (flm->flm_refcount > 0) {
		FIB_MOD_UNLOCK();
		return (EBUSY);
	}
	fib_error_clear_flm(flm);
	ALGO_PRINTF("detaching %s from %s", flm->flm_name,
	    print_family(flm->flm_family));
	TAILQ_REMOVE(&all_algo_list, flm, entries);
	FIB_MOD_UNLOCK();

	return (0);
}

void
vnet_fib_init(void)
{

	TAILQ_INIT(&V_fib_data_list);
}

void
vnet_fib_destroy(void)
{

	FIB_MOD_LOCK();
	fib_error_clear();
	FIB_MOD_UNLOCK();
}
