/*
 * Copyright (c) 1999, 2000, 2001, 2002 Markus Friedl.  All rights reserved.
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
RCSID("$OpenBSD: compat.c,v 1.65 2002/09/27 10:42:09 mickey Exp $");
RCSID("$FreeBSD$");

#include "buffer.h"
#include "packet.h"
#include "xmalloc.h"
#include "compat.h"
#include "log.h"
#include "match.h"

int compat13 = 0;
int compat20 = 0;
int datafellows = 0;

void
enable_compat20(void)
{
	debug("Enabling compatibility mode for protocol 2.0");
	compat20 = 1;
}
void
enable_compat13(void)
{
	debug("Enabling compatibility mode for protocol 1.3");
	compat13 = 1;
}
/* datafellows bug compatibility */
void
compat_datafellows(const char *version)
{
	int i;
	static struct {
		char	*pat;
		int	bugs;
	} check[] = {
		{ "OpenSSH-2.0*,"
		  "OpenSSH-2.1*,"
		  "OpenSSH_2.1*,"
		  "OpenSSH_2.2*",	SSH_OLD_SESSIONID|SSH_BUG_BANNER|
					SSH_OLD_DHGEX|SSH_BUG_NOREKEY|
					SSH_BUG_EXTEOF},
		{ "OpenSSH_2.3.0*",	SSH_BUG_BANNER|SSH_BUG_BIGENDIANAES|
					SSH_OLD_DHGEX|SSH_BUG_NOREKEY|
					SSH_BUG_EXTEOF},
		{ "OpenSSH_2.3.*",	SSH_BUG_BIGENDIANAES|SSH_OLD_DHGEX|
					SSH_BUG_NOREKEY|SSH_BUG_EXTEOF},
		{ "OpenSSH_2.5.0p1*,"
		  "OpenSSH_2.5.1p1*",
					SSH_BUG_BIGENDIANAES|SSH_OLD_DHGEX|
					SSH_BUG_NOREKEY|SSH_BUG_EXTEOF},
		{ "OpenSSH_2.5.0*,"
		  "OpenSSH_2.5.1*,"
		  "OpenSSH_2.5.2*",	SSH_OLD_DHGEX|SSH_BUG_NOREKEY|
					SSH_BUG_EXTEOF},
		{ "OpenSSH_2.5.3*",	SSH_BUG_NOREKEY|SSH_BUG_EXTEOF},
		{ "OpenSSH_2.*,"
		  "OpenSSH_3.0*,"
		  "OpenSSH_3.1*",	SSH_BUG_EXTEOF},
		{ "Sun_SSH_1.0*",	SSH_BUG_NOREKEY|SSH_BUG_EXTEOF},
		{ "OpenSSH*",		0 },
		{ "*MindTerm*",		0 },
		{ "2.1.0*",		SSH_BUG_SIGBLOB|SSH_BUG_HMAC|
					SSH_OLD_SESSIONID|SSH_BUG_DEBUG|
					SSH_BUG_RSASIGMD5|SSH_BUG_HBSERVICE },
		{ "2.1 *",		SSH_BUG_SIGBLOB|SSH_BUG_HMAC|
					SSH_OLD_SESSIONID|SSH_BUG_DEBUG|
					SSH_BUG_RSASIGMD5|SSH_BUG_HBSERVICE },
		{ "2.0.13*,"
		  "2.0.14*,"
		  "2.0.15*,"
		  "2.0.16*,"
		  "2.0.17*,"
		  "2.0.18*,"
		  "2.0.19*",		SSH_BUG_SIGBLOB|SSH_BUG_HMAC|
					SSH_OLD_SESSIONID|SSH_BUG_DEBUG|
					SSH_BUG_PKSERVICE|SSH_BUG_X11FWD|
					SSH_BUG_PKOK|SSH_BUG_RSASIGMD5|
					SSH_BUG_HBSERVICE|SSH_BUG_OPENFAILURE|
					SSH_BUG_DUMMYCHAN },
		{ "2.0.11*,"
		  "2.0.12*",		SSH_BUG_SIGBLOB|SSH_BUG_HMAC|
					SSH_OLD_SESSIONID|SSH_BUG_DEBUG|
					SSH_BUG_PKSERVICE|SSH_BUG_X11FWD|
					SSH_BUG_PKAUTH|SSH_BUG_PKOK|
					SSH_BUG_RSASIGMD5|SSH_BUG_OPENFAILURE|
					SSH_BUG_DUMMYCHAN },
		{ "2.0.*",		SSH_BUG_SIGBLOB|SSH_BUG_HMAC|
					SSH_OLD_SESSIONID|SSH_BUG_DEBUG|
					SSH_BUG_PKSERVICE|SSH_BUG_X11FWD|
					SSH_BUG_PKAUTH|SSH_BUG_PKOK|
					SSH_BUG_RSASIGMD5|SSH_BUG_OPENFAILURE|
					SSH_BUG_DERIVEKEY|SSH_BUG_DUMMYCHAN },
		{ "2.2.0*,"
		  "2.3.0*",		SSH_BUG_HMAC|SSH_BUG_DEBUG|
					SSH_BUG_RSASIGMD5 },
		{ "2.3.*",		SSH_BUG_DEBUG|SSH_BUG_RSASIGMD5 },
		{ "2.4",		SSH_OLD_SESSIONID },	/* Van Dyke */
		{ "2.*",		SSH_BUG_DEBUG },
		{ "3.0.*",		SSH_BUG_DEBUG },
		{ "3.0 SecureCRT*",	SSH_OLD_SESSIONID },
		{ "1.7 SecureFX*",	SSH_OLD_SESSIONID },
		{ "1.2.18*,"
		  "1.2.19*,"
		  "1.2.20*,"
		  "1.2.21*,"
		  "1.2.22*",		SSH_BUG_IGNOREMSG|SSH_BUG_K5USER },
		{ "1.3.2*",		/* F-Secure */
					SSH_BUG_IGNOREMSG|SSH_BUG_K5USER },
		{ "1.2.1*,"
		  "1.2.2*,"
		  "1.2.3*",		SSH_BUG_K5USER },
		{ "*SSH Compatible Server*",			/* Netscreen */
					SSH_BUG_PASSWORDPAD },
		{ "*OSU_0*,"
		  "OSU_1.0*,"
		  "OSU_1.1*,"
		  "OSU_1.2*,"
		  "OSU_1.3*,"
		  "OSU_1.4*,"
		  "OSU_1.5alpha1*,"
		  "OSU_1.5alpha2*,"
		  "OSU_1.5alpha3*",	SSH_BUG_PASSWORDPAD },
		{ "*SSH_Version_Mapper*",
					SSH_BUG_SCANNER },
		{ "Probe-*",
					SSH_BUG_PROBE },
		{ NULL,			0 }
	};

	/* process table, return first match */
	for (i = 0; check[i].pat; i++) {
		if (match_pattern_list(version, check[i].pat,
		    strlen(check[i].pat), 0) == 1) {
			debug("match: %s pat %s", version, check[i].pat);
			datafellows = check[i].bugs;
			return;
		}
	}
	debug("no match: %s", version);
}

