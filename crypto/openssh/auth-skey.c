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
RCSID("$OpenBSD: auth-skey.c,v 1.8 2000/09/07 20:27:49 deraadt Exp $");
RCSID("$FreeBSD$");

#include <sys/sysctl.h>
#include "ssh.h"
#include "packet.h"
#include <sha.h>

/*
 * try skey authentication,
 * return 1 on success, 0 on failure, -1 if skey is not available
 */

int
auth_skey_password(struct passwd * pw, const char *password)
{
	if (strncasecmp(password, "s/key", 5) == 0) {
		char *skeyinfo = opie_keyinfo(pw->pw_name);
		if (skeyinfo == NULL) {
			debug("generating fake skeyinfo for %.100s.",
			    pw->pw_name);
			skeyinfo = skey_fake_keyinfo(pw->pw_name);
		}
		if (skeyinfo != NULL)
			packet_send_debug(skeyinfo);
		/* Try again. */
		return 0;
	} else if (opie_haskey(pw->pw_name) == 0 &&
		   opie_passverify(pw->pw_name, (char *) password) != -1) {
		/* Authentication succeeded. */
		return 1;
	}
	/* Fall back to ordinary passwd authentication. */
	return -1;
}

/* from %OpenBSD: skeylogin.c,v 1.32 1999/08/16 14:46:56 millert Exp % */

#define ROUND(x)   (((x)[0] << 24) + (((x)[1]) << 16) + (((x)[2]) << 8) + \
		    ((x)[3]))

/*
 * hash_collapse()
 */
static u_int32_t
hash_collapse(s)
	u_char *s;
{
	int len, target;
	u_int32_t i;
	
	if ((strlen(s) % sizeof(u_int32_t)) == 0)
		target = strlen(s);    /* Multiple of 4 */
	else
		target = strlen(s) - (strlen(s) % sizeof(u_int32_t));

	for (i = 0, len = 0; len < target; len += 4)
		i ^= ROUND(s + len);

	return i;
}

char *
skey_fake_keyinfo(char *username)
{
	int i;
	u_int ptr;
	u_char hseed[OPIE_SEED_MAX], flg = 1, *up;
	char pbuf[OPIE_SECRET_MAX+1];
	static char skeyprompt[OPIE_CHALLENGE_MAX+1];
	char *secret = NULL;
	size_t secretlen = 0;
	SHA1_CTX ctx;
	char *p, *u;
	int mib[2];
	size_t size;
	struct timeval boottime;

	/*
	 * Base first 2 chars of seed on hostname.
	 * Add some filler for short hostnames if necessary.
	 */
	if (gethostname(pbuf, sizeof(pbuf)) == -1)
		*(p = pbuf) = '.';
	else
		for (p = pbuf; *p && isalnum(*p); p++)
			if (isalpha(*p) && isupper(*p))
				*p = tolower(*p);
	if (*p && pbuf - p < 2)
		(void)strncpy(p, "asjd", 2 - (pbuf - p));
	pbuf[2] = '\0';

	/* Hash the username if possible */
	if ((up = SHA1_Data(username, strlen(username), NULL)) != NULL) {
		struct stat sb;
		time_t t;

		/* Collapse the hash */
		ptr = hash_collapse(up);
		memset(up, 0, strlen(up));

		/*
		 * Seed the fake challenge with the system boot time,
		 * otherwise use ctime.
		 *
		 * XXX This should be a random source which is constant
		 * over short time periods, but changes over timescales on
		 * the order of a week.
		 */
		mib[0] = CTL_KERN;
		mib[1] = KERN_BOOTTIME;
		size = sizeof(boottime);
		bzero(&boottime, size);
		if (sysctl(mib, 2, &boottime, &size, NULL, 0) != -1 &&
			boottime.tv_sec != 0) {
			secret = (char *)&boottime;
			secretlen = size/sizeof(char); 
			flg = 0;
		} else if (!stat(_PATH_MEM, &sb) || !stat("/", &sb)) {
			t = sb.st_ctime;
			secret = ctime(&t);
			secretlen = strlen(secret);
			flg = 0;
		}
	}

	/* Put that in your pipe and smoke it */
	if (flg == 0) {
		/* Hash secret value with username */
		SHA1_Init(&ctx);
		SHA1_Update(&ctx, secret, secretlen);
		SHA1_Update(&ctx, username, strlen(username));
		SHA1_End(&ctx, up);
		
		/* Zero out */
		memset(secret, 0, secretlen);

		/* Now hash the hash */
		SHA1_Init(&ctx);
		SHA1_Update(&ctx, up, strlen(up));
		SHA1_End(&ctx, up);
		
		ptr = hash_collapse(up + 4);
		
		for (i = 2; i < 6; i++) {
			pbuf[i] = (ptr % 10) + '0';
			ptr /= 10;
		}
		pbuf[i] = '\0';

		/* Sequence number */
		ptr = ((up[2] + up[3]) % 499) + 1;

		memset(up, 0, 20); /* SHA1 specific */
		free(up);

		(void)snprintf(skeyprompt, sizeof skeyprompt,
			      "otp-%.*s %d %.*s ext",
			      OPIE_HASHNAME_MAX,
			      opie_get_algorithm(),
			      ptr, OPIE_SEED_MAX,
			      pbuf);
	} else {
		/* Base last 4 chars of seed on username */
		u = username;
		i = 4;
		p = &pbuf[2];
		do {
			if (*u == 0) {
				/* Pad remainder with zeros */
				while (--i >= 0)
					*p++ = '0';
				break;
			}

			*p++ = (*u++ % 10) + '0';
		} while (--i != 0);
		pbuf[6] = '\0';

		(void)snprintf(skeyprompt, sizeof skeyprompt,
			      "otp-md5 %d %.*s ext",
			      499, OPIE_SEED_MAX, pbuf);
	}
	return skeyprompt;
}
