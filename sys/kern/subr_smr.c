/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019,2020 Jeffrey Roberson <jeff@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/smr.h>
#include <sys/sysctl.h>

#include <vm/uma.h>

/*
 * This is a novel safe memory reclamation technique inspired by
 * epoch based reclamation from Samy Al Bahra's concurrency kit which
 * in turn was based on work described in:
 *   Fraser, K. 2004. Practical Lock-Freedom. PhD Thesis, University
 *   of Cambridge Computing Laboratory.
 * And shares some similarities with:
 *   Wang, Stamler, Parmer. 2016 Parallel Sections: Scaling System-Level
 *   Data-Structures
 *
 * This is not an implementation of hazard pointers or related
 * techniques.  The term safe memory reclamation is used as a
 * generic descriptor for algorithms that defer frees to avoid
 * use-after-free errors with lockless datastructures.
 *
 * The basic approach is to maintain a monotonic write sequence
 * number that is updated on some application defined granularity.
 * Readers record the most recent write sequence number they have
 * observed.  A shared read sequence number records the lowest
 * sequence number observed by any reader as of the last poll.  Any
 * write older than this value has been observed by all readers
 * and memory can be reclaimed.  Like Epoch we also detect idle
 * readers by storing an invalid sequence number in the per-cpu
 * state when the read section exits.  Like Parsec we establish
 * a global write clock that is used to mark memory on free.
 *
 * The write and read sequence numbers can be thought of as a two
 * handed clock with readers always advancing towards writers.  SMR
 * maintains the invariant that all readers can safely access memory
 * that was visible at the time they loaded their copy of the sequence
 * number.  Periodically the read sequence or hand is polled and
 * advanced as far towards the write sequence as active readers allow.
 * Memory which was freed between the old and new global read sequence
 * number can now be reclaimed.  When the system is idle the two hands
 * meet and no deferred memory is outstanding.  Readers never advance
 * any sequence number, they only observe them.  The shared read
 * sequence number is consequently never higher than the write sequence.
 * A stored sequence number that falls outside of this range has expired
 * and needs no scan to reclaim.
 *
 * A notable distinction between this SMR and Epoch, qsbr, rcu, etc. is
 * that advancing the sequence number is decoupled from detecting its
 * observation.  This results in a more granular assignment of sequence
 * numbers even as read latencies prohibit all or some expiration.
 * It also allows writers to advance the sequence number and save the
 * poll for expiration until a later time when it is likely to
 * complete without waiting.  The batch granularity and free-to-use
 * latency is dynamic and can be significantly smaller than in more
 * strict systems.
 *
 * This mechanism is primarily intended to be used in coordination with
 * UMA.  By integrating with the allocator we avoid all of the callout
 * queue machinery and are provided with an efficient way to batch
 * sequence advancement and waiting.  The allocator accumulates a full
 * per-cpu cache of memory before advancing the sequence.  It then
 * delays waiting for this sequence to expire until the memory is
 * selected for reuse.  In this way we only increment the sequence
 * value once for n=cache-size frees and the waits are done long
 * after the sequence has been expired so they need only be verified
 * to account for pathological conditions and to advance the read
 * sequence.  Tying the sequence number to the bucket size has the
 * nice property that as the zone gets busier the buckets get larger
 * and the sequence writes become fewer.  If the coherency of advancing
 * the write sequence number becomes too costly we can advance
 * it for every N buckets in exchange for higher free-to-use
 * latency and consequently higher memory consumption.
 *
 * If the read overhead of accessing the shared cacheline becomes
 * especially burdensome an invariant TSC could be used in place of the
 * sequence.  The algorithm would then only need to maintain the minimum
 * observed tsc.  This would trade potential cache synchronization
 * overhead for local serialization and cpu timestamp overhead.
 */

