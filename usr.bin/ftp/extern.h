/*	$Id: extern.h,v 1.6 1997/12/13 20:38:15 pst Exp $	*/
/*	$NetBSD: extern.h,v 1.17.2.1 1997/11/18 00:59:50 mellon Exp $	*/

/*-
 * Copyright (c) 1994 The Regents of the University of California.
 * All rights reserved.
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
 *	@(#)extern.h	8.3 (Berkeley) 10/9/94
 */

struct fd_set;

void    abort_remote __P((FILE *));
void    abortpt __P((int));
void    abortrecv __P((int));
void    abortsend __P((int));
void	account __P((int, char **));
void	alarmtimer __P((int));
int	another __P((int *, char ***, const char *));
int	auto_fetch __P((int, char **));
void	blkfree __P((char **));
void	cd __P((int, char **));
void	cdup __P((int, char **));
void	changetype __P((int, int));
void	cmdabort __P((int));
void	cmdscanner __P((int));
int	command __P((const char *, ...));
#ifndef SMALL
unsigned char complete __P((EditLine *, int));
void	controlediting __P((void));
#endif /* !SMALL */
int	confirm __P((const char *, const char *));
FILE   *dataconn __P((const char *));
void	delete __P((int, char **));
void	disconnect __P((int, char **));
void	do_chmod __P((int, char **));
void	do_umask __P((int, char **));
void	domacro __P((int, char **));
char   *domap __P((char *));
void	doproxy __P((int, char **));
char   *dotrans __P((char *));
int     empty __P((struct fd_set *, int));
void	get __P((int, char **));
struct cmd *getcmd __P((const char *));
int	getit __P((int, char **, int, const char *));
int	getreply __P((int));
int	globulize __P((char **));
char   *gunique __P((const char *));
void	help __P((int, char **));
char   *hookup __P((const char *, int));
void	idle __P((int, char **));
int     initconn __P((void));
void	intr __P((void));
void	list_vertical __P((StringList *));
void	lcd __P((int, char **));
int	login __P((const char *, char *, char *));
void	lostpeer __P((void));
void	lpwd __P((int, char **));
void	ls __P((int, char **));
void	mabort __P((int));
void	macdef __P((int, char **));
void	makeargv __P((void));
void	makedir __P((int, char **));
void	mdelete __P((int, char **));
void	mget __P((int, char **));
void	mls __P((int, char **));
void	modtime __P((int, char **));
void	mput __P((int, char **));
char   *onoff __P((int));
void	newer __P((int, char **));
void	page __P((int, char **));
void    progressmeter __P((int));
char   *prompt __P((void));
void	proxabort __P((int));
void    proxtrans __P((const char *, const char *, const char *));
void    psabort __P((int));
void	psummary __P((int));
void    pswitch __P((int));
void    ptransfer __P((int));
void	put __P((int, char **));
void	pwd __P((int, char **));
void	quit __P((int, char **));
void	quote __P((int, char **));
void	quote1 __P((const char *, int, char **));
void    recvrequest __P((const char *, const char *, const char *,
	    const char *, int, int));
void	reget __P((int, char **));
char   *remglob __P((char **, int, char **));
off_t	remotesize __P((const char *, int));
time_t	remotemodtime __P((const char *, int));
void	removedir __P((int, char **));
void	renamefile __P((int, char **));
void    reset __P((int, char **));
void	restart __P((int, char **));
void	rmthelp __P((int, char **));
void	rmtstatus __P((int, char **));
int	ruserpass __P((const char *, char **, char **, char **));
void    sendrequest __P((const char *, const char *, const char *, int));
void	setascii __P((int, char **));
void	setbell __P((int, char **));
void	setbinary __P((int, char **));
void	setcase __P((int, char **));
void	setcr __P((int, char **));
void	setdebug __P((int, char **));
void	setedit __P((int, char **));
void	setform __P((int, char **));
void	setftmode __P((int, char **));
void	setgate __P((int, char **));
void	setglob __P((int, char **));
void	sethash __P((int, char **));
void	setnmap __P((int, char **));
void	setntrans __P((int, char **));
void	setpassive __P((int, char **));
void	setpeer __P((int, char **));
void	setport __P((int, char **));
void	setpreserve __P((int, char **));
void	setprogress __P((int, char **));
void	setprompt __P((int, char **));
void	setrestrict __P((int, char **));
void	setrunique __P((int, char **));
void	setstruct __P((int, char **));
void	setsunique __P((int, char **));
void	settenex __P((int, char **));
void	settrace __P((int, char **));
void	setttywidth __P((int));
void	settype __P((int, char **));
void	setverbose __P((int, char **));
void	shell __P((int, char **));
void	site __P((int, char **));
void	sizecmd __P((int, char **));
char   *slurpstring __P((void));
void	status __P((int, char **));
void	syst __P((int, char **));
int	togglevar __P((int, char **, int *, const char *));
void	usage __P((void));
void	user __P((int, char **));

extern struct	cmd cmdtab[];
extern FILE    *cout;
extern int	data;
extern char    *home;
extern int	proxy;
extern char	reply_string[];
extern int	NCMDS;

extern char *__progname;		/* from crt0.o */

