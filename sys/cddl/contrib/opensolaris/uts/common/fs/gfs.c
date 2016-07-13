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
/* Portions Copyright 2007 Shivakumar GN */
/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/dirent.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/sunddi.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/cred.h>

#include <sys/gfs.h>

/*
 * Generic pseudo-filesystem routines.
 *
 * There are significant similarities between the implementation of certain file
 * system entry points across different filesystems.  While one could attempt to
 * "choke up on the bat" and incorporate common functionality into a VOP
 * preamble or postamble, such an approach is limited in the benefit it can
 * provide.  In this file we instead define a toolkit of routines which can be
 * called from a filesystem (with in-kernel pseudo-filesystems being the focus
 * of the exercise) in a more component-like fashion.
 *
 * There are three basic classes of routines:
 *
 * 1) Lowlevel support routines
 *
 *    These routines are designed to play a support role for existing
 *    pseudo-filesystems (such as procfs).  They simplify common tasks,
 *    without forcing the filesystem to hand over management to GFS.  The
 *    routines covered are:
 *
 *	gfs_readdir_init()
 *	gfs_readdir_emit()
 *	gfs_readdir_emitn()
 *	gfs_readdir_pred()
 *	gfs_readdir_fini()
 *	gfs_lookup_dot()
 *
 * 2) Complete GFS management
 *
 *    These routines take a more active role in management of the
 *    pseudo-filesystem.  They handle the relationship between vnode private
 *    data and VFS data, as well as the relationship between vnodes in the
 *    directory hierarchy.
 *
 *    In order to use these interfaces, the first member of every private
 *    v_data must be a gfs_file_t or a gfs_dir_t.  This hands over all control
 *    to GFS.
 *
 * 	gfs_file_create()
 * 	gfs_dir_create()
 * 	gfs_root_create()
 *
 *	gfs_file_inactive()
 *	gfs_dir_inactive()
 *	gfs_dir_lookup()
 *	gfs_dir_readdir()
 *
 * 	gfs_vop_inactive()
 * 	gfs_vop_lookup()
 * 	gfs_vop_readdir()
 * 	gfs_vop_map()
 *
 * 3) Single File pseudo-filesystems
 *
 *    This routine creates a rooted file to be overlayed ontop of another
 *    file in the physical filespace.
 *
 *    Note that the parent is NULL (actually the vfs), but there is nothing
 *    technically keeping such a file from utilizing the "Complete GFS
 *    management" set of routines.
 *
 * 	gfs_root_create_file()
 */

#ifdef sun
/*
 * gfs_make_opsvec: take an array of vnode type definitions and create
 * their vnodeops_t structures
 *
 * This routine takes an array of gfs_opsvec_t's.  It could
 * alternatively take an array of gfs_opsvec_t*'s, which would allow
 * vnode types to be completely defined in files external to the caller
 * of gfs_make_opsvec().  As it stands, much more sharing takes place --
 * both the caller and the vnode type provider need to access gfsv_ops
 * and gfsv_template, and the caller also needs to know gfsv_name.
 */
int
gfs_make_opsvec(gfs_opsvec_t *vec)
{
	int error, i;

	for (i = 0; ; i++) {
		if (vec[i].gfsv_name == NULL)
			return (0);
		error = vn_make_ops(vec[i].gfsv_name, vec[i].gfsv_template,
		    vec[i].gfsv_ops);
		if (error)
			break;
	}

	cmn_err(CE_WARN, "gfs_make_opsvec: bad vnode ops template for '%s'",
	    vec[i].gfsv_name);
	for (i--; i >= 0; i--) {
		vn_freevnodeops(*vec[i].gfsv_ops);
		*vec[i].gfsv_ops = NULL;
	}
	return (error);
}
#endif	/* sun */

/*
 * Low level directory routines
 *
 * These routines provide some simple abstractions for reading directories.
 * They are designed to be used by existing pseudo filesystems (namely procfs)
 * that already have a complicated management infrastructure.
 */

/*
 * gfs_get_parent_ino: used to obtain a parent inode number and the
 * inode number of the given vnode in preparation for calling gfs_readdir_init.
 */
int
gfs_get_parent_ino(vnode_t *dvp, cred_t *cr, caller_context_t *ct,
    ino64_t *pino, ino64_t *ino)
{
	vnode_t *parent;
	gfs_dir_t *dp = dvp->v_data;
	int error;

	*ino = dp->gfsd_file.gfs_ino;
	parent = dp->gfsd_file.gfs_parent;

	if (parent == NULL) {
		*pino = *ino;		/* root of filesystem */
	} else if (dvp->v_flag & V_XATTRDIR) {
#ifdef TODO
		vattr_t va;

		va.va_mask = AT_NODEID;
		error = VOP_GETATTR(parent, &va, 0, cr, ct);
		if (error)
			return (error);
		*pino = va.va_nodeid;
#else
		panic("%s:%u: not implemented", __func__, __LINE__);
#endif
	} else {
		*pino = ((gfs_file_t *)(parent->v_data))->gfs_ino;
	}

	return (0);
}

