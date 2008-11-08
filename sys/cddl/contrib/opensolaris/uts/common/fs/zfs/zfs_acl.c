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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/resource.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/unistd.h>
#include <sys/sdt.h>
#include <sys/fs/zfs.h>
#include <sys/policy.h>
#include <sys/zfs_znode.h>
#include <sys/zfs_acl.h>
#include <sys/zfs_dir.h>
#include <sys/zfs_vfsops.h>
#include <sys/dmu.h>
#include <sys/zap.h>
#include <acl/acl_common.h>

#define	ALLOW	ACE_ACCESS_ALLOWED_ACE_TYPE
#define	DENY	ACE_ACCESS_DENIED_ACE_TYPE

#define	OWNING_GROUP		(ACE_GROUP|ACE_IDENTIFIER_GROUP)
#define	EVERYONE_ALLOW_MASK (ACE_READ_ACL|ACE_READ_ATTRIBUTES | \
    ACE_READ_NAMED_ATTRS|ACE_SYNCHRONIZE)
#define	EVERYONE_DENY_MASK (ACE_WRITE_ACL|ACE_WRITE_OWNER | \
    ACE_WRITE_ATTRIBUTES|ACE_WRITE_NAMED_ATTRS)
#define	OWNER_ALLOW_MASK (ACE_WRITE_ACL | ACE_WRITE_OWNER | \
    ACE_WRITE_ATTRIBUTES|ACE_WRITE_NAMED_ATTRS)
#define	WRITE_MASK (ACE_WRITE_DATA|ACE_APPEND_DATA|ACE_WRITE_NAMED_ATTRS| \
    ACE_WRITE_ATTRIBUTES|ACE_WRITE_ACL|ACE_WRITE_OWNER)

#define	OGE_CLEAR	(ACE_READ_DATA|ACE_LIST_DIRECTORY|ACE_WRITE_DATA| \
    ACE_ADD_FILE|ACE_APPEND_DATA|ACE_ADD_SUBDIRECTORY|ACE_EXECUTE)

#define	OKAY_MASK_BITS (ACE_READ_DATA|ACE_LIST_DIRECTORY|ACE_WRITE_DATA| \
    ACE_ADD_FILE|ACE_APPEND_DATA|ACE_ADD_SUBDIRECTORY|ACE_EXECUTE)

#define	ALL_INHERIT	(ACE_FILE_INHERIT_ACE|ACE_DIRECTORY_INHERIT_ACE | \
    ACE_NO_PROPAGATE_INHERIT_ACE|ACE_INHERIT_ONLY_ACE)

#define	SECURE_CLEAR	(ACE_WRITE_ACL|ACE_WRITE_OWNER)

#define	OGE_PAD	6		/* traditional owner/group/everyone ACES */

static int zfs_ace_can_use(znode_t *zp, ace_t *);

static zfs_acl_t *
zfs_acl_alloc(int slots)
{
	zfs_acl_t *aclp;

	aclp = kmem_zalloc(sizeof (zfs_acl_t), KM_SLEEP);
	if (slots != 0) {
		aclp->z_acl = kmem_alloc(ZFS_ACL_SIZE(slots), KM_SLEEP);
		aclp->z_acl_count = 0;
		aclp->z_state = ACL_DATA_ALLOCED;
	} else {
		aclp->z_state = 0;
	}
	aclp->z_slots = slots;
	return (aclp);
}

void
zfs_acl_free(zfs_acl_t *aclp)
{
	if (aclp->z_state == ACL_DATA_ALLOCED) {
		kmem_free(aclp->z_acl, ZFS_ACL_SIZE(aclp->z_slots));
	}
	kmem_free(aclp, sizeof (zfs_acl_t));
}

static uint32_t
zfs_v4_to_unix(uint32_t access_mask)
{
	uint32_t new_mask = 0;

	/*
	 * This is used for mapping v4 permissions into permissions
	 * that can be passed to secpolicy_vnode_access()
	 */
	if (access_mask & (ACE_READ_DATA | ACE_LIST_DIRECTORY |
	    ACE_READ_ATTRIBUTES | ACE_READ_ACL))
		new_mask |= S_IROTH;
	if (access_mask & (ACE_WRITE_DATA | ACE_APPEND_DATA |
	    ACE_WRITE_ATTRIBUTES | ACE_ADD_FILE | ACE_WRITE_NAMED_ATTRS))
		new_mask |= S_IWOTH;
	if (access_mask & (ACE_EXECUTE | ACE_READ_NAMED_ATTRS))
		new_mask |= S_IXOTH;

	return (new_mask);
}

/*
 * Convert unix access mask to v4 access mask
 */
static uint32_t
zfs_unix_to_v4(uint32_t access_mask)
{
	uint32_t new_mask = 0;

	if (access_mask & 01)
		new_mask |= (ACE_EXECUTE);
	if (access_mask & 02) {
		new_mask |= (ACE_WRITE_DATA);
	} if (access_mask & 04) {
		new_mask |= ACE_READ_DATA;
	}
	return (new_mask);
}

static void
zfs_set_ace(ace_t *zacep, uint32_t access_mask, int access_type,
    uid_t uid, int entry_type)
{
	zacep->a_access_mask = access_mask;
	zacep->a_type = access_type;
	zacep->a_who = uid;
	zacep->a_flags = entry_type;
}

