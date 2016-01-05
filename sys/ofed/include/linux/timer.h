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
#ifndef _LINUX_TIMER_H_
#define	_LINUX_TIMER_H_

#include <linux/types.h>

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/callout.h>

struct timer_list {
	struct callout timer_callout;
	void    (*function) (unsigned long);
	unsigned long data;
	unsigned long expires;
};

extern unsigned long linux_timer_hz_mask;

#define	setup_timer(timer, func, dat)					\
do {									\
	(timer)->function = (func);					\
	(timer)->data = (dat);						\
	callout_init(&(timer)->timer_callout, 1);			\
} while (0)

#define	init_timer(timer)						\
do {									\
	(timer)->function = NULL;					\
	(timer)->data = 0;						\
	callout_init(&(timer)->timer_callout, 1);			\
} while (0)

extern void mod_timer(struct timer_list *, unsigned long);
extern void add_timer(struct timer_list *);

#define	del_timer(timer)	callout_stop(&(timer)->timer_callout)
#define	del_timer_sync(timer)	callout_drain(&(timer)->timer_callout)
#define	timer_pending(timer)	callout_pending(&(timer)->timer_callout)
#define	round_jiffies(j) \
	((unsigned long)(((j) + linux_timer_hz_mask) & ~linux_timer_hz_mask))
#define	round_jiffies_relative(j) \
	round_jiffies(j)

#endif					/* _LINUX_TIMER_H_ */
