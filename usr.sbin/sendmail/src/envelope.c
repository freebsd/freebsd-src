/*
 * Copyright (c) 1983 Eric P. Allman
 * Copyright (c) 1988, 1993
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
static char sccsid[] = "@(#)envelope.c	8.3 (Berkeley) 7/13/93";
#endif /* not lint */

#include "sendmail.h"
#include <sys/time.h>
#include <pwd.h>

/*
**  NEWENVELOPE -- allocate a new envelope
**
**	Supports inheritance.
**
**	Parameters:
**		e -- the new envelope to fill in.
**		parent -- the envelope to be the parent of e.
**
**	Returns:
**		e.
**
**	Side Effects:
**		none.
*/

ENVELOPE *
newenvelope(e, parent)
	register ENVELOPE *e;
	register ENVELOPE *parent;
{
	extern putheader(), putbody();
	extern ENVELOPE BlankEnvelope;

	if (e == parent && e->e_parent != NULL)
		parent = e->e_parent;
	clearenvelope(e, TRUE);
	if (e == CurEnv)
		bcopy((char *) &NullAddress, (char *) &e->e_from, sizeof e->e_from);
	else
		bcopy((char *) &CurEnv->e_from, (char *) &e->e_from, sizeof e->e_from);
	e->e_parent = parent;
	e->e_ctime = curtime();
	if (parent != NULL)
		e->e_msgpriority = parent->e_msgsize;
	e->e_puthdr = putheader;
	e->e_putbody = putbody;
	if (CurEnv->e_xfp != NULL)
		(void) fflush(CurEnv->e_xfp);

	return (e);
}
/*
**  DROPENVELOPE -- deallocate an envelope.
**
**	Parameters:
**		e -- the envelope to deallocate.
**
**	Returns:
**		none.
**
**	Side Effects:
**		housekeeping necessary to dispose of an envelope.
**		Unlocks this queue file.
*/

void
dropenvelope(e)
	register ENVELOPE *e;
{
	bool queueit = FALSE;
	register ADDRESS *q;
	char *id = e->e_id;
	char buf[MAXLINE];

	if (tTd(50, 1))
	{
		printf("dropenvelope %x: id=", e);
		xputs(e->e_id);
		printf(", flags=%o\n", e->e_flags);
		if (tTd(50, 10))
		{
			printf("sendq=");
			printaddr(e->e_sendqueue, TRUE);
		}
	}

	/* we must have an id to remove disk files */
	if (id == NULL)
		return;

#ifdef LOG
	if (LogLevel > 84)
		syslog(LOG_DEBUG, "dropenvelope, id=%s, flags=%o, pid=%d",
				  id, e->e_flags, getpid());
#endif /* LOG */

	/* post statistics */
	poststats(StatFile);

	/*
	**  Extract state information from dregs of send list.
	*/

	for (q = e->e_sendqueue; q != NULL; q = q->q_next)
	{
		if (bitset(QQUEUEUP, q->q_flags))
			queueit = TRUE;
	}

	/*
	**  See if the message timed out.
	*/

	if (!queueit)
		/* nothing to do */ ;
	else if (curtime() > e->e_ctime + TimeOuts.to_q_return)
	{
		if (!bitset(EF_TIMEOUT, e->e_flags))
		{
			(void) sprintf(buf, "Cannot send message for %s",
				pintvl(TimeOuts.to_q_return, FALSE));
			if (e->e_message != NULL)
				free(e->e_message);
			e->e_message = newstr(buf);
			message(buf);
		}
		e->e_flags |= EF_TIMEOUT|EF_CLRQUEUE;
		fprintf(e->e_xfp, "Message could not be delivered for %s\n",
			pintvl(TimeOuts.to_q_return, FALSE));
		fprintf(e->e_xfp, "Message will be deleted from queue\n");
		for (q = e->e_sendqueue; q != NULL; q = q->q_next)
		{
			if (bitset(QQUEUEUP, q->q_flags))
				q->q_flags |= QBADADDR;
		}
	}
	else if (TimeOuts.to_q_warning > 0 &&
	    curtime() > e->e_ctime + TimeOuts.to_q_warning)
	{
		if (!bitset(EF_WARNING|EF_RESPONSE, e->e_flags) &&
		    e->e_class >= 0 &&
		    strcmp(e->e_from.q_paddr, "<>") != 0)
		{
			(void) sprintf(buf,
				"warning: cannot send message for %s",
				pintvl(TimeOuts.to_q_warning, FALSE));
			if (e->e_message != NULL)
				free(e->e_message);
			e->e_message = newstr(buf);
			message(buf);
			e->e_flags |= EF_WARNING|EF_TIMEOUT;
		}
		fprintf(e->e_xfp,
			"Warning: message still undelivered after %s\n",
			pintvl(TimeOuts.to_q_warning, FALSE));
		fprintf(e->e_xfp, "Will keep trying until message is %s old\n",
			pintvl(TimeOuts.to_q_return, FALSE));
		for (q = e->e_sendqueue; q != NULL; q = q->q_next)
		{
			if (bitset(QQUEUEUP, q->q_flags))
				q->q_flags |= QREPORT;
		}
	}

	/*
	**  Send back return receipts as requested.
	*/

	if (e->e_receiptto != NULL && bitset(EF_SENDRECEIPT, e->e_flags))
	{
		auto ADDRESS *rlist = NULL;

		(void) sendtolist(e->e_receiptto, (ADDRESS *) NULL, &rlist, e);
		(void) returntosender("Return receipt", rlist, FALSE, e);
	}

	/*
	**  Arrange to send error messages if there are fatal errors.
	*/

	if (bitset(EF_FATALERRS|EF_TIMEOUT, e->e_flags) &&
	    e->e_errormode != EM_QUIET)
		savemail(e);

	/*
	**  Instantiate or deinstantiate the queue.
	*/

	if ((!queueit && !bitset(EF_KEEPQUEUE, e->e_flags)) ||
	    bitset(EF_CLRQUEUE, e->e_flags))
	{
		if (tTd(50, 2))
			printf("Dropping envelope\n");
		if (e->e_df != NULL)
			xunlink(e->e_df);
		xunlink(queuename(e, 'q'));
	}
	else if (queueit || !bitset(EF_INQUEUE, e->e_flags))
	{
#ifdef QUEUE
		queueup(e, FALSE, FALSE);
#else /* QUEUE */
		syserr("554 dropenvelope: queueup");
#endif /* QUEUE */
	}

	/* now unlock the job */
	closexscript(e);
	unlockqueue(e);

	/* make sure that this envelope is marked unused */
	if (e->e_dfp != NULL)
		(void) xfclose(e->e_dfp, "dropenvelope", e->e_df);
	e->e_dfp = NULL;
	e->e_id = e->e_df = NULL;

#ifdef LOG
	if (LogLevel > 74)
		syslog(LOG_INFO, "%s: done", id);
#endif /* LOG */
}
/*
**  CLEARENVELOPE -- clear an envelope without unlocking
**
**	This is normally used by a child process to get a clean
**	envelope without disturbing the parent.
**
**	Parameters:
**		e -- the envelope to clear.
**		fullclear - if set, the current envelope is total
**			garbage and should be ignored; otherwise,
**			release any resources it may indicate.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Closes files associated with the envelope.
**		Marks the envelope as unallocated.
*/

