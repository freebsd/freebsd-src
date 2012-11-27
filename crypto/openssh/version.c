/*-
 * Copyright (c) 2001 Brian Fundakowski Feldman
 * Copyright (c) 2012 Eygene Ryabinkin <rea@FreeBSD.org>
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
 */

#include "includes.h"
__RCSID("$FreeBSD$");

#include <string.h>

#include "version.h"
#include "xmalloc.h"


static char *version = NULL;
/* NULL means "use default value", empty string means "unset" */
static const char *addendum = NULL;
static unsigned char update_version = 1;

/*
 * Constructs the version string if it is empty or needs updating.
 *
 * HPN patch we're running requires both parties
 * to have the "hpn" string inside the advertized version
 * (see compat.c::compat_datafellows), so we should
 * include it to the generated string if HPN is enabled.
 */
const char *
ssh_version_get(int hpn_disabled)
{
	const char *hpn = NULL, *add = NULL;
	char *newvers = NULL;
	size_t size = 0;

	if (version != NULL && !update_version)
		return (version);

	hpn = (hpn_disabled ? NULL : SSH_VERSION_HPN);
	add = (addendum == NULL ? SSH_VERSION_ADDENDUM :
	    (addendum[0] == '\0' ? NULL : addendum));

	size = strlen(SSH_VERSION_BASE) + (hpn ? strlen(hpn) : 0) +
	    (add ? strlen(add) + 1 : 0) + 1;
	newvers = xmalloc(size);
	strcpy(newvers, SSH_VERSION_BASE);
	if (hpn)
		strcat(newvers, hpn);
	if (add) {
		strcat(newvers, " ");
		strcat(newvers, add);
	}

	if (version)
		xfree(version);
	version = newvers;
	update_version = 0;

	return (version);
}

void
ssh_version_set_addendum(const char *add)
{
	if (add && addendum && !strcmp(add, addendum))
		return;

	if (addendum)
		xfree((void *)addendum);
	addendum = (add ? xstrdup(add) : xstrdup(""));

	update_version = 1;
}
