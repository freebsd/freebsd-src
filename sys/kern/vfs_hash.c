/*-
 * Copyright (c) 2005 Poul-Henning Kamp
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/vnode.h>

#if 0
#include "opt_mac.h"

#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/event.h>
#include <sys/eventhandler.h>
#include <sys/extattr.h>
#include <sys/fcntl.h>
#include <sys/kdb.h>
#include <sys/kthread.h>
#include <sys/mac.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/reboot.h>
#include <sys/sleepqueue.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/vmmeter.h>

#include <machine/stdarg.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>
#include <vm/uma.h>

#endif

static MALLOC_DEFINE(M_VFS_HASH, "VFS hash", "VFS hash table");

static LIST_HEAD(vfs_hashhead, vnode)	*vfs_hash_tbl;
static u_long				vfs_hash_mask;
static struct mtx			vfs_hash_mtx;

static void
vfs_hashinit(void *dummy __unused)
{

	vfs_hash_tbl = hashinit(desiredvnodes, M_VFS_HASH, &vfs_hash_mask);
	mtx_init(&vfs_hash_mtx, "vfs hash", NULL, MTX_DEF);
}

/* Must be SI_ORDER_SECOND so desiredvnodes is available */
SYSINIT(vfs_hash, SI_SUB_VFS, SI_ORDER_SECOND, vfs_hashinit, NULL)

int
vfs_hash_get(struct mount *mp, u_int hash, int flags, struct thread *td, struct vnode **vpp)
{
	struct vnode *vp;
	int error;

	while (1) {
		mtx_lock(&vfs_hash_mtx);
		LIST_FOREACH(vp,
		    &vfs_hash_tbl[hash & vfs_hash_mask], v_hashlist) {
			if (vp->v_hash != hash)
				continue;
			if (vp->v_mount != mp)
				continue;
			VI_LOCK(vp);
			mtx_unlock(&vfs_hash_mtx);
			error = vget(vp, flags | LK_INTERLOCK, td);
			if (error == ENOENT)
				break;
			if (error)
				return (error);
			*vpp = vp;
			return (0);
		}
		if (vp == NULL) {
			mtx_unlock(&vfs_hash_mtx);
			*vpp = NULL;
			return (0);
		}
	}
}

void
vfs_hash_remove(struct vnode *vp)
{

	mtx_lock(&vfs_hash_mtx);
	VI_LOCK(vp);
	VNASSERT(vp->v_iflag & VI_HASHED, vp, ("vfs_hash_remove: not hashed"));
	LIST_REMOVE(vp, v_hashlist);
	vp->v_iflag &= ~VI_HASHED;
	VI_UNLOCK(vp);
	mtx_unlock(&vfs_hash_mtx);
}

int
vfs_hash_insert(struct vnode *vp, u_int hash, int flags, struct thread *td, struct vnode **vpp)
{
	struct vnode *vp2;
	int error;

	while (1) {
		mtx_lock(&vfs_hash_mtx);
		LIST_FOREACH(vp2,
		    &vfs_hash_tbl[hash & vfs_hash_mask], v_hashlist) {
			if (vp2->v_hash != hash)
				continue;
			if (vp2->v_mount != vp->v_mount)
				continue;
			VI_LOCK(vp2);
			mtx_unlock(&vfs_hash_mtx);
			error = vget(vp2, flags | LK_INTERLOCK, td);
			if (error == ENOENT)
				break;
			if (error)
				return (error);
			*vpp = vp2;
			return (0);
		}
		if (vp2 == NULL)
			break;
			
	}
	VI_LOCK(vp);
	VNASSERT(!(vp->v_iflag & VI_HASHED), vp,
	    ("vfs_hash_insert: already hashed"));
	vp->v_hash = hash;
	LIST_INSERT_HEAD(&vfs_hash_tbl[hash & vfs_hash_mask], vp, v_hashlist);
	vp->v_iflag |= VI_HASHED;
	VI_UNLOCK(vp);
	mtx_unlock(&vfs_hash_mtx);
	*vpp = NULL;
	return (0);
}

void
vfs_hash_rehash(struct vnode *vp, u_int hash)
{

	mtx_lock(&vfs_hash_mtx);
	VNASSERT(vp->v_iflag & VI_HASHED, vp, ("vfs_hash_rehash: not hashed"));
	LIST_REMOVE(vp, v_hashlist);
	LIST_INSERT_HEAD(&vfs_hash_tbl[hash & vfs_hash_mask], vp, v_hashlist);
	vp->v_hash = hash;
	mtx_unlock(&vfs_hash_mtx);
}
