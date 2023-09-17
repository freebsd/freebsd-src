#include <ck_ec.h>
#include <ck_limits.h>

#include "ck_ec_timeutil.h"

#define DEFAULT_BUSY_LOOP_ITER 100U

/*
 * The 2ms, 8x/iter default parameter hit 1.024 seconds after 3
 * iterations.
 */
#define DEFAULT_INITIAL_WAIT_NS 2000000L  /* Start at 2 ms */
/* Grow the wait time 8x/iteration. */
#define DEFAULT_WAIT_SCALE_FACTOR 8
#define DEFAULT_WAIT_SHIFT_COUNT 0

struct ck_ec32_slow_path_state {
	struct ck_ec32 *ec;
	uint32_t flagged_word;
};

#ifdef CK_F_EC64
struct ck_ec64_slow_path_state {
	struct ck_ec64 *ec;
	uint64_t flagged_word;
};
#endif

/* Once we've waited for >= 1 sec, go for the full deadline. */
static const struct timespec final_wait_time = {
	.tv_sec = 1
};

void
ck_ec32_wake(struct ck_ec32 *ec, const struct ck_ec_ops *ops)
{
	/* Spurious wake-ups are OK. Clear the flag before futexing. */
	ck_pr_and_32(&ec->counter, (1U << 31) - 1);
	ops->wake32(ops, &ec->counter);
	return;
}

int
ck_ec32_wait_slow(struct ck_ec32 *ec,
    const struct ck_ec_ops *ops,
    uint32_t old_value,
    const struct timespec *deadline)
{
	return ck_ec32_wait_pred_slow(ec, ops, old_value,
				      NULL, NULL, deadline);
}

#ifdef CK_F_EC64
void
ck_ec64_wake(struct ck_ec64 *ec, const struct ck_ec_ops *ops)
{
	ck_pr_and_64(&ec->counter, ~1);
	ops->wake64(ops, &ec->counter);
	return;
}

int
ck_ec64_wait_slow(struct ck_ec64 *ec,
    const struct ck_ec_ops *ops,
    uint64_t old_value,
    const struct timespec *deadline)
{
	return ck_ec64_wait_pred_slow(ec, ops, old_value,
				      NULL, NULL, deadline);
}
#endif

int
ck_ec_deadline_impl(struct timespec *new_deadline,
    const struct ck_ec_ops *ops,
    const struct timespec *timeout)
{
	struct timespec now;
	int r;

	if (timeout == NULL) {
		new_deadline->tv_sec = TIME_MAX;
		new_deadline->tv_nsec = NSEC_MAX;
		return 0;
	}

	r = ops->gettime(ops, &now);
	if (r != 0) {
		return -1;
	}

	*new_deadline = timespec_add(now, *timeout);
	return 0;
}

/* The rest of the file implements wait_pred_slow. */

/*
 * Returns a timespec value for deadline_ptr. If deadline_ptr is NULL,
 * returns a timespec far in the future.
 */
static struct timespec
canonical_deadline(const struct timespec *deadline_ptr)
{

	if (deadline_ptr == NULL) {
		return (struct timespec) { .tv_sec = TIME_MAX };
	}

	return *deadline_ptr;
}

/*
 * Really slow (sleeping) path for ck_ec_wait.	Drives the exponential
 * backoff scheme to sleep for longer and longer periods of time,
 * until either the sleep function returns true (the eventcount's
 * value has changed), or the predicate returns non-0 (something else
 * has changed).
 *
 * If deadline is ever reached, returns -1 (timeout).
 *
 * TODO: add some form of randomisation to the intermediate timeout
 * values.
 */
