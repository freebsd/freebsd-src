/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
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

#include <sys/systm.h>
#include <sys/param.h>
#include <sys/libkern.h>
#include <sys/stat.h>
#include <sys/smp.h>

#include <linux/bitops.h>
#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/stddef.h>
#include <linux/kthread.h>
#include <linux/types.h>
#include <linux/jiffies.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/notifier.h>
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

#define BUG()			panic("BUG")
#define BUG_ON(condition)	do { if (condition) BUG(); } while(0)
#define	WARN_ON			BUG_ON

#undef	ALIGN
#define	ALIGN(x, y)		roundup2((x), (y))
#define	DIV_ROUND_UP		howmany

#define	printk(X...)		printf(X)
#define	pr_debug(fmt, ...)	printk(KERN_DEBUG # fmt, ##__VA_ARGS__)
#define udelay(t)       	DELAY(t)

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

/*
 * Print a one-time message (analogous to WARN_ONCE() et al):
 */
#define printk_once(x...) ({                    \
        static bool __print_once;               \
                                                \
        if (!__print_once) {                    \
                __print_once = true;            \
                printk(x);                      \
        }                                       \
})



#define pr_emerg(fmt, ...) \
        printk(KERN_EMERG pr_fmt(fmt), ##__VA_ARGS__)
#define pr_alert(fmt, ...) \
        printk(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__)
#define pr_crit(fmt, ...) \
        printk(KERN_CRIT pr_fmt(fmt), ##__VA_ARGS__)
#define pr_err(fmt, ...) \
        printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warning(fmt, ...) \
        printk(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn pr_warning
#define pr_notice(fmt, ...) \
        printk(KERN_NOTICE pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info(fmt, ...) \
        printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#define pr_cont(fmt, ...) \
        printk(KERN_CONT fmt, ##__VA_ARGS__)

/* pr_devel() should produce zero code unless DEBUG is defined */
#ifdef DEBUG
#define pr_devel(fmt, ...) \
        printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#else
#define pr_devel(fmt, ...) \
        ({ if (0) printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__); 0; })
#endif

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

#define min(x, y)	(x < y ? x : y)
#define max(x, y)	(x > y ? x : y)
#define min_t(type, _x, _y)	(type)(_x) < (type)(_y) ? (type)(_x) : (_y)
#define max_t(type, _x, _y)	(type)(_x) > (type)(_y) ? (type)(_x) : (_y)

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

typedef struct pm_message {
        int event;
} pm_message_t;

#endif	/* _LINUX_KERNEL_H_ */
