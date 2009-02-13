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

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>
#include <sys/acl.h>
#include <sys/stat.h>
#if defined(_KERNEL)
#include <sys/systm.h>
#include <sys/debug.h>
#else
#include <errno.h>
#include <stdlib.h>
#include <strings.h>
#include <assert.h>
#define	ASSERT	assert
#endif


ace_t trivial_acl[] = {
	{-1, 0, ACE_OWNER, ACE_ACCESS_DENIED_ACE_TYPE},
	{-1, ACE_WRITE_ACL|ACE_WRITE_OWNER|ACE_WRITE_ATTRIBUTES|
	    ACE_WRITE_NAMED_ATTRS, ACE_OWNER, ACE_ACCESS_ALLOWED_ACE_TYPE},
	{-1, 0, ACE_GROUP|ACE_IDENTIFIER_GROUP, ACE_ACCESS_DENIED_ACE_TYPE},
	{-1, 0, ACE_GROUP|ACE_IDENTIFIER_GROUP, ACE_ACCESS_ALLOWED_ACE_TYPE},
	{-1, ACE_WRITE_ACL|ACE_WRITE_OWNER| ACE_WRITE_ATTRIBUTES|
	    ACE_WRITE_NAMED_ATTRS, ACE_EVERYONE, ACE_ACCESS_DENIED_ACE_TYPE},
	{-1, ACE_READ_ACL|ACE_READ_ATTRIBUTES|ACE_READ_NAMED_ATTRS|
	    ACE_SYNCHRONIZE, ACE_EVERYONE, ACE_ACCESS_ALLOWED_ACE_TYPE}
};


void
adjust_ace_pair(ace_t *pair, mode_t mode)
{
	if (mode & S_IROTH)
		pair[1].a_access_mask |= ACE_READ_DATA;
	else
		pair[0].a_access_mask |= ACE_READ_DATA;
	if (mode & S_IWOTH)
		pair[1].a_access_mask |=
		    ACE_WRITE_DATA|ACE_APPEND_DATA;
	else
		pair[0].a_access_mask |=
		    ACE_WRITE_DATA|ACE_APPEND_DATA;
	if (mode & S_IXOTH)
		pair[1].a_access_mask |= ACE_EXECUTE;
	else
		pair[0].a_access_mask |= ACE_EXECUTE;
}

/*
 * ace_trivial:
 * determine whether an ace_t acl is trivial
 *
 * Trivialness implys that the acl is composed of only
 * owner, group, everyone entries.  ACL can't
 * have read_acl denied, and write_owner/write_acl/write_attributes
 * can only be owner@ entry.
 */
int
ace_trivial(ace_t *acep, int aclcnt)
{
	int i;
	int owner_seen = 0;
	int group_seen = 0;
	int everyone_seen = 0;

	for (i = 0; i != aclcnt; i++) {
		switch (acep[i].a_flags & 0xf040) {
		case ACE_OWNER:
			if (group_seen || everyone_seen)
				return (1);
			owner_seen++;
			break;
		case ACE_GROUP|ACE_IDENTIFIER_GROUP:
			if (everyone_seen || owner_seen == 0)
				return (1);
			group_seen++;
			break;

		case ACE_EVERYONE:
			if (owner_seen == 0 || group_seen == 0)
				return (1);
			everyone_seen++;
			break;
		default:
			return (1);

		}

		if (acep[i].a_flags & (ACE_FILE_INHERIT_ACE|
		    ACE_DIRECTORY_INHERIT_ACE|ACE_NO_PROPAGATE_INHERIT_ACE|
		    ACE_INHERIT_ONLY_ACE))
			return (1);

		/*
		 * Special check for some special bits
		 *
		 * Don't allow anybody to deny reading basic
		 * attributes or a files ACL.
		 */
		if ((acep[i].a_access_mask &
		    (ACE_READ_ACL|ACE_READ_ATTRIBUTES)) &&
		    (acep[i].a_type == ACE_ACCESS_DENIED_ACE_TYPE))
			return (1);

		/*
		 * Allow on owner@ to allow
		 * write_acl/write_owner/write_attributes
		 */
		if (acep[i].a_type == ACE_ACCESS_ALLOWED_ACE_TYPE &&
		    (!(acep[i].a_flags & ACE_OWNER) && (acep[i].a_access_mask &
		    (ACE_WRITE_OWNER|ACE_WRITE_ACL|ACE_WRITE_ATTRIBUTES))))
			return (1);
	}

	if ((owner_seen == 0) || (group_seen == 0) || (everyone_seen == 0))
	    return (1);

	return (0);
}


/*
 * Generic shellsort, from K&R (1st ed, p 58.), somewhat modified.
 * v = Ptr to array/vector of objs
 * n = # objs in the array
 * s = size of each obj (must be multiples of a word size)
 * f = ptr to function to compare two objs
 *	returns (-1 = less than, 0 = equal, 1 = greater than
 */
void
ksort(caddr_t v, int n, int s, int (*f)())
{
	int g, i, j, ii;
	unsigned int *p1, *p2;
	unsigned int tmp;

	/* No work to do */
	if (v == NULL || n <= 1)
		return;

	/* Sanity check on arguments */
	ASSERT(((uintptr_t)v & 0x3) == 0 && (s & 0x3) == 0);
	ASSERT(s > 0);
	for (g = n / 2; g > 0; g /= 2) {
		for (i = g; i < n; i++) {
			for (j = i - g; j >= 0 &&
				(*f)(v + j * s, v + (j + g) * s) == 1;
					j -= g) {
				p1 = (void *)(v + j * s);
				p2 = (void *)(v + (j + g) * s);
				for (ii = 0; ii < s / 4; ii++) {
					tmp = *p1;
					*p1++ = *p2;
					*p2++ = tmp;
				}
			}
		}
	}
}

/*
 * Compare two acls, all fields.  Returns:
 * -1 (less than)
 *  0 (equal)
 * +1 (greater than)
 */
int
cmp2acls(void *a, void *b)
{
	aclent_t *x = (aclent_t *)a;
	aclent_t *y = (aclent_t *)b;

	/* Compare types */
	if (x->a_type < y->a_type)
		return (-1);
	if (x->a_type > y->a_type)
		return (1);
	/* Equal types; compare id's */
	if (x->a_id < y->a_id)
		return (-1);
	if (x->a_id > y->a_id)
		return (1);
	/* Equal ids; compare perms */
	if (x->a_perm < y->a_perm)
		return (-1);
	if (x->a_perm > y->a_perm)
		return (1);
	/* Totally equal */
	return (0);
}
