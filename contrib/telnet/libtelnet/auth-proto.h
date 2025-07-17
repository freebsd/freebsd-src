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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)auth-proto.h	8.1 (Berkeley) 6/4/93
 * $FreeBSD$
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

#ifdef	AUTHENTICATION

Authenticator *findauthenticator(int, int);

void auth_init(const char *, int);
int auth_cmd(int, char **);
void auth_request(void);
void auth_send(unsigned char *, int);
void auth_send_retry(void);
void auth_is(unsigned char *, int);
void auth_reply(unsigned char *, int);
void auth_finished(Authenticator *, int);
int auth_wait(char *);
void auth_disable_name(char *);
void auth_gen_printsub(unsigned char *, int, unsigned char *, int);
void auth_name(unsigned char *, int);
void auth_printsub(unsigned char *, int, unsigned char *, int);
int auth_sendname(unsigned char *, int);
void auth_encrypt_user(char *);
int auth_disable(char *);
int auth_enable(char *);
int auth_togdebug(int);
int auth_status(void);

int getauthmask(char *, int *);

#ifdef	KRB4
int kerberos4_init(Authenticator *, int);
int kerberos4_send(Authenticator *);
void kerberos4_is(Authenticator *, unsigned char *, int);
void kerberos4_reply(Authenticator *, unsigned char *, int);
int kerberos4_status(Authenticator *, char *, int);
void kerberos4_printsub(unsigned char *, int, unsigned char *, int);
#endif

#ifdef	KRB5
int kerberos5_init(Authenticator *, int);
int kerberos5_send_mutual(Authenticator *);
int kerberos5_send_oneway(Authenticator *);
void kerberos5_is(Authenticator *, unsigned char *, int);
void kerberos5_reply(Authenticator *, unsigned char *, int);
int kerberos5_status(Authenticator *, char *, int level);
void kerberos5_printsub(unsigned char *, int, unsigned char *, int);
#endif

#ifdef SRA
int sra_init(Authenticator *, int);
int sra_send(Authenticator *);
void sra_is(Authenticator *, unsigned char *, int);
void sra_reply(Authenticator *, unsigned char *, int);
int sra_status(Authenticator *, char *, int);
void sra_printsub(unsigned char *, int, unsigned char *, int);
#endif

#endif
