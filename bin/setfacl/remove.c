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
#include <sys/acl.h>
#include <sys/stat.h>

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>

#include "setfacl.h"

/* remove ACL entries from an ACL */
int
remove_acl(acl_t acl, acl_t *prev_acl)
{
	acl_t acl_new;
	int carried_error, i;

	carried_error = 0;

	if (acl_type == ACL_TYPE_ACCESS)
		acl_new = acl_dup(prev_acl[0]);
	else
		acl_new = acl_dup(prev_acl[1]);
	if (!acl_new)
		err(EX_OSERR, "acl_dup() failed");

	/* find and delete the entry */
	for (i = 0; i < acl->acl_cnt; i++) {
		if (acl->acl_entry[i].ae_tag == ACL_MASK)
			have_mask++;
		if (acl_delete_entry(acl_new, &acl->acl_entry[i]) == -1) {
			carried_error++;
			warnx("cannot remove non-existent acl entry");
		}
	}

	if (acl_type == ACL_TYPE_ACCESS) {
		acl_free(prev_acl[0]);
		prev_acl[0] = acl_new;
	} else {
		acl_free(prev_acl[1]);
		prev_acl[1] = acl_new;
	}

	if (carried_error)
		return -1;

	return 0;
}

/* remove default entries */
int
remove_default(acl_t *prev_acl)
{

	if (prev_acl[1]) {
		bzero(prev_acl[1], sizeof(struct acl));
		prev_acl[1]->acl_cnt = 0;
	} else {
		warn("cannot remove default ACL");
		return -1;
	}
	return 0;
}

/* remove extended entries */
void
remove_ext(acl_t *prev_acl)
{
	acl_t acl_new, acl_old;
	acl_perm_t group_perm, mask_perm;
	int have_mask_entry, i;

	if (acl_type == ACL_TYPE_ACCESS)
		acl_old = acl_dup(prev_acl[0]);
	else
		acl_old = acl_dup(prev_acl[1]);
	if (!acl_old)
		err(EX_OSERR, "acl_dup() failed");

	group_perm = mask_perm = 0;
	have_mask_entry = 0;
	acl_new = acl_init(ACL_MAX_ENTRIES);
	if (!acl_new)
		err(EX_OSERR, "%s", "acl_init() failed");

	/* only save the default user/group/other entries */
	for (i = 0; i < acl_old->acl_cnt; i++)
		switch(acl_old->acl_entry[i].ae_tag) {
		case ACL_USER_OBJ:
			acl_new->acl_entry[0] = acl_old->acl_entry[i];
			break;
		case ACL_GROUP_OBJ:
			acl_new->acl_entry[1] = acl_old->acl_entry[i];
			group_perm = acl_old->acl_entry[i].ae_perm;
			break;
		case ACL_OTHER_OBJ:
			acl_new->acl_entry[2] = acl_old->acl_entry[i];
			break;
		case ACL_MASK:
			mask_perm = acl_old->acl_entry[i].ae_perm;
			have_mask_entry = 1;
			break;
		default:
			break;
		}
	/*
	 * If the ACL contains a mask entry, then the permissions associated
	 * with the owning group entry in the resulting ACL shall be set to
	 * only those permissions associated with both the owning group entry
	 * and the mask entry of the current ACL.
	 */
	if (have_mask_entry)
		acl_new->acl_entry[1].ae_perm = group_perm & mask_perm;
	acl_new->acl_cnt = 3;

	if (acl_type == ACL_TYPE_ACCESS) {
		acl_free(prev_acl[0]);
		prev_acl[0] = acl_new;
	} else {
		acl_free(prev_acl[1]);
		prev_acl[1] = acl_new;
	}

	have_mask = 0;
}
