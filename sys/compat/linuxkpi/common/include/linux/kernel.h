/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2016 Mellanox Technologies, Ltd.
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
#ifndef	_LINUXKPI_LINUX_KERNEL_H_
#define	_LINUXKPI_LINUX_KERNEL_H_

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/libkern.h>
#include <sys/stat.h>
#include <sys/smp.h>
#include <sys/stddef.h>
#include <sys/syslog.h>
#include <sys/time.h>

#include <linux/bitops.h>
#include <linux/build_bug.h>
#include <linux/compiler.h>
#include <linux/container_of.h>
#include <linux/kstrtox.h>
#include <linux/limits.h>
#include <linux/math.h>
#include <linux/minmax.h>
#include <linux/stringify.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/typecheck.h>
#include <linux/jiffies.h>
#include <linux/log2.h>
#include <linux/kconfig.h>

#include <asm/byteorder.h>
#include <asm/cpufeature.h>
#include <asm/processor.h>
#include <asm/uaccess.h>

#include <linux/stdarg.h>

#define KERN_CONT       ""
#define	KERN_EMERG	"<0>"
#define	KERN_ALERT	"<1>"
#define	KERN_CRIT	"<2>"
#define	KERN_ERR	"<3>"
#define	KERN_WARNING	"<4>"
#define	KERN_NOTICE	"<5>"
#define	KERN_INFO	"<6>"
#define	KERN_DEBUG	"<7>"

#define	S8_C(x)  x
#define	U8_C(x)  x ## U
#define	S16_C(x) x
#define	U16_C(x) x ## U
#define	S32_C(x) x
#define	U32_C(x) x ## U
#define	S64_C(x) x ## LL
#define	U64_C(x) x ## ULL

#define	BUG()			panic("BUG at %s:%d", __FILE__, __LINE__)
#define	BUG_ON(cond)		do {				\
	if (cond) {						\
		panic("BUG ON %s failed at %s:%d",		\
		    __stringify(cond), __FILE__, __LINE__);	\
	}							\
} while (0)

extern int linuxkpi_warn_dump_stack;
#define	WARN_ON(cond) ({					\
	bool __ret = (cond);					\
	if (__ret) {						\
		printf("WARNING %s failed at %s:%d\n",		\
		    __stringify(cond), __FILE__, __LINE__);	\
		if (linuxkpi_warn_dump_stack)				\
			linux_dump_stack();				\
	}								\
	unlikely(__ret);						\
})

#define	WARN_ON_SMP(cond)	WARN_ON(cond)

#define	WARN_ON_ONCE(cond) ({					\
	static bool __warn_on_once;				\
	bool __ret = (cond);					\
	if (__ret && !__warn_on_once) {				\
		__warn_on_once = 1;				\
		printf("WARNING %s failed at %s:%d\n",		\
		    __stringify(cond), __FILE__, __LINE__);	\
		if (linuxkpi_warn_dump_stack)				\
			linux_dump_stack();				\
	}								\
	unlikely(__ret);						\
})

#define	oops_in_progress	SCHEDULER_STOPPED()

#undef	ALIGN
#define	ALIGN(x, y)		roundup2((x), (y))
#define	ALIGN_DOWN(x, y)	rounddown2(x, y)
#undef PTR_ALIGN
#define	PTR_ALIGN(p, a)		((__typeof(p))ALIGN((uintptr_t)(p), (a)))
#define	IS_ALIGNED(x, a)	(((x) & ((__typeof(x))(a) - 1)) == 0)
#define	__KERNEL_DIV_ROUND_UP(x, n)	howmany(x, n)
#define	FIELD_SIZEOF(t, f)	sizeof(((t *)0)->f)

#define	printk(...)		printf(__VA_ARGS__)
#define	vprintk(f, a)		vprintf(f, a)

#define PTR_IF(x, p)		((x) ? (p) : NULL)

#define	asm			__asm

extern void linux_dump_stack(void);
#define	dump_stack()		linux_dump_stack()

struct va_format {
	const char *fmt;
	va_list *va;
};

static inline int
vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	ssize_t ssize = size;
	int i;

	i = vsnprintf(buf, size, fmt, args);

	return ((i >= ssize) ? (ssize - 1) : i);
}

static inline int
scnprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vscnprintf(buf, size, fmt, args);
	va_end(args);

	return (i);
}

/*
 * The "pr_debug()" and "pr_devel()" macros should produce zero code
 * unless DEBUG is defined:
 */
