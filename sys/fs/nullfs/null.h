/*
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)null.h	8.2 (Berkeley) 1/21/94
 *
 * $Id: null.h,v 1.1.1.1 1994/05/24 10:05:04 rgrimes Exp $
 */

struct null_args {
	char		*target;	/* Target of loopback  */
};

struct null_mount {
	struct mount	*nullm_vfs;
	struct vnode	*nullm_rootvp;	/* Reference to root null_node */
};

#ifdef KERNEL
/*
 * A cache of vnode references
 */
struct null_node {
	struct null_node	*null_forw;	/* Hash chain */
	struct null_node	*null_back;
	struct vnode	        *null_lowervp;	/* VREFed once */
	struct vnode		*null_vnode;	/* Back pointer */
};

extern int null_node_create __P((struct mount *mp, struct vnode *target, struct vnode **vpp));

#define	MOUNTTONULLMOUNT(mp) ((struct null_mount *)((mp)->mnt_data))
#define	VTONULL(vp) ((struct null_node *)(vp)->v_data)
#define	NULLTOV(xp) ((xp)->null_vnode)
#ifdef NULLFS_DIAGNOSTIC
extern struct vnode *null_checkvp __P((struct vnode *vp, char *fil, int lno));
#define	NULLVPTOLOWERVP(vp) null_checkvp((vp), __FILE__, __LINE__)
#else
#define	NULLVPTOLOWERVP(vp) (VTONULL(vp)->null_lowervp)
#endif

extern vop_t **null_vnodeop_p;
extern struct vfsops null_vfsops;
#endif /* KERNEL */
