/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1997, 1998, 1999, 2000, 03 by Ralf Baechle
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 */
#ifndef _ASM_UACCESS_H
#define _ASM_UACCESS_H

#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/sched.h>

#define STR(x)  __STR(x)
#define __STR(x)  #x

/*
 * The fs value determines whether argument validity checking should be
 * performed or not.  If get_fs() == USER_DS, checking is performed, with
 * get_fs() == KERNEL_DS, checking is bypassed.
 *
 * For historical reasons, these macros are grossly misnamed.
 */
#define KERNEL_DS	((mm_segment_t) { (unsigned long) 0L })
#define USER_DS		((mm_segment_t) { (unsigned long) -1L })

#define VERIFY_READ    0
#define VERIFY_WRITE   1

#define get_fs()        (current->thread.current_ds)
#define get_ds()	(KERNEL_DS)
#define set_fs(x)       (current->thread.current_ds=(x))

#define segment_eq(a,b)	((a).seg == (b).seg)


/*
 * Is a address valid? This does a straighforward calculation rather
 * than tests.
 *
 * Address valid if:
 *  - "addr" doesn't have any high-bits set
 *  - AND "size" doesn't have any high-bits set
 *  - AND "addr+size" doesn't have any high-bits set
 *  - OR we are in kernel mode.
 */
#define __ua_size(size)							\
	(__builtin_constant_p(size) && (signed long) (size) > 0 ? 0 : (size))

#define __access_ok(addr,size,mask)                                     \
	(((signed long)((mask)&(addr | (addr + size) | __ua_size(size)))) >= 0)

#define __access_mask ((long)(get_fs().seg))

/*
 * access_ok: - Checks if a user space pointer is valid
 * @type: Type of access: %VERIFY_READ or %VERIFY_WRITE.  Note that
 *        %VERIFY_WRITE is a superset of %VERIFY_READ - if it is safe
 *        to write to a block, it is always safe to read from it.
 * @addr: User space pointer to start of block to check
 * @size: Size of block to check
 *
 * Context: User context only.  This function may sleep.
 *
 * Checks if a pointer to a block of memory in user space is valid.
 *
 * Returns true (nonzero) if the memory block may be valid, false (zero)
 * if it is definitely invalid.
 *
 * Note that, depending on architecture, this function probably just
 * checks that the pointer is in the user space range - after calling
 * this function, memory access functions may still return -EFAULT.
 */
#define access_ok(type, addr, size)					\
	likely(__access_ok((unsigned long)(addr), (size), __access_mask))

/*
 * verify_area: - Obsolete, use access_ok()
 * @type: Type of access: %VERIFY_READ or %VERIFY_WRITE
 * @addr: User space pointer to start of block to check
 * @size: Size of block to check
 *
 * Context: User context only.  This function may sleep.
 *
 * This function has been replaced by access_ok().
 *
 * Checks if a pointer to a block of memory in user space is valid.
 *
 * Returns zero if the memory block may be valid, -EFAULT
 * if it is definitely invalid.
 *
 * See access_ok() for more details.
 */
static inline int verify_area(int type, const void * addr, unsigned long size)
{
	return access_ok(type, addr, size) ? 0 : -EFAULT;
}

/*
 * put_user: - Write a simple value into user space.
 * @x:   Value to copy to user space.
 * @ptr: Destination address, in user space.
 *
 * Context: User context only.  This function may sleep.
 *
 * This macro copies a single simple value from kernel space to user
 * space.  It supports simple types like char and int, but not larger
 * data types like structures or arrays.
 *
 * @ptr must have pointer-to-simple-variable type, and @x must be assignable
 * to the result of dereferencing @ptr.
 *
 * Returns zero on success, or -EFAULT on error.
 */
#define put_user(x,ptr)	\
	__put_user_check((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)))

/*
 * get_user: - Get a simple variable from user space.
 * @x:   Variable to store result.
 * @ptr: Source address, in user space.
 *
 * Context: User context only.  This function may sleep.
 *
 * This macro copies a single simple variable from user space to kernel
 * space.  It supports simple types like char and int, but not larger
 * data types like structures or arrays.
 *
 * @ptr must have pointer-to-simple-variable type, and the result of
 * dereferencing @ptr must be assignable to @x without a cast.
 *
 * Returns zero on success, or -EFAULT on error.
 * On error, the variable @x is set to zero.
 */
