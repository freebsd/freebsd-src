/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Fischbein.
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
 */

#ifndef lint
static char sccsid[] = "@(#)cmp.c	5.4 (Berkeley) 3/8/91";
static char rcsid[] = "$Header: /a/cvs/386BSD/src/bin/ls/cmp.c,v 1.2 1993/06/29 02:59:30 nate Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include "ls.h"

namecmp(a, b)
	LS *a, *b;
{
	return(strcmp(a->name, b->name));
}

revnamecmp(a, b)
	LS *a, *b;
{
	return(strcmp(b->name, a->name));
}

modcmp(a, b)
	LS *a, *b;
{
	return(b->lstat.st_mtime - a->lstat.st_mtime);
}

revmodcmp(a, b)
	LS *a, *b;
{
	return(a->lstat.st_mtime - b->lstat.st_mtime);
}

acccmp(a, b)
	LS *a, *b;
{
	return(b->lstat.st_atime - a->lstat.st_atime);
}

revacccmp(a, b)
	LS *a, *b;
{
	return(a->lstat.st_atime - b->lstat.st_atime);
}

statcmp(a, b)
	LS *a, *b;
{
	return(b->lstat.st_ctime - a->lstat.st_ctime);
}

revstatcmp(a, b)
	LS *a, *b;
{
	return(a->lstat.st_ctime - b->lstat.st_ctime);
}
