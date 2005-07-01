/*-
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
 * $FreeBSD$
 */

#ifndef _SYS_VNODE_H_
#define	_SYS_VNODE_H_

/*
 * XXX - compatability until lockmgr() goes away or all the #includes are
 * updated.
 */
#include <sys/lockmgr.h>

#include <sys/bufobj.h>
#include <sys/queue.h>
#include <sys/_lock.h>
#include <sys/lock.h>
#include <sys/_mutex.h>
#include <sys/mutex.h>
#include <sys/selinfo.h>
#include <sys/uio.h>
#include <sys/acl.h>
#include <sys/ktr.h>

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
 * Each underlying filesystem allocates its own private area and hangs
 * it from v_data.  If non-null, this area is freed in getnewvnode().
 */

struct namecache;

struct vpollinfo {
	struct	mtx vpi_lock;		/* lock to protect below */
	struct	selinfo vpi_selinfo;	/* identity of poller(s) */
	short	vpi_events;		/* what they are looking for */
	short	vpi_revents;		/* what has happened */
};

/*
 * Reading or writing any of these items requires holding the appropriate lock.
 *
 * Lock reference:
 *	c - namecache mutex
 *	f - freelist mutex
 *	G - Giant
 *	i - interlock
 *	m - mntvnodes mutex
 *	p - pollinfo lock
 *	s - spechash mutex
 *	S - syncer mutex
 *	u - Only a reference to the vnode is needed to read.
 *	v - vnode lock
 *
 * Vnodes may be found on many lists.  The general way to deal with operating
 * on a vnode that is on a list is:
 *	1) Lock the list and find the vnode.
 *	2) Lock interlock so that the vnode does not go away.
 *	3) Unlock the list to avoid lock order reversals.
 *	4) vget with LK_INTERLOCK and check for ENOENT, or
 *	5) Check for DOOMED if the vnode lock is not required.
 *	6) Perform your operation, then vput().
 *
 * XXX Not all fields are locked yet and some fields that are marked are not
 * locked consistently.  This is a work in progress.  Requires Giant!
 */

#if defined(_KERNEL) || defined(_KVM_VNODE)

struct vnode {
	/*
	 * Fields which define the identity of the vnode.  These fields are
	 * owned by the filesystem (XXX: and vgone() ?)
	 */
	enum	vtype v_type;			/* u vnode type */
	const char *v_tag;			/* u type of underlying data */
	struct	vop_vector *v_op;		/* u vnode operations vector */
	void	*v_data;			/* u private data for fs */

	/*
	 * Filesystem instance stuff
	 */
	struct	mount *v_mount;			/* u ptr to vfs we are in */
	TAILQ_ENTRY(vnode) v_nmntvnodes;	/* m vnodes for mount point */

	/*
	 * Type specific fields, only one applies to any given vnode.
	 * See #defines below for renaming to v_* namespace.
	 */
	union {
		struct mount	*vu_mount;	/* v ptr to mountpoint (VDIR) */
		struct socket	*vu_socket;	/* v unix domain net (VSOCK) */
		struct cdev	*vu_cdev; 	/* v device (VCHR, VBLK) */
		struct fifoinfo	*vu_fifoinfo;	/* v fifo (VFIFO) */
	} v_un;

	/*
	 * vfs_hash:  (mount + inode) -> vnode hash.
	 */
	LIST_ENTRY(vnode)	v_hashlist;
	u_int			v_hash;

	/*
	 * VFS_namecache stuff
	 */
	LIST_HEAD(, namecache) v_cache_src;	/* c Cache entries from us */
	TAILQ_HEAD(, namecache) v_cache_dst;	/* c Cache entries to us */
	struct	vnode *v_dd;			/* c .. vnode */

	/*
	 * clustering stuff
	 */
	daddr_t	v_cstart;			/* v start block of cluster */
	daddr_t	v_lasta;			/* v last allocation  */
	daddr_t	v_lastw;			/* v last write  */
	int	v_clen;				/* v length of cur. cluster */

