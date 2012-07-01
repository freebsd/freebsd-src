/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/errno.h>
#include <sys/cpuvar.h>
#include <sys/stat.h>
#include <sys/modctl.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ksynch.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/kcpc.h>
#include <sys/cap_util.h>
#include <sys/cpc_pcbe.h>
#include <sys/cpc_impl.h>
#include <sys/dtrace_impl.h>

/*
 * DTrace CPU Performance Counter Provider
 * ---------------------------------------
 *
 * The DTrace cpc provider allows DTrace consumers to access the CPU
 * performance counter overflow mechanism of a CPU. The configuration
 * presented in a probe specification is programmed into the performance
 * counter hardware of all available CPUs on a system. Programming the
 * hardware causes a counter on each CPU to begin counting events of the
 * given type. When the specified number of events have occurred, an overflow
 * interrupt will be generated and the probe is fired.
 *
 * The required configuration for the performance counter is encoded into
 * the probe specification and this includes the performance counter event
 * name, processor mode, overflow rate and an optional unit mask.
 *
 * Most processors provide several counters (PICs) which can count all or a
 * subset of the events available for a given CPU. However, when overflow
 * profiling is being used, not all CPUs can detect which counter generated the
 * overflow interrupt. In this case we cannot reliably determine which counter
 * overflowed and we therefore only allow such CPUs to configure one event at
 * a time. Processors that can determine the counter which overflowed are
 * allowed to program as many events at one time as possible (in theory up to
 * the number of instrumentation counters supported by that platform).
 * Therefore, multiple consumers can enable multiple probes at the same time
 * on such platforms. Platforms which cannot determine the source of an
 * overflow interrupt are only allowed to program a single event at one time.
 *
 * The performance counter hardware is made available to consumers on a
 * first-come, first-served basis. Only a finite amount of hardware resource
 * is available and, while we make every attempt to accomodate requests from
 * consumers, we must deny requests when hardware resources have been exhausted.
 * A consumer will fail to enable probes when resources are currently in use.
 *
 * The cpc provider contends for shared hardware resources along with other
 * consumers of the kernel CPU performance counter subsystem (e.g. cpustat(1M)).
 * Only one such consumer can use the performance counters at any one time and
 * counters are made available on a first-come, first-served basis. As with
 * cpustat, the cpc provider has priority over per-LWP libcpc usage (e.g.
 * cputrack(1)). Invoking the cpc provider will cause all existing per-LWP
 * counter contexts to be invalidated.
 */

typedef struct dcpc_probe {
	char		dcpc_event_name[CPC_MAX_EVENT_LEN];
	int		dcpc_flag;	/* flags (USER/SYS) */
	uint32_t	dcpc_ovfval;	/* overflow value */
	int64_t		dcpc_umask;	/* umask/emask for this event */
	int		dcpc_picno;	/* pic this event is programmed in */
	int		dcpc_enabled;	/* probe is actually enabled? */
	int		dcpc_disabling;	/* probe is currently being disabled */
	dtrace_id_t	dcpc_id;	/* probeid this request is enabling */
	int		dcpc_actv_req_idx;	/* idx into dcpc_actv_reqs[] */
} dcpc_probe_t;

static dev_info_t			*dcpc_devi;
static dtrace_provider_id_t		dcpc_pid;
static dcpc_probe_t			**dcpc_actv_reqs;
static uint32_t				dcpc_enablings = 0;
static int				dcpc_ovf_mask = 0;
static int				dcpc_mult_ovf_cap = 0;
static int				dcpc_mask_type = 0;

/*
 * When the dcpc provider is loaded, dcpc_min_overflow is set to either
 * DCPC_MIN_OVF_DEFAULT or the value that dcpc-min-overflow is set to in
 * the dcpc.conf file. Decrease this value to set probes with smaller
 * overflow values. Remember that very small values could render a system
 * unusable with frequently occurring events.
 */
#define	DCPC_MIN_OVF_DEFAULT		5000
static uint32_t				dcpc_min_overflow;

static int dcpc_aframes = 0;	/* override for artificial frame setting */
#if defined(__x86)
#define	DCPC_ARTIFICIAL_FRAMES	8
#elif defined(__sparc)
#define	DCPC_ARTIFICIAL_FRAMES	2
#endif

/*
 * Called from the platform overflow interrupt handler. 'bitmap' is a mask
 * which contains the pic(s) that have overflowed.
 */
