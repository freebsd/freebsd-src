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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

/* $Id: krb4.h,v 1.10 1997/04/01 08:17:22 joda Exp $ */

#ifndef __KRB4_H__
#define __KRB4_H__

#include <stdio.h>
#include <stdarg.h>

extern int auth_complete;

void sec_status(void);

enum { prot_clear, prot_safe, prot_confidential, prot_private };

void sec_prot(int, char**);

int sec_getc(FILE *F);
int sec_putc(int c, FILE *F);
int sec_fflush(FILE *F);
int sec_read(int fd, void *data, int length);
int sec_write(int fd, char *data, int length);

int krb4_getc(FILE *F);
int krb4_read(int fd, char *data, int length);



void sec_set_protection_level(void);
int sec_request_prot(char *level);

void kauth(int, char **);
void klist(int, char **);

void krb4_quit(void);

int krb4_write_enc(FILE *F, char *fmt, va_list ap);
int krb4_read_msg(char *s, int priv);
int krb4_read_mic(char *s);
int krb4_read_enc(char *s);

int do_klogin(char *host);

#endif /* __KRB4_H__ */
