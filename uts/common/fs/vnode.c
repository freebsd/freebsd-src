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
 * Copyright (c) 2013, Joyent, Inc. All rights reserved.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/pathname.h>
#include <sys/vfs.h>
#include <sys/vfs_opreg.h>
#include <sys/vnode.h>
#include <sys/rwstlock.h>
#include <sys/fem.h>
#include <sys/stat.h>
#include <sys/mode.h>
#include <sys/conf.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <c2/audit.h>
#include <sys/acl.h>
#include <sys/nbmlock.h>
#include <sys/fcntl.h>
#include <fs/fs_subr.h>
#include <sys/taskq.h>
#include <fs/fs_reparse.h>

/* Determine if this vnode is a file that is read-only */
#define	ISROFILE(vp)	\
	((vp)->v_type != VCHR && (vp)->v_type != VBLK && \
	    (vp)->v_type != VFIFO && vn_is_readonly(vp))

/* Tunable via /etc/system; used only by admin/install */
int nfs_global_client_only;

/*
 * Array of vopstats_t for per-FS-type vopstats.  This array has the same
 * number of entries as and parallel to the vfssw table.  (Arguably, it could
 * be part of the vfssw table.)  Once it's initialized, it's accessed using
 * the same fstype index that is used to index into the vfssw table.
 */
vopstats_t **vopstats_fstype;

/* vopstats initialization template used for fast initialization via bcopy() */
static vopstats_t *vs_templatep;

/* Kmem cache handle for vsk_anchor_t allocations */
kmem_cache_t *vsk_anchor_cache;

/* file events cleanup routine */
extern void free_fopdata(vnode_t *);

/*
 * Root of AVL tree for the kstats associated with vopstats.  Lock protects
 * updates to vsktat_tree.
 */
avl_tree_t	vskstat_tree;
kmutex_t	vskstat_tree_lock;

/* Global variable which enables/disables the vopstats collection */
int vopstats_enabled = 1;

/*
 * forward declarations for internal vnode specific data (vsd)
 */
static void *vsd_realloc(void *, size_t, size_t);

/*
 * forward declarations for reparse point functions
 */
static int fs_reparse_mark(char *target, vattr_t *vap, xvattr_t *xvattr);

/*
 * VSD -- VNODE SPECIFIC DATA
 * The v_data pointer is typically used by a file system to store a
 * pointer to the file system's private node (e.g. ufs inode, nfs rnode).
 * However, there are times when additional project private data needs
 * to be stored separately from the data (node) pointed to by v_data.
 * This additional data could be stored by the file system itself or
 * by a completely different kernel entity.  VSD provides a way for
 * callers to obtain a key and store a pointer to private data associated
 * with a vnode.
 *
 * Callers are responsible for protecting the vsd by holding v_vsd_lock
 * for calls to vsd_set() and vsd_get().
 */

/*
 * vsd_lock protects:
 *   vsd_nkeys - creation and deletion of vsd keys
 *   vsd_list - insertion and deletion of vsd_node in the vsd_list
 *   vsd_destructor - adding and removing destructors to the list
 */
static kmutex_t		vsd_lock;
static uint_t		vsd_nkeys;	 /* size of destructor array */
/* list of vsd_node's */
static list_t *vsd_list = NULL;
/* per-key destructor funcs */
static void 		(**vsd_destructor)(void *);

/*
 * The following is the common set of actions needed to update the
 * vopstats structure from a vnode op.  Both VOPSTATS_UPDATE() and
 * VOPSTATS_UPDATE_IO() do almost the same thing, except for the
 * recording of the bytes transferred.  Since the code is similar
 * but small, it is nearly a duplicate.  Consequently any changes
 * to one may need to be reflected in the other.
 * Rundown of the variables:
 * vp - Pointer to the vnode
 * counter - Partial name structure member to update in vopstats for counts
 * bytecounter - Partial name structure member to update in vopstats for bytes
 * bytesval - Value to update in vopstats for bytes
 * fstype - Index into vsanchor_fstype[], same as index into vfssw[]
 * vsp - Pointer to vopstats structure (either in vfs or vsanchor_fstype[i])
 */

#define	VOPSTATS_UPDATE(vp, counter) {					\
	vfs_t *vfsp = (vp)->v_vfsp;					\
	if (vfsp && vfsp->vfs_implp &&					\
	    (vfsp->vfs_flag & VFS_STATS) && (vp)->v_type != VBAD) {	\
		vopstats_t *vsp = &vfsp->vfs_vopstats;			\
		uint64_t *stataddr = &(vsp->n##counter.value.ui64);	\
		extern void __dtrace_probe___fsinfo_##counter(vnode_t *, \
		    size_t, uint64_t *);				\
		__dtrace_probe___fsinfo_##counter(vp, 0, stataddr);	\
		(*stataddr)++;						\
		if ((vsp = vfsp->vfs_fstypevsp) != NULL) {		\
			vsp->n##counter.value.ui64++;			\
		}							\
	}								\
}

#define	VOPSTATS_UPDATE_IO(vp, counter, bytecounter, bytesval) {	\
	vfs_t *vfsp = (vp)->v_vfsp;					\
	if (vfsp && vfsp->vfs_implp &&					\
	    (vfsp->vfs_flag & VFS_STATS) && (vp)->v_type != VBAD) {	\
		vopstats_t *vsp = &vfsp->vfs_vopstats;			\
		uint64_t *stataddr = &(vsp->n##counter.value.ui64);	\
		extern void __dtrace_probe___fsinfo_##counter(vnode_t *, \
		    size_t, uint64_t *);				\
		__dtrace_probe___fsinfo_##counter(vp, bytesval, stataddr); \
		(*stataddr)++;						\
		vsp->bytecounter.value.ui64 += bytesval;		\
		if ((vsp = vfsp->vfs_fstypevsp) != NULL) {		\
			vsp->n##counter.value.ui64++;			\
			vsp->bytecounter.value.ui64 += bytesval;	\
		}							\
	}								\
}

/*
 * If the filesystem does not support XIDs map credential
 * If the vfsp is NULL, perhaps we should also map?
 */
#define	VOPXID_MAP_CR(vp, cr)	{					\
	vfs_t *vfsp = (vp)->v_vfsp;					\
	if (vfsp != NULL && (vfsp->vfs_flag & VFS_XID) == 0)		\
		cr = crgetmapped(cr);					\
	}

/*
 * Convert stat(2) formats to vnode types and vice versa.  (Knows about
 * numerical order of S_IFMT and vnode types.)
 */
enum vtype iftovt_tab[] = {
	VNON, VFIFO, VCHR, VNON, VDIR, VNON, VBLK, VNON,
	VREG, VNON, VLNK, VNON, VSOCK, VNON, VNON, VNON
};

ushort_t vttoif_tab[] = {
	0, S_IFREG, S_IFDIR, S_IFBLK, S_IFCHR, S_IFLNK, S_IFIFO,
	S_IFDOOR, 0, S_IFSOCK, S_IFPORT, 0
};

/*
 * The system vnode cache.
 */

kmem_cache_t *vn_cache;


/*
 * Vnode operations vector.
 */

static const fs_operation_trans_def_t vn_ops_table[] = {
	VOPNAME_OPEN, offsetof(struct vnodeops, vop_open),
	    fs_nosys, fs_nosys,

	VOPNAME_CLOSE, offsetof(struct vnodeops, vop_close),
	    fs_nosys, fs_nosys,

	VOPNAME_READ, offsetof(struct vnodeops, vop_read),
	    fs_nosys, fs_nosys,

	VOPNAME_WRITE, offsetof(struct vnodeops, vop_write),
	    fs_nosys, fs_nosys,

	VOPNAME_IOCTL, offsetof(struct vnodeops, vop_ioctl),
	    fs_nosys, fs_nosys,

	VOPNAME_SETFL, offsetof(struct vnodeops, vop_setfl),
	    fs_setfl, fs_nosys,

	VOPNAME_GETATTR, offsetof(struct vnodeops, vop_getattr),
	    fs_nosys, fs_nosys,

	VOPNAME_SETATTR, offsetof(struct vnodeops, vop_setattr),
	    fs_nosys, fs_nosys,

	VOPNAME_ACCESS, offsetof(struct vnodeops, vop_access),
	    fs_nosys, fs_nosys,

	VOPNAME_LOOKUP, offsetof(struct vnodeops, vop_lookup),
	    fs_nosys, fs_nosys,

	VOPNAME_CREATE, offsetof(struct vnodeops, vop_create),
	    fs_nosys, fs_nosys,

	VOPNAME_REMOVE, offsetof(struct vnodeops, vop_remove),
	    fs_nosys, fs_nosys,

	VOPNAME_LINK, offsetof(struct vnodeops, vop_link),
	    fs_nosys, fs_nosys,

	VOPNAME_RENAME, offsetof(struct vnodeops, vop_rename),
	    fs_nosys, fs_nosys,

	VOPNAME_MKDIR, offsetof(struct vnodeops, vop_mkdir),
	    fs_nosys, fs_nosys,

	VOPNAME_RMDIR, offsetof(struct vnodeops, vop_rmdir),
	    fs_nosys, fs_nosys,

	VOPNAME_READDIR, offsetof(struct vnodeops, vop_readdir),
	    fs_nosys, fs_nosys,

	VOPNAME_SYMLINK, offsetof(struct vnodeops, vop_symlink),
	    fs_nosys, fs_nosys,

	VOPNAME_READLINK, offsetof(struct vnodeops, vop_readlink),
	    fs_nosys, fs_nosys,

	VOPNAME_FSYNC, offsetof(struct vnodeops, vop_fsync),
	    fs_nosys, fs_nosys,

	VOPNAME_INACTIVE, offsetof(struct vnodeops, vop_inactive),
	    fs_nosys, fs_nosys,

	VOPNAME_FID, offsetof(struct vnodeops, vop_fid),
	    fs_nosys, fs_nosys,

	VOPNAME_RWLOCK, offsetof(struct vnodeops, vop_rwlock),
	    fs_rwlock, fs_rwlock,

	VOPNAME_RWUNLOCK, offsetof(struct vnodeops, vop_rwunlock),
	    (fs_generic_func_p) fs_rwunlock,
	    (fs_generic_func_p) fs_rwunlock,	/* no errors allowed */

	VOPNAME_SEEK, offsetof(struct vnodeops, vop_seek),
	    fs_nosys, fs_nosys,

	VOPNAME_CMP, offsetof(struct vnodeops, vop_cmp),
	    fs_cmp, fs_cmp,		/* no errors allowed */

	VOPNAME_FRLOCK, offsetof(struct vnodeops, vop_frlock),
	    fs_frlock, fs_nosys,

	VOPNAME_SPACE, offsetof(struct vnodeops, vop_space),
	    fs_nosys, fs_nosys,

	VOPNAME_REALVP, offsetof(struct vnodeops, vop_realvp),
	    fs_nosys, fs_nosys,

	VOPNAME_GETPAGE, offsetof(struct vnodeops, vop_getpage),
	    fs_nosys, fs_nosys,

	VOPNAME_PUTPAGE, offsetof(struct vnodeops, vop_putpage),
	    fs_nosys, fs_nosys,

	VOPNAME_MAP, offsetof(struct vnodeops, vop_map),
	    (fs_generic_func_p) fs_nosys_map,
	    (fs_generic_func_p) fs_nosys_map,

	VOPNAME_ADDMAP, offsetof(struct vnodeops, vop_addmap),
	    (fs_generic_func_p) fs_nosys_addmap,
	    (fs_generic_func_p) fs_nosys_addmap,

	VOPNAME_DELMAP, offsetof(struct vnodeops, vop_delmap),
	    fs_nosys, fs_nosys,

	VOPNAME_POLL, offsetof(struct vnodeops, vop_poll),
	    (fs_generic_func_p) fs_poll, (fs_generic_func_p) fs_nosys_poll,

	VOPNAME_DUMP, offsetof(struct vnodeops, vop_dump),
	    fs_nosys, fs_nosys,

	VOPNAME_PATHCONF, offsetof(struct vnodeops, vop_pathconf),
	    fs_pathconf, fs_nosys,

	VOPNAME_PAGEIO, offsetof(struct vnodeops, vop_pageio),
	    fs_nosys, fs_nosys,

	VOPNAME_DUMPCTL, offsetof(struct vnodeops, vop_dumpctl),
	    fs_nosys, fs_nosys,

	VOPNAME_DISPOSE, offsetof(struct vnodeops, vop_dispose),
	    (fs_generic_func_p) fs_dispose,
	    (fs_generic_func_p) fs_nodispose,

	VOPNAME_SETSECATTR, offsetof(struct vnodeops, vop_setsecattr),
	    fs_nosys, fs_nosys,

	VOPNAME_GETSECATTR, offsetof(struct vnodeops, vop_getsecattr),
	    fs_fab_acl, fs_nosys,

	VOPNAME_SHRLOCK, offsetof(struct vnodeops, vop_shrlock),
	    fs_shrlock, fs_nosys,

	VOPNAME_VNEVENT, offsetof(struct vnodeops, vop_vnevent),
	    (fs_generic_func_p) fs_vnevent_nosupport,
	    (fs_generic_func_p) fs_vnevent_nosupport,

	VOPNAME_REQZCBUF, offsetof(struct vnodeops, vop_reqzcbuf),
	    fs_nosys, fs_nosys,

	VOPNAME_RETZCBUF, offsetof(struct vnodeops, vop_retzcbuf),
	    fs_nosys, fs_nosys,

	NULL, 0, NULL, NULL
};

/* Extensible attribute (xva) routines. */

/*
 * Zero out the structure, set the size of the requested/returned bitmaps,
 * set AT_XVATTR in the embedded vattr_t's va_mask, and set up the pointer
 * to the returned attributes array.
 */
void
xva_init(xvattr_t *xvap)
{
	bzero(xvap, sizeof (xvattr_t));
	xvap->xva_mapsize = XVA_MAPSIZE;
	xvap->xva_magic = XVA_MAGIC;
	xvap->xva_vattr.va_mask = AT_XVATTR;
	xvap->xva_rtnattrmapp = &(xvap->xva_rtnattrmap)[0];
}

/*
 * If AT_XVATTR is set, returns a pointer to the embedded xoptattr_t
 * structure.  Otherwise, returns NULL.
 */
xoptattr_t *
xva_getxoptattr(xvattr_t *xvap)
{
	xoptattr_t *xoap = NULL;
	if (xvap->xva_vattr.va_mask & AT_XVATTR)
		xoap = &xvap->xva_xoptattrs;
	return (xoap);
}

/*
 * Used by the AVL routines to compare two vsk_anchor_t structures in the tree.
 * We use the f_fsid reported by VFS_STATVFS() since we use that for the
 * kstat name.
 */
static int
vska_compar(const void *n1, const void *n2)
{
	int ret;
	ulong_t p1 = ((vsk_anchor_t *)n1)->vsk_fsid;
	ulong_t p2 = ((vsk_anchor_t *)n2)->vsk_fsid;

	if (p1 < p2) {
		ret = -1;
	} else if (p1 > p2) {
		ret = 1;
	} else {
		ret = 0;
	}

	return (ret);
}

/*
 * Used to create a single template which will be bcopy()ed to a newly
 * allocated vsanchor_combo_t structure in new_vsanchor(), below.
 */
