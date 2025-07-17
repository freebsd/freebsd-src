/*
 * timespecops.c -- calculations on 'struct timespec' values
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 *
 */

#include "config.h"

#include <sys/types.h>
#include <stdio.h>
#include <math.h>

#include "ntp.h"
#include "timetoa.h"
#include "timespecops.h"


/* nanoseconds per second */
#define NANOSECONDS 1000000000

/* conversion between l_fp fractions and nanoseconds */
#ifdef HAVE_U_INT64
# define FTOTVN(tsf)						\
	((int32)						\
	 (((u_int64)(tsf) * NANOSECONDS + 0x80000000) >> 32))
# define TVNTOF(tvu)						\
	((u_int32)						\
	 ((((u_int64)(tvu) << 32) + NANOSECONDS / 2) /		\
	  NANOSECONDS))
#else
# define NSECFRAC	(FRAC / NANOSECONDS)
# define FTOTVN(tsf)						\
	((int32)((tsf) / NSECFRAC + 0.5))
# define TVNTOF(tvu)						\
	((u_int32)((tvu) * NSECFRAC + 0.5))
#endif



/* make sure nanoseconds are in nominal range */
struct timespec
normalize_tspec(
	struct timespec x
	)
{
#if SIZEOF_LONG > 4
	long	z;

	/* 
	 * tv_nsec is of type 'long', and on a 64-bit machine using only
	 * loops becomes prohibitive once the upper 32 bits get
	 * involved. On the other hand, division by constant should be
	 * fast enough; so we do a division of the nanoseconds in that
	 * case. The floor adjustment step follows with the standard
	 * normalisation loops. And labs() is intentionally not used
	 * here: it has implementation-defined behaviour when applied
	 * to LONG_MIN.
	 */
	if (x.tv_nsec < -3l * NANOSECONDS ||
	    x.tv_nsec > 3l * NANOSECONDS) {
		z = x.tv_nsec / NANOSECONDS;
		x.tv_nsec -= z * NANOSECONDS;
		x.tv_sec += z;
	}
#endif
	/* since 10**9 is close to 2**32, we don't divide but do a
	 * normalisation in a loop; this takes 3 steps max, and should
	 * outperform a division even if the mul-by-inverse trick is
	 * employed. */
	if (x.tv_nsec < 0)
		do {
			x.tv_nsec += NANOSECONDS;
			x.tv_sec--;
		} while (x.tv_nsec < 0);
	else if (x.tv_nsec >= NANOSECONDS)
		do {
			x.tv_nsec -= NANOSECONDS;
			x.tv_sec++;
		} while (x.tv_nsec >= NANOSECONDS);

	return x;
}

/* x = abs(a) */
struct timespec
abs_tspec(
	struct timespec	a
	)
{
	struct timespec	c;

	c = normalize_tspec(a);
	if (c.tv_sec < 0) {
		if (c.tv_nsec != 0) {
			c.tv_sec = -c.tv_sec - 1;
			c.tv_nsec = NANOSECONDS - c.tv_nsec;
		} else {
			c.tv_sec = -c.tv_sec;
		}
	}

	return c;
}

/*
 * compare previously-normalised a and b
 * return 1 / 0 / -1 if a < / == / > b
 */
int
cmp_tspec(
	struct timespec a,
	struct timespec b
	)
{
	int r;

	r = (a.tv_sec > b.tv_sec) - (a.tv_sec < b.tv_sec);
	if (0 == r)
		r = (a.tv_nsec > b.tv_nsec) -
		    (a.tv_nsec < b.tv_nsec);
	
	return r;
}

/*
 * test previously-normalised a
 * return 1 / 0 / -1 if a < / == / > 0
 */
int
test_tspec(
	struct timespec	a
	)
{
	int		r;

	r = (a.tv_sec > 0) - (a.tv_sec < 0);
	if (r == 0)
		r = (a.tv_nsec > 0);
	
	return r;
}

/*
 *  convert to l_fp type, relative and absolute
 */

/* convert from timespec duration to l_fp duration */
l_fp
tspec_intv_to_lfp(
	struct timespec	x
	)
{
	struct timespec	v;
	l_fp		y;
	
	v = normalize_tspec(x);
	y.l_uf = TVNTOF(v.tv_nsec);
	y.l_i = (int32)v.tv_sec;

	return y;
}

/* convert from l_fp type, relative signed/unsigned and absolute */
struct timespec
lfp_intv_to_tspec(
	l_fp		x
	)
{
	struct timespec out;
	l_fp		absx;
	int		neg;
	
	neg = L_ISNEG(&x);
	absx = x;
	if (neg) {
		L_NEG(&absx);	
	}
	out.tv_nsec = FTOTVN(absx.l_uf);
	out.tv_sec = absx.l_i;
	if (neg) {
		out.tv_sec = -out.tv_sec;
		out.tv_nsec = -out.tv_nsec;
		out = normalize_tspec(out);
	}

	return out;
}

struct timespec
lfp_uintv_to_tspec(
	l_fp		x
	)
{
	struct timespec	out;
	
	out.tv_nsec = FTOTVN(x.l_uf);
	out.tv_sec = x.l_ui;

	return out;
}

/*
 * absolute (timestamp) conversion. Input is time in NTP epoch, output
 * is in UN*X epoch. The NTP time stamp will be expanded around the
 * pivot time *p or the current time, if p is NULL.
 */
struct timespec
lfp_stamp_to_tspec(
	l_fp		x,
	const time_t *	p
	)
{
	struct timespec	out;
	vint64		sec;

	sec = ntpcal_ntp_to_time(x.l_ui, p);
	out.tv_nsec = FTOTVN(x.l_uf);

	/* copying a vint64 to a time_t needs some care... */
#if SIZEOF_TIME_T <= 4
	out.tv_sec = (time_t)sec.d_s.lo;
#elif defined(HAVE_INT64)
	out.tv_sec = (time_t)sec.q_s;
#else
	out.tv_sec = ((time_t)sec.d_s.hi << 32) | sec.d_s.lo;
#endif
	
	return out;
}

/* -*-EOF-*- */
