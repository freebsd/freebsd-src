/*
 * CDDL HEADER SART
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2013 Martin Matuska <mm@FreeBSD.org>. All rights reserved.
 */

#ifndef	_LIBZFS_COMPAT_H
#define	_LIBZFS_COMPAT_H

#include <zfs_ioctl_compat.h>

#ifdef	__cplusplus
extern "C" {
#endif

static int zfs_kernel_version = 0;
static int zfs_ioctl_version = 0;

/*
 * This is FreeBSD version of ioctl, because Solaris' ioctl() updates
 * zc_nvlist_dst_size even if an error is returned, on FreeBSD if an
 * error is returned zc_nvlist_dst_size won't be updated.
 */
static __inline int
zcmd_ioctl(int fd, int request, zfs_cmd_t *zc)
{
	size_t oldsize, zfs_kernel_version_size, zfs_ioctl_version_size;
	int version, ret, cflag = ZFS_CMD_COMPAT_NONE;

	zfs_ioctl_version_size = sizeof(zfs_ioctl_version);
	if (zfs_ioctl_version == 0) {
		sysctlbyname("vfs.zfs.version.ioctl", &zfs_ioctl_version,
		    &zfs_ioctl_version_size, NULL, 0);
	}

	if (zfs_ioctl_version == ZFS_IOCVER_DEADMAN)
		cflag = ZFS_CMD_COMPAT_DEADMAN;

	/*
	 * If vfs.zfs.version.ioctl is not defined, assume we have v28
	 * compatible binaries and use vfs.zfs.version.spa to test for v15
	 */
	if (zfs_ioctl_version < ZFS_IOCVER_DEADMAN) {
		cflag = ZFS_CMD_COMPAT_V28;
		zfs_kernel_version_size = sizeof(zfs_kernel_version);

		if (zfs_kernel_version == 0) {
			sysctlbyname("vfs.zfs.version.spa",
			    &zfs_kernel_version,
			    &zfs_kernel_version_size, NULL, 0);
		}

		if (zfs_kernel_version == SPA_VERSION_15 ||
		    zfs_kernel_version == SPA_VERSION_14 ||
		    zfs_kernel_version == SPA_VERSION_13)
			cflag = ZFS_CMD_COMPAT_V15;
	}

	oldsize = zc->zc_nvlist_dst_size;
	ret = zcmd_ioctl_compat(fd, request, zc, cflag);

	if (ret == 0 && oldsize < zc->zc_nvlist_dst_size) {
		ret = -1;
		errno = ENOMEM;
	}

	return (ret);
}
#define	ioctl(fd, ioc, zc)	zcmd_ioctl((fd), (ioc), (zc))

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBZFS_COMPAT_H */
