/*
 * Copyright (c) 1999, 2000 Boris Popov
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * $FreeBSD$
 */
#ifndef _NWFS_NODE_H_
#define _NWFS_NODE_H_

#define	NWFS_ROOT_INO	0x7ffffffd
#define	NWFS_ROOTVOL	"#.ROOT"

/* Bits for nwnode.n_flag */
#define	NFLUSHINPROG	0x0001
#define	NFLUSHWANT	0x0002		/* they should gone ... */
#define	NMODIFIED	0x0004		/* bogus, until async IO implemented */
#define	NNEW		0x0008		/* vnode has been allocated */
#define	NVOLUME		0x0010		/* vnode references a volume */
#define	NSHOULDFREE	0x0020		/* vnode should be removed from hash */

struct nwnode {
	LIST_ENTRY(nwnode)	n_hash;
	struct vnode 		*n_vnode;
	struct vattr		n_vattr;
	struct nwmount		*n_mount;
	time_t			n_atime;	/* attributes cache time*/
	time_t			n_ctime;
	time_t			n_mtime;
	int			n_flag;
	ncpfid			n_parent;
	ncpfid			n_fid;
	int			n_refparent;
	u_long			n_attr;		/* LH */
	u_long			n_size;
	u_long			n_dosfid;
	int 			opened;
/*	int 			access;*/
	u_long 			n_origfh;
	ncp_fh			n_fh;
	struct nw_search_seq	n_seq;
	u_char			n_nmlen;
	u_char			n_name[256];
};

#define VTONW(vp)	((struct nwnode *)(vp)->v_data)
#define NWTOV(np)	((struct vnode *)(np)->n_vnode)
#define	NWCMPF(f1,f2)	((f1)->f_parent == (f2)->f_parent && \
			 (f1)->f_id == (f2)->f_id)
#define	NWCMPN(np1,np2)	NWCMPF(&(np1)->n_fid, &(np2)->n_fid)
#define NWCMPV(vp1,vp2)	NWCMPN(VTONW(vp1),VTONW(vp2))

struct vop_getpages_args;
struct vop_inactive_args;
struct vop_putpages_args;
struct vop_reclaim_args;
struct ucred;
struct uio;

void nwfs_hash_init(void);
void nwfs_hash_free(void);
int  nwfs_allocvp(struct mount *mp, ncpfid fid, struct vnode **vpp);
int  nwfs_lookupnp(struct nwmount *nmp, ncpfid fid, struct proc *p,
	struct nwnode **npp);
int  nwfs_inactive(struct vop_inactive_args *);
int  nwfs_reclaim(struct vop_reclaim_args *);
int nwfs_nget(struct mount *mp, ncpfid fid, struct nw_entry_info *fap,
    struct vnode *dvp, struct vnode **vpp);

int  nwfs_getpages(struct vop_getpages_args *);
int  nwfs_putpages(struct vop_putpages_args *);
int  nwfs_readvnode(struct vnode *vp, struct uio *uiop, struct ucred *cred);
int  nwfs_writevnode(struct vnode *vp, struct uio *uiop, struct ucred *cred, int ioflag);
void nwfs_attr_cacheenter(struct vnode *vp, struct nw_entry_info *fi);
int  nwfs_attr_cachelookup(struct vnode *vp,struct vattr *va);

#define nwfs_attr_cacheremove(vp)	VTONW(vp)->n_atime = 0

#endif /* _NWFS_NODE_H_ */