static vopstats_t *
create_vopstats_template()
{
	vopstats_t		*vsp;

	vsp = kmem_alloc(sizeof (vopstats_t), KM_SLEEP);
	bzero(vsp, sizeof (*vsp));	/* Start fresh */

	/* VOP_OPEN */
	kstat_named_init(&vsp->nopen, "nopen", KSTAT_DATA_UINT64);
	/* VOP_CLOSE */
	kstat_named_init(&vsp->nclose, "nclose", KSTAT_DATA_UINT64);
	/* VOP_READ I/O */
	kstat_named_init(&vsp->nread, "nread", KSTAT_DATA_UINT64);
	kstat_named_init(&vsp->read_bytes, "read_bytes", KSTAT_DATA_UINT64);
	/* VOP_WRITE I/O */
	kstat_named_init(&vsp->nwrite, "nwrite", KSTAT_DATA_UINT64);
	kstat_named_init(&vsp->write_bytes, "write_bytes", KSTAT_DATA_UINT64);
	/* VOP_IOCTL */
	kstat_named_init(&vsp->nioctl, "nioctl", KSTAT_DATA_UINT64);
	/* VOP_SETFL */
	kstat_named_init(&vsp->nsetfl, "nsetfl", KSTAT_DATA_UINT64);
	/* VOP_GETATTR */
	kstat_named_init(&vsp->ngetattr, "ngetattr", KSTAT_DATA_UINT64);
	/* VOP_SETATTR */
	kstat_named_init(&vsp->nsetattr, "nsetattr", KSTAT_DATA_UINT64);
	/* VOP_ACCESS */
	kstat_named_init(&vsp->naccess, "naccess", KSTAT_DATA_UINT64);
	/* VOP_LOOKUP */
	kstat_named_init(&vsp->nlookup, "nlookup", KSTAT_DATA_UINT64);
	/* VOP_CREATE */
	kstat_named_init(&vsp->ncreate, "ncreate", KSTAT_DATA_UINT64);
	/* VOP_REMOVE */
	kstat_named_init(&vsp->nremove, "nremove", KSTAT_DATA_UINT64);
	/* VOP_LINK */
	kstat_named_init(&vsp->nlink, "nlink", KSTAT_DATA_UINT64);
	/* VOP_RENAME */
	kstat_named_init(&vsp->nrename, "nrename", KSTAT_DATA_UINT64);
	/* VOP_MKDIR */
	kstat_named_init(&vsp->nmkdir, "nmkdir", KSTAT_DATA_UINT64);
	/* VOP_RMDIR */
	kstat_named_init(&vsp->nrmdir, "nrmdir", KSTAT_DATA_UINT64);
	/* VOP_READDIR I/O */
	kstat_named_init(&vsp->nreaddir, "nreaddir", KSTAT_DATA_UINT64);
	kstat_named_init(&vsp->readdir_bytes, "readdir_bytes",
	    KSTAT_DATA_UINT64);
	/* VOP_SYMLINK */
	kstat_named_init(&vsp->nsymlink, "nsymlink", KSTAT_DATA_UINT64);
	/* VOP_READLINK */
	kstat_named_init(&vsp->nreadlink, "nreadlink", KSTAT_DATA_UINT64);
	/* VOP_FSYNC */
	kstat_named_init(&vsp->nfsync, "nfsync", KSTAT_DATA_UINT64);
	/* VOP_INACTIVE */
	kstat_named_init(&vsp->ninactive, "ninactive", KSTAT_DATA_UINT64);
	/* VOP_FID */
	kstat_named_init(&vsp->nfid, "nfid", KSTAT_DATA_UINT64);
	/* VOP_RWLOCK */
	kstat_named_init(&vsp->nrwlock, "nrwlock", KSTAT_DATA_UINT64);
	/* VOP_RWUNLOCK */
	kstat_named_init(&vsp->nrwunlock, "nrwunlock", KSTAT_DATA_UINT64);
	/* VOP_SEEK */
	kstat_named_init(&vsp->nseek, "nseek", KSTAT_DATA_UINT64);
	/* VOP_CMP */
	kstat_named_init(&vsp->ncmp, "ncmp", KSTAT_DATA_UINT64);
	/* VOP_FRLOCK */
	kstat_named_init(&vsp->nfrlock, "nfrlock", KSTAT_DATA_UINT64);
	/* VOP_SPACE */
	kstat_named_init(&vsp->nspace, "nspace", KSTAT_DATA_UINT64);
	/* VOP_REALVP */
	kstat_named_init(&vsp->nrealvp, "nrealvp", KSTAT_DATA_UINT64);
	/* VOP_GETPAGE */
	kstat_named_init(&vsp->ngetpage, "ngetpage", KSTAT_DATA_UINT64);
	/* VOP_PUTPAGE */
	kstat_named_init(&vsp->nputpage, "nputpage", KSTAT_DATA_UINT64);
	/* VOP_MAP */
	kstat_named_init(&vsp->nmap, "nmap", KSTAT_DATA_UINT64);
	/* VOP_ADDMAP */
	kstat_named_init(&vsp->naddmap, "naddmap", KSTAT_DATA_UINT64);
	/* VOP_DELMAP */
	kstat_named_init(&vsp->ndelmap, "ndelmap", KSTAT_DATA_UINT64);
	/* VOP_POLL */
	kstat_named_init(&vsp->npoll, "npoll", KSTAT_DATA_UINT64);
	/* VOP_DUMP */
	kstat_named_init(&vsp->ndump, "ndump", KSTAT_DATA_UINT64);
	/* VOP_PATHCONF */
	kstat_named_init(&vsp->npathconf, "npathconf", KSTAT_DATA_UINT64);
	/* VOP_PAGEIO */
	kstat_named_init(&vsp->npageio, "npageio", KSTAT_DATA_UINT64);
	/* VOP_DUMPCTL */
	kstat_named_init(&vsp->ndumpctl, "ndumpctl", KSTAT_DATA_UINT64);
	/* VOP_DISPOSE */
	kstat_named_init(&vsp->ndispose, "ndispose", KSTAT_DATA_UINT64);
	/* VOP_SETSECATTR */
	kstat_named_init(&vsp->nsetsecattr, "nsetsecattr", KSTAT_DATA_UINT64);
	/* VOP_GETSECATTR */
	kstat_named_init(&vsp->ngetsecattr, "ngetsecattr", KSTAT_DATA_UINT64);
	/* VOP_SHRLOCK */
	kstat_named_init(&vsp->nshrlock, "nshrlock", KSTAT_DATA_UINT64);
	/* VOP_VNEVENT */
	kstat_named_init(&vsp->nvnevent, "nvnevent", KSTAT_DATA_UINT64);
	/* VOP_REQZCBUF */
	kstat_named_init(&vsp->nreqzcbuf, "nreqzcbuf", KSTAT_DATA_UINT64);
	/* VOP_RETZCBUF */
	kstat_named_init(&vsp->nretzcbuf, "nretzcbuf", KSTAT_DATA_UINT64);

	return (vsp);
}

/*
 * Creates a kstat structure associated with a vopstats structure.
 */
kstat_t *
new_vskstat(char *ksname, vopstats_t *vsp)
{
	kstat_t		*ksp;

	if (!vopstats_enabled) {
		return (NULL);
	}

	ksp = kstat_create("unix", 0, ksname, "misc", KSTAT_TYPE_NAMED,
	    sizeof (vopstats_t)/sizeof (kstat_named_t),
	    KSTAT_FLAG_VIRTUAL|KSTAT_FLAG_WRITABLE);
	if (ksp) {
		ksp->ks_data = vsp;
		kstat_install(ksp);
	}

	return (ksp);
}

/*
 * Called from vfsinit() to initialize the support mechanisms for vopstats
 */
void
vopstats_startup()
{
	if (!vopstats_enabled)
		return;

	/*
	 * Creates the AVL tree which holds per-vfs vopstat anchors.  This
	 * is necessary since we need to check if a kstat exists before we
	 * attempt to create it.  Also, initialize its lock.
	 */
	avl_create(&vskstat_tree, vska_compar, sizeof (vsk_anchor_t),
	    offsetof(vsk_anchor_t, vsk_node));
	mutex_init(&vskstat_tree_lock, NULL, MUTEX_DEFAULT, NULL);

	vsk_anchor_cache = kmem_cache_create("vsk_anchor_cache",
	    sizeof (vsk_anchor_t), sizeof (uintptr_t), NULL, NULL, NULL,
	    NULL, NULL, 0);

	/*
	 * Set up the array of pointers for the vopstats-by-FS-type.
	 * The entries will be allocated/initialized as each file system
	 * goes through modload/mod_installfs.
	 */
	vopstats_fstype = (vopstats_t **)kmem_zalloc(
	    (sizeof (vopstats_t *) * nfstype), KM_SLEEP);

	/* Set up the global vopstats initialization template */
	vs_templatep = create_vopstats_template();
}

/*
 * We need to have the all of the counters zeroed.
 * The initialization of the vopstats_t includes on the order of
 * 50 calls to kstat_named_init().  Rather that do that on every call,
 * we do it once in a template (vs_templatep) then bcopy it over.
 */
void
initialize_vopstats(vopstats_t *vsp)
{
	if (vsp == NULL)
		return;

	bcopy(vs_templatep, vsp, sizeof (vopstats_t));
}

/*
 * If possible, determine which vopstats by fstype to use and
 * return a pointer to the caller.
 */
vopstats_t *
get_fstype_vopstats(vfs_t *vfsp, struct vfssw *vswp)
{
	int		fstype = 0;	/* Index into vfssw[] */
	vopstats_t	*vsp = NULL;

	if (vfsp == NULL || (vfsp->vfs_flag & VFS_STATS) == 0 ||
	    !vopstats_enabled)
		return (NULL);
	/*
	 * Set up the fstype.  We go to so much trouble because all versions
	 * of NFS use the same fstype in their vfs even though they have
	 * distinct entries in the vfssw[] table.
	 * NOTE: A special vfs (e.g., EIO_vfs) may not have an entry.
	 */
	if (vswp) {
		fstype = vswp - vfssw;	/* Gets us the index */
	} else {
		fstype = vfsp->vfs_fstype;
	}

	/*
	 * Point to the per-fstype vopstats. The only valid values are
	 * non-zero positive values less than the number of vfssw[] table
	 * entries.
	 */
	if (fstype > 0 && fstype < nfstype) {
		vsp = vopstats_fstype[fstype];
	}

	return (vsp);
}

/*
 * Generate a kstat name, create the kstat structure, and allocate a
 * vsk_anchor_t to hold it together.  Return the pointer to the vsk_anchor_t
 * to the caller.  This must only be called from a mount.
 */
vsk_anchor_t *
get_vskstat_anchor(vfs_t *vfsp)
{
	char		kstatstr[KSTAT_STRLEN]; /* kstat name for vopstats */
	statvfs64_t	statvfsbuf;		/* Needed to find f_fsid */
	vsk_anchor_t	*vskp = NULL;		/* vfs <--> kstat anchor */
	kstat_t		*ksp;			/* Ptr to new kstat */
	avl_index_t	where;			/* Location in the AVL tree */

	if (vfsp == NULL || vfsp->vfs_implp == NULL ||
	    (vfsp->vfs_flag & VFS_STATS) == 0 || !vopstats_enabled)
		return (NULL);

	/* Need to get the fsid to build a kstat name */
	if (VFS_STATVFS(vfsp, &statvfsbuf) == 0) {
		/* Create a name for our kstats based on fsid */
		(void) snprintf(kstatstr, KSTAT_STRLEN, "%s%lx",
		    VOPSTATS_STR, statvfsbuf.f_fsid);

		/* Allocate and initialize the vsk_anchor_t */
		vskp = kmem_cache_alloc(vsk_anchor_cache, KM_SLEEP);
		bzero(vskp, sizeof (*vskp));
		vskp->vsk_fsid = statvfsbuf.f_fsid;

		mutex_enter(&vskstat_tree_lock);
		if (avl_find(&vskstat_tree, vskp, &where) == NULL) {
			avl_insert(&vskstat_tree, vskp, where);
			mutex_exit(&vskstat_tree_lock);

			/*
			 * Now that we've got the anchor in the AVL
			 * tree, we can create the kstat.
			 */
			ksp = new_vskstat(kstatstr, &vfsp->vfs_vopstats);
			if (ksp) {
				vskp->vsk_ksp = ksp;
			}
		} else {
			/* Oops, found one! Release memory and lock. */
			mutex_exit(&vskstat_tree_lock);
			kmem_cache_free(vsk_anchor_cache, vskp);
			vskp = NULL;
		}
	}
	return (vskp);
}

/*
 * We're in the process of tearing down the vfs and need to cleanup
 * the data structures associated with the vopstats. Must only be called
 * from dounmount().
 */
void
teardown_vopstats(vfs_t *vfsp)
{
	vsk_anchor_t	*vskap;
	avl_index_t	where;

	if (vfsp == NULL || vfsp->vfs_implp == NULL ||
	    (vfsp->vfs_flag & VFS_STATS) == 0 || !vopstats_enabled)
		return;

	/* This is a safe check since VFS_STATS must be set (see above) */
	if ((vskap = vfsp->vfs_vskap) == NULL)
		return;

	/* Whack the pointer right away */
	vfsp->vfs_vskap = NULL;

	/* Lock the tree, remove the node, and delete the kstat */
	mutex_enter(&vskstat_tree_lock);
	if (avl_find(&vskstat_tree, vskap, &where)) {
		avl_remove(&vskstat_tree, vskap);
	}

	if (vskap->vsk_ksp) {
		kstat_delete(vskap->vsk_ksp);
	}
	mutex_exit(&vskstat_tree_lock);

	kmem_cache_free(vsk_anchor_cache, vskap);
}

/*
 * Read or write a vnode.  Called from kernel code.
 */
int
vn_rdwr(
	enum uio_rw rw,
	struct vnode *vp,
	caddr_t base,
	ssize_t len,
	offset_t offset,
	enum uio_seg seg,
	int ioflag,
	rlim64_t ulimit,	/* meaningful only if rw is UIO_WRITE */
	cred_t *cr,
	ssize_t *residp)
{
	struct uio uio;
	struct iovec iov;
	int error;
	int in_crit = 0;

	if (rw == UIO_WRITE && ISROFILE(vp))
		return (EROFS);

	if (len < 0)
		return (EIO);

	VOPXID_MAP_CR(vp, cr);

	iov.iov_base = base;
	iov.iov_len = len;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_loffset = offset;
	uio.uio_segflg = (short)seg;
	uio.uio_resid = len;
	uio.uio_llimit = ulimit;

	/*
	 * We have to enter the critical region before calling VOP_RWLOCK
	 * to avoid a deadlock with ufs.
	 */
	if (nbl_need_check(vp)) {
		int svmand;

		nbl_start_crit(vp, RW_READER);
		in_crit = 1;
		error = nbl_svmand(vp, cr, &svmand);
		if (error != 0)
			goto done;
		if (nbl_conflict(vp, rw == UIO_WRITE ? NBL_WRITE : NBL_READ,
		    uio.uio_offset, uio.uio_resid, svmand, NULL)) {
			error = EACCES;
			goto done;
		}
	}

	(void) VOP_RWLOCK(vp,
	    rw == UIO_WRITE ? V_WRITELOCK_TRUE : V_WRITELOCK_FALSE, NULL);
	if (rw == UIO_WRITE) {
		uio.uio_fmode = FWRITE;
		uio.uio_extflg = UIO_COPY_DEFAULT;
		error = VOP_WRITE(vp, &uio, ioflag, cr, NULL);
	} else {
		uio.uio_fmode = FREAD;
		uio.uio_extflg = UIO_COPY_CACHED;
		error = VOP_READ(vp, &uio, ioflag, cr, NULL);
	}
	VOP_RWUNLOCK(vp,
	    rw == UIO_WRITE ? V_WRITELOCK_TRUE : V_WRITELOCK_FALSE, NULL);
	if (residp)
		*residp = uio.uio_resid;
	else if (uio.uio_resid)
		error = EIO;

done:
	if (in_crit)
		nbl_end_crit(vp);
	return (error);
}

/*
 * Release a vnode.  Call VOP_INACTIVE on last reference or
 * decrement reference count.
 *
 * To avoid race conditions, the v_count is left at 1 for
 * the call to VOP_INACTIVE. This prevents another thread
 * from reclaiming and releasing the vnode *before* the
 * VOP_INACTIVE routine has a chance to destroy the vnode.
 * We can't have more than 1 thread calling VOP_INACTIVE
 * on a vnode.
 */
void
vn_rele(vnode_t *vp)
{
	VERIFY(vp->v_count > 0);
	mutex_enter(&vp->v_lock);
	if (vp->v_count == 1) {
		mutex_exit(&vp->v_lock);
		VOP_INACTIVE(vp, CRED(), NULL);
		return;
	}
	vp->v_count--;
	mutex_exit(&vp->v_lock);
}

/*
 * Release a vnode referenced by the DNLC. Multiple DNLC references are treated
 * as a single reference, so v_count is not decremented until the last DNLC hold
 * is released. This makes it possible to distinguish vnodes that are referenced
 * only by the DNLC.
 */
void
vn_rele_dnlc(vnode_t *vp)
{
	VERIFY((vp->v_count > 0) && (vp->v_count_dnlc > 0));
	mutex_enter(&vp->v_lock);
	if (--vp->v_count_dnlc == 0) {
		if (vp->v_count == 1) {
			mutex_exit(&vp->v_lock);
			VOP_INACTIVE(vp, CRED(), NULL);
			return;
		}
		vp->v_count--;
	}
	mutex_exit(&vp->v_lock);
}

/*
 * Like vn_rele() except that it clears v_stream under v_lock.
 * This is used by sockfs when it dismantels the association between
 * the sockfs node and the vnode in the underlaying file system.
 * v_lock has to be held to prevent a thread coming through the lookupname
 * path from accessing a stream head that is going away.
 */
void
vn_rele_stream(vnode_t *vp)
{
	VERIFY(vp->v_count > 0);
	mutex_enter(&vp->v_lock);
	vp->v_stream = NULL;
	if (vp->v_count == 1) {
		mutex_exit(&vp->v_lock);
		VOP_INACTIVE(vp, CRED(), NULL);
		return;
	}
	vp->v_count--;
	mutex_exit(&vp->v_lock);
}

static void
vn_rele_inactive(vnode_t *vp)
{
	VOP_INACTIVE(vp, CRED(), NULL);
}

/*
 * Like vn_rele() except if we are going to call VOP_INACTIVE() then do it
 * asynchronously using a taskq. This can avoid deadlocks caused by re-entering
 * the file system as a result of releasing the vnode. Note, file systems
 * already have to handle the race where the vnode is incremented before the
 * inactive routine is called and does its locking.
 *
 * Warning: Excessive use of this routine can lead to performance problems.
 * This is because taskqs throttle back allocation if too many are created.
 */
void
vn_rele_async(vnode_t *vp, taskq_t *taskq)
{
	VERIFY(vp->v_count > 0);
	mutex_enter(&vp->v_lock);
	if (vp->v_count == 1) {
		mutex_exit(&vp->v_lock);
		VERIFY(taskq_dispatch(taskq, (task_func_t *)vn_rele_inactive,
		    vp, TQ_SLEEP) != NULL);
		return;
	}
	vp->v_count--;
	mutex_exit(&vp->v_lock);
}

int
vn_open(
	char *pnamep,
	enum uio_seg seg,
	int filemode,
	int createmode,
	struct vnode **vpp,
	enum create crwhy,
	mode_t umask)
{
	return (vn_openat(pnamep, seg, filemode, createmode, vpp, crwhy,
	    umask, NULL, -1));
}


/*
 * Open/create a vnode.
 * This may be callable by the kernel, the only known use
 * of user context being that the current user credentials
 * are used for permissions.  crwhy is defined iff filemode & FCREAT.
 */