#define get_user(x,ptr) \
	__get_user_check((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)))

/*
 * __put_user: - Write a simple value into user space, with less checking.
 * @x:   Value to copy to user space.
 * @ptr: Destination address, in user space.
 *
 * Context: User context only.  This function may sleep.
 *
 * This macro copies a single simple value from kernel space to user
 * space.  It supports simple types like char and int, but not larger
 * data types like structures or arrays.
 *
 * @ptr must have pointer-to-simple-variable type, and @x must be assignable
 * to the result of dereferencing @ptr.
 *
 * Caller must check the pointer with access_ok() before calling this
 * function.
 *
 * Returns zero on success, or -EFAULT on error.
 */
#define __put_user(x,ptr) \
	__put_user_nocheck((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)))

/*
 * __get_user: - Get a simple variable from user space, with less checking.
 * @x:   Variable to store result.
 * @ptr: Source address, in user space.
 *
 * Context: User context only.  This function may sleep.
 *
 * This macro copies a single simple variable from user space to kernel
 * space.  It supports simple types like char and int, but not larger
 * data types like structures or arrays.
 *
 * @ptr must have pointer-to-simple-variable type, and the result of
 * dereferencing @ptr must be assignable to @x without a cast.
 *
 * Caller must check the pointer with access_ok() before calling this
 * function.
 *
 * Returns zero on success, or -EFAULT on error.
 * On error, the variable @x is set to zero.
 */
#define __get_user(x,ptr) \
	__get_user_nocheck((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)))

struct __large_struct { unsigned long buf[100]; };
#define __m(x) (*(struct __large_struct *)(x))

/*
 * Yuck.  We need two variants, one for 64bit operation and one
 * for 32 bit mode and old iron.
 */
#ifdef __mips64
#define __GET_USER_DW __get_user_asm("ld")
#else
#define __GET_USER_DW __get_user_asm_ll32
#endif

#define __get_user_nocheck(x,ptr,size)					\
({									\
	long __gu_err;							\
	__typeof(*(ptr)) __gu_val;					\
	long __gu_addr;							\
	__asm__("":"=r" (__gu_val));					\
	__gu_addr = (long) (ptr);					\
	__asm__("":"=r" (__gu_err));					\
	switch (size) {							\
		case 1: __get_user_asm("lb"); break;			\
		case 2: __get_user_asm("lh"); break;			\
		case 4: __get_user_asm("lw"); break;			\
		case 8: __GET_USER_DW; break;				\
		default: __get_user_unknown(); break;			\
	} x = (__typeof__(*(ptr))) __gu_val;				\
	__gu_err;							\
})

#define __get_user_check(x,ptr,size)					\
({									\
	long __gu_err;							\
	__typeof__(*(ptr)) __gu_val;					\
	long __gu_addr;							\
	__asm__("":"=r" (__gu_val));					\
	__gu_addr = (long) (ptr);					\
	__asm__("":"=r" (__gu_err));					\
	if (access_ok(VERIFY_READ, __gu_addr, size)) {			\
		switch (size) {						\
		case 1: __get_user_asm("lb"); break;			\
		case 2: __get_user_asm("lh"); break;			\
		case 4: __get_user_asm("lw"); break;			\
		case 8: __GET_USER_DW; break;				\
		default: __get_user_unknown(); break;			\
		}							\
	} x = (__typeof__(*(ptr))) __gu_val;				\
	__gu_err;							\
})

#define __get_user_asm(insn)						\
({									\
	__asm__ __volatile__(						\
	"1:\t" insn "\t%1,%2\n\t"					\
	"move\t%0,$0\n"							\
	"2:\n\t"							\
	".section\t.fixup,\"ax\"\n"					\
	"3:\tli\t%0,%3\n\t"						\
	"move\t%1,$0\n\t"						\
	"j\t2b\n\t"							\
	".previous\n\t"							\
	".section\t__ex_table,\"a\"\n\t"				\
	".word\t1b,3b\n\t"						\
	".previous"							\
	:"=r" (__gu_err), "=r" (__gu_val)				\
	:"o" (__m(__gu_addr)), "i" (-EFAULT));				\
})

