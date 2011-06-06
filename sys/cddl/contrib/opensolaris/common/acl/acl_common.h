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
 * Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
 */

#ifndef	_ACL_COMMON_H
#define	_ACL_COMMON_H

#include <sys/types.h>
#include <sys/acl.h>
#include <sys/stat.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern ace_t trivial_acl[6];

extern int acltrivial(const char *);
extern void adjust_ace_pair(ace_t *pair, mode_t mode);
extern void adjust_ace_pair_common(void *, size_t, size_t, mode_t);
extern int ace_trivial(ace_t *acep, int aclcnt);
extern int ace_trivial_common(void *, int,
    uint64_t (*walk)(void *, uint64_t, int aclcnt, uint16_t *, uint16_t *,
    uint32_t *mask));
#if !defined(_KERNEL)
extern acl_t *acl_alloc(acl_type_t);
extern void acl_free(acl_t *aclp);
extern int acl_translate(acl_t *aclp, int target_flavor,
    int isdir, uid_t owner, gid_t group);
#endif	/* !_KERNEL */
void ksort(caddr_t v, int n, int s, int (*f)());
int cmp2acls(void *a, void *b);
int acl_trivial_create(mode_t mode, ace_t **acl, int *count);
void acl_trivial_access_masks(mode_t mode, uint32_t *allow0, uint32_t *deny1,
    uint32_t *deny2, uint32_t *owner, uint32_t *group, uint32_t *everyone);

#ifdef	__cplusplus
}
#endif

#endif /* _ACL_COMMON_H */
