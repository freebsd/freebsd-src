/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * the UCLA Ficus project.
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
 *	@(#)umap.h	8.4 (Berkeley) 8/20/94
 *
 * $FreeBSD$
 */

#define MAPFILEENTRIES 64
#define GMAPFILEENTRIES 16
#define NOBODY 32767
#define NULLGROUP 65534

struct umap_args {
	char		*target;	/* Target of loopback  */
	int 		nentries;       /* # of entries in user map array */
	int 		gnentries;	/* # of entries in group map array */
	u_long 		(*mapdata)[2];	/* pointer to array of user mappings */
	u_long 		(*gmapdata)[2];	/* pointer to array of group mappings */
};

struct umap_mount {
	struct mount	*umapm_vfs;
	struct vnode	*umapm_rootvp;	/* Reference to root umap_node */
	int             info_nentries;  /* number of uid mappings */
	int		info_gnentries;	/* number of gid mappings */
	u_long		info_mapdata[MAPFILEENTRIES][2]; /* mapping data for
	    user mapping in ficus */
	u_long		info_gmapdata[GMAPFILEENTRIES][2]; /*mapping data for
	    group mapping in ficus */
};

#ifdef _KERNEL
/*
 * A cache of vnode references
 */
struct umap_node {
	LIST_ENTRY(umap_node) umap_hash;	/* Hash list */
	struct vnode	*umap_lowervp;	/* Aliased vnode - VREFed once */
	struct vnode	*umap_vnode;	/* Back pointer to vnode/umap_node */
};

extern int umapfs_init(struct vfsconf *vfsp);
extern int umap_node_create(struct mount *mp, struct vnode *target, struct vnode **vpp);
extern u_long umap_reverse_findid(u_long id, u_long map[][2], int nentries);
extern void umap_mapids(struct mount *v_mount, struct ucred *credp);

#define	MOUNTTOUMAPMOUNT(mp) ((struct umap_mount *)((mp)->mnt_data))
#define	VTOUMAP(vp) ((struct umap_node *)(vp)->v_data)
#define UMAPTOV(xp) ((xp)->umap_vnode)
#ifdef DIAGNOSTIC
extern struct vnode *umap_checkvp(struct vnode *vp, char *fil, int lno);
#define	UMAPVPTOLOWERVP(vp) umap_checkvp((vp), __FILE__, __LINE__)
#else
#define	UMAPVPTOLOWERVP(vp) (VTOUMAP(vp)->umap_lowervp)
#endif

#endif /* _KERNEL */
