/*-
 * Copyright (c) 1999, 2000, 2001 Robert N. M. Watson
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
 *
 * $FreeBSD$
 */
/*
 * acl_init -- return a fresh acl structure
 * acl_dup -- duplicate an acl and return the new copy
 */

#include <sys/types.h>
#include "namespace.h"
#include <sys/acl.h>
#include "un-namespace.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

acl_t
acl_init(int count)
{
	acl_t acl;

	if (count > ACL_MAX_ENTRIES) {
		errno = ENOMEM;
		return (NULL);
	}
	if (count < 0) {
		errno = EINVAL;
		return (NULL);
	}

	acl = malloc(sizeof(struct acl_t_struct));
	if (acl != NULL)
		bzero(acl, sizeof(struct acl_t_struct));

	return (acl);
}

acl_t
acl_dup(acl_t acl)
{
	acl_t	acl_new;

	acl_new = acl_init(ACL_MAX_ENTRIES);
	if (!acl_new)
		return NULL;
	*acl_new = *acl;
	acl->ats_cur_entry = 0;
	acl_new->ats_cur_entry = 0;

	return(acl_new);
}