void
clearenvelope(e, fullclear)
	register ENVELOPE *e;
	bool fullclear;
{
	register HDR *bh;
	register HDR **nhp;
	extern ENVELOPE BlankEnvelope;

	if (!fullclear)
	{
		/* clear out any file information */
		if (e->e_xfp != NULL)
			(void) xfclose(e->e_xfp, "clearenvelope xfp", e->e_id);
		if (e->e_dfp != NULL)
			(void) xfclose(e->e_dfp, "clearenvelope dfp", e->e_df);
		e->e_xfp = e->e_dfp = NULL;
	}

	/* now clear out the data */
	STRUCTCOPY(BlankEnvelope, *e);
	if (Verbose)
		e->e_sendmode = SM_DELIVER;
	bh = BlankEnvelope.e_header;
	nhp = &e->e_header;
	while (bh != NULL)
	{
		*nhp = (HDR *) xalloc(sizeof *bh);
		bcopy((char *) bh, (char *) *nhp, sizeof *bh);
		bh = bh->h_link;
		nhp = &(*nhp)->h_link;
	}
}
/*
**  INITSYS -- initialize instantiation of system
**
**	In Daemon mode, this is done in the child.
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Initializes the system macros, some global variables,
**		etc.  In particular, the current time in various
**		forms is set.
*/