static void
dcpc_fire(uint64_t bitmap)
{
	int i;

	/*
	 * No counter was marked as overflowing. Shout about it and get out.
	 */
	if ((bitmap & dcpc_ovf_mask) == 0) {
		cmn_err(CE_NOTE, "dcpc_fire: no counter overflow found\n");
		return;
	}

	/*
	 * This is the common case of a processor that doesn't support
	 * multiple overflow events. Such systems are only allowed a single
	 * enabling and therefore we just look for the first entry in
	 * the active request array.
	 */
	if (!dcpc_mult_ovf_cap) {
		for (i = 0; i < cpc_ncounters; i++) {
			if (dcpc_actv_reqs[i] != NULL) {
				dtrace_probe(dcpc_actv_reqs[i]->dcpc_id,
				    CPU->cpu_cpcprofile_pc,
				    CPU->cpu_cpcprofile_upc, 0, 0, 0);
				return;
			}
		}
		return;
	}

	/*
	 * This is a processor capable of handling multiple overflow events.
	 * Iterate over the array of active requests and locate the counters
	 * that overflowed (note: it is possible for more than one counter to
	 * have overflowed at the same time).
	 */
	for (i = 0; i < cpc_ncounters; i++) {
		if (dcpc_actv_reqs[i] != NULL &&
		    (bitmap & (1ULL << dcpc_actv_reqs[i]->dcpc_picno))) {
			dtrace_probe(dcpc_actv_reqs[i]->dcpc_id,
			    CPU->cpu_cpcprofile_pc,
			    CPU->cpu_cpcprofile_upc, 0, 0, 0);
		}
	}
}

static void
dcpc_create_probe(dtrace_provider_id_t id, const char *probename,
    char *eventname, int64_t umask, uint32_t ovfval, char flag)
{
	dcpc_probe_t *pp;
	int nr_frames = DCPC_ARTIFICIAL_FRAMES + dtrace_mach_aframes();

	if (dcpc_aframes)
		nr_frames = dcpc_aframes;

	if (dtrace_probe_lookup(id, NULL, NULL, probename) != 0)
		return;

	pp = kmem_zalloc(sizeof (dcpc_probe_t), KM_SLEEP);
	(void) strncpy(pp->dcpc_event_name, eventname,
	    sizeof (pp->dcpc_event_name) - 1);
	pp->dcpc_event_name[sizeof (pp->dcpc_event_name) - 1] = '\0';
	pp->dcpc_flag = flag | CPC_OVF_NOTIFY_EMT;
	pp->dcpc_ovfval = ovfval;
	pp->dcpc_umask = umask;
	pp->dcpc_actv_req_idx = pp->dcpc_picno = pp->dcpc_disabling = -1;

	pp->dcpc_id = dtrace_probe_create(id, NULL, NULL, probename,
	    nr_frames, pp);
}

/*ARGSUSED*/
static void
dcpc_provide(void *arg, const dtrace_probedesc_t *desc)
{
	/*
	 * The format of a probe is:
	 *
	 *	event_name-mode-{optional_umask}-overflow_rate
	 * e.g.
	 *	DC_refill_from_system-user-0x1e-50000, or,
	 *	DC_refill_from_system-all-10000
	 *
	 */
	char *str, *end, *p;
	int i, flag = 0;
	char event[CPC_MAX_EVENT_LEN];
	long umask = -1, val = 0;
	size_t evlen, len;

	/*
	 * The 'cpc' provider offers no probes by default.
	 */
	if (desc == NULL)
		return;

	len = strlen(desc->dtpd_name);
	p = str = kmem_alloc(len + 1, KM_SLEEP);
	(void) strcpy(str, desc->dtpd_name);

	/*
	 * We have a poor man's strtok() going on here. Replace any hyphens
	 * in the the probe name with NULL characters in order to make it
	 * easy to parse the string with regular string functions.
	 */
	for (i = 0; i < len; i++) {
		if (str[i] == '-')
			str[i] = '\0';
	}

	/*
	 * The first part of the string must be either a platform event
	 * name or a generic event name.
	 */
	evlen = strlen(p);
	(void) strncpy(event, p, CPC_MAX_EVENT_LEN - 1);
	event[CPC_MAX_EVENT_LEN - 1] = '\0';

	/*
	 * The next part of the name is the mode specification. Valid
	 * settings are "user", "kernel" or "all".
	 */
	p += evlen + 1;

	if (strcmp(p, "user") == 0)
		flag |= CPC_COUNT_USER;
	else if (strcmp(p, "kernel") == 0)
		flag |= CPC_COUNT_SYSTEM;
	else if (strcmp(p, "all") == 0)
		flag |= CPC_COUNT_USER | CPC_COUNT_SYSTEM;
	else
		goto err;

	/*
	 * Next we either have a mask specification followed by an overflow
	 * rate or just an overflow rate on its own.
	 */
	p += strlen(p) + 1;
	if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
		/*
		 * A unit mask can only be specified if:
		 * 1) this performance counter back end supports masks.
		 * 2) the specified event is platform specific.
		 * 3) a valid hex number is converted.
		 * 4) no extraneous characters follow the mask specification.
		 */
		if (dcpc_mask_type != 0 && strncmp(event, "PAPI", 4) != 0 &&
		    ddi_strtol(p, &end, 16, &umask) == 0 &&
		    end == p + strlen(p)) {
			p += strlen(p) + 1;
		} else {
			goto err;
		}
	}

	/*
	 * This final part must be an overflow value which has to be greater
	 * than the minimum permissible overflow rate.
	 */
	if ((ddi_strtol(p, &end, 10, &val) != 0) || end != p + strlen(p) ||
	    val < dcpc_min_overflow)
		goto err;

	/*
	 * Validate the event and create the probe.
	 */
	for (i = 0; i < cpc_ncounters; i++) {
		char *events, *cp, *p, *end;
		int found = 0, j;
		size_t llen;

		if ((events = kcpc_list_events(i)) == NULL)
			goto err;

		llen = strlen(events);
		p = cp = ddi_strdup(events, KM_NOSLEEP);
		end = cp + llen;

		for (j = 0; j < llen; j++) {
			if (cp[j] == ',')
				cp[j] = '\0';
		}

		while (p < end && found == 0) {
			if (strcmp(p, event) == 0) {
				dcpc_create_probe(dcpc_pid, desc->dtpd_name,
				    event, umask, (uint32_t)val, flag);
				found = 1;
			}
			p += strlen(p) + 1;
		}
		kmem_free(cp, llen + 1);

		if (found)
			break;
	}

