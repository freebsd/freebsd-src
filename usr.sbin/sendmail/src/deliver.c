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
static char sccsid[] = "@(#)deliver.c	8.3 (Berkeley) 7/13/93";
#endif /* not lint */

#include "sendmail.h"
#include <signal.h>
#include <netdb.h>
#include <errno.h>
#ifdef NAMED_BIND
#include <arpa/nameser.h>
#include <resolv.h>

extern int	h_errno;
#endif

/*
**  SENDALL -- actually send all the messages.
**
**	Parameters:
**		e -- the envelope to send.
**		mode -- the delivery mode to use.  If SM_DEFAULT, use
**			the current e->e_sendmode.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Scans the send lists and sends everything it finds.
**		Delivers any appropriate error messages.
**		If we are running in a non-interactive mode, takes the
**			appropriate action.
*/

sendall(e, mode)
	ENVELOPE *e;
	char mode;
{
	register ADDRESS *q;
	char *owner;
	int otherowners;
	register ENVELOPE *ee;
	ENVELOPE *splitenv = NULL;
	bool announcequeueup;

	if (bitset(EF_FATALERRS, e->e_flags))
	{
		/* this will get a return message -- so don't send it */
		e->e_flags |= EF_CLRQUEUE;
		return;
	}

	/* determine actual delivery mode */
	if (mode == SM_DEFAULT)
	{
		mode = e->e_sendmode;
		if (mode != SM_VERIFY &&
		    shouldqueue(e->e_msgpriority, e->e_ctime))
			mode = SM_QUEUE;
		announcequeueup = mode == SM_QUEUE;
	}
	else
		announcequeueup = FALSE;

	if (tTd(13, 1))
	{
		printf("\nSENDALL: mode %c, e_from ", mode);
		printaddr(&e->e_from, FALSE);
		printf("sendqueue:\n");
		printaddr(e->e_sendqueue, TRUE);
	}

	/*
	**  Do any preprocessing necessary for the mode we are running.
	**	Check to make sure the hop count is reasonable.
	**	Delete sends to the sender in mailing lists.
	*/

	CurEnv = e;

	if (e->e_hopcount > MaxHopCount)
	{
		errno = 0;
		syserr("554 too many hops %d (%d max): from %s, to %s",
			e->e_hopcount, MaxHopCount, e->e_from.q_paddr,
			e->e_sendqueue->q_paddr);
		return;
	}

	/*
	**  Do sender deletion.
	**
	**	If the sender has the QQUEUEUP flag set, skip this.
	**	This can happen if the name server is hosed when you
	**	are trying to send mail.  The result is that the sender
	**	is instantiated in the queue as a recipient.
	*/

	if (!MeToo && !bitset(QQUEUEUP, e->e_from.q_flags))
	{
		if (tTd(13, 5))
		{
			printf("sendall: QDONTSEND ");
			printaddr(&e->e_from, FALSE);
		}
		e->e_from.q_flags |= QDONTSEND;
		(void) recipient(&e->e_from, &e->e_sendqueue, e);
	}

	/*
	**  Handle alias owners.
	**
	**	We scan up the q_alias chain looking for owners.
	**	We discard owners that are the same as the return path.
	*/

	for (q = e->e_sendqueue; q != NULL; q = q->q_next)
	{
		register struct address *a;

		for (a = q; a != NULL && a->q_owner == NULL; a = a->q_alias)
			continue;
		if (a != NULL)
			q->q_owner = a->q_owner;
				
		if (q->q_owner != NULL &&
		    !bitset(QDONTSEND, q->q_flags) &&
		    strcmp(q->q_owner, e->e_from.q_paddr) == 0)
			q->q_owner = NULL;
	}
		
	owner = "";
	otherowners = 1;
	while (owner != NULL && otherowners > 0)
	{
		owner = NULL;
		otherowners = 0;

		for (q = e->e_sendqueue; q != NULL; q = q->q_next)
		{
			if (bitset(QDONTSEND, q->q_flags))
				continue;

			if (q->q_owner != NULL)
			{
				if (owner == NULL)
					owner = q->q_owner;
				else if (owner != q->q_owner)
				{
					if (strcmp(owner, q->q_owner) == 0)
					{
						/* make future comparisons cheap */
						q->q_owner = owner;
					}
					else
					{
						otherowners++;
					}
					owner = q->q_owner;
				}
			}
			else
			{
				otherowners++;
			}
		}

		if (owner != NULL && otherowners > 0)
		{
			extern HDR *copyheader();
			extern ADDRESS *copyqueue();

			/*
			**  Split this envelope into two.
			*/

			ee = (ENVELOPE *) xalloc(sizeof(ENVELOPE));
			*ee = *e;
			ee->e_id = NULL;
			(void) queuename(ee, '\0');

			if (tTd(13, 1))
				printf("sendall: split %s into %s\n",
					e->e_id, ee->e_id);

			ee->e_header = copyheader(e->e_header);
			ee->e_sendqueue = copyqueue(e->e_sendqueue);
			ee->e_errorqueue = copyqueue(e->e_errorqueue);
			ee->e_flags = e->e_flags & ~(EF_INQUEUE|EF_CLRQUEUE|EF_FATALERRS);
			setsender(owner, ee, NULL, TRUE);
			if (tTd(13, 5))
			{
				printf("sendall(split): QDONTSEND ");
				printaddr(&ee->e_from, FALSE);
			}
			ee->e_from.q_flags |= QDONTSEND;
			ee->e_dfp = NULL;
			ee->e_xfp = NULL;
			ee->e_lockfp = NULL;
			ee->e_df = NULL;
			ee->e_errormode = EM_MAIL;
			ee->e_sibling = splitenv;
			splitenv = ee;
			
			for (q = e->e_sendqueue; q != NULL; q = q->q_next)
				if (q->q_owner == owner)
					q->q_flags |= QDONTSEND;
			for (q = ee->e_sendqueue; q != NULL; q = q->q_next)
				if (q->q_owner != owner)
					q->q_flags |= QDONTSEND;

			if (e->e_df != NULL && mode != SM_VERIFY)
			{
				ee->e_dfp = NULL;
				ee->e_df = newstr(queuename(ee, 'd'));
				if (link(e->e_df, ee->e_df) < 0)
				{
					syserr("sendall: link(%s, %s)",
						e->e_df, ee->e_df);
				}
			}

			if (mode != SM_VERIFY)
				openxscript(ee);
#ifdef LOG
			if (LogLevel > 4)
				syslog(LOG_INFO, "%s: clone %s",
					ee->e_id, e->e_id);
#endif
		}
	}

	if (owner != NULL)
	{
		setsender(owner, e, NULL, TRUE);
		if (tTd(13, 5))
		{
			printf("sendall(owner): QDONTSEND ");
			printaddr(&e->e_from, FALSE);
		}
		e->e_from.q_flags |= QDONTSEND;
		e->e_errormode = EM_MAIL;
	}

# ifdef QUEUE
	if ((mode == SM_QUEUE || mode == SM_FORK ||
	     (mode != SM_VERIFY && SuperSafe)) &&
	    !bitset(EF_INQUEUE, e->e_flags))
	{
		/* be sure everything is instantiated in the queue */
		queueup(e, TRUE, announcequeueup);
		for (ee = splitenv; ee != NULL; ee = ee->e_sibling)
			queueup(ee, TRUE, announcequeueup);
	}
#endif /* QUEUE */

	if (splitenv != NULL)
	{
		if (tTd(13, 1))
		{
			printf("\nsendall: Split queue; remaining queue:\n");
			printaddr(e->e_sendqueue, TRUE);
		}

		for (ee = splitenv; ee != NULL; ee = ee->e_sibling)
		{
			CurEnv = ee;
			sendenvelope(ee, mode);
		}

		CurEnv = e;
	}
	sendenvelope(e, mode);

	for (; splitenv != NULL; splitenv = splitenv->e_sibling)
		dropenvelope(splitenv);
}

