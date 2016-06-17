#ifndef __PARISC_UACCESS_H
#define __PARISC_UACCESS_H

/*
 * User space memory access functions
 */
#include <linux/sched.h>
#include <asm/page.h>
#include <asm/system.h>
#include <asm/cache.h>

#define VERIFY_READ 0
#define VERIFY_WRITE 1

#define KERNEL_DS	((mm_segment_t){0})
#define USER_DS 	((mm_segment_t){1})

#define segment_eq(a,b)	((a).seg == (b).seg)

#define get_ds()	(KERNEL_DS)
#define get_fs()	(current->addr_limit)
#define set_fs(x)	(current->addr_limit = (x))

/*
 * Note that since kernel addresses are in a separate address space on
 * parisc, we don't need to do anything for access_ok() or verify_area().
 * We just let the page fault handler do the right thing. This also means
 * that put_user is the same as __put_user, etc.
 */

#define access_ok(type,addr,size)   (1)
#define verify_area(type,addr,size) (0)

#define put_user __put_user
#define get_user __get_user

#if BITS_PER_LONG == 32
#define LDD_KERNEL(ptr)		BUG()
#define LDD_USER(ptr)		BUG()
#define STD_KERNEL(x, ptr) __put_kernel_asm64(x,ptr)
#define STD_USER(x, ptr) __put_user_asm64(x,ptr)
#else
#define LDD_KERNEL(ptr) __get_kernel_asm("ldd",ptr)
#define LDD_USER(ptr) __get_user_asm("ldd",ptr)
#define STD_KERNEL(x, ptr) __put_kernel_asm("std",x,ptr)
#define STD_USER(x, ptr) __put_user_asm("std",x,ptr)
#endif

/*
 * The exception table contains two values: the first is an address
 * for an instruction that is allowed to fault, and the second is
 * the number of bytes to skip if a fault occurs. We also support in
 * two bit flags: 0x2 tells the exception handler to clear register
 * r9 and 0x1 tells the exception handler to put -EFAULT in r8.
 * This allows us to handle the simple cases for put_user and
 * get_user without having to have .fixup sections.
 */

struct exception_table_entry {
	unsigned long addr;  /* address of insn that is allowed to fault.   */
	long skip;           /* pcoq skip | r9 clear flag | r8 -EFAULT flag */
};

extern const struct exception_table_entry 
    *search_exception_table(unsigned long addr);

#define __get_user(x,ptr)                               \
({                                                      \
	register long __gu_err __asm__ ("r8") = 0;      \
	register long __gu_val __asm__ ("r9") = 0;      \
							\
	if (segment_eq(get_fs(),KERNEL_DS)) {           \
	    switch (sizeof(*(ptr))) {                   \
	    case 1: __get_kernel_asm("ldb",ptr); break; \
	    case 2: __get_kernel_asm("ldh",ptr); break; \
	    case 4: __get_kernel_asm("ldw",ptr); break; \
	    case 8: LDD_KERNEL(ptr); break;		\
	    default: BUG(); break;                      \
	    }                                           \
	}                                               \
	else {                                          \
	    switch (sizeof(*(ptr))) {                   \
	    case 1: __get_user_asm("ldb",ptr); break;   \
	    case 2: __get_user_asm("ldh",ptr); break;   \
	    case 4: __get_user_asm("ldw",ptr); break;   \
	    case 8: LDD_USER(ptr);  break;		\
	    default: BUG(); break;                      \
	    }                                           \
	}                                               \
							\
	(x) = (__typeof__(*(ptr))) __gu_val;            \
	__gu_err;                                       \
})

#ifdef __LP64__
#define __get_kernel_asm(ldx,ptr)                       \
	__asm__("\n1:\t" ldx "\t0(%2),%0\n"             \
		"2:\n"					\
		"\t.section __ex_table,\"a\"\n"         \
		 "\t.dword\t1b\n"                       \
		 "\t.dword\t(2b-1b)+3\n"                \
		 "\t.previous"                          \
		: "=r"(__gu_val), "=r"(__gu_err)        \
		: "r"(ptr), "1"(__gu_err));

#define __get_user_asm(ldx,ptr)                         \
	__asm__("\n1:\t" ldx "\t0(%%sr3,%2),%0\n"       \
		"2:\n"					\
		"\t.section __ex_table,\"a\"\n"         \
		 "\t.dword\t1b\n"                       \
		 "\t.dword\t(2b-1b)+3\n"                \
		 "\t.previous"                          \
		: "=r"(__gu_val), "=r"(__gu_err)        \
		: "r"(ptr), "1"(__gu_err));
#else
#define __get_kernel_asm(ldx,ptr)                       \
	__asm__("\n1:\t" ldx "\t0(%2),%0\n"             \
		"2:\n"					\
		"\t.section __ex_table,\"a\"\n"         \
		 "\t.word\t1b\n"                        \
		 "\t.word\t(2b-1b)+3\n"                 \
		 "\t.previous"                          \
		: "=r"(__gu_val), "=r"(__gu_err)        \
		: "r"(ptr), "1"(__gu_err));

#define __get_user_asm(ldx,ptr)                         \
	__asm__("\n1:\t" ldx "\t0(%%sr3,%2),%0\n"       \
		"2:\n"					\
		"\t.section __ex_table,\"a\"\n"         \
		 "\t.word\t1b\n"                        \
		 "\t.word\t(2b-1b)+3\n"                 \
		 "\t.previous"                          \
		: "=r"(__gu_val), "=r"(__gu_err)        \
		: "r"(ptr), "1"(__gu_err));
#endif

