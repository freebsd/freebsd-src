#ifndef __XFS_SUPPORT_ATOMIC_H__

#include <sys/types.h>
#include <machine/atomic.h>

typedef struct {
	volatile unsigned int	val;
} atomic_t;

#define	atomic_read(v)			((v)->val)
#define atomic_set(v, i)		((v)->val = (i))

#define	atomic_add(i, v)		atomic_add_int(&(v)->val, (i))
#define	atomic_inc(v)			atomic_add_int(&(v)->val, 1)
#define	atomic_dec(v)			atomic_subtract_int(&(v)->val, 1)
#define	atomic_sub(i, v)		atomic_subtract_int(&(v)->val, (i))
#define	atomic_dec_and_test(v)		(atomic_fetchadd_int(&(v)->val, -1) == 1)

/*
 * This is used for two variables in XFS, one of which is a debug trace
 * buffer index.
 */

static __inline__ int atomicIncWithWrap(volatile unsigned int *ip, int val)
{
	unsigned int oldval, newval;

	do {
		oldval = *ip;
		newval = (oldval + 1 >= val) ? 0 : oldval + 1;
        } while (atomic_cmpset_rel_int(ip, oldval, newval) == 0);

	return oldval;
}

#endif /* __XFS_SUPPORT_ATOMIC_H__ */
