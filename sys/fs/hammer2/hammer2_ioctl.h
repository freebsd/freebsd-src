/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2022 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2011-2022 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _FS_HAMMER2_IOCTL_H_
#define _FS_HAMMER2_IOCTL_H_

#include <sys/param.h>
#include <sys/ioccom.h>
#include <sys/syslimits.h>

#include "hammer2_disk.h"

/*
 * Ioctl to get version.
 */
struct hammer2_ioc_version {
	int			version;
	char			reserved[256 - 4];
};

typedef struct hammer2_ioc_version hammer2_ioc_version_t;

/*
 * Ioctls to manage PFSs.
 *
 * PFSs can be clustered by matching their pfs_clid, and the PFSs making up
 * a cluster can be uniquely identified by combining the vol_id with
 * the pfs_clid.
 */
struct hammer2_ioc_pfs {
	hammer2_key_t		name_key;	/* super-root directory scan */
	hammer2_key_t		name_next;	/* (GET only) */
	uint8_t			pfs_type;
	uint8_t			pfs_subtype;
	uint8_t			reserved0012;
	uint8_t			reserved0013;
	uint32_t		pfs_flags;
	uint64_t		reserved0018;
	struct uuid		pfs_fsid;	/* identifies PFS instance */
	struct uuid		pfs_clid;	/* identifies PFS cluster */
	char			name[NAME_MAX+1]; /* PFS label */
};

typedef struct hammer2_ioc_pfs hammer2_ioc_pfs_t;

/*
 * Ioctl to manage inodes.
 */
struct hammer2_ioc_inode {
	uint32_t		flags;
	void			*unused;
	hammer2_key_t		data_count;
	hammer2_key_t		inode_count;
	hammer2_inode_data_t	ip_data;
};

typedef struct hammer2_ioc_inode hammer2_ioc_inode_t;

/*
 * Ioctl to manage volumes.
 */
struct hammer2_ioc_volume {
	char			path[MAXPATHLEN];
	int			id;
	hammer2_off_t		offset;
	hammer2_off_t		size;
};

typedef struct hammer2_ioc_volume hammer2_ioc_volume_t;

struct hammer2_ioc_volume_list {
	hammer2_ioc_volume_t	*volumes;
	int			nvolumes;
	int			version;
	char			pfs_name[HAMMER2_INODE_MAXNAME];
};

typedef struct hammer2_ioc_volume_list hammer2_ioc_volume_list_t;

/*
 * Ioctl list.
 */
#define HAMMER2IOC_VERSION_GET	_IOWR('h', 64, struct hammer2_ioc_version)
#define HAMMER2IOC_PFS_GET	_IOWR('h', 80, struct hammer2_ioc_pfs)
#define HAMMER2IOC_PFS_LOOKUP	_IOWR('h', 83, struct hammer2_ioc_pfs)
#define HAMMER2IOC_INODE_GET	_IOWR('h', 86, struct hammer2_ioc_inode)
#define HAMMER2IOC_DEBUG_DUMP	_IOWR('h', 91, int)
#define HAMMER2IOC_VOLUME_LIST	_IOWR('h', 97, struct hammer2_ioc_volume_list)

#endif /* !_FS_HAMMER2_IOCTL_H_ */
