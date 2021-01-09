/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#ifndef _RTE_COMMON_H_
#define _RTE_COMMON_H_

/**
 * @file
 *
 * Generic, commonly-used macro and inline function definitions
 * for DPDK.
 */

#ifdef __cplusplus
extern "C" {
#endif

//#include <rte_config.h>

/* OS specific include */
//#include <rte_os.h>

#ifndef typeof
#define typeof __typeof__
#endif

#ifndef asm
#define asm __asm__
#endif

/** C extension macro for environments lacking C11 features. */
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#define RTE_STD_C11 __extension__
#else
#define RTE_STD_C11
#endif

/*
 * RTE_TOOLCHAIN_GCC is defined if the target is built with GCC,
 * while a host application (like pmdinfogen) may have another compiler.
 * RTE_CC_IS_GNU is true if the file is compiled with GCC,
 * no matter it is a target or host application.
 */
#define RTE_CC_IS_GNU 0
#if defined __clang__
#define RTE_CC_CLANG
#elif defined __INTEL_COMPILER
#define RTE_CC_ICC
#elif defined __GNUC__
#define RTE_CC_GCC
#undef RTE_CC_IS_GNU
#define RTE_CC_IS_GNU 1
#endif
#if RTE_CC_IS_GNU
#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 +	\
		__GNUC_PATCHLEVEL__)
#endif

/**
 * Force alignment
 */
#define __rte_aligned(a) __attribute__((__aligned__(a)))

#ifdef RTE_ARCH_STRICT_ALIGN
typedef uint64_t unaligned_uint64_t __rte_aligned(1);
typedef uint32_t unaligned_uint32_t __rte_aligned(1);
typedef uint16_t unaligned_uint16_t __rte_aligned(1);
#else
typedef uint64_t unaligned_uint64_t;
typedef uint32_t unaligned_uint32_t;
typedef uint16_t unaligned_uint16_t;
#endif

/**
 * Force a structure to be packed
 */
#define __rte_packed __attribute__((__packed__))

/******* Macro to mark functions and fields scheduled for removal *****/
#define __rte_deprecated	__attribute__((__deprecated__))

/**
 * Mark a function or variable to a weak reference.
 */
#define __rte_weak __attribute__((__weak__))

/**
 * Force symbol to be generated even if it appears to be unused.
 */
#define __rte_used __attribute__((used))

/*********** Macros to eliminate unused variable warnings ********/

/**
 * short definition to mark a function parameter unused
 */
#define __rte_unused __attribute__((__unused__))

/**
 * definition to mark a variable or function parameter as used so
 * as to avoid a compiler warning
 */
#define RTE_SET_USED(x) (void)(x)

/**
 * Check format string and its arguments at compile-time.
 *
 * GCC on Windows assumes MS-specific format string by default,
 * even if the underlying stdio implementation is ANSI-compliant,
 * so this must be overridden.
 */
#if RTE_CC_IS_GNU
#define __rte_format_printf(format_index, first_arg) \
	__attribute__((format(gnu_printf, format_index, first_arg)))
#else
#define __rte_format_printf(format_index, first_arg) \
	__attribute__((format(printf, format_index, first_arg)))
#endif

#define RTE_PRIORITY_LOG 101
#define RTE_PRIORITY_BUS 110
#define RTE_PRIORITY_CLASS 120
#define RTE_PRIORITY_LAST 65535

#define RTE_PRIO(prio) \
	RTE_PRIORITY_ ## prio

/**
 * Run function before main() with high priority.
 *
 * @param func
 *   Constructor function.
 * @param prio
 *   Priority number must be above 100.
 *   Lowest number is the first to run.
 */
#ifndef RTE_INIT_PRIO /* Allow to override from EAL */
#define RTE_INIT_PRIO(func, prio) \
static void __attribute__((constructor(RTE_PRIO(prio)), used)) func(void)
#endif

/**
 * Run function before main() with low priority.
 *
 * The constructor will be run after prioritized constructors.
 *
 * @param func
 *   Constructor function.
 */
