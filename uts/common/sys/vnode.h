/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 1988, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2017, Joyent, Inc.
 * Copyright (c) 2011, 2017 by Delphix. All rights reserved.
 */

/*	Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */

#ifndef _SYS_VNODE_H
#define	_SYS_VNODE_H

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/rwstlock.h>
#include <sys/time_impl.h>
#include <sys/cred.h>
#include <sys/uio.h>
#include <sys/resource.h>
#include <vm/seg_enum.h>
#include <sys/kstat.h>
#include <sys/kmem.h>
#include <sys/list.h>
#ifdef	_KERNEL
#include <sys/buf.h>
#include <sys/sdt.h>
#endif	/* _KERNEL */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Statistics for all vnode operations.
 * All operations record number of ops (since boot/mount/zero'ed).
 * Certain I/O operations (read, write, readdir) also record number
 * of bytes transferred.
 * This appears in two places in the system: one is embedded in each
 * vfs_t.  There is also an array of vopstats_t structures allocated
 * on a per-fstype basis.
 */

#define	VOPSTATS_STR	"vopstats_"	/* Initial string for vopstat kstats */

typedef struct vopstats {
	kstat_named_t	nopen;		/* VOP_OPEN */
	kstat_named_t	nclose;		/* VOP_CLOSE */
	kstat_named_t	nread;		/* VOP_READ */
	kstat_named_t	read_bytes;
	kstat_named_t	nwrite;		/* VOP_WRITE */
	kstat_named_t	write_bytes;
	kstat_named_t	nioctl;		/* VOP_IOCTL */
	kstat_named_t	nsetfl;		/* VOP_SETFL */
	kstat_named_t	ngetattr;	/* VOP_GETATTR */
	kstat_named_t	nsetattr;	/* VOP_SETATTR */
	kstat_named_t	naccess;	/* VOP_ACCESS */
	kstat_named_t	nlookup;	/* VOP_LOOKUP */
	kstat_named_t	ncreate;	/* VOP_CREATE */
	kstat_named_t	nremove;	/* VOP_REMOVE */
	kstat_named_t	nlink;		/* VOP_LINK */
	kstat_named_t	nrename;	/* VOP_RENAME */
	kstat_named_t	nmkdir;		/* VOP_MKDIR */
	kstat_named_t	nrmdir;		/* VOP_RMDIR */
	kstat_named_t	nreaddir;	/* VOP_READDIR */
	kstat_named_t	readdir_bytes;
	kstat_named_t	nsymlink;	/* VOP_SYMLINK */
	kstat_named_t	nreadlink;	/* VOP_READLINK */
	kstat_named_t	nfsync;		/* VOP_FSYNC */
	kstat_named_t	ninactive;	/* VOP_INACTIVE */
	kstat_named_t	nfid;		/* VOP_FID */
	kstat_named_t	nrwlock;	/* VOP_RWLOCK */
	kstat_named_t	nrwunlock;	/* VOP_RWUNLOCK */
	kstat_named_t	nseek;		/* VOP_SEEK */
	kstat_named_t	ncmp;		/* VOP_CMP */
	kstat_named_t	nfrlock;	/* VOP_FRLOCK */
	kstat_named_t	nspace;		/* VOP_SPACE */
	kstat_named_t	nrealvp;	/* VOP_REALVP */
	kstat_named_t	ngetpage;	/* VOP_GETPAGE */
	kstat_named_t	nputpage;	/* VOP_PUTPAGE */
	kstat_named_t	nmap;		/* VOP_MAP */
	kstat_named_t	naddmap;	/* VOP_ADDMAP */
	kstat_named_t	ndelmap;	/* VOP_DELMAP */
	kstat_named_t	npoll;		/* VOP_POLL */
	kstat_named_t	ndump;		/* VOP_DUMP */
	kstat_named_t	npathconf;	/* VOP_PATHCONF */
	kstat_named_t	npageio;	/* VOP_PAGEIO */
	kstat_named_t	ndumpctl;	/* VOP_DUMPCTL */
	kstat_named_t	ndispose;	/* VOP_DISPOSE */
	kstat_named_t	nsetsecattr;	/* VOP_SETSECATTR */
	kstat_named_t	ngetsecattr;	/* VOP_GETSECATTR */
	kstat_named_t	nshrlock;	/* VOP_SHRLOCK */
	kstat_named_t	nvnevent;	/* VOP_VNEVENT */
	kstat_named_t	nreqzcbuf;	/* VOP_REQZCBUF */
	kstat_named_t	nretzcbuf;	/* VOP_RETZCBUF */
} vopstats_t;

/*
 * The vnode is the focus of all file activity in UNIX.
 * A vnode is allocated for each active file, each current
 * directory, each mounted-on file, and the root.
 *
 * Each vnode is usually associated with a file-system-specific node (for
 * UFS, this is the in-memory inode).  Generally, a vnode and an fs-node
 * should be created and destroyed together as a pair.
 *
 * If a vnode is reused for a new file, it should be reinitialized by calling
 * either vn_reinit() or vn_recycle().
 *
 * vn_reinit() resets the entire vnode as if it was returned by vn_alloc().
 * The caller is responsible for setting up the entire vnode after calling
 * vn_reinit().  This is important when using kmem caching where the vnode is
 * allocated by a constructor, for instance.
 *
 * vn_recycle() is used when the file system keeps some state around in both
 * the vnode and the associated FS-node.  In UFS, for example, the inode of
 * a deleted file can be reused immediately.  The v_data, v_vfsp, v_op, etc.
 * remains the same but certain fields related to the previous instance need
 * to be reset.  In particular:
 *	v_femhead
 *	v_path
 *	v_rdcnt, v_wrcnt
 *	v_mmap_read, v_mmap_write
 */

/*
 * vnode types.  VNON means no type.  These values are unrelated to
 * values in on-disk inodes.
 */
typedef enum vtype {
	VNON	= 0,
	VREG	= 1,
	VDIR	= 2,
	VBLK	= 3,
	VCHR	= 4,
	VLNK	= 5,
	VFIFO	= 6,
	VDOOR	= 7,
	VPROC	= 8,
	VSOCK	= 9,
	VPORT	= 10,
	VBAD	= 11
} vtype_t;

/*
 * VSD - Vnode Specific Data
 * Used to associate additional private data with a vnode.
 */
struct vsd_node {
	list_node_t vs_nodes;		/* list of all VSD nodes */
	uint_t vs_nkeys;		/* entries in value array */
	void **vs_value;		/* array of value/key */
};

/*
 * Many of the fields in the vnode are read-only once they are initialized
 * at vnode creation time.  Other fields are protected by locks.
 *
 * IMPORTANT: vnodes should be created ONLY by calls to vn_alloc().  They
 * may not be embedded into the file-system specific node (inode).  The
 * size of vnodes may change.
 *
 * The v_lock protects:
 *   v_flag
 *   v_stream
 *   v_count
 *   v_shrlocks
 *   v_path
 *   v_vsd
 *   v_xattrdir
 *
 * A special lock (implemented by vn_vfswlock in vnode.c) protects:
 *   v_vfsmountedhere
 *
 * The global flock_lock mutex (in flock.c) protects:
 *   v_filocks
 *
 * IMPORTANT NOTE:
 *
 *   The following vnode fields are considered public and may safely be
 *   accessed by file systems or other consumers:
 *
 *     v_lock
 *     v_flag
 *     v_count
 *     v_data
 *     v_vfsp
 *     v_stream
 *     v_type
 *     v_rdev
 *
 * ALL OTHER FIELDS SHOULD BE ACCESSED ONLY BY THE OWNER OF THAT FIELD.
 * In particular, file systems should not access other fields; they may
 * change or even be removed.  The functionality which was once provided
 * by these fields is available through vn_* functions.
 *
 * VNODE PATH THEORY:
 * In each vnode, the v_path field holds a cached version of the canonical
 * filesystem path which that node represents.  Because vnodes lack contextual
 * information about their own name or position in the VFS hierarchy, this path
 * must be calculated when the vnode is instantiated by operations such as
 * fop_create, fop_lookup, or fop_mkdir.  During said operations, both the
 * parent vnode (and its cached v_path) and future name are known, so the
 * v_path of the resulting object can easily be set.
 *
 * The caching nature of v_path is complicated in the face of directory
 * renames.  Filesystem drivers are responsible for calling vn_renamepath when
 * a fop_rename operation succeeds.  While the v_path on the renamed vnode will
 * be updated, existing children of the directory (direct, or at deeper levels)
 * will now possess v_path caches which are stale.
 *
 * It is expensive (and for non-directories, impossible) to recalculate stale
 * v_path entries during operations such as vnodetopath.  The best time during
 * which to correct such wrongs is the same as when v_path is first
 * initialized: during fop_create/fop_lookup/fop_mkdir/etc, where adequate
 * context is available to generate the current path.
 *
 * In order to quickly detect stale v_path entries (without full lookup
 * verification) to trigger a v_path update, the v_path_stamp field has been
 * added to vnode_t.  As part of successful fop_create/fop_lookup/fop_mkdir
 * operations, where the name and parent vnode are available, the following
 * rules are used to determine updates to the child:
 *
 * 1. If the parent lacks a v_path, clear any existing v_path and v_path_stamp
 *    on the child.  Until the parent v_path is refreshed to a valid state, the
 *    child v_path must be considered invalid too.
 *
 * 2. If the child lacks a v_path (implying v_path_stamp == 0), it inherits the
 *    v_path_stamp value from its parent and its v_path is updated.
 *
 * 3. If the child v_path_stamp is less than v_path_stamp in the parent, it is
 *    an indication that the child v_path is stale.  The v_path is updated and
 *    v_path_stamp in the child is set to the current hrtime().
 *
 *    It does _not_ inherit the parent v_path_stamp in order to propagate the
 *    the time of v_path invalidation through the directory structure.  This
 *    prevents concurrent invalidations (operating with a now-incorrect v_path)
 *    at deeper levels in the tree from persisting.
 *
 * 4. If the child v_path_stamp is greater or equal to the parent, no action
 *    needs to be taken.
 *
 * Note that fop_rename operations do not follow this ruleset.  They perform an
 * explicit update of v_path and v_path_stamp (setting it to the current time)
 *
 * With these constraints in place, v_path invalidations and updates should
 * proceed in a timely manner as vnodes are accessed.  While there still are
 * limited cases where vnodetopath operations will fail, the risk is minimized.
 */

struct fem_head;	/* from fem.h */

typedef struct vnode {
	kmutex_t	v_lock;		/* protects vnode fields */
	uint_t		v_flag;		/* vnode flags (see below) */
	uint_t		v_count;	/* reference count */
	void		*v_data;	/* private data for fs */
	struct vfs	*v_vfsp;	/* ptr to containing VFS */
	struct stdata	*v_stream;	/* associated stream */
	enum vtype	v_type;		/* vnode type */
	dev_t		v_rdev;		/* device (VCHR, VBLK) */

	/* PRIVATE FIELDS BELOW - DO NOT USE */

	struct vfs	*v_vfsmountedhere; /* ptr to vfs mounted here */
	struct vnodeops	*v_op;		/* vnode operations */
	struct page	*v_pages;	/* vnode pages list */
	struct filock	*v_filocks;	/* ptr to filock list */
	struct shrlocklist *v_shrlocks;	/* ptr to shrlock list */
	krwlock_t	v_nbllock;	/* sync for NBMAND locks */
	kcondvar_t	v_cv;		/* synchronize locking */
	void		*v_locality;	/* hook for locality info */
	struct fem_head	*v_femhead;	/* fs monitoring */
	char		*v_path;	/* cached path */
	hrtime_t	v_path_stamp;	/* timestamp for cached path */
	uint_t		v_rdcnt;	/* open for read count  (VREG only) */
	uint_t		v_wrcnt;	/* open for write count (VREG only) */
	u_longlong_t	v_mmap_read;	/* mmap read count */
	u_longlong_t	v_mmap_write;	/* mmap write count */
	void		*v_mpssdata;	/* info for large page mappings */
	void		*v_fopdata;	/* list of file ops event watches */
	kmutex_t	v_vsd_lock;	/* protects v_vsd field */
	struct vsd_node *v_vsd;		/* vnode specific data */
	struct vnode	*v_xattrdir;	/* unnamed extended attr dir (GFS) */
	uint_t		v_count_dnlc;	/* dnlc reference count */
} vnode_t;

#define	IS_DEVVP(vp)	\
	((vp)->v_type == VCHR || (vp)->v_type == VBLK || (vp)->v_type == VFIFO)

#define	VNODE_ALIGN	64
/* Count of low-order 0 bits in a vnode *, based on size and alignment. */
#if defined(_LP64)
#define	VNODE_ALIGN_LOG2	8
#else
#define	VNODE_ALIGN_LOG2	7
#endif

/*
 * vnode flags.
 */
#define	VROOT		0x01	/* root of its file system */
#define	VNOCACHE	0x02	/* don't keep cache pages on vnode */
#define	VNOMAP		0x04	/* file cannot be mapped/faulted */
#define	VDUP		0x08	/* file should be dup'ed rather then opened */
#define	VNOSWAP		0x10	/* file cannot be used as virtual swap device */
#define	VNOMOUNT	0x20	/* file cannot be covered by mount */
#define	VISSWAP		0x40	/* vnode is being used for swap */
#define	VSWAPLIKE	0x80	/* vnode acts like swap (but may not be) */

#define	IS_SWAPVP(vp)	(((vp)->v_flag & (VISSWAP | VSWAPLIKE)) != 0)

typedef struct vn_vfslocks_entry {
	rwstlock_t ve_lock;
	void *ve_vpvfs;
	struct vn_vfslocks_entry *ve_next;
	uint32_t ve_refcnt;
	char pad[64 - sizeof (rwstlock_t) - 2 * sizeof (void *) - \
	    sizeof (uint32_t)];
} vn_vfslocks_entry_t;

/*
 * The following two flags are used to lock the v_vfsmountedhere field
 */
#define	VVFSLOCK	0x100
#define	VVFSWAIT	0x200

/*
 * Used to serialize VM operations on a vnode
 */
#define	VVMLOCK		0x400

/*
 * Tell vn_open() not to fail a directory open for writing but
 * to go ahead and call VOP_OPEN() to let the filesystem check.
 */
#define	VDIROPEN	0x800

/*
 * Flag to let the VM system know that this file is most likely a binary
 * or shared library since it has been mmap()ed EXEC at some time.
 */
#define	VVMEXEC		0x1000

#define	VPXFS		0x2000  /* clustering: global fs proxy vnode */

#define	IS_PXFSVP(vp)	((vp)->v_flag & VPXFS)

#define	V_XATTRDIR	0x4000	/* attribute unnamed directory */

#define	IS_XATTRDIR(vp)	((vp)->v_flag & V_XATTRDIR)

#define	V_LOCALITY	0x8000	/* whether locality aware */

/*
 * Flag that indicates the VM should maintain the v_pages list with all modified
 * pages on one end and unmodified pages at the other. This makes finding dirty
 * pages to write back to disk much faster at the expense of taking a minor
 * fault on the first store instruction which touches a writable page.
 */
#define	VMODSORT	(0x10000)
#define	IS_VMODSORT(vp) \
	(pvn_vmodsort_supported != 0 && ((vp)->v_flag  & VMODSORT) != 0)

#define	VISSWAPFS	0x20000	/* vnode is being used for swapfs */

/*
 * The mdb memstat command assumes that IS_SWAPFSVP only uses the
 * vnode's v_flag field.  If this changes, cache the additional
 * fields in mdb; see vn_get in mdb/common/modules/genunix/memory.c
 */
#define	IS_SWAPFSVP(vp)	(((vp)->v_flag & VISSWAPFS) != 0)

#define	V_SYSATTR	0x40000	/* vnode is a GFS system attribute */

/*
 * Indication that VOP_LOOKUP operations on this vnode may yield results from a
 * different VFS instance.  The main use of this is to suppress v_path
 * calculation logic when filesystems such as procfs emit results which defy
 * expectations about normal VFS behavior.
 */
#define	VTRAVERSE	0x80000

/*
 * Vnode attributes.  A bit-mask is supplied as part of the
 * structure to indicate the attributes the caller wants to
 * set (setattr) or extract (getattr).
 */

/*
 * Note that va_nodeid and va_nblocks are 64bit data type.
 * We support large files over NFSV3. With Solaris client and
 * Server that generates 64bit ino's and sizes these fields
 * will overflow if they are 32 bit sizes.
 */

typedef struct vattr {
	uint_t		va_mask;	/* bit-mask of attributes */
	vtype_t		va_type;	/* vnode type (for create) */
	mode_t		va_mode;	/* file access mode */
	uid_t		va_uid;		/* owner user id */
	gid_t		va_gid;		/* owner group id */
	dev_t		va_fsid;	/* file system id (dev for now) */
	u_longlong_t	va_nodeid;	/* node id */
	nlink_t		va_nlink;	/* number of references to file */
	u_offset_t	va_size;	/* file size in bytes */
	timestruc_t	va_atime;	/* time of last access */
	timestruc_t	va_mtime;	/* time of last modification */
	timestruc_t	va_ctime;	/* time of last status change */
	dev_t		va_rdev;	/* device the file represents */
	uint_t		va_blksize;	/* fundamental block size */
	u_longlong_t	va_nblocks;	/* # of blocks allocated */
	uint_t		va_seq;		/* sequence number */
} vattr_t;

#define	AV_SCANSTAMP_SZ	32		/* length of anti-virus scanstamp */

/*
 * Structure of all optional attributes.
 */
typedef struct xoptattr {
	timestruc_t	xoa_createtime;	/* Create time of file */
	uint8_t		xoa_archive;
	uint8_t		xoa_system;
	uint8_t		xoa_readonly;
	uint8_t		xoa_hidden;
	uint8_t		xoa_nounlink;
	uint8_t		xoa_immutable;
	uint8_t		xoa_appendonly;
	uint8_t		xoa_nodump;
	uint8_t		xoa_opaque;
	uint8_t		xoa_av_quarantined;
	uint8_t		xoa_av_modified;
	uint8_t		xoa_av_scanstamp[AV_SCANSTAMP_SZ];
	uint8_t		xoa_reparse;
	uint64_t	xoa_generation;
	uint8_t		xoa_offline;
	uint8_t		xoa_sparse;
} xoptattr_t;

/*
 * The xvattr structure is really a variable length structure that
 * is made up of:
 * - The classic vattr_t (xva_vattr)
 * - a 32 bit quantity (xva_mapsize) that specifies the size of the
 *   attribute bitmaps in 32 bit words.
 * - A pointer to the returned attribute bitmap (needed because the
 *   previous element, the requested attribute bitmap) is variable lenth.
 * - The requested attribute bitmap, which is an array of 32 bit words.
 *   Callers use the XVA_SET_REQ() macro to set the bits corresponding to
 *   the attributes that are being requested.
 * - The returned attribute bitmap, which is an array of 32 bit words.
 *   File systems that support optional attributes use the XVA_SET_RTN()
 *   macro to set the bits corresponding to the attributes that are being
 *   returned.
 * - The xoptattr_t structure which contains the attribute values
 *
 * xva_mapsize determines how many words in the attribute bitmaps.
 * Immediately following the attribute bitmaps is the xoptattr_t.
 * xva_getxoptattr() is used to get the pointer to the xoptattr_t
 * section.
 */

#define	XVA_MAPSIZE	3		/* Size of attr bitmaps */
#define	XVA_MAGIC	0x78766174	/* Magic # for verification */

/*
 * The xvattr structure is an extensible structure which permits optional
 * attributes to be requested/returned.  File systems may or may not support
 * optional attributes.  They do so at their own discretion but if they do
 * support optional attributes, they must register the VFSFT_XVATTR feature
 * so that the optional attributes can be set/retrived.
 *
 * The fields of the xvattr structure are:
 *
 * xva_vattr - The first element of an xvattr is a legacy vattr structure
 * which includes the common attributes.  If AT_XVATTR is set in the va_mask
 * then the entire structure is treated as an xvattr.  If AT_XVATTR is not
 * set, then only the xva_vattr structure can be used.
 *
 * xva_magic - 0x78766174 (hex for "xvat"). Magic number for verification.
 *
 * xva_mapsize - Size of requested and returned attribute bitmaps.
 *
 * xva_rtnattrmapp - Pointer to xva_rtnattrmap[].  We need this since the
 * size of the array before it, xva_reqattrmap[], could change which means
 * the location of xva_rtnattrmap[] could change.  This will allow unbundled
 * file systems to find the location of xva_rtnattrmap[] when the sizes change.
 *
 * xva_reqattrmap[] - Array of requested attributes.  Attributes are
 * represented by a specific bit in a specific element of the attribute
 * map array.  Callers set the bits corresponding to the attributes
 * that the caller wants to get/set.
 *
 * xva_rtnattrmap[] - Array of attributes that the file system was able to
 * process.  Not all file systems support all optional attributes.  This map
 * informs the caller which attributes the underlying file system was able
 * to set/get.  (Same structure as the requested attributes array in terms
 * of each attribute  corresponding to specific bits and array elements.)
 *
 * xva_xoptattrs - Structure containing values of optional attributes.
 * These values are only valid if the corresponding bits in xva_reqattrmap
 * are set and the underlying file system supports those attributes.
 */
typedef struct xvattr {
	vattr_t		xva_vattr;	/* Embedded vattr structure */
	uint32_t	xva_magic;	/* Magic Number */
	uint32_t	xva_mapsize;	/* Size of attr bitmap (32-bit words) */
	uint32_t	*xva_rtnattrmapp;	/* Ptr to xva_rtnattrmap[] */
	uint32_t	xva_reqattrmap[XVA_MAPSIZE];	/* Requested attrs */
	uint32_t	xva_rtnattrmap[XVA_MAPSIZE];	/* Returned attrs */
	xoptattr_t	xva_xoptattrs;	/* Optional attributes */
} xvattr_t;

#ifdef _SYSCALL32
/*
 * For bigtypes time_t changed to 64 bit on the 64-bit kernel.
 * Define an old version for user/kernel interface
 */

#if _LONG_LONG_ALIGNMENT == 8 && _LONG_LONG_ALIGNMENT_32 == 4
#pragma pack(4)
#endif

typedef struct vattr32 {
	uint32_t	va_mask;	/* bit-mask of attributes */
	vtype_t		va_type;	/* vnode type (for create) */
	mode32_t	va_mode;	/* file access mode */
	uid32_t		va_uid;		/* owner user id */
	gid32_t		va_gid;		/* owner group id */
	dev32_t		va_fsid;	/* file system id (dev for now) */
	u_longlong_t	va_nodeid;	/* node id */
	nlink_t		va_nlink;	/* number of references to file */
	u_offset_t	va_size;	/* file size in bytes */
	timestruc32_t	va_atime;	/* time of last access */
	timestruc32_t	va_mtime;	/* time of last modification */
	timestruc32_t	va_ctime;	/* time of last status change */
	dev32_t		va_rdev;	/* device the file represents */
	uint32_t	va_blksize;	/* fundamental block size */
	u_longlong_t	va_nblocks;	/* # of blocks allocated */
	uint32_t	va_seq;		/* sequence number */
} vattr32_t;

#if _LONG_LONG_ALIGNMENT == 8 && _LONG_LONG_ALIGNMENT_32 == 4
#pragma pack()
#endif

#else  /* not _SYSCALL32 */
#define	vattr32		vattr
typedef vattr_t		vattr32_t;
#endif /* _SYSCALL32 */

/*
 * Attributes of interest to the caller of setattr or getattr.
 */
#define	AT_TYPE		0x00001
#define	AT_MODE		0x00002
#define	AT_UID		0x00004
#define	AT_GID		0x00008
#define	AT_FSID		0x00010
#define	AT_NODEID	0x00020
#define	AT_NLINK	0x00040
#define	AT_SIZE		0x00080
#define	AT_ATIME	0x00100
#define	AT_MTIME	0x00200
#define	AT_CTIME	0x00400
#define	AT_RDEV		0x00800
#define	AT_BLKSIZE	0x01000
#define	AT_NBLOCKS	0x02000
/*			0x04000 */	/* unused */
#define	AT_SEQ		0x08000
/*
 * If AT_XVATTR is set then there are additional bits to process in
 * the xvattr_t's attribute bitmap.  If this is not set then the bitmap
 * MUST be ignored.  Note that this bit must be set/cleared explicitly.
 * That is, setting AT_ALL will NOT set AT_XVATTR.
 */
#define	AT_XVATTR	0x10000

#define	AT_ALL		(AT_TYPE|AT_MODE|AT_UID|AT_GID|AT_FSID|AT_NODEID|\
			AT_NLINK|AT_SIZE|AT_ATIME|AT_MTIME|AT_CTIME|\
			AT_RDEV|AT_BLKSIZE|AT_NBLOCKS|AT_SEQ)

