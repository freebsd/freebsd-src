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

# include "sendmail.h"

#ifndef lint
#ifdef QUEUE
static char sccsid[] = "@(#)queue.c	8.3 (Berkeley) 7/13/93 (with queueing)";
#else
static char sccsid[] = "@(#)queue.c	8.3 (Berkeley) 7/13/93 (without queueing)";
#endif
#endif /* not lint */

# include <signal.h>
# include <errno.h>
# include <pwd.h>
# include <dirent.h>

# ifdef QUEUE

/*
**  Work queue.
*/

struct work
{
	char		*w_name;	/* name of control file */
	long		w_pri;		/* priority of message, see below */
	time_t		w_ctime;	/* creation time of message */
	struct work	*w_next;	/* next in queue */
};

typedef struct work	WORK;

WORK	*WorkQ;			/* queue of things to be done */
/*
**  QUEUEUP -- queue a message up for future transmission.
**
**	Parameters:
**		e -- the envelope to queue up.
**		queueall -- if TRUE, queue all addresses, rather than
**			just those with the QQUEUEUP flag set.
**		announce -- if TRUE, tell when you are queueing up.
**
**	Returns:
**		none.
**
**	Side Effects:
**		The current request are saved in a control file.
**		The queue file is left locked.
*/

queueup(e, queueall, announce)
	register ENVELOPE *e;
	bool queueall;
	bool announce;
{
	char *qf;
	register FILE *tfp;
	register HDR *h;
	register ADDRESS *q;
	int fd;
	int i;
	bool newid;
	register char *p;
	MAILER nullmailer;
	char buf[MAXLINE], tf[MAXLINE];

	/*
	**  Create control file.
	*/

	newid = (e->e_id == NULL);
	strcpy(tf, queuename(e, 't'));
	tfp = e->e_lockfp;
	if (tfp == NULL)
		newid = FALSE;
	if (newid)
	{
		tfp = e->e_lockfp;
	}
	else
	{
		/* get a locked tf file */
		for (i = 100; --i >= 0; )
		{
			fd = open(tf, O_CREAT|O_WRONLY|O_EXCL, FileMode);
			if (fd < 0)
			{
				if (errno == EEXIST)
					continue;
notemp:
				syserr("!queueup: cannot create temp file %s", tf);
			}

			if (lockfile(fd, tf, LOCK_EX|LOCK_NB))
				break;

			close(fd);
			sleep(i);
		}
		if (fd < 0)
			goto notemp;

		tfp = fdopen(fd, "w");
	}

	if (tTd(40, 1))
		printf("queueing %s\n", e->e_id);

	/*
	**  If there is no data file yet, create one.
	*/

	if (e->e_df == NULL)
	{
		register FILE *dfp;
		extern putbody();

		e->e_df = newstr(queuename(e, 'd'));
		fd = open(e->e_df, O_WRONLY|O_CREAT, FileMode);
		if (fd < 0)
			syserr("!queueup: cannot create %s", e->e_df);
		dfp = fdopen(fd, "w");
		(*e->e_putbody)(dfp, FileMailer, e, NULL);
		(void) xfclose(dfp, "queueup dfp", e->e_id);
		e->e_putbody = putbody;
	}

	/*
	**  Output future work requests.
	**	Priority and creation time should be first, since
	**	they are required by orderq.
	*/

	/* output message priority */
	fprintf(tfp, "P%ld\n", e->e_msgpriority);

	/* output creation time */
	fprintf(tfp, "T%ld\n", e->e_ctime);

	/* output type and name of data file */
	if (e->e_bodytype != NULL)
		fprintf(tfp, "B%s\n", e->e_bodytype);
	fprintf(tfp, "D%s\n", e->e_df);

	/* message from envelope, if it exists */
	if (e->e_message != NULL)
		fprintf(tfp, "M%s\n", e->e_message);

	/* send various flag bits through */
	p = buf;
	if (bitset(EF_WARNING, e->e_flags))
		*p++ = 'w';
	if (bitset(EF_RESPONSE, e->e_flags))
		*p++ = 'r';
	*p++ = '\0';
	if (buf[0] != '\0')
		fprintf(tfp, "F%s\n", buf);

	/* $r and $s and $_ macro values */
	if ((p = macvalue('r', e)) != NULL)
		fprintf(tfp, "$r%s\n", p);
	if ((p = macvalue('s', e)) != NULL)
		fprintf(tfp, "$s%s\n", p);
	if ((p = macvalue('_', e)) != NULL)
		fprintf(tfp, "$_%s\n", p);

	/* output name of sender */
	fprintf(tfp, "S%s\n", e->e_from.q_paddr);

	/* output list of error recipients */
	printctladdr(NULL, NULL);
	for (q = e->e_errorqueue; q != NULL; q = q->q_next)
	{
		if (!bitset(QDONTSEND|QBADADDR, q->q_flags))
		{
			printctladdr(q, tfp);
			fprintf(tfp, "E%s\n", q->q_paddr);
		}
	}

	/* output list of recipient addresses */
	for (q = e->e_sendqueue; q != NULL; q = q->q_next)
	{
		if (bitset(QQUEUEUP, q->q_flags) ||
		    (queueall && !bitset(QDONTSEND|QBADADDR|QSENT, q->q_flags)))
		{
			printctladdr(q, tfp);
			fprintf(tfp, "R%s\n", q->q_paddr);
			if (announce)
			{
				e->e_to = q->q_paddr;
				message("queued");
				if (LogLevel > 8)
					logdelivery(NULL, NULL, "queued", e);
				e->e_to = NULL;
			}
			if (tTd(40, 1))
			{
				printf("queueing ");
				printaddr(q, FALSE);
			}
		}
	}

	/*
	**  Output headers for this message.
	**	Expand macros completely here.  Queue run will deal with
	**	everything as absolute headers.
	**		All headers that must be relative to the recipient
	**		can be cracked later.
	**	We set up a "null mailer" -- i.e., a mailer that will have
	**	no effect on the addresses as they are output.
	*/

	bzero((char *) &nullmailer, sizeof nullmailer);
	nullmailer.m_re_rwset = nullmailer.m_rh_rwset =
			nullmailer.m_se_rwset = nullmailer.m_sh_rwset = -1;
	nullmailer.m_eol = "\n";

	define('g', "\201f", e);
	for (h = e->e_header; h != NULL; h = h->h_link)
	{
		extern bool bitzerop();

		/* don't output null headers */
		if (h->h_value == NULL || h->h_value[0] == '\0')
			continue;

		/* don't output resent headers on non-resent messages */
		if (bitset(H_RESENT, h->h_flags) && !bitset(EF_RESENT, e->e_flags))
			continue;

		/* output this header */
		fprintf(tfp, "H");

		/* if conditional, output the set of conditions */
		if (!bitzerop(h->h_mflags) && bitset(H_CHECK|H_ACHECK, h->h_flags))
		{
			int j;

			(void) putc('?', tfp);
			for (j = '\0'; j <= '\177'; j++)
				if (bitnset(j, h->h_mflags))
					(void) putc(j, tfp);
			(void) putc('?', tfp);
		}

		/* output the header: expand macros, convert addresses */
		if (bitset(H_DEFAULT, h->h_flags))
		{
			(void) expand(h->h_value, buf, &buf[sizeof buf], e);
			fprintf(tfp, "%s: %s\n", h->h_field, buf);
		}
		else if (bitset(H_FROM|H_RCPT, h->h_flags))
		{
			bool oldstyle = bitset(EF_OLDSTYLE, e->e_flags);
			FILE *savetrace = TrafficLogFile;

			TrafficLogFile = NULL;

			if (bitset(H_FROM, h->h_flags))
				oldstyle = FALSE;

			commaize(h, h->h_value, tfp, oldstyle,
				 &nullmailer, e);

			TrafficLogFile = savetrace;
		}
		else
			fprintf(tfp, "%s: %s\n", h->h_field, h->h_value);
	}

	/*
	**  Clean up.
	*/

	fflush(tfp);
	fsync(fileno(tfp));
	if (ferror(tfp))
	{
		if (newid)
			syserr("!552 Error writing control file %s", tf);
		else
			syserr("!452 Error writing control file %s", tf);
	}

	if (!newid)
	{
		qf = queuename(e, 'q');
		if (rename(tf, qf) < 0)
			syserr("cannot rename(%s, %s), df=%s", tf, qf, e->e_df);
		if (e->e_lockfp != NULL)
			(void) xfclose(e->e_lockfp, "queueup lockfp", e->e_id);
		e->e_lockfp = tfp;
	}
	else
		qf = tf;
	errno = 0;

# ifdef LOG
	/* save log info */
	if (LogLevel > 79)
		syslog(LOG_DEBUG, "%s: queueup, qf=%s, df=%s\n", e->e_id, qf, e->e_df);
# endif /* LOG */
	return;
}

