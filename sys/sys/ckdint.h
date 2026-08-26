/*-
 * Copyright (c) 2023 Dag-Erling Smørgrav
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _SYS_CKDINT_H_
#define	_SYS_CKDINT_H_

#include <sys/cdefs.h>

#ifdef _KERNEL
/* If you're not checking the return value, what's the point? */
__nodiscard static inline bool
__ckd_result(bool result)
{
	return (result);
}
#else
#define	__ckd_result(result)	(result)
#endif

#if __GNUC_PREREQ__(5, 1) || __has_builtin(__builtin_add_overflow)
#define ckd_add(result, a, b)						\
	__ckd_result(__builtin_add_overflow((a), (b), (result)))
#else
#define ckd_add(result, a, b)						\
	_Static_assert(0, "checked addition not supported")
#endif

#if __GNUC_PREREQ__(5, 1) || __has_builtin(__builtin_sub_overflow)
#define ckd_sub(result, a, b)						\
	__ckd_result(__builtin_sub_overflow((a), (b), (result)))
#else
#define ckd_sub(result, a, b)						\
	_Static_assert(0, "checked subtraction not supported")
#endif

#if __GNUC_PREREQ__(5, 1) || __has_builtin(__builtin_mul_overflow)
#define ckd_mul(result, a, b)						\
	__ckd_result(__builtin_mul_overflow((a), (b), (result)))
#else
#define ckd_mul(result, a, b)						\
	_Static_assert(0, "checked multiplication not supported")
#endif

#endif /* _SYS_CKDINT_H_ */
