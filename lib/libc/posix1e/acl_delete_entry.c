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

/* acl_delete_entry() - delete an ACL entry from an ACL */

#include <sys/types.h>
#include "namespace.h"
#include <sys/acl.h>
#include "un-namespace.h"
#include <errno.h>
#include <string.h>

int
acl_delete_entry(acl_t acl, acl_entry_t entry_d)
{
	int i;

	if (!acl || !entry_d || (acl->acl_cnt < 1) ||
	    (acl->acl_cnt > ACL_MAX_ENTRIES)) {
		errno = EINVAL;
		return -1;
	}
	for (i = 0; i < acl->acl_cnt; i++) {
		/* if this is our entry... */
		if ((acl->acl_entry[i].ae_tag == entry_d->ae_tag) &&
		    (acl->acl_entry[i].ae_id == entry_d->ae_id)) {
			/* ...shift the remaining entries... */
			while (i < acl->acl_cnt - 1)
				acl->acl_entry[i] = acl->acl_entry[++i];
			/* ...drop the count and zero the unused entry... */
			acl->acl_cnt--;
			bzero(&acl->acl_entry[i], sizeof(struct acl_entry));
			return 0;
		}
	}


	errno = EINVAL;
	return -1;
}
