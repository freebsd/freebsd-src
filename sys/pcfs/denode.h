/*
 *  Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 *  You can do anything you want with this software,
 *    just don't say you wrote it,
 *    and don't remove this notice.
 *
 *  This software is provided "as is".
 *
 *  The author supplies this software to be publicly
 *  redistributed on the understanding that the author
 *  is not responsible for the correct functioning of
 *  this software in any circumstances and is not liable
 *  for any damages caused by this software.
 *
 *  October 1992
 *
 *	$Id: denode.h,v 1.6 1993/12/19 00:54:25 wollman Exp $
 */

#ifndef _PCFS_DENODE_H_
#define _PCFS_DENODE_H_ 1

/*
 *  This is the pc filesystem specific portion of the
 *  vnode structure.
 *  To describe a file uniquely the de_dirclust, de_diroffset,
 *  and de_de.deStartCluster fields are used.  de_dirclust
 *  contains the cluster number of the directory cluster containing
 *  the entry for a file or directory.  de_diroffset is the
 *  index into the cluster for the entry describing a file
 *  or directory.  de_de.deStartCluster is the number of the
 *  first cluster of the file or directory.  Now to describe the
 *  quirks of the pc filesystem.
 *  - Clusters 0 and 1 are reserved.
 *  - The first allocatable cluster is 2.
 *  - The root directory is of fixed size and all blocks that
 *    make it up are contiguous.
 *  - Cluster 0 refers to the root directory when it is found
 *    in the startcluster field of a directory entry that points
 *    to another directory.
 *  - Cluster 0 implies a 0 length file when found in the start
 *    cluster field of a directory entry that points to a file.
 *  - You can't use the cluster number 0 to derive
 *    the address of the root directory.
 *  - Multiple directory entries can point to a directory.
 *    The entry in the parent directory points to a child
 *    directory.  Any directories in the child directory contain
 *    a ".." entry that points back to the child.  The child
 *    directory itself contains a "." entry that points to
 *    itself.
 *  - The root directory does not contain a "." or ".." entry.
 *  - Directory entries for directories are never changed once
 *    they are created (except when removed).  The size stays
 *    0, and the last modification time is never changed.  This
 *    is because so many directory entries can point to the physical
 *    clusters that make up a directory.  It would lead to an update
 *    nightmare.
 *  - The length field in a directory entry pointing to a directory
 *    contains 0 (always).  The only way to find the end of a directory
 *    is to follow the cluster chain until the "last cluster"
 *    marker is found.
 *  My extensions to make this house of cards work.  These apply
 *  only to the in memory copy of the directory entry.
 *  - A reference count for each denode will be kept since dos doesn't
 *    keep such things.
 */

/* Internal pseudo-offset for (nonexistent) directory entry for the root dir
 * in the root dir */
#define	PCFSROOT_OFS	0x1fffffff

/*
 *  The fat cache structure.
 *  fc_fsrcn is the filesystem relative cluster number
 *  that corresponds to the file relative cluster number
 *  in this structure (fc_frcn).
 */
struct fatcache {
	u_short fc_frcn;	/* file relative cluster number	*/
	u_short fc_fsrcn;	/* filesystem relative cluster number */
};

/*
 *  The fat entry cache as it stands helps make extending
 *  files a "quick" operation by avoiding having to scan
 *  the fat to discover the last cluster of the file.
 *  The cache also helps sequential reads by remembering
 *  the last cluster read from the file.  This also prevents
 *  us from having to rescan the fat to find the next cluster
 *  to read.  This cache is probably pretty worthless if a
 *  file is opened by multiple processes.
 */
#define	FC_SIZE		2	/* number of entries in the cache	*/
#define	FC_LASTMAP	0	/* entry the last call to pcbmap() resolved to */
#define	FC_LASTFC	1	/* entry for the last cluster in the file */

#define	FCE_EMPTY	0xffff	/* doesn't represent an actual cluster # */

/*
 *  Set a slot in the fat cache.
 */
#define	fc_setcache(dep, slot, frcn, fsrcn) \
	(dep)->de_fc[slot].fc_frcn = frcn; \
	(dep)->de_fc[slot].fc_fsrcn = fsrcn;

/*
 *  This is the in memory variant of a dos directory
 *  entry.  It is usually contained within a vnode.
 */
