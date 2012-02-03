#ifndef __XFS_SUPPORT_MRLOCK_H__
#define __XFS_SUPPORT_MRLOCK_H__

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/sx.h>

/*
 * Implement mrlocks on FreeBSD that work for XFS.
 * Map mrlock functions to corresponding equivalents in
 * sx.
 */
typedef struct sx mrlock_t;

#define MR_ACCESS	1
#define MR_UPDATE	2

/*
 * Compatibility defines, not really used
 */
#define MRLOCK_BARRIER		0x1
#define MRLOCK_ALLOW_EQUAL_PRI	0x8

#define mrlock_init(lock, type, name, seq) sx_init(lock, name)
#define mrtryaccess(lock)	sx_try_slock(lock)
#define mrtryupdate(lock)	sx_try_xlock(lock)
#define mraccess(lock)		sx_slock(lock)
#define mrupdate(lock)		sx_xlock(lock)
#define mrdemote(lock)		sx_downgrade(lock)
#define mrunlock(lock)		sx_unlock(lock)

#define mrfree(lock) do {		\
	if (sx_xlocked(lock))		\
		sx_xunlock(lock);	\
	sx_destroy(lock);		\
} while (0)

int ismrlocked(mrlock_t *mrp, int type);

#endif /* __XFS_SUPPORT_MRLOCK_H__ */
