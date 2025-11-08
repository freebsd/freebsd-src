/*-
 * Copyright (c) 2017 Oliver Pinter
 * Copyright (c) 2017 W. Dean Freeman
 * Copyright (c) 2000-2015 Mark R V Murray
 * Copyright (c) 2013 Arthur Mesh
 * Copyright (c) 2004 Robert N. M. Watson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ck.h>
#include <sys/conf.h>
#include <sys/epoch.h>
#include <sys/eventhandler.h>
#include <sys/hash.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/random.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>

#include <machine/atomic.h>
#include <machine/cpu.h>

#include <crypto/rijndael/rijndael-api-fst.h>
#include <crypto/sha2/sha256.h>

#include <dev/random/fortuna.h>
#include <dev/random/hash.h>
#include <dev/random/randomdev.h>
#include <dev/random/random_harvestq.h>

#if defined(RANDOM_ENABLE_ETHER)
#define _RANDOM_HARVEST_ETHER_OFF 0
#else
#define _RANDOM_HARVEST_ETHER_OFF (1u << RANDOM_NET_ETHER)
#endif
#if defined(RANDOM_ENABLE_UMA)
#define _RANDOM_HARVEST_UMA_OFF 0
#else
#define _RANDOM_HARVEST_UMA_OFF (1u << RANDOM_UMA)
#endif

/*
 * Note that random_sources_feed() will also use this to try and split up
 * entropy into a subset of pools per iteration with the goal of feeding
 * HARVESTSIZE into every pool at least once per second.
 */
#define	RANDOM_KTHREAD_HZ	10

static void random_kthread(void);
static void random_sources_feed(void);

/*
 * Random must initialize much earlier than epoch, but we can initialize the
 * epoch code before SMP starts.  Prior to SMP, we can safely bypass
 * concurrency primitives.
 */
static __read_mostly bool epoch_inited;
static __read_mostly epoch_t rs_epoch;

static const char *random_source_descr[];

/*
 * How many events to queue up. We create this many items in
 * an 'empty' queue, then transfer them to the 'harvest' queue with
 * supplied junk. When used, they are transferred back to the
 * 'empty' queue.
 */
#define	RANDOM_RING_MAX		1024
#define	RANDOM_ACCUM_MAX	8

/* 1 to let the kernel thread run, 0 to terminate, -1 to mark completion */
volatile int random_kthread_control;


/*
 * Allow the sysadmin to select the broad category of entropy types to harvest.
 *
 * Updates are synchronized by the harvest mutex.
 */
__read_frequently u_int hc_source_mask;
CTASSERT(ENTROPYSOURCE <= sizeof(hc_source_mask) * NBBY);

struct random_sources {
	CK_LIST_ENTRY(random_sources)	 rrs_entries;
	const struct random_source	*rrs_source;
};

static CK_LIST_HEAD(sources_head, random_sources) source_list =
    CK_LIST_HEAD_INITIALIZER(source_list);

SYSCTL_NODE(_kern_random, OID_AUTO, harvest, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Entropy Device Parameters");

/*
 * Put all the harvest queue context stuff in one place.
 * this make is a bit easier to lock and protect.
 */
static struct harvest_context {
	/* The harvest mutex protects all of harvest_context and
	 * the related data.
	 */
	struct mtx hc_mtx;
	/* Round-robin destination cache. */
	u_int hc_destination[ENTROPYSOURCE];
	/* The context of the kernel thread processing harvested entropy */
	struct proc *hc_kthread_proc;
	/*
	 * A pair of buffers for queued events.  New events are added to the
	 * active queue while the kthread processes the other one in parallel.
	 */
	struct entropy_buffer {
		struct harvest_event ring[RANDOM_RING_MAX];
		u_int pos;
	} hc_entropy_buf[2];
	u_int hc_active_buf;
	struct fast_entropy_accumulator {
		volatile u_int pos;
		uint32_t buf[RANDOM_ACCUM_MAX];
	} hc_entropy_fast_accumulator;
} harvest_context;

#define	RANDOM_HARVEST_INIT_LOCK()	mtx_init(&harvest_context.hc_mtx, \
					    "entropy harvest mutex", NULL, MTX_SPIN)
#define	RANDOM_HARVEST_LOCK()		mtx_lock_spin(&harvest_context.hc_mtx)
#define	RANDOM_HARVEST_UNLOCK()		mtx_unlock_spin(&harvest_context.hc_mtx)