/*
 * gfs_readdir_init: initiate a generic readdir
 *   st		- a pointer to an uninitialized gfs_readdir_state_t structure
 *   name_max	- the directory's maximum file name length
 *   ureclen	- the exported file-space record length (1 for non-legacy FSs)
 *   uiop	- the uiop passed to readdir
 *   parent	- the parent directory's inode
 *   self	- this directory's inode
 *   flags	- flags from VOP_READDIR
 *
 * Returns 0 or a non-zero errno.
 *
 * Typical VOP_READDIR usage of gfs_readdir_*:
 *
 *	if ((error = gfs_readdir_init(...)) != 0)
 *		return (error);
 *	eof = 0;
 *	while ((error = gfs_readdir_pred(..., &voffset)) != 0) {
 *		if (!consumer_entry_at(voffset))
 *			voffset = consumer_next_entry(voffset);
 *		if (consumer_eof(voffset)) {
 *			eof = 1
 *			break;
 *		}
 *		if ((error = gfs_readdir_emit(..., voffset,
 *		    consumer_ino(voffset), consumer_name(voffset))) != 0)
 *			break;
 *	}
 *	return (gfs_readdir_fini(..., error, eofp, eof));
 *
 * As you can see, a zero result from gfs_readdir_pred() or
 * gfs_readdir_emit() indicates that processing should continue,
 * whereas a non-zero result indicates that the loop should terminate.
 * Most consumers need do nothing more than let gfs_readdir_fini()
 * determine what the cause of failure was and return the appropriate
 * value.
 */
int
gfs_readdir_init(gfs_readdir_state_t *st, int name_max, int ureclen,
    uio_t *uiop, ino64_t parent, ino64_t self, int flags)
{
	size_t dirent_size;

	if (uiop->uio_loffset < 0 || uiop->uio_resid <= 0 ||
	    (uiop->uio_loffset % ureclen) != 0)
		return (EINVAL);

	st->grd_ureclen = ureclen;
	st->grd_oresid = uiop->uio_resid;
	st->grd_namlen = name_max;
	if (flags & V_RDDIR_ENTFLAGS)
		dirent_size = EDIRENT_RECLEN(st->grd_namlen);
	else
		dirent_size = DIRENT64_RECLEN(st->grd_namlen);
	st->grd_dirent = kmem_zalloc(dirent_size, KM_SLEEP);
	st->grd_parent = parent;
	st->grd_self = self;
	st->grd_flags = flags;

	return (0);
}

/*
 * gfs_readdir_emit_int: internal routine to emit directory entry
 *
 *   st		- the current readdir state, which must have d_ino/ed_ino
 *		  and d_name/ed_name set
 *   uiop	- caller-supplied uio pointer
 *   next	- the offset of the next entry
 */
static int
gfs_readdir_emit_int(gfs_readdir_state_t *st, uio_t *uiop, offset_t next,
    int *ncookies, u_long **cookies)
{
	int reclen, namlen;
	dirent64_t *dp;
	edirent_t *edp;

	if (st->grd_flags & V_RDDIR_ENTFLAGS) {
		edp = st->grd_dirent;
		namlen = strlen(edp->ed_name);
		reclen = EDIRENT_RECLEN(namlen);
	} else {
		dp = st->grd_dirent;
		namlen = strlen(dp->d_name);
		reclen = DIRENT64_RECLEN(namlen);
	}

	if (reclen > uiop->uio_resid) {
		/*
		 * Error if no entries were returned yet
		 */
		if (uiop->uio_resid == st->grd_oresid)
			return (EINVAL);
		return (-1);
	}

	if (st->grd_flags & V_RDDIR_ENTFLAGS) {
		edp->ed_off = next;
		edp->ed_reclen = (ushort_t)reclen;
	} else {
		/* XXX: This can change in the future. */
		dp->d_reclen = (ushort_t)reclen;
		dp->d_type = DT_DIR;
		dp->d_namlen = namlen;
	}

	if (uiomove((caddr_t)st->grd_dirent, reclen, UIO_READ, uiop))
		return (EFAULT);

	uiop->uio_loffset = next;
	if (*cookies != NULL) {
		**cookies = next;
		(*cookies)++;
		(*ncookies)--;
		KASSERT(*ncookies >= 0, ("ncookies=%d", *ncookies));
	}

	return (0);
}

/*
 * gfs_readdir_emit: emit a directory entry
 *   voff       - the virtual offset (obtained from gfs_readdir_pred)
 *   ino        - the entry's inode
 *   name       - the entry's name
 *   eflags	- value for ed_eflags (if processing edirent_t)
 *
 * Returns a 0 on success, a non-zero errno on failure, or -1 if the
 * readdir loop should terminate.  A non-zero result (either errno or
 * -1) from this function is typically passed directly to
 * gfs_readdir_fini().
 */