#define	AT_STAT		(AT_MODE|AT_UID|AT_GID|AT_FSID|AT_NODEID|AT_NLINK|\
			AT_SIZE|AT_ATIME|AT_MTIME|AT_CTIME|AT_RDEV|AT_TYPE)

#define	AT_TIMES	(AT_ATIME|AT_MTIME|AT_CTIME)

#define	AT_NOSET	(AT_NLINK|AT_RDEV|AT_FSID|AT_NODEID|AT_TYPE|\
			AT_BLKSIZE|AT_NBLOCKS|AT_SEQ)

/*
 * Attribute bits used in the extensible attribute's (xva's) attribute
 * bitmaps.  Note that the bitmaps are made up of a variable length number
 * of 32-bit words.  The convention is to use XAT{n}_{attrname} where "n"
 * is the element in the bitmap (starting at 1).  This convention is for
 * the convenience of the maintainer to keep track of which element each
 * attribute belongs to.
 *
 * NOTE THAT CONSUMERS MUST *NOT* USE THE XATn_* DEFINES DIRECTLY.  CONSUMERS
 * MUST USE THE XAT_* DEFINES.
 */
#define	XAT0_INDEX	0LL		/* Index into bitmap for XAT0 attrs */
#define	XAT0_CREATETIME	0x00000001	/* Create time of file */
#define	XAT0_ARCHIVE	0x00000002	/* Archive */
#define	XAT0_SYSTEM	0x00000004	/* System */
#define	XAT0_READONLY	0x00000008	/* Readonly */
#define	XAT0_HIDDEN	0x00000010	/* Hidden */
#define	XAT0_NOUNLINK	0x00000020	/* Nounlink */
#define	XAT0_IMMUTABLE	0x00000040	/* immutable */
#define	XAT0_APPENDONLY	0x00000080	/* appendonly */
#define	XAT0_NODUMP	0x00000100	/* nodump */
#define	XAT0_OPAQUE	0x00000200	/* opaque */
#define	XAT0_AV_QUARANTINED	0x00000400	/* anti-virus quarantine */
#define	XAT0_AV_MODIFIED	0x00000800	/* anti-virus modified */
#define	XAT0_AV_SCANSTAMP	0x00001000	/* anti-virus scanstamp */
#define	XAT0_REPARSE	0x00002000	/* FS reparse point */
#define	XAT0_GEN	0x00004000	/* object generation number */
#define	XAT0_OFFLINE	0x00008000	/* offline */
#define	XAT0_SPARSE	0x00010000	/* sparse */