void
initsys(e)
	register ENVELOPE *e;
{
	static char cbuf[5];			/* holds hop count */
	static char pbuf[10];			/* holds pid */
#ifdef TTYNAME
	static char ybuf[60];			/* holds tty id */
	register char *p;
#endif /* TTYNAME */
	extern char *ttyname();
	extern void settime();
	extern char Version[];

	/*
	**  Give this envelope a reality.
	**	I.e., an id, a transcript, and a creation time.
	*/

	openxscript(e);
	e->e_ctime = curtime();

	/*
	**  Set OutChannel to something useful if stdout isn't it.
	**	This arranges that any extra stuff the mailer produces
	**	gets sent back to the user on error (because it is
	**	tucked away in the transcript).
	*/

	if (OpMode == MD_DAEMON && !bitset(EF_QUEUERUN, e->e_flags) &&
	    e->e_xfp != NULL)
		OutChannel = e->e_xfp;

	/*
	**  Set up some basic system macros.
	*/

	/* process id */
	(void) sprintf(pbuf, "%d", getpid());
	define('p', pbuf, e);

	/* hop count */
	(void) sprintf(cbuf, "%d", e->e_hopcount);
	define('c', cbuf, e);

	/* time as integer, unix time, arpa time */
	settime(e);

#ifdef TTYNAME
	/* tty name */
	if (macvalue('y', e) == NULL)
	{
		p = ttyname(2);
		if (p != NULL)
		{
			if (strrchr(p, '/') != NULL)
				p = strrchr(p, '/') + 1;
			(void) strcpy(ybuf, p);
			define('y', ybuf, e);
		}
	}
#endif /* TTYNAME */
}
/*
**  SETTIME -- set the current time.
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Sets the various time macros -- $a, $b, $d, $t.
*/

void
settime(e)
	register ENVELOPE *e;
{
	register char *p;
	auto time_t now;
	static char tbuf[20];			/* holds "current" time */
	static char dbuf[30];			/* holds ctime(tbuf) */
	register struct tm *tm;
	extern char *arpadate();
	extern struct tm *gmtime();

	now = curtime();
	tm = gmtime(&now);
	(void) sprintf(tbuf, "%04d%02d%02d%02d%02d", tm->tm_year + 1900,
			tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);
	define('t', tbuf, e);
	(void) strcpy(dbuf, ctime(&now));
	p = strchr(dbuf, '\n');
	if (p != NULL)
		*p = '\0';
	define('d', dbuf, e);
	p = newstr(arpadate(dbuf));
	if (macvalue('a', e) == NULL)
		define('a', p, e);
	define('b', p, e);
}
/*
**  OPENXSCRIPT -- Open transcript file
**
**	Creates a transcript file for possible eventual mailing or
**	sending back.
**
**	Parameters:
**		e -- the envelope to create the transcript in/for.
**
**	Returns:
**		none
**
**	Side Effects:
**		Creates the transcript file.
*/

#ifndef O_APPEND
#define O_APPEND	0
#endif

void
openxscript(e)
	register ENVELOPE *e;
{
	register char *p;
	int fd;

	if (e->e_xfp != NULL)
		return;
	p = queuename(e, 'x');
	fd = open(p, O_WRONLY|O_CREAT|O_APPEND, 0644);
	if (fd < 0)
	{
		syserr("Can't create transcript file %s", p);
		fd = open("/dev/null", O_WRONLY, 0644);
		if (fd < 0)
			syserr("!Can't open /dev/null");
	}
	e->e_xfp = fdopen(fd, "w");
}
/*
**  CLOSEXSCRIPT -- close the transcript file.
**
**	Parameters:
**		e -- the envelope containing the transcript to close.
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
*/

void
closexscript(e)
	register ENVELOPE *e;
{
	if (e->e_xfp == NULL)
		return;
	(void) xfclose(e->e_xfp, "closexscript", e->e_id);
	e->e_xfp = NULL;
}
/*
**  SETSENDER -- set the person who this message is from
**
**	Under certain circumstances allow the user to say who
**	s/he is (using -f or -r).  These are:
**	1.  The user's uid is zero (root).
**	2.  The user's login name is in an approved list (typically
**	    from a network server).
**	3.  The address the user is trying to claim has a
**	    "!" character in it (since #2 doesn't do it for
**	    us if we are dialing out for UUCP).
**	A better check to replace #3 would be if the
**	effective uid is "UUCP" -- this would require me
**	to rewrite getpwent to "grab" uucp as it went by,
**	make getname more nasty, do another passwd file
**	scan, or compile the UID of "UUCP" into the code,
**	all of which are reprehensible.
**
**	Assuming all of these fail, we figure out something
**	ourselves.
**
**	Parameters:
**		from -- the person we would like to believe this message
**			is from, as specified on the command line.
**		e -- the envelope in which we would like the sender set.
**		delimptr -- if non-NULL, set to the location of the
**			trailing delimiter.
**		internal -- set if this address is coming from an internal
**			source such as an owner alias.
**
**	Returns:
**		none.
**
**	Side Effects:
**		sets sendmail's notion of who the from person is.
*/

