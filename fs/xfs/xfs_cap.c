/*
 * Copyright (c) 2002 Silicon Graphics, Inc.  All Rights Reserved.
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

STATIC int xfs_cap_allow_set(vnode_t *);


/*
 * Test for existence of capability attribute as efficiently as possible.
 */
int
xfs_cap_vhascap(
	vnode_t		*vp)
{
	int		error;
	int		len = sizeof(xfs_cap_set_t);
	int		flags = ATTR_KERNOVAL|ATTR_ROOT;

	VOP_ATTR_GET(vp, SGI_CAP_LINUX, NULL, &len, flags, sys_cred, error);
	return (error == 0);
}

/*
 * Convert from extended attribute representation to in-memory for XFS.
 */
STATIC int
posix_cap_xattr_to_xfs(
	posix_cap_xattr		*src,
	size_t			size,
	xfs_cap_set_t		*dest)
{
	if (!src || !dest)
		return EINVAL;

	if (src->c_version != cpu_to_le32(POSIX_CAP_XATTR_VERSION))
		return EINVAL;
	if (src->c_abiversion != cpu_to_le32(_LINUX_CAPABILITY_VERSION))
		return EINVAL;

	if (size < sizeof(posix_cap_xattr))
		return EINVAL;

	ASSERT(sizeof(dest->cap_effective) == sizeof(src->c_effective));

	dest->cap_effective	= src->c_effective;
	dest->cap_permitted	= src->c_permitted;
	dest->cap_inheritable	= src->c_inheritable;

	return 0;
}

/*
 * Convert from in-memory XFS to extended attribute representation.
 */
STATIC int
posix_cap_xfs_to_xattr(
	xfs_cap_set_t		*src,
	posix_cap_xattr		*xattr_cap,
	size_t			size)
{
	size_t			new_size = posix_cap_xattr_size();

	if (size < new_size)
		return -ERANGE;

	ASSERT(sizeof(xattr_cap->c_effective) == sizeof(src->cap_effective));

	xattr_cap->c_version	= cpu_to_le32(POSIX_CAP_XATTR_VERSION);
	xattr_cap->c_abiversion	= cpu_to_le32(_LINUX_CAPABILITY_VERSION);
	xattr_cap->c_effective	= src->cap_effective;
	xattr_cap->c_permitted	= src->cap_permitted;
	xattr_cap->c_inheritable= src->cap_inheritable;

	return new_size;
}

int
xfs_cap_vget(
	vnode_t		*vp,
	void		*cap,
	size_t		size)
{
	int		error;
	int		len = sizeof(xfs_cap_set_t);
	int		flags = ATTR_ROOT;
	xfs_cap_set_t	xfs_cap = { 0 };
	posix_cap_xattr	*xattr_cap = cap;
	char		*data = (char *)&xfs_cap;

	VN_HOLD(vp);
	if ((error = _MAC_VACCESS(vp, NULL, VREAD)))
		goto out;

	if (!size) {
		flags |= ATTR_KERNOVAL;
		data = NULL;
	}
	VOP_ATTR_GET(vp, SGI_CAP_LINUX, data, &len, flags, sys_cred, error);
	if (error)
		goto out;
	ASSERT(len == sizeof(xfs_cap_set_t));

	error = (size)? -posix_cap_xattr_size() :
			-posix_cap_xfs_to_xattr(&xfs_cap, xattr_cap, size);
out:
	VN_RELE(vp);
	return -error;
}

int
xfs_cap_vremove(
	vnode_t		*vp)
{
	int		error;

	VN_HOLD(vp);
	error = xfs_cap_allow_set(vp);
	if (!error) {
		VOP_ATTR_REMOVE(vp, SGI_CAP_LINUX, ATTR_ROOT, sys_cred, error);
		if (error == ENOATTR)
			error = 0;	/* 'scool */
	}
	VN_RELE(vp);
	return -error;
}

int
xfs_cap_vset(
	vnode_t			*vp,
	void			*cap,
	size_t			size)
{
	posix_cap_xattr		*xattr_cap = cap;
	xfs_cap_set_t		xfs_cap;
	int			error;

	if (!cap)
		return -EINVAL;

	error = posix_cap_xattr_to_xfs(xattr_cap, size, &xfs_cap);
	if (error)
		return -error;

	VN_HOLD(vp);
	error = xfs_cap_allow_set(vp);
	if (error)
		goto out;

	VOP_ATTR_SET(vp, SGI_CAP_LINUX, (char *)&xfs_cap,
			sizeof(xfs_cap_set_t), ATTR_ROOT, sys_cred, error);
out:
	VN_RELE(vp);
	return -error;
}

STATIC int
xfs_cap_allow_set(
	vnode_t		*vp)
{
	vattr_t		va;
	int		error;

	if (vp->v_vfsp->vfs_flag & VFS_RDONLY)
		return EROFS;
	if (vp->v_inode.i_flags & (S_IMMUTABLE|S_APPEND))
		return EPERM;
	if ((error = _MAC_VACCESS(vp, NULL, VWRITE)))
		return error;
	va.va_mask = XFS_AT_UID;
	VOP_GETATTR(vp, &va, 0, NULL, error);
	if (error)
		return error;
	if (va.va_uid != current->fsuid && !capable(CAP_FOWNER))
		return EPERM;
	return error;
}