#define RTE_INIT(func) \
	RTE_INIT_PRIO(func, LAST)

/**
 * Run after main() with low priority.
 *
 * @param func
 *   Destructor function name.
 * @param prio
 *   Priority number must be above 100.
 *   Lowest number is the last to run.
 */
#ifndef RTE_FINI_PRIO /* Allow to override from EAL */
#define RTE_FINI_PRIO(func, prio) \
static void __attribute__((destructor(RTE_PRIO(prio)), used)) func(void)
#endif

/**
 * Run after main() with high priority.
 *
 * The destructor will be run *before* prioritized destructors.
 *
 * @param func
 *   Destructor function name.
 */
#define RTE_FINI(func) \
	RTE_FINI_PRIO(func, LAST)

/**
 * Hint never returning function
 */
#define __rte_noreturn __attribute__((noreturn))

/**
 * Force a function to be inlined
 */
#define __rte_always_inline inline __attribute__((always_inline))

/**
 * Force a function to be noinlined
 */
#define __rte_noinline __attribute__((noinline))

/**
 * Hint function in the hot path
 */
#define __rte_hot __attribute__((hot))

/**
 * Hint function in the cold path
 */
#define __rte_cold __attribute__((cold))

/*********** Macros for pointer arithmetic ********/

/**
 * add a byte-value offset to a pointer
 */
#define RTE_PTR_ADD(ptr, x) ((void*)((uintptr_t)(ptr) + (x)))

/**
 * subtract a byte-value offset from a pointer
 */
#define RTE_PTR_SUB(ptr, x) ((void*)((uintptr_t)ptr - (x)))

/**
 * get the difference between two pointer values, i.e. how far apart
 * in bytes are the locations they point two. It is assumed that
 * ptr1 is greater than ptr2.
 */
#define RTE_PTR_DIFF(ptr1, ptr2) ((uintptr_t)(ptr1) - (uintptr_t)(ptr2))

/**
 * Workaround to cast a const field of a structure to non-const type.
 */
#define RTE_CAST_FIELD(var, field, type) \
	(*(type *)((uintptr_t)(var) + offsetof(typeof(*(var)), field)))

/*********** Macros/static functions for doing alignment ********/


/**
 * Macro to align a pointer to a given power-of-two. The resultant
 * pointer will be a pointer of the same type as the first parameter, and
 * point to an address no higher than the first parameter. Second parameter
 * must be a power-of-two value.
 */
#define RTE_PTR_ALIGN_FLOOR(ptr, align) \
	((typeof(ptr))RTE_ALIGN_FLOOR((uintptr_t)ptr, align))

/**
 * Macro to align a value to a given power-of-two. The resultant value
 * will be of the same type as the first parameter, and will be no
 * bigger than the first parameter. Second parameter must be a
 * power-of-two value.
 */
#define RTE_ALIGN_FLOOR(val, align) \
	(typeof(val))((val) & (~((typeof(val))((align) - 1))))

/**
 * Macro to align a pointer to a given power-of-two. The resultant
 * pointer will be a pointer of the same type as the first parameter, and
 * point to an address no lower than the first parameter. Second parameter
 * must be a power-of-two value.
 */
#define RTE_PTR_ALIGN_CEIL(ptr, align) \
	RTE_PTR_ALIGN_FLOOR((typeof(ptr))RTE_PTR_ADD(ptr, (align) - 1), align)

/**
 * Macro to align a value to a given power-of-two. The resultant value
 * will be of the same type as the first parameter, and will be no lower
 * than the first parameter. Second parameter must be a power-of-two
 * value.
 */
#define RTE_ALIGN_CEIL(val, align) \
	RTE_ALIGN_FLOOR(((val) + ((typeof(val)) (align) - 1)), align)

/**
 * Macro to align a pointer to a given power-of-two. The resultant
 * pointer will be a pointer of the same type as the first parameter, and
 * point to an address no lower than the first parameter. Second parameter
 * must be a power-of-two value.
 * This function is the same as RTE_PTR_ALIGN_CEIL
 */