printctladdr(a, tfp)
	register ADDRESS *a;
	FILE *tfp;
{
	char *uname;
	register struct passwd *pw;
	register ADDRESS *q;
	uid_t uid;
	static ADDRESS *lastctladdr;
	static uid_t lastuid;

	/* initialization */
	if (a == NULL || tfp == NULL)
	{
		if (lastctladdr != NULL && tfp != NULL)
			fprintf(tfp, "C\n");
		lastctladdr = NULL;
		lastuid = 0;
		return;
	}

	/* find the active uid */
	q = getctladdr(a);
	if (q == NULL)
		uid = 0;
	else
		uid = q->q_uid;

	/* if a is an alias, use that for printing */
	if (a->q_alias != NULL)
		a = a->q_alias;

	/* check to see if this is the same as last time */
	if (lastctladdr != NULL && uid == lastuid &&
	    strcmp(lastctladdr->q_paddr, a->q_paddr) == 0)
		return;
	lastuid = uid;
	lastctladdr = a;

	if (uid == 0 || (pw = getpwuid(uid)) == NULL)
		uname = "";
	else
		uname = pw->pw_name;

	fprintf(tfp, "C%s:%s\n", uname, a->q_paddr);
}

/*
**  RUNQUEUE -- run the jobs in the queue.
**
**	Gets the stuff out of the queue in some presumably logical
**	order and processes them.
**
**	Parameters:
**		forkflag -- TRUE if the queue scanning should be done in
**			a child process.  We double-fork so it is not our
**			child and we don't have to clean up after it.
**
**	Returns:
**		none.
**
**	Side Effects:
**		runs things in the mail queue.
*/