int
gfs_readdir_emit(gfs_readdir_state_t *st, uio_t *uiop, offset_t voff,
    ino64_t ino, const char *name, int eflags, int *ncookies, u_long **cookies)
{
	offset_t off = (voff + 2) * st->grd_ureclen;

	if (st->grd_flags & V_RDDIR_ENTFLAGS) {
		edirent_t *edp = st->grd_dirent;

		edp->ed_ino = ino;
		(void) strncpy(edp->ed_name, name, st->grd_namlen);
		edp->ed_eflags = eflags;
	} else {
		dirent64_t *dp = st->grd_dirent;

		dp->d_ino = ino;
		(void) strncpy(dp->d_name, name, st->grd_namlen);
	}

	/*
	 * Inter-entry offsets are invalid, so we assume a record size of
	 * grd_ureclen and explicitly set the offset appropriately.
	 */
	return (gfs_readdir_emit_int(st, uiop, off + st->grd_ureclen, ncookies,
	    cookies));
}

#ifdef sun
/*
 * gfs_readdir_emitn: like gfs_readdir_emit(), but takes an integer
 * instead of a string for the entry's name.
 */
int
gfs_readdir_emitn(gfs_readdir_state_t *st, uio_t *uiop, offset_t voff,
    ino64_t ino, unsigned long num)
{
	char buf[40];

	numtos(num, buf);
	return (gfs_readdir_emit(st, uiop, voff, ino, buf, 0));
}
#endif

/*
 * gfs_readdir_pred: readdir loop predicate
 *   voffp - a pointer in which the next virtual offset should be stored
 *
 * Returns a 0 on success, a non-zero errno on failure, or -1 if the
 * readdir loop should terminate.  A non-zero result (either errno or
 * -1) from this function is typically passed directly to
 * gfs_readdir_fini().
 */
int
gfs_readdir_pred(gfs_readdir_state_t *st, uio_t *uiop, offset_t *voffp,
    int *ncookies, u_long **cookies)
{
	offset_t off, voff;
	int error;

top:
	if (uiop->uio_resid <= 0)
		return (-1);

	off = uiop->uio_loffset / st->grd_ureclen;
	voff = off - 2;
	if (off == 0) {
		if ((error = gfs_readdir_emit(st, uiop, voff, st->grd_self,
		    ".", 0, ncookies, cookies)) == 0)
			goto top;
	} else if (off == 1) {
		if ((error = gfs_readdir_emit(st, uiop, voff, st->grd_parent,
		    "..", 0, ncookies, cookies)) == 0)
			goto top;
	} else {
		*voffp = voff;
		return (0);
	}

	return (error);
}

/*
 * gfs_readdir_fini: generic readdir cleanup
 *   error	- if positive, an error to return
 *   eofp	- the eofp passed to readdir
 *   eof	- the eof value
 *
 * Returns a 0 on success, a non-zero errno on failure.  This result
 * should be returned from readdir.
 */
int
gfs_readdir_fini(gfs_readdir_state_t *st, int error, int *eofp, int eof)
{
	size_t dirent_size;

	if (st->grd_flags & V_RDDIR_ENTFLAGS)
		dirent_size = EDIRENT_RECLEN(st->grd_namlen);
	else
		dirent_size = DIRENT64_RECLEN(st->grd_namlen);
	kmem_free(st->grd_dirent, dirent_size);
	if (error > 0)
		return (error);
	if (eofp)
		*eofp = eof;
	return (0);
}

/*
 * gfs_lookup_dot
 *
 * Performs a basic check for "." and ".." directory entries.
 */
int
gfs_lookup_dot(vnode_t **vpp, vnode_t *dvp, vnode_t *pvp, const char *nm)
{
	if (*nm == '\0' || strcmp(nm, ".") == 0) {
		VN_HOLD(dvp);
		*vpp = dvp;
		return (0);
	} else if (strcmp(nm, "..") == 0) {
		ASSERT(pvp != NULL);
		VN_HOLD(pvp);
		*vpp = pvp;
		vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY);
		return (0);
	}

	return (-1);
}

/*
 * gfs_file_create(): create a new GFS file
 *
 *   size	- size of private data structure (v_data)
 *   pvp	- parent vnode (GFS directory)
 *   ops	- vnode operations vector
 *
 * In order to use this interface, the parent vnode must have been created by
 * gfs_dir_create(), and the private data stored in v_data must have a
 * 'gfs_file_t' as its first field.
 *
 * Given these constraints, this routine will automatically:
 *
 * 	- Allocate v_data for the vnode
 * 	- Initialize necessary fields in the vnode
 * 	- Hold the parent
 */
