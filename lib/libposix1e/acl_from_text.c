/*-
 * Copyright (c) 1999, 2000 Robert N. M. Watson
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
 * acl_from_text: Convert a text-form ACL from a string to an acl_t.
 */

#include <sys/types.h>
#include <sys/acl.h>
#include <sys/errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acl_support.h"

static char *
string_skip_whitespace(char *string)
{

	while (*string && ((*string == ' ') || (*string == '\t'))) {
		string++;
	}
	return (string);
}

static void
string_trim_trailing_whitespace(char *string)
{
	char	*end;

	if (*string == '\0')
		return;

	end = string + strlen(string) - 1;

	while (end != string) {
		if ((*end == ' ') || (*end == '\t')) {
			*end = '\0';
			end--;
		} else {
			return;
		}
	}

	return;
}

acl_tag_t
acl_string_to_tag(char *tag, char *qualifier)
{

	if (*qualifier == '\0') {
		if ((!strcmp(tag, "user")) || (!strcmp(tag, "u"))) {
			return (ACL_USER_OBJ);
		} else
		if ((!strcmp(tag, "group")) || (!strcmp(tag, "g"))) {
			return (ACL_GROUP_OBJ);
		} else
		if ((!strcmp(tag, "mask")) || (!strcmp(tag, "m"))) {
			return (ACL_MASK);
		} else
		if ((!strcmp(tag, "other")) || (!strcmp(tag, "o"))) {
			return (ACL_OTHER);
		} else
			return(-1);
	} else {
		if ((!strcmp(tag, "user")) || (!strcmp(tag, "u"))) {
			return(ACL_USER);
		} else
		if ((!strcmp(tag, "group")) || (!strcmp(tag, "g"))) {
			return(ACL_GROUP);
		} else
			return(-1);
	}
}

/*
 * acl_from_text -- Convert a string into an ACL.
 * Postpone most validity checking until the end and call acl_valid() to do
 * that.
 */
acl_t
acl_from_text(const char *buf_p)
{
	acl_tag_t	t;
	acl_perm_t	p;
	acl_t	acl;
	uid_t	id;
	char	*mybuf_p, *line, *cur, *notcomment, *comment, *entry;
	char	*tag, *qualifier, *permission;
	int	error;

	/* Local copy we can mess up. */
	mybuf_p = strdup(buf_p);
	if (!mybuf_p) {
		errno = ENOMEM;
		return(0);
	}

	acl = acl_init(3);
	if (!acl) {
		free(mybuf_p);
		errno = ENOMEM;
		return(0);
	}

	/* Outer loop: delimit at \n boundaries. */
	cur = mybuf_p;
	while ((line = strsep(&cur, "\n"))) {
		/* Now split the line on the first # to strip out comments. */
		comment = line;
		notcomment = strsep(&comment, "#");

		/* Inner loop: delimit at ',' boundaries. */
		while ((entry = strsep(&notcomment, ","))) {
			/* Now split into three ':' delimited fields. */
			tag = strsep(&entry, ":");
			if (!tag) {
				errno = EINVAL;
				goto error_label;
			}
			tag = string_skip_whitespace(tag);
			if ((*tag == '\0') && (!entry)) {
				/*
				 * Is an entirely comment line, skip to next
				 * comma.
				 */
				continue;
			}
			string_trim_trailing_whitespace(tag);

			qualifier = strsep(&entry, ":");
			if (!qualifier) {
				errno = EINVAL;
				goto error_label;
			}
			qualifier = string_skip_whitespace(qualifier);
			string_trim_trailing_whitespace(qualifier);

			permission = strsep(&entry, ":");
			if ((!permission) || (entry)) {
				errno = EINVAL;
				goto error_label;
			}
			permission = string_skip_whitespace(permission);
			string_trim_trailing_whitespace(permission);

			t = acl_string_to_tag(tag, qualifier);
			if (t == -1) {
				errno = EINVAL;
				goto error_label;
			}

			error = acl_string_to_perm(permission, &p);
			if (error == -1) {
				errno = EINVAL;
				goto error_label;
			}		

			switch(t) {
			case ACL_USER_OBJ:
			case ACL_GROUP_OBJ:
			case ACL_MASK:
			case ACL_OTHER:
				if (*qualifier != '\0') {
					errno = EINVAL;
					goto error_label;
				}
				id = 0;
				break;

			case ACL_USER:
			case ACL_GROUP:
				error = acl_name_to_id(t, qualifier, &id);
				if (error == -1)
					goto error_label;
				break;

			default:
				errno = EINVAL;
				goto error_label;
			}

			error = acl_add_entry(acl, t, id, p);
			if (error == -1)
				goto error_label;
		}
	}

#if 0
	/* XXX Should we only return ACLs valid according to acl_valid? */
	/* Verify validity of the ACL we read in. */
	if (acl_valid(acl) == -1) {
		errno = EINVAL;
		goto error_label;
	}
#endif

	return(acl);

error_label:
	acl_free(acl);
	free(mybuf_p);
	return(0);
}



