/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: local_passwd.c,v 1.20 1998/05/19 03:48:07 jkoshy Exp $
 */

#ifndef lint
static const char sccsid[] = "@(#)local_passwd.c	8.3 (Berkeley) 4/2/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <libutil.h>

#include <pw_copy.h>
#include <pw_util.h>
#ifdef YP
#include <pw_yp.h>
#endif

#ifdef LOGGING
#include <syslog.h>
#endif

#ifdef LOGIN_CAP
#ifdef AUTH_NONE /* multiple defs :-( */
#undef AUTH_NONE
#endif
#include <login_cap.h>
#endif

#include "extern.h"

static uid_t uid;
int randinit;

char   *tempname;

static unsigned char itoa64[] =		/* 0 ... 63 => ascii - 64 */
	"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

void
to64(s, v, n)
	char *s;
	long v;
	int n;
{
	while (--n >= 0) {
		*s++ = itoa64[v&0x3f];
		v >>= 6;
	}
}

/*
** used in generating new salt
*/
char * gen_des_salt(char * salt, int htype, struct timeval * tvp);
char * gen_mcf_salt(char * salt, int htype, struct timeval * tvp);

#define DES_CRYPT  0 /* which index is the 'des' crypt.  Do not change */
#define BEST_CRYPT 1 /* which index is the 'best' crypt.  Do not change
                        the index, change the crypt_token for the index */
#define DEFAULT_CRYPT BEST_CRYPT /* or 0 for old behaviour */
struct hash_type {
    char * auth_token;
    char * crypt_token;
    char * (*gensalt)(char *, int, struct timeval *);
} hash_types[] = {
    { "des",  "",     gen_des_salt },
    { "best", "SHA1", gen_mcf_salt },
    { "md5",  "MD5",  gen_mcf_salt },
    { "sha1", "SHA1", gen_mcf_salt },
    { NULL,   NULL,   NULL }
};

char * gen_long_salt(char * salt, struct timeval * tvp) {
    char * s, * p;
    int    r, x;

    s = p = salt;

    to64(s, random(), 2);
    to64(s+2, tvp->tv_usec, 2);
    s += 4;
    for (x = 0; x < 3; x++) {
        to64(s, random(), 4);
        s+=4;
    }
    r = (random()%9) + 8;
    p[r] = '\0';

    return &salt[0];
}

char * gen_des_salt(char * salt, int htype, struct timeval * tvp) {
#ifdef NEWSALT
    salt[0] = _PASSWORD_EFMT1;
    to64(&salt[1], (long)(29 * 25), 4);
    to64(&salt[5], random(), 4);
    salt[9] = '\0';
    return &salt[0];
#else
    return gen_long_salt(salt, tvp);
#endif
}

/*
** Make a good size salt for algoritms that can use it,
** infact randomize it from 8 to 16 chars in length too
*/
char * gen_mcf_salt(char * salt, int htype, struct timeval * tvp) {
    char * s;

    sprintf(salt, "$%s$", hash_types[htype].crypt_token);
    s = &salt[strlen(salt)];
    gen_long_salt(s, tvp);
    return salt;
}

char * generate_salt(char * salt, char * curpwd) {
    int    x;
    char * p, *s;
    char * v = auth_getval("auth_default");
    char   sbuf[32];
    short  descrypt = 0;
    struct timeval tv;

    if (!randinit) {
        randinit = 1;
        srandomdev();
    }
    gettimeofday(&tv,0);

    /* see if DES crypt is crypt()'s default */
    p = crypt("a", "bc");
    if (p[0] != '$')
        descrypt = 1;

    /* nothing defined, default to old behaviour */
    if (v == NULL) {
        if (curpwd && curpwd[0] == '$') {
            salt[0] = '$';
            s = &salt[1];
            p = &curpwd[1];
            for (;*p && isalnum(*p); s++, p++) {
                *s = *p;
            }
            *s++ = '$';
        } else {
            s = &salt[0];
        }
        gen_long_salt(s, &tv);
        return salt;
    }

    /* cleanup the auth token */
    for (p = &v[strlen(v) - 1]; p >= v; p--) {
        if (isalnum(*p))
            break;
        *p = (char) NULL;
    }

    for (x = 0; hash_types[x].crypt_token != NULL; x++) {
        if (strcasecmp(v, hash_types[x].auth_token) == 0) {
            if (x == DES_CRYPT && !descrypt) {
                if (strlen(hash_types[DEFAULT_CRYPT].crypt_token))
                    x = DEFAULT_CRYPT;
                else
                    x = BEST_CRYPT;
                printf("WARNING: DES crypt specified in /etc/auth.conf, but crypt(3) library does\nnot have DES hash support installed.  Will use default (%s) instead!\n", hash_types[x].crypt_token);
                return (hash_types[x].gensalt)(salt, x, &tv);
            }

            /* we have a match, verify it is valid */
            if (!strlen(hash_types[x].crypt_token))
                return (hash_types[x].gensalt)(salt, x, &tv);

            sprintf(sbuf, "$%s$", hash_types[x].crypt_token);
            p = crypt(hash_types[x].auth_token, sbuf);
            if (strncmp(&p[1], hash_types[x].crypt_token,
                           strlen(hash_types[x].crypt_token)))
            {
                printf("WARNING: Specified auth default (%s) in /etc/auth.conf is not supported\nby crypt(3) library, using best (%s) instead.\n",
hash_types[x].auth_token, hash_types[BEST_CRYPT].crypt_token);
                return (hash_types[BEST_CRYPT].gensalt)(salt, BEST_CRYPT, &tv);
            }
            return (hash_types[x].gensalt)(salt, x, &tv);
        }
    }
    printf("WARNING: Specified auth default (%s) in /etc/auth.conf is not recognized.\nUsing best type (%s) instead.\n", v, hash_types[BEST_CRYPT].crypt_token);
    return (hash_types[BEST_CRYPT].gensalt)(salt, BEST_CRYPT, &tv);
}