ENVELOPE	QueueEnvelope;		/* the queue run envelope */

runqueue(forkflag)
	bool forkflag;
{
	register ENVELOPE *e;
	extern ENVELOPE BlankEnvelope;

	/*
	**  If no work will ever be selected, don't even bother reading
	**  the queue.
	*/

	CurrentLA = getla();	/* get load average */

	if (shouldqueue(0L, curtime()))
	{
		if (Verbose)
			printf("Skipping queue run -- load average too high\n");
		if (forkflag && QueueIntvl != 0)
			(void) setevent(QueueIntvl, runqueue, TRUE);
		return;
	}

	/*
	**  See if we want to go off and do other useful work.
	*/

	if (forkflag)
	{
		int pid;

		pid = dofork();
		if (pid != 0)
		{
			extern void reapchild();

			/* parent -- pick up intermediate zombie */
#ifndef SIGCHLD
			(void) waitfor(pid);
#else /* SIGCHLD */
			(void) signal(SIGCHLD, reapchild);
#endif /* SIGCHLD */
			if (QueueIntvl != 0)
				(void) setevent(QueueIntvl, runqueue, TRUE);
			return;
		}
		/* child -- double fork */
#ifndef SIGCHLD
		if (fork() != 0)
			exit(EX_OK);
#else /* SIGCHLD */
		(void) signal(SIGCHLD, SIG_DFL);
#endif /* SIGCHLD */
	}

	setproctitle("running queue: %s", QueueDir);

# ifdef LOG
	if (LogLevel > 69)
		syslog(LOG_DEBUG, "runqueue %s, pid=%d, forkflag=%d",
			QueueDir, getpid(), forkflag);
# endif /* LOG */

	/*
	**  Release any resources used by the daemon code.
	*/

# ifdef DAEMON
	clrdaemon();
# endif /* DAEMON */

	/*
	**  Create ourselves an envelope
	*/

	CurEnv = &QueueEnvelope;
	e = newenvelope(&QueueEnvelope, CurEnv);
	e->e_flags = BlankEnvelope.e_flags;

	/*
	**  Make sure the alias database is open.
	*/

	initmaps(FALSE, e);

	/*
	**  Start making passes through the queue.
	**	First, read and sort the entire queue.
	**	Then, process the work in that order.
	**		But if you take too long, start over.
	*/

	/* order the existing work requests */
	(void) orderq(FALSE);

	/* process them once at a time */
	while (WorkQ != NULL)
	{
		WORK *w = WorkQ;

		WorkQ = WorkQ->w_next;

		/*
		**  Ignore jobs that are too expensive for the moment.
		*/

		if (shouldqueue(w->w_pri, w->w_ctime))
		{
			if (Verbose)
				printf("\nSkipping %s\n", w->w_name + 2);
		}
		else
		{
			dowork(w->w_name + 2, ForkQueueRuns, FALSE, e);
		}
		free(w->w_name);
		free((char *) w);
	}

	/* exit without the usual cleanup */
	e->e_id = NULL;
	finis();
}
/*
**  ORDERQ -- order the work queue.
**
**	Parameters:
**		doall -- if set, include everything in the queue (even
**			the jobs that cannot be run because the load
**			average is too high).  Otherwise, exclude those
**			jobs.
**
**	Returns:
**		The number of request in the queue (not necessarily
**		the number of requests in WorkQ however).
**
**	Side Effects:
**		Sets WorkQ to the queue of available work, in order.
*/

# define NEED_P		001
# define NEED_T		002
# define NEED_R		004
# define NEED_S		010