#define RTE_PTR_ALIGN(ptr, align) RTE_PTR_ALIGN_CEIL(ptr, align)

/**
 * Macro to align a value to a given power-of-two. The resultant
 * value will be of the same type as the first parameter, and
 * will be no lower than the first parameter. Second parameter
 * must be a power-of-two value.
 * This function is the same as RTE_ALIGN_CEIL
 */
#define RTE_ALIGN(val, align) RTE_ALIGN_CEIL(val, align)

/**
 * Macro to align a value to the multiple of given value. The resultant
 * value will be of the same type as the first parameter and will be no lower
 * than the first parameter.
 */
#define RTE_ALIGN_MUL_CEIL(v, mul) \
	(((v + (typeof(v))(mul) - 1) / ((typeof(v))(mul))) * (typeof(v))(mul))

/**
 * Macro to align a value to the multiple of given value. The resultant
 * value will be of the same type as the first parameter and will be no higher
 * than the first parameter.
 */
#define RTE_ALIGN_MUL_FLOOR(v, mul) \
	((v / ((typeof(v))(mul))) * (typeof(v))(mul))

/**
 * Macro to align value to the nearest multiple of the given value.
 * The resultant value might be greater than or less than the first parameter
 * whichever difference is the lowest.
 */
#define RTE_ALIGN_MUL_NEAR(v, mul)				\
	({							\
		typeof(v) ceil = RTE_ALIGN_MUL_CEIL(v, mul);	\
		typeof(v) floor = RTE_ALIGN_MUL_FLOOR(v, mul);	\
		(ceil - v) > (v - floor) ? floor : ceil;	\
	})

/**
 * Checks if a pointer is aligned to a given power-of-two value
 *
 * @param ptr
 *   The pointer whose alignment is to be checked
 * @param align
 *   The power-of-two value to which the ptr should be aligned
 *
 * @return
 *   True(1) where the pointer is correctly aligned, false(0) otherwise
 */
static inline int
rte_is_aligned(void *ptr, unsigned align)
{
	return RTE_PTR_ALIGN(ptr, align) == ptr;
}

/*********** Macros for compile type checks ********/

/**
 * Triggers an error at compilation time if the condition is true.
 */
#define RTE_BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

/*********** Cache line related macros ********/

/** Cache line mask. */
#define RTE_CACHE_LINE_MASK (RTE_CACHE_LINE_SIZE-1)

/** Return the first cache-aligned value greater or equal to size. */
#define RTE_CACHE_LINE_ROUNDUP(size) \
	(RTE_CACHE_LINE_SIZE * ((size + RTE_CACHE_LINE_SIZE - 1) / \
	RTE_CACHE_LINE_SIZE))

/** Cache line size in terms of log2 */
#if RTE_CACHE_LINE_SIZE == 64
#define RTE_CACHE_LINE_SIZE_LOG2 6
#elif RTE_CACHE_LINE_SIZE == 128
#define RTE_CACHE_LINE_SIZE_LOG2 7
#else
#error "Unsupported cache line size"
#endif

/** Minimum Cache line size. */
#define RTE_CACHE_LINE_MIN_SIZE 64

/** Force alignment to cache line. */
#define __rte_cache_aligned __rte_aligned(RTE_CACHE_LINE_SIZE)

/** Force minimum cache line alignment. */
#define __rte_cache_min_aligned __rte_aligned(RTE_CACHE_LINE_MIN_SIZE)

/*********** PA/IOVA type definitions ********/

/** Physical address */
typedef uint64_t phys_addr_t;
#define RTE_BAD_PHYS_ADDR ((phys_addr_t)-1)

/**
 * IO virtual address type.
 * When the physical addressing mode (IOVA as PA) is in use,
 * the translation from an IO virtual address (IOVA) to a physical address
 * is a direct mapping, i.e. the same value.
 * Otherwise, in virtual mode (IOVA as VA), an IOMMU may do the translation.
 */