sendenvelope(e, mode)
	register ENVELOPE *e;
	char mode;
{
	bool oldverbose;
	int pid;
	register ADDRESS *q;
#ifdef LOCKF
	struct flock lfd;
#endif

	oldverbose = Verbose;
	switch (mode)
	{
	  case SM_VERIFY:
		Verbose = TRUE;
		break;

	  case SM_QUEUE:
  queueonly:
		e->e_flags |= EF_INQUEUE|EF_KEEPQUEUE;
		return;

	  case SM_FORK:
		if (e->e_xfp != NULL)
			(void) fflush(e->e_xfp);

# ifdef LOCKF
		/*
		**  Since lockf has the interesting semantic that the
		**  lock is lost when we fork, we have to risk losing
		**  the lock here by closing before the fork, and then
		**  trying to get it back in the child.
		*/

		if (e->e_lockfp != NULL)
		{
			(void) xfclose(e->e_lockfp, "sendenvelope", "lockfp");
			e->e_lockfp = NULL;
		}
# endif /* LOCKF */

		pid = fork();
		if (pid < 0)
		{
			goto queueonly;
		}
		else if (pid > 0)
		{
			/* be sure we leave the temp files to our child */
			e->e_id = e->e_df = NULL;
# ifndef LOCKF
			if (e->e_lockfp != NULL)
			{
				(void) xfclose(e->e_lockfp, "sendenvelope", "lockfp");
				e->e_lockfp = NULL;
			}
# endif

			/* close any random open files in the envelope */
			if (e->e_dfp != NULL)
			{
				(void) xfclose(e->e_dfp, "sendenvelope", "dfp");
				e->e_dfp = NULL;
			}
			if (e->e_xfp != NULL)
			{
				(void) xfclose(e->e_xfp, "sendenvelope", "xfp");
				e->e_xfp = NULL;
			}
			return;
		}

		/* double fork to avoid zombies */
		if (fork() > 0)
			exit(EX_OK);

		/* be sure we are immune from the terminal */
		disconnect(FALSE, e);

# ifdef LOCKF
		/*
		**  Now try to get our lock back.
		*/

		lfd.l_type = F_WRLCK;
		lfd.l_whence = lfd.l_start = lfd.l_len = 0;
		e->e_lockfp = fopen(queuename(e, 'q'), "r+");
		if (e->e_lockfp == NULL ||
		    fcntl(fileno(e->e_lockfp), F_SETLK, &lfd) < 0)
		{
			/* oops....  lost it */
			if (tTd(13, 1))
				printf("sendenvelope: %s lost lock: lockfp=%x, %s\n",
					e->e_id, e->e_lockfp, errstring(errno));

# ifdef LOG
			if (LogLevel > 29)
				syslog(LOG_NOTICE, "%s: lost lock: %m",
					e->e_id);
# endif /* LOG */
			exit(EX_OK);
		}
# endif /* LOCKF */

		/*
		**  Close any cached connections.
		**
		**	We don't send the QUIT protocol because the parent
		**	still knows about the connection.
		**
		**	This should only happen when delivering an error
		**	message.
		*/

		mci_flush(FALSE, NULL);

		break;
	}

	/*
	**  Run through the list and send everything.
	*/

	e->e_nsent = 0;
	for (q = e->e_sendqueue; q != NULL; q = q->q_next)
	{
		if (mode == SM_VERIFY)
		{
			e->e_to = q->q_paddr;
			if (!bitset(QDONTSEND|QBADADDR, q->q_flags))
			{
				message("deliverable: mailer %s, host %s, user %s",
					q->q_mailer->m_name,
					q->q_host,
					q->q_user);
			}
		}
		else if (!bitset(QDONTSEND|QBADADDR, q->q_flags))
		{
# ifdef QUEUE
			/*
			**  Checkpoint the send list every few addresses
			*/

			if (e->e_nsent >= CheckpointInterval)
			{
				queueup(e, TRUE, FALSE);
				e->e_nsent = 0;
			}
# endif /* QUEUE */
			(void) deliver(e, q);
		}
	}
	Verbose = oldverbose;

	/*
	**  Now run through and check for errors.
	*/

	if (mode == SM_VERIFY)
	{
		return;
	}

	for (q = e->e_sendqueue; q != NULL; q = q->q_next)
	{
		if (tTd(13, 3))
		{
			printf("Checking ");
			printaddr(q, FALSE);
		}

		/* only send errors if the message failed */
		if (!bitset(QBADADDR, q->q_flags) ||
		    bitset(QDONTSEND, q->q_flags))
			continue;

		if (tTd(13, 3))
			printf("FATAL ERRORS\n");

		e->e_flags |= EF_FATALERRS;

		if (q->q_owner == NULL && strcmp(e->e_from.q_paddr, "<>") != 0)
			(void) sendtolist(e->e_from.q_paddr, NULL,
					  &e->e_errorqueue, e);
	}

	if (mode == SM_FORK)
		finis();
}
/*
**  DOFORK -- do a fork, retrying a couple of times on failure.
**
**	This MUST be a macro, since after a vfork we are running
**	two processes on the same stack!!!
**
**	Parameters:
**		none.
**
**	Returns:
**		From a macro???  You've got to be kidding!
**
**	Side Effects:
**		Modifies the ==> LOCAL <== variable 'pid', leaving:
**			pid of child in parent, zero in child.
**			-1 on unrecoverable error.
**
**	Notes:
**		I'm awfully sorry this looks so awful.  That's
**		vfork for you.....
*/

# define NFORKTRIES	5

# ifndef FORK
# define FORK	fork
# endif

# define DOFORK(fORKfN) \
{\
	register int i;\
\
	for (i = NFORKTRIES; --i >= 0; )\
	{\
		pid = fORKfN();\
		if (pid >= 0)\
			break;\
		if (i > 0)\
			sleep((unsigned) NFORKTRIES - i);\
	}\
}
/*
**  DOFORK -- simple fork interface to DOFORK.
**
**	Parameters:
**		none.
**
**	Returns:
**		pid of child in parent.
**		zero in child.
**		-1 on error.
**
**	Side Effects:
**		returns twice, once in parent and once in child.
*/

dofork()
{
	register int pid;

	DOFORK(fork);
	return (pid);
}
/*
**  DELIVER -- Deliver a message to a list of addresses.
**
**	This routine delivers to everyone on the same host as the
**	user on the head of the list.  It is clever about mailers
**	that don't handle multiple users.  It is NOT guaranteed
**	that it will deliver to all these addresses however -- so
**	deliver should be called once for each address on the
**	list.
**
**	Parameters:
**		e -- the envelope to deliver.
**		firstto -- head of the address list to deliver to.
**
**	Returns:
**		zero -- successfully delivered.
**		else -- some failure, see ExitStat for more info.
**
**	Side Effects:
**		The standard input is passed off to someone.
*/

