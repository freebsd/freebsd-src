/*
 * Copyright (c) 2001 Chris D. Faulhaber
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR THE VOICES IN HIS HEAD BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include "namespace.h"
#include <sys/acl.h>
#include "un-namespace.h"

#include <errno.h>

/*
 * acl_calc_mask() calculates and set the permissions associated
 * with the ACL_MASK ACL entry.  If the ACL already contains an
 * ACL_MASK entry, its permissions shall be overwritten; if not,
 * one shall be added.
 */
int
acl_calc_mask(acl_t *acl_p)
{
	acl_t acl_new;
	int group_obj, i, mask_mode, mask_num, other_obj, user_obj;

	/* check args */
	if (!acl_p || !*acl_p || ((*acl_p)->acl_cnt < 3) ||
	    ((*acl_p)->acl_cnt > ACL_MAX_ENTRIES)) {
		errno = EINVAL;
		return -1;
	}

	acl_new = acl_dup(*acl_p);
	if (!acl_new)
		return -1;

	user_obj = group_obj = other_obj = mask_mode = 0;
	mask_num = -1;

	/* gather permissions and find a mask entry */
	for (i = 0; i < acl_new->acl_cnt; i++) {
		switch(acl_new->acl_entry[i].ae_tag) {
		case ACL_USER_OBJ:
			user_obj++;
			break;
		case ACL_OTHER:
			other_obj++;
			break;
		case ACL_GROUP_OBJ:
			group_obj++;
			/* FALLTHROUGH */
		case ACL_GROUP:
		case ACL_USER:
			mask_mode |=
			    acl_new->acl_entry[i].ae_perm & ACL_PERM_BITS;
			break;
		case ACL_MASK:
			mask_num = i;
			break;
		default:
			errno = EINVAL;
			acl_free(acl_new);
			return -1;
			/* NOTREACHED */
		}
	}
	if ((user_obj != 1) || (group_obj != 1) || (other_obj != 1)) {
		errno = EINVAL;
		acl_free(acl_new);
		return -1;
	}
	/* if a mask entry already exists, overwrite the perms */
	if (mask_num != -1) {
		acl_new->acl_entry[mask_num].ae_perm = mask_mode;
	} else {
		/* if no mask exists, check acl_cnt... */
		if (acl_new->acl_cnt == ACL_MAX_ENTRIES) {
			errno = EINVAL;
			acl_free(acl_new);
			return -1;
		}
		/* ...and add the mask entry */
		acl_new->acl_entry[acl_new->acl_cnt].ae_tag  = ACL_MASK;
		acl_new->acl_entry[acl_new->acl_cnt].ae_id   = 0;
		acl_new->acl_entry[acl_new->acl_cnt].ae_perm = mask_mode;
		acl_new->acl_cnt++;
	}

	if (acl_valid(acl_new) == -1) {
		errno = EINVAL;
		acl_free(acl_new);
		return -1;
	}

	**acl_p = *acl_new;
	acl_free(acl_new);

	return 0;
}
