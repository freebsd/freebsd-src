/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "xfs_macros.h"
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_trans_priv.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_dmapi.h"
#include "xfs_mount.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_btree.h"
#include "xfs_imap.h"
#include "xfs_alloc.h"
#include "xfs_ialloc.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_inode.h"

void
vn_init(void)
{
}

struct xfs_vnode *
vn_initialize(
	xfs_vnode_t	*vp)
{
	XFS_STATS_INC(vn_active);
	XFS_STATS_INC(vn_alloc);

	/* Initialize the first behavior and the behavior chain head. */
	vn_bhv_head_init(VN_BHV_HEAD(vp), "vnode");

#ifdef	CONFIG_XFS_VNODE_TRACING
	vp->v_trace = ktrace_alloc(VNODE_TRACE_SIZE, KM_SLEEP);
#endif	/* CONFIG_XFS_VNODE_TRACING */

	vn_trace_exit(vp, "vn_initialize", (inst_t *)__return_address);
	return vp;
}

/*
 * Get a reference on a vnode. Need to drop vnode reference
 * to accomodate for vhold by VMAP regardless of whether or
 * not we were able to successfully grab the vnode.
 */
xfs_vnode_t *
vn_get(
	struct xfs_vnode	*xfs_vp,
	vmap_t			*vmap)
{
	struct vnode *vp;
	int error;

	XFS_STATS_INC(vn_get);

	vp = vmap->v_vp;

	error = vget(vp, 0, curthread);
	if (error) {
		vdrop(vp);
		return (NULL);
	}

	vdrop(vp);
	if (vp->v_data != xfs_vp) {
		vput(vp);
		return (NULL);
	}

	vn_trace_exit(vp, "vn_get", (inst_t *)__return_address);
	return xfs_vp;
}

/*
 * purge a vnode from the cache
 * At this point the vnode is guaranteed to have no references (vn_count == 0)
 * The caller has to make sure that there are no ways someone could
 * get a handle (via vn_get) on the vnode (usually done via a mount/vfs lock).
 */
void
vn_purge(
	struct xfs_vnode	*xfs_vp,
	vmap_t			*vmap)
{
	struct vnode *vp;

	vn_trace_entry(vp, "vn_purge", (inst_t *)__return_address);

	vp = vmap->v_vp;

	vn_lock(vp, LK_EXCLUSIVE, curthread);
	vgone(vp);
	VOP_UNLOCK(vp, 0, curthread);
	vdrop(vp);
}

/*
 * Finish the removal of a vnode.
 */
void
vn_remove(
	struct xfs_vnode *vp)
{
	vmap_t		vmap;

	/* Make sure we don't do this to the same vnode twice */
	if (!(vp->v_fbhv))
		return;

	XFS_STATS_INC(vn_remove);
	vn_trace_exit(vp, "vn_remove", (inst_t *)__return_address);
	/*
	 * After the following purge the vnode
	 * will no longer exist.
	 */
	VMAP(vp, vmap);
	vn_purge(vp, &vmap);
}


#ifdef	CONFIG_XFS_VNODE_TRACING

#define KTRACE_ENTER(vp, vk, s, line, ra)			\
	ktrace_enter(	(vp)->v_trace,				\
/*  0 */		(void *)(__psint_t)(vk),		\
/*  1 */		(void *)(s),				\
/*  2 */		(void *)(__psint_t) line,		\
/*  3 */		(void *)(vn_count(vp)), \
/*  4 */		(void *)(ra),				\
/*  5 */		(void *)(__psunsigned_t)(vp)->v_flag,	\
/*  6 */		(void *)(__psint_t)smp_processor_id(),	\
/*  7 */		(void *)(__psint_t)(current->pid),	\
/*  8 */		(void *)__return_address,		\
/*  9 */		0, 0, 0, 0, 0, 0, 0)

/*
 * Vnode tracing code.
 */
void
vn_trace_entry(xfs_vnode_t *vp, char *func, inst_t *ra)
{
	KTRACE_ENTER(vp, VNODE_KTRACE_ENTRY, func, 0, ra);
}

void
vn_trace_exit(xfs_vnode_t *vp, char *func, inst_t *ra)
{
	KTRACE_ENTER(vp, VNODE_KTRACE_EXIT, func, 0, ra);
}

void
vn_trace_hold(xfs_vnode_t *vp, char *file, int line, inst_t *ra)
{
	KTRACE_ENTER(vp, VNODE_KTRACE_HOLD, file, line, ra);
}

void
vn_trace_ref(xfs_vnode_t *vp, char *file, int line, inst_t *ra)
{
	KTRACE_ENTER(vp, VNODE_KTRACE_REF, file, line, ra);
}

void
vn_trace_rele(xfs_vnode_t *vp, char *file, int line, inst_t *ra)
{
	KTRACE_ENTER(vp, VNODE_KTRACE_RELE, file, line, ra);
}
#endif	/* CONFIG_XFS_VNODE_TRACING */
