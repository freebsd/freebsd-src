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
 *
 *	@(#)exec.h	8.3 (Berkeley) 6/8/95
 *	$Id: exec.h,v 1.3 1996/09/01 10:20:05 peter Exp $
 */

/* values of cmdtype */
#define CMDUNKNOWN -1		/* no entry in table for command */
#define CMDNORMAL 0		/* command is an executable program */
#define CMDBUILTIN 1		/* command is a shell builtin */
#define CMDFUNCTION 2		/* command is a shell function */


struct cmdentry {
	int cmdtype;
	union param {
		int index;
		union node *func;
	} u;
};


extern char *pathopt;		/* set by padvance */
extern int exerrno;		/* last exec error */

void shellexec __P((char **, char **, char *, int));
char *padvance __P((char **, char *));
int hashcmd __P((int, char **));
void find_command __P((char *, struct cmdentry *, int, char *));
int find_builtin __P((char *));
void hashcd __P((void));
void changepath __P((const char *));
void deletefuncs __P((void));
void getcmdentry __P((char *, struct cmdentry *));
void addcmdentry __P((char *, struct cmdentry *));
void defun __P((char *, union node *));
int unsetfunc __P((char *));
int typecmd __P((int, char **));