#define	SEP	","
int
proto_spec(const char *spec)
{
	char *s, *p, *q;
	int ret = SSH_PROTO_UNKNOWN;

	if (spec == NULL)
		return ret;
	q = s = xstrdup(spec);
	for ((p = strsep(&q, SEP)); p && *p != '\0'; (p = strsep(&q, SEP))) {
		switch (atoi(p)) {
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

char *
compat_cipher_proposal(char *cipher_prop)
{
	Buffer b;
	char *orig_prop, *fix_ciphers;
	char *cp, *tmp;

	if (!(datafellows & SSH_BUG_BIGENDIANAES))
		return(cipher_prop);

	buffer_init(&b);
	tmp = orig_prop = xstrdup(cipher_prop);
	while ((cp = strsep(&tmp, ",")) != NULL) {
		if (strncmp(cp, "aes", 3) != 0) {
			if (buffer_len(&b) > 0)
				buffer_append(&b, ",", 1);
			buffer_append(&b, cp, strlen(cp));
		}
	}
	buffer_append(&b, "\0", 1);
	fix_ciphers = xstrdup(buffer_ptr(&b));
	buffer_free(&b);
	xfree(orig_prop);
	debug2("Original cipher proposal: %s", cipher_prop);
	debug2("Compat cipher proposal: %s", fix_ciphers);
	if (!*fix_ciphers)
		fatal("No available ciphers found.");

	return(fix_ciphers);
}
