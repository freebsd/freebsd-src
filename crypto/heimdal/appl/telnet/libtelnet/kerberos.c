/*-
 * Copyright (c) 1991, 1993
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

/*
 * Copyright (C) 1990 by the Massachusetts Institute of Technology
 *
 * Export of this software from the United States of America is assumed
 * to require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

RCSID("$Id: kerberos.c,v 1.51 2001/02/15 04:20:52 assar Exp $");

#ifdef	KRB4
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_ARPA_TELNET_H
#include <arpa/telnet.h>
#endif
#include <stdio.h>
#include <krb.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <roken.h>
#ifdef SOCKS
#include <socks.h>
#endif


#include "encrypt.h"
#include "auth.h"
#include "misc.h"

int kerberos4_cksum (unsigned char *, int);
extern int auth_debug_mode;

static unsigned char str_data[2048] = { IAC, SB, TELOPT_AUTHENTICATION, 0,
			  		AUTHTYPE_KERBEROS_V4, };

#define	KRB_AUTH	0		/* Authentication data follows */
#define	KRB_REJECT	1		/* Rejected (reason might follow) */
#define	KRB_ACCEPT	2		/* Accepted */
#define	KRB_CHALLENGE	3		/* Challenge for mutual auth. */
#define	KRB_RESPONSE	4		/* Response for mutual auth. */

#define KRB_FORWARD		5	/* */
#define KRB_FORWARD_ACCEPT	6	/* */
#define KRB_FORWARD_REJECT	7	/* */

#define KRB_SERVICE_NAME   "rcmd"

static	KTEXT_ST auth;
static	char name[ANAME_SZ];
static	AUTH_DAT adat;
static des_cblock session_key;
static des_cblock cred_session;
static des_key_schedule sched;
static des_cblock challenge;
static int auth_done; /* XXX */

static int pack_cred(CREDENTIALS *cred, unsigned char *buf);
static int unpack_cred(unsigned char *buf, int len, CREDENTIALS *cred);