err:
	kmem_free(str, len + 1);
}

/*ARGSUSED*/
static void
dcpc_destroy(void *arg, dtrace_id_t id, void *parg)
{
	dcpc_probe_t *pp = parg;

	ASSERT(pp->dcpc_enabled == 0);
	kmem_free(pp, sizeof (dcpc_probe_t));
}

/*ARGSUSED*/
static int
dcpc_mode(void *arg, dtrace_id_t id, void *parg)
{
	if (CPU->cpu_cpcprofile_pc == 0) {
		return (DTRACE_MODE_NOPRIV_DROP | DTRACE_MODE_USER);
	} else {
		return (DTRACE_MODE_NOPRIV_DROP | DTRACE_MODE_KERNEL);
	}
}

static void
dcpc_populate_set(cpu_t *c, dcpc_probe_t *pp, kcpc_set_t *set, int reqno)
{
	kcpc_set_t *oset;
	int i;

	(void) strncpy(set->ks_req[reqno].kr_event, pp->dcpc_event_name,
	    CPC_MAX_EVENT_LEN);
	set->ks_req[reqno].kr_config = NULL;
	set->ks_req[reqno].kr_index = reqno;
	set->ks_req[reqno].kr_picnum = -1;
	set->ks_req[reqno].kr_flags =  pp->dcpc_flag;

	/*
	 * If a unit mask has been specified then detect which attribute
	 * the platform needs. For now, it's either "umask" or "emask".
	 */
	if (pp->dcpc_umask >= 0) {
		set->ks_req[reqno].kr_attr =
		    kmem_zalloc(sizeof (kcpc_attr_t), KM_SLEEP);
		set->ks_req[reqno].kr_nattrs = 1;
		if (dcpc_mask_type & DCPC_UMASK)
			(void) strncpy(set->ks_req[reqno].kr_attr->ka_name,
			    "umask", 5);
		else
			(void) strncpy(set->ks_req[reqno].kr_attr->ka_name,
			    "emask", 5);
		set->ks_req[reqno].kr_attr->ka_val = pp->dcpc_umask;
	} else {
		set->ks_req[reqno].kr_attr = NULL;
		set->ks_req[reqno].kr_nattrs = 0;
	}

	/*
	 * If this probe is enabled, obtain its current countdown value
	 * and use that. The CPUs cpc context might not exist yet if we
	 * are dealing with a CPU that is just coming online.
	 */
	if (pp->dcpc_enabled && (c->cpu_cpc_ctx != NULL)) {
		oset = c->cpu_cpc_ctx->kc_set;

		for (i = 0; i < oset->ks_nreqs; i++) {
			if (strcmp(oset->ks_req[i].kr_event,
			    set->ks_req[reqno].kr_event) == 0) {
				set->ks_req[reqno].kr_preset =
				    *(oset->ks_req[i].kr_data);
			}
		}
	} else {
		set->ks_req[reqno].kr_preset = UINT64_MAX - pp->dcpc_ovfval;
	}

	set->ks_nreqs++;
}


