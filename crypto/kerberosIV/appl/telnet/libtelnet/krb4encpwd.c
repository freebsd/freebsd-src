/*-
 * Copyright (c) 1992, 1993
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
 */
/* $FreeBSD$ */

#include <config.h>

RCSID("$Id: krb4encpwd.c,v 1.18 1999/09/16 20:41:34 assar Exp $");

#ifdef	KRB4_ENCPWD
/*
 * COPYRIGHT (C) 1990 DIGITAL EQUIPMENT CORPORATION
 * ALL RIGHTS RESERVED
 *
 * "Digital Equipment Corporation authorizes the reproduction,
 * distribution and modification of this software subject to the following
 * restrictions:
 *
 * 1.  Any partial or whole copy of this software, or any modification
 * thereof, must include this copyright notice in its entirety.
 *
 * 2.  This software is supplied "as is" with no warranty of any kind,
 * expressed or implied, for any purpose, including any warranty of fitness
 * or merchantibility.  DIGITAL assumes no responsibility for the use or
 * reliability of this software, nor promises to provide any form of
 * support for it on any basis.
 *
 * 3.  Distribution of this software is authorized only if no profit or
 * remuneration of any kind is received in exchange for such distribution.
 *
 * 4.  This software produces public key authentication certificates
 * bearing an expiration date established by DIGITAL and RSA Data
 * Security, Inc.  It may cease to generate certificates after the expiration
 * date.  Any modification of this software that changes or defeats
 * the expiration date or its effect is unauthorized.
 *
 * 5.  Software that will renew or extend the expiration date of
 * authentication certificates produced by this software may be obtained
 * from RSA Data Security, Inc., 10 Twin Dolphin Drive, Redwood City, CA
 * 94065, (415)595-8782, or from DIGITAL"
 *
 */

#include <sys/types.h>
#include <arpa/telnet.h>
#include <pwd.h>
#include <stdio.h>

#include <openssl/des.h>
#include <krb.h>
#include <stdlib.h>
#include <string.h>
#ifdef SOCKS
#include <socks.h>
#endif

#include "encrypt.h"
#include "auth.h"
#include "misc.h"

int krb_mk_encpwd_req (KTEXT, char *, char *, char *, char *, char *, char *);
int krb_rd_encpwd_req (KTEXT, char *, char *, u_long, AUTH_DAT *, char *, char *, char *, char *);

extern auth_debug_mode;

static unsigned char str_data[1024] = { IAC, SB, TELOPT_AUTHENTICATION, 0,
			  		AUTHTYPE_KRB4_ENCPWD, };
static unsigned char str_name[1024] = { IAC, SB, TELOPT_AUTHENTICATION,
					TELQUAL_NAME, };

#define	KRB4_ENCPWD_AUTH	0	/* Authentication data follows */
#define	KRB4_ENCPWD_REJECT	1	/* Rejected (reason might follow) */
#define KRB4_ENCPWD_ACCEPT	2	/* Accepted */
#define	KRB4_ENCPWD_CHALLENGE	3	/* Challenge for mutual auth. */
#define	KRB4_ENCPWD_ACK		4	/* Acknowledge */

#define KRB_SERVICE_NAME    "rcmd"

static	KTEXT_ST auth;
static	char name[ANAME_SZ];
static	char user_passwd[ANAME_SZ];
static	AUTH_DAT adat = { 0 };
static des_key_schedule sched;
static char  challenge[REALM_SZ];

	static int
Data(ap, type, d, c)
	Authenticator *ap;
	int type;
	void *d;
	int c;
{
	unsigned char *p = str_data + 4;
	unsigned char *cd = (unsigned char *)d;

	if (c == -1)
		c = strlen(cd);

	if (0) {
		printf("%s:%d: [%d] (%d)",
			str_data[3] == TELQUAL_IS ? ">>>IS" : ">>>REPLY",
			str_data[3],
			type, c);
		printd(d, c);
		printf("\r\n");
	}
	*p++ = ap->type;
	*p++ = ap->way;
	*p++ = type;
	while (c-- > 0) {
		if ((*p++ = *cd++) == IAC)
			*p++ = IAC;
	}
	*p++ = IAC;
	*p++ = SE;
	if (str_data[3] == TELQUAL_IS)
		printsub('>', &str_data[2], p - (&str_data[2]));
	return(telnet_net_write(str_data, p - str_data));
}

	int