	/*
	 * Locking
	 */
	struct	lock v_lock;			/* u (if fs don't have one) */
	struct	mtx v_interlock;		/* lock for "i" things */
	struct	lock *v_vnlock;			/* u pointer to vnode lock */
#ifdef	DEBUG_LOCKS
	const char *filename;			/* Source file doing locking */
	int line;				/* Line number doing locking */
#endif
	int	v_holdcnt;			/* i prevents recycling. */
	int	v_usecount;			/* i ref count of users */
	u_long	v_iflag;			/* i vnode flags (see below) */
	u_long	v_vflag;			/* v vnode flags */
	int	v_writecount;			/* v ref count of writers */

	/*
	 * The machinery of being a vnode
	 */
	TAILQ_ENTRY(vnode) v_freelist;		/* f vnode freelist */
	struct bufobj	v_bufobj;		/* * Buffer cache object */

	/*
	 * Hooks for various subsystems and features.
	 */
	struct vpollinfo *v_pollinfo;		/* G Poll events, p for *v_pi */
	struct label *v_label;			/* MAC label for vnode */
};

#endif /* defined(_KERNEL) || defined(_KVM_VNODE) */

#define	v_mountedhere	v_un.vu_mount
#define	v_socket	v_un.vu_socket
#define	v_rdev		v_un.vu_cdev
#define	v_fifoinfo	v_un.vu_fifoinfo

/* XXX: These are temporary to avoid a source sweep at this time */
#define v_object	v_bufobj.bo_object

/*
 * Userland version of struct vnode, for sysctl.
 */
struct xvnode {
	size_t	xv_size;			/* sizeof(struct xvnode) */
	void	*xv_vnode;			/* address of real vnode */
	u_long	xv_flag;			/* vnode vflags */
	int	xv_usecount;			/* reference count of users */
	int	xv_writecount;			/* reference count of writers */
	int	xv_holdcnt;			/* page & buffer references */
	u_long	xv_id;				/* capability identifier */
	void	*xv_mount;			/* address of parent mount */
	long	xv_numoutput;			/* num of writes in progress */
	enum	vtype xv_type;			/* vnode type */
	union {
		void	*xvu_socket;		/* socket, if VSOCK */
		void	*xvu_fifo;		/* fifo, if VFIFO */
		dev_t	xvu_rdev;		/* maj/min, if VBLK/VCHR */
		struct {
			dev_t	xvu_dev;	/* device, if VDIR/VREG/VLNK */
			ino_t	xvu_ino;	/* id, if VDIR/VREG/VLNK */
		} xv_uns;
	} xv_un;
};
#define xv_socket	xv_un.xvu_socket
#define xv_fifo		xv_un.xvu_fifo
#define xv_rdev		xv_un.xvu_rdev
#define xv_dev		xv_un.xv_uns.xvu_dev
#define xv_ino		xv_un.xv_uns.xvu_ino

/* We don't need to lock the knlist */
#define	VN_KNLIST_EMPTY(vp) ((vp)->v_pollinfo == NULL ||	\
	    KNLIST_EMPTY(&(vp)->v_pollinfo->vpi_selinfo.si_note))

#define VN_KNOTE(vp, b, a)					\
	do {							\
		if (!VN_KNLIST_EMPTY(vp))			\
			KNOTE(&vp->v_pollinfo->vpi_selinfo.si_note, (b), (a)); \
	} while (0)
#define	VN_KNOTE_UNLOCKED(vp, b)	VN_KNOTE(vp, b, 0)
#define	VN_KNOTE_UNLOCKED(vp, b)	VN_KNOTE(vp, b, 0)

/*
 * Vnode flags.
 *	VI flags are protected by interlock and live in v_iflag
 *	VV flags are protected by the vnode lock and live in v_vflag
 */
#define	VI_MOUNT	0x0020	/* Mount in progress */
#define	VI_AGE		0x0040	/* Insert vnode at head of free list */
#define	VI_DOOMED	0x0080	/* This vnode is being recycled */
#define	VI_FREE		0x0100	/* This vnode is on the freelist */
#define	VI_OBJDIRTY	0x0400	/* object might be dirty */
#define	VI_DOINGINACT	0x0800	/* VOP_INACTIVE is in progress */
#define	VI_OWEINACT	0x1000	/* Need to call inactive */

