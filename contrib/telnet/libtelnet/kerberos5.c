/*
 *	$Source: /mit/krb5/.cvsroot/src/appl/telnet/libtelnet/kerberos5.c,v $
 *	$Author: tytso $
 *	$Id: kerberos5.c,v 1.1 1997/09/04 06:11:15 markm Exp $
 */

#if !defined(lint) && !defined(SABER)
static
#ifdef __STDC__
const
#endif
char rcsid_kerberos5_c[] = "$Id: kerberos5.c,v 1.1 1997/09/04 06:11:15 markm Exp $";
#endif /* lint */

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

#ifndef lint
static char sccsid[] = "@(#)kerberos5.c	8.3 (Berkeley) 5/30/95";
#endif /* not lint */

/*
 * Copyright (C) 1990 by the Massachusetts Institute of Technology
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
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


#ifdef	KRB5
#include <arpa/telnet.h>
#include <stdio.h>
#include <krb5/krb5.h>
#include <krb5/asn1.h>
#include <krb5/crc-32.h>
#include <krb5/los-proto.h>
#include <krb5/ext-proto.h>
#include <com_err.h>
#include <netdb.h>
#include <ctype.h>

/* kerberos 5 include files (ext-proto.h) will get an appropriate stdlib.h
   and string.h/strings.h */

#include "encrypt.h"
#include "auth.h"
#include "misc.h"

extern auth_debug_mode;

#ifdef	FORWARD
int forward_flags = 0;  /* Flags get set in telnet/main.c on -f and -F */

/* These values need to be the same as those defined in telnet/main.c. */
/* Either define them in both places, or put in some common header file. */
#define OPTS_FORWARD_CREDS	0x00000002
#define OPTS_FORWARDABLE_CREDS	0x00000001

void kerberos5_forward();

#endif	/* FORWARD */

static unsigned char str_data[1024] = { IAC, SB, TELOPT_AUTHENTICATION, 0,
			  		AUTHTYPE_KERBEROS_V5, };
/*static unsigned char str_name[1024] = { IAC, SB, TELOPT_AUTHENTICATION,
					TELQUAL_NAME, };*/

#define	KRB_AUTH		0	/* Authentication data follows */
#define	KRB_REJECT		1	/* Rejected (reason might follow) */
#define	KRB_ACCEPT		2	/* Accepted */
#define	KRB_RESPONSE		3	/* Response for mutual auth. */

#ifdef	FORWARD
#define KRB_FORWARD     	4       /* Forwarded credentials follow */
#define KRB_FORWARD_ACCEPT     	5       /* Forwarded credentials accepted */
#define KRB_FORWARD_REJECT     	6       /* Forwarded credentials rejected */
#endif	/* FORWARD */

static	krb5_data auth;
	/* telnetd gets session key from here */
static	krb5_tkt_authent *authdat = NULL;
/* telnet matches the AP_REQ and AP_REP with this */
static	krb5_authenticator authenticator;

/* some compilers can't hack void *, so we use the Kerberos krb5_pointer,
   which is either void * or char *, depending on the compiler. */

#define Voidptr krb5_pointer

Block	session_key;

	static int
Data(ap, type, d, c)
	Authenticator *ap;
	int type;
	Voidptr d;
	int c;
{
	unsigned char *p = str_data + 4;
	unsigned char *cd = (unsigned char *)d;

	if (c == -1)
		c = strlen((char *)cd);

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
		printsub('>', &str_data[2], p - &str_data[2]);
	return(net_write(str_data, p - str_data));
}

	int
kerberos5_init(ap, server)
	Authenticator *ap;
	int server;
{
	if (server)
		str_data[3] = TELQUAL_REPLY;
	else
		str_data[3] = TELQUAL_IS;
	krb5_init_ets();
	return(1);
}

	int
