/*	$NetBSD: progressbar.h,v 1.5 2005/02/10 16:00:38 jmc Exp $	*/

/*-
 * Copyright (c) 1996-2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef STANDALONE_PROGRESS
#include <setjmp.h>
#endif	/* !STANDALONE_PROGRESS */

#ifndef	GLOBAL
#define	GLOBAL	extern
#endif


#define	STALLTIME	5	/* # of seconds of no xfer before "stalling" */

typedef void (*sigfunc)(int);


GLOBAL	FILE   *ttyout;		/* stdout, or stderr if retrieving to stdout */

GLOBAL	int	progress;	/* display transfer progress bar */
GLOBAL	int	ttywidth;	/* width of tty */

GLOBAL	off_t	bytes;		/* current # of bytes read */
GLOBAL	off_t	filesize;	/* size of file being transferred */
GLOBAL	off_t	restart_point;	/* offset to restart transfer */
GLOBAL	char   *prefix;		/* Text written left of progress bar */


#ifndef	STANDALONE_PROGRESS
GLOBAL	int	fromatty;	/* input is from a terminal */
GLOBAL	int	verbose;	/* print messages coming back from server */
GLOBAL	int	quit_time;	/* maximum time to wait if stalled */

GLOBAL	char   *direction;	/* direction transfer is occurring */

GLOBAL	sigjmp_buf toplevel;	/* non-local goto stuff for cmd scanner */
#endif	/* !STANDALONE_PROGRESS */

int	foregroundproc(void);
void	alarmtimer(int);
void	progressmeter(int);
sigfunc	xsignal(int, sigfunc);
sigfunc	xsignal_restart(int, sigfunc, int);

#ifndef STANDALONE_PROGRESS
void	psummary(int);
void	ptransfer(int);
#endif	/* !STANDALONE_PROGRESS */

#ifdef NO_LONG_LONG
# define LLF		"%ld"
# define LLFP(x)	"%" x "ld"
# define LLT		long
# define ULLF		"%lu"
# define ULLFP(x)	"%" x "lu"
# define ULLT		unsigned long
#else
# define LLF		"%lld"
# define LLFP(x)	"%" x "lld"
# define LLT		long long
# define ULLF		"%llu"
# define ULLFP(x)	"%" x "llu"
# define ULLT		unsigned long long
#endif