vnode_t *
gfs_file_create(size_t size, vnode_t *pvp, vfs_t *vfsp, vnodeops_t *ops)
{
	gfs_file_t *fp;
	vnode_t *vp;
	int error;

	/*
	 * Allocate vnode and internal data structure
	 */
	fp = kmem_zalloc(size, KM_SLEEP);
	error = getnewvnode("zfs", vfsp, ops, &vp);
	ASSERT(error == 0);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	vp->v_data = (caddr_t)fp;

	/*
	 * Set up various pointers
	 */
	fp->gfs_vnode = vp;
	fp->gfs_parent = pvp;
	fp->gfs_size = size;
	fp->gfs_type = GFS_FILE;

	vp->v_vflag |= VV_FORCEINSMQ;
	error = insmntque(vp, vfsp);
	vp->v_vflag &= ~VV_FORCEINSMQ;
	KASSERT(error == 0, ("insmntque() failed: error %d", error));

	/*
	 * Initialize vnode and hold parent.
	 */
	if (pvp)
		VN_HOLD(pvp);

	return (vp);
}

/*
 * gfs_dir_create: creates a new directory in the parent
 *
 *   size	- size of private data structure (v_data)
 *   pvp	- parent vnode (GFS directory)
 *   ops	- vnode operations vector
 *   entries	- NULL-terminated list of static entries (if any)
 *   maxlen	- maximum length of a directory entry
 *   readdir_cb	- readdir callback (see gfs_dir_readdir)
 *   inode_cb	- inode callback (see gfs_dir_readdir)
 *   lookup_cb	- lookup callback (see gfs_dir_lookup)
 *
 * In order to use this function, the first member of the private vnode
 * structure (v_data) must be a gfs_dir_t.  For each directory, there are
 * static entries, defined when the structure is initialized, and dynamic
 * entries, retrieved through callbacks.
 *
 * If a directory has static entries, then it must supply a inode callback,
 * which will compute the inode number based on the parent and the index.
 * For a directory with dynamic entries, the caller must supply a readdir
 * callback and a lookup callback.  If a static lookup fails, we fall back to
 * the supplied lookup callback, if any.
 *
 * This function also performs the same initialization as gfs_file_create().
 */
vnode_t *
gfs_dir_create(size_t struct_size, vnode_t *pvp, vfs_t *vfsp, vnodeops_t *ops,
    gfs_dirent_t *entries, gfs_inode_cb inode_cb, int maxlen,
    gfs_readdir_cb readdir_cb, gfs_lookup_cb lookup_cb)
{
	vnode_t *vp;
	gfs_dir_t *dp;
	gfs_dirent_t *de;

	vp = gfs_file_create(struct_size, pvp, vfsp, ops);
	vp->v_type = VDIR;

	dp = vp->v_data;
	dp->gfsd_file.gfs_type = GFS_DIR;
	dp->gfsd_maxlen = maxlen;

	if (entries != NULL) {
		for (de = entries; de->gfse_name != NULL; de++)
			dp->gfsd_nstatic++;

		dp->gfsd_static = kmem_alloc(
		    dp->gfsd_nstatic * sizeof (gfs_dirent_t), KM_SLEEP);
		bcopy(entries, dp->gfsd_static,
		    dp->gfsd_nstatic * sizeof (gfs_dirent_t));
	}

	dp->gfsd_readdir = readdir_cb;
	dp->gfsd_lookup = lookup_cb;
	dp->gfsd_inode = inode_cb;

	mutex_init(&dp->gfsd_lock, NULL, MUTEX_DEFAULT, NULL);

	return (vp);
}

/*
 * gfs_root_create(): create a root vnode for a GFS filesystem
 *
 * Similar to gfs_dir_create(), this creates a root vnode for a filesystem.  The
 * only difference is that it takes a vfs_t instead of a vnode_t as its parent.
 */
vnode_t *
gfs_root_create(size_t size, vfs_t *vfsp, vnodeops_t *ops, ino64_t ino,
    gfs_dirent_t *entries, gfs_inode_cb inode_cb, int maxlen,
    gfs_readdir_cb readdir_cb, gfs_lookup_cb lookup_cb)
{
	vnode_t *vp;

#ifdef illumos
	VFS_HOLD(vfsp);
#endif
	vp = gfs_dir_create(size, NULL, vfsp, ops, entries, inode_cb,
	    maxlen, readdir_cb, lookup_cb);
	/* Manually set the inode */
	((gfs_file_t *)vp->v_data)->gfs_ino = ino;
	vp->v_flag |= VROOT;

	return (vp);
}

#ifdef sun
/*
 * gfs_root_create_file(): create a root vnode for a GFS file as a filesystem
 *
 * Similar to gfs_root_create(), this creates a root vnode for a file to
 * be the pseudo-filesystem.
 */
vnode_t *
gfs_root_create_file(size_t size, vfs_t *vfsp, vnodeops_t *ops, ino64_t ino)
{
	vnode_t	*vp = gfs_file_create(size, NULL, ops);

	((gfs_file_t *)vp->v_data)->gfs_ino = ino;

	VFS_HOLD(vfsp);
	VN_SET_VFS_TYPE_DEV(vp, vfsp, VREG, 0);
	vp->v_flag |= VROOT | VNOCACHE | VNOMAP | VNOSWAP | VNOMOUNT;

	return (vp);
}
#endif	/* sun */