#define	VV_ROOT		0x0001	/* root of its filesystem */
#define	VV_ISTTY	0x0002	/* vnode represents a tty */
#define	VV_NOSYNC	0x0004	/* unlinked, stop syncing */
#define	VV_CACHEDLABEL	0x0010	/* Vnode has valid cached MAC label */
#define	VV_TEXT		0x0020	/* vnode is a pure text prototype */
#define	VV_COPYONWRITE	0x0040	/* vnode is doing copy-on-write */
#define	VV_SYSTEM	0x0080	/* vnode being used by kernel */
#define	VV_PROCDEP	0x0100	/* vnode is process dependent */

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
	dev_t		va_fsid;	/* filesystem id */
	long		va_fileid;	/* file id */
	u_quad_t	va_size;	/* file size in bytes */
	long		va_blocksize;	/* blocksize preferred for i/o */
	struct timespec	va_atime;	/* time of last access */
	struct timespec	va_mtime;	/* time of last modification */
	struct timespec	va_ctime;	/* time file changed */
	struct timespec	va_birthtime;	/* time file created */
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
#define	VA_EXCLUSIVE	0x02		/* exclusive create request */
#define	VA_EXECVE_ATIME	0x04		/* setting atime for execve */

/*
 * Flags for ioflag. (high 16 bits used to ask for read-ahead and
 * help with write clustering)
 * NB: IO_NDELAY and IO_DIRECT are linked to fcntl.h
 */
#define	IO_UNIT		0x0001		/* do I/O as atomic unit */
#define	IO_APPEND	0x0002		/* append write to end */
#define	IO_NDELAY	0x0004		/* FNDELAY flag set in file table */
#define	IO_NODELOCKED	0x0008		/* underlying node already locked */
#define	IO_ASYNC	0x0010		/* bawrite rather then bdwrite */
#define	IO_VMIO		0x0020		/* data already in VMIO space */
#define	IO_INVAL	0x0040		/* invalidate after I/O */
#define	IO_SYNC		0x0080		/* do I/O synchronously */
#define	IO_DIRECT	0x0100		/* attempt to bypass buffer cache */
#define	IO_EXT		0x0400		/* operate on external attributes */
#define	IO_NORMAL	0x0800		/* operate on regular data */
#define	IO_NOMACCHECK	0x1000		/* MAC checks unnecessary */

#define IO_SEQMAX	0x7F		/* seq heuristic max value */
#define IO_SEQSHIFT	16		/* seq heuristic in upper 16 bits */

/*
 *  Modes.  Some values same as Ixxx entries from inode.h for now.
 */
#define	VEXEC	000100		/* execute/search permission */
#define	VWRITE	000200		/* write permission */
#define	VREAD	000400		/* read permission */
#define	VSVTX	001000		/* save swapped text even after use */
#define	VSGID	002000		/* set group id on execution */
#define	VSUID	004000		/* set user id on execution */
#define	VADMIN	010000		/* permission to administer */
#define	VSTAT	020000		/* permission to retrieve attrs */
#define	VAPPEND	040000		/* permission to write/append */
#define	VALLPERM	(VEXEC | VWRITE | VREAD | VADMIN | VSTAT | VAPPEND)

/*
 * Token indicating no attribute value yet assigned.
 */
#define	VNOVAL	(-1)

/*
 * LK_TIMELOCK timeout for vnode locks (used mainly by the pageout daemon)
 */
#define VLKTIMEOUT	(hz / 20 + 1)

#ifdef _KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_VNODE);
#endif

/*
 * Convert between vnode types and inode formats (since POSIX.1
 * defines mode word of stat structure in terms of inode formats).
 */
extern enum vtype	iftovt_tab[];
extern int		vttoif_tab[];
#define	IFTOVT(mode)	(iftovt_tab[((mode) & S_IFMT) >> 12])
#define	VTTOIF(indx)	(vttoif_tab[(int)(indx)])
#define	MAKEIMODE(indx, mode)	(int)(VTTOIF(indx) | (mode))

/*
 * Flags to various vnode functions.
 */
#define	SKIPSYSTEM	0x0001	/* vflush: skip vnodes marked VSYSTEM */
#define	FORCECLOSE	0x0002	/* vflush: force file closure */
#define	WRITECLOSE	0x0004	/* vflush: only close writable files */
#define	DOCLOSE		0x0008	/* vclean: close active files */
#define	V_SAVE		0x0001	/* vinvalbuf: sync file first */
#define	V_ALT		0x0002	/* vinvalbuf: invalidate only alternate bufs */
#define	V_NORMAL	0x0004	/* vinvalbuf: invalidate only regular bufs */
#define	REVOKEALL	0x0001	/* vop_revoke: revoke all aliases */
#define	V_WAIT		0x0001	/* vn_start_write: sleep for suspend */
#define	V_NOWAIT	0x0002	/* vn_start_write: don't sleep for suspend */
#define	V_XSLEEP	0x0004	/* vn_start_write: just return after sleep */

