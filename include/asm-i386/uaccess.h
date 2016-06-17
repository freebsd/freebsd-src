#ifndef __i386_UACCESS_H
#define __i386_UACCESS_H

/*
 * User space memory access functions
 */
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/prefetch.h>
#include <asm/page.h>

#define VERIFY_READ 0
#define VERIFY_WRITE 1

/*
 * The fs value determines whether argument validity checking should be
 * performed or not.  If get_fs() == USER_DS, checking is performed, with
 * get_fs() == KERNEL_DS, checking is bypassed.
 *
 * For historical reasons, these macros are grossly misnamed.
 */

#define MAKE_MM_SEG(s)	((mm_segment_t) { (s) })


#define KERNEL_DS	MAKE_MM_SEG(0xFFFFFFFF)
#define USER_DS		MAKE_MM_SEG(PAGE_OFFSET)

#define get_ds()	(KERNEL_DS)
#define get_fs()	(current->addr_limit)
#define set_fs(x)	(current->addr_limit = (x))

#define segment_eq(a,b)	((a).seg == (b).seg)

extern int __verify_write(const void *, unsigned long);

#define __addr_ok(addr) ((unsigned long)(addr) < (current->addr_limit.seg))

/*
 * Test whether a block of memory is a valid user space address.
 * Returns 0 if the range is valid, nonzero otherwise.
 *
 * This is equivalent to the following test:
 * (u33)addr + (u33)size >= (u33)current->addr_limit.seg
 *
 * This needs 33-bit arithmetic. We have a carry...
 */
#define __range_ok(addr,size) ({ \
	unsigned long flag,sum; \
	asm("addl %3,%1 ; sbbl %0,%0; cmpl %1,%4; sbbl $0,%0" \
		:"=&r" (flag), "=r" (sum) \
		:"1" (addr),"g" ((int)(size)),"g" (current->addr_limit.seg)); \
	flag; })

#ifdef CONFIG_X86_WP_WORKS_OK

/**
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
#define access_ok(type,addr,size) (__range_ok(addr,size) == 0)

#else

#define access_ok(type,addr,size) ( (__range_ok(addr,size) == 0) && \
			 ((type) == VERIFY_READ || boot_cpu_data.wp_works_ok || \
			 segment_eq(get_fs(),KERNEL_DS) || \
			  __verify_write((void *)(addr),(size))))

#endif

/**
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
 * and hide all the uglyness from the user.
 *
 * The "__xxx" versions of the user access functions are versions that
 * do not verify the address space, that must have been done previously
 * with a separate "access_ok()" call (this is used when we do multiple
 * accesses to the same area of user memory).
 */

extern void __get_user_1(void);
extern void __get_user_2(void);
extern void __get_user_4(void);

#define __get_user_x(size,ret,x,ptr) \
	__asm__ __volatile__("call __get_user_" #size \
		:"=a" (ret),"=d" (x) \
		:"0" (ptr))