/*
 * A simplified diagram:
 *
 * 0                                                          UINT_MAX
 * | -------------------- sequence number space -------------------- |
 *              ^ rd seq                            ^ wr seq
 *              | ----- valid sequence numbers ---- |
 *                ^cpuA  ^cpuC
 * | -- free -- | --------- deferred frees -------- | ---- free ---- |
 *
 * 
 * In this example cpuA has the lowest sequence number and poll can
 * advance rd seq.  cpuB is not running and is considered to observe
 * wr seq.
 *
 * Freed memory that is tagged with a sequence number between rd seq and
 * wr seq can not be safely reclaimed because cpuA may hold a reference to
 * it.  Any other memory is guaranteed to be unreferenced.
 *
 * Any writer is free to advance wr seq at any time however it may busy
 * poll in pathological cases.
 */

static uma_zone_t smr_shared_zone;
static uma_zone_t smr_zone;

#ifndef INVARIANTS
#define	SMR_SEQ_INIT	1		/* All valid sequence numbers are odd. */
#define	SMR_SEQ_INCR	2

/*
 * SMR_SEQ_MAX_DELTA is the maximum distance allowed between rd_seq and
 * wr_seq.  For the modular arithmetic to work a value of UNIT_MAX / 2
 * would be possible but it is checked after we increment the wr_seq so
 * a safety margin is left to prevent overflow.
 *
 * We will block until SMR_SEQ_MAX_ADVANCE sequence numbers have progressed
 * to prevent integer wrapping.  See smr_advance() for more details.
 */
#define	SMR_SEQ_MAX_DELTA	(UINT_MAX / 4)
#define	SMR_SEQ_MAX_ADVANCE	(SMR_SEQ_MAX_DELTA - 1024)
#else
/* We want to test the wrapping feature in invariants kernels. */
#define	SMR_SEQ_INCR	(UINT_MAX / 10000)
#define	SMR_SEQ_INIT	(UINT_MAX - 100000)
/* Force extra polls to test the integer overflow detection. */
#define	SMR_SEQ_MAX_DELTA	(SMR_SEQ_INCR * 32)
#define	SMR_SEQ_MAX_ADVANCE	SMR_SEQ_MAX_DELTA / 2
#endif

static SYSCTL_NODE(_debug, OID_AUTO, smr, CTLFLAG_RW, NULL, "SMR Stats");
static counter_u64_t advance = EARLY_COUNTER;
SYSCTL_COUNTER_U64(_debug_smr, OID_AUTO, advance, CTLFLAG_RD, &advance, "");
static counter_u64_t advance_wait = EARLY_COUNTER;
SYSCTL_COUNTER_U64(_debug_smr, OID_AUTO, advance_wait, CTLFLAG_RD, &advance_wait, "");
static counter_u64_t poll = EARLY_COUNTER;
SYSCTL_COUNTER_U64(_debug_smr, OID_AUTO, poll, CTLFLAG_RD, &poll, "");
static counter_u64_t poll_scan = EARLY_COUNTER;
SYSCTL_COUNTER_U64(_debug_smr, OID_AUTO, poll_scan, CTLFLAG_RD, &poll_scan, "");


/*
 * Advance the write sequence and return the new value for use as the
 * wait goal.  This guarantees that any changes made by the calling
 * thread prior to this call will be visible to all threads after
 * rd_seq meets or exceeds the return value.
 *
 * This function may busy loop if the readers are roughly 1 billion
 * sequence numbers behind the writers.
 */
smr_seq_t
smr_advance(smr_t smr)
{
	smr_shared_t s;
	smr_seq_t goal, s_rd_seq;

	/*
	 * It is illegal to enter while in an smr section.
	 */
	KASSERT(curthread->td_critnest == 0,
	    ("smr_advance: Not allowed in a critical section."));

	/*
	 * Modifications not done in a smr section need to be visible
	 * before advancing the seq.
	 */
	atomic_thread_fence_rel();

	/*
	 * Load the current read seq before incrementing the goal so
	 * we are guaranteed it is always < goal.
	 */
	s = zpcpu_get(smr)->c_shared;
	s_rd_seq = atomic_load_acq_int(&s->s_rd_seq);

	/*
	 * Increment the shared write sequence by 2.  Since it is
	 * initialized to 1 this means the only valid values are
	 * odd and an observed value of 0 in a particular CPU means
	 * it is not currently in a read section.
	 */
	goal = atomic_fetchadd_int(&s->s_wr_seq, SMR_SEQ_INCR) + SMR_SEQ_INCR;
	counter_u64_add(advance, 1);

	/*
	 * Force a synchronization here if the goal is getting too
	 * far ahead of the read sequence number.  This keeps the
	 * wrap detecting arithmetic working in pathological cases.
	 */
	if (SMR_SEQ_DELTA(goal, s_rd_seq) >= SMR_SEQ_MAX_DELTA) {
		counter_u64_add(advance_wait, 1);
		smr_wait(smr, goal - SMR_SEQ_MAX_ADVANCE);
	}

	return (goal);
}