#define	XAT0_ALL_ATTRS	(XAT0_CREATETIME|XAT0_ARCHIVE|XAT0_SYSTEM| \
    XAT0_READONLY|XAT0_HIDDEN|XAT0_NOUNLINK|XAT0_IMMUTABLE|XAT0_APPENDONLY| \
    XAT0_NODUMP|XAT0_OPAQUE|XAT0_AV_QUARANTINED|  XAT0_AV_MODIFIED| \
    XAT0_AV_SCANSTAMP|XAT0_REPARSE|XATO_GEN|XAT0_OFFLINE|XAT0_SPARSE)

/* Support for XAT_* optional attributes */
#define	XVA_MASK		0xffffffff	/* Used to mask off 32 bits */
#define	XVA_SHFT		32		/* Used to shift index */

/*
 * Used to pry out the index and attribute bits from the XAT_* attributes
 * defined below.  Note that we're masking things down to 32 bits then
 * casting to uint32_t.
 */
#define	XVA_INDEX(attr)		((uint32_t)(((attr) >> XVA_SHFT) & XVA_MASK))
#define	XVA_ATTRBIT(attr)	((uint32_t)((attr) & XVA_MASK))

/*
 * The following defines present a "flat namespace" so that consumers don't
 * need to keep track of which element belongs to which bitmap entry.
 *
 * NOTE THAT THESE MUST NEVER BE OR-ed TOGETHER
 */
