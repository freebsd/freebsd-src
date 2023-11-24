#ifndef CK_EC_TIMEUTIL_H
#define CK_EC_TIMEUTIL_H
#include <ck_cc.h>
#include <ck_ec.h>
#include <ck_limits.h>
#include <ck_stdint.h>
#include <sys/time.h>

#define TIME_MAX ((time_t)((1ULL << ((sizeof(time_t) * CHAR_BIT) - 1)) - 1))
#define NSEC_MAX ((1000L * 1000 * 1000) - 1)

/*
 * Approximates (nsec * multiplier) >> shift. Clamps to UINT32_MAX on
 * overflow.
 */
CK_CC_UNUSED static uint32_t
wait_time_scale(uint32_t nsec,
		uint32_t multiplier,
		unsigned int shift)
{
	uint64_t temp = (uint64_t)nsec * multiplier;
	uint64_t max = (uint64_t)UINT32_MAX << shift;

	if (temp >= max) {
		return UINT32_MAX;
	}

	return temp >> shift;
}


/*
 * Returns ts + ns. ns is clamped to at most 1 second. Clamps the
 * return value to TIME_MAX, NSEC_MAX on overflow.
 *
 */
CK_CC_UNUSED static struct timespec timespec_add_ns(const struct timespec ts,
						    uint32_t ns)
{
	struct timespec ret = {
		.tv_sec = TIME_MAX,
		.tv_nsec = NSEC_MAX
	};
	time_t sec;
	uint32_t sum_ns;

	if (ns > (uint32_t)NSEC_MAX) {
		if (ts.tv_sec >= TIME_MAX) {
			return ret;
		}

		ret.tv_sec = ts.tv_sec + 1;
		ret.tv_nsec = ts.tv_nsec;
		return ret;
	}

	sec = ts.tv_sec;
	sum_ns = ns + ts.tv_nsec;
	if (sum_ns > NSEC_MAX) {
		if (sec >= TIME_MAX) {
			return ret;
		}

		sec++;
		sum_ns -= (NSEC_MAX + 1);
	}

	ret.tv_sec = sec;
	ret.tv_nsec = sum_ns;
	return ret;
}


/*
 * Returns ts + inc. If inc is negative, it is normalized to 0.
 * Clamps the return value to TIME_MAX, NSEC_MAX on overflow.
 */
CK_CC_UNUSED static struct timespec timespec_add(const struct timespec ts,
						 const struct timespec inc)
{
	/* Initial return value is clamped to infinite future. */
	struct timespec ret = {
		.tv_sec = TIME_MAX,
		.tv_nsec = NSEC_MAX
	};
	time_t sec;
	unsigned long nsec;

	/* Non-positive delta is a no-op. Invalid nsec is another no-op. */
	if (inc.tv_sec < 0 || inc.tv_nsec < 0 || inc.tv_nsec > NSEC_MAX) {
		return ts;
	}

	/* Detect overflow early. */
	if (inc.tv_sec > TIME_MAX - ts.tv_sec) {
		return ret;
	}

	sec = ts.tv_sec + inc.tv_sec;
	/* This sum can't overflow if the inputs are valid.*/
	nsec = (unsigned long)ts.tv_nsec + inc.tv_nsec;

	if (nsec > NSEC_MAX) {
		if (sec >= TIME_MAX) {
			return ret;
		}

		sec++;
		nsec -= (NSEC_MAX + 1);
	}

	ret.tv_sec = sec;
	ret.tv_nsec = nsec;
	return ret;
}

/* Compares two timespecs. Returns -1 if x < y, 0 if x == y, and 1 if x > y. */
CK_CC_UNUSED static int timespec_cmp(const struct timespec x,
				     const struct timespec y)
{
	if (x.tv_sec != y.tv_sec) {
		return (x.tv_sec < y.tv_sec) ? -1 : 1;
	}

	if (x.tv_nsec != y.tv_nsec) {
		return (x.tv_nsec < y.tv_nsec) ? -1 : 1;
	}

	return 0;
}

/*
 * Overwrites now with the current CLOCK_MONOTONIC time, and returns
 * true if the current time is greater than or equal to the deadline,
 * or the clock is somehow broken.
 */
CK_CC_UNUSED static bool check_deadline(struct timespec *now,
					const struct ck_ec_ops *ops,
					const struct timespec deadline)
{
	int r;

	r = ops->gettime(ops, now);
	if (r != 0) {
		return true;
	}

	return timespec_cmp(*now, deadline) >= 0;
}
#endif /* !CK_EC_TIMEUTIL_H */