static uint64_t
zfs_mode_compute(znode_t *zp, zfs_acl_t *aclp)
{
	int 	i;
	int	entry_type;
	mode_t	mode = (zp->z_phys->zp_mode &
	    (S_IFMT | S_ISUID | S_ISGID | S_ISVTX));
	mode_t	 seen = 0;
	ace_t 	*acep;

	for (i = 0, acep = aclp->z_acl;
	    i != aclp->z_acl_count; i++, acep++) {
		entry_type = (acep->a_flags & ACE_TYPE_FLAGS);
		if (entry_type == ACE_OWNER) {
			if ((acep->a_access_mask & ACE_READ_DATA) &&
			    (!(seen & S_IRUSR))) {
				seen |= S_IRUSR;
				if (acep->a_type == ALLOW) {
					mode |= S_IRUSR;
				}
			}
			if ((acep->a_access_mask & ACE_WRITE_DATA) &&
			    (!(seen & S_IWUSR))) {
				seen |= S_IWUSR;
				if (acep->a_type == ALLOW) {
					mode |= S_IWUSR;
				}
			}
			if ((acep->a_access_mask & ACE_EXECUTE) &&
			    (!(seen & S_IXUSR))) {
				seen |= S_IXUSR;
				if (acep->a_type == ALLOW) {
					mode |= S_IXUSR;
				}
			}
		} else if (entry_type == OWNING_GROUP) {
			if ((acep->a_access_mask & ACE_READ_DATA) &&
			    (!(seen & S_IRGRP))) {
				seen |= S_IRGRP;
				if (acep->a_type == ALLOW) {
					mode |= S_IRGRP;
				}
			}
			if ((acep->a_access_mask & ACE_WRITE_DATA) &&
			    (!(seen & S_IWGRP))) {
				seen |= S_IWGRP;
				if (acep->a_type == ALLOW) {
					mode |= S_IWGRP;
				}
			}
			if ((acep->a_access_mask & ACE_EXECUTE) &&
			    (!(seen & S_IXGRP))) {
				seen |= S_IXGRP;
				if (acep->a_type == ALLOW) {
					mode |= S_IXGRP;
				}
			}
		} else if (entry_type == ACE_EVERYONE) {
			if ((acep->a_access_mask & ACE_READ_DATA)) {
				if (!(seen & S_IRUSR)) {
					seen |= S_IRUSR;
					if (acep->a_type == ALLOW) {
						mode |= S_IRUSR;
					}
				}
				if (!(seen & S_IRGRP)) {
					seen |= S_IRGRP;
					if (acep->a_type == ALLOW) {
						mode |= S_IRGRP;
					}
				}
				if (!(seen & S_IROTH)) {
					seen |= S_IROTH;
					if (acep->a_type == ALLOW) {
						mode |= S_IROTH;
					}
				}
			}
			if ((acep->a_access_mask & ACE_WRITE_DATA)) {
				if (!(seen & S_IWUSR)) {
					seen |= S_IWUSR;
					if (acep->a_type == ALLOW) {
						mode |= S_IWUSR;
					}
				}
				if (!(seen & S_IWGRP)) {
					seen |= S_IWGRP;
					if (acep->a_type == ALLOW) {
						mode |= S_IWGRP;
					}
				}
				if (!(seen & S_IWOTH)) {
					seen |= S_IWOTH;
					if (acep->a_type == ALLOW) {
						mode |= S_IWOTH;
					}
				}
			}
			if ((acep->a_access_mask & ACE_EXECUTE)) {
				if (!(seen & S_IXUSR)) {
					seen |= S_IXUSR;
					if (acep->a_type == ALLOW) {
						mode |= S_IXUSR;
					}
				}
				if (!(seen & S_IXGRP)) {
					seen |= S_IXGRP;
					if (acep->a_type == ALLOW) {
						mode |= S_IXGRP;
					}
				}
				if (!(seen & S_IXOTH)) {
					seen |= S_IXOTH;
					if (acep->a_type == ALLOW) {
						mode |= S_IXOTH;
					}
				}
			}
		}
	}
	return (mode);
}

static zfs_acl_t *
zfs_acl_node_read_internal(znode_t *zp)
{
	zfs_acl_t	*aclp;

	aclp = zfs_acl_alloc(0);
	aclp->z_acl_count = zp->z_phys->zp_acl.z_acl_count;
	aclp->z_acl = &zp->z_phys->zp_acl.z_ace_data[0];

	return (aclp);
}

/*
 * Read an external acl object.
 */
static int
zfs_acl_node_read(znode_t *zp, zfs_acl_t **aclpp)
{
	uint64_t extacl = zp->z_phys->zp_acl.z_acl_extern_obj;
	zfs_acl_t	*aclp;
	int error;

	ASSERT(MUTEX_HELD(&zp->z_acl_lock));

	if (zp->z_phys->zp_acl.z_acl_extern_obj == 0) {
		*aclpp = zfs_acl_node_read_internal(zp);
		return (0);
	}

	aclp = zfs_acl_alloc(zp->z_phys->zp_acl.z_acl_count);

	error = dmu_read(zp->z_zfsvfs->z_os, extacl, 0,
	    ZFS_ACL_SIZE(zp->z_phys->zp_acl.z_acl_count), aclp->z_acl);
	if (error != 0) {
		zfs_acl_free(aclp);
		return (error);
	}

	aclp->z_acl_count = zp->z_phys->zp_acl.z_acl_count;

	*aclpp = aclp;
	return (0);
}

static boolean_t
zfs_acl_valid(znode_t *zp, ace_t *uace, int aclcnt, int *inherit)
{
	ace_t 	*acep;
	int i;

	*inherit = 0;

	if (aclcnt > MAX_ACL_ENTRIES || aclcnt <= 0) {
		return (B_FALSE);
	}

	for (i = 0, acep = uace; i != aclcnt; i++, acep++) {

		/*
		 * first check type of entry
		 */

		switch (acep->a_flags & ACE_TYPE_FLAGS) {
		case ACE_OWNER:
			acep->a_who = -1;
			break;
		case (ACE_IDENTIFIER_GROUP | ACE_GROUP):
		case ACE_IDENTIFIER_GROUP:
			if (acep->a_flags & ACE_GROUP) {
				acep->a_who = -1;
			}
			break;
		case ACE_EVERYONE:
			acep->a_who = -1;
			break;
		}

		/*
		 * next check inheritance level flags
		 */

		if (acep->a_type != ALLOW && acep->a_type != DENY)
			return (B_FALSE);

		/*
		 * Only directories should have inheritance flags.
		 */
		if (ZTOV(zp)->v_type != VDIR && (acep->a_flags &
		    (ACE_FILE_INHERIT_ACE|ACE_DIRECTORY_INHERIT_ACE|
		    ACE_INHERIT_ONLY_ACE|ACE_NO_PROPAGATE_INHERIT_ACE))) {
			return (B_FALSE);
		}

		if (acep->a_flags &
		    (ACE_FILE_INHERIT_ACE|ACE_DIRECTORY_INHERIT_ACE))
			*inherit = 1;

		if (acep->a_flags &
		    (ACE_INHERIT_ONLY_ACE|ACE_NO_PROPAGATE_INHERIT_ACE)) {
			if ((acep->a_flags & (ACE_FILE_INHERIT_ACE|
			    ACE_DIRECTORY_INHERIT_ACE)) == 0) {
				return (B_FALSE);
			}
		}
	}

	return (B_TRUE);
}
/*
 * common code for setting acl's.
 *
 * This function is called from zfs_mode_update, zfs_perm_init, and zfs_setacl.
 * zfs_setacl passes a non-NULL inherit pointer (ihp) to indicate that it's
 * already checked the acl and knows whether to inherit.
 */
