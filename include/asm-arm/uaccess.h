#ifndef _ASMARM_UACCESS_H
#define _ASMARM_UACCESS_H

/*
 * User space memory access functions
 */
#include <linux/sched.h>
#include <asm/errno.h>

#define VERIFY_READ 0
#define VERIFY_WRITE 1

/*
 * The exception table consists of pairs of addresses: the first is the
 * address of an instruction that is allowed to fault, and the second is
 * the address at which the program should continue.  No registers are
 * modified, so it is entirely up to the continuation code to figure out
 * what to do.
 *
 * All the routines below use bits of fixup code that are out of line
 * with the main instruction path.  This means when everything is well,
 * we don't even have to jump over them.  Further, they do not intrude
 * on our cache or tlb entries.
 */

struct exception_table_entry
{
	unsigned long insn, fixup;
};

/* Returns 0 if exception not found and fixup otherwise.  */
extern unsigned long search_exception_table(unsigned long);

#define get_ds()	(KERNEL_DS)
#define get_fs()	(current->addr_limit)
#define segment_eq(a,b)	((a) == (b))

#include <asm/proc/uaccess.h>

#define access_ok(type,addr,size)	(__range_ok(addr,size) == 0)

static inline int verify_area(int type, const void * addr, unsigned long size)
{
	return access_ok(type, addr, size) ? 0 : -EFAULT;
}

/*
 * Single-value transfer routines.  They automatically use the right
 * size if we just have the right pointer type.  Note that the functions
 * which read from user space (*get_*) need to take care not to leak
 * kernel data even if the calling code is buggy and fails to check
 * the return value.  This means zeroing out the destination variable
 * or buffer on error.  Normally this is done out of line by the
 * fixup code, but there are a few places where it intrudes on the
 * main code path.  When we only write to user space, there is no
 * problem.
 *
 * The "__xxx" versions of the user access functions do not verify the
 * address space - it must have been done previously with a separate
 * "access_ok()" call.
 *
 * The "xxx_error" versions set the third argument to EFAULT if an
 * error occurs, and leave it unchanged on success.  Note that these
 * versions are void (ie, don't return a value as such).
 */

extern int __get_user_1(void *);
extern int __get_user_2(void *);
extern int __get_user_4(void *);
extern int __get_user_8(void *);
extern int __get_user_bad(void);

#define __get_user_x(__r1,__p,__e,__s,__i...)				\
	   __asm__ __volatile__ ("bl	__get_user_" #__s		\
		: "=&r" (__e), "=r" (__r1)				\
		: "0" (__p)						\
		: __i)

#define get_user(x,p)							\
	({								\
		const register typeof(*(p)) *__p asm("r0") = (p);	\
		register typeof(*(p)) __r1 asm("r1");			\
		register int __e asm("r0");				\
		switch (sizeof(*(p))) {					\
		case 1:							\
			__get_user_x(__r1, __p, __e, 1, "lr");		\
	       		break;						\
		case 2:							\
			__get_user_x(__r1, __p, __e, 2, "r2", "lr");	\
			break;						\
		case 4:							\
	       		__get_user_x(__r1, __p, __e, 4, "lr");		\
			break;						\
		case 8:							\
			__get_user_x(__r1, __p, __e, 8, "lr");		\
	       		break;						\
		default: __e = __get_user_bad(); break;			\
		}							\
		x = __r1;						\
		__e;							\
	})

#define __get_user(x,p)		__get_user_nocheck((x),(p),sizeof(*(p)))
#define __get_user_error(x,p,e)	__get_user_nocheck_error((x),(p),sizeof(*(p)),(e))

extern int __put_user_1(void *, unsigned int);
extern int __put_user_2(void *, unsigned int);
extern int __put_user_4(void *, unsigned int);
extern int __put_user_8(void *, unsigned long long);
extern int __put_user_bad(void);

#define __put_user_x(__r1,__p,__e,__s,__i...)				\
	   __asm__ __volatile__ ("bl	__put_user_" #__s		\
		: "=&r" (__e)						\
		: "0" (__p), "r" (__r1)					\
		: __i)

#define put_user(x,p)							\
	({								\
		const register typeof(*(p)) __r1 asm("r1") = (x);	\
		const register typeof(*(p)) *__p asm("r0") = (p);	\
		register int __e asm("r0");				\
		switch (sizeof(*(p))) {					\
		case 1:							\
			__put_user_x(__r1, __p, __e, 1, "r2", "lr");	\
			break;						\
		case 2:							\
			__put_user_x(__r1, __p, __e, 2, "r2", "lr");	\
			break;						\
		case 4:							\
			__put_user_x(__r1, __p, __e, 4, "r2", "lr");	\
			break;						\
		case 8:							\
			__put_user_x(__r1, __p, __e, 8, "ip", "lr");	\
			break;						\
		default: __e = __put_user_bad(); break;			\
		}							\
		__e;							\
	})

