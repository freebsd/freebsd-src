#ifndef __XFS_SUPPORT_MRLOCK_H__
#define __XFS_SUPPORT_MRLOCK_H__

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>

#include <support/debug.h>

/*
 * Implement mrlocks on FreeBSD that work for XFS.
 * Use FreeBSD sx lock and add necessary functions
 * if additional functionality is requested
 */
typedef struct sx mrlock_t;

#define MR_ACCESS	1
#define MR_UPDATE	2

/* 
 * Compatibility defines, not really used
 */
#define MRLOCK_BARRIER		0x1
#define MRLOCK_ALLOW_EQUAL_PRI	0x8

/*
 * mraccessf/mrupdatef take flags to be passed in while sleeping;
 * only PLTWAIT is currently supported.
 */
#define mrinit(lock, name)	sx_init(lock, name)
#define mrlock_init(lock, type, name, seq) sx_init(lock, name)
#define mrfree(lock)		_sx_xfs_destroy(lock)
#define	mraccessf(lock, f)	sx_slock(lock)
#define	mrupdatef(lock, f)	sx_xlock(lock)
#define mraccunlock(lock)	sx_sunlock(lock)
#define mrtryaccess(lock)	sx_try_slock(lock)
#define mrtryupdate(lock)	sx_try_xlock(lock)
#define mraccess(mrp)		mraccessf(mrp, 0)
#define mrupdate(mrp)		mrupdatef(mrp, 0)
#define mrislocked_access(lock)	_sx_xfs_xowned(lock)
#define mrislocked_update(lock)	_sx_xfs_sowned(lock)
#define mrtrypromote(lock)	sx_try_upgrade(lock)
#define mrdemote(lock)		sx_downgrade(lock)

int	ismrlocked(mrlock_t *, int);
void	_sx_xfs_lock(struct sx *sx, int type, const char *file, int line);
void	_sx_xfs_unlock(struct sx *sx, const char *file, int line);
void	_sx_xfs_destroy(struct sx *sx);
#define _sx_xfs_xowned(lock) ((lock)->sx_cnt < 0)
#define _sx_xfs_sowned(lock) ((lock)->sx_cnt > 0)

/*
 * Functions, not implemented in FreeBSD 
 */
#define mrunlock(lock) \
        _sx_xfs_unlock(lock, __FILE__, __LINE__)

#define	mrlock(lock, type, flags) \
        _sx_xfs_lock(lock, type, __FILE__, __LINE__)



#endif /* __XFS_SUPPORT_MRLOCK_H__ */