int
vn_openat(
	char *pnamep,
	enum uio_seg seg,
	int filemode,
	int createmode,
	struct vnode **vpp,
	enum create crwhy,
	mode_t umask,
	struct vnode *startvp,
	int fd)
{
	struct vnode *vp;
	int mode;
	int accessflags;
	int error;
	int in_crit = 0;
	int open_done = 0;
	int shrlock_done = 0;
	struct vattr vattr;
	enum symfollow follow;
	int estale_retry = 0;
	struct shrlock shr;
	struct shr_locowner shr_own;

	mode = 0;
	accessflags = 0;
	if (filemode & FREAD)
		mode |= VREAD;
	if (filemode & (FWRITE|FTRUNC))
		mode |= VWRITE;
	if (filemode & (FSEARCH|FEXEC|FXATTRDIROPEN))
		mode |= VEXEC;

	/* symlink interpretation */
	if (filemode & FNOFOLLOW)
		follow = NO_FOLLOW;
	else
		follow = FOLLOW;

	if (filemode & FAPPEND)
		accessflags |= V_APPEND;

top:
	if (filemode & FCREAT) {
		enum vcexcl excl;

		/*
		 * Wish to create a file.
		 */
		vattr.va_type = VREG;
		vattr.va_mode = createmode;
		vattr.va_mask = AT_TYPE|AT_MODE;
		if (filemode & FTRUNC) {
			vattr.va_size = 0;
			vattr.va_mask |= AT_SIZE;
		}
		if (filemode & FEXCL)
			excl = EXCL;
		else
			excl = NONEXCL;

		if (error =
		    vn_createat(pnamep, seg, &vattr, excl, mode, &vp, crwhy,
		    (filemode & ~(FTRUNC|FEXCL)), umask, startvp))
			return (error);
	} else {
		/*
		 * Wish to open a file.  Just look it up.
		 */
		if (error = lookupnameat(pnamep, seg, follow,
		    NULLVPP, &vp, startvp)) {
			if ((error == ESTALE) &&
			    fs_need_estale_retry(estale_retry++))
				goto top;
			return (error);
		}

		/*
		 * Get the attributes to check whether file is large.
		 * We do this only if the FOFFMAX flag is not set and
		 * only for regular files.
		 */

		if (!(filemode & FOFFMAX) && (vp->v_type == VREG)) {
			vattr.va_mask = AT_SIZE;
			if ((error = VOP_GETATTR(vp, &vattr, 0,
			    CRED(), NULL))) {
				goto out;
			}
			if (vattr.va_size > (u_offset_t)MAXOFF32_T) {
				/*
				 * Large File API - regular open fails
				 * if FOFFMAX flag is set in file mode
				 */
				error = EOVERFLOW;
				goto out;
			}
		}
		/*
		 * Can't write directories, active texts, or
		 * read-only filesystems.  Can't truncate files
		 * on which mandatory locking is in effect.
		 */
		if (filemode & (FWRITE|FTRUNC)) {
			/*
			 * Allow writable directory if VDIROPEN flag is set.
			 */
			if (vp->v_type == VDIR && !(vp->v_flag & VDIROPEN)) {
				error = EISDIR;
				goto out;
			}
			if (ISROFILE(vp)) {
				error = EROFS;
				goto out;
			}
			/*
			 * Can't truncate files on which
			 * sysv mandatory locking is in effect.
			 */
			if (filemode & FTRUNC) {
				vnode_t *rvp;

				if (VOP_REALVP(vp, &rvp, NULL) != 0)
					rvp = vp;
				if (rvp->v_filocks != NULL) {
					vattr.va_mask = AT_MODE;
					if ((error = VOP_GETATTR(vp,
					    &vattr, 0, CRED(), NULL)) == 0 &&
					    MANDLOCK(vp, vattr.va_mode))
						error = EAGAIN;
				}
			}
			if (error)
				goto out;
		}
		/*
		 * Check permissions.
		 */
		if (error = VOP_ACCESS(vp, mode, accessflags, CRED(), NULL))
			goto out;
		/*
		 * Require FSEARCH to return a directory.
		 * Require FEXEC to return a regular file.
		 */
		if ((filemode & FSEARCH) && vp->v_type != VDIR) {
			error = ENOTDIR;
			goto out;
		}
		if ((filemode & FEXEC) && vp->v_type != VREG) {
			error = ENOEXEC;	/* XXX: error code? */
			goto out;
		}
	}

	/*
	 * Do remaining checks for FNOFOLLOW and FNOLINKS.
	 */
	if ((filemode & FNOFOLLOW) && vp->v_type == VLNK) {
		error = ELOOP;
		goto out;
	}
	if (filemode & FNOLINKS) {
		vattr.va_mask = AT_NLINK;
		if ((error = VOP_GETATTR(vp, &vattr, 0, CRED(), NULL))) {
			goto out;
		}
		if (vattr.va_nlink != 1) {
			error = EMLINK;
			goto out;
		}
	}

	/*
	 * Opening a socket corresponding to the AF_UNIX pathname
	 * in the filesystem name space is not supported.
	 * However, VSOCK nodes in namefs are supported in order
	 * to make fattach work for sockets.
	 *
	 * XXX This uses VOP_REALVP to distinguish between
	 * an unopened namefs node (where VOP_REALVP returns a
	 * different VSOCK vnode) and a VSOCK created by vn_create
	 * in some file system (where VOP_REALVP would never return
	 * a different vnode).
	 */
	if (vp->v_type == VSOCK) {
		struct vnode *nvp;

		error = VOP_REALVP(vp, &nvp, NULL);
		if (error != 0 || nvp == NULL || nvp == vp ||
		    nvp->v_type != VSOCK) {
			error = EOPNOTSUPP;
			goto out;
		}
	}

	if ((vp->v_type == VREG) && nbl_need_check(vp)) {
		/* get share reservation */
		shr.s_access = 0;
		if (filemode & FWRITE)
			shr.s_access |= F_WRACC;
		if (filemode & FREAD)
			shr.s_access |= F_RDACC;
		shr.s_deny = 0;
		shr.s_sysid = 0;
		shr.s_pid = ttoproc(curthread)->p_pid;
		shr_own.sl_pid = shr.s_pid;
		shr_own.sl_id = fd;
		shr.s_own_len = sizeof (shr_own);
		shr.s_owner = (caddr_t)&shr_own;
		error = VOP_SHRLOCK(vp, F_SHARE_NBMAND, &shr, filemode, CRED(),
		    NULL);
		if (error)
			goto out;
		shrlock_done = 1;

		/* nbmand conflict check if truncating file */
		if ((filemode & FTRUNC) && !(filemode & FCREAT)) {
			nbl_start_crit(vp, RW_READER);
			in_crit = 1;

			vattr.va_mask = AT_SIZE;
			if (error = VOP_GETATTR(vp, &vattr, 0, CRED(), NULL))
				goto out;
			if (nbl_conflict(vp, NBL_WRITE, 0, vattr.va_size, 0,
			    NULL)) {
				error = EACCES;
				goto out;
			}
		}
	}

	/*
	 * Do opening protocol.
	 */
	error = VOP_OPEN(&vp, filemode, CRED(), NULL);
	if (error)
		goto out;
	open_done = 1;

	/*
	 * Truncate if required.
	 */
	if ((filemode & FTRUNC) && !(filemode & FCREAT)) {
		vattr.va_size = 0;
		vattr.va_mask = AT_SIZE;
		if ((error = VOP_SETATTR(vp, &vattr, 0, CRED(), NULL)) != 0)
			goto out;
	}
out:
	ASSERT(vp->v_count > 0);

	if (in_crit) {
		nbl_end_crit(vp);
		in_crit = 0;
	}
	if (error) {
		if (open_done) {
			(void) VOP_CLOSE(vp, filemode, 1, (offset_t)0, CRED(),
			    NULL);
			open_done = 0;
			shrlock_done = 0;
		}
		if (shrlock_done) {
			(void) VOP_SHRLOCK(vp, F_UNSHARE, &shr, 0, CRED(),
			    NULL);
			shrlock_done = 0;
		}

		/*
		 * The following clause was added to handle a problem
		 * with NFS consistency.  It is possible that a lookup
		 * of the file to be opened succeeded, but the file
		 * itself doesn't actually exist on the server.  This
		 * is chiefly due to the DNLC containing an entry for
		 * the file which has been removed on the server.  In
		 * this case, we just start over.  If there was some
		 * other cause for the ESTALE error, then the lookup
		 * of the file will fail and the error will be returned
		 * above instead of looping around from here.
		 */
		VN_RELE(vp);
		if ((error == ESTALE) && fs_need_estale_retry(estale_retry++))
			goto top;
	} else
		*vpp = vp;
	return (error);
}

/*
 * The following two accessor functions are for the NFSv4 server.  Since there
 * is no VOP_OPEN_UP/DOWNGRADE we need a way for the NFS server to keep the
 * vnode open counts correct when a client "upgrades" an open or does an
 * open_downgrade.  In NFS, an upgrade or downgrade can not only change the
 * open mode (add or subtract read or write), but also change the share/deny
 * modes.  However, share reservations are not integrated with OPEN, yet, so
 * we need to handle each separately.  These functions are cleaner than having
 * the NFS server manipulate the counts directly, however, nobody else should
 * use these functions.
 */
void
vn_open_upgrade(
	vnode_t *vp,
	int filemode)
{
	ASSERT(vp->v_type == VREG);

	if (filemode & FREAD)
		atomic_inc_32(&vp->v_rdcnt);
	if (filemode & FWRITE)
		atomic_inc_32(&vp->v_wrcnt);

}

void
vn_open_downgrade(
	vnode_t *vp,
	int filemode)
{
	ASSERT(vp->v_type == VREG);

	if (filemode & FREAD) {
		ASSERT(vp->v_rdcnt > 0);
		atomic_dec_32(&vp->v_rdcnt);
	}
	if (filemode & FWRITE) {
		ASSERT(vp->v_wrcnt > 0);
		atomic_dec_32(&vp->v_wrcnt);
	}

}

int
vn_create(
	char *pnamep,
	enum uio_seg seg,
	struct vattr *vap,
	enum vcexcl excl,
	int mode,
	struct vnode **vpp,
	enum create why,
	int flag,
	mode_t umask)
{
	return (vn_createat(pnamep, seg, vap, excl, mode, vpp, why, flag,
	    umask, NULL));
}

/*
 * Create a vnode (makenode).
 */
int
vn_createat(
	char *pnamep,
	enum uio_seg seg,
	struct vattr *vap,
	enum vcexcl excl,
	int mode,
	struct vnode **vpp,
	enum create why,
	int flag,
	mode_t umask,
	struct vnode *startvp)
{
	struct vnode *dvp;	/* ptr to parent dir vnode */
	struct vnode *vp = NULL;
	struct pathname pn;
	int error;
	int in_crit = 0;
	struct vattr vattr;
	enum symfollow follow;
	int estale_retry = 0;
	uint32_t auditing = AU_AUDITING();

	ASSERT((vap->va_mask & (AT_TYPE|AT_MODE)) == (AT_TYPE|AT_MODE));

	/* symlink interpretation */
	if ((flag & FNOFOLLOW) || excl == EXCL)
		follow = NO_FOLLOW;
	else
		follow = FOLLOW;
	flag &= ~(FNOFOLLOW|FNOLINKS);

top:
	/*
	 * Lookup directory.
	 * If new object is a file, call lower level to create it.
	 * Note that it is up to the lower level to enforce exclusive
	 * creation, if the file is already there.
	 * This allows the lower level to do whatever
	 * locking or protocol that is needed to prevent races.
	 * If the new object is directory call lower level to make
	 * the new directory, with "." and "..".
	 */
	if (error = pn_get(pnamep, seg, &pn))
		return (error);
	if (auditing)
		audit_vncreate_start();
	dvp = NULL;
	*vpp = NULL;
	/*
	 * lookup will find the parent directory for the vnode.
	 * When it is done the pn holds the name of the entry
	 * in the directory.
	 * If this is a non-exclusive create we also find the node itself.
	 */
	error = lookuppnat(&pn, NULL, follow, &dvp,
	    (excl == EXCL) ? NULLVPP : vpp, startvp);
	if (error) {
		pn_free(&pn);
		if ((error == ESTALE) && fs_need_estale_retry(estale_retry++))
			goto top;
		if (why == CRMKDIR && error == EINVAL)
			error = EEXIST;		/* SVID */
		return (error);
	}

	if (why != CRMKNOD)
		vap->va_mode &= ~VSVTX;

	/*
	 * If default ACLs are defined for the directory don't apply the
	 * umask if umask is passed.
	 */

	if (umask) {

		vsecattr_t vsec;

		vsec.vsa_aclcnt = 0;
		vsec.vsa_aclentp = NULL;
		vsec.vsa_dfaclcnt = 0;
		vsec.vsa_dfaclentp = NULL;
		vsec.vsa_mask = VSA_DFACLCNT;
		error = VOP_GETSECATTR(dvp, &vsec, 0, CRED(), NULL);
		/*
		 * If error is ENOSYS then treat it as no error
		 * Don't want to force all file systems to support
		 * aclent_t style of ACL's.
		 */
		if (error == ENOSYS)
			error = 0;
		if (error) {
			if (*vpp != NULL)
				VN_RELE(*vpp);
			goto out;
		} else {
			/*
			 * Apply the umask if no default ACLs.
			 */
			if (vsec.vsa_dfaclcnt == 0)
				vap->va_mode &= ~umask;

			/*
			 * VOP_GETSECATTR() may have allocated memory for
			 * ACLs we didn't request, so double-check and
			 * free it if necessary.
			 */
			if (vsec.vsa_aclcnt && vsec.vsa_aclentp != NULL)
				kmem_free((caddr_t)vsec.vsa_aclentp,
				    vsec.vsa_aclcnt * sizeof (aclent_t));
			if (vsec.vsa_dfaclcnt && vsec.vsa_dfaclentp != NULL)
				kmem_free((caddr_t)vsec.vsa_dfaclentp,
				    vsec.vsa_dfaclcnt * sizeof (aclent_t));
		}
	}

	/*
	 * In general we want to generate EROFS if the file system is
	 * readonly.  However, POSIX (IEEE Std. 1003.1) section 5.3.1
	 * documents the open system call, and it says that O_CREAT has no
	 * effect if the file already exists.  Bug 1119649 states
	 * that open(path, O_CREAT, ...) fails when attempting to open an
	 * existing file on a read only file system.  Thus, the first part
	 * of the following if statement has 3 checks:
	 *	if the file exists &&
	 *		it is being open with write access &&
	 *		the file system is read only
	 *	then generate EROFS
	 */
	if ((*vpp != NULL && (mode & VWRITE) && ISROFILE(*vpp)) ||
	    (*vpp == NULL && dvp->v_vfsp->vfs_flag & VFS_RDONLY)) {
		if (*vpp)
			VN_RELE(*vpp);
		error = EROFS;
	} else if (excl == NONEXCL && *vpp != NULL) {
		vnode_t *rvp;

		/*
		 * File already exists.  If a mandatory lock has been
		 * applied, return error.
		 */
		vp = *vpp;
		if (VOP_REALVP(vp, &rvp, NULL) != 0)
			rvp = vp;
		if ((vap->va_mask & AT_SIZE) && nbl_need_check(vp)) {
			nbl_start_crit(vp, RW_READER);
			in_crit = 1;
		}
		if (rvp->v_filocks != NULL || rvp->v_shrlocks != NULL) {
			vattr.va_mask = AT_MODE|AT_SIZE;
			if (error = VOP_GETATTR(vp, &vattr, 0, CRED(), NULL)) {
				goto out;
			}
			if (MANDLOCK(vp, vattr.va_mode)) {
				error = EAGAIN;
				goto out;
			}
			/*
			 * File cannot be truncated if non-blocking mandatory
			 * locks are currently on the file.
			 */
			if ((vap->va_mask & AT_SIZE) && in_crit) {
				u_offset_t offset;
				ssize_t length;

				offset = vap->va_size > vattr.va_size ?
				    vattr.va_size : vap->va_size;
				length = vap->va_size > vattr.va_size ?
				    vap->va_size - vattr.va_size :
				    vattr.va_size - vap->va_size;
				if (nbl_conflict(vp, NBL_WRITE, offset,
				    length, 0, NULL)) {
					error = EACCES;
					goto out;
				}
			}
		}

		/*
		 * If the file is the root of a VFS, we've crossed a
		 * mount point and the "containing" directory that we
		 * acquired above (dvp) is irrelevant because it's in
		 * a different file system.  We apply VOP_CREATE to the
		 * target itself instead of to the containing directory
		 * and supply a null path name to indicate (conventionally)
		 * the node itself as the "component" of interest.
		 *
		 * The intercession of the file system is necessary to
		 * ensure that the appropriate permission checks are
		 * done.
		 */
		if (vp->v_flag & VROOT) {
			ASSERT(why != CRMKDIR);
			error = VOP_CREATE(vp, "", vap, excl, mode, vpp,
			    CRED(), flag, NULL, NULL);
			/*
			 * If the create succeeded, it will have created
			 * a new reference to the vnode.  Give up the
			 * original reference.  The assertion should not
			 * get triggered because NBMAND locks only apply to
			 * VREG files.  And if in_crit is non-zero for some
			 * reason, detect that here, rather than when we
			 * deference a null vp.
			 */
			ASSERT(in_crit == 0);
			VN_RELE(vp);
			vp = NULL;
			goto out;
		}

		/*
		 * Large File API - non-large open (FOFFMAX flag not set)
		 * of regular file fails if the file size exceeds MAXOFF32_T.
		 */
		if (why != CRMKDIR &&
		    !(flag & FOFFMAX) &&
		    (vp->v_type == VREG)) {
			vattr.va_mask = AT_SIZE;
			if ((error = VOP_GETATTR(vp, &vattr, 0,
			    CRED(), NULL))) {
				goto out;
			}
			if ((vattr.va_size > (u_offset_t)MAXOFF32_T)) {
				error = EOVERFLOW;
				goto out;
			}
		}
	}