kerberos5_send(ap)
	Authenticator *ap;
{
	char **realms;
	char *name;
	char *p1, *p2;
	krb5_checksum ksum;
	krb5_octet sum[CRC32_CKSUM_LENGTH];
 	krb5_principal server;
	krb5_error_code r;
	krb5_ccache ccache;
	krb5_creds creds;		/* telnet gets session key from here */
	extern krb5_flags krb5_kdc_default_options;
	int ap_opts;

#ifdef	ENCRYPTION
	krb5_keyblock *newkey = 0;
#endif	/* ENCRYPTION */

	ksum.checksum_type = CKSUMTYPE_CRC32;
	ksum.contents = sum;
	ksum.length = sizeof(sum);
	memset((Voidptr )sum, 0, sizeof(sum));

	if (!UserNameRequested) {
		if (auth_debug_mode) {
			printf("Kerberos V5: no user name supplied\r\n");
		}
		return(0);
	}

	if (r = krb5_cc_default(&ccache)) {
		if (auth_debug_mode) {
			printf("Kerberos V5: could not get default ccache\r\n");
		}
		return(0);
	}

	if ((name = malloc(strlen(RemoteHostName)+1)) == NULL) {
		if (auth_debug_mode)
			printf("Out of memory for hostname in Kerberos V5\r\n");
		return(0);
	}

	if (r = krb5_get_host_realm(RemoteHostName, &realms)) {
		if (auth_debug_mode)
			printf("Kerberos V5: no realm for %s\r\n", RemoteHostName);
		free(name);
		return(0);
	}

	p1 = RemoteHostName;
	p2 = name;

	while (*p2 = *p1++) {
		if (isupper(*p2))
			*p2 |= 040;
		++p2;
	}

	if (r = krb5_build_principal_ext(&server,
					 strlen(realms[0]), realms[0],
					 4, "host",
					 p2 - name, name,
					 0)) {
		if (auth_debug_mode) {
			printf("Kerberos V5: failure setting up principal (%s)\r\n",
			       error_message(r));
		}
		free(name);
		krb5_free_host_realm(realms);
		return(0);
	}


	memset((char *)&creds, 0, sizeof(creds));
	creds.server = server;

	if (r = krb5_cc_get_principal(ccache, &creds.client)) {
		if (auth_debug_mode) {
			printf("Kerberos V5: failure on principal (%s)\r\n",
				error_message(r));
		}
		free(name);
		krb5_free_principal(server);
		krb5_free_host_realm(realms);
		return(0);
	}

	if (r = krb5_get_credentials(krb5_kdc_default_options, ccache, &creds)) {
		if (auth_debug_mode) {
			printf("Kerberos V5: failure on credentials(%d)\r\n",r);
		}
		free(name);
		krb5_free_host_realm(realms);
		krb5_free_principal(server);
		return(0);
	}

	if ((ap->way & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL)
	    ap_opts = AP_OPTS_MUTUAL_REQUIRED;
	else
	    ap_opts = 0;

	r = krb5_mk_req_extended(ap_opts, &ksum, krb5_kdc_default_options, 0,
#ifdef	ENCRYPTION
				 &newkey,
#else	/* ENCRYPTION */
				 0,
#endif	/* ENCRYPTION */
				 ccache, &creds, &authenticator, &auth);
	/* don't let the key get freed if we clean up the authenticator */
	authenticator.subkey = 0;

	free(name);
	krb5_free_host_realm(realms);
	krb5_free_principal(server);
#ifdef	ENCRYPTION
	if (newkey) {
	    /* keep the key in our private storage, but don't use it
	       yet---see kerberos5_reply() below */
	    if (newkey->keytype != KEYTYPE_DES) {
		if (creds.keyblock.keytype == KEYTYPE_DES)
		    /* use the session key in credentials instead */
		    memmove((char *)session_key,
			   (char *)creds.keyblock.contents, sizeof(Block));
		else
		    /* XXX ? */;
	    } else {
		memmove((char *)session_key, (char *)newkey->contents,
		       sizeof(Block));
	    }
	    krb5_free_keyblock(newkey);
	}
#endif	/* ENCRYPTION */
	if (r) {
		if (auth_debug_mode) {
			printf("Kerberos V5: mk_req failed (%s)\r\n",
			       error_message(r));
		}
		return(0);
	}

	if (!auth_sendname(UserNameRequested, strlen(UserNameRequested))) {
		if (auth_debug_mode)
			printf("Not enough room for user name\r\n");
		return(0);
	}
	if (!Data(ap, KRB_AUTH, auth.data, auth.length)) {
		if (auth_debug_mode)
			printf("Not enough room for authentication data\r\n");
		return(0);
	}
	if (auth_debug_mode) {
		printf("Sent Kerberos V5 credentials to server\r\n");
	}
	return(1);
}

	void
kerberos5_is(ap, data, cnt)
	Authenticator *ap;
	unsigned char *data;
	int cnt;
{
	int r;
	struct hostent *hp;
	char *p1, *p2;
	static char *realm = NULL;
	krb5_principal server;
	krb5_ap_rep_enc_part reply;
	krb5_data outbuf;
#ifdef ENCRYPTION
	Session_Key skey;
#endif	/* ENCRYPTION */
	char *name;
	char *getenv();
	krb5_data inbuf;

	if (cnt-- < 1)
		return;
	switch (*data++) {
	case KRB_AUTH:
		auth.data = (char *)data;
		auth.length = cnt;

		if (!(hp = gethostbyname(LocalHostName))) {
			if (auth_debug_mode)
				printf("Cannot resolve local host name\r\n");
			Data(ap, KRB_REJECT, "Unknown local hostname.", -1);
			auth_finished(ap, AUTH_REJECT);
			return;
		}

		if (!realm && (krb5_get_default_realm(&realm))) {
			if (auth_debug_mode)
				printf("Could not get default realm\r\n");
			Data(ap, KRB_REJECT, "Could not get default realm.", -1);
			auth_finished(ap, AUTH_REJECT);
			return;
		}

		if ((name = malloc(strlen(hp->h_name)+1)) == NULL) {
			if (auth_debug_mode)
				printf("Out of memory for hostname in Kerberos V5\r\n");
			Data(ap, KRB_REJECT, "Out of memory.", -1);
			auth_finished(ap, AUTH_REJECT);
			return;
		}

		p1 = hp->h_name;
		p2 = name;

		while (*p2 = *p1++) {
			if (isupper(*p2))
				*p2 |= 040;
			++p2;
		}

		if (authdat)
			krb5_free_tkt_authent(authdat);

		r = krb5_build_principal_ext(&server,
					     strlen(realm), realm,
					     4, "host",
					     p2 - name, name,
					     0);
		if (!r) {
		    r = krb5_rd_req_simple(&auth, server, 0, &authdat);
		    krb5_free_principal(server);
		}
		if (r) {
			char errbuf[128];

		    errout:
			authdat = 0;
			(void) strcpy(errbuf, "Read req failed: ");
			(void) strcat(errbuf, error_message(r));
			Data(ap, KRB_REJECT, errbuf, -1);
			if (auth_debug_mode)
				printf("%s\r\n", errbuf);
			return;
		}
		free(name);
		if ((ap->way & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL) {
		    /* do ap_rep stuff here */
		    reply.ctime = authdat->authenticator->ctime;
		    reply.cusec = authdat->authenticator->cusec;
		    reply.subkey = 0;	/* use the one he gave us, so don't
					   need to return one here */
		    reply.seq_number = 0; /* we don't do seq #'s. */

		    if (r = krb5_mk_rep(&reply,
					authdat->authenticator->subkey ?
					authdat->authenticator->subkey :
					authdat->ticket->enc_part2->session,
					&outbuf)) {
			goto errout;
		    }
		    Data(ap, KRB_RESPONSE, outbuf.data, outbuf.length);
		}
		if (krb5_unparse_name(authdat->ticket->enc_part2 ->client,
				      					&name))
			name = 0;
		Data(ap, KRB_ACCEPT, name, name ? -1 : 0);
		if (auth_debug_mode) {
			printf("Kerberos5 identifies him as ``%s''\r\n",
							name ? name : "");
		}
		auth_finished(ap, AUTH_USER);

		free(name);
	    	if (authdat->authenticator->subkey &&
		    authdat->authenticator->subkey->keytype == KEYTYPE_DES) {
		    memmove((Voidptr )session_key,
			   (Voidptr )authdat->authenticator->subkey->contents,
			   sizeof(Block));
		} else if (authdat->ticket->enc_part2->session->keytype ==
			   KEYTYPE_DES) {
		    memmove((Voidptr )session_key,
			(Voidptr )authdat->ticket->enc_part2->session->contents,
			sizeof(Block));
		} else
		    break;

#ifdef ENCRYPTION
		skey.type = SK_DES;
		skey.length = 8;
		skey.data = session_key;
		encrypt_session_key(&skey, 1);
#endif	/* ENCRYPTION */
		break;
#ifdef	FORWARD
	case KRB_FORWARD:
		inbuf.data = (char *)data;
		inbuf.length = cnt;
		if (r = rd_and_store_for_creds(&inbuf, authdat->ticket,
					       UserNameRequested)) {
		    char errbuf[128];

		    (void) strcpy(errbuf, "Read forwarded creds failed: ");
		    (void) strcat(errbuf, error_message(r));
		    Data(ap, KRB_FORWARD_REJECT, errbuf, -1);
		    if (auth_debug_mode)
		      printf("Could not read forwarded credentials\r\n");
		}
		else
		  Data(ap, KRB_FORWARD_ACCEPT, 0, 0);
		  if (auth_debug_mode)
		    printf("Forwarded credentials obtained\r\n");
		break;
#endif	/* FORWARD */
	default:
		if (auth_debug_mode)
			printf("Unknown Kerberos option %d\r\n", data[-1]);
		Data(ap, KRB_REJECT, 0, 0);
		break;
	}
}

	void
kerberos5_reply(ap, data, cnt)
	Authenticator *ap;
	unsigned char *data;
	int cnt;
{
	Session_Key skey;
	static int mutual_complete = 0;

	if (cnt-- < 1)
		return;
	switch (*data++) {
	case KRB_REJECT:
		if (cnt > 0) {
			printf("[ Kerberos V5 refuses authentication because %.*s ]\r\n",
				cnt, data);
		} else
			printf("[ Kerberos V5 refuses authentication ]\r\n");
		auth_send_retry();
		return;
	case KRB_ACCEPT:
		if ((ap->way & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL &&
		    !mutual_complete) {
		    printf("[ Kerberos V5 accepted you, but didn't provide mutual authentication! ]\n");
		    auth_send_retry();
		    return;
		}
		if (cnt)
		    printf("[ Kerberos V5 accepts you as ``%.*s'' ]\n", cnt, data);
		else
		    printf("[ Kerberos V5 accepts you ]\n");
		auth_finished(ap, AUTH_USER);
#ifdef	FORWARD
		if (forward_flags & OPTS_FORWARD_CREDS)
		  kerberos5_forward(ap);
#endif	/* FORWARD */
		break;
	case KRB_RESPONSE:
		if ((ap->way & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL) {
		    /* the rest of the reply should contain a krb_ap_rep */
		    krb5_ap_rep_enc_part *reply;
		    krb5_data inbuf;
		    krb5_error_code r;
		    krb5_keyblock tmpkey;

		    inbuf.length = cnt;
		    inbuf.data = (char *)data;

		    tmpkey.keytype = KEYTYPE_DES;
		    tmpkey.contents = session_key;
		    tmpkey.length = sizeof(Block);

		    if (r = krb5_rd_rep(&inbuf, &tmpkey, &reply)) {
			printf("[ Mutual authentication failed: %s ]\n",
			       error_message(r));
			auth_send_retry();
			return;
		    }
		    if (reply->ctime != authenticator.ctime ||
			reply->cusec != authenticator.cusec) {
			printf("[ Mutual authentication failed (mismatched KRB_AP_REP) ]\n");
			auth_send_retry();
			return;
		    }
		    krb5_free_ap_rep_enc_part(reply);
#ifdef	ENCRYPTION
			skey.type = SK_DES;
			skey.length = 8;
			skey.data = session_key;
			encrypt_session_key(&skey, 0);
#endif	/* ENCRYPTION */
		    mutual_complete = 1;
		}
		return;
#ifdef	FORWARD
	case KRB_FORWARD_ACCEPT:
		printf("[ Kerberos V5 accepted forwarded credentials ]\n");
		return;
	case KRB_FORWARD_REJECT:
		printf("[ Kerberos V5 refuses forwarded credentials because %.*s ]\r\n",
				cnt, data);
		return;
#endif	/* FORWARD */
	default:
		if (auth_debug_mode)
			printf("Unknown Kerberos option %d\r\n", data[-1]);
		return;
	}
}

	int
kerberos5_status(ap, name, level)
	Authenticator *ap;
	char *name;
	int level;
{
	if (level < AUTH_USER)
		return(level);

	if (UserNameRequested &&
	    krb5_kuserok(authdat->ticket->enc_part2->client, UserNameRequested))
	{
		strcpy(name, UserNameRequested);
		return(AUTH_VALID);
	} else
		return(AUTH_USER);
}

#define	BUMP(buf, len)		while (*(buf)) {++(buf), --(len);}
#define	ADDC(buf, len, c)	if ((len) > 0) {*(buf)++ = (c); --(len);}

	void
kerberos5_printsub(data, cnt, buf, buflen)
	unsigned char *data, *buf;
	int cnt, buflen;
{
	char lbuf[32];
	register int i;

	buf[buflen-1] = '\0';		/* make sure its NULL terminated */
	buflen -= 1;

	switch(data[3]) {
	case KRB_REJECT:		/* Rejected (reason might follow) */
		strncpy((char *)buf, " REJECT ", buflen);
		goto common;

	case KRB_ACCEPT:		/* Accepted (name might follow) */
		strncpy((char *)buf, " ACCEPT ", buflen);
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
		strncpy((char *)buf, " AUTH", buflen);
		goto common2;

	case KRB_RESPONSE:
		strncpy((char *)buf, " RESPONSE", buflen);
		goto common2;

#ifdef	FORWARD
	case KRB_FORWARD:		/* Forwarded credentials follow */
		strncpy((char *)buf, " FORWARD", buflen);
		goto common2;

	case KRB_FORWARD_ACCEPT:	/* Forwarded credentials accepted */
		strncpy((char *)buf, " FORWARD_ACCEPT", buflen);
		goto common2;

	case KRB_FORWARD_REJECT:	/* Forwarded credentials rejected */
					       /* (reason might follow) */
		strncpy((char *)buf, " FORWARD_REJECT", buflen);
		goto common2;
#endif	/* FORWARD */

	default:
		sprintf(lbuf, " %d (unknown)", data[3]);
		strncpy((char *)buf, lbuf, buflen);
	common2:
		BUMP(buf, buflen);
		for (i = 4; i < cnt; i++) {
			sprintf(lbuf, " %d", data[i]);
			strncpy((char *)buf, lbuf, buflen);
			BUMP(buf, buflen);
		}
		break;
	}
}

#ifdef	FORWARD
	void
kerberos5_forward(ap)
     Authenticator *ap;
{
    struct hostent *hp;
    krb5_creds *local_creds;
    krb5_error_code r;
    krb5_data forw_creds;
    extern krb5_cksumtype krb5_kdc_req_sumtype;
    krb5_ccache ccache;
    int i;

    if (!(local_creds = (krb5_creds *)
	  calloc(1, sizeof(*local_creds)))) {
	if (auth_debug_mode)
	  printf("Kerberos V5: could not allocate memory for credentials\r\n");
	return;
    }

    if (r = krb5_sname_to_principal(RemoteHostName, "host", 1,
				    &local_creds->server)) {
	if (auth_debug_mode)
	  printf("Kerberos V5: could not build server name - %s\r\n",
		 error_message(r));
	krb5_free_creds(local_creds);
	return;
    }

    if (r = krb5_cc_default(&ccache)) {
	if (auth_debug_mode)
	  printf("Kerberos V5: could not get default ccache - %s\r\n",
		 error_message(r));
	krb5_free_creds(local_creds);
	return;
    }

    if (r = krb5_cc_get_principal(ccache, &local_creds->client)) {
	if (auth_debug_mode)
	  printf("Kerberos V5: could not get default principal - %s\r\n",
		 error_message(r));
	krb5_free_creds(local_creds);
	return;
    }

    /* Get ticket from credentials cache */
    if (r = krb5_get_credentials(KRB5_GC_CACHED, ccache, local_creds)) {
	if (auth_debug_mode)
	  printf("Kerberos V5: could not obtain credentials - %s\r\n",
		 error_message(r));
	krb5_free_creds(local_creds);
	return;
    }

    if (r = get_for_creds(ETYPE_DES_CBC_CRC,
			  krb5_kdc_req_sumtype,
			  RemoteHostName,
			  local_creds->client,
			  &local_creds->keyblock,
			  forward_flags & OPTS_FORWARDABLE_CREDS,
			  &forw_creds)) {
	if (auth_debug_mode)
	  printf("Kerberos V5: error getting forwarded creds - %s\r\n",
		 error_message(r));
	krb5_free_creds(local_creds);
	return;
    }

    /* Send forwarded credentials */
    if (!Data(ap, KRB_FORWARD, forw_creds.data, forw_creds.length)) {
	if (auth_debug_mode)
	  printf("Not enough room for authentication data\r\n");
    }
    else {
	if (auth_debug_mode)
	  printf("Forwarded local Kerberos V5 credentials to server\r\n");
    }

    krb5_free_creds(local_creds);
}
#endif	/* FORWARD */

#endif /* KRB5 */