smr_seq_t
smr_advance_deferred(smr_t smr, int limit)
{
	smr_seq_t goal;
	smr_t csmr;

	critical_enter();
	csmr = zpcpu_get(smr);
	if (++csmr->c_deferred >= limit) {
		goal = SMR_SEQ_INVALID;
		csmr->c_deferred = 0;
	} else
		goal = smr_shared_current(csmr->c_shared) + SMR_SEQ_INCR;
	critical_exit();
	if (goal != SMR_SEQ_INVALID)
		return (goal);

	return (smr_advance(smr));
}

/*
 * Poll to determine whether all readers have observed the 'goal' write
 * sequence number.
 *
 * If wait is true this will spin until the goal is met.
 *
 * This routine will updated the minimum observed read sequence number in
 * s_rd_seq if it does a scan.  It may not do a scan if another call has
 * advanced s_rd_seq beyond the callers goal already.
 *
 * Returns true if the goal is met and false if not.
 */
bool
smr_poll(smr_t smr, smr_seq_t goal, bool wait)
{
	smr_shared_t s;
	smr_t c;
	smr_seq_t s_wr_seq, s_rd_seq, rd_seq, c_seq;
	int i;
	bool success;

	/*
	 * It is illegal to enter while in an smr section.
	 */
	KASSERT(!wait || curthread->td_critnest == 0,
	    ("smr_poll: Blocking not allowed in a critical section."));

	/*
	 * Use a critical section so that we can avoid ABA races
	 * caused by long preemption sleeps.
	 */
	success = true;
	critical_enter();
	s = zpcpu_get(smr)->c_shared;
	counter_u64_add_protected(poll, 1);

	/*
	 * Acquire barrier loads s_wr_seq after s_rd_seq so that we can not
	 * observe an updated read sequence that is larger than write.
	 */
	s_rd_seq = atomic_load_acq_int(&s->s_rd_seq);

	/*
	 * wr_seq must be loaded prior to any c_seq value so that a stale
	 * c_seq can only reference time after this wr_seq.
	 */
	s_wr_seq = atomic_load_acq_int(&s->s_wr_seq);

	/*
	 * This may have come from a deferred advance.  Consider one
	 * increment past the current wr_seq valid and make sure we
	 * have advanced far enough to succeed.  We simply add to avoid
	 * an additional fence.
	 */
	if (goal == s_wr_seq + SMR_SEQ_INCR) {
		atomic_add_int(&s->s_wr_seq, SMR_SEQ_INCR);
		s_wr_seq = goal;
	}

	/*
	 * Detect whether the goal is valid and has already been observed.
	 *
	 * The goal must be in the range of s_wr_seq >= goal >= s_rd_seq for
	 * it to be valid.  If it is not then the caller held on to it and
	 * the integer wrapped.  If we wrapped back within range the caller
	 * will harmlessly scan.
	 *
	 * A valid goal must be greater than s_rd_seq or we have not verified
	 * that it has been observed and must fall through to polling.
	 */
	if (SMR_SEQ_GEQ(s_rd_seq, goal) || SMR_SEQ_LT(s_wr_seq, goal))
		goto out;

	/*
	 * Loop until all cores have observed the goal sequence or have
	 * gone inactive.  Keep track of the oldest sequence currently
	 * active as rd_seq.
	 */
	counter_u64_add_protected(poll_scan, 1);
	rd_seq = s_wr_seq;
	CPU_FOREACH(i) {
		c = zpcpu_get_cpu(smr, i);
		c_seq = SMR_SEQ_INVALID;
		for (;;) {
			c_seq = atomic_load_int(&c->c_seq);
			if (c_seq == SMR_SEQ_INVALID)
				break;

			/*
			 * There is a race described in smr.h:smr_enter that
			 * can lead to a stale seq value but not stale data
			 * access.  If we find a value out of range here we
			 * pin it to the current min to prevent it from
			 * advancing until that stale section has expired.
			 *
			 * The race is created when a cpu loads the s_wr_seq
			 * value in a local register and then another thread
			 * advances s_wr_seq and calls smr_poll() which will 
			 * oberve no value yet in c_seq and advance s_rd_seq
			 * up to s_wr_seq which is beyond the register
			 * cached value.  This is only likely to happen on
			 * hypervisor or with a system management interrupt.
			 */
			if (SMR_SEQ_LT(c_seq, s_rd_seq))
				c_seq = s_rd_seq;

			/*
			 * If the sequence number meets the goal we are
			 * done with this cpu.
			 */
			if (SMR_SEQ_GEQ(c_seq, goal))
				break;

			/*
			 * If we're not waiting we will still scan the rest
			 * of the cpus and update s_rd_seq before returning
			 * an error.
			 */
			if (!wait) {
				success = false;
				break;
			}
			cpu_spinwait();
		}

		/*
		 * Limit the minimum observed rd_seq whether we met the goal
		 * or not.
		 */
		if (c_seq != SMR_SEQ_INVALID && SMR_SEQ_GT(rd_seq, c_seq))
			rd_seq = c_seq;
	}

	/*
	 * Advance the rd_seq as long as we observed the most recent one.
	 */
	s_rd_seq = atomic_load_int(&s->s_rd_seq);
	do {
		if (SMR_SEQ_LEQ(rd_seq, s_rd_seq))
			goto out;
	} while (atomic_fcmpset_int(&s->s_rd_seq, &s_rd_seq, rd_seq) == 0);

out:
	critical_exit();

	/*
	 * Serialize with smr_advance()/smr_exit().  The caller is now free
	 * to modify memory as expected.
	 */
	atomic_thread_fence_acq();

	return (success);
}

