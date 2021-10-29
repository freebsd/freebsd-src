/*
 * Copyright 2018 Paul Khuong, Google LLC.
 * All rights reserved.
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

/*
 * Overview
 * ========
 *
 * ck_ec implements 32- and 64- bit event counts. Event counts let us
 * easily integrate OS-level blocking (e.g., futexes) in lock-free
 * protocols. Waiters block conditionally, if the event count's value
 * is still equal to some old value.
 *
 * Event counts come in four variants: 32 and 64 bit (with one bit
 * stolen for internal signaling, so 31 and 63 bit counters), and
 * single or multiple producers (wakers). Waiters are always multiple
 * consumers. The 32 bit variants are smaller, and more efficient,
 * especially in single producer mode. The 64 bit variants are larger,
 * but practically invulnerable to ABA.
 *
 * The 32 bit variant is always available. The 64 bit variant is only
 * available if CK supports 64-bit atomic operations. Currently,
 * specialization for single producer is only implemented for x86 and
 * x86-64, on compilers that support GCC extended inline assembly;
 * other platforms fall back to the multiple producer code path.
 *
 * A typical usage pattern is:
 *
 *  1. On the producer side:
 *
 *    - Make changes to some shared data structure, without involving
 *	the event count at all.
 *    - After each change, call ck_ec_inc on the event count. The call
 *	acts as a write-write barrier, and wakes up any consumer blocked
 *	on the event count (waiting for new changes).
 *
 *  2. On the consumer side:
 *
 *    - Snapshot ck_ec_value of the event count. The call acts as a
 *	read barrier.
 *    - Read and process the shared data structure.
 *    - Wait for new changes by calling ck_ec_wait with the snapshot value.
 *
 * Some data structures may opt for tighter integration with their
 * event count. For example, an SPMC ring buffer or disruptor might
 * use the event count's value as the write pointer. If the buffer is
 * regularly full, it might also make sense to store the read pointer
 * in an MP event count.
 *
 * This event count implementation supports tighter integration in two
 * ways.
 *
 * Producers may opt to increment by an arbitrary value (less than
 * INT32_MAX / INT64_MAX), in order to encode, e.g., byte
 * offsets. Larger increment values make wraparound more likely, so
 * the increments should still be relatively small.
 *
 * Consumers may pass a predicate to ck_ec_wait_pred. This predicate
 * can make `ck_ec_wait_pred` return early, before the event count's
 * value changes, and can override the deadline passed to futex_wait.
 * This lets consumer block on one eventcount, while optimistically
 * looking at other waking conditions.
 *
 * API Reference
 * =============
 *
 * When compiled as C11 or later, this header defines type-generic
 * macros for ck_ec32 and ck_ec64; the reference describes this
 * type-generic API.
 *
 * ck_ec needs additional OS primitives to determine the current time,
 * to wait on an address, and to wake all threads waiting on a given
 * address. These are defined with fields in a struct ck_ec_ops.  Each
 * ck_ec_ops may additionally define the number of spin loop
 * iterations in the slow path, as well as the initial wait time in
 * the internal exponential backoff, the exponential scale factor, and
 * the right shift count (< 32).
 *
 * The ops, in addition to the single/multiple producer flag, are
 * encapsulated in a struct ck_ec_mode, passed to most ck_ec
 * operations.
 *
 * ec is a struct ck_ec32 *, or a struct ck_ec64 *.
 *
 * value is an uint32_t for ck_ec32, and an uint64_t for ck_ec64. It
 * never exceeds INT32_MAX and INT64_MAX respectively.
 *
 * mode is a struct ck_ec_mode *.
 *
 * deadline is either NULL, or a `const struct timespec *` that will
 * be treated as an absolute deadline.
 *
 * `void ck_ec_init(ec, value)`: initializes the event count to value.
 *
 * `value ck_ec_value(ec)`: returns the current value of the event
 *  counter.  This read acts as a read (acquire) barrier.
 *
 * `bool ck_ec_has_waiters(ec)`: returns whether some thread has
 *  marked the event count as requiring an OS wakeup.
 *
 * `void ck_ec_inc(ec, mode)`: increments the value of the event
 *  counter by one. This writes acts as a write barrier. Wakes up
 *  any waiting thread.
 *
 * `value ck_ec_add(ec, mode, value)`: increments the event counter by
 *  `value`, and returns the event counter's previous value. This
 *  write acts as a write barrier. Wakes up any waiting thread.
 *
 * `int ck_ec_deadline(struct timespec *new_deadline,
 *		       mode,
 *		       const struct timespec *timeout)`:
 *  computes a deadline `timeout` away from the current time. If
 *  timeout is NULL, computes a deadline in the infinite future. The
 *  resulting deadline is written to `new_deadline`. Returns 0 on
 *  success, and -1 if ops->gettime failed (without touching errno).
 *
 * `int ck_ec_wait(ec, mode, value, deadline)`: waits until the event
 *  counter's value differs from `value`, or, if `deadline` is
 *  provided and non-NULL, until the current time is after that
 *  deadline. Use a deadline with tv_sec = 0 for a non-blocking
 *  execution. Returns 0 if the event counter has changed, and -1 on
 *  timeout. This function acts as a read (acquire) barrier.
 *
 * `int ck_ec_wait_pred(ec, mode, value, pred, data, deadline)`: waits
 * until the event counter's value differs from `value`, or until
 * `pred` returns non-zero, or, if `deadline` is provided and
 * non-NULL, until the current time is after that deadline. Use a
 * deadline with tv_sec = 0 for a non-blocking execution. Returns 0 if
 * the event counter has changed, `pred`'s return value if non-zero,
 * and -1 on timeout. This function acts as a read (acquire) barrier.
 *
 * `pred` is always called as `pred(data, iteration_deadline, now)`,
 * where `iteration_deadline` is a timespec of the deadline for this
 * exponential backoff iteration, and `now` is the current time. If
 * `pred` returns a non-zero value, that value is immediately returned
 * to the waiter. Otherwise, `pred` is free to modify
 * `iteration_deadline` (moving it further in the future is a bad
 * idea).
 *
 * Implementation notes
 * ====================
 *
 * The multiple producer implementation is a regular locked event
 * count, with a single flag bit to denote the need to wake up waiting
 * threads.
 *
 * The single producer specialization is heavily tied to
 * [x86-TSO](https://www.cl.cam.ac.uk/~pes20/weakmemory/cacm.pdf), and
 * to non-atomic read-modify-write instructions (e.g., `inc mem`);
 * these non-atomic RMW let us write to the same memory locations with
 * atomic and non-atomic instructions, without suffering from process
 * scheduling stalls.
 *
 * The reason we can mix atomic and non-atomic writes to the `counter`
 * word is that every non-atomic write obviates the need for the
 * atomically flipped flag bit: we only use non-atomic writes to
 * update the event count, and the atomic flag only informs the
 * producer that we would like a futex_wake, because of the update.
 * We only require the non-atomic RMW counter update to prevent
 * preemption from introducing arbitrarily long worst case delays.
 *
 * Correctness does not rely on the usual ordering argument: in the
 * absence of fences, there is no strict ordering between atomic and
 * non-atomic writes. The key is instead x86-TSO's guarantee that a
 * read is satisfied from the most recent buffered write in the local
 * store queue if there is one, or from memory if there is no write to
 * that address in the store queue.
 *
 * x86-TSO's constraint on reads suffices to guarantee that the
 * producer will never forget about a counter update. If the last
 * update is still queued, the new update will be based on the queued
 * value. Otherwise, the new update will be based on the value in
 * memory, which may or may not have had its flag flipped. In either
 * case, the value of the counter (modulo flag) is correct.
 *
 * When the producer forwards the counter's value from its store
 * queue, the new update might not preserve a flag flip. Any waiter
 * thus has to check from time to time to determine if it wasn't
 * woken up because the flag bit was silently cleared.
 *
 * In reality, the store queue in x86-TSO stands for in-flight
 * instructions in the chip's out-of-order backend. In the vast
 * majority of cases, instructions will only remain in flight for a
 * few hundred or thousand of cycles. That's why ck_ec_wait spins on
 * the `counter` word for ~100 iterations after flipping its flag bit:
 * if the counter hasn't changed after that many iterations, it is
 * very likely that the producer's next counter update will observe
 * the flag flip.
 *
 * That's still not a hard guarantee of correctness. Conservatively,
 * we can expect that no instruction will remain in flight for more
 * than 1 second... if only because some interrupt will have forced
 * the chip to store its architectural state in memory, at which point
 * an instruction is either fully retired or rolled back. Interrupts,
 * particularly the pre-emption timer, are why single-producer updates
 * must happen in a single non-atomic read-modify-write instruction.
 * Having a single instruction as the critical section means we only
 * have to consider the worst-case execution time for that
 * instruction. That's easier than doing the same for a pair of
 * instructions, which an unlucky pre-emption could delay for
 * arbitrarily long.
 *
 * Thus, after a short spin loop, ck_ec_wait enters an exponential
 * backoff loop, where each "sleep" is instead a futex_wait.  The
 * backoff is only necessary to handle rare cases where the flag flip
 * was overwritten after the spin loop. Eventually, more than one
 * second will have elapsed since the flag flip, and the sleep timeout
 * becomes infinite: since the flag bit has been set for much longer
 * than the time for which an instruction may remain in flight, the
 * flag will definitely be observed at the next counter update.
 *
 * The 64 bit ck_ec_wait pulls another trick: futexes only handle 32
 * bit ints, so we must treat the 64 bit counter's low 32 bits as an
 * int in futex_wait. That's a bit dodgy, but fine in practice, given
 * that the OS's futex code will always read whatever value is
 * currently in memory: even if the producer thread were to wait on
 * its own event count, the syscall and ring transition would empty
 * the store queue (the out-of-order execution backend).
 *
 * Finally, what happens when the producer is migrated to another core
 * or otherwise pre-empted? Migration must already incur a barrier, so
 * that thread always sees its own writes, so that's safe. As for
 * pre-emption, that requires storing the architectural state, which
 * means every instruction must either be executed fully or not at
 * all when pre-emption happens.
 */

