#if defined(LIBC_SCCS) && !defined(lint)
static const char sccsid[] = "@(#)strerror.c	8.1 (Berkeley) 6/4/93";
static const char rcsid[] = "$Id: strerror.c,v 1.3.2.1.10.1 2008/04/28 04:25:42 marka Exp $";
#endif /* LIBC_SCCS and not lint */

/*
 * Copyright (c) 1988, 1993
 *    The Regents of the University of California.  All rights reserved.
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
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
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

#include "port_before.h"

#include <sys/param.h>
#include <sys/types.h>

#include <string.h>

#include "port_after.h"

#ifndef NEED_STRERROR
int __strerror_unneeded__;
#else

#ifdef USE_SYSERROR_LIST
extern int sys_nerr;
extern char *sys_errlist[];
#endif

const char *
isc_strerror(int num) {
#define	UPREFIX	"Unknown error: "
	static char ebuf[40] = UPREFIX;		/* 64-bit number + slop */
	u_int errnum;
	char *p, *t;
#ifndef USE_SYSERROR_LIST
	const char *ret;
#endif
	char tmp[40];

	errnum = num;				/* convert to unsigned */
#ifdef USE_SYSERROR_LIST
	if (errnum < (u_int)sys_nerr)
		return (sys_errlist[errnum]);
#else
#undef strerror
	ret = strerror(num);			/* call strerror() in libc */
	if (ret != NULL)
		return(ret);
#endif

	/* Do this by hand, so we don't include stdio(3). */
	t = tmp;
	do {
		*t++ = "0123456789"[errnum % 10];
	} while (errnum /= 10);
	for (p = ebuf + sizeof(UPREFIX) - 1;;) {
		*p++ = *--t;
		if (t <= tmp)
			break;
	}
	return (ebuf);
}

#endif /*NEED_STRERROR*/
