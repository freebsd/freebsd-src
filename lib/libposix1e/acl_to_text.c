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
 * acl_to_text - return a text string with a text representation of the acl
 * in it.
 */

#include <sys/types.h>
#include <sys/acl.h>
#include <sys/errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utmp.h>

#include "acl_support.h"

/*
 * acl_to_text - generate a text form of an acl
 * spec says nothing about output ordering, so leave in acl order
 *
 * This function will not produce nice results if it is called with
 * a non-POSIX.1e semantics ACL.
 */
char *
acl_to_text(acl_t acl, ssize_t *len_p)
{
	char	*buf, *tmpbuf;
	char	name_buf[UT_NAMESIZE+1];
	char	perm_buf[ACL_STRING_PERM_MAXSIZE+1],
		effective_perm_buf[ACL_STRING_PERM_MAXSIZE+1];
	int	i, error, len;
	uid_t	ae_id;
	acl_tag_t	ae_tag;
	acl_perm_t	ae_perm, effective_perm, mask_perm;

	buf = strdup("");

	mask_perm = ACL_PERM_BITS;	/* effective is regular if no mask */
	for (i = 0; i < acl->acl_cnt; i++)
		if (acl->acl_entry[i].ae_tag == ACL_MASK) 
			mask_perm = acl->acl_entry[i].ae_perm;

	for (i = 0; i < acl->acl_cnt; i++) {
		ae_tag = acl->acl_entry[i].ae_tag;
		ae_id = acl->acl_entry[i].ae_id;
		ae_perm = acl->acl_entry[i].ae_perm;

		switch(ae_tag) {
		case ACL_USER_OBJ:
			error = acl_perm_to_string(ae_perm,
			    ACL_STRING_PERM_MAXSIZE+1, perm_buf);
			if (error)
				goto error_label;
			len = asprintf(&tmpbuf, "%suser::%s\n", buf,
			    perm_buf);
			if (len == -1) {
				errno = ENOMEM;
				goto error_label;
			}
			free(buf);
			buf = tmpbuf;
			break;

		case ACL_USER:
			error = acl_perm_to_string(ae_perm,
			    ACL_STRING_PERM_MAXSIZE+1, perm_buf);
			if (error)
				goto error_label;

			error = acl_id_to_name(ae_tag, ae_id, UT_NAMESIZE+1,
			    name_buf);
			if (error)
				goto error_label;

			effective_perm = ae_perm & mask_perm;
			if (effective_perm != ae_perm) {
				error = acl_perm_to_string(effective_perm,
				    ACL_STRING_PERM_MAXSIZE+1,
				    effective_perm_buf);
				if (error)
					goto error_label;
				len = asprintf(&tmpbuf, "%suser:%s:%s\t\t# "
				    "effective: %s\n",
				    buf, name_buf, perm_buf,
				    effective_perm_buf);
			} else {
				len = asprintf(&tmpbuf, "%suser:%s:%s\n", buf,
				    name_buf, perm_buf);
			}
			if (len == -1) {
				errno = ENOMEM;
				goto error_label;
			}
			free(buf);
			buf = tmpbuf;
			break;

		case ACL_GROUP_OBJ:
			error = acl_perm_to_string(ae_perm,
			    ACL_STRING_PERM_MAXSIZE+1, perm_buf);
			if (error)
				goto error_label;

			effective_perm = ae_perm & mask_perm;
			if (effective_perm != ae_perm) {
				error = acl_perm_to_string(effective_perm,
				    ACL_STRING_PERM_MAXSIZE+1,
				    effective_perm_buf);
				if (error)
					goto error_label;
				len = asprintf(&tmpbuf, "%sgroup::%s\t\t# "
				    "effective: %s\n",
				    buf, perm_buf, effective_perm_buf);
			} else {
				len = asprintf(&tmpbuf, "%sgroup::%s\n", buf,
				    perm_buf);
			}
			if (len == -1) {
				errno = ENOMEM;
				goto error_label;
			}
			free(buf);
			buf = tmpbuf;
			break;

		case ACL_GROUP:
			error = acl_perm_to_string(ae_perm,
			    ACL_STRING_PERM_MAXSIZE+1, perm_buf);
			if (error)
				goto error_label;

			error = acl_id_to_name(ae_tag, ae_id, UT_NAMESIZE+1,
			    name_buf);
			if (error)
				goto error_label;

			effective_perm = ae_perm & mask_perm;
			if (effective_perm != ae_perm) {
				error = acl_perm_to_string(effective_perm,
				    ACL_STRING_PERM_MAXSIZE+1,
				    effective_perm_buf);
				if (error)
					goto error_label;
				len = asprintf(&tmpbuf, "%sgroup::%s\t\t# "
				    "effective: %s\n",
				    buf, perm_buf, effective_perm_buf);
			} else {
				len = asprintf(&tmpbuf, "%sgroup:%s:%s\n", buf,
				    name_buf, perm_buf);
			}
			if (len == -1) {
				errno = ENOMEM;
				goto error_label;
			}
			free(buf);
			buf = tmpbuf;
			break;

		case ACL_MASK:
			error = acl_perm_to_string(ae_perm,
			    ACL_STRING_PERM_MAXSIZE+1, perm_buf);
			if (error)
				goto error_label;

			len = asprintf(&tmpbuf, "%smask::%s\n", buf,
			    perm_buf);
			if (len == -1) {
				errno = ENOMEM;
				goto error_label;
			}
			free(buf);
			buf = tmpbuf;
			break;

		case ACL_OTHER:
			error = acl_perm_to_string(ae_perm,
			    ACL_STRING_PERM_MAXSIZE+1, perm_buf);
			if (error)
				goto error_label;

			len = asprintf(&tmpbuf, "%sother::%s\n", buf,
			    perm_buf);
			if (len == -1) {
				errno = ENOMEM;
				goto error_label;
			}
			free(buf);
			buf = tmpbuf;
			break;

		default:
			free(buf);
			errno = EINVAL;
			return (0);
		}
	}

	if (len_p) {
		*len_p = strlen(buf);
	}
	return (buf);

error_label:
	/* jump to here sets errno already, we just clean up */
	if (buf) free(buf);
	return (0);
}