#ifndef CK_EC_H
#define CK_EC_H
#include <ck_cc.h>
#include <ck_pr.h>
#include <ck_stdbool.h>
#include <ck_stdint.h>
#include <ck_stddef.h>
#include <sys/time.h>

/*
 * If we have ck_pr_faa_64 (and, presumably, ck_pr_load_64), we
 * support 63 bit counters.
 */
#ifdef CK_F_PR_FAA_64
#define CK_F_EC64
#endif /* CK_F_PR_FAA_64 */

/*
 * GCC inline assembly lets us exploit non-atomic read-modify-write
 * instructions on x86/x86_64 for a fast single-producer mode.
 *
 * If we CK_F_EC_SP is not defined, CK_EC always uses the slower
 * multiple producer code.
 */
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
#define CK_F_EC_SP
#endif /* GNUC && (__i386__ || __x86_64__) */

struct ck_ec_ops;

struct ck_ec_wait_state {
	struct timespec start;	/* Time when we entered ck_ec_wait. */
	struct timespec now;  /* Time now. */
	const struct ck_ec_ops *ops;
	void *data;  /* Opaque pointer for the predicate's internal state. */

};

/*
 * ck_ec_ops define system-specific functions to get the current time,
 * atomically wait on an address if it still has some expected value,
 * and to wake all threads waiting on an address.
 *
 * Each platform is expected to have few (one) opaque pointer to a
 * const ops struct, and reuse it for all ck_ec_mode structs.
 */
