/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)vnode.h	8.7 (Berkeley) 2/4/94
 * $Id: vnode.h,v 1.54 1997/10/26 20:26:14 phk Exp $
 */

#ifndef _SYS_VNODE_H_
#define	_SYS_VNODE_H_

#include <sys/lock.h>
#include <sys/queue.h>

#ifdef SMP
#include <machine/smp.h>
#endif

/*
 * The vnode is the focus of all file activity in UNIX.  There is a
 * unique vnode allocated for each active file, each current directory,
 * each mounted-on file, text file, and the root.
 */

/*
 * Vnode types.  VNON means no type.
 */
enum vtype	{ VNON, VREG, VDIR, VBLK, VCHR, VLNK, VSOCK, VFIFO, VBAD };

/*
 * Vnode tag types.
 * These are for the benefit of external programs only (e.g., pstat)
 * and should NEVER be inspected by the kernel.
 */
enum vtagtype	{
	VT_NON, VT_UFS, VT_NFS, VT_MFS, VT_PC, VT_LFS, VT_LOFS, VT_FDESC,
	VT_PORTAL, VT_NULL, VT_UMAP, VT_KERNFS, VT_PROCFS, VT_AFS, VT_ISOFS,
	VT_UNION, VT_MSDOSFS, VT_DEVFS, VT_TFS
};

/*
 * Each underlying filesystem allocates its own private area and hangs
 * it from v_data.  If non-null, this area is freed in getnewvnode().
 */
LIST_HEAD(buflists, buf);

typedef	int 	vop_t __P((void *));
struct vm_object;
struct namecache;

/*
 * Reading or writing any of these items requires holding the appropriate lock.
 * v_freelist is locked by the global vnode_free_list simple lock.
 * v_mntvnodes is locked by the global mntvnodes simple lock.
 * v_flag, v_usecount, v_holdcount and v_writecount are
 *    locked by the v_interlock simple lock.
 */
struct vnode {
	u_long	v_flag;				/* vnode flags (see below) */
	int	v_usecount;			/* reference count of users */
	int	v_writecount;			/* reference count of writers */
	int	v_holdcnt;			/* page & buffer references */
	daddr_t	v_lastr;			/* last read (read-ahead) */
	u_long	v_id;				/* capability identifier */
	struct	mount *v_mount;			/* ptr to vfs we are in */
	vop_t	**v_op;				/* vnode operations vector */
	TAILQ_ENTRY(vnode) v_freelist;		/* vnode freelist */
	LIST_ENTRY(vnode) v_mntvnodes;		/* vnodes for mount point */
	struct	buflists v_cleanblkhd;		/* clean blocklist head */
	struct	buflists v_dirtyblkhd;		/* dirty blocklist head */
	long	v_numoutput;			/* num of writes in progress */
	enum	vtype v_type;			/* vnode type */
	union {
		struct mount	*vu_mountedhere;/* ptr to mounted vfs (VDIR) */
		struct socket	*vu_socket;	/* unix ipc (VSOCK) */
		struct specinfo	*vu_specinfo;	/* device (VCHR, VBLK) */
		struct fifoinfo	*vu_fifoinfo;	/* fifo (VFIFO) */
	} v_un;
	struct	nqlease *v_lease;		/* Soft reference to lease */
	daddr_t	v_lastw;			/* last write (write cluster) */
	daddr_t	v_cstart;			/* start block of cluster */
	daddr_t	v_lasta;			/* last allocation */
	int	v_clen;				/* length of current cluster */
	struct vm_object *v_object;		/* Place to store VM object */
	struct	simplelock v_interlock;		/* lock on usecount and flag */
	struct	lock *v_vnlock;			/* used for non-locking fs's */
	enum	vtagtype v_tag;			/* type of underlying data */
	void 	*v_data;			/* private data for fs */
	LIST_HEAD(, namecache) v_cache_src;	/* Cache entries from us */
	TAILQ_HEAD(, namecache) v_cache_dst;	/* Cache entries to us */
	struct	vnode *v_dd;			/* .. vnode */
	u_long	v_ddid;				/* .. capability identifier */
};
#define	v_mountedhere	v_un.vu_mountedhere
#define	v_socket	v_un.vu_socket
#define	v_specinfo	v_un.vu_specinfo
#define	v_fifoinfo	v_un.vu_fifoinfo