/*
 * Create a fresh request set for the enablings represented in the
 * 'dcpc_actv_reqs' array which contains the probes we want to be
 * in the set. This can be called for several reasons:
 *
 * 1)	We are on a single or multi overflow platform and we have no
 *	current events so we can just create the set and initialize it.
 * 2)	We are on a multi-overflow platform and we already have one or
 *	more existing events and we are adding a new enabling. Create a
 *	new set and copy old requests in and then add the new request.
 * 3)	We are on a multi-overflow platform and we have just removed an
 *	enabling but we still have enablings whch are valid. Create a new
 *	set and copy in still valid requests.
 */
static kcpc_set_t *
dcpc_create_set(cpu_t *c)
{
	int i, reqno = 0;
	int active_requests = 0;
	kcpc_set_t *set;

	/*
	 * First get a count of the number of currently active requests.
	 * Note that dcpc_actv_reqs[] should always reflect which requests
	 * we want to be in the set that is to be created. It is the
	 * responsibility of the caller of dcpc_create_set() to adjust that
	 * array accordingly beforehand.
	 */
	for (i = 0; i < cpc_ncounters; i++) {
		if (dcpc_actv_reqs[i] != NULL)
			active_requests++;
	}

	set = kmem_zalloc(sizeof (kcpc_set_t), KM_SLEEP);

	set->ks_req =
	    kmem_zalloc(sizeof (kcpc_request_t) * active_requests, KM_SLEEP);

	set->ks_data =
	    kmem_zalloc(active_requests * sizeof (uint64_t), KM_SLEEP);

	/*
	 * Look for valid entries in the active requests array and populate
	 * the request set for any entries found.
	 */
	for (i = 0; i < cpc_ncounters; i++) {
		if (dcpc_actv_reqs[i] != NULL) {
			dcpc_populate_set(c, dcpc_actv_reqs[i], set, reqno);
			reqno++;
		}
	}

	return (set);
}

static int
dcpc_program_cpu_event(cpu_t *c)
{
	int i, j, subcode;
	kcpc_ctx_t *ctx, *octx;
	kcpc_set_t *set;

	set = dcpc_create_set(c);

	set->ks_ctx = ctx = kcpc_ctx_alloc(KM_SLEEP);
	ctx->kc_set = set;
	ctx->kc_cpuid = c->cpu_id;

	if (kcpc_assign_reqs(set, ctx) != 0)
		goto err;

	if (kcpc_configure_reqs(ctx, set, &subcode) != 0)
		goto err;

	for (i = 0; i < set->ks_nreqs; i++) {
		for (j = 0; j < cpc_ncounters; j++) {
			if (dcpc_actv_reqs[j] != NULL &&
			    strcmp(set->ks_req[i].kr_event,
			    dcpc_actv_reqs[j]->dcpc_event_name) == 0) {
				dcpc_actv_reqs[j]->dcpc_picno =
				    set->ks_req[i].kr_picnum;
			}
		}
	}

	/*
	 * If we already have an active enabling then save the current cpc
	 * context away.
	 */
	octx = c->cpu_cpc_ctx;

	kcpc_cpu_program(c, ctx);

	if (octx != NULL) {
		kcpc_set_t *oset = octx->kc_set;
		kmem_free(oset->ks_data, oset->ks_nreqs * sizeof (uint64_t));
		kcpc_free_configs(oset);
		kcpc_free_set(oset);
		kcpc_ctx_free(octx);
	}

	return (0);

err:
	/*
	 * We failed to configure this request up so free things up and
	 * get out.
	 */
	kcpc_free_configs(set);
	kmem_free(set->ks_data, set->ks_nreqs * sizeof (uint64_t));
	kcpc_free_set(set);
	kcpc_ctx_free(ctx);

	return (-1);
}

static void
dcpc_disable_cpu(cpu_t *c)
{
	kcpc_ctx_t *ctx;
	kcpc_set_t *set;

	/*
	 * Leave this CPU alone if it's already offline.
	 */
	if (c->cpu_flags & CPU_OFFLINE)
		return;

	/*
	 * Grab CPUs CPC context before kcpc_cpu_stop() stops counters and
	 * changes it.
	 */
	ctx = c->cpu_cpc_ctx;

	kcpc_cpu_stop(c, B_FALSE);

	set = ctx->kc_set;

	kcpc_free_configs(set);
	kmem_free(set->ks_data, set->ks_nreqs * sizeof (uint64_t));
	kcpc_free_set(set);
	kcpc_ctx_free(ctx);
}

