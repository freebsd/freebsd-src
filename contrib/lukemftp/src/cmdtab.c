/*	$NetBSD: cmdtab.c,v 1.41 2003/08/07 11:13:53 agc Exp $	*/

/*-
 * Copyright (c) 1996-2000 The NetBSD Foundation, Inc.
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
 * 3. Neither the name of the University nor the names of its contributors
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
__RCSID("$NetBSD: cmdtab.c,v 1.41 2003/08/07 11:13:53 agc Exp $");
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
char	disconhelp[] =	"terminate ftp session";
char	domachelp[] =	"execute macro";
char	edithelp[] =	"toggle command line editing";
char	epsv4help[] =	"toggle use of EPSV/EPRT on IPv4 ftp";
char	feathelp[] =	"show FEATures supported by remote system";
char	formhelp[] =	"set file transfer format";
char	gatehelp[] =	"toggle gate-ftp; specify host[:port] to change proxy";
char	globhelp[] =	"toggle metacharacter expansion of local file names";
char	hashhelp[] =	"toggle printing `#' marks; specify number to set size";
char	helphelp[] =	"print local help information";
char	idlehelp[] =	"get (set) idle timer on remote side";
char	lcdhelp[] =	"change local working directory";
char	lpagehelp[] =	"view a local file through your pager";
char	lpwdhelp[] =	"print local working directory";
char	lshelp[] =	"list contents of remote path";
char	macdefhelp[] =  "define a macro";
char	mdeletehelp[] =	"delete multiple files";
char	mgethelp[] =	"get multiple files";
char	mregethelp[] =	"get multiple files restarting at end of local file";
char	fgethelp[] =	"get files using a localfile as a source of names";
char	mkdirhelp[] =	"make directory on the remote machine";
char	mlshelp[] =	"list contents of multiple remote directories";
char	mlsdhelp[] =	"list contents of remote directory in a machine "
			"parsable form";
char	mlsthelp[] =	"list remote path in a machine parsable form";
char	modehelp[] =	"set file transfer mode";
char	modtimehelp[] = "show last modification time of remote file";
char	mputhelp[] =	"send multiple files";
char	newerhelp[] =	"get file if remote file is newer than local file ";
char	nmaphelp[] =	"set templates for default file name mapping";
char	ntranshelp[] =	"set translation table for default file name mapping";
char	optshelp[] =	"show or set options for remote commands";
char	pagehelp[] =	"view a remote file through your pager";
char	passivehelp[] =	"toggle use of passive transfer mode";
char	plshelp[] =	"list contents of remote path through your pager";
char	pmlsdhelp[] =	"list contents of remote directory in a machine "
			"parsable form through your pager";
char	porthelp[] =	"toggle use of PORT/LPRT cmd for each data connection";
char	preservehelp[] ="toggle preservation of modification time of "
			"retrieved files";
char	progresshelp[] ="toggle transfer progress meter";
char	prompthelp[] =	"force interactive prompting on multiple commands";
char	proxyhelp[] =	"issue command on alternate connection";
char	pwdhelp[] =	"print working directory on remote machine";
char	quithelp[] =	"terminate ftp session and exit";
char	quotehelp[] =	"send arbitrary ftp command";
char	ratehelp[] =	"set transfer rate limit (in bytes/second)";
char	receivehelp[] =	"receive file";
char	regethelp[] =	"get file restarting at end of local file";
char	remotehelp[] =	"get help from remote server";
char	renamehelp[] =	"rename file";
char	resethelp[] =	"clear queued command replies";
char	restarthelp[]=	"restart file transfer at bytecount";
char	rmdirhelp[] =	"remove directory on the remote machine";
char	rmtstatushelp[]="show status of remote machine";
char	runiquehelp[] = "toggle store unique for local files";
char	sendhelp[] =	"send one file";
char	sethelp[] =	"set or display options";
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
char	unsethelp[] =	"unset an option";
char	usagehelp[] =	"show command usage";
char	userhelp[] =	"send new user information";
char	verbosehelp[] =	"toggle verbose mode";
char	xferbufhelp[] =	"set socket send/receive buffer size";

#ifdef NO_EDITCOMPLETE
#define	CMPL(x)
#define	CMPL0
#else  /* !NO_EDITCOMPLETE */
#define	CMPL(x)	#x,
#define	CMPL0	"",
#endif /* !NO_EDITCOMPLETE */

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
	{ "dir",	lshelp,		1, 1, 1, CMPL(rl)	ls },
	{ "disconnect",	disconhelp,	0, 1, 1, CMPL0		disconnect },
	{ "edit",	edithelp,	0, 0, 0, CMPL0		setedit },
	{ "epsv4",	epsv4help,	0, 0, 0, CMPL0		setepsv4 },
	{ "exit",	quithelp,	0, 0, 0, CMPL0		quit },
	{ "features",	feathelp,	0, 1, 1, CMPL0		feat },
	{ "fget",	fgethelp,	1, 1, 1, CMPL(l)	fget },
	{ "form",	formhelp,	0, 1, 1, CMPL0		setform },
	{ "ftp",	connecthelp,	0, 0, 1, CMPL0		setpeer },
	{ "gate",	gatehelp,	0, 0, 0, CMPL0		setgate },
	{ "get",	receivehelp,	1, 1, 1, CMPL(rl)	get },
	{ "glob",	globhelp,	0, 0, 0, CMPL0		setglob },
	{ "hash",	hashhelp,	0, 0, 0, CMPL0		sethash },
	{ "help",	helphelp,	0, 0, 1, CMPL(C)	help },
	{ "idle",	idlehelp,	0, 1, 1, CMPL0		idlecmd },
	{ "image",	binaryhelp,	0, 1, 1, CMPL0		setbinary },
	{ "lcd",	lcdhelp,	0, 0, 0, CMPL(l)	lcd },
	{ "less",	pagehelp,	1, 1, 1, CMPL(r)	page },
	{ "lpage",	lpagehelp,	0, 0, 0, CMPL(l)	lpage },
	{ "lpwd",	lpwdhelp,	0, 0, 0, CMPL0		lpwd },
	{ "ls",		lshelp,		1, 1, 1, CMPL(rl)	ls },
	{ "macdef",	macdefhelp,	0, 0, 0, CMPL0		macdef },
	{ "mdelete",	mdeletehelp,	1, 1, 1, CMPL(R)	mdelete },
	{ "mdir",	mlshelp,	1, 1, 1, CMPL(R)	mls },
	{ "mget",	mgethelp,	1, 1, 1, CMPL(R)	mget },
	{ "mkdir",	mkdirhelp,	0, 1, 1, CMPL(r)	makedir },
	{ "mls",	mlshelp,	1, 1, 1, CMPL(R)	mls },
	{ "mlsd",	mlsdhelp,	1, 1, 1, CMPL(r)	ls },
	{ "mlst",	mlsthelp,	1, 1, 1, CMPL(r)	mlst },
	{ "mode",	modehelp,	0, 1, 1, CMPL0		setftmode },
	{ "modtime",	modtimehelp,	0, 1, 1, CMPL(r)	modtime },
	{ "more",	pagehelp,	1, 1, 1, CMPL(r)	page },
	{ "mput",	mputhelp,	1, 1, 1, CMPL(L)	mput },
	{ "mreget",	mregethelp,	1, 1, 1, CMPL(R)	mget },
	{ "msend",	mputhelp,	1, 1, 1, CMPL(L)	mput },
	{ "newer",	newerhelp,	1, 1, 1, CMPL(r)	newer },
	{ "nlist",	lshelp,		1, 1, 1, CMPL(rl)	ls },
	{ "nmap",	nmaphelp,	0, 0, 1, CMPL0		setnmap },
	{ "ntrans",	ntranshelp,	0, 0, 1, CMPL0		setntrans },
	{ "open",	connecthelp,	0, 0, 1, CMPL0		setpeer },
	{ "page",	pagehelp,	1, 1, 1, CMPL(r)	page },
	{ "passive",	passivehelp,	0, 0, 0, CMPL0		setpassive },
	{ "pdir",	plshelp,	1, 1, 1, CMPL(r)	ls },
	{ "pls",	plshelp,	1, 1, 1, CMPL(r)	ls },
	{ "pmlsd",	pmlsdhelp,	1, 1, 1, CMPL(r)	ls },
	{ "preserve",	preservehelp,	0, 0, 0, CMPL0		setpreserve },
	{ "progress",	progresshelp,	0, 0, 0, CMPL0		setprogress },
	{ "prompt",	prompthelp,	0, 0, 0, CMPL0		setprompt },
	{ "proxy",	proxyhelp,	0, 0, 1, CMPL(c)	doproxy },
	{ "put",	sendhelp,	1, 1, 1, CMPL(lr)	put },
	{ "pwd",	pwdhelp,	0, 1, 1, CMPL0		pwd },
	{ "quit",	quithelp,	0, 0, 0, CMPL0		quit },
	{ "quote",	quotehelp,	1, 1, 1, CMPL0		quote },
	{ "rate",	ratehelp,	0, 0, 0, CMPL0		setrate },
	{ "rcvbuf",	xferbufhelp,	0, 0, 0, CMPL0		setxferbuf },
	{ "recv",	receivehelp,	1, 1, 1, CMPL(rl)	get },
	{ "reget",	regethelp,	1, 1, 1, CMPL(rl)	reget },
	{ "remopts",	optshelp,	0, 1, 1, CMPL0		opts },
	{ "rename",	renamehelp,	0, 1, 1, CMPL(rr)	renamefile },
	{ "reset",	resethelp,	0, 1, 1, CMPL0		reset },
	{ "restart",	restarthelp,	1, 1, 1, CMPL0		restart },
	{ "rhelp",	remotehelp,	0, 1, 1, CMPL0		rmthelp },
	{ "rmdir",	rmdirhelp,	0, 1, 1, CMPL(r)	removedir },
	{ "rstatus",	rmtstatushelp,	0, 1, 1, CMPL(r)	rmtstatus },
	{ "runique",	runiquehelp,	0, 0, 1, CMPL0		setrunique },
	{ "send",	sendhelp,	1, 1, 1, CMPL(lr)	put },
	{ "sendport",	porthelp,	0, 0, 0, CMPL0		setport },
	{ "set",	sethelp,	0, 0, 0, CMPL(o)	setoption },
	{ "site",	sitehelp,	0, 1, 1, CMPL0		site },
	{ "size",	sizecmdhelp,	1, 1, 1, CMPL(r)	sizecmd },
	{ "sndbuf",	xferbufhelp,	0, 0, 0, CMPL0		setxferbuf },
	{ "status",	statushelp,	0, 0, 1, CMPL0		status },
	{ "struct",	structhelp,	0, 1, 1, CMPL0		setstruct },
	{ "sunique",	suniquehelp,	0, 0, 1, CMPL0		setsunique },
	{ "system",	systemhelp,	0, 1, 1, CMPL0		syst },
	{ "tenex",	tenexhelp,	0, 1, 1, CMPL0		settenex },
	{ "throttle",	ratehelp,	0, 0, 0, CMPL0		setrate },
	{ "trace",	tracehelp,	0, 0, 0, CMPL0		settrace },
	{ "type",	typehelp,	0, 1, 1, CMPL0		settype },
	{ "umask",	umaskhelp,	0, 1, 1, CMPL0		do_umask },
	{ "unset",	unsethelp,	0, 0, 0, CMPL(o)	unsetoption },
	{ "usage",	usagehelp,	0, 0, 1, CMPL(C)	help },
	{ "user",	userhelp,	0, 1, 1, CMPL0		user },
	{ "verbose",	verbosehelp,	0, 0, 0, CMPL0		setverbose },
	{ "xferbuf",	xferbufhelp,	0, 0, 0, CMPL0		setxferbuf },
	{ "?",		helphelp,	0, 0, 1, CMPL(C)	help },
	{ 0 },
};

struct option optiontab[] = {
	{ "anonpass",	NULL },
	{ "ftp_proxy",	NULL },
	{ "http_proxy",	NULL },
	{ "no_proxy",	NULL },
	{ "pager",	NULL },
	{ "prompt",	NULL },
	{ "rprompt",	NULL },
	{ 0 },
};
