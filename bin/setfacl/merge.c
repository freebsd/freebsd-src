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
#include <sysexits.h>

#include "setfacl.h"

/* merge acl into existing file's ACL */
int
merge_acl(acl_t acl, acl_t *prev_acl)
{
	acl_t acl_new;
	int blank_acl_user, blank_acl_group, have_entry, i, j;
	struct stat sb;

	blank_acl_user = blank_acl_group = 0;

	if (acl_type == ACL_TYPE_ACCESS)
		acl_new = acl_dup(prev_acl[0]);
	else
		acl_new = acl_dup(prev_acl[1]);
	if (!acl_new)
		err(EX_OSERR, "acl_dup() failed");

	/* step through new ACL entries */
	for (i = 0; i < acl->acl_cnt; i++) {
		have_entry = 0;

		/* oh look, we have an ACL_MASK entry */
		if (acl->acl_entry[i].ae_tag == ACL_MASK)
			have_mask = 1;

		/* check against the existing ACL entries */
		for (j = 0; j < acl_new->acl_cnt && !have_entry; j++) {
			if (acl_new->acl_entry[j].ae_tag ==
			    acl->acl_entry[i].ae_tag) {
				switch(acl->acl_entry[i].ae_tag) {
				case ACL_USER_OBJ:
					acl_new->acl_entry[j].ae_perm =
					    acl->acl_entry[i].ae_perm;
					acl_new->acl_entry[j].ae_id = sb.st_uid;
					have_entry = 1;
					break;
				case ACL_GROUP_OBJ:
					acl_new->acl_entry[j].ae_perm =
					    acl->acl_entry[i].ae_perm;
					acl_new->acl_entry[j].ae_id = sb.st_gid;
					have_entry = 1;
					break;
				default:
					if (acl_new->acl_entry[j].ae_id == 
					    acl->acl_entry[i].ae_id) {
						/* any other matches */
						acl_new->acl_entry[j].ae_perm =
						    acl->acl_entry[i].ae_perm;
						have_entry = 1;
					}
					break;
				}
			}
		}

		/* if this entry has not been found, it must be new */
		if (!have_entry) {
			if (acl_new->acl_cnt == ACL_MAX_ENTRIES) {
				warn("too many ACL entries");
				acl_free(acl_new);
				return -1;
			}
			acl_new->acl_entry[acl_new->acl_cnt++] =
			    acl->acl_entry[i];
		}
	}

	if (acl_type == ACL_TYPE_ACCESS)
		*prev_acl[0] = *acl_new;
	else
		*prev_acl[1] = *acl_new;
	acl_free(acl_new);

	return 0;
}