/*
 * The dcpc_*_interrupts() routines are responsible for manipulating the
 * per-CPU dcpc interrupt state byte. The purpose of the state byte is to
 * synchronize processing of hardware overflow interrupts wth configuration
 * changes made to the CPU performance counter subsystem by the dcpc provider.
 *
 * The dcpc provider claims ownership of the overflow interrupt mechanism
 * by transitioning the state byte from DCPC_INTR_INACTIVE (indicating the
 * dcpc provider is not in use) to DCPC_INTR_FREE (the dcpc provider owns the
 * overflow mechanism and interrupts may be processed). Before modifying
 * a CPUs configuration state the state byte is transitioned from
 * DCPC_INTR_FREE to DCPC_INTR_CONFIG ("configuration in process" state).
 * The hardware overflow handler, kcpc_hw_overflow_intr(), will only process
 * an interrupt when a configuration is not in process (i.e. the state is
 * marked as free). During interrupt processing the state is set to
 * DCPC_INTR_PROCESSING by the overflow handler. When the last dcpc based
 * enabling is removed, the state byte is set to DCPC_INTR_INACTIVE to indicate
 * the dcpc provider is no longer interested in overflow interrupts.
 */
static void
dcpc_block_interrupts(void)
{
	cpu_t *c = cpu_list;
	uint8_t *state;

	ASSERT(cpu_core[c->cpu_id].cpuc_dcpc_intr_state != DCPC_INTR_INACTIVE);

	do {
		state = &cpu_core[c->cpu_id].cpuc_dcpc_intr_state;

		while (atomic_cas_8(state, DCPC_INTR_FREE,
		    DCPC_INTR_CONFIG) != DCPC_INTR_FREE)
			continue;

	} while ((c = c->cpu_next) != cpu_list);
}

/*
 * Set all CPUs dcpc interrupt state to DCPC_INTR_FREE to indicate that
 * overflow interrupts can be processed safely.
 */
static void
dcpc_release_interrupts(void)
{
	cpu_t *c = cpu_list;

	ASSERT(cpu_core[c->cpu_id].cpuc_dcpc_intr_state != DCPC_INTR_INACTIVE);

	do {
		cpu_core[c->cpu_id].cpuc_dcpc_intr_state = DCPC_INTR_FREE;
		membar_producer();
	} while ((c = c->cpu_next) != cpu_list);
}

/*
 * Transition all CPUs dcpc interrupt state from DCPC_INTR_INACTIVE to
 * to DCPC_INTR_FREE. This indicates that the dcpc provider is now
 * responsible for handling all overflow interrupt activity. Should only be
 * called before enabling the first dcpc based probe.
 */
static void
dcpc_claim_interrupts(void)
{
	cpu_t *c = cpu_list;

	ASSERT(cpu_core[c->cpu_id].cpuc_dcpc_intr_state == DCPC_INTR_INACTIVE);

	do {
		cpu_core[c->cpu_id].cpuc_dcpc_intr_state = DCPC_INTR_FREE;
		membar_producer();
	} while ((c = c->cpu_next) != cpu_list);
}

/*
 * Set all CPUs dcpc interrupt state to DCPC_INTR_INACTIVE to indicate that
 * the dcpc provider is no longer processing overflow interrupts. Only called
 * during removal of the last dcpc based enabling.
 */
static void
dcpc_surrender_interrupts(void)
{
	cpu_t *c = cpu_list;

	ASSERT(cpu_core[c->cpu_id].cpuc_dcpc_intr_state != DCPC_INTR_INACTIVE);

	do {
		cpu_core[c->cpu_id].cpuc_dcpc_intr_state = DCPC_INTR_INACTIVE;
		membar_producer();
	} while ((c = c->cpu_next) != cpu_list);
}

/*
 * dcpc_program_event() can be called owing to a new enabling or if a multi
 * overflow platform has disabled a request but needs to  program the requests
 * that are still valid.
 *
 * Every invocation of dcpc_program_event() will create a new kcpc_ctx_t
 * and a new request set which contains the new enabling and any old enablings
 * which are still valid (possible with multi-overflow platforms).
 */