#define __put_user(x,p)		__put_user_nocheck((__typeof(*(p)))(x),(p),sizeof(*(p)))
#define __put_user_error(x,p,e)	__put_user_nocheck_error((x),(p),sizeof(*(p)),(e))

static __inline__ unsigned long copy_from_user(void *to, const void *from, unsigned long n)
{
	if (access_ok(VERIFY_READ, from, n))
		__do_copy_from_user(to, from, n);
	else /* security hole - plug it */
		memzero(to, n);
	return n;
}

static __inline__ unsigned long __copy_from_user(void *to, const void *from, unsigned long n)
{
	__do_copy_from_user(to, from, n);
	return n;
}

static __inline__ unsigned long copy_to_user(void *to, const void *from, unsigned long n)
{
	if (access_ok(VERIFY_WRITE, to, n))
		__do_copy_to_user(to, from, n);
	return n;
}

static __inline__ unsigned long __copy_to_user(void *to, const void *from, unsigned long n)
{
	__do_copy_to_user(to, from, n);
	return n;
}

static __inline__ unsigned long clear_user (void *to, unsigned long n)
{
	if (access_ok(VERIFY_WRITE, to, n))
		__do_clear_user(to, n);
	return n;
}

static __inline__ unsigned long __clear_user (void *to, unsigned long n)
{
	__do_clear_user(to, n);
	return n;
}

static __inline__ long strncpy_from_user (char *dst, const char *src, long count)
{
	long res = -EFAULT;
	if (access_ok(VERIFY_READ, src, 1))
		__do_strncpy_from_user(dst, src, count, res);
	return res;
}

static __inline__ long __strncpy_from_user (char *dst, const char *src, long count)
{
	long res;
	__do_strncpy_from_user(dst, src, count, res);
	return res;
}

#define strlen_user(s)	strnlen_user(s, ~0UL >> 1)

static inline long strnlen_user(const char *s, long n)
{
	unsigned long res = 0;

	if (__addr_ok(s))
		__do_strnlen_user(s, n, res);

	return res;
}

/*
 * These are the work horses of the get/put_user functions
 */
#if 0
#define __get_user_check(x,ptr,size)					\
({									\
	long __gu_err = -EFAULT, __gu_val = 0;				\
	const __typeof__(*(ptr)) *__gu_addr = (ptr);			\
	if (access_ok(VERIFY_READ,__gu_addr,size)) {			\
		__gu_err = 0;						\
		__get_user_size(__gu_val,__gu_addr,(size),__gu_err);	\
	}								\
	(x) = (__typeof__(*(ptr)))__gu_val;				\
	__gu_err;							\
})
#endif

#define __get_user_nocheck(x,ptr,size)					\
({									\
	long __gu_err = 0, __gu_val;					\
	__get_user_size(__gu_val,(ptr),(size),__gu_err);		\
	(x) = (__typeof__(*(ptr)))__gu_val;				\
	__gu_err;							\
})

#define __get_user_nocheck_error(x,ptr,size,err)			\
({									\
	long __gu_val;							\
	__get_user_size(__gu_val,(ptr),(size),(err));			\
	(x) = (__typeof__(*(ptr)))__gu_val;				\
	(void) 0;							\
})

#define __put_user_check(x,ptr,size)					\
({									\
	long __pu_err = -EFAULT;					\
	__typeof__(*(ptr)) *__pu_addr = (ptr);				\
	if (access_ok(VERIFY_WRITE,__pu_addr,size)) {			\
		__pu_err = 0;						\
		__put_user_size((x),__pu_addr,(size),__pu_err);		\
	}								\
	__pu_err;							\
})

#define __put_user_nocheck(x,ptr,size)					\
({									\
	long __pu_err = 0;						\
	__typeof__(*(ptr)) *__pu_addr = (ptr);				\
	__put_user_size((x),__pu_addr,(size),__pu_err);			\
	__pu_err;							\
})

#define __put_user_nocheck_error(x,ptr,size,err)			\
({									\
	__put_user_size((x),(ptr),(size),err);				\
	(void) 0;							\
})

#define __get_user_size(x,ptr,size,retval)				\
do {									\
	switch (size) {							\
	case 1:	__get_user_asm_byte(x,ptr,retval);	break;		\
	case 2:	__get_user_asm_half(x,ptr,retval);	break;		\
	case 4:	__get_user_asm_word(x,ptr,retval);	break;		\
	default: (x) = __get_user_bad();				\
	}								\
} while (0)

#define __put_user_size(x,ptr,size,retval)				\
do {									\
	switch (size) {							\
	case 1: __put_user_asm_byte(x,ptr,retval);	break;		\
	case 2: __put_user_asm_half(x,ptr,retval);	break;		\
	case 4: __put_user_asm_word(x,ptr,retval);	break;		\
	default: __put_user_bad();					\
	}								\
} while (0)

#endif /* _ASMARM_UACCESS_H */
