/*-
 * Copyright (c) 1999, 2000, 2001 Robert N. M. Watson
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
 * $FreeBSD$
 */
/*
 * Developed by the TrustedBSD Project.
 * Support for POSIX.1e access control lists: UFS-specific support functions.
 */

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

#define VN_KNOTE(vp, b) \
	KNOTE(&vp->v_pollinfo.vpi_selinfo.si_note, (b))

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
			acl->acl_entry[i].ae_id = ip->i_uid;
			break;
	
		case ACL_GROUP_OBJ:
			acl_group_obj = &acl->acl_entry[i];
			acl->acl_entry[i].ae_id = ip->i_gid;
			break;

		case ACL_OTHER:
			acl->acl_entry[i].ae_perm = acl_posix1e_mode_to_perm(
			    ACL_OTHER, ip->i_mode);
			acl->acl_entry[i].ae_id = 0;
			break;

		case ACL_MASK:
			acl_mask = &acl->acl_entry[i];
			acl->acl_entry[i].ae_id = 0;
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
 * Synchronize an inode and an ACL by copying over appropriate ACL fields to
 * the passed inode.  Assumes an ACL that would satisfy acl_posix1e_check(),
 * and may panic if not.  This code will preserve existing use of the
 * sticky, setugid, and non-permission bits in the mode field.  It may
 * be that the caller wishes to have previously authorized these changes,
 * and may also want to clear the setugid bits in some situations.
 */
