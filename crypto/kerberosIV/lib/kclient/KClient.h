/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* KClient.h - KClient glue to krb4.dll
 * Author: Jörgen Karlsson - d93-jka@nada.kth.se
 * Date: June 1996
 */

/* $Id: KClient.h,v 1.8 1999/12/02 16:58:40 joda Exp $ */

#ifndef	KCLIENT_H
#define	KCLIENT_H

#ifdef MacOS
#include <Types.h>
typedef OSerr Kerr;
#endif /* MacOS */

#ifdef WIN32	/* Visual C++ 4.0 (Windows95/NT) */
typedef int Kerr;
#endif /* WIN32 */

enum { KClientLoggedIn, KClientNotLoggedIn };

struct _KClientKey
{
    unsigned char keyBytes[8];
};
typedef struct _KClientKey KClientKey;

struct _KClientSessionInfo
{
    unsigned long lAddr;
    unsigned short lPort;
    unsigned long fAddr;
    unsigned short fPort;
    char user[32];
    char inst[32];
    char realm[32];
    char key[8];
};
typedef struct _KClientSessionInfo KClientSessionInfo;

#ifdef __cplusplus
extern "C" {
#endif

Kerr KClientMessage(char *text, Kerr error);

/* KClientInitSession */
Kerr KClientInitSession(KClientSessionInfo *session,
			unsigned long lAddr,
			unsigned short lPort,
			unsigned long fAddr,
			unsigned short fPort);

/* KClientGetTicketForService */
Kerr KClientGetTicketForService(KClientSessionInfo *session,
				char *service,
				void *buf,
				unsigned long *buflen);


/* KClientLogin	*/
Kerr KClientLogin(KClientSessionInfo *session,
		  KClientKey *privateKey );

/* KClientPasswordLogin */
Kerr KClientPasswordLogin(KClientSessionInfo *session,
			  char *password,
			  KClientKey *privateKey);

/* KClientKeyLogin */
Kerr KClientKeyLogin(KClientSessionInfo *session, KClientKey *privateKey);

/* KClientLogout */
Kerr KClientLogout(void);

/* KClientStatus */
short KClientStatus(KClientSessionInfo *session);

/* KClientGetUserName */
Kerr KClientGetUserName(char *user);

/* KClientSetUserName */
Kerr KClientSetUserName(char *user);

/* KClientCacheInitialTicket */
Kerr KClientCacheInitialTicket(KClientSessionInfo *session,
			       char *service);

/* KClientGetSessionKey */
Kerr KClientGetSessionKey(KClientSessionInfo *session,
			  KClientKey *sessionKey);

/* KClientMakeSendAuth */
Kerr KClientMakeSendAuth(KClientSessionInfo *session,
			 char *service,
			 void *buf,
			 unsigned long *buflen,
			 long checksum,
			 char *applicationVersion);

/* KClientVerifySendAuth */
Kerr KClientVerifySendAuth(KClientSessionInfo *session,
			   void *buf,
			   unsigned long *buflen);

/* KClientEncrypt */
Kerr KClientEncrypt(KClientSessionInfo *session,
		    void *buf,
		    unsigned long buflen,
		    void *encryptBuf,
		    unsigned long *encryptLength);

/* KClientDecrypt */
Kerr KClientDecrypt(KClientSessionInfo *session,
		    void *buf,
		    unsigned long buflen,
		    unsigned long *decryptOffset,
		    unsigned long *decryptLength);

/* KClientErrorText */
char *KClientErrorText(Kerr err, char *text);

#ifdef __cplusplus
}
#endif

#endif /* KCLIENT_H */
