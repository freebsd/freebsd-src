/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2007-2009 Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of Google Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (C) 2005 Csaba Henk.
 * All rights reserved.
 *
 * Copyright (c) 2019 The FreeBSD Foundation
 *
 * Portions of this software were developed by BFF Storage Systems, LLC under
 * sponsorship from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/types.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/sdt.h>
#include <sys/sysctl.h>

#include "fuse.h"
#include "fuse_file.h"
#include "fuse_ipc.h"
#include "fuse_internal.h"
#include "fuse_node.h"

static void fuse_bringdown(eventhandler_tag eh_tag);
static int fuse_loader(struct module *m, int what, void *arg);

struct mtx fuse_mtx;

extern struct vfsops fuse_vfsops;
extern struct cdevsw fuse_cdevsw;
extern struct vop_vector fuse_fifonops;
extern uma_zone_t fuse_pbuf_zone;

static struct vfsconf fuse_vfsconf = {
	.vfc_version = VFS_VERSION,
	.vfc_name = "fusefs",
	.vfc_vfsops = &fuse_vfsops,
	.vfc_typenum = -1,
	.vfc_flags = VFCF_JAIL | VFCF_SYNTHETIC
};

SYSCTL_NODE(_vfs, OID_AUTO, fusefs, CTLFLAG_RW, 0, "FUSE tunables");
SYSCTL_NODE(_vfs_fusefs, OID_AUTO, stats, CTLFLAG_RW, 0, "FUSE statistics");
SYSCTL_INT(_vfs_fusefs, OID_AUTO, kernelabi_major, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, FUSE_KERNEL_VERSION, "FUSE kernel abi major version");
SYSCTL_INT(_vfs_fusefs, OID_AUTO, kernelabi_minor, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, FUSE_KERNEL_MINOR_VERSION, "FUSE kernel abi minor version");
SDT_PROVIDER_DEFINE(fusefs);

/******************************
 *
 * >>> Module management stuff
 *
 ******************************/

static void
fuse_bringdown(eventhandler_tag eh_tag)
{
	fuse_node_destroy();
	fuse_internal_destroy();
	fuse_file_destroy();
	fuse_ipc_destroy();
	fuse_device_destroy();
	mtx_destroy(&fuse_mtx);
}

static int
fuse_loader(struct module *m, int what, void *arg)
{
	static eventhandler_tag eh_tag = NULL;
	int err = 0;

	switch (what) {
	case MOD_LOAD:			/* kldload */
		mtx_init(&fuse_mtx, "fuse_mtx", NULL, MTX_DEF);
		err = fuse_device_init();
		if (err) {
			mtx_destroy(&fuse_mtx);
			return (err);
		}
		fuse_ipc_init();
		fuse_file_init();
		fuse_internal_init();
		fuse_node_init();
		fuse_pbuf_zone = pbuf_zsecond_create("fusepbuf", nswbuf / 2);

		/* vfs_modevent ignores its first arg */
		if ((err = vfs_modevent(NULL, what, &fuse_vfsconf)))
			fuse_bringdown(eh_tag);
		break;
	case MOD_UNLOAD:
		if ((err = vfs_modevent(NULL, what, &fuse_vfsconf)))
			return (err);
		fuse_bringdown(eh_tag);
		uma_zdestroy(fuse_pbuf_zone);
		break;
	default:
		return (EINVAL);
	}

	return (err);
}

/* Registering the module */

static moduledata_t fuse_moddata = {
	"fusefs",
	fuse_loader,
	&fuse_vfsconf
};

DECLARE_MODULE(fusefs, fuse_moddata, SI_SUB_VFS, SI_ORDER_MIDDLE);
MODULE_VERSION(fusefs, 1);