int
zfs_aclset_common(znode_t *zp, zfs_acl_t *aclp, dmu_tx_t *tx, int *ihp)
{
	int 		inherit = 0;
	int		error;
	znode_phys_t	*zphys = zp->z_phys;
	zfs_znode_acl_t	*zacl = &zphys->zp_acl;
	uint32_t	acl_phys_size = ZFS_ACL_SIZE(aclp->z_acl_count);
	zfsvfs_t	*zfsvfs = zp->z_zfsvfs;
	uint64_t	aoid = zphys->zp_acl.z_acl_extern_obj;

	ASSERT(MUTEX_HELD(&zp->z_lock));
	ASSERT(MUTEX_HELD(&zp->z_acl_lock));

	if (ihp)
		inherit = *ihp;		/* already determined by caller */
	else if (!zfs_acl_valid(zp, aclp->z_acl,
	    aclp->z_acl_count, &inherit)) {
		return (EINVAL);
	}

	dmu_buf_will_dirty(zp->z_dbuf, tx);

	/*
	 * Will ACL fit internally?
	 */
	if (aclp->z_acl_count > ACE_SLOT_CNT) {
		if (aoid == 0) {
			aoid = dmu_object_alloc(zfsvfs->z_os,
			    DMU_OT_ACL, acl_phys_size, DMU_OT_NONE, 0, tx);
		} else {
			(void) dmu_object_set_blocksize(zfsvfs->z_os, aoid,
			    acl_phys_size, 0, tx);
		}
		zphys->zp_acl.z_acl_extern_obj = aoid;
		zphys->zp_acl.z_acl_count = aclp->z_acl_count;
		dmu_write(zfsvfs->z_os, aoid, 0,
		    acl_phys_size, aclp->z_acl, tx);
	} else {
		/*
		 * Migrating back embedded?
		 */
		if (zphys->zp_acl.z_acl_extern_obj) {
			error = dmu_object_free(zfsvfs->z_os,
				zp->z_phys->zp_acl.z_acl_extern_obj, tx);
			if (error)
				return (error);
			zphys->zp_acl.z_acl_extern_obj = 0;
		}
		bcopy(aclp->z_acl, zacl->z_ace_data,
		    aclp->z_acl_count * sizeof (ace_t));
		zacl->z_acl_count = aclp->z_acl_count;
	}

	zp->z_phys->zp_flags &= ~(ZFS_ACL_TRIVIAL|ZFS_INHERIT_ACE);
	if (inherit) {
		zp->z_phys->zp_flags |= ZFS_INHERIT_ACE;
	} else if (ace_trivial(zacl->z_ace_data, zacl->z_acl_count) == 0) {
		zp->z_phys->zp_flags |= ZFS_ACL_TRIVIAL;
	}

	zphys->zp_mode = zfs_mode_compute(zp, aclp);
	zfs_time_stamper_locked(zp, STATE_CHANGED, tx);

	return (0);
}

/*
 * Create space for slots_needed ACEs to be append
 * to aclp.
 */
static void
zfs_acl_append(zfs_acl_t *aclp, int slots_needed)
{
	ace_t	*newacep;
	ace_t	*oldaclp;
	int	slot_cnt;
	int 	slots_left = aclp->z_slots - aclp->z_acl_count;

	if (aclp->z_state == ACL_DATA_ALLOCED)
		ASSERT(aclp->z_slots >= aclp->z_acl_count);
	if (slots_left < slots_needed || aclp->z_state != ACL_DATA_ALLOCED) {
		slot_cnt = aclp->z_slots +  1 + (slots_needed - slots_left);
		newacep = kmem_alloc(ZFS_ACL_SIZE(slot_cnt), KM_SLEEP);
		bcopy(aclp->z_acl, newacep,
		    ZFS_ACL_SIZE(aclp->z_acl_count));
		oldaclp = aclp->z_acl;
		if (aclp->z_state == ACL_DATA_ALLOCED)
			kmem_free(oldaclp, ZFS_ACL_SIZE(aclp->z_slots));
		aclp->z_acl = newacep;
		aclp->z_slots = slot_cnt;
		aclp->z_state = ACL_DATA_ALLOCED;
	}
}

/*
 * Remove "slot" ACE from aclp
 */
static void
zfs_ace_remove(zfs_acl_t *aclp, int slot)
{
	if (aclp->z_acl_count > 1) {
		(void) memmove(&aclp->z_acl[slot],
		    &aclp->z_acl[slot +1], sizeof (ace_t) *
		    (--aclp->z_acl_count - slot));
	} else
		aclp->z_acl_count--;
}

/*
 * Update access mask for prepended ACE
 *
 * This applies the "groupmask" value for aclmode property.
 */
static void
zfs_acl_prepend_fixup(ace_t *acep, ace_t *origacep, mode_t mode, uid_t owner)
{

	int	rmask, wmask, xmask;
	int	user_ace;

	user_ace = (!(acep->a_flags &
	    (ACE_OWNER|ACE_GROUP|ACE_IDENTIFIER_GROUP)));

	if (user_ace && (acep->a_who == owner)) {
		rmask = S_IRUSR;
		wmask = S_IWUSR;
		xmask = S_IXUSR;
	} else {
		rmask = S_IRGRP;
		wmask = S_IWGRP;
		xmask = S_IXGRP;
	}

	if (origacep->a_access_mask & ACE_READ_DATA) {
		if (mode & rmask)
			acep->a_access_mask &= ~ACE_READ_DATA;
		else
			acep->a_access_mask |= ACE_READ_DATA;
	}

	if (origacep->a_access_mask & ACE_WRITE_DATA) {
		if (mode & wmask)
			acep->a_access_mask &= ~ACE_WRITE_DATA;
		else
			acep->a_access_mask |= ACE_WRITE_DATA;
	}

	if (origacep->a_access_mask & ACE_APPEND_DATA) {
		if (mode & wmask)
			acep->a_access_mask &= ~ACE_APPEND_DATA;
		else
			acep->a_access_mask |= ACE_APPEND_DATA;
	}

	if (origacep->a_access_mask & ACE_EXECUTE) {
		if (mode & xmask)
			acep->a_access_mask &= ~ACE_EXECUTE;
		else
			acep->a_access_mask |= ACE_EXECUTE;
	}
}

/*
 * Apply mode to canonical six ACEs.
 */
static void
zfs_acl_fixup_canonical_six(zfs_acl_t *aclp, mode_t mode)
{
	int	cnt;
	ace_t	*acep;

	cnt = aclp->z_acl_count -1;
	acep = aclp->z_acl;

	/*
	 * Fixup final ACEs to match the mode
	 */

	ASSERT(cnt >= 5);
	adjust_ace_pair(&acep[cnt - 1], mode);	/* everyone@ */
	adjust_ace_pair(&acep[cnt - 3], (mode & 0070) >> 3);	/* group@ */
	adjust_ace_pair(&acep[cnt - 5], (mode & 0700) >> 6);	/* owner@ */
}


static int
zfs_acl_ace_match(ace_t *acep, int allow_deny, int type, int mask)
{
	return (acep->a_access_mask == mask && acep->a_type == allow_deny &&
	    ((acep->a_flags & ACE_TYPE_FLAGS) == type));
}

