/*	$Id$	*/
/*	$NetBSD: cmdtab.c,v 1.17 1997/08/18 10:20:17 lukem Exp $	*/

/*
 * Copyright (c) 1985, 1989, 1993, 1994
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

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)cmdtab.c	8.4 (Berkeley) 10/9/94";
#else
__RCSID("$Id$");
__RCSID_SOURCE("$NetBSD: cmdtab.c,v 1.17 1997/08/18 10:20:17 lukem Exp $");
#endif
#endif /* not lint */

#include <stdio.h>
#include "ftp_var.h"

/*
 * User FTP -- Command Tables.
 */

char	accounthelp[] =	"send account command to remote server";
char	appendhelp[] =	"append to a file";
char	asciihelp[] =	"set ascii transfer type";
char	beephelp[] =	"beep when command completed";
char	binaryhelp[] =	"set binary transfer type";
char	casehelp[] =	"toggle mget upper/lower case id mapping";
char	cdhelp[] =	"change remote working directory";
char	cduphelp[] =	"change remote working directory to parent directory";
char	chmodhelp[] =	"change file permissions of remote file";
char	connecthelp[] =	"connect to remote ftp server";
char	crhelp[] =	"toggle carriage return stripping on ascii gets";
char	debughelp[] =	"toggle/set debugging mode";
char	deletehelp[] =	"delete remote file";
char	dirhelp[] =	"list contents of remote directory";
char	disconhelp[] =	"terminate ftp session";
char	domachelp[] =	"execute macro";
#ifndef SMALL
char	edithelp[] =	"toggle command line editing";
#endif /* !SMALL */
char	formhelp[] =	"set file transfer format";
char	gatehelp[] =	"toggle gate-ftp; specify host[:port] to change proxy";
char	globhelp[] =	"toggle metacharacter expansion of local file names";
char	hashhelp[] =	"toggle printing `#' marks; specify number to set size";
char	helphelp[] =	"print local help information";
char	idlehelp[] =	"get (set) idle timer on remote side";
char	lcdhelp[] =	"change local working directory";
char	lpwdhelp[] =	"print local working directory";
char	lshelp[] =	"list contents of remote directory";
char	macdefhelp[] =  "define a macro";
char	mdeletehelp[] =	"delete multiple files";
char	mdirhelp[] =	"list contents of multiple remote directories";
char	mgethelp[] =	"get multiple files";
char	mkdirhelp[] =	"make directory on the remote machine";
char	mlshelp[] =	"list contents of multiple remote directories";
char	modehelp[] =	"set file transfer mode";
char	modtimehelp[] = "show last modification time of remote file";
char	mputhelp[] =	"send multiple files";
char	newerhelp[] =	"get file if remote file is newer than local file ";
char	nlisthelp[] =	"nlist contents of remote directory";
char	nmaphelp[] =	"set templates for default file name mapping";
char	ntranshelp[] =	"set translation table for default file name mapping";
char	pagehelp[] =	"view a remote file through your pager";
char	passivehelp[] =	"enter passive transfer mode";
char	porthelp[] =	"toggle use of PORT cmd for each data connection";
char	preservehelp[] ="toggle preservation of modification time of "
			"retreived files";
char	progresshelp[] ="toggle transfer progress meter";
char	prompthelp[] =	"force interactive prompting on multiple commands";
char	proxyhelp[] =	"issue command on alternate connection";
char	pwdhelp[] =	"print working directory on remote machine";
char	quithelp[] =	"terminate ftp session and exit";
char	quotehelp[] =	"send arbitrary ftp command";
char	receivehelp[] =	"receive file";
char	regethelp[] =	"get file restarting at end of local file";
char	remotehelp[] =	"get help from remote server";
char	renamehelp[] =	"rename file";
char	resethelp[] =	"clear queued command replies";
char	restarthelp[]=	"restart file transfer at bytecount";
char	restricthelp[]= "toggle restriction of data port range";
char	rmdirhelp[] =	"remove directory on the remote machine";
char	rmtstatushelp[]="show status of remote machine";
char	runiquehelp[] = "toggle store unique for local files";
char	sendhelp[] =	"send one file";
char	shellhelp[] =	"escape to the shell";
char	sitehelp[] =	"send site specific command to remote server\n"
			"\t\tTry \"rhelp site\" or \"site help\" "
			"for more information";
