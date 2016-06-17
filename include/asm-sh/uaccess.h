/* $Id: uaccess.h,v 1.1.1.1.2.4 2002/08/28 16:52:43 gniibe Exp $
 *
 * User space memory access functions
 *
 * Copyright (C) 1999  Niibe Yutaka
 *
 *  Based on:
 *     MIPS implementation version 1.15 by
 *              Copyright (C) 1996, 1997, 1998 by Ralf Baechle
 *     and i386 version.
 */
#ifndef __ASM_SH_UACCESS_H
#define __ASM_SH_UACCESS_H

#include <linux/errno.h>
#include <linux/sched.h>

#define VERIFY_READ    0
#define VERIFY_WRITE   1

/*
 * The fs value determines whether argument validity checking should be
 * performed or not.  If get_fs() == USER_DS, checking is performed, with
 * get_fs() == KERNEL_DS, checking is bypassed.
 *
 * For historical reasons (Data Segment Register?), these macros are misnamed.
 */

#define MAKE_MM_SEG(s)	((mm_segment_t) { (s) })

#define KERNEL_DS	MAKE_MM_SEG(0xFFFFFFFF)
#define USER_DS		MAKE_MM_SEG(0x80000000)

#define get_ds()	(KERNEL_DS)
#define get_fs()        (current->addr_limit)
#define set_fs(x)       (current->addr_limit=(x))

#define segment_eq(a,b)	((a).seg == (b).seg)

#define __addr_ok(addr) ((unsigned long)(addr) < (current->addr_limit.seg))

/*
 * Uhhuh, this needs 33-bit arithmetic. We have a carry..
 *
 * sum := addr + size;  carry? --> flag = true;
 * if (sum >= addr_limit) flag = true;
 */
#define __range_ok(addr,size) ({					      \
	unsigned long flag,sum; 					      \
	__asm__("clrt; addc %3, %1; movt %0; cmp/hi %4, %1; rotcl %0"	      \
		:"=&r" (flag), "=r" (sum) 				      \
		:"1" (addr), "r" ((int)(size)), "r" (current->addr_limit.seg) \
		:"t"); 							      \
	flag; })

#define access_ok(type,addr,size) (__range_ok(addr,size) == 0)
#define __access_ok(addr,size) (__range_ok(addr,size) == 0)

static inline int verify_area(int type, const void * addr, unsigned long size)
{
	return access_ok(type,addr,size) ? 0 : -EFAULT;
}

/*
 * Uh, these should become the main single-value transfer routines ...
 * They automatically use the right size if we just have the right
 * pointer type ...
 *
 * As SuperH uses the same address space for kernel and user data, we
 * can just do these as direct assignments.
 *
 * Careful to not
 * (a) re-use the arguments for side effects (sizeof is ok)
 * (b) require any knowledge of processes at this stage
 */
#define put_user(x,ptr)	__put_user_check((x),(ptr),sizeof(*(ptr)))
#define get_user(x,ptr) __get_user_check((x),(ptr),sizeof(*(ptr)))

/*
 * The "__xxx" versions do not do address space checking, useful when
 * doing multiple accesses to the same area (the user has to do the
 * checks by hand with "access_ok()")
 */
#define __put_user(x,ptr) __put_user_nocheck((x),(ptr),sizeof(*(ptr)))
#define __get_user(x,ptr) __get_user_nocheck((x),(ptr),sizeof(*(ptr)))

struct __large_struct { unsigned long buf[100]; };
#define __m(x) (*(struct __large_struct *)(x))

#define __get_user_nocheck(x,ptr,size) ({ \
long __gu_err; \
__typeof(*(ptr)) __gu_val; \
long __gu_addr; \
__asm__("":"=r" (__gu_val)); \
__gu_addr = (long) (ptr); \
__asm__("":"=r" (__gu_err)); \
switch (size) { \
case 1: __get_user_asm("b"); break; \
case 2: __get_user_asm("w"); break; \
case 4: __get_user_asm("l"); break; \
default: __get_user_unknown(); break; \
} x = (__typeof__(*(ptr))) __gu_val; __gu_err; })

#define __get_user_check(x,ptr,size) ({ \
long __gu_err = -EFAULT; \
__typeof__(*(ptr)) __gu_val; \
long __gu_addr; \
__asm__("":"=r" (__gu_val)); \
__gu_addr = (long) (ptr); \
if (__access_ok(__gu_addr,size)) { \
switch (size) { \
case 1: __get_user_asm("b"); break; \
case 2: __get_user_asm("w"); break; \
case 4: __get_user_asm("l"); break; \
default: __get_user_unknown(); break; \
} } x = (__typeof__(*(ptr))) __gu_val; __gu_err; })

