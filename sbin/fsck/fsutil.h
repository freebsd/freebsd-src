/*	$NetBSD: fsutil.h,v 1.4 1998/07/26 20:02:36 mycroft Exp $	*/

/*
 * Copyright (c) 1996 Christos Zoulas.  All rights reserved.
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
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * $FreeBSD$
 */

void perror __P((const char *));
void errexit __P((const char *, ...))
    __attribute__((__noreturn__,__format__(__printf__,1,2)));  
void pfatal __P((const char *, ...))
    __attribute__((__format__(__printf__,1,2)));  
void pwarn __P((const char *, ...))
    __attribute__((__format__(__printf__,1,2)));  
void panic __P((const char *, ...))
    __attribute__((__noreturn__,__format__(__printf__,1,2)));  
const char *rawname __P((const char *));
const char *unrawname __P((const char *));
#if 0
const char *blockcheck __P((const char *));
#endif
const char *devcheck __P((const char *));
const char *cdevname __P((void));
void setcdevname __P((const char *, int));
int  hotroot __P((void));
void *emalloc __P((size_t));
void *erealloc __P((void *, size_t));
char *estrdup __P((const char *));

#define CHECK_PREEN	1
#define	CHECK_VERBOSE	2
#define	CHECK_DEBUG	4

struct fstab;
int checkfstab __P((int, void *(*)(struct fstab *), 
    int (*) (const char *, const char *, const char *, void *, pid_t *)));
