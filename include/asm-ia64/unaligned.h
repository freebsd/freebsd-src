#ifndef _ASM_IA64_UNALIGNED_H
#define _ASM_IA64_UNALIGNED_H

#include <linux/types.h>

/*
 * The main single-value unaligned transfer routines.
 *
 * Based on <asm-alpha/unaligned.h>.
 *
 * Copyright (C) 1998, 1999, 2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */
#define get_unaligned(ptr) \
	((__typeof__(*(ptr)))ia64_get_unaligned((ptr), sizeof(*(ptr))))

#define put_unaligned(x,ptr) \
	ia64_put_unaligned((unsigned long)(x), (ptr), sizeof(*(ptr)))

struct __una_u64 { __u64 x __attribute__((packed)); };
struct __una_u32 { __u32 x __attribute__((packed)); };
struct __una_u16 { __u16 x __attribute__((packed)); };

static inline unsigned long
__uld8 (const unsigned long * addr)
{
	const struct __una_u64 *ptr = (const struct __una_u64 *) addr;
	return ptr->x;
}

static inline unsigned long
__uld4 (const unsigned int * addr)
{
	const struct __una_u32 *ptr = (const struct __una_u32 *) addr;
	return ptr->x;
}

static inline unsigned long
__uld2 (const unsigned short * addr)
{
	const struct __una_u16 *ptr = (const struct __una_u16 *) addr;
	return ptr->x;
}

static inline void
__ust8 (unsigned long val, unsigned long * addr)
{
	struct __una_u64 *ptr = (struct __una_u64 *) addr;
	ptr->x = val;
}

static inline void
__ust4 (unsigned long val, unsigned int * addr)
{
	struct __una_u32 *ptr = (struct __una_u32 *) addr;
	ptr->x = val;
}

static inline void
__ust2 (unsigned long val, unsigned short * addr)
{
	struct __una_u16 *ptr = (struct __una_u16 *) addr;
	ptr->x = val;
}


/*
 * This function doesn't actually exist.  The idea is that when someone uses the macros
 * below with an unsupported size (datatype), the linker will alert us to the problem via
 * an unresolved reference error.
 */
extern unsigned long ia64_bad_unaligned_access_length (void);

#define ia64_get_unaligned(_ptr,size)						\
({										\
	const void *__ia64_ptr = (_ptr);					\
	unsigned long __ia64_val;						\
										\
	switch (size) {								\
	      case 1:								\
		__ia64_val = *(const unsigned char *) __ia64_ptr;		\
		break;								\
	      case 2:								\
		__ia64_val = __uld2((const unsigned short *)__ia64_ptr);	\
		break;								\
	      case 4:								\
		__ia64_val = __uld4((const unsigned int *)__ia64_ptr);		\
		break;								\
	      case 8:								\
		__ia64_val = __uld8((const unsigned long *)__ia64_ptr);		\
		break;								\
	      default:								\
		__ia64_val = ia64_bad_unaligned_access_length();		\
	}									\
	__ia64_val;								\
})

#define ia64_put_unaligned(_val,_ptr,size)				\
do {									\
	const void *__ia64_ptr = (_ptr);				\
	unsigned long __ia64_val = (_val);				\
									\
	switch (size) {							\
	      case 1:							\
		*(unsigned char *)__ia64_ptr = (__ia64_val);		\
	        break;							\
	      case 2:							\
		__ust2(__ia64_val, (unsigned short *)__ia64_ptr);	\
		break;							\
	      case 4:							\
		__ust4(__ia64_val, (unsigned int *)__ia64_ptr);		\
		break;							\
	      case 8:							\
		__ust8(__ia64_val, (unsigned long *)__ia64_ptr);	\
		break;							\
	      default:							\
	    	ia64_bad_unaligned_access_length();			\
	}								\
} while (0)

#endif /* _ASM_IA64_UNALIGNED_H */
