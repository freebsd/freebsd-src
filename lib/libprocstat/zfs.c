/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2007 Ulf Lilleengen
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#define _WANT_MOUNT
#include <sys/mount.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/vnode.h>
#define _WANT_ZNODE
#include <sys/zfs_context.h>
#include <sys/zfs_znode.h>

#include <netinet/in.h>

#include <err.h>
#include <kvm.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define ZFS
#include "libprocstat.h"
#include "common_kvm.h"

int
zfs_filestat(kvm_t *kd, struct vnode *vp, struct vnstat *vn)
{

	struct mount mount, *mountptr;
	znode_t *kznodeptr, *znode;
	size_t len;
	int size;

	len = sizeof(size);
	if (sysctlbyname("debug.sizeof.znode", &size, &len, NULL, 0) == -1) {
		warnx("error getting sysctl");
		return (1);
	}
	znode = malloc(size);
	if (znode == NULL) {
		warnx("error allocating memory for znode storage");
		return (1);
	}

	if ((size_t)size != sizeof(znode_t))
		warnx("znode_t size mismatch, data could be wrong");

	if ((size_t)size < offsetof(znode_t, z_id) + sizeof(znode->z_id) ||
	    (size_t)size < offsetof(znode_t, z_mode) + sizeof(znode->z_mode) ||
	    (size_t)size < offsetof(znode_t, z_size) + sizeof(znode->z_size)) {
		warnx("znode_t size is too small");
		goto bad;
	}

	/*
	 * OpenZFS's libspl provides a dummy sys/vnode.h that shadows ours so
	 * struct vnode is an incomplete type. Use the wrapper until that is
	 * resolved.
	 */
	kznodeptr = getvnodedata(vp);
	if (!kvm_read_all(kd, (unsigned long)kznodeptr, znode, (size_t)size)) {
		warnx("can't read znode at %p", (void *)kznodeptr);
		goto bad;
	}

	/* Get the mount pointer, and read from the address. */
	mountptr = getvnodemount(vp);
	if (!kvm_read_all(kd, (unsigned long)mountptr, &mount, sizeof(mount))) {
		warnx("can't read mount at %p", (void *)mountptr);
		goto bad;
	}

	/*
	 * XXX Assume that this is a znode, but it can be a special node
	 * under .zfs/.
	 */
	vn->vn_fsid = mount.mnt_stat.f_fsid.val[0];
	vn->vn_fileid = znode->z_id;
	vn->vn_mode = znode->z_mode;
	vn->vn_size = znode->z_size;
	return (0);
bad:
	return (1);
}
