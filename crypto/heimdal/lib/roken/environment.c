/*
 * Copyright (c) 2000 Kungliga Tekniska Högskolan
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
RCSID("$Id: environment.c,v 1.1 2000/06/21 02:05:03 assar Exp $");
#endif

#include <stdio.h>
#include <string.h>
#include "roken.h"

/*
 * return count of environment assignments from `file' and 
 * list of malloced strings in `env'
 */

int
read_environment(const char *file, char ***env)
{
    int i, k;
    FILE *F;
    char **l;
    char buf[BUFSIZ], *p, *r;

    if ((F = fopen(file, "r")) == NULL) {
	return 0;
    }

    i = 0;
    if (*env) {
	l = *env;
	while (*l != NULL) {
	    i++;
	    l++;
	}
    }
    l = *env;
    /* This is somewhat more relaxed on what it accepts then
     * Wietses sysv_environ from K4 was...
     */
    while (fgets(buf, BUFSIZ, F) != NULL) {
	if (buf[0] == '#')
	    continue;

	p = strchr(buf, '#');
	if (p != NULL)
	    *p = '\0';

	p = buf;
	while (*p == ' ' || *p == '\t' || *p == '\n') p++;
	if (*p == '\0')
	    continue;

	k = strlen(p);
	if (p[k-1] == '\n')
	    p[k-1] = '\0';

	/* Here one should check that is is a 'valid' env string... */
	r = strchr(p, '=');
	if (r == NULL)
	    continue;

	l = realloc(l, (i+1) * sizeof (char *));
	l[i++] = strdup(p);
    }
    fclose(F);
    l = realloc(l, (i+1) * sizeof (char *));
    l[i] = NULL;
    *env = l;
    return i;
}