static struct kproc_desc random_proc_kp = {
	"rand_harvestq",
	random_kthread,
	&harvest_context.hc_kthread_proc,
};

/* Pass the given event straight through to Fortuna/Whatever. */
static __inline void
random_harvestq_fast_process_event(struct harvest_event *event)
{
	p_random_alg_context->ra_event_processor(event);
	explicit_bzero(event, sizeof(*event));
}

static void
random_kthread(void)
{
	struct harvest_context *hc;

	hc = &harvest_context;
	for (random_kthread_control = 1; random_kthread_control;) {
		struct entropy_buffer *buf;
		u_int entries;

		/* Deal with queued events. */
		RANDOM_HARVEST_LOCK();
		buf = &hc->hc_entropy_buf[hc->hc_active_buf];
		entries = buf->pos;
		buf->pos = 0;
		hc->hc_active_buf = (hc->hc_active_buf + 1) %
		    nitems(hc->hc_entropy_buf);
		RANDOM_HARVEST_UNLOCK();
		for (u_int i = 0; i < entries; i++)
			random_harvestq_fast_process_event(&buf->ring[i]);

		/* Poll sources of noise. */
		random_sources_feed();

		/* XXX: FIX!! Increase the high-performance data rate? Need some measurements first. */
		for (u_int i = 0; i < RANDOM_ACCUM_MAX; i++) {
			if (hc->hc_entropy_fast_accumulator.buf[i]) {
				random_harvest_direct(&hc->hc_entropy_fast_accumulator.buf[i],
				    sizeof(hc->hc_entropy_fast_accumulator.buf[0]), RANDOM_UMA);
				hc->hc_entropy_fast_accumulator.buf[i] = 0;
			}
		}
		/* XXX: FIX!! This is a *great* place to pass hardware/live entropy to random(9) */
		tsleep_sbt(&hc->hc_kthread_proc, 0, "-",
		    SBT_1S/RANDOM_KTHREAD_HZ, 0, C_PREL(1));
	}
	random_kthread_control = -1;
	wakeup(&hc->hc_kthread_proc);
	kproc_exit(0);
	/* NOTREACHED */
}
SYSINIT(random_device_h_proc, SI_SUB_KICK_SCHEDULER, SI_ORDER_ANY, kproc_start,
    &random_proc_kp);
_Static_assert(SI_SUB_KICK_SCHEDULER > SI_SUB_RANDOM,
    "random kthread starting before subsystem initialization");

static void
rs_epoch_init(void *dummy __unused)
{
	rs_epoch = epoch_alloc("Random Sources", EPOCH_PREEMPT);
	epoch_inited = true;
}
SYSINIT(rs_epoch_init, SI_SUB_EPOCH, SI_ORDER_ANY, rs_epoch_init, NULL);

/*
 * Run through all fast sources reading entropy for the given
 * number of rounds, which should be a multiple of the number
 * of entropy accumulation pools in use; it is 32 for Fortuna.
 */