static int
exponential_backoff(struct ck_ec_wait_state *wait_state,
    bool (*sleep)(const void *sleep_state,
	const struct ck_ec_wait_state *wait_state,
	const struct timespec *partial_deadline),
    const void *sleep_state,
    int (*pred)(const struct ck_ec_wait_state *state,
	struct timespec *deadline),
    const struct timespec *deadline)
{
	struct timespec begin;
	struct timespec stop_backoff;
	const struct ck_ec_ops *ops = wait_state->ops;
	const uint32_t scale_factor = (ops->wait_scale_factor != 0)
	    ? ops->wait_scale_factor
	    : DEFAULT_WAIT_SCALE_FACTOR;
	const uint32_t shift_count = (ops->wait_shift_count != 0)
	    ? ops->wait_shift_count
	    : DEFAULT_WAIT_SHIFT_COUNT;
	uint32_t wait_ns = (ops->initial_wait_ns != 0)
	    ? ops->initial_wait_ns
	    : DEFAULT_INITIAL_WAIT_NS;
	bool first = true;

	for (;;) {
		struct timespec now;
		struct timespec partial_deadline;

		if (check_deadline(&now, ops, *deadline) == true) {
			/* Timeout. Bail out. */
			return -1;
		}

		if (first) {
			begin = now;
			wait_state->start = begin;
			stop_backoff = timespec_add(begin, final_wait_time);
			first = false;
		}

		wait_state->now = now;
		if (timespec_cmp(now, stop_backoff) >= 0) {
			partial_deadline = *deadline;
		} else {
			do {
				partial_deadline =
				    timespec_add_ns(begin, wait_ns);
				wait_ns =
				    wait_time_scale(wait_ns,
						    scale_factor,
						    shift_count);
			} while (timespec_cmp(partial_deadline, now) <= 0);
		}

		if (pred != NULL) {
			int r = pred(wait_state, &partial_deadline);
			if (r != 0) {
				return r;
			}
		}

		/* Canonicalize deadlines in the far future to NULL. */
		if (sleep(sleep_state, wait_state,
			  ((partial_deadline.tv_sec == TIME_MAX)
			   ? NULL :  &partial_deadline)) == true) {
			return 0;
		}
	}
}

/*
 * Loops up to BUSY_LOOP_ITER times, or until ec's counter value
 * (including the flag) differs from old_value.
 *
 * Returns the new value in ec.
 */
#define DEF_WAIT_EASY(W)						\
	static uint##W##_t ck_ec##W##_wait_easy(struct ck_ec##W* ec,	\
						const struct ck_ec_ops *ops, \
						uint##W##_t expected)	\
	{								\
		uint##W##_t current = ck_pr_load_##W(&ec->counter);	\
		size_t n = (ops->busy_loop_iter != 0)			\
		    ? ops->busy_loop_iter				\
		    : DEFAULT_BUSY_LOOP_ITER;				\
									\
		for (size_t i = 0;					\
		     i < n && current == expected;			\
		     i++) {						\
			ck_pr_stall();					\
			current = ck_pr_load_##W(&ec->counter);		\
		}							\
									\
		return current;						\
	}

DEF_WAIT_EASY(32)
#ifdef CK_F_EC64
DEF_WAIT_EASY(64)
#endif
#undef DEF_WAIT_EASY
/*
 * Attempts to upgrade ec->counter from unflagged to flagged.
 *
 * Returns true if the event count has changed. Otherwise, ec's
 * counter word is equal to flagged on return, or has been at some
 * time before the return.
 */
#define DEF_UPGRADE(W)							\
	static bool ck_ec##W##_upgrade(struct ck_ec##W* ec,		\
				       uint##W##_t current,		\
				       uint##W##_t unflagged,		\
				       uint##W##_t flagged)		\
	{								\
		uint##W##_t old_word;					\
									\
		if (current == flagged) {				\
			/* Nothing to do, no change. */			\
			return false;					\
		}							\
									\
		if (current != unflagged) {				\
			/* We have a different counter value! */	\
			return true;					\
		}							\
									\
		/*							\
		 * Flag the counter value. The CAS only fails if the	\
		 * counter is already flagged, or has a new value.	\
		 */							\
		return (ck_pr_cas_##W##_value(&ec->counter,		\
					      unflagged, flagged,	\
					      &old_word) == false &&	\
			old_word != flagged);				\
	}

DEF_UPGRADE(32)
#ifdef CK_F_EC64
DEF_UPGRADE(64)
#endif
#undef DEF_UPGRADE

/*
 * Blocks until partial_deadline on the ck_ec. Returns true if the
 * eventcount's value has changed. If partial_deadline is NULL, wait
 * forever.
 */
static bool
ck_ec32_wait_slow_once(const void *vstate,
    const struct ck_ec_wait_state *wait_state,
    const struct timespec *partial_deadline)
{
	const struct ck_ec32_slow_path_state *state = vstate;
	const struct ck_ec32 *ec = state->ec;
	const uint32_t flagged_word = state->flagged_word;

	wait_state->ops->wait32(wait_state, &ec->counter,
				flagged_word, partial_deadline);
	return ck_pr_load_32(&ec->counter) != flagged_word;
}

