/*-
 * Copyright (c) 1999, 2000 Robert N. M. Watson
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
 *	$FreeBSD$
 */
/*
 * TrustedBSD Project - extended attribute support for UFS-like file systems
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/lock.h>

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/inode.h>

#define MIN(a,b) (((a)<(b))?(a):(b))

static MALLOC_DEFINE(M_UFS_EXTATTR, "ufs_extattr", "ufs extended attribute");

static int	ufs_extattr_credcheck(struct ufs_extattr_list_entry *uele,
    u_int32_t fowner, struct ucred *cred, struct proc *p, int access);
static int	ufs_extattr_enable(struct ufsmount *ump, const char *attrname,
    struct vnode *backing_vnode, struct proc *p);
static int	ufs_extattr_disable(struct ufsmount *ump, const char *attrname,
    struct proc *p);
static int	ufs_extattr_get(struct vnode *vp, const char *name,
    struct uio *uio, struct ucred *cred, struct proc *p);
static int	ufs_extattr_set(struct vnode *vp, const char *name,
    struct uio *uio, struct ucred *cred, struct proc *p);
static int	ufs_extattr_rm(struct vnode *vp, const char *name,
    struct ucred *cred, struct proc *p);

/*
 * Per-FS attribute lock protecting attribute operations
 * XXX Right now there is a lot of lock contention due to having a single
 * lock per-FS; really, this should be far more fine-grained.
 */
static void
ufs_extattr_uepm_lock(struct ufsmount *ump, struct proc *p)
{

	/* ideally, LK_CANRECURSE would not be used, here */
	lockmgr(&ump->um_extattr.uepm_lock, LK_EXCLUSIVE | LK_RETRY |
	    LK_CANRECURSE, 0, p);
}

static void
ufs_extattr_uepm_unlock(struct ufsmount *ump, struct proc *p)
{

	lockmgr(&ump->um_extattr.uepm_lock, LK_RELEASE, 0, p);
}

/*
 * Locate an attribute given a name and mountpoint.
 * Must be holding uepm lock for the mount point.
 */
static struct ufs_extattr_list_entry *
ufs_extattr_find_attr(struct ufsmount *ump, const char *attrname)
{
        struct ufs_extattr_list_entry   *search_attribute;

        for (search_attribute = ump->um_extattr.uepm_list.lh_first;
	    search_attribute;
	    search_attribute = search_attribute->uele_entries.le_next) {
                if (!(strncmp(attrname, search_attribute->uele_attrname,
                    UFS_EXTATTR_MAXEXTATTRNAME))) {
                        return (search_attribute);
                }
        }

        return (0);
}

/*
 * Initialize per-FS structures supporting extended attributes.  Do not
 * start extended attributes yet.
 */
void
ufs_extattr_uepm_init(struct ufs_extattr_per_mount *uepm)
{

	uepm->uepm_flags = 0;

	LIST_INIT(&uepm->uepm_list);
	/* XXX is PVFS right, here? */
	lockinit(&uepm->uepm_lock, PVFS, "extattr", 0, 0);
	uepm->uepm_flags |= UFS_EXTATTR_UEPM_INITIALIZED;
}

/*
 * Start extended attribute support on an FS
 */
int
ufs_extattr_start(struct mount *mp, struct proc *p)
{
	struct ufsmount	*ump;
	int	error = 0;

	ump = VFSTOUFS(mp);

	ufs_extattr_uepm_lock(ump, p);

	if (!(ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_INITIALIZED)) {
		error = EOPNOTSUPP;
		goto unlock;
	}
	if (ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_STARTED) {
		error = EBUSY;
		goto unlock;
	}

	ump->um_extattr.uepm_flags |= UFS_EXTATTR_UEPM_STARTED;

	crhold(p->p_ucred);
	ump->um_extattr.uepm_ucred = p->p_ucred;

unlock:
	ufs_extattr_uepm_unlock(ump, p);

	return (error);
}

/*
 * Stop extended attribute support on an FS
 */
