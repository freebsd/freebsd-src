/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
 *	@(#)context.h	8.1 (Berkeley) 6/6/93
 */

#include <stdio.h>

struct context {
	struct context *x_link;		/* nested contexts */
	char x_type;			/* tag for union */
	union {
		struct {	/* input is a file */
			char *X_filename;	/* input file name */
			FILE *X_fp;		/* input stream */
			short X_lineno;		/* current line number */
			char X_bol;		/* at beginning of line */
			char X_noerr;		/* don't report errors */
			struct ww *X_errwin;	/* error window */
		} x_f;
		struct {	/* input is a buffer */
			char *X_buf;		/* input buffer */
			char *X_bufp;		/* current position in buf */
			struct value *X_arg;	/* argument for alias */
			int X_narg;		/* number of arguments */
		} x_b;
	} x_un;
		/* holding place for current token */
	int x_token;			/* the token */
	struct value x_val;		/* values associated with token */
		/* parser error flags */
	unsigned x_erred :1;		/* had an error */
	unsigned x_synerred :1;		/* had syntax error */
	unsigned x_abort :1;		/* fatal error */
};
#define x_buf		x_un.x_b.X_buf
#define x_bufp		x_un.x_b.X_bufp
#define x_arg		x_un.x_b.X_arg
#define x_narg		x_un.x_b.X_narg
#define x_filename	x_un.x_f.X_filename
#define x_fp		x_un.x_f.X_fp
#define x_lineno	x_un.x_f.X_lineno
#define x_bol		x_un.x_f.X_bol
#define x_errwin	x_un.x_f.X_errwin
#define x_noerr		x_un.x_f.X_noerr

	/* x_type values, 0 is reserved */
#define X_FILE		1		/* input is a file */
#define X_BUF		2		/* input is a buffer */

struct context cx;			/* the current context */