#define __get_user_asm(insn) \
({ \
__asm__ __volatile__( \
	"1:\n\t" \
	"mov." insn "	%2, %1\n\t" \
	"mov	#0, %0\n" \
	"2:\n" \
	".section	.fixup,\"ax\"\n" \
	"3:\n\t" \
	"mov	#0, %1\n\t" \
	"mov.l	4f, %0\n\t" \
	"jmp	@%0\n\t" \
	" mov	%3, %0\n" \
	"4:	.long	2b\n\t" \
	".previous\n" \
	".section	__ex_table,\"a\"\n\t" \
	".long	1b, 3b\n\t" \
	".previous" \
	:"=&r" (__gu_err), "=&r" (__gu_val) \
	:"m" (__m(__gu_addr)), "i" (-EFAULT)); })

extern void __get_user_unknown(void);

#define __put_user_nocheck(x,ptr,size) ({ \
long __pu_err; \
__typeof__(*(ptr)) __pu_val; \
long __pu_addr; \
__pu_val = (x); \
__pu_addr = (long) (ptr); \
__asm__("":"=r" (__pu_err)); \
switch (size) { \
case 1: __put_user_asm("b"); break; \
case 2: __put_user_asm("w"); break; \
case 4: __put_user_asm("l"); break; \
case 8: __put_user_u64(__pu_val,__pu_addr,__pu_err); break; \
default: __put_user_unknown(); break; \
} __pu_err; })

#define __put_user_check(x,ptr,size) ({ \
long __pu_err = -EFAULT; \
__typeof__(*(ptr)) __pu_val; \
long __pu_addr; \
__pu_val = (x); \
__pu_addr = (long) (ptr); \
if (__access_ok(__pu_addr,size)) { \
switch (size) { \
case 1: __put_user_asm("b"); break; \
case 2: __put_user_asm("w"); break; \
case 4: __put_user_asm("l"); break; \
case 8: __put_user_u64(__pu_val,__pu_addr,__pu_err); break; \
default: __put_user_unknown(); break; \
} } __pu_err; })

#define __put_user_asm(insn) \
({ \
__asm__ __volatile__( \
	"1:\n\t" \
	"mov." insn "	%1, %2\n\t" \
	"mov	#0, %0\n" \
	"2:\n" \
	".section	.fixup,\"ax\"\n" \
	"3:\n\t" \
	"nop\n\t" \
	"mov.l	4f, %0\n\t" \
	"jmp	@%0\n\t" \
	"mov	%3, %0\n" \
	"4:	.long	2b\n\t" \
	".previous\n" \
	".section	__ex_table,\"a\"\n\t" \
	".long	1b, 3b\n\t" \
	".previous" \
	:"=&r" (__pu_err) \
	:"r" (__pu_val), "m" (__m(__pu_addr)), "i" (-EFAULT) \
        :"memory"); })

#if defined(__LITTLE_ENDIAN__)
#define __put_user_u64(val,addr,retval) \
({ \
__asm__ __volatile__( \
	"1:\n\t" \
	"mov.l	%R1,%2\n\t" \
	"mov.l	%S1,%T2\n\t" \
	"mov	#0,%0\n" \
	"2:\n" \
	".section	.fixup,\"ax\"\n" \
	"3:\n\t" \
	"nop\n\t" \
	"mov.l	4f,%0\n\t" \
	"jmp	@%0\n\t" \
	" mov	%3,%0\n" \
	"4:	.long	2b\n\t" \
	".previous\n" \
	".section	__ex_table,\"a\"\n\t" \
	".long	1b, 3b\n\t" \
	".previous" \
	: "=r" (retval) \
	: "r" (val), "m" (__m(addr)), "i" (-EFAULT) \
        : "memory"); })
#else
#define __put_user_u64(val,addr,retval) \
({ \
__asm__ __volatile__( \
	"1:\n\t" \
	"mov.l	%S1,%2\n\t" \
	"mov.l	%R1,%T2\n\t" \
	"mov	#0,%0\n" \
	"2:\n" \
	".section	.fixup,\"ax\"\n" \
	"3:\n\t" \
	"nop\n\t" \
	"mov.l	4f,%0\n\t" \
	"jmp	@%0\n\t" \
	" mov	%3,%0\n" \
	"4:	.long	2b\n\t" \
	".previous\n" \
	".section	__ex_table,\"a\"\n\t" \
	".long	1b, 3b\n\t" \
	".previous" \
	: "=r" (retval) \
	: "r" (val), "m" (__m(addr)), "i" (-EFAULT) \
        : "memory"); })
#endif

extern void __put_user_unknown(void);

/* Generic arbitrary sized copy.  */
/* Return the number of bytes NOT copied */
extern __kernel_size_t __copy_user(void *to, const void *from, __kernel_size_t n);

#define copy_to_user(to,from,n) ({ \
void *__copy_to = (void *) (to); \
__kernel_size_t __copy_size = (__kernel_size_t) (n); \
__kernel_size_t __copy_res; \
if(__copy_size && __access_ok((unsigned long)__copy_to, __copy_size)) { \
__copy_res = __copy_user(__copy_to, (void *) (from), __copy_size); \
} else __copy_res = __copy_size; \
__copy_res; })