int
ufs_extattr_stop(struct mount *mp, struct proc *p)
{
	struct ufs_extattr_list_entry	*uele;
	struct ufsmount	*ump = VFSTOUFS(mp);
	int	error = 0;

	ufs_extattr_uepm_lock(ump, p);

	if (!(ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_STARTED)) {
		error = EOPNOTSUPP;
		goto unlock;
	}

        while (ump->um_extattr.uepm_list.lh_first != NULL) {
                uele = ump->um_extattr.uepm_list.lh_first;
		ufs_extattr_disable(ump, uele->uele_attrname, p);
        }

	ump->um_extattr.uepm_flags &= ~UFS_EXTATTR_UEPM_STARTED;

	crfree(ump->um_extattr.uepm_ucred);
	ump->um_extattr.uepm_ucred = NULL;

unlock:
	ufs_extattr_uepm_unlock(ump, p);

	return (error);
}

/*
 * Enable a named attribute on the specified file system; provide a
 * backing vnode to hold the attribute data.
 */
static int
ufs_extattr_enable(struct ufsmount *ump, const char *attrname,
    struct vnode *backing_vnode, struct proc *p)
{
	struct ufs_extattr_list_entry	*attribute;
	struct iovec	aiov;
	struct uio	auio;
	int	error = 0;

	if (backing_vnode->v_type != VREG)
		return (EINVAL);

	MALLOC(attribute, struct ufs_extattr_list_entry *,
	    sizeof(struct ufs_extattr_list_entry), M_UFS_EXTATTR, M_WAITOK);
	if (attribute == NULL)
		return (ENOMEM);

	if (!(ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_STARTED)) {
		error = EOPNOTSUPP;
		goto free_exit;
	}

	if (ufs_extattr_find_attr(ump, attrname)) {
		error = EOPNOTSUPP;
		goto free_exit;
	}

	strncpy(attribute->uele_attrname, attrname, UFS_EXTATTR_MAXEXTATTRNAME);
	bzero(&attribute->uele_fileheader,
	    sizeof(struct ufs_extattr_fileheader));
	
	attribute->uele_backing_vnode = backing_vnode;

	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	aiov.iov_base = (caddr_t) &attribute->uele_fileheader;
	aiov.iov_len = sizeof(struct ufs_extattr_fileheader);
	auio.uio_resid = sizeof(struct ufs_extattr_fileheader);
	auio.uio_offset = (off_t) 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_procp = (struct proc *) p;

	VOP_LEASE(backing_vnode, p, p->p_cred->pc_ucred, LEASE_WRITE);
	vn_lock(backing_vnode, LK_SHARED | LK_NOPAUSE | LK_RETRY, p);
	error = VOP_READ(backing_vnode, &auio, 0, ump->um_extattr.uepm_ucred);
	VOP_UNLOCK(backing_vnode, 0, p);

	if (error) {
		goto free_exit;
	}

	if (auio.uio_resid != 0) {
		printf("ufs_extattr_enable: malformed attribute header\n");
		error = EINVAL;
		goto free_exit;
	}

	if (attribute->uele_fileheader.uef_magic != UFS_EXTATTR_MAGIC) {
		printf("ufs_extattr_enable: invalid attribute header magic\n");
		error = EINVAL;
		goto free_exit;
	}

	if (attribute->uele_fileheader.uef_version != UFS_EXTATTR_VERSION) {
		printf("ufs_extattr_enable: incorrect attribute header "
		    "version\n");
		error = EINVAL;
		goto free_exit;
	}

	backing_vnode->v_flag |= VSYSTEM;
	LIST_INSERT_HEAD(&ump->um_extattr.uepm_list, attribute, uele_entries);

	return (0);

free_exit:
	FREE(attribute, M_UFS_EXTATTR);
	return (error);
}

/*
 * Disable extended attribute support on an FS
 */
static int
ufs_extattr_disable(struct ufsmount *ump, const char *attrname, struct proc *p)
{
	struct ufs_extattr_list_entry	*uele;
	int	error = 0;

	uele = ufs_extattr_find_attr(ump, attrname);
	if (!uele)
		return (ENOENT);

	LIST_REMOVE(uele, uele_entries);

	uele->uele_backing_vnode->v_flag &= ~VSYSTEM;
        error = vn_close(uele->uele_backing_vnode, FREAD|FWRITE, p->p_ucred, p);

	FREE(uele, M_UFS_EXTATTR);

	return (error);
}

/*
 * VFS call to manage extended attributes in UFS
 * attrname, arg are userspace pointers from the syscall
 */