/* Careful: we have to cast the result to the type of the pointer for sign reasons */
/**
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
#define get_user(x,ptr)							\
({	int __ret_gu,__val_gu;						\
	switch(sizeof (*(ptr))) {					\
	case 1:  __get_user_x(1,__ret_gu,__val_gu,ptr); break;		\
	case 2:  __get_user_x(2,__ret_gu,__val_gu,ptr); break;		\
	case 4:  __get_user_x(4,__ret_gu,__val_gu,ptr); break;		\
	default: __get_user_x(X,__ret_gu,__val_gu,ptr); break;		\
	}								\
	(x) = (__typeof__(*(ptr)))__val_gu;				\
	__ret_gu;							\
})

extern void __put_user_1(void);
extern void __put_user_2(void);
extern void __put_user_4(void);
extern void __put_user_8(void);

extern void __put_user_bad(void);


/**
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
#define put_user(x,ptr)							\
  __put_user_check((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)))


/**
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
  __get_user_nocheck((x),(ptr),sizeof(*(ptr)))


/**
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

#define __put_user_nocheck(x,ptr,size)			\
({							\
	long __pu_err;					\
	__put_user_size((x),(ptr),(size),__pu_err);	\
	__pu_err;					\
})


#define __put_user_check(x,ptr,size)			\
({							\
	long __pu_err = -EFAULT;					\
	__typeof__(*(ptr)) *__pu_addr = (ptr);		\
	if (access_ok(VERIFY_WRITE,__pu_addr,size))	\
		__put_user_size((x),__pu_addr,(size),__pu_err);	\
	__pu_err;					\
})							

#define __put_user_u64(x, addr, err)				\
	__asm__ __volatile__(					\
		"1:	movl %%eax,0(%2)\n"			\
		"2:	movl %%edx,4(%2)\n"			\
		"3:\n"						\
		".section .fixup,\"ax\"\n"			\
		"4:	movl %3,%0\n"				\
		"	jmp 3b\n"				\
		".previous\n"					\
		".section __ex_table,\"a\"\n"			\
		"	.align 4\n"				\
		"	.long 1b,4b\n"				\
		"	.long 2b,4b\n"				\
		".previous"					\
		: "=r"(err)					\
		: "A" (x), "r" (addr), "i"(-EFAULT), "0"(err))

#define __put_user_size(x,ptr,size,retval)				\
do {									\
	retval = 0;							\
	switch (size) {							\
	  case 1: __put_user_asm(x,ptr,retval,"b","b","iq"); break;	\
	  case 2: __put_user_asm(x,ptr,retval,"w","w","ir"); break;	\
	  case 4: __put_user_asm(x,ptr,retval,"l","","ir"); break;	\
	  case 8: __put_user_u64(x,ptr,retval); break;			\
	  default: __put_user_bad();					\
	}								\
} while (0)

struct __large_struct { unsigned long buf[100]; };
#define __m(x) (*(struct __large_struct *)(x))

/*
 * Tell gcc we read from memory instead of writing: this is because
 * we do not write to any memory gcc knows about, so there are no
 * aliasing issues.
 */
#define __put_user_asm(x, addr, err, itype, rtype, ltype)	\
	__asm__ __volatile__(					\
		"1:	mov"itype" %"rtype"1,%2\n"		\
		"2:\n"						\
		".section .fixup,\"ax\"\n"			\
		"3:	movl %3,%0\n"				\
		"	jmp 2b\n"				\
		".previous\n"					\
		".section __ex_table,\"a\"\n"			\
		"	.align 4\n"				\
		"	.long 1b,3b\n"				\
		".previous"					\
		: "=r"(err)					\
		: ltype (x), "m"(__m(addr)), "i"(-EFAULT), "0"(err))


#define __get_user_nocheck(x,ptr,size)				\
({								\
	long __gu_err, __gu_val;				\
	__get_user_size(__gu_val,(ptr),(size),__gu_err);	\
	(x) = (__typeof__(*(ptr)))__gu_val;			\
	__gu_err;						\
})

extern long __get_user_bad(void);

#define __get_user_size(x,ptr,size,retval)				\
do {									\
	retval = 0;							\
	switch (size) {							\
	  case 1: __get_user_asm(x,ptr,retval,"b","b","=q"); break;	\
	  case 2: __get_user_asm(x,ptr,retval,"w","w","=r"); break;	\
	  case 4: __get_user_asm(x,ptr,retval,"l","","=r"); break;	\
	  default: (x) = __get_user_bad();				\
	}								\
} while (0)

#define __get_user_asm(x, addr, err, itype, rtype, ltype)	\
	__asm__ __volatile__(					\
		"1:	mov"itype" %2,%"rtype"1\n"		\
		"2:\n"						\
		".section .fixup,\"ax\"\n"			\
		"3:	movl %3,%0\n"				\
		"	xor"itype" %"rtype"1,%"rtype"1\n"	\
		"	jmp 2b\n"				\
		".previous\n"					\
		".section __ex_table,\"a\"\n"			\
		"	.align 4\n"				\
		"	.long 1b,3b\n"				\
		".previous"					\
		: "=r"(err), ltype (x)				\
		: "m"(__m(addr)), "i"(-EFAULT), "0"(err))


/*
 * Copy To/From Userspace
 */

