/*
 * Copyright (c) 1998 - 2000 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: print_version.c,v 1.1 2000/07/01 19:47:35 assar Exp $");
#endif
#include "roken.h"

#include "print_version.h"

void
print_version(const char *progname)
{
    const char *arg[] = VERSIONLIST;
    const int num_args = sizeof(arg) / sizeof(arg[0]);
    char *msg;
    size_t len = 0;
    int i;
    
    if(progname == NULL)
	progname = __progname;
    
    if(num_args == 0)
	msg = "no version information";
    else {
	for(i = 0; i < num_args; i++) {
	    if(i > 0)
		len += 2;
	    len += strlen(arg[i]);
	}
	msg = malloc(len + 1);
	if(msg == NULL) {
	    fprintf(stderr, "%s: out of memory\n", progname);
	    return;
	}
	msg[0] = '\0';
	for(i = 0; i < num_args; i++) {
	    if(i > 0)
		strcat(msg, ", ");
	    strcat(msg, arg[i]);
	}
    }
    fprintf(stderr, "%s (%s)\n", progname, msg);
    fprintf(stderr, "Copyright (c) 1999 - 2000 Kungliga Tekniska Högskolan\n");
    if(num_args != 0)
	free(msg);
}