void
setsender(from, e, delimptr, internal)
	char *from;
	register ENVELOPE *e;
	char **delimptr;
	bool internal;
{
	register char **pvp;
	char *realname = NULL;
	register struct passwd *pw;
	char delimchar;
	char buf[MAXNAME];
	char pvpbuf[PSBUFSIZE];
	extern struct passwd *getpwnam();
	extern char *FullName;

	if (tTd(45, 1))
		printf("setsender(%s)\n", from == NULL ? "" : from);

	/*
	**  Figure out the real user executing us.
	**	Username can return errno != 0 on non-errors.
	*/

	if (bitset(EF_QUEUERUN, e->e_flags) || OpMode == MD_SMTP)
		realname = from;
	if (realname == NULL || realname[0] == '\0')
		realname = username();

	if (ConfigLevel < 2)
		SuprErrs = TRUE;

	delimchar = internal ? '\0' : ' ';
	if (from == NULL ||
	    parseaddr(from, &e->e_from, 1, delimchar, delimptr, e) == NULL)
	{
		/* log garbage addresses for traceback */
# ifdef LOG
		if (from != NULL && LogLevel > 2)
		{
			char *p;
			char ebuf[MAXNAME * 2 + 2];

			p = macvalue('_', e);
			if (p == NULL)
			{
				char *host = RealHostName;
				if (host == NULL)
					host = MyHostName;
				(void) sprintf(ebuf, "%s@%s", realname, host);
				p = ebuf;
			}
			syslog(LOG_NOTICE,
				"from=%s unparseable, received from %s",
				from, p);
		}
# endif /* LOG */
		if (from != NULL)
			SuprErrs = TRUE;
		if (from == realname ||
		    parseaddr(from = newstr(realname), &e->e_from, 1, ' ', NULL, e) == NULL)
		{
			SuprErrs = TRUE;
			if (parseaddr("postmaster", &e->e_from, 1, ' ', NULL, e) == NULL)
				syserr("553 setsender: can't even parse postmaster!");
		}
	}
	else
		FromFlag = TRUE;
	e->e_from.q_flags |= QDONTSEND;
	if (tTd(45, 5))
	{
		printf("setsender: QDONTSEND ");
		printaddr(&e->e_from, FALSE);
	}
	SuprErrs = FALSE;

	pvp = NULL;
	if (e->e_from.q_mailer == LocalMailer)
	{
# ifdef USERDB
		register char *p;
		extern char *udbsender();
# endif

		if (!internal)
		{
			/* if the user has given fullname already, don't redefine */
			if (FullName == NULL)
				FullName = macvalue('x', e);
			if (FullName != NULL && FullName[0] == '\0')
				FullName = NULL;

# ifdef USERDB
			p = udbsender(from);

			if (p != NULL)
			{
				/*
				**  We have an alternate address for the sender
				*/

				pvp = prescan(p, '\0', pvpbuf, NULL);
			}
# endif /* USERDB */
		}

		if ((pw = getpwnam(e->e_from.q_user)) != NULL)
		{
			/*
			**  Process passwd file entry.
			*/


			/* extract home directory */
			e->e_from.q_home = newstr(pw->pw_dir);
			define('z', e->e_from.q_home, e);

			/* extract user and group id */
			e->e_from.q_uid = pw->pw_uid;
			e->e_from.q_gid = pw->pw_gid;

			/* extract full name from passwd file */
			if (FullName == NULL && pw->pw_gecos != NULL &&
			    strcmp(pw->pw_name, e->e_from.q_user) == 0 &&
			    !internal)
			{
				buildfname(pw->pw_gecos, e->e_from.q_user, buf);
				if (buf[0] != '\0')
					FullName = newstr(buf);
			}
		}
		if (FullName != NULL && !internal)
			define('x', FullName, e);
	}
	else if (!internal)
	{
		if (e->e_from.q_home == NULL)
			e->e_from.q_home = getenv("HOME");
		e->e_from.q_uid = RealUid;
		e->e_from.q_gid = RealGid;
	}

	/*
	**  Rewrite the from person to dispose of possible implicit
	**	links in the net.
	*/

	if (pvp == NULL)
		pvp = prescan(from, '\0', pvpbuf, NULL);
	if (pvp == NULL)
	{
		/* don't need to give error -- prescan did that already */
# ifdef LOG
		if (LogLevel > 2)
			syslog(LOG_NOTICE, "cannot prescan from (%s)", from);
# endif
		finis();
	}
	(void) rewrite(pvp, 3, e);
	(void) rewrite(pvp, 1, e);
	(void) rewrite(pvp, 4, e);
	cataddr(pvp, NULL, buf, sizeof buf, '\0');
	e->e_sender = newstr(buf);
	define('f', e->e_sender, e);

	/* save the domain spec if this mailer wants it */
	if (!internal && e->e_from.q_mailer != NULL &&
	    bitnset(M_CANONICAL, e->e_from.q_mailer->m_flags))
	{
		extern char **copyplist();

		while (*pvp != NULL && strcmp(*pvp, "@") != 0)
			pvp++;
		if (*pvp != NULL)
			e->e_fromdomain = copyplist(pvp, TRUE);
	}
}