/*
 * Can prepended ACE be reused?
 */
static int
zfs_reuse_deny(ace_t *acep, int i)
{
	int okay_masks;

	if (i < 1)
		return (B_FALSE);

	if (acep[i-1].a_type != DENY)
		return (B_FALSE);

	if (acep[i-1].a_flags != (acep[i].a_flags & ACE_IDENTIFIER_GROUP))
		return (B_FALSE);

	okay_masks = (acep[i].a_access_mask & OKAY_MASK_BITS);

	if (acep[i-1].a_access_mask & ~okay_masks)
		return (B_FALSE);

	return (B_TRUE);
}

/*
 * Create space to prepend an ACE
 */
static void
zfs_acl_prepend(zfs_acl_t *aclp, int i)
{
	ace_t	*oldaclp = NULL;
	ace_t	*to, *from;
	int	slots_left = aclp->z_slots - aclp->z_acl_count;
	int	oldslots;
	int	need_free = 0;

	if (aclp->z_state == ACL_DATA_ALLOCED)
		ASSERT(aclp->z_slots >= aclp->z_acl_count);

	if (slots_left == 0 || aclp->z_state != ACL_DATA_ALLOCED) {

		to = kmem_alloc(ZFS_ACL_SIZE(aclp->z_acl_count +
		    OGE_PAD), KM_SLEEP);
		if (aclp->z_state == ACL_DATA_ALLOCED)
			need_free++;
		from = aclp->z_acl;
		oldaclp = aclp->z_acl;
		(void) memmove(to, from,
		    sizeof (ace_t) * aclp->z_acl_count);
		aclp->z_state = ACL_DATA_ALLOCED;
	} else {
		from = aclp->z_acl;
		to = aclp->z_acl;
	}


	(void) memmove(&to[i + 1], &from[i],
	    sizeof (ace_t) * (aclp->z_acl_count - i));

	if (oldaclp) {
		aclp->z_acl = to;
		oldslots = aclp->z_slots;
		aclp->z_slots = aclp->z_acl_count + OGE_PAD;
		if (need_free)
			kmem_free(oldaclp, ZFS_ACL_SIZE(oldslots));
	}

}

/*
 * Prepend deny ACE
 */
static void
zfs_acl_prepend_deny(znode_t *zp, zfs_acl_t *aclp, int i,
    mode_t mode)
{
	ace_t	*acep;

	zfs_acl_prepend(aclp, i);

	acep = aclp->z_acl;
	zfs_set_ace(&acep[i], 0, DENY, acep[i + 1].a_who,
	    (acep[i + 1].a_flags & ACE_TYPE_FLAGS));
	zfs_acl_prepend_fixup(&acep[i], &acep[i+1], mode, zp->z_phys->zp_uid);
	aclp->z_acl_count++;
}

/*
 * Split an inherited ACE into inherit_only ACE
 * and original ACE with inheritance flags stripped off.
 */
static void
zfs_acl_split_ace(zfs_acl_t *aclp, int i)
{
	ace_t *acep = aclp->z_acl;

	zfs_acl_prepend(aclp, i);
	acep = aclp->z_acl;
	acep[i] = acep[i + 1];
	acep[i].a_flags |= ACE_INHERIT_ONLY_ACE;
	acep[i + 1].a_flags &= ~ALL_INHERIT;
	aclp->z_acl_count++;
}

/*
 * Are ACES started at index i, the canonical six ACES?
 */
static int
zfs_have_canonical_six(zfs_acl_t *aclp, int i)
{
	ace_t *acep = aclp->z_acl;

	if ((zfs_acl_ace_match(&acep[i],
	    DENY, ACE_OWNER, 0) &&
	    zfs_acl_ace_match(&acep[i + 1], ALLOW, ACE_OWNER,
	    OWNER_ALLOW_MASK) && zfs_acl_ace_match(&acep[i + 2],
	    DENY, OWNING_GROUP, 0) && zfs_acl_ace_match(&acep[i + 3],
	    ALLOW, OWNING_GROUP, 0) && zfs_acl_ace_match(&acep[i + 4],
	    DENY, ACE_EVERYONE, EVERYONE_DENY_MASK) &&
	    zfs_acl_ace_match(&acep[i + 5], ALLOW, ACE_EVERYONE,
	    EVERYONE_ALLOW_MASK))) {
		return (1);
	} else {
		return (0);
	}
}

/*
 * Apply step 1g, to group entries
 *
 * Need to deal with corner case where group may have
 * greater permissions than owner.  If so then limit
 * group permissions, based on what extra permissions
 * group has.
 */
static void
zfs_fixup_group_entries(ace_t *acep, mode_t mode)
{
	mode_t extramode = (mode >> 3) & 07;
	mode_t ownermode = (mode >> 6);

	if (acep[0].a_flags & ACE_IDENTIFIER_GROUP) {

		extramode &= ~ownermode;

		if (extramode) {
			if (extramode & 04) {
				acep[0].a_access_mask &= ~ACE_READ_DATA;
				acep[1].a_access_mask &= ~ACE_READ_DATA;
			}
			if (extramode & 02) {
				acep[0].a_access_mask &=
				    ~(ACE_WRITE_DATA|ACE_APPEND_DATA);
				acep[1].a_access_mask &=
				    ~(ACE_WRITE_DATA|ACE_APPEND_DATA);
			}
			if (extramode & 01) {
				acep[0].a_access_mask &= ~ACE_EXECUTE;
				acep[1].a_access_mask &= ~ACE_EXECUTE;
			}
		}
	}
}

/*
 * Apply the chmod algorithm as described
 * in PSARC/2002/240
 */
