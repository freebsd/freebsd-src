#ifdef __KERNEL__
#ifndef _PPC_UACCESS_H
#define _PPC_UACCESS_H

#ifndef __ASSEMBLY__
#include <linux/sched.h>
#include <linux/errno.h>
#include <asm/processor.h>

#define VERIFY_READ	0
#define VERIFY_WRITE	1

/*
 * The fs value determines whether argument validity checking should be
 * performed or not.  If get_fs() == USER_DS, checking is performed, with
 * get_fs() == KERNEL_DS, checking is bypassed.
 *
 * For historical reasons, these macros are grossly misnamed.
 */

#define KERNEL_DS	((mm_segment_t) { 0 })
#define USER_DS		((mm_segment_t) { 1 })

#define get_ds()	(KERNEL_DS)
#define get_fs()	(current->thread.fs)
#define set_fs(val)	(current->thread.fs = (val))

#define segment_eq(a,b)	((a).seg == (b).seg)

#define __kernel_ok (segment_eq(get_fs(), KERNEL_DS))
#define __user_ok(addr,size) (((size) <= TASK_SIZE)&&((addr) <= TASK_SIZE-(size)))
#define __access_ok(addr,size) (__kernel_ok || __user_ok((addr),(size)))
#define access_ok(type,addr,size) __access_ok((unsigned long)(addr),(size))

extern inline int verify_area(int type, const void * addr, unsigned long size)
{
	return access_ok(type,addr,size) ? 0 : -EFAULT;
}


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
extern void sort_exception_table(void);

/*
 * These are the main single-value transfer routines.  They automatically
 * use the right size if we just have the right pointer type.
 *
 * This gets kind of ugly. We want to return _two_ values in "get_user()"
 * and yet we don't want to do any pointers, because that is too much
 * of a performance impact. Thus we have a few rather ugly macros here,
 * and hide all the uglyness from the user.
 *
 * The "__xxx" versions of the user access functions are versions that
 * do not verify the address space, that must have been done previously
 * with a separate "access_ok()" call (this is used when we do multiple
 * accesses to the same area of user memory).
 *
 * As we use the same address space for kernel and user data on the
 * PowerPC, we can just do these as direct assignments.  (Of course, the
 * exception handling means that it's no longer "just"...)
 *
 * The "user64" versions of the user access functions are versions that 
 * allow access of 64-bit data. The "get_user" functions do not 
 * properly handle 64-bit data because the value gets down cast to a long. 
 * The "put_user" functions already handle 64-bit data properly but we add 
 * "user64" versions for completeness
 */
#define get_user(x,ptr) \
  __get_user_check((x),(ptr),sizeof(*(ptr)))
#define get_user64(x,ptr) \
  __get_user64_check((x),(ptr),sizeof(*(ptr)))
#define put_user(x,ptr) \
  __put_user_check((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)))
#define put_user64(x,ptr) put_user(x,ptr)

#define __get_user(x,ptr) \
  __get_user_nocheck((x),(ptr),sizeof(*(ptr)))
#define __get_user64(x,ptr) \
  __get_user64_nocheck((x),(ptr),sizeof(*(ptr)))
#define __put_user(x,ptr) \
  __put_user_nocheck((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)))
#define __put_user64(x,ptr) __put_user(x,ptr)

extern long __put_user_bad(void);

#define __put_user_nocheck(x,ptr,size)			\
({							\
	long __pu_err;					\
	__put_user_size((x),(ptr),(size),__pu_err);	\
	__pu_err;					\
})

#define __put_user_check(x,ptr,size)				\
({								\
	long __pu_err = -EFAULT;				\
	__typeof__(*(ptr)) *__pu_addr = (ptr);			\
	if (access_ok(VERIFY_WRITE,__pu_addr,size))		\
		__put_user_size((x),__pu_addr,(size),__pu_err);	\
	__pu_err;						\
})

#define __put_user_size(x,ptr,size,retval)			\
do {								\
	retval = 0;						\
	switch (size) {						\
	  case 1: __put_user_asm(x,ptr,retval,"stb"); break;	\
	  case 2: __put_user_asm(x,ptr,retval,"sth"); break;	\
	  case 4: __put_user_asm(x,ptr,retval,"stw"); break;	\
	  case 8: __put_user_asm2(x,ptr,retval); break;		\
	  default: __put_user_bad();				\
	}							\
} while (0)

