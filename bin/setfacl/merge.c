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

#include "setfacl.h"

static int merge_user_group(acl_entry_t *entry, acl_entry_t *entry_new);

static int
merge_user_group(acl_entry_t *entry, acl_entry_t *entry_new)
{
	acl_permset_t permset;
	int have_entry;
	uid_t *id, *id_new;

	have_entry = 0;

	id = acl_get_qualifier(*entry);
	if (id == NULL)
		err(1, "acl_get_qualifier() failed");
	id_new = acl_get_qualifier(*entry_new);
	if (id_new == NULL)
		err(1, "acl_get_qualifier() failed");
	if (*id == *id_new) {
		/* any other matches */
		if (acl_get_permset(*entry, &permset) == -1)
			err(1, "acl_get_permset() failed");
		if (acl_set_permset(*entry_new, permset) == -1)
			err(1, "acl_set_permset() failed");
		have_entry = 1;
	}
	acl_free(id);
	acl_free(id_new);

	return (have_entry);
}

/*
 * merge an ACL into existing file's ACL
 */
int
merge_acl(acl_t acl, acl_t *prev_acl)
{
	acl_entry_t entry, entry_new;
	acl_permset_t permset;
	acl_t acl_new;
	acl_tag_t tag, tag_new;
	int entry_id, entry_id_new, have_entry;

	if (acl_type == ACL_TYPE_ACCESS)
		acl_new = acl_dup(prev_acl[ACCESS_ACL]);
	else
		acl_new = acl_dup(prev_acl[DEFAULT_ACL]);
	if (acl_new == NULL)
		err(1, "acl_dup() failed");

	entry_id = ACL_FIRST_ENTRY;

	while (acl_get_entry(acl, entry_id, &entry) == 1) {
		entry_id = ACL_NEXT_ENTRY;
		have_entry = 0;

		/* keep track of existing ACL_MASK entries */
		if (acl_get_tag_type(entry, &tag) == -1)
			err(1, "acl_get_tag_type() failed - invalid ACL entry");
		if (tag == ACL_MASK)
			have_mask = 1;

		/* check against the existing ACL entries */
		entry_id_new = ACL_FIRST_ENTRY;
		while (have_entry == 0 &&
		    acl_get_entry(acl_new, entry_id_new, &entry_new) == 1) {
			entry_id_new = ACL_NEXT_ENTRY;

			if (acl_get_tag_type(entry, &tag) == -1)
				err(1, "acl_get_tag_type() failed");
			if (acl_get_tag_type(entry_new, &tag_new) == -1)
				err(1, "acl_get_tag_type() failed");
			if (tag != tag_new)
				continue;

			switch(tag) {
			case ACL_USER:
			case ACL_GROUP:
				have_entry = merge_user_group(&entry,
				    &entry_new);
				if (have_entry == 0)
					break;
				/* FALLTHROUGH */
			case ACL_USER_OBJ:
			case ACL_GROUP_OBJ:
			case ACL_OTHER:
			case ACL_MASK:
				if (acl_get_permset(entry, &permset) == -1)
					err(1, "acl_get_permset() failed");
				if (acl_set_permset(entry_new, permset) == -1)
					err(1, "acl_set_permset() failed");
				have_entry = 1;
				break;
			default:
				/* should never be here */
				errx(1, "Invalid tag type: %i", tag);
				break;
			}
		}

		/* if this entry has not been found, it must be new */
		if (have_entry == 0) {
			if (acl_create_entry(&acl_new, &entry_new) == -1) {
				acl_free(acl_new);
				return (-1);
			}
			if (acl_copy_entry(entry_new, entry) == -1)
				err(1, "acl_copy_entry() failed");
		}
	}

	if (acl_type == ACL_TYPE_ACCESS) {
		acl_free(prev_acl[ACCESS_ACL]);
		prev_acl[ACCESS_ACL] = acl_new;
	} else {
		acl_free(prev_acl[DEFAULT_ACL]);
		prev_acl[DEFAULT_ACL] = acl_new;
	}

	return (0);
}
