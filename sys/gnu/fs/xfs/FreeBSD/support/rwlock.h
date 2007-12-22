#ifndef __XFS_SUPPORT_RWLOCK_H__
#define __XFS_SUPPORT_RWLOCK_H__

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>

typedef	struct sx rwlock_t;
typedef int    wait_queue_head_t;

#define	rwlock_init(lock)	sx_init(lock, "rwlock")
#define	rwlock_destroy(lock)	sx_destroy(lock)
#define	read_lock(lock)		sx_slock(lock)
#define	read_unlock(lock)	sx_sunlock(lock)
#define	write_lock(lock)	sx_xlock(lock)
#define write_trylock(lock)	sx_try_xlock(lock)
#define	write_unlock(lock)	sx_xunlock(lock)
#define	rwlock_trypromote(lock)	sx_try_upgrade(lock)
#define	rwlock_demote(lock)	sx_downgrade(lock)


#endif /* __XFS_SUPPORT_RWLOCK_H__ */
