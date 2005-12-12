#ifndef __XFS_SUPPORT_RWSEM_H__
#define __XFS_SUPPORT_RWSEM_H__

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>

#define rw_semaphore	sx

#define	init_rwsem(sem)		sx_init(sem, "rwsem")
#define	free_rwsem(sem)		sx_destroy(sem)
#define	down_read(sem)		sx_slock(sem)
#define	down_read_trylock(sem)	sx_try_slock(sem)
#define	down_write(sem)		sx_xlock(sem)
#define	down_write_trylock(sem)	sx_try_xlock(sem)
#define	up_read(sem)		sx_sunlock(sem)
#define	up_write(sem)		sx_xunlock(sem)
#define	downgrade_write(sem)	sx_downgrade(sem)

#endif /* __XFS_SUPPORT_RWSEM_H__ */
