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

#include <sys/types.h>
#include "namespace.h"
#include <sys/acl.h>
#include "un-namespace.h"

#include <errno.h>
#include <stdlib.h>

#define ACL_UNDEFINED_ID	-1
#define ACL_UNDEFINED_TAG	-1

int
acl_create_entry(acl_t *acl_p, acl_entry_t *entry_p)
{
	acl_t acl;

	if (!acl_p || !*acl_p || ((*acl_p)->acl_cnt >= ACL_MAX_ENTRIES) ||
	    ((*acl_p)->acl_cnt < 0)) {
		errno = EINVAL;
		return -1;
	}

	acl = *acl_p;

	*entry_p = &acl->acl_entry[acl->acl_cnt++];

	(**entry_p).ae_tag  = ACL_UNDEFINED_TAG;
	(**entry_p).ae_id   = ACL_UNDEFINED_ID;
	(**entry_p).ae_perm = ACL_PERM_NONE;

	return 0;
}

int
acl_get_entry(acl_t acl, int entry_id, acl_entry_t *entry_p)
{

	errno = ENOSYS;
	return -1;
}