struct __large_struct { unsigned long buf[100]; };
#define __m(x) (*(struct __large_struct *)(x))

/*
 * We don't tell gcc that we are accessing memory, but this is OK
 * because we do not write to any memory gcc knows about, so there
 * are no aliasing issues.
 */
#define __put_user_asm(x, addr, err, op)			\
	__asm__ __volatile__(					\
		"1:	"op" %1,0(%2)\n"			\
		"2:\n"						\
		".section .fixup,\"ax\"\n"			\
		"3:	li %0,%3\n"				\
		"	b 2b\n"					\
		".previous\n"					\
		".section __ex_table,\"a\"\n"			\
		"	.align 2\n"				\
		"	.long 1b,3b\n"				\
		".previous"					\
		: "=r"(err)					\
		: "r"(x), "b"(addr), "i"(-EFAULT), "0"(err))

#define __put_user_asm2(x, addr, err)				\
	__asm__ __volatile__(					\
		"1:	stw %1,0(%2)\n"				\
		"2:	stw %1+1,4(%2)\n"				\
		"3:\n"						\
		".section .fixup,\"ax\"\n"			\
		"4:	li %0,%3\n"				\
		"	b 3b\n"					\
		".previous\n"					\
		".section __ex_table,\"a\"\n"			\
		"	.align 2\n"				\
		"	.long 1b,4b\n"				\
		"	.long 2b,4b\n"				\
		".previous"					\
		: "=r"(err)					\
		: "r"(x), "b"(addr), "i"(-EFAULT), "0"(err))

#define __get_user_nocheck(x,ptr,size)				\
({								\
	long __gu_err, __gu_val;				\
	__get_user_size(__gu_val,(ptr),(size),__gu_err);	\
	(x) = (__typeof__(*(ptr)))__gu_val;			\
	__gu_err;						\
})

#define __get_user64_nocheck(x,ptr,size)			\
({								\
	long __gu_err;						\
	long long __gu_val;					\
	__get_user_size64(__gu_val,(ptr),(size),__gu_err);	\
	(x) = (__typeof__(*(ptr)))__gu_val;			\
	__gu_err;						\
})

#define __get_user_check(x,ptr,size)					\
({									\
	long __gu_err = -EFAULT, __gu_val = 0;				\
	const __typeof__(*(ptr)) *__gu_addr = (ptr);			\
	if (access_ok(VERIFY_READ,__gu_addr,size))			\
		__get_user_size(__gu_val,__gu_addr,(size),__gu_err);	\
	(x) = (__typeof__(*(ptr)))__gu_val;				\
	__gu_err;							\
})

#define __get_user64_check(x,ptr,size)					\
({									\
	long __gu_err = -EFAULT;					\
	long long __gu_val = 0;						\
	const __typeof__(*(ptr)) *__gu_addr = (ptr);			\
	if (access_ok(VERIFY_READ,__gu_addr,size))			\
		__get_user_size64(__gu_val,__gu_addr,(size),__gu_err);	\
	(x) = (__typeof__(*(ptr)))__gu_val;				\
	__gu_err;							\
})

extern long __get_user_bad(void);

#define __get_user_size(x,ptr,size,retval)			\
do {								\
	retval = 0;						\
	switch (size) {						\
	  case 1: __get_user_asm(x,ptr,retval,"lbz"); break;	\
	  case 2: __get_user_asm(x,ptr,retval,"lhz"); break;	\
	  case 4: __get_user_asm(x,ptr,retval,"lwz"); break;	\
	  default: (x) = __get_user_bad();			\
	}							\
} while (0)

#define __get_user_size64(x,ptr,size,retval)			\
do {								\
	retval = 0;						\
	switch (size) {						\
	  case 1: __get_user_asm(x,ptr,retval,"lbz"); break;	\
	  case 2: __get_user_asm(x,ptr,retval,"lhz"); break;	\
	  case 4: __get_user_asm(x,ptr,retval,"lwz"); break;	\
	  case 8: __get_user_asm2(x, ptr, retval); break;	\
	  default: (x) = __get_user_bad();			\
	}							\
} while (0)

