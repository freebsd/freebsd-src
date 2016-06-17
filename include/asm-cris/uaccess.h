/* 
 * Authors:    Bjorn Wesen (bjornw@axis.com)
 *	       Hans-Peter Nilsson (hp@axis.com)
 *
 * $Log: uaccess.h,v $
 * Revision 1.12  2003/06/17 14:00:42  starvik
 * Merge of Linux 2.4.21
 *
 * Revision 1.11  2003/06/04 19:36:45  hp
 * Remove unused copy-pasted register clobber from __asm_clear
 *
 * Revision 1.10  2003/04/09 08:22:38  pkj
 * Typo correction (taken from Linux 2.5).
 *
 * Revision 1.9  2002/11/20 18:20:17  hp
 * Make all static inline functions extern inline.
 *
 * Revision 1.8  2001/10/29 13:01:48  bjornw
 * Removed unused variable tmp2 in strnlen_user
 *
 * Revision 1.7  2001/10/02 12:44:52  hp
 * Add support for 64-bit put_user/get_user
 *
 * Revision 1.6  2001/10/01 14:51:17  bjornw
 * Added register prefixes and removed underscores
 *
 * Revision 1.5  2000/10/25 03:33:21  hp
 * - Provide implementation for everything else but get_user and put_user;
 *   copying inline to/from user for constant length 0..16, 20, 24, and
 *   clearing for 0..4, 8, 12, 16, 20, 24, strncpy_from_user and strnlen_user
 *   always inline.
 * - Constraints for destination addr in get_user cannot be memory, only reg.
 * - Correct labels for PC at expected fault points.
 * - Nits with assembly code.
 * - Don't use statement expressions without value; use "do {} while (0)".
 * - Return correct values from __generic_... functions.
 *
 * Revision 1.4  2000/09/12 16:28:25  bjornw
 * * Removed comments from the get/put user asm code
 * * Constrains for destination addr in put_user cannot be memory, only reg
 *
 * Revision 1.3  2000/09/12 14:30:20  bjornw
 * MAX_ADDR_USER does not exist anymore
 *
 * Revision 1.2  2000/07/13 15:52:48  bjornw
 * New user-access functions
 *
 * Revision 1.1.1.1  2000/07/10 16:32:31  bjornw
 * CRIS architecture, working draft
 *
 *
 *
 */

/* Asm:s have been tweaked (within the domain of correctness) to give
   satisfactory results for "gcc version 2.96 20000427 (experimental)".

   Check regularly...

   Register $r9 is chosen for temporaries, being a call-clobbered register
   first in line to be used (notably for local blocks), not colliding with
   parameter registers.  */

#ifndef _CRIS_UACCESS_H
#define _CRIS_UACCESS_H

#ifndef __ASSEMBLY__
#include <linux/sched.h>
#include <linux/errno.h>
#include <asm/processor.h>
#include <asm/page.h>

#define VERIFY_READ	0
#define VERIFY_WRITE	1

/*
 * The fs value determines whether argument validity checking should be
 * performed or not.  If get_fs() == USER_DS, checking is performed, with
 * get_fs() == KERNEL_DS, checking is bypassed.
 *
 * For historical reasons, these macros are grossly misnamed.
 */

#define MAKE_MM_SEG(s)	((mm_segment_t) { (s) })

/* addr_limit is the maximum accessible address for the task. we misuse
 * the KERNEL_DS and USER_DS values to both assign and compare the 
 * addr_limit values through the equally misnamed get/set_fs macros.
 * (see above)
 */

#define KERNEL_DS	MAKE_MM_SEG(0xFFFFFFFF)
#define USER_DS		MAKE_MM_SEG(TASK_SIZE)

#define get_ds()	(KERNEL_DS)
#define get_fs()	(current->addr_limit)
#define set_fs(x)	(current->addr_limit = (x))

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


/*
 * These are the main single-value transfer routines.  They automatically
 * use the right size if we just have the right pointer type.
 *
 * This gets kind of ugly. We want to return _two_ values in "get_user()"
 * and yet we don't want to do any pointers, because that is too much
 * of a performance impact. Thus we have a few rather ugly macros here,
 * and hide all the ugliness from the user.
 *
 * The "__xxx" versions of the user access functions are versions that
 * do not verify the address space, that must have been done previously
 * with a separate "access_ok()" call (this is used when we do multiple
 * accesses to the same area of user memory).
 *
 * As we use the same address space for kernel and user data on
 * CRIS, we can just do these as direct assignments.  (Of course, the
 * exception handling means that it's no longer "just"...)
 */
#define get_user(x,ptr) \
  __get_user_check((x),(ptr),sizeof(*(ptr)))
#define put_user(x,ptr) \
  __put_user_check((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)))

#define __get_user(x,ptr) \
  __get_user_nocheck((x),(ptr),sizeof(*(ptr)))
