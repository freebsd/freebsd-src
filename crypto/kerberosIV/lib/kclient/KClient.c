/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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

/* KClient.c - KClient glue to krb4.dll
 * Author: Jörgen Karlsson - d93-jka@nada.kth.se
 * Date: June 1996
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: KClient.c,v 1.14 1999/12/02 16:58:40 joda Exp $");
#endif

#ifdef WIN32	/* Visual C++ 4.0 (Windows95/NT) */
#include <Windows.h>
#endif /* WIN32 */

//#include <string.h>
#include <winsock.h>
#include "passwd_dlg.h"
#include "KClient.h"
#include "krb.h"

char guser[64];

void
msg(char *text)
{
    HWND wnd = GetActiveWindow();
    MessageBox(wnd, text, "KClient message", MB_OK|MB_APPLMODAL);
}
 
BOOL
SendTicketForService(LPSTR service, LPSTR version, int fd)
{
    KTEXT_ST ticket;
    MSG_DAT mdat;
    CREDENTIALS cred;
    des_key_schedule schedule;
    char name[SNAME_SZ], inst[INST_SZ], realm[REALM_SZ];
    int ret;
    static KClientSessionInfo foo;
    KClientKey key;

    kname_parse(name, inst, realm, service);
    strlcpy(foo.realm, realm, sizeof(foo.realm));

    if(KClientStatus(&foo) == KClientNotLoggedIn)
	KClientLogin(&foo, &key);

    ret = krb_sendauth (0, fd, &ticket,
			name, inst, realm, 17, &mdat,
			&cred, &schedule, NULL, NULL, version);
    if(ret)
	return FALSE;
    return TRUE;
}

BOOL WINAPI
DllMain(HANDLE hInst, ULONG reason, LPVOID lpReserved)
{
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

    switch(reason){
    case DLL_PROCESS_ATTACH:
	wVersionRequested = MAKEWORD(1, 1);

	err = WSAStartup(wVersionRequested, &wsaData);

	if (err != 0)
	{
	    /* Tell the user that we couldn't find a useable */
	    /* winsock.dll.     */
	    msg("Cannot find winsock.dll");
	    return FALSE;
	}
	break;
    case DLL_PROCESS_DETACH:
	WSACleanup();
    }

    return TRUE;
}

Kerr
KClientMessage(char *text, Kerr error)
{
    msg(text);
    return error;
}

/* KClientInitSession
 * You need to call this routine before calling most other routines.
 * It initializes a KClientSessionInfo structure.
 * The local and remote addresses are for use in KClientEncrypt,
 * KClientDecrypt, KClientMakeSendAuth and KClientVerifySendAuth.
 * If you don't use any of these routines it's perfectly OK to do the following...
 * err = KClientInitSession(session,0,0,0,0);
 */
Kerr
KClientInitSession(KClientSessionInfo *session,
		   unsigned long lAddr,
		   unsigned short lPort,
		   unsigned long fAddr,
		   unsigned short fPort)
{
    session->lAddr = lAddr;
    session->lPort = lPort;
    session->fAddr = fAddr;
    session->fPort = fPort;
    if(tf_get_pname(session->user) != KSUCCESS)
	*(session->user) = '\0';
    if(tf_get_pinst(session->inst) != KSUCCESS)
	*(session->inst) = '\0';
    krb_get_lrealm (session->realm, 1);
    if(*(session->user))
	strlcpy(guser, session->user, sizeof(guser));
    else
	*guser ='\0';
    
    return 0;
}


/* KClientGetTicketForService
 * This routine gets an authenticator to be passed to a service.
 * If the user isn't already logged in the user is prompted for a password.
 */
Kerr
KClientGetTicketForService(KClientSessionInfo *session,
			   char *service,
			   void *buf,
			   unsigned long *buflen)
{
    CREDENTIALS c;
    KClientKey k;
    KTEXT_ST ticket;
    char serv[255], inst[255], realm[255];
    Kerr err;

    //	KClientSetUserName(session->user);
    err = kname_parse(serv,inst,realm,service);
    if(*realm)
	strlcpy(session->realm, realm, sizeof(session->realm));
    else
	strlcpy(realm, session->realm, sizeof(realm));
    if(KClientStatus(session) == KClientNotLoggedIn)
	if((err = KClientLogin(session, &k)) != KSUCCESS)
	    return err;

    if((err = krb_mk_req(&ticket, serv, inst, realm, 0)) != KSUCCESS)
	return KClientMessage(KClientErrorText(err,0),err);
    if((err = krb_get_cred(serv, inst, realm, &c)) != KSUCCESS)
	return KClientMessage(KClientErrorText(err,0),err);

    if(*buflen >= ticket.length)
    {
	*buflen = ticket.length + sizeof(unsigned long);
	CopyMemory(buf, &ticket, *buflen);
	CopyMemory(session->key, c.session, sizeof(session->key));
    }
    else
	err = -1;
    return err;
}


/* KClientLogin
 * This routine "logs in" by getting a ticket granting ticket from kerberos.
 * It returns the user's private key which can be used to automate login at
 * a later time with KClientKeyLogin.
 */

Kerr
KClientLogin(KClientSessionInfo *session,
	     KClientKey *privateKey)
{
    CREDENTIALS c;
    Kerr err;
    char passwd[100];
	
    if((err = pwd_dialog(guser, passwd)))
	return err;
    if(KClientStatus(session) == KClientNotLoggedIn)
    {

	if((err = krb_get_pw_in_tkt(guser, session->inst, session->realm,
				    "krbtgt", session->realm,
				    DEFAULT_TKT_LIFE, passwd)) != KSUCCESS)
	    return KClientMessage(KClientErrorText(err,0),err);
    }
    if((err = krb_get_cred("krbtgt", session->realm,
			   session->realm, &c)) == KSUCCESS)
	CopyMemory(privateKey, c.session, sizeof(*privateKey));
    return err;
}