	if (error == 0) {
		/*
		 * Call mkdir() if specified, otherwise create().
		 */
		int must_be_dir = pn_fixslash(&pn);	/* trailing '/'? */

		if (why == CRMKDIR)
			/*
			 * N.B., if vn_createat() ever requests
			 * case-insensitive behavior then it will need
			 * to be passed to VOP_MKDIR().  VOP_CREATE()
			 * will already get it via "flag"
			 */
			error = VOP_MKDIR(dvp, pn.pn_path, vap, vpp, CRED(),
			    NULL, 0, NULL);
		else if (!must_be_dir)
			error = VOP_CREATE(dvp, pn.pn_path, vap,
			    excl, mode, vpp, CRED(), flag, NULL, NULL);
		else
			error = ENOTDIR;
	}

out:

	if (auditing)
		audit_vncreate_finish(*vpp, error);
	if (in_crit) {
		nbl_end_crit(vp);
		in_crit = 0;
	}
	if (vp != NULL) {
		VN_RELE(vp);
		vp = NULL;
	}
	pn_free(&pn);
	VN_RELE(dvp);
	/*
	 * The following clause was added to handle a problem
	 * with NFS consistency.  It is possible that a lookup
	 * of the file to be created succeeded, but the file
	 * itself doesn't actually exist on the server.  This
	 * is chiefly due to the DNLC containing an entry for
	 * the file which has been removed on the server.  In
	 * this case, we just start over.  If there was some
	 * other cause for the ESTALE error, then the lookup
	 * of the file will fail and the error will be returned
	 * above instead of looping around from here.
	 */
	if ((error == ESTALE) && fs_need_estale_retry(estale_retry++))
		goto top;
	return (error);
}

int
vn_link(char *from, char *to, enum uio_seg seg)
{
	return (vn_linkat(NULL, from, NO_FOLLOW, NULL, to, seg));
}

int
vn_linkat(vnode_t *fstartvp, char *from, enum symfollow follow,
    vnode_t *tstartvp, char *to, enum uio_seg seg)
{
	struct vnode *fvp;		/* from vnode ptr */
	struct vnode *tdvp;		/* to directory vnode ptr */
	struct pathname pn;
	int error;
	struct vattr vattr;
	dev_t fsid;
	int estale_retry = 0;
	uint32_t auditing = AU_AUDITING();

top:
	fvp = tdvp = NULL;
	if (error = pn_get(to, seg, &pn))
		return (error);
	if (auditing && fstartvp != NULL)
		audit_setfsat_path(1);
	if (error = lookupnameat(from, seg, follow, NULLVPP, &fvp, fstartvp))
		goto out;
	if (auditing && tstartvp != NULL)
		audit_setfsat_path(3);
	if (error = lookuppnat(&pn, NULL, NO_FOLLOW, &tdvp, NULLVPP, tstartvp))
		goto out;
	/*
	 * Make sure both source vnode and target directory vnode are
	 * in the same vfs and that it is writeable.
	 */
	vattr.va_mask = AT_FSID;
	if (error = VOP_GETATTR(fvp, &vattr, 0, CRED(), NULL))
		goto out;
	fsid = vattr.va_fsid;
	vattr.va_mask = AT_FSID;
	if (error = VOP_GETATTR(tdvp, &vattr, 0, CRED(), NULL))
		goto out;
	if (fsid != vattr.va_fsid) {
		error = EXDEV;
		goto out;
	}
	if (tdvp->v_vfsp->vfs_flag & VFS_RDONLY) {
		error = EROFS;
		goto out;
	}
	/*
	 * Do the link.
	 */
	(void) pn_fixslash(&pn);
	error = VOP_LINK(tdvp, fvp, pn.pn_path, CRED(), NULL, 0);
out:
	pn_free(&pn);
	if (fvp)
		VN_RELE(fvp);
	if (tdvp)
		VN_RELE(tdvp);
	if ((error == ESTALE) && fs_need_estale_retry(estale_retry++))
		goto top;
	return (error);
}

int
vn_rename(char *from, char *to, enum uio_seg seg)
{
	return (vn_renameat(NULL, from, NULL, to, seg));
}

int
vn_renameat(vnode_t *fdvp, char *fname, vnode_t *tdvp,
		char *tname, enum uio_seg seg)
{
	int error;
	struct vattr vattr;
	struct pathname fpn;		/* from pathname */
	struct pathname tpn;		/* to pathname */
	dev_t fsid;
	int in_crit_src, in_crit_targ;
	vnode_t *fromvp, *fvp;
	vnode_t *tovp, *targvp;
	int estale_retry = 0;
	uint32_t auditing = AU_AUDITING();

top:
	fvp = fromvp = tovp = targvp = NULL;
	in_crit_src = in_crit_targ = 0;
	/*
	 * Get to and from pathnames.
	 */
	if (error = pn_get(fname, seg, &fpn))
		return (error);
	if (error = pn_get(tname, seg, &tpn)) {
		pn_free(&fpn);
		return (error);
	}

	/*
	 * First we need to resolve the correct directories
	 * The passed in directories may only be a starting point,
	 * but we need the real directories the file(s) live in.
	 * For example the fname may be something like usr/lib/sparc
	 * and we were passed in the / directory, but we need to
	 * use the lib directory for the rename.
	 */

	if (auditing && fdvp != NULL)
		audit_setfsat_path(1);
	/*
	 * Lookup to and from directories.
	 */
	if (error = lookuppnat(&fpn, NULL, NO_FOLLOW, &fromvp, &fvp, fdvp)) {
		goto out;
	}

	/*
	 * Make sure there is an entry.
	 */
	if (fvp == NULL) {
		error = ENOENT;
		goto out;
	}

	if (auditing && tdvp != NULL)
		audit_setfsat_path(3);
	if (error = lookuppnat(&tpn, NULL, NO_FOLLOW, &tovp, &targvp, tdvp)) {
		goto out;
	}

	/*
	 * Make sure both the from vnode directory and the to directory
	 * are in the same vfs and the to directory is writable.
	 * We check fsid's, not vfs pointers, so loopback fs works.
	 */
	if (fromvp != tovp) {
		vattr.va_mask = AT_FSID;
		if (error = VOP_GETATTR(fromvp, &vattr, 0, CRED(), NULL))
			goto out;
		fsid = vattr.va_fsid;
		vattr.va_mask = AT_FSID;
		if (error = VOP_GETATTR(tovp, &vattr, 0, CRED(), NULL))
			goto out;
		if (fsid != vattr.va_fsid) {
			error = EXDEV;
			goto out;
		}
	}

	if (tovp->v_vfsp->vfs_flag & VFS_RDONLY) {
		error = EROFS;
		goto out;
	}

	if (targvp && (fvp != targvp)) {
		nbl_start_crit(targvp, RW_READER);
		in_crit_targ = 1;
		if (nbl_conflict(targvp, NBL_REMOVE, 0, 0, 0, NULL)) {
			error = EACCES;
			goto out;
		}
	}

	if (nbl_need_check(fvp)) {
		nbl_start_crit(fvp, RW_READER);
		in_crit_src = 1;
		if (nbl_conflict(fvp, NBL_RENAME, 0, 0, 0, NULL)) {
			error = EACCES;
			goto out;
		}
	}

	/*
	 * Do the rename.
	 */
	(void) pn_fixslash(&tpn);
	error = VOP_RENAME(fromvp, fpn.pn_path, tovp, tpn.pn_path, CRED(),
	    NULL, 0);

out:
	pn_free(&fpn);
	pn_free(&tpn);
	if (in_crit_src)
		nbl_end_crit(fvp);
	if (in_crit_targ)
		nbl_end_crit(targvp);
	if (fromvp)
		VN_RELE(fromvp);
	if (tovp)
		VN_RELE(tovp);
	if (targvp)
		VN_RELE(targvp);
	if (fvp)
		VN_RELE(fvp);
	if ((error == ESTALE) && fs_need_estale_retry(estale_retry++))
		goto top;
	return (error);
}

/*
 * Remove a file or directory.
 */
int
vn_remove(char *fnamep, enum uio_seg seg, enum rm dirflag)
{
	return (vn_removeat(NULL, fnamep, seg, dirflag));
}

int
vn_removeat(vnode_t *startvp, char *fnamep, enum uio_seg seg, enum rm dirflag)
{
	struct vnode *vp;		/* entry vnode */
	struct vnode *dvp;		/* ptr to parent dir vnode */
	struct vnode *coveredvp;
	struct pathname pn;		/* name of entry */
	enum vtype vtype;
	int error;
	struct vfs *vfsp;
	struct vfs *dvfsp;	/* ptr to parent dir vfs */
	int in_crit = 0;
	int estale_retry = 0;

top:
	if (error = pn_get(fnamep, seg, &pn))
		return (error);
	dvp = vp = NULL;
	if (error = lookuppnat(&pn, NULL, NO_FOLLOW, &dvp, &vp, startvp)) {
		pn_free(&pn);
		if ((error == ESTALE) && fs_need_estale_retry(estale_retry++))
			goto top;
		return (error);
	}

	/*
	 * Make sure there is an entry.
	 */
	if (vp == NULL) {
		error = ENOENT;
		goto out;
	}

	vfsp = vp->v_vfsp;
	dvfsp = dvp->v_vfsp;

	/*
	 * If the named file is the root of a mounted filesystem, fail,
	 * unless it's marked unlinkable.  In that case, unmount the
	 * filesystem and proceed to unlink the covered vnode.  (If the
	 * covered vnode is a directory, use rmdir instead of unlink,
	 * to avoid file system corruption.)
	 */
	if (vp->v_flag & VROOT) {
		if ((vfsp->vfs_flag & VFS_UNLINKABLE) == 0) {
			error = EBUSY;
			goto out;
		}

		/*
		 * Namefs specific code starts here.
		 */

		if (dirflag == RMDIRECTORY) {
			/*
			 * User called rmdir(2) on a file that has
			 * been namefs mounted on top of.  Since
			 * namefs doesn't allow directories to
			 * be mounted on other files we know
			 * vp is not of type VDIR so fail to operation.
			 */
			error = ENOTDIR;
			goto out;
		}

		/*
		 * If VROOT is still set after grabbing vp->v_lock,
		 * noone has finished nm_unmount so far and coveredvp
		 * is valid.
		 * If we manage to grab vn_vfswlock(coveredvp) before releasing
		 * vp->v_lock, any race window is eliminated.
		 */

		mutex_enter(&vp->v_lock);
		if ((vp->v_flag & VROOT) == 0) {
			/* Someone beat us to the unmount */
			mutex_exit(&vp->v_lock);
			error = EBUSY;
			goto out;
		}
		vfsp = vp->v_vfsp;
		coveredvp = vfsp->vfs_vnodecovered;
		ASSERT(coveredvp);
		/*
		 * Note: Implementation of vn_vfswlock shows that ordering of
		 * v_lock / vn_vfswlock is not an issue here.
		 */
		error = vn_vfswlock(coveredvp);
		mutex_exit(&vp->v_lock);

		if (error)
			goto out;

		VN_HOLD(coveredvp);
		VN_RELE(vp);
		error = dounmount(vfsp, 0, CRED());

		/*
		 * Unmounted the namefs file system; now get
		 * the object it was mounted over.
		 */
		vp = coveredvp;
		/*
		 * If namefs was mounted over a directory, then
		 * we want to use rmdir() instead of unlink().
		 */
		if (vp->v_type == VDIR)
			dirflag = RMDIRECTORY;

		if (error)
			goto out;
	}

	/*
	 * Make sure filesystem is writeable.
	 * We check the parent directory's vfs in case this is an lofs vnode.
	 */
	if (dvfsp && dvfsp->vfs_flag & VFS_RDONLY) {
		error = EROFS;
		goto out;
	}

	vtype = vp->v_type;

	/*
	 * If there is the possibility of an nbmand share reservation, make
	 * sure it's okay to remove the file.  Keep a reference to the
	 * vnode, so that we can exit the nbl critical region after
	 * calling VOP_REMOVE.
	 * If there is no possibility of an nbmand share reservation,
	 * release the vnode reference now.  Filesystems like NFS may
	 * behave differently if there is an extra reference, so get rid of
	 * this one.  Fortunately, we can't have nbmand mounts on NFS
	 * filesystems.
	 */
	if (nbl_need_check(vp)) {
		nbl_start_crit(vp, RW_READER);
		in_crit = 1;
		if (nbl_conflict(vp, NBL_REMOVE, 0, 0, 0, NULL)) {
			error = EACCES;
			goto out;
		}
	} else {
		VN_RELE(vp);
		vp = NULL;
	}

	if (dirflag == RMDIRECTORY) {
		/*
		 * Caller is using rmdir(2), which can only be applied to
		 * directories.
		 */
		if (vtype != VDIR) {
			error = ENOTDIR;
		} else {
			vnode_t *cwd;
			proc_t *pp = curproc;

			mutex_enter(&pp->p_lock);
			cwd = PTOU(pp)->u_cdir;
			VN_HOLD(cwd);
			mutex_exit(&pp->p_lock);
			error = VOP_RMDIR(dvp, pn.pn_path, cwd, CRED(),
			    NULL, 0);
			VN_RELE(cwd);
		}
	} else {
		/*
		 * Unlink(2) can be applied to anything.
		 */
		error = VOP_REMOVE(dvp, pn.pn_path, CRED(), NULL, 0);
	}

out:
	pn_free(&pn);
	if (in_crit) {
		nbl_end_crit(vp);
		in_crit = 0;
	}
	if (vp != NULL)
		VN_RELE(vp);
	if (dvp != NULL)
		VN_RELE(dvp);
	if ((error == ESTALE) && fs_need_estale_retry(estale_retry++))
		goto top;
	return (error);
}

/*
 * Utility function to compare equality of vnodes.
 * Compare the underlying real vnodes, if there are underlying vnodes.
 * This is a more thorough comparison than the VN_CMP() macro provides.
 */
int
vn_compare(vnode_t *vp1, vnode_t *vp2)
{
	vnode_t *realvp;

	if (vp1 != NULL && VOP_REALVP(vp1, &realvp, NULL) == 0)
		vp1 = realvp;
	if (vp2 != NULL && VOP_REALVP(vp2, &realvp, NULL) == 0)
		vp2 = realvp;
	return (VN_CMP(vp1, vp2));
}

/*
 * The number of locks to hash into.  This value must be a power
 * of 2 minus 1 and should probably also be prime.
 */
#define	NUM_BUCKETS	1023

struct  vn_vfslocks_bucket {
	kmutex_t vb_lock;
	vn_vfslocks_entry_t *vb_list;
	char pad[64 - sizeof (kmutex_t) - sizeof (void *)];
};

/*
 * Total number of buckets will be NUM_BUCKETS + 1 .
 */

#pragma	align	64(vn_vfslocks_buckets)
static	struct vn_vfslocks_bucket	vn_vfslocks_buckets[NUM_BUCKETS + 1];

#define	VN_VFSLOCKS_SHIFT	9

#define	VN_VFSLOCKS_HASH(vfsvpptr)	\
	((((intptr_t)(vfsvpptr)) >> VN_VFSLOCKS_SHIFT) & NUM_BUCKETS)

/*
 * vn_vfslocks_getlock() uses an HASH scheme to generate
 * rwstlock using vfs/vnode pointer passed to it.
 *
 * vn_vfslocks_rele() releases a reference in the
 * HASH table which allows the entry allocated by
 * vn_vfslocks_getlock() to be freed at a later
 * stage when the refcount drops to zero.
 */

vn_vfslocks_entry_t *
vn_vfslocks_getlock(void *vfsvpptr)
{
	struct vn_vfslocks_bucket *bp;
	vn_vfslocks_entry_t *vep;
	vn_vfslocks_entry_t *tvep;

	ASSERT(vfsvpptr != NULL);
	bp = &vn_vfslocks_buckets[VN_VFSLOCKS_HASH(vfsvpptr)];

	mutex_enter(&bp->vb_lock);
	for (vep = bp->vb_list; vep != NULL; vep = vep->ve_next) {
		if (vep->ve_vpvfs == vfsvpptr) {
			vep->ve_refcnt++;
			mutex_exit(&bp->vb_lock);
			return (vep);
		}
	}
	mutex_exit(&bp->vb_lock);
	vep = kmem_alloc(sizeof (*vep), KM_SLEEP);
	rwst_init(&vep->ve_lock, NULL, RW_DEFAULT, NULL);
	vep->ve_vpvfs = (char *)vfsvpptr;
	vep->ve_refcnt = 1;
	mutex_enter(&bp->vb_lock);
	for (tvep = bp->vb_list; tvep != NULL; tvep = tvep->ve_next) {
		if (tvep->ve_vpvfs == vfsvpptr) {
			tvep->ve_refcnt++;
			mutex_exit(&bp->vb_lock);

			/*
			 * There is already an entry in the hash
			 * destroy what we just allocated.
			 */
			rwst_destroy(&vep->ve_lock);
			kmem_free(vep, sizeof (*vep));
			return (tvep);
		}
	}
	vep->ve_next = bp->vb_list;
	bp->vb_list = vep;
	mutex_exit(&bp->vb_lock);
	return (vep);
}