static int
dcpc_program_event(dcpc_probe_t *pp)
{
	cpu_t *c;
	int ret = 0;

	ASSERT(MUTEX_HELD(&cpu_lock));

	kpreempt_disable();

	dcpc_block_interrupts();

	c = cpu_list;

	do {
		/*
		 * Skip CPUs that are currently offline.
		 */
		if (c->cpu_flags & CPU_OFFLINE)
			continue;

		/*
		 * Stop counters but preserve existing DTrace CPC context
		 * if there is one.
		 *
		 * If we come here when the first event is programmed for a CPU,
		 * there should be no DTrace CPC context installed. In this
		 * case, kcpc_cpu_stop() will ensure that there is no other
		 * context on the CPU.
		 *
		 * If we add new enabling to the original one, the CPU should
		 * have the old DTrace CPC context which we need to keep around
		 * since dcpc_program_event() will add to it.
		 */
		if (c->cpu_cpc_ctx != NULL)
			kcpc_cpu_stop(c, B_TRUE);
	} while ((c = c->cpu_next) != cpu_list);

	dcpc_release_interrupts();

	/*
	 * If this enabling is being removed (in the case of a multi event
	 * capable system with more than one active enabling), we can now
	 * update the active request array to reflect the enablings that need
	 * to be reprogrammed.
	 */
	if (pp->dcpc_disabling == 1)
		dcpc_actv_reqs[pp->dcpc_actv_req_idx] = NULL;

	do {
		/*
		 * Skip CPUs that are currently offline.
		 */
		if (c->cpu_flags & CPU_OFFLINE)
			continue;

		ret = dcpc_program_cpu_event(c);
	} while ((c = c->cpu_next) != cpu_list && ret == 0);

	/*
	 * If dcpc_program_cpu_event() fails then it is because we couldn't
	 * configure the requests in the set for the CPU and not because of
	 * an error programming the hardware. If we have a failure here then
	 * we assume no CPUs have been programmed in the above step as they
	 * are all configured identically.
	 */
	if (ret != 0) {
		pp->dcpc_enabled = 0;
		kpreempt_enable();
		return (-1);
	}

	if (pp->dcpc_disabling != 1)
		pp->dcpc_enabled = 1;

	kpreempt_enable();

	return (0);
}

/*ARGSUSED*/
static int
dcpc_enable(void *arg, dtrace_id_t id, void *parg)
{
	dcpc_probe_t *pp = parg;
	int i, found = 0;
	cpu_t *c;

	ASSERT(MUTEX_HELD(&cpu_lock));

	/*
	 * Bail out if the counters are being used by a libcpc consumer.
	 */
	rw_enter(&kcpc_cpuctx_lock, RW_READER);
	if (kcpc_cpuctx > 0) {
		rw_exit(&kcpc_cpuctx_lock);
		return (-1);
	}

	dtrace_cpc_in_use++;
	rw_exit(&kcpc_cpuctx_lock);

	/*
	 * Locate this enabling in the first free entry of the active
	 * request array.
	 */
	for (i = 0; i < cpc_ncounters; i++) {
		if (dcpc_actv_reqs[i] == NULL) {
			dcpc_actv_reqs[i] = pp;
			pp->dcpc_actv_req_idx = i;
			found = 1;
			break;
		}
	}

	/*
	 * If we couldn't find a slot for this probe then there is no
	 * room at the inn.
	 */
	if (!found) {
		dtrace_cpc_in_use--;
		return (-1);
	}

	ASSERT(pp->dcpc_actv_req_idx >= 0);

	/*
	 * DTrace is taking over CPC contexts, so stop collecting
	 * capacity/utilization data for all CPUs.
	 */
	if (dtrace_cpc_in_use == 1)
		cu_disable();

	/*
	 * The following must hold true if we are to (attempt to) enable
	 * this request:
	 *
	 * 1) No enablings currently exist. We allow all platforms to
	 * proceed if this is true.
	 *
	 * OR
	 *
	 * 2) If the platform is multi overflow capable and there are
	 * less valid enablings than there are counters. There is no
	 * guarantee that a platform can accommodate as many events as
	 * it has counters for but we will at least try to program
	 * up to that many requests.
	 *
	 * The 'dcpc_enablings' variable is implictly protected by locking
	 * provided by the DTrace framework and the cpu management framework.
	 */
	if (dcpc_enablings == 0 || (dcpc_mult_ovf_cap &&
	    dcpc_enablings < cpc_ncounters)) {
		/*
		 * Before attempting to program the first enabling we need to
		 * invalidate any lwp-based contexts and lay claim to the
		 * overflow interrupt mechanism.
		 */
		if (dcpc_enablings == 0) {
			kcpc_invalidate_all();
			dcpc_claim_interrupts();
		}

		if (dcpc_program_event(pp) == 0) {
			dcpc_enablings++;
			return (0);
		}
	}

	/*
	 * If active enablings existed before we failed to enable this probe
	 * on a multi event capable platform then we need to restart counters
	 * as they will have been stopped in the attempted configuration. The
	 * context should now just contain the request prior to this failed
	 * enabling.
	 */
	if (dcpc_enablings > 0 && dcpc_mult_ovf_cap) {
		c = cpu_list;

		ASSERT(dcpc_mult_ovf_cap == 1);
		do {
			/*
			 * Skip CPUs that are currently offline.
			 */
			if (c->cpu_flags & CPU_OFFLINE)
				continue;

			kcpc_cpu_program(c, c->cpu_cpc_ctx);
		} while ((c = c->cpu_next) != cpu_list);
	}

	/*
	 * Give up any claim to the overflow interrupt mechanism if no
	 * dcpc based enablings exist.
	 */
	if (dcpc_enablings == 0)
		dcpc_surrender_interrupts();

	dtrace_cpc_in_use--;
	dcpc_actv_reqs[pp->dcpc_actv_req_idx] = NULL;
	pp->dcpc_actv_req_idx = pp->dcpc_picno = -1;

	/*
	 * If all probes are removed, enable capacity/utilization data
	 * collection for every CPU.
	 */
	if (dtrace_cpc_in_use == 0)
		cu_enable();

	return (-1);
}