struct ck_ec_ops {
	/* Populates out with the current time. Returns non-zero on failure. */
	int (*gettime)(const struct ck_ec_ops *, struct timespec *out);

	/*
	 * Waits on address if its value is still `expected`.  If
	 * deadline is non-NULL, stops waiting once that deadline is
	 * reached. May return early for any reason.
	 */
	void (*wait32)(const struct ck_ec_wait_state *, const uint32_t *,
		       uint32_t expected, const struct timespec *deadline);

	/*
	 * Same as wait32, but for a 64 bit counter. Only used if
	 * CK_F_EC64 is defined.
	 *
	 * If underlying blocking primitive only supports 32 bit
	 * control words, it should be safe to block on the least
	 * significant half of the 64 bit address.
	 */
	void (*wait64)(const struct ck_ec_wait_state *, const uint64_t *,
		       uint64_t expected, const struct timespec *deadline);

	/* Wakes up all threads waiting on address. */
	void (*wake32)(const struct ck_ec_ops *, const uint32_t *address);

	/*
	 * Same as wake32, but for a 64 bit counter. Only used if
	 * CK_F_EC64 is defined.
	 *
	 * When wait64 truncates the control word at address to `only`
	 * consider its least significant half, wake64 should perform
	 * any necessary fixup (e.g., on big endian platforms).
	 */
	void (*wake64)(const struct ck_ec_ops *, const uint64_t *address);