#ifdef DEBUG
extern int linuxkpi_debug;
#define pr_debug(fmt, ...)					\
	do {							\
		if (linuxkpi_debug)				\
			log(LOG_DEBUG, fmt, ##__VA_ARGS__);	\
	} while (0)
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
	if (unlikely(!__log_once)) {		\
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
#define pr_err_once(fmt, ...) \
	log_once(LOG_ERR, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warning(fmt, ...) \
	log(LOG_WARNING, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn(...) \
	pr_warning(__VA_ARGS__)
#define pr_warn_once(fmt, ...) \
	log_once(LOG_WARNING, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_notice(fmt, ...) \
	log(LOG_NOTICE, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info(fmt, ...) \
	log(LOG_INFO, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info_once(fmt, ...) \
	log_once(LOG_INFO, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_cont(fmt, ...) \
	printk(KERN_CONT fmt, ##__VA_ARGS__)
#define	pr_warn_ratelimited(...) do {		\
	static linux_ratelimit_t __ratelimited;	\
	if (linux_ratelimited(&__ratelimited))	\
		pr_warning(__VA_ARGS__);	\
} while (0)

#ifndef WARN
#define	WARN(condition, ...) ({			\
	bool __ret_warn_on = (condition);	\
	if (unlikely(__ret_warn_on))		\
		pr_warning(__VA_ARGS__);	\
	unlikely(__ret_warn_on);		\
})
#endif

#ifndef WARN_ONCE
#define	WARN_ONCE(condition, ...) ({		\
	bool __ret_warn_on = (condition);	\
	if (unlikely(__ret_warn_on))		\
		pr_warn_once(__VA_ARGS__);	\
	unlikely(__ret_warn_on);		\
})
#endif

#define	ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

#define	u64_to_user_ptr(val)	((void *)(uintptr_t)(val))

#define _RET_IP_		__builtin_return_address(0)

#define offsetofend(t, m)	\
        (offsetof(t, m) + sizeof((((t *)0)->m)))

#define	smp_processor_id()	PCPU_GET(cpuid)
#define	num_possible_cpus()	mp_ncpus
#define	num_online_cpus()	mp_ncpus

#if defined(__i386__) || defined(__amd64__)
extern bool linux_cpu_has_clflush;
#define	cpu_has_clflush		linux_cpu_has_clflush
#endif

typedef struct linux_ratelimit {
	struct timeval lasttime;
	int counter;
} linux_ratelimit_t;

static inline bool
linux_ratelimited(linux_ratelimit_t *rl)
{
	return (ppsratecheck(&rl->lasttime, &rl->counter, 1));
}

#define	__is_constexpr(x) \
	__builtin_constant_p(x)

/*
 * The is_signed() macro below returns true if the passed data type is
 * signed. Else false is returned.
 */
#define	is_signed(datatype) (((datatype)-1 / (datatype)2) == (datatype)0)

#define	TAINT_WARN	0
#define	test_taint(x)	(0)
#define	add_taint(x,y)	do {	\
	} while (0)

static inline int
_h2b(const char c)
{

	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'a' && c <= 'f')
		return (10 + c - 'a');
	if (c >= 'A' && c <= 'F')
		return (10 + c - 'A');
	return (-EINVAL);
}

static inline int
hex2bin(uint8_t *bindst, const char *hexsrc, size_t binlen)
{
	int hi4, lo4;

	while (binlen > 0) {
		hi4 = _h2b(*hexsrc++);
		lo4 = _h2b(*hexsrc++);
		if (hi4 < 0 || lo4 < 0)
			return (-EINVAL);

		*bindst++ = (hi4 << 4) | lo4;
		binlen--;
	}

	return (0);
}

static inline bool
mac_pton(const char *macin, uint8_t *macout)
{
	const char *s, *d;
	uint8_t mac[6], hx, lx;
	int i;

	if (strlen(macin) < (3 * 6 - 1))
		return (false);

	i = 0;
	s = macin;
	do {
		/* Should we also support '-'-delimiters? */
		d = strchrnul(s, ':');
		hx = lx = 0;
		while (s < d) {
			/* Fail on abc:123:xxx:... */
			if ((d - s) > 2)
				return (false);
			/* We do support non-well-formed strings: 3:45:6:... */
			if ((d - s) > 1) {
				hx = _h2b(*s);
				if (hx < 0)
					return (false);
				s++;
			}
			lx = _h2b(*s);
			if (lx < 0)
				return (false);
			s++;
		}
		mac[i] = (hx << 4) | lx;
		i++;
		if (i >= 6)
			return (false);
	} while (d != NULL && *d != '\0');

	memcpy(macout, mac, 6);
	return (true);
}

#define	DECLARE_FLEX_ARRAY(_t, _n)					\
    struct { struct { } __dummy_ ## _n; _t _n[0]; }

#endif	/* _LINUXKPI_LINUX_KERNEL_H_ */
