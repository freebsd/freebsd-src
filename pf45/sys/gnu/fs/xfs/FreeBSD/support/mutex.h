#ifndef __XFS_SUPPORT_MUTEX_H__
#define __XFS_SUPPORT_MUTEX_H__

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>

/*
 * Map the mutex'es from IRIX to FreeBSD. Irix holds mutexes across
 * sleeps, so on FreeBSD we have a choice of sema, sx or lockmgr
 * to use as a underlining implemenation. Go with sx always locked
 * in exclusive mode for now as it gets all the benefits of witness
 * checking.
 */
typedef struct sx mutex_t;

#define mutex_init(lock, type, name)	sx_init(lock, name)
#define mutex_lock(lock, num)		sx_xlock(lock)
#define mutex_trylock(lock)	        sx_try_xlock(lock)
#define mutex_unlock(lock)		sx_xunlock(lock)
#define mutex_destroy(lock)		sx_destroy(lock)

/*
 * Type for mutex_init()
 */
#define MUTEX_DEFAULT		0

#endif /* __XFS_SUPPORT_MUTEX_H__ */