void
ufs_sync_inode_from_acl(struct acl *acl, struct inode *ip,
    mode_t preserve_mask)
{
	struct acl_entry	*acl_mask, *acl_user_obj, *acl_group_obj;
	struct acl_entry	*acl_other;
	mode_t	preserve_mode;
	int	i;

	/*
	 * Preserve old mode so we can restore appropriate bits of it.
	 */
	preserve_mode = (ip->i_mode & preserve_mask);

	/*
	 * Identify the ACL_MASK and all other entries appearing in the
	 * inode mode.
	 */
	acl_user_obj = NULL;
	acl_group_obj = NULL;
	acl_other = NULL;
	acl_mask = NULL;
	for (i = 0; i < acl->acl_cnt; i++) {
		switch (acl->acl_entry[i].ae_tag) {
		case ACL_USER_OBJ:
			acl_user_obj = &acl->acl_entry[i];
			ip->i_uid = acl->acl_entry[i].ae_id;
			break;

		case ACL_GROUP_OBJ:
			acl_group_obj = &acl->acl_entry[i];
			ip->i_gid = acl->acl_entry[i].ae_id;
			break;

		case ACL_OTHER:
			acl_other = &acl->acl_entry[i];
			break;

		case ACL_MASK:
			acl_mask = &acl->acl_entry[i];
			break;

		case ACL_USER:
		case ACL_GROUP:
			break;

		default:
			panic("ufs_sync_inode_from_acl(): bad ae_tag");
		}
	}

	if (acl_user_obj == NULL || acl_group_obj == NULL || acl_other == NULL)
		panic("ufs_sync_inode_from_acl(): missing ae_tags");

	if (acl_mask == NULL) {
		/*
		 * There is no ACL_MASK, so use the ACL_GROUP_OBJ entry.
		 */
		ip->i_mode &= ~ALLPERMS;
		ip->i_mode |= acl_posix1e_perms_to_mode(acl_user_obj,
		    acl_group_obj, acl_other);
	} else {
		/*
		 * Use the ACL_MASK entry.
		 */
		ip->i_mode &= ~ALLPERMS;
		ip->i_mode |= acl_posix1e_perms_to_mode(acl_user_obj,
		    acl_mask, acl_other);
	}
	ip->i_mode |= preserve_mode;
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
		struct proc *p;
	} */ *ap;
{
	struct inode *ip = VTOI(ap->a_vp);
	int error, len;


	/*
	 * Attempt to retrieve the ACL based on the ACL type.
	 */
	bzero(ap->a_aclp, sizeof(*ap->a_aclp));
	switch(ap->a_type) {
	case ACL_TYPE_ACCESS:
		/*
		 * ACL_TYPE_ACCESS ACLs may or may not be stored in the
		 * EA, as they are in fact a combination of the inode
		 * ownership/permissions and the EA contents.  If the
		 * EA is present, merge the two in a temporary ACL
		 * storage, otherwise just return the inode contents.
		 */
		len = sizeof(*ap->a_aclp);
		error = vn_extattr_get(ap->a_vp, IO_NODELOCKED,
		    POSIX1E_ACL_ACCESS_EXTATTR_NAMESPACE,
		    POSIX1E_ACL_ACCESS_EXTATTR_NAME, &len, (char *) ap->a_aclp,
		    ap->a_p);
		switch (error) {
		/* XXX: Will be ENOATTR. */
		/* XXX: If ufs_getacl() should work on file systems without
		 * the EA configured, add case EOPNOTSUPP here. */
		case ENOENT:
			/*
			 * Legitimately no ACL set on object, purely
			 * emulate it through the inode.  These fields will
			 * be updated when the ACL is synchronized with
			 * the inode later.
			 */
			ap->a_aclp->acl_cnt = 3;
			ap->a_aclp->acl_entry[0].ae_tag = ACL_USER_OBJ;
			ap->a_aclp->acl_entry[0].ae_id = 0;
			ap->a_aclp->acl_entry[0].ae_perm = 0;
			ap->a_aclp->acl_entry[1].ae_tag = ACL_GROUP_OBJ;
			ap->a_aclp->acl_entry[1].ae_id = 0;
			ap->a_aclp->acl_entry[1].ae_perm = 0;
			ap->a_aclp->acl_entry[2].ae_tag = ACL_OTHER;
			ap->a_aclp->acl_entry[2].ae_id = 0;
			ap->a_aclp->acl_entry[2].ae_perm = 0;
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
				    "%d bytes)\n", len);
				return (EPERM);
			}
			ufs_sync_acl_from_inode(ip, ap->a_aclp);
			break;

		default:
		}
		break;

	case ACL_TYPE_DEFAULT:
		if (ap->a_vp->v_type != VDIR) {
			error = EINVAL;
			break;
		}
		bzero(ap->a_aclp, sizeof(*ap->a_aclp));
		error = vn_extattr_get(ap->a_vp, IO_NODELOCKED,
		    POSIX1E_ACL_DEFAULT_EXTATTR_NAMESPACE,
		    POSIX1E_ACL_DEFAULT_EXTATTR_NAME, &len,
		    (char *) ap->a_aclp, ap->a_p);
		/*
		 * Unlike ACL_TYPE_ACCESS, there is no relationship between
		 * the inode contents and the ACL, and it is therefore
		 * possible for the request for the ACL to fail since the
		 * ACL is undefined.  In this situation, return success
		 * and an empty ACL, as required by POSIX.1e.
		 */
		switch (error) {
		/* XXX: Will be ENOATTR. */
		/* XXX: If ufs_getacl() should work on file systems without
		 * the EA configured, add case EOPNOTSUPP here. */
		case ENOENT:
			bzero(ap->a_aclp, sizeof(*ap->a_aclp));
			ap->a_aclp->acl_cnt = 0;
			error = 0;
			break;

		case 0:
			break;

		default:
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
	struct acl_entry *acl_user_obj, *acl_group_obj, *acl_other;
	mode_t old_mode, preserve_mask;
	uid_t old_uid, new_uid = 0;
	gid_t old_gid, new_gid = 0;
	int error, i;

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
		    ap->a_cred, ap->a_p);
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
	if ((error = VOP_ACCESS(ap->a_vp, VADMIN, ap->a_cred, ap->a_p)))
		return (error);

	/*
	 * ACL_TYPE_ACCESS may involve the changing of ownership, sticky
	 * bit, setugid bits on the file or directory.  As such, it requires
	 * special handling to identify these changes, and to authorize
	 * them.
	 * ACL_TYPE_DEFAULT does not require this, and ap->a_aclp should
	 * not be dereferenced without a NULL check, as it may be a delete
	 * operation.
	 */
	switch(ap->a_type) {
	case ACL_TYPE_ACCESS:
		/*
		 * Identify ACL_USER_OBJ, ACL_GROUP_OBJ, and determine if
		 * they have changed.  If so, authorize in the style of
		 * ufs_chown().  While we're at it, identify ACL_OTHER.
	 	 */
		acl_user_obj = acl_group_obj = acl_other = NULL;
		for (i = 0; i < ap->a_aclp->acl_cnt; i++)
			switch(ap->a_aclp->acl_entry[i].ae_tag) {
			case ACL_USER_OBJ:
				acl_user_obj = &ap->a_aclp->acl_entry[i];
				new_uid = acl_user_obj->ae_id;
				break;
			case ACL_GROUP_OBJ:
				acl_group_obj = &ap->a_aclp->acl_entry[i];
				new_gid = acl_group_obj->ae_id;
				break;
			case ACL_OTHER:
				acl_other = &ap->a_aclp->acl_entry[i];
				break;
			default:
			}
		old_uid = ip->i_uid;
		old_gid = ip->i_gid;

		/*
		 * Authorize changes to base object ownership in the style
		 * of ufs_chown().
		 */
		if (new_uid != old_uid && (error = suser_xxx(ap->a_cred,
		    ap->a_p, PRISON_ROOT)))
			return (error);
		if (new_gid != old_gid && !groupmember(new_gid, ap->a_cred) &&
		    (error = suser_xxx(ap->a_cred, ap->a_p, PRISON_ROOT)))
			return (error);

	case ACL_TYPE_DEFAULT:
		/*
		 * ACL_TYPE_DEFAULT can literally be written straight into
		 * the EA unhindered, as it has gone through sanity checking
		 * already.
	 	 */
		break;

	default:
		panic("ufs_setacl(): unknown acl type\n");
	}

	switch(ap->a_type) {
	case ACL_TYPE_ACCESS:
		error = vn_extattr_set(ap->a_vp, IO_NODELOCKED,
		    POSIX1E_ACL_ACCESS_EXTATTR_NAMESPACE,
		    POSIX1E_ACL_ACCESS_EXTATTR_NAME, sizeof(*ap->a_aclp),
		    (char *) ap->a_aclp, ap->a_p);
		break;

	case ACL_TYPE_DEFAULT:
		if (ap->a_aclp == NULL) {
			error = vn_extattr_rm(ap->a_vp, IO_NODELOCKED,
			    POSIX1E_ACL_DEFAULT_EXTATTR_NAMESPACE,
			    POSIX1E_ACL_DEFAULT_EXTATTR_NAME, ap->a_p);
			/*
			 * Attempting to delete a non-present default ACL
			 * will return success for portability purposes.
			 * (TRIX)
		 	 */
			/* XXX: the ENOENT here will eventually be ENOATTR. */
			if (error == EINVAL)
				error = 0;
		} else
			error = vn_extattr_set(ap->a_vp, IO_NODELOCKED,
			    POSIX1E_ACL_DEFAULT_EXTATTR_NAMESPACE,
			    POSIX1E_ACL_DEFAULT_EXTATTR_NAME,
			    sizeof(*ap->a_aclp), (char *) ap->a_aclp, ap->a_p);
		break;

	default:
		error = EINVAL;
	}
	/*
	 * Map lack of attribute definition in UFS_EXTATTR into lack of
	 * support for ACLs on the file system.
	 */
	/* XXX: ENOENT here will eventually be ENOATTR. */
	if (error == ENOENT)
		return (EOPNOTSUPP);
	if (error != 0)
		return (error);

	if (ap->a_type == ACL_TYPE_ACCESS) {
		/*
		 * Now that the EA is successfully updated, update the
		 * inode and mark it as changed.
		 */
		old_uid = ip->i_uid;
		old_gid = ip->i_gid;
		old_mode = ip->i_mode;
		preserve_mask = ISVTX | ISGID | ISUID;
		ufs_sync_inode_from_acl(ap->a_aclp, ip, preserve_mask);

		/*
		 * Clear the ISGID and ISUID bits if the ownership has
		 * changed, or appropriate privilege is not available.
		 * XXX: This should probably be a check for broadening
		 * availability of the bits, but it's not clear from the
		 * spec.
		 */
		if (suser_xxx(ap->a_cred, NULL, PRISON_ROOT) &&
		    (ip->i_gid != old_gid || ip->i_uid != old_uid))
			ip->i_mode &= ~(ISUID | ISGID);
		ip->i_flag |= IN_CHANGE;
	}

	VN_KNOTE(ap->a_vp, NOTE_ATTRIB);
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
		struct proc *p;
	} */ *ap;
{

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
