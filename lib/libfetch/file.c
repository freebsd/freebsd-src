/*-
 * Copyright (c) 1998 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: file.c,v 1.2 1998/11/06 22:14:08 des Exp $
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

#include "fetch.h"
#include "common.h"

FILE *
fetchGetFile(struct url *u, char *flags)
{
    FILE *f;
    
    f = fopen(u->doc, "r");
    
    if (f == NULL)
	_fetch_syserr();
    return f;
}

FILE *
fetchPutFile(struct url *u, char *flags)
{
    FILE *f;
    
    if (strchr(flags, 'a'))
	f = fopen(u->doc, "a");
    else
	f = fopen(u->doc, "w");
    
    if (f == NULL)
	_fetch_syserr();
    return f;
}

int
fetchStatFile(struct url *u, struct url_stat *us, char *flags)
{
    struct stat sb;

    if (stat(u->doc, &sb) == -1) {
	_fetch_syserr();
	return -1;
    }
    us->size = sb.st_size;
    us->atime = sb.st_atime;
    us->mtime = sb.st_mtime;
    return 0;
}
