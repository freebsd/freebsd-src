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

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: warnerr.c,v 1.6 1997/04/02 14:59:54 bg Exp $");
#endif

#include "roken.h"
#include "err.h"

#ifndef HAVE___PROGNAME
const char *__progname;
#endif

void
set_progname(char *argv0)
{
#ifndef HAVE___PROGNAME
    char *p;
    if(argv0 == NULL)
	return;
    p = strrchr(argv0, '/');
    if(p == NULL)
	p = argv0;
    else
	p++;
    __progname = p;
#endif
}

void
warnerr(int doexit, int eval, int doerrno, const char *fmt, va_list ap)
{
    int sverrno = errno;
    if(__progname != NULL){
	fprintf(stderr, "%s", __progname);
	if(fmt != NULL || doerrno)
	    fprintf(stderr, ": ");
    }
    if (fmt != NULL){
	vfprintf(stderr, fmt, ap);
	if(doerrno)
	    fprintf(stderr, ": ");
    }
    if(doerrno)
	fprintf(stderr, "%s", strerror(sverrno));
    fprintf(stderr, "\n");
    if(doexit)
	exit(eval);
}
