/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1999, 2000 by Ralf Baechle
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 */
#ifndef _ASM_UNALIGNED_H
#define _ASM_UNALIGNED_H

extern void __get_unaligned_bad_length(void);
extern void __put_unaligned_bad_length(void);

/*
 * Load double unaligned.
 *
 * This could have been implemented in plain C like IA64 but egcs 1.0.3a
 * inflates this to 23 instructions ...
 */
static inline unsigned long long __ldq_u(const unsigned long long * __addr)
{
	unsigned long long __res;

	__asm__("ulw\t%0, %1\n\t"
		"ulw\t%D0, 4+%1"
		: "=&r" (__res)
		: "m" (*__addr));

	return __res;
}

/*
 * Load word unaligned.
 */
static inline unsigned long __ldl_u(const unsigned int * __addr)
{
	unsigned long __res;

	__asm__("ulw\t%0,%1"
		: "=&r" (__res)
		: "m" (*__addr));

	return __res;
}

/*
 * Load halfword unaligned.
 */
static inline unsigned long __ldw_u(const unsigned short * __addr)
{
	unsigned long __res;

	__asm__("ulh\t%0,%1"
		: "=&r" (__res)
		: "m" (*__addr));

	return __res;
}

/*
 * Store doubleword ununaligned.
 */
static inline void __stq_u(unsigned long __val, unsigned long long * __addr)
{
	__asm__("usw\t%1, %0\n\t"
		"usw\t%D1, 4+%0"
		: "=m" (*__addr)
		: "r" (__val));
}

/*
 * Store long ununaligned.
 */
static inline void __stl_u(unsigned long __val, unsigned int * __addr)
{
	__asm__("usw\t%1, %0"
		: "=m" (*__addr)
		: "r" (__val));
}

/*
 * Store word ununaligned.
 */
static inline void __stw_u(unsigned long __val, unsigned short * __addr)
{
	__asm__("ush\t%1, %0"
		: "=m" (*__addr)
		: "r" (__val));
}

/*
 * get_unaligned - get value from possibly mis-aligned location
 * @ptr: pointer to value
 *
 * This macro should be used for accessing values larger in size than
 * single bytes at locations that are expected to be improperly aligned,
 * e.g. retrieving a u16 value from a location not u16-aligned.
 *
 * Note that unaligned accesses can be very expensive on some architectures.
 */
#define get_unaligned(ptr)						\
({									\
	__typeof__(*(ptr)) __val;					\
									\
	switch (sizeof(*(ptr))) {					\
	case 1:								\
		__val = *(const unsigned char *)ptr;			\
		break;							\
	case 2:								\
		__val = __ldw_u((const unsigned short *)ptr);		\
		break;							\
	case 4:								\
		__val = __ldl_u((const unsigned int *)ptr);		\
		break;							\
	case 8:								\
		__val = __ldq_u((const unsigned long long *)ptr);	\
		break;							\
	default:							\
		__get_unaligned_bad_length();				\
		break;							\
	}								\
									\
	__val;								\
})

/*
 * put_unaligned - put value to a possibly mis-aligned location
 * @val: value to place
 * @ptr: pointer to location
 *
 * This macro should be used for placing values larger in size than
 * single bytes at locations that are expected to be improperly aligned,
 * e.g. writing a u16 value to a location not u16-aligned.
 *
 * Note that unaligned accesses can be very expensive on some architectures.
 */
#define put_unaligned(val,ptr)						\
do {									\
	switch (sizeof(*(ptr))) {					\
	case 1:								\
		*(unsigned char *)(ptr) = (val);			\
		break;							\
	case 2:								\
		__stw_u(val, (unsigned short *)(ptr));			\
		break;							\
	case 4:								\
		__stl_u(val, (unsigned int *)(ptr));			\
		break;							\
	case 8:								\
		__stq_u(val, (unsigned long long *)(ptr));		\
		break;							\
	default:							\
		__put_unaligned_bad_length();				\
		break;							\
	}								\
} while(0)

#endif /* _ASM_UNALIGNED_H */