/*
 * gfs_file_inactive()
 *
 * Called from the VOP_INACTIVE() routine.  If necessary, this routine will
 * remove the given vnode from the parent directory and clean up any references
 * in the VFS layer.
 *
 * If the vnode was not removed (due to a race with vget), then NULL is
 * returned.  Otherwise, a pointer to the private data is returned.
 */
void *
gfs_file_inactive(vnode_t *vp)
{
	int i;
	gfs_dirent_t *ge = NULL;
	gfs_file_t *fp = vp->v_data;
	gfs_dir_t *dp = NULL;
	void *data;

	if (fp->gfs_parent == NULL || (vp->v_flag & V_XATTRDIR))
		goto found;

	/*
	 * XXX cope with a FreeBSD-specific race wherein the parent's
	 * snapshot data can be freed before the parent is
	 */
	if ((dp = fp->gfs_parent->v_data) == NULL)
		return (NULL);

	/*
	 * First, see if this vnode is cached in the parent.
	 */
	gfs_dir_lock(dp);

	/*
	 * Find it in the set of static entries.
	 */
	for (i = 0; i < dp->gfsd_nstatic; i++)  {
		ge = &dp->gfsd_static[i];

		if (ge->gfse_vnode == vp)
			goto found;
	}

	/*
	 * If 'ge' is NULL, then it is a dynamic entry.
	 */
	ge = NULL;

found:
#ifdef TODO
	if (vp->v_flag & V_XATTRDIR)
		VI_LOCK(fp->gfs_parent);
#endif
	VI_LOCK(vp);
	/*
	 * Really remove this vnode
	 */
	data = vp->v_data;
	if (ge != NULL) {
		/*
		 * If this was a statically cached entry, simply set the
		 * cached vnode to NULL.
		 */
		ge->gfse_vnode = NULL;
	}
	VI_UNLOCK(vp);

	/*
	 * Free vnode and release parent
	 */
	if (fp->gfs_parent) {
		if (dp)
			gfs_dir_unlock(dp);
		VOP_UNLOCK(vp, 0);
		VN_RELE(fp->gfs_parent);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	} else {
		ASSERT(vp->v_vfsp != NULL);
#ifdef illumos
		VFS_RELE(vp->v_vfsp);
#endif
	}
#ifdef TODO
	if (vp->v_flag & V_XATTRDIR)
		VI_UNLOCK(fp->gfs_parent);
#endif
	return (data);
}

/*
 * gfs_dir_inactive()
 *
 * Same as above, but for directories.
 */
void *
gfs_dir_inactive(vnode_t *vp)
{
	gfs_dir_t *dp;

	ASSERT(vp->v_type == VDIR);

	if ((dp = gfs_file_inactive(vp)) != NULL) {
		mutex_destroy(&dp->gfsd_lock);
		if (dp->gfsd_nstatic)
			kmem_free(dp->gfsd_static,
			    dp->gfsd_nstatic * sizeof (gfs_dirent_t));
	}

	return (dp);
}

/*
 * gfs_dir_lookup_dynamic()
 *
 * This routine looks up the provided name amongst the dynamic entries
 * in the gfs directory and returns the corresponding vnode, if found.
 *
 * The gfs directory is expected to be locked by the caller prior to
 * calling this function.  The directory will be unlocked during the
 * execution of this function, but will be locked upon return from the
 * function.  This function returns 0 on success, non-zero on error.
 *
 * The dynamic lookups are performed by invoking the lookup
 * callback, which is passed to this function as the first argument.
 * The arguments to the callback are:
 *
 * int gfs_lookup_cb(vnode_t *pvp, const char *nm, vnode_t **vpp, cred_t *cr,
 *     int flags, int *deflgs, pathname_t *rpnp);
 *
 *	pvp	- parent vnode
 *	nm	- name of entry
 *	vpp	- pointer to resulting vnode
 *	cr	- pointer to cred
 *	flags	- flags value from lookup request
 *		ignored here; currently only used to request
 *		insensitive lookups
 *	direntflgs - output parameter, directory entry flags
 *		ignored here; currently only used to indicate a lookup
 *		has more than one possible match when case is not considered
 *	realpnp	- output parameter, real pathname
 *		ignored here; when lookup was performed case-insensitively,
 *		this field contains the "real" name of the file.
 *
 * 	Returns 0 on success, non-zero on error.
 */
static int
gfs_dir_lookup_dynamic(gfs_lookup_cb callback, gfs_dir_t *dp,
    const char *nm, vnode_t *dvp, vnode_t **vpp, cred_t *cr, int flags,
    int *direntflags, pathname_t *realpnp)
{
	gfs_file_t *fp;
	ino64_t ino;
	int ret;

	ASSERT(GFS_DIR_LOCKED(dp));

	/*
	 * Drop the directory lock, as the lookup routine
	 * will need to allocate memory, or otherwise deadlock on this
	 * directory.
	 */
	gfs_dir_unlock(dp);
	ret = callback(dvp, nm, vpp, &ino, cr, flags, direntflags, realpnp);
	gfs_dir_lock(dp);

	/*
	 * The callback for extended attributes returns a vnode
	 * with v_data from an underlying fs.
	 */
	if (ret == 0 && !IS_XATTRDIR(dvp)) {
		fp = (gfs_file_t *)((*vpp)->v_data);
		fp->gfs_index = -1;
		fp->gfs_ino = ino;
	}

	return (ret);
}