#define __put_user(x,ptr) \
  __put_user_nocheck((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)))

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
	  case 1: __put_user_asm(x,ptr,retval,"move.b"); break;	\
	  case 2: __put_user_asm(x,ptr,retval,"move.w"); break;	\
	  case 4: __put_user_asm(x,ptr,retval,"move.d"); break;	\
	  case 8: __put_user_asm_64(x,ptr,retval); break;	\
	  default: __put_user_bad();				\
	}							\
} while (0)

struct __large_struct { unsigned long buf[100]; };
#define __m(x) (*(struct __large_struct *)(x))

/*
 * We don't tell gcc that we are accessing memory, but this is OK
 * because we do not write to any memory gcc knows about, so there
 * are no aliasing issues.
 *
 * Note that PC at a fault is the address *after* the faulting
 * instruction.
 */
#define __put_user_asm(x, addr, err, op)			\
	__asm__ __volatile__(					\
		"	"op" %1,[%2]\n"				\
		"2:\n"						\
		"	.section .fixup,\"ax\"\n"		\
		"3:	move.d %3,%0\n"				\
		"	jump 2b\n"				\
		"	.previous\n"				\
		"	.section __ex_table,\"a\"\n"		\
		"	.dword 2b,3b\n"				\
		"	.previous\n"				\
		: "=r" (err)					\
		: "r" (x), "r" (addr), "g" (-EFAULT), "0" (err))

#define __put_user_asm_64(x, addr, err)				\
	__asm__ __volatile__(					\
		"	move.d %M1,[%2]\n"			\
		"2:	move.d %H1,[%2+4]\n"			\
		"4:\n"						\
		"	.section .fixup,\"ax\"\n"		\
		"3:	move.d %3,%0\n"				\
		"	jump 4b\n"				\
		"	.previous\n"				\
		"	.section __ex_table,\"a\"\n"		\
		"	.dword 2b,3b\n"				\
		"	.dword 4b,3b\n"				\
		"	.previous\n"				\
		: "=r" (err)					\
		: "r" (x), "r" (addr), "g" (-EFAULT), "0" (err))


