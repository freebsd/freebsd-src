#ifndef __XFS_SUPPORT_SPIN_H__
#define __XFS_SUPPORT_SPIN_H__

#include <sys/param.h>
#include <sys/types.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#define SPLDECL(s)	register_t s

/*
 * Map the spinlocks from IRIX to FreeBSD
 */
#define spinlock_init(lock, name)	mtx_init(lock, name, NULL, MTX_DEF)
#define spinlock_destroy(lock)		mtx_destroy(lock)

/*
 * Map lock_t from IRIX to FreeBSD mutexes
 */
typedef struct mtx lock_t;

#define nested_spinunlock(lock)		mtx_unlock(lock)
#define nested_spinlock(lock)		mtx_lock(lock)
#define nested_spintrylock(lock)	mtx_trylock(lock)

#define spin_lock(lock)			mtx_lock(lock)
#define spin_unlock(lock)		mtx_unlock(lock)

#if LOCK_DEBUG > 0
#define mutex_spinlock(lock)		(spin_lock(lock),0)
#else
static __inline register_t
mutex_spinlock(lock_t *lock)		{ mtx_lock(lock); return 0; }
#endif

#define mutex_spinunlock(lock, s) \
	do { \
		spin_unlock(lock); \
		if (s != 0) {} \
	} while (0)

#endif /* __XFS_SUPPORT_SPIN_H__ */
