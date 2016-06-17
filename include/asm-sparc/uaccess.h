/* $Id: uaccess.h,v 1.24 2001/10/30 04:32:24 davem Exp $
 * uaccess.h: User space memore access functions.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1996,1997 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */
#ifndef _ASM_UACCESS_H
#define _ASM_UACCESS_H

#ifdef __KERNEL__
#include <linux/sched.h>
#include <linux/string.h>
#include <asm/vac-ops.h>
#include <asm/a.out.h>
#endif

#ifndef __ASSEMBLY__

/* Sparc is not segmented, however we need to be able to fool verify_area()
 * when doing system calls from kernel mode legitimately.
 *
 * "For historical reasons, these macros are grossly misnamed." -Linus
 */

#define KERNEL_DS   ((mm_segment_t) { 0 })
#define USER_DS     ((mm_segment_t) { -1 })

#define VERIFY_READ	0
#define VERIFY_WRITE	1

#define get_ds()	(KERNEL_DS)
#define get_fs()	(current->thread.current_ds)
#define set_fs(val)	((current->thread.current_ds) = (val))

#define segment_eq(a,b)	((a).seg == (b).seg)

/* We have there a nice not-mapped page at PAGE_OFFSET - PAGE_SIZE, so that this test
 * can be fairly lightweight.
 * No one can read/write anything from userland in the kernel space by setting
 * large size and address near to PAGE_OFFSET - a fault will break his intentions.
 */
#define __user_ok(addr,size) ((addr) < STACK_TOP)
#define __kernel_ok (segment_eq(get_fs(), KERNEL_DS))
#define __access_ok(addr,size) (__user_ok((addr) & get_fs().seg,(size)))
#define access_ok(type,addr,size) __access_ok((unsigned long)(addr),(size))

extern inline int verify_area(int type, const void * addr, unsigned long size)
{
	return access_ok(type,addr,size)?0:-EFAULT;
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
 *
 * There is a special way how to put a range of potentially faulting
 * insns (like twenty ldd/std's with now intervening other instructions)
 * You specify address of first in insn and 0 in fixup and in the next
 * exception_table_entry you specify last potentially faulting insn + 1
 * and in fixup the routine which should handle the fault.
 * That fixup code will get
 * (faulting_insn_address - first_insn_in_the_range_address)/4
 * in %g2 (ie. index of the faulting instruction in the range).
 */

struct exception_table_entry
{
        unsigned long insn, fixup;
};

/* Returns 0 if exception not found and fixup otherwise.  */
extern unsigned long search_exception_table(unsigned long, unsigned long *);

extern void __ret_efault(void);

/* Uh, these should become the main single-value transfer routines..
 * They automatically use the right size if we just have the right
 * pointer type..
 *
 * This gets kind of ugly. We want to return _two_ values in "get_user()"
 * and yet we don't want to do any pointers, because that is too much
 * of a performance impact. Thus we have a few rather ugly macros here,
 * and hide all the uglyness from the user.
 */
#define put_user(x,ptr) ({ \
unsigned long __pu_addr = (unsigned long)(ptr); \
__put_user_check((__typeof__(*(ptr)))(x),__pu_addr,sizeof(*(ptr))); })

#define get_user(x,ptr) ({ \
unsigned long __gu_addr = (unsigned long)(ptr); \
__get_user_check((x),__gu_addr,sizeof(*(ptr)),__typeof__(*(ptr))); })

/*
 * The "__xxx" versions do not do address space checking, useful when
 * doing multiple accesses to the same area (the user has to do the
 * checks by hand with "access_ok()")
 */
#define __put_user(x,ptr) __put_user_nocheck((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)))
#define __get_user(x,ptr) __get_user_nocheck((x),(ptr),sizeof(*(ptr)),__typeof__(*(ptr)))

struct __large_struct { unsigned long buf[100]; };
#define __m(x) ((struct __large_struct *)(x))

#define __put_user_check(x,addr,size) ({ \
register int __pu_ret; \
if (__access_ok(addr,size)) { \
switch (size) { \
case 1: __put_user_asm(x,b,addr,__pu_ret); break; \
case 2: __put_user_asm(x,h,addr,__pu_ret); break; \
case 4: __put_user_asm(x,,addr,__pu_ret); break; \
case 8: __put_user_asm(x,d,addr,__pu_ret); break; \
default: __pu_ret = __put_user_bad(); break; \
} } else { __pu_ret = -EFAULT; } __pu_ret; })

#define __put_user_check_ret(x,addr,size,retval) ({ \
register int __foo __asm__ ("l1"); \
if (__access_ok(addr,size)) { \
switch (size) { \
case 1: __put_user_asm_ret(x,b,addr,retval,__foo); break; \
case 2: __put_user_asm_ret(x,h,addr,retval,__foo); break; \
case 4: __put_user_asm_ret(x,,addr,retval,__foo); break; \
case 8: __put_user_asm_ret(x,d,addr,retval,__foo); break; \
default: if (__put_user_bad()) return retval; break; \
} } else return retval; })

