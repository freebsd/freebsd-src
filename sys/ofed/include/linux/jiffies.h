/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
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
#ifndef	_LINUX_JIFFIES_H_
#define	_LINUX_JIFFIES_H_

#include <linux/types.h>
#include <linux/kernel.h>

#include <sys/time.h>
#include <sys/kernel.h>

static inline int
msecs_to_jiffies(int msec)
{
	struct timeval tv;

	tv.tv_sec = msec / 1000;
	tv.tv_usec = (msec % 1000) * 1000;
	return (tvtohz(&tv));
}


#define jiffies                 ticks
#define jiffies_to_msecs(x)     (((int64_t)(x)) * 1000 / hz)


#define	time_after(a, b)	((long)(b) - (long)(a) < 0)
#define	time_before(a, b)	time_after(b,a)
#define	time_after_eq(a, b)	((long)(a) - (long)(b) >= 0)
#define	time_before_eq(a, b)	time_after_eq(b, a)

#define	HZ	hz

#endif	/* _LINUX_JIFFIES_H_ */