/* Generic arbitrary sized copy.  */
#define __copy_user(to,from,size)					\
do {									\
	int __d0, __d1;							\
	__asm__ __volatile__(						\
		"0:	rep; movsl\n"					\
		"	movl %3,%0\n"					\
		"1:	rep; movsb\n"					\
		"2:\n"							\
		".section .fixup,\"ax\"\n"				\
		"3:	lea 0(%3,%0,4),%0\n"				\
		"	jmp 2b\n"					\
		".previous\n"						\
		".section __ex_table,\"a\"\n"				\
		"	.align 4\n"					\
		"	.long 0b,3b\n"					\
		"	.long 1b,2b\n"					\
		".previous"						\
		: "=&c"(size), "=&D" (__d0), "=&S" (__d1)		\
		: "r"(size & 3), "0"(size / 4), "1"(to), "2"(from)	\
		: "memory");						\
} while (0)

#define __copy_user_zeroing(to,from,size)				\
do {									\
	int __d0, __d1;							\
	__asm__ __volatile__(						\
		"0:	rep; movsl\n"					\
		"	movl %3,%0\n"					\
		"1:	rep; movsb\n"					\
		"2:\n"							\
		".section .fixup,\"ax\"\n"				\
		"3:	lea 0(%3,%0,4),%0\n"				\
		"4:	pushl %0\n"					\
		"	pushl %%eax\n"					\
		"	xorl %%eax,%%eax\n"				\
		"	rep; stosb\n"					\
		"	popl %%eax\n"					\
		"	popl %0\n"					\
		"	jmp 2b\n"					\
		".previous\n"						\
		".section __ex_table,\"a\"\n"				\
		"	.align 4\n"					\
		"	.long 0b,3b\n"					\
		"	.long 1b,4b\n"					\
		".previous"						\
		: "=&c"(size), "=&D" (__d0), "=&S" (__d1)		\
		: "r"(size & 3), "0"(size / 4), "1"(to), "2"(from)	\
		: "memory");						\
} while (0)

/* We let the __ versions of copy_from/to_user inline, because they're often
 * used in fast paths and have only a small space overhead.
 */
static inline unsigned long
__generic_copy_from_user_nocheck(void *to, const void *from, unsigned long n)
{
	__copy_user_zeroing(to,from,n);
	return n;
}

static inline unsigned long
__generic_copy_to_user_nocheck(void *to, const void *from, unsigned long n)
{
	__copy_user(to,from,n);
	return n;
}


/* Optimize just a little bit when we know the size of the move. */
#define __constant_copy_user(to, from, size)			\
do {								\
	int __d0, __d1;						\
	switch (size & 3) {					\
	default:						\
		__asm__ __volatile__(				\
			"0:	rep; movsl\n"			\
			"1:\n"					\
			".section .fixup,\"ax\"\n"		\
			"2:	shl $2,%0\n"			\
			"	jmp 1b\n"			\
			".previous\n"				\
			".section __ex_table,\"a\"\n"		\
			"	.align 4\n"			\
			"	.long 0b,2b\n"			\
			".previous"				\
			: "=c"(size), "=&S" (__d0), "=&D" (__d1)\
			: "1"(from), "2"(to), "0"(size/4)	\
			: "memory");				\
		break;						\
	case 1:							\
		__asm__ __volatile__(				\
			"0:	rep; movsl\n"			\
			"1:	movsb\n"			\
			"2:\n"					\
			".section .fixup,\"ax\"\n"		\
			"3:	shl $2,%0\n"			\
			"4:	incl %0\n"			\
			"	jmp 2b\n"			\
			".previous\n"				\
			".section __ex_table,\"a\"\n"		\
			"	.align 4\n"			\
			"	.long 0b,3b\n"			\
			"	.long 1b,4b\n"			\
			".previous"				\
			: "=c"(size), "=&S" (__d0), "=&D" (__d1)\
			: "1"(from), "2"(to), "0"(size/4)	\
			: "memory");				\
		break;						\
	case 2:							\
		__asm__ __volatile__(				\
			"0:	rep; movsl\n"			\
			"1:	movsw\n"			\
			"2:\n"					\
			".section .fixup,\"ax\"\n"		\
			"3:	shl $2,%0\n"			\
			"4:	addl $2,%0\n"			\
			"	jmp 2b\n"			\
			".previous\n"				\
			".section __ex_table,\"a\"\n"		\
			"	.align 4\n"			\
			"	.long 0b,3b\n"			\
			"	.long 1b,4b\n"			\
			".previous"				\
			: "=c"(size), "=&S" (__d0), "=&D" (__d1)\
			: "1"(from), "2"(to), "0"(size/4)	\
			: "memory");				\
		break;						\
	case 3:							\
		__asm__ __volatile__(				\
			"0:	rep; movsl\n"			\
			"1:	movsw\n"			\
			"2:	movsb\n"			\
			"3:\n"					\
			".section .fixup,\"ax\"\n"		\
			"4:	shl $2,%0\n"			\
			"5:	addl $2,%0\n"			\
			"6:	incl %0\n"			\
			"	jmp 3b\n"			\
			".previous\n"				\
			".section __ex_table,\"a\"\n"		\
			"	.align 4\n"			\
			"	.long 0b,4b\n"			\
			"	.long 1b,5b\n"			\
			"	.long 2b,6b\n"			\
			".previous"				\
			: "=c"(size), "=&S" (__d0), "=&D" (__d1)\
			: "1"(from), "2"(to), "0"(size/4)	\
			: "memory");				\
		break;						\
	}							\
} while (0)