#define __get_user_nocheck(x,ptr,size)				\
({								\
	long __gu_err, __gu_val;				\
	__get_user_size(__gu_val,(ptr),(size),__gu_err);	\
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

extern long __get_user_bad(void);

#define __get_user_size(x,ptr,size,retval)			\
do {								\
	retval = 0;						\
	switch (size) {						\
	  case 1: __get_user_asm(x,ptr,retval,"move.b"); break;	\
	  case 2: __get_user_asm(x,ptr,retval,"move.w"); break;	\
	  case 4: __get_user_asm(x,ptr,retval,"move.d"); break;	\
	  case 8: __get_user_asm_64(x,ptr,retval); break;	\
	  default: (x) = __get_user_bad();			\
	}							\
} while (0)

/* See comment before __put_user_asm.  */

#define __get_user_asm(x, addr, err, op)		\
	__asm__ __volatile__(				\
		"	"op" [%2],%1\n"			\
		"2:\n"					\
		"	.section .fixup,\"ax\"\n"	\
		"3:	move.d %3,%0\n"			\
		"	moveq 0,%1\n"			\
		"	jump 2b\n"			\
		"	.previous\n"			\
		"	.section __ex_table,\"a\"\n"	\
		"	.dword 2b,3b\n"			\
		"	.previous\n"			\
		: "=r" (err), "=r" (x)			\
		: "r" (addr), "g" (-EFAULT), "0" (err))

#define __get_user_asm_64(x, addr, err)			\
	__asm__ __volatile__(				\
		"	move.d [%2],%M1\n"		\
		"2:	move.d [%2+4],%H1\n"		\
		"4:\n"					\
		"	.section .fixup,\"ax\"\n"	\
		"3:	move.d %3,%0\n"			\
		"	moveq 0,%1\n"			\
		"	jump 4b\n"			\
		"	.previous\n"			\
		"	.section __ex_table,\"a\"\n"	\
		"	.dword 2b,3b\n"			\
		"	.dword 4b,3b\n"			\
		"	.previous\n"			\
		: "=r" (err), "=r" (x)			\
		: "r" (addr), "g" (-EFAULT), "0" (err))

/* More complex functions.  Most are inline, but some call functions that
   live in lib/usercopy.c  */

extern unsigned long __copy_user(void *to, const void *from, unsigned long n);
extern unsigned long __copy_user_zeroing(void *to, const void *from, unsigned long n);
extern unsigned long __do_clear_user(void *to, unsigned long n);

/*
 * Copy a null terminated string from userspace.
 *
 * Must return:
 * -EFAULT		for an exception
 * count		if we hit the buffer limit
 * bytes copied		if we hit a null byte
 * (without the null byte)
 */

extern inline long         
__do_strncpy_from_user(char *dst, const char *src, long count)
{
	long res;

	if (count == 0)
		return 0;

	/*
	 * Currently, in 2.4.0-test9, most ports use a simple byte-copy loop.
	 *  So do we.
	 *
	 *  This code is deduced from:
	 *
	 *	char tmp2;
	 *	long tmp1, tmp3	
	 *	tmp1 = count;
	 *	while ((*dst++ = (tmp2 = *src++)) != 0
	 *	       && --tmp1)
	 *	  ;
	 *
	 *	res = count - tmp1;
	 *
	 *  with tweaks.
	 */

	__asm__ __volatile__ (
		"	move.d %3,%0\n"
		"	move.b [%2+],$r9\n"
		"1:	beq 2f\n"
		"	move.b $r9,[%1+]\n"

		"	subq 1,%0\n"
		"	bne 1b\n"
		"	move.b [%2+],$r9\n"

		"2:	sub.d %3,%0\n"
		"	neg.d %0,%0\n"
		"3:\n"
		"	.section .fixup,\"ax\"\n"
		"4:	move.d %7,%0\n"
		"	jump 3b\n"

		/* There's one address for a fault at the first move, and
		   two possible PC values for a fault at the second move,
		   being a delay-slot filler.  However, the branch-target
		   for the second move is the same as the first address.
		   Just so you don't get confused...  */
		"	.previous\n"
		"	.section __ex_table,\"a\"\n"
		"	.dword 1b,4b\n"
		"	.dword 2b,4b\n"
		"	.previous"
		: "=r" (res), "=r" (dst), "=r" (src), "=r" (count)
		: "3" (count), "1" (dst), "2" (src), "g" (-EFAULT)
		: "r9");

	return res;
}

extern inline unsigned long
__generic_copy_to_user(void *to, const void *from, unsigned long n)
{
	if (access_ok(VERIFY_WRITE, to, n))
		return __copy_user(to,from,n);
	return n;
}

extern inline unsigned long
__generic_copy_from_user(void *to, const void *from, unsigned long n)
{
	if (access_ok(VERIFY_READ, from, n))
		return __copy_user_zeroing(to,from,n);
	return n;
}

extern inline unsigned long
__generic_clear_user(void *to, unsigned long n)
{
	if (access_ok(VERIFY_WRITE, to, n))
		return __do_clear_user(to,n);
	return n;
}

extern inline long
__strncpy_from_user(char *dst, const char *src, long count)
{
	return __do_strncpy_from_user(dst, src, count);
}

extern inline long
strncpy_from_user(char *dst, const char *src, long count)
{
	long res = -EFAULT;
	if (access_ok(VERIFY_READ, src, 1))
		res = __do_strncpy_from_user(dst, src, count);
	return res;
}

/* A few copy asms to build up the more complex ones from.

   Note again, a post-increment is performed regardless of whether a bus
   fault occurred in that instruction, and PC for a faulted insn is the
   address *after* the insn.  */

#define __asm_copy_user_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm__ __volatile__ (				\
			COPY				\
		"1:\n"					\
		"	.section .fixup,\"ax\"\n"	\
			FIXUP				\
		"	jump 1b\n"			\
		"	.previous\n"			\
		"	.section __ex_table,\"a\"\n"	\
			TENTRY				\
		"	.previous\n"			\
		: "=r" (to), "=r" (from), "=r" (ret)	\
		: "0" (to), "1" (from), "2" (ret)	\
		: "r9", "memory")

#define __asm_copy_from_user_1(to, from, ret) \
	__asm_copy_user_cont(to, from, ret,	\
		"	move.b [%1+],$r9\n"	\
		"2:	move.b $r9,[%0+]\n",	\
		"3:	addq 1,%2\n"		\
		"	clear.b [%0+]\n",	\
		"	.dword 2b,3b\n")

#define __asm_copy_from_user_2x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_user_cont(to, from, ret,		\
		"	move.w [%1+],$r9\n"		\
		"2:	move.w $r9,[%0+]\n" COPY,	\
		"3:	addq 2,%2\n"			\
		"	clear.w [%0+]\n" FIXUP,		\
		"	.dword 2b,3b\n" TENTRY)

#define __asm_copy_from_user_2(to, from, ret) \
	__asm_copy_from_user_2x_cont(to, from, ret, "", "", "")

#define __asm_copy_from_user_3(to, from, ret)		\
	__asm_copy_from_user_2x_cont(to, from, ret,	\
		"	move.b [%1+],$r9\n"		\
		"4:	move.b $r9,[%0+]\n",		\
		"5:	addq 1,%2\n"			\
		"	clear.b [%0+]\n",		\
		"	.dword 4b,5b\n")

#define __asm_copy_from_user_4x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_user_cont(to, from, ret,		\
		"	move.d [%1+],$r9\n"		\
		"2:	move.d $r9,[%0+]\n" COPY,	\
		"3:	addq 4,%2\n"			\
		"	clear.d [%0+]\n" FIXUP,		\
		"	.dword 2b,3b\n" TENTRY)

#define __asm_copy_from_user_4(to, from, ret) \
	__asm_copy_from_user_4x_cont(to, from, ret, "", "", "")

#define __asm_copy_from_user_5(to, from, ret) \
	__asm_copy_from_user_4x_cont(to, from, ret,	\
		"	move.b [%1+],$r9\n"		\
		"4:	move.b $r9,[%0+]\n",		\
		"5:	addq 1,%2\n"			\
		"	clear.b [%0+]\n",		\
		"	.dword 4b,5b\n")