orderq(doall)
	bool doall;
{
	register struct dirent *d;
	register WORK *w;
	DIR *f;
	register int i;
	WORK wlist[QUEUESIZE+1];
	int wn = -1;
	extern workcmpf();

	if (tTd(41, 1))
	{
		printf("orderq:\n");
		if (QueueLimitId != NULL)
			printf("\tQueueLimitId = %s\n", QueueLimitId);
		if (QueueLimitSender != NULL)
			printf("\tQueueLimitSender = %s\n", QueueLimitSender);
		if (QueueLimitRecipient != NULL)
			printf("\tQueueLimitRecipient = %s\n", QueueLimitRecipient);
	}

	/* clear out old WorkQ */
	for (w = WorkQ; w != NULL; )
	{
		register WORK *nw = w->w_next;

		WorkQ = nw;
		free(w->w_name);
		free((char *) w);
		w = nw;
	}

	/* open the queue directory */
	f = opendir(".");
	if (f == NULL)
	{
		syserr("orderq: cannot open \"%s\" as \".\"", QueueDir);
		return (0);
	}

	/*
	**  Read the work directory.
	*/

	while ((d = readdir(f)) != NULL)
	{
		FILE *cf;
		char lbuf[MAXNAME];
		extern bool strcontainedin();

		/* is this an interesting entry? */
		if (d->d_name[0] != 'q' || d->d_name[1] != 'f')
			continue;

		if (QueueLimitId != NULL &&
		    !strcontainedin(QueueLimitId, d->d_name))
			continue;

		/*
		**  Check queue name for plausibility.  This handles
		**  both old and new type ids.
		*/

		i = strlen(d->d_name);
		if (i != 9 && i != 10)
		{
			if (Verbose)
				printf("orderq: bogus qf name %s\n", d->d_name);
#ifdef LOG
			if (LogLevel > 3)
				syslog(LOG_CRIT, "orderq: bogus qf name %s",
					d->d_name);
#endif
			if (strlen(d->d_name) >= MAXNAME)
				d->d_name[MAXNAME - 1] = '\0';
			strcpy(lbuf, d->d_name);
			lbuf[0] = 'Q';
			(void) rename(d->d_name, lbuf);
			continue;
		}

		/* yes -- open control file (if not too many files) */
		if (++wn >= QUEUESIZE)
			continue;

		cf = fopen(d->d_name, "r");
		if (cf == NULL)
		{
			/* this may be some random person sending hir msgs */
			/* syserr("orderq: cannot open %s", cbuf); */
			if (tTd(41, 2))
				printf("orderq: cannot open %s (%d)\n",
					d->d_name, errno);
			errno = 0;
			wn--;
			continue;
		}
		w = &wlist[wn];
		w->w_name = newstr(d->d_name);

		/* make sure jobs in creation don't clog queue */
		w->w_pri = 0x7fffffff;
		w->w_ctime = 0;

		/* extract useful information */
		i = NEED_P | NEED_T;
		if (QueueLimitSender != NULL)
			i |= NEED_S;
		if (QueueLimitRecipient != NULL)
			i |= NEED_R;
		while (i != 0 && fgets(lbuf, sizeof lbuf, cf) != NULL)
		{
			extern long atol();
			extern bool strcontainedin();

			switch (lbuf[0])
			{
			  case 'P':
				w->w_pri = atol(&lbuf[1]);
				i &= ~NEED_P;
				break;

			  case 'T':
				w->w_ctime = atol(&lbuf[1]);
				i &= ~NEED_T;
				break;

			  case 'R':
				if (QueueLimitRecipient != NULL &&
				    strcontainedin(QueueLimitRecipient, &lbuf[1]))
					i &= ~NEED_R;
				break;

			  case 'S':
				if (QueueLimitSender != NULL &&
				    strcontainedin(QueueLimitSender, &lbuf[1]))
					i &= ~NEED_S;
				break;
			}
		}
		(void) fclose(cf);

		if ((!doall && shouldqueue(w->w_pri, w->w_ctime)) ||
		    bitset(NEED_R|NEED_S, i))
		{
			/* don't even bother sorting this job in */
			wn--;
		}
	}
	(void) closedir(f);
	wn++;

	/*
	**  Sort the work directory.
	*/

	qsort((char *) wlist, min(wn, QUEUESIZE), sizeof *wlist, workcmpf);

	/*
	**  Convert the work list into canonical form.
	**	Should be turning it into a list of envelopes here perhaps.
	*/

	WorkQ = NULL;
	for (i = min(wn, QUEUESIZE); --i >= 0; )
	{
		w = (WORK *) xalloc(sizeof *w);
		w->w_name = wlist[i].w_name;
		w->w_pri = wlist[i].w_pri;
		w->w_ctime = wlist[i].w_ctime;
		w->w_next = WorkQ;
		WorkQ = w;
	}

	if (tTd(40, 1))
	{
		for (w = WorkQ; w != NULL; w = w->w_next)
			printf("%32s: pri=%ld\n", w->w_name, w->w_pri);
	}

	return (wn);
}
/*
**  WORKCMPF -- compare function for ordering work.
**
**	Parameters:
**		a -- the first argument.
**		b -- the second argument.
**
**	Returns:
**		-1 if a < b
**		 0 if a == b
**		+1 if a > b
**
**	Side Effects:
**		none.
*/