static void
random_sources_feed(void)
{
	uint32_t entropy[HARVESTSIZE];
	struct epoch_tracker et;
	struct random_sources *rrs;
	u_int i, n, npools;
	bool rse_warm;

	rse_warm = epoch_inited;

	/*
	 * Evenly-ish distribute pool population across the second based on how
	 * frequently random_kthread iterates.
	 *
	 * For Fortuna, the math currently works out as such:
	 *
	 * 64 bits * 4 pools = 256 bits per iteration
	 * 256 bits * 10 Hz = 2560 bits per second, 320 B/s
	 *
	 */
	npools = howmany(p_random_alg_context->ra_poolcount, RANDOM_KTHREAD_HZ);

	/*-
	 * If we're not seeded yet, attempt to perform a "full seed", filling
	 * all of the PRNG's pools with entropy; if there is enough entropy
	 * available from "fast" entropy sources this will allow us to finish
	 * seeding and unblock the boot process immediately rather than being
	 * stuck for a few seconds with random_kthread gradually collecting a
	 * small chunk of entropy every 1 / RANDOM_KTHREAD_HZ seconds.
	 *
	 * We collect RANDOM_FORTUNA_DEFPOOLSIZE bytes per pool, i.e. enough
	 * to fill Fortuna's pools in the default configuration.  With another
	 * PRNG or smaller pools for Fortuna, we might collect more entropy
	 * than needed to fill the pools, but this is harmless; alternatively,
	 * a different PRNG, larger pools, or fast entropy sources which are
	 * not able to provide as much entropy as we request may result in the
	 * not being fully seeded (and thus remaining blocked) but in that
	 * case we will return here after 1 / RANDOM_KTHREAD_HZ seconds and
	 * try again for a large amount of entropy.
	 */
	if (!p_random_alg_context->ra_seeded())
		npools = howmany(p_random_alg_context->ra_poolcount *
		    RANDOM_FORTUNA_DEFPOOLSIZE, sizeof(entropy));

	/*
	 * Step over all of live entropy sources, and feed their output
	 * to the system-wide RNG.
	 */
	if (rse_warm)
		epoch_enter_preempt(rs_epoch, &et);
	CK_LIST_FOREACH(rrs, &source_list, rrs_entries) {
		for (i = 0; i < npools; i++) {
			if (rrs->rrs_source->rs_read == NULL) {
				/* Source pushes entropy asynchronously. */
				continue;
			}
			n = rrs->rrs_source->rs_read(entropy, sizeof(entropy));
			KASSERT((n <= sizeof(entropy)),
			    ("%s: rs_read returned too much data (%u > %zu)",
			    __func__, n, sizeof(entropy)));

			/*
			 * Sometimes the HW entropy source doesn't have anything
			 * ready for us.  This isn't necessarily untrustworthy.
			 * We don't perform any other verification of an entropy
			 * source (i.e., length is allowed to be anywhere from 1
			 * to sizeof(entropy), quality is unchecked, etc), so
			 * don't balk verbosely at slow random sources either.
			 * There are reports that RDSEED on x86 metal falls
			 * behind the rate at which we query it, for example.
			 * But it's still a better entropy source than RDRAND.
			 */
			if (n == 0)
				continue;
			random_harvest_direct(entropy, n, rrs->rrs_source->rs_source);
		}
	}
	if (rse_warm)
		epoch_exit_preempt(rs_epoch, &et);
	explicit_bzero(entropy, sizeof(entropy));
}

/*
 * State used for conducting NIST SP 800-90B health tests on entropy sources.
 */
static struct health_test_softc {
	uint32_t ht_rct_value[HARVESTSIZE + 1];
	u_int ht_rct_count;	/* number of samples with the same value */
	u_int ht_rct_limit;	/* constant after init */

	uint32_t ht_apt_value[HARVESTSIZE + 1];
	u_int ht_apt_count;	/* number of samples with the same value */
	u_int ht_apt_seq;	/* sequence number of the last sample */
	u_int ht_apt_cutoff;	/* constant after init */

	uint64_t ht_total_samples;
	bool ondemand;		/* Set to true to restart the state machine */
	enum {
		INIT = 0,	/* initial state */
		DISABLED,	/* health checking is disabled */
		STARTUP,	/* doing startup tests, samples are discarded */
		STEADY,		/* steady-state operation */
		FAILED,		/* health check failed, discard samples */
	} ht_state;
} healthtest[ENTROPYSOURCE];

#define	RANDOM_SELFTEST_STARTUP_SAMPLES	1024	/* 4.3, requirement 4 */
#define	RANDOM_SELFTEST_APT_WINDOW	512	/* 4.4.2 */

static void
copy_event(uint32_t dst[static HARVESTSIZE + 1],
    const struct harvest_event *event)
{
	memset(dst, 0, sizeof(uint32_t) * (HARVESTSIZE + 1));
	memcpy(dst, event->he_entropy, event->he_size);
	if (event->he_source <= RANDOM_ENVIRONMENTAL_END) {
		/*
		 * For pure entropy sources the timestamp counter is generally
		 * quite determinstic since samples are taken at regular
		 * intervals, so does not contribute much to the entropy.  To
		 * make health tests more effective, exclude it from the sample,
		 * since it might otherwise defeat the health tests in a
		 * scenario where the source is stuck.
		 */
		dst[HARVESTSIZE] = event->he_somecounter;
	}
}

static void
random_healthtest_rct_init(struct health_test_softc *ht,
    const struct harvest_event *event)
{
	ht->ht_rct_count = 1;
	copy_event(ht->ht_rct_value, event);
}

/*
 * Apply the repitition count test to a sample.
 *
 * Return false if the test failed, i.e., we observed >= C consecutive samples
 * with the same value, and true otherwise.
 */