int
ufs_extattrctl(struct mount *mp, int cmd, const char *attrname,
	       caddr_t arg, struct proc *p)
{
	struct nameidata	nd;
	struct ufsmount	*ump = VFSTOUFS(mp);
	struct vnode	*vp;
	char	local_attrname[UFS_EXTATTR_MAXEXTATTRNAME]; /* inc null */
	char	*filename;
	int	error, len, flags;

	if ((error = suser_xxx(p->p_cred->pc_ucred, p, 0)))
		return (error);

	switch(cmd) {
	case UFS_EXTATTR_CMD_START:
		error = ufs_extattr_start(mp, p);

		return (error);
		
	case UFS_EXTATTR_CMD_STOP:
		return (ufs_extattr_stop(mp, p));

	case UFS_EXTATTR_CMD_ENABLE:
		error = copyinstr(attrname, local_attrname,
		    UFS_EXTATTR_MAXEXTATTRNAME, &len);
		if (error)
			return (error);

		filename = (char *) arg;
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, filename, p);
		flags = FREAD | FWRITE;
		error = vn_open(&nd, &flags, 0);
		if (error)
			return (error);

		vp = nd.ni_vp;
		VOP_UNLOCK(vp, 0, p);
		
		ufs_extattr_uepm_lock(ump, p);
		error = ufs_extattr_enable(ump, local_attrname, vp, p);
		ufs_extattr_uepm_unlock(ump, p);

		return (error);

	case UFS_EXTATTR_CMD_DISABLE:
		error = copyinstr(attrname, local_attrname,
		    UFS_EXTATTR_MAXEXTATTRNAME, &len);

		ufs_extattr_uepm_lock(ump, p);
		error = ufs_extattr_disable(ump, local_attrname, p);
		ufs_extattr_uepm_unlock(ump, p);

		return (error);

	default:
		return (EINVAL);
	}
}

/*
 * Credential check based on process requesting service, and per-attribute
 * permissions.
 */
static int
ufs_extattr_credcheck(struct ufs_extattr_list_entry *uele, u_int32_t fowner,
		      struct ucred *cred, struct proc *p, int access)
{
	u_int	uef_perm;

	switch(access) {
	case IREAD:
		uef_perm = uele->uele_fileheader.uef_read_perm;
		break;
	case IWRITE:
		uef_perm = uele->uele_fileheader.uef_write_perm;
		break;
	default:
		return (EACCES);
	}

	/* Kernel sponsoring request does so without passing a cred */
	if (!cred)
		return (0);

	/* XXX there might eventually be a capability check here */

	/* If it's set to root-only, check for suser(p) */
	if (uef_perm == UFS_EXTATTR_PERM_ROOT && !suser(p))
		return (0);

	/* Allow the owner if appropriate */
	if (uef_perm == UFS_EXTATTR_PERM_OWNER && cred->cr_uid == fowner)
		return (0);

	/* Allow anyone if appropriate */
	if (uef_perm == UFS_EXTATTR_PERM_ANYONE)
		return (0);

	return (EACCES);
}

/*
 * Vnode operating to retrieve a named extended attribute
 */
int
ufs_vop_getextattr(struct vop_getextattr_args *ap)
/*
vop_getextattr {
        IN struct vnode *a_vp;
        IN const char *a_name;
        INOUT struct uio *a_uio;
        IN struct ucred *a_cred;
        IN struct proc *a_p;
};
*/
{
	struct mount	*mp = ap->a_vp->v_mount;
	struct ufsmount	*ump = VFSTOUFS(mp);
	int	error;

	ufs_extattr_uepm_lock(ump, ap->a_p);

	error = ufs_extattr_get(ap->a_vp, ap->a_name, ap->a_uio, ap->a_cred,
	    ap->a_p);

	ufs_extattr_uepm_unlock(ump, ap->a_p);

	return (error);
}

/*
 * Real work associated with retrieving a named attribute--assumes that
 * the attribute lock has already been grabbed.
 */
static int
ufs_extattr_get(struct vnode *vp, const char *name, struct uio *uio,
    struct ucred *cred, struct proc *p)
{
	struct ufs_extattr_list_entry	*attribute;
	struct ufs_extattr_header	ueh;
	struct iovec	local_aiov;
	struct uio	local_aio;
	struct mount	*mp = vp->v_mount;
	struct ufsmount	*ump = VFSTOUFS(mp);
	struct inode	*ip = VTOI(vp);
	off_t	base_offset;
	size_t	size, old_size;
	int	error = 0;