/* Optimize just a little bit when we know the size of the move. */
#define __constant_copy_user_zeroing(to, from, size)		\
do {								\
	int __d0, __d1;						\
	switch (size & 3) {					\
	default:						\
		__asm__ __volatile__(				\
			"0:	rep; movsl\n"			\
			"1:\n"					\
			".section .fixup,\"ax\"\n"		\
			"2:	pushl %0\n"			\
			"	pushl %%eax\n"			\
			"	xorl %%eax,%%eax\n"		\
			"	rep; stosl\n"			\
			"	popl %%eax\n"			\
			"	popl %0\n"			\
			"	shl $2,%0\n"			\
			"	jmp 1b\n"			\
			".previous\n"				\
			".section __ex_table,\"a\"\n"		\
			"	.align 4\n"			\
			"	.long 0b,2b\n"			\
			".previous"				\
			: "=c"(size), "=&S" (__d0), "=&D" (__d1)\
			: "1"(from), "2"(to), "0"(size/4)	\
			: "memory");				\
		break;						\
	case 1:							\
		__asm__ __volatile__(				\
			"0:	rep; movsl\n"			\
			"1:	movsb\n"			\
			"2:\n"					\
			".section .fixup,\"ax\"\n"		\
			"3:	pushl %0\n"			\
			"	pushl %%eax\n"			\
			"	xorl %%eax,%%eax\n"		\
			"	rep; stosl\n"			\
			"	stosb\n"			\
			"	popl %%eax\n"			\
			"	popl %0\n"			\
			"	shl $2,%0\n"			\
			"	incl %0\n"			\
			"	jmp 2b\n"			\
			"4:	pushl %%eax\n"			\
			"	xorl %%eax,%%eax\n"		\
			"	stosb\n"			\
			"	popl %%eax\n"			\
			"	incl %0\n"			\
			"	jmp 2b\n"			\
			".previous\n"				\
			".section __ex_table,\"a\"\n"		\
			"	.align 4\n"			\
			"	.long 0b,3b\n"			\
			"	.long 1b,4b\n"			\
			".previous"				\
			: "=c"(size), "=&S" (__d0), "=&D" (__d1)\
			: "1"(from), "2"(to), "0"(size/4)	\
			: "memory");				\
		break;						\
	case 2:							\
		__asm__ __volatile__(				\
			"0:	rep; movsl\n"			\
			"1:	movsw\n"			\
			"2:\n"					\
			".section .fixup,\"ax\"\n"		\
			"3:	pushl %0\n"			\
			"	pushl %%eax\n"			\
			"	xorl %%eax,%%eax\n"		\
			"	rep; stosl\n"			\
			"	stosw\n"			\
			"	popl %%eax\n"			\
			"	popl %0\n"			\
			"	shl $2,%0\n"			\
			"	addl $2,%0\n"			\
			"	jmp 2b\n"			\
			"4:	pushl %%eax\n"			\
			"	xorl %%eax,%%eax\n"		\
			"	stosw\n"			\
			"	popl %%eax\n"			\
			"	addl $2,%0\n"			\
			"	jmp 2b\n"			\
			".previous\n"				\
			".section __ex_table,\"a\"\n"		\
			"	.align 4\n"			\
			"	.long 0b,3b\n"			\
			"	.long 1b,4b\n"			\
			".previous"				\
			: "=c"(size), "=&S" (__d0), "=&D" (__d1)\
			: "1"(from), "2"(to), "0"(size/4)	\
			: "memory");				\
		break;						\
	case 3:							\
		__asm__ __volatile__(				\
			"0:	rep; movsl\n"			\
			"1:	movsw\n"			\
			"2:	movsb\n"			\
			"3:\n"					\
			".section .fixup,\"ax\"\n"		\
			"4:	pushl %0\n"			\
			"	pushl %%eax\n"			\
			"	xorl %%eax,%%eax\n"		\
			"	rep; stosl\n"			\
			"	stosw\n"			\
			"	stosb\n"			\
			"	popl %%eax\n"			\
			"	popl %0\n"			\
			"	shl $2,%0\n"			\
			"	addl $3,%0\n"			\
			"	jmp 2b\n"			\
			"5:	pushl %%eax\n"			\
			"	xorl %%eax,%%eax\n"		\
			"	stosw\n"			\
			"	stosb\n"			\
			"	popl %%eax\n"			\
			"	addl $3,%0\n"			\
			"	jmp 2b\n"			\
			"6:	pushl %%eax\n"			\
			"	xorl %%eax,%%eax\n"		\
			"	stosb\n"			\
			"	popl %%eax\n"			\
			"	incl %0\n"			\
			"	jmp 3b\n"			\
			".previous\n"				\
			".section __ex_table,\"a\"\n"		\
			"	.align 4\n"			\
			"	.long 0b,4b\n"			\
			"	.long 1b,5b\n"			\
			"	.long 2b,6b\n"			\
			".previous"				\
			: "=c"(size), "=&S" (__d0), "=&D" (__d1)\
			: "1"(from), "2"(to), "0"(size/4)	\
			: "memory");				\
		break;						\
	}							\
} while (0)

