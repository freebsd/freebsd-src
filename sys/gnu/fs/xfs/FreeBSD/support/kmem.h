#ifndef __XFS_SUPPORT_KMEM_H__
#define __XFS_SUPPORT_KMEM_H__

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <vm/uma.h>

typedef unsigned long xfs_pflags_t;

#define PFLAGS_TEST_NOIO()              0
#define PFLAGS_TEST_FSTRANS()           0

#define PFLAGS_SET_NOIO(STATEP) do {    \
} while (0)

#define PFLAGS_SET_FSTRANS(STATEP) do { \
} while (0)

#define PFLAGS_RESTORE(STATEP) do {     \
} while (0)

#define PFLAGS_DUP(OSTATEP, NSTATEP) do { \
} while (0)

/*
 * memory management routines
 */
#define KM_SLEEP	M_WAITOK
#define KM_SLEEP_IO	M_WAITOK
#define KM_NOFS		M_WAITOK
#define KM_NOSLEEP	M_NOWAIT
#define KM_CACHEALIGN	0

#define kmem_zone	uma_zone

typedef struct uma_zone kmem_zone_t;
typedef struct uma_zone xfs_zone_t;

#define kmem_zone_init(len, name)		\
	uma_zcreate(name, len, NULL, NULL, NULL, NULL, 0, 0)
#define kmem_zone_free(zone, ptr)		\
	uma_zfree(zone, ptr)
#define kmem_cache_destroy(zone)		\
	uma_zdestroy(zone)
#define kmem_zone_alloc(zone, flg)		\
	uma_zalloc(zone, flg)
#define kmem_zone_zalloc(zone, flg)		\
	uma_zalloc(zone, (flg) | M_ZERO)

#define	kmem_alloc(len, flg)			\
	malloc(len, M_XFS, flg)
#define	kmem_zalloc(len, flg)			\
	malloc(len, M_XFS, (flg) | M_ZERO)
#define kmem_free(ptr, size)			\
	free(ptr, M_XFS)
#define kmem_realloc(ptr, nsize, osize, flg)	\
	realloc(ptr, nsize, M_XFS, flg)

MALLOC_DECLARE(M_XFS);

#endif /* __XFS_SUPPORT_KMEM_H__ */
