/*-
 * Copyright (c) 1999 Robert N. M. Watson
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
 *	$FreeBSD$
 */
/*
 * acl_calc_mask(): POSIX.1e routine to recalculate the mask value
 */

#include <sys/types.h>
#include <sys/acl.h>
#include <sys/errno.h>
#include <string.h>

#include "acl_support.h"

/*
 * POSIX.1e ACL semantics:
 *
 * acl_calc_mask(): calculate an ACL_MASK entry for the ACL, then either
 * insert into the ACL if there is none already, or replace the existing
 * one.  This will act up if called on a non-POSIX.1e semantics ACL.
 */
int
acl_calc_mask(acl_t *acl_p)
{
	acl_perm_t	perm_union = ACL_PERM_NONE;
	acl_t	acl = *acl_p;
	int	mask_entry = -1;
	int	i;

	/* search for ACL_MASK */
	for (i = 0; i < acl->acl_cnt; i++)
		if (acl->acl_entry[i].ae_tag == ACL_MASK)
			mask_entry = i;
		else
			perm_union |= acl->acl_entry[i].ae_perm;

	if (mask_entry != -1) {
		/* already have a mask, replace */
		acl->acl_entry[mask_entry].ae_perm = perm_union;
	} else {
		/* must add a new mask */
		if (acl_add_entry(acl, ACL_MASK, 0, perm_union) == -1)
			return (-1);
	}

	return (0);
}
