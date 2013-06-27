/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)vpsfs.h	8.3 (Berkeley) 8/20/94
 *
 * $FreeBSD: head/sys/fs/vpsfsfs/vpsfs.h 250505 2013-05-11 11:17:44Z kib $
 */

#ifndef	FS_VPSFS_H
#define	FS_VPSFS_H

/* based on fs/nullfs r250505 */

/*
#define VPSFS_DEBUG
*/

#define	VPSFSM_CACHE	0x0001

struct vpsfs_limits;

struct vpsfs_mount {
	struct mount	*vpsfsm_vfs;
	struct vnode	*vpsfsm_rootvp;	/* Reference to root vpsfs_node */
	uint64_t	vpsfsm_flags;
	struct vpsfs_limits *vpsfsm_limits;
	int	     limits_last_sync;
	struct task	limits_sync_task;
	char		limits_sync_task_enqueued;
	struct mtx	vpsfs_mtx;
};

#ifdef _KERNEL
/*
 * A cache of vnode references
 */
struct vpsfs_node {
	LIST_ENTRY(vpsfs_node)	vpsfs_hash;	/* Hash list */
	struct vnode		*vpsfs_lowervp;	/* VREFed once */
	struct vnode		*vpsfs_vnode;	/* Back pointer */
	u_int			vpsfs_flags;
};

struct vpsfs_limits {
	size_t space_used;
	size_t nodes_used;
	size_t space_soft;
	size_t nodes_soft;
	size_t space_hard;
	size_t nodes_hard;
};	  

#define	VPSFSV_NOUNLOCK		0x0001
#define	VPSFSV_DROP		0x0002
#define VPSFSV_FORBIDDEN	0x0003

#define	MOUNTTOVPSFSMOUNT(mp) ((struct vpsfs_mount *)((mp)->mnt_data))
#define	VTOVPSFS(vp) ((struct vpsfs_node *)(vp)->v_data)
#define	VPSFSTOV(xp) ((xp)->vpsfs_vnode)

int vpsfs_init(struct vfsconf *vfsp);
int vpsfs_uninit(struct vfsconf *vfsp);
int vpsfs_nodeget(struct mount *mp, struct vnode *target,
    struct vnode **vpp);
struct vnode *vpsfs_hashget(struct mount *mp, struct vnode *lowervp);
void vpsfs_hashrem(struct vpsfs_node *xp);
int vpsfs_bypass(struct vop_generic_args *ap);

int vpsfs_calcusage(struct vpsfs_mount *, struct vpsfs_limits *);
int vpsfs_calcusage_path(const char *, struct vpsfs_limits *);
int vpsfs_mount_is_vpsfs(struct mount *mp);
int vpsfs_read_usage(struct vpsfs_mount *mount,
    struct vpsfs_limits *limits);
int vpsfs_write_usage(struct vpsfs_mount *mount,
    struct vpsfs_limits *limits);
int vpsfs_limit_alloc(struct vpsfs_mount *mount, size_t space,
    size_t nodes);
int vpsfs_limit_free(struct vpsfs_mount *mount, size_t space,
    size_t nodes);

#ifdef DIAGNOSTIC
struct vnode *vpsfs_checkvp(struct vnode *vp, char *fil, int lno);
#define	VPSFSVPTOLOWERVP(vp) vpsfs_checkvp((vp), __FILE__, __LINE__)
#else
#define	VPSFSVPTOLOWERVP(vp) (VTOVPSFS(vp)->vpsfs_lowervp)
#endif

extern struct vop_vector vpsfs_vnodeops;

/*
extern int (*vpsfs_calcusage_path_p)(const char *, struct vpsfs_limits *);
*/

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_VPSFSNODE);
#endif

#ifdef VPSFS_DEBUG
#define VPSFSDEBUG(format, args...) printf(format ,## args)
#else
#define VPSFSDEBUG(format, args...)
#endif /* VPSFS_DEBUG */

#endif /* _KERNEL */

#endif