void
vn_vfslocks_rele(vn_vfslocks_entry_t *vepent)
{
	struct vn_vfslocks_bucket *bp;
	vn_vfslocks_entry_t *vep;
	vn_vfslocks_entry_t *pvep;

	ASSERT(vepent != NULL);
	ASSERT(vepent->ve_vpvfs != NULL);

	bp = &vn_vfslocks_buckets[VN_VFSLOCKS_HASH(vepent->ve_vpvfs)];

	mutex_enter(&bp->vb_lock);
	vepent->ve_refcnt--;

	if ((int32_t)vepent->ve_refcnt < 0)
		cmn_err(CE_PANIC, "vn_vfslocks_rele: refcount negative");

	if (vepent->ve_refcnt == 0) {
		for (vep = bp->vb_list; vep != NULL; vep = vep->ve_next) {
			if (vep->ve_vpvfs == vepent->ve_vpvfs) {
				if (bp->vb_list == vep)
					bp->vb_list = vep->ve_next;
				else {
					/* LINTED */
					pvep->ve_next = vep->ve_next;
				}
				mutex_exit(&bp->vb_lock);
				rwst_destroy(&vep->ve_lock);
				kmem_free(vep, sizeof (*vep));
				return;
			}
			pvep = vep;
		}
		cmn_err(CE_PANIC, "vn_vfslocks_rele: vp/vfs not found");
	}
	mutex_exit(&bp->vb_lock);
}

/*
 * vn_vfswlock_wait is used to implement a lock which is logically a writers
 * lock protecting the v_vfsmountedhere field.
 * vn_vfswlock_wait has been modified to be similar to vn_vfswlock,
 * except that it blocks to acquire the lock VVFSLOCK.
 *
 * traverse() and routines re-implementing part of traverse (e.g. autofs)
 * need to hold this lock. mount(), vn_rename(), vn_remove() and so on
 * need the non-blocking version of the writers lock i.e. vn_vfswlock
 */
int
vn_vfswlock_wait(vnode_t *vp)
{
	int retval;
	vn_vfslocks_entry_t *vpvfsentry;
	ASSERT(vp != NULL);

	vpvfsentry = vn_vfslocks_getlock(vp);
	retval = rwst_enter_sig(&vpvfsentry->ve_lock, RW_WRITER);

	if (retval == EINTR) {
		vn_vfslocks_rele(vpvfsentry);
		return (EINTR);
	}
	return (retval);
}

int
vn_vfsrlock_wait(vnode_t *vp)
{
	int retval;
	vn_vfslocks_entry_t *vpvfsentry;
	ASSERT(vp != NULL);

	vpvfsentry = vn_vfslocks_getlock(vp);
	retval = rwst_enter_sig(&vpvfsentry->ve_lock, RW_READER);

	if (retval == EINTR) {
		vn_vfslocks_rele(vpvfsentry);
		return (EINTR);
	}

	return (retval);
}


/*
 * vn_vfswlock is used to implement a lock which is logically a writers lock
 * protecting the v_vfsmountedhere field.
 */
int
vn_vfswlock(vnode_t *vp)
{
	vn_vfslocks_entry_t *vpvfsentry;

	/*
	 * If vp is NULL then somebody is trying to lock the covered vnode
	 * of /.  (vfs_vnodecovered is NULL for /).  This situation will
	 * only happen when unmounting /.  Since that operation will fail
	 * anyway, return EBUSY here instead of in VFS_UNMOUNT.
	 */
	if (vp == NULL)
		return (EBUSY);

	vpvfsentry = vn_vfslocks_getlock(vp);

	if (rwst_tryenter(&vpvfsentry->ve_lock, RW_WRITER))
		return (0);

	vn_vfslocks_rele(vpvfsentry);
	return (EBUSY);
}

int
vn_vfsrlock(vnode_t *vp)
{
	vn_vfslocks_entry_t *vpvfsentry;

	/*
	 * If vp is NULL then somebody is trying to lock the covered vnode
	 * of /.  (vfs_vnodecovered is NULL for /).  This situation will
	 * only happen when unmounting /.  Since that operation will fail
	 * anyway, return EBUSY here instead of in VFS_UNMOUNT.
	 */
	if (vp == NULL)
		return (EBUSY);

	vpvfsentry = vn_vfslocks_getlock(vp);

	if (rwst_tryenter(&vpvfsentry->ve_lock, RW_READER))
		return (0);

	vn_vfslocks_rele(vpvfsentry);
	return (EBUSY);
}

void
vn_vfsunlock(vnode_t *vp)
{
	vn_vfslocks_entry_t *vpvfsentry;

	/*
	 * ve_refcnt needs to be decremented twice.
	 * 1. To release refernce after a call to vn_vfslocks_getlock()
	 * 2. To release the reference from the locking routines like
	 *    vn_vfsrlock/vn_vfswlock etc,.
	 */
	vpvfsentry = vn_vfslocks_getlock(vp);
	vn_vfslocks_rele(vpvfsentry);

	rwst_exit(&vpvfsentry->ve_lock);
	vn_vfslocks_rele(vpvfsentry);
}

int
vn_vfswlock_held(vnode_t *vp)
{
	int held;
	vn_vfslocks_entry_t *vpvfsentry;

	ASSERT(vp != NULL);

	vpvfsentry = vn_vfslocks_getlock(vp);
	held = rwst_lock_held(&vpvfsentry->ve_lock, RW_WRITER);

	vn_vfslocks_rele(vpvfsentry);
	return (held);
}


int
vn_make_ops(
	const char *name,			/* Name of file system */
	const fs_operation_def_t *templ,	/* Operation specification */
	vnodeops_t **actual)			/* Return the vnodeops */
{
	int unused_ops;
	int error;

	*actual = (vnodeops_t *)kmem_alloc(sizeof (vnodeops_t), KM_SLEEP);

	(*actual)->vnop_name = name;

	error = fs_build_vector(*actual, &unused_ops, vn_ops_table, templ);
	if (error) {
		kmem_free(*actual, sizeof (vnodeops_t));
	}

#if DEBUG
	if (unused_ops != 0)
		cmn_err(CE_WARN, "vn_make_ops: %s: %d operations supplied "
		    "but not used", name, unused_ops);
#endif

	return (error);
}

/*
 * Free the vnodeops created as a result of vn_make_ops()
 */
void
vn_freevnodeops(vnodeops_t *vnops)
{
	kmem_free(vnops, sizeof (vnodeops_t));
}

/*
 * Vnode cache.
 */

/* ARGSUSED */
static int
vn_cache_constructor(void *buf, void *cdrarg, int kmflags)
{
	struct vnode *vp;

	vp = buf;

	mutex_init(&vp->v_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&vp->v_vsd_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&vp->v_cv, NULL, CV_DEFAULT, NULL);
	rw_init(&vp->v_nbllock, NULL, RW_DEFAULT, NULL);
	vp->v_femhead = NULL;	/* Must be done before vn_reinit() */
	vp->v_path = NULL;
	vp->v_mpssdata = NULL;
	vp->v_vsd = NULL;
	vp->v_fopdata = NULL;

	return (0);
}

/* ARGSUSED */
static void
vn_cache_destructor(void *buf, void *cdrarg)
{
	struct vnode *vp;

	vp = buf;

	rw_destroy(&vp->v_nbllock);
	cv_destroy(&vp->v_cv);
	mutex_destroy(&vp->v_vsd_lock);
	mutex_destroy(&vp->v_lock);
}

void
vn_create_cache(void)
{
	/* LINTED */
	ASSERT((1 << VNODE_ALIGN_LOG2) ==
	    P2ROUNDUP(sizeof (struct vnode), VNODE_ALIGN));
	vn_cache = kmem_cache_create("vn_cache", sizeof (struct vnode),
	    VNODE_ALIGN, vn_cache_constructor, vn_cache_destructor, NULL, NULL,
	    NULL, 0);
}

void
vn_destroy_cache(void)
{
	kmem_cache_destroy(vn_cache);
}

/*
 * Used by file systems when fs-specific nodes (e.g., ufs inodes) are
 * cached by the file system and vnodes remain associated.
 */
void
vn_recycle(vnode_t *vp)
{
	ASSERT(vp->v_pages == NULL);

	/*
	 * XXX - This really belongs in vn_reinit(), but we have some issues
	 * with the counts.  Best to have it here for clean initialization.
	 */
	vp->v_rdcnt = 0;
	vp->v_wrcnt = 0;
	vp->v_mmap_read = 0;
	vp->v_mmap_write = 0;

	/*
	 * If FEM was in use, make sure everything gets cleaned up
	 * NOTE: vp->v_femhead is initialized to NULL in the vnode
	 * constructor.
	 */
	if (vp->v_femhead) {
		/* XXX - There should be a free_femhead() that does all this */
		ASSERT(vp->v_femhead->femh_list == NULL);
		mutex_destroy(&vp->v_femhead->femh_lock);
		kmem_free(vp->v_femhead, sizeof (*(vp->v_femhead)));
		vp->v_femhead = NULL;
	}
	if (vp->v_path) {
		kmem_free(vp->v_path, strlen(vp->v_path) + 1);
		vp->v_path = NULL;
	}

	if (vp->v_fopdata != NULL) {
		free_fopdata(vp);
	}
	vp->v_mpssdata = NULL;
	vsd_free(vp);
}

/*
 * Used to reset the vnode fields including those that are directly accessible
 * as well as those which require an accessor function.
 *
 * Does not initialize:
 *	synchronization objects: v_lock, v_vsd_lock, v_nbllock, v_cv
 *	v_data (since FS-nodes and vnodes point to each other and should
 *		be updated simultaneously)
 *	v_op (in case someone needs to make a VOP call on this object)
 */
void
vn_reinit(vnode_t *vp)
{
	vp->v_count = 1;
	vp->v_count_dnlc = 0;
	vp->v_vfsp = NULL;
	vp->v_stream = NULL;
	vp->v_vfsmountedhere = NULL;
	vp->v_flag = 0;
	vp->v_type = VNON;
	vp->v_rdev = NODEV;

	vp->v_filocks = NULL;
	vp->v_shrlocks = NULL;
	vp->v_pages = NULL;

	vp->v_locality = NULL;
	vp->v_xattrdir = NULL;

	/* Handles v_femhead, v_path, and the r/w/map counts */
	vn_recycle(vp);
}

vnode_t *
vn_alloc(int kmflag)
{
	vnode_t *vp;

	vp = kmem_cache_alloc(vn_cache, kmflag);

	if (vp != NULL) {
		vp->v_femhead = NULL;	/* Must be done before vn_reinit() */
		vp->v_fopdata = NULL;
		vn_reinit(vp);
	}

	return (vp);
}

void
vn_free(vnode_t *vp)
{
	ASSERT(vp->v_shrlocks == NULL);
	ASSERT(vp->v_filocks == NULL);

	/*
	 * Some file systems call vn_free() with v_count of zero,
	 * some with v_count of 1.  In any case, the value should
	 * never be anything else.
	 */
	ASSERT((vp->v_count == 0) || (vp->v_count == 1));
	ASSERT(vp->v_count_dnlc == 0);
	if (vp->v_path != NULL) {
		kmem_free(vp->v_path, strlen(vp->v_path) + 1);
		vp->v_path = NULL;
	}

	/* If FEM was in use, make sure everything gets cleaned up */
	if (vp->v_femhead) {
		/* XXX - There should be a free_femhead() that does all this */
		ASSERT(vp->v_femhead->femh_list == NULL);
		mutex_destroy(&vp->v_femhead->femh_lock);
		kmem_free(vp->v_femhead, sizeof (*(vp->v_femhead)));
		vp->v_femhead = NULL;
	}

	if (vp->v_fopdata != NULL) {
		free_fopdata(vp);
	}
	vp->v_mpssdata = NULL;
	vsd_free(vp);
	kmem_cache_free(vn_cache, vp);
}

/*
 * vnode status changes, should define better states than 1, 0.
 */
void
vn_reclaim(vnode_t *vp)
{
	vfs_t   *vfsp = vp->v_vfsp;

	if (vfsp == NULL ||
	    vfsp->vfs_implp == NULL || vfsp->vfs_femhead == NULL) {
		return;
	}
	(void) VFS_VNSTATE(vfsp, vp, VNTRANS_RECLAIMED);
}

void
vn_idle(vnode_t *vp)
{
	vfs_t   *vfsp = vp->v_vfsp;

	if (vfsp == NULL ||
	    vfsp->vfs_implp == NULL || vfsp->vfs_femhead == NULL) {
		return;
	}
	(void) VFS_VNSTATE(vfsp, vp, VNTRANS_IDLED);
}
void
vn_exists(vnode_t *vp)
{
	vfs_t   *vfsp = vp->v_vfsp;

	if (vfsp == NULL ||
	    vfsp->vfs_implp == NULL || vfsp->vfs_femhead == NULL) {
		return;
	}
	(void) VFS_VNSTATE(vfsp, vp, VNTRANS_EXISTS);
}

void
vn_invalid(vnode_t *vp)
{
	vfs_t   *vfsp = vp->v_vfsp;

	if (vfsp == NULL ||
	    vfsp->vfs_implp == NULL || vfsp->vfs_femhead == NULL) {
		return;
	}
	(void) VFS_VNSTATE(vfsp, vp, VNTRANS_DESTROYED);
}

/* Vnode event notification */

int
vnevent_support(vnode_t *vp, caller_context_t *ct)
{
	if (vp == NULL)
		return (EINVAL);

	return (VOP_VNEVENT(vp, VE_SUPPORT, NULL, NULL, ct));
}

void
vnevent_rename_src(vnode_t *vp, vnode_t *dvp, char *name, caller_context_t *ct)
{
	if (vp == NULL || vp->v_femhead == NULL) {
		return;
	}
	(void) VOP_VNEVENT(vp, VE_RENAME_SRC, dvp, name, ct);
}

void
vnevent_rename_dest(vnode_t *vp, vnode_t *dvp, char *name,
    caller_context_t *ct)
{
	if (vp == NULL || vp->v_femhead == NULL) {
		return;
	}
	(void) VOP_VNEVENT(vp, VE_RENAME_DEST, dvp, name, ct);
}

void
vnevent_rename_dest_dir(vnode_t *vp, caller_context_t *ct)
{
	if (vp == NULL || vp->v_femhead == NULL) {
		return;
	}
	(void) VOP_VNEVENT(vp, VE_RENAME_DEST_DIR, NULL, NULL, ct);
}

void
vnevent_remove(vnode_t *vp, vnode_t *dvp, char *name, caller_context_t *ct)
{
	if (vp == NULL || vp->v_femhead == NULL) {
		return;
	}
	(void) VOP_VNEVENT(vp, VE_REMOVE, dvp, name, ct);
}

void
vnevent_rmdir(vnode_t *vp, vnode_t *dvp, char *name, caller_context_t *ct)
{
	if (vp == NULL || vp->v_femhead == NULL) {
		return;
	}
	(void) VOP_VNEVENT(vp, VE_RMDIR, dvp, name, ct);
}

void
vnevent_create(vnode_t *vp, caller_context_t *ct)
{
	if (vp == NULL || vp->v_femhead == NULL) {
		return;
	}
	(void) VOP_VNEVENT(vp, VE_CREATE, NULL, NULL, ct);
}

void
vnevent_link(vnode_t *vp, caller_context_t *ct)
{
	if (vp == NULL || vp->v_femhead == NULL) {
		return;
	}
	(void) VOP_VNEVENT(vp, VE_LINK, NULL, NULL, ct);
}

void
vnevent_mountedover(vnode_t *vp, caller_context_t *ct)
{
	if (vp == NULL || vp->v_femhead == NULL) {
		return;
	}
	(void) VOP_VNEVENT(vp, VE_MOUNTEDOVER, NULL, NULL, ct);
}

void
vnevent_truncate(vnode_t *vp, caller_context_t *ct)
{
	if (vp == NULL || vp->v_femhead == NULL) {
		return;
	}
	(void) VOP_VNEVENT(vp, VE_TRUNCATE, NULL, NULL, ct);
}

/*
 * Vnode accessors.
 */

int
vn_is_readonly(vnode_t *vp)
{
	return (vp->v_vfsp->vfs_flag & VFS_RDONLY);
}

int
vn_has_flocks(vnode_t *vp)
{
	return (vp->v_filocks != NULL);
}

int
vn_has_mandatory_locks(vnode_t *vp, int mode)
{
	return ((vp->v_filocks != NULL) && (MANDLOCK(vp, mode)));
}

int
vn_has_cached_data(vnode_t *vp)
{
	return (vp->v_pages != NULL);
}

/*
 * Return 0 if the vnode in question shouldn't be permitted into a zone via
 * zone_enter(2).
 */
int
vn_can_change_zones(vnode_t *vp)
{
	struct vfssw *vswp;
	int allow = 1;
	vnode_t *rvp;

	if (nfs_global_client_only != 0)
		return (1);

	/*
	 * We always want to look at the underlying vnode if there is one.
	 */
	if (VOP_REALVP(vp, &rvp, NULL) != 0)
		rvp = vp;
	/*
	 * Some pseudo filesystems (including doorfs) don't actually register
	 * their vfsops_t, so the following may return NULL; we happily let
	 * such vnodes switch zones.
	 */
	vswp = vfs_getvfsswbyvfsops(vfs_getops(rvp->v_vfsp));
	if (vswp != NULL) {
		if (vswp->vsw_flag & VSW_NOTZONESAFE)
			allow = 0;
		vfs_unrefvfssw(vswp);
	}
	return (allow);
}

/*
 * Return nonzero if the vnode is a mount point, zero if not.
 */
int
vn_ismntpt(vnode_t *vp)
{
	return (vp->v_vfsmountedhere != NULL);
}

/* Retrieve the vfs (if any) mounted on this vnode */
vfs_t *
vn_mountedvfs(vnode_t *vp)
{
	return (vp->v_vfsmountedhere);
}

/*
 * Return nonzero if the vnode is referenced by the dnlc, zero if not.
 */
int
vn_in_dnlc(vnode_t *vp)
{
	return (vp->v_count_dnlc > 0);
}

/*
 * vn_has_other_opens() checks whether a particular file is opened by more than
 * just the caller and whether the open is for read and/or write.
 * This routine is for calling after the caller has already called VOP_OPEN()
 * and the caller wishes to know if they are the only one with it open for
 * the mode(s) specified.
 *
 * Vnode counts are only kept on regular files (v_type=VREG).
 */