unsigned long __generic_copy_to_user(void *, const void *, unsigned long);
unsigned long __generic_copy_from_user(void *, const void *, unsigned long);

static inline unsigned long
__constant_copy_to_user(void *to, const void *from, unsigned long n)
{
	prefetch(from);
	if (access_ok(VERIFY_WRITE, to, n))
		__constant_copy_user(to,from,n);
	return n;
}

static inline unsigned long
__constant_copy_from_user(void *to, const void *from, unsigned long n)
{
	if (access_ok(VERIFY_READ, from, n))
		__constant_copy_user_zeroing(to,from,n);
	else
		memset(to, 0, n);
	return n;
}

static inline unsigned long
__constant_copy_to_user_nocheck(void *to, const void *from, unsigned long n)
{
	__constant_copy_user(to,from,n);
	return n;
}

static inline unsigned long
__constant_copy_from_user_nocheck(void *to, const void *from, unsigned long n)
{
	__constant_copy_user_zeroing(to,from,n);
	return n;
}

/**
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
#define copy_to_user(to,from,n)				\
	(__builtin_constant_p(n) ?			\
	 __constant_copy_to_user((to),(from),(n)) :	\
	 __generic_copy_to_user((to),(from),(n)))

/**
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
#define copy_from_user(to,from,n)			\
	(__builtin_constant_p(n) ?			\
	 __constant_copy_from_user((to),(from),(n)) :	\
	 __generic_copy_from_user((to),(from),(n)))

/**
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
#define __copy_to_user(to,from,n)			\
	(__builtin_constant_p(n) ?			\
	 __constant_copy_to_user_nocheck((to),(from),(n)) :	\
	 __generic_copy_to_user_nocheck((to),(from),(n)))

/**
 * __copy_from_user: - Copy a block of data from user space, with less checking.
 * @to:   Destination address, in kernel space.
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
#define __copy_from_user(to,from,n)			\
	(__builtin_constant_p(n) ?			\
	 __constant_copy_from_user_nocheck((to),(from),(n)) :	\
	 __generic_copy_from_user_nocheck((to),(from),(n)))

long strncpy_from_user(char *dst, const char *src, long count);
long __strncpy_from_user(char *dst, const char *src, long count);

/**
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
#define strlen_user(str) strnlen_user(str, ~0UL >> 1)

long strnlen_user(const char *str, long n);
unsigned long clear_user(void *mem, unsigned long len);
unsigned long __clear_user(void *mem, unsigned long len);

#endif /* __i386_UACCESS_H */