smr_t
smr_create(const char *name)
{
	smr_t smr, c;
	smr_shared_t s;
	int i;

	s = uma_zalloc(smr_shared_zone, M_WAITOK);
	smr = uma_zalloc(smr_zone, M_WAITOK);

	s->s_name = name;
	s->s_rd_seq = s->s_wr_seq = SMR_SEQ_INIT;

	/* Initialize all CPUS, not just those running. */
	for (i = 0; i <= mp_maxid; i++) {
		c = zpcpu_get_cpu(smr, i);
		c->c_seq = SMR_SEQ_INVALID;
		c->c_shared = s;
	}
	atomic_thread_fence_seq_cst();

	return (smr);
}

void
smr_destroy(smr_t smr)
{

	smr_synchronize(smr);
	uma_zfree(smr_shared_zone, smr->c_shared);
	uma_zfree(smr_zone, smr);
}

/*
 * Initialize the UMA slab zone.
 */
void
smr_init(void)
{

	smr_shared_zone = uma_zcreate("SMR SHARED", sizeof(struct smr_shared),
	    NULL, NULL, NULL, NULL, (CACHE_LINE_SIZE * 2) - 1, 0);
	smr_zone = uma_zcreate("SMR CPU", sizeof(struct smr),
	    NULL, NULL, NULL, NULL, (CACHE_LINE_SIZE * 2) - 1, UMA_ZONE_PCPU);
}

static void
smr_init_counters(void *unused)
{

	advance = counter_u64_alloc(M_WAITOK);
	advance_wait = counter_u64_alloc(M_WAITOK);
	poll = counter_u64_alloc(M_WAITOK);
	poll_scan = counter_u64_alloc(M_WAITOK);
}
SYSINIT(smr_counters, SI_SUB_CPU, SI_ORDER_ANY, smr_init_counters, NULL);