#define __asm_copy_from_user_6x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_from_user_4x_cont(to, from, ret,	\
		"	move.w [%1+],$r9\n"		\
		"4:	move.w $r9,[%0+]\n" COPY,	\
		"5:	addq 2,%2\n"			\
		"	clear.w [%0+]\n" FIXUP,		\
		"	.dword 4b,5b\n" TENTRY)

#define __asm_copy_from_user_6(to, from, ret) \
	__asm_copy_from_user_6x_cont(to, from, ret, "", "", "")

#define __asm_copy_from_user_7(to, from, ret) \
	__asm_copy_from_user_6x_cont(to, from, ret,	\
		"	move.b [%1+],$r9\n"		\
		"6:	move.b $r9,[%0+]\n",		\
		"7:	addq 1,%2\n"			\
		"	clear.b [%0+]\n",		\
		"	.dword 6b,7b\n")

#define __asm_copy_from_user_8x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_from_user_4x_cont(to, from, ret,	\
		"	move.d [%1+],$r9\n"		\
		"4:	move.d $r9,[%0+]\n" COPY,	\
		"5:	addq 4,%2\n"			\
		"	clear.d [%0+]\n" FIXUP,		\
		"	.dword 4b,5b\n" TENTRY)

#define __asm_copy_from_user_8(to, from, ret) \
	__asm_copy_from_user_8x_cont(to, from, ret, "", "", "")

#define __asm_copy_from_user_9(to, from, ret) \
	__asm_copy_from_user_8x_cont(to, from, ret,	\
		"	move.b [%1+],$r9\n"		\
		"6:	move.b $r9,[%0+]\n",		\
		"7:	addq 1,%2\n"			\
		"	clear.b [%0+]\n",		\
		"	.dword 6b,7b\n")

#define __asm_copy_from_user_10x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_from_user_8x_cont(to, from, ret,	\
		"	move.w [%1+],$r9\n"		\
		"6:	move.w $r9,[%0+]\n" COPY,	\
		"7:	addq 2,%2\n"			\
		"	clear.w [%0+]\n" FIXUP,		\
		"	.dword 6b,7b\n" TENTRY)

#define __asm_copy_from_user_10(to, from, ret) \
	__asm_copy_from_user_10x_cont(to, from, ret, "", "", "")

#define __asm_copy_from_user_11(to, from, ret)		\
	__asm_copy_from_user_10x_cont(to, from, ret,	\
		"	move.b [%1+],$r9\n"		\
		"8:	move.b $r9,[%0+]\n",		\
		"9:	addq 1,%2\n"			\
		"	clear.b [%0+]\n",		\
		"	.dword 8b,9b\n")

#define __asm_copy_from_user_12x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_from_user_8x_cont(to, from, ret,	\
		"	move.d [%1+],$r9\n"		\
		"6:	move.d $r9,[%0+]\n" COPY,	\
		"7:	addq 4,%2\n"			\
		"	clear.d [%0+]\n" FIXUP,		\
		"	.dword 6b,7b\n" TENTRY)

#define __asm_copy_from_user_12(to, from, ret) \
	__asm_copy_from_user_12x_cont(to, from, ret, "", "", "")

#define __asm_copy_from_user_13(to, from, ret) \
	__asm_copy_from_user_12x_cont(to, from, ret,	\
		"	move.b [%1+],$r9\n"		\
		"8:	move.b $r9,[%0+]\n",		\
		"9:	addq 1,%2\n"			\
		"	clear.b [%0+]\n",		\
		"	.dword 8b,9b\n")

#define __asm_copy_from_user_14x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_from_user_12x_cont(to, from, ret,	\
		"	move.w [%1+],$r9\n"		\
		"8:	move.w $r9,[%0+]\n" COPY,	\
		"9:	addq 2,%2\n"			\
		"	clear.w [%0+]\n" FIXUP,		\
		"	.dword 8b,9b\n" TENTRY)

#define __asm_copy_from_user_14(to, from, ret) \
	__asm_copy_from_user_14x_cont(to, from, ret, "", "", "")

#define __asm_copy_from_user_15(to, from, ret) \
	__asm_copy_from_user_14x_cont(to, from, ret,	\
		"	move.b [%1+],$r9\n"		\
		"10:	move.b $r9,[%0+]\n",		\
		"11:	addq 1,%2\n"			\
		"	clear.b [%0+]\n",		\
		"	.dword 10b,11b\n")

#define __asm_copy_from_user_16x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_from_user_12x_cont(to, from, ret,	\
		"	move.d [%1+],$r9\n"		\
		"8:	move.d $r9,[%0+]\n" COPY,	\
		"9:	addq 4,%2\n"			\
		"	clear.d [%0+]\n" FIXUP,		\
		"	.dword 8b,9b\n" TENTRY)

#define __asm_copy_from_user_16(to, from, ret) \
	__asm_copy_from_user_16x_cont(to, from, ret, "", "", "")