#ifdef CK_F_EC64
static bool
ck_ec64_wait_slow_once(const void *vstate,
    const struct ck_ec_wait_state *wait_state,
    const struct timespec *partial_deadline)
{
	const struct ck_ec64_slow_path_state *state = vstate;
	const struct ck_ec64 *ec = state->ec;
	const uint64_t flagged_word = state->flagged_word;

	/* futex_wait will only compare the low 32 bits. Perform a
	 * full comparison here to maximise the changes of catching an
	 * ABA in the low 32 bits.
	 */
	if (ck_pr_load_64(&ec->counter) != flagged_word) {
		return true;
	}

	wait_state->ops->wait64(wait_state, &ec->counter,
				flagged_word, partial_deadline);
	return ck_pr_load_64(&ec->counter) != flagged_word;
}
#endif

/*
 * The full wait logic is a lot of code (> 1KB). Encourage the
 * compiler to lay this all out linearly with LIKELY annotations on
 * every early exit.
 */
#define WAIT_SLOW_BODY(W, ec, ops, pred, data, deadline_ptr,		\
		       old_value, unflagged, flagged)			\
	do {								\
		struct ck_ec_wait_state wait_state = {			\
			.ops = ops,					\
			.data = data					\
		};							\
		const struct ck_ec##W##_slow_path_state state = {	\
			.ec = ec,					\
			.flagged_word = flagged				\
		};							\
		const struct timespec deadline =			\
			canonical_deadline(deadline_ptr);		\
									\
		/* Detect infinite past deadlines. */			\
		if (CK_CC_LIKELY(deadline.tv_sec <= 0)) {		\
			return -1;					\
		}							\
									\
		for (;;) {						\
			uint##W##_t current;				\
			int r;						\
									\
			current = ck_ec##W##_wait_easy(ec, ops, unflagged); \
									\
			/*						\
			 * We're about to wait harder (i.e.,		\
			 * potentially with futex). Make sure the	\
			 * counter word is flagged.			\
			 */						\
			if (CK_CC_LIKELY(				\
				ck_ec##W##_upgrade(ec, current,		\
					unflagged, flagged) == true)) { \
				ck_pr_fence_acquire();			\
				return 0;				\
			}						\
									\
			/*						\
			 * By now, ec->counter == flagged_word (at	\
			 * some point in the past). Spin some more to	\
			 * heuristically let any in-flight SP inc/add	\
			 * to retire. This does not affect		\
			 * correctness, but practically eliminates	\
			 * lost wake-ups.				\
			 */						\
			current = ck_ec##W##_wait_easy(ec, ops, flagged); \
			if (CK_CC_LIKELY(current != flagged_word)) {	\
				ck_pr_fence_acquire();			\
				return 0;				\
			}						\
									\
			r = exponential_backoff(&wait_state,		\
						ck_ec##W##_wait_slow_once, \
						&state,			\
						pred, &deadline); \
			if (r != 0) {					\
				return r;				\
			}						\
									\
			if (ck_ec##W##_value(ec) != old_value) {	\
				ck_pr_fence_acquire();			\
				return 0;				\
			}						\
									\
			/* Spurious wake-up. Redo the slow path. */	\
		}							\
	} while (0)

int
ck_ec32_wait_pred_slow(struct ck_ec32 *ec,
    const struct ck_ec_ops *ops,
    uint32_t old_value,
    int (*pred)(const struct ck_ec_wait_state *state,
	struct timespec *deadline),
    void *data,
    const struct timespec *deadline_ptr)
{
	const uint32_t unflagged_word = old_value;
	const uint32_t flagged_word = old_value | (1UL << 31);

	if (CK_CC_UNLIKELY(ck_ec32_value(ec) != old_value)) {
		return 0;
	}

	WAIT_SLOW_BODY(32, ec, ops, pred, data, deadline_ptr,
		       old_value, unflagged_word, flagged_word);
}

#ifdef CK_F_EC64
int
ck_ec64_wait_pred_slow(struct ck_ec64 *ec,
    const struct ck_ec_ops *ops,
    uint64_t old_value,
    int (*pred)(const struct ck_ec_wait_state *state,
	struct timespec *deadline),
    void *data,
    const struct timespec *deadline_ptr)
{
	const uint64_t unflagged_word = old_value << 1;
	const uint64_t flagged_word = unflagged_word | 1;

	if (CK_CC_UNLIKELY(ck_ec64_value(ec) != old_value)) {
		return 0;
	}

	WAIT_SLOW_BODY(64, ec, ops, pred, data, deadline_ptr,
		       old_value, unflagged_word, flagged_word);
}
#endif

#undef WAIT_SLOW_BODY
