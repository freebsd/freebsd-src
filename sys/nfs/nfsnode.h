/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	@(#)nfsnode.h	8.9 (Berkeley) 5/14/95
 */


#ifndef _NFS_NFSNODE_H_
#define _NFS_NFSNODE_H_

#ifndef _NFS_NFS_H_
#include <nfs/nfs.h>
#endif

/*
 * Silly rename structure that hangs off the nfsnode until the name
 * can be removed by nfs_inactive()
 */
struct sillyrename {
	struct	ucred *s_cred;
	struct	vnode *s_dvp;
	long	s_namlen;
	char	s_name[20];
};

/*
 * This structure is used to save the logical directory offset to
 * NFS cookie mappings.
 * The mappings are stored in a list headed
 * by n_cookies, as required.
 * There is one mapping for each NFS_DIRBLKSIZ bytes of directory information
 * stored in increasing logical offset byte order.
 */
#define NFSNUMCOOKIES		31

struct nfsdmap {
	LIST_ENTRY(nfsdmap)	ndm_list;
	int			ndm_eocookie;
	nfsuint64		ndm_cookies[NFSNUMCOOKIES];
};

/*
 * The nfsnode is the nfs equivalent to ufs's inode. Any similarity
 * is purely coincidental.
 * There is a unique nfsnode allocated for each active file,
 * each current directory, each mounted-on file, text file, and the root.
 * An nfsnode is 'named' by its file handle. (nget/nfs_node.c)
 * If this structure exceeds 256 bytes (it is currently 256 using 4.4BSD-Lite
 * type definitions), file handles of > 32 bytes should probably be split out
 * into a separate MALLOC()'d data structure. (Reduce the size of nfsfh_t by
 * changing the definition in sys/mount.h of NFS_SMALLFH.)
 * NB: Hopefully the current order of the fields is such that everything will
 *     be well aligned and, therefore, tightly packed.
 */
struct nfsnode {
	LIST_ENTRY(nfsnode)	n_hash;		/* Hash chain */
	CIRCLEQ_ENTRY(nfsnode)	n_timer;	/* Nqnfs timer chain */
	u_quad_t		n_size;		/* Current size of file */
	u_quad_t		n_brev;		/* Modify rev when cached */
	u_quad_t		n_lrev;		/* Modify rev for lease */
	struct vattr		n_vattr;	/* Vnode attribute cache */
	time_t			n_attrstamp;	/* Attr. cache timestamp */
	time_t			n_mtime;	/* Prev modify time. */
	time_t			n_ctime;	/* Prev create time. */
	time_t			n_expiry;	/* Lease expiry time */
	nfsfh_t			*n_fhp;		/* NFS File Handle */
	struct vnode		*n_vnode;	/* associated vnode */
	struct lockf		*n_lockf;	/* Locking record of file */
	int			n_error;	/* Save write error value */
	union {
		struct timespec	nf_atim;	/* Special file times */
		nfsuint64	nd_cookieverf;	/* Cookie verifier (dir only) */
	} n_un1;
	union {
		struct timespec	nf_mtim;
		off_t		nd_direof;	/* Dir. EOF offset cache */
	} n_un2;
	union {
		struct sillyrename *nf_silly;	/* Ptr to silly rename struct */
		LIST_HEAD(, nfsdmap) nd_cook;	/* cookies */
	} n_un3;
	short			n_fhsize;	/* size in bytes, of fh */
	short			n_flag;		/* Flag for locking.. */
	nfsfh_t			n_fh;		/* Small File Handle */
};

#define n_atim		n_un1.nf_atim
#define n_mtim		n_un2.nf_mtim
#define n_sillyrename	n_un3.nf_silly
#define n_cookieverf	n_un1.nd_cookieverf
#define n_direofoffset	n_un2.nd_direof
#define n_cookies	n_un3.nd_cook

/*
 * Flags for n_flag
 */
#define	NFLUSHWANT	0x0001	/* Want wakeup from a flush in prog. */
#define	NFLUSHINPROG	0x0002	/* Avoid multiple calls to vinvalbuf() */
#define	NMODIFIED	0x0004	/* Might have a modified buffer in bio */
#define	NWRITEERR	0x0008	/* Flag write errors so close will know */
#define	NQNFSNONCACHE	0x0020	/* Non-cachable lease */
#define	NQNFSWRITE	0x0040	/* Write lease */
#define	NQNFSEVICTED	0x0080	/* Has been evicted */
#define	NACC		0x0100	/* Special file accessed */
#define	NUPD		0x0200	/* Special file updated */
#define	NCHG		0x0400	/* Special file times changed */

/*
 * Convert between nfsnode pointers and vnode pointers
 */
#define VTONFS(vp)	((struct nfsnode *)(vp)->v_data)
#define NFSTOV(np)	((struct vnode *)(np)->n_vnode)

/*
 * Queue head for nfsiod's
 */
TAILQ_HEAD(, buf) nfs_bufq;

#ifdef KERNEL
/*
 * Prototypes for NFS vnode operations
 */
int	nfs_lookup __P((struct vop_lookup_args *));
int	nfs_create __P((struct vop_create_args *));
int	nfs_mknod __P((struct vop_mknod_args *));
int	nfs_open __P((struct vop_open_args *));
int	nfs_close __P((struct vop_close_args *));
int	nfsspec_close __P((struct vop_close_args *));
int	nfsfifo_close __P((struct vop_close_args *));
int	nfs_access __P((struct vop_access_args *));
int	nfsspec_access __P((struct vop_access_args *));
int	nfs_getattr __P((struct vop_getattr_args *));
int	nfs_setattr __P((struct vop_setattr_args *));
int	nfs_read __P((struct vop_read_args *));
int	nfs_write __P((struct vop_write_args *));
#define	nfs_lease_check ((int (*) __P((struct  vop_lease_args *)))nullop)
#define nqnfs_vop_lease_check	lease_check
int	nfsspec_read __P((struct vop_read_args *));
int	nfsspec_write __P((struct vop_write_args *));
int	nfsfifo_read __P((struct vop_read_args *));
int	nfsfifo_write __P((struct vop_write_args *));
#define nfs_ioctl ((int (*) __P((struct  vop_ioctl_args *)))enoioctl)
#define nfs_select ((int (*) __P((struct  vop_select_args *)))seltrue)
#define nfs_revoke vop_revoke
int	nfs_mmap __P((struct vop_mmap_args *));
int	nfs_fsync __P((struct vop_fsync_args *));
#define nfs_seek ((int (*) __P((struct  vop_seek_args *)))nullop)
int	nfs_remove __P((struct vop_remove_args *));
int	nfs_link __P((struct vop_link_args *));
int	nfs_rename __P((struct vop_rename_args *));
int	nfs_mkdir __P((struct vop_mkdir_args *));
int	nfs_rmdir __P((struct vop_rmdir_args *));
int	nfs_symlink __P((struct vop_symlink_args *));
int	nfs_readdir __P((struct vop_readdir_args *));
int	nfs_readlink __P((struct vop_readlink_args *));
int	nfs_abortop __P((struct vop_abortop_args *));
int	nfs_inactive __P((struct vop_inactive_args *));
int	nfs_reclaim __P((struct vop_reclaim_args *));
#define nfs_lock ((int (*) __P((struct vop_lock_args *)))vop_nolock)
#define nfs_unlock ((int (*) __P((struct vop_unlock_args *)))vop_nounlock)
#define nfs_islocked ((int (*) __P((struct vop_islocked_args *)))vop_noislocked)
int	nfs_bmap __P((struct vop_bmap_args *));
int	nfs_strategy __P((struct vop_strategy_args *));
int	nfs_print __P((struct vop_print_args *));
int	nfs_pathconf __P((struct vop_pathconf_args *));
int	nfs_advlock __P((struct vop_advlock_args *));
int	nfs_blkatoff __P((struct vop_blkatoff_args *));
int	nfs_bwrite __P((struct vop_bwrite_args *));
int	nfs_vget __P((struct mount *, ino_t, struct vnode **));
int	nfs_valloc __P((struct vop_valloc_args *));
#define nfs_reallocblks \
	((int (*) __P((struct  vop_reallocblks_args *)))eopnotsupp)
int	nfs_vfree __P((struct vop_vfree_args *));
int	nfs_truncate __P((struct vop_truncate_args *));
int	nfs_update __P((struct vop_update_args *));

/* other stuff */
int	nfs_removeit __P((struct sillyrename *));
int	nfs_nget __P((struct mount *,nfsfh_t *,int,struct nfsnode **));
int	nfs_lookitup __P((struct vnode *,char *,int,struct ucred *,struct proc *,struct nfsnode **));
int	nfs_sillyrename __P((struct vnode *,struct vnode *,struct componentname *));
nfsuint64 *nfs_getcookie __P((struct nfsnode *, off_t, int));
void nfs_invaldir __P((struct vnode *));
#define nqnfs_lease_updatetime	lease_updatetime

#endif /* KERNEL */

#endif