#define	XAT_CREATETIME		((XAT0_INDEX << XVA_SHFT) | XAT0_CREATETIME)
#define	XAT_ARCHIVE		((XAT0_INDEX << XVA_SHFT) | XAT0_ARCHIVE)
#define	XAT_SYSTEM		((XAT0_INDEX << XVA_SHFT) | XAT0_SYSTEM)
#define	XAT_READONLY		((XAT0_INDEX << XVA_SHFT) | XAT0_READONLY)
#define	XAT_HIDDEN		((XAT0_INDEX << XVA_SHFT) | XAT0_HIDDEN)
#define	XAT_NOUNLINK		((XAT0_INDEX << XVA_SHFT) | XAT0_NOUNLINK)
#define	XAT_IMMUTABLE		((XAT0_INDEX << XVA_SHFT) | XAT0_IMMUTABLE)
#define	XAT_APPENDONLY		((XAT0_INDEX << XVA_SHFT) | XAT0_APPENDONLY)
#define	XAT_NODUMP		((XAT0_INDEX << XVA_SHFT) | XAT0_NODUMP)
#define	XAT_OPAQUE		((XAT0_INDEX << XVA_SHFT) | XAT0_OPAQUE)
#define	XAT_AV_QUARANTINED	((XAT0_INDEX << XVA_SHFT) | XAT0_AV_QUARANTINED)
#define	XAT_AV_MODIFIED		((XAT0_INDEX << XVA_SHFT) | XAT0_AV_MODIFIED)
#define	XAT_AV_SCANSTAMP	((XAT0_INDEX << XVA_SHFT) | XAT0_AV_SCANSTAMP)
#define	XAT_REPARSE		((XAT0_INDEX << XVA_SHFT) | XAT0_REPARSE)
#define	XAT_GEN			((XAT0_INDEX << XVA_SHFT) | XAT0_GEN)
#define	XAT_OFFLINE		((XAT0_INDEX << XVA_SHFT) | XAT0_OFFLINE)
#define	XAT_SPARSE		((XAT0_INDEX << XVA_SHFT) | XAT0_SPARSE)

/*
 * The returned attribute map array (xva_rtnattrmap[]) is located past the
 * requested attribute map array (xva_reqattrmap[]).  Its location changes
 * when the array sizes change.  We use a separate pointer in a known location
 * (xva_rtnattrmapp) to hold the location of xva_rtnattrmap[].  This is
 * set in xva_init()
 */
#define	XVA_RTNATTRMAP(xvap)	((xvap)->xva_rtnattrmapp)

/*
 * XVA_SET_REQ() sets an attribute bit in the proper element in the bitmap
 * of requested attributes (xva_reqattrmap[]).
 */
#define	XVA_SET_REQ(xvap, attr)					\
	ASSERT((xvap)->xva_vattr.va_mask | AT_XVATTR);		\
	ASSERT((xvap)->xva_magic == XVA_MAGIC);			\
	(xvap)->xva_reqattrmap[XVA_INDEX(attr)] |= XVA_ATTRBIT(attr)
/*
 * XVA_CLR_REQ() clears an attribute bit in the proper element in the bitmap
 * of requested attributes (xva_reqattrmap[]).
 */
#define	XVA_CLR_REQ(xvap, attr)					\
	ASSERT((xvap)->xva_vattr.va_mask | AT_XVATTR);		\
	ASSERT((xvap)->xva_magic == XVA_MAGIC);			\
	(xvap)->xva_reqattrmap[XVA_INDEX(attr)] &= ~XVA_ATTRBIT(attr)

/*
 * XVA_SET_RTN() sets an attribute bit in the proper element in the bitmap
 * of returned attributes (xva_rtnattrmap[]).
 */
#define	XVA_SET_RTN(xvap, attr)					\
	ASSERT((xvap)->xva_vattr.va_mask | AT_XVATTR);		\
	ASSERT((xvap)->xva_magic == XVA_MAGIC);			\
	(XVA_RTNATTRMAP(xvap))[XVA_INDEX(attr)] |= XVA_ATTRBIT(attr)

/*
 * XVA_ISSET_REQ() checks the requested attribute bitmap (xva_reqattrmap[])
 * to see of the corresponding attribute bit is set.  If so, returns non-zero.
 */
#define	XVA_ISSET_REQ(xvap, attr)					\
	((((xvap)->xva_vattr.va_mask | AT_XVATTR) &&			\
		((xvap)->xva_magic == XVA_MAGIC) &&			\
		((xvap)->xva_mapsize > XVA_INDEX(attr))) ?		\
	((xvap)->xva_reqattrmap[XVA_INDEX(attr)] & XVA_ATTRBIT(attr)) :	0)

/*
 * XVA_ISSET_RTN() checks the returned attribute bitmap (xva_rtnattrmap[])
 * to see of the corresponding attribute bit is set.  If so, returns non-zero.
 */
#define	XVA_ISSET_RTN(xvap, attr)					\
	((((xvap)->xva_vattr.va_mask | AT_XVATTR) &&			\
		((xvap)->xva_magic == XVA_MAGIC) &&			\
		((xvap)->xva_mapsize > XVA_INDEX(attr))) ?		\
	((XVA_RTNATTRMAP(xvap))[XVA_INDEX(attr)] & XVA_ATTRBIT(attr)) : 0)

/*
 *  Modes.  Some values same as S_xxx entries from stat.h for convenience.
 */
#define	VSUID		04000		/* set user id on execution */
#define	VSGID		02000		/* set group id on execution */
#define	VSVTX		01000		/* save swapped text even after use */

/*
 * Permissions.
 */
#define	VREAD		00400
#define	VWRITE		00200
#define	VEXEC		00100

#define	MODEMASK	07777		/* mode bits plus permission bits */
#define	PERMMASK	00777		/* permission bits */

/*
 * VOP_ACCESS flags
 */
#define	V_ACE_MASK	0x1	/* mask represents  NFSv4 ACE permissions */
#define	V_APPEND	0x2	/* want to do append only check */

/*
 * Check whether mandatory file locking is enabled.
 */

#define	MANDMODE(mode)		(((mode) & (VSGID|(VEXEC>>3))) == VSGID)
#define	MANDLOCK(vp, mode)	((vp)->v_type == VREG && MANDMODE(mode))

/*
 * Flags for vnode operations.
 */
enum rm		{ RMFILE, RMDIRECTORY };	/* rm or rmdir (remove) */
enum symfollow	{ NO_FOLLOW, FOLLOW };		/* follow symlinks (or not) */
enum vcexcl	{ NONEXCL, EXCL };		/* (non)excl create */
enum create	{ CRCREAT, CRMKNOD, CRMKDIR };	/* reason for create */

typedef enum rm		rm_t;
typedef enum symfollow	symfollow_t;
typedef enum vcexcl	vcexcl_t;
typedef enum create	create_t;

/*
 * Vnode Events - Used by VOP_VNEVENT
 * The VE_PRE_RENAME_* events fire before the rename operation and are
 * primarily used for specialized applications, such as NFSv4 delegation, which
 * need to know about rename before it occurs.
 */
typedef enum vnevent	{
	VE_SUPPORT	= 0,	/* Query */
	VE_RENAME_SRC	= 1,	/* Rename, with vnode as source */
	VE_RENAME_DEST	= 2,	/* Rename, with vnode as target/destination */
	VE_REMOVE	= 3,	/* Remove of vnode's name */
	VE_RMDIR	= 4,	/* Remove of directory vnode's name */
	VE_CREATE	= 5,	/* Create with vnode's name which exists */
	VE_LINK		= 6, 	/* Link with vnode's name as source */
	VE_RENAME_DEST_DIR	= 7, 	/* Rename with vnode as target dir */
	VE_MOUNTEDOVER	= 8, 	/* File or Filesystem got mounted over vnode */
	VE_TRUNCATE = 9,	/* Truncate */
	VE_PRE_RENAME_SRC = 10,	/* Pre-rename, with vnode as source */
	VE_PRE_RENAME_DEST = 11, /* Pre-rename, with vnode as target/dest. */
	VE_PRE_RENAME_DEST_DIR = 12 /* Pre-rename with vnode as target dir */
} vnevent_t;

/*
 * Values for checking vnode open and map counts
 */
enum v_mode { V_READ, V_WRITE, V_RDORWR, V_RDANDWR };

typedef enum v_mode v_mode_t;