deliver(e, firstto)
	register ENVELOPE *e;
	ADDRESS *firstto;
{
	char *host;			/* host being sent to */
	char *user;			/* user being sent to */
	char **pvp;
	register char **mvp;
	register char *p;
	register MAILER *m;		/* mailer for this recipient */
	ADDRESS *ctladdr;
	register MCI *mci;
	register ADDRESS *to = firstto;
	bool clever = FALSE;		/* running user smtp to this mailer */
	ADDRESS *tochain = NULL;	/* chain of users in this mailer call */
	int rcode;			/* response code */
	char *firstsig;			/* signature of firstto */
	int pid;
	char *curhost;
	int mpvect[2];
	int rpvect[2];
	char *pv[MAXPV+1];
	char tobuf[TOBUFSIZE];		/* text line of to people */
	char buf[MAXNAME];
	char rpathbuf[MAXNAME];		/* translated return path */
	extern int checkcompat();
	extern FILE *fdopen();

	errno = 0;
	if (bitset(QDONTSEND|QBADADDR|QQUEUEUP, to->q_flags))
		return (0);

#ifdef NAMED_BIND
	/* unless interactive, try twice, over a minute */
	if (OpMode == MD_DAEMON || OpMode == MD_SMTP) {
		_res.retrans = 30;
		_res.retry = 2;
	}
#endif 

	m = to->q_mailer;
	host = to->q_host;
	CurEnv = e;			/* just in case */
	e->e_statmsg = NULL;

	if (tTd(10, 1))
		printf("\n--deliver, mailer=%d, host=`%s', first user=`%s'\n",
			m->m_mno, host, to->q_user);

	/*
	**  If this mailer is expensive, and if we don't want to make
	**  connections now, just mark these addresses and return.
	**	This is useful if we want to batch connections to
	**	reduce load.  This will cause the messages to be
	**	queued up, and a daemon will come along to send the
	**	messages later.
	**		This should be on a per-mailer basis.
	*/

	if (NoConnect && !bitset(EF_QUEUERUN, e->e_flags) &&
	    bitnset(M_EXPENSIVE, m->m_flags) && !Verbose)
	{
		for (; to != NULL; to = to->q_next)
		{
			if (bitset(QDONTSEND|QBADADDR|QQUEUEUP, to->q_flags) ||
			    to->q_mailer != m)
				continue;
			to->q_flags |= QQUEUEUP|QDONTSEND;
			e->e_to = to->q_paddr;
			message("queued");
			if (LogLevel > 8)
				logdelivery(m, NULL, "queued", e);
		}
		e->e_to = NULL;
		return (0);
	}

	/*
	**  Do initial argv setup.
	**	Insert the mailer name.  Notice that $x expansion is
	**	NOT done on the mailer name.  Then, if the mailer has
	**	a picky -f flag, we insert it as appropriate.  This
	**	code does not check for 'pv' overflow; this places a
	**	manifest lower limit of 4 for MAXPV.
	**		The from address rewrite is expected to make
	**		the address relative to the other end.
	*/

	/* rewrite from address, using rewriting rules */
	rcode = EX_OK;
	(void) strcpy(rpathbuf, remotename(e->e_from.q_paddr, m,
					   RF_SENDERADDR|RF_CANONICAL,
					   &rcode, e));
	define('g', rpathbuf, e);		/* translated return path */
	define('h', host, e);			/* to host */
	Errors = 0;
	pvp = pv;
	*pvp++ = m->m_argv[0];

	/* insert -f or -r flag as appropriate */
	if (FromFlag && (bitnset(M_FOPT, m->m_flags) || bitnset(M_ROPT, m->m_flags)))
	{
		if (bitnset(M_FOPT, m->m_flags))
			*pvp++ = "-f";
		else
			*pvp++ = "-r";
		*pvp++ = newstr(rpathbuf);
	}

	/*
	**  Append the other fixed parts of the argv.  These run
	**  up to the first entry containing "$u".  There can only
	**  be one of these, and there are only a few more slots
	**  in the pv after it.
	*/

	for (mvp = m->m_argv; (p = *++mvp) != NULL; )
	{
		/* can't use strchr here because of sign extension problems */
		while (*p != '\0')
		{
			if ((*p++ & 0377) == MACROEXPAND)
			{
				if (*p == 'u')
					break;
			}
		}

		if (*p != '\0')
			break;

		/* this entry is safe -- go ahead and process it */
		expand(*mvp, buf, &buf[sizeof buf - 1], e);
		*pvp++ = newstr(buf);
		if (pvp >= &pv[MAXPV - 3])
		{
			syserr("554 Too many parameters to %s before $u", pv[0]);
			return (-1);
		}
	}

	/*
	**  If we have no substitution for the user name in the argument
	**  list, we know that we must supply the names otherwise -- and
	**  SMTP is the answer!!
	*/

	if (*mvp == NULL)
	{
		/* running SMTP */
# ifdef SMTP
		clever = TRUE;
		*pvp = NULL;
# else /* SMTP */
		/* oops!  we don't implement SMTP */
		syserr("554 SMTP style mailer");
		return (EX_SOFTWARE);
# endif /* SMTP */
	}

	/*
	**  At this point *mvp points to the argument with $u.  We
	**  run through our address list and append all the addresses
	**  we can.  If we run out of space, do not fret!  We can
	**  always send another copy later.
	*/

	tobuf[0] = '\0';
	e->e_to = tobuf;
	ctladdr = NULL;
	firstsig = hostsignature(firstto->q_mailer, firstto->q_host, e);
	for (; to != NULL; to = to->q_next)
	{
		/* avoid sending multiple recipients to dumb mailers */
		if (tobuf[0] != '\0' && !bitnset(M_MUSER, m->m_flags))
			break;

		/* if already sent or not for this host, don't send */
		if (bitset(QDONTSEND|QBADADDR|QQUEUEUP, to->q_flags) ||
		    to->q_mailer != firstto->q_mailer ||
		    strcmp(hostsignature(to->q_mailer, to->q_host, e), firstsig) != 0)
			continue;

		/* avoid overflowing tobuf */
		if (sizeof tobuf < (strlen(to->q_paddr) + strlen(tobuf) + 2))
			break;

		if (tTd(10, 1))
		{
			printf("\nsend to ");
			printaddr(to, FALSE);
		}

		/* compute effective uid/gid when sending */
		if (to->q_mailer == ProgMailer)
			ctladdr = getctladdr(to);

		user = to->q_user;
		e->e_to = to->q_paddr;
		if (tTd(10, 5))
		{
			printf("deliver: QDONTSEND ");
			printaddr(to, FALSE);
		}
		to->q_flags |= QDONTSEND;

		/*
		**  Check to see that these people are allowed to
		**  talk to each other.
		*/

		if (m->m_maxsize != 0 && e->e_msgsize > m->m_maxsize)
		{
			NoReturn = TRUE;
			usrerr("552 Message is too large; %ld bytes max", m->m_maxsize);
			giveresponse(EX_UNAVAILABLE, m, NULL, e);
			continue;
		}
		rcode = checkcompat(to, e);
		if (rcode != EX_OK)
		{
			markfailure(e, to, rcode);
			giveresponse(rcode, m, NULL, e);
			continue;
		}

		/*
		**  Strip quote bits from names if the mailer is dumb
		**	about them.
		*/

		if (bitnset(M_STRIPQ, m->m_flags))
		{
			stripquotes(user);
			stripquotes(host);
		}

		/* hack attack -- delivermail compatibility */
		if (m == ProgMailer && *user == '|')
			user++;

		/*
		**  If an error message has already been given, don't
		**	bother to send to this address.
		**
		**	>>>>>>>>>> This clause assumes that the local mailer
		**	>> NOTE >> cannot do any further aliasing; that
		**	>>>>>>>>>> function is subsumed by sendmail.
		*/

		if (bitset(QBADADDR|QQUEUEUP, to->q_flags))
			continue;

		/* save statistics.... */
		markstats(e, to);

		/*
		**  See if this user name is "special".
		**	If the user name has a slash in it, assume that this
		**	is a file -- send it off without further ado.  Note
		**	that this type of addresses is not processed along
		**	with the others, so we fudge on the To person.
		*/

		if (m == FileMailer)
		{
			rcode = mailfile(user, getctladdr(to), e);
			giveresponse(rcode, m, NULL, e);
			if (rcode == EX_OK)
				to->q_flags |= QSENT;
			continue;
		}

		/*
		**  Address is verified -- add this user to mailer
		**  argv, and add it to the print list of recipients.
		*/

		/* link together the chain of recipients */
		to->q_tchain = tochain;
		tochain = to;

		/* create list of users for error messages */
		(void) strcat(tobuf, ",");
		(void) strcat(tobuf, to->q_paddr);
		define('u', user, e);		/* to user */
		define('z', to->q_home, e);	/* user's home */

		/*
		**  Expand out this user into argument list.
		*/

		if (!clever)
		{
			expand(*mvp, buf, &buf[sizeof buf - 1], e);
			*pvp++ = newstr(buf);
			if (pvp >= &pv[MAXPV - 2])
			{
				/* allow some space for trailing parms */
				break;
			}
		}
	}

	/* see if any addresses still exist */
	if (tobuf[0] == '\0')
	{
		define('g', (char *) NULL, e);
		return (0);
	}

	/* print out messages as full list */
	e->e_to = tobuf + 1;

	/*
	**  Fill out any parameters after the $u parameter.
	*/

	while (!clever && *++mvp != NULL)
	{
		expand(*mvp, buf, &buf[sizeof buf - 1], e);
		*pvp++ = newstr(buf);
		if (pvp >= &pv[MAXPV])
			syserr("554 deliver: pv overflow after $u for %s", pv[0]);
	}
	*pvp++ = NULL;

	/*
	**  Call the mailer.
	**	The argument vector gets built, pipes
	**	are created as necessary, and we fork & exec as
	**	appropriate.
	**	If we are running SMTP, we just need to clean up.
	*/

	if (ctladdr == NULL && m != ProgMailer)
		ctladdr = &e->e_from;
#ifdef NAMED_BIND
	if (ConfigLevel < 2)
		_res.options &= ~(RES_DEFNAMES | RES_DNSRCH);	/* XXX */
#endif

	if (tTd(11, 1))
	{
		printf("openmailer:");
		printav(pv);
	}
	errno = 0;

	CurHostName = m->m_mailer;

	/*
	**  Deal with the special case of mail handled through an IPC
	**  connection.
	**	In this case we don't actually fork.  We must be
	**	running SMTP for this to work.  We will return a
	**	zero pid to indicate that we are running IPC.
	**  We also handle a debug version that just talks to stdin/out.
	*/

	curhost = NULL;

	/* check for Local Person Communication -- not for mortals!!! */
	if (strcmp(m->m_mailer, "[LPC]") == 0)
	{
		mci = (MCI *) xalloc(sizeof *mci);
		bzero((char *) mci, sizeof *mci);
		mci->mci_in = stdin;
		mci->mci_out = stdout;
		mci->mci_state = clever ? MCIS_OPENING : MCIS_OPEN;
		mci->mci_mailer = m;
	}
	else if (strcmp(m->m_mailer, "[IPC]") == 0 ||
		 strcmp(m->m_mailer, "[TCP]") == 0)
	{
#ifdef DAEMON
		register int i;
		register u_short port;

		CurHostName = pv[1];
		curhost = hostsignature(m, pv[1], e);

		if (curhost == NULL || curhost[0] == '\0')
		{
			syserr("null signature");
			rcode = EX_OSERR;
			goto give_up;
		}

		if (!clever)
		{
			syserr("554 non-clever IPC");
			rcode = EX_OSERR;
			goto give_up;
		}
		if (pv[2] != NULL)
			port = atoi(pv[2]);
		else
			port = 0;
tryhost:
		mci = NULL;
		while (*curhost != '\0')
		{
			register char *p;
			static char hostbuf[MAXNAME];

			mci = NULL;

			/* pull the next host from the signature */
			p = strchr(curhost, ':');
			if (p == NULL)
				p = &curhost[strlen(curhost)];
			strncpy(hostbuf, curhost, p - curhost);
			hostbuf[p - curhost] = '\0';
			if (*p != '\0')
				p++;
			curhost = p;

			/* see if we already know that this host is fried */
			CurHostName = hostbuf;
			mci = mci_get(hostbuf, m);
			if (mci->mci_state != MCIS_CLOSED)
			{
				if (tTd(11, 1))
				{
					printf("openmailer: ");
					mci_dump(mci);
				}
				CurHostName = mci->mci_host;
				break;
			}
			mci->mci_mailer = m;
			if (mci->mci_exitstat != EX_OK)
				continue;

			/* try the connection */
			setproctitle("%s %s: %s", e->e_id, hostbuf, "user open");
			message("Connecting to %s (%s)...",
				hostbuf, m->m_name);
			i = makeconnection(hostbuf, port, mci,
				bitnset(M_SECURE_PORT, m->m_flags));
			mci->mci_exitstat = i;
			mci->mci_errno = errno;
#ifdef NAMED_BIND
			mci->mci_herrno = h_errno;
#endif
			if (i == EX_OK)
			{
				mci->mci_state = MCIS_OPENING;
				mci_cache(mci);
				if (TrafficLogFile != NULL)
					fprintf(TrafficLogFile, "%05d == CONNECT %s\n",
						getpid(), hostbuf);
				break;
			}
			else if (tTd(11, 1))
				printf("openmailer: makeconnection => stat=%d, errno=%d\n",
					i, errno);


			/* enter status of this host */
			setstat(i);
		}
		mci->mci_pid = 0;
#else /* no DAEMON */
		syserr("554 openmailer: no IPC");
		if (tTd(11, 1))
			printf("openmailer: NULL\n");
		return NULL;
#endif /* DAEMON */
	}
	else
	{
#ifdef XDEBUG
		char wbuf[MAXLINE];

		/* make absolutely certain 0, 1, and 2 are in use */
		sprintf(wbuf, "%s... openmailer(%s)", e->e_to, m->m_name);
		checkfd012(wbuf);
#endif

		if (TrafficLogFile != NULL)
		{
			char **av;

			fprintf(TrafficLogFile, "%05d === EXEC", getpid());
			for (av = pv; *av != NULL; av++)
				fprintf(TrafficLogFile, " %s", *av);
			fprintf(TrafficLogFile, "\n");
		}

		/* create a pipe to shove the mail through */
		if (pipe(mpvect) < 0)
		{
			syserr("%s... openmailer(%s): pipe (to mailer)",
				e->e_to, m->m_name);
			if (tTd(11, 1))
				printf("openmailer: NULL\n");
			rcode = EX_OSERR;
			goto give_up;
		}

		/* if this mailer speaks smtp, create a return pipe */
		if (clever && pipe(rpvect) < 0)
		{
			syserr("%s... openmailer(%s): pipe (from mailer)",
				e->e_to, m->m_name);
			(void) close(mpvect[0]);
			(void) close(mpvect[1]);
			if (tTd(11, 1))
				printf("openmailer: NULL\n");
			rcode = EX_OSERR;
			goto give_up;
		}

		/*
		**  Actually fork the mailer process.
		**	DOFORK is clever about retrying.
		**
		**	Dispose of SIGCHLD signal catchers that may be laying
		**	around so that endmail will get it.
		*/

		if (e->e_xfp != NULL)
			(void) fflush(e->e_xfp);		/* for debugging */
		(void) fflush(stdout);
# ifdef SIGCHLD
		(void) signal(SIGCHLD, SIG_DFL);
# endif /* SIGCHLD */
		DOFORK(FORK);
		/* pid is set by DOFORK */
		if (pid < 0)
		{
			/* failure */
			syserr("%s... openmailer(%s): cannot fork",
				e->e_to, m->m_name);
			(void) close(mpvect[0]);
			(void) close(mpvect[1]);
			if (clever)
			{
				(void) close(rpvect[0]);
				(void) close(rpvect[1]);
			}
			if (tTd(11, 1))
				printf("openmailer: NULL\n");
			rcode = EX_OSERR;
			goto give_up;
		}
		else if (pid == 0)
		{
			int i;
			int saveerrno;
			char **ep;
			char *env[MAXUSERENVIRON];
			extern char **environ;
			extern int DtableSize;

			/* child -- set up input & exec mailer */
			/* make diagnostic output be standard output */
			(void) signal(SIGINT, SIG_IGN);
			(void) signal(SIGHUP, SIG_IGN);
			(void) signal(SIGTERM, SIG_DFL);

			/* close any other cached connections */
			mci_flush(FALSE, mci);

			/* move into some "safe" directory */
			if (m->m_execdir != NULL)
			{
				char *p, *q;
				char buf[MAXLINE];

				for (p = m->m_execdir; p != NULL; p = q)
				{
					q = strchr(p, ':');
					if (q != NULL)
						*q = '\0';
					expand(p, buf, &buf[sizeof buf] - 1, e);
					if (q != NULL)
						*q++ = ':';
					if (tTd(11, 20))
						printf("openmailer: trydir %s\n",
							buf);
					if (buf[0] != '\0' && chdir(buf) >= 0)
						break;
				}
			}

			/* arrange to filter std & diag output of command */
			if (clever)
			{
				(void) close(rpvect[0]);
				if (dup2(rpvect[1], STDOUT_FILENO) < 0)
				{
					syserr("%s... openmailer(%s): cannot dup pipe %d for stdout",
						e->e_to, m->m_name, rpvect[1]);
					_exit(EX_OSERR);
				}
				(void) close(rpvect[1]);
			}
			else if (OpMode == MD_SMTP || HoldErrs)
			{
				/* put mailer output in transcript */
				if (dup2(fileno(e->e_xfp), STDOUT_FILENO) < 0)
				{
					syserr("%s... openmailer(%s): cannot dup xscript %d for stdout",
						e->e_to, m->m_name,
						fileno(e->e_xfp));
					_exit(EX_OSERR);
				}
			}
			if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0)
			{
				syserr("%s... openmailer(%s): cannot dup stdout for stderr",
					e->e_to, m->m_name);
				_exit(EX_OSERR);
			}

			/* arrange to get standard input */
			(void) close(mpvect[1]);
			if (dup2(mpvect[0], STDIN_FILENO) < 0)
			{
				syserr("%s... openmailer(%s): cannot dup pipe %d for stdin",
					e->e_to, m->m_name, mpvect[0]);
				_exit(EX_OSERR);
			}
			(void) close(mpvect[0]);
			if (!bitnset(M_RESTR, m->m_flags))
			{
				if (ctladdr == NULL || ctladdr->q_uid == 0)
				{
					(void) setgid(DefGid);
					(void) initgroups(DefUser, DefGid);
					(void) setuid(DefUid);
				}
				else
				{
					(void) setgid(ctladdr->q_gid);
					(void) initgroups(ctladdr->q_ruser?
						ctladdr->q_ruser: ctladdr->q_user,
						ctladdr->q_gid);
					(void) setuid(ctladdr->q_uid);
				}
			}

			/* arrange for all the files to be closed */
			for (i = 3; i < DtableSize; i++)
			{
				register int j;
				if ((j = fcntl(i, F_GETFD, 0)) != -1)
					(void)fcntl(i, F_SETFD, j|1);
			}

			/* set up the mailer environment */
			i = 0;
			env[i++] = "AGENT=sendmail";
			for (ep = environ; *ep != NULL; ep++)
			{
				if (strncmp(*ep, "TZ=", 3) == 0)
					env[i++] = *ep;
			}
			env[i++] = NULL;

			/* try to execute the mailer */
			execve(m->m_mailer, pv, env);
			saveerrno = errno;
			syserr("Cannot exec %s", m->m_mailer);
			if (m == LocalMailer || transienterror(saveerrno))
				_exit(EX_OSERR);
			_exit(EX_UNAVAILABLE);
		}

		/*
		**  Set up return value.
		*/

		mci = (MCI *) xalloc(sizeof *mci);
		bzero((char *) mci, sizeof *mci);
		mci->mci_mailer = m;
		mci->mci_state = clever ? MCIS_OPENING : MCIS_OPEN;
		mci->mci_pid = pid;
		(void) close(mpvect[0]);
		mci->mci_out = fdopen(mpvect[1], "w");
		if (clever)
		{
			(void) close(rpvect[1]);
			mci->mci_in = fdopen(rpvect[0], "r");
		}
		else
		{
			mci->mci_flags |= MCIF_TEMP;
			mci->mci_in = NULL;
		}
	}

	/*
	**  If we are in SMTP opening state, send initial protocol.
	*/

	if (clever && mci->mci_state != MCIS_CLOSED)
	{
		smtpinit(m, mci, e);
	}
	if (tTd(11, 1))
	{
		printf("openmailer: ");
		mci_dump(mci);
	}

	if (mci->mci_state != MCIS_OPEN)
	{
		/* couldn't open the mailer */
		rcode = mci->mci_exitstat;
		errno = mci->mci_errno;
#ifdef NAMED_BIND
		h_errno = mci->mci_herrno;
#endif
		if (rcode == EX_OK)
		{
			/* shouldn't happen */
			syserr("554 deliver: rcode=%d, mci_state=%d, sig=%s",
				rcode, mci->mci_state, firstsig);
			rcode = EX_SOFTWARE;
		}
		else if (rcode == EX_TEMPFAIL && *curhost != '\0')
		{
			/* try next MX site */
			goto tryhost;
		}
	}
	else if (!clever)
	{
		/*
		**  Format and send message.
		*/

		putfromline(mci->mci_out, m, e);
		(*e->e_puthdr)(mci->mci_out, m, e);
		putline("\n", mci->mci_out, m);
		(*e->e_putbody)(mci->mci_out, m, e, NULL);

		/* get the exit status */
		rcode = endmailer(mci, e, pv);
	}
	else
