/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2016 Mellanox Technologies, Ltd.
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
#ifndef	_LINUXKPI_LINUX_JIFFIES_H_
#define	_LINUXKPI_LINUX_JIFFIES_H_

#include <linux/types.h>
#include <linux/time.h>

#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/limits.h>

#define	jiffies			ticks
#define	jiffies_64		ticks
#define	jiffies_to_msecs(x)     ((unsigned int)(((int64_t)(int)(x)) * 1000 / hz))

#define	MAX_JIFFY_OFFSET	((INT_MAX >> 1) - 1)

#define	time_after(a, b)	((int)((b) - (a)) < 0)
#define	time_after32(a, b)	((int32_t)((uint32_t)(b) - (uint32_t)(a)) < 0)
#define	time_before(a, b)	time_after(b,a)
#define	time_before32(a, b)	time_after32(b, a)
#define	time_after_eq(a, b)	((int)((a) - (b)) >= 0)
#define	time_before_eq(a, b)	time_after_eq(b, a)
#define	time_in_range(a,b,c)	\
	(time_after_eq(a,b) && time_before_eq(a,c))
#define	time_is_after_eq_jiffies(a) time_after_eq(a, jiffies)
#define	time_is_after_jiffies(a) time_after(a, jiffies)
#define	time_is_before_jiffies(a)	time_before(a, jiffies)

#define	HZ	hz

extern uint64_t lkpi_nsec2hz_rem;
extern uint64_t lkpi_nsec2hz_div;
extern uint64_t lkpi_nsec2hz_max;

extern uint64_t lkpi_usec2hz_rem;
extern uint64_t lkpi_usec2hz_div;
extern uint64_t lkpi_usec2hz_max;

extern uint64_t lkpi_msec2hz_rem;
extern uint64_t lkpi_msec2hz_div;
extern uint64_t lkpi_msec2hz_max;

static inline int
msecs_to_jiffies(uint64_t msec)
{
	uint64_t result;

	if (msec > lkpi_msec2hz_max)
		msec = lkpi_msec2hz_max;
	result = howmany(msec * lkpi_msec2hz_rem, lkpi_msec2hz_div);
	if (result > MAX_JIFFY_OFFSET)
		result = MAX_JIFFY_OFFSET;

	return ((int)result);
}

static inline int
usecs_to_jiffies(uint64_t usec)
{
	uint64_t result;

	if (usec > lkpi_usec2hz_max)
		usec = lkpi_usec2hz_max;
	result = howmany(usec * lkpi_usec2hz_rem, lkpi_usec2hz_div);
	if (result > MAX_JIFFY_OFFSET)
		result = MAX_JIFFY_OFFSET;

	return ((int)result);
}

static inline uint64_t
nsecs_to_jiffies64(uint64_t nsec)
{

	if (nsec > lkpi_nsec2hz_max)
		nsec = lkpi_nsec2hz_max;
	return (howmany(nsec * lkpi_nsec2hz_rem, lkpi_nsec2hz_div));
}

static inline unsigned long
nsecs_to_jiffies(uint64_t nsec)
{

	if (sizeof(unsigned long) >= sizeof(uint64_t)) {
		if (nsec > lkpi_nsec2hz_max)
			nsec = lkpi_nsec2hz_max;
	} else {
		if (nsec > (lkpi_nsec2hz_max >> 32))
			nsec = (lkpi_nsec2hz_max >> 32);
	}
	return (howmany(nsec * lkpi_nsec2hz_rem, lkpi_nsec2hz_div));
}

static inline uint64_t
jiffies_to_nsecs(int j)
{

	return ((1000000000ULL / hz) * (uint64_t)(unsigned int)j);
}

static inline uint64_t
jiffies_to_usecs(int j)
{

	return ((1000000ULL / hz) * (uint64_t)(unsigned int)j);
}

static inline uint64_t
get_jiffies_64(void)
{

	return ((uint64_t)(unsigned int)ticks);
}

static inline int
linux_timer_jiffies_until(int expires)
{
	int delta = expires - jiffies;
	/* guard against already expired values */
	if (delta < 1)
		delta = 1;
	return (delta);
}

#endif	/* _LINUXKPI_LINUX_JIFFIES_H_ */