#define	VREF(vp)	vref(vp)

#ifdef DIAGNOSTIC
#define	VATTR_NULL(vap)	vattr_null(vap)
#else
#define	VATTR_NULL(vap)	(*(vap) = va_null)	/* initialize a vattr */
#endif /* DIAGNOSTIC */

#define	NULLVP	((struct vnode *)NULL)

/*
 * Global vnode data.
 */
extern	struct vnode *rootvnode;	/* root (i.e. "/") vnode */
extern	int async_io_version;		/* 0 or POSIX version of AIO i'face */
extern	int desiredvnodes;		/* number of vnodes desired */
extern	struct uma_zone *namei_zone;
extern	int prtactive;			/* nonzero to call vprint() */
extern	struct vattr va_null;		/* predefined null vattr structure */

/*
 * Macro/function to check for client cache inconsistency w.r.t. leasing.
 */
#define	LEASE_READ	0x1		/* Check lease for readers */
#define	LEASE_WRITE	0x2		/* Check lease for modifiers */

extern void	(*lease_updatetime)(int deltat);

#define	VI_LOCK(vp)	mtx_lock(&(vp)->v_interlock)
#define	VI_LOCK_FLAGS(vp, flags) mtx_lock_flags(&(vp)->v_interlock, (flags))
#define	VI_TRYLOCK(vp)	mtx_trylock(&(vp)->v_interlock)
#define	VI_UNLOCK(vp)	mtx_unlock(&(vp)->v_interlock)
#define	VI_MTX(vp)	(&(vp)->v_interlock)

#endif /* _KERNEL */

/*
 * Mods for extensibility.
 */

/*
 * Flags for vdesc_flags:
 */
#define	VDESC_MAX_VPS		16
/* Low order 16 flag bits are reserved for willrele flags for vp arguments. */
#define	VDESC_VP0_WILLRELE	0x0001
#define	VDESC_VP1_WILLRELE	0x0002
#define	VDESC_VP2_WILLRELE	0x0004
#define	VDESC_VP3_WILLRELE	0x0008
#define	VDESC_NOMAP_VPP		0x0100
#define	VDESC_VPP_WILLRELE	0x0200

/*
 * A generic structure.
 * This can be used by bypass routines to identify generic arguments.
 */
struct vop_generic_args {
	struct vnodeop_desc *a_desc;
	/* other random data follows, presumably */
};

typedef int vop_bypass_t(struct vop_generic_args *);

/*
 * VDESC_NO_OFFSET is used to identify the end of the offset list
 * and in places where no such field exists.
 */
#define VDESC_NO_OFFSET -1

/*
 * This structure describes the vnode operation taking place.
 */
struct vnodeop_desc {
	char	*vdesc_name;		/* a readable name for debugging */
	int	 vdesc_flags;		/* VDESC_* flags */
	vop_bypass_t	*vdesc_call;	/* Function to call */

	/*
	 * These ops are used by bypass routines to map and locate arguments.
	 * Creds and procs are not needed in bypass routines, but sometimes
	 * they are useful to (for example) transport layers.
	 * Nameidata is useful because it has a cred in it.
	 */
	int	*vdesc_vp_offsets;	/* list ended by VDESC_NO_OFFSET */
	int	vdesc_vpp_offset;	/* return vpp location */
	int	vdesc_cred_offset;	/* cred location, if any */
	int	vdesc_thread_offset;	/* thread location, if any */
	int	vdesc_componentname_offset; /* if any */
	/*
	 * Finally, we've got a list of private data (about each operation)
	 * for each transport layer.  (Support to manage this list is not
	 * yet part of BSD.)
	 */
	caddr_t	*vdesc_transports;
};

#ifdef _KERNEL
/*
 * A list of all the operation descs.
 */
extern struct vnodeop_desc *vnodeop_descs[];