#ifdef SMTP
	{
		/*
		**  Send the MAIL FROM: protocol
		*/

		rcode = smtpmailfrom(m, mci, e);
		if (rcode == EX_OK)
		{
			register char *t = tobuf;
			register int i;

			/* send the recipient list */
			tobuf[0] = '\0';
			for (to = tochain; to != NULL; to = to->q_tchain)
			{
				e->e_to = to->q_paddr;
				if ((i = smtprcpt(to, m, mci, e)) != EX_OK)
				{
					markfailure(e, to, i);
					giveresponse(i, m, mci, e);
				}
				else
				{
					*t++ = ',';
					for (p = to->q_paddr; *p; *t++ = *p++)
						continue;
				}
			}

			/* now send the data */
			if (tobuf[0] == '\0')
			{
				rcode = EX_OK;
				e->e_to = NULL;
				if (bitset(MCIF_CACHED, mci->mci_flags))
					smtprset(m, mci, e);
			}
			else
			{
				e->e_to = tobuf + 1;
				rcode = smtpdata(m, mci, e);
			}

			/* now close the connection */
			if (!bitset(MCIF_CACHED, mci->mci_flags))
				smtpquit(m, mci, e);
		}
		if (rcode != EX_OK && *curhost != '\0')
		{
			/* try next MX site */
			goto tryhost;
		}
	}