static bool
random_healthtest_rct_next(struct health_test_softc *ht,
    const struct harvest_event *event)
{
	uint32_t val[HARVESTSIZE + 1];

	copy_event(val, event);
	if (memcmp(val, ht->ht_rct_value, sizeof(ht->ht_rct_value)) != 0) {
		ht->ht_rct_count = 1;
		memcpy(ht->ht_rct_value, val, sizeof(ht->ht_rct_value));
		return (true);
	} else {
		ht->ht_rct_count++;
		return (ht->ht_rct_count < ht->ht_rct_limit);
	}
}

static void
random_healthtest_apt_init(struct health_test_softc *ht,
    const struct harvest_event *event)
{
	ht->ht_apt_count = 1;
	ht->ht_apt_seq = 1;
	copy_event(ht->ht_apt_value, event);
}

static bool
random_healthtest_apt_next(struct health_test_softc *ht,
    const struct harvest_event *event)
{
	uint32_t val[HARVESTSIZE + 1];

	if (ht->ht_apt_seq == 0) {
		random_healthtest_apt_init(ht, event);
		return (true);
	}

	copy_event(val, event);
	if (memcmp(val, ht->ht_apt_value, sizeof(ht->ht_apt_value)) == 0) {
		ht->ht_apt_count++;
		if (ht->ht_apt_count >= ht->ht_apt_cutoff)
			return (false);
	}

	ht->ht_apt_seq++;
	if (ht->ht_apt_seq == RANDOM_SELFTEST_APT_WINDOW)
		ht->ht_apt_seq = 0;

	return (true);
}

/*
 * Run the health tests for the given event.  This is assumed to be called from
 * a serialized context.
 */
bool
random_harvest_healthtest(const struct harvest_event *event)
{
	struct health_test_softc *ht;

	ht = &healthtest[event->he_source];

	/*
	 * Was on-demand testing requested?  Restart the state machine if so,
	 * restarting the startup tests.
	 */
	if (atomic_load_bool(&ht->ondemand)) {
		atomic_store_bool(&ht->ondemand, false);
		ht->ht_state = INIT;
	}

	switch (ht->ht_state) {
	case __predict_false(INIT):
		/* Store the first sample and initialize test state. */
		random_healthtest_rct_init(ht, event);
		random_healthtest_apt_init(ht, event);
		ht->ht_total_samples = 0;
		ht->ht_state = STARTUP;
		return (false);
	case DISABLED:
		/* No health testing for this source. */
		return (true);
	case STEADY:
	case STARTUP:
		ht->ht_total_samples++;
		if (random_healthtest_rct_next(ht, event) &&
		    random_healthtest_apt_next(ht, event)) {
			if (ht->ht_state == STARTUP &&
			    ht->ht_total_samples >=
			    RANDOM_SELFTEST_STARTUP_SAMPLES) {
				printf(
			    "random: health test passed for source %s\n",
				    random_source_descr[event->he_source]);
				ht->ht_state = STEADY;
			}
			return (ht->ht_state == STEADY);
		}
		ht->ht_state = FAILED;
		printf(
	    "random: health test failed for source %s, discarding samples\n",
		    random_source_descr[event->he_source]);
		/* FALLTHROUGH */
	case FAILED:
		return (false);
	}
}

static bool nist_healthtest_enabled = false;
SYSCTL_BOOL(_kern_random, OID_AUTO, nist_healthtest_enabled,
    CTLFLAG_RDTUN, &nist_healthtest_enabled, 0,
    "Enable NIST SP 800-90B health tests for noise sources");