/*
 * Get a long long 64 using 32 bit registers.
 */
#define __get_user_asm_ll32						\
({									\
__asm__ __volatile__(							\
	"1:\tlw\t%1,%2\n"						\
	"2:\tlw\t%D1,%3\n\t"						\
	"move\t%0,$0\n"							\
	"3:\t.section\t.fixup,\"ax\"\n"					\
	"4:\tli\t%0,%4\n\t"						\
	"move\t%1,$0\n\t"						\
	"move\t%D1,$0\n\t"						\
	"j\t3b\n\t"							\
	".previous\n\t"							\
	".section\t__ex_table,\"a\"\n\t"				\
	".word\t1b,4b\n\t"						\
	".word\t2b,4b\n\t"						\
	".previous"							\
	:"=r" (__gu_err), "=&r" (__gu_val)				\
	:"o" (__m(__gu_addr)), "o" (__m(__gu_addr + 4)),		\
	 "i" (-EFAULT));						\
})

extern void __get_user_unknown(void);

/*
 * Yuck.  We need two variants, one for 64bit operation and one
 * for 32 bit mode and old iron.
 */
#ifdef __mips64
#define __PUT_USER_DW __put_user_asm("sd")
#else
#define __PUT_USER_DW __put_user_asm_ll32
#endif

#define __put_user_nocheck(x,ptr,size)					\
({									\
	long __pu_err;							\
	__typeof__(*(ptr)) __pu_val;					\
	long __pu_addr;							\
	__pu_val = (x);							\
	__pu_addr = (long) (ptr);					\
	__asm__("":"=r" (__pu_err));					\
	switch (size) {							\
		case 1: __put_user_asm("sb"); break;			\
		case 2: __put_user_asm("sh"); break;			\
		case 4: __put_user_asm("sw"); break;			\
		case 8: __PUT_USER_DW; break;				\
		default: __put_user_unknown(); break;			\
	}								\
	__pu_err;							\
})

#define __put_user_check(x,ptr,size)					\
({									\
	long __pu_err;							\
	__typeof__(*(ptr)) __pu_val;					\
	long __pu_addr;							\
	__pu_val = (x);							\
	__pu_addr = (long) (ptr);					\
	__asm__("":"=r" (__pu_err));					\
	if (access_ok(VERIFY_WRITE, __pu_addr, size)) {			\
		switch (size) {						\
		case 1: __put_user_asm("sb"); break;			\
		case 2: __put_user_asm("sh"); break;			\
		case 4: __put_user_asm("sw"); break;			\
		case 8: __PUT_USER_DW; break;				\
		default: __put_user_unknown(); break;			\
		}							\
	}								\
	__pu_err;							\
})

#define __put_user_asm(insn)						\
({									\
	__asm__ __volatile__(						\
	"1:\t" insn "\t%z1, %2\t\t\t# __put_user_asm\n\t"		\
	"move\t%0, $0\n"						\
	"2:\n\t"							\
	".section\t.fixup,\"ax\"\n"					\
	"3:\tli\t%0, %3\n\t"						\
	"j\t2b\n\t"							\
	".previous\n\t"							\
	".section\t__ex_table,\"a\"\n\t"				\
	".word\t1b, 3b\n\t"						\
	".previous"							\
	:"=r" (__pu_err)						\
	:"Jr" (__pu_val), "o" (__m(__pu_addr)), "i" (-EFAULT));		\
})