#define __asm_copy_from_user_20x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_from_user_16x_cont(to, from, ret,	\
		"	move.d [%1+],$r9\n"		\
		"10:	move.d $r9,[%0+]\n" COPY,	\
		"11:	addq 4,%2\n"			\
		"	clear.d [%0+]\n" FIXUP,		\
		"	.dword 10b,11b\n" TENTRY)

#define __asm_copy_from_user_20(to, from, ret) \
	__asm_copy_from_user_20x_cont(to, from, ret, "", "", "")

#define __asm_copy_from_user_24x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_from_user_20x_cont(to, from, ret,	\
		"	move.d [%1+],$r9\n"		\
		"12:	move.d $r9,[%0+]\n" COPY,	\
		"13:	addq 4,%2\n"			\
		"	clear.d [%0+]\n" FIXUP,		\
		"	.dword 12b,13b\n" TENTRY)

#define __asm_copy_from_user_24(to, from, ret) \
	__asm_copy_from_user_24x_cont(to, from, ret, "", "", "")

/* And now, the to-user ones.  */

#define __asm_copy_to_user_1(to, from, ret)	\
	__asm_copy_user_cont(to, from, ret,	\
		"	move.b [%1+],$r9\n"	\
		"	move.b $r9,[%0+]\n2:\n",	\
		"3:	addq 1,%2\n",		\
		"	.dword 2b,3b\n")

#define __asm_copy_to_user_2x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_user_cont(to, from, ret,		\
		"	move.w [%1+],$r9\n"		\
		"	move.w $r9,[%0+]\n2:\n" COPY,	\
		"3:	addq 2,%2\n" FIXUP,		\
		"	.dword 2b,3b\n" TENTRY)

#define __asm_copy_to_user_2(to, from, ret) \
	__asm_copy_to_user_2x_cont(to, from, ret, "", "", "")

#define __asm_copy_to_user_3(to, from, ret) \
	__asm_copy_to_user_2x_cont(to, from, ret,	\
		"	move.b [%1+],$r9\n"		\
		"	move.b $r9,[%0+]\n4:\n",		\
		"5:	addq 1,%2\n",			\
		"	.dword 4b,5b\n")

#define __asm_copy_to_user_4x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_user_cont(to, from, ret,		\
		"	move.d [%1+],$r9\n"		\
		"	move.d $r9,[%0+]\n2:\n" COPY,	\
		"3:	addq 4,%2\n" FIXUP,		\
		"	.dword 2b,3b\n" TENTRY)

#define __asm_copy_to_user_4(to, from, ret) \
	__asm_copy_to_user_4x_cont(to, from, ret, "", "", "")

#define __asm_copy_to_user_5(to, from, ret) \
	__asm_copy_to_user_4x_cont(to, from, ret,	\
		"	move.b [%1+],$r9\n"		\
		"	move.b $r9,[%0+]\n4:\n",		\
		"5:	addq 1,%2\n",			\
		"	.dword 4b,5b\n")

#define __asm_copy_to_user_6x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_to_user_4x_cont(to, from, ret,	\
		"	move.w [%1+],$r9\n"		\
		"	move.w $r9,[%0+]\n4:\n" COPY,	\
		"5:	addq 2,%2\n" FIXUP,		\
		"	.dword 4b,5b\n" TENTRY)

#define __asm_copy_to_user_6(to, from, ret) \
	__asm_copy_to_user_6x_cont(to, from, ret, "", "", "")

#define __asm_copy_to_user_7(to, from, ret) \
	__asm_copy_to_user_6x_cont(to, from, ret,	\
		"	move.b [%1+],$r9\n"		\
		"	move.b $r9,[%0+]\n6:\n",		\
		"7:	addq 1,%2\n",			\
		"	.dword 6b,7b\n")

#define __asm_copy_to_user_8x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_to_user_4x_cont(to, from, ret,	\
		"	move.d [%1+],$r9\n"		\
		"	move.d $r9,[%0+]\n4:\n" COPY,	\
		"5:	addq 4,%2\n"  FIXUP,		\
		"	.dword 4b,5b\n" TENTRY)

#define __asm_copy_to_user_8(to, from, ret) \
	__asm_copy_to_user_8x_cont(to, from, ret, "", "", "")

#define __asm_copy_to_user_9(to, from, ret) \
	__asm_copy_to_user_8x_cont(to, from, ret,	\
		"	move.b [%1+],$r9\n"		\
		"	move.b $r9,[%0+]\n6:\n",		\
		"7:	addq 1,%2\n",			\
		"	.dword 6b,7b\n")

#define __asm_copy_to_user_10x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_to_user_8x_cont(to, from, ret,	\
		"	move.w [%1+],$r9\n"		\
		"	move.w $r9,[%0+]\n6:\n" COPY,	\
		"7:	addq 2,%2\n" FIXUP,		\
		"	.dword 6b,7b\n" TENTRY)

#define __asm_copy_to_user_10(to, from, ret) \
	__asm_copy_to_user_10x_cont(to, from, ret, "", "", "")

