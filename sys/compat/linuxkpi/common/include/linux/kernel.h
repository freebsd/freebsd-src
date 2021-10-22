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
 *
 * $FreeBSD$
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
#include <sys/time.h>

#include <linux/bitops.h>
#include <linux/compiler.h>
#include <linux/stringify.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/jiffies.h>
#include <linux/log2.h>

#include <asm/byteorder.h>
#include <asm/uaccess.h>

#include <machine/stdarg.h>

#define KERN_CONT       ""
#define	KERN_EMERG	"<0>"
#define	KERN_ALERT	"<1>"
#define	KERN_CRIT	"<2>"
#define	KERN_ERR	"<3>"
#define	KERN_WARNING	"<4>"
#define	KERN_NOTICE	"<5>"
#define	KERN_INFO	"<6>"
#define	KERN_DEBUG	"<7>"

#define	U8_MAX		((u8)~0U)
#define	S8_MAX		((s8)(U8_MAX >> 1))
#define	S8_MIN		((s8)(-S8_MAX - 1))
#define	U16_MAX		((u16)~0U)
#define	S16_MAX		((s16)(U16_MAX >> 1))
#define	S16_MIN		((s16)(-S16_MAX - 1))
#define	U32_MAX		((u32)~0U)
#define	S32_MAX		((s32)(U32_MAX >> 1))
#define	S32_MIN		((s32)(-S32_MAX - 1))
#define	U64_MAX		((u64)~0ULL)
#define	S64_MAX		((s64)(U64_MAX >> 1))
#define	S64_MIN		((s64)(-S64_MAX - 1))

#define	S8_C(x)  x
#define	U8_C(x)  x ## U
#define	S16_C(x) x
#define	U16_C(x) x ## U
#define	S32_C(x) x
#define	U32_C(x) x ## U
#define	S64_C(x) x ## LL
#define	U64_C(x) x ## ULL

/*
 * BUILD_BUG_ON() can happen inside functions where _Static_assert() does not
 * seem to work.  Use old-schoold-ish CTASSERT from before commit
 * a3085588a88fa58eb5b1eaae471999e1995a29cf but also make sure we do not
 * end up with an unused typedef or variable. The compiler should optimise
 * it away entirely.
 */
#define	_O_CTASSERT(x)		_O__CTASSERT(x, __LINE__)
#define	_O__CTASSERT(x, y)	_O___CTASSERT(x, y)
#define	_O___CTASSERT(x, y)	while (0) { \
    typedef char __assert_line_ ## y[(x) ? 1 : -1]; \
    __assert_line_ ## y _x; \
    _x[0] = '\0'; \
}

#define	BUILD_BUG()			do { CTASSERT(0); } while (0)
#define	BUILD_BUG_ON(x)			do { _O_CTASSERT(!(x)) } while (0)
#define	BUILD_BUG_ON_MSG(x, msg)	BUILD_BUG_ON(x)
#define	BUILD_BUG_ON_NOT_POWER_OF_2(x)	BUILD_BUG_ON(!powerof2(x))
#define	BUILD_BUG_ON_INVALID(expr)	while (0) { (void)(expr); }

extern const volatile int lkpi_build_bug_on_zero;
#define	BUILD_BUG_ON_ZERO(x)	((x) ? lkpi_build_bug_on_zero : 0)

#define	BUG()			panic("BUG at %s:%d", __FILE__, __LINE__)
#define	BUG_ON(cond)		do {				\
	if (cond) {						\
		panic("BUG ON %s failed at %s:%d",		\
		    __stringify(cond), __FILE__, __LINE__);	\
	}							\
} while (0)

#define	WARN_ON(cond) ({					\
      bool __ret = (cond);					\
      if (__ret) {						\
		printf("WARNING %s failed at %s:%d\n",		\
		    __stringify(cond), __FILE__, __LINE__);	\
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
		linux_dump_stack();				\
      }								\
      unlikely(__ret);						\
})

#define	oops_in_progress	SCHEDULER_STOPPED()

