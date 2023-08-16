/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
 * Global Unbounded Sequences (GUS)
 *
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
 * use-after-free errors with lockless datastructures or as
 * a mechanism to detect quiescence for writer synchronization.
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
 * handed clock with readers always advancing towards writers.  GUS 
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
 * A notable distinction between GUS and Epoch, qsbr, rcu, etc. is
 * that advancing the sequence number is decoupled from detecting its
 * observation.  That is to say, the delta between read and write
 * sequence numbers is not bound.  This can be thought of as a more
 * generalized form of epoch which requires them at most one step
 * apart.  This results in a more granular assignment of sequence
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

/*
 * The grace period for lazy (tick based) SMR.
 *
 * Hardclock is responsible for advancing ticks on a single CPU while every
 * CPU receives a regular clock interrupt.  The clock interrupts are flushing
 * the store buffers and any speculative loads that may violate our invariants.
 * Because these interrupts are not synchronized we must wait one additional
 * tick in the future to be certain that all processors have had their state
 * synchronized by an interrupt.
 *
 * This assumes that the clock interrupt will only be delayed by other causes
 * that will flush the store buffer or prevent access to the section protected
 * data.  For example, an idle processor, or an system management interrupt,
 * or a vm exit.
 */
#define	SMR_LAZY_GRACE		2
#define	SMR_LAZY_INCR		(SMR_LAZY_GRACE * SMR_SEQ_INCR)

/*
 * The maximum sequence number ahead of wr_seq that may still be valid.  The
 * sequence may not be advanced on write for lazy or deferred SMRs.  In this
 * case poll needs to attempt to forward the sequence number if the goal is
 * within wr_seq + SMR_SEQ_ADVANCE.
 */
#define	SMR_SEQ_ADVANCE		SMR_LAZY_INCR

static SYSCTL_NODE(_debug, OID_AUTO, smr, CTLFLAG_RW | CTLFLAG_MPSAFE, NULL,
    "SMR Stats");
static COUNTER_U64_DEFINE_EARLY(advance);
SYSCTL_COUNTER_U64(_debug_smr, OID_AUTO, advance, CTLFLAG_RW, &advance, "");
static COUNTER_U64_DEFINE_EARLY(advance_wait);
SYSCTL_COUNTER_U64(_debug_smr, OID_AUTO, advance_wait, CTLFLAG_RW, &advance_wait, "");
static COUNTER_U64_DEFINE_EARLY(poll);
SYSCTL_COUNTER_U64(_debug_smr, OID_AUTO, poll, CTLFLAG_RW, &poll, "");
static COUNTER_U64_DEFINE_EARLY(poll_scan);
SYSCTL_COUNTER_U64(_debug_smr, OID_AUTO, poll_scan, CTLFLAG_RW, &poll_scan, "");
static COUNTER_U64_DEFINE_EARLY(poll_fail);
SYSCTL_COUNTER_U64(_debug_smr, OID_AUTO, poll_fail, CTLFLAG_RW, &poll_fail, "");

/*
 * Advance a lazy write sequence number.  These move forward at the rate of
 * ticks.  Grace is SMR_LAZY_INCR (2 ticks) in the future.
 *
 * This returns the goal write sequence number.
 */
static smr_seq_t
smr_lazy_advance(smr_t smr, smr_shared_t s)
{
	union s_wr s_wr, old;
	int t, d;

	CRITICAL_ASSERT(curthread);

	/*
	 * Load the stored ticks value before the current one.  This way the
	 * current value can only be the same or larger.
	 */
	old._pair = s_wr._pair = atomic_load_acq_64(&s->s_wr._pair);
	t = ticks;

	/*
	 * The most probable condition that the update already took place.
	 */
	d = t - s_wr.ticks;
	if (__predict_true(d == 0))
		goto out;
	/* Cap the rate of advancement and handle long idle periods. */
	if (d > SMR_LAZY_GRACE || d < 0)
		d = SMR_LAZY_GRACE;
	s_wr.ticks = t;
	s_wr.seq += d * SMR_SEQ_INCR;

	/*
	 * This can only fail if another thread races to call advance().
	 * Strong cmpset semantics mean we are guaranteed that the update
	 * happened.
	 */
	atomic_cmpset_64(&s->s_wr._pair, old._pair, s_wr._pair);
out:
	return (s_wr.seq + SMR_LAZY_INCR);
}

/*
 * Increment the shared write sequence by 2.  Since it is initialized
 * to 1 this means the only valid values are odd and an observed value
 * of 0 in a particular CPU means it is not currently in a read section.
 */
static smr_seq_t
smr_shared_advance(smr_shared_t s)
{

	return (atomic_fetchadd_int(&s->s_wr.seq, SMR_SEQ_INCR) + SMR_SEQ_INCR);
}

/*
 * Advance the write sequence number for a normal smr section.  If the
 * write sequence is too far behind the read sequence we have to poll
 * to advance rd_seq and prevent undetectable wraps.
 */