#define __put_user_nocheck(x,addr,size) ({ \
register int __pu_ret; \
switch (size) { \
case 1: __put_user_asm(x,b,addr,__pu_ret); break; \
case 2: __put_user_asm(x,h,addr,__pu_ret); break; \
case 4: __put_user_asm(x,,addr,__pu_ret); break; \
case 8: __put_user_asm(x,d,addr,__pu_ret); break; \
default: __pu_ret = __put_user_bad(); break; \
} __pu_ret; })

#define __put_user_nocheck_ret(x,addr,size,retval) ({ \
register int __foo __asm__ ("l1"); \
switch (size) { \
case 1: __put_user_asm_ret(x,b,addr,retval,__foo); break; \
case 2: __put_user_asm_ret(x,h,addr,retval,__foo); break; \
case 4: __put_user_asm_ret(x,,addr,retval,__foo); break; \
case 8: __put_user_asm_ret(x,d,addr,retval,__foo); break; \
default: if (__put_user_bad()) return retval; break; \
} })

#define __put_user_asm(x,size,addr,ret)					\
__asm__ __volatile__(							\
	"/* Put user asm, inline. */\n"					\
"1:\t"	"st"#size " %1, %2\n\t"						\
	"clr	%0\n"							\
"2:\n\n\t"								\
	".section .fixup,#alloc,#execinstr\n\t"				\
	".align	4\n"							\
"3:\n\t"								\
	"b	2b\n\t"							\
	" mov	%3, %0\n\t"						\
        ".previous\n\n\t"						\
	".section __ex_table,#alloc\n\t"				\
	".align	4\n\t"							\
	".word	1b, 3b\n\t"						\
	".previous\n\n\t"						\
       : "=&r" (ret) : "r" (x), "m" (*__m(addr)),			\
	 "i" (-EFAULT))

#define __put_user_asm_ret(x,size,addr,ret,foo)				\
if (__builtin_constant_p(ret) && ret == -EFAULT)			\
__asm__ __volatile__(							\
	"/* Put user asm ret, inline. */\n"				\
"1:\t"	"st"#size " %1, %2\n\n\t"					\
	".section __ex_table,#alloc\n\t"				\
	".align	4\n\t"							\
	".word	1b, __ret_efault\n\n\t"					\
	".previous\n\n\t"						\
       : "=r" (foo) : "r" (x), "m" (*__m(addr)));			\
else									\
__asm__ __volatile(							\
	"/* Put user asm ret, inline. */\n"				\
"1:\t"	"st"#size " %1, %2\n\n\t"					\
	".section .fixup,#alloc,#execinstr\n\t"				\
	".align	4\n"							\
"3:\n\t"								\
	"ret\n\t"							\
	" restore %%g0, %3, %%o0\n\t"					\
	".previous\n\n\t"						\
	".section __ex_table,#alloc\n\t"				\
	".align	4\n\t"							\
	".word	1b, 3b\n\n\t"						\
	".previous\n\n\t"						\
       : "=r" (foo) : "r" (x), "m" (*__m(addr)), "i" (ret))

extern int __put_user_bad(void);

#define __get_user_check(x,addr,size,type) ({ \
register int __gu_ret; \
register unsigned long __gu_val; \
if (__access_ok(addr,size)) { \
switch (size) { \
case 1: __get_user_asm(__gu_val,ub,addr,__gu_ret); break; \
case 2: __get_user_asm(__gu_val,uh,addr,__gu_ret); break; \
case 4: __get_user_asm(__gu_val,,addr,__gu_ret); break; \
case 8: __get_user_asm(__gu_val,d,addr,__gu_ret); break; \
default: __gu_val = 0; __gu_ret = __get_user_bad(); break; \
} } else { __gu_val = 0; __gu_ret = -EFAULT; } x = (type) __gu_val; __gu_ret; })

#define __get_user_check_ret(x,addr,size,type,retval) ({ \
register unsigned long __gu_val __asm__ ("l1"); \
if (__access_ok(addr,size)) { \
switch (size) { \
case 1: __get_user_asm_ret(__gu_val,ub,addr,retval); break; \
case 2: __get_user_asm_ret(__gu_val,uh,addr,retval); break; \
case 4: __get_user_asm_ret(__gu_val,,addr,retval); break; \
case 8: __get_user_asm_ret(__gu_val,d,addr,retval); break; \
default: if (__get_user_bad()) return retval; \
} x = (type) __gu_val; } else return retval; })

#define __get_user_nocheck(x,addr,size,type) ({ \
register int __gu_ret; \
register unsigned long __gu_val; \
switch (size) { \
case 1: __get_user_asm(__gu_val,ub,addr,__gu_ret); break; \
case 2: __get_user_asm(__gu_val,uh,addr,__gu_ret); break; \
case 4: __get_user_asm(__gu_val,,addr,__gu_ret); break; \
case 8: __get_user_asm(__gu_val,d,addr,__gu_ret); break; \
default: __gu_val = 0; __gu_ret = __get_user_bad(); break; \
} x = (type) __gu_val; __gu_ret; })