workcmpf(a, b)
	register WORK *a;
	register WORK *b;
{
	long pa = a->w_pri;
	long pb = b->w_pri;

	if (pa == pb)
		return (0);
	else if (pa > pb)
		return (1);
	else
		return (-1);
}
/*
**  DOWORK -- do a work request.
**
**	Parameters:
**		id -- the ID of the job to run.
**		forkflag -- if set, run this in background.
**		requeueflag -- if set, reinstantiate the queue quickly.
**			This is used when expanding aliases in the queue.
**			If forkflag is also set, it doesn't wait for the
**			child.
**		e - the envelope in which to run it.
**
**	Returns:
**		none.
**
**	Side Effects:
**		The work request is satisfied if possible.
*/

dowork(id, forkflag, requeueflag, e)
	char *id;
	bool forkflag;
	bool requeueflag;
	register ENVELOPE *e;
{
	register int i;
	extern bool readqf();

	if (tTd(40, 1))
		printf("dowork(%s)\n", id);

	/*
	**  Fork for work.
	*/

	if (forkflag)
	{
		i = fork();
		if (i < 0)
		{
			syserr("dowork: cannot fork");
			return;
		}
	}
	else
	{
		i = 0;
	}

	if (i == 0)
	{
		/*
		**  CHILD
		**	Lock the control file to avoid duplicate deliveries.
		**		Then run the file as though we had just read it.
		**	We save an idea of the temporary name so we
		**		can recover on interrupt.
		*/

		/* set basic modes, etc. */
		(void) alarm(0);
		clearenvelope(e, FALSE);
		e->e_flags |= EF_QUEUERUN;
		e->e_errormode = EM_MAIL;
		e->e_id = id;
# ifdef LOG
		if (LogLevel > 76)
			syslog(LOG_DEBUG, "%s: dowork, pid=%d", e->e_id,
			       getpid());
# endif /* LOG */

		/* don't use the headers from sendmail.cf... */
		e->e_header = NULL;

		/* read the queue control file -- return if locked */
		if (!readqf(e))
		{
			if (tTd(40, 4))
				printf("readqf(%s) failed\n", e->e_id);
			if (forkflag)
				exit(EX_OK);
			else
				return;
		}

		e->e_flags |= EF_INQUEUE;
		eatheader(e, requeueflag);

		if (requeueflag)
			queueup(e, TRUE, FALSE);

		/* do the delivery */
		sendall(e, SM_DELIVER);

		/* finish up and exit */
		if (forkflag)
			finis();
		else
			dropenvelope(e);
	}
	else if (!requeueflag)
	{
		/*
		**  Parent -- pick up results.
		*/

		errno = 0;
		(void) waitfor(i);
	}
}
/*
**  READQF -- read queue file and set up environment.
**
**	Parameters:
**		e -- the envelope of the job to run.
**
**	Returns:
**		TRUE if it successfully read the queue file.
**		FALSE otherwise.
**
**	Side Effects:
**		The queue file is returned locked.
*/