#define __asm_copy_to_user_11(to, from, ret) \
	__asm_copy_to_user_10x_cont(to, from, ret,	\
		"	move.b [%1+],$r9\n"		\
		"	move.b $r9,[%0+]\n8:\n",		\
		"9:	addq 1,%2\n",			\
		"	.dword 8b,9b\n")

#define __asm_copy_to_user_12x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_to_user_8x_cont(to, from, ret,	\
		"	move.d [%1+],$r9\n"		\
		"	move.d $r9,[%0+]\n6:\n" COPY,	\
		"7:	addq 4,%2\n" FIXUP,		\
		"	.dword 6b,7b\n" TENTRY)

#define __asm_copy_to_user_12(to, from, ret) \
	__asm_copy_to_user_12x_cont(to, from, ret, "", "", "")

#define __asm_copy_to_user_13(to, from, ret) \
	__asm_copy_to_user_12x_cont(to, from, ret,	\
		"	move.b [%1+],$r9\n"		\
		"	move.b $r9,[%0+]\n8:\n",		\
		"9:	addq 1,%2\n",			\
		"	.dword 8b,9b\n")

#define __asm_copy_to_user_14x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_to_user_12x_cont(to, from, ret,	\
		"	move.w [%1+],$r9\n"		\
		"	move.w $r9,[%0+]\n8:\n" COPY,	\
		"9:	addq 2,%2\n" FIXUP,		\
		"	.dword 8b,9b\n" TENTRY)

#define __asm_copy_to_user_14(to, from, ret)	\
	__asm_copy_to_user_14x_cont(to, from, ret, "", "", "")

#define __asm_copy_to_user_15(to, from, ret) \
	__asm_copy_to_user_14x_cont(to, from, ret,	\
		"	move.b [%1+],$r9\n"		\
		"	move.b $r9,[%0+]\n10:\n",		\
		"11:	addq 1,%2\n",			\
		"	.dword 10b,11b\n")

#define __asm_copy_to_user_16x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_to_user_12x_cont(to, from, ret,	\
		"	move.d [%1+],$r9\n"		\
		"	move.d $r9,[%0+]\n8:\n" COPY,	\
		"9:	addq 4,%2\n" FIXUP,		\
		"	.dword 8b,9b\n" TENTRY)

#define __asm_copy_to_user_16(to, from, ret) \
	__asm_copy_to_user_16x_cont(to, from, ret, "", "", "")

#define __asm_copy_to_user_20x_cont(to, from, ret, COPY, FIXUP, TENTRY) \
	__asm_copy_to_user_16x_cont(to, from, ret,	\
		"	move.d [%1+],$r9\n"		\
		"	move.d $r9,[%0+]\n10:\n" COPY,	\
		"11:	addq 4,%2\n" FIXUP,		\
		"	.dword 10b,11b\n" TENTRY)

#define __asm_copy_to_user_20(to, from, ret) \
	__asm_copy_to_user_20x_cont(to, from, ret, "", "", "")

#define __asm_copy_to_user_24x_cont(to, from, ret, COPY, FIXUP, TENTRY)	\
	__asm_copy_to_user_20x_cont(to, from, ret,	\
		"	move.d [%1+],$r9\n"		\
		"	move.d $r9,[%0+]\n12:\n" COPY,	\
		"13:	addq 4,%2\n" FIXUP,		\
		"	.dword 12b,13b\n" TENTRY)

#define __asm_copy_to_user_24(to, from, ret)	\
	__asm_copy_to_user_24x_cont(to, from, ret, "", "", "")

/* Define a few clearing asms with exception handlers.  */

/* This frame-asm is like the __asm_copy_user_cont one, but has one less
   input.  */

#define __asm_clear(to, ret, CLEAR, FIXUP, TENTRY) \
	__asm__ __volatile__ (				\
			CLEAR				\
		"1:\n"					\
		"	.section .fixup,\"ax\"\n"	\
			FIXUP				\
		"	jump 1b\n"			\
		"	.previous\n"			\
		"	.section __ex_table,\"a\"\n"	\
			TENTRY				\
		"	.previous"			\
		: "=r" (to), "=r" (ret)			\
		: "0" (to), "1" (ret)			\
		: "memory")

#define __asm_clear_1(to, ret) \
	__asm_clear(to, ret,			\
		"	clear.b [%0+]\n2:\n",	\
		"3:	addq 1,%1\n",		\
		"	.dword 2b,3b\n")

#define __asm_clear_2(to, ret) \
	__asm_clear(to, ret,			\
		"	clear.w [%0+]\n2:\n",	\
		"3:	addq 2,%1\n",		\
		"	.dword 2b,3b\n")

#define __asm_clear_3(to, ret) \
     __asm_clear(to, ret,			\
		 "	clear.w [%0+]\n"	\
		 "2:	clear.b [%0+]\n3:\n",	\
		 "4:	addq 2,%1\n"		\
		 "5:	addq 1,%1\n",		\
		 "	.dword 2b,4b\n"		\
		 "	.dword 3b,5b\n")

