/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 The FreeBSD Foundation
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
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

#include <ctype.h>
#include <string.h>

#include "libiscsiutil.h"

#define	MAX_NAME_LEN			223

char *
checked_strdup(const char *s)
{
	char *c;

	c = strdup(s);
	if (c == NULL)
		log_err(1, "strdup");
	return (c);
}

bool
valid_iscsi_name(const char *name, void (*warn_fn)(const char *, ...))
{
	int i;

	if (strlen(name) >= MAX_NAME_LEN) {
		warn_fn("overlong name for target \"%s\"; max length allowed "
		    "by iSCSI specification is %d characters",
		    name, MAX_NAME_LEN);
		return (false);
	}

	/*
	 * In the cases below, we don't return an error, just in case the admin
	 * was right, and we're wrong.
	 */
	if (strncasecmp(name, "iqn.", strlen("iqn.")) == 0) {
		for (i = strlen("iqn."); name[i] != '\0'; i++) {
			/*
			 * XXX: We should verify UTF-8 normalisation, as defined
			 *      by 3.2.6.2: iSCSI Name Encoding.
			 */
			if (isalnum(name[i]))
				continue;
			if (name[i] == '-' || name[i] == '.' || name[i] == ':')
				continue;
			warn_fn("invalid character \"%c\" in target name "
			    "\"%s\"; allowed characters are letters, digits, "
			    "'-', '.', and ':'", name[i], name);
			break;
		}
		/*
		 * XXX: Check more stuff: valid date and a valid reversed domain.
		 */
	} else if (strncasecmp(name, "eui.", strlen("eui.")) == 0) {
		if (strlen(name) != strlen("eui.") + 16)
			warn_fn("invalid target name \"%s\"; the \"eui.\" "
			    "should be followed by exactly 16 hexadecimal "
			    "digits", name);
		for (i = strlen("eui."); name[i] != '\0'; i++) {
			if (!isxdigit(name[i])) {
				warn_fn("invalid character \"%c\" in target "
				    "name \"%s\"; allowed characters are 1-9 "
				    "and A-F", name[i], name);
				break;
			}
		}
	} else if (strncasecmp(name, "naa.", strlen("naa.")) == 0) {
		if (strlen(name) > strlen("naa.") + 32)
			warn_fn("invalid target name \"%s\"; the \"naa.\" "
			    "should be followed by at most 32 hexadecimal "
			    "digits", name);
		for (i = strlen("naa."); name[i] != '\0'; i++) {
			if (!isxdigit(name[i])) {
				warn_fn("invalid character \"%c\" in target "
				    "name \"%s\"; allowed characters are 1-9 "
				    "and A-F", name[i], name);
				break;
			}
		}
	} else {
		warn_fn("invalid target name \"%s\"; should start with "
		    "either \"iqn.\", \"eui.\", or \"naa.\"",
		    name);
	}
	return (true);
}