bool
readqf(e)
	register ENVELOPE *e;
{
	register FILE *qfp;
	ADDRESS *ctladdr;
	struct stat st;
	char *bp;
	char qf[20];
	char buf[MAXLINE];
	extern long atol();
	extern ADDRESS *setctluser();

	/*
	**  Read and process the file.
	*/

	strcpy(qf, queuename(e, 'q'));
	qfp = fopen(qf, "r+");
	if (qfp == NULL)
	{
		if (tTd(40, 8))
			printf("readqf(%s): fopen failure (%s)\n",
				qf, errstring(errno));
		if (errno != ENOENT)
			syserr("readqf: no control file %s", qf);
		return FALSE;
	}

	/*
	**  Check the queue file for plausibility to avoid attacks.
	*/

	if (fstat(fileno(qfp), &st) < 0)
	{
		/* must have been being processed by someone else */
		if (tTd(40, 8))
			printf("readqf(%s): fstat failure (%s)\n",
				qf, errstring(errno));
		fclose(qfp);
		return FALSE;
	}

	if (st.st_uid != geteuid() || (st.st_mode & 07777) != FileMode)
	{
# ifdef LOG
		if (LogLevel > 0)
		{
			syslog(LOG_ALERT, "%s: bogus queue file, uid=%d, mode=%o",
				e->e_id, st.st_uid, st.st_mode);
		}
# endif /* LOG */
		if (tTd(40, 8))
			printf("readqf(%s): bogus file\n", qf);
		fclose(qfp);
		rename(qf, queuename(e, 'Q'));
		return FALSE;
	}

	if (!lockfile(fileno(qfp), qf, LOCK_EX|LOCK_NB))
	{
		/* being processed by another queuer */
		if (tTd(40, 8))
			printf("readqf(%s): locked\n", qf);
		if (Verbose)
			printf("%s: locked\n", e->e_id);
# ifdef LOG
		if (LogLevel > 19)
			syslog(LOG_DEBUG, "%s: locked", e->e_id);
# endif /* LOG */
		(void) fclose(qfp);
		return FALSE;
	}

	if (st.st_size == 0)
	{
		/* must be a bogus file -- just remove it */
		(void) unlink(qf);
		fclose(qfp);
		return FALSE;
	}

	/* save this lock */
	e->e_lockfp = qfp;

	/* do basic system initialization */
	initsys(e);

	FileName = qf;
	LineNumber = 0;
	if (Verbose)
		printf("\nRunning %s\n", e->e_id);
	ctladdr = NULL;
	while ((bp = fgetfolded(buf, sizeof buf, qfp)) != NULL)
	{
		register char *p;
		struct stat st;

		if (tTd(40, 4))
			printf("+++++ %s\n", bp);
		switch (bp[0])
		{
		  case 'C':		/* specify controlling user */
			ctladdr = setctluser(&bp[1]);
			break;

		  case 'R':		/* specify recipient */
			(void) sendtolist(&bp[1], ctladdr, &e->e_sendqueue, e);
			break;

		  case 'E':		/* specify error recipient */
			(void) sendtolist(&bp[1], ctladdr, &e->e_errorqueue, e);
			break;

		  case 'H':		/* header */
			(void) chompheader(&bp[1], FALSE, e);
			break;

		  case 'M':		/* message */
			e->e_message = newstr(&bp[1]);
			break;

		  case 'S':		/* sender */
			setsender(newstr(&bp[1]), e, NULL, TRUE);
			break;

		  case 'B':		/* body type */
			e->e_bodytype = newstr(&bp[1]);
			break;

		  case 'D':		/* data file name */
			e->e_df = newstr(&bp[1]);
			e->e_dfp = fopen(e->e_df, "r");
			if (e->e_dfp == NULL)
			{
				syserr("readqf: cannot open %s", e->e_df);
				e->e_msgsize = -1;
			}
			else if (fstat(fileno(e->e_dfp), &st) >= 0)
				e->e_msgsize = st.st_size;
			break;

		  case 'T':		/* init time */
			e->e_ctime = atol(&bp[1]);
			break;

		  case 'P':		/* message priority */
			e->e_msgpriority = atol(&bp[1]) + WkTimeFact;
			break;

		  case 'F':		/* flag bits */
			for (p = &bp[1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				  case 'w':	/* warning sent */
					e->e_flags |= EF_WARNING;
					break;

				  case 'r':	/* response */
					e->e_flags |= EF_RESPONSE;
					break;
				}
			}
			break;

		  case '$':		/* define macro */
			define(bp[1], newstr(&bp[2]), e);
			break;

		  case '\0':		/* blank line; ignore */
			break;

		  default:
			syserr("readqf: bad line \"%s\"", e->e_id,
				LineNumber, bp);
			fclose(qfp);
			rename(qf, queuename(e, 'Q'));
			return FALSE;
		}

		if (bp != buf)
			free(bp);
	}

	FileName = NULL;

	/*
	**  If we haven't read any lines, this queue file is empty.
	**  Arrange to remove it without referencing any null pointers.
	*/

	if (LineNumber == 0)
	{
		errno = 0;
		e->e_flags |= EF_CLRQUEUE | EF_FATALERRS | EF_RESPONSE;
	}
	return TRUE;
}
/*
**  PRINTQUEUE -- print out a representation of the mail queue
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Prints a listing of the mail queue on the standard output.
*/