#undef	ALIGN
#define	ALIGN(x, y)		roundup2((x), (y))
#undef PTR_ALIGN
#define	PTR_ALIGN(p, a)		((__typeof(p))ALIGN((uintptr_t)(p), (a)))
#define	IS_ALIGNED(x, a)	(((x) & ((__typeof(x))(a) - 1)) == 0)
#define	DIV_ROUND_UP(x, n)	howmany(x, n)
#define	__KERNEL_DIV_ROUND_UP(x, n)	howmany(x, n)
#define	DIV_ROUND_UP_ULL(x, n)	DIV_ROUND_UP((unsigned long long)(x), (n))
#define	DIV_ROUND_DOWN_ULL(x, n) (((unsigned long long)(x) / (n)) * (n))
#define	FIELD_SIZEOF(t, f)	sizeof(((t *)0)->f)

#define	printk(...)		printf(__VA_ARGS__)
#define	vprintk(f, a)		vprintf(f, a)

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

#define container_of(ptr, type, member)				\
({								\
	const __typeof(((type *)0)->member) *__p = (ptr);	\
	(type *)((uintptr_t)__p - offsetof(type, member));	\
})

#define	ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

#define	u64_to_user_ptr(val)	((void *)(uintptr_t)(val))

#define _RET_IP_		__builtin_return_address(0)

static inline unsigned long long
simple_strtoull(const char *cp, char **endp, unsigned int base)
{
	return (strtouq(cp, endp, base));
}

static inline long long
simple_strtoll(const char *cp, char **endp, unsigned int base)
{
	return (strtoq(cp, endp, base));
}

static inline unsigned long
simple_strtoul(const char *cp, char **endp, unsigned int base)
{
	return (strtoul(cp, endp, base));
}

static inline long
simple_strtol(const char *cp, char **endp, unsigned int base)
{
	return (strtol(cp, endp, base));
}

static inline int
kstrtoul(const char *cp, unsigned int base, unsigned long *res)
{
	char *end;

	*res = strtoul(cp, &end, base);

	/* skip newline character, if any */
	if (*end == '\n')
		end++;
	if (*cp == 0 || *end != 0)
		return (-EINVAL);
	return (0);
}

static inline int
kstrtol(const char *cp, unsigned int base, long *res)
{
	char *end;

	*res = strtol(cp, &end, base);

	/* skip newline character, if any */
	if (*end == '\n')
		end++;
	if (*cp == 0 || *end != 0)
		return (-EINVAL);
	return (0);
}

static inline int
kstrtoint(const char *cp, unsigned int base, int *res)
{
	char *end;
	long temp;

	*res = temp = strtol(cp, &end, base);

	/* skip newline character, if any */
	if (*end == '\n')
		end++;
	if (*cp == 0 || *end != 0)
		return (-EINVAL);
	if (temp != (int)temp)
		return (-ERANGE);
	return (0);
}

static inline int
kstrtouint(const char *cp, unsigned int base, unsigned int *res)
{
	char *end;
	unsigned long temp;

	*res = temp = strtoul(cp, &end, base);

	/* skip newline character, if any */
	if (*end == '\n')
		end++;
	if (*cp == 0 || *end != 0)
		return (-EINVAL);
	if (temp != (unsigned int)temp)
		return (-ERANGE);
	return (0);
}

static inline int
kstrtou8(const char *cp, unsigned int base, u8 *res)
{
	char *end;
	unsigned long temp;

	*res = temp = strtoul(cp, &end, base);

	/* skip newline character, if any */
	if (*end == '\n')
		end++;
	if (*cp == 0 || *end != 0)
		return (-EINVAL);
	if (temp != (u8)temp)
		return (-ERANGE);
	return (0);
}

static inline int
kstrtou16(const char *cp, unsigned int base, u16 *res)
{
	char *end;
	unsigned long temp;

	*res = temp = strtoul(cp, &end, base);

	/* skip newline character, if any */
	if (*end == '\n')
		end++;
	if (*cp == 0 || *end != 0)
		return (-EINVAL);
	if (temp != (u16)temp)
		return (-ERANGE);
	return (0);
}

static inline int
kstrtou32(const char *cp, unsigned int base, u32 *res)
{
	char *end;
	unsigned long temp;

	*res = temp = strtoul(cp, &end, base);

	/* skip newline character, if any */
	if (*end == '\n')
		end++;
	if (*cp == 0 || *end != 0)
		return (-EINVAL);
	if (temp != (u32)temp)
		return (-ERANGE);
	return (0);
}

