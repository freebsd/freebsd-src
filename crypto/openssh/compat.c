/*
 * Copyright (c) 1999,2000 Markus Friedl.  All rights reserved.
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
RCSID("$FreeBSD$");
RCSID("$OpenBSD: compat.c,v 1.47 2001/04/18 23:43:25 markus Exp $");

#include <regex.h>

#include "packet.h"
#include "xmalloc.h"
#include "compat.h"
#include "log.h"

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
	int i, ret;
	char ebuf[1024];
	regex_t reg;
	static struct {
		char	*pat;
		int	bugs;
	} check[] = {
		{ "^OpenSSH[-_]2\\.[012]",
					SSH_OLD_SESSIONID|SSH_BUG_BANNER|
					SSH_OLD_DHGEX|SSH_BUG_NOREKEY },
		{ "^OpenSSH_2\\.3\\.0", SSH_BUG_BANNER|SSH_BUG_BIGENDIANAES|
					SSH_OLD_DHGEX|SSH_BUG_NOREKEY},
		{ "^OpenSSH_2\\.3\\.",  SSH_BUG_BIGENDIANAES|SSH_OLD_DHGEX|
					SSH_BUG_NOREKEY},
		{ "^OpenSSH_2\\.5\\.[01]p1",
					SSH_BUG_BIGENDIANAES|SSH_OLD_DHGEX|
					SSH_BUG_NOREKEY },
		{ "^OpenSSH_2\\.5\\.[012]",
					SSH_OLD_DHGEX|SSH_BUG_NOREKEY },
		{ "^OpenSSH_2\\.5\\.3",
					SSH_BUG_NOREKEY },
		{ "^OpenSSH",		0 },
		{ "MindTerm",		0 },
		{ "^2\\.1\\.0",		SSH_BUG_SIGBLOB|SSH_BUG_HMAC|
					SSH_OLD_SESSIONID|SSH_BUG_DEBUG|
					SSH_BUG_RSASIGMD5|SSH_BUG_HBSERVICE },
		{ "^2\\.1 ",		SSH_BUG_SIGBLOB|SSH_BUG_HMAC|
					SSH_OLD_SESSIONID|SSH_BUG_DEBUG|
					SSH_BUG_RSASIGMD5|SSH_BUG_HBSERVICE },
		{ "^2\\.0\\.1[3-9]",	SSH_BUG_SIGBLOB|SSH_BUG_HMAC|
					SSH_OLD_SESSIONID|SSH_BUG_DEBUG|
					SSH_BUG_PKSERVICE|SSH_BUG_X11FWD|
					SSH_BUG_PKOK|SSH_BUG_RSASIGMD5|
					SSH_BUG_HBSERVICE },
		{ "^2\\.0\\.",		SSH_BUG_SIGBLOB|SSH_BUG_HMAC|
					SSH_OLD_SESSIONID|SSH_BUG_DEBUG|
					SSH_BUG_PKSERVICE|SSH_BUG_X11FWD|
					SSH_BUG_PKAUTH|SSH_BUG_PKOK|
					SSH_BUG_RSASIGMD5 },
		{ "^2\\.[23]\\.0",	SSH_BUG_HMAC|SSH_BUG_RSASIGMD5 },
		{ "^2\\.3\\.",		SSH_BUG_RSASIGMD5 },
		{ "^2\\.[2-9]\\.",	0 },
		{ "^2\\.4$",		SSH_OLD_SESSIONID },	/* Van Dyke */
		{ "^3\\.0 SecureCRT",	SSH_OLD_SESSIONID },
		{ "^1\\.7 SecureFX",	SSH_OLD_SESSIONID },
		{ "^1\\.2\\.1[89]",	SSH_BUG_IGNOREMSG },
		{ "^1\\.2\\.2[012]",	SSH_BUG_IGNOREMSG },
		{ "^1\\.3\\.2",		SSH_BUG_IGNOREMSG },	/* f-secure */
		{ "^SSH Compatible Server",			/* Netscreen */
					SSH_BUG_PASSWORDPAD },
		{ "^OSU_0",		SSH_BUG_PASSWORDPAD },
		{ "^OSU_1\\.[0-4]",	SSH_BUG_PASSWORDPAD },
		{ "^OSU_1\\.5alpha[1-3]",
					SSH_BUG_PASSWORDPAD },
		{ "^SSH_Version_Mapper",
					SSH_BUG_SCANNER },
		{ NULL,			0 }
	};
	/* process table, return first match */
	for (i = 0; check[i].pat; i++) {
		ret = regcomp(&reg, check[i].pat, REG_EXTENDED|REG_NOSUB);
		if (ret != 0) {
			regerror(ret, &reg, ebuf, sizeof(ebuf));
			ebuf[sizeof(ebuf)-1] = '\0';
			error("regerror: %s", ebuf);
			continue;
		}
		ret = regexec(&reg, version, 0, NULL, 0);
		regfree(&reg);
		if (ret == 0) {
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

char *
compat_cipher_proposal(char *cipher_prop)
{
	char *orig_prop, *fix_ciphers;
	char *cp, *tmp;
	size_t len;

	if (!(datafellows & SSH_BUG_BIGENDIANAES))
		return(cipher_prop);

	len = strlen(cipher_prop) + 1;
	fix_ciphers = xmalloc(len);
	*fix_ciphers = '\0';
	tmp = orig_prop = xstrdup(cipher_prop);
	while((cp = strsep(&tmp, ",")) != NULL) {
		if (strncmp(cp, "aes", 3) && strncmp(cp, "rijndael", 8)) {
			if (*fix_ciphers)
				strlcat(fix_ciphers, ",", len);
			strlcat(fix_ciphers, cp, len);
		}
	}
	xfree(orig_prop);
	debug2("Original cipher proposal: %s", cipher_prop);
	debug2("Compat cipher proposal: %s", fix_ciphers);
	if (!*fix_ciphers)
		fatal("No available ciphers found.");

	return(fix_ciphers);
}
