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

#ifndef	_SYS_FS_ZFS_ACL_H
#define	_SYS_FS_ZFS_ACL_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef _KERNEL
#include <sys/cred.h>
#endif
#include <sys/acl.h>
#include <sys/dmu.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct znode_phys;

#define	ACCESS_UNDETERMINED	-1

#define	ACE_SLOT_CNT	6

typedef struct zfs_znode_acl {
	uint64_t	z_acl_extern_obj;	  /* ext acl pieces */
	uint32_t	z_acl_count;		  /* Number of ACEs */
	uint16_t	z_acl_version;		  /* acl version */
	uint16_t	z_acl_pad;		  /* pad */
	ace_t		z_ace_data[ACE_SLOT_CNT]; /* 6 standard ACEs */
} zfs_znode_acl_t;

#define	ACL_DATA_ALLOCED	0x1

/*
 * Max ACL size is prepended deny for all entries + the
 * canonical six tacked on * the end.
 */
#define	MAX_ACL_SIZE	(MAX_ACL_ENTRIES * 2 + 6)

typedef struct zfs_acl {
	int		z_slots;	/* number of allocated slots for ACEs */
	int		z_acl_count;
	uint_t		z_state;
	ace_t		*z_acl;
} zfs_acl_t;

#define	ZFS_ACL_SIZE(aclcnt)	(sizeof (ace_t) * (aclcnt))

/*
 * Property values for acl_mode and acl_inherit.
 *
 * acl_mode can take discard, noallow, groupmask and passthrough.
 * whereas acl_inherit has secure instead of groupmask.
 */

#define	ZFS_ACL_DISCARD		0
#define	ZFS_ACL_NOALLOW		1
#define	ZFS_ACL_GROUPMASK	2
#define	ZFS_ACL_PASSTHROUGH	3
#define	ZFS_ACL_SECURE		4

struct znode;

#ifdef _KERNEL
void zfs_perm_init(struct znode *, struct znode *, int, vattr_t *,
    dmu_tx_t *, cred_t *);
#ifdef TODO
int zfs_getacl(struct znode *, vsecattr_t *, cred_t *);
#endif
int zfs_mode_update(struct znode *, uint64_t, dmu_tx_t  *);
#ifdef TODO
int zfs_setacl(struct znode *, vsecattr_t *, cred_t *);
#endif
void zfs_acl_rele(void *);
void zfs_ace_byteswap(ace_t *, int);
extern int zfs_zaccess(struct znode *, int, cred_t *);
extern int zfs_zaccess_rwx(struct znode *, mode_t, cred_t *);
extern int zfs_acl_access(struct znode *, int, cred_t *);
int zfs_acl_chmod_setattr(struct znode *, uint64_t, dmu_tx_t *);
int zfs_zaccess_delete(struct znode *, struct znode *, cred_t *);
int zfs_zaccess_rename(struct znode *, struct znode *,
    struct znode *, struct znode *, cred_t *cr);
int zfs_zaccess_v4_perm(struct znode *, int, cred_t *);
void zfs_acl_free(zfs_acl_t *);

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* !ZFS_NO_ACL */