static int
zfs_acl_chmod(znode_t *zp, uint64_t mode, zfs_acl_t *aclp,
    dmu_tx_t *tx)
{
	zfsvfs_t	*zfsvfs = zp->z_zfsvfs;
	ace_t 		*acep;
	int 		i;
	int		error;
	int 		entry_type;
	int 		reuse_deny;
	int 		need_canonical_six = 1;
	int		inherit = 0;
	int		iflags;

	ASSERT(MUTEX_HELD(&zp->z_acl_lock));
	ASSERT(MUTEX_HELD(&zp->z_lock));

	i = 0;
	while (i < aclp->z_acl_count) {
		acep = aclp->z_acl;
		entry_type = (acep[i].a_flags & ACE_TYPE_FLAGS);
		iflags = (acep[i].a_flags & ALL_INHERIT);

		if ((acep[i].a_type != ALLOW && acep[i].a_type != DENY) ||
		    (iflags & ACE_INHERIT_ONLY_ACE)) {
			i++;
			if (iflags)
				inherit = 1;
			continue;
		}


		if (zfsvfs->z_acl_mode == ZFS_ACL_DISCARD) {
			zfs_ace_remove(aclp, i);
			continue;
		}

		/*
		 * Need to split ace into two?
		 */
		if ((iflags & (ACE_FILE_INHERIT_ACE|
		    ACE_DIRECTORY_INHERIT_ACE)) &&
		    (!(iflags & ACE_INHERIT_ONLY_ACE))) {
			zfs_acl_split_ace(aclp, i);
			i++;
			inherit = 1;
			continue;
		}

		if (entry_type == ACE_OWNER || entry_type == ACE_EVERYONE ||
		    (entry_type == OWNING_GROUP)) {
			acep[i].a_access_mask &= ~OGE_CLEAR;
			i++;
			continue;

		} else {
			if (acep[i].a_type == ALLOW) {

				/*
				 * Check preceding ACE if any, to see
				 * if we need to prepend a DENY ACE.
				 * This is only applicable when the acl_mode
				 * property == groupmask.
				 */
				if (zfsvfs->z_acl_mode == ZFS_ACL_GROUPMASK) {

					reuse_deny = zfs_reuse_deny(acep, i);

					if (reuse_deny == B_FALSE) {
						zfs_acl_prepend_deny(zp, aclp,
						    i, mode);
						i++;
						acep = aclp->z_acl;
					} else {
						zfs_acl_prepend_fixup(
						    &acep[i - 1],
						    &acep[i], mode,
						    zp->z_phys->zp_uid);
					}
					zfs_fixup_group_entries(&acep[i - 1],
					    mode);
				}
			}
			i++;
		}
	}

	/*
	 * Check out last six aces, if we have six.
	 */

	if (aclp->z_acl_count >= 6) {
		i = aclp->z_acl_count - 6;

		if (zfs_have_canonical_six(aclp, i)) {
			need_canonical_six = 0;
		}
	}

	if (need_canonical_six) {

		zfs_acl_append(aclp, 6);
		i = aclp->z_acl_count;
		acep = aclp->z_acl;
		zfs_set_ace(&acep[i++], 0, DENY, -1, ACE_OWNER);
		zfs_set_ace(&acep[i++], OWNER_ALLOW_MASK, ALLOW, -1, ACE_OWNER);
		zfs_set_ace(&acep[i++], 0, DENY, -1, OWNING_GROUP);
		zfs_set_ace(&acep[i++], 0, ALLOW, -1, OWNING_GROUP);
		zfs_set_ace(&acep[i++], EVERYONE_DENY_MASK,
		    DENY, -1, ACE_EVERYONE);
		zfs_set_ace(&acep[i++], EVERYONE_ALLOW_MASK,
		    ALLOW, -1, ACE_EVERYONE);
		aclp->z_acl_count += 6;
	}

	zfs_acl_fixup_canonical_six(aclp, mode);

	zp->z_phys->zp_mode = mode;
	error = zfs_aclset_common(zp, aclp, tx, &inherit);
	return (error);
}


int
zfs_acl_chmod_setattr(znode_t *zp, uint64_t mode, dmu_tx_t *tx)
{
	zfs_acl_t *aclp = NULL;
	int error;

	ASSERT(MUTEX_HELD(&zp->z_lock));
	mutex_enter(&zp->z_acl_lock);
	error = zfs_acl_node_read(zp, &aclp);
	if (error == 0)
		error = zfs_acl_chmod(zp, mode, aclp, tx);
	mutex_exit(&zp->z_acl_lock);
	if (aclp)
		zfs_acl_free(aclp);
	return (error);
}

/*
 * strip off write_owner and write_acl
 */
static void
zfs_securemode_update(zfsvfs_t *zfsvfs, ace_t *acep)
{
	if ((zfsvfs->z_acl_inherit == ZFS_ACL_SECURE) &&
	    (acep->a_type == ALLOW))
		acep->a_access_mask &= ~SECURE_CLEAR;
}

/*
 * inherit inheritable ACEs from parent
 */
static zfs_acl_t *
zfs_acl_inherit(znode_t *zp, zfs_acl_t *paclp)
{
	zfsvfs_t	*zfsvfs = zp->z_zfsvfs;
	ace_t 		*pacep;
	ace_t		*acep;
	int 		ace_cnt = 0;
	int		pace_cnt;
	int 		i, j;
	zfs_acl_t	*aclp = NULL;

	i = j = 0;
	pace_cnt = paclp->z_acl_count;
	pacep = paclp->z_acl;
	if (zfsvfs->z_acl_inherit != ZFS_ACL_DISCARD) {
		for (i = 0; i != pace_cnt; i++) {

			if (zfsvfs->z_acl_inherit == ZFS_ACL_NOALLOW &&
			    pacep[i].a_type == ALLOW)
				continue;

			if (zfs_ace_can_use(zp, &pacep[i])) {
				ace_cnt++;
				if (!(pacep[i].a_flags &
				    ACE_NO_PROPAGATE_INHERIT_ACE))
					ace_cnt++;
			}
		}
	}

	aclp = zfs_acl_alloc(ace_cnt + OGE_PAD);
	if (ace_cnt && zfsvfs->z_acl_inherit != ZFS_ACL_DISCARD) {
		acep = aclp->z_acl;
		pacep = paclp->z_acl;
		for (i = 0; i != pace_cnt; i++) {

			if (zfsvfs->z_acl_inherit == ZFS_ACL_NOALLOW &&
			    pacep[i].a_type == ALLOW)
				continue;

			if (zfs_ace_can_use(zp, &pacep[i])) {

				/*
				 * Now create entry for inherited ace
				 */

				acep[j] = pacep[i];

				/*
				 * When AUDIT/ALARM a_types are supported
				 * they should be inherited here.
				 */

				if ((pacep[i].a_flags &
				    ACE_NO_PROPAGATE_INHERIT_ACE) ||
				    (ZTOV(zp)->v_type != VDIR)) {
					acep[j].a_flags &= ~ALL_INHERIT;
					zfs_securemode_update(zfsvfs, &acep[j]);
					j++;
					continue;
				}

				ASSERT(ZTOV(zp)->v_type == VDIR);

				/*
				 * If we are inheriting an ACE targeted for
				 * only files, then make sure inherit_only
				 * is on for future propagation.
				 */
				if ((pacep[i].a_flags & (ACE_FILE_INHERIT_ACE |
				    ACE_DIRECTORY_INHERIT_ACE)) !=
				    ACE_FILE_INHERIT_ACE) {
					j++;
					acep[j] = acep[j-1];
					acep[j-1].a_flags |=
					    ACE_INHERIT_ONLY_ACE;
					acep[j].a_flags &= ~ALL_INHERIT;
				} else {
					acep[j].a_flags |= ACE_INHERIT_ONLY_ACE;
				}
				zfs_securemode_update(zfsvfs, &acep[j]);
				j++;
			}
		}
	}
	aclp->z_acl_count = j;
	ASSERT(aclp->z_slots >= aclp->z_acl_count);

	return (aclp);
}