#define	V_TRUE	1
#define	V_FALSE	0

/*
 * Structure used on VOP_GETSECATTR and VOP_SETSECATTR operations
 */

typedef struct vsecattr {
	uint_t		vsa_mask;	/* See below */
	int		vsa_aclcnt;	/* ACL entry count */
	void		*vsa_aclentp;	/* pointer to ACL entries */
	int		vsa_dfaclcnt;	/* default ACL entry count */
	void		*vsa_dfaclentp;	/* pointer to default ACL entries */
	size_t		vsa_aclentsz;	/* ACE size in bytes of vsa_aclentp */
	uint_t		vsa_aclflags;	/* ACE ACL flags */
} vsecattr_t;

/* vsa_mask values */
#define	VSA_ACL			0x0001
#define	VSA_ACLCNT		0x0002
#define	VSA_DFACL		0x0004
#define	VSA_DFACLCNT		0x0008
#define	VSA_ACE			0x0010
#define	VSA_ACECNT		0x0020
#define	VSA_ACE_ALLTYPES	0x0040
#define	VSA_ACE_ACLFLAGS	0x0080	/* get/set ACE ACL flags */

/*
 * Structure used by various vnode operations to determine
 * the context (pid, host, identity) of a caller.
 *
 * The cc_caller_id is used to identify one or more callers who invoke
 * operations, possibly on behalf of others.  For example, the NFS
 * server could have it's own cc_caller_id which can be detected by
 * vnode/vfs operations or (FEM) monitors on those operations.  New
 * caller IDs are generated by fs_new_caller_id().
 */
typedef struct caller_context {
	pid_t		cc_pid;		/* Process ID of the caller */
	int		cc_sysid;	/* System ID, used for remote calls */
	u_longlong_t	cc_caller_id;	/* Identifier for (set of) caller(s) */
	ulong_t		cc_flags;
} caller_context_t;

/*
 * Flags for caller context.  The caller sets CC_DONTBLOCK if it does not
 * want to block inside of a FEM monitor.  The monitor will set CC_WOULDBLOCK
 * and return EAGAIN if the operation would have blocked.
 */
#define	CC_WOULDBLOCK	0x01
#define	CC_DONTBLOCK	0x02

/*
 * Structure tags for function prototypes, defined elsewhere.
 */
struct pathname;
struct fid;
struct flock64;
struct flk_callback;
struct shrlock;
struct page;
struct seg;
struct as;
struct pollhead;
struct taskq;

#ifdef	_KERNEL

/*
 * VNODE_OPS defines all the vnode operations.  It is used to define
 * the vnodeops structure (below) and the fs_func_p union (vfs_opreg.h).
 */
#define	VNODE_OPS							\
	int	(*vop_open)(vnode_t **, int, cred_t *,			\
				caller_context_t *);			\
	int	(*vop_close)(vnode_t *, int, int, offset_t, cred_t *,	\
				caller_context_t *);			\
	int	(*vop_read)(vnode_t *, uio_t *, int, cred_t *,		\
				caller_context_t *);			\
	int	(*vop_write)(vnode_t *, uio_t *, int, cred_t *,		\
				caller_context_t *);			\
	int	(*vop_ioctl)(vnode_t *, int, intptr_t, int, cred_t *,	\
				int *, caller_context_t *);		\
	int	(*vop_setfl)(vnode_t *, int, int, cred_t *,		\
				caller_context_t *);			\
	int	(*vop_getattr)(vnode_t *, vattr_t *, int, cred_t *,	\
				caller_context_t *);			\
	int	(*vop_setattr)(vnode_t *, vattr_t *, int, cred_t *,	\
				caller_context_t *);			\
	int	(*vop_access)(vnode_t *, int, int, cred_t *,		\
				caller_context_t *);			\
	int	(*vop_lookup)(vnode_t *, char *, vnode_t **,		\
				struct pathname *,			\
				int, vnode_t *, cred_t *,		\
				caller_context_t *, int *,		\
				struct pathname *);			\
	int	(*vop_create)(vnode_t *, char *, vattr_t *, vcexcl_t,	\
				int, vnode_t **, cred_t *, int,		\
				caller_context_t *, vsecattr_t *);	\
	int	(*vop_remove)(vnode_t *, char *, cred_t *,		\
				caller_context_t *, int);		\
	int	(*vop_link)(vnode_t *, vnode_t *, char *, cred_t *,	\
				caller_context_t *, int);		\
	int	(*vop_rename)(vnode_t *, char *, vnode_t *, char *,	\
				cred_t *, caller_context_t *, int);	\
	int	(*vop_mkdir)(vnode_t *, char *, vattr_t *, vnode_t **,	\
				cred_t *, caller_context_t *, int,	\
				vsecattr_t *);				\
	int	(*vop_rmdir)(vnode_t *, char *, vnode_t *, cred_t *,	\
				caller_context_t *, int);		\
	int	(*vop_readdir)(vnode_t *, uio_t *, cred_t *, int *,	\
				caller_context_t *, int);		\
	int	(*vop_symlink)(vnode_t *, char *, vattr_t *, char *,	\
				cred_t *, caller_context_t *, int);	\
	int	(*vop_readlink)(vnode_t *, uio_t *, cred_t *,		\
				caller_context_t *);			\
	int	(*vop_fsync)(vnode_t *, int, cred_t *,			\
				caller_context_t *);			\
	void	(*vop_inactive)(vnode_t *, cred_t *,			\
				caller_context_t *);			\
	int	(*vop_fid)(vnode_t *, struct fid *,			\
				caller_context_t *);			\
	int	(*vop_rwlock)(vnode_t *, int, caller_context_t *);	\
	void	(*vop_rwunlock)(vnode_t *, int, caller_context_t *);	\
	int	(*vop_seek)(vnode_t *, offset_t, offset_t *,		\
				caller_context_t *);			\
	int	(*vop_cmp)(vnode_t *, vnode_t *, caller_context_t *);	\
	int	(*vop_frlock)(vnode_t *, int, struct flock64 *,		\
				int, offset_t,				\
				struct flk_callback *, cred_t *,	\
				caller_context_t *);			\
	int	(*vop_space)(vnode_t *, int, struct flock64 *,		\
				int, offset_t,				\
				cred_t *, caller_context_t *);		\
	int	(*vop_realvp)(vnode_t *, vnode_t **,			\
				caller_context_t *);			\
	int	(*vop_getpage)(vnode_t *, offset_t, size_t, uint_t *,	\
				struct page **, size_t, struct seg *,	\
				caddr_t, enum seg_rw, cred_t *,		\
				caller_context_t *);			\
	int	(*vop_putpage)(vnode_t *, offset_t, size_t,		\
				int, cred_t *, caller_context_t *);	\
	int	(*vop_map)(vnode_t *, offset_t, struct as *,		\
				caddr_t *, size_t,			\
				uchar_t, uchar_t, uint_t, cred_t *,	\
				caller_context_t *);			\
	int	(*vop_addmap)(vnode_t *, offset_t, struct as *,		\
				caddr_t, size_t,			\
				uchar_t, uchar_t, uint_t, cred_t *,	\
				caller_context_t *);			\
	int	(*vop_delmap)(vnode_t *, offset_t, struct as *,		\
				caddr_t, size_t,			\
				uint_t, uint_t, uint_t, cred_t *,	\
				caller_context_t *);			\
	int	(*vop_poll)(vnode_t *, short, int, short *,		\
				struct pollhead **,			\
				caller_context_t *);			\
	int	(*vop_dump)(vnode_t *, caddr_t, offset_t, offset_t,	\
				caller_context_t *);			\
	int	(*vop_pathconf)(vnode_t *, int, ulong_t *, cred_t *,	\
				caller_context_t *);			\
	int	(*vop_pageio)(vnode_t *, struct page *,			\
				u_offset_t, size_t, int, cred_t *,	\
				caller_context_t *);			\
	int	(*vop_dumpctl)(vnode_t *, int, offset_t *,		\
				caller_context_t *);			\
	void	(*vop_dispose)(vnode_t *, struct page *,		\
				int, int, cred_t *,			\
				caller_context_t *);			\
	int	(*vop_setsecattr)(vnode_t *, vsecattr_t *,		\
				int, cred_t *, caller_context_t *);	\
	int	(*vop_getsecattr)(vnode_t *, vsecattr_t *,		\
				int, cred_t *, caller_context_t *);	\
	int	(*vop_shrlock)(vnode_t *, int, struct shrlock *,	\
				int, cred_t *, caller_context_t *);	\
	int	(*vop_vnevent)(vnode_t *, vnevent_t, vnode_t *,		\
				char *, caller_context_t *);		\
	int	(*vop_reqzcbuf)(vnode_t *, enum uio_rw, xuio_t *,	\
				cred_t *, caller_context_t *);		\
	int	(*vop_retzcbuf)(vnode_t *, xuio_t *, cred_t *,		\
				caller_context_t *)
	/* NB: No ";" */

/*
 * Operations on vnodes.  Note: File systems must never operate directly
 * on a 'vnodeops' structure -- it WILL change in future releases!  They
 * must use vn_make_ops() to create the structure.
 */
typedef struct vnodeops {
	const char *vnop_name;
	VNODE_OPS;	/* Signatures of all vnode operations (vops) */
} vnodeops_t;

typedef int (*fs_generic_func_p) ();	/* Generic vop/vfsop/femop/fsemop ptr */