#define __asm_clear_4x_cont(to, ret, CLEAR, FIXUP, TENTRY) \
	__asm_clear(to, ret,				\
		"	clear.d [%0+]\n2:\n" CLEAR,	\
		"3:	addq 4,%1\n" FIXUP,		\
		"	.dword 2b,3b\n" TENTRY)

#define __asm_clear_4(to, ret) \
	__asm_clear_4x_cont(to, ret, "", "", "")

#define __asm_clear_8x_cont(to, ret, CLEAR, FIXUP, TENTRY) \
	__asm_clear_4x_cont(to, ret,			\
		"	clear.d [%0+]\n4:\n" CLEAR,	\
		"5:	addq 4,%1\n" FIXUP,		\
		"	.dword 4b,5b\n" TENTRY)

#define __asm_clear_8(to, ret) \
	__asm_clear_8x_cont(to, ret, "", "", "")

#define __asm_clear_12x_cont(to, ret, CLEAR, FIXUP, TENTRY) \
	__asm_clear_8x_cont(to, ret,			\
		"	clear.d [%0+]\n6:\n" CLEAR,	\
		"7:	addq 4,%1\n" FIXUP,		\
		"	.dword 6b,7b\n" TENTRY)

#define __asm_clear_12(to, ret) \
	__asm_clear_12x_cont(to, ret, "", "", "")

#define __asm_clear_16x_cont(to, ret, CLEAR, FIXUP, TENTRY) \
	__asm_clear_12x_cont(to, ret,			\
		"	clear.d [%0+]\n8:\n" CLEAR,	\
		"9:	addq 4,%1\n" FIXUP,		\
		"	.dword 8b,9b\n" TENTRY)

#define __asm_clear_16(to, ret) \
	__asm_clear_16x_cont(to, ret, "", "", "")

#define __asm_clear_20x_cont(to, ret, CLEAR, FIXUP, TENTRY) \
	__asm_clear_16x_cont(to, ret,			\
		"	clear.d [%0+]\n10:\n" CLEAR,	\
		"11:	addq 4,%1\n" FIXUP,		\
		"	.dword 10b,11b\n" TENTRY)

#define __asm_clear_20(to, ret) \
	__asm_clear_20x_cont(to, ret, "", "", "")

#define __asm_clear_24x_cont(to, ret, CLEAR, FIXUP, TENTRY) \
	__asm_clear_20x_cont(to, ret,			\
		"	clear.d [%0+]\n12:\n" CLEAR,	\
		"13:	addq 4,%1\n" FIXUP,		\
		"	.dword 12b,13b\n" TENTRY)

#define __asm_clear_24(to, ret) \
	__asm_clear_24x_cont(to, ret, "", "", "")

/* Note that if these expand awfully if made into switch constructs, so
   don't do that.  */

extern inline unsigned long
__constant_copy_from_user(void *to, const void *from, unsigned long n)
{
	unsigned long ret = 0;
	if (n == 0)
		;
	else if (n == 1)
		__asm_copy_from_user_1(to, from, ret);
	else if (n == 2)
		__asm_copy_from_user_2(to, from, ret);
	else if (n == 3)
		__asm_copy_from_user_3(to, from, ret);
	else if (n == 4)
		__asm_copy_from_user_4(to, from, ret);
	else if (n == 5)
		__asm_copy_from_user_5(to, from, ret);
	else if (n == 6)
		__asm_copy_from_user_6(to, from, ret);
	else if (n == 7)
		__asm_copy_from_user_7(to, from, ret);
	else if (n == 8)
		__asm_copy_from_user_8(to, from, ret);
	else if (n == 9)
		__asm_copy_from_user_9(to, from, ret);
	else if (n == 10)
		__asm_copy_from_user_10(to, from, ret);
	else if (n == 11)
		__asm_copy_from_user_11(to, from, ret);
	else if (n == 12)
		__asm_copy_from_user_12(to, from, ret);
	else if (n == 13)
		__asm_copy_from_user_13(to, from, ret);
	else if (n == 14)
		__asm_copy_from_user_14(to, from, ret);
	else if (n == 15)
		__asm_copy_from_user_15(to, from, ret);
	else if (n == 16)
		__asm_copy_from_user_16(to, from, ret);
	else if (n == 20)
		__asm_copy_from_user_20(to, from, ret);
	else if (n == 24)
		__asm_copy_from_user_24(to, from, ret);
	else
		ret = __generic_copy_from_user(to, from, n);

	return ret;
}

/* Ditto, don't make a switch out of this.  */

