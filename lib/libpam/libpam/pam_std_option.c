/*-
 * Copyright 1998 Juniper Networks, Inc.
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

#include <security/pam_modules.h>
#include <string.h>
#include "pam_mod_misc.h"

/*
 * If the given name is a standard option, set the corresponding flag in
 * the options word and return 0.  Else return -1.
 */
int
pam_std_option(int *options, const char *name)
{
	struct opttab {
		const char *name;
		int value;
	};
	static struct opttab std_options[] = {
		{ "debug",		PAM_OPT_DEBUG },
		{ "no_warn",		PAM_OPT_NO_WARN },
		{ "use_first_pass",	PAM_OPT_USE_FIRST_PASS },
		{ "try_first_pass",	PAM_OPT_TRY_FIRST_PASS },
		{ "use_mapped_pass",	PAM_OPT_USE_MAPPED_PASS },
		{ "echo_pass",		PAM_OPT_ECHO_PASS },
		{ NULL,			0 }
	};
	struct opttab *p;

	for (p = std_options;  p->name != NULL;  p++) {
		if (strcmp(name, p->name) == 0) {
			*options |= p->value;
			return 0;
		}
	}
	return -1;
}
