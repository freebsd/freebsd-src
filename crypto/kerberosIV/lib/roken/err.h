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

/* $Id: err.h,v 1.13 1997/05/02 14:29:30 assar Exp $ */

#ifndef __ERR_H__
#define __ERR_H__

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

extern const char *__progname;

#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

void warnerr(int doexit, int eval, int doerrno, const char *fmt, va_list ap)
     __attribute__ ((format (printf, 4, 0)));

void verr(int eval, const char *fmt, va_list ap)
     __attribute__ ((noreturn, format (printf, 2, 0)));
void err(int eval, const char *fmt, ...)
     __attribute__ ((noreturn, format (printf, 2, 3)));
void verrx(int eval, const char *fmt, va_list ap)
     __attribute__ ((noreturn, format (printf, 2, 0)));
void errx(int eval, const char *fmt, ...)
     __attribute__ ((noreturn, format (printf, 2, 3)));
void vwarn(const char *fmt, va_list ap)
     __attribute__ ((format (printf, 1, 0)));
void warn(const char *fmt, ...)
     __attribute__ ((format (printf, 1, 2)));
void vwarnx(const char *fmt, va_list ap)
     __attribute__ ((format (printf, 1, 0)));
void warnx(const char *fmt, ...)
     __attribute__ ((format (printf, 1, 2)));

#endif /* __ERR_H__ */
