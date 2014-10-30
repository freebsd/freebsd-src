/*-
 * Copyright (c) 2014 Mellanox Technologies, Ltd.
 * All rights reserved.
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

#ifndef _LINUX_KTIME_H
#define _LINUX_KTIME_H

#include <sys/time.h>
#include <linux/types.h>
#include <linux/jiffies.h>


/* Get the monotonic time in timespec format: */ 
#define ktime_get_ts getnanouptime

#define NSEC_PER_USEC   1000L
#define NSEC_PER_SEC    1000000000L

/*
 * ktime_t:
 *
 * On 64-bit CPUs a single 64-bit variable is used to store the hrtimers
 * internal representation of time values in scalar nanoseconds. The
 * design plays out best on 64-bit CPUs, where most conversions are
 * NOPs and most arithmetic ktime_t operations are plain arithmetic
 * operations.
 *
 * On 32-bit CPUs an optimized representation of the timespec structure
 * is used to avoid expensive conversions from and to timespecs. The
 * endian-aware order of the tv struct members is chosen to allow
 * mathematical operations on the tv64 member of the union too, which
 * for certain operations produces better code.
 *
 * For architectures with efficient support for 64/32-bit conversions the
 * plain scalar nanosecond based representation can be selected by the
 * config switch CONFIG_KTIME_SCALAR.
 */
union ktime {
	s64	tv64;
#if BITS_PER_LONG != 64 && !defined(CONFIG_KTIME_SCALAR)
	struct {
# ifdef __BIG_ENDIAN
	s32	sec, nsec;
# else
	s32	nsec, sec;
# endif
	} tv;
#endif
};

typedef union ktime ktime_t;		/* Kill this */

#define KTIME_MAX                       ((s64)~((u64)1 << 63))
#define KTIME_SEC_MAX                   (KTIME_MAX / NSEC_PER_SEC)

/*
 * ktime_t definitions when using the 64-bit scalar representation:
 */

#if (BITS_PER_LONG == 64) || defined(CONFIG_KTIME_SCALAR)

/**
 * ktime_set - Set a ktime_t variable from a seconds/nanoseconds value
 * @secs:	seconds to set
 * @nsecs:	nanoseconds to set
 *
 * Return the ktime_t representation of the value
 */
static inline ktime_t ktime_set(const long secs, const unsigned long nsecs)
{
#if (BITS_PER_LONG == 64)
	if (unlikely(secs >= KTIME_SEC_MAX))
		return (ktime_t){ .tv64 = KTIME_MAX };
#endif
	return (ktime_t) { .tv64 = (s64)secs * NSEC_PER_SEC + (s64)nsecs };
}

/* Subtract two ktime_t variables. rem = lhs -rhs: */
#define ktime_sub(lhs, rhs) \
		({ (ktime_t){ .tv64 = (lhs).tv64 - (rhs).tv64 }; })

/* Add two ktime_t variables. res = lhs + rhs: */
#define ktime_add(lhs, rhs) \
		({ (ktime_t){ .tv64 = (lhs).tv64 + (rhs).tv64 }; })

/*
 * Add a ktime_t variable and a scalar nanosecond value.
 * res = kt + nsval:
 */
#define ktime_add_ns(kt, nsval) \
		({ (ktime_t){ .tv64 = (kt).tv64 + (nsval) }; })

/*
 * Subtract a scalar nanosecod from a ktime_t variable
 * res = kt - nsval:
 */
#define ktime_sub_ns(kt, nsval) \
		({ (ktime_t){ .tv64 = (kt).tv64 - (nsval) }; })

/* convert a timespec to ktime_t format: */
static inline ktime_t timespec_to_ktime(struct timespec ts)
{
	return ktime_set(ts.tv_sec, ts.tv_nsec);
}

/* convert a timeval to ktime_t format: */
static inline ktime_t timeval_to_ktime(struct timeval tv)
{
	return ktime_set(tv.tv_sec, tv.tv_usec * NSEC_PER_USEC);
}

/* Map the ktime_t to timespec conversion to ns_to_timespec function */
#define ktime_to_timespec(kt)		ns_to_timespec((kt).tv64)

/* Map the ktime_t to timeval conversion to ns_to_timeval function */
#define ktime_to_timeval(kt)		ns_to_timeval((kt).tv64)

/* Convert ktime_t to nanoseconds - NOP in the scalar storage format: */
#define ktime_to_ns(kt)			((kt).tv64)

#else	/* !((BITS_PER_LONG == 64) || defined(CONFIG_KTIME_SCALAR)) */