struct denode {
	struct denode *de_chain[2];	/* hash chain ptrs		*/
	struct vnode *de_vnode;		/* addr of vnode we are part of	*/
	struct vnode *de_devvp;		/* vnode of blk dev we live on	*/
	u_long de_flag;			/* flag bits			*/
	dev_t de_dev;			/* device where direntry lives	*/
	u_long de_dirclust;		/* cluster of the directory file
					 *  containing this entry	*/
	u_long de_diroffset;		/* ordinal of this entry in the
					 *  directory			*/
	long de_refcnt;			/* reference count		*/
	struct pcfsmount *de_pmp;	/* addr of our mount struct	*/
	struct lockf *de_lockf;		/* byte level lock list		*/
	long de_spare0;			/* current lock holder		*/
	long de_spare1;			/* lock wanter			*/
	struct direntry de_de;		/* the actual directory entry	*/
	struct fatcache de_fc[FC_SIZE];	/* fat cache			*/
};

/*
 *  Values for the de_flag field of the denode.
 */
#define	DELOCKED	0x0001		/* directory entry is locked	*/
#define	DEWANT		0x0002		/* someone wants this de	*/
#define	DERENAME	0x0004		/* de is being renamed		*/
#define	DEUPD		0x0008		/* file has been modified	*/
#define	DESHLOCK	0x0010		/* file has shared lock		*/
#define	DEEXLOCK	0x0020		/* file has exclusive lock	*/
#define	DELWAIT		0x0040		/* someone waiting on file lock	*/
#define	DEMOD		0x0080		/* denode wants to be written back
					 *  to disk			*/

/*
 *  Shorthand macros used to reference fields in the direntry
 *  contained in the denode structure.
 */
#define	de_Name		de_de.deName
#define	de_Extension	de_de.deExtension
#define	de_Attributes	de_de.deAttributes
#define	de_Reserved	de_de.deReserved
#define	de_Time		de_de.deTime
#define	de_Date		de_de.deDate
#define	de_StartCluster	de_de.deStartCluster
#define	de_FileSize	de_de.deFileSize
#define	de_forw		de_chain[0]
#define	de_back		de_chain[1]

#if defined(KERNEL)

#define	VTODE(vp)	((struct denode *)(vp)->v_data)
#define	DETOV(de)	((de)->de_vnode)

#define	DELOCK(de)	delock(de)
extern void delock(struct denode *);
#define	DEUNLOCK(de)	deunlock(de)
extern void deunlock(struct denode *);

#define	DEUPDAT(dep, t, waitfor) \
	if (dep->de_flag & DEUPD) \
		(void) deupdat(dep, t, waitfor);

#define	DETIMES(dep, t) \
	if (dep->de_flag & DEUPD) { \
		(dep)->de_flag |= DEMOD; \
		unix2dostime(t, (union dosdate *)&dep->de_Date, \
			(union dostime *)&dep->de_Time); \
		(dep)->de_flag &= ~DEUPD; \
	}

/*
 * This overlays the fid sturcture (see mount.h)
 */
struct defid {
	u_short	defid_len;	/* length of structure */
	u_short	defid_pad;	/* force long alignment */

	u_long	defid_dirclust;	/* cluster this dir entry came from	*/
	u_long	defid_dirofs;	/* index of entry within the cluster	*/

/*	u_long	defid_gen;*/	/* generation number */
};

/*
 * Prototypes for PCFS vnode operations
 */
int pcfs_lookup __P((struct vnode *vp, struct nameidata *ndp, struct proc *p));
int pcfs_create __P((struct nameidata *ndp, struct vattr *vap, struct proc *p));
int pcfs_mknod __P((struct nameidata *ndp, struct vattr *vap, struct ucred *cred,
	struct proc *p));
int pcfs_open __P((struct vnode *vp, int mode, struct ucred *cred,
	struct proc *p));
int pcfs_close __P((struct vnode *vp, int fflag, struct ucred *cred,
	struct proc *p));
int pcfs_access __P((struct vnode *vp, int mode, struct ucred *cred,
	struct proc *p));
int pcfs_getattr __P((struct vnode *vp, struct vattr *vap, struct ucred *cred,
	struct proc *p));