#else /* not SMTP */
	{
		syserr("554 deliver: need SMTP compiled to use clever mailer");
		rcode = EX_CONFIG;
		goto give_up;
	}
#endif /* SMTP */
#ifdef NAMED_BIND
	if (ConfigLevel < 2)
		_res.options |= RES_DEFNAMES | RES_DNSRCH;	/* XXX */
#endif

	/* arrange a return receipt if requested */
	if (e->e_receiptto != NULL && bitnset(M_LOCALMAILER, m->m_flags))
	{
		e->e_flags |= EF_SENDRECEIPT;
		/* do we want to send back more info? */
	}

	/*
	**  Do final status disposal.
	**	We check for something in tobuf for the SMTP case.
	**	If we got a temporary failure, arrange to queue the
	**		addressees.
	*/

  give_up:
	if (tobuf[0] != '\0')
		giveresponse(rcode, m, mci, e);
	for (to = tochain; to != NULL; to = to->q_tchain)
	{
		if (rcode != EX_OK)
			markfailure(e, to, rcode);
		else
		{
			to->q_flags |= QSENT;
			e->e_nsent++;
		}
	}

	/*
	**  Restore state and return.
	*/

	errno = 0;
	define('g', (char *) NULL, e);
	return (rcode);
}
/*
**  MARKFAILURE -- mark a failure on a specific address.
**
**	Parameters:
**		e -- the envelope we are sending.
**		q -- the address to mark.
**		rcode -- the code signifying the particular failure.
**
**	Returns:
**		none.
**
**	Side Effects:
**		marks the address (and possibly the envelope) with the
**			failure so that an error will be returned or
**			the message will be queued, as appropriate.
*/