printqueue()
{
	register WORK *w;
	FILE *f;
	int nrequests;
	char buf[MAXLINE];

	/*
	**  Check for permission to print the queue
	*/

	if (bitset(PRIV_RESTRMAILQ, PrivacyFlags) && RealUid != 0)
	{
		struct stat st;
# ifdef NGROUPS
		int n;
		int gidset[NGROUPS];
# endif

		if (stat(QueueDir, &st) < 0)
		{
			syserr("Cannot stat %s", QueueDir);
			return;
		}
# ifdef NGROUPS
		n = getgroups(NGROUPS, gidset);
		while (--n >= 0)
		{
			if (gidset[n] == st.st_gid)
				break;
		}
		if (n < 0)
# else
		if (RealGid != st.st_gid)
# endif
		{
			usrerr("510 You are not permitted to see the queue");
			setstat(EX_NOPERM);
			return;
		}
	}

	/*
	**  Read and order the queue.
	*/

	nrequests = orderq(TRUE);

	/*
	**  Print the work list that we have read.
	*/

	/* first see if there is anything */
	if (nrequests <= 0)
	{
		printf("Mail queue is empty\n");
		return;
	}

	CurrentLA = getla();	/* get load average */

	printf("\t\tMail Queue (%d request%s", nrequests, nrequests == 1 ? "" : "s");
	if (nrequests > QUEUESIZE)
		printf(", only %d printed", QUEUESIZE);
	if (Verbose)
		printf(")\n--Q-ID-- --Size-- -Priority- ---Q-Time--- -----------Sender/Recipient-----------\n");
	else
		printf(")\n--Q-ID-- --Size-- -----Q-Time----- ------------Sender/Recipient------------\n");
	for (w = WorkQ; w != NULL; w = w->w_next)
	{
		struct stat st;
		auto time_t submittime = 0;
		long dfsize = -1;
		int flags = 0;
		char message[MAXLINE];
		char bodytype[MAXNAME];

		f = fopen(w->w_name, "r");
		if (f == NULL)
		{
			errno = 0;
			continue;
		}
		printf("%8s", w->w_name + 2);
		if (!lockfile(fileno(f), w->w_name, LOCK_SH|LOCK_NB))
			printf("*");
		else if (shouldqueue(w->w_pri, w->w_ctime))
			printf("X");
		else
			printf(" ");
		errno = 0;

		message[0] = bodytype[0] = '\0';
		while (fgets(buf, sizeof buf, f) != NULL)
		{
			register int i;
			register char *p;

			fixcrlf(buf, TRUE);
			switch (buf[0])
			{
			  case 'M':	/* error message */
				if ((i = strlen(&buf[1])) >= sizeof message)
					i = sizeof message - 1;
				bcopy(&buf[1], message, i);
				message[i] = '\0';
				break;

			  case 'B':	/* body type */
				if ((i = strlen(&buf[1])) >= sizeof bodytype)
					i = sizeof bodytype - 1;
				bcopy(&buf[1], bodytype, i);
				bodytype[i] = '\0';
				break;

			  case 'S':	/* sender name */
				if (Verbose)
					printf("%8ld %10ld%c%.12s %.38s",
					    dfsize,
					    w->w_pri,
					    bitset(EF_WARNING, flags) ? '+' : ' ',
					    ctime(&submittime) + 4,
					    &buf[1]);
				else
					printf("%8ld %.16s %.45s", dfsize,
					    ctime(&submittime), &buf[1]);
				if (message[0] != '\0' || bodytype[0] != '\0')
				{
					printf("\n    %10.10s", bodytype);
					if (message[0] != '\0')
						printf("   (%.60s)", message);
				}
				break;

			  case 'C':	/* controlling user */
				if (Verbose)
					printf("\n\t\t\t\t      (---%.34s---)",
						&buf[1]);
				break;

			  case 'R':	/* recipient name */
				if (Verbose)
					printf("\n\t\t\t\t\t  %.38s", &buf[1]);
				else
					printf("\n\t\t\t\t   %.45s", &buf[1]);
				break;

			  case 'T':	/* creation time */
				submittime = atol(&buf[1]);
				break;

			  case 'D':	/* data file name */
				if (stat(&buf[1], &st) >= 0)
					dfsize = st.st_size;
				break;

			  case 'F':	/* flag bits */
				for (p = &buf[1]; *p != '\0'; p++)
				{
					switch (*p)
					{
					  case 'w':
						flags |= EF_WARNING;
						break;
					}
				}
			}
		}
		if (submittime == (time_t) 0)
			printf(" (no control file)");
		printf("\n");
		(void) fclose(f);
	}
}

# endif /* QUEUE */
/*
**  QUEUENAME -- build a file name in the queue directory for this envelope.
**
**	Assigns an id code if one does not already exist.
**	This code is very careful to avoid trashing existing files
**	under any circumstances.
**
**	Parameters:
**		e -- envelope to build it in/from.
**		type -- the file type, used as the first character
**			of the file name.
**
**	Returns:
**		a pointer to the new file name (in a static buffer).
**
**	Side Effects:
**		If no id code is already assigned, queuename will
**		assign an id code, create a qf file, and leave a
**		locked, open-for-write file pointer in the envelope.
*/

