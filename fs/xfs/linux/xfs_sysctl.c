/*
 * Copyright (c) 2001-2003 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "xfs.h"
#include "xfs_refcache.h"
#include <linux/sysctl.h>
#include <linux/proc_fs.h>


static struct ctl_table_header *xfs_table_header;


/* Custom proc handlers */

STATIC int
xfs_refcache_resize_proc_handler(
	ctl_table	*ctl,
	int		write,
	struct file	*filp,
	void		*buffer,
	size_t		*lenp)
{
	int		ret, *valp = ctl->data;
	int		xfs_refcache_new_size;
	int		xfs_refcache_old_size = *valp;

	ret = proc_dointvec_minmax(ctl, write, filp, buffer, lenp);
	xfs_refcache_new_size = *valp;

	if (!ret && write && xfs_refcache_new_size != xfs_refcache_old_size) {
		xfs_refcache_resize(xfs_refcache_new_size);
		/* Don't purge more than size of the cache */
		if (xfs_refcache_new_size < xfs_refcache_purge_count)
			xfs_refcache_purge_count = xfs_refcache_new_size;
	}

	return ret;
}

#ifdef CONFIG_PROC_FS
STATIC int
xfs_stats_clear_proc_handler(
	ctl_table	*ctl,
	int		write,
	struct file	*filp,
	void		*buffer,
	size_t		*lenp)
{
	int		ret, *valp = ctl->data;
	__uint32_t	vn_active;

	ret = proc_dointvec_minmax(ctl, write, filp, buffer, lenp);

	if (!ret && write && *valp) {
		printk("XFS Clearing xfsstats\n");
		/* save vn_active, it's a universal truth! */
		vn_active = xfsstats.vn_active;
		memset(&xfsstats, 0, sizeof(xfsstats));
		xfsstats.vn_active = vn_active;
		xfs_stats_clear = 0;
	}

	return ret;
}
#endif /* CONFIG_PROC_FS */

STATIC ctl_table xfs_table[] = {
	{XFS_REFCACHE_SIZE, "refcache_size", &xfs_params.refcache_size.val,
	sizeof(int), 0644, NULL, &xfs_refcache_resize_proc_handler,
	&sysctl_intvec, NULL, 
	&xfs_params.refcache_size.min, &xfs_params.refcache_size.max},

	/* Note, the max here is different, it is the current refcache size */
	{XFS_REFCACHE_PURGE, "refcache_purge", &xfs_params.refcache_purge.val,
	sizeof(int), 0644, NULL, &proc_dointvec_minmax,
	&sysctl_intvec, NULL, 
	&xfs_params.refcache_purge.min, &xfs_params.refcache_size.val},

	{XFS_RESTRICT_CHOWN, "restrict_chown", &xfs_params.restrict_chown.val,
	sizeof(int), 0644, NULL, &proc_dointvec_minmax,
	&sysctl_intvec, NULL, 
	&xfs_params.restrict_chown.min, &xfs_params.restrict_chown.max},

	{XFS_SGID_INHERIT, "irix_sgid_inherit", &xfs_params.sgid_inherit.val,
	sizeof(int), 0644, NULL, &proc_dointvec_minmax,
	&sysctl_intvec, NULL, 
	&xfs_params.sgid_inherit.min, &xfs_params.sgid_inherit.max},

	{XFS_SYMLINK_MODE, "irix_symlink_mode", &xfs_params.symlink_mode.val,
	sizeof(int), 0644, NULL, &proc_dointvec_minmax,
	&sysctl_intvec, NULL, 
	&xfs_params.symlink_mode.min, &xfs_params.symlink_mode.max},

	{XFS_PANIC_MASK, "panic_mask", &xfs_params.panic_mask.val,
	sizeof(int), 0644, NULL, &proc_dointvec_minmax,
	&sysctl_intvec, NULL, 
	&xfs_params.panic_mask.min, &xfs_params.panic_mask.max},

	{XFS_ERRLEVEL, "error_level", &xfs_params.error_level.val,
	sizeof(int), 0644, NULL, &proc_dointvec_minmax,
	&sysctl_intvec, NULL, 
	&xfs_params.error_level.min, &xfs_params.error_level.max},

	{XFS_SYNC_INTERVAL, "sync_interval", &xfs_params.sync_interval.val,
	sizeof(int), 0644, NULL, &proc_dointvec_minmax,
	&sysctl_intvec, NULL, 
	&xfs_params.sync_interval.min, &xfs_params.sync_interval.max},

	{XFS_INHERIT_SYNC, "inherit_sync", &xfs_params.inherit_sync.val,
	sizeof(int), 0644, NULL, &proc_dointvec_minmax,
	&sysctl_intvec, NULL,
	&xfs_params.inherit_sync.min, &xfs_params.inherit_sync.max},

	{XFS_INHERIT_NODUMP, "inherit_nodump", &xfs_params.inherit_nodump.val,
	sizeof(int), 0644, NULL, &proc_dointvec_minmax,
	&sysctl_intvec, NULL,
	&xfs_params.inherit_nodump.min, &xfs_params.inherit_nodump.max},

	{XFS_INHERIT_NOATIME, "inherit_noatime", &xfs_params.inherit_noatim.val,
	sizeof(int), 0644, NULL, &proc_dointvec_minmax,
	&sysctl_intvec, NULL,
	&xfs_params.inherit_noatim.min, &xfs_params.inherit_noatim.max},
	
	{XFS_FLUSH_INTERVAL, "flush_interval", &xfs_params.flush_interval.val,
	sizeof(int), 0644, NULL, &proc_dointvec_minmax,
	&sysctl_intvec, NULL,
	&xfs_params.flush_interval.min, &xfs_params.flush_interval.max},

	{XFS_AGE_BUFFER, "age_buffer", &xfs_params.age_buffer.val,
	sizeof(int), 0644, NULL, &proc_dointvec_minmax,
	&sysctl_intvec, NULL,
	&xfs_params.age_buffer.min, &xfs_params.age_buffer.max},

	/* please keep this the last entry */
#ifdef CONFIG_PROC_FS
	{XFS_STATS_CLEAR, "stats_clear", &xfs_params.stats_clear.val,
	sizeof(int), 0644, NULL, &xfs_stats_clear_proc_handler,
	&sysctl_intvec, NULL, 
	&xfs_params.stats_clear.min, &xfs_params.stats_clear.max},
#endif /* CONFIG_PROC_FS */

	{0}
};

STATIC ctl_table xfs_dir_table[] = {
	{FS_XFS, "xfs", NULL, 0, 0555, xfs_table},
	{0}
};

STATIC ctl_table xfs_root_table[] = {
	{CTL_FS, "fs",  NULL, 0, 0555, xfs_dir_table},
	{0}
};

void
xfs_sysctl_register(void)
{
	xfs_table_header = register_sysctl_table(xfs_root_table, 1);
}

void
xfs_sysctl_unregister(void)
{
	if (xfs_table_header)
		unregister_sysctl_table(xfs_table_header);
}
