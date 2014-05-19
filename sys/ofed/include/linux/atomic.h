#ifndef _COMPAT_LINUX_ATOMIC_H
#define _COMPAT_LINUX_ATOMIC_H 1

/*
#include <linux/version.h>

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36))
#include_next <linux/atomic.h>
#else
*/

#include <asm/atomic.h>

/* Shahar Klein: atomic_inc_not_zero_hint do we need it? */
#if 0

/**
 * atomic_inc_not_zero_hint - increment if not null
 * @v: pointer of type atomic_t
 * @hint: probable value of the atomic before the increment
 *
 * This version of atomic_inc_not_zero() gives a hint of probable
 * value of the atomic. This helps processor to not read the memory
 * before doing the atomic read/modify/write cycle, lowering
 * number of bus transactions on some arches.
 *
 * Returns: 0 if increment was not done, 1 otherwise.
 */

#ifndef atomic_inc_not_zero_hint
static inline int atomic_inc_not_zero_hint(atomic_t *v, int hint)
{
	int val, c = hint;

	/* sanity test, should be removed by compiler if hint is a constant */
	if (!hint)
		return atomic_inc_not_zero(v);

	do {
		val = atomic_cmpxchg(v, c, c + 1);
		if (val == c)
			return 1;
		c = val;
	} while (c);

	return 0;
}
#endif
#endif

//#endif /* (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)) */

#endif	/* _COMPAT_LINUX_ATOMIC_H */
