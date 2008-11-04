/*-
 * Copyright (c) 1999-2003 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
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
 */

/*
 * Support for POSIX.1e access control lists: UFS-specific support functions.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ufs.h"
#include "opt_quota.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/types.h>
#include <sys/acl.h>
#include <sys/event.h>
#include <sys/extattr.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/acl.h>
#include <ufs/ufs/extattr.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ffs/fs.h>

#ifdef UFS_ACL

/*
 * Synchronize an ACL and an inode by copying over appropriate inode fields
 * to the passed ACL.  Assumes an ACL that would satisfy acl_posix1e_check(),
 * and may panic if not.
 */
void
ufs_sync_acl_from_inode(struct inode *ip, struct acl *acl)
{
	struct acl_entry	*acl_mask, *acl_group_obj;
	int	i;

	/*
	 * Update ACL_USER_OBJ, ACL_OTHER, but simply identify ACL_MASK
	 * and ACL_GROUP_OBJ for use after we know whether ACL_MASK is
	 * present.
	 */
	acl_mask = NULL;
	acl_group_obj = NULL;
	for (i = 0; i < acl->acl_cnt; i++) {
		switch (acl->acl_entry[i].ae_tag) {
		case ACL_USER_OBJ:
			acl->acl_entry[i].ae_perm = acl_posix1e_mode_to_perm(
			    ACL_USER_OBJ, ip->i_mode);
			acl->acl_entry[i].ae_id = ACL_UNDEFINED_ID;
			break;
	
		case ACL_GROUP_OBJ:
			acl_group_obj = &acl->acl_entry[i];
			acl->acl_entry[i].ae_id = ACL_UNDEFINED_ID;
			break;

		case ACL_OTHER:
			acl->acl_entry[i].ae_perm = acl_posix1e_mode_to_perm(
			    ACL_OTHER, ip->i_mode);
			acl->acl_entry[i].ae_id = ACL_UNDEFINED_ID;
			break;

		case ACL_MASK:
			acl_mask = &acl->acl_entry[i];
			acl->acl_entry[i].ae_id = ACL_UNDEFINED_ID;
			break;

		case ACL_USER:
		case ACL_GROUP:
			break;
	
		default:
			panic("ufs_sync_acl_from_inode(): bad ae_tag");
		}
	}

	if (acl_group_obj == NULL)
		panic("ufs_sync_acl_from_inode(): no ACL_GROUP_OBJ");

	if (acl_mask == NULL) {
		/*
		 * There is no ACL_MASK, so update ACL_GROUP_OBJ.
		 */
		acl_group_obj->ae_perm = acl_posix1e_mode_to_perm(
		    ACL_GROUP_OBJ, ip->i_mode);
	} else {
		/*
		 * Update the ACL_MASK entry instead of ACL_GROUP_OBJ.
		 */
		acl_mask->ae_perm = acl_posix1e_mode_to_perm(ACL_GROUP_OBJ,
		    ip->i_mode);
	}
}

/*
 * Calculate what the inode mode should look like based on an authoritative
 * ACL for the inode.  Replace only the fields in the inode that the ACL
 * can represent.
 */
void
ufs_sync_inode_from_acl(struct acl *acl, struct inode *ip)
{

	ip->i_mode &= ACL_PRESERVE_MASK;
	ip->i_mode |= acl_posix1e_acl_to_mode(acl);
	DIP_SET(ip, i_mode, ip->i_mode);
}

/*
 * Retrieve the ACL on a file.
 *
 * As part of the ACL is stored in the inode, and the rest in an EA,
 * assemble both into a final ACL product.  Right now this is not done
 * very efficiently.
 */
