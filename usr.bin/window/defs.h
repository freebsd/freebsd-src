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
 *	@(#)defs.h	8.1 (Berkeley) 6/6/93
 */

#include "ww.h"
#include <sys/time.h>

#define NWINDOW 9

struct timeval starttime;

struct ww *window[NWINDOW];	/* the windows */
struct ww *selwin;		/* the selected window */
struct ww *lastselwin;		/* the last selected window */
struct ww *cmdwin;		/* the command window */
struct ww *framewin;		/* the window for framing */
struct ww *boxwin;		/* the window for the box */
struct ww *fgwin;		/* the last foreground window */

#define isfg(w)		((w)->ww_order <= fgwin->ww_order)

char *default_shell[128];	/* default shell argv */
char *default_shellfile;	/* default shell program */
int default_nline;		/* default buffer size for new windows */
int default_smooth;		/* default "smooth" parameter */
char escapec;			/* the escape character */

	/* flags */
char quit;			/* quit command issued */
char terse;			/* terse mode */
char debug;			/* debug mode */
char incmd;			/* in command mode */

struct ww *getwin();
struct ww *openwin();
struct ww *vtowin();
struct ww *openiwin();