#define __put_user_asm_ll32						\
({									\
__asm__ __volatile__(							\
	"1:\tsw\t%1, %2\t\t\t# __put_user_asm_ll32\n\t"			\
	"2:\tsw\t%D1, %3\n"						\
	"move\t%0, $0\n"						\
	"3:\n\t"							\
	".section\t.fixup,\"ax\"\n"					\
	"4:\tli\t%0, %4\n\t"						\
	"j\t3b\n\t"							\
	".previous\n\t"							\
	".section\t__ex_table,\"a\"\n\t"				\
	".word\t1b,4b\n\t"						\
	".word\t2b,4b\n\t"						\
	".previous"							\
	:"=r" (__pu_err)						\
	:"r" (__pu_val), "o" (__m(__pu_addr)), "o" (__m(__pu_addr + 4)),\
	 "i" (-EFAULT));						\
})

extern void __put_user_unknown(void);

/*
 * We're generating jump to subroutines which will be outside the range of
 * jump instructions
 */
#ifdef MODULE
#define __MODULE_JAL(destination)					\
	".set\tnoat\n\t"						\
	"la\t$1, " #destination "\n\t"					\
	"jalr\t$1\n\t"							\
	".set\tat\n\t"
#else
#define __MODULE_JAL(destination)					\
	"jal\t" #destination "\n\t"
#endif

extern size_t __copy_user(void *__to, const void *__from, size_t __n);

#define __invoke_copy_to_user(to,from,n)				\
({									\
	register void *__cu_to_r __asm__ ("$4");			\
	register const void *__cu_from_r __asm__ ("$5");		\
	register long __cu_len_r __asm__ ("$6");			\
									\
	__cu_to_r = (to);						\
	__cu_from_r = (from);						\
	__cu_len_r = (n);						\
	__asm__ __volatile__(						\
	__MODULE_JAL(__copy_user)					\
	: "+r" (__cu_to_r), "+r" (__cu_from_r), "+r" (__cu_len_r)	\
	:								\
	: "$8", "$9", "$10", "$11", "$12", "$15", "$24", "$31",		\
	  "memory");							\
	__cu_len_r;							\
})

/*
 * __copy_to_user: - Copy a block of data into user space, with less checking.
 * @to:   Destination address, in user space.
 * @from: Source address, in kernel space.
 * @n:    Number of bytes to copy.
 *
 * Context: User context only.  This function may sleep.
 *
 * Copy data from kernel space to user space.  Caller must check
 * the specified block with access_ok() before calling this function.
 *
 * Returns number of bytes that could not be copied.
 * On success, this will be zero.
 */
#define __copy_to_user(to,from,n)					\
({									\
	void *__cu_to;							\
	const void *__cu_from;						\
	long __cu_len;							\
									\
	__cu_to = (to);							\
	__cu_from = (from);						\
	__cu_len = (n);							\
	__cu_len = __invoke_copy_to_user(__cu_to, __cu_from, __cu_len);	\
	__cu_len;							\
})

/*
 * copy_to_user: - Copy a block of data into user space.
 * @to:   Destination address, in user space.
 * @from: Source address, in kernel space.
 * @n:    Number of bytes to copy.
 *
 * Context: User context only.  This function may sleep.
 *
 * Copy data from kernel space to user space.
 *
 * Returns number of bytes that could not be copied.
 * On success, this will be zero.
 */
#define copy_to_user(to,from,n)						\
({									\
	void *__cu_to;							\
	const void *__cu_from;						\
	long __cu_len;							\
									\
	__cu_to = (to);							\
	__cu_from = (from);						\
	__cu_len = (n);							\
	if (access_ok(VERIFY_WRITE, __cu_to, __cu_len))			\
		__cu_len = __invoke_copy_to_user(__cu_to, __cu_from,	\
		                                 __cu_len);		\
	__cu_len;							\
})

#define __invoke_copy_from_user(to,from,n)				\
({									\
	register void *__cu_to_r __asm__ ("$4");			\
	register const void *__cu_from_r __asm__ ("$5");		\
	register long __cu_len_r __asm__ ("$6");			\
									\
	__cu_to_r = (to);						\
	__cu_from_r = (from);						\
	__cu_len_r = (n);						\
	__asm__ __volatile__(						\
	".set\tnoreorder\n\t"						\
	__MODULE_JAL(__copy_user)					\
	".set\tnoat\n\t"						\
	"addu\t$1, %1, %2\n\t"						\
	".set\tat\n\t"							\
	".set\treorder\n\t"						\
	: "+r" (__cu_to_r), "+r" (__cu_from_r), "+r" (__cu_len_r)	\
	:								\
	: "$8", "$9", "$10", "$11", "$12", "$15", "$24", "$31",		\
	  "memory");							\
	__cu_len_r;							\
})