/*
 * gfs_dir_lookup_static()
 *
 * This routine looks up the provided name amongst the static entries
 * in the gfs directory and returns the corresponding vnode, if found.
 * The first argument to the function is a pointer to the comparison
 * function this function should use to decide if names are a match.
 *
 * If a match is found, and GFS_CACHE_VNODE is set and the vnode
 * exists, we simply return the existing vnode.  Otherwise, we call
 * the static entry's callback routine, caching the result if
 * necessary.  If the idx pointer argument is non-NULL, we use it to
 * return the index of the matching static entry.
 *
 * The gfs directory is expected to be locked by the caller prior to calling
 * this function.  The directory may be unlocked during the execution of
 * this function, but will be locked upon return from the function.
 *
 * This function returns 0 if a match is found, ENOENT if not.
 */
static int
gfs_dir_lookup_static(int (*compare)(const char *, const char *),
    gfs_dir_t *dp, const char *nm, vnode_t *dvp, int *idx,
    vnode_t **vpp, pathname_t *rpnp)
{
	gfs_dirent_t *ge;
	vnode_t *vp = NULL;
	int i;

	ASSERT(GFS_DIR_LOCKED(dp));

	/*
	 * Search static entries.
	 */
	for (i = 0; i < dp->gfsd_nstatic; i++) {
		ge = &dp->gfsd_static[i];

		if (compare(ge->gfse_name, nm) == 0) {
			if (rpnp)
				(void) strlcpy(rpnp->pn_buf, ge->gfse_name,
				    rpnp->pn_bufsize);

			if (ge->gfse_vnode) {
				ASSERT(ge->gfse_flags & GFS_CACHE_VNODE);
				vp = ge->gfse_vnode;
				VN_HOLD(vp);
				break;
			}

			/*
			 * We drop the directory lock, as the constructor will
			 * need to do KM_SLEEP allocations.  If we return from
			 * the constructor only to find that a parallel
			 * operation has completed, and GFS_CACHE_VNODE is set
			 * for this entry, we discard the result in favor of
			 * the cached vnode.
			 */
			gfs_dir_unlock(dp);
			vp = ge->gfse_ctor(dvp);
			gfs_dir_lock(dp);

			((gfs_file_t *)vp->v_data)->gfs_index = i;

			/* Set the inode according to the callback. */
			((gfs_file_t *)vp->v_data)->gfs_ino =
			    dp->gfsd_inode(dvp, i);

			if (ge->gfse_flags & GFS_CACHE_VNODE) {
				if (ge->gfse_vnode == NULL) {
					ge->gfse_vnode = vp;
				} else {
					/*
					 * A parallel constructor beat us to it;
					 * return existing vnode.  We have to be
					 * careful because we can't release the
					 * current vnode while holding the
					 * directory lock; its inactive routine
					 * will try to lock this directory.
					 */
					vnode_t *oldvp = vp;
					vp = ge->gfse_vnode;
					VN_HOLD(vp);

					gfs_dir_unlock(dp);
					VN_RELE(oldvp);
					gfs_dir_lock(dp);
				}
			}
			break;
		}
	}

	if (vp == NULL)
		return (ENOENT);
	else if (idx)
		*idx = i;
	*vpp = vp;
	return (0);
}

/*
 * gfs_dir_lookup()
 *
 * Looks up the given name in the directory and returns the corresponding
 * vnode, if found.
 *
 * First, we search statically defined entries, if any, with a call to
 * gfs_dir_lookup_static().  If no static entry is found, and we have
 * a callback function we try a dynamic lookup via gfs_dir_lookup_dynamic().
 *
 * This function returns 0 on success, non-zero on error.
 */