static void
random_healthtest_init(enum random_entropy_source source, int min_entropy)
{
	struct health_test_softc *ht;

	ht = &healthtest[source];
	memset(ht, 0, sizeof(*ht));
	KASSERT(ht->ht_state == INIT,
	    ("%s: health test state is %d for source %d",
	    __func__, ht->ht_state, source));

	/*
	 * If health-testing is enabled, validate all sources except CACHED and
	 * VMGENID: they are deterministic sources used only a small, fixed
	 * number of times, so statistical testing is not applicable.
	 */
	if (!nist_healthtest_enabled ||
	    source == RANDOM_CACHED || source == RANDOM_PURE_VMGENID) {
		ht->ht_state = DISABLED;
		return;
	}

	/*
	 * Set cutoff values for the two tests, given a min-entropy estimate for
	 * the source and allowing for an error rate of 1 in 2^{34}.  With a
	 * min-entropy estimate of 1 bit and a sample rate of RANDOM_KTHREAD_HZ,
	 * we expect to see an false positive once in ~54.5 years.
	 *
	 * The RCT limit comes from the formula in section 4.4.1.
	 *
	 * The APT cutoffs are calculated using the formula in section 4.4.2
	 * footnote 10 with the number of Bernoulli trials changed from W to
	 * W-1, since the test as written counts the number of samples equal to
	 * the first sample in the window, and thus tests W-1 samples.  We
	 * provide cutoffs for estimates up to sizeof(uint32_t)*HARVESTSIZE*8
	 * bits.
	 */
	const int apt_cutoffs[] = {
		[1] = 329,
		[2] = 195,
		[3] = 118,
		[4] = 73,
		[5] = 48,
		[6] = 33,
		[7] = 23,
		[8] = 17,
		[9] = 13,
		[10] = 11,
		[11] = 9,
		[12] = 8,
		[13] = 7,
		[14] = 6,
		[15] = 5,
		[16] = 5,
		[17 ... 19] = 4,
		[20 ... 25] = 3,
		[26 ... 42] = 2,
		[43 ... 64] = 1,
	};
	const int error_rate = 34;

	if (min_entropy == 0) {
		/*
		 * For environmental sources, the main source of entropy is the
		 * associated timecounter value.  Since these sources can be
		 * influenced by unprivileged users, we conservatively use a
		 * min-entropy estimate of 1 bit per sample.  For "pure"
		 * sources, we assume 8 bits per sample, as such sources provide
		 * a variable amount of data per read and in particular might
		 * only provide a single byte at a time.
		 */
		min_entropy = source >= RANDOM_PURE_START ? 8 : 1;
	} else if (min_entropy < 0 || min_entropy >= nitems(apt_cutoffs)) {
		panic("invalid min_entropy %d for %s", min_entropy,
		    random_source_descr[source]);
	}

	ht->ht_rct_limit = 1 + howmany(error_rate, min_entropy);
	ht->ht_apt_cutoff = apt_cutoffs[min_entropy];
}