/*
 * Helper macros/inlines to get the ktime_t math right in the timespec
 * representation. The macros are sometimes ugly - their actual use is
 * pretty okay-ish, given the circumstances. We do all this for
 * performance reasons. The pure scalar nsec_t based code was nice and
 * simple, but created too many 64-bit / 32-bit conversions and divisions.
 *
 * Be especially aware that negative values are represented in a way
 * that the tv.sec field is negative and the tv.nsec field is greater
 * or equal to zero but less than nanoseconds per second. This is the
 * same representation which is used by timespecs.
 *
 *   tv.sec < 0 and 0 >= tv.nsec < NSEC_PER_SEC
 */

/* Set a ktime_t variable to a value in sec/nsec representation: */
static inline ktime_t ktime_set(const long secs, const unsigned long nsecs)
{
	return (ktime_t) { .tv = { .sec = secs, .nsec = nsecs } };
}

/**
 * ktime_sub - subtract two ktime_t variables
 * @lhs:	minuend
 * @rhs:	subtrahend
 *
 * Returns the remainder of the subtraction
 */
static inline ktime_t ktime_sub(const ktime_t lhs, const ktime_t rhs)
{
	ktime_t res;

	res.tv64 = lhs.tv64 - rhs.tv64;
	if (res.tv.nsec < 0)
		res.tv.nsec += NSEC_PER_SEC;

	return res;
}

/**
 * ktime_add - add two ktime_t variables
 * @add1:	addend1
 * @add2:	addend2
 *
 * Returns the sum of @add1 and @add2.
 */
static inline ktime_t ktime_add(const ktime_t add1, const ktime_t add2)
{
	ktime_t res;

	res.tv64 = add1.tv64 + add2.tv64;
	/*
	 * performance trick: the (u32) -NSEC gives 0x00000000Fxxxxxxx
	 * so we subtract NSEC_PER_SEC and add 1 to the upper 32 bit.
	 *
	 * it's equivalent to:
	 *   tv.nsec -= NSEC_PER_SEC
	 *   tv.sec ++;
	 */
	if (res.tv.nsec >= NSEC_PER_SEC)
		res.tv64 += (u32)-NSEC_PER_SEC;

	return res;
}

/**
 * ktime_add_ns - Add a scalar nanoseconds value to a ktime_t variable
 * @kt:		addend
 * @nsec:	the scalar nsec value to add
 *
 * Returns the sum of @kt and @nsec in ktime_t format
 */
extern ktime_t ktime_add_ns(const ktime_t kt, u64 nsec);

/**
 * ktime_sub_ns - Subtract a scalar nanoseconds value from a ktime_t variable
 * @kt:		minuend
 * @nsec:	the scalar nsec value to subtract
 *
 * Returns the subtraction of @nsec from @kt in ktime_t format
 */
extern ktime_t ktime_sub_ns(const ktime_t kt, u64 nsec);

/**
 * timespec_to_ktime - convert a timespec to ktime_t format
 * @ts:		the timespec variable to convert
 *
 * Returns a ktime_t variable with the converted timespec value
 */
static inline ktime_t timespec_to_ktime(const struct timespec ts)
{
	return (ktime_t) { .tv = { .sec = (s32)ts.tv_sec,
			   	   .nsec = (s32)ts.tv_nsec } };
}

/**
 * timeval_to_ktime - convert a timeval to ktime_t format
 * @tv:		the timeval variable to convert
 *
 * Returns a ktime_t variable with the converted timeval value
 */
static inline ktime_t timeval_to_ktime(const struct timeval tv)
{
	return (ktime_t) { .tv = { .sec = (s32)tv.tv_sec,
				   .nsec = (s32)(tv.tv_usec *
						 NSEC_PER_USEC) } };
}

/**
 * ktime_to_timespec - convert a ktime_t variable to timespec format
 * @kt:		the ktime_t variable to convert
 *
 * Returns the timespec representation of the ktime value
 */
static inline struct timespec ktime_to_timespec(const ktime_t kt)
{
	return (struct timespec) { .tv_sec = (time_t) kt.tv.sec,
				   .tv_nsec = (long) kt.tv.nsec };
}

/**
 * ktime_to_timeval - convert a ktime_t variable to timeval format
 * @kt:		the ktime_t variable to convert
 *
 * Returns the timeval representation of the ktime value
 */
static inline struct timeval ktime_to_timeval(const ktime_t kt)
{
	return (struct timeval) {
		.tv_sec = (time_t) kt.tv.sec,
		.tv_usec = (suseconds_t) (kt.tv.nsec / NSEC_PER_USEC) };
}

/**
 * ktime_to_ns - convert a ktime_t variable to scalar nanoseconds
 * @kt:		the ktime_t variable to convert
 *
 * Returns the scalar nanoseconds representation of @kt
 */
static inline s64 ktime_to_ns(const ktime_t kt)
{
	return (s64) kt.tv.sec * NSEC_PER_SEC + kt.tv.nsec;
}

#endif	/* !((BITS_PER_LONG == 64) || defined(CONFIG_KTIME_SCALAR)) */

#endif	/* _LINUX_KTIME_H */
