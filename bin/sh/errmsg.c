/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
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
static char sccsid[] = "@(#)errmsg.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

#include "shell.h"
#include "output.h"
#include "errmsg.h"
#include <errno.h>


#define ALL (E_OPEN|E_CREAT|E_EXEC)


struct errname {
	short errcode;		/* error number */
	short action;		/* operation which encountered the error */
	char *msg;		/* text describing the error */
};


STATIC const struct errname errormsg[] = {
	EINTR, ALL,	"interrupted",
	EACCES, ALL,	"permission denied",
	EIO, ALL,		"I/O error",
	ENOENT, E_OPEN,	"no such file",
	ENOENT, E_CREAT,	"directory nonexistent",
	ENOENT, E_EXEC,	"not found",
	ENOTDIR, E_OPEN,	"no such file",
	ENOTDIR, E_CREAT,	"directory nonexistent",
	ENOTDIR, E_EXEC,	"not found",
	EISDIR, ALL,	"is a directory",
/*    EMFILE, ALL,	"too many open files", */
	ENFILE, ALL,	"file table overflow",
	ENOSPC, ALL,	"file system full",
#ifdef EDQUOT
	EDQUOT, ALL,	"disk quota exceeded",
#endif
#ifdef ENOSR
	ENOSR, ALL,	"no streams resources",
#endif
	ENXIO, ALL,	"no such device or address",
	EROFS, ALL,	"read-only file system",
	ETXTBSY, ALL,	"text busy",
#ifdef SYSV
	EAGAIN, E_EXEC,	"not enough memory",
#endif
	ENOMEM, ALL,	"not enough memory",
#ifdef ENOLINK
	ENOLINK, ALL,	"remote access failed"
#endif
#ifdef EMULTIHOP
	EMULTIHOP, ALL,	"remote access failed",
#endif
#ifdef ECOMM
	ECOMM, ALL,	"remote access failed",
#endif
#ifdef ESTALE
	ESTALE, ALL,	"remote access failed",
#endif
#ifdef ETIMEDOUT
	ETIMEDOUT, ALL,	"remote access failed",
#endif
#ifdef ELOOP
	ELOOP, ALL,	"symbolic link loop",
#endif
	E2BIG, E_EXEC,	"argument list too long",
#ifdef ELIBACC
	ELIBACC, E_EXEC,	"shared library missing",
#endif
	0, 0,		NULL
};


/*
 * Return a string describing an error.  The returned string may be a
 * pointer to a static buffer that will be overwritten on the next call.
 * Action describes the operation that got the error.
 */

char *
errmsg(e, action) {
	struct errname const *ep;
	static char buf[12];

	for (ep = errormsg ; ep->errcode ; ep++) {
		if (ep->errcode == e && (ep->action & action) != 0)
			return ep->msg;
	}
	fmtstr(buf, sizeof buf, "error %d", e);
	return buf;
}