static int
random_healthtest_ondemand(SYSCTL_HANDLER_ARGS)
{
	u_int mask, source;
	int error;

	mask = 0;
	error = sysctl_handle_int(oidp, &mask, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	while (mask != 0) {
		source = ffs(mask) - 1;
		if (source < nitems(healthtest))
			atomic_store_bool(&healthtest[source].ondemand, true);
		mask &= ~(1u << source);
	}
	return (0);
}
SYSCTL_PROC(_kern_random, OID_AUTO, nist_healthtest_ondemand,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0,
    random_healthtest_ondemand, "I",
    "Re-run NIST SP 800-90B startup health tests for a noise source");

static int
random_check_uint_harvestmask(SYSCTL_HANDLER_ARGS)
{
	static const u_int user_immutable_mask =
	    (((1 << ENTROPYSOURCE) - 1) & (-1UL << RANDOM_PURE_START)) |
	    _RANDOM_HARVEST_ETHER_OFF | _RANDOM_HARVEST_UMA_OFF;

	int error;
	u_int value;

	value = atomic_load_int(&hc_source_mask);
	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (flsl(value) > ENTROPYSOURCE)
		return (EINVAL);

	/*
	 * Disallow userspace modification of pure entropy sources.
	 */
	RANDOM_HARVEST_LOCK();
	hc_source_mask = (value & ~user_immutable_mask) |
	    (hc_source_mask & user_immutable_mask);
	RANDOM_HARVEST_UNLOCK();
	return (0);
}
SYSCTL_PROC(_kern_random_harvest, OID_AUTO, mask,
    CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0,
    random_check_uint_harvestmask, "IU",
    "Entropy harvesting mask");

static int
random_print_harvestmask(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sbuf;
	int error, i;

	error = sysctl_wire_old_buffer(req, 0);
	if (error == 0) {
		u_int mask;

		sbuf_new_for_sysctl(&sbuf, NULL, 128, req);
		mask = atomic_load_int(&hc_source_mask);
		for (i = ENTROPYSOURCE - 1; i >= 0; i--) {
			bool present;

			present = (mask & (1u << i)) != 0;
			sbuf_cat(&sbuf, present ? "1" : "0");
		}
		error = sbuf_finish(&sbuf);
		sbuf_delete(&sbuf);
	}
	return (error);
}
SYSCTL_PROC(_kern_random_harvest, OID_AUTO, mask_bin,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    random_print_harvestmask, "A",
    "Entropy harvesting mask (printable)");

static const char *random_source_descr[/*ENTROPYSOURCE*/] = {
	[RANDOM_CACHED] = "CACHED",
	[RANDOM_ATTACH] = "ATTACH",
	[RANDOM_KEYBOARD] = "KEYBOARD",
	[RANDOM_MOUSE] = "MOUSE",
	[RANDOM_NET_TUN] = "NET_TUN",
	[RANDOM_NET_ETHER] = "NET_ETHER",
	[RANDOM_NET_NG] = "NET_NG",
	[RANDOM_INTERRUPT] = "INTERRUPT",
	[RANDOM_SWI] = "SWI",
	[RANDOM_FS_ATIME] = "FS_ATIME",
	[RANDOM_UMA] = "UMA",
	[RANDOM_CALLOUT] = "CALLOUT",
	[RANDOM_RANDOMDEV] = "RANDOMDEV", /* ENVIRONMENTAL_END */
	[RANDOM_PURE_TPM] = "PURE_TPM", /* PURE_START */
	[RANDOM_PURE_RDRAND] = "PURE_RDRAND",
	[RANDOM_PURE_RDSEED] = "PURE_RDSEED",
	[RANDOM_PURE_NEHEMIAH] = "PURE_NEHEMIAH",
	[RANDOM_PURE_RNDTEST] = "PURE_RNDTEST",
	[RANDOM_PURE_VIRTIO] = "PURE_VIRTIO",
	[RANDOM_PURE_BROADCOM] = "PURE_BROADCOM",
	[RANDOM_PURE_CCP] = "PURE_CCP",
	[RANDOM_PURE_DARN] = "PURE_DARN",
	[RANDOM_PURE_VMGENID] = "PURE_VMGENID",
	[RANDOM_PURE_QUALCOMM] = "PURE_QUALCOMM",
	[RANDOM_PURE_ARMV8] = "PURE_ARMV8",
	[RANDOM_PURE_ARM_TRNG] = "PURE_ARM_TRNG",
	[RANDOM_PURE_SAFE] = "PURE_SAFE",
	[RANDOM_PURE_GLXSB] = "PURE_GLXSB",
	[RANDOM_PURE_HIFN] = "PURE_HIFN",
	/* "ENTROPYSOURCE" */
};
CTASSERT(nitems(random_source_descr) == ENTROPYSOURCE);

static int
random_print_harvestmask_symbolic(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sbuf;
	int error, i;
	bool first;

	first = true;
	error = sysctl_wire_old_buffer(req, 0);
	if (error == 0) {
		u_int mask;

		sbuf_new_for_sysctl(&sbuf, NULL, 128, req);
		mask = atomic_load_int(&hc_source_mask);
		for (i = ENTROPYSOURCE - 1; i >= 0; i--) {
			bool present;

			present = (mask & (1u << i)) != 0;
			if (i >= RANDOM_PURE_START && !present)
				continue;
			if (!first)
				sbuf_cat(&sbuf, ",");
			sbuf_cat(&sbuf, !present ? "[" : "");
			sbuf_cat(&sbuf, random_source_descr[i]);
			sbuf_cat(&sbuf, !present ? "]" : "");
			first = false;
		}
		error = sbuf_finish(&sbuf);
		sbuf_delete(&sbuf);
	}
	return (error);
}
SYSCTL_PROC(_kern_random_harvest, OID_AUTO, mask_symbolic,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    random_print_harvestmask_symbolic, "A",
    "Entropy harvesting mask (symbolic)");

static void
random_harvestq_init(void *unused __unused)
{
	static const u_int almost_everything_mask =
	    (((1 << (RANDOM_ENVIRONMENTAL_END + 1)) - 1) &
	    ~_RANDOM_HARVEST_ETHER_OFF & ~_RANDOM_HARVEST_UMA_OFF);

	hc_source_mask = almost_everything_mask;
	RANDOM_HARVEST_INIT_LOCK();
	harvest_context.hc_active_buf = 0;

	for (int i = RANDOM_START; i <= RANDOM_ENVIRONMENTAL_END; i++)
		random_healthtest_init(i, 0);
}
SYSINIT(random_device_h_init, SI_SUB_RANDOM, SI_ORDER_THIRD, random_harvestq_init, NULL);

/*
 * Subroutine to slice up a contiguous chunk of 'entropy' and feed it into the
 * underlying algorithm.  Returns number of bytes actually fed into underlying
 * algorithm.
 */
static size_t
random_early_prime(char *entropy, size_t len)
{
	struct harvest_event event;
	size_t i;

	len = rounddown(len, sizeof(event.he_entropy));
	if (len == 0)
		return (0);

	for (i = 0; i < len; i += sizeof(event.he_entropy)) {
		event.he_somecounter = random_get_cyclecount();
		event.he_size = sizeof(event.he_entropy);
		event.he_source = RANDOM_CACHED;
		event.he_destination =
		    harvest_context.hc_destination[RANDOM_CACHED]++;
		memcpy(event.he_entropy, entropy + i, sizeof(event.he_entropy));
		random_harvestq_fast_process_event(&event);
	}
	explicit_bzero(entropy, len);
	return (len);
}

/*
 * Subroutine to search for known loader-loaded files in memory and feed them
 * into the underlying algorithm early in boot.  Returns the number of bytes
 * loaded (zero if none were loaded).
 */
static size_t
random_prime_loader_file(const char *type)
{
	uint8_t *keyfile, *data;
	size_t size;

	keyfile = preload_search_by_type(type);
	if (keyfile == NULL)
		return (0);

	data = preload_fetch_addr(keyfile);
	size = preload_fetch_size(keyfile);
	if (data == NULL)
		return (0);

	return (random_early_prime(data, size));
}

/*
 * This is used to prime the RNG by grabbing any early random stuff
 * known to the kernel, and inserting it directly into the hashing
 * module, currently Fortuna.
 */
static void
random_harvestq_prime(void *unused __unused)
{
	size_t size;

	/*
	 * Get entropy that may have been preloaded by loader(8)
	 * and use it to pre-charge the entropy harvest queue.
	 */
	size = random_prime_loader_file(RANDOM_CACHED_BOOT_ENTROPY_MODULE);
	if (bootverbose) {
		if (size > 0)
			printf("random: read %zu bytes from preloaded cache\n",
			    size);
		else
			printf("random: no preloaded entropy cache\n");
	}
	size = random_prime_loader_file(RANDOM_PLATFORM_BOOT_ENTROPY_MODULE);
	if (bootverbose) {
		if (size > 0)
			printf("random: read %zu bytes from platform bootloader\n",
			    size);
		else
			printf("random: no platform bootloader entropy\n");
	}
}
SYSINIT(random_device_prime, SI_SUB_RANDOM, SI_ORDER_MIDDLE, random_harvestq_prime, NULL);

static void
random_harvestq_deinit(void *unused __unused)
{

	/* Command the hash/reseed thread to end and wait for it to finish */
	random_kthread_control = 0;
	while (random_kthread_control >= 0)
		tsleep(&harvest_context.hc_kthread_proc, 0, "harvqterm", hz/5);
}
SYSUNINIT(random_device_h_init, SI_SUB_RANDOM, SI_ORDER_THIRD, random_harvestq_deinit, NULL);

/*-
 * Entropy harvesting queue routine.
 *
 * This is supposed to be fast; do not do anything slow in here!
 * It is also illegal (and morally reprehensible) to insert any
 * high-rate data here. "High-rate" is defined as a data source
 * that is likely to fill up the buffer in much less than 100ms.
 * This includes the "always-on" sources like the Intel "rdrand"
 * or the VIA Nehamiah "xstore" sources.
 */
/* XXXRW: get_cyclecount() is cheap on most modern hardware, where cycle
 * counters are built in, but on older hardware it will do a real time clock
 * read which can be quite expensive.
 */
void
random_harvest_queue_(const void *entropy, u_int size, enum random_entropy_source origin)
{
	struct harvest_context *hc;
	struct entropy_buffer *buf;
	struct harvest_event *event;

	KASSERT(origin >= RANDOM_START && origin < ENTROPYSOURCE,
	    ("%s: origin %d invalid", __func__, origin));

	hc = &harvest_context;
	RANDOM_HARVEST_LOCK();
	buf = &hc->hc_entropy_buf[hc->hc_active_buf];
	if (buf->pos < RANDOM_RING_MAX) {
		event = &buf->ring[buf->pos++];
		event->he_somecounter = random_get_cyclecount();
		event->he_source = origin;
		event->he_destination = hc->hc_destination[origin]++;
		if (size <= sizeof(event->he_entropy)) {
			event->he_size = size;
			memcpy(event->he_entropy, entropy, size);
		} else {
			/* Big event, so squash it */
			event->he_size = sizeof(event->he_entropy[0]);
			event->he_entropy[0] = jenkins_hash(entropy, size, (uint32_t)(uintptr_t)event);
		}
	}
	RANDOM_HARVEST_UNLOCK();
}

/*-
 * Entropy harvesting fast routine.
 *
 * This is supposed to be very fast; do not do anything slow in here!
 * This is the right place for high-rate harvested data.
 */
void
random_harvest_fast_(const void *entropy, u_int size)
{
	u_int pos;

	pos = harvest_context.hc_entropy_fast_accumulator.pos;
	harvest_context.hc_entropy_fast_accumulator.buf[pos] ^=
	    jenkins_hash(entropy, size, random_get_cyclecount());
	harvest_context.hc_entropy_fast_accumulator.pos = (pos + 1)%RANDOM_ACCUM_MAX;
}

/*-
 * Entropy harvesting direct routine.
 *
 * This is not supposed to be fast, but will only be used during
 * (e.g.) booting when initial entropy is being gathered.
 */
void
random_harvest_direct_(const void *entropy, u_int size, enum random_entropy_source origin)
{
	struct harvest_event event;

	KASSERT(origin >= RANDOM_START && origin < ENTROPYSOURCE, ("%s: origin %d invalid\n", __func__, origin));
	size = MIN(size, sizeof(event.he_entropy));
	event.he_somecounter = random_get_cyclecount();
	event.he_size = size;
	event.he_source = origin;
	event.he_destination = harvest_context.hc_destination[origin]++;
	memcpy(event.he_entropy, entropy, size);
	random_harvestq_fast_process_event(&event);
}

void
random_source_register(const struct random_source *rsource)
{
	struct random_sources *rrs;

	KASSERT(rsource != NULL, ("invalid input to %s", __func__));

	rrs = malloc(sizeof(*rrs), M_ENTROPY, M_WAITOK);
	rrs->rrs_source = rsource;

	printf("random: registering fast source %s\n", rsource->rs_ident);

	random_healthtest_init(rsource->rs_source, rsource->rs_min_entropy);

	RANDOM_HARVEST_LOCK();
	hc_source_mask |= (1 << rsource->rs_source);
	CK_LIST_INSERT_HEAD(&source_list, rrs, rrs_entries);
	RANDOM_HARVEST_UNLOCK();
}

void
random_source_deregister(const struct random_source *rsource)
{
	struct random_sources *rrs = NULL;

	KASSERT(rsource != NULL, ("invalid input to %s", __func__));

	RANDOM_HARVEST_LOCK();
	hc_source_mask &= ~(1 << rsource->rs_source);
	CK_LIST_FOREACH(rrs, &source_list, rrs_entries)
		if (rrs->rrs_source == rsource) {
			CK_LIST_REMOVE(rrs, rrs_entries);
			break;
		}
	RANDOM_HARVEST_UNLOCK();

	if (rrs != NULL && epoch_inited)
		epoch_wait_preempt(rs_epoch);
	free(rrs, M_ENTROPY);
}

static int
random_source_handler(SYSCTL_HANDLER_ARGS)
{
	struct epoch_tracker et;
	struct random_sources *rrs;
	struct sbuf sbuf;
	int error, count;

	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);

	sbuf_new_for_sysctl(&sbuf, NULL, 64, req);
	count = 0;
	epoch_enter_preempt(rs_epoch, &et);
	CK_LIST_FOREACH(rrs, &source_list, rrs_entries) {
		sbuf_cat(&sbuf, (count++ ? ",'" : "'"));
		sbuf_cat(&sbuf, rrs->rrs_source->rs_ident);
		sbuf_cat(&sbuf, "'");
	}
	epoch_exit_preempt(rs_epoch, &et);
	error = sbuf_finish(&sbuf);
	sbuf_delete(&sbuf);
	return (error);
}
SYSCTL_PROC(_kern_random, OID_AUTO, random_sources, CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    NULL, 0, random_source_handler, "A",
	    "List of active fast entropy sources.");

MODULE_VERSION(random_harvestq, 1);
