/* 
 * linux/include/linux/hfs_fs_sb.h
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file defines the type (struct hfs_sb_info) which contains the
 * HFS-specific information in the in-core superblock.
 */

#ifndef _LINUX_HFS_FS_SB_H
#define _LINUX_HFS_FS_SB_H

/* forward declaration: */
struct hfs_name;

typedef int (*hfs_namein_fn) (char *, const struct hfs_name *);
typedef void (*hfs_nameout_fn) (struct hfs_name *, const char *, int);
typedef void (*hfs_ifill_fn) (struct inode *, ino_t, const int);

/*
 * struct hfs_sb_info
 *
 * The HFS-specific part of a Linux (struct super_block)
 */
struct hfs_sb_info {
	int			magic;		/* A magic number */
	struct hfs_mdb		*s_mdb;		/* The HFS MDB */
	int			s_quiet;	/* Silent failure when 
						   changing owner or mode? */
	int			s_lowercase;	/* Map names to lowercase? */
	int			s_afpd;		/* AFPD compatible mode? */
	int                     s_version;      /* version info */
	hfs_namein_fn		s_namein;	/* The function used to
						   map Mac filenames to
						   Linux filenames */
	hfs_nameout_fn		s_nameout;	/* The function used to
						    map Linux filenames
						    to Mac filenames */
	hfs_ifill_fn		s_ifill;	/* The function used
						   to fill in inode fields */
	const struct hfs_name	*s_reserved1;	/* Reserved names */
	const struct hfs_name	*s_reserved2;	/* Reserved names */
	__u32			s_type;		/* Type for new files */
	__u32			s_creator;	/* Creator for new files */
	umode_t			s_umask;	/* The umask applied to the
						   permissions on all files */
	uid_t			s_uid;		/* The uid of all files */
	gid_t			s_gid;		/* The gid of all files */
	char			s_conv;		/* Type of text conversion */
};

#endif