/*
 * If only one enabling is active then remove the context and free
 * everything up. If there are multiple enablings active then remove this
 * one, its associated meta-data and re-program the hardware.
 */
/*ARGSUSED*/
static void
dcpc_disable(void *arg, dtrace_id_t id, void *parg)
{
	cpu_t *c;
	dcpc_probe_t *pp = parg;

	ASSERT(MUTEX_HELD(&cpu_lock));

	kpreempt_disable();

	/*
	 * This probe didn't actually make it as far as being fully enabled
	 * so we needn't do anything with it.
	 */
	if (pp->dcpc_enabled == 0) {
		/*
		 * If we actually allocated this request a slot in the
		 * request array but failed to enabled it then remove the
		 * entry in the array.
		 */
		if (pp->dcpc_actv_req_idx >= 0) {
			dcpc_actv_reqs[pp->dcpc_actv_req_idx] = NULL;
			pp->dcpc_actv_req_idx = pp->dcpc_picno =
			    pp->dcpc_disabling = -1;
		}

		kpreempt_enable();
		return;
	}

	/*
	 * If this is the only enabling then stop all the counters and
	 * free up the meta-data.
	 */
	if (dcpc_enablings == 1) {
		ASSERT(dtrace_cpc_in_use == 1);

		dcpc_block_interrupts();

		c = cpu_list;

		do {
			dcpc_disable_cpu(c);
		} while ((c = c->cpu_next) != cpu_list);

		dcpc_actv_reqs[pp->dcpc_actv_req_idx] = NULL;
		dcpc_surrender_interrupts();
	} else {
		/*
		 * This platform can support multiple overflow events and
		 * the enabling being disabled is not the last one. Remove this
		 * enabling and re-program the hardware with the new config.
		 */
		ASSERT(dcpc_mult_ovf_cap);
		ASSERT(dcpc_enablings > 1);

		pp->dcpc_disabling = 1;
		(void) dcpc_program_event(pp);
	}

	kpreempt_enable();

	dcpc_enablings--;
	dtrace_cpc_in_use--;
	pp->dcpc_enabled = 0;
	pp->dcpc_actv_req_idx = pp->dcpc_picno = pp->dcpc_disabling = -1;

	/*
	 * If all probes are removed, enable capacity/utilization data
	 * collection for every CPU
	 */
	if (dtrace_cpc_in_use == 0)
		cu_enable();
}

/*ARGSUSED*/
static int
dcpc_cpu_setup(cpu_setup_t what, processorid_t cpu, void *arg)
{
	cpu_t *c;
	uint8_t *state;

	ASSERT(MUTEX_HELD(&cpu_lock));

	switch (what) {
	case CPU_OFF:
		/*
		 * Offline CPUs are not allowed to take part so remove this
		 * CPU if we are actively tracing.
		 */
		if (dtrace_cpc_in_use) {
			c = cpu_get(cpu);
			state = &cpu_core[c->cpu_id].cpuc_dcpc_intr_state;

			/*
			 * Indicate that a configuration is in process in
			 * order to stop overflow interrupts being processed
			 * on this CPU while we disable it.
			 */
			while (atomic_cas_8(state, DCPC_INTR_FREE,
			    DCPC_INTR_CONFIG) != DCPC_INTR_FREE)
				continue;

			dcpc_disable_cpu(c);

			/*
			 * Reset this CPUs interrupt state as the configuration
			 * has ended.
			 */
			cpu_core[c->cpu_id].cpuc_dcpc_intr_state =
			    DCPC_INTR_FREE;
			membar_producer();
		}
		break;

	case CPU_ON:
	case CPU_SETUP:
		/*
		 * This CPU is being initialized or brought online so program
		 * it with the current request set if we are actively tracing.
		 */
		if (dtrace_cpc_in_use) {
			c = cpu_get(cpu);
			(void) dcpc_program_cpu_event(c);
		}
		break;

	default:
		break;
	}

	return (0);
}