char *
getnewpasswd(pw, nis)
	struct passwd *pw;
	int nis;
{
	int tries, min_length = 6;
	char *p, *t;
#ifdef LOGIN_CAP
	login_cap_t * lc;
#endif
	char buf[_PASSWORD_LEN+1], salt[10];

	if (!nis)
		(void)printf("Changing local password for %s.\n", pw->pw_name);

	if (uid && pw->pw_passwd[0] &&
	    strcmp(crypt(getpass("Old password:"), pw->pw_passwd),
	    pw->pw_passwd)) {
		errno = EACCES;
		pw_error(NULL, 1, 1);
	}

#ifdef LOGIN_CAP
	/*
	 * Determine minimum password length and next password change date.
	 * Note that even for NIS passwords, login_cap is still used.
	 */
	if ((lc = login_getpwclass(pw)) != NULL) {
		time_t	period;

		/* minpasswordlen capablity */
		min_length = (int)login_getcapnum(lc, "minpasswordlen",
				min_length, min_length);
		/* passwordtime capability */
		period = login_getcaptime(lc, "passwordtime", 0, 0);
		if (period > (time_t)0) {
			pw->pw_change = time(NULL) + period;
		}
		login_close(lc);
	}
#endif

	for (buf[0] = '\0', tries = 0;;) {
		p = getpass("New password:");
		if (!*p) {
			(void)printf("Password unchanged.\n");
			pw_error(NULL, 0, 0);
		}
		if (strlen(p) < min_length && (uid != 0 || ++tries < 2)) {
			(void)printf("Please enter a password at least %d characters in length.\n", min_length);
			continue;
		}
		for (t = p; *t && islower(*t); ++t);
		if (!*t && (uid != 0 || ++tries < 2)) {
			(void)printf("Please don't use an all-lower case password.\nUnusual capitalization, control characters or digits are suggested.\n");
			continue;
		}
		(void)strcpy(buf, p);
		if (!strcmp(buf, getpass("Retype new password:")))
			break;
		(void)printf("Mismatch; try again, EOF to quit.\n");
	}
        generate_salt(salt, pw->pw_passwd);
	return (crypt(buf, salt));
}

int
local_passwd(uname)
	char *uname;
{
	struct passwd *pw;
	int pfd, tfd;

	if (!(pw = getpwnam(uname)))
		errx(1, "unknown user %s", uname);

#ifdef YP
	/* Use the right password information. */
	pw = (struct passwd *)&local_password;
#endif
	uid = getuid();
	if (uid && uid != pw->pw_uid)
		errx(1, "%s", strerror(EACCES));

	pw_init();

	/*
	 * Get the new password.  Reset passwd change time to zero by
	 * default. If the user has a valid login class (or the default
	 * fallback exists), then the next password change date is set
	 * by getnewpasswd() according to the "passwordtime" capability
	 * if one has been specified.
	 */
	pw->pw_change = 0;
	pw->pw_passwd = getnewpasswd(pw, 0);

	pfd = pw_lock();
	tfd = pw_tmp();
	pw_copy(pfd, tfd, pw);

	if (!pw_mkdb(uname))
		pw_error((char *)NULL, 0, 1);
#ifdef LOGGING
	syslog(LOG_DEBUG, "user %s changed their local password\n", uname);
#endif
	return (0);
}