int
gfs_dir_lookup(vnode_t *dvp, const char *nm, vnode_t **vpp, cred_t *cr,
    int flags, int *direntflags, pathname_t *realpnp)
{
	gfs_dir_t *dp = dvp->v_data;
	boolean_t casecheck;
	vnode_t *dynvp = NULL;
	vnode_t *vp = NULL;
	int (*compare)(const char *, const char *);
	int error, idx;

	ASSERT(dvp->v_type == VDIR);

	if (gfs_lookup_dot(vpp, dvp, dp->gfsd_file.gfs_parent, nm) == 0)
		return (0);

	casecheck = (flags & FIGNORECASE) != 0 && direntflags != NULL;
	if (vfs_has_feature(dvp->v_vfsp, VFSFT_NOCASESENSITIVE) ||
	    (flags & FIGNORECASE))
		compare = strcasecmp;
	else
		compare = strcmp;

	gfs_dir_lock(dp);

	error = gfs_dir_lookup_static(compare, dp, nm, dvp, &idx, &vp, realpnp);

	if (vp && casecheck) {
		gfs_dirent_t *ge;
		int i;

		for (i = idx + 1; i < dp->gfsd_nstatic; i++) {
			ge = &dp->gfsd_static[i];

			if (strcasecmp(ge->gfse_name, nm) == 0) {
				*direntflags |= ED_CASE_CONFLICT;
				goto out;
			}
		}
	}

	if ((error || casecheck) && dp->gfsd_lookup)
		error = gfs_dir_lookup_dynamic(dp->gfsd_lookup, dp, nm, dvp,
		    &dynvp, cr, flags, direntflags, vp ? NULL : realpnp);

	if (vp && dynvp) {
		/* static and dynamic entries are case-insensitive conflict */
		ASSERT(casecheck);
		*direntflags |= ED_CASE_CONFLICT;
		VN_RELE(dynvp);
	} else if (vp == NULL) {
		vp = dynvp;
	} else if (error == ENOENT) {
		error = 0;
	} else if (error) {
		VN_RELE(vp);
		vp = NULL;
	}

out:
	gfs_dir_unlock(dp);

	*vpp = vp;
	return (error);
}

/*
 * gfs_dir_readdir: does a readdir() on the given directory
 *
 *    dvp	- directory vnode
 *    uiop	- uio structure
 *    eofp	- eof pointer
 *    data	- arbitrary data passed to readdir callback
 *
 * This routine does all the readdir() dirty work.  Even so, the caller must
 * supply two callbacks in order to get full compatibility.
 *
 * If the directory contains static entries, an inode callback must be
 * specified.  This avoids having to create every vnode and call VOP_GETATTR()
 * when reading the directory.  This function has the following arguments:
 *
 *	ino_t gfs_inode_cb(vnode_t *vp, int index);
 *
 * 	vp	- vnode for the directory
 * 	index	- index in original gfs_dirent_t array
 *
 * 	Returns the inode number for the given entry.
 *
 * For directories with dynamic entries, a readdir callback must be provided.
 * This is significantly more complex, thanks to the particulars of
 * VOP_READDIR().
 *
 *	int gfs_readdir_cb(vnode_t *vp, void *dp, int *eofp,
 *	    offset_t *off, offset_t *nextoff, void *data, int flags)
 *
 *	vp	- directory vnode
 *	dp	- directory entry, sized according to maxlen given to
 *		  gfs_dir_create().  callback must fill in d_name and
 *		  d_ino (if a dirent64_t), or ed_name, ed_ino, and ed_eflags
 *		  (if an edirent_t). edirent_t is used if V_RDDIR_ENTFLAGS
 *		  is set in 'flags'.
 *	eofp	- callback must set to 1 when EOF has been reached
 *	off	- on entry, the last offset read from the directory.  Callback
 *		  must set to the offset of the current entry, typically left
 *		  untouched.
 *	nextoff	- callback must set to offset of next entry.  Typically
 *		  (off + 1)
 *	data	- caller-supplied data
 *	flags	- VOP_READDIR flags
 *
 *	Return 0 on success, or error on failure.
 */
int
gfs_dir_readdir(vnode_t *dvp, uio_t *uiop, int *eofp, int *ncookies,
    u_long **cookies, void *data, cred_t *cr, int flags)
{
	gfs_readdir_state_t gstate;
	int error, eof = 0;
	ino64_t ino, pino;
	offset_t off, next;
	gfs_dir_t *dp = dvp->v_data;

	error = gfs_get_parent_ino(dvp, cr, NULL, &pino, &ino);
	if (error)
		return (error);

	if ((error = gfs_readdir_init(&gstate, dp->gfsd_maxlen, 1, uiop,
	    pino, ino, flags)) != 0)
		return (error);

	while ((error = gfs_readdir_pred(&gstate, uiop, &off, ncookies,
	    cookies)) == 0 && !eof) {

		if (off >= 0 && off < dp->gfsd_nstatic) {
			ino = dp->gfsd_inode(dvp, off);

			if ((error = gfs_readdir_emit(&gstate, uiop,
			    off, ino, dp->gfsd_static[off].gfse_name, 0,
			    ncookies, cookies)) != 0)
				break;

		} else if (dp->gfsd_readdir) {
			off -= dp->gfsd_nstatic;

			if ((error = dp->gfsd_readdir(dvp,
			    gstate.grd_dirent, &eof, &off, &next,
			    data, flags)) != 0 || eof)
				break;

			off += dp->gfsd_nstatic + 2;
			next += dp->gfsd_nstatic + 2;

			if ((error = gfs_readdir_emit_int(&gstate, uiop,
			    next, ncookies, cookies)) != 0)
				break;
		} else {
			/*
			 * Offset is beyond the end of the static entries, and
			 * we have no dynamic entries.  Set EOF.
			 */
			eof = 1;
		}
	}

	return (gfs_readdir_fini(&gstate, error, eofp, eof));
}


/*
 * gfs_vop_lookup: VOP_LOOKUP() entry point
 *
 * For use directly in vnode ops table.  Given a GFS directory, calls
 * gfs_dir_lookup() as necessary.
 */