/* KClientPasswordLogin
 * This routine is similiar to KClientLogin but instead of prompting the user
 * for a password it uses the password supplied to establish login.
 */
Kerr
KClientPasswordLogin(KClientSessionInfo *session,
		     char *password,
		     KClientKey *privateKey)
{
    return krb_get_pw_in_tkt(guser, session->inst, session->realm,
			     "krbtgt",
			     session->realm,
			     DEFAULT_TKT_LIFE,
			     password);
}


static key_proc_t
key_proc(void *arg)
{
	return arg;
}

/* KClientKeyLogin
 * This routine is similiar to KClientLogin but instead of prompting the user
 * for a password it uses the private key supplied to establish login.
 */
Kerr
KClientKeyLogin(KClientSessionInfo *session,
		KClientKey *privateKey)
{
    return krb_get_in_tkt(guser, session->inst, session->realm,
			  "krbtgt",
			  session->realm,
			  DEFAULT_TKT_LIFE,
			  key_proc,
			  0,
			  privateKey);
}

/* KClientLogout
 * This routine destroys all credentials stored in the credential cache
 * effectively logging the user out.
 */
Kerr
KClientLogout(void)
{
    return 0;
}


/* KClientStatus
 * This routine returns the user's login status which can be
 * KClientLoggedIn or KClientNotLoggedIn.
 */
short
KClientStatus(KClientSessionInfo *session)
{
    CREDENTIALS c;
    if(krb_get_cred("krbtgt",
		    session->realm,
		    session->realm, &c) == KSUCCESS)
	return KClientLoggedIn;
    else
	return KClientNotLoggedIn;
}


/* KClientGetUserName
 * This routine returns the name the user supplied in the login dialog.
 * No name is returned if the user is not logged in.
 */
Kerr
KClientGetUserName(char *user)
{
    strcpy(user, guser);
    return 0;
}


/* KClientSetUserName
 * This routine sets the name that will come up in the login dialog
 * the next time the user is prompted for a password.
 */
Kerr
KClientSetUserName(char *user)
{
    strlcpy(guser, user, sizeof(guser));
    return 0;
}


/* KClientCacheInitialTicket
 * This routine is used to obtain a ticket for the password changing service.
 */
Kerr
KClientCacheInitialTicket(KClientSessionInfo *session,
			  char *service)
{
    return 0;
}


/* KClientGetSessionKey
 * This routine can be used to obtain the session key which is stored
 * in the KClientSessionInfo record. The session key has no usefullness
 * with any KClient calls but it can be used to with the MIT kerberos API.
 */
Kerr
KClientGetSessionKey(KClientSessionInfo *session,
		     KClientKey *sessionKey)
{
    CopyMemory(sessionKey, session->key, sizeof(*sessionKey));
    return 0;
}


/* KClientMakeSendAuth
 * This routine is used to create an authenticator that is the same as those
 * created by the kerberos routine SendAuth.
 */
Kerr
KClientMakeSendAuth(KClientSessionInfo *session,
		    char *service,
		    void *buf,
		    unsigned long *buflen,
		    long checksum,
		    char *applicationVersion)
{
    return 0;
}


/* KClientVerifySendAuth
 * This routine is used to verify a response made by a server doing RecvAuth.
 * The two routines KClientMakeSendAuth and KClientVerifySendAuth together
 * provide the functionality of SendAuth minus the transmission of authenticators
 * between client->server->client.
 */
Kerr
KClientVerifySendAuth(KClientSessionInfo *session,
		      void *buf,
		      unsigned long *buflen)
{
    return 0;
}


/* KClientEncrypt
 * This routine encrypts a series a bytes for transmission to the remote host.
 * For this to work properly you must be logged in and you must have specified
 * the remote and local addresses in KClientInitSession. The unencrypted
 * message pointed to by buf and of length buflen is returned encrypted
 * in encryptBuf of length encryptLength.
 * The encrypted buffer must be at least 26 bytes longer the buf.
 */
Kerr
KClientEncrypt(KClientSessionInfo *session,
	       void *buf,
	       unsigned long buflen,
	       void *encryptBuf,
	       unsigned long *encryptLength)
{
    int num = 64;
    des_cfb64_encrypt(buf, encryptBuf, buflen,
		      (struct des_ks_struct*) session->key,
		      0, &num, 1);
    return 0;
}


/* KClientDecrypt
 * This routine decrypts a series of bytes received from the remote host.

 * NOTE: this routine will not reverse a KClientEncrypt call.
 * It can only decrypt messages sent from the remote host.

 * Instead of copying the decrypted message to an out buffer,
 * the message is decrypted in place and you are returned
 * an offset into the buffer where the decrypted message begins.
 */
Kerr
KClientDecrypt(KClientSessionInfo *session,
	       void *buf,
	       unsigned long buflen,
	       unsigned long *decryptOffset,
	       unsigned long *decryptLength)
{
    int num;
    des_cfb64_encrypt(buf, buf, buflen,
		      (struct des_ks_struct*)session->key, 0, &num, 0);
    *decryptOffset = 0;
    *decryptLength = buflen;
    return 0;
}


/* KClientErrorText
 * This routine returns a text description of errors returned by any of
 * the calls in this library.
 */
char *
KClientErrorText(Kerr err,
		 char *text)
{
    char *t = krb_get_err_text(err);
    if(text)
	strcpy(text, t);
    return t;
}