char	sizecmdhelp[] = "show size of remote file";
char	statushelp[] =	"show current status";
char	structhelp[] =	"set file transfer structure";
char	suniquehelp[] = "toggle store unique on remote machine";
char	systemhelp[] =  "show remote system type";
char	tenexhelp[] =	"set tenex file transfer type";
char	tracehelp[] =	"toggle packet tracing";
char	typehelp[] =	"set file transfer type";
char	umaskhelp[] =	"get (set) umask on remote side";
char	userhelp[] =	"send new user information";
char	verbosehelp[] =	"toggle verbose mode";

#ifdef SMALL
#define CMPL(x)
#define CMPL0
#else  /* !SMALL */
#define CMPL(x)	__STRING(x), 
#define CMPL0	"",
#endif /* !SMALL */

struct cmd cmdtab[] = {
	{ "!",		shellhelp,	0, 0, 0, CMPL0		shell },
	{ "$",		domachelp,	1, 0, 0, CMPL0		domacro },
	{ "account",	accounthelp,	0, 1, 1, CMPL0		account},
	{ "append",	appendhelp,	1, 1, 1, CMPL(lr)	put },
	{ "ascii",	asciihelp,	0, 1, 1, CMPL0		setascii },
	{ "bell",	beephelp,	0, 0, 0, CMPL0		setbell },
	{ "binary",	binaryhelp,	0, 1, 1, CMPL0		setbinary },
	{ "bye",	quithelp,	0, 0, 0, CMPL0		quit },
	{ "case",	casehelp,	0, 0, 1, CMPL0		setcase },
	{ "cd",		cdhelp,		0, 1, 1, CMPL(r)	cd },
	{ "cdup",	cduphelp,	0, 1, 1, CMPL0		cdup },
	{ "chmod",	chmodhelp,	0, 1, 1, CMPL(nr)	do_chmod },
	{ "close",	disconhelp,	0, 1, 1, CMPL0		disconnect },
	{ "cr",		crhelp,		0, 0, 0, CMPL0		setcr },
	{ "debug",	debughelp,	0, 0, 0, CMPL0		setdebug },
	{ "delete",	deletehelp,	0, 1, 1, CMPL(r)	delete },
	{ "dir",	dirhelp,	1, 1, 1, CMPL(rl)	ls },
	{ "disconnect",	disconhelp,	0, 1, 1, CMPL0		disconnect },
#ifndef SMALL
	{ "edit",	edithelp,	0, 0, 0, CMPL0		setedit },
#endif /* !SMALL */
	{ "exit",	quithelp,	0, 0, 0, CMPL0		quit },
	{ "form",	formhelp,	0, 1, 1, CMPL0		setform },
	{ "ftp",	connecthelp,	0, 0, 1, CMPL0		setpeer },
	{ "get",	receivehelp,	1, 1, 1, CMPL(rl)	get },
	{ "gate",	gatehelp,	0, 0, 0, CMPL0		setgate },
	{ "glob",	globhelp,	0, 0, 0, CMPL0		setglob },
	{ "hash",	hashhelp,	0, 0, 0, CMPL0		sethash },
	{ "help",	helphelp,	0, 0, 1, CMPL(C)	help },
	{ "idle",	idlehelp,	0, 1, 1, CMPL0		idle },
	{ "image",	binaryhelp,	0, 1, 1, CMPL0		setbinary },
	{ "lcd",	lcdhelp,	0, 0, 0, CMPL(l)	lcd },
	{ "less",	pagehelp,	1, 1, 1, CMPL(r)	page },
	{ "lpwd",	lpwdhelp,	0, 0, 0, CMPL0		lpwd },
	{ "ls",		lshelp,		1, 1, 1, CMPL(rl)	ls },
	{ "macdef",	macdefhelp,	0, 0, 0, CMPL0		macdef },
	{ "mdelete",	mdeletehelp,	1, 1, 1, CMPL(R)	mdelete },
	{ "mdir",	mdirhelp,	1, 1, 1, CMPL(R)	mls },
	{ "mget",	mgethelp,	1, 1, 1, CMPL(R)	mget },
	{ "mkdir",	mkdirhelp,	0, 1, 1, CMPL(r)	makedir },
	{ "mls",	mlshelp,	1, 1, 1, CMPL(R)	mls },
	{ "mode",	modehelp,	0, 1, 1, CMPL0		setftmode },
	{ "modtime",	modtimehelp,	0, 1, 1, CMPL(r)	modtime },
	{ "more",	pagehelp,	1, 1, 1, CMPL(r)	page },
	{ "mput",	mputhelp,	1, 1, 1, CMPL(L)	mput },
	{ "msend",	mputhelp,	1, 1, 1, CMPL(L)	mput },
	{ "newer",	newerhelp,	1, 1, 1, CMPL(r)	newer },
	{ "nlist",	nlisthelp,	1, 1, 1, CMPL(rl)	ls },
	{ "nmap",	nmaphelp,	0, 0, 1, CMPL0		setnmap },
	{ "ntrans",	ntranshelp,	0, 0, 1, CMPL0		setntrans },
	{ "open",	connecthelp,	0, 0, 1, CMPL0		setpeer },
	{ "page",	pagehelp,	1, 1, 1, CMPL(r)	page },
	{ "passive",	passivehelp,	0, 0, 0, CMPL0		setpassive },
	{ "preserve",	preservehelp,	0, 0, 0, CMPL0		setpreserve },
	{ "progress",	progresshelp,	0, 0, 0, CMPL0		setprogress },
	{ "prompt",	prompthelp,	0, 0, 0, CMPL0		setprompt },
	{ "proxy",	proxyhelp,	0, 0, 1, CMPL(c)	doproxy },
	{ "put",	sendhelp,	1, 1, 1, CMPL(lr)	put },
	{ "pwd",	pwdhelp,	0, 1, 1, CMPL0		pwd },
	{ "quit",	quithelp,	0, 0, 0, CMPL0		quit },
	{ "quote",	quotehelp,	1, 1, 1, CMPL0		quote },
	{ "recv",	receivehelp,	1, 1, 1, CMPL(rl)	get },
	{ "reget",	regethelp,	1, 1, 1, CMPL(rl)	reget },
	{ "rename",	renamehelp,	0, 1, 1, CMPL(rr)	renamefile },
	{ "reset",	resethelp,	0, 1, 1, CMPL0		reset },
	{ "restart",	restarthelp,	1, 1, 1, CMPL0		restart },
	{ "restrict",	restricthelp,	0, 0, 0, CMPL0		setrestrict },
	{ "rhelp",	remotehelp,	0, 1, 1, CMPL0		rmthelp },
	{ "rmdir",	rmdirhelp,	0, 1, 1, CMPL(r)	removedir },
	{ "rstatus",	rmtstatushelp,	0, 1, 1, CMPL(r)	rmtstatus },
	{ "runique",	runiquehelp,	0, 0, 1, CMPL0		setrunique },
	{ "send",	sendhelp,	1, 1, 1, CMPL(lr)	put },
	{ "sendport",	porthelp,	0, 0, 0, CMPL0		setport },
	{ "site",	sitehelp,	0, 1, 1, CMPL0		site },
	{ "size",	sizecmdhelp,	1, 1, 1, CMPL(r)	sizecmd },
	{ "status",	statushelp,	0, 0, 1, CMPL0		status },
	{ "struct",	structhelp,	0, 1, 1, CMPL0		setstruct },
	{ "sunique",	suniquehelp,	0, 0, 1, CMPL0		setsunique },
	{ "system",	systemhelp,	0, 1, 1, CMPL0		syst },
	{ "tenex",	tenexhelp,	0, 1, 1, CMPL0		settenex },
	{ "trace",	tracehelp,	0, 0, 0, CMPL0		settrace },
	{ "type",	typehelp,	0, 1, 1, CMPL0		settype },
	{ "umask",	umaskhelp,	0, 1, 1, CMPL0		do_umask },
	{ "user",	userhelp,	0, 1, 1, CMPL0		user },
	{ "verbose",	verbosehelp,	0, 0, 0, CMPL0		setverbose },
	{ "?",		helphelp,	0, 0, 1, CMPL(C)	help },
	{ 0 },
};

int	NCMDS = (sizeof(cmdtab) / sizeof(cmdtab[0])) - 1;