extern int	fop_open(vnode_t **, int, cred_t *, caller_context_t *);
extern int	fop_close(vnode_t *, int, int, offset_t, cred_t *,
				caller_context_t *);
extern int	fop_read(vnode_t *, uio_t *, int, cred_t *, caller_context_t *);
extern int	fop_write(vnode_t *, uio_t *, int, cred_t *,
				caller_context_t *);
extern int	fop_ioctl(vnode_t *, int, intptr_t, int, cred_t *, int *,
				caller_context_t *);
extern int	fop_setfl(vnode_t *, int, int, cred_t *, caller_context_t *);
extern int	fop_getattr(vnode_t *, vattr_t *, int, cred_t *,
				caller_context_t *);
extern int	fop_setattr(vnode_t *, vattr_t *, int, cred_t *,
				caller_context_t *);
extern int	fop_access(vnode_t *, int, int, cred_t *, caller_context_t *);
extern int	fop_lookup(vnode_t *, char *, vnode_t **, struct pathname *,
				int, vnode_t *, cred_t *, caller_context_t *,
				int *, struct pathname *);
extern int	fop_create(vnode_t *, char *, vattr_t *, vcexcl_t, int,
				vnode_t **, cred_t *, int, caller_context_t *,
				vsecattr_t *);
extern int	fop_remove(vnode_t *vp, char *, cred_t *, caller_context_t *,
				int);
extern int	fop_link(vnode_t *, vnode_t *, char *, cred_t *,
				caller_context_t *, int);
extern int	fop_rename(vnode_t *, char *, vnode_t *, char *, cred_t *,
				caller_context_t *, int);
extern int	fop_mkdir(vnode_t *, char *, vattr_t *, vnode_t **, cred_t *,
				caller_context_t *, int, vsecattr_t *);
extern int	fop_rmdir(vnode_t *, char *, vnode_t *, cred_t *,
				caller_context_t *, int);
extern int	fop_readdir(vnode_t *, uio_t *, cred_t *, int *,
				caller_context_t *, int);
extern int	fop_symlink(vnode_t *, char *, vattr_t *, char *, cred_t *,
				caller_context_t *, int);
extern int	fop_readlink(vnode_t *, uio_t *, cred_t *, caller_context_t *);
extern int	fop_fsync(vnode_t *, int, cred_t *, caller_context_t *);
extern void	fop_inactive(vnode_t *, cred_t *, caller_context_t *);
extern int	fop_fid(vnode_t *, struct fid *, caller_context_t *);
extern int	fop_rwlock(vnode_t *, int, caller_context_t *);
extern void	fop_rwunlock(vnode_t *, int, caller_context_t *);
extern int	fop_seek(vnode_t *, offset_t, offset_t *, caller_context_t *);
extern int	fop_cmp(vnode_t *, vnode_t *, caller_context_t *);
extern int	fop_frlock(vnode_t *, int, struct flock64 *, int, offset_t,
				struct flk_callback *, cred_t *,
				caller_context_t *);
extern int	fop_space(vnode_t *, int, struct flock64 *, int, offset_t,
				cred_t *, caller_context_t *);
extern int	fop_realvp(vnode_t *, vnode_t **, caller_context_t *);
extern int	fop_getpage(vnode_t *, offset_t, size_t, uint_t *,
				struct page **, size_t, struct seg *,
				caddr_t, enum seg_rw, cred_t *,
				caller_context_t *);
extern int	fop_putpage(vnode_t *, offset_t, size_t, int, cred_t *,
				caller_context_t *);
extern int	fop_map(vnode_t *, offset_t, struct as *, caddr_t *, size_t,
				uchar_t, uchar_t, uint_t, cred_t *cr,
				caller_context_t *);
extern int	fop_addmap(vnode_t *, offset_t, struct as *, caddr_t, size_t,
				uchar_t, uchar_t, uint_t, cred_t *,
				caller_context_t *);
extern int	fop_delmap(vnode_t *, offset_t, struct as *, caddr_t, size_t,
				uint_t, uint_t, uint_t, cred_t *,
				caller_context_t *);
extern int	fop_poll(vnode_t *, short, int, short *, struct pollhead **,
				caller_context_t *);
extern int	fop_dump(vnode_t *, caddr_t, offset_t, offset_t,
    caller_context_t *);
extern int	fop_pathconf(vnode_t *, int, ulong_t *, cred_t *,
				caller_context_t *);
extern int	fop_pageio(vnode_t *, struct page *, u_offset_t, size_t, int,
				cred_t *, caller_context_t *);
extern int	fop_dumpctl(vnode_t *, int, offset_t *, caller_context_t *);
extern void	fop_dispose(vnode_t *, struct page *, int, int, cred_t *,
				caller_context_t *);
extern int	fop_setsecattr(vnode_t *, vsecattr_t *, int, cred_t *,
				caller_context_t *);
extern int	fop_getsecattr(vnode_t *, vsecattr_t *, int, cred_t *,
				caller_context_t *);
extern int	fop_shrlock(vnode_t *, int, struct shrlock *, int, cred_t *,
				caller_context_t *);
extern int	fop_vnevent(vnode_t *, vnevent_t, vnode_t *, char *,
				caller_context_t *);
extern int	fop_reqzcbuf(vnode_t *, enum uio_rw, xuio_t *, cred_t *,
				caller_context_t *);
extern int	fop_retzcbuf(vnode_t *, xuio_t *, cred_t *, caller_context_t *);

#endif	/* _KERNEL */

#define	VOP_OPEN(vpp, mode, cr, ct) \
	fop_open(vpp, mode, cr, ct)
#define	VOP_CLOSE(vp, f, c, o, cr, ct) \
	fop_close(vp, f, c, o, cr, ct)
#define	VOP_READ(vp, uiop, iof, cr, ct) \
	fop_read(vp, uiop, iof, cr, ct)
#define	VOP_WRITE(vp, uiop, iof, cr, ct) \
	fop_write(vp, uiop, iof, cr, ct)
#define	VOP_IOCTL(vp, cmd, a, f, cr, rvp, ct) \
	fop_ioctl(vp, cmd, a, f, cr, rvp, ct)
#define	VOP_SETFL(vp, f, a, cr, ct) \
	fop_setfl(vp, f, a, cr, ct)
#define	VOP_GETATTR(vp, vap, f, cr, ct) \
	fop_getattr(vp, vap, f, cr, ct)
#define	VOP_SETATTR(vp, vap, f, cr, ct) \
	fop_setattr(vp, vap, f, cr, ct)
#define	VOP_ACCESS(vp, mode, f, cr, ct) \
	fop_access(vp, mode, f, cr, ct)
#define	VOP_LOOKUP(vp, cp, vpp, pnp, f, rdir, cr, ct, defp, rpnp) \
	fop_lookup(vp, cp, vpp, pnp, f, rdir, cr, ct, defp, rpnp)
#define	VOP_CREATE(dvp, p, vap, ex, mode, vpp, cr, flag, ct, vsap) \
	fop_create(dvp, p, vap, ex, mode, vpp, cr, flag, ct, vsap)
#define	VOP_REMOVE(dvp, p, cr, ct, f) \
	fop_remove(dvp, p, cr, ct, f)
#define	VOP_LINK(tdvp, fvp, p, cr, ct, f) \
	fop_link(tdvp, fvp, p, cr, ct, f)
#define	VOP_RENAME(fvp, fnm, tdvp, tnm, cr, ct, f) \
	fop_rename(fvp, fnm, tdvp, tnm, cr, ct, f)
#define	VOP_MKDIR(dp, p, vap, vpp, cr, ct, f, vsap) \
	fop_mkdir(dp, p, vap, vpp, cr, ct, f, vsap)
#define	VOP_RMDIR(dp, p, cdir, cr, ct, f) \
	fop_rmdir(dp, p, cdir, cr, ct, f)
#define	VOP_READDIR(vp, uiop, cr, eofp, ct, f) \
	fop_readdir(vp, uiop, cr, eofp, ct, f)
#define	VOP_SYMLINK(dvp, lnm, vap, tnm, cr, ct, f) \
	fop_symlink(dvp, lnm, vap, tnm, cr, ct, f)
#define	VOP_READLINK(vp, uiop, cr, ct) \
	fop_readlink(vp, uiop, cr, ct)
#define	VOP_FSYNC(vp, syncflag, cr, ct) \
	fop_fsync(vp, syncflag, cr, ct)
#define	VOP_INACTIVE(vp, cr, ct) \
	fop_inactive(vp, cr, ct)
#define	VOP_FID(vp, fidp, ct) \
	fop_fid(vp, fidp, ct)
#define	VOP_RWLOCK(vp, w, ct) \
	fop_rwlock(vp, w, ct)
#define	VOP_RWUNLOCK(vp, w, ct) \
	fop_rwunlock(vp, w, ct)
#define	VOP_SEEK(vp, ooff, noffp, ct) \
	fop_seek(vp, ooff, noffp, ct)
#define	VOP_CMP(vp1, vp2, ct) \
	fop_cmp(vp1, vp2, ct)
#define	VOP_FRLOCK(vp, cmd, a, f, o, cb, cr, ct) \
	fop_frlock(vp, cmd, a, f, o, cb, cr, ct)
#define	VOP_SPACE(vp, cmd, a, f, o, cr, ct) \
	fop_space(vp, cmd, a, f, o, cr, ct)
