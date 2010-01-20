#ifndef __XFS_CONFIG_H__
#define	__XFS_CONFIG_H__

#define	HAVE_FID		1
/*
 * Linux config variables, harcoded to values desirable for FreeBSD.
 */
#define	CONFIG_SYSCTL		1
#define	CONFIG_LBD		1
#define	CONFIG_XFS_TRACE	0

/*
 * Tracing.
 */
#if CONFIG_XFS_TRACE == 1
#define	XFS_ALLOC_TRACE		1
#define	XFS_ALLOC_TRACE		1
#define	XFS_ATTR_TRACE		1
#define	XFS_BLI_TRACE		1
#define	XFS_BMAP_TRACE		1
#define	XFS_BMBT_TRACE		1
#define	XFS_DIR_TRACE		1
#define	XFS_DIR2_TRACE		1
#define	XFS_DQUOT_TRACE		1
#define	XFS_ILOCK_TRACE		1
#define	XFS_LOG_TRACE		1
#define	XFS_RW_TRACE		1
#endif

/*
 * XFS config defines.
 */
#define XFS_BIG_BLKNOS		1
#define XFS_BIG_INUMS		0

#undef XFS_STATS_OFF

#endif /* __XFS_CONFIG_H__ */