#define	VOPARG_OFFSETOF(s_type, field)	__offsetof(s_type, field)
#define	VOPARG_OFFSETTO(s_type, s_offset, struct_p) \
    ((s_type)(((char*)(struct_p)) + (s_offset)))


#ifdef DEBUG_VFS_LOCKS
/*
 * Support code to aid in debugging VFS locking problems.  Not totally
 * reliable since if the thread sleeps between changing the lock
 * state and checking it with the assert, some other thread could
 * change the state.  They are good enough for debugging a single
 * filesystem using a single-threaded test.
 */
void	assert_vi_locked(struct vnode *vp, const char *str);
void	assert_vi_unlocked(struct vnode *vp, const char *str);
void	assert_vop_elocked(struct vnode *vp, const char *str);
#if 0
void	assert_vop_elocked_other(struct vnode *vp, const char *str);
#endif
void	assert_vop_locked(struct vnode *vp, const char *str);
#if 0
voi0	assert_vop_slocked(struct vnode *vp, const char *str);
#endif
void	assert_vop_unlocked(struct vnode *vp, const char *str);

#define	ASSERT_VI_LOCKED(vp, str)	assert_vi_locked((vp), (str))
#define	ASSERT_VI_UNLOCKED(vp, str)	assert_vi_unlocked((vp), (str))
#define	ASSERT_VOP_ELOCKED(vp, str)	assert_vop_elocked((vp), (str))
#if 0
#define	ASSERT_VOP_ELOCKED_OTHER(vp, str) assert_vop_locked_other((vp), (str))
#endif
#define	ASSERT_VOP_LOCKED(vp, str)	assert_vop_locked((vp), (str))
#if 0
#define	ASSERT_VOP_SLOCKED(vp, str)	assert_vop_slocked((vp), (str))
#endif
#define	ASSERT_VOP_UNLOCKED(vp, str)	assert_vop_unlocked((vp), (str))

#else /* !DEBUG_VFS_LOCKS */

#define	ASSERT_VI_LOCKED(vp, str)
#define	ASSERT_VI_UNLOCKED(vp, str)
#define	ASSERT_VOP_ELOCKED(vp, str)
#if 0
#define	ASSERT_VOP_ELOCKED_OTHER(vp, str)
#endif
#define	ASSERT_VOP_LOCKED(vp, str)
#if 0
#define	ASSERT_VOP_SLOCKED(vp, str)
#endif
#define	ASSERT_VOP_UNLOCKED(vp, str)
#endif /* DEBUG_VFS_LOCKS */


/*
 * This call works for vnodes in the kernel.
 */
#define VCALL(c) ((c)->a_desc->vdesc_call(c))

/*
 * VMIO support inline
 */

extern int vmiodirenable;

static __inline int
vn_canvmio(struct vnode *vp)
{
      if (vp && (vp->v_type == VREG || (vmiodirenable && vp->v_type == VDIR)))
		return(TRUE);
	return(FALSE);
}

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
struct thread;
struct proc;
struct stat;
struct nstat;
struct ucred;
struct uio;
struct vattr;
struct vnode;

extern int	(*lease_check_hook)(struct vop_lease_args *);
extern int	(*softdep_process_worklist_hook)(struct mount *);

/* cache_* may belong in namei.h. */
void	cache_enter(struct vnode *dvp, struct vnode *vp,
	    struct componentname *cnp);
int	cache_lookup(struct vnode *dvp, struct vnode **vpp,
	    struct componentname *cnp);
void	cache_purge(struct vnode *vp);
void	cache_purgevfs(struct mount *mp);
int	cache_leaf_test(struct vnode *vp);
int	change_dir(struct vnode *vp, struct thread *td);
int	change_root(struct vnode *vp, struct thread *td);
void	cvtstat(struct stat *st, struct ostat *ost);
void	cvtnstat(struct stat *sb, struct nstat *nsb);
int	getnewvnode(const char *tag, struct mount *mp, struct vop_vector *vops,
	    struct vnode **vpp);
u_quad_t init_va_filerev(void);
int	lease_check(struct vop_lease_args *ap);
int	speedup_syncer(void);
#define textvp_fullpath(p, rb, rfb) \
	vn_fullpath(FIRST_THREAD_IN_PROC(p), (p)->p_textvp, rb, rfb)