	if (!(ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_STARTED))
		return (EOPNOTSUPP);

	attribute = ufs_extattr_find_attr(ump, name);
	if (!attribute)
		return (ENOENT);

	if ((error = ufs_extattr_credcheck(attribute, ip->i_uid, cred, p,
	    IREAD)))
		return (error);

	/*
	 * Allow only offsets of zero to encourage the read/replace
	 * extended attribute semantic.  Otherwise we can't guarantee
	 * atomicity, as we don't provide locks for extended
	 * attributes.
	 */
	if (uio->uio_offset != 0)
		return (ENXIO);

	/*
	 * Find base offset of header in file based on file header size, and
	 * data header size + maximum data size, indexed by inode number
	 */
	base_offset = sizeof(struct ufs_extattr_fileheader) +
	    ip->i_number * (sizeof(struct ufs_extattr_header) +
	    attribute->uele_fileheader.uef_size);

	/*
	 * Read in the data header to see if the data is defined, and if so
	 * how much.
	 */
	bzero(&ueh, sizeof(struct ufs_extattr_header));
	local_aiov.iov_base = (caddr_t) &ueh;
	local_aiov.iov_len = sizeof(struct ufs_extattr_header);
	local_aio.uio_iov = &local_aiov;
	local_aio.uio_iovcnt = 1;
	local_aio.uio_rw = UIO_READ;
	local_aio.uio_segflg = UIO_SYSSPACE;
	local_aio.uio_procp = p;
	local_aio.uio_offset = base_offset;
	local_aio.uio_resid = sizeof(struct ufs_extattr_header);
	
	VOP_LEASE(attribute->uele_backing_vnode, p, cred, LEASE_READ);
	vn_lock(attribute->uele_backing_vnode, LK_SHARED | LK_NOPAUSE |
	    LK_RETRY, p);

	error = VOP_READ(attribute->uele_backing_vnode, &local_aio, 0,
	    ump->um_extattr.uepm_ucred);
	if (error)
		goto vopunlock_exit;

	/* defined? */
	if ((ueh.ueh_flags & UFS_EXTATTR_ATTR_FLAG_INUSE) == 0) {
		error = ENOENT;
		goto vopunlock_exit;
	}

	/* valid for the current inode generation? */
	if (ueh.ueh_i_gen != ip->i_gen) {
		/*
		 * The inode itself has a different generation number
		 * than the attribute data.  For now, the best solution
		 * is to coerce this to undefined, and let it get cleaned
		 * up by the next write or extattrctl clean.
		 */
		printf("ufs_extattr: inode number inconsistency (%d, %d)\n",
		    ueh.ueh_i_gen, ip->i_gen);
		error = ENOENT;
		goto vopunlock_exit;
	}

	/* local size consistency check */
	if (ueh.ueh_len > attribute->uele_fileheader.uef_size) {
		error = ENXIO;
		goto vopunlock_exit;
	}

	if (ueh.ueh_len < uio->uio_offset) {
		error = 0;
		goto vopunlock_exit;
	}

	/* allow for offset into the attr data */
	uio->uio_offset = base_offset + sizeof(struct ufs_extattr_header);

	/*
	 * Figure out maximum to transfer -- use buffer size and local data
	 * limit.
	 */
	size = MIN(uio->uio_resid, ueh.ueh_len);
	old_size = uio->uio_resid;
	uio->uio_resid = size;

	error = VOP_READ(attribute->uele_backing_vnode, uio, 0,
	    ump->um_extattr.uepm_ucred);
	if (error) {
		uio->uio_offset = 0;
		goto vopunlock_exit;
	}

	uio->uio_offset = 0;
	uio->uio_resid = old_size - (size - uio->uio_resid);

vopunlock_exit:
	VOP_UNLOCK(attribute->uele_backing_vnode, 0, p);

	return (error);
}

/*
 * Vnode operation to set a named attribute
 */