/*
 * Vnode flags.
 */
#define	VROOT		0x00001	/* root of its file system */
#define	VTEXT		0x00002	/* vnode is a pure text prototype */
#define	VSYSTEM		0x00004	/* vnode being used by kernel */
#define	VISTTY		0x00008	/* vnode represents a tty */
#define	VXLOCK		0x00100	/* vnode is locked to change underlying type */
#define	VXWANT		0x00200	/* process is waiting for vnode */
#define	VBWAIT		0x00400	/* waiting for output to complete */
#define	VALIASED	0x00800	/* vnode has an alias */
#define	VDIROP		0x01000	/* LFS: vnode is involved in a directory op */
#define	VVMIO		0x02000	/* VMIO flag */
#define	VNINACT		0x04000	/* LFS: skip ufs_inactive() in lfs_vunref */
#define	VAGE		0x08000	/* Insert vnode at head of free list */
#define	VOLOCK		0x10000	/* vnode is locked waiting for an object */
#define	VOWANT		0x20000	/* a process is waiting for VOLOCK */
#define	VDOOMED		0x40000	/* This vnode is being recycled */
#define	VFREE		0x80000	/* This vnode is on the freelist */

/*
 * Vnode attributes.  A field value of VNOVAL represents a field whose value
 * is unavailable (getattr) or which is not to be changed (setattr).
 */
struct vattr {
	enum vtype	va_type;	/* vnode type (for create) */
	u_short		va_mode;	/* files access mode and type */
	short		va_nlink;	/* number of references to file */
	uid_t		va_uid;		/* owner user id */
	gid_t		va_gid;		/* owner group id */
	long		va_fsid;	/* file system id (dev for now) */
	long		va_fileid;	/* file id */
	u_quad_t	va_size;	/* file size in bytes */
	long		va_blocksize;	/* blocksize preferred for i/o */
	struct timespec	va_atime;	/* time of last access */
	struct timespec	va_mtime;	/* time of last modification */
	struct timespec	va_ctime;	/* time file changed */
	u_long		va_gen;		/* generation number of file */
	u_long		va_flags;	/* flags defined for file */
	dev_t		va_rdev;	/* device the special file represents */
	u_quad_t	va_bytes;	/* bytes of disk space held by file */
	u_quad_t	va_filerev;	/* file modification number */
	u_int		va_vaflags;	/* operations flags, see below */
	long		va_spare;	/* remain quad aligned */
};

/*
 * Flags for va_vaflags.
 */
#define	VA_UTIMES_NULL	0x01		/* utimes argument was NULL */
#define VA_EXCLUSIVE	0x02		/* exclusive create request */

/*
 * Flags for ioflag.
 */
#define	IO_UNIT		0x01		/* do I/O as atomic unit */
#define	IO_APPEND	0x02		/* append write to end */
#define	IO_SYNC		0x04		/* do I/O synchronously */
#define	IO_NODELOCKED	0x08		/* underlying node already locked */
#define	IO_NDELAY	0x10		/* FNDELAY flag set in file table */
#define	IO_VMIO		0x20		/* data already in VMIO space */

/*
 *  Modes.  Some values same as Ixxx entries from inode.h for now.
 */
#define	VSUID	04000		/* set user id on execution */
#define	VSGID	02000		/* set group id on execution */
#define	VSVTX	01000		/* save swapped text even after use */
#define	VREAD	00400		/* read, write, execute permissions */
#define	VWRITE	00200
#define	VEXEC	00100

/*
 * Token indicating no attribute value yet assigned.
 */
#define	VNOVAL	(-1)

#ifdef KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_VNODE);
#endif

/*
 * Convert between vnode types and inode formats (since POSIX.1
 * defines mode word of stat structure in terms of inode formats).
 */
extern enum vtype	iftovt_tab[];
extern int		vttoif_tab[];
#define IFTOVT(mode)	(iftovt_tab[((mode) & S_IFMT) >> 12])
#define VTTOIF(indx)	(vttoif_tab[(int)(indx)])
#define MAKEIMODE(indx, mode)	(int)(VTTOIF(indx) | (mode))