int
vn_has_other_opens(
	vnode_t *vp,
	v_mode_t mode)
{

	ASSERT(vp != NULL);

	switch (mode) {
	case V_WRITE:
		if (vp->v_wrcnt > 1)
			return (V_TRUE);
		break;
	case V_RDORWR:
		if ((vp->v_rdcnt > 1) || (vp->v_wrcnt > 1))
			return (V_TRUE);
		break;
	case V_RDANDWR:
		if ((vp->v_rdcnt > 1) && (vp->v_wrcnt > 1))
			return (V_TRUE);
		break;
	case V_READ:
		if (vp->v_rdcnt > 1)
			return (V_TRUE);
		break;
	}

	return (V_FALSE);
}

/*
 * vn_is_opened() checks whether a particular file is opened and
 * whether the open is for read and/or write.
 *
 * Vnode counts are only kept on regular files (v_type=VREG).
 */
int
vn_is_opened(
	vnode_t *vp,
	v_mode_t mode)
{

	ASSERT(vp != NULL);

	switch (mode) {
	case V_WRITE:
		if (vp->v_wrcnt)
			return (V_TRUE);
		break;
	case V_RDANDWR:
		if (vp->v_rdcnt && vp->v_wrcnt)
			return (V_TRUE);
		break;
	case V_RDORWR:
		if (vp->v_rdcnt || vp->v_wrcnt)
			return (V_TRUE);
		break;
	case V_READ:
		if (vp->v_rdcnt)
			return (V_TRUE);
		break;
	}

	return (V_FALSE);
}

/*
 * vn_is_mapped() checks whether a particular file is mapped and whether
 * the file is mapped read and/or write.
 */
int
vn_is_mapped(
	vnode_t *vp,
	v_mode_t mode)
{

	ASSERT(vp != NULL);

#if !defined(_LP64)
	switch (mode) {
	/*
	 * The atomic_add_64_nv functions force atomicity in the
	 * case of 32 bit architectures. Otherwise the 64 bit values
	 * require two fetches. The value of the fields may be
	 * (potentially) changed between the first fetch and the
	 * second
	 */
	case V_WRITE:
		if (atomic_add_64_nv((&(vp->v_mmap_write)), 0))
			return (V_TRUE);
		break;
	case V_RDANDWR:
		if ((atomic_add_64_nv((&(vp->v_mmap_read)), 0)) &&
		    (atomic_add_64_nv((&(vp->v_mmap_write)), 0)))
			return (V_TRUE);
		break;
	case V_RDORWR:
		if ((atomic_add_64_nv((&(vp->v_mmap_read)), 0)) ||
		    (atomic_add_64_nv((&(vp->v_mmap_write)), 0)))
			return (V_TRUE);
		break;
	case V_READ:
		if (atomic_add_64_nv((&(vp->v_mmap_read)), 0))
			return (V_TRUE);
		break;
	}
#else
	switch (mode) {
	case V_WRITE:
		if (vp->v_mmap_write)
			return (V_TRUE);
		break;
	case V_RDANDWR:
		if (vp->v_mmap_read && vp->v_mmap_write)
			return (V_TRUE);
		break;
	case V_RDORWR:
		if (vp->v_mmap_read || vp->v_mmap_write)
			return (V_TRUE);
		break;
	case V_READ:
		if (vp->v_mmap_read)
			return (V_TRUE);
		break;
	}
#endif

	return (V_FALSE);
}

/*
 * Set the operations vector for a vnode.
 *
 * FEM ensures that the v_femhead pointer is filled in before the
 * v_op pointer is changed.  This means that if the v_femhead pointer
 * is NULL, and the v_op field hasn't changed since before which checked
 * the v_femhead pointer; then our update is ok - we are not racing with
 * FEM.
 */
void
vn_setops(vnode_t *vp, vnodeops_t *vnodeops)
{
	vnodeops_t	*op;

	ASSERT(vp != NULL);
	ASSERT(vnodeops != NULL);

	op = vp->v_op;
	membar_consumer();
	/*
	 * If vp->v_femhead == NULL, then we'll call atomic_cas_ptr() to do
	 * the compare-and-swap on vp->v_op.  If either fails, then FEM is
	 * in effect on the vnode and we need to have FEM deal with it.
	 */
	if (vp->v_femhead != NULL || atomic_cas_ptr(&vp->v_op, op, vnodeops) !=
	    op) {
		fem_setvnops(vp, vnodeops);
	}
}

/*
 * Retrieve the operations vector for a vnode
 * As with vn_setops(above); make sure we aren't racing with FEM.
 * FEM sets the v_op to a special, internal, vnodeops that wouldn't
 * make sense to the callers of this routine.
 */
vnodeops_t *
vn_getops(vnode_t *vp)
{
	vnodeops_t	*op;

	ASSERT(vp != NULL);

	op = vp->v_op;
	membar_consumer();
	if (vp->v_femhead == NULL && op == vp->v_op) {
		return (op);
	} else {
		return (fem_getvnops(vp));
	}
}

/*
 * Returns non-zero (1) if the vnodeops matches that of the vnode.
 * Returns zero (0) if not.
 */
int
vn_matchops(vnode_t *vp, vnodeops_t *vnodeops)
{
	return (vn_getops(vp) == vnodeops);
}

/*
 * Returns non-zero (1) if the specified operation matches the
 * corresponding operation for that the vnode.
 * Returns zero (0) if not.
 */

#define	MATCHNAME(n1, n2) (((n1)[0] == (n2)[0]) && (strcmp((n1), (n2)) == 0))

int
vn_matchopval(vnode_t *vp, char *vopname, fs_generic_func_p funcp)
{
	const fs_operation_trans_def_t *otdp;
	fs_generic_func_p *loc = NULL;
	vnodeops_t	*vop = vn_getops(vp);

	ASSERT(vopname != NULL);

	for (otdp = vn_ops_table; otdp->name != NULL; otdp++) {
		if (MATCHNAME(otdp->name, vopname)) {
			loc = (fs_generic_func_p *)
			    ((char *)(vop) + otdp->offset);
			break;
		}
	}

	return ((loc != NULL) && (*loc == funcp));
}

/*
 * fs_new_caller_id() needs to return a unique ID on a given local system.
 * The IDs do not need to survive across reboots.  These are primarily
 * used so that (FEM) monitors can detect particular callers (such as
 * the NFS server) to a given vnode/vfs operation.
 */
u_longlong_t
fs_new_caller_id()
{
	static uint64_t next_caller_id = 0LL; /* First call returns 1 */

	return ((u_longlong_t)atomic_inc_64_nv(&next_caller_id));
}

/*
 * Given a starting vnode and a path, updates the path in the target vnode in
 * a safe manner.  If the vnode already has path information embedded, then the
 * cached path is left untouched.
 */

size_t max_vnode_path = 4 * MAXPATHLEN;

void
vn_setpath(vnode_t *rootvp, struct vnode *startvp, struct vnode *vp,
    const char *path, size_t plen)
{
	char	*rpath;
	vnode_t	*base;
	size_t	rpathlen, rpathalloc;
	int	doslash = 1;

	if (*path == '/') {
		base = rootvp;
		path++;
		plen--;
	} else {
		base = startvp;
	}

	/*
	 * We cannot grab base->v_lock while we hold vp->v_lock because of
	 * the potential for deadlock.
	 */
	mutex_enter(&base->v_lock);
	if (base->v_path == NULL) {
		mutex_exit(&base->v_lock);
		return;
	}

	rpathlen = strlen(base->v_path);
	rpathalloc = rpathlen + plen + 1;
	/* Avoid adding a slash if there's already one there */
	if (base->v_path[rpathlen-1] == '/')
		doslash = 0;
	else
		rpathalloc++;

	/*
	 * We don't want to call kmem_alloc(KM_SLEEP) with kernel locks held,
	 * so we must do this dance.  If, by chance, something changes the path,
	 * just give up since there is no real harm.
	 */
	mutex_exit(&base->v_lock);

	/* Paths should stay within reason */
	if (rpathalloc > max_vnode_path)
		return;

	rpath = kmem_alloc(rpathalloc, KM_SLEEP);

	mutex_enter(&base->v_lock);
	if (base->v_path == NULL || strlen(base->v_path) != rpathlen) {
		mutex_exit(&base->v_lock);
		kmem_free(rpath, rpathalloc);
		return;
	}
	bcopy(base->v_path, rpath, rpathlen);
	mutex_exit(&base->v_lock);

	if (doslash)
		rpath[rpathlen++] = '/';
	bcopy(path, rpath + rpathlen, plen);
	rpath[rpathlen + plen] = '\0';

	mutex_enter(&vp->v_lock);
	if (vp->v_path != NULL) {
		mutex_exit(&vp->v_lock);
		kmem_free(rpath, rpathalloc);
	} else {
		vp->v_path = rpath;
		mutex_exit(&vp->v_lock);
	}
}

/*
 * Sets the path to the vnode to be the given string, regardless of current
 * context.  The string must be a complete path from rootdir.  This is only used
 * by fsop_root() for setting the path based on the mountpoint.
 */
void
vn_setpath_str(struct vnode *vp, const char *str, size_t len)
{
	char *buf = kmem_alloc(len + 1, KM_SLEEP);

	mutex_enter(&vp->v_lock);
	if (vp->v_path != NULL) {
		mutex_exit(&vp->v_lock);
		kmem_free(buf, len + 1);
		return;
	}

	vp->v_path = buf;
	bcopy(str, vp->v_path, len);
	vp->v_path[len] = '\0';

	mutex_exit(&vp->v_lock);
}

/*
 * Called from within filesystem's vop_rename() to handle renames once the
 * target vnode is available.
 */
void
vn_renamepath(vnode_t *dvp, vnode_t *vp, const char *nm, size_t len)
{
	char *tmp;

	mutex_enter(&vp->v_lock);
	tmp = vp->v_path;
	vp->v_path = NULL;
	mutex_exit(&vp->v_lock);
	vn_setpath(rootdir, dvp, vp, nm, len);
	if (tmp != NULL)
		kmem_free(tmp, strlen(tmp) + 1);
}

/*
 * Similar to vn_setpath_str(), this function sets the path of the destination
 * vnode to the be the same as the source vnode.
 */
void
vn_copypath(struct vnode *src, struct vnode *dst)
{
	char *buf;
	int alloc;

	mutex_enter(&src->v_lock);
	if (src->v_path == NULL) {
		mutex_exit(&src->v_lock);
		return;
	}
	alloc = strlen(src->v_path) + 1;

	/* avoid kmem_alloc() with lock held */
	mutex_exit(&src->v_lock);
	buf = kmem_alloc(alloc, KM_SLEEP);
	mutex_enter(&src->v_lock);
	if (src->v_path == NULL || strlen(src->v_path) + 1 != alloc) {
		mutex_exit(&src->v_lock);
		kmem_free(buf, alloc);
		return;
	}
	bcopy(src->v_path, buf, alloc);
	mutex_exit(&src->v_lock);

	mutex_enter(&dst->v_lock);
	if (dst->v_path != NULL) {
		mutex_exit(&dst->v_lock);
		kmem_free(buf, alloc);
		return;
	}
	dst->v_path = buf;
	mutex_exit(&dst->v_lock);
}

/*
 * XXX Private interface for segvn routines that handle vnode
 * large page segments.
 *
 * return 1 if vp's file system VOP_PAGEIO() implementation
 * can be safely used instead of VOP_GETPAGE() for handling
 * pagefaults against regular non swap files. VOP_PAGEIO()
 * interface is considered safe here if its implementation
 * is very close to VOP_GETPAGE() implementation.
 * e.g. It zero's out the part of the page beyond EOF. Doesn't
 * panic if there're file holes but instead returns an error.
 * Doesn't assume file won't be changed by user writes, etc.
 *
 * return 0 otherwise.
 *
 * For now allow segvn to only use VOP_PAGEIO() with ufs and nfs.
 */
int
vn_vmpss_usepageio(vnode_t *vp)
{
	vfs_t   *vfsp = vp->v_vfsp;
	char *fsname = vfssw[vfsp->vfs_fstype].vsw_name;
	char *pageio_ok_fss[] = {"ufs", "nfs", NULL};
	char **fsok = pageio_ok_fss;

	if (fsname == NULL) {
		return (0);
	}

	for (; *fsok; fsok++) {
		if (strcmp(*fsok, fsname) == 0) {
			return (1);
		}
	}
	return (0);
}

/* VOP_XXX() macros call the corresponding fop_xxx() function */

int
fop_open(
	vnode_t **vpp,
	int mode,
	cred_t *cr,
	caller_context_t *ct)
{
	int ret;
	vnode_t *vp = *vpp;

	VN_HOLD(vp);
	/*
	 * Adding to the vnode counts before calling open
	 * avoids the need for a mutex. It circumvents a race
	 * condition where a query made on the vnode counts results in a
	 * false negative. The inquirer goes away believing the file is
	 * not open when there is an open on the file already under way.
	 *
	 * The counts are meant to prevent NFS from granting a delegation
	 * when it would be dangerous to do so.
	 *
	 * The vnode counts are only kept on regular files
	 */
	if ((*vpp)->v_type == VREG) {
		if (mode & FREAD)
			atomic_inc_32(&(*vpp)->v_rdcnt);
		if (mode & FWRITE)
			atomic_inc_32(&(*vpp)->v_wrcnt);
	}

	VOPXID_MAP_CR(vp, cr);

	ret = (*(*(vpp))->v_op->vop_open)(vpp, mode, cr, ct);

	if (ret) {
		/*
		 * Use the saved vp just in case the vnode ptr got trashed
		 * by the error.
		 */
		VOPSTATS_UPDATE(vp, open);
		if ((vp->v_type == VREG) && (mode & FREAD))
			atomic_dec_32(&vp->v_rdcnt);
		if ((vp->v_type == VREG) && (mode & FWRITE))
			atomic_dec_32(&vp->v_wrcnt);
	} else {
		/*
		 * Some filesystems will return a different vnode,
		 * but the same path was still used to open it.
		 * So if we do change the vnode and need to
		 * copy over the path, do so here, rather than special
		 * casing each filesystem. Adjust the vnode counts to
		 * reflect the vnode switch.
		 */
		VOPSTATS_UPDATE(*vpp, open);
		if (*vpp != vp && *vpp != NULL) {
			vn_copypath(vp, *vpp);
			if (((*vpp)->v_type == VREG) && (mode & FREAD))
				atomic_inc_32(&(*vpp)->v_rdcnt);
			if ((vp->v_type == VREG) && (mode & FREAD))
				atomic_dec_32(&vp->v_rdcnt);
			if (((*vpp)->v_type == VREG) && (mode & FWRITE))
				atomic_inc_32(&(*vpp)->v_wrcnt);
			if ((vp->v_type == VREG) && (mode & FWRITE))
				atomic_dec_32(&vp->v_wrcnt);
		}
	}
	VN_RELE(vp);
	return (ret);
}

int
fop_close(
	vnode_t *vp,
	int flag,
	int count,
	offset_t offset,
	cred_t *cr,
	caller_context_t *ct)
{
	int err;

	VOPXID_MAP_CR(vp, cr);

	err = (*(vp)->v_op->vop_close)(vp, flag, count, offset, cr, ct);
	VOPSTATS_UPDATE(vp, close);
	/*
	 * Check passed in count to handle possible dups. Vnode counts are only
	 * kept on regular files
	 */
	if ((vp->v_type == VREG) && (count == 1))  {
		if (flag & FREAD) {
			ASSERT(vp->v_rdcnt > 0);
			atomic_dec_32(&vp->v_rdcnt);
		}
		if (flag & FWRITE) {
			ASSERT(vp->v_wrcnt > 0);
			atomic_dec_32(&vp->v_wrcnt);
		}
	}
	return (err);
}

int
fop_read(
	vnode_t *vp,
	uio_t *uiop,
	int ioflag,
	cred_t *cr,
	caller_context_t *ct)
{
	int	err;
	ssize_t	resid_start = uiop->uio_resid;

	VOPXID_MAP_CR(vp, cr);

	err = (*(vp)->v_op->vop_read)(vp, uiop, ioflag, cr, ct);
	VOPSTATS_UPDATE_IO(vp, read,
	    read_bytes, (resid_start - uiop->uio_resid));
	return (err);
}

int
fop_write(
	vnode_t *vp,
	uio_t *uiop,
	int ioflag,
	cred_t *cr,
	caller_context_t *ct)
{
	int	err;
	ssize_t	resid_start = uiop->uio_resid;

	VOPXID_MAP_CR(vp, cr);

	err = (*(vp)->v_op->vop_write)(vp, uiop, ioflag, cr, ct);
	VOPSTATS_UPDATE_IO(vp, write,
	    write_bytes, (resid_start - uiop->uio_resid));
	return (err);
}

int
fop_ioctl(
	vnode_t *vp,
	int cmd,
	intptr_t arg,
	int flag,
	cred_t *cr,
	int *rvalp,
	caller_context_t *ct)
{
	int	err;

	VOPXID_MAP_CR(vp, cr);

	err = (*(vp)->v_op->vop_ioctl)(vp, cmd, arg, flag, cr, rvalp, ct);
	VOPSTATS_UPDATE(vp, ioctl);
	return (err);
}

int
fop_setfl(
	vnode_t *vp,
	int oflags,
	int nflags,
	cred_t *cr,
	caller_context_t *ct)
{
	int	err;

	VOPXID_MAP_CR(vp, cr);

	err = (*(vp)->v_op->vop_setfl)(vp, oflags, nflags, cr, ct);
	VOPSTATS_UPDATE(vp, setfl);
	return (err);
}

