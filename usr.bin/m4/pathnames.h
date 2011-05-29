/*	$OpenBSD: pathnames.h,v 1.4 1997/04/04 18:41:29 deraadt Exp $	*/
/*	$NetBSD: pathnames.h,v 1.6 1995/09/29 00:27:55 cgd Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ozan Yigit at York University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *
 *	@(#)pathnames.h	8.1 (Berkeley) 6/6/93
 * $FreeBSD$
 */

/*
 * Definitions of diversion files.  If the name of the file is changed,
 * adjust UNIQUE to point to the wildcard (*) character in the filename.
 */

#ifdef msdos
#define _PATH_DIVNAME	"\\M4*XXXXXX"		/* msdos diversion files */
#define	UNIQUE		3			/* unique char location */
#endif

#if defined(unix) || defined(__FreeBSD__) || defined(__NetBSD__) || \
	defined(__OpenBSD__)
#define _PATH_DIVNAME	"/tmp/m4.0XXXXXXXXXX"	/* unix diversion files */
#define UNIQUE		8			/* unique char location */
#endif

#ifdef vms
#define _PATH_DIVNAME	"sys$login:m4*XXXXXX"	/* vms diversion files */
#define UNIQUE		12			/* unique char location */
#endif
