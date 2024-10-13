/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2009 Michihiro NAKAJIMA
 * All rights reserved.
 */

#ifndef BSDTAR_WINDOWS_H
#define	BSDTAR_WINDOWS_H 1
#include <direct.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>

#ifndef PRId64
#define	PRId64 "I64"
#endif
#define	geteuid()	0

#ifndef __WATCOMC__

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

#endif

#endif /* BSDTAR_WINDOWS_H */