static smr_seq_t
smr_default_advance(smr_t smr, smr_shared_t s)
{
	smr_seq_t goal, s_rd_seq;

	CRITICAL_ASSERT(curthread);
	KASSERT((zpcpu_get(smr)->c_flags & SMR_LAZY) == 0,
	    ("smr_default_advance: called with lazy smr."));

	/*
	 * Load the current read seq before incrementing the goal so
	 * we are guaranteed it is always < goal.
	 */
	s_rd_seq = atomic_load_acq_int(&s->s_rd_seq);
	goal = smr_shared_advance(s);

	/*
	 * Force a synchronization here if the goal is getting too
	 * far ahead of the read sequence number.  This keeps the
	 * wrap detecting arithmetic working in pathological cases.
	 */
	if (SMR_SEQ_DELTA(goal, s_rd_seq) >= SMR_SEQ_MAX_DELTA) {
		counter_u64_add(advance_wait, 1);
		smr_wait(smr, goal - SMR_SEQ_MAX_ADVANCE);
	}
	counter_u64_add(advance, 1);

	return (goal);
}

/*
 * Deferred SMRs conditionally update s_wr_seq based on an
 * cpu local interval count.
 */
static smr_seq_t
smr_deferred_advance(smr_t smr, smr_shared_t s, smr_t self)
{

	if (++self->c_deferred < self->c_limit)
		return (smr_shared_current(s) + SMR_SEQ_INCR);
	self->c_deferred = 0;
	return (smr_default_advance(smr, s));
}

/*
 * Advance the write sequence and return the value for use as the
 * wait goal.  This guarantees that any changes made by the calling
 * thread prior to this call will be visible to all threads after
 * rd_seq meets or exceeds the return value.
 *
 * This function may busy loop if the readers are roughly 1 billion
 * sequence numbers behind the writers.
 *
 * Lazy SMRs will not busy loop and the wrap happens every 25 days
 * at 1khz and 60 hours at 10khz.  Readers can block for no longer
 * than half of this for SMR_SEQ_ macros to continue working.
 */
smr_seq_t
smr_advance(smr_t smr)
{
	smr_t self;
	smr_shared_t s;
	smr_seq_t goal;
	int flags;

	/*
	 * It is illegal to enter while in an smr section.
	 */
	SMR_ASSERT_NOT_ENTERED(smr);

	/*
	 * Modifications not done in a smr section need to be visible
	 * before advancing the seq.
	 */
	atomic_thread_fence_rel();

	critical_enter();
	/* Try to touch the line once. */
	self = zpcpu_get(smr);
	s = self->c_shared;
	flags = self->c_flags;
	goal = SMR_SEQ_INVALID;
	if ((flags & (SMR_LAZY | SMR_DEFERRED)) == 0)
		goal = smr_default_advance(smr, s);
	else if ((flags & SMR_LAZY) != 0)
		goal = smr_lazy_advance(smr, s);
	else if ((flags & SMR_DEFERRED) != 0)
		goal = smr_deferred_advance(smr, s, self);
	critical_exit();

	return (goal);
}

/*
 * Poll to determine the currently observed sequence number on a cpu
 * and spinwait if the 'wait' argument is true.
 */
static smr_seq_t
smr_poll_cpu(smr_t c, smr_seq_t s_rd_seq, smr_seq_t goal, bool wait)
{
	smr_seq_t c_seq;

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
		 * If the sequence number meets the goal we are done
		 * with this cpu.
		 */
		if (SMR_SEQ_LEQ(goal, c_seq))
			break;

		if (!wait)
			break;
		cpu_spinwait();
	}

	return (c_seq);
}

/*
 * Loop until all cores have observed the goal sequence or have
 * gone inactive.  Returns the oldest sequence currently active;
 *
 * This function assumes a snapshot of sequence values has
 * been obtained and validated by smr_poll().
 */
