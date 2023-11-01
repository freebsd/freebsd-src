/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Robert N M Watson, Gleb Popov
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
 * acl_from_mode_np: Create an ACL from a mode_t.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/acl.h>
#include <sys/stat.h>

/*
 * return an ACL corresponding to the permissions
 * contained in mode_t
 */
acl_t
acl_from_mode_np(const mode_t mode)
{
	acl_t acl;
	acl_entry_t entry;
	acl_permset_t perms;

	/* create the ACL */
	acl = acl_init(3);
	/* here and below, the only possible reason to fail is ENOMEM, so
	 * no need to set errno again
	 */
	if (acl == NULL)
		return (NULL);

	/* First entry: ACL_USER_OBJ */
	if (acl_create_entry(&acl, &entry) == -1)
		return (NULL);
	/* TODO: need to handle error there and below? */
	acl_set_tag_type(entry, ACL_USER_OBJ);

	acl_get_permset(entry, &perms);
	acl_clear_perms(perms);

	/* calculate user mode */
	if (mode & S_IRUSR)
		acl_add_perm(perms, ACL_READ);
	if (mode & S_IWUSR)
		acl_add_perm(perms, ACL_WRITE);
	if (mode & S_IXUSR)
		acl_add_perm(perms, ACL_EXECUTE);

	acl_set_permset(entry, perms);

	/* Second entry: ACL_GROUP_OBJ */
	if (acl_create_entry(&acl, &entry) == -1)
		return (NULL);
	acl_set_tag_type(entry, ACL_GROUP_OBJ);

	acl_get_permset(entry, &perms);
	acl_clear_perms(perms);

	/* calculate group mode */
	if (mode & S_IRGRP)
		acl_add_perm(perms, ACL_READ);
	if (mode & S_IWGRP)
		acl_add_perm(perms, ACL_WRITE);
	if (mode & S_IXGRP)
		acl_add_perm(perms, ACL_EXECUTE);

	acl_set_permset(entry, perms);

	/* Third entry: ACL_OTHER */
	if (acl_create_entry(&acl, &entry) == -1)
		return (NULL);
	acl_set_tag_type(entry, ACL_OTHER);

	acl_get_permset(entry, &perms);
	acl_clear_perms(perms);

	/* calculate other mode */
	if (mode & S_IROTH)
		acl_add_perm(perms, ACL_READ);
	if (mode & S_IWOTH)
		acl_add_perm(perms, ACL_WRITE);
	if (mode & S_IXOTH)
		acl_add_perm(perms, ACL_EXECUTE);

	acl_set_permset(entry, perms);

	return (acl);
}
