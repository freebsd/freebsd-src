/*-
 * Copyright (c) 2007 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/priv.h>
#include <sys/vnode.h>
#include <sys/mntent.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/jail.h>
#include <sys/policy.h>
#include <sys/zfs_vfsops.h>

int
secpolicy_nfs(struct ucred *cred)
{

	return (priv_check_cred(cred, PRIV_NFS_DAEMON, 0));
}

int
secpolicy_zfs(struct ucred *cred)
{

	return (priv_check_cred(cred, PRIV_VFS_MOUNT, 0));
}

int
secpolicy_sys_config(struct ucred *cred, int checkonly __unused)
{

	return (priv_check_cred(cred, PRIV_ZFS_POOL_CONFIG, 0));
}

int
secpolicy_zinject(struct ucred *cred)
{

	return (priv_check_cred(cred, PRIV_ZFS_INJECT, 0));
}

int
secpolicy_fs_unmount(struct ucred *cred, struct mount *vfsp __unused)
{

	return (priv_check_cred(cred, PRIV_VFS_UNMOUNT, 0));
}

int
secpolicy_fs_owner(struct mount *mp, struct ucred *cred)
{

	if (zfs_super_owner) {
		if (cred->cr_uid == mp->mnt_cred->cr_uid &&
		    cred->cr_prison == mp->mnt_cred->cr_prison) {
			return (0);
		}
	}
	return (EPERM);
}

/*
 * This check is done in kern_link(), so we could just return 0 here.
 */
extern int hardlink_check_uid;
int
secpolicy_basic_link(struct vnode *vp, struct ucred *cred)
{

	if (!hardlink_check_uid)
		return (0);
	if (secpolicy_fs_owner(vp->v_mount, cred) == 0)
		return (0);
	return (priv_check_cred(cred, PRIV_VFS_LINK, 0));
}

int
secpolicy_vnode_stky_modify(struct ucred *cred)
{

	return (EPERM);
}

int
secpolicy_vnode_remove(struct vnode *vp, struct ucred *cred)
{

	if (secpolicy_fs_owner(vp->v_mount, cred) == 0)
		return (0);
	return (priv_check_cred(cred, PRIV_VFS_ADMIN, 0));
}

int
secpolicy_vnode_access(struct ucred *cred, struct vnode *vp, uint64_t owner,
    accmode_t accmode)
{

	if (secpolicy_fs_owner(vp->v_mount, cred) == 0)
		return (0);

	if ((accmode & VREAD) && priv_check_cred(cred, PRIV_VFS_READ, 0) != 0)
		return (EACCES);
	if ((accmode & VWRITE) &&
	    priv_check_cred(cred, PRIV_VFS_WRITE, 0) != 0) {
		return (EACCES);
	}
	if (accmode & VEXEC) {
		if (vp->v_type == VDIR) {
			if (priv_check_cred(cred, PRIV_VFS_LOOKUP, 0) != 0) {
				return (EACCES);
			}
		} else {
			if (priv_check_cred(cred, PRIV_VFS_EXEC, 0) != 0) {
				return (EACCES);
			}
		}
	}
	return (0);
}

int
secpolicy_vnode_setdac(struct vnode *vp, struct ucred *cred, uid_t owner)
{

	if (owner == cred->cr_uid)
		return (0);
	if (secpolicy_fs_owner(vp->v_mount, cred) == 0)
		return (0);
	return (priv_check_cred(cred, PRIV_VFS_ADMIN, 0));
}

int
secpolicy_vnode_setattr(struct ucred *cred, struct vnode *vp, struct vattr *vap,
    const struct vattr *ovap, int flags,
    int unlocked_access(void *, int, struct ucred *), void *node)
{
	int mask = vap->va_mask;
	int error;

	if (mask & AT_SIZE) {
		if (vp->v_type == VDIR)
			return (EISDIR);
		error = unlocked_access(node, VWRITE, cred);
		if (error)
			return (error);
	}
	if (mask & AT_MODE) {
		/*
		 * If not the owner of the file then check privilege
		 * for two things: the privilege to set the mode at all
		 * and, if we're setting setuid, we also need permissions
		 * to add the set-uid bit, if we're not the owner.
		 * In the specific case of creating a set-uid root
		 * file, we need even more permissions.
		 */
		error = secpolicy_vnode_setdac(vp, cred, ovap->va_uid);
		if (error)
			return (error);
		error = secpolicy_setid_setsticky_clear(vp, vap, ovap, cred);
		if (error)
			return (error);
	} else {
		vap->va_mode = ovap->va_mode;
	}
	if (mask & (AT_UID | AT_GID)) {
		error = secpolicy_vnode_setdac(vp, cred, ovap->va_uid);
		if (error)
			return (error);

		/*
		 * To change the owner of a file, or change the group of a file to a
		 * group of which we are not a member, the caller must have
		 * privilege.
		 */
		if (((mask & AT_UID) && vap->va_uid != ovap->va_uid) ||
		    ((mask & AT_GID) && vap->va_gid != ovap->va_gid &&
		     !groupmember(vap->va_gid, cred))) {
			if (secpolicy_fs_owner(vp->v_mount, cred) != 0) {
				error = priv_check_cred(cred, PRIV_VFS_CHOWN, 0);
				if (error)
					return (error);
			}
		}

		if (((mask & AT_UID) && vap->va_uid != ovap->va_uid) ||
		    ((mask & AT_GID) && vap->va_gid != ovap->va_gid)) {
			secpolicy_setid_clear(vap, vp, cred);
		}
	}
	if (mask & (AT_ATIME | AT_MTIME)) {
		/*
		 * From utimes(2):
		 * If times is NULL, ... The caller must be the owner of
		 * the file, have permission to write the file, or be the
		 * super-user.
		 * If times is non-NULL, ... The caller must be the owner of
		 * the file or be the super-user.
		 */
		error = secpolicy_vnode_setdac(vp, cred, ovap->va_uid);
		if (error && (vap->va_vaflags & VA_UTIMES_NULL))
			error = unlocked_access(node, VWRITE, cred);
		if (error)
			return (error);
	}
	return (0);
}

