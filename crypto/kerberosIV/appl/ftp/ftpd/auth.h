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

/* $Id: auth.h,v 1.9 1997/05/11 11:04:28 assar Exp $ */

#ifndef __AUTH_H__
#define __AUTH_H__

#include <stdarg.h>

struct at {
  char *name;
  int (*auth)(char*);
  int (*adat)(char*);
  int (*pbsz)(int);
  int (*prot)(int);
  int (*ccc)(void);
  int (*mic)(char*);
  int (*conf)(char*);
  int (*enc)(char*);
  int (*read)(int, void*, int);
  int (*write)(int, void*, int);
  int (*userok)(char*);
  int (*vprintf)(const char*, va_list);
};

extern struct at *ct;

enum protection_levels {
  prot_clear, prot_safe, prot_confidential, prot_private
};

extern char *protection_names[];

extern char *ftp_command;
extern int prot_level;

void delete_ftp_command(void);

extern int data_protection;
extern int buffer_size;
extern unsigned char *data_buffer;
extern int auth_complete;

void auth_init(void);

int auth_ok(void);

void auth(char*);
void adat(char*);
void pbsz(int);
void prot(char*);
void ccc(void);
void mic(char*);
void conf(char*);
void enc(char*);

int auth_read(int, void*, int);
int auth_write(int, void*, int);

void auth_vprintf(const char *fmt, va_list ap)
#ifdef __GNUC__
__attribute__ ((format (printf, 1, 0)))
#endif
;
void auth_printf(const char *fmt, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 1, 2)))
#endif
;

void new_ftp_command(char *command);

#endif /* __AUTH_H__ */
