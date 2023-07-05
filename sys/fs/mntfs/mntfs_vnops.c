/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Netflix, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/vnode.h>

/*
 * The "mntfs" VCHR vnodes implemented here provide a safe way for file systems
 * to access their disk devices.  Using the normal devfs vnode has the problem
 * that if the device disappears, the devfs vnode is vgone'd as part of
 * removing it from the application-visible namespace, and some file systems
 * (notably FFS with softdep) get very unhappy if their dirty buffers are
 * invalidated out from under them.  By using a separate, private vnode,
 * file systems are able to clean up their buffer state in a controlled fashion
 * when the underlying device disappears.
 */

static int
mntfs_reclaim(struct vop_reclaim_args *ap)
{
	struct vnode *vp = ap->a_vp;

	dev_rel(vp->v_rdev);
	return (0);
}

struct vop_vector mntfs_vnodeops = {
	.vop_default =		&default_vnodeops,

	.vop_fsync =		vop_stdfsync,
	.vop_strategy = 	VOP_PANIC,
	.vop_reclaim =		mntfs_reclaim,
};
VFS_VOP_VECTOR_REGISTER(mntfs_vnodeops);

/*
 * Allocate a private VCHR vnode for use by a mounted fs.
 * The underlying device will be the same as for the given vnode.
 * This mntfs vnode must be freed with mntfs_freevp() rather than just
 * releasing the reference.
 */
struct vnode *
mntfs_allocvp(struct mount *mp, struct vnode *ovp)
{
	struct vnode *vp;
	struct cdev *dev;

	ASSERT_VOP_ELOCKED(ovp, __func__);

	dev = ovp->v_rdev;

	getnewvnode("mntfs", mp, &mntfs_vnodeops, &vp);
	vp->v_type = VCHR;
	vp->v_data = NULL;
	dev_ref(dev);
	vp->v_rdev = dev;

	VOP_UNLOCK(ovp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	vn_set_state(vp, VSTATE_CONSTRUCTED);
	return (vp);
}

void
mntfs_freevp(struct vnode *vp)
{
	ASSERT_VOP_ELOCKED(vp, "mntfs_freevp");
	vgone(vp);
	vput(vp);
}