/*
 * Create file system object initial permissions
 * including inheritable ACEs.
 */
void
zfs_perm_init(znode_t *zp, znode_t *parent, int flag,
    vattr_t *vap, dmu_tx_t *tx, cred_t *cr)
{
	uint64_t	mode;
	uid_t		uid;
	gid_t		gid;
	int		error;
	int		pull_down;
	zfs_acl_t	*aclp, *paclp;

	mode = MAKEIMODE(vap->va_type, vap->va_mode);

	/*
	 * Determine uid and gid.
	 */
	if ((flag & (IS_ROOT_NODE | IS_REPLAY)) ||
	    ((flag & IS_XATTR) && (vap->va_type == VDIR))) {
		uid = vap->va_uid;
		gid = vap->va_gid;
	} else {
		uid = crgetuid(cr);
		if ((vap->va_mask & AT_GID) &&
		    ((vap->va_gid == parent->z_phys->zp_gid) ||
		    groupmember(vap->va_gid, cr) ||
		    secpolicy_vnode_create_gid(cr) == 0))
			gid = vap->va_gid;
		else
#ifdef __FreeBSD__
			gid = parent->z_phys->zp_gid;
#else
			gid = (parent->z_phys->zp_mode & S_ISGID) ?
			    parent->z_phys->zp_gid : crgetgid(cr);
#endif
	}

	/*
	 * If we're creating a directory, and the parent directory has the
	 * set-GID bit set, set in on the new directory.
	 * Otherwise, if the user is neither privileged nor a member of the
	 * file's new group, clear the file's set-GID bit.
	 */

	if ((parent->z_phys->zp_mode & S_ISGID) && (vap->va_type == VDIR))
		mode |= S_ISGID;
	else {
		if ((mode & S_ISGID) &&
		    secpolicy_vnode_setids_setgids(cr, gid) != 0)
			mode &= ~S_ISGID;
	}

	zp->z_phys->zp_uid = uid;
	zp->z_phys->zp_gid = gid;
	zp->z_phys->zp_mode = mode;

	mutex_enter(&parent->z_lock);
	pull_down = (parent->z_phys->zp_flags & ZFS_INHERIT_ACE);
	if (pull_down) {
		mutex_enter(&parent->z_acl_lock);
		VERIFY(0 == zfs_acl_node_read(parent, &paclp));
		mutex_exit(&parent->z_acl_lock);
		aclp = zfs_acl_inherit(zp, paclp);
		zfs_acl_free(paclp);
	} else {
		aclp = zfs_acl_alloc(6);
	}
	mutex_exit(&parent->z_lock);
	mutex_enter(&zp->z_lock);
	mutex_enter(&zp->z_acl_lock);
	error = zfs_acl_chmod(zp, mode, aclp, tx);
	mutex_exit(&zp->z_lock);
	mutex_exit(&zp->z_acl_lock);
	ASSERT3U(error, ==, 0);
	zfs_acl_free(aclp);
}

/*
 * Should ACE be inherited?
 */
static int
zfs_ace_can_use(znode_t *zp, ace_t *acep)
{
	int vtype = ZTOV(zp)->v_type;

	int	iflags = (acep->a_flags & 0xf);

	if ((vtype == VDIR) && (iflags & ACE_DIRECTORY_INHERIT_ACE))
		return (1);
	else if (iflags & ACE_FILE_INHERIT_ACE)
		return (!((vtype == VDIR) &&
		    (iflags & ACE_NO_PROPAGATE_INHERIT_ACE)));
	return (0);
}

#ifdef TODO
/*
 * Retrieve a files ACL
 */
int
zfs_getacl(znode_t *zp, vsecattr_t  *vsecp, cred_t *cr)
{
	zfs_acl_t	*aclp;
	ulong_t		mask = vsecp->vsa_mask & (VSA_ACE | VSA_ACECNT);
	int		error;

	if (error = zfs_zaccess(zp, ACE_READ_ACL, cr)) {
		/*
		 * If owner of file then allow reading of the
		 * ACL.
		 */
		if (crgetuid(cr) != zp->z_phys->zp_uid)
			return (error);
	}

	if (mask == 0)
		return (ENOSYS);

	mutex_enter(&zp->z_acl_lock);

	error = zfs_acl_node_read(zp, &aclp);
	if (error != 0) {
		mutex_exit(&zp->z_acl_lock);
		return (error);
	}


	if (mask & VSA_ACECNT) {
		vsecp->vsa_aclcnt = aclp->z_acl_count;
	}

	if (mask & VSA_ACE) {
		vsecp->vsa_aclentp = kmem_alloc(aclp->z_acl_count *
		    sizeof (ace_t), KM_SLEEP);
		bcopy(aclp->z_acl, vsecp->vsa_aclentp,
		    aclp->z_acl_count * sizeof (ace_t));
	}

	mutex_exit(&zp->z_acl_lock);

	zfs_acl_free(aclp);

	return (0);
}
#endif	/* TODO */

#ifdef TODO
/*
 * Set a files ACL
 */
