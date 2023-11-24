/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
 * acl_extended_file_np: Check if the file has extended ACLs set.
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/acl.h>

#include <unistd.h>

typedef acl_t (*acl_get_func)(const char *, acl_type_t);
typedef long (*pathconf_func)(const char *, int);

static int
_acl_extended_file(acl_get_func f, pathconf_func pathconf_f, const char* path_p);

int
acl_extended_file_np(const char *path_p)
{
	return (_acl_extended_file(acl_get_file, pathconf, path_p));
}

int
acl_extended_file_nofollow_np(const char *path_p)
{
	return (_acl_extended_file(acl_get_link_np, lpathconf, path_p));
}

int
acl_extended_link_np(const char *path_p)
{
	return (_acl_extended_file(acl_get_link_np, lpathconf, path_p));
}

int
_acl_extended_file(acl_get_func acl_get, pathconf_func pathconf_f, const char* path_p)
{
	acl_t acl;
	int retval, istrivial, acltype = ACL_TYPE_ACCESS;

	retval = pathconf_f(path_p, _PC_ACL_NFS4);
	if (retval > 0)
		acltype = ACL_TYPE_NFS4;

	acl = acl_get(path_p, acltype);
	if (acl == NULL)
		return (-1);

	retval = acl_is_trivial_np(acl, &istrivial);
	acl_free(acl);
	if (retval == -1)
		return (-1);

	return (!istrivial);
}