int pcfs_setattr __P((struct vnode *vp, struct vattr *vap, struct ucred *cred,
	struct proc *p));
int pcfs_read __P((struct vnode *vp, struct uio *uio, int ioflag,
	struct ucred *cred));
int pcfs_write __P((struct vnode *vp, struct uio *uio, int ioflag,
	struct ucred *cred));
int pcfs_ioctl __P((struct vnode *vp, int command, caddr_t data, int fflag,
	struct ucred *cred, struct proc *p));
int pcfs_select __P((struct vnode *vp, int which, int fflags, struct ucred *cred,
	struct proc *p));
int pcfs_mmap __P((struct vnode *vp, int fflags, struct ucred *cred,
	struct proc *p));
int pcfs_fsync __P((struct vnode *vp, int fflags, struct ucred *cred,
	int waitfor, struct proc *p));
int pcfs_seek __P((struct vnode *vp, off_t oldoff, off_t newoff,
	struct ucred *cred));
int pcfs_remove __P((struct nameidata *ndp, struct proc *p));
int pcfs_link __P((struct vnode *vp, struct nameidata *ndp, struct proc *p));
int pcfs_rename __P((struct nameidata *fndp, struct nameidata *tdnp,
	struct proc *p));
int pcfs_mkdir __P((struct nameidata *ndp, struct vattr *vap, struct proc *p));
int pcfs_rmdir __P((struct nameidata *ndp, struct proc *p));
int pcfs_symlink __P((struct nameidata *ndp, struct vattr *vap, char *target,
	struct proc *p));
int pcfs_readdir __P((struct vnode *vp, struct uio *uio, struct ucred *cred,
	int *eofflagp));
int pcfs_readlink __P((struct vnode *vp, struct uio *uio, struct ucred *cred));
int pcfs_abortop __P((struct nameidata *ndp));
int pcfs_inactive __P((struct vnode *vp, struct proc *p));
int pcfs_reclaim __P((struct vnode *vp));
int pcfs_lock __P((struct vnode *vp));
int pcfs_unlock __P((struct vnode *vp));
int pcfs_bmap __P((struct vnode *vp, daddr_t bn, struct vnode **vpp,
	daddr_t *bnp));
int pcfs_strategy __P((struct buf *bp));
void pcfs_print __P((struct vnode *vp));
int pcfs_islocked __P((struct vnode *vp));
int pcfs_advlock __P((struct vnode *vp, caddr_t id, int op, struct flock *fl,
	int flags));

/*
 *  Internal service routine prototypes.
 */
int deget __P((struct pcfsmount *pmp, u_long dirclust, u_long diroffset,
	struct direntry *direntptr, struct denode **depp));

extern int pcbmap(struct denode *, u_long, daddr_t *, u_long *);
extern void fc_purge(struct denode *, u_int);
extern void updateotherfats(struct pcfsmount *, struct buf *, u_long);
extern int clusterfree(struct pcfsmount *, u_long, u_long *);
extern int fatentry(int, struct pcfsmount *, u_long, u_long *, u_long);
extern int clusteralloc(struct pcfsmount *, u_long *, u_long);
extern int freeclusterchain(struct pcfsmount *, u_long);
extern int fillinusemap(struct pcfsmount *);
extern int extendfile(struct denode *, struct buf **, u_int *);
extern int readep(struct pcfsmount *, u_long, u_long, struct buf **,
		  struct direntry **);
extern int readde(struct denode *, struct buf **, struct direntry **);
extern int doscheckpath(struct denode *, struct denode *);
extern int dosdirempty(struct denode *);
extern int removede(struct nameidata *);
extern int markdeleted(struct pcfsmount *, u_long, u_long);
extern int createde(struct denode *, struct nameidata *, struct denode **);
extern int pcfs_lookup(struct vnode *, struct nameidata *, struct proc *);

extern void pcfs_init(void);
extern void deput(struct denode *);
extern int deupdat(struct denode *, struct timeval *, int);
extern int detrunc(struct denode *, u_long, int);
extern void reinsert(struct denode *);
extern int pcfs_reclaim(struct vnode *);
extern int pcfs_inactive(struct vnode *, struct proc *);


#endif /* defined(KERNEL) */
#endif /* _PCFS_DENODE_H_ */
