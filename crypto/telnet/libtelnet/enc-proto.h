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
 *
 *	@(#)enc-proto.h	8.1 (Berkeley) 6/4/93
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
#if	!defined(P)
#ifdef	__STDC__
#define	P(x)	x
#else
#define	P(x)	()
#endif
#endif

#ifdef	ENCRYPTION
void encrypt_init P((char *, int));
Encryptions *findencryption P((int));
void encrypt_send_supprt P((void));
void encrypt_auto P((int));
void decrypt_auto P((int));
void encrypt_is P((unsigned char *, int));
void encrypt_reply P((unsigned char *, int));
void encrypt_start_input P((int));
void encrypt_session_key P((Session_Key *, int));
void encrypt_end_input P((void));
void encrypt_start_output P((int));
void encrypt_end_output P((void));
void encrypt_send_request_start P((void));
void encrypt_send_request_end P((void));
void encrypt_send_end P((void));
void encrypt_wait P((void));
void encrypt_send_support P((void));
void encrypt_send_keyid P((int, unsigned char *, int, int));
void encrypt_start P((unsigned char *, int));
void encrypt_end P((void));
void encrypt_support P((unsigned char *, int));
void encrypt_request_start P((unsigned char *, int));
void encrypt_request_end P((void));
void encrypt_enc_keyid P((unsigned char *, int));
void encrypt_dec_keyid P((unsigned char *, int));
void encrypt_printsub P((unsigned char *, int, unsigned char *, int));
int net_write P((unsigned char *, int));

#ifndef	TELENTD
int encrypt_cmd P((int, char **));
void encrypt_display P((void));
#endif

#ifdef DES_ENCRYPTION
void krbdes_encrypt P((unsigned char *, int));
int krbdes_decrypt P((int));
int krbdes_is P((unsigned char *, int));
int krbdes_reply P((unsigned char *, int));
void krbdes_init P((int));
int krbdes_start P((int, int));
void krbdes_session P((Session_Key *, int));
void krbdes_printsub P((unsigned char *, int, unsigned char *, int));

void cfb64_encrypt P((unsigned char *, int));
int cfb64_decrypt P((int));
void cfb64_init P((int));
int cfb64_start P((int, int));
int cfb64_is P((unsigned char *, int));
int cfb64_reply P((unsigned char *, int));
void cfb64_session P((Session_Key *, int));
int cfb64_keyid P((int, unsigned char *, int *));
void cfb64_printsub P((unsigned char *, int, unsigned char *, int));

void ofb64_encrypt P((unsigned char *, int));
int ofb64_decrypt P((int));
void ofb64_init P((int));
int ofb64_start P((int, int));
int ofb64_is P((unsigned char *, int));
int ofb64_reply P((unsigned char *, int));
void ofb64_session P((Session_Key *, int));
int ofb64_keyid P((int, unsigned char *, int *));
void ofb64_printsub P((unsigned char *, int, unsigned char *, int));
#endif /* DES_ENCRYPTION */

#endif	/* ENCRYPTION */