/*
 * __copy_from_user: - Copy a block of data from user space, with less checking. * @to:   Destination address, in kernel space.
 * @from: Source address, in user space.
 * @n:    Number of bytes to copy.
 *
 * Context: User context only.  This function may sleep.
 *
 * Copy data from user space to kernel space.  Caller must check
 * the specified block with access_ok() before calling this function.
 *
 * Returns number of bytes that could not be copied.
 * On success, this will be zero.
 *
 * If some data could not be copied, this function will pad the copied
 * data to the requested size using zero bytes.
 */
#define __copy_from_user(to,from,n)					\
({									\
	void *__cu_to;							\
	const void *__cu_from;						\
	long __cu_len;							\
									\
	__cu_to = (to);							\
	__cu_from = (from);						\
	__cu_len = (n);							\
	__cu_len = __invoke_copy_from_user(__cu_to, __cu_from,		\
	                                   __cu_len);			\
	__cu_len;							\
})

/*
 * copy_from_user: - Copy a block of data from user space.
 * @to:   Destination address, in kernel space.
 * @from: Source address, in user space.
 * @n:    Number of bytes to copy.
 *
 * Context: User context only.  This function may sleep.
 *
 * Copy data from user space to kernel space.
 *
 * Returns number of bytes that could not be copied.
 * On success, this will be zero.
 *
 * If some data could not be copied, this function will pad the copied
 * data to the requested size using zero bytes.
 */
#define copy_from_user(to,from,n)					\
({									\
	void *__cu_to;							\
	const void *__cu_from;						\
	long __cu_len;							\
									\
	__cu_to = (to);							\
	__cu_from = (from);						\
	__cu_len = (n);							\
	if (access_ok(VERIFY_READ, __cu_from, __cu_len))		\
		__cu_len = __invoke_copy_from_user(__cu_to, __cu_from,	\
		                                   __cu_len);		\
	__cu_len;							\
})

/*
 * __clear_user: - Zero a block of memory in user space, with less checking.
 * @to:   Destination address, in user space.
 * @n:    Number of bytes to zero.
 *
 * Zero a block of memory in user space.  Caller must check
 * the specified block with access_ok() before calling this function.
 *
 * Returns number of bytes that could not be cleared.
 * On success, this will be zero.
 */
static inline __kernel_size_t
__clear_user(void *addr, __kernel_size_t size)
{
	__kernel_size_t res;

	__asm__ __volatile__(
		"move\t$4, %1\n\t"
		"move\t$5, $0\n\t"
		"move\t$6, %2\n\t"
		__MODULE_JAL(__bzero)
		"move\t%0, $6"
		: "=r" (res)
		: "r" (addr), "r" (size)
		: "$4", "$5", "$6", "$8", "$9", "$31");

	return res;
}

#define clear_user(addr,n)						\
({									\
	void * __cl_addr = (addr);					\
	unsigned long __cl_size = (n);					\
	if (__cl_size && access_ok(VERIFY_WRITE,			\
		((unsigned long)(__cl_addr)), __cl_size))		\
		__cl_size = __clear_user(__cl_addr, __cl_size);		\
	__cl_size;							\
})

/*
 * __strncpy_from_user: - Copy a NUL terminated string from userspace, with less checking.
 * @dst:   Destination address, in kernel space.  This buffer must be at
 *         least @count bytes long.
 * @src:   Source address, in user space.
 * @count: Maximum number of bytes to copy, including the trailing NUL.
 *
 * Copies a NUL-terminated string from userspace to kernel space.
 * Caller must check the specified block with access_ok() before calling
 * this function.
 *
 * On success, returns the length of the string (not including the trailing
 * NUL).
 *
 * If access to userspace fails, returns -EFAULT (some data may have been
 * copied).
 *
 * If @count is smaller than the length of the string, copies @count bytes
 * and returns @count.
 */
