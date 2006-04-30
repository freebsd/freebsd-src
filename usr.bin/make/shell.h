/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
 * $FreeBSD$
 */

#ifndef shell_h_6002e3b8
#define	shell_h_6002e3b8

#include <sys/queue.h>

#include "str.h"
#include "util.h"

/**
 * Shell Specifications:
 *
 * Some special stuff goes on if a shell doesn't have error control. In such
 * a case, errCheck becomes a printf template for echoing the command,
 * should echoing be on and ignErr becomes another printf template for
 * executing the command while ignoring the return status. If either of these
 * strings is empty when hasErrCtl is FALSE, the command will be executed
 * anyway as is and if it causes an error, so be it.
 */
struct Shell {
	TAILQ_ENTRY(Shell) link;	/* link all shell descriptions */

	/*
	 * the name of the shell. For Bourne and C shells, this is used
	 * only to find the shell description when used as the single
	 * source of a .SHELL target.
	 */
	char	*name;

	char	*path;		/* full path to the shell */

	/* True if both echoOff and echoOn defined */
	Boolean	hasEchoCtl;

	char	*echoOff;	/* command to turn off echo */
	char	*echoOn;	/* command to turn it back on */

	/*
	 * What the shell prints, and its length, when given the
	 * echo-off command. This line will not be printed when
	 * received from the shell. This is usually the command which
	 * was executed to turn off echoing
	 */
	char	*noPrint;

	/* set if can control error checking for individual commands */
	Boolean	hasErrCtl;

	/* string to turn error checking on */
	char	*errCheck;

	/* string to turn off error checking */
	char	*ignErr;

	char	*echo;	/* command line flag: echo commands */
	char	*exit;	/* command line flag: exit on error */

	ArgArray builtins;	/* ordered list of shell builtins */
	char	*meta;		/* shell meta characters */

	Boolean	unsetenv;	/* unsetenv("ENV") before exec */
};
TAILQ_HEAD(Shells, Shell);

extern struct Shell		*commandShell;

void				Shell_Init(void);
Boolean				Shell_Parse(const char []);

#endif /* shell_h_6002e3b8 */
