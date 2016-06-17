/* 
 * linux/include/linux/hfs_fs_i.h
 *
 * Copyright (C) 1995, 1996  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file defines the type (struct hfs_inode_info) and the two
 * subordinate types hfs_extent and hfs_file.
 */

#ifndef _LINUX_HFS_FS_I_H
#define _LINUX_HFS_FS_I_H

/*
 * struct hfs_inode_info
 *
 * The HFS-specific part of a Linux (struct inode)
 */
struct hfs_inode_info {
	int				magic;     /* A magic number */

	unsigned long			mmu_private;
	struct hfs_cat_entry		*entry;

	/* For a regular or header file */
	struct hfs_fork 		*fork;
	int				convert;

	/* For a directory */
	ino_t				file_type;
	char				dir_size;

	/* For header files */
	const struct hfs_hdr_layout	*default_layout;
	struct hfs_hdr_layout		*layout;

	/* to deal with localtime ugliness */
	int                             tz_secondswest;

        /* for dentry cleanup */
        void (*d_drop_op)(struct dentry *, const ino_t);
};

#endif
