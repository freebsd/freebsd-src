/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2015 Mellanox Technologies, Ltd.
 * Copyright (c) 2014-2015 François Tigeot
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
#ifndef	_LINUX_KERNEL_H_
#define	_LINUX_KERNEL_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/libkern.h>
#include <sys/stat.h>
#include <sys/smp.h>
#include <sys/stddef.h>
#include <sys/syslog.h>

#include <linux/bitops.h>
#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/kthread.h>
#include <linux/types.h>
#include <linux/jiffies.h>
#include <linux/wait.h>
#include <linux/log2.h> 
#include <asm/byteorder.h>

#define KERN_CONT       ""
#define	KERN_EMERG	"<0>"
#define	KERN_ALERT	"<1>"
#define	KERN_CRIT	"<2>"
#define	KERN_ERR	"<3>"
#define	KERN_WARNING	"<4>"
#define	KERN_NOTICE	"<5>"
#define	KERN_INFO	"<6>"
#define	KERN_DEBUG	"<7>"

#define	BUILD_BUG_ON(x)		CTASSERT(!(x))

#define BUG()			panic("BUG")
#define BUG_ON(condition)	do { if (condition) BUG(); } while(0)
#define	WARN_ON			BUG_ON

#undef	ALIGN
#define	ALIGN(x, y)		roundup2((x), (y))
#undef PTR_ALIGN
#define	PTR_ALIGN(p, a)		((__typeof(p))ALIGN((uintptr_t)(p), (a)))
#define	DIV_ROUND_UP(x, n)	howmany(x, n)
#define	DIV_ROUND_UP_ULL(x, n)	DIV_ROUND_UP((unsigned long long)(x), (n))
#define	FIELD_SIZEOF(t, f)	sizeof(((t *)0)->f)

#define	printk(X...)		printf(X)

/*
 * The "pr_debug()" and "pr_devel()" macros should produce zero code
 * unless DEBUG is defined:
 */
#ifdef DEBUG
#define pr_debug(fmt, ...) \
        log(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define pr_devel(fmt, ...) \
	log(LOG_DEBUG, pr_fmt(fmt), ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...) \
        ({ if (0) log(LOG_DEBUG, fmt, ##__VA_ARGS__); 0; })
#define pr_devel(fmt, ...) \
	({ if (0) log(LOG_DEBUG, pr_fmt(fmt), ##__VA_ARGS__); 0; })
#endif

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

/*
 * Print a one-time message (analogous to WARN_ONCE() et al):
 */
#define printk_once(...) do {			\
	static bool __print_once;		\
						\
	if (!__print_once) {			\
		__print_once = true;		\
		printk(__VA_ARGS__);		\
	}					\
} while (0)

/*
 * Log a one-time message (analogous to WARN_ONCE() et al):
 */
#define log_once(level,...) do {		\
	static bool __log_once;			\
						\
	if (!__log_once) {			\
		__log_once = true;		\
		log(level, __VA_ARGS__);	\
	}					\
} while (0)

#define pr_emerg(fmt, ...) \
	log(LOG_EMERG, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_alert(fmt, ...) \
	log(LOG_ALERT, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_crit(fmt, ...) \
	log(LOG_CRIT, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_err(fmt, ...) \
	log(LOG_ERR, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warning(fmt, ...) \
	log(LOG_WARNING, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn pr_warning
#define pr_notice(fmt, ...) \
	log(LOG_NOTICE, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info(fmt, ...) \
	log(LOG_INFO, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info_once(fmt, ...) \
	log_once(LOG_INFO, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_cont(fmt, ...) \
	printk(KERN_CONT fmt, ##__VA_ARGS__)

#ifndef WARN
#define WARN(condition, format...) ({                                   \
        int __ret_warn_on = !!(condition);                              \
        if (unlikely(__ret_warn_on))                                    \
                pr_warning(format);                                     \
        unlikely(__ret_warn_on);                                        \
})
#endif

#define container_of(ptr, type, member)				\
({								\
	__typeof(((type *)0)->member) *_p = (ptr);		\
	(type *)((char *)_p - offsetof(type, member));		\
})
  
#define	ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

#define	simple_strtoul	strtoul
#define	simple_strtol	strtol
#define kstrtol(a,b,c) ({*(c) = strtol(a,0,b);})

#define min(x, y)	((x) < (y) ? (x) : (y))
#define max(x, y)	((x) > (y) ? (x) : (y))

#define min3(a, b, c)	min(a, min(b,c))
#define max3(a, b, c)	max(a, max(b,c))

#define min_t(type, _x, _y)	((type)(_x) < (type)(_y) ? (type)(_x) : (type)(_y))
#define max_t(type, _x, _y)	((type)(_x) > (type)(_y) ? (type)(_x) : (type)(_y))

#define clamp_t(type, _x, min, max)	min_t(type, max_t(type, _x, min), max)
#define clamp(x, lo, hi)		min( max(x,lo), hi)

/*
 * This looks more complex than it should be. But we need to
 * get the type for the ~ right in round_down (it needs to be
 * as wide as the result!), and we want to evaluate the macro
 * arguments just once each.
 */
#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)
#define round_down(x, y) ((x) & ~__round_mask(x, y))

#define	num_possible_cpus()	mp_ncpus
#define	num_online_cpus()	mp_ncpus

typedef struct pm_message {
        int event;
} pm_message_t;

/* Swap values of a and b */
#define swap(a, b) do {			\
	typeof(a) _swap_tmp = a;	\
	a = b;				\
	b = _swap_tmp;			\
} while (0)

#define	DIV_ROUND_CLOSEST(x, divisor)	(((x) + ((divisor) / 2)) / (divisor))

static inline uintmax_t
mult_frac(uintmax_t x, uintmax_t multiplier, uintmax_t divisor)
{
	uintmax_t q = (x / divisor);
	uintmax_t r = (x % divisor);

	return ((q * multiplier) + ((r * multiplier) / divisor));
}

static inline int64_t
abs64(int64_t x)
{
	return (x < 0 ? -x : x);
}

#endif	/* _LINUX_KERNEL_H_ */