#define __put_user(x,ptr)                                       \
({								\
	register long __pu_err __asm__ ("r8") = 0;		\
								\
	if (segment_eq(get_fs(),KERNEL_DS)) {                   \
	    switch (sizeof(*(ptr))) {                           \
	    case 1: __put_kernel_asm("stb",x,ptr); break;       \
	    case 2: __put_kernel_asm("sth",x,ptr); break;       \
	    case 4: __put_kernel_asm("stw",x,ptr); break;       \
	    case 8: STD_KERNEL(x,ptr); break;			\
	    default: BUG(); break;                              \
	    }                                                   \
	}                                                       \
	else {                                                  \
	    switch (sizeof(*(ptr))) {                           \
	    case 1: __put_user_asm("stb",x,ptr); break;         \
	    case 2: __put_user_asm("sth",x,ptr); break;         \
	    case 4: __put_user_asm("stw",x,ptr); break;         \
	    case 8: STD_USER(x,ptr); break;			\
	    default: BUG(); break;                              \
	    }                                                   \
	}                                                       \
								\
	__pu_err;						\
})

/*
 * The "__put_user/kernel_asm()" macros tell gcc they read from memory
 * instead of writing. This is because they do not write to any memory
 * gcc knows about, so there are no aliasing issues.
 */

#ifdef __LP64__
#define __put_kernel_asm(stx,x,ptr)                         \
	__asm__ __volatile__ (                              \
		"\n1:\t" stx "\t%2,0(%1)\n"                 \
		"2:\n"					    \
		"\t.section __ex_table,\"a\"\n"             \
		 "\t.dword\t1b\n"                           \
		 "\t.dword\t(2b-1b)+1\n"                    \
		 "\t.previous"                              \
		: "=r"(__pu_err)                            \
		: "r"(ptr), "r"(x), "0"(__pu_err))

#define __put_user_asm(stx,x,ptr)                           \
	__asm__ __volatile__ (                              \
		"\n1:\t" stx "\t%2,0(%%sr3,%1)\n"           \
		"2:\n"					    \
		"\t.section __ex_table,\"a\"\n"             \
		 "\t.dword\t1b\n"                           \
		 "\t.dword\t(2b-1b)+1\n"                    \
		 "\t.previous"                              \
		: "=r"(__pu_err)                            \
		: "r"(ptr), "r"(x), "0"(__pu_err))
#else
#define __put_kernel_asm(stx,x,ptr)                         \
	__asm__ __volatile__ (                              \
		"\n1:\t" stx "\t%2,0(%1)\n"                 \
		"2:\n"					    \
		"\t.section __ex_table,\"a\"\n"             \
		 "\t.word\t1b\n"                            \
		 "\t.word\t(2b-1b)+1\n"                     \
		 "\t.previous"                              \
		: "=r"(__pu_err)                            \
		: "r"(ptr), "r"(x), "0"(__pu_err))

#define __put_user_asm(stx,x,ptr)                           \
	__asm__ __volatile__ (                              \
		"\n1:\t" stx "\t%2,0(%%sr3,%1)\n"           \
		"2:\n"					    \
		"\t.section __ex_table,\"a\"\n"             \
		 "\t.word\t1b\n"                            \
		 "\t.word\t(2b-1b)+1\n"                     \
		 "\t.previous"                              \
		: "=r"(__pu_err)                            \
		: "r"(ptr), "r"(x), "0"(__pu_err))

static inline void __put_kernel_asm64(u64 x, void *ptr)
{
	u32 hi = x>>32;
	u32 lo = x&0xffffffff;
	__asm__ __volatile__ (
		"\n1:\tstw %1,0(%0)\n"
		"\n2:\tstw %2,4(%0)\n"
		"3:\n"
		"\t.section __ex_table,\"a\"\n"
		 "\t.word\t1b\n"
		 "\t.word\t(3b-1b)+1\n"
		 "\t.word\t2b\n"
		 "\t.word\t(3b-2b)+1\n"
		 "\t.previous"
		: : "r"(ptr), "r"(hi), "r"(lo));

}

static inline void __put_user_asm64(u64 x, void *ptr)
{
	u32 hi = x>>32;
	u32 lo = x&0xffffffff;
	__asm__ __volatile__ (
		"\n1:\tstw %1,0(%%sr3,%0)\n"
		"\n2:\tstw %2,4(%%sr3,%0)\n"
		"3:\n"
		"\t.section __ex_table,\"a\"\n"
		 "\t.word\t1b\n"
		 "\t.word\t(3b-1b)+1\n"
		 "\t.word\t2b\n"
		 "\t.word\t(3b-2b)+1\n"
		 "\t.previous"
		: : "r"(ptr), "r"(hi), "r"(lo));

}

#endif


/*
 * Complex access routines -- external declarations
 */

extern unsigned long lcopy_to_user(void *, const void *, unsigned long);
extern unsigned long lcopy_from_user(void *, const void *, unsigned long);
extern long lstrncpy_from_user(char *, const char *, long);
extern unsigned lclear_user(void *,unsigned long);
extern long lstrnlen_user(const char *,long);

/*
 * Complex access routines -- macros
 */

#define strncpy_from_user lstrncpy_from_user
#define strnlen_user lstrnlen_user
#define strlen_user(str) lstrnlen_user(str, 0x7fffffffL)
#define clear_user lclear_user
#define __clear_user lclear_user

#define copy_from_user lcopy_from_user
#define __copy_from_user lcopy_from_user
#define copy_to_user lcopy_to_user
#define __copy_to_user lcopy_to_user

#endif /* __PARISC_UACCESS_H */