#define __copy_to_user(to,from,n)		\
	__copy_user((void *)(to),		\
		    (void *)(from), n)

#define copy_from_user(to,from,n) ({ \
void *__copy_to = (void *) (to); \
void *__copy_from = (void *) (from); \
__kernel_size_t __copy_size = (__kernel_size_t) (n); \
__kernel_size_t __copy_res; \
if(__copy_size && __access_ok((unsigned long)__copy_from, __copy_size)) { \
__copy_res = __copy_user(__copy_to, __copy_from, __copy_size); \
} else __copy_res = __copy_size; \
__copy_res; })

#define __copy_from_user(to,from,n)		\
	__copy_user((void *)(to),		\
		    (void *)(from), n)

/*
 * Clear the area and return remaining number of bytes
 * (on failure.  Usually it's 0.)
 */
extern __kernel_size_t __clear_user(void *addr, __kernel_size_t size);

#define clear_user(addr,n) ({ \
void * __cl_addr = (addr); \
unsigned long __cl_size = (n); \
if (__cl_size && __access_ok(((unsigned long)(__cl_addr)), __cl_size)) \
__cl_size = __clear_user(__cl_addr, __cl_size); \
__cl_size; })

static __inline__ int
__strncpy_from_user(unsigned long __dest, unsigned long __src, int __count)
{
	__kernel_size_t res;
	unsigned long __dummy, _d, _s;

	__asm__ __volatile__(
		"9:\n"
		"mov.b	@%2+, %1\n\t"
		"cmp/eq	#0, %1\n\t"
		"bt/s	2f\n"
		"1:\n"
		"mov.b	%1, @%3\n\t"
		"dt	%7\n\t"
		"bf/s	9b\n\t"
		" add	#1, %3\n\t"
		"2:\n\t"
		"sub	%7, %0\n"
		"3:\n"
		".section .fixup,\"ax\"\n"
		"4:\n\t"
		"mov.l	5f, %1\n\t"
		"jmp	@%1\n\t"
		" mov	%8, %0\n\t"
		".balign 4\n"
		"5:	.long 3b\n"
		".previous\n"
		".section __ex_table,\"a\"\n"
		"	.balign 4\n"
		"	.long 9b,4b\n"
		".previous"
		: "=r" (res), "=&z" (__dummy), "=r" (_s), "=r" (_d)
		: "0" (__count), "2" (__src), "3" (__dest), "r" (__count),
		  "i" (-EFAULT)
		: "memory", "t");

	return res;
}

#define strncpy_from_user(dest,src,count) ({ \
unsigned long __sfu_src = (unsigned long) (src); \
int __sfu_count = (int) (count); \
long __sfu_res = -EFAULT; \
if(__access_ok(__sfu_src, __sfu_count)) { \
__sfu_res = __strncpy_from_user((unsigned long) (dest), __sfu_src, __sfu_count); \
} __sfu_res; })

/*
 * Return the size of a string (including the ending 0!)
 */
static __inline__ long __strnlen_user(const char *__s, long __n)
{
	unsigned long res;
	unsigned long __dummy;

	__asm__ __volatile__(
		"9:\n"
		"cmp/eq	%4, %0\n\t"
		"bt	2f\n"
		"1:\t"
		"mov.b	@(%0,%3), %1\n\t"
		"tst	%1, %1\n\t"
		"bf/s	9b\n\t"
		" add	#1, %0\n"
		"2:\n"
		".section .fixup,\"ax\"\n"
		"3:\n\t"
		"mov.l	4f, %1\n\t"
		"jmp	@%1\n\t"
		" mov	%5, %0\n"
		".balign 4\n"
		"4:	.long 2b\n"
		".previous\n"
		".section __ex_table,\"a\"\n"
		"	.balign 4\n"
		"	.long 1b,3b\n"
		".previous"
		: "=z" (res), "=&r" (__dummy)
		: "0" (0), "r" (__s), "r" (__n), "i" (-EFAULT)
		: "t");
	return res;
}

static __inline__ long strnlen_user(const char *s, long n)
{
	if (!access_ok(VERIFY_READ, s, n))
		return 0;
	else
		return __strnlen_user(s, n);
}

static __inline__ long strlen_user(const char *s)
{
	if (!access_ok(VERIFY_READ, s, 0))
		return 0;
	else
		return __strnlen_user(s, ~0UL >> 1);
}

struct exception_table_entry
{
	unsigned long insn, fixup;
};

/* Returns 0 if exception not found and fixup.unit otherwise.  */
extern unsigned long search_exception_table(unsigned long addr);

/* Returns the new pc */
#define fixup_exception(map_reg, fixup_unit, pc)                \
({                                                              \
	fixup_unit;                                             \
})

#endif /* __ASM_SH_UACCESS_H */