#define __get_user_nocheck_ret(x,addr,size,type,retval) ({ \
register unsigned long __gu_val __asm__ ("l1"); \
switch (size) { \
case 1: __get_user_asm_ret(__gu_val,ub,addr,retval); break; \
case 2: __get_user_asm_ret(__gu_val,uh,addr,retval); break; \
case 4: __get_user_asm_ret(__gu_val,,addr,retval); break; \
case 8: __get_user_asm_ret(__gu_val,d,addr,retval); break; \
default: if (__get_user_bad()) return retval; \
} x = (type) __gu_val; })

#define __get_user_asm(x,size,addr,ret)					\
__asm__ __volatile__(							\
	"/* Get user asm, inline. */\n"					\
"1:\t"	"ld"#size " %2, %1\n\t"						\
	"clr	%0\n"							\
"2:\n\n\t"								\
	".section .fixup,#alloc,#execinstr\n\t"				\
	".align	4\n"							\
"3:\n\t"								\
	"clr	%1\n\t"							\
	"b	2b\n\t"							\
	" mov	%3, %0\n\n\t"						\
	".previous\n\t"							\
	".section __ex_table,#alloc\n\t"				\
	".align	4\n\t"							\
	".word	1b, 3b\n\n\t"						\
	".previous\n\t"							\
       : "=&r" (ret), "=&r" (x) : "m" (*__m(addr)),			\
	 "i" (-EFAULT))

#define __get_user_asm_ret(x,size,addr,retval)				\
if (__builtin_constant_p(retval) && retval == -EFAULT)			\
__asm__ __volatile__(							\
	"/* Get user asm ret, inline. */\n"				\
"1:\t"	"ld"#size " %1, %0\n\n\t"					\
	".section __ex_table,#alloc\n\t"				\
	".align	4\n\t"							\
	".word	1b,__ret_efault\n\n\t"					\
	".previous\n\t"							\
       : "=&r" (x) : "m" (*__m(addr)));					\
else									\
__asm__ __volatile__(							\
	"/* Get user asm ret, inline. */\n"				\
"1:\t"	"ld"#size " %1, %0\n\n\t"					\
	".section .fixup,#alloc,#execinstr\n\t"				\
	".align	4\n"							\
"3:\n\t"								\
	"ret\n\t"							\
	" restore %%g0, %2, %%o0\n\n\t"					\
	".previous\n\t"							\
	".section __ex_table,#alloc\n\t"				\
	".align	4\n\t"							\
	".word	1b, 3b\n\n\t"						\
	".previous\n\t"							\
       : "=&r" (x) : "m" (*__m(addr)), "i" (retval))

extern int __get_user_bad(void);

extern __kernel_size_t __copy_user(void *to, void *from, __kernel_size_t size);

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

extern __inline__ __kernel_size_t __clear_user(void *addr, __kernel_size_t size)
{
  __kernel_size_t ret;
  __asm__ __volatile__ (
	".section __ex_table,#alloc\n\t"
	".align 4\n\t"
	".word 1f,3\n\t"
	".previous\n\t"
	"mov %2, %%o1\n"
	"1:\n\t"
	"call __bzero\n\t"
	" mov %1, %%o0\n\t"
	"mov %%o0, %0\n"
	: "=r" (ret) : "r" (addr), "r" (size) :
	"o0", "o1", "o2", "o3", "o4", "o5", "o7",
	"g1", "g2", "g3", "g4", "g5", "g7", "cc");
  return ret;
}

#define clear_user(addr,n) ({ \
void *__clear_addr = (void *) (addr); \
__kernel_size_t __clear_size = (__kernel_size_t) (n); \
__kernel_size_t __clear_res; \
if(__clear_size && __access_ok((unsigned long)__clear_addr, __clear_size)) { \
__clear_res = __clear_user(__clear_addr, __clear_size); \
} else __clear_res = __clear_size; \
__clear_res; })

extern int __strncpy_from_user(unsigned long dest, unsigned long src, int count);

#define strncpy_from_user(dest,src,count) ({ \
unsigned long __sfu_src = (unsigned long) (src); \
int __sfu_count = (int) (count); \
long __sfu_res = -EFAULT; \
if(__access_ok(__sfu_src, __sfu_count)) { \
__sfu_res = __strncpy_from_user((unsigned long) (dest), __sfu_src, __sfu_count); \
} __sfu_res; })

extern int __strlen_user(const char *);
extern int __strnlen_user(const char *, long len);

extern __inline__ int strlen_user(const char *str)
{
	if(!access_ok(VERIFY_READ, str, 0))
		return 0;
	else
		return __strlen_user(str);
}

extern __inline__ int strnlen_user(const char *str, long len)
{
	if(!access_ok(VERIFY_READ, str, 0))
		return 0;
	else
		return __strnlen_user(str, len);
}

#endif  /* __ASSEMBLY__ */

#endif /* _ASM_UACCESS_H */