typedef uint64_t rte_iova_t;
#define RTE_BAD_IOVA ((rte_iova_t)-1)

/*********** Structure alignment markers ********/

/** Generic marker for any place in a structure. */
__extension__ typedef void    *RTE_MARKER[0];
/** Marker for 1B alignment in a structure. */
__extension__ typedef uint8_t  RTE_MARKER8[0];
/** Marker for 2B alignment in a structure. */
__extension__ typedef uint16_t RTE_MARKER16[0];
/** Marker for 4B alignment in a structure. */
__extension__ typedef uint32_t RTE_MARKER32[0];
/** Marker for 8B alignment in a structure. */
__extension__ typedef uint64_t RTE_MARKER64[0];

/**
 * Combines 32b inputs most significant set bits into the least
 * significant bits to construct a value with the same MSBs as x
 * but all 1's under it.
 *
 * @param x
 *    The integer whose MSBs need to be combined with its LSBs
 * @return
 *    The combined value.
 */
static inline uint32_t
rte_combine32ms1b(register uint32_t x)
{
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;

	return x;
}

/**
 * Combines 64b inputs most significant set bits into the least
 * significant bits to construct a value with the same MSBs as x
 * but all 1's under it.
 *
 * @param v
 *    The integer whose MSBs need to be combined with its LSBs
 * @return
 *    The combined value.
 */
static inline uint64_t
rte_combine64ms1b(register uint64_t v)
{
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v |= v >> 32;

	return v;
}

/*********** Macros to work with powers of 2 ********/

/**
 * Macro to return 1 if n is a power of 2, 0 otherwise
 */
#define RTE_IS_POWER_OF_2(n) ((n) && !(((n) - 1) & (n)))

/**
 * Returns true if n is a power of 2
 * @param n
 *     Number to check
 * @return 1 if true, 0 otherwise
 */
static inline int
rte_is_power_of_2(uint32_t n)
{
	return n && !(n & (n - 1));
}

/**
 * Aligns input parameter to the next power of 2
 *
 * @param x
 *   The integer value to align
 *
 * @return
 *   Input parameter aligned to the next power of 2
 */
static inline uint32_t
rte_align32pow2(uint32_t x)
{
	x--;
	x = rte_combine32ms1b(x);

	return x + 1;
}

/**
 * Aligns input parameter to the previous power of 2
 *
 * @param x
 *   The integer value to align
 *
 * @return
 *   Input parameter aligned to the previous power of 2
 */
static inline uint32_t
rte_align32prevpow2(uint32_t x)
{
	x = rte_combine32ms1b(x);

	return x - (x >> 1);
}

/**
 * Aligns 64b input parameter to the next power of 2
 *
 * @param v
 *   The 64b value to align
 *
 * @return
 *   Input parameter aligned to the next power of 2
 */
static inline uint64_t
rte_align64pow2(uint64_t v)
{
	v--;
	v = rte_combine64ms1b(v);

	return v + 1;
}

/**
 * Aligns 64b input parameter to the previous power of 2
 *
 * @param v
 *   The 64b value to align
 *
 * @return
 *   Input parameter aligned to the previous power of 2
 */
static inline uint64_t
rte_align64prevpow2(uint64_t v)
{
	v = rte_combine64ms1b(v);

	return v - (v >> 1);
}

/*********** Macros for calculating min and max **********/

/**
 * Macro to return the minimum of two numbers
 */
#define RTE_MIN(a, b) \
	__extension__ ({ \
		typeof (a) _a = (a); \
		typeof (b) _b = (b); \
		_a < _b ? _a : _b; \
	})

/**
 * Macro to return the maximum of two numbers
 */
#define RTE_MAX(a, b) \
	__extension__ ({ \
		typeof (a) _a = (a); \
		typeof (b) _b = (b); \
		_a > _b ? _a : _b; \
	})

/*********** Other general functions / macros ********/