	/*
	 * Number of iterations for the initial busy wait. 0 defaults
	 * to 100 (not ABI stable).
	 */
	uint32_t busy_loop_iter;

	/*
	 * Delay in nanoseconds for the first iteration of the
	 * exponential backoff. 0 defaults to 2 ms (not ABI stable).
	 */
	uint32_t initial_wait_ns;

	/*
	 * Scale factor for the exponential backoff. 0 defaults to 8x
	 * (not ABI stable).
	 */
	uint32_t wait_scale_factor;

	/*
	 * Right shift count for the exponential backoff. The update
	 * after each iteration is
	 *     wait_ns = (wait_ns * wait_scale_factor) >> wait_shift_count,
	 * until one second has elapsed. After that, the deadline goes
	 * to infinity.
	 */
	uint32_t wait_shift_count;
};

/*
 * ck_ec_mode wraps the ops table, and informs the fast path whether
 * it should attempt to specialize for single producer mode.
 *
 * mode structs are expected to be exposed by value, e.g.,
 *
 *    extern const struct ck_ec_ops system_ec_ops;
 *
 *    static const struct ck_ec_mode ec_sp = {
 *	  .ops = &system_ec_ops,
 *	  .single_producer = true
 *    };
 *
 *    static const struct ck_ec_mode ec_mp = {
 *	  .ops = &system_ec_ops,
 *	  .single_producer = false
 *    };
 *
 * ck_ec_mode structs are only passed to inline functions defined in
 * this header, and never escape to their slow paths, so they should
 * not result in any object file size increase.
 */
struct ck_ec_mode {
	const struct ck_ec_ops *ops;
	/*
	 * If single_producer is true, the event count has a unique
	 * incrementer. The implementation will specialize ck_ec_inc
	 * and ck_ec_add if possible (if CK_F_EC_SP is defined).
	 */
	bool single_producer;
};

struct ck_ec32 {
	/* Flag is "sign" bit, value in bits 0:30. */
	uint32_t counter;
};

typedef struct ck_ec32 ck_ec32_t;

#ifdef CK_F_EC64
struct ck_ec64 {
	/*
	 * Flag is bottom bit, value in bits 1:63. Eventcount only
	 * works on x86-64 (i.e., little endian), so the futex int
	 * lies in the first 4 (bottom) bytes.
	 */
	uint64_t counter;
};

typedef struct ck_ec64 ck_ec64_t;
#endif /* CK_F_EC64 */

#define CK_EC_INITIALIZER { .counter = 0 }

/*
 * Initializes the event count to `value`. The value must not
 * exceed INT32_MAX.
 */
static void ck_ec32_init(struct ck_ec32 *ec, uint32_t value);

#ifndef CK_F_EC64
#define ck_ec_init ck_ec32_init
#else
/*
 * Initializes the event count to `value`. The value must not
 * exceed INT64_MAX.
 */
static void ck_ec64_init(struct ck_ec64 *ec, uint64_t value);

#if __STDC_VERSION__ >= 201112L
#define ck_ec_init(EC, VALUE)				\
	(_Generic(*(EC),				\
		  struct ck_ec32 : ck_ec32_init,	\
		  struct ck_ec64 : ck_ec64_init)((EC), (VALUE)))
#endif /* __STDC_VERSION__ */
#endif /* CK_F_EC64 */

/*
 * Returns the counter value in the event count. The value is at most
 * INT32_MAX.
 */
static uint32_t ck_ec32_value(const struct ck_ec32* ec);

#ifndef CK_F_EC64
#define ck_ec_value ck_ec32_value
#else
/*
 * Returns the counter value in the event count. The value is at most
 * INT64_MAX.
 */
static uint64_t ck_ec64_value(const struct ck_ec64* ec);

#if __STDC_VERSION__ >= 201112L
#define ck_ec_value(EC)					\
	(_Generic(*(EC),				\
		  struct ck_ec32 : ck_ec32_value,	\
		struct ck_ec64 : ck_ec64_value)((EC)))
#endif /* __STDC_VERSION__ */
#endif /* CK_F_EC64 */

/*
 * Returns whether there may be slow pathed waiters that need an
 * explicit OS wakeup for this event count.
 */