int
ufs_getacl(ap)
	struct vop_getacl_args /* {
		struct vnode *vp;
		struct acl_type_t type;
		struct acl *aclp;
		struct ucred *cred;
		struct thread *td;
	} */ *ap;
{
	struct inode *ip = VTOI(ap->a_vp);
	int error, len;

	/*
	 * XXX: If ufs_getacl() should work on file systems not supporting
	 * ACLs, remove this check.
	 */
	if ((ap->a_vp->v_mount->mnt_flag & MNT_ACLS) == 0)
		return (EOPNOTSUPP);

	/*
	 * Attempt to retrieve the ACL based on the ACL type.
	 */
	bzero(ap->a_aclp, sizeof(*ap->a_aclp));
	len = sizeof(*ap->a_aclp);
	switch(ap->a_type) {
	case ACL_TYPE_ACCESS:
		/*
		 * ACL_TYPE_ACCESS ACLs may or may not be stored in the
		 * EA, as they are in fact a combination of the inode
		 * ownership/permissions and the EA contents.  If the
		 * EA is present, merge the two in a temporary ACL
		 * storage, otherwise just return the inode contents.
		 */
		error = vn_extattr_get(ap->a_vp, IO_NODELOCKED,
		    POSIX1E_ACL_ACCESS_EXTATTR_NAMESPACE,
		    POSIX1E_ACL_ACCESS_EXTATTR_NAME, &len, (char *) ap->a_aclp,
		    ap->a_td);
		switch (error) {
		/* XXX: If ufs_getacl() should work on filesystems without
		 * the EA configured, add case EOPNOTSUPP here. */
		case ENOATTR:
			/*
			 * Legitimately no ACL set on object, purely
			 * emulate it through the inode.  These fields will
			 * be updated when the ACL is synchronized with
			 * the inode later.
			 */
			ap->a_aclp->acl_cnt = 3;
			ap->a_aclp->acl_entry[0].ae_tag = ACL_USER_OBJ;
			ap->a_aclp->acl_entry[0].ae_id = ACL_UNDEFINED_ID;
			ap->a_aclp->acl_entry[0].ae_perm = ACL_PERM_NONE;
			ap->a_aclp->acl_entry[1].ae_tag = ACL_GROUP_OBJ;
			ap->a_aclp->acl_entry[1].ae_id = ACL_UNDEFINED_ID;
			ap->a_aclp->acl_entry[1].ae_perm = ACL_PERM_NONE;
			ap->a_aclp->acl_entry[2].ae_tag = ACL_OTHER;
			ap->a_aclp->acl_entry[2].ae_id = ACL_UNDEFINED_ID;
			ap->a_aclp->acl_entry[2].ae_perm = ACL_PERM_NONE;
			ufs_sync_acl_from_inode(ip, ap->a_aclp);
			error = 0;
			break;

		case 0:
			if (len != sizeof(*ap->a_aclp)) {
				/*
				 * A short (or long) read, meaning that for
				 * some reason the ACL is corrupted.  Return
				 * EPERM since the object DAC protections
				 * are unsafe.
				 */
				printf("ufs_getacl(): Loaded invalid ACL ("
				    "%d bytes), inumber %d on %s\n", len,
				    ip->i_number, ip->i_fs->fs_fsmnt);
				return (EPERM);
			}
			ufs_sync_acl_from_inode(ip, ap->a_aclp);
			break;

		default:
			break;
		}
		break;

	case ACL_TYPE_DEFAULT:
		if (ap->a_vp->v_type != VDIR) {
			error = EINVAL;
			break;
		}
		error = vn_extattr_get(ap->a_vp, IO_NODELOCKED,
		    POSIX1E_ACL_DEFAULT_EXTATTR_NAMESPACE,
		    POSIX1E_ACL_DEFAULT_EXTATTR_NAME, &len,
		    (char *) ap->a_aclp, ap->a_td);
		/*
		 * Unlike ACL_TYPE_ACCESS, there is no relationship between
		 * the inode contents and the ACL, and it is therefore
		 * possible for the request for the ACL to fail since the
		 * ACL is undefined.  In this situation, return success
		 * and an empty ACL, as required by POSIX.1e.
		 */
		switch (error) {
		/* XXX: If ufs_getacl() should work on filesystems without
		 * the EA configured, add case EOPNOTSUPP here. */
		case ENOATTR:
			bzero(ap->a_aclp, sizeof(*ap->a_aclp));
			ap->a_aclp->acl_cnt = 0;
			error = 0;
			break;

		case 0:
			if (len != sizeof(*ap->a_aclp)) {
				/*
				 * A short (or long) read, meaning that for
				 * some reason the ACL is corrupted.  Return
				 * EPERM since the object default DAC
				 * protections are unsafe.
				 */
				printf("ufs_getacl(): Loaded invalid ACL ("
				    "%d bytes), inumber %d on %s\n", len,
				    ip->i_number, ip->i_fs->fs_fsmnt);
				return (EPERM);
			}
			break;

		default:
			break;
		}
		break;

	default:
		error = EINVAL;
	}

	return (error);
}

/*
 * Set the ACL on a file.
 *
 * As part of the ACL is stored in the inode, and the rest in an EA,
 * this is necessarily non-atomic, and has complex authorization.
 * As ufs_setacl() includes elements of ufs_chown() and ufs_chmod(),
 * a fair number of different access checks may be required to go ahead
 * with the operation at all.
 */
