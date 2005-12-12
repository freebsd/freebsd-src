/*
 * Copyright (c) 2001-2002 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

#ifndef __XFS_SYSCTL_H__
#define __XFS_SYSCTL_H__

/*
 * Tunable xfs parameters
 */

typedef struct xfs_sysctl_val {
	int min;
	int val;
	int max;
} xfs_sysctl_val_t;

typedef struct xfs_param {
	xfs_sysctl_val_t refcache_size;	/* Size of NFS reference cache.      */
	xfs_sysctl_val_t refcache_purge;/* # of entries to purge each time.  */
	xfs_sysctl_val_t restrict_chown;/* Root/non-root can give away files.*/
	xfs_sysctl_val_t sgid_inherit;	/* Inherit S_ISGID bit if process' GID 
					 * is not a member of the parent dir
					 * GID */
	xfs_sysctl_val_t symlink_mode;	/* Link creat mode affected by umask */
	xfs_sysctl_val_t panic_mask;	/* bitmask to cause panic on errors. */
	xfs_sysctl_val_t error_level;	/* Degree of reporting for problems  */
	xfs_sysctl_val_t sync_interval;	/* time between sync calls           */
	xfs_sysctl_val_t stats_clear;	/* Reset all XFS statistics to zero. */
	xfs_sysctl_val_t probe_dmapi;	/* probe for DMAPI module on mount. */
	xfs_sysctl_val_t probe_ioops;	/* probe for an IO module on mount. */
	xfs_sysctl_val_t probe_quota;	/* probe for quota module on mount. */
	xfs_sysctl_val_t inherit_sync;	/* Inherit the "sync" inode flag. */
	xfs_sysctl_val_t inherit_nodump;/* Inherit the "nodump" inode flag. */
	xfs_sysctl_val_t inherit_noatim;/* Inherit the "noatime" inode flag. */
	xfs_sysctl_val_t flush_interval;/* interval between runs of the
					 * delwri flush daemon.  */
	xfs_sysctl_val_t age_buffer;	/* time for buffer to age before
					 * we flush it.  */
	xfs_sysctl_val_t io_bypass;	/* toggle for directio io bypass */
} xfs_param_t;

/*
 * xfs_error_level:
 *
 * How much error reporting will be done when internal problems are
 * encountered.  These problems normally return an EFSCORRUPTED to their
 * caller, with no other information reported.
 *
 * 0	No error reports
 * 1	Report EFSCORRUPTED errors that will cause a filesystem shutdown
 * 5	Report all EFSCORRUPTED errors (all of the above errors, plus any
 *	additional errors that are known to not cause shutdowns)
 *
 * xfs_panic_mask bit 0x8 turns the error reports into panics
 */

enum {
	XFS_REFCACHE_SIZE = 1,
	XFS_REFCACHE_PURGE = 2,
	XFS_RESTRICT_CHOWN = 3,
	XFS_SGID_INHERIT = 4,
	XFS_SYMLINK_MODE = 5,
	XFS_PANIC_MASK = 6,
	XFS_ERRLEVEL = 7,
	XFS_SYNC_INTERVAL = 8,
	XFS_PROBE_DMAPI = 9,
	XFS_PROBE_IOOPS = 10,
	XFS_PROBE_QUOTA = 11,
	XFS_STATS_CLEAR = 12,
	XFS_INHERIT_SYNC = 13,
	XFS_INHERIT_NODUMP = 14,
	XFS_INHERIT_NOATIME = 15,
	XFS_FLUSH_INTERVAL = 16,
	XFS_AGE_BUFFER = 17,
	XFS_IO_BYPASS = 18,
};

extern xfs_param_t	xfs_params;

#ifdef CONFIG_SYSCTL
extern void xfs_sysctl_register(void);
extern void xfs_sysctl_unregister(void);
#else
# define xfs_sysctl_register()		do { } while (0)
# define xfs_sysctl_unregister()	do { } while (0)
#endif /* CONFIG_SYSCTL */

#endif /* __XFS_SYSCTL_H__ */