markfailure(e, q, rcode)
	register ENVELOPE *e;
	register ADDRESS *q;
	int rcode;
{
	char buf[MAXLINE];

	if (rcode == EX_OK)
		return;
	else if (rcode == EX_TEMPFAIL)
		q->q_flags |= QQUEUEUP;
	else if (rcode != EX_IOERR && rcode != EX_OSERR)
		q->q_flags |= QBADADDR;
}
/*
**  ENDMAILER -- Wait for mailer to terminate.
**
**	We should never get fatal errors (e.g., segmentation
**	violation), so we report those specially.  For other
**	errors, we choose a status message (into statmsg),
**	and if it represents an error, we print it.
**
**	Parameters:
**		pid -- pid of mailer.
**		e -- the current envelope.
**		pv -- the parameter vector that invoked the mailer
**			(for error messages).
**
**	Returns:
**		exit code of mailer.
**
**	Side Effects:
**		none.
*/

endmailer(mci, e, pv)
	register MCI *mci;
	register ENVELOPE *e;
	char **pv;
{
	int st;

	/* close any connections */
	if (mci->mci_in != NULL)
		(void) xfclose(mci->mci_in, pv[0], "mci_in");
	if (mci->mci_out != NULL)
		(void) xfclose(mci->mci_out, pv[0], "mci_out");
	mci->mci_in = mci->mci_out = NULL;
	mci->mci_state = MCIS_CLOSED;

	/* in the IPC case there is nothing to wait for */
	if (mci->mci_pid == 0)
		return (EX_OK);

	/* wait for the mailer process to die and collect status */
	st = waitfor(mci->mci_pid);
	if (st == -1)
	{
		syserr("endmailer %s: wait", pv[0]);
		return (EX_SOFTWARE);
	}

	/* see if it died a horrid death */
	if ((st & 0377) != 0)
	{
		syserr("mailer %s died with signal %o", pv[0], st);

		/* log the arguments */
		if (e->e_xfp != NULL)
		{
			register char **av;

			fprintf(e->e_xfp, "Arguments:");
			for (av = pv; *av != NULL; av++)
				fprintf(e->e_xfp, " %s", *av);
			fprintf(e->e_xfp, "\n");
		}

		ExitStat = EX_TEMPFAIL;
		return (EX_TEMPFAIL);
	}

	/* normal death -- return status */
	st = (st >> 8) & 0377;
	return (st);
}
/*
**  GIVERESPONSE -- Interpret an error response from a mailer
**
**	Parameters:
**		stat -- the status code from the mailer (high byte
**			only; core dumps must have been taken care of
**			already).
**		m -- the mailer info for this mailer.
**		mci -- the mailer connection info -- can be NULL if the
**			response is given before the connection is made.
**		e -- the current envelope.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Errors may be incremented.
**		ExitStat may be set.
*/