#define __get_user_asm(x, addr, err, op)		\
	__asm__ __volatile__(				\
		"1:	"op" %1,0(%2)\n"		\
		"2:\n"					\
		".section .fixup,\"ax\"\n"		\
		"3:	li %0,%3\n"			\
		"	li %1,0\n"			\
		"	b 2b\n"				\
		".previous\n"				\
		".section __ex_table,\"a\"\n"		\
		"	.align 2\n"			\
		"	.long 1b,3b\n"			\
		".previous"				\
		: "=r"(err), "=r"(x)			\
		: "b"(addr), "i"(-EFAULT), "0"(err))

#define __get_user_asm2(x, addr, err)			\
	__asm__ __volatile__(				\
		"1:	lwz %1,0(%2)\n"			\
		"2:	lwz %1+1,4(%2)\n"		\
		"3:\n"					\
		".section .fixup,\"ax\"\n"		\
		"4:	li %0,%3\n"			\
		"	li %1,0\n"			\
		"	li %1+1,0\n"			\
		"	b 3b\n"				\
		".previous\n"				\
		".section __ex_table,\"a\"\n"		\
		"	.align 2\n"			\
		"	.long 1b,4b\n"			\
		"	.long 2b,4b\n"			\
		".previous"				\
		: "=r"(err), "=&r"(x)			\
		: "b"(addr), "i"(-EFAULT), "0"(err))

/* more complex routines */

extern int __copy_tofrom_user(void *to, const void *from, unsigned long size);

extern inline unsigned long
copy_from_user(void *to, const void *from, unsigned long n)
{
	unsigned long over;

	if (access_ok(VERIFY_READ, from, n))
		return __copy_tofrom_user(to, from, n);
	if ((unsigned long)from < TASK_SIZE) {
		over = (unsigned long)from + n - TASK_SIZE;
		return __copy_tofrom_user(to, from, n - over) + over;
	}
	return n;
}

extern inline unsigned long
copy_to_user(void *to, const void *from, unsigned long n)
{
	unsigned long over;

	if (access_ok(VERIFY_WRITE, to, n))
		return __copy_tofrom_user(to, from, n);
	if ((unsigned long)to < TASK_SIZE) {
		over = (unsigned long)to + n - TASK_SIZE;
		return __copy_tofrom_user(to, from, n - over) + over;
	}
	return n;
}

#define __copy_from_user(to, from, size) \
	__copy_tofrom_user((to), (from), (size))
#define __copy_to_user(to, from, size) \
	__copy_tofrom_user((to), (from), (size))

extern unsigned long __clear_user(void *addr, unsigned long size);

extern inline unsigned long
clear_user(void *addr, unsigned long size)
{
	if (access_ok(VERIFY_WRITE, addr, size))
		return __clear_user(addr, size);
	if ((unsigned long)addr < TASK_SIZE) {
		unsigned long over = (unsigned long)addr + size - TASK_SIZE;
		return __clear_user(addr, size - over) + over;
	}
	return size;
}

extern int __strncpy_from_user(char *dst, const char *src, long count);

extern inline long
strncpy_from_user(char *dst, const char *src, long count)
{
	if (access_ok(VERIFY_READ, src, 1))
		return __strncpy_from_user(dst, src, count);
	return -EFAULT;
}

/*
 * Return the size of a string (including the ending 0)
 *
 * Return 0 for error
 */

extern int __strnlen_user(const char *str, long len, unsigned long top);

/*
 * Returns the length of the string at str (including the null byte),
 * or 0 if we hit a page we can't access,
 * or something > len if we didn't find a null byte.
 *
 * The `top' parameter to __strnlen_user is to make sure that
 * we can never overflow from the user area into kernel space.
 */
extern __inline__ int strnlen_user(const char *str, long len)
{
	unsigned long top = __kernel_ok? ~0UL: TASK_SIZE - 1;

	if ((unsigned long)str > top)
		return 0;
	return __strnlen_user(str, len, top);
}

#define strlen_user(str)	strnlen_user((str), 0x7ffffffe)

#endif  /* __ASSEMBLY__ */

#endif	/* _PPC_UACCESS_H */
#endif /* __KERNEL__ */