static inline int
kstrtou64(const char *cp, unsigned int base, u64 *res)
{
       char *end;

       *res = strtouq(cp, &end, base);

       /* skip newline character, if any */
       if (*end == '\n')
               end++;
       if (*cp == 0 || *end != 0)
               return (-EINVAL);
       return (0);
}

static inline int
kstrtobool(const char *s, bool *res)
{
	int len;

	if (s == NULL || (len = strlen(s)) == 0 || res == NULL)
		return (-EINVAL);

	/* skip newline character, if any */
	if (s[len - 1] == '\n')
		len--;

	if (len == 1 && strchr("yY1", s[0]) != NULL)
		*res = true;
	else if (len == 1 && strchr("nN0", s[0]) != NULL)
		*res = false;
	else if (strncasecmp("on", s, len) == 0)
		*res = true;
	else if (strncasecmp("off", s, len) == 0)
		*res = false;
	else
		return (-EINVAL);

	return (0);
}

static inline int
kstrtobool_from_user(const char __user *s, size_t count, bool *res)
{
	char buf[8] = {};

	if (count > (sizeof(buf) - 1))
		count = (sizeof(buf) - 1);

	if (copy_from_user(buf, s, count))
		return (-EFAULT);

	return (kstrtobool(buf, res));
}

static inline int
kstrtou8_from_user(const char __user *s, size_t count, unsigned int base,
    u8 *p)
{
	char buf[8] = {};

	if (count > (sizeof(buf) - 1))
		count = (sizeof(buf) - 1);

	if (copy_from_user(buf, s, count))
		return (-EFAULT);

	return (kstrtou8(buf, base, p));
}

#define min(x, y)	((x) < (y) ? (x) : (y))
#define max(x, y)	((x) > (y) ? (x) : (y))

#define min3(a, b, c)	min(a, min(b,c))
#define max3(a, b, c)	max(a, max(b,c))

#define	min_t(type, x, y) ({			\
	type __min1 = (x);			\
	type __min2 = (y);			\
	__min1 < __min2 ? __min1 : __min2; })

#define	max_t(type, x, y) ({			\
	type __max1 = (x);			\
	type __max2 = (y);			\
	__max1 > __max2 ? __max1 : __max2; })

#define offsetofend(t, m)	\
        (offsetof(t, m) + sizeof((((t *)0)->m)))

#define clamp_t(type, _x, min, max)	min_t(type, max_t(type, _x, min), max)
#define clamp(x, lo, hi)		min( max(x,lo), hi)
#define	clamp_val(val, lo, hi) clamp_t(typeof(val), val, lo, hi)

/*
 * This looks more complex than it should be. But we need to
 * get the type for the ~ right in round_down (it needs to be
 * as wide as the result!), and we want to evaluate the macro
 * arguments just once each.
 */
#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)
#define round_down(x, y) ((x) & ~__round_mask(x, y))

#define	smp_processor_id()	PCPU_GET(cpuid)
#define	num_possible_cpus()	mp_ncpus
#define	num_online_cpus()	mp_ncpus

#if defined(__i386__) || defined(__amd64__)
extern bool linux_cpu_has_clflush;
#define	cpu_has_clflush		linux_cpu_has_clflush
#endif

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

#define	DIV_ROUND_CLOSEST_ULL(x, divisor) ({		\
	__typeof(divisor) __d = (divisor);		\
	unsigned long long __ret = (x) + (__d) / 2;	\
	__ret /= __d;					\
	__ret;						\
})

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

typedef struct linux_ratelimit {
	struct timeval lasttime;
	int counter;
} linux_ratelimit_t;

static inline bool
linux_ratelimited(linux_ratelimit_t *rl)
{
	return (ppsratecheck(&rl->lasttime, &rl->counter, 1));
}

#define	struct_size(ptr, field, num) ({ \
	const size_t __size = offsetof(__typeof(*(ptr)), field); \
	const size_t __max = (SIZE_MAX - __size) / sizeof((ptr)->field[0]); \
	((num) > __max) ? SIZE_MAX : (__size + sizeof((ptr)->field[0]) * (num)); \
})