#define	VOP_REALVP(vp1, vp2, ct) \
	fop_realvp(vp1, vp2, ct)
#define	VOP_GETPAGE(vp, of, sz, pr, pl, ps, sg, a, rw, cr, ct) \
	fop_getpage(vp, of, sz, pr, pl, ps, sg, a, rw, cr, ct)
#define	VOP_PUTPAGE(vp, of, sz, fl, cr, ct) \
	fop_putpage(vp, of, sz, fl, cr, ct)
#define	VOP_MAP(vp, of, as, a, sz, p, mp, fl, cr, ct) \
	fop_map(vp, of, as, a, sz, p, mp, fl, cr, ct)
#define	VOP_ADDMAP(vp, of, as, a, sz, p, mp, fl, cr, ct) \
	fop_addmap(vp, of, as, a, sz, p, mp, fl, cr, ct)
#define	VOP_DELMAP(vp, of, as, a, sz, p, mp, fl, cr, ct) \
	fop_delmap(vp, of, as, a, sz, p, mp, fl, cr, ct)
#define	VOP_POLL(vp, events, anyyet, reventsp, phpp, ct) \
	fop_poll(vp, events, anyyet, reventsp, phpp, ct)
#define	VOP_DUMP(vp, addr, bn, count, ct) \
	fop_dump(vp, addr, bn, count, ct)
#define	VOP_PATHCONF(vp, cmd, valp, cr, ct) \
	fop_pathconf(vp, cmd, valp, cr, ct)
#define	VOP_PAGEIO(vp, pp, io_off, io_len, flags, cr, ct) \
	fop_pageio(vp, pp, io_off, io_len, flags, cr, ct)
#define	VOP_DUMPCTL(vp, action, blkp, ct) \
	fop_dumpctl(vp, action, blkp, ct)
#define	VOP_DISPOSE(vp, pp, flag, dn, cr, ct) \
	fop_dispose(vp, pp, flag, dn, cr, ct)
#define	VOP_GETSECATTR(vp, vsap, f, cr, ct) \
	fop_getsecattr(vp, vsap, f, cr, ct)
#define	VOP_SETSECATTR(vp, vsap, f, cr, ct) \
	fop_setsecattr(vp, vsap, f, cr, ct)
#define	VOP_SHRLOCK(vp, cmd, shr, f, cr, ct) \
	fop_shrlock(vp, cmd, shr, f, cr, ct)
#define	VOP_VNEVENT(vp, vnevent, dvp, fnm, ct) \
	fop_vnevent(vp, vnevent, dvp, fnm, ct)
#define	VOP_REQZCBUF(vp, rwflag, xuiop, cr, ct) \
	fop_reqzcbuf(vp, rwflag, xuiop, cr, ct)
#define	VOP_RETZCBUF(vp, xuiop, cr, ct) \
	fop_retzcbuf(vp, xuiop, cr, ct)

#define	VOPNAME_OPEN		"open"
#define	VOPNAME_CLOSE		"close"
#define	VOPNAME_READ		"read"
#define	VOPNAME_WRITE		"write"
#define	VOPNAME_IOCTL		"ioctl"
#define	VOPNAME_SETFL		"setfl"
#define	VOPNAME_GETATTR		"getattr"
#define	VOPNAME_SETATTR		"setattr"
#define	VOPNAME_ACCESS		"access"
#define	VOPNAME_LOOKUP		"lookup"
#define	VOPNAME_CREATE		"create"
#define	VOPNAME_REMOVE		"remove"
#define	VOPNAME_LINK		"link"
#define	VOPNAME_RENAME		"rename"
#define	VOPNAME_MKDIR		"mkdir"
#define	VOPNAME_RMDIR		"rmdir"
#define	VOPNAME_READDIR		"readdir"
#define	VOPNAME_SYMLINK		"symlink"
#define	VOPNAME_READLINK	"readlink"
#define	VOPNAME_FSYNC		"fsync"
#define	VOPNAME_INACTIVE	"inactive"
#define	VOPNAME_FID		"fid"
#define	VOPNAME_RWLOCK		"rwlock"
#define	VOPNAME_RWUNLOCK	"rwunlock"
#define	VOPNAME_SEEK		"seek"
#define	VOPNAME_CMP		"cmp"
#define	VOPNAME_FRLOCK		"frlock"
#define	VOPNAME_SPACE		"space"
#define	VOPNAME_REALVP		"realvp"
#define	VOPNAME_GETPAGE		"getpage"
#define	VOPNAME_PUTPAGE		"putpage"
#define	VOPNAME_MAP		"map"
#define	VOPNAME_ADDMAP		"addmap"
#define	VOPNAME_DELMAP		"delmap"
#define	VOPNAME_POLL		"poll"
#define	VOPNAME_DUMP		"dump"
#define	VOPNAME_PATHCONF	"pathconf"
#define	VOPNAME_PAGEIO		"pageio"
#define	VOPNAME_DUMPCTL		"dumpctl"
#define	VOPNAME_DISPOSE		"dispose"
#define	VOPNAME_GETSECATTR	"getsecattr"
#define	VOPNAME_SETSECATTR	"setsecattr"
#define	VOPNAME_SHRLOCK		"shrlock"
#define	VOPNAME_VNEVENT		"vnevent"
#define	VOPNAME_REQZCBUF	"reqzcbuf"
#define	VOPNAME_RETZCBUF	"retzcbuf"

/*
 * Flags for VOP_LOOKUP
 *
 * Defined in file.h, but also possible, FIGNORECASE and FSEARCH
 *
 */
#define	LOOKUP_DIR		0x01	/* want parent dir vp */
#define	LOOKUP_XATTR		0x02	/* lookup up extended attr dir */
#define	CREATE_XATTR_DIR	0x04	/* Create extended attr dir */
#define	LOOKUP_HAVE_SYSATTR_DIR	0x08	/* Already created virtual GFS dir */

/*
 * Flags for VOP_READDIR
 */
#define	V_RDDIR_ENTFLAGS	0x01	/* request dirent flags */
#define	V_RDDIR_ACCFILTER	0x02	/* filter out inaccessible dirents */

/*
 * Flags for VOP_RWLOCK/VOP_RWUNLOCK
 * VOP_RWLOCK will return the flag that was actually set, or -1 if none.
 */
#define	V_WRITELOCK_TRUE	(1)	/* Request write-lock on the vnode */
#define	V_WRITELOCK_FALSE	(0)	/* Request read-lock on the vnode */

/*
 * Flags for VOP_DUMPCTL
 */
#define	DUMP_ALLOC	0
#define	DUMP_FREE	1
#define	DUMP_SCAN	2

/*
 * Public vnode manipulation functions.
 */
#ifdef	_KERNEL

vnode_t *vn_alloc(int);
void	vn_reinit(vnode_t *);
void	vn_recycle(vnode_t *);
void	vn_free(vnode_t *);

int	vn_is_readonly(vnode_t *);
int   	vn_is_opened(vnode_t *, v_mode_t);
int   	vn_is_mapped(vnode_t *, v_mode_t);
int   	vn_has_other_opens(vnode_t *, v_mode_t);
void	vn_open_upgrade(vnode_t *, int);
void	vn_open_downgrade(vnode_t *, int);

int	vn_can_change_zones(vnode_t *vp);

int	vn_has_flocks(vnode_t *);
int	vn_has_mandatory_locks(vnode_t *, int);
int	vn_has_cached_data(vnode_t *);

void	vn_setops(vnode_t *, vnodeops_t *);
vnodeops_t *vn_getops(vnode_t *);
int	vn_matchops(vnode_t *, vnodeops_t *);
int	vn_matchopval(vnode_t *, char *, fs_generic_func_p);
int	vn_ismntpt(vnode_t *);

struct vfs *vn_mountedvfs(vnode_t *);

int	vn_in_dnlc(vnode_t *);

void	vn_create_cache(void);
void	vn_destroy_cache(void);

void	vn_freevnodeops(vnodeops_t *);

int	vn_open(char *pnamep, enum uio_seg seg, int filemode, int createmode,
		struct vnode **vpp, enum create crwhy, mode_t umask);
int	vn_openat(char *pnamep, enum uio_seg seg, int filemode, int createmode,
		struct vnode **vpp, enum create crwhy,
		mode_t umask, struct vnode *startvp, int fd);
int	vn_create(char *pnamep, enum uio_seg seg, struct vattr *vap,
		enum vcexcl excl, int mode, struct vnode **vpp,
		enum create why, int flag, mode_t umask);
int	vn_createat(char *pnamep, enum uio_seg seg, struct vattr *vap,
		enum vcexcl excl, int mode, struct vnode **vpp,
		enum create why, int flag, mode_t umask, struct vnode *startvp);
int	vn_rdwr(enum uio_rw rw, struct vnode *vp, caddr_t base, ssize_t len,
		offset_t offset, enum uio_seg seg, int ioflag, rlim64_t ulimit,
		cred_t *cr, ssize_t *residp);
void	vn_rele(struct vnode *vp);
void	vn_rele_async(struct vnode *vp, struct taskq *taskq);
void	vn_rele_dnlc(struct vnode *vp);
void	vn_rele_stream(struct vnode *vp);
int	vn_link(char *from, char *to, enum uio_seg seg);
int	vn_linkat(vnode_t *fstartvp, char *from, enum symfollow follow,
		vnode_t *tstartvp, char *to, enum uio_seg seg);