static bool ck_ec32_has_waiters(const struct ck_ec32 *ec);

#ifndef CK_F_EC64
#define ck_ec_has_waiters ck_ec32_has_waiters
#else
static bool ck_ec64_has_waiters(const struct ck_ec64 *ec);

#if __STDC_VERSION__ >= 201112L
#define ck_ec_has_waiters(EC)				      \
	(_Generic(*(EC),				      \
		  struct ck_ec32 : ck_ec32_has_waiters,	      \
		  struct ck_ec64 : ck_ec64_has_waiters)((EC)))
#endif /* __STDC_VERSION__ */
#endif /* CK_F_EC64 */

/*
 * Increments the counter value in the event count by one, and wakes
 * up any waiter.
 */
static void ck_ec32_inc(struct ck_ec32 *ec, const struct ck_ec_mode *mode);

#ifndef CK_F_EC64
#define ck_ec_inc ck_ec32_inc
#else
static void ck_ec64_inc(struct ck_ec64 *ec, const struct ck_ec_mode *mode);

#if __STDC_VERSION__ >= 201112L
#define ck_ec_inc(EC, MODE)					\
	(_Generic(*(EC),					\
		  struct ck_ec32 : ck_ec32_inc,			\
		  struct ck_ec64 : ck_ec64_inc)((EC), (MODE)))
#endif /* __STDC_VERSION__ */
#endif /* CK_F_EC64 */

/*
 * Increments the counter value in the event count by delta, wakes
 * up any waiter, and returns the previous counter value.
 */
static uint32_t ck_ec32_add(struct ck_ec32 *ec,
			    const struct ck_ec_mode *mode,
			    uint32_t delta);

#ifndef CK_F_EC64
#define ck_ec_add ck_ec32_add
#else
static uint64_t ck_ec64_add(struct ck_ec64 *ec,
			    const struct ck_ec_mode *mode,
			    uint64_t delta);

#if __STDC_VERSION__ >= 201112L
#define ck_ec_add(EC, MODE, DELTA)					\
	(_Generic(*(EC),						\
		  struct ck_ec32 : ck_ec32_add,				\
		  struct ck_ec64 : ck_ec64_add)((EC), (MODE), (DELTA)))
#endif /* __STDC_VERSION__ */
#endif /* CK_F_EC64 */

/*
 * Populates `new_deadline` with a deadline `timeout` in the future.
 * Returns 0 on success, and -1 if clock_gettime failed, in which
 * case errno is left as is.
 */
static int ck_ec_deadline(struct timespec *new_deadline,
			  const struct ck_ec_mode *mode,
			  const struct timespec *timeout);

/*
 * Waits until the counter value in the event count differs from
 * old_value, or, if deadline is non-NULL, until CLOCK_MONOTONIC is
 * past the deadline.
 *
 * Returns 0 on success, and -1 on timeout.
 */
static int ck_ec32_wait(struct ck_ec32 *ec,
			const struct ck_ec_mode *mode,
			uint32_t old_value,
			const struct timespec *deadline);

#ifndef CK_F_EC64
#define ck_ec_wait ck_ec32_wait
#else
static int ck_ec64_wait(struct ck_ec64 *ec,
			const struct ck_ec_mode *mode,
			uint64_t old_value,
			const struct timespec *deadline);

#if __STDC_VERSION__ >= 201112L
#define ck_ec_wait(EC, MODE, OLD_VALUE, DEADLINE)			\
	(_Generic(*(EC),						\
		  struct ck_ec32 : ck_ec32_wait,			\
		  struct ck_ec64 : ck_ec64_wait)((EC), (MODE),		\
						 (OLD_VALUE), (DEADLINE)))

#endif /* __STDC_VERSION__ */
#endif /* CK_F_EC64 */

/*
 * Waits until the counter value in the event count differs from
 * old_value, pred returns non-zero, or, if deadline is non-NULL,
 * until CLOCK_MONOTONIC is past the deadline.
 *
 * Returns 0 on success, -1 on timeout, and the return value of pred
 * if it returns non-zero.
 *
 * A NULL pred represents a function that always returns 0.
 */
static int ck_ec32_wait_pred(struct ck_ec32 *ec,
			     const struct ck_ec_mode *mode,
			     uint32_t old_value,
			     int (*pred)(const struct ck_ec_wait_state *,
					 struct timespec *deadline),
			     void *data,
			     const struct timespec *deadline);