int
ufs_setacl(ap)
	struct vop_setacl_args /* {
		struct vnode *vp;
		acl_type_t type;
		struct acl *aclp;
		struct ucred *cred;
		struct proc *p;
	} */ *ap;
{
	struct inode *ip = VTOI(ap->a_vp);
	int error;

	if ((ap->a_vp->v_mount->mnt_flag & MNT_ACLS) == 0)
		return (EOPNOTSUPP);

	/*
	 * If this is a set operation rather than a delete operation,
	 * invoke VOP_ACLCHECK() on the passed ACL to determine if it is
	 * valid for the target.  This will include a check on ap->a_type.
	 */
	if (ap->a_aclp != NULL) {
		/*
		 * Set operation.
		 */
		error = VOP_ACLCHECK(ap->a_vp, ap->a_type, ap->a_aclp,
		    ap->a_cred, ap->a_td);
		if (error != 0)
			return (error);
	} else {
		/*
		 * Delete operation.
		 * POSIX.1e allows only deletion of the default ACL on a
		 * directory (ACL_TYPE_DEFAULT).
		 */
		if (ap->a_type != ACL_TYPE_DEFAULT)
			return (EINVAL);
		if (ap->a_vp->v_type != VDIR)
			return (ENOTDIR);
	}

	if (ap->a_vp->v_mount->mnt_flag & MNT_RDONLY)
		return (EROFS);

	/*
	 * Authorize the ACL operation.
	 */
	if (ip->i_flags & (IMMUTABLE | APPEND))
		return (EPERM);

	/*
	 * Must hold VADMIN (be file owner) or have appropriate privilege.
	 */
	if ((error = VOP_ACCESS(ap->a_vp, VADMIN, ap->a_cred, ap->a_td)))
		return (error);

	switch(ap->a_type) {
	case ACL_TYPE_ACCESS:
		error = vn_extattr_set(ap->a_vp, IO_NODELOCKED,
		    POSIX1E_ACL_ACCESS_EXTATTR_NAMESPACE,
		    POSIX1E_ACL_ACCESS_EXTATTR_NAME, sizeof(*ap->a_aclp),
		    (char *) ap->a_aclp, ap->a_td);
		break;

	case ACL_TYPE_DEFAULT:
		if (ap->a_aclp == NULL) {
			error = vn_extattr_rm(ap->a_vp, IO_NODELOCKED,
			    POSIX1E_ACL_DEFAULT_EXTATTR_NAMESPACE,
			    POSIX1E_ACL_DEFAULT_EXTATTR_NAME, ap->a_td);
			/*
			 * Attempting to delete a non-present default ACL
			 * will return success for portability purposes.
			 * (TRIX)
			 *
			 * XXX: Note that since we can't distinguish
			 * "that EA is not supported" from "that EA is not
			 * defined", the success case here overlaps the
			 * the ENOATTR->EOPNOTSUPP case below.
		 	 */
			if (error == ENOATTR)
				error = 0;
		} else
			error = vn_extattr_set(ap->a_vp, IO_NODELOCKED,
			    POSIX1E_ACL_DEFAULT_EXTATTR_NAMESPACE,
			    POSIX1E_ACL_DEFAULT_EXTATTR_NAME,
			    sizeof(*ap->a_aclp), (char *) ap->a_aclp, ap->a_td);
		break;

	default:
		error = EINVAL;
	}
	/*
	 * Map lack of attribute definition in UFS_EXTATTR into lack of
	 * support for ACLs on the filesystem.
	 */
	if (error == ENOATTR)
		return (EOPNOTSUPP);
	if (error != 0)
		return (error);

	if (ap->a_type == ACL_TYPE_ACCESS) {
		/*
		 * Now that the EA is successfully updated, update the
		 * inode and mark it as changed.
		 */
		ufs_sync_inode_from_acl(ap->a_aclp, ip);
		ip->i_flag |= IN_CHANGE;
	}

	VN_KNOTE_UNLOCKED(ap->a_vp, NOTE_ATTRIB);
	return (0);
}

/*
 * Check the validity of an ACL for a file.
 */
int
ufs_aclcheck(ap)
	struct vop_aclcheck_args /* {
		struct vnode *vp;
		acl_type_t type;
		struct acl *aclp;
		struct ucred *cred;
		struct thread *td;
	} */ *ap;
{

	if ((ap->a_vp->v_mount->mnt_flag & MNT_ACLS) == 0)
		return (EOPNOTSUPP);

	/*
	 * Verify we understand this type of ACL, and that it applies
	 * to this kind of object.
	 * Rely on the acl_posix1e_check() routine to verify the contents.
	 */
	switch(ap->a_type) {
	case ACL_TYPE_ACCESS:
		break;

	case ACL_TYPE_DEFAULT:
		if (ap->a_vp->v_type != VDIR)
			return (EINVAL);
		break;

	default:
		return (EINVAL);
	}
	return (acl_posix1e_check(ap->a_aclp));
}

#endif /* !UFS_ACL */
