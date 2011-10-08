/*-
 * Copyright (c) 2005-2009 Stanislav Sedov <stas@FreeBSD.org>
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
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/mount.h>

#include <netinet/in.h>

#include <assert.h>
#include <err.h>
#include <kvm.h>
#include <stdlib.h>

#include <fs/ntfs/ntfs.h>
#include <fs/ntfs/ntfs_inode.h>

#include "libprocstat.h"
#include "common_kvm.h"

int
ntfs_filestat(kvm_t *kd, struct vnode *vp, struct vnstat *vn)
{
	struct fnode fnod;
	struct ntnode node;
	int error;

	assert(kd);
	assert(vn);
	error = kvm_read_all(kd, (unsigned long)VTOF(vp), &fnod, sizeof(fnod));
	if (error != 0) {
		warnx("can't read ntfs fnode at %p", (void *)VTOF(vp));
		return (1);
	}
	error = kvm_read_all(kd, (unsigned long)FTONT(&fnod), &node,
	    sizeof(node));
	if (error != 0) {
		warnx("can't read ntfs node at %p", (void *)FTONT(&fnod));
		return (1);
	}
	vn->vn_fileid = node.i_number;
	vn->vn_fsid = dev2udev(kd, node.i_dev);
	return (0);
}