static dtrace_pattr_t dcpc_attr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_CPU },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
};

static dtrace_pops_t dcpc_pops = {
    dcpc_provide,
    NULL,
    dcpc_enable,
    dcpc_disable,
    NULL,
    NULL,
    NULL,
    NULL,
    dcpc_mode,
    dcpc_destroy
};

/*ARGSUSED*/
static int
dcpc_open(dev_t *devp, int flag, int otyp, cred_t *cred_p)
{
	return (0);
}

/*ARGSUSED*/
static int
dcpc_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *)dcpc_devi;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

static int
dcpc_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	if (dtrace_unregister(dcpc_pid) != 0)
		return (DDI_FAILURE);

	ddi_remove_minor_node(devi, NULL);

	mutex_enter(&cpu_lock);
	unregister_cpu_setup_func(dcpc_cpu_setup, NULL);
	mutex_exit(&cpu_lock);

	kmem_free(dcpc_actv_reqs, cpc_ncounters * sizeof (dcpc_probe_t *));

	kcpc_unregister_dcpc();

	return (DDI_SUCCESS);
}

static int
dcpc_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	uint_t caps;
	char *attrs;

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	if (kcpc_pcbe_loaded() == -1)
		return (DDI_FAILURE);

	caps = kcpc_pcbe_capabilities();

	if (!(caps & CPC_CAP_OVERFLOW_INTERRUPT)) {
		cmn_err(CE_NOTE, "!dcpc: Counter Overflow not supported"\
		    " on this processor");
		return (DDI_FAILURE);
	}

	if (ddi_create_minor_node(devi, "dcpc", S_IFCHR, 0,
	    DDI_PSEUDO, NULL) == DDI_FAILURE ||
	    dtrace_register("cpc", &dcpc_attr, DTRACE_PRIV_KERNEL,
	    NULL, &dcpc_pops, NULL, &dcpc_pid) != 0) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}

	mutex_enter(&cpu_lock);
	register_cpu_setup_func(dcpc_cpu_setup, NULL);
	mutex_exit(&cpu_lock);

	dcpc_ovf_mask = (1 << cpc_ncounters) - 1;
	ASSERT(dcpc_ovf_mask != 0);

	if (caps & CPC_CAP_OVERFLOW_PRECISE)
		dcpc_mult_ovf_cap = 1;

	/*
	 * Determine which, if any, mask attribute the back-end can use.
	 */
	attrs = kcpc_list_attrs();
	if (strstr(attrs, "umask") != NULL)
		dcpc_mask_type |= DCPC_UMASK;
	else if (strstr(attrs, "emask") != NULL)
		dcpc_mask_type |= DCPC_EMASK;

	/*
	 * The dcpc_actv_reqs array is used to store the requests that
	 * we currently have programmed. The order of requests in this
	 * array is not necessarily the order that the event appears in
	 * the kcpc_request_t array. Once entered into a slot in the array
	 * the entry is not moved until it's removed.
	 */
	dcpc_actv_reqs =
	    kmem_zalloc(cpc_ncounters * sizeof (dcpc_probe_t *), KM_SLEEP);

	dcpc_min_overflow = ddi_prop_get_int(DDI_DEV_T_ANY, devi,
	    DDI_PROP_DONTPASS, "dcpc-min-overflow", DCPC_MIN_OVF_DEFAULT);

	kcpc_register_dcpc(dcpc_fire);

	ddi_report_dev(devi);
	dcpc_devi = devi;

	return (DDI_SUCCESS);
}

static struct cb_ops dcpc_cb_ops = {
	dcpc_open,		/* open */
	nodev,			/* close */
	nulldev,		/* strategy */
	nulldev,		/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	nodev,			/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */
};

static struct dev_ops dcpc_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	dcpc_info,		/* get_dev_info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	dcpc_attach,		/* attach */
	dcpc_detach,		/* detach */
	nodev,			/* reset */
	&dcpc_cb_ops,		/* driver operations */
	NULL,			/* bus operations */
	nodev,			/* dev power */
	ddi_quiesce_not_needed	/* quiesce */
};

/*
 * Module linkage information for the kernel.
 */
static struct modldrv modldrv = {
	&mod_driverops,		/* module type */
	"DTrace CPC Module",	/* name of module */
	&dcpc_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}