int
ufs_vop_setextattr(struct vop_setextattr_args *ap)
/*
vop_setextattr {
        IN struct vnode *a_vp;
        IN const char *a_name;
        INOUT struct uio *a_uio;
        IN struct ucred *a_cred;
        IN struct proc *a_p;
};
*/
{
	struct mount	*mp = ap->a_vp->v_mount;
	struct ufsmount	*ump = VFSTOUFS(mp); 

	int	error;

	ufs_extattr_uepm_lock(ump, ap->a_p);

	if (ap->a_uio)
		error = ufs_extattr_set(ap->a_vp, ap->a_name, ap->a_uio,
		    ap->a_cred, ap->a_p);
	else
		error = ufs_extattr_rm(ap->a_vp, ap->a_name, ap->a_cred,
		    ap->a_p);

	ufs_extattr_uepm_unlock(ump, ap->a_p);

	return (error);
}

/*
 * Real work associated with setting a vnode's extended attributes;
 * assumes that the attribute lock has already been grabbed.
 */
static int
ufs_extattr_set(struct vnode *vp, const char *name, struct uio *uio,
    struct ucred *cred, struct proc *p)
{
	struct ufs_extattr_list_entry	*attribute;
	struct ufs_extattr_header	ueh;
	struct iovec	local_aiov;
	struct uio	local_aio;
	struct mount	*mp = vp->v_mount;
	struct ufsmount	*ump = VFSTOUFS(mp);
	struct inode	*ip = VTOI(vp);
	off_t	base_offset;

	int	error = 0;

	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return (EROFS);

	if (!(ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_STARTED))
		return (EOPNOTSUPP);

	attribute = ufs_extattr_find_attr(ump, name);
	if (!attribute)
		return (ENOENT);

	if ((error = ufs_extattr_credcheck(attribute, ip->i_uid, cred,
	    p, IWRITE)))
		return (error);

	/*
	 * Early rejection of invalid offsets/lengths	
	 * Reject: any offset but 0 (replace)
	 *         Any size greater than attribute size limit
 	 */
	if (uio->uio_offset != 0 ||
	    uio->uio_resid > attribute->uele_fileheader.uef_size)
		return (ENXIO);

	/*
	 * Find base offset of header in file based on file header size, and
	 * data header size + maximum data size, indexed by inode number
	 */
	base_offset = sizeof(struct ufs_extattr_fileheader) +
	    ip->i_number * (sizeof(struct ufs_extattr_header) +
	    attribute->uele_fileheader.uef_size);

	/*
	 * Write out a data header for the data
	 */
	ueh.ueh_len = uio->uio_resid;
	ueh.ueh_flags = UFS_EXTATTR_ATTR_FLAG_INUSE;
	ueh.ueh_i_gen = ip->i_gen;
	local_aiov.iov_base = (caddr_t) &ueh;
	local_aiov.iov_len = sizeof(struct ufs_extattr_header);
	local_aio.uio_iov = &local_aiov;
	local_aio.uio_iovcnt = 1;
	local_aio.uio_rw = UIO_WRITE;
	local_aio.uio_segflg = UIO_SYSSPACE;
	local_aio.uio_procp = p;
	local_aio.uio_offset = base_offset;
	local_aio.uio_resid = sizeof(struct ufs_extattr_header);

	/*
	 * Acquire locks
	 */
	VOP_LEASE(attribute->uele_backing_vnode, p, cred, LEASE_WRITE);

	/*
	 * Don't need to get a lock on the backing file if the setattr is
	 * being applied to the backing file, as the lock is already held
	 */
	if (attribute->uele_backing_vnode != vp)
		vn_lock(attribute->uele_backing_vnode, 
		    LK_EXCLUSIVE | LK_NOPAUSE | LK_RETRY, p);

	error = VOP_WRITE(attribute->uele_backing_vnode, &local_aio, 0,
	    ump->um_extattr.uepm_ucred);
	if (error)
		goto vopunlock_exit;

	if (local_aio.uio_resid != 0) {
		error = ENXIO;
		goto vopunlock_exit;
	}

	/*
	 * Write out user data
	 */
	uio->uio_offset = base_offset + sizeof(struct ufs_extattr_header);
	
	error = VOP_WRITE(attribute->uele_backing_vnode, uio, IO_SYNC,
	    ump->um_extattr.uepm_ucred);

vopunlock_exit:
	uio->uio_offset = 0;