#ifndef CK_F_EC64
#define ck_ec_wait_pred ck_ec32_wait_pred
#else
static int ck_ec64_wait_pred(struct ck_ec64 *ec,
			     const struct ck_ec_mode *mode,
			     uint64_t old_value,
			     int (*pred)(const struct ck_ec_wait_state *,
					 struct timespec *deadline),
			     void *data,
			     const struct timespec *deadline);

#if __STDC_VERSION__ >= 201112L
#define ck_ec_wait_pred(EC, MODE, OLD_VALUE, PRED, DATA, DEADLINE)	\
	(_Generic(*(EC),						\
		  struct ck_ec32 : ck_ec32_wait_pred,			\
		  struct ck_ec64 : ck_ec64_wait_pred)			\
	 ((EC), (MODE), (OLD_VALUE), (PRED), (DATA), (DEADLINE)))
#endif /* __STDC_VERSION__ */
#endif /* CK_F_EC64 */

/*
 * Inline implementation details. 32 bit first, then 64 bit
 * conditionally.
 */
CK_CC_FORCE_INLINE void ck_ec32_init(struct ck_ec32 *ec, uint32_t value)
{
	ec->counter = value & ~(1UL << 31);
	return;
}

CK_CC_FORCE_INLINE uint32_t ck_ec32_value(const struct ck_ec32 *ec)
{
	uint32_t ret = ck_pr_load_32(&ec->counter) & ~(1UL << 31);

	ck_pr_fence_acquire();
	return ret;
}

CK_CC_FORCE_INLINE bool ck_ec32_has_waiters(const struct ck_ec32 *ec)
{
	return ck_pr_load_32(&ec->counter) & (1UL << 31);
}

/* Slow path for ck_ec{32,64}_{inc,add} */
void ck_ec32_wake(struct ck_ec32 *ec, const struct ck_ec_ops *ops);

CK_CC_FORCE_INLINE void ck_ec32_inc(struct ck_ec32 *ec,
				    const struct ck_ec_mode *mode)
{
#if !defined(CK_F_EC_SP)
	/* Nothing to specialize if we don't have EC_SP. */
	ck_ec32_add(ec, mode, 1);
	return;
#else
	char flagged;

#if __GNUC__ >= 6
	/*
	 * We don't want to wake if the sign bit is 0. We do want to
	 * wake if the sign bit just flipped from 1 to 0. We don't
	 * care what happens when our increment caused the sign bit to
	 * flip from 0 to 1 (that's once per 2^31 increment).
	 *
	 * This leaves us with four cases:
	 *
	 *  old sign bit | new sign bit | SF | OF | ZF
	 *  -------------------------------------------
	 *	       0 |	      0 |  0 |	0 | ?
	 *	       0 |	      1 |  1 |	0 | ?
	 *	       1 |	      1 |  1 |	0 | ?
	 *	       1 |	      0 |  0 |	0 | 1
	 *
	 * In the first case, we don't want to hit ck_ec32_wake. In
	 * the last two cases, we do want to call ck_ec32_wake. In the
	 * second case, we don't care, so we arbitrarily choose to
	 * call ck_ec32_wake.
	 *
	 * The "le" condition checks if SF != OF, or ZF == 1, which
	 * meets our requirements.
	 */
#define CK_EC32_INC_ASM(PREFIX)					\
	__asm__ volatile(PREFIX " incl %0"		    \
			 : "+m"(ec->counter), "=@ccle"(flagged)	 \
			 :: "cc", "memory")
#else
#define CK_EC32_INC_ASM(PREFIX)						\
	__asm__ volatile(PREFIX " incl %0; setle %1"			\
			 : "+m"(ec->counter), "=r"(flagged)		\
			 :: "cc", "memory")
#endif /* __GNUC__ */

	if (mode->single_producer == true) {
		ck_pr_fence_store();
		CK_EC32_INC_ASM("");
	} else {
		ck_pr_fence_store_atomic();
		CK_EC32_INC_ASM("lock");
	}
#undef CK_EC32_INC_ASM

	if (CK_CC_UNLIKELY(flagged)) {
		ck_ec32_wake(ec, mode->ops);
	}

	return;
#endif /* CK_F_EC_SP */
}