/*
 * Flags to various vnode functions.
 */
#define	SKIPSYSTEM	0x0001		/* vflush: skip vnodes marked VSYSTEM */
#define	FORCECLOSE	0x0002		/* vflush: force file closure */
#define	WRITECLOSE	0x0004		/* vflush: only close writable files */
#define	DOCLOSE		0x0008		/* vclean: close active files */
#define	V_SAVE		0x0001		/* vinvalbuf: sync file first */
#define	V_SAVEMETA	0x0002		/* vinvalbuf: leave indirect blocks */
#define	REVOKEALL	0x0001		/* vop_revoke: revoke all aliases */

#define	VREF(vp)	vref(vp)

#ifdef DIAGNOSTIC
#define	VATTR_NULL(vap)	vattr_null(vap)
#else
#define	VATTR_NULL(vap)	(*(vap) = va_null)	/* initialize a vattr */
#endif /* DIAGNOSTIC */

#define	NULLVP	((struct vnode *)NULL)

#ifdef VFS_LKM
#define	VNODEOP_SET(f) DATA_SET(MODVNOPS,f)
#else
#define	VNODEOP_SET(f) DATA_SET(vfs_opv_descs_,f)
#endif

/*
 * Global vnode data.
 */
extern	struct vnode *rootvnode;	/* root (i.e. "/") vnode */
extern	int desiredvnodes;		/* number of vnodes desired */
extern	int prtactive;			/* nonzero to call vprint() */
extern	struct vattr va_null;		/* predefined null vattr structure */

/*
 * Macro/function to check for client cache inconsistency w.r.t. leasing.
 */
#define	LEASE_READ	0x1		/* Check lease for readers */
#define	LEASE_WRITE	0x2		/* Check lease for modifiers */


extern void	(*lease_updatetime) __P((int deltat));

#define VSHOULDFREE(vp)	\
	(!((vp)->v_flag & (VFREE|VDOOMED)) && \
	 !(vp)->v_holdcnt && !(vp)->v_usecount)

#define VSHOULDBUSY(vp)	\
	(((vp)->v_flag & VFREE) && \
	 ((vp)->v_holdcnt || (vp)->v_usecount))


#endif /* KERNEL */


/*
 * Mods for extensibility.
 */

/*
 * Flags for vdesc_flags:
 */
#define VDESC_MAX_VPS		16
/* Low order 16 flag bits are reserved for willrele flags for vp arguments. */
#define VDESC_VP0_WILLRELE	0x0001
#define VDESC_VP1_WILLRELE	0x0002
#define VDESC_VP2_WILLRELE	0x0004
#define VDESC_VP3_WILLRELE	0x0008
#define VDESC_NOMAP_VPP		0x0100
#define VDESC_VPP_WILLRELE	0x0200

/*
 * VDESC_NO_OFFSET is used to identify the end of the offset list
 * and in places where no such field exists.
 */
#define VDESC_NO_OFFSET -1

/*
 * This structure describes the vnode operation taking place.
 */
struct vnodeop_desc {
	int	vdesc_offset;		/* offset in vector--first for speed */
	char    *vdesc_name;		/* a readable name for debugging */
	int	vdesc_flags;		/* VDESC_* flags */

	/*
	 * These ops are used by bypass routines to map and locate arguments.
	 * Creds and procs are not needed in bypass routines, but sometimes
	 * they are useful to (for example) transport layers.
	 * Nameidata is useful because it has a cred in it.
	 */
	int	*vdesc_vp_offsets;	/* list ended by VDESC_NO_OFFSET */
	int	vdesc_vpp_offset;	/* return vpp location */
	int	vdesc_cred_offset;	/* cred location, if any */
	int	vdesc_proc_offset;	/* proc location, if any */
	int	vdesc_componentname_offset; /* if any */
	/*
	 * Finally, we've got a list of private data (about each operation)
	 * for each transport layer.  (Support to manage this list is not
	 * yet part of BSD.)
	 */
	caddr_t	*vdesc_transports;
};