static int
Data(Authenticator *ap, int type, const void *d, int c)
{
    unsigned char *p = str_data + 4;
    const unsigned char *cd = (const unsigned char *)d;

    if (c == -1)
	c = strlen((const char *)cd);

    if (auth_debug_mode) {
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
kerberos4_init(Authenticator *ap, int server)
{
    FILE *fp;

    if (server) {
	str_data[3] = TELQUAL_REPLY;
	if ((fp = fopen(KEYFILE, "r")) == NULL)
	    return(0);
	fclose(fp);
    } else {
	str_data[3] = TELQUAL_IS;
    }
    return(1);
}

char dst_realm_buf[REALM_SZ], *dest_realm = NULL;
int dst_realm_sz = REALM_SZ;

static int
kerberos4_send(char *name, Authenticator *ap)
{
    KTEXT_ST auth;
    char instance[INST_SZ];
    char *realm;
    CREDENTIALS cred;
    int r;

    if (!UserNameRequested) {
	if (auth_debug_mode) {
	    printf("Kerberos V4: no user name supplied\r\n");
	}
	return(0);
    }

    memset(instance, 0, sizeof(instance));

    strlcpy (instance,
		     krb_get_phost(RemoteHostName),
		     INST_SZ);

    realm = dest_realm ? dest_realm : krb_realmofhost(RemoteHostName);

    if (!realm) {
	printf("Kerberos V4: no realm for %s\r\n", RemoteHostName);
	return(0);
    }
    printf("[ Trying %s (%s.%s@%s) ... ]\r\n", name, 
	   KRB_SERVICE_NAME, instance, realm);
    r = krb_mk_req(&auth, KRB_SERVICE_NAME, instance, realm, 0L);
    if (r) {
	printf("mk_req failed: %s\r\n", krb_get_err_text(r));
	return(0);
    }
    r = krb_get_cred(KRB_SERVICE_NAME, instance, realm, &cred);
    if (r) {
	printf("get_cred failed: %s\r\n", krb_get_err_text(r));
	return(0);
    }
    if (!auth_sendname(UserNameRequested, strlen(UserNameRequested))) {
	if (auth_debug_mode)
	    printf("Not enough room for user name\r\n");
	return(0);
    }
    if (auth_debug_mode)
	printf("Sent %d bytes of authentication data\r\n", auth.length);
    if (!Data(ap, KRB_AUTH, (void *)auth.dat, auth.length)) {
	if (auth_debug_mode)
	    printf("Not enough room for authentication data\r\n");
	return(0);
    }
#ifdef ENCRYPTION
    /* create challenge */
    if ((ap->way & AUTH_HOW_MASK)==AUTH_HOW_MUTUAL) {
	int i;

	des_key_sched(&cred.session, sched);
	memcpy (&cred_session, &cred.session, sizeof(cred_session));
	des_init_random_number_generator(&cred.session);
	des_new_random_key(&session_key);
	des_ecb_encrypt(&session_key, &session_key, sched, 0);
	des_ecb_encrypt(&session_key, &challenge, sched, 0);

	/*
	  old code
	  Some CERT Advisory thinks this is a bad thing...
	    
	  des_init_random_number_generator(&cred.session);
	  des_new_random_key(&challenge);
	  des_ecb_encrypt(&challenge, &session_key, sched, 1);
	  */
	  
	/*
	 * Increment the challenge by 1, and encrypt it for
	 * later comparison.
	 */
	for (i = 7; i >= 0; --i) 
	    if(++challenge[i] != 0) /* No carry! */
		break;
	des_ecb_encrypt(&challenge, &challenge, sched, 1);
    }

#endif

    if (auth_debug_mode) {
	printf("CK: %d:", kerberos4_cksum(auth.dat, auth.length));
	printd(auth.dat, auth.length);
	printf("\r\n");
	printf("Sent Kerberos V4 credentials to server\r\n");
    }
    return(1);
}
int
kerberos4_send_mutual(Authenticator *ap)
{
    return kerberos4_send("mutual KERBEROS4", ap);
}

int
kerberos4_send_oneway(Authenticator *ap)
{
    return kerberos4_send("KERBEROS4", ap);
}

void
kerberos4_is(Authenticator *ap, unsigned char *data, int cnt)
{
    struct sockaddr_in addr;
    char realm[REALM_SZ];
    char instance[INST_SZ];
    int r;
    socklen_t addr_len;

    if (cnt-- < 1)
	return;
    switch (*data++) {
    case KRB_AUTH:
	if (krb_get_lrealm(realm, 1) != KSUCCESS) {
	    Data(ap, KRB_REJECT, (void *)"No local V4 Realm.", -1);
	    auth_finished(ap, AUTH_REJECT);
	    if (auth_debug_mode)
		printf("No local realm\r\n");
	    return;
	}
	memmove(auth.dat, data, auth.length = cnt);
	if (auth_debug_mode) {
	    printf("Got %d bytes of authentication data\r\n", cnt);
	    printf("CK: %d:", kerberos4_cksum(auth.dat, auth.length));
	    printd(auth.dat, auth.length);
	    printf("\r\n");
	}
	k_getsockinst(0, instance, sizeof(instance));
	addr_len = sizeof(addr);
	if(getpeername(0, (struct sockaddr *)&addr, &addr_len) < 0) {
	    if(auth_debug_mode)
		printf("getpeername failed\r\n");
	    Data(ap, KRB_REJECT, "getpeername failed", -1);
	    auth_finished(ap, AUTH_REJECT);
	    return;
	}
	if (addr.sin_family != AF_INET) {
	    if (auth_debug_mode)
		printf("unknown address family: %d\r\n", addr.sin_family);
	    Data(ap, KRB_REJECT, "bad address family", -1);
	    auth_finished(ap, AUTH_REJECT);
	    return;
	}

	r = krb_rd_req(&auth, KRB_SERVICE_NAME,
		       instance, addr.sin_addr.s_addr, &adat, "");
	if (r) {
	    if (auth_debug_mode)
		printf("Kerberos failed him as %s\r\n", name);
	    Data(ap, KRB_REJECT, (void *)krb_get_err_text(r), -1);
	    auth_finished(ap, AUTH_REJECT);
	    return;
	}
	/* save the session key */
	memmove(session_key, adat.session, sizeof(adat.session));
	krb_kntoln(&adat, name);

	if (UserNameRequested && !kuserok(&adat, UserNameRequested)){
	    char ts[MaxPathLen];
	    struct passwd *pw = getpwnam(UserNameRequested);

	    if(pw){
		snprintf(ts, sizeof(ts),
			 "%s%u",
			 TKT_ROOT,
			 (unsigned)pw->pw_uid);
		esetenv("KRBTKFILE", ts, 1);

		if (pw->pw_uid == 0)
		    syslog(LOG_INFO|LOG_AUTH,
			   "ROOT Kerberos login from %s on %s\n",
			   krb_unparse_name_long(adat.pname,
						 adat.pinst,
						 adat.prealm),
			   RemoteHostName);
	    }
	    Data(ap, KRB_ACCEPT, NULL, 0);
	} else {
	    char *msg;

	    asprintf (&msg, "user `%s' is not authorized to "
		      "login as `%s'", 
		      krb_unparse_name_long(adat.pname, 
					    adat.pinst, 
					    adat.prealm), 
		      UserNameRequested ? UserNameRequested : "<nobody>");
	    if (msg == NULL)
		Data(ap, KRB_REJECT, NULL, 0);
	    else {
		Data(ap, KRB_REJECT, (void *)msg, -1);
		free(msg);
	    }
	    auth_finished(ap, AUTH_REJECT);
	    break;
	}
	auth_finished(ap, AUTH_USER);
	break;
	
    case KRB_CHALLENGE:
#ifndef ENCRYPTION
	Data(ap, KRB_RESPONSE, NULL, 0);
#else
	if(!VALIDKEY(session_key)){
	    Data(ap, KRB_RESPONSE, NULL, 0);
	    break;
	}
	des_key_sched(&session_key, sched);
	{
	    des_cblock d_block;
	    int i;
	    Session_Key skey;

	    memmove(d_block, data, sizeof(d_block));

	    /* make a session key for encryption */
	    des_ecb_encrypt(&d_block, &session_key, sched, 1);
	    skey.type=SK_DES;
	    skey.length=8;
	    skey.data=session_key;
	    encrypt_session_key(&skey, 1);

	    /* decrypt challenge, add one and encrypt it */
	    des_ecb_encrypt(&d_block, &challenge, sched, 0);
	    for (i = 7; i >= 0; i--)
		if(++challenge[i] != 0)
		    break;
	    des_ecb_encrypt(&challenge, &challenge, sched, 1);
	    Data(ap, KRB_RESPONSE, (void *)challenge, sizeof(challenge));
	}
#endif
	break;

    case KRB_FORWARD:
	{
	    des_key_schedule ks;
	    unsigned char netcred[sizeof(CREDENTIALS)];
	    CREDENTIALS cred;
	    int ret;
	    if(cnt > sizeof(cred))
		abort();

	    memcpy (session_key, adat.session, sizeof(session_key));
	    des_set_key(&session_key, ks);
	    des_pcbc_encrypt((void*)data, (void*)netcred, cnt, 
			     ks, &session_key, DES_DECRYPT);
	    unpack_cred(netcred, cnt, &cred);
	    {
		if(strcmp(cred.service, KRB_TICKET_GRANTING_TICKET) ||
		   strncmp(cred.instance, cred.realm, sizeof(cred.instance)) ||
		   cred.lifetime < 0 || cred.lifetime > 255 ||
		   cred.kvno < 0 || cred.kvno > 255 ||
		   cred.issue_date < 0 || 
		   cred.issue_date > time(0) + CLOCK_SKEW ||
		   strncmp(cred.pname, adat.pname, sizeof(cred.pname)) ||
		   strncmp(cred.pinst, adat.pinst, sizeof(cred.pinst))){
		    Data(ap, KRB_FORWARD_REJECT, "Bad credentials", -1);
		}else{
		    if((ret = tf_setup(&cred,
				       cred.pname,
				       cred.pinst)) == KSUCCESS){
		        struct passwd *pw = getpwnam(UserNameRequested);

			if (pw)
			  chown(tkt_string(), pw->pw_uid, pw->pw_gid);
			Data(ap, KRB_FORWARD_ACCEPT, 0, 0);
		    } else{
			Data(ap, KRB_FORWARD_REJECT, 
			     krb_get_err_text(ret), -1);
		    }
		}
	    }
	    memset(data, 0, cnt);
	    memset(ks, 0, sizeof(ks));
	    memset(&cred, 0, sizeof(cred));
	}
	
	break;

    default:
	if (auth_debug_mode)
	    printf("Unknown Kerberos option %d\r\n", data[-1]);
	Data(ap, KRB_REJECT, 0, 0);
	break;
    }
}

void
kerberos4_reply(Authenticator *ap, unsigned char *data, int cnt)
{
    Session_Key skey;

    if (cnt-- < 1)
	return;
    switch (*data++) {
    case KRB_REJECT:
	if(auth_done){ /* XXX Ick! */
	    printf("[ Kerberos V4 received unknown opcode ]\r\n");
	}else{
	    printf("[ Kerberos V4 refuses authentication ");
	    if (cnt > 0) 
		printf("because %.*s ", cnt, data);
	    printf("]\r\n");
	    auth_send_retry();
	}
	return;
    case KRB_ACCEPT:
	printf("[ Kerberos V4 accepts you ]\r\n");
	auth_done = 1;
	if ((ap->way & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL) {
	    /*
	     * Send over the encrypted challenge.
	     */
	    Data(ap, KRB_CHALLENGE, session_key, 
		 sizeof(session_key));
	    des_ecb_encrypt(&session_key, &session_key, sched, 1);
	    skey.type = SK_DES;
	    skey.length = 8;
	    skey.data = session_key;
	    encrypt_session_key(&skey, 0);
#if 0
	    kerberos4_forward(ap, &cred_session);
#endif
	    return;
	}
	auth_finished(ap, AUTH_USER);
	return;
    case KRB_RESPONSE:
	/* make sure the response is correct */
	if ((cnt != sizeof(des_cblock)) ||
	    (memcmp(data, challenge, sizeof(challenge)))){
	    printf("[ Kerberos V4 challenge failed!!! ]\r\n");
	    auth_send_retry();
	    return;
	}
	printf("[ Kerberos V4 challenge successful ]\r\n");
	auth_finished(ap, AUTH_USER);
	break;
    case KRB_FORWARD_ACCEPT:
	printf("[ Kerberos V4 accepted forwarded credentials ]\r\n");
	break;
    case KRB_FORWARD_REJECT:
	printf("[ Kerberos V4 rejected forwarded credentials: `%.*s']\r\n",
	       cnt, data);
	break;
    default:
	if (auth_debug_mode)
	    printf("Unknown Kerberos option %d\r\n", data[-1]);
	return;
    }
}

int
kerberos4_status(Authenticator *ap, char *name, size_t name_sz, int level)
{
    if (level < AUTH_USER)
	return(level);

    if (UserNameRequested && !kuserok(&adat, UserNameRequested)) {
	strlcpy(name, UserNameRequested, name_sz);
	return(AUTH_VALID);
    } else
	return(AUTH_USER);
}

#define	BUMP(buf, len)		while (*(buf)) {++(buf), --(len);}
#define	ADDC(buf, len, c)	if ((len) > 0) {*(buf)++ = (c); --(len);}

void
kerberos4_printsub(unsigned char *data, int cnt, unsigned char *buf, int buflen)
{
    int i;

    buf[buflen-1] = '\0';		/* make sure its NULL terminated */
    buflen -= 1;

    switch(data[3]) {
    case KRB_REJECT:		/* Rejected (reason might follow) */
	strlcpy((char *)buf, " REJECT ", buflen);
	goto common;

    case KRB_ACCEPT:		/* Accepted (name might follow) */
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

    case KRB_AUTH:			/* Authentication data follows */
	strlcpy((char *)buf, " AUTH", buflen);
	goto common2;

    case KRB_CHALLENGE:
	strlcpy((char *)buf, " CHALLENGE", buflen);
	goto common2;

    case KRB_RESPONSE:
	strlcpy((char *)buf, " RESPONSE", buflen);
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

int
kerberos4_cksum(unsigned char *d, int n)
{
    int ck = 0;

    /*
     * A comment is probably needed here for those not
     * well versed in the "C" language.  Yes, this is
     * supposed to be a "switch" with the body of the
     * "switch" being a "while" statement.  The whole
     * purpose of the switch is to allow us to jump into
     * the middle of the while() loop, and then not have
     * to do any more switch()s.
     *
     * Some compilers will spit out a warning message
     * about the loop not being entered at the top.
     */
    switch (n&03)
	while (n > 0) {
	case 0:
	    ck ^= (int)*d++ << 24;
	    --n;
	case 3:
	    ck ^= (int)*d++ << 16;
	    --n;
	case 2:
	    ck ^= (int)*d++ << 8;
	    --n;
	case 1:
	    ck ^= (int)*d++;
	    --n;
	}
    return(ck);
}

static int
pack_cred(CREDENTIALS *cred, unsigned char *buf)
{
    unsigned char *p = buf;
    
    memcpy (p, cred->service, ANAME_SZ);
    p += ANAME_SZ;
    memcpy (p, cred->instance, INST_SZ);
    p += INST_SZ;
    memcpy (p, cred->realm, REALM_SZ);
    p += REALM_SZ;
    memcpy(p, cred->session, 8);
    p += 8;
    p += KRB_PUT_INT(cred->lifetime, p, 4, 4);
    p += KRB_PUT_INT(cred->kvno, p, 4, 4);
    p += KRB_PUT_INT(cred->ticket_st.length, p, 4, 4);
    memcpy(p, cred->ticket_st.dat, cred->ticket_st.length);
    p += cred->ticket_st.length;
    p += KRB_PUT_INT(0, p, 4, 4);
    p += KRB_PUT_INT(cred->issue_date, p, 4, 4);
    memcpy (p, cred->pname, ANAME_SZ);
    p += ANAME_SZ;
    memcpy (p, cred->pinst, INST_SZ);
    p += INST_SZ;
    return p - buf;
}

static int
unpack_cred(unsigned char *buf, int len, CREDENTIALS *cred)
{
    unsigned char *p = buf;
    u_int32_t tmp;

    strncpy (cred->service, p, ANAME_SZ);
    cred->service[ANAME_SZ - 1] = '\0';
    p += ANAME_SZ;
    strncpy (cred->instance, p, INST_SZ);
    cred->instance[INST_SZ - 1] = '\0';
    p += INST_SZ;
    strncpy (cred->realm, p, REALM_SZ);
    cred->realm[REALM_SZ - 1] = '\0';
    p += REALM_SZ;

    memcpy(cred->session, p, 8);
    p += 8;
    p += krb_get_int(p, &tmp, 4, 0);
    cred->lifetime = tmp;
    p += krb_get_int(p, &tmp, 4, 0);
    cred->kvno = tmp;

    p += krb_get_int(p, &cred->ticket_st.length, 4, 0);
    memcpy(cred->ticket_st.dat, p, cred->ticket_st.length);
    p += cred->ticket_st.length;
    p += krb_get_int(p, &tmp, 4, 0);
    cred->ticket_st.mbz = 0;
    p += krb_get_int(p, (u_int32_t *)&cred->issue_date, 4, 0);

    strncpy (cred->pname, p, ANAME_SZ);
    cred->pname[ANAME_SZ - 1] = '\0';
    p += ANAME_SZ;
    strncpy (cred->pinst, p, INST_SZ);
    cred->pinst[INST_SZ - 1] = '\0';
    p += INST_SZ;
    return 0;
}


int
kerberos4_forward(Authenticator *ap, void *v)
{
    des_cblock *key = (des_cblock *)v;
    CREDENTIALS cred;
    char *realm;
    des_key_schedule ks;
    int len;
    unsigned char netcred[sizeof(CREDENTIALS)];
    int ret;

    realm = krb_realmofhost(RemoteHostName);
    if(realm == NULL)
	return -1;
    memset(&cred, 0, sizeof(cred));
    ret = krb_get_cred(KRB_TICKET_GRANTING_TICKET,
		       realm,
		       realm, 
		       &cred);
    if(ret)
	return ret;
    des_set_key(key, ks);
    len = pack_cred(&cred, netcred);
    des_pcbc_encrypt((void*)netcred, (void*)netcred, len,
		     ks, key, DES_ENCRYPT);
    memset(ks, 0, sizeof(ks));
    Data(ap, KRB_FORWARD, netcred, len);
    memset(netcred, 0, sizeof(netcred));
    return 0;
}

#endif /* KRB4 */

