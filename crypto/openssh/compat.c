/*
 * Copyright (c) 1999 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Markus Friedl.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"
RCSID("$Id: compat.c,v 1.13 2000/05/08 17:42:24 markus Exp $");

#include "ssh.h"
#include "packet.h"
#include "xmalloc.h"
#include "compat.h"

int compat13 = 0;
int compat20 = 0;
int datafellows = 0;

void
enable_compat20(void)
{
	verbose("Enabling compatibility mode for protocol 2.0");
	compat20 = 1;
}
void
enable_compat13(void)
{
	verbose("Enabling compatibility mode for protocol 1.3");
	compat13 = 1;
}
/* datafellows bug compatibility */
void
compat_datafellows(const char *version)
{
	int i;
	size_t len;
	struct {
		char	*version;
		int	bugs;
	} check[] = {
		{"2.1.0",	SSH_BUG_SIGBLOB|SSH_BUG_HMAC},
		{"2.0.1",	SSH_BUG_SIGBLOB|SSH_BUG_HMAC|SSH_BUG_PUBKEYAUTH|SSH_BUG_X11FWD},
		{NULL,		0}
	};
	for (i = 0; check[i].version; i++) {
		len = strlen(check[i].version);
		if (strlen(version) >= len &&
		   (strncmp(version, check[i].version, len) == 0)) {
			verbose("datafellows: %.200s", version);
			datafellows = check[i].bugs;
			return;
		}
	}
}

#define	SEP	","
int
proto_spec(const char *spec)
{
	char *s = xstrdup(spec);
	char *p;
	int ret = SSH_PROTO_UNKNOWN;

	for ((p = strtok(s, SEP)); p; (p = strtok(NULL, SEP))) {
		switch(atoi(p)) {
		case 1:
			if (ret == SSH_PROTO_UNKNOWN)
				ret |= SSH_PROTO_1_PREFERRED;
			ret |= SSH_PROTO_1;
			break;
		case 2:
			ret |= SSH_PROTO_2;
			break;
		default:
			log("ignoring bad proto spec: '%s'.", p);
			break;
		}
	}
	xfree(s);
	return ret;
}
