/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_ACL_ACL_UTILS_H
#define	_ACL_ACL_UTILS_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"


#include <sys/types.h>
#include <sys/acl.h>
#include <sys/stat.h>

#ifdef  __cplusplus
extern "C" {
#endif

extern ace_t trivial_acl[6];

extern int acltrivial(const char *);
extern void adjust_ace_pair(ace_t *pair, mode_t mode);
extern int ace_trivial(ace_t *acep, int aclcnt);
void ksort(caddr_t v, int n, int s, int (*f)());
int cmp2acls(void *a, void *b);




#ifdef  __cplusplus
}
#endif

#endif /* _ACL_ACL_UTILS_H */