giveresponse(stat, m, mci, e)
	int stat;
	register MAILER *m;
	register MCI *mci;
	ENVELOPE *e;
{
	register const char *statmsg;
	extern char *SysExMsg[];
	register int i;
	extern int N_SysEx;
	char buf[MAXLINE];

	/*
	**  Compute status message from code.
	*/

	i = stat - EX__BASE;
	if (stat == 0)
	{
		statmsg = "250 Sent";
		if (e->e_statmsg != NULL)
		{
			(void) sprintf(buf, "%s (%s)", statmsg, e->e_statmsg);
			statmsg = buf;
		}
	}
	else if (i < 0 || i > N_SysEx)
	{
		(void) sprintf(buf, "554 unknown mailer error %d", stat);
		stat = EX_UNAVAILABLE;
		statmsg = buf;
	}
	else if (stat == EX_TEMPFAIL)
	{
		(void) strcpy(buf, SysExMsg[i] + 1);
#ifdef NAMED_BIND
		if (h_errno == TRY_AGAIN)
			statmsg = errstring(h_errno+MAX_ERRNO);
		else
#endif
		{
			if (errno != 0)
				statmsg = errstring(errno);
			else
			{
#ifdef SMTP
				extern char SmtpError[];

				statmsg = SmtpError;
#else /* SMTP */
				statmsg = NULL;
#endif /* SMTP */
			}
		}
		if (statmsg != NULL && statmsg[0] != '\0')
		{
			(void) strcat(buf, ": ");
			(void) strcat(buf, statmsg);
		}
		statmsg = buf;
	}
#ifdef NAMED_BIND
	else if (stat == EX_NOHOST && h_errno != 0)
	{
		statmsg = errstring(h_errno + MAX_ERRNO);
		(void) sprintf(buf, "%s (%s)", SysExMsg[i], statmsg);
		statmsg = buf;
	}
#endif
	else
	{
		statmsg = SysExMsg[i];
		if (*statmsg++ == ':')
		{
			(void) sprintf(buf, "%s: %s", statmsg, errstring(errno));
			statmsg = buf;
		}
	}

	/*
	**  Print the message as appropriate
	*/

	if (stat == EX_OK || stat == EX_TEMPFAIL)
		message(&statmsg[4], errstring(errno));
	else
	{
		Errors++;
		usrerr(statmsg, errstring(errno));
	}

	/*
	**  Final cleanup.
	**	Log a record of the transaction.  Compute the new
	**	ExitStat -- if we already had an error, stick with
	**	that.
	*/

	if (LogLevel > ((stat == EX_TEMPFAIL) ? 8 : (stat == EX_OK) ? 7 : 6))
		logdelivery(m, mci, &statmsg[4], e);

	if (stat != EX_TEMPFAIL)
		setstat(stat);
	if (stat != EX_OK)
	{
		if (e->e_message != NULL)
			free(e->e_message);
		e->e_message = newstr(&statmsg[4]);
	}
	errno = 0;
#ifdef NAMED_BIND
	h_errno = 0;
#endif
}
/*
**  LOGDELIVERY -- log the delivery in the system log
**
**	Parameters:
**		m -- the mailer info.  Can be NULL for initial queue.
**		mci -- the mailer connection info -- can be NULL if the
**			log is occuring when no connection is active.
**		stat -- the message to print for the status.
**		e -- the current envelope.
**
**	Returns:
**		none
**
**	Side Effects:
**		none
*/

logdelivery(m, mci, stat, e)
	MAILER *m;
	register MCI *mci;
	char *stat;
	register ENVELOPE *e;
{
# ifdef LOG
	char buf[512];

	(void) sprintf(buf, "delay=%s", pintvl(curtime() - e->e_ctime, TRUE));

	if (m != NULL)
	{
		(void) strcat(buf, ", mailer=");
		(void) strcat(buf, m->m_name);
	}

	if (mci != NULL && mci->mci_host != NULL)
	{
# ifdef DAEMON
		extern SOCKADDR CurHostAddr;
# endif

		(void) strcat(buf, ", relay=");
		(void) strcat(buf, mci->mci_host);

# ifdef DAEMON
		(void) strcat(buf, " (");
		(void) strcat(buf, anynet_ntoa(&CurHostAddr));
		(void) strcat(buf, ")");
# endif
	}
	else
	{
		char *p = macvalue('h', e);

		if (p != NULL && p[0] != '\0')
		{
			(void) strcat(buf, ", relay=");
			(void) strcat(buf, p);
		}
	}
		
	syslog(LOG_INFO, "%s: to=%s, %s, stat=%s",
	       e->e_id, e->e_to, buf, stat);
# endif /* LOG */
}
/*
**  PUTFROMLINE -- output a UNIX-style from line (or whatever)
**
**	This can be made an arbitrary message separator by changing $l
**
**	One of the ugliest hacks seen by human eyes is contained herein:
**	UUCP wants those stupid "remote from <host>" lines.  Why oh why
**	does a well-meaning programmer such as myself have to deal with
**	this kind of antique garbage????
**
**	Parameters:
**		fp -- the file to output to.
**		m -- the mailer describing this entry.
**
**	Returns:
**		none
**
**	Side Effects:
**		outputs some text to fp.
*/

putfromline(fp, m, e)
	register FILE *fp;
	register MAILER *m;
	ENVELOPE *e;
{
	char *template = "\201l\n";
	char buf[MAXLINE];

	if (bitnset(M_NHDR, m->m_flags))
		return;

# ifdef UGLYUUCP
	if (bitnset(M_UGLYUUCP, m->m_flags))
	{
		char *bang;
		char xbuf[MAXLINE];

		expand("\201g", buf, &buf[sizeof buf - 1], e);
		bang = strchr(buf, '!');
		if (bang == NULL)
			syserr("554 No ! in UUCP! (%s)", buf);
		else
		{
			*bang++ = '\0';
			(void) sprintf(xbuf, "From %s  \201d remote from %s\n", bang, buf);
			template = xbuf;
		}
	}
# endif /* UGLYUUCP */
	expand(template, buf, &buf[sizeof buf - 1], e);
	putline(buf, fp, m);
}
/*
**  PUTBODY -- put the body of a message.
**
**	Parameters:
**		fp -- file to output onto.
**		m -- a mailer descriptor to control output format.
**		e -- the envelope to put out.
**		separator -- if non-NULL, a message separator that must
**			not be permitted in the resulting message.
**
**	Returns:
**		none.
**
**	Side Effects:
**		The message is written onto fp.
*/

putbody(fp, m, e, separator)
	FILE *fp;
	MAILER *m;
	register ENVELOPE *e;
	char *separator;
{
	char buf[MAXLINE];

	/*
	**  Output the body of the message
	*/

	if (e->e_dfp == NULL)
	{
		if (e->e_df != NULL)
		{
			e->e_dfp = fopen(e->e_df, "r");
			if (e->e_dfp == NULL)
				syserr("putbody: Cannot open %s for %s from %s",
				e->e_df, e->e_to, e->e_from);
		}
		else
			putline("<<< No Message Collected >>>", fp, m);
	}
	if (e->e_dfp != NULL)
	{
		rewind(e->e_dfp);
		while (!ferror(fp) && fgets(buf, sizeof buf, e->e_dfp) != NULL)
		{
			if (buf[0] == 'F' && bitnset(M_ESCFROM, m->m_flags) &&
			    strncmp(buf, "From ", 5) == 0)
				(void) putc('>', fp);
			if (buf[0] == '-' && buf[1] == '-' && separator != NULL)
			{
				/* possible separator */
				int sl = strlen(separator);

				if (strncmp(&buf[2], separator, sl) == 0)
					(void) putc(' ', fp);
			}
			putline(buf, fp, m);
		}

		if (ferror(e->e_dfp))
		{
			syserr("putbody: read error");
			ExitStat = EX_IOERR;
		}
	}

	/* some mailers want extra blank line at end of message */
	if (bitnset(M_BLANKEND, m->m_flags) && buf[0] != '\0' && buf[0] != '\n')
		putline("", fp, m);

	(void) fflush(fp);
	if (ferror(fp) && errno != EPIPE)
	{
		syserr("putbody: write error");
		ExitStat = EX_IOERR;
	}
	errno = 0;
}
/*
**  MAILFILE -- Send a message to a file.
**
**	If the file has the setuid/setgid bits set, but NO execute
**	bits, sendmail will try to become the owner of that file
**	rather than the real user.  Obviously, this only works if
**	sendmail runs as root.
**
**	This could be done as a subordinate mailer, except that it
**	is used implicitly to save messages in ~/dead.letter.  We
**	view this as being sufficiently important as to include it
**	here.  For example, if the system is dying, we shouldn't have
**	to create another process plus some pipes to save the message.
**
**	Parameters:
**		filename -- the name of the file to send to.
**		ctladdr -- the controlling address header -- includes
**			the userid/groupid to be when sending.
**
**	Returns:
**		The exit code associated with the operation.
**
**	Side Effects:
**		none.
*/