	if (attribute->uele_backing_vnode != vp)
		VOP_UNLOCK(attribute->uele_backing_vnode, 0, p);

	return (error);
}

/*
 * Real work associated with removing an extended attribute from a vnode.
 * Assumes the attribute lock has already been grabbed.
 */
static int
ufs_extattr_rm(struct vnode *vp, const char *name, struct ucred *cred,
    struct proc *p)
{
	struct ufs_extattr_list_entry	*attribute;
	struct ufs_extattr_header	ueh;
	struct iovec	local_aiov;
	struct uio	local_aio;
	struct mount	*mp = vp->v_mount;
	struct ufsmount	*ump = VFSTOUFS(mp);
	struct inode	*ip = VTOI(vp);
	off_t	base_offset;
	int	error = 0;

	if (vp->v_mount->mnt_flag & MNT_RDONLY)  
		return (EROFS);

	if (!(ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_STARTED))
		return (EOPNOTSUPP);

	attribute = ufs_extattr_find_attr(ump, name);
	if (!attribute)
		return (ENOENT);

	if ((error = ufs_extattr_credcheck(attribute, ip->i_uid, cred, p,
	    IWRITE)))
		return (error);

	/*
	 * Find base offset of header in file based on file header size, and
	 * data header size + maximum data size, indexed by inode number
	 */
	base_offset = sizeof(struct ufs_extattr_fileheader) +
	    ip->i_number * (sizeof(struct ufs_extattr_header) +
	    attribute->uele_fileheader.uef_size);

	/*
	 * Read in the data header to see if the data is defined
	 */
	bzero(&ueh, sizeof(struct ufs_extattr_header));

	local_aiov.iov_base = (caddr_t) &ueh;
	local_aiov.iov_len = sizeof(struct ufs_extattr_header);
	local_aio.uio_iov = &local_aiov;
	local_aio.uio_iovcnt = 1;
	local_aio.uio_rw = UIO_READ;
	local_aio.uio_segflg = UIO_SYSSPACE;
	local_aio.uio_procp = p;
	local_aio.uio_offset = base_offset;
	local_aio.uio_resid = sizeof(struct ufs_extattr_header);

	VOP_LEASE(attribute->uele_backing_vnode, p, cred, LEASE_WRITE);

	/*
	 * Don't need to get the lock on the backing vnode if the vnode we're
	 * modifying is it, as we already hold the lock.
	 */
	if (attribute->uele_backing_vnode != vp)
		vn_lock(attribute->uele_backing_vnode,
		    LK_EXCLUSIVE | LK_NOPAUSE | LK_RETRY, p);

	error = VOP_READ(attribute->uele_backing_vnode, &local_aio, 0,
	    ump->um_extattr.uepm_ucred);
	if (error)
		goto vopunlock_exit;

	/* defined? */
	if ((ueh.ueh_flags & UFS_EXTATTR_ATTR_FLAG_INUSE) == 0) {
		error = ENOENT;
		goto vopunlock_exit;
	}

	/* flag it as not in use */
	ueh.ueh_flags = 0;

	error = VOP_WRITE(attribute->uele_backing_vnode, &local_aio, 0,
	    ump->um_extattr.uepm_ucred);
	if (error)
		goto vopunlock_exit;

	if (local_aio.uio_resid != 0)
		error = ENXIO;

vopunlock_exit:
	VOP_UNLOCK(attribute->uele_backing_vnode, 0, p);

	return (error);
}

/*
 * Called by UFS when an inode is no longer active and should have its
 * attributes stripped.
 */
void
ufs_extattr_vnode_inactive(struct vnode *vp, struct proc *p)
{
	struct ufs_extattr_list_entry	*uele;
	struct mount	*mp = vp->v_mount;
	struct ufsmount	*ump = VFSTOUFS(mp);

	ufs_extattr_uepm_lock(ump, p);
        
	if (!(ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_STARTED)) {
		ufs_extattr_uepm_unlock(ump, p);
		return;
	}

	for (uele = ump->um_extattr.uepm_list.lh_first; uele != NULL;
	    uele = uele->uele_entries.le_next)
		ufs_extattr_rm(vp, uele->uele_attrname, 0, p);

	ufs_extattr_uepm_unlock(ump, p);
}