extern inline unsigned long
__constant_copy_to_user(void *to, const void *from, unsigned long n)
{
	unsigned long ret = 0;
	if (n == 0)
		;
	else if (n == 1)
		__asm_copy_to_user_1(to, from, ret);
	else if (n == 2)
		__asm_copy_to_user_2(to, from, ret);
	else if (n == 3)
		__asm_copy_to_user_3(to, from, ret);
	else if (n == 4)
		__asm_copy_to_user_4(to, from, ret);
	else if (n == 5)
		__asm_copy_to_user_5(to, from, ret);
	else if (n == 6)
		__asm_copy_to_user_6(to, from, ret);
	else if (n == 7)
		__asm_copy_to_user_7(to, from, ret);
	else if (n == 8)
		__asm_copy_to_user_8(to, from, ret);
	else if (n == 9)
		__asm_copy_to_user_9(to, from, ret);
	else if (n == 10)
		__asm_copy_to_user_10(to, from, ret);
	else if (n == 11)
		__asm_copy_to_user_11(to, from, ret);
	else if (n == 12)
		__asm_copy_to_user_12(to, from, ret);
	else if (n == 13)
		__asm_copy_to_user_13(to, from, ret);
	else if (n == 14)
		__asm_copy_to_user_14(to, from, ret);
	else if (n == 15)
		__asm_copy_to_user_15(to, from, ret);
	else if (n == 16)
		__asm_copy_to_user_16(to, from, ret);
	else if (n == 20)
		__asm_copy_to_user_20(to, from, ret);
	else if (n == 24)
		__asm_copy_to_user_24(to, from, ret);
	else
		ret = __generic_copy_to_user(to, from, n);

	return ret;
}

/* No switch, please.  */

extern inline unsigned long
__constant_clear_user(void *to, unsigned long n)
{
	unsigned long ret = 0;
	if (n == 0)
		;
	else if (n == 1)
		__asm_clear_1(to, ret);
	else if (n == 2)
		__asm_clear_2(to, ret);
	else if (n == 3)
		__asm_clear_3(to, ret);
	else if (n == 4)
		__asm_clear_4(to, ret);
	else if (n == 8)
		__asm_clear_8(to, ret);
	else if (n == 12)
		__asm_clear_12(to, ret);
	else if (n == 16)
		__asm_clear_16(to, ret);
	else if (n == 20)
		__asm_clear_20(to, ret);
	else if (n == 24)
		__asm_clear_24(to, ret);
	else
		ret = __generic_clear_user(to, n);

	return ret;
}


#define clear_user(to, n)			\
(__builtin_constant_p(n) ?			\
 __constant_clear_user(to, n) :			\
 __generic_clear_user(to, n))

#define copy_from_user(to, from, n)		\
(__builtin_constant_p(n) ?			\
 __constant_copy_from_user(to, from, n) :	\
 __generic_copy_from_user(to, from, n))

#define copy_to_user(to, from, n)		\
(__builtin_constant_p(n) ?			\
 __constant_copy_to_user(to, from, n) :		\
 __generic_copy_to_user(to, from, n))

/* We let the __ versions of copy_from/to_user inline, because they're often
 * used in fast paths and have only a small space overhead.
 */

extern inline unsigned long
__generic_copy_from_user_nocheck(void *to, const void *from, unsigned long n)
{
	return __copy_user_zeroing(to,from,n);
}

extern inline unsigned long
__generic_copy_to_user_nocheck(void *to, const void *from, unsigned long n)
{
	return __copy_user(to,from,n);
}

extern inline unsigned long
__generic_clear_user_nocheck(void *to, unsigned long n)
{
	return __do_clear_user(to,n);
}

/* without checking */

#define __copy_to_user(to,from,n)   __generic_copy_to_user_nocheck((to),(from),(n))
#define __copy_from_user(to,from,n) __generic_copy_from_user_nocheck((to),(from),(n))
#define __clear_user(to,n) __generic_clear_user_nocheck((to),(n))

/*
 * Return the size of a string (including the ending 0)
 *
 * Return length of string in userspace including terminating 0
 * or 0 for error.  Return a value greater than N if too long.
 */

extern inline long
strnlen_user(const char *s, long n)
{
	long res, tmp1;

	if (!access_ok(VERIFY_READ, s, 0))
		return 0;

	/*
	 * This code is deduced from:
	 *
	 *	tmp1 = n;
	 *	while (tmp1-- > 0 && *s++)
	 *	  ;
	 *
	 *	res = n - tmp1;
	 *
	 *  (with tweaks).
	 */

	__asm__ __volatile__ (
		"	move.d %1,$r9\n"
		"0:\n"
		"	ble 1f\n"
		"	subq 1,$r9\n"

		"	test.b [%0+]\n"
		"	bne 0b\n"
		"	test.d $r9\n"
		"1:\n"
		"	move.d %1,%0\n"
		"	sub.d $r9,%0\n"
		"2:\n"
		"	.section .fixup,\"ax\"\n"

		"3:	clear.d %0\n"
		"	jump 2b\n"

		/* There's one address for a fault at the first move, and
		   two possible PC values for a fault at the second move,
		   being a delay-slot filler.  However, the branch-target
		   for the second move is the same as the first address.
		   Just so you don't get confused...  */
		"	.previous\n"
		"	.section __ex_table,\"a\"\n"
		"	.dword 0b,3b\n"
		"	.dword 1b,3b\n"
		"	.previous\n"
		: "=r" (res), "=r" (tmp1)
		: "0" (s), "1" (n)
		: "r9");

	return res;
}

#define strlen_user(str)	strnlen_user((str), 0x7ffffffe)

#endif  /* __ASSEMBLY__ */

#endif	/* _CRIS_UACCESS_H */
