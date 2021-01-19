/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Gleb Popov
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * acl_equiv_mode_np: Check if an ACL can be represented as a mode_t.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/acl.h>

#include "acl_support.h"

int
acl_equiv_mode_np(acl_t acl, mode_t *mode_p)
{
	mode_t ret_mode = 0;

	if (acl == NULL) {
		errno = EINVAL;
		return (-1);
	}

	/* Linux returns 0 for ACL returned by acl_init() */
	if (_acl_brand(acl) == ACL_BRAND_UNKNOWN && acl->ats_acl.acl_cnt == 0)
		return (0);

	// TODO: Do we want to handle ACL_BRAND_NFS4 in this function? */
	if (_acl_brand(acl) != ACL_BRAND_POSIX)
		return (1);

	for (int cur_entry = 0; cur_entry < acl->ats_acl.acl_cnt; cur_entry++) {
		acl_entry_t entry = &acl->ats_acl.acl_entry[cur_entry];

		if ((entry->ae_perm & ACL_PERM_BITS) != entry->ae_perm)
			return (1);

		switch (entry->ae_tag) {
		case ACL_USER_OBJ:
			if (entry->ae_perm & ACL_READ)
				ret_mode |= S_IRUSR;
			if (entry->ae_perm & ACL_WRITE)
				ret_mode |= S_IWUSR;
			if (entry->ae_perm & ACL_EXECUTE)
				ret_mode |= S_IXUSR;
			break;
		case ACL_GROUP_OBJ:
			if (entry->ae_perm & ACL_READ)
				ret_mode |= S_IRGRP;
			if (entry->ae_perm & ACL_WRITE)
				ret_mode |= S_IWGRP;
			if (entry->ae_perm & ACL_EXECUTE)
				ret_mode |= S_IXGRP;
			break;
		case ACL_OTHER:
			if (entry->ae_perm & ACL_READ)
				ret_mode |= S_IROTH;
			if (entry->ae_perm & ACL_WRITE)
				ret_mode |= S_IWOTH;
			if (entry->ae_perm & ACL_EXECUTE)
				ret_mode |= S_IXOTH;
			break;
		default:
			return (1);
		}
	}

	if (mode_p != NULL)
		*mode_p = ret_mode;

	return (0);
}