int	vn_fullpath(struct thread *td, struct vnode *vn,
	    char **retbuf, char **freebuf);
int	vaccess(enum vtype type, mode_t file_mode, uid_t uid, gid_t gid,
	    mode_t acc_mode, struct ucred *cred, int *privused);
int	vaccess_acl_posix1e(enum vtype type, uid_t file_uid,
	    gid_t file_gid, struct acl *acl, mode_t acc_mode,
	    struct ucred *cred, int *privused);
void	vattr_null(struct vattr *vap);
int	vcount(struct vnode *vp);
void	vdrop(struct vnode *);
void	vfs_add_vnodeops(const void *);
void	vfs_rm_vnodeops(const void *);
int	vflush(struct mount *mp, int rootrefs, int flags, struct thread *td);
int	vget(struct vnode *vp, int lockflag, struct thread *td);
void	vgone(struct vnode *vp);
void	vhold(struct vnode *);
void	vholdl(struct vnode *);
int	vinvalbuf(struct vnode *vp, int save,
	    struct thread *td, int slpflag, int slptimeo);
int	vtruncbuf(struct vnode *vp, struct ucred *cred, struct thread *td,
	    off_t length, int blksize);
void	vn_printf(struct vnode *vp, const char *fmt, ...) __printflike(2,3);
#define vprint(label, vp) vn_printf((vp), "%s\n", (label))
int	vrecycle(struct vnode *vp, struct thread *td);
int	vn_close(struct vnode *vp,
	    int flags, struct ucred *file_cred, struct thread *td);
void	vn_finished_write(struct mount *mp);
int	vn_isdisk(struct vnode *vp, int *errp);
int	vn_lock(struct vnode *vp, int flags, struct thread *td);
#ifdef	DEBUG_LOCKS
int	debug_vn_lock(struct vnode *vp, int flags, struct thread *p,
	    const char *filename, int line);
#define vn_lock(vp,flags,p) debug_vn_lock(vp,flags,p,__FILE__,__LINE__)
#endif
int	vn_open(struct nameidata *ndp, int *flagp, int cmode, int fdidx);
int	vn_open_cred(struct nameidata *ndp, int *flagp, int cmode,
	    struct ucred *cred, int fdidx);
int	vn_pollrecord(struct vnode *vp, struct thread *p, int events);
int	vn_rdwr(enum uio_rw rw, struct vnode *vp, caddr_t base,
	    int len, off_t offset, enum uio_seg segflg, int ioflg,
	    struct ucred *active_cred, struct ucred *file_cred, int *aresid,
	    struct thread *td);
int	vn_rdwr_inchunks(enum uio_rw rw, struct vnode *vp, caddr_t base,
	    size_t len, off_t offset, enum uio_seg segflg, int ioflg,
	    struct ucred *active_cred, struct ucred *file_cred, size_t *aresid,
	    struct thread *td);
int	vn_stat(struct vnode *vp, struct stat *sb, struct ucred *active_cred,
	    struct ucred *file_cred, struct thread *td);
int	vn_start_write(struct vnode *vp, struct mount **mpp, int flags);
int	vn_write_suspend_wait(struct vnode *vp, struct mount *mp,
	    int flags);
int	vn_writechk(struct vnode *vp);
int	vn_extattr_get(struct vnode *vp, int ioflg, int attrnamespace,
	    const char *attrname, int *buflen, char *buf, struct thread *td);
int	vn_extattr_set(struct vnode *vp, int ioflg, int attrnamespace,
	    const char *attrname, int buflen, char *buf, struct thread *td);
int	vn_extattr_rm(struct vnode *vp, int ioflg, int attrnamespace,
	    const char *attrname, struct thread *td);
