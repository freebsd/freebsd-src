/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef __XFS_IOPS_H__
#define __XFS_IOPS_H__

/*
 * Extended system attributes.
 * So far only POSIX ACLs are supported, but this will need to
 * grow in time (capabilities, mandatory access control, etc).
 */
#define XFS_SYSTEM_NAMESPACE	SYSTEM_POSIXACL

/*
 * Define a table of the namespaces XFS supports
 */
typedef int (*xattr_exists_t)(xfs_vnode_t *);

typedef struct xattr_namespace {
	char		*name;
	unsigned int	namelen;
	xattr_exists_t	exists;
} xattr_namespace_t;

#define SYSTEM_NAMES	0
#define ROOT_NAMES	1
#define USER_NAMES	2
extern struct xattr_namespace *xfs_namespaces;

extern int xfs_ioctl(struct bhv_desc *, struct inode *, struct file *,
		    int, unsigned int, void *);

#endif /* __XFS_IOPS_H__ */