/* ARGSUSED */
int
gfs_vop_lookup(vnode_t *dvp, char *nm, vnode_t **vpp, pathname_t *pnp,
    int flags, vnode_t *rdir, cred_t *cr, caller_context_t *ct,
    int *direntflags, pathname_t *realpnp)
{
	return (gfs_dir_lookup(dvp, nm, vpp, cr, flags, direntflags, realpnp));
}

/*
 * gfs_vop_readdir: VOP_READDIR() entry point
 *
 * For use directly in vnode ops table.  Given a GFS directory, calls
 * gfs_dir_readdir() as necessary.
 */
/* ARGSUSED */
int
gfs_vop_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		int *ncookies;
		u_long **a_cookies;
	} */ *ap;
{
	vnode_t *vp = ap->a_vp;
	uio_t *uiop = ap->a_uio;
	cred_t *cr = ap->a_cred;
	int *eofp = ap->a_eofflag;
	int ncookies = 0;
	u_long *cookies = NULL;
	int error;

	if (ap->a_ncookies) {
		/*
		 * Minimum entry size is dirent size and 1 byte for a file name.
		 */
		ncookies = uiop->uio_resid / (sizeof(struct dirent) - sizeof(((struct dirent *)NULL)->d_name) + 1);
		cookies = malloc(ncookies * sizeof(u_long), M_TEMP, M_WAITOK);
		*ap->a_cookies = cookies;
		*ap->a_ncookies = ncookies;
	}

	error = gfs_dir_readdir(vp, uiop, eofp, &ncookies, &cookies, NULL,
	    cr, 0);

	if (error == 0) {
		/* Subtract unused cookies */
		if (ap->a_ncookies)
			*ap->a_ncookies -= ncookies;
	} else if (ap->a_ncookies) {
		free(*ap->a_cookies, M_TEMP);
		*ap->a_cookies = NULL;
		*ap->a_ncookies = 0;
	}

	return (error);
}


#ifdef sun
/*
 * gfs_vop_map: VOP_MAP() entry point
 *
 * Convenient routine for handling pseudo-files that wish to allow mmap() calls.
 * This function only works for readonly files, and uses the read function for
 * the vnode to fill in the data.  The mapped data is immediately faulted in and
 * filled with the necessary data during this call; there are no getpage() or
 * putpage() routines.
 */
/* ARGSUSED */
int
gfs_vop_map(vnode_t *vp, offset_t off, struct as *as, caddr_t *addrp,
    size_t len, uchar_t prot, uchar_t maxprot, uint_t flags, cred_t *cred,
    caller_context_t *ct)
{
	int rv;
	ssize_t resid = len;

	/*
	 * Check for bad parameters
	 */
#ifdef _ILP32
	if (len > MAXOFF_T)
		return (ENOMEM);
#endif
	if (vp->v_flag & VNOMAP)
		return (ENOTSUP);
	if (off > MAXOFF_T)
		return (EFBIG);
	if ((long)off < 0 || (long)(off + len) < 0)
		return (EINVAL);
	if (vp->v_type != VREG)
		return (ENODEV);
	if ((prot & (PROT_EXEC | PROT_WRITE)) != 0)
		return (EACCES);

	/*
	 * Find appropriate address if needed, otherwise clear address range.
	 */
	as_rangelock(as);
	rv = choose_addr(as, addrp, len, off, ADDR_VACALIGN, flags);
	if (rv != 0) {
		as_rangeunlock(as);
		return (rv);
	}

	/*
	 * Create mapping
	 */
	rv = as_map(as, *addrp, len, segvn_create, zfod_argsp);
	as_rangeunlock(as);
	if (rv != 0)
		return (rv);

	/*
	 * Fill with data from read()
	 */
	rv = vn_rdwr(UIO_READ, vp, *addrp, len, off, UIO_USERSPACE,
	    0, (rlim64_t)0, cred, &resid);

	if (rv == 0 && resid != 0)
		rv = ENXIO;

	if (rv != 0) {
		as_rangelock(as);
		(void) as_unmap(as, *addrp, len);
		as_rangeunlock(as);
	}

	return (rv);
}
#endif	/* sun */

/*
 * gfs_vop_inactive: VOP_INACTIVE() entry point
 *
 * Given a vnode that is a GFS file or directory, call gfs_file_inactive() or
 * gfs_dir_inactive() as necessary, and kmem_free()s associated private data.
 */
/* ARGSUSED */
int
gfs_vop_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct thread *a_td;
	} */ *ap;
{
	vnode_t *vp = ap->a_vp;
	gfs_file_t *fp = vp->v_data;

	if (fp->gfs_type == GFS_DIR)
		gfs_dir_inactive(vp);
	else
		gfs_file_inactive(vp);

	VI_LOCK(vp);
	vp->v_data = NULL;
	VI_UNLOCK(vp);
	kmem_free(fp, fp->gfs_size);

	return (0);
}
