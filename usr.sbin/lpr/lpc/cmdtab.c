/*
 * Copyright (c) 1983, 1993
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
 */

#ifndef lint
/*
static char sccsid[] = "@(#)cmdtab.c	8.1 (Berkeley) 6/6/93";
*/
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/cdefs.h>

#include "lpc.h"
#include "extern.h"

/*
 * lpc -- command tables
 */
char	aborthelp[] =	"terminate a spooling daemon immediately and disable printing";
char	cleanhelp[] =	"remove cruft files from a queue";
char	enablehelp[] =	"turn a spooling queue on";
char	disablehelp[] =	"turn a spooling queue off";
char	downhelp[] =	"do a 'stop' followed by 'disable' and put a message in status";
char	helphelp[] =	"get help on commands";
char	quithelp[] =	"exit lpc";
char	restarthelp[] =	"kill (if possible) and restart a spooling daemon";
char	starthelp[] =	"enable printing and start a spooling daemon";
char	statushelp[] =	"show status of daemon and queue";
char	stophelp[] =	"stop a spooling daemon after current job completes and disable printing";
char	tcleanhelp[] =	"test to see what files a clean cmd would remove";
char	topqhelp[] =	"put job at top of printer queue";
char	uphelp[] =	"enable everything and restart spooling daemon";

#define PR	1	/* a privileged command */

struct cmd cmdtab[] = {
	{ "abort",	aborthelp,	PR,	0,		doabort },
	{ "clean",	cleanhelp,	PR,	init_clean,	clean_q },
	{ "enable",	enablehelp,	PR,	0,		enable },
	{ "exit",	quithelp,	0,	quit,		0 },
	{ "disable",	disablehelp,	PR,	0, 		disable },
	{ "down",	downhelp,	PR,	down,		0 },
	{ "help",	helphelp,	0,	help,		0 },
	{ "quit",	quithelp,	0,	quit,		0 },
	{ "restart",	restarthelp,	0,	0,		restart },
	{ "start",	starthelp,	PR,	0,		startcmd },
	{ "status",	statushelp,	0,	0,		status },
	{ "stop",	stophelp,	PR,	0,		stop },
	{ "tclean",	tcleanhelp,	0,	init_tclean,	clean_q },
	{ "topq",	topqhelp,	PR,	topq,		0 },
	{ "up",		uphelp,		PR,	0,		up },
	{ "?",		helphelp,	0,	help,		0 },
	{ 0, 0, 0, 0, 0},
};

int	NCMDS = sizeof (cmdtab) / sizeof (cmdtab[0]);