int
zfs_setacl(znode_t *zp, vsecattr_t *vsecp, cred_t *cr)
{
	zfsvfs_t	*zfsvfs = zp->z_zfsvfs;
	zilog_t		*zilog = zfsvfs->z_log;
	ace_t		*acep = vsecp->vsa_aclentp;
	int		aclcnt = vsecp->vsa_aclcnt;
	ulong_t		mask = vsecp->vsa_mask & (VSA_ACE | VSA_ACECNT);
	dmu_tx_t	*tx;
	int		error;
	int		inherit;
	zfs_acl_t	*aclp;

	if (mask == 0)
		return (EINVAL);

	if (!zfs_acl_valid(zp, acep, aclcnt, &inherit))
		return (EINVAL);
top:
	error = zfs_zaccess_v4_perm(zp, ACE_WRITE_ACL, cr);
	if (error == EACCES || error == ACCESS_UNDETERMINED) {
		if ((error = secpolicy_vnode_setdac(cr,
		    zp->z_phys->zp_uid)) != 0) {
			return (error);
		}
	} else if (error) {
		return (error == EROFS ? error : EPERM);
	}

	mutex_enter(&zp->z_lock);
	mutex_enter(&zp->z_acl_lock);

	tx = dmu_tx_create(zfsvfs->z_os);
	dmu_tx_hold_bonus(tx, zp->z_id);

	if (zp->z_phys->zp_acl.z_acl_extern_obj) {
		dmu_tx_hold_write(tx, zp->z_phys->zp_acl.z_acl_extern_obj,
		    0, ZFS_ACL_SIZE(aclcnt));
	} else if (aclcnt > ACE_SLOT_CNT) {
		dmu_tx_hold_write(tx, DMU_NEW_OBJECT, 0, ZFS_ACL_SIZE(aclcnt));
	}

	error = dmu_tx_assign(tx, zfsvfs->z_assign);
	if (error) {
		mutex_exit(&zp->z_acl_lock);
		mutex_exit(&zp->z_lock);

		if (error == ERESTART && zfsvfs->z_assign == TXG_NOWAIT) {
			dmu_tx_wait(tx);
			dmu_tx_abort(tx);
			goto top;
		}
		dmu_tx_abort(tx);
		return (error);
	}

	aclp = zfs_acl_alloc(aclcnt);
	bcopy(acep, aclp->z_acl, sizeof (ace_t) * aclcnt);
	aclp->z_acl_count = aclcnt;
	error = zfs_aclset_common(zp, aclp, tx, &inherit);
	ASSERT(error == 0);

	zfs_acl_free(aclp);
	zfs_log_acl(zilog, tx, TX_ACL, zp, aclcnt, acep);
	dmu_tx_commit(tx);
done:
	mutex_exit(&zp->z_acl_lock);
	mutex_exit(&zp->z_lock);

	return (error);
}
#endif	/* TODO */

static int
zfs_ace_access(ace_t *zacep, int *working_mode)
{
	if (*working_mode == 0) {
		return (0);
	}

	if (zacep->a_access_mask & *working_mode) {
		if (zacep->a_type == ALLOW) {
			*working_mode &=
			    ~(*working_mode & zacep->a_access_mask);
			if (*working_mode == 0)
				return (0);
		} else if (zacep->a_type == DENY) {
			return (EACCES);
		}
	}

	/*
	 * haven't been specifcally denied at this point
	 * so return UNDETERMINED.
	 */

	return (ACCESS_UNDETERMINED);
}


static int
zfs_zaccess_common(znode_t *zp, int v4_mode, int *working_mode, cred_t *cr)
{
	zfs_acl_t	*aclp;
	zfsvfs_t	*zfsvfs = zp->z_zfsvfs;
	ace_t		*zacep;
	gid_t		gid;
	int		cnt;
	int		i;
	int		error;
	int		access_deny = ACCESS_UNDETERMINED;
	uint_t		entry_type;
	uid_t		uid = crgetuid(cr);

	if (zfsvfs->z_assign >= TXG_INITIAL) {		/* ZIL replay */
		*working_mode = 0;
		return (0);
	}

	*working_mode = v4_mode;

	if ((v4_mode & WRITE_MASK) &&
	    (zp->z_zfsvfs->z_vfs->vfs_flag & VFS_RDONLY) &&
	    (!IS_DEVVP(ZTOV(zp)))) {
		return (EROFS);
	}

	mutex_enter(&zp->z_acl_lock);

	error = zfs_acl_node_read(zp, &aclp);
	if (error != 0) {
		mutex_exit(&zp->z_acl_lock);
		return (error);
	}


	zacep = aclp->z_acl;
	cnt = aclp->z_acl_count;

	for (i = 0; i != cnt; i++) {

		DTRACE_PROBE2(zfs__access__common,
		    ace_t *, &zacep[i], int, *working_mode);

		if (zacep[i].a_flags & ACE_INHERIT_ONLY_ACE)
			continue;

		entry_type = (zacep[i].a_flags & ACE_TYPE_FLAGS);
		switch (entry_type) {
		case ACE_OWNER:
			if (uid == zp->z_phys->zp_uid) {
				access_deny = zfs_ace_access(&zacep[i],
				    working_mode);
			}
			break;
		case (ACE_IDENTIFIER_GROUP | ACE_GROUP):
		case ACE_IDENTIFIER_GROUP:
			/*
			 * Owning group gid is in znode not ACL
			 */
			if (entry_type == (ACE_IDENTIFIER_GROUP | ACE_GROUP))
				gid = zp->z_phys->zp_gid;
			else
				gid = zacep[i].a_who;

			if (groupmember(gid, cr)) {
				access_deny = zfs_ace_access(&zacep[i],
				    working_mode);
			}
			break;
		case ACE_EVERYONE:
			access_deny = zfs_ace_access(&zacep[i], working_mode);
			break;

		/* USER Entry */
		default:
			if (entry_type == 0) {
				if (uid == zacep[i].a_who) {
					access_deny = zfs_ace_access(&zacep[i],
					    working_mode);
				}
				break;
			}
			zfs_acl_free(aclp);
			mutex_exit(&zp->z_acl_lock);
			return (EIO);
		}

		if (access_deny != ACCESS_UNDETERMINED)
			break;
	}

	mutex_exit(&zp->z_acl_lock);
	zfs_acl_free(aclp);

	return (access_deny);
}


/*
 * Determine whether Access should be granted/denied, invoking least
 * priv subsytem when a deny is determined.
 */
int
zfs_zaccess(znode_t *zp, int mode, cred_t *cr)
{
	int	working_mode;
	int	error;
	int	is_attr;
	znode_t	*xzp;
	znode_t *check_zp = zp;

	is_attr = ((zp->z_phys->zp_flags & ZFS_XATTR) &&
	    (ZTOV(zp)->v_type == VDIR));

	/*
	 * If attribute then validate against base file
	 */
	if (is_attr) {
		if ((error = zfs_zget(zp->z_zfsvfs,
		    zp->z_phys->zp_parent, &xzp)) != 0)	{
			return (error);
		}
		check_zp = xzp;
		/*
		 * fixup mode to map to xattr perms
		 */

		if (mode & (ACE_WRITE_DATA|ACE_APPEND_DATA)) {
			mode &= ~(ACE_WRITE_DATA|ACE_APPEND_DATA);
			mode |= ACE_WRITE_NAMED_ATTRS;
		}

		if (mode & (ACE_READ_DATA|ACE_EXECUTE)) {
			mode &= ~(ACE_READ_DATA|ACE_EXECUTE);
			mode |= ACE_READ_NAMED_ATTRS;
		}
	}

	error = zfs_zaccess_common(check_zp, mode, &working_mode, cr);

	if (error == EROFS) {
		if (is_attr)
			VN_RELE(ZTOV(xzp));
		return (error);
	}

	if (error || working_mode) {
		working_mode = (zfs_v4_to_unix(working_mode) << 6);
		error = secpolicy_vnode_access(cr, ZTOV(check_zp),
		    check_zp->z_phys->zp_uid, working_mode);
	}

	if (is_attr)
		VN_RELE(ZTOV(xzp));

	return (error);
}