mailfile(filename, ctladdr, e)
	char *filename;
	ADDRESS *ctladdr;
	register ENVELOPE *e;
{
	register FILE *f;
	register int pid;
	int mode;

	if (tTd(11, 1))
	{
		printf("mailfile %s\n  ctladdr=", filename);
		printaddr(ctladdr, FALSE);
	}

	if (e->e_xfp != NULL)
		fflush(e->e_xfp);

	/*
	**  Fork so we can change permissions here.
	**	Note that we MUST use fork, not vfork, because of
	**	the complications of calling subroutines, etc.
	*/

	DOFORK(fork);

	if (pid < 0)
		return (EX_OSERR);
	else if (pid == 0)
	{
		/* child -- actually write to file */
		struct stat stb;

		(void) signal(SIGINT, SIG_DFL);
		(void) signal(SIGHUP, SIG_DFL);
		(void) signal(SIGTERM, SIG_DFL);
		(void) umask(OldUmask);

		if (stat(filename, &stb) < 0)
			stb.st_mode = FileMode;
		mode = stb.st_mode;

		/* limit the errors to those actually caused in the child */
		errno = 0;
		ExitStat = EX_OK;

		if (bitset(0111, stb.st_mode))
			exit(EX_CANTCREAT);
		if (ctladdr == NULL)
			ctladdr = &e->e_from;
		else
		{
			/* ignore setuid and setgid bits */
			mode &= ~(S_ISGID|S_ISUID);
		}

		/* we have to open the dfile BEFORE setuid */
		if (e->e_dfp == NULL && e->e_df != NULL)
		{
			e->e_dfp = fopen(e->e_df, "r");
			if (e->e_dfp == NULL)
			{
				syserr("mailfile: Cannot open %s for %s from %s",
					e->e_df, e->e_to, e->e_from);
			}
		}

		if (!bitset(S_ISGID, mode) || setgid(stb.st_gid) < 0)
		{
			if (ctladdr->q_uid == 0)
			{
				(void) setgid(DefGid);
				(void) initgroups(DefUser, DefGid);
			}
			else
			{
				(void) setgid(ctladdr->q_gid);
				(void) initgroups(ctladdr->q_ruser ?
					ctladdr->q_ruser : ctladdr->q_user,
					ctladdr->q_gid);
			}
		}
		if (!bitset(S_ISUID, mode) || setuid(stb.st_uid) < 0)
		{
			if (ctladdr->q_uid == 0)
				(void) setuid(DefUid);
			else
				(void) setuid(ctladdr->q_uid);
		}
		FileName = filename;
		LineNumber = 0;
		f = dfopen(filename, O_WRONLY|O_CREAT|O_APPEND, FileMode);
		if (f == NULL)
		{
			message("554 cannot open");
			exit(EX_CANTCREAT);
		}

		putfromline(f, FileMailer, e);
		(*e->e_puthdr)(f, FileMailer, e);
		putline("\n", f, FileMailer);
		(*e->e_putbody)(f, FileMailer, e, NULL);
		putline("\n", f, FileMailer);
		if (ferror(f))
		{
			message("451 I/O error");
			setstat(EX_IOERR);
		}
		(void) xfclose(f, "mailfile", filename);
		(void) fflush(stdout);

		/* reset ISUID & ISGID bits for paranoid systems */
		(void) chmod(filename, (int) stb.st_mode);
		exit(ExitStat);
		/*NOTREACHED*/
	}
	else
	{
		/* parent -- wait for exit status */
		int st;

		st = waitfor(pid);
		if ((st & 0377) != 0)
			return (EX_UNAVAILABLE);
		else
			return ((st >> 8) & 0377);
		/*NOTREACHED*/
	}
}
/*
**  HOSTSIGNATURE -- return the "signature" for a host.
**
**	The signature describes how we are going to send this -- it
**	can be just the hostname (for non-Internet hosts) or can be
**	an ordered list of MX hosts.
**
**	Parameters:
**		m -- the mailer describing this host.
**		host -- the host name.
**		e -- the current envelope.
**
**	Returns:
**		The signature for this host.
**
**	Side Effects:
**		Can tweak the symbol table.
*/

char *
hostsignature(m, host, e)
	register MAILER *m;
	char *host;
	ENVELOPE *e;
{
	register char *p;
	register STAB *s;
	int i;
	int len;
#ifdef NAMED_BIND
	int nmx;
	auto int rcode;
	char *hp;
	char *endp;
	int oldoptions;
	char *mxhosts[MAXMXHOSTS + 1];
#endif

	/*
	**  Check to see if this uses IPC -- if not, it can't have MX records.
	*/

	p = m->m_mailer;
	if (strcmp(p, "[IPC]") != 0 && strcmp(p, "[TCP]") != 0)
	{
		/* just an ordinary mailer */
		return host;
	}

	/*
	**  If it is a numeric address, just return it.
	*/

	if (host[0] == '[')
		return host;

	/*
	**  Look it up in the symbol table.
	*/

	s = stab(host, ST_HOSTSIG, ST_ENTER);
	if (s->s_hostsig != NULL)
		return s->s_hostsig;

	/*
	**  Not already there -- create a signature.
	*/

#ifdef NAMED_BIND
	if (ConfigLevel < 2)
	{
		oldoptions = _res.options;
		_res.options &= ~(RES_DEFNAMES | RES_DNSRCH);	/* XXX */
	}

	for (hp = host; hp != NULL; hp = endp)
	{
		endp = strchr(hp, ':');
		if (endp != NULL)
			*endp = '\0';

		nmx = getmxrr(hp, mxhosts, TRUE, &rcode);

		if (nmx <= 0)
		{
			register MCI *mci;
			extern int errno;

			/* update the connection info for this host */
			mci = mci_get(hp, m);
			mci->mci_exitstat = rcode;
			mci->mci_errno = errno;
#ifdef NAMED_BIND
			mci->mci_herrno = h_errno;
#endif

			/* and return the original host name as the signature */
			nmx = 1;
			mxhosts[0] = hp;
		}

		len = 0;
		for (i = 0; i < nmx; i++)
		{
			len += strlen(mxhosts[i]) + 1;
		}
		if (s->s_hostsig != NULL)
			len += strlen(s->s_hostsig) + 1;
		p = xalloc(len);
		if (s->s_hostsig != NULL)
		{
			(void) strcpy(p, s->s_hostsig);
			free(s->s_hostsig);
			s->s_hostsig = p;
			p += strlen(p);
			*p++ = ':';
		}
		else
			s->s_hostsig = p;
		for (i = 0; i < nmx; i++)
		{
			if (i != 0)
				*p++ = ':';
			strcpy(p, mxhosts[i]);
			p += strlen(p);
		}
		if (endp != NULL)
			*endp++ = ':';
	}
	makelower(s->s_hostsig);
	if (ConfigLevel < 2)
		_res.options = oldoptions;
#else
	/* not using BIND -- the signature is just the host name */
	s->s_hostsig = host;
#endif
	if (tTd(17, 1))
		printf("hostsignature(%s) = %s\n", host, s->s_hostsig);
	return s->s_hostsig;
}
