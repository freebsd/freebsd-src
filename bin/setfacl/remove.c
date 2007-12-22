/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/acl.h>
#include <sys/stat.h>

#include <err.h>
#include <stdio.h>
#include <string.h>

#include "setfacl.h"

/*
 * remove ACL entries from an ACL
 */
int
remove_acl(acl_t acl, acl_t *prev_acl)
{
	acl_entry_t	entry;
	acl_t		acl_new;
	acl_tag_t	tag;
	int		carried_error, entry_id;

	carried_error = 0;

	if (acl_type == ACL_TYPE_ACCESS)
		acl_new = acl_dup(prev_acl[ACCESS_ACL]);
	else
		acl_new = acl_dup(prev_acl[DEFAULT_ACL]);
	if (acl_new == NULL)
		err(1, "acl_dup() failed");

	tag = ACL_UNDEFINED_TAG;

	/* find and delete the entry */
	entry_id = ACL_FIRST_ENTRY;
	while (acl_get_entry(acl, entry_id, &entry) == 1) {
		entry_id = ACL_NEXT_ENTRY;
		if (acl_get_tag_type(entry, &tag) == -1)
			err(1, "acl_get_tag_type() failed");
		if (tag == ACL_MASK)
			have_mask++;
		if (acl_delete_entry(acl_new, entry) == -1) {
			carried_error++;
			warnx("cannot remove non-existent acl entry");
		}
	}

	if (acl_type == ACL_TYPE_ACCESS) {
		acl_free(prev_acl[ACCESS_ACL]);
		prev_acl[ACCESS_ACL] = acl_new;
	} else {
		acl_free(prev_acl[DEFAULT_ACL]);
		prev_acl[DEFAULT_ACL] = acl_new;
	}

	if (carried_error)
		return (-1);

	return (0);
}

/*
 * remove default entries
 */
int
remove_default(acl_t *prev_acl)
{

	if (prev_acl[1]) {
		acl_free(prev_acl[1]);
		prev_acl[1] = acl_init(ACL_MAX_ENTRIES);
		if (prev_acl[1] == NULL)
			err(1, "acl_init() failed");
	} else {
		warn("cannot remove default ACL");
		return (-1);
	}
	return (0);
}

/*
 * remove extended entries
 */
void
remove_ext(acl_t *prev_acl)
{
	acl_t acl_new, acl_old;
	acl_entry_t entry, entry_new;
	acl_permset_t perm;
	acl_tag_t tag;
	int entry_id, have_mask_entry;

	if (acl_type == ACL_TYPE_ACCESS)
		acl_old = acl_dup(prev_acl[ACCESS_ACL]);
	else
		acl_old = acl_dup(prev_acl[DEFAULT_ACL]);
	if (acl_old == NULL)
		err(1, "acl_dup() failed");

	have_mask_entry = 0;
	acl_new = acl_init(ACL_MAX_ENTRIES);
	if (acl_new == NULL)
		err(1, "acl_init() failed");
	tag = ACL_UNDEFINED_TAG;

	/* only save the default user/group/other entries */
	entry_id = ACL_FIRST_ENTRY;
	while (acl_get_entry(acl_old, entry_id, &entry) == 1) {
		entry_id = ACL_NEXT_ENTRY;

		if (acl_get_tag_type(entry, &tag) == -1)
			err(1, "acl_get_tag_type() failed");

		switch(tag) {
		case ACL_USER_OBJ:
		case ACL_GROUP_OBJ:
		case ACL_OTHER:
			if (acl_get_tag_type(entry, &tag) == -1)
				err(1, "acl_get_tag_type() failed");
			if (acl_get_permset(entry, &perm) == -1)
				err(1, "acl_get_permset() failed");
			if (acl_create_entry(&acl_new, &entry_new) == -1)
				err(1, "acl_create_entry() failed");
			if (acl_set_tag_type(entry_new, tag) == -1)
				err(1, "acl_set_tag_type() failed");
			if (acl_set_permset(entry_new, perm) == -1)
				err(1, "acl_get_permset() failed");
			if (acl_copy_entry(entry_new, entry) == -1)
				err(1, "acl_copy_entry() failed");
			break;
		case ACL_MASK:
			have_mask_entry = 1;
			break;
		default:
			break;
		}
	}
	if (have_mask_entry && n_flag == 0) {
		if (acl_calc_mask(&acl_new) == -1)
			err(1, "acl_calc_mask() failed");
	} else {
		have_mask = 1;
	}

	if (acl_type == ACL_TYPE_ACCESS) {
		acl_free(prev_acl[ACCESS_ACL]);
		prev_acl[ACCESS_ACL] = acl_new;
	} else {
		acl_free(prev_acl[DEFAULT_ACL]);
		prev_acl[DEFAULT_ACL] = acl_new;
	}
}