static inline long
__strncpy_from_user(char *__to, const char *__from, long __len)
{
	long res;

	__asm__ __volatile__(
		"move\t$4, %1\n\t"
		"move\t$5, %2\n\t"
		"move\t$6, %3\n\t"
		__MODULE_JAL(__strncpy_from_user_nocheck_asm)
		"move\t%0, $2"
		: "=r" (res)
		: "r" (__to), "r" (__from), "r" (__len)
		: "$2", "$3", "$4", "$5", "$6", "$8", "$31", "memory");

	return res;
}

/*
 * strncpy_from_user: - Copy a NUL terminated string from userspace.
 * @dst:   Destination address, in kernel space.  This buffer must be at
 *         least @count bytes long.
 * @src:   Source address, in user space.
 * @count: Maximum number of bytes to copy, including the trailing NUL.
 *
 * Copies a NUL-terminated string from userspace to kernel space.
 *
 * On success, returns the length of the string (not including the trailing
 * NUL).
 *
 * If access to userspace fails, returns -EFAULT (some data may have been
 * copied).
 *
 * If @count is smaller than the length of the string, copies @count bytes
 * and returns @count.
 */
static inline long
strncpy_from_user(char *__to, const char *__from, long __len)
{
	long res;

	__asm__ __volatile__(
		"move\t$4, %1\n\t"
		"move\t$5, %2\n\t"
		"move\t$6, %3\n\t"
		__MODULE_JAL(__strncpy_from_user_asm)
		"move\t%0, $2"
		: "=r" (res)
		: "r" (__to), "r" (__from), "r" (__len)
		: "$2", "$3", "$4", "$5", "$6", "$8", "$31", "memory");

	return res;
}

/* Returns: 0 if bad, string length+1 (memory size) of string if ok */
static inline long __strlen_user(const char *s)
{
	long res;

	__asm__ __volatile__(
		"move\t$4, %1\n\t"
		__MODULE_JAL(__strlen_user_nocheck_asm)
		"move\t%0, $2"
		: "=r" (res)
		: "r" (s)
		: "$2", "$4", "$8", "$31");

	return res;
}

/*
 * strlen_user: - Get the size of a string in user space.
 * @str: The string to measure.
 *
 * Context: User context only.  This function may sleep.
 *
 * Get the size of a NUL-terminated string in user space.
 *
 * Returns the size of the string INCLUDING the terminating NUL.
 * On exception, returns 0.
 *
 * If there is a limit on the length of a valid string, you may wish to
 * consider using strnlen_user() instead.
 */
static inline long strlen_user(const char *s)
{
	long res;

	__asm__ __volatile__(
		"move\t$4, %1\n\t"
		__MODULE_JAL(__strlen_user_asm)
		"move\t%0, $2"
		: "=r" (res)
		: "r" (s)
		: "$2", "$4", "$8", "$31");

	return res;
}

/*
 * strlen_user: - Get the size of a string in user space.
 * @str: The string to measure.
 * @n:   The maximum valid length
 *
 * Get the size of a NUL-terminated string in user space.
 *
 * Returns the size of the string INCLUDING the terminating NUL.
 * On exception, returns 0.
 * If the string is too long, returns a value greater than @n.
 */
static inline long strnlen_user(const char *s, long n)
{
	long res;

	__asm__ __volatile__(
		"move\t$4, %1\n\t"
		"move\t$5, %2\n\t"
		__MODULE_JAL(__strnlen_user_asm)
		"move\t%0, $2"
		: "=r" (res)
		: "r" (s), "r" (n)
		: "$2", "$4", "$5", "$8", "$31");

	return res;
}

struct exception_table_entry
{
	unsigned long insn;
	unsigned long nextinsn;
};

/* Returns 0 if exception not found and fixup.unit otherwise.  */
extern unsigned long search_exception_table(unsigned long addr);

/* Returns the new pc */
#define fixup_exception(map_reg, fixup_unit, pc)			\
({									\
	fixup_unit;							\
})

#endif /* _ASM_UACCESS_H */