/*
 * Special zaccess function to check for special nfsv4 perm.
 * doesn't call secpolicy_vnode_access() for failure, since that
 * would probably be the wrong policy function to call.
 * instead its up to the caller to handle that situation.
 */

int
zfs_zaccess_v4_perm(znode_t *zp, int mode, cred_t *cr)
{
	int working_mode = 0;
	return (zfs_zaccess_common(zp, mode, &working_mode, cr));
}

/*
 * Translate tradition unix VREAD/VWRITE/VEXEC mode into
 * native ACL format and call zfs_zaccess()
 */
int
zfs_zaccess_rwx(znode_t *zp, mode_t mode, cred_t *cr)
{
	int v4_mode = zfs_unix_to_v4(mode >> 6);

	return (zfs_zaccess(zp, v4_mode, cr));
}

static int
zfs_delete_final_check(znode_t *zp, znode_t *dzp, cred_t *cr)
{
	int error;

	error = secpolicy_vnode_access(cr, ZTOV(zp),
	    dzp->z_phys->zp_uid, S_IWRITE|S_IEXEC);

	if (error == 0)
		error = zfs_sticky_remove_access(dzp, zp, cr);

	return (error);
}

/*
 * Determine whether Access should be granted/deny, without
 * consulting least priv subsystem.
 *
 *
 * The following chart is the recommended NFSv4 enforcement for
 * ability to delete an object.
 *
 *      -------------------------------------------------------
 *      |   Parent Dir  |           Target Object Permissions |
 *      |  permissions  |                                     |
 *      -------------------------------------------------------
 *      |               | ACL Allows | ACL Denies| Delete     |
 *      |               |  Delete    |  Delete   | unspecified|
 *      -------------------------------------------------------
 *      |  ACL Allows   | Permit     | Permit    | Permit     |
 *      |  DELETE_CHILD |                                     |
 *      -------------------------------------------------------
 *      |  ACL Denies   | Permit     | Deny      | Deny       |
 *      |  DELETE_CHILD |            |           |            |
 *      -------------------------------------------------------
 *      | ACL specifies |            |           |            |
 *      | only allow    | Permit     | Permit    | Permit     |
 *      | write and     |            |           |            |
 *      | execute       |            |           |            |
 *      -------------------------------------------------------
 *      | ACL denies    |            |           |            |
 *      | write and     | Permit     | Deny      | Deny       |
 *      | execute       |            |           |            |
 *      -------------------------------------------------------
 *         ^
 *         |
 *         No search privilege, can't even look up file?
 *
 */
int
zfs_zaccess_delete(znode_t *dzp, znode_t *zp, cred_t *cr)
{
	int dzp_working_mode = 0;
	int zp_working_mode = 0;
	int dzp_error, zp_error;

	/*
	 * Arghh, this check is going to require a couple of questions
	 * to be asked.  We want specific DELETE permissions to
	 * take precedence over WRITE/EXECUTE.  We don't
	 * want an ACL such as this to mess us up.
	 * user:joe:write_data:deny,user:joe:delete:allow
	 *
	 * However, deny permissions may ultimately be overridden
	 * by secpolicy_vnode_access().
	 */

	dzp_error = zfs_zaccess_common(dzp, ACE_DELETE_CHILD,
	    &dzp_working_mode, cr);
	zp_error = zfs_zaccess_common(zp, ACE_DELETE, &zp_working_mode, cr);

	if (dzp_error == EROFS || zp_error == EROFS)
		return (dzp_error);

	/*
	 * First check the first row.
	 * We only need to see if parent Allows delete_child
	 */
	if ((dzp_working_mode & ACE_DELETE_CHILD) == 0)
		return (0);

	/*
	 * Second row
	 * we already have the necessary information in
	 * zp_working_mode, zp_error and dzp_error.
	 */

	if ((zp_working_mode & ACE_DELETE) == 0)
		return (0);

	/*
	 * Now zp_error should either be EACCES which indicates
	 * a "deny" delete entry or ACCESS_UNDETERMINED if the "delete"
	 * entry exists on the target.
	 *
	 * dzp_error should be either EACCES which indicates a "deny"
	 * entry for delete_child or ACCESS_UNDETERMINED if no delete_child
	 * entry exists.  If value is EACCES then we are done
	 * and zfs_delete_final_check() will make the final decision
	 * regarding to allow the delete.
	 */

	ASSERT(zp_error != 0 && dzp_error != 0);
	if (dzp_error == EACCES)
		return (zfs_delete_final_check(zp, dzp, cr));

	/*
	 * Third Row
	 * Only need to check for write/execute on parent
	 */

	dzp_error = zfs_zaccess_common(dzp, ACE_WRITE_DATA|ACE_EXECUTE,
	    &dzp_working_mode, cr);

	if (dzp_error == EROFS)
		return (dzp_error);

	if ((dzp_working_mode & (ACE_WRITE_DATA|ACE_EXECUTE)) == 0)
		return (zfs_sticky_remove_access(dzp, zp, cr));

	/*
	 * Fourth Row
	 */

	if (((dzp_working_mode & (ACE_WRITE_DATA|ACE_EXECUTE)) != 0) &&
	    ((zp_working_mode & ACE_DELETE) == 0))
		return (zfs_sticky_remove_access(dzp, zp, cr));

	return (zfs_delete_final_check(zp, dzp, cr));
}

int
zfs_zaccess_rename(znode_t *sdzp, znode_t *szp, znode_t *tdzp,
    znode_t *tzp, cred_t *cr)
{
	int add_perm;
	int error;

	add_perm = (ZTOV(szp)->v_type == VDIR) ?
	    ACE_ADD_SUBDIRECTORY : ACE_ADD_FILE;

	/*
	 * Rename permissions are combination of delete permission +
	 * add file/subdir permission.
	 *
	 * BSD operating systems also require write permission
	 * on the directory being moved from one parent directory
	 * to another.
	 */
	if (ZTOV(szp)->v_type == VDIR && ZTOV(sdzp) != ZTOV(tdzp)) {
		if (error = zfs_zaccess(szp, ACE_WRITE_DATA, cr))
			return (error);
	}

	/*
	 * first make sure we do the delete portion.
	 *
	 * If that succeeds then check for add_file/add_subdir permissions
	 */

	if (error = zfs_zaccess_delete(sdzp, szp, cr))
		return (error);

	/*
	 * If we have a tzp, see if we can delete it?
	 */
	if (tzp) {
		if (error = zfs_zaccess_delete(tdzp, tzp, cr))
			return (error);
	}

	/*
	 * Now check for add permissions
	 */
	error = zfs_zaccess(tdzp, add_perm, cr);

	return (error);
}