int
fop_getattr(
	vnode_t *vp,
	vattr_t *vap,
	int flags,
	cred_t *cr,
	caller_context_t *ct)
{
	int	err;

	VOPXID_MAP_CR(vp, cr);

	/*
	 * If this file system doesn't understand the xvattr extensions
	 * then turn off the xvattr bit.
	 */
	if (vfs_has_feature(vp->v_vfsp, VFSFT_XVATTR) == 0) {
		vap->va_mask &= ~AT_XVATTR;
	}

	/*
	 * We're only allowed to skip the ACL check iff we used a 32 bit
	 * ACE mask with VOP_ACCESS() to determine permissions.
	 */
	if ((flags & ATTR_NOACLCHECK) &&
	    vfs_has_feature(vp->v_vfsp, VFSFT_ACEMASKONACCESS) == 0) {
		return (EINVAL);
	}
	err = (*(vp)->v_op->vop_getattr)(vp, vap, flags, cr, ct);
	VOPSTATS_UPDATE(vp, getattr);
	return (err);
}

int
fop_setattr(
	vnode_t *vp,
	vattr_t *vap,
	int flags,
	cred_t *cr,
	caller_context_t *ct)
{
	int	err;

	VOPXID_MAP_CR(vp, cr);

	/*
	 * If this file system doesn't understand the xvattr extensions
	 * then turn off the xvattr bit.
	 */
	if (vfs_has_feature(vp->v_vfsp, VFSFT_XVATTR) == 0) {
		vap->va_mask &= ~AT_XVATTR;
	}

	/*
	 * We're only allowed to skip the ACL check iff we used a 32 bit
	 * ACE mask with VOP_ACCESS() to determine permissions.
	 */
	if ((flags & ATTR_NOACLCHECK) &&
	    vfs_has_feature(vp->v_vfsp, VFSFT_ACEMASKONACCESS) == 0) {
		return (EINVAL);
	}
	err = (*(vp)->v_op->vop_setattr)(vp, vap, flags, cr, ct);
	VOPSTATS_UPDATE(vp, setattr);
	return (err);
}

int
fop_access(
	vnode_t *vp,
	int mode,
	int flags,
	cred_t *cr,
	caller_context_t *ct)
{
	int	err;

	if ((flags & V_ACE_MASK) &&
	    vfs_has_feature(vp->v_vfsp, VFSFT_ACEMASKONACCESS) == 0) {
		return (EINVAL);
	}

	VOPXID_MAP_CR(vp, cr);

	err = (*(vp)->v_op->vop_access)(vp, mode, flags, cr, ct);
	VOPSTATS_UPDATE(vp, access);
	return (err);
}

int
fop_lookup(
	vnode_t *dvp,
	char *nm,
	vnode_t **vpp,
	pathname_t *pnp,
	int flags,
	vnode_t *rdir,
	cred_t *cr,
	caller_context_t *ct,
	int *deflags,		/* Returned per-dirent flags */
	pathname_t *ppnp)	/* Returned case-preserved name in directory */
{
	int ret;

	/*
	 * If this file system doesn't support case-insensitive access
	 * and said access is requested, fail quickly.  It is required
	 * that if the vfs supports case-insensitive lookup, it also
	 * supports extended dirent flags.
	 */
	if (flags & FIGNORECASE &&
	    (vfs_has_feature(dvp->v_vfsp, VFSFT_CASEINSENSITIVE) == 0 &&
	    vfs_has_feature(dvp->v_vfsp, VFSFT_NOCASESENSITIVE) == 0))
		return (EINVAL);

	VOPXID_MAP_CR(dvp, cr);

	if ((flags & LOOKUP_XATTR) && (flags & LOOKUP_HAVE_SYSATTR_DIR) == 0) {
		ret = xattr_dir_lookup(dvp, vpp, flags, cr);
	} else {
		ret = (*(dvp)->v_op->vop_lookup)
		    (dvp, nm, vpp, pnp, flags, rdir, cr, ct, deflags, ppnp);
	}
	if (ret == 0 && *vpp) {
		VOPSTATS_UPDATE(*vpp, lookup);
		if ((*vpp)->v_path == NULL) {
			vn_setpath(rootdir, dvp, *vpp, nm, strlen(nm));
		}
	}

	return (ret);
}

int
fop_create(
	vnode_t *dvp,
	char *name,
	vattr_t *vap,
	vcexcl_t excl,
	int mode,
	vnode_t **vpp,
	cred_t *cr,
	int flags,
	caller_context_t *ct,
	vsecattr_t *vsecp)	/* ACL to set during create */
{
	int ret;

	if (vsecp != NULL &&
	    vfs_has_feature(dvp->v_vfsp, VFSFT_ACLONCREATE) == 0) {
		return (EINVAL);
	}
	/*
	 * If this file system doesn't support case-insensitive access
	 * and said access is requested, fail quickly.
	 */
	if (flags & FIGNORECASE &&
	    (vfs_has_feature(dvp->v_vfsp, VFSFT_CASEINSENSITIVE) == 0 &&
	    vfs_has_feature(dvp->v_vfsp, VFSFT_NOCASESENSITIVE) == 0))
		return (EINVAL);

	VOPXID_MAP_CR(dvp, cr);

	ret = (*(dvp)->v_op->vop_create)
	    (dvp, name, vap, excl, mode, vpp, cr, flags, ct, vsecp);
	if (ret == 0 && *vpp) {
		VOPSTATS_UPDATE(*vpp, create);
		if ((*vpp)->v_path == NULL) {
			vn_setpath(rootdir, dvp, *vpp, name, strlen(name));
		}
	}

	return (ret);
}

int
fop_remove(
	vnode_t *dvp,
	char *nm,
	cred_t *cr,
	caller_context_t *ct,
	int flags)
{
	int	err;

	/*
	 * If this file system doesn't support case-insensitive access
	 * and said access is requested, fail quickly.
	 */
	if (flags & FIGNORECASE &&
	    (vfs_has_feature(dvp->v_vfsp, VFSFT_CASEINSENSITIVE) == 0 &&
	    vfs_has_feature(dvp->v_vfsp, VFSFT_NOCASESENSITIVE) == 0))
		return (EINVAL);

	VOPXID_MAP_CR(dvp, cr);

	err = (*(dvp)->v_op->vop_remove)(dvp, nm, cr, ct, flags);
	VOPSTATS_UPDATE(dvp, remove);
	return (err);
}

int
fop_link(
	vnode_t *tdvp,
	vnode_t *svp,
	char *tnm,
	cred_t *cr,
	caller_context_t *ct,
	int flags)
{
	int	err;

	/*
	 * If the target file system doesn't support case-insensitive access
	 * and said access is requested, fail quickly.
	 */
	if (flags & FIGNORECASE &&
	    (vfs_has_feature(tdvp->v_vfsp, VFSFT_CASEINSENSITIVE) == 0 &&
	    vfs_has_feature(tdvp->v_vfsp, VFSFT_NOCASESENSITIVE) == 0))
		return (EINVAL);

	VOPXID_MAP_CR(tdvp, cr);

	err = (*(tdvp)->v_op->vop_link)(tdvp, svp, tnm, cr, ct, flags);
	VOPSTATS_UPDATE(tdvp, link);
	return (err);
}

int
fop_rename(
	vnode_t *sdvp,
	char *snm,
	vnode_t *tdvp,
	char *tnm,
	cred_t *cr,
	caller_context_t *ct,
	int flags)
{
	int	err;

	/*
	 * If the file system involved does not support
	 * case-insensitive access and said access is requested, fail
	 * quickly.
	 */
	if (flags & FIGNORECASE &&
	    ((vfs_has_feature(sdvp->v_vfsp, VFSFT_CASEINSENSITIVE) == 0 &&
	    vfs_has_feature(sdvp->v_vfsp, VFSFT_NOCASESENSITIVE) == 0)))
		return (EINVAL);

	VOPXID_MAP_CR(tdvp, cr);

	err = (*(sdvp)->v_op->vop_rename)(sdvp, snm, tdvp, tnm, cr, ct, flags);
	VOPSTATS_UPDATE(sdvp, rename);
	return (err);
}

int
fop_mkdir(
	vnode_t *dvp,
	char *dirname,
	vattr_t *vap,
	vnode_t **vpp,
	cred_t *cr,
	caller_context_t *ct,
	int flags,
	vsecattr_t *vsecp)	/* ACL to set during create */
{
	int ret;

	if (vsecp != NULL &&
	    vfs_has_feature(dvp->v_vfsp, VFSFT_ACLONCREATE) == 0) {
		return (EINVAL);
	}
	/*
	 * If this file system doesn't support case-insensitive access
	 * and said access is requested, fail quickly.
	 */
	if (flags & FIGNORECASE &&
	    (vfs_has_feature(dvp->v_vfsp, VFSFT_CASEINSENSITIVE) == 0 &&
	    vfs_has_feature(dvp->v_vfsp, VFSFT_NOCASESENSITIVE) == 0))
		return (EINVAL);

	VOPXID_MAP_CR(dvp, cr);

	ret = (*(dvp)->v_op->vop_mkdir)
	    (dvp, dirname, vap, vpp, cr, ct, flags, vsecp);
	if (ret == 0 && *vpp) {
		VOPSTATS_UPDATE(*vpp, mkdir);
		if ((*vpp)->v_path == NULL) {
			vn_setpath(rootdir, dvp, *vpp, dirname,
			    strlen(dirname));
		}
	}

	return (ret);
}

int
fop_rmdir(
	vnode_t *dvp,
	char *nm,
	vnode_t *cdir,
	cred_t *cr,
	caller_context_t *ct,
	int flags)
{
	int	err;

	/*
	 * If this file system doesn't support case-insensitive access
	 * and said access is requested, fail quickly.
	 */
	if (flags & FIGNORECASE &&
	    (vfs_has_feature(dvp->v_vfsp, VFSFT_CASEINSENSITIVE) == 0 &&
	    vfs_has_feature(dvp->v_vfsp, VFSFT_NOCASESENSITIVE) == 0))
		return (EINVAL);

	VOPXID_MAP_CR(dvp, cr);

	err = (*(dvp)->v_op->vop_rmdir)(dvp, nm, cdir, cr, ct, flags);
	VOPSTATS_UPDATE(dvp, rmdir);
	return (err);
}

int
fop_readdir(
	vnode_t *vp,
	uio_t *uiop,
	cred_t *cr,
	int *eofp,
	caller_context_t *ct,
	int flags)
{
	int	err;
	ssize_t	resid_start = uiop->uio_resid;

	/*
	 * If this file system doesn't support retrieving directory
	 * entry flags and said access is requested, fail quickly.
	 */
	if (flags & V_RDDIR_ENTFLAGS &&
	    vfs_has_feature(vp->v_vfsp, VFSFT_DIRENTFLAGS) == 0)
		return (EINVAL);

	VOPXID_MAP_CR(vp, cr);

	err = (*(vp)->v_op->vop_readdir)(vp, uiop, cr, eofp, ct, flags);
	VOPSTATS_UPDATE_IO(vp, readdir,
	    readdir_bytes, (resid_start - uiop->uio_resid));
	return (err);
}

int
fop_symlink(
	vnode_t *dvp,
	char *linkname,
	vattr_t *vap,
	char *target,
	cred_t *cr,
	caller_context_t *ct,
	int flags)
{
	int	err;
	xvattr_t xvattr;

	/*
	 * If this file system doesn't support case-insensitive access
	 * and said access is requested, fail quickly.
	 */
	if (flags & FIGNORECASE &&
	    (vfs_has_feature(dvp->v_vfsp, VFSFT_CASEINSENSITIVE) == 0 &&
	    vfs_has_feature(dvp->v_vfsp, VFSFT_NOCASESENSITIVE) == 0))
		return (EINVAL);

	VOPXID_MAP_CR(dvp, cr);

	/* check for reparse point */
	if ((vfs_has_feature(dvp->v_vfsp, VFSFT_REPARSE)) &&
	    (strncmp(target, FS_REPARSE_TAG_STR,
	    strlen(FS_REPARSE_TAG_STR)) == 0)) {
		if (!fs_reparse_mark(target, vap, &xvattr))
			vap = (vattr_t *)&xvattr;
	}

	err = (*(dvp)->v_op->vop_symlink)
	    (dvp, linkname, vap, target, cr, ct, flags);
	VOPSTATS_UPDATE(dvp, symlink);
	return (err);
}

int
fop_readlink(
	vnode_t *vp,
	uio_t *uiop,
	cred_t *cr,
	caller_context_t *ct)
{
	int	err;

	VOPXID_MAP_CR(vp, cr);

	err = (*(vp)->v_op->vop_readlink)(vp, uiop, cr, ct);
	VOPSTATS_UPDATE(vp, readlink);
	return (err);
}

int
fop_fsync(
	vnode_t *vp,
	int syncflag,
	cred_t *cr,
	caller_context_t *ct)
{
	int	err;

	VOPXID_MAP_CR(vp, cr);

	err = (*(vp)->v_op->vop_fsync)(vp, syncflag, cr, ct);
	VOPSTATS_UPDATE(vp, fsync);
	return (err);
}

void
fop_inactive(
	vnode_t *vp,
	cred_t *cr,
	caller_context_t *ct)
{
	/* Need to update stats before vop call since we may lose the vnode */
	VOPSTATS_UPDATE(vp, inactive);

	VOPXID_MAP_CR(vp, cr);

	(*(vp)->v_op->vop_inactive)(vp, cr, ct);
}

int
fop_fid(
	vnode_t *vp,
	fid_t *fidp,
	caller_context_t *ct)
{
	int	err;

	err = (*(vp)->v_op->vop_fid)(vp, fidp, ct);
	VOPSTATS_UPDATE(vp, fid);
	return (err);
}

int
fop_rwlock(
	vnode_t *vp,
	int write_lock,
	caller_context_t *ct)
{
	int	ret;

	ret = ((*(vp)->v_op->vop_rwlock)(vp, write_lock, ct));
	VOPSTATS_UPDATE(vp, rwlock);
	return (ret);
}

void
fop_rwunlock(
	vnode_t *vp,
	int write_lock,
	caller_context_t *ct)
{
	(*(vp)->v_op->vop_rwunlock)(vp, write_lock, ct);
	VOPSTATS_UPDATE(vp, rwunlock);
}

int
fop_seek(
	vnode_t *vp,
	offset_t ooff,
	offset_t *noffp,
	caller_context_t *ct)
{
	int	err;

	err = (*(vp)->v_op->vop_seek)(vp, ooff, noffp, ct);
	VOPSTATS_UPDATE(vp, seek);
	return (err);
}

int
fop_cmp(
	vnode_t *vp1,
	vnode_t *vp2,
	caller_context_t *ct)
{
	int	err;

	err = (*(vp1)->v_op->vop_cmp)(vp1, vp2, ct);
	VOPSTATS_UPDATE(vp1, cmp);
	return (err);
}

int
fop_frlock(
	vnode_t *vp,
	int cmd,
	flock64_t *bfp,
	int flag,
	offset_t offset,
	struct flk_callback *flk_cbp,
	cred_t *cr,
	caller_context_t *ct)
{
	int	err;

	VOPXID_MAP_CR(vp, cr);

	err = (*(vp)->v_op->vop_frlock)
	    (vp, cmd, bfp, flag, offset, flk_cbp, cr, ct);
	VOPSTATS_UPDATE(vp, frlock);
	return (err);
}

int
fop_space(
	vnode_t *vp,
	int cmd,
	flock64_t *bfp,
	int flag,
	offset_t offset,
	cred_t *cr,
	caller_context_t *ct)
{
	int	err;

	VOPXID_MAP_CR(vp, cr);

	err = (*(vp)->v_op->vop_space)(vp, cmd, bfp, flag, offset, cr, ct);
	VOPSTATS_UPDATE(vp, space);
	return (err);
}

int
fop_realvp(
	vnode_t *vp,
	vnode_t **vpp,
	caller_context_t *ct)
{
	int	err;

	err = (*(vp)->v_op->vop_realvp)(vp, vpp, ct);
	VOPSTATS_UPDATE(vp, realvp);
	return (err);
}

int
fop_getpage(
	vnode_t *vp,
	offset_t off,
	size_t len,
	uint_t *protp,
	page_t **plarr,
	size_t plsz,
	struct seg *seg,
	caddr_t addr,
	enum seg_rw rw,
	cred_t *cr,
	caller_context_t *ct)
{
	int	err;

	VOPXID_MAP_CR(vp, cr);

	err = (*(vp)->v_op->vop_getpage)
	    (vp, off, len, protp, plarr, plsz, seg, addr, rw, cr, ct);
	VOPSTATS_UPDATE(vp, getpage);
	return (err);
}

int
fop_putpage(
	vnode_t *vp,
	offset_t off,
	size_t len,
	int flags,
	cred_t *cr,
	caller_context_t *ct)
{
	int	err;

	VOPXID_MAP_CR(vp, cr);

	err = (*(vp)->v_op->vop_putpage)(vp, off, len, flags, cr, ct);
	VOPSTATS_UPDATE(vp, putpage);
	return (err);
}

int
fop_map(
	vnode_t *vp,
	offset_t off,
	struct as *as,
	caddr_t *addrp,
	size_t len,
	uchar_t prot,
	uchar_t maxprot,
	uint_t flags,
	cred_t *cr,
	caller_context_t *ct)
{
	int	err;

	VOPXID_MAP_CR(vp, cr);

	err = (*(vp)->v_op->vop_map)
	    (vp, off, as, addrp, len, prot, maxprot, flags, cr, ct);
	VOPSTATS_UPDATE(vp, map);
	return (err);
}