/**
 * Searches the input parameter for the least significant set bit
 * (starting from zero).
 * If a least significant 1 bit is found, its bit index is returned.
 * If the content of the input parameter is zero, then the content of the return
 * value is undefined.
 * @param v
 *     input parameter, should not be zero.
 * @return
 *     least significant set bit in the input parameter.
 */
static inline uint32_t
rte_bsf32(uint32_t v)
{
	return (uint32_t)__builtin_ctz(v);
}

/**
 * Searches the input parameter for the least significant set bit
 * (starting from zero). Safe version (checks for input parameter being zero).
 *
 * @warning ``pos`` must be a valid pointer. It is not checked!
 *
 * @param v
 *     The input parameter.
 * @param pos
 *     If ``v`` was not 0, this value will contain position of least significant
 *     bit within the input parameter.
 * @return
 *     Returns 0 if ``v`` was 0, otherwise returns 1.
 */
static inline int
rte_bsf32_safe(uint64_t v, uint32_t *pos)
{
	if (v == 0)
		return 0;

	*pos = rte_bsf32(v);
	return 1;
}

/**
 * Return the rounded-up log2 of a integer.
 *
 * @note Contrary to the logarithm mathematical operation,
 * rte_log2_u32(0) == 0 and not -inf.
 *
 * @param v
 *     The input parameter.
 * @return
 *     The rounded-up log2 of the input, or 0 if the input is 0.
 */
static inline uint32_t
rte_log2_u32(uint32_t v)
{
	if (v == 0)
		return 0;
	v = rte_align32pow2(v);
	return rte_bsf32(v);
}


/**
 * Return the last (most-significant) bit set.
 *
 * @note The last (most significant) bit is at position 32.
 * @note rte_fls_u32(0) = 0, rte_fls_u32(1) = 1, rte_fls_u32(0x80000000) = 32
 *
 * @param x
 *     The input parameter.
 * @return
 *     The last (most-significant) bit set, or 0 if the input is 0.
 */
static inline int
rte_fls_u32(uint32_t x)
{
	return (x == 0) ? 0 : 32 - __builtin_clz(x);
}

/**
 * Searches the input parameter for the least significant set bit
 * (starting from zero).
 * If a least significant 1 bit is found, its bit index is returned.
 * If the content of the input parameter is zero, then the content of the return
 * value is undefined.
 * @param v
 *     input parameter, should not be zero.
 * @return
 *     least significant set bit in the input parameter.
 */
static inline int
rte_bsf64(uint64_t v)
{
	return (uint32_t)__builtin_ctzll(v);
}

/**
 * Searches the input parameter for the least significant set bit
 * (starting from zero). Safe version (checks for input parameter being zero).
 *
 * @warning ``pos`` must be a valid pointer. It is not checked!
 *
 * @param v
 *     The input parameter.
 * @param pos
 *     If ``v`` was not 0, this value will contain position of least significant
 *     bit within the input parameter.
 * @return
 *     Returns 0 if ``v`` was 0, otherwise returns 1.
 */
static inline int
rte_bsf64_safe(uint64_t v, uint32_t *pos)
{
	if (v == 0)
		return 0;

	*pos = rte_bsf64(v);
	return 1;
}

/**
 * Return the last (most-significant) bit set.
 *
 * @note The last (most significant) bit is at position 64.
 * @note rte_fls_u64(0) = 0, rte_fls_u64(1) = 1,
 *       rte_fls_u64(0x8000000000000000) = 64
 *
 * @param x
 *     The input parameter.
 * @return
 *     The last (most-significant) bit set, or 0 if the input is 0.
 */
static inline int
rte_fls_u64(uint64_t x)
{
	return (x == 0) ? 0 : 64 - __builtin_clzll(x);
}

/**
 * Return the rounded-up log2 of a 64-bit integer.
 *
 * @note Contrary to the logarithm mathematical operation,
 * rte_log2_u64(0) == 0 and not -inf.
 *
 * @param v
 *     The input parameter.
 * @return
 *     The rounded-up log2 of the input, or 0 if the input is 0.
 */