int	vn_rename(char *from, char *to, enum uio_seg seg);
int	vn_renameat(vnode_t *fdvp, char *fname, vnode_t *tdvp, char *tname,
		enum uio_seg seg);
int	vn_remove(char *fnamep, enum uio_seg seg, enum rm dirflag);
int	vn_removeat(vnode_t *startvp, char *fnamep, enum uio_seg seg,
		enum rm dirflag);
int	vn_compare(vnode_t *vp1, vnode_t *vp2);
int	vn_vfswlock(struct vnode *vp);
int	vn_vfswlock_wait(struct vnode *vp);
int	vn_vfsrlock(struct vnode *vp);
int	vn_vfsrlock_wait(struct vnode *vp);
void	vn_vfsunlock(struct vnode *vp);
int	vn_vfswlock_held(struct vnode *vp);
vnode_t *specvp(struct vnode *vp, dev_t dev, vtype_t type, struct cred *cr);
vnode_t *makespecvp(dev_t dev, vtype_t type);
vn_vfslocks_entry_t *vn_vfslocks_getlock(void *);
void	vn_vfslocks_rele(vn_vfslocks_entry_t *);
boolean_t vn_is_reparse(vnode_t *, cred_t *, caller_context_t *);

void vn_copypath(struct vnode *src, struct vnode *dst);
void vn_setpath_str(struct vnode *vp, const char *str, size_t len);
void vn_setpath(vnode_t *rootvp, struct vnode *startvp, struct vnode *vp,
    const char *path, size_t plen);
void vn_renamepath(vnode_t *dvp, vnode_t *vp, const char *nm, size_t len);

/* Private vnode manipulation functions */
void vn_clearpath(vnode_t *, hrtime_t);
void vn_updatepath(vnode_t *, vnode_t *, const char *);


/* Vnode event notification */
void	vnevent_rename_src(vnode_t *, vnode_t *, char *, caller_context_t *);
void	vnevent_rename_dest(vnode_t *, vnode_t *, char *, caller_context_t *);
void	vnevent_remove(vnode_t *, vnode_t *, char *, caller_context_t *);
void	vnevent_rmdir(vnode_t *, vnode_t *, char *, caller_context_t *);
void	vnevent_create(vnode_t *, caller_context_t *);
void	vnevent_link(vnode_t *, caller_context_t *);
void	vnevent_rename_dest_dir(vnode_t *, caller_context_t *ct);
void	vnevent_mountedover(vnode_t *, caller_context_t *);
void	vnevent_truncate(vnode_t *, caller_context_t *);
int	vnevent_support(vnode_t *, caller_context_t *);
void	vnevent_pre_rename_src(vnode_t *, vnode_t *, char *,
	    caller_context_t *);
void	vnevent_pre_rename_dest(vnode_t *, vnode_t *, char *,
	    caller_context_t *);
void	vnevent_pre_rename_dest_dir(vnode_t *, vnode_t *, char *,
	    caller_context_t *);

/* Vnode specific data */
void vsd_create(uint_t *, void (*)(void *));
void vsd_destroy(uint_t *);
void *vsd_get(vnode_t *, uint_t);
int vsd_set(vnode_t *, uint_t, void *);
void vsd_free(vnode_t *);

/*
 * Extensible vnode attribute (xva) routines:
 * xva_init() initializes an xvattr_t (zero struct, init mapsize, set AT_XATTR)
 * xva_getxoptattr() returns a ponter to the xoptattr_t section of xvattr_t
 */
void		xva_init(xvattr_t *);
xoptattr_t	*xva_getxoptattr(xvattr_t *);	/* Get ptr to xoptattr_t */

void xattr_init(void);		/* Initialize vnodeops for xattrs */

/* GFS tunnel for xattrs */
int xattr_dir_lookup(vnode_t *, vnode_t **, int, cred_t *);

/* Reparse Point */
void reparse_point_init(void);

/* Context identification */
u_longlong_t	fs_new_caller_id();

int	vn_vmpss_usepageio(vnode_t *);

/* Empty v_path placeholder */
extern char *vn_vpath_empty;

/*
 * Needed for use of IS_VMODSORT() in kernel.
 */
extern uint_t pvn_vmodsort_supported;

/*
 * All changes to v_count should be done through VN_HOLD() or VN_RELE(), or
 * one of their variants. This makes it possible to ensure proper locking,
 * and to guarantee that all modifications are accompanied by a firing of
 * the vn-hold or vn-rele SDT DTrace probe.
 *
 * Example DTrace command for tracing vnode references using these probes:
 *
 * dtrace -q -n 'sdt:::vn-hold,sdt:::vn-rele
 * {
 *	this->vp = (vnode_t *)arg0;
 *	printf("%s %s(%p[%s]) %d\n", execname, probename, this->vp,
 *	    this->vp->v_path == NULL ? "NULL" : stringof(this->vp->v_path),
 *	    this->vp->v_count)
 * }'
 */
#define	VN_HOLD_LOCKED(vp) {			\
	ASSERT(mutex_owned(&(vp)->v_lock));	\
	(vp)->v_count++;			\
	DTRACE_PROBE1(vn__hold, vnode_t *, vp);	\
}

#define	VN_HOLD(vp)	{		\
	mutex_enter(&(vp)->v_lock);	\
	VN_HOLD_LOCKED(vp);		\
	mutex_exit(&(vp)->v_lock);	\
}

#define	VN_RELE(vp)	{ \
	vn_rele(vp); \
}

#define	VN_RELE_ASYNC(vp, taskq)	{ \
	vn_rele_async(vp, taskq); \
}

#define	VN_RELE_LOCKED(vp) {			\
	ASSERT(mutex_owned(&(vp)->v_lock));	\
	ASSERT((vp)->v_count >= 1);		\
	(vp)->v_count--;			\
	DTRACE_PROBE1(vn__rele, vnode_t *, vp);	\
}

#define	VN_SET_VFS_TYPE_DEV(vp, vfsp, type, dev)	{ \
	(vp)->v_vfsp = (vfsp); \
	(vp)->v_type = (type); \
	(vp)->v_rdev = (dev); \
}

/*
 * Compare two vnodes for equality.  In general this macro should be used
 * in preference to calling VOP_CMP directly.
 */
#define	VN_CMP(VP1, VP2)	((VP1) == (VP2) ? 1 : 	\
	((VP1) && (VP2) && (vn_getops(VP1) == vn_getops(VP2)) ? \
	VOP_CMP(VP1, VP2, NULL) : 0))

/*
 * Some well-known global vnodes used by the VM system to name pages.
 */
extern struct vnode kvps[];

typedef enum {
	KV_KVP,		/* vnode for all segkmem pages */
	KV_ZVP,		/* vnode for all ZFS pages */
#if defined(__sparc)
	KV_MPVP,	/* vnode for all page_t meta-pages */
	KV_PROMVP,	/* vnode for all PROM pages */
#endif	/* __sparc */
	KV_MAX		/* total number of vnodes in kvps[] */
} kvps_index_t;

#define	VN_ISKAS(vp)	((vp) >= &kvps[0] && (vp) < &kvps[KV_MAX])

#endif	/* _KERNEL */

/*
 * Flags to VOP_SETATTR/VOP_GETATTR.
 */
#define	ATTR_UTIME	0x01	/* non-default utime(2) request */
#define	ATTR_EXEC	0x02	/* invocation from exec(2) */
#define	ATTR_COMM	0x04	/* yield common vp attributes */
#define	ATTR_HINT	0x08	/* information returned will be `hint' */
#define	ATTR_REAL	0x10	/* yield attributes of the real vp */
#define	ATTR_NOACLCHECK	0x20	/* Don't check ACL when checking permissions */
#define	ATTR_TRIGGER	0x40	/* Mount first if vnode is a trigger mount */
/*
 * Generally useful macros.
 */
#define	VBSIZE(vp)	((vp)->v_vfsp->vfs_bsize)

#define	VTOZONE(vp)	((vp)->v_vfsp->vfs_zone)

#define	NULLVP		((struct vnode *)0)
#define	NULLVPP		((struct vnode **)0)

#ifdef	_KERNEL

/*
 * Structure used while handling asynchronous VOP_PUTPAGE operations.
 */
struct async_reqs {
	struct async_reqs *a_next;	/* pointer to next arg struct */
	struct vnode *a_vp;		/* vnode pointer */
	u_offset_t a_off;			/* offset in file */
	uint_t a_len;			/* size of i/o request */
	int a_flags;			/* flags to indicate operation type */
	struct cred *a_cred;		/* cred pointer	*/
	ushort_t a_prealloced;		/* set if struct is pre-allocated */
};

/*
 * VN_DISPOSE() -- given a page pointer, safely invoke VOP_DISPOSE().
 * Note that there is no guarantee that the page passed in will be
 * freed.  If that is required, then a check after calling VN_DISPOSE would
 * be necessary to ensure the page was freed.
 */
#define	VN_DISPOSE(pp, flag, dn, cr)	{ \
	if ((pp)->p_vnode != NULL && !VN_ISKAS((pp)->p_vnode)) \
		VOP_DISPOSE((pp)->p_vnode, (pp), (flag), (dn), (cr), NULL); \
	else if ((flag) == B_FREE) \
		page_free((pp), (dn)); \
	else \
		page_destroy((pp), (dn)); \
	}

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VNODE_H */