krb4encpwd_init(ap, server)
	Authenticator *ap;
	int server;
{
	char hostname[80], *cp, *realm;
	des_clock skey;

	if (server) {
		str_data[3] = TELQUAL_REPLY;
	} else {
		str_data[3] = TELQUAL_IS;
		gethostname(hostname, sizeof(hostname));
		realm = krb_realmofhost(hostname);
		cp = strchr(hostname, '.');
		if (*cp != NULL) *cp = NULL;
		if (read_service_key(KRB_SERVICE_NAME, hostname, realm, 0,
					KEYFILE, (char *)skey)) {
		  return(0);
		}
	}
	return(1);
}

	int
krb4encpwd_send(ap)
	Authenticator *ap;
{

	printf("[ Trying KRB4ENCPWD ... ]\r\n");
	if (!UserNameRequested) {
		return(0);
	}
	if (!auth_sendname(UserNameRequested, strlen(UserNameRequested))) {
		return(0);
	}

	if (!Data(ap, KRB4_ENCPWD_ACK, NULL, 0)) {
		return(0);
	}

	return(1);
}

	void
krb4encpwd_is(ap, data, cnt)
	Authenticator *ap;
	unsigned char *data;
	int cnt;
{
	Session_Key skey;
	des_cblock datablock;
	char  r_passwd[ANAME_SZ], r_user[ANAME_SZ];
	char  lhostname[ANAME_SZ], *cp;
	int r;
	time_t now;

	if (cnt-- < 1)
		return;
	switch (*data++) {
	case KRB4_ENCPWD_AUTH:
		memmove(auth.dat, data, auth.length = cnt);

		gethostname(lhostname, sizeof(lhostname));
		if ((cp = strchr(lhostname, '.')) != 0)  *cp = '\0';

		if (r = krb_rd_encpwd_req(&auth, KRB_SERVICE_NAME, lhostname, 0, &adat, NULL, challenge, r_user, r_passwd)) {
			Data(ap, KRB4_ENCPWD_REJECT, "Auth failed", -1);
			auth_finished(ap, AUTH_REJECT);
			return;
		}
		auth_encrypt_userpwd(r_passwd);
		if (passwdok(UserNameRequested, UserPassword) == 0) {
		  /*
		   *  illegal username and password
		   */
		  Data(ap, KRB4_ENCPWD_REJECT, "Illegal password", -1);
		  auth_finished(ap, AUTH_REJECT);
		  return;
		}

		memmove(session_key, adat.session, sizeof(des_cblock));
		Data(ap, KRB4_ENCPWD_ACCEPT, 0, 0);
		auth_finished(ap, AUTH_USER);
		break;

	case KRB4_ENCPWD_CHALLENGE:
		/*
		 *  Take the received random challenge text and save
		 *  for future authentication.
		 */
		memmove(challenge, data, sizeof(des_cblock));
		break;


	case KRB4_ENCPWD_ACK:
		/*
		 *  Receive ack, if mutual then send random challenge
		 */

		/*
		 * If we are doing mutual authentication, get set up to send
		 * the challenge, and verify it when the response comes back.
		 */

		if ((ap->way & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL) {
		  int i;

		  time(&now);
		  snprintf(challenge, sizeof(challenge), "%x", now);
		  Data(ap, KRB4_ENCPWD_CHALLENGE, challenge, strlen(challenge));
		}
		break;

	default:
		Data(ap, KRB4_ENCPWD_REJECT, 0, 0);
		break;
	}
}


	void
krb4encpwd_reply(ap, data, cnt)
	Authenticator *ap;
	unsigned char *data;
	int cnt;
{
	Session_Key skey;
	KTEXT_ST krb_token;
	des_cblock enckey;
	CREDENTIALS cred;
	int r;
	char	randchal[REALM_SZ], instance[ANAME_SZ], *cp;
	char	hostname[80], *realm;

	if (cnt-- < 1)
		return;
	switch (*data++) {
	case KRB4_ENCPWD_REJECT:
		if (cnt > 0) {
			printf("[ KRB4_ENCPWD refuses authentication because %.*s ]\r\n",
				cnt, data);
		} else
			printf("[ KRB4_ENCPWD refuses authentication ]\r\n");
		auth_send_retry();
		return;
	case KRB4_ENCPWD_ACCEPT:
		printf("[ KRB4_ENCPWD accepts you ]\r\n");
		auth_finished(ap, AUTH_USER);
		return;
	case KRB4_ENCPWD_CHALLENGE:
		/*
		 * Verify that the response to the challenge is correct.
		 */

		gethostname(hostname, sizeof(hostname));
		realm = krb_realmofhost(hostname);
		memmove(challenge, data, cnt);
		memset(user_passwd, 0, sizeof(user_passwd));
		des_read_pw_string(user_passwd, sizeof(user_passwd)-1, "Password: ", 0);
		UserPassword = user_passwd;
		Challenge = challenge;
		strlcpy(instance, RemoteHostName, sizeof(instance));
		if ((cp = strchr(instance, '.')) != 0)  *cp = '\0';

		if (r = krb_mk_encpwd_req(&krb_token, KRB_SERVICE_NAME, instance, realm, Challenge, UserNameRequested, user_passwd)) {
		  krb_token.length = 0;
		}

		if (!Data(ap, KRB4_ENCPWD_AUTH, krb_token.dat, krb_token.length)) {
		  return;
		}

		break;

	default:
		return;
	}
}

	int
krb4encpwd_status(ap, name, name_sz, level)
	Authenticator *ap;
	char *name;
	size_t name_sz;
	int level;
{

	if (level < AUTH_USER)
		return(level);

	if (UserNameRequested && passwdok(UserNameRequested, UserPassword)) {
		strlcpy(name, UserNameRequested, name_sz);
		return(AUTH_VALID);
	} else {
		return(AUTH_USER);
	}
}

#define	BUMP(buf, len)		while (*(buf)) {++(buf), --(len);}
#define	ADDC(buf, len, c)	if ((len) > 0) {*(buf)++ = (c); --(len);}

	void
krb4encpwd_printsub(data, cnt, buf, buflen)
	unsigned char *data, *buf;
	int cnt, buflen;
{
	int i;

	buf[buflen-1] = '\0';		/* make sure its NULL terminated */
	buflen -= 1;

	switch(data[3]) {
	case KRB4_ENCPWD_REJECT:	/* Rejected (reason might follow) */
		strlcpy((char *)buf, " REJECT ", buflen);
		goto common;

	case KRB4_ENCPWD_ACCEPT:	/* Accepted (name might follow) */
		strlcpy((char *)buf, " ACCEPT ", buflen);
	common:
		BUMP(buf, buflen);
		if (cnt <= 4)
			break;
		ADDC(buf, buflen, '"');
		for (i = 4; i < cnt; i++)
			ADDC(buf, buflen, data[i]);
		ADDC(buf, buflen, '"');
		ADDC(buf, buflen, '\0');
		break;

	case KRB4_ENCPWD_AUTH:		/* Authentication data follows */
		strlcpy((char *)buf, " AUTH", buflen);
		goto common2;

	case KRB4_ENCPWD_CHALLENGE:
		strlcpy((char *)buf, " CHALLENGE", buflen);
		goto common2;

	case KRB4_ENCPWD_ACK:
		strlcpy((char *)buf, " ACK", buflen);
		goto common2;

	default:
		snprintf(buf, buflen, " %d (unknown)", data[3]);
	common2:
		BUMP(buf, buflen);
		for (i = 4; i < cnt; i++) {
			snprintf(buf, buflen, " %d", data[i]);
			BUMP(buf, buflen);
		}
		break;
	}
}

int passwdok(name, passwd)
char *name, *passwd;
{
  char *crypt();
  char *salt, *p;
  struct passwd *pwd;
  int   passwdok_status = 0;

  if (pwd = k_getpwnam(name))
    salt = pwd->pw_passwd;
  else salt = "xx";

  p = crypt(passwd, salt);

  if (pwd && !strcmp(p, pwd->pw_passwd)) {
    passwdok_status = 1;
  } else passwdok_status = 0;
  return(passwdok_status);
}

#endif

#ifdef notdef

prkey(msg, key)
	char *msg;
	unsigned char *key;
{
	int i;
	printf("%s:", msg);
	for (i = 0; i < 8; i++)
		printf(" %3d", key[i]);
	printf("\r\n");
}
#endif