static inline uint32_t
rte_log2_u64(uint64_t v)
{
	if (v == 0)
		return 0;
	v = rte_align64pow2(v);
	/* we checked for v being 0 already, so no undefined behavior */
	return rte_bsf64(v);
}

#ifndef offsetof
/** Return the offset of a field in a structure. */
#define offsetof(TYPE, MEMBER)  __builtin_offsetof (TYPE, MEMBER)
#endif

/**
 * Return pointer to the wrapping struct instance.
 *
 * Example:
 *
 *  struct wrapper {
 *      ...
 *      struct child c;
 *      ...
 *  };
 *
 *  struct child *x = obtain(...);
 *  struct wrapper *w = container_of(x, struct wrapper, c);
 */
#ifndef container_of
#define container_of(ptr, type, member)	__extension__ ({		\
			const typeof(((type *)0)->member) *_ptr = (ptr); \
			__rte_unused type *_target_ptr =	\
				(type *)(ptr);				\
			(type *)(((uintptr_t)_ptr) - offsetof(type, member)); \
		})
#endif

/**
 * Get the size of a field in a structure.
 *
 * @param type
 *   The type of the structure.
 * @param field
 *   The field in the structure.
 * @return
 *   The size of the field in the structure, in bytes.
 */
#define RTE_SIZEOF_FIELD(type, field) (sizeof(((type *)0)->field))

#define _RTE_STR(x) #x
/** Take a macro value and get a string version of it */
#define RTE_STR(x) _RTE_STR(x)

/**
 * ISO C helpers to modify format strings using variadic macros.
 * This is a replacement for the ", ## __VA_ARGS__" GNU extension.
 * An empty %s argument is appended to avoid a dangling comma.
 */
#define RTE_FMT(fmt, ...) fmt "%.0s", __VA_ARGS__ ""
#define RTE_FMT_HEAD(fmt, ...) fmt
#define RTE_FMT_TAIL(fmt, ...) __VA_ARGS__

/** Mask value of type "tp" for the first "ln" bit set. */
#define	RTE_LEN2MASK(ln, tp)	\
	((tp)((uint64_t)-1 >> (sizeof(uint64_t) * CHAR_BIT - (ln))))

/** Number of elements in the array. */
#define	RTE_DIM(a)	(sizeof (a) / sizeof ((a)[0]))

/**
 * Converts a numeric string to the equivalent uint64_t value.
 * As well as straight number conversion, also recognises the suffixes
 * k, m and g for kilobytes, megabytes and gigabytes respectively.
 *
 * If a negative number is passed in  i.e. a string with the first non-black
 * character being "-", zero is returned. Zero is also returned in the case of
 * an error with the strtoull call in the function.
 *
 * @param str
 *     String containing number to convert.
 * @return
 *     Number.
 */
#if 0
static inline uint64_t
rte_str_to_size(const char *str)
{
	char *endptr;
	unsigned long long size;

	while (isspace((int)*str))
		str++;
	if (*str == '-')
		return 0;

	errno = 0;
	size = strtoull(str, &endptr, 0);
	if (errno)
		return 0;

	if (*endptr == ' ')
		endptr++; /* allow 1 space gap */

	switch (*endptr){
	case 'G': case 'g': size *= 1024; /* fall-through */
	case 'M': case 'm': size *= 1024; /* fall-through */
	case 'K': case 'k': size *= 1024; /* fall-through */
	default:
		break;
	}
	return size;
}
#endif

/**
 * Function to terminate the application immediately, printing an error
 * message and returning the exit_code back to the shell.
 *
 * This function never returns
 *
 * @param exit_code
 *     The exit code to be returned by the application
 * @param format
 *     The format string to be used for printing the message. This can include
 *     printf format characters which will be expanded using any further parameters
 *     to the function.
 */
__rte_noreturn void
rte_exit(int exit_code, const char *format, ...)
	__rte_format_printf(2, 3);

#ifdef __cplusplus
}
#endif

#endif