int
fop_addmap(
	vnode_t *vp,
	offset_t off,
	struct as *as,
	caddr_t addr,
	size_t len,
	uchar_t prot,
	uchar_t maxprot,
	uint_t flags,
	cred_t *cr,
	caller_context_t *ct)
{
	int error;
	u_longlong_t delta;

	VOPXID_MAP_CR(vp, cr);

	error = (*(vp)->v_op->vop_addmap)
	    (vp, off, as, addr, len, prot, maxprot, flags, cr, ct);

	if ((!error) && (vp->v_type == VREG)) {
		delta = (u_longlong_t)btopr(len);
		/*
		 * If file is declared MAP_PRIVATE, it can't be written back
		 * even if open for write. Handle as read.
		 */
		if (flags & MAP_PRIVATE) {
			atomic_add_64((uint64_t *)(&(vp->v_mmap_read)),
			    (int64_t)delta);
		} else {
			/*
			 * atomic_add_64 forces the fetch of a 64 bit value to
			 * be atomic on 32 bit machines
			 */
			if (maxprot & PROT_WRITE)
				atomic_add_64((uint64_t *)(&(vp->v_mmap_write)),
				    (int64_t)delta);
			if (maxprot & PROT_READ)
				atomic_add_64((uint64_t *)(&(vp->v_mmap_read)),
				    (int64_t)delta);
			if (maxprot & PROT_EXEC)
				atomic_add_64((uint64_t *)(&(vp->v_mmap_read)),
				    (int64_t)delta);
		}
	}
	VOPSTATS_UPDATE(vp, addmap);
	return (error);
}

int
fop_delmap(
	vnode_t *vp,
	offset_t off,
	struct as *as,
	caddr_t addr,
	size_t len,
	uint_t prot,
	uint_t maxprot,
	uint_t flags,
	cred_t *cr,
	caller_context_t *ct)
{
	int error;
	u_longlong_t delta;

	VOPXID_MAP_CR(vp, cr);

	error = (*(vp)->v_op->vop_delmap)
	    (vp, off, as, addr, len, prot, maxprot, flags, cr, ct);

	/*
	 * NFS calls into delmap twice, the first time
	 * it simply establishes a callback mechanism and returns EAGAIN
	 * while the real work is being done upon the second invocation.
	 * We have to detect this here and only decrement the counts upon
	 * the second delmap request.
	 */
	if ((error != EAGAIN) && (vp->v_type == VREG)) {

		delta = (u_longlong_t)btopr(len);

		if (flags & MAP_PRIVATE) {
			atomic_add_64((uint64_t *)(&(vp->v_mmap_read)),
			    (int64_t)(-delta));
		} else {
			/*
			 * atomic_add_64 forces the fetch of a 64 bit value
			 * to be atomic on 32 bit machines
			 */
			if (maxprot & PROT_WRITE)
				atomic_add_64((uint64_t *)(&(vp->v_mmap_write)),
				    (int64_t)(-delta));
			if (maxprot & PROT_READ)
				atomic_add_64((uint64_t *)(&(vp->v_mmap_read)),
				    (int64_t)(-delta));
			if (maxprot & PROT_EXEC)
				atomic_add_64((uint64_t *)(&(vp->v_mmap_read)),
				    (int64_t)(-delta));
		}
	}
	VOPSTATS_UPDATE(vp, delmap);
	return (error);
}


int
fop_poll(
	vnode_t *vp,
	short events,
	int anyyet,
	short *reventsp,
	struct pollhead **phpp,
	caller_context_t *ct)
{
	int	err;

	err = (*(vp)->v_op->vop_poll)(vp, events, anyyet, reventsp, phpp, ct);
	VOPSTATS_UPDATE(vp, poll);
	return (err);
}

int
fop_dump(
	vnode_t *vp,
	caddr_t addr,
	offset_t lbdn,
	offset_t dblks,
	caller_context_t *ct)
{
	int	err;

	/* ensure lbdn and dblks can be passed safely to bdev_dump */
	if ((lbdn != (daddr_t)lbdn) || (dblks != (int)dblks))
		return (EIO);

	err = (*(vp)->v_op->vop_dump)(vp, addr, lbdn, dblks, ct);
	VOPSTATS_UPDATE(vp, dump);
	return (err);
}

int
fop_pathconf(
	vnode_t *vp,
	int cmd,
	ulong_t *valp,
	cred_t *cr,
	caller_context_t *ct)
{
	int	err;

	VOPXID_MAP_CR(vp, cr);

	err = (*(vp)->v_op->vop_pathconf)(vp, cmd, valp, cr, ct);
	VOPSTATS_UPDATE(vp, pathconf);
	return (err);
}

int
fop_pageio(
	vnode_t *vp,
	struct page *pp,
	u_offset_t io_off,
	size_t io_len,
	int flags,
	cred_t *cr,
	caller_context_t *ct)
{
	int	err;

	VOPXID_MAP_CR(vp, cr);

	err = (*(vp)->v_op->vop_pageio)(vp, pp, io_off, io_len, flags, cr, ct);
	VOPSTATS_UPDATE(vp, pageio);
	return (err);
}

int
fop_dumpctl(
	vnode_t *vp,
	int action,
	offset_t *blkp,
	caller_context_t *ct)
{
	int	err;
	err = (*(vp)->v_op->vop_dumpctl)(vp, action, blkp, ct);
	VOPSTATS_UPDATE(vp, dumpctl);
	return (err);
}

void
fop_dispose(
	vnode_t *vp,
	page_t *pp,
	int flag,
	int dn,
	cred_t *cr,
	caller_context_t *ct)
{
	/* Must do stats first since it's possible to lose the vnode */
	VOPSTATS_UPDATE(vp, dispose);

	VOPXID_MAP_CR(vp, cr);

	(*(vp)->v_op->vop_dispose)(vp, pp, flag, dn, cr, ct);
}

int
fop_setsecattr(
	vnode_t *vp,
	vsecattr_t *vsap,
	int flag,
	cred_t *cr,
	caller_context_t *ct)
{
	int	err;

	VOPXID_MAP_CR(vp, cr);

	/*
	 * We're only allowed to skip the ACL check iff we used a 32 bit
	 * ACE mask with VOP_ACCESS() to determine permissions.
	 */
	if ((flag & ATTR_NOACLCHECK) &&
	    vfs_has_feature(vp->v_vfsp, VFSFT_ACEMASKONACCESS) == 0) {
		return (EINVAL);
	}
	err = (*(vp)->v_op->vop_setsecattr) (vp, vsap, flag, cr, ct);
	VOPSTATS_UPDATE(vp, setsecattr);
	return (err);
}

int
fop_getsecattr(
	vnode_t *vp,
	vsecattr_t *vsap,
	int flag,
	cred_t *cr,
	caller_context_t *ct)
{
	int	err;

	/*
	 * We're only allowed to skip the ACL check iff we used a 32 bit
	 * ACE mask with VOP_ACCESS() to determine permissions.
	 */
	if ((flag & ATTR_NOACLCHECK) &&
	    vfs_has_feature(vp->v_vfsp, VFSFT_ACEMASKONACCESS) == 0) {
		return (EINVAL);
	}

	VOPXID_MAP_CR(vp, cr);

	err = (*(vp)->v_op->vop_getsecattr) (vp, vsap, flag, cr, ct);
	VOPSTATS_UPDATE(vp, getsecattr);
	return (err);
}

int
fop_shrlock(
	vnode_t *vp,
	int cmd,
	struct shrlock *shr,
	int flag,
	cred_t *cr,
	caller_context_t *ct)
{
	int	err;

	VOPXID_MAP_CR(vp, cr);

	err = (*(vp)->v_op->vop_shrlock)(vp, cmd, shr, flag, cr, ct);
	VOPSTATS_UPDATE(vp, shrlock);
	return (err);
}

int
fop_vnevent(vnode_t *vp, vnevent_t vnevent, vnode_t *dvp, char *fnm,
    caller_context_t *ct)
{
	int	err;

	err = (*(vp)->v_op->vop_vnevent)(vp, vnevent, dvp, fnm, ct);
	VOPSTATS_UPDATE(vp, vnevent);
	return (err);
}

int
fop_reqzcbuf(vnode_t *vp, enum uio_rw ioflag, xuio_t *uiop, cred_t *cr,
    caller_context_t *ct)
{
	int err;

	if (vfs_has_feature(vp->v_vfsp, VFSFT_ZEROCOPY_SUPPORTED) == 0)
		return (ENOTSUP);
	err = (*(vp)->v_op->vop_reqzcbuf)(vp, ioflag, uiop, cr, ct);
	VOPSTATS_UPDATE(vp, reqzcbuf);
	return (err);
}

int
fop_retzcbuf(vnode_t *vp, xuio_t *uiop, cred_t *cr, caller_context_t *ct)
{
	int err;

	if (vfs_has_feature(vp->v_vfsp, VFSFT_ZEROCOPY_SUPPORTED) == 0)
		return (ENOTSUP);
	err = (*(vp)->v_op->vop_retzcbuf)(vp, uiop, cr, ct);
	VOPSTATS_UPDATE(vp, retzcbuf);
	return (err);
}

/*
 * Default destructor
 *	Needed because NULL destructor means that the key is unused
 */
/* ARGSUSED */
void
vsd_defaultdestructor(void *value)
{}

/*
 * Create a key (index into per vnode array)
 *	Locks out vsd_create, vsd_destroy, and vsd_free
 *	May allocate memory with lock held
 */
void
vsd_create(uint_t *keyp, void (*destructor)(void *))
{
	int	i;
	uint_t	nkeys;

	/*
	 * if key is allocated, do nothing
	 */
	mutex_enter(&vsd_lock);
	if (*keyp) {
		mutex_exit(&vsd_lock);
		return;
	}
	/*
	 * find an unused key
	 */
	if (destructor == NULL)
		destructor = vsd_defaultdestructor;

	for (i = 0; i < vsd_nkeys; ++i)
		if (vsd_destructor[i] == NULL)
			break;

	/*
	 * if no unused keys, increase the size of the destructor array
	 */
	if (i == vsd_nkeys) {
		if ((nkeys = (vsd_nkeys << 1)) == 0)
			nkeys = 1;
		vsd_destructor =
		    (void (**)(void *))vsd_realloc((void *)vsd_destructor,
		    (size_t)(vsd_nkeys * sizeof (void (*)(void *))),
		    (size_t)(nkeys * sizeof (void (*)(void *))));
		vsd_nkeys = nkeys;
	}

	/*
	 * allocate the next available unused key
	 */
	vsd_destructor[i] = destructor;
	*keyp = i + 1;

	/* create vsd_list, if it doesn't exist */
	if (vsd_list == NULL) {
		vsd_list = kmem_alloc(sizeof (list_t), KM_SLEEP);
		list_create(vsd_list, sizeof (struct vsd_node),
		    offsetof(struct vsd_node, vs_nodes));
	}

	mutex_exit(&vsd_lock);
}

/*
 * Destroy a key
 *
 * Assumes that the caller is preventing vsd_set and vsd_get
 * Locks out vsd_create, vsd_destroy, and vsd_free
 * May free memory with lock held
 */
void
vsd_destroy(uint_t *keyp)
{
	uint_t key;
	struct vsd_node *vsd;

	/*
	 * protect the key namespace and our destructor lists
	 */
	mutex_enter(&vsd_lock);
	key = *keyp;
	*keyp = 0;

	ASSERT(key <= vsd_nkeys);

	/*
	 * if the key is valid
	 */
	if (key != 0) {
		uint_t k = key - 1;
		/*
		 * for every vnode with VSD, call key's destructor
		 */
		for (vsd = list_head(vsd_list); vsd != NULL;
		    vsd = list_next(vsd_list, vsd)) {
			/*
			 * no VSD for key in this vnode
			 */
			if (key > vsd->vs_nkeys)
				continue;
			/*
			 * call destructor for key
			 */
			if (vsd->vs_value[k] && vsd_destructor[k])
				(*vsd_destructor[k])(vsd->vs_value[k]);
			/*
			 * reset value for key
			 */
			vsd->vs_value[k] = NULL;
		}
		/*
		 * actually free the key (NULL destructor == unused)
		 */
		vsd_destructor[k] = NULL;
	}

	mutex_exit(&vsd_lock);
}

/*
 * Quickly return the per vnode value that was stored with the specified key
 * Assumes the caller is protecting key from vsd_create and vsd_destroy
 * Assumes the caller is holding v_vsd_lock to protect the vsd.
 */
void *
vsd_get(vnode_t *vp, uint_t key)
{
	struct vsd_node *vsd;

	ASSERT(vp != NULL);
	ASSERT(mutex_owned(&vp->v_vsd_lock));

	vsd = vp->v_vsd;

	if (key && vsd != NULL && key <= vsd->vs_nkeys)
		return (vsd->vs_value[key - 1]);
	return (NULL);
}

/*
 * Set a per vnode value indexed with the specified key
 * Assumes the caller is holding v_vsd_lock to protect the vsd.
 */
int
vsd_set(vnode_t *vp, uint_t key, void *value)
{
	struct vsd_node *vsd;

	ASSERT(vp != NULL);
	ASSERT(mutex_owned(&vp->v_vsd_lock));

	if (key == 0)
		return (EINVAL);

	vsd = vp->v_vsd;
	if (vsd == NULL)
		vsd = vp->v_vsd = kmem_zalloc(sizeof (*vsd), KM_SLEEP);

	/*
	 * If the vsd was just allocated, vs_nkeys will be 0, so the following
	 * code won't happen and we will continue down and allocate space for
	 * the vs_value array.
	 * If the caller is replacing one value with another, then it is up
	 * to the caller to free/rele/destroy the previous value (if needed).
	 */
	if (key <= vsd->vs_nkeys) {
		vsd->vs_value[key - 1] = value;
		return (0);
	}

	ASSERT(key <= vsd_nkeys);

	if (vsd->vs_nkeys == 0) {
		mutex_enter(&vsd_lock);	/* lock out vsd_destroy() */
		/*
		 * Link onto list of all VSD nodes.
		 */
		list_insert_head(vsd_list, vsd);
		mutex_exit(&vsd_lock);
	}

	/*
	 * Allocate vnode local storage and set the value for key
	 */
	vsd->vs_value = vsd_realloc(vsd->vs_value,
	    vsd->vs_nkeys * sizeof (void *),
	    key * sizeof (void *));
	vsd->vs_nkeys = key;
	vsd->vs_value[key - 1] = value;

	return (0);
}

/*
 * Called from vn_free() to run the destructor function for each vsd
 *	Locks out vsd_create and vsd_destroy
 *	Assumes that the destructor *DOES NOT* use vsd
 */
void
vsd_free(vnode_t *vp)
{
	int i;
	struct vsd_node *vsd = vp->v_vsd;

	if (vsd == NULL)
		return;

	if (vsd->vs_nkeys == 0) {
		kmem_free(vsd, sizeof (*vsd));
		vp->v_vsd = NULL;
		return;
	}

	/*
	 * lock out vsd_create and vsd_destroy, call
	 * the destructor, and mark the value as destroyed.
	 */
	mutex_enter(&vsd_lock);

	for (i = 0; i < vsd->vs_nkeys; i++) {
		if (vsd->vs_value[i] && vsd_destructor[i])
			(*vsd_destructor[i])(vsd->vs_value[i]);
		vsd->vs_value[i] = NULL;
	}

	/*
	 * remove from linked list of VSD nodes
	 */
	list_remove(vsd_list, vsd);

	mutex_exit(&vsd_lock);

	/*
	 * free up the VSD
	 */
	kmem_free(vsd->vs_value, vsd->vs_nkeys * sizeof (void *));
	kmem_free(vsd, sizeof (struct vsd_node));
	vp->v_vsd = NULL;
}

/*
 * realloc
 */
static void *
vsd_realloc(void *old, size_t osize, size_t nsize)
{
	void *new;

	new = kmem_zalloc(nsize, KM_SLEEP);
	if (old) {
		bcopy(old, new, osize);
		kmem_free(old, osize);
	}
	return (new);
}

/*
 * Setup the extensible system attribute for creating a reparse point.
 * The symlink data 'target' is validated for proper format of a reparse
 * string and a check also made to make sure the symlink data does not
 * point to an existing file.
 *
 * return 0 if ok else -1.
 */
static int
fs_reparse_mark(char *target, vattr_t *vap, xvattr_t *xvattr)
{
	xoptattr_t *xoap;

	if ((!target) || (!vap) || (!xvattr))
		return (-1);

	/* validate reparse string */
	if (reparse_validate((const char *)target))
		return (-1);

	xva_init(xvattr);
	xvattr->xva_vattr = *vap;
	xvattr->xva_vattr.va_mask |= AT_XVATTR;
	xoap = xva_getxoptattr(xvattr);
	ASSERT(xoap);
	XVA_SET_REQ(xvattr, XAT_REPARSE);
	xoap->xoa_reparse = 1;

	return (0);
}

/*
 * Function to check whether a symlink is a reparse point.
 * Return B_TRUE if it is a reparse point, else return B_FALSE
 */
boolean_t
vn_is_reparse(vnode_t *vp, cred_t *cr, caller_context_t *ct)
{
	xvattr_t xvattr;
	xoptattr_t *xoap;

	if ((vp->v_type != VLNK) ||
	    !(vfs_has_feature(vp->v_vfsp, VFSFT_XVATTR)))
		return (B_FALSE);

	xva_init(&xvattr);
	xoap = xva_getxoptattr(&xvattr);
	ASSERT(xoap);
	XVA_SET_REQ(&xvattr, XAT_REPARSE);

	if (VOP_GETATTR(vp, &xvattr.xva_vattr, 0, cr, ct))
		return (B_FALSE);

	if ((!(xvattr.xva_vattr.va_mask & AT_XVATTR)) ||
	    (!(XVA_ISSET_RTN(&xvattr, XAT_REPARSE))))
		return (B_FALSE);

	return (xoap->xoa_reparse ? B_TRUE : B_FALSE);
}