#ifdef KERNEL
/*
 * A list of all the operation descs.
 */
extern struct vnodeop_desc *vnodeop_descs[];

/*
 * Interlock for scanning list of vnodes attached to a mountpoint
 */
extern struct simplelock mntvnode_slock;

/*
 * This macro is very helpful in defining those offsets in the vdesc struct.
 *
 * This is stolen from X11R4.  I ignored all the fancy stuff for
 * Crays, so if you decide to port this to such a serious machine,
 * you might want to consult Intrinsic.h's XtOffset{,Of,To}.
 */
#define VOPARG_OFFSET(p_type,field) \
        ((int) (((char *) (&(((p_type)NULL)->field))) - ((char *) NULL)))
#define VOPARG_OFFSETOF(s_type,field) \
	VOPARG_OFFSET(s_type*,field)
#define VOPARG_OFFSETTO(S_TYPE,S_OFFSET,STRUCT_P) \
	((S_TYPE)(((char*)(STRUCT_P))+(S_OFFSET)))


/*
 * This structure is used to configure the new vnodeops vector.
 */
struct vnodeopv_entry_desc {
	struct vnodeop_desc *opve_op;   /* which operation this is */
	vop_t *opve_impl;		/* code implementing this operation */
};
struct vnodeopv_desc {
			/* ptr to the ptr to the vector where op should go */
	vop_t ***opv_desc_vector_p;
	struct vnodeopv_entry_desc *opv_desc_ops;   /* null terminated list */
};

/*
 * A generic structure.
 * This can be used by bypass routines to identify generic arguments.
 */
struct vop_generic_args {
	struct vnodeop_desc *a_desc;
	/* other random data follows, presumably */
};


#ifdef DEBUG_VFS_LOCKS
/*
 * Macros to aid in tracing VFS locking problems.  Not totally
 * reliable since if the process sleeps between changing the lock
 * state and checking it with the assert, some other process could
 * change the state.  They are good enough for debugging a single
 * filesystem using a single-threaded test.  I find that 'cvs co src'
 * is a pretty good test.
 */

/*
 * [dfr] Kludge until I get around to fixing all the vfs locking.
 */
#define IS_LOCKING_VFS(vp)	((vp)->v_tag == VT_UFS		\
				 || (vp)->v_tag == VT_MFS	\
				 || (vp)->v_tag == VT_NFS	\
				 || (vp)->v_tag == VT_LFS	\
				 || (vp)->v_tag == VT_ISOFS	\
				 || (vp)->v_tag == VT_MSDOSFS	\
				 || (vp)->v_tag == VT_DEVFS)

#define ASSERT_VOP_LOCKED(vp, str)				\
    if ((vp) && IS_LOCKING_VFS(vp) && !VOP_ISLOCKED(vp)) {	\
	panic("%s: %x is not locked but should be", str, vp);	\
    }

#define ASSERT_VOP_UNLOCKED(vp, str)				\
    if ((vp) && IS_LOCKING_VFS(vp) && VOP_ISLOCKED(vp)) {	\
	panic("%s: %x is locked but shouldn't be", str, vp);	\
    }

#else

#define ASSERT_VOP_LOCKED(vp, str)
#define ASSERT_VOP_UNLOCKED(vp, str)

#endif

/*
 * VOCALL calls an op given an ops vector.  We break it out because BSD's
 * vclean changes the ops vector and then wants to call ops with the old
 * vector.
 */
#define VOCALL(OPSV,OFF,AP) (( *((OPSV)[(OFF)])) (AP))

/*
 * This call works for vnodes in the kernel.
 */
#define VCALL(VP,OFF,AP) VOCALL((VP)->v_op,(OFF),(AP))
#define VDESC(OP) (& __CONCAT(OP,_desc))
#define VOFFSET(OP) (VDESC(OP)->vdesc_offset)

/*
 * Finally, include the default set of vnode operations.
 */
#include "vnode_if.h"

/*
 * Public vnode manipulation functions.
 */
struct componentname;
struct file;
struct mount;
struct nameidata;
struct ostat;
struct proc;
struct stat;
struct ucred;
struct uio;
struct vattr;
struct vnode;
struct vop_bwrite_args;
struct vm_zone;