CK_CC_FORCE_INLINE uint32_t ck_ec32_add_epilogue(struct ck_ec32 *ec,
						 const struct ck_ec_mode *mode,
						 uint32_t old)
{
	const uint32_t flag_mask = 1U << 31;
	uint32_t ret;

	ret = old & ~flag_mask;
	/* These two only differ if the flag bit is set. */
	if (CK_CC_UNLIKELY(old != ret)) {
		ck_ec32_wake(ec, mode->ops);
	}

	return ret;
}

static CK_CC_INLINE uint32_t ck_ec32_add_mp(struct ck_ec32 *ec,
					    const struct ck_ec_mode *mode,
					    uint32_t delta)
{
	uint32_t old;

	ck_pr_fence_store_atomic();
	old = ck_pr_faa_32(&ec->counter, delta);
	return ck_ec32_add_epilogue(ec, mode, old);
}

#ifdef CK_F_EC_SP
static CK_CC_INLINE uint32_t ck_ec32_add_sp(struct ck_ec32 *ec,
					    const struct ck_ec_mode *mode,
					    uint32_t delta)
{
	uint32_t old;

	/*
	 * Correctness of this racy write depends on actually
	 * having an update to write. Exit here if the update
	 * is a no-op.
	 */
	if (CK_CC_UNLIKELY(delta == 0)) {
		return ck_ec32_value(ec);
	}

	ck_pr_fence_store();
	old = delta;
	__asm__ volatile("xaddl %1, %0"
			 : "+m"(ec->counter), "+r"(old)
			 :: "cc", "memory");
	return ck_ec32_add_epilogue(ec, mode, old);
}
#endif /* CK_F_EC_SP */

CK_CC_FORCE_INLINE uint32_t ck_ec32_add(struct ck_ec32 *ec,
					const struct ck_ec_mode *mode,
					uint32_t delta)
{
#ifdef CK_F_EC_SP
	if (mode->single_producer == true) {
		return ck_ec32_add_sp(ec, mode, delta);
	}
#endif

	return ck_ec32_add_mp(ec, mode, delta);
}

int ck_ec_deadline_impl(struct timespec *new_deadline,
			const struct ck_ec_ops *ops,
			const struct timespec *timeout);

CK_CC_FORCE_INLINE int ck_ec_deadline(struct timespec *new_deadline,
				      const struct ck_ec_mode *mode,
				      const struct timespec *timeout)
{
	return ck_ec_deadline_impl(new_deadline, mode->ops, timeout);
}


int ck_ec32_wait_slow(struct ck_ec32 *ec,
		      const struct ck_ec_ops *ops,
		      uint32_t old_value,
		      const struct timespec *deadline);

CK_CC_FORCE_INLINE int ck_ec32_wait(struct ck_ec32 *ec,
				    const struct ck_ec_mode *mode,
				    uint32_t old_value,
				    const struct timespec *deadline)
{
	if (ck_ec32_value(ec) != old_value) {
		return 0;
	}

	return ck_ec32_wait_slow(ec, mode->ops, old_value, deadline);
}

int ck_ec32_wait_pred_slow(struct ck_ec32 *ec,
			   const struct ck_ec_ops *ops,
			   uint32_t old_value,
			   int (*pred)(const struct ck_ec_wait_state *state,
				       struct timespec *deadline),
			   void *data,
			   const struct timespec *deadline);

CK_CC_FORCE_INLINE int
ck_ec32_wait_pred(struct ck_ec32 *ec,
		  const struct ck_ec_mode *mode,
		  uint32_t old_value,
		  int (*pred)(const struct ck_ec_wait_state *state,
			      struct timespec *deadline),
		  void *data,
		  const struct timespec *deadline)
{
	if (ck_ec32_value(ec) != old_value) {
		return 0;
	}

	return ck_ec32_wait_pred_slow(ec, mode->ops, old_value,
				      pred, data, deadline);
}

#ifdef CK_F_EC64
CK_CC_FORCE_INLINE void ck_ec64_init(struct ck_ec64 *ec, uint64_t value)
{
	ec->counter = value << 1;
	return;
}

CK_CC_FORCE_INLINE uint64_t ck_ec64_value(const struct ck_ec64 *ec)
{
	uint64_t ret = ck_pr_load_64(&ec->counter) >> 1;

	ck_pr_fence_acquire();
	return ret;
}

