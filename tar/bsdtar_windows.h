/*-
 * Copyright (c) 2009 Michihiro NAKAJIMA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef BSDTAR_WINDOWS_H
#define	BSDTAR_WINDOWS_H 1
#include <direct.h>
#include <windows.h>

#ifndef PRId64
#define	PRId64 "I64"
#endif
#define	geteuid()	0

#ifndef S_IFIFO
#define	S_IFIFO	0010000 /* pipe */
#endif

#include <string.h>  /* Must include before redefining 'strdup' */
#if !defined(__BORLANDC__)
#define	strdup _strdup
#endif
#if !defined(__BORLANDC__)
#define	getcwd _getcwd
#endif

#define	chdir __tar_chdir
int __tar_chdir(const char *);

#ifndef S_ISREG
#define	S_ISREG(a)	(a & _S_IFREG)
#endif
#ifndef S_ISBLK
#define	S_ISBLK(a)	(0)
#endif

#endif /* BSDTAR_WINDOWS_H */
