/*
 * timespecops.h -- calculations on 'struct timespec' values
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 *
 * Rationale
 * ---------
 *
 * Doing basic arithmetic on a 'struct timespec' is not exceedingly
 * hard, but it requires tedious and repetitive code to keep the result
 * normalised. We consider a timespec normalised when the nanosecond
 * fraction is in the interval [0 .. 10^9[ ; there are multiple value
 * pairs of seconds and nanoseconds that denote the same time interval,
 * but the normalised representation is unique. No two different
 * intervals can have the same normalised representation.
 *
 * Another topic is the representation of negative time intervals.
 * There's more than one way to this, since both the seconds and the
 * nanoseconds of a timespec are signed values. IMHO, the easiest way is
 * to use a complement representation where the nanoseconds are still
 * normalised, no matter what the sign of the seconds value. This makes
 * normalisation easier, since the sign of the integer part is
 * irrelevant, and it removes several sign decision cases during the
 * calculations.
 *
 * As long as no signed integer overflow can occur with the nanosecond
 * part of the operands, all operations work as expected and produce a
 * normalised result.
 *
 * The exception to this are functions fix a '_fast' suffix, which do no
 * normalisation on input data and therefore expect the input data to be
 * normalised.
 *
 * Input and output operands may overlap; all input is consumed before
 * the output is written to.
 */
#ifndef TIMESPECOPS_H
#define TIMESPECOPS_H

#include <sys/types.h>
#include <stdio.h>
#include <math.h>

#include "ntp.h"
#include "timetoa.h"


/* nanoseconds per second */
#define NANOSECONDS 1000000000

/* predicate: returns TRUE if the nanoseconds are in nominal range */
#define timespec_isnormal(x) \
	((x)->tv_nsec >= 0 && (x)->tv_nsec < NANOSECONDS)

/* predicate: returns TRUE if the nanoseconds are out-of-bounds */
#define timespec_isdenormal(x)	(!timespec_isnormal(x))




/* make sure nanoseconds are in nominal range */
extern struct timespec normalize_tspec(struct timespec x);

/* x = a + b */
static inline struct timespec
add_tspec(
	struct timespec	a,
	struct timespec	b
	)
{
	struct timespec	x;

	x = a;
	x.tv_sec += b.tv_sec;
	x.tv_nsec += b.tv_nsec;

	return normalize_tspec(x);
}

/* x = a + b, b is fraction only */
static inline struct timespec
add_tspec_ns(
	struct timespec	a,
	long		b
	)
{
	struct timespec x;

	x = a;
	x.tv_nsec += b;

	return normalize_tspec(x);
}

/* x = a - b */
static inline struct timespec
sub_tspec(
	struct timespec	a,
	struct timespec	b
	)
{	
	struct timespec x;

	x = a;
	x.tv_sec -= b.tv_sec;
	x.tv_nsec -= b.tv_nsec;

	return normalize_tspec(x);
}

/* x = a - b, b is fraction only */
static inline struct timespec
sub_tspec_ns(
	struct timespec	a,
	long		b
	)
{
	struct timespec	x;

	x = a;
	x.tv_nsec -= b;

	return normalize_tspec(x);
}

/* x = -a */
static inline struct timespec
neg_tspec(
	struct timespec	a
	)
{	
	struct timespec	x;

	x.tv_sec = -a.tv_sec;
	x.tv_nsec = -a.tv_nsec;

	return normalize_tspec(x);
}

/* x = abs(a) */
struct timespec abs_tspec(struct timespec a);

/*
 * compare previously-normalised a and b
 * return 1 / 0 / -1 if a < / == / > b
 */
extern int cmp_tspec(struct timespec a,	struct timespec b);

/*
 * compare possibly-denormal a and b
 * return 1 / 0 / -1 if a < / == / > b
 */
static inline int
cmp_tspec_denorm(
	struct timespec	a,
	struct timespec	b
	)
{
	return cmp_tspec(normalize_tspec(a), normalize_tspec(b));
}

/*
 * test previously-normalised a
 * return 1 / 0 / -1 if a < / == / > 0
 */
extern int test_tspec(struct timespec a);

/*
 * test possibly-denormal a
 * return 1 / 0 / -1 if a < / == / > 0
 */
static inline int
test_tspec_denorm(
	struct timespec	a
	)
{
	return test_tspec(normalize_tspec(a));
}

/* return LIB buffer ptr to string rep */
static inline const char *
tspectoa(
	struct timespec	x
	)
{
	return format_time_fraction(x.tv_sec, x.tv_nsec, 9);
}

/*
 *  convert to l_fp type, relative and absolute
 */

/* convert from timespec duration to l_fp duration */
extern l_fp tspec_intv_to_lfp(struct timespec x);

/* x must be UN*X epoch, output will be in NTP epoch */
static inline l_fp
tspec_stamp_to_lfp(
	struct timespec	x
	)
{
	l_fp		y;

	y = tspec_intv_to_lfp(x);
	y.l_ui += JAN_1970;

	return y;
}

/* convert from l_fp type, relative signed/unsigned and absolute */
extern struct timespec lfp_intv_to_tspec(l_fp x);
extern struct timespec lfp_uintv_to_tspec(l_fp x);

/*
 * absolute (timestamp) conversion. Input is time in NTP epoch, output
 * is in UN*X epoch. The NTP time stamp will be expanded around the
 * pivot time *p or the current time, if p is NULL.
 */
extern struct timespec lfp_stamp_to_tspec(l_fp x, const time_t *pivot);

#endif	/* TIMESPECOPS_H */