static smr_seq_t
smr_poll_scan(smr_t smr, smr_shared_t s, smr_seq_t s_rd_seq,
    smr_seq_t s_wr_seq, smr_seq_t goal, bool wait)
{
	smr_seq_t rd_seq, c_seq;
	int i;

	CRITICAL_ASSERT(curthread);
	counter_u64_add_protected(poll_scan, 1);

	/*
	 * The read sequence can be no larger than the write sequence at
	 * the start of the poll.
	 */
	rd_seq = s_wr_seq;
	CPU_FOREACH(i) {
		/*
		 * Query the active sequence on this cpu.  If we're not
		 * waiting and we don't meet the goal we will still scan
		 * the rest of the cpus to update s_rd_seq before returning
		 * failure.
		 */
		c_seq = smr_poll_cpu(zpcpu_get_cpu(smr, i), s_rd_seq, goal,
		    wait);

		/*
		 * Limit the minimum observed rd_seq whether we met the goal
		 * or not.
		 */
		if (c_seq != SMR_SEQ_INVALID)
			rd_seq = SMR_SEQ_MIN(rd_seq, c_seq);
	}

	/*
	 * Advance the rd_seq as long as we observed a more recent value.
	 */
	s_rd_seq = atomic_load_int(&s->s_rd_seq);
	if (SMR_SEQ_GT(rd_seq, s_rd_seq)) {
		atomic_cmpset_int(&s->s_rd_seq, s_rd_seq, rd_seq);
		s_rd_seq = rd_seq;
	}

	return (s_rd_seq);
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
	smr_t self;
	smr_seq_t s_wr_seq, s_rd_seq;
	smr_delta_t delta;
	int flags;
	bool success;

	/*
	 * It is illegal to enter while in an smr section.
	 */
	KASSERT(!wait || !SMR_ENTERED(smr),
	    ("smr_poll: Blocking not allowed in a SMR section."));
	KASSERT(!wait || (zpcpu_get(smr)->c_flags & SMR_LAZY) == 0,
	    ("smr_poll: Blocking not allowed on lazy smrs."));

	/*
	 * Use a critical section so that we can avoid ABA races
	 * caused by long preemption sleeps.
	 */
	success = true;
	critical_enter();
	/* Attempt to load from self only once. */
	self = zpcpu_get(smr);
	s = self->c_shared;
	flags = self->c_flags;
	counter_u64_add_protected(poll, 1);

	/*
	 * Conditionally advance the lazy write clock on any writer
	 * activity.
	 */
	if ((flags & SMR_LAZY) != 0)
		smr_lazy_advance(smr, s);

	/*
	 * Acquire barrier loads s_wr_seq after s_rd_seq so that we can not
	 * observe an updated read sequence that is larger than write.
	 */
	s_rd_seq = atomic_load_acq_int(&s->s_rd_seq);

	/*
	 * If we have already observed the sequence number we can immediately
	 * return success.  Most polls should meet this criterion.
	 */
	if (SMR_SEQ_LEQ(goal, s_rd_seq))
		goto out;

	/*
	 * wr_seq must be loaded prior to any c_seq value so that a
	 * stale c_seq can only reference time after this wr_seq.
	 */
	s_wr_seq = atomic_load_acq_int(&s->s_wr.seq);

	/*
	 * This is the distance from s_wr_seq to goal.  Positive values
	 * are in the future.
	 */
	delta = SMR_SEQ_DELTA(goal, s_wr_seq);

	/*
	 * Detect a stale wr_seq.
	 *
	 * This goal may have come from a deferred advance or a lazy
	 * smr.  If we are not blocking we can not succeed but the
	 * sequence number is valid.
	 */
	if (delta > 0 && delta <= SMR_SEQ_ADVANCE &&
	    (flags & (SMR_LAZY | SMR_DEFERRED)) != 0) {
		if (!wait) {
			success = false;
			goto out;
		}
		/* LAZY is always !wait. */
		s_wr_seq = smr_shared_advance(s);
		delta = 0;
	}

	/*
	 * Detect an invalid goal.
	 *
	 * The goal must be in the range of s_wr_seq >= goal >= s_rd_seq for
	 * it to be valid.  If it is not then the caller held on to it and
	 * the integer wrapped.  If we wrapped back within range the caller
	 * will harmlessly scan.
	 */
	if (delta > 0)
		goto out;

	/* Determine the lowest visible sequence number. */
	s_rd_seq = smr_poll_scan(smr, s, s_rd_seq, s_wr_seq, goal, wait);
	success = SMR_SEQ_LEQ(goal, s_rd_seq);
out:
	if (!success)
		counter_u64_add_protected(poll_fail, 1);
	critical_exit();

	/*
	 * Serialize with smr_advance()/smr_exit().  The caller is now free
	 * to modify memory as expected.
	 */
	atomic_thread_fence_acq();

	KASSERT(success || !wait, ("%s: blocking poll failed", __func__));
	return (success);
}

smr_t
smr_create(const char *name, int limit, int flags)
{
	smr_t smr, c;
	smr_shared_t s;
	int i;

	s = uma_zalloc(smr_shared_zone, M_WAITOK);
	smr = uma_zalloc_pcpu(smr_zone, M_WAITOK);

	s->s_name = name;
	s->s_rd_seq = s->s_wr.seq = SMR_SEQ_INIT;
	s->s_wr.ticks = ticks;

	/* Initialize all CPUS, not just those running. */
	for (i = 0; i <= mp_maxid; i++) {
		c = zpcpu_get_cpu(smr, i);
		c->c_seq = SMR_SEQ_INVALID;
		c->c_shared = s;
		c->c_deferred = 0;
		c->c_limit = limit;
		c->c_flags = flags;
	}
	atomic_thread_fence_seq_cst();

	return (smr);
}

void
smr_destroy(smr_t smr)
{

	smr_synchronize(smr);
	uma_zfree(smr_shared_zone, smr->c_shared);
	uma_zfree_pcpu(smr_zone, smr);
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