char *
queuename(e, type)
	register ENVELOPE *e;
	int type;
{
	static int pid = -1;
	static char c0;
	static char c1;
	static char c2;
	time_t now;
	struct tm *tm;
	static char buf[MAXNAME];

	if (e->e_id == NULL)
	{
		char qf[20];

		/* find a unique id */
		if (pid != getpid())
		{
			/* new process -- start back at "AA" */
			pid = getpid();
			now = curtime();
			tm = localtime(&now);
			c0 = 'A' + tm->tm_hour;
			c1 = 'A';
			c2 = 'A' - 1;
		}
		(void) sprintf(qf, "qf%cAA%05d", c0, pid);

		while (c1 < '~' || c2 < 'Z')
		{
			int i;

			if (c2 >= 'Z')
			{
				c1++;
				c2 = 'A' - 1;
			}
			qf[3] = c1;
			qf[4] = ++c2;
			if (tTd(7, 20))
				printf("queuename: trying \"%s\"\n", qf);

			i = open(qf, O_WRONLY|O_CREAT|O_EXCL, FileMode);
			if (i < 0)
			{
				if (errno == EEXIST)
					continue;
				syserr("queuename: Cannot create \"%s\" in \"%s\"",
					qf, QueueDir);
				exit(EX_UNAVAILABLE);
			}
			if (lockfile(i, qf, LOCK_EX|LOCK_NB))
			{
				e->e_lockfp = fdopen(i, "w");
				break;
			}

			/* a reader got the file; abandon it and try again */
			(void) close(i);
		}
		if (c1 >= '~' && c2 >= 'Z')
		{
			syserr("queuename: Cannot create \"%s\" in \"%s\"",
				qf, QueueDir);
			exit(EX_OSERR);
		}
		e->e_id = newstr(&qf[2]);
		define('i', e->e_id, e);
		if (tTd(7, 1))
			printf("queuename: assigned id %s, env=%x\n", e->e_id, e);
# ifdef LOG
		if (LogLevel > 93)
			syslog(LOG_DEBUG, "%s: assigned id", e->e_id);
# endif /* LOG */
	}

	if (type == '\0')
		return (NULL);
	(void) sprintf(buf, "%cf%s", type, e->e_id);
	if (tTd(7, 2))
		printf("queuename: %s\n", buf);
	return (buf);
}
/*
**  UNLOCKQUEUE -- unlock the queue entry for a specified envelope
**
**	Parameters:
**		e -- the envelope to unlock.
**
**	Returns:
**		none
**
**	Side Effects:
**		unlocks the queue for `e'.
*/

unlockqueue(e)
	ENVELOPE *e;
{
	if (tTd(51, 4))
		printf("unlockqueue(%s)\n", e->e_id);

	/* if there is a lock file in the envelope, close it */
	if (e->e_lockfp != NULL)
		xfclose(e->e_lockfp, "unlockqueue", e->e_id);
	e->e_lockfp = NULL;

	/* don't create a queue id if we don't already have one */
	if (e->e_id == NULL)
		return;

	/* remove the transcript */
# ifdef LOG
	if (LogLevel > 87)
		syslog(LOG_DEBUG, "%s: unlock", e->e_id);
# endif /* LOG */
	if (!tTd(51, 104))
		xunlink(queuename(e, 'x'));

}
/*
**  SETCTLUSER -- create a controlling address
**
**	Create a fake "address" given only a local login name; this is
**	used as a "controlling user" for future recipient addresses.
**
**	Parameters:
**		user -- the user name of the controlling user.
**
**	Returns:
**		An address descriptor for the controlling user.
**
**	Side Effects:
**		none.
*/

ADDRESS *
setctluser(user)
	char *user;
{
	register ADDRESS *a;
	struct passwd *pw;
	char *p;

	/*
	**  See if this clears our concept of controlling user.
	*/

	if (user == NULL)
		user = "";

	/*
	**  Set up addr fields for controlling user.
	*/

	a = (ADDRESS *) xalloc(sizeof *a);
	bzero((char *) a, sizeof *a);

	p = strchr(user, ':');
	if (p != NULL)
		*p++ = '\0';
	if (*user != '\0' && (pw = getpwnam(user)) != NULL)
	{
		a->q_home = newstr(pw->pw_dir);
		a->q_uid = pw->pw_uid;
		a->q_gid = pw->pw_gid;
		a->q_user = newstr(user);
		a->q_flags |= QGOODUID;
	}
	else
	{
		a->q_user = newstr(DefUser);
	}

	a->q_flags |= QPRIMARY;		/* flag as a "ctladdr"  */
	a->q_mailer = LocalMailer;
	if (p == NULL)
		a->q_paddr = a->q_user;
	else
		a->q_paddr = newstr(p);
	return a;
}