extern struct vm_zone *namei_zone;

extern int	(*lease_check_hook) __P((struct vop_lease_args *));

int 	bdevvp __P((dev_t dev, struct vnode **vpp));
/* cache_* may belong in namei.h. */
void	cache_enter __P((struct vnode *dvp, struct vnode *vp,
	    struct componentname *cnp));
int	cache_lookup __P((struct vnode *dvp, struct vnode **vpp,
	    struct componentname *cnp));
void	cache_purge __P((struct vnode *vp));
void	cache_purgevfs __P((struct mount *mp));
void	cvtstat __P((struct stat *st, struct ostat *ost));
int 	getnewvnode __P((enum vtagtype tag,
	    struct mount *mp, vop_t **vops, struct vnode **vpp));
void	insmntque __P((struct vnode *vp, struct mount *mp));
int	lease_check __P((struct vop_lease_args *ap));
void 	vattr_null __P((struct vattr *vap));
void	vbusy __P((struct vnode *));
int 	vcount __P((struct vnode *vp));
void	vdrop __P((struct vnode *));
int	vfinddev __P((dev_t dev, enum vtype type, struct vnode **vpp));
void	vfree __P((struct vnode *));
void	vfs_opv_init __P((struct vnodeopv_desc **them));
int	vflush __P((struct mount *mp, struct vnode *skipvp, int flags));
int 	vget __P((struct vnode *vp, int lockflag, struct proc *p));
void 	vgone __P((struct vnode *vp));
void	vhold __P((struct vnode *));
int	vinvalbuf __P((struct vnode *vp, int save, struct ucred *cred,
	    struct proc *p, int slpflag, int slptimeo));
void	vprint __P((char *label, struct vnode *vp));
int	vrecycle __P((struct vnode *vp, struct simplelock *inter_lkp,
	    struct proc *p));
int 	vn_close __P((struct vnode *vp,
	    int flags, struct ucred *cred, struct proc *p));
int	vn_lock __P((struct vnode *vp, int flags, struct proc *p));
int 	vn_open __P((struct nameidata *ndp, int fmode, int cmode));
int 	vn_rdwr __P((enum uio_rw rw, struct vnode *vp, caddr_t base,
	    int len, off_t offset, enum uio_seg segflg, int ioflg,
	    struct ucred *cred, int *aresid, struct proc *p));
int	vn_stat __P((struct vnode *vp, struct stat *sb, struct proc *p));
int	vfs_cache_lookup __P((struct vop_lookup_args *ap));
int	vfs_object_create __P((struct vnode *vp, struct proc *p,
                struct ucred *cred, int waslocked));
int 	vn_writechk __P((struct vnode *vp));
int	vop_stdbwrite __P((struct vop_bwrite_args *ap));
int	vop_stdislocked __P((struct vop_islocked_args *));
int	vop_stdlock __P((struct vop_lock_args *));
int	vop_stdunlock __P((struct vop_unlock_args *));
int	vop_noislocked __P((struct vop_islocked_args *));
int	vop_nolock __P((struct vop_lock_args *));
int	vop_nopoll __P((struct vop_poll_args *));
int	vop_nounlock __P((struct vop_unlock_args *));
int	vop_stdpathconf __P((struct vop_pathconf_args *));
int	vop_revoke __P((struct vop_revoke_args *));
int	vop_sharedlock __P((struct vop_lock_args *));
int	vop_eopnotsupp __P((struct vop_generic_args *ap));
int	vop_ebadf __P((struct vop_generic_args *ap));
int	vop_einval __P((struct vop_generic_args *ap));
int	vop_enotty __P((struct vop_generic_args *ap));
int	vop_defaultop __P((struct vop_generic_args *ap));
int	vop_null __P((struct vop_generic_args *ap));

struct vnode *
	checkalias __P((struct vnode *vp, dev_t nvp_rdev, struct mount *mp));
void 	vput __P((struct vnode *vp));
void 	vref __P((struct vnode *vp));
void 	vrele __P((struct vnode *vp));

extern	vop_t	**default_vnodeop_p;
#endif /* KERNEL */

#endif /* !_SYS_VNODE_H_ */