#define	__is_constexpr(x) \
	__builtin_constant_p(x)

/*
 * The is_signed() macro below returns true if the passed data type is
 * signed. Else false is returned.
 */
#define	is_signed(datatype) (((datatype)-1 / (datatype)2) == (datatype)0)

/*
 * The type_max() macro below returns the maxium positive value the
 * passed data type can hold.
 */
#define	type_max(datatype) ( \
  (sizeof(datatype) >= 8) ? (is_signed(datatype) ? INT64_MAX : UINT64_MAX) : \
  (sizeof(datatype) >= 4) ? (is_signed(datatype) ? INT32_MAX : UINT32_MAX) : \
  (sizeof(datatype) >= 2) ? (is_signed(datatype) ? INT16_MAX : UINT16_MAX) : \
			    (is_signed(datatype) ? INT8_MAX : UINT8_MAX) \
)

/*
 * The type_min() macro below returns the minimum value the passed
 * data type can hold. For unsigned types the minimum value is always
 * zero. For signed types it may vary.
 */
#define	type_min(datatype) ( \
  (sizeof(datatype) >= 8) ? (is_signed(datatype) ? INT64_MIN : 0) : \
  (sizeof(datatype) >= 4) ? (is_signed(datatype) ? INT32_MIN : 0) : \
  (sizeof(datatype) >= 2) ? (is_signed(datatype) ? INT16_MIN : 0) : \
			    (is_signed(datatype) ? INT8_MIN : 0) \
)

#define	TAINT_WARN	0
#define	test_taint(x)	(0)

/*
 * Checking if an option is defined would be easy if we could do CPP inside CPP.
 * The defined case whether -Dxxx or -Dxxx=1 are easy to deal with.  In either
 * case the defined value is "1". A more general -Dxxx=<c> case will require
 * more effort to deal with all possible "true" values. Hope we do not have
 * to do this as well.
 * The real problem is the undefined case.  To avoid this problem we do the
 * concat/varargs trick: "yyy" ## xxx can make two arguments if xxx is "1"
 * by having a #define for yyy_1 which is "ignore,".
 * Otherwise we will just get "yyy".
 * Need to be careful about variable substitutions in macros though.
 * This way we make a (true, false) problem a (don't care, true, false) or a
 * (don't care true, false).  Then we can use a variadic macro to only select
 * the always well known and defined argument #2.  And that seems to be
 * exactly what we need.  Use 1 for true and 0 for false to also allow
 * #if IS_*() checks pre-compiler checks which do not like #if true.
 */
#define ___XAB_1		dontcare,
#define ___IS_XAB(_ignore, _x, ...)	(_x)
#define	__IS_XAB(_x)		___IS_XAB(_x 1, 0)
#define	_IS_XAB(_x)		__IS_XAB(__CONCAT(___XAB_, _x))

/* This is if CONFIG_ccc=y. */
#define	IS_BUILTIN(_x)		_IS_XAB(_x)
/* This is if CONFIG_ccc=m. */
#define	IS_MODULE(_x)		_IS_XAB(_x ## _MODULE)
/* This is if CONFIG_ccc is compiled in(=y) or a module(=m). */
#define	IS_ENABLED(_x)		(IS_BUILTIN(_x) || IS_MODULE(_x))
/*
 * This is weird case.  If the CONFIG_ccc is builtin (=y) this returns true;
 * or if the CONFIG_ccc is a module (=m) and the caller is built as a module
 * (-DMODULE defined) this returns true, but if the callers is not a module
 * (-DMODULE not defined, which means caller is BUILTIN) then it returns
 * false.  In other words, a module can reach the kernel, a module can reach
 * a module, but the kernel cannot reach a module, and code never compiled
 * cannot be reached either.
 * XXX -- I'd hope the module-to-module case would be handled by a proper
 * module dependency definition (MODULE_DEPEND() in FreeBSD).
 */
#define	IS_REACHABLE(_x)	(IS_BUILTIN(_x) || \
				    (IS_MODULE(_x) && IS_BUILTIN(MODULE)))

#endif	/* _LINUX_KERNEL_H_ */