int
secpolicy_vnode_create_gid(struct ucred *cred)
{

	return (EPERM);
}

int
secpolicy_vnode_setids_setgids(struct vnode *vp, struct ucred *cred, gid_t gid)
{

	if (groupmember(gid, cred))
		return (0);
	if (secpolicy_fs_owner(vp->v_mount, cred) == 0)
		return (0);
	return (priv_check_cred(cred, PRIV_VFS_SETGID, 0));
}

int
secpolicy_vnode_setid_retain(struct vnode *vp, struct ucred *cred,
    boolean_t issuidroot __unused)
{

	if (secpolicy_fs_owner(vp->v_mount, cred) == 0)
		return (0);
	return (priv_check_cred(cred, PRIV_VFS_RETAINSUGID, 0));
}

void
secpolicy_setid_clear(struct vattr *vap, struct vnode *vp, struct ucred *cred)
{

	if (secpolicy_fs_owner(vp->v_mount, cred) == 0)
		return;

	if ((vap->va_mode & (S_ISUID | S_ISGID)) != 0) {
		if (priv_check_cred(cred, PRIV_VFS_RETAINSUGID, 0)) {
			vap->va_mask |= AT_MODE;
			vap->va_mode &= ~(S_ISUID|S_ISGID);
		}
	}
}

int
secpolicy_setid_setsticky_clear(struct vnode *vp, struct vattr *vap,
    const struct vattr *ovap, struct ucred *cred)
{
        int error;

	if (secpolicy_fs_owner(vp->v_mount, cred) == 0)
		return (0);

	/*
	 * Privileged processes may set the sticky bit on non-directories,
	 * as well as set the setgid bit on a file with a group that the process
	 * is not a member of. Both of these are allowed in jail(8).
	 */
	if (vp->v_type != VDIR && (vap->va_mode & S_ISTXT)) {
		if (priv_check_cred(cred, PRIV_VFS_STICKYFILE, 0))
			return (EFTYPE);
	}
	/*
	 * Check for privilege if attempting to set the
	 * group-id bit.
	 */
	if ((vap->va_mode & S_ISGID) != 0) {
		error = secpolicy_vnode_setids_setgids(vp, cred, ovap->va_gid);
		if (error)
			return (error);
	}
	/*
	 * Deny setting setuid if we are not the file owner.
	 */
	if ((vap->va_mode & S_ISUID) && ovap->va_uid != cred->cr_uid) {
		error = priv_check_cred(cred, PRIV_VFS_ADMIN, 0);
		if (error)
			return (error);
	}
	return (0);
}

int
secpolicy_fs_mount(cred_t *cr, vnode_t *mvp, struct mount *vfsp)
{

	return (priv_check_cred(cr, PRIV_VFS_MOUNT, 0));
}

int
secpolicy_vnode_owner(struct vnode *vp, cred_t *cred, uid_t owner)
{

	if (owner == cred->cr_uid)
		return (0);
	if (secpolicy_fs_owner(vp->v_mount, cred) == 0)
		return (0);

	/* XXX: vfs_suser()? */
	return (priv_check_cred(cred, PRIV_VFS_MOUNT_OWNER, 0));
}

int
secpolicy_vnode_chown(struct vnode *vp, cred_t *cred, boolean_t check_self)
{

	if (secpolicy_fs_owner(vp->v_mount, cred) == 0)
		return (0);
	return (priv_check_cred(cred, PRIV_VFS_CHOWN, 0));
}

void
secpolicy_fs_mount_clearopts(cred_t *cr, struct mount *vfsp)
{

	if (priv_check_cred(cr, PRIV_VFS_MOUNT_NONUSER, 0) != 0) {
		MNT_ILOCK(vfsp);
		vfsp->vfs_flag |= VFS_NOSETUID | MNT_USER;
		vfs_clearmntopt(vfsp, MNTOPT_SETUID);
		vfs_setmntopt(vfsp, MNTOPT_NOSETUID, NULL, 0);
		MNT_IUNLOCK(vfsp);
	}
}

/*
 * Check privileges for setting xvattr attributes
 */
int
secpolicy_xvattr(xvattr_t *xvap, uid_t owner, cred_t *cr, vtype_t vtype)
{

	return (priv_check_cred(cr, PRIV_VFS_SYSFLAGS, 0));
}