int	vfs_cache_lookup(struct vop_lookup_args *ap);
void	vfs_timestamp(struct timespec *);
void	vfs_write_resume(struct mount *mp);
int	vfs_write_suspend(struct mount *mp);
int	vop_stdbmap(struct vop_bmap_args *);
int	vop_stdfsync(struct vop_fsync_args *);
int	vop_stdgetwritemount(struct vop_getwritemount_args *);
int	vop_stdgetpages(struct vop_getpages_args *);
int	vop_stdinactive(struct vop_inactive_args *);
int	vop_stdislocked(struct vop_islocked_args *);
int	vop_stdkqfilter(struct vop_kqfilter_args *);
int	vop_stdlock(struct vop_lock_args *);
int	vop_stdputpages(struct vop_putpages_args *);
int	vop_stdunlock(struct vop_unlock_args *);
int	vop_nopoll(struct vop_poll_args *);
int	vop_stdpathconf(struct vop_pathconf_args *);
int	vop_stdpoll(struct vop_poll_args *);
int	vop_eopnotsupp(struct vop_generic_args *ap);
int	vop_ebadf(struct vop_generic_args *ap);
int	vop_einval(struct vop_generic_args *ap);
int	vop_enotty(struct vop_generic_args *ap);
int	vop_null(struct vop_generic_args *ap);
int	vop_panic(struct vop_generic_args *ap);

/* These are called from within the actual VOPS. */
void	vop_create_post(void *a, int rc);
void	vop_link_post(void *a, int rc);
void	vop_lock_pre(void *a);
void	vop_lock_post(void *a, int rc);
void	vop_lookup_post(void *a, int rc);
void	vop_lookup_pre(void *a);
void	vop_mkdir_post(void *a, int rc);
void	vop_mknod_post(void *a, int rc);
void	vop_remove_post(void *a, int rc);
void	vop_rename_post(void *a, int rc);
void	vop_rename_pre(void *a);
void	vop_rmdir_post(void *a, int rc);
void	vop_setattr_post(void *a, int rc);
void	vop_strategy_pre(void *a);
void	vop_symlink_post(void *a, int rc);
void	vop_unlock_post(void *a, int rc);
void	vop_unlock_pre(void *a);

#define	VOP_WRITE_PRE(ap)						\
	struct vattr va;						\
	int error, osize, ooffset, noffset;				\
									\
	osize = ooffset = noffset = 0;					\
	if (!VN_KNLIST_EMPTY((ap)->a_vp)) {				\
		error = VOP_GETATTR((ap)->a_vp, &va, (ap)->a_cred,	\
		    curthread);						\
		if (error)						\
			return (error);					\
		ooffset = (ap)->a_uio->uio_offset;			\
		osize = va.va_size;					\
	}

#define VOP_WRITE_POST(ap, ret)						\
	noffset = (ap)->a_uio->uio_offset;				\
	if (noffset > ooffset && !VN_KNLIST_EMPTY((ap)->a_vp)) {	\
		VFS_KNOTE_LOCKED((ap)->a_vp, NOTE_WRITE			\
		    | (noffset > osize ? NOTE_EXTEND : 0));		\
	}

void	vput(struct vnode *vp);
void	vrele(struct vnode *vp);
void	vref(struct vnode *vp);
int	vrefcnt(struct vnode *vp);
void 	v_addpollinfo(struct vnode *vp);

int vnode_create_vobject(struct vnode *vp, size_t size, struct thread *td);
void vnode_destroy_vobject(struct vnode *vp);

extern struct vop_vector fifo_specops;
extern struct vop_vector dead_vnodeops;
extern struct vop_vector default_vnodeops;

#define VOP_PANIC	((void*)(uintptr_t)vop_panic)
#define VOP_NULL	((void*)(uintptr_t)vop_null)
#define VOP_EBADF	((void*)(uintptr_t)vop_ebadf)
#define VOP_ENOTTY	((void*)(uintptr_t)vop_enotty)
#define VOP_EINVAL	((void*)(uintptr_t)vop_einval)
#define VOP_EOPNOTSUPP	((void*)(uintptr_t)vop_eopnotsupp)

/* vfs_hash.c */
typedef int vfs_hash_cmp_t(struct vnode *vp, void *arg);

int vfs_hash_get(struct mount *mp, u_int hash, int flags, struct thread *td, struct vnode **vpp, vfs_hash_cmp_t *fn, void *arg);
int vfs_hash_insert(struct vnode *vp, u_int hash, int flags, struct thread *td, struct vnode **vpp, vfs_hash_cmp_t *fn, void *arg);
void vfs_hash_rehash(struct vnode *vp, u_int hash);
void vfs_hash_remove(struct vnode *vp);

int vfs_kqfilter(struct vop_kqfilter_args *);

#endif /* _KERNEL */

#endif /* !_SYS_VNODE_H_ */
