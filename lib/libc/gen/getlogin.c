/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *
 * $FreeBSD: src/lib/libc/gen/getlogin.c,v 1.4 1999/11/28 22:41:38 wes Exp $
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getlogin.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <pwd.h>
#include <utmp.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int	_logname_valid;		/* known to setlogin() */

char *
getlogin()
{
	static char logname[MAXLOGNAME];

	if (_logname_valid == 0) {
#ifdef __NETBSD_SYSCALLS
		if (__getlogin(logname, sizeof(logname) - 1) < 0)
#else
		if (_getlogin(logname, sizeof(logname)) < 0)
#endif
			return ((char *)NULL);
		_logname_valid = 1;
	}
	return (*logname ? logname : (char *)NULL);
}


char *
getlogin_r(char *logname, int namelen)
{
	if (_logname_valid == 0) {
#ifdef __NETBSD_SYSCALLS
		if (__getlogin(logname, namelen - 1) < 0)
#else
		if (_getlogin(logname, namelen) < 0)
#endif
			return ((char *)NULL);
		_logname_valid = 1;
	}
	return (*logname ? logname : (char *)NULL);
}