CK_CC_FORCE_INLINE bool ck_ec64_has_waiters(const struct ck_ec64 *ec)
{
	return ck_pr_load_64(&ec->counter) & 1;
}

void ck_ec64_wake(struct ck_ec64 *ec, const struct ck_ec_ops *ops);

CK_CC_FORCE_INLINE void ck_ec64_inc(struct ck_ec64 *ec,
				    const struct ck_ec_mode *mode)
{
	/* We always xadd, so there's no special optimization here. */
	(void)ck_ec64_add(ec, mode, 1);
	return;
}

CK_CC_FORCE_INLINE uint64_t ck_ec_add64_epilogue(struct ck_ec64 *ec,
					       const struct ck_ec_mode *mode,
					       uint64_t old)
{
	uint64_t ret = old >> 1;

	if (CK_CC_UNLIKELY(old & 1)) {
		ck_ec64_wake(ec, mode->ops);
	}

	return ret;
}

static CK_CC_INLINE uint64_t ck_ec64_add_mp(struct ck_ec64 *ec,
					    const struct ck_ec_mode *mode,
					    uint64_t delta)
{
	uint64_t inc = 2 * delta;  /* The low bit is the flag bit. */

	ck_pr_fence_store_atomic();
	return ck_ec_add64_epilogue(ec, mode, ck_pr_faa_64(&ec->counter, inc));
}

#ifdef CK_F_EC_SP
/* Single-producer specialisation. */
static CK_CC_INLINE uint64_t ck_ec64_add_sp(struct ck_ec64 *ec,
					    const struct ck_ec_mode *mode,
					    uint64_t delta)
{
	uint64_t old;

	/*
	 * Correctness of this racy write depends on actually
	 * having an update to write. Exit here if the update
	 * is a no-op.
	 */
	if (CK_CC_UNLIKELY(delta == 0)) {
		return ck_ec64_value(ec);
	}

	ck_pr_fence_store();
	old = 2 * delta;  /* The low bit is the flag bit. */
	__asm__ volatile("xaddq %1, %0"
			 : "+m"(ec->counter), "+r"(old)
			 :: "cc", "memory");
	return ck_ec_add64_epilogue(ec, mode, old);
}
#endif /* CK_F_EC_SP */

/*
 * Dispatch on mode->single_producer in this FORCE_INLINE function:
 * the end result is always small, but not all compilers have enough
 * foresight to inline and get the reduction.
 */
CK_CC_FORCE_INLINE uint64_t ck_ec64_add(struct ck_ec64 *ec,
					const struct ck_ec_mode *mode,
					uint64_t delta)
{
#ifdef CK_F_EC_SP
	if (mode->single_producer == true) {
		return ck_ec64_add_sp(ec, mode, delta);
	}
#endif

	return ck_ec64_add_mp(ec, mode, delta);
}

int ck_ec64_wait_slow(struct ck_ec64 *ec,
		      const struct ck_ec_ops *ops,
		      uint64_t old_value,
		      const struct timespec *deadline);

CK_CC_FORCE_INLINE int ck_ec64_wait(struct ck_ec64 *ec,
				    const struct ck_ec_mode *mode,
				    uint64_t old_value,
				    const struct timespec *deadline)
{
	if (ck_ec64_value(ec) != old_value) {
		return 0;
	}

	return ck_ec64_wait_slow(ec, mode->ops, old_value, deadline);
}

int ck_ec64_wait_pred_slow(struct ck_ec64 *ec,
			   const struct ck_ec_ops *ops,
			   uint64_t old_value,
			   int (*pred)(const struct ck_ec_wait_state *state,
				       struct timespec *deadline),
			   void *data,
			   const struct timespec *deadline);


CK_CC_FORCE_INLINE int
ck_ec64_wait_pred(struct ck_ec64 *ec,
		  const struct ck_ec_mode *mode,
		  uint64_t old_value,
		  int (*pred)(const struct ck_ec_wait_state *state,
			      struct timespec *deadline),
		  void *data,
		  const struct timespec *deadline)
{
	if (ck_ec64_value(ec) != old_value) {
		return 0;
	}

	return ck_ec64_wait_pred_slow(ec, mode->ops, old_value,
				      pred, data, deadline);
}
#endif /* CK_F_EC64 */
#endif /* !CK_EC_H */
