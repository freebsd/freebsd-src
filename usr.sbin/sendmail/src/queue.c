/*
 * Copyright (c) 1983, 1995, 1996 Eric P. Allman
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
#if QUEUE
static char sccsid[] = "@(#)queue.c	8.153 (Berkeley) 1/14/97 (with queueing)";
#else
static char sccsid[] = "@(#)queue.c	8.153 (Berkeley) 1/14/97 (without queueing)";
#endif
#endif /* not lint */

# include <errno.h>
# include <dirent.h>

# if QUEUE

/*
**  Work queue.
*/

struct work
{
	char		*w_name;	/* name of control file */
	char		*w_host;	/* name of recipient host */
	bool		w_lock;		/* is message locked? */
	bool		w_tooyoung;	/* is it too young to run? */
	long		w_pri;		/* priority of message, see below */
	time_t		w_ctime;	/* creation time of message */
	struct work	*w_next;	/* next in queue */
};

typedef struct work	WORK;

WORK	*WorkQ;			/* queue of things to be done */

#define QF_VERSION	2	/* version number of this queue format */

extern int orderq __P((bool));
/*
**  QUEUEUP -- queue a message up for future transmission.
**
**	Parameters:
**		e -- the envelope to queue up.
**		announce -- if TRUE, tell when you are queueing up.
**
**	Returns:
**		none.
**
**	Side Effects:
**		The current request are saved in a control file.
**		The queue file is left locked.
*/

void
queueup(e, announce)
	register ENVELOPE *e;
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
	MCI mcibuf;
	char buf[MAXLINE], tf[MAXLINE];
	extern void printctladdr __P((ADDRESS *, FILE *));

	/*
	**  Create control file.
	*/

	newid = (e->e_id == NULL) || !bitset(EF_INQUEUE, e->e_flags);

	/* if newid, queuename will create a locked qf file in e->lockfp */
	strcpy(tf, queuename(e, 't'));
	tfp = e->e_lockfp;
	if (tfp == NULL)
		newid = FALSE;

	/* if newid, just write the qf file directly (instead of tf file) */
	if (!newid)
	{
		/* get a locked tf file */
		for (i = 0; i < 128; i++)
		{
			fd = open(tf, O_CREAT|O_WRONLY|O_EXCL, FileMode);
			if (fd < 0)
			{
				if (errno != EEXIST)
					break;
#ifdef LOG
				if (LogLevel > 0 && (i % 32) == 0)
					syslog(LOG_ALERT, "queueup: cannot create %s, uid=%d: %s",
						tf, geteuid(), errstring(errno));
#endif
			}
			else
			{
				if (lockfile(fd, tf, NULL, LOCK_EX|LOCK_NB))
					break;
#ifdef LOG
				else if (LogLevel > 0 && (i % 32) == 0)
					syslog(LOG_ALERT, "queueup: cannot lock %s: %s",
						tf, errstring(errno));
#endif
				close(fd);
			}

			if ((i % 32) == 31)
			{
				/* save the old temp file away */
				(void) rename(tf, queuename(e, 'T'));
			}
			else
				sleep(i % 32);
		}
		if (fd < 0 || (tfp = fdopen(fd, "w")) == NULL)
		{
			printopenfds(TRUE);
			syserr("!queueup: cannot create queue temp file %s, uid=%d",
				tf, geteuid());
		}
	}

	if (tTd(40, 1))
		printf("\n>>>>> queueing %s%s >>>>>\n", e->e_id,
			newid ? " (new id)" : "");
	if (tTd(40, 3))
	{
		extern void printenvflags();

		printf("  e_flags=");
		printenvflags(e);
	}
	if (tTd(40, 32))
	{
		printf("  sendq=");
		printaddr(e->e_sendqueue, TRUE);
	}
	if (tTd(40, 9))
	{
		printf("  tfp=");
		dumpfd(fileno(tfp), TRUE, FALSE);
		printf("  lockfp=");
		if (e->e_lockfp == NULL)
			printf("NULL\n");
		else
			dumpfd(fileno(e->e_lockfp), TRUE, FALSE);
	}

	/*
	**  If there is no data file yet, create one.
	*/

	if (!bitset(EF_HAS_DF, e->e_flags))
	{
		register FILE *dfp = NULL;
		char dfname[20];
		struct stat stbuf;

		strcpy(dfname, queuename(e, 'd'));
		fd = open(dfname, O_WRONLY|O_CREAT|O_TRUNC, FileMode);
		if (fd < 0 || (dfp = fdopen(fd, "w")) == NULL)
			syserr("!queueup: cannot create data temp file %s, uid=%d",
				dfname, geteuid());
		if (fstat(fd, &stbuf) < 0)
			e->e_dfino = -1;
		else
		{
			e->e_dfdev = stbuf.st_dev;
			e->e_dfino = stbuf.st_ino;
		}
		e->e_flags |= EF_HAS_DF;
		bzero(&mcibuf, sizeof mcibuf);
		mcibuf.mci_out = dfp;
		mcibuf.mci_mailer = FileMailer;
		(*e->e_putbody)(&mcibuf, e, NULL);
		(void) xfclose(dfp, "queueup dfp", e->e_id);
		e->e_putbody = putbody;
	}

	/*
	**  Output future work requests.
	**	Priority and creation time should be first, since
	**	they are required by orderq.
	*/

	/* output queue version number (must be first!) */
	fprintf(tfp, "V%d\n", QF_VERSION);

	/* output creation time */
	fprintf(tfp, "T%ld\n", e->e_ctime);

	/* output last delivery time */
	fprintf(tfp, "K%ld\n", e->e_dtime);

	/* output number of delivery attempts */
	fprintf(tfp, "N%d\n", e->e_ntries);

	/* output message priority */
	fprintf(tfp, "P%ld\n", e->e_msgpriority);

	/* output inode number of data file */
	/* XXX should probably include device major/minor too */
	if (e->e_dfino != -1)
		fprintf(tfp, "I%d/%d/%ld\n",
			major(e->e_dfdev), minor(e->e_dfdev), e->e_dfino);

	/* output body type */
	if (e->e_bodytype != NULL)
		fprintf(tfp, "B%s\n", denlstring(e->e_bodytype, TRUE, FALSE));

	/* message from envelope, if it exists */
	if (e->e_message != NULL)
		fprintf(tfp, "M%s\n", denlstring(e->e_message, TRUE, FALSE));

	/* send various flag bits through */
	p = buf;
	if (bitset(EF_WARNING, e->e_flags))
		*p++ = 'w';
	if (bitset(EF_RESPONSE, e->e_flags))
		*p++ = 'r';
	if (bitset(EF_HAS8BIT, e->e_flags))
		*p++ = '8';
	if (bitset(EF_DELETE_BCC, e->e_flags))
		*p++ = 'b';
	if (bitset(EF_RET_PARAM, e->e_flags))
		*p++ = 'd';
	if (bitset(EF_NO_BODY_RETN, e->e_flags))
		*p++ = 'n';
	*p++ = '\0';
	if (buf[0] != '\0')
		fprintf(tfp, "F%s\n", buf);

	/* $r and $s and $_ macro values */
	if ((p = macvalue('r', e)) != NULL)
		fprintf(tfp, "$r%s\n", denlstring(p, TRUE, FALSE));
	if ((p = macvalue('s', e)) != NULL)
		fprintf(tfp, "$s%s\n", denlstring(p, TRUE, FALSE));
	if ((p = macvalue('_', e)) != NULL)
		fprintf(tfp, "$_%s\n", denlstring(p, TRUE, FALSE));

	/* output name of sender */
	if (bitnset(M_UDBENVELOPE, e->e_from.q_mailer->m_flags))
		p = e->e_sender;
	else
		p = e->e_from.q_paddr;
	fprintf(tfp, "S%s\n", denlstring(p, TRUE, FALSE));

	/* output ESMTP-supplied "original" information */
	if (e->e_envid != NULL)
		fprintf(tfp, "Z%s\n", denlstring(e->e_envid, TRUE, FALSE));

	/* output list of recipient addresses */
	printctladdr(NULL, NULL);
	for (q = e->e_sendqueue; q != NULL; q = q->q_next)
	{
		if (bitset(QDONTSEND|QBADADDR|QSENT, q->q_flags))
		{
#if XDEBUG
			if (bitset(QQUEUEUP, q->q_flags))
				syslog(LOG_DEBUG,
					"dropenvelope: %s: q_flags = %x, paddr = %s",
					e->e_id, q->q_flags, q->q_paddr);
#endif
			continue;
		}
		printctladdr(q, tfp);
		if (q->q_orcpt != NULL)
			fprintf(tfp, "Q%s\n",
				denlstring(q->q_orcpt, TRUE, FALSE));
		putc('R', tfp);
		if (bitset(QPRIMARY, q->q_flags))
			putc('P', tfp);
		if (bitset(QHASNOTIFY, q->q_flags))
			putc('N', tfp);
		if (bitset(QPINGONSUCCESS, q->q_flags))
			putc('S', tfp);
		if (bitset(QPINGONFAILURE, q->q_flags))
			putc('F', tfp);
		if (bitset(QPINGONDELAY, q->q_flags))
			putc('D', tfp);
		putc(':', tfp);
		fprintf(tfp, "%s\n", denlstring(q->q_paddr, TRUE, FALSE));
		if (announce)
		{
			e->e_to = q->q_paddr;
			message("queued");
			if (LogLevel > 8)
				logdelivery(q->q_mailer, NULL, "queued",
					    NULL, (time_t) 0, e);
			e->e_to = NULL;
		}
		if (tTd(40, 1))
		{
			printf("queueing ");
			printaddr(q, FALSE);
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
	bzero(&mcibuf, sizeof mcibuf);
	mcibuf.mci_mailer = &nullmailer;
	mcibuf.mci_out = tfp;

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

		/* expand macros; if null, don't output header at all */
		if (bitset(H_DEFAULT, h->h_flags))
		{
			(void) expand(h->h_value, buf, sizeof buf, e);
			if (buf[0] == '\0')
				continue;
		}

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
			fprintf(tfp, "%s: %s\n",
				h->h_field,
				denlstring(buf, FALSE, TRUE));
		}
		else if (bitset(H_FROM|H_RCPT, h->h_flags))
		{
			bool oldstyle = bitset(EF_OLDSTYLE, e->e_flags);
			FILE *savetrace = TrafficLogFile;

			TrafficLogFile = NULL;

			if (bitset(H_FROM, h->h_flags))
				oldstyle = FALSE;

			commaize(h, h->h_value, oldstyle, &mcibuf, e);

			TrafficLogFile = savetrace;
		}
		else
		{
			fprintf(tfp, "%s: %s\n",
				h->h_field,
				denlstring(h->h_value, FALSE, TRUE));
		}
	}

	/*
	**  Clean up.
	**
	**	Write a terminator record -- this is to prevent
	**	scurrilous crackers from appending any data.
	*/

	fprintf(tfp, ".\n");

	if (fflush(tfp) < 0 || fsync(fileno(tfp)) < 0 || ferror(tfp))
	{
		if (newid)
			syserr("!552 Error writing control file %s", tf);
		else
			syserr("!452 Error writing control file %s", tf);
	}

	if (!newid)
	{
		/* rename (locked) tf to be (locked) qf */
		qf = queuename(e, 'q');
		if (rename(tf, qf) < 0)
			syserr("cannot rename(%s, %s), uid=%d",
				tf, qf, geteuid());

		/* close and unlock old (locked) qf */
		if (e->e_lockfp != NULL)
			(void) xfclose(e->e_lockfp, "queueup lockfp", e->e_id);
		e->e_lockfp = tfp;
	}
	else
		qf = tf;
	errno = 0;
	e->e_flags |= EF_INQUEUE;

# ifdef LOG
	/* save log info */
	if (LogLevel > 79)
		syslog(LOG_DEBUG, "%s: queueup, qf=%s", e->e_id, qf);
# endif /* LOG */

	if (tTd(40, 1))
		printf("<<<<< done queueing %s <<<<<\n\n", e->e_id);
	return;
}

void
printctladdr(a, tfp)
	register ADDRESS *a;
	FILE *tfp;
{
	char *uname;
	char *paddr;
	register ADDRESS *q;
	uid_t uid;
	gid_t gid;
	static ADDRESS *lastctladdr;
	static uid_t lastuid;

	/* initialization */
	if (a == NULL || a->q_alias == NULL || tfp == NULL)
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
	{
		uname = NULL;
		uid = 0;
		gid = 0;
	}
	else
	{
		uname = q->q_ruser != NULL ? q->q_ruser : q->q_user;
		uid = q->q_uid;
		gid = q->q_gid;
	}
	a = a->q_alias;

	/* check to see if this is the same as last time */
	if (lastctladdr != NULL && uid == lastuid &&
	    strcmp(lastctladdr->q_paddr, a->q_paddr) == 0)
		return;
	lastuid = uid;
	lastctladdr = a;

	paddr = denlstring(a->q_paddr, TRUE, FALSE);
	if (uid == 0 || uname == NULL || uname[0] == '\0')
		fprintf(tfp, "C:%s\n", paddr);
	else
		fprintf(tfp, "C%s:%ld:%ld:%s\n",
			uname, (long) uid, (long) gid, paddr);
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
**		verbose -- if TRUE, print out status information.
**
**	Returns:
**		TRUE if the queue run successfully began.
**
**	Side Effects:
**		runs things in the mail queue.
*/

ENVELOPE	QueueEnvelope;		/* the queue run envelope */

bool
runqueue(forkflag, verbose)
	bool forkflag;
	bool verbose;
{
	register ENVELOPE *e;
	int njobs;
	int sequenceno = 0;
	extern ENVELOPE BlankEnvelope;
	extern void clrdaemon __P((void));
	extern void runqueueevent __P((bool));
	extern void drop_privileges __P((void));

	/*
	**  If no work will ever be selected, don't even bother reading
	**  the queue.
	*/

	CurrentLA = getla();	/* get load average */

	if (CurrentLA >= QueueLA)
	{
		char *msg = "Skipping queue run -- load average too high";

		if (verbose)
			message("458 %s\n", msg);
#ifdef LOG
		if (LogLevel > 8)
			syslog(LOG_INFO, "runqueue: %s", msg);
#endif
		if (forkflag && QueueIntvl != 0)
			(void) setevent(QueueIntvl, runqueueevent, TRUE);
		return FALSE;
	}

	/*
	**  See if we want to go off and do other useful work.
	*/

	if (forkflag)
	{
		pid_t pid;
		extern SIGFUNC_DECL intsig __P((int));
#ifdef SIGCHLD
		extern SIGFUNC_DECL reapchild __P((int));

		blocksignal(SIGCHLD);
		(void) setsignal(SIGCHLD, reapchild);
#endif

		pid = dofork();
		if (pid == -1)
		{
			const char *msg = "Skipping queue run -- fork() failed";
			const char *err = errstring(errno);

			if (verbose)
				message("458 %s: %s\n", msg, err);
#ifdef LOG
			if (LogLevel > 8)
				syslog(LOG_INFO, "runqueue: %s: %s", msg, err);
#endif
			if (QueueIntvl != 0)
				(void) setevent(QueueIntvl, runqueueevent, TRUE);
			(void) releasesignal(SIGCHLD);
			return FALSE;
		}
		if (pid != 0)
		{
			/* parent -- pick up intermediate zombie */
#ifndef SIGCHLD
			(void) waitfor(pid);
#else
			(void) blocksignal(SIGALRM);
			proc_list_add(pid);
			(void) releasesignal(SIGALRM);
			releasesignal(SIGCHLD);
#endif /* SIGCHLD */
			if (QueueIntvl != 0)
				(void) setevent(QueueIntvl, runqueueevent, TRUE);
			return TRUE;
		}
		/* child -- double fork and clean up signals */
		proc_list_clear();
#ifndef SIGCHLD
		if (fork() != 0)
			exit(EX_OK);
#else /* SIGCHLD */
		releasesignal(SIGCHLD);
		(void) setsignal(SIGCHLD, SIG_DFL);
#endif /* SIGCHLD */
		(void) setsignal(SIGHUP, intsig);
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

# if DAEMON
	clrdaemon();
# endif /* DAEMON */

	/* force it to run expensive jobs */
	NoConnect = FALSE;

	/* drop privileges */
	if (geteuid() == (uid_t) 0)
		drop_privileges();

	/*
	**  Create ourselves an envelope
	*/

	CurEnv = &QueueEnvelope;
	e = newenvelope(&QueueEnvelope, CurEnv);
	e->e_flags = BlankEnvelope.e_flags;

	/* make sure we have disconnected from parent */
	if (forkflag)
		disconnect(1, e);

	/*
	**  Make sure the alias database is open.
	*/

	initmaps(FALSE, e);

	/*
	**  If we are running part of the queue, always ignore stored
	**  host status.
	*/

	if (QueueLimitId != NULL || QueueLimitSender != NULL ||
	    QueueLimitRecipient != NULL)
	{
		IgnoreHostStatus = TRUE;
		MinQueueAge = 0;
	}

	/*
	**  Start making passes through the queue.
	**	First, read and sort the entire queue.
	**	Then, process the work in that order.
	**		But if you take too long, start over.
	*/

	/* order the existing work requests */
	njobs = orderq(FALSE);

	/* process them once at a time */
	while (WorkQ != NULL)
	{
		WORK *w = WorkQ;

		WorkQ = WorkQ->w_next;
		e->e_to = NULL;

		/*
		**  Ignore jobs that are too expensive for the moment.
		*/

		sequenceno++;
		if (shouldqueue(w->w_pri, w->w_ctime))
		{
			if (Verbose)
			{
				message("");
				message("Skipping %s (sequence %d of %d)",
					w->w_name + 2, sequenceno, njobs);
			}
		}
		else
		{
			pid_t pid;
			extern pid_t dowork();

			if (Verbose)
			{
				message("");
				message("Running %s (sequence %d of %d)",
					w->w_name + 2, sequenceno, njobs);
			}
			pid = dowork(w->w_name + 2, ForkQueueRuns, FALSE, e);
			errno = 0;
			if (pid != 0)
				(void) waitfor(pid);
		}
		free(w->w_name);
		if (w->w_host)
			free(w->w_host);
		free((char *) w);
	}

	/* exit without the usual cleanup */
	e->e_id = NULL;
	finis();
	/*NOTREACHED*/
	return TRUE;
}


/*
**  RUNQUEUEEVENT -- stub for use in setevent
*/

void
runqueueevent(forkflag)
	bool forkflag;
{
	(void) runqueue(forkflag, FALSE);
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

static WORK	*WorkList = NULL;
static int	WorkListSize = 0;

int
orderq(doall)
	bool doall;
{
	register struct dirent *d;
	register WORK *w;
	DIR *f;
	register int i;
	int wn = -1;
	int wc;

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
		if (w->w_host)
			free(w->w_host);
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
		register char *p;
		char lbuf[MAXNAME + 1];
		extern bool strcontainedin();

		if (tTd(41, 50))
			printf("orderq: checking %s\n", d->d_name);

		/* is this an interesting entry? */
		if (d->d_name[0] != 'q' || d->d_name[1] != 'f')
			continue;

		if (QueueLimitId != NULL &&
		    !strcontainedin(QueueLimitId, d->d_name))
			continue;

#ifdef PICKY_QF_NAME_CHECK
		/*
		**  Check queue name for plausibility.  This handles
		**  both old and new type ids.
		*/

		p = d->d_name + 2;
		if (isupper(p[0]) && isupper(p[2]))
			p += 3;
		else if (isupper(p[1]))
			p += 2;
		else
			p = d->d_name;
		for (i = 0; isdigit(*p); p++)
			i++;
		if (i < 5 || *p != '\0')
		{
			if (Verbose)
				printf("orderq: bogus qf name %s\n", d->d_name);
# ifdef LOG
			if (LogLevel > 0)
				syslog(LOG_ALERT, "orderq: bogus qf name %s",
					d->d_name);
# endif
			if (strlen(d->d_name) > (SIZE_T) MAXNAME)
				d->d_name[MAXNAME] = '\0';
			strcpy(lbuf, d->d_name);
			lbuf[0] = 'Q';
			(void) rename(d->d_name, lbuf);
			continue;
		}
#endif

		/* open control file (if not too many files) */
		if (++wn >= MaxQueueRun && MaxQueueRun > 0)
		{
# ifdef LOG
			if (wn == MaxQueueRun && LogLevel > 0)
				syslog(LOG_ALERT, "WorkList for %s maxed out at %d",
						QueueDir, MaxQueueRun);
# endif
			continue;
		}
		if (wn >= WorkListSize)
		{
			extern void grow_wlist __P((void));

			grow_wlist();
			if (wn >= WorkListSize)
				continue;
		}

		cf = fopen(d->d_name, "r");
		if (cf == NULL)
		{
			/* this may be some random person sending hir msgs */
			/* syserr("orderq: cannot open %s", cbuf); */
			if (tTd(41, 2))
				printf("orderq: cannot open %s: %s\n",
					d->d_name, errstring(errno));
			errno = 0;
			wn--;
			continue;
		}
		w = &WorkList[wn];
		w->w_name = newstr(d->d_name);
		w->w_host = NULL;
		w->w_lock = !lockfile(fileno(cf), w->w_name, NULL, LOCK_SH|LOCK_NB);
		w->w_tooyoung = FALSE;

		/* make sure jobs in creation don't clog queue */
		w->w_pri = 0x7fffffff;
		w->w_ctime = 0;

		/* extract useful information */
		i = NEED_P | NEED_T;
		if (QueueLimitSender != NULL)
			i |= NEED_S;
		if (QueueSortOrder == QS_BYHOST || QueueLimitRecipient != NULL)
			i |= NEED_R;
		while (i != 0 && fgets(lbuf, sizeof lbuf, cf) != NULL)
		{
			int qfver = 0;
			extern bool strcontainedin();

			switch (lbuf[0])
			{
			  case 'V':
				qfver = atoi(&lbuf[1]);
				break;

			  case 'P':
				w->w_pri = atol(&lbuf[1]);
				i &= ~NEED_P;
				break;

			  case 'T':
				w->w_ctime = atol(&lbuf[1]);
				i &= ~NEED_T;
				break;

			  case 'R':
				if (w->w_host == NULL &&
				    (p = strrchr(&lbuf[1], '@')) != NULL)
					w->w_host = newstr(&p[1]);
				if (QueueLimitRecipient == NULL)
				{
					i &= ~NEED_R;
					break;
				}
				if (qfver > 0)
				{
					p = strchr(&lbuf[1], ':');
					if (p == NULL)
						p = &lbuf[1];
				}
				else
					p = &lbuf[1];
				if (strcontainedin(QueueLimitRecipient, p))
					i &= ~NEED_R;
				break;

			  case 'S':
				if (QueueLimitSender != NULL &&
				    strcontainedin(QueueLimitSender, &lbuf[1]))
					i &= ~NEED_S;
				break;

			  case 'K':
				if ((curtime() - (time_t) atol(&lbuf[1])) < MinQueueAge)
					w->w_tooyoung = TRUE;
				break;

			  case 'N':
				if (atol(&lbuf[1]) == 0)
					w->w_tooyoung = FALSE;
				break;
			}
		}
		(void) fclose(cf);

		if ((!doall && shouldqueue(w->w_pri, w->w_ctime)) ||
		    bitset(NEED_R|NEED_S, i))
		{
			/* don't even bother sorting this job in */
			if (tTd(41, 49))
				printf("skipping %s (%x)\n", w->w_name, i);
			free(w->w_name);
			if (w->w_host)
				free(w->w_host);
			wn--;
		}
	}
	(void) closedir(f);
	wn++;

	wc = min(wn, WorkListSize);
	if (wc > MaxQueueRun && MaxQueueRun > 0)
		wc = MaxQueueRun;

	if (QueueSortOrder == QS_BYHOST)
	{
		extern workcmpf1();
		extern workcmpf2();

		/*
		**  Sort the work directory for the first time,
		**  based on host name, lock status, and priority.
		*/

		qsort((char *) WorkList, wc, sizeof *WorkList, workcmpf1);

		/*
		**  If one message to host is locked, "lock" all messages
		**  to that host.
		*/

		i = 0;
		while (i < wc)
		{
			if (!WorkList[i].w_lock)
			{
				i++;
				continue;
			}
			w = &WorkList[i];
			while (++i < wc)
			{
				if (WorkList[i].w_host == NULL &&
				    w->w_host == NULL)
					WorkList[i].w_lock = TRUE;
				else if (WorkList[i].w_host != NULL &&
					 w->w_host != NULL &&
					 strcmp(WorkList[i].w_host, w->w_host) == 0)
					WorkList[i].w_lock = TRUE;
				else
					break;
			}
		}

		/*
		**  Sort the work directory for the second time,
		**  based on lock status, host name, and priority.
		*/

		qsort((char *) WorkList, wc, sizeof *WorkList, workcmpf2);
	}
	else if (QueueSortOrder == QS_BYTIME)
	{
		extern workcmpf3();

		/*
		**  Simple sort based on submission time only.
		*/

		qsort((char *) WorkList, wc, sizeof *WorkList, workcmpf3);
	}
	else
	{
		extern workcmpf0();

		/*
		**  Simple sort based on queue priority only.
		*/

		qsort((char *) WorkList, wc, sizeof *WorkList, workcmpf0);
	}

	/*
	**  Convert the work list into canonical form.
	**	Should be turning it into a list of envelopes here perhaps.
	*/

	WorkQ = NULL;
	for (i = wc; --i >= 0; )
	{
		w = (WORK *) xalloc(sizeof *w);
		w->w_name = WorkList[i].w_name;
		w->w_host = WorkList[i].w_host;
		w->w_lock = WorkList[i].w_lock;
		w->w_tooyoung = WorkList[i].w_tooyoung;
		w->w_pri = WorkList[i].w_pri;
		w->w_ctime = WorkList[i].w_ctime;
		w->w_next = WorkQ;
		WorkQ = w;
	}
	if (WorkList != NULL)
		free(WorkList);
	WorkList = NULL;

	if (tTd(40, 1))
	{
		for (w = WorkQ; w != NULL; w = w->w_next)
			printf("%32s: pri=%ld\n", w->w_name, w->w_pri);
	}

	return (wn);
}
/*
**  GROW_WLIST -- make the work list larger
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Adds another QUEUESEGSIZE entries to WorkList if possible.
**		It can fail if there isn't enough memory, so WorkListSize
**		should be checked again upon return.
*/

void
grow_wlist()
{
	if (tTd(41, 1))
		printf("grow_wlist: WorkListSize=%d\n", WorkListSize);
	if (WorkList == NULL)
	{
		WorkList = (WORK *) xalloc(sizeof(WORK) * (QUEUESEGSIZE + 1));
		WorkListSize = QUEUESEGSIZE;
	}
	else
	{
		int newsize = WorkListSize + QUEUESEGSIZE;
		WORK *newlist = (WORK *) realloc((char *)WorkList,
					  (unsigned)sizeof(WORK) * (newsize + 1));

		if (newlist != NULL)
		{
			WorkListSize = newsize;
			WorkList = newlist;
# ifdef LOG
			if (LogLevel > 1)
			{
				syslog(LOG_NOTICE, "grew WorkList for %s to %d",
						QueueDir, WorkListSize);
			}
		}
		else if (LogLevel > 0)
		{
			syslog(LOG_ALERT, "FAILED to grow WorkList for %s to %d",
					QueueDir, newsize);
# endif
		}
	}
	if (tTd(41, 1))
		printf("grow_wlist: WorkListSize now %d\n", WorkListSize);
}
/*
**  WORKCMPF0 -- simple priority-only compare function.
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

int
workcmpf0(a, b)
	register WORK *a;
	register WORK *b;
{
	long pa = a->w_pri;
	long pb = b->w_pri;

	if (pa == pb)
		return 0;
	else if (pa > pb)
		return 1;
	else
		return -1;
}
/*
**  WORKCMPF1 -- first compare function for ordering work based on host name.
**
**	Sorts on host name, lock status, and priority in that order.
**
**	Parameters:
**		a -- the first argument.
**		b -- the second argument.
**
**	Returns:
**		<0 if a < b
**		 0 if a == b
**		>0 if a > b
**
**	Side Effects:
**		none.
*/

int
workcmpf1(a, b)
	register WORK *a;
	register WORK *b;
{
	int i;

	/* host name */
	if (a->w_host != NULL && b->w_host == NULL)
		return 1;
	else if (a->w_host == NULL && b->w_host != NULL)
		return -1;
	if (a->w_host != NULL && b->w_host != NULL &&
	    (i = strcmp(a->w_host, b->w_host)))
		return i;

	/* lock status */
	if (a->w_lock != b->w_lock)
		return b->w_lock - a->w_lock;

	/* job priority */
	return a->w_pri - b->w_pri;
}
/*
**  WORKCMPF2 -- second compare function for ordering work based on host name.
**
**	Sorts on lock status, host name, and priority in that order.
**
**	Parameters:
**		a -- the first argument.
**		b -- the second argument.
**
**	Returns:
**		<0 if a < b
**		 0 if a == b
**		>0 if a > b
**
**	Side Effects:
**		none.
*/

int
workcmpf2(a, b)
	register WORK *a;
	register WORK *b;
{
	int i;

	/* lock status */
	if (a->w_lock != b->w_lock)
		return a->w_lock - b->w_lock;

	/* host name */
	if (a->w_host != NULL && b->w_host == NULL)
		return 1;
	else if (a->w_host == NULL && b->w_host != NULL)
		return -1;
	if (a->w_host != NULL && b->w_host != NULL &&
	    (i = strcmp(a->w_host, b->w_host)))
		return i;

	/* job priority */
	return a->w_pri - b->w_pri;
}
/*
**  WORKCMPF3 -- simple submission-time-only compare function.
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

int
workcmpf3(a, b)
	register WORK *a;
	register WORK *b;
{
	if (a->w_ctime > b->w_ctime)
		return 1;
	else if (a->w_ctime < b->w_ctime)
		return -1;
	else
		return 0;
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
**		process id of process that is running the queue job.
**
**	Side Effects:
**		The work request is satisfied if possible.
*/

pid_t
dowork(id, forkflag, requeueflag, e)
	char *id;
	bool forkflag;
	bool requeueflag;
	register ENVELOPE *e;
{
	register pid_t pid;
	extern bool readqf();

	if (tTd(40, 1))
		printf("dowork(%s)\n", id);

	/*
	**  Fork for work.
	*/

	if (forkflag)
	{
		pid = fork();
		if (pid < 0)
		{
			syserr("dowork: cannot fork");
			return 0;
		}
		else if (pid > 0)
		{
			/* parent -- clean out connection cache */
			mci_flush(FALSE, NULL);
		}
	}
	else
	{
		pid = 0;
	}

	if (pid == 0)
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
		e->e_flags |= EF_QUEUERUN|EF_GLOBALERRS;
		e->e_sendmode = SM_DELIVER;
		e->e_errormode = EM_MAIL;
		e->e_id = id;
		GrabTo = UseErrorsTo = FALSE;
		ExitStat = EX_OK;
		if (forkflag)
		{
			disconnect(1, e);
			OpMode = MD_DELIVER;
		}
		setproctitle("%s: from queue", id);
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
				return 0;
		}

		e->e_flags |= EF_INQUEUE;
		eatheader(e, requeueflag);

		if (requeueflag)
			queueup(e, FALSE);

		/* do the delivery */
		sendall(e, SM_DELIVER);

		/* finish up and exit */
		if (forkflag)
			finis();
		else
			dropenvelope(e, TRUE);
	}
	e->e_id = NULL;
	return pid;
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
	int qfver = 0;
	long hdrsize = 0;
	register char *p;
	char *orcpt = NULL;
	bool nomore = FALSE;
	char qf[20];
	char buf[MAXLINE];
	extern ADDRESS *setctluser __P((char *, int));

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

	if (!lockfile(fileno(qfp), qf, NULL, LOCK_EX|LOCK_NB))
	{
		/* being processed by another queuer */
		if (Verbose || tTd(40, 8))
			printf("%s: locked\n", e->e_id);
# ifdef LOG
		if (LogLevel > 19)
			syslog(LOG_DEBUG, "%s: locked", e->e_id);
# endif /* LOG */
		(void) fclose(qfp);
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

	if ((st.st_uid != geteuid() && geteuid() != RealUid) ||
	    bitset(S_IWOTH|S_IWGRP, st.st_mode))
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
		loseqfile(e, "bogus file uid in mqueue");
		fclose(qfp);
		return FALSE;
	}

	if (st.st_size == 0)
	{
		/* must be a bogus file -- just remove it */
		qf[0] = 'd';
		(void) unlink(qf);
		qf[0] = 'q';
		(void) unlink(qf);
		fclose(qfp);
		return FALSE;
	}

	if (st.st_nlink == 0)
	{
		/*
		**  Race condition -- we got a file just as it was being
		**  unlinked.  Just assume it is zero length.
		*/

		fclose(qfp);
		return FALSE;
	}

	/* good file -- save this lock */
	e->e_lockfp = qfp;

	/* do basic system initialization */
	initsys(e);
	define('i', e->e_id, e);

	LineNumber = 0;
	e->e_flags |= EF_GLOBALERRS;
	OpMode = MD_DELIVER;
	ctladdr = NULL;
	e->e_dfino = -1;
	e->e_msgsize = -1;
	while ((bp = fgetfolded(buf, sizeof buf, qfp)) != NULL)
	{
		register char *p;
		u_long qflags;
		ADDRESS *q;
		int mid;
		auto char *ep;

		if (tTd(40, 4))
			printf("+++++ %s\n", bp);
		if (nomore)
		{
			/* hack attack */
			syserr("SECURITY ALERT: extra data in qf: %s", bp);
			fclose(qfp);
			loseqfile(e, "bogus queue line");
			return FALSE;
		}
		switch (bp[0])
		{
		  case 'V':		/* queue file version number */
			qfver = atoi(&bp[1]);
			if (qfver <= QF_VERSION)
				break;
			syserr("Version number in qf (%d) greater than max (%d)",
				qfver, QF_VERSION);
			fclose(qfp);
			loseqfile(e, "unsupported qf file version");
			return FALSE;

		  case 'C':		/* specify controlling user */
			ctladdr = setctluser(&bp[1], qfver);
			break;

		  case 'Q':		/* original recipient */
			orcpt = newstr(&bp[1]);
			break;

		  case 'R':		/* specify recipient */
			p = bp;
			qflags = 0;
			if (qfver >= 1)
			{
				/* get flag bits */
				while (*++p != '\0' && *p != ':')
				{
					switch (*p)
					{
					  case 'N':
						qflags |= QHASNOTIFY;
						break;

					  case 'S':
						qflags |= QPINGONSUCCESS;
						break;

					  case 'F':
						qflags |= QPINGONFAILURE;
						break;

					  case 'D':
						qflags |= QPINGONDELAY;
						break;

					  case 'P':
						qflags |= QPRIMARY;
						break;
					}
				}
			}
			else
				qflags |= QPRIMARY;
			q = parseaddr(++p, NULLADDR, RF_COPYALL, '\0', NULL, e);
			if (q != NULL)
			{
				q->q_alias = ctladdr;
				if (qfver >= 1)
					q->q_flags &= ~Q_PINGFLAGS;
				q->q_flags |= qflags;
				q->q_orcpt = orcpt;
				(void) recipient(q, &e->e_sendqueue, 0, e);
			}
			orcpt = NULL;
			break;

		  case 'E':		/* specify error recipient */
			/* no longer used */
			break;

		  case 'H':		/* header */
			(void) chompheader(&bp[1], FALSE, NULL, e);
			hdrsize += strlen(&bp[1]);
			break;

		  case 'M':		/* message */
			/* ignore this; we want a new message next time */
			break;

		  case 'S':		/* sender */
			setsender(newstr(&bp[1]), e, NULL, '\0', TRUE);
			break;

		  case 'B':		/* body type */
			e->e_bodytype = newstr(&bp[1]);
			break;

		  case 'D':		/* data file name */
			/* obsolete -- ignore */
			break;

		  case 'T':		/* init time */
			e->e_ctime = atol(&bp[1]);
			break;

		  case 'I':		/* data file's inode number */
			/* regenerated below */
			break;

		  case 'K':		/* time of last deliver attempt */
			e->e_dtime = atol(&buf[1]);
			break;

		  case 'N':		/* number of delivery attempts */
			e->e_ntries = atoi(&buf[1]);

			/* if this has been tried recently, let it be */
			if (e->e_ntries > 0 &&
			    (curtime() - e->e_dtime) < MinQueueAge)
			{
				char *howlong = pintvl(curtime() - e->e_dtime, TRUE);
				extern void unlockqueue();

				if (Verbose || tTd(40, 8))
					printf("%s: too young (%s)\n",
						e->e_id, howlong);
#ifdef LOG
				if (LogLevel > 19)
					syslog(LOG_DEBUG, "%s: too young (%s)",
						e->e_id, howlong);
#endif
				e->e_id = NULL;
				unlockqueue(e);
				return FALSE;
			}
			break;

		  case 'P':		/* message priority */
			e->e_msgpriority = atol(&bp[1]) + WkTimeFact;
			break;

		  case 'F':		/* flag bits */
			if (strncmp(bp, "From ", 5) == 0)
			{
				/* we are being spoofed! */
				syserr("SECURITY ALERT: bogus qf line %s", bp);
				fclose(qfp);
				loseqfile(e, "bogus queue line");
				return FALSE;
			}
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

				  case '8':	/* has 8 bit data */
					e->e_flags |= EF_HAS8BIT;
					break;

				  case 'b':	/* delete Bcc: header */
					e->e_flags |= EF_DELETE_BCC;
					break;

				  case 'd':	/* envelope has DSN RET= */
					e->e_flags |= EF_RET_PARAM;
					break;

				  case 'n':	/* don't return body */
					e->e_flags |= EF_NO_BODY_RETN;
					break;
				}
			}
			break;

		  case 'Z':		/* original envelope id from ESMTP */
			e->e_envid = newstr(&bp[1]);
			break;

		  case '$':		/* define macro */
			mid = macid(&bp[1], &ep);
			define(mid, newstr(ep), e);
			break;

		  case '.':		/* terminate file */
			nomore = TRUE;
			break;

		  default:
			syserr("readqf: %s: line %d: bad line \"%s\"",
				qf, LineNumber, shortenstring(bp, 203));
			fclose(qfp);
			loseqfile(e, "unrecognized line");
			return FALSE;
		}

		if (bp != buf)
			free(bp);
	}

	/*
	**  If we haven't read any lines, this queue file is empty.
	**  Arrange to remove it without referencing any null pointers.
	*/

	if (LineNumber == 0)
	{
		errno = 0;
		e->e_flags |= EF_CLRQUEUE | EF_FATALERRS | EF_RESPONSE;
		return TRUE;
	}

	/*
	**  Arrange to read the data file.
	*/

	p = queuename(e, 'd');
	e->e_dfp = fopen(p, "r");
	if (e->e_dfp == NULL)
	{
		syserr("readqf: cannot open %s", p);
	}
	else
	{
		e->e_flags |= EF_HAS_DF;
		if (fstat(fileno(e->e_dfp), &st) >= 0)
		{
			e->e_msgsize = st.st_size + hdrsize;
			e->e_dfdev = st.st_dev;
			e->e_dfino = st.st_ino;
		}
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

void
printqueue()
{
	register WORK *w;
	FILE *f;
	int nrequests;
	char buf[MAXLINE];

	/*
	**  Check for permission to print the queue
	*/

	if (bitset(PRIV_RESTRICTMAILQ, PrivacyFlags) && RealUid != 0)
	{
		struct stat st;
# ifdef NGROUPS_MAX
		int n;
		GIDSET_T gidset[NGROUPS_MAX];
# endif

		if (stat(QueueDir, &st) < 0)
		{
			syserr("Cannot stat %s", QueueDir);
			return;
		}
# ifdef NGROUPS_MAX
		n = getgroups(NGROUPS_MAX, gidset);
		while (--n >= 0)
		{
			if (gidset[n] == st.st_gid)
				break;
		}
		if (n < 0 && RealGid != st.st_gid)
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
	if (MaxQueueRun > 0 && nrequests > MaxQueueRun)
		printf(", only %d printed", MaxQueueRun);
	if (Verbose)
		printf(")\n--Q-ID-- --Size-- -Priority- ---Q-Time--- -----------Sender/Recipient-----------\n");
	else
		printf(")\n--Q-ID-- --Size-- -----Q-Time----- ------------Sender/Recipient------------\n");
	for (w = WorkQ; w != NULL; w = w->w_next)
	{
		struct stat st;
		auto time_t submittime = 0;
		long dfsize;
		int flags = 0;
		int qfver;
		char statmsg[MAXLINE];
		char bodytype[MAXNAME + 1];

		printf("%8s", w->w_name + 2);
		f = fopen(w->w_name, "r");
		if (f == NULL)
		{
			printf(" (job completed)\n");
			errno = 0;
			continue;
		}
		w->w_name[0] = 'd';
		if (stat(w->w_name, &st) >= 0)
			dfsize = st.st_size;
		else
			dfsize = -1;
		if (w->w_lock)
			printf("*");
		else if (w->w_tooyoung)
			printf("-");
		else if (shouldqueue(w->w_pri, w->w_ctime))
			printf("X");
		else
			printf(" ");
		errno = 0;

		statmsg[0] = bodytype[0] = '\0';
		qfver = 0;
		while (fgets(buf, sizeof buf, f) != NULL)
		{
			register int i;
			register char *p;

			fixcrlf(buf, TRUE);
			switch (buf[0])
			{
			  case 'V':	/* queue file version */
				qfver = atoi(&buf[1]);
				break;

			  case 'M':	/* error message */
				if ((i = strlen(&buf[1])) >= sizeof statmsg)
					i = sizeof statmsg - 1;
				bcopy(&buf[1], statmsg, i);
				statmsg[i] = '\0';
				break;

			  case 'B':	/* body type */
				if ((i = strlen(&buf[1])) >= sizeof bodytype)
					i = sizeof bodytype - 1;
				bcopy(&buf[1], bodytype, i);
				bodytype[i] = '\0';
				break;

			  case 'S':	/* sender name */
				if (Verbose)
					printf("%8ld %10ld%c%.12s %.78s",
					    dfsize,
					    w->w_pri,
					    bitset(EF_WARNING, flags) ? '+' : ' ',
					    ctime(&submittime) + 4,
					    &buf[1]);
				else
					printf("%8ld %.16s %.45s", dfsize,
					    ctime(&submittime), &buf[1]);
				if (statmsg[0] != '\0' || bodytype[0] != '\0')
				{
					printf("\n    %10.10s", bodytype);
					if (statmsg[0] != '\0')
						printf("   (%.*s)",
							Verbose ? 100 : 60,
							statmsg);
				}
				break;

			  case 'C':	/* controlling user */
				if (Verbose)
					printf("\n\t\t\t\t      (---%.74s---)",
						&buf[1]);
				break;

			  case 'R':	/* recipient name */
				p = &buf[1];
				if (qfver >= 1)
				{
					p = strchr(p, ':');
					if (p == NULL)
						break;
					p++;
				}
				if (Verbose)
					printf("\n\t\t\t\t\t  %.78s", p);
				else
					printf("\n\t\t\t\t   %.45s", p);
				break;

			  case 'T':	/* creation time */
				submittime = atol(&buf[1]);
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
	static pid_t pid = -1;
	static char c0;
	static char c1;
	static char c2;
	time_t now;
	struct tm *tm;
	static char buf[MAXNAME + 1];

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
		(void) snprintf(qf, sizeof qf, "qf%cAA%05d", c0, pid);

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
				syserr("queuename: Cannot create \"%s\" in \"%s\" (euid=%d)",
					qf, QueueDir, geteuid());
				exit(EX_UNAVAILABLE);
			}
			if (lockfile(i, qf, NULL, LOCK_EX|LOCK_NB))
			{
				e->e_lockfp = fdopen(i, "w");
				break;
			}

			/* a reader got the file; abandon it and try again */
			(void) close(i);
		}
		if (c1 >= '~' && c2 >= 'Z')
		{
			syserr("queuename: Cannot create \"%s\" in \"%s\" (euid=%d)",
				qf, QueueDir, geteuid());
			exit(EX_OSERR);
		}
		e->e_id = newstr(&qf[2]);
		define('i', e->e_id, e);
		if (tTd(7, 1))
			printf("queuename: assigned id %s, env=%lx\n",
			       e->e_id, (u_long) e);
		if (tTd(7, 9))
		{
			printf("  lockfd=");
			dumpfd(fileno(e->e_lockfp), TRUE, FALSE);
		}
# ifdef LOG
		if (LogLevel > 93)
			syslog(LOG_DEBUG, "%s: assigned id", e->e_id);
# endif /* LOG */
	}

	if (type == '\0')
		return (NULL);
	(void) snprintf(buf, sizeof buf, "%cf%s", type, e->e_id);
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

void
unlockqueue(e)
	ENVELOPE *e;
{
	if (tTd(51, 4))
		printf("unlockqueue(%s)\n",
			e->e_id == NULL ? "NOQUEUE" : e->e_id);

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
**		qfver -- the version stamp of this qf file.
**
**	Returns:
**		An address descriptor for the controlling user.
**
**	Side Effects:
**		none.
*/

ADDRESS *
setctluser(user, qfver)
	char *user;
	int qfver;
{
	register ADDRESS *a;
	struct passwd *pw;
	char *p;

	/*
	**  See if this clears our concept of controlling user.
	*/

	if (user == NULL || *user == '\0')
		return NULL;

	/*
	**  Set up addr fields for controlling user.
	*/

	a = (ADDRESS *) xalloc(sizeof *a);
	bzero((char *) a, sizeof *a);

	if (*user == '\0')
	{
		p = NULL;
		a->q_user = newstr(DefUser);
	}
	else if (*user == ':')
	{
		p = &user[1];
		a->q_user = newstr(p);
	}
	else
	{
		p = strtok(user, ":");
		a->q_user = newstr(user);
		if (qfver >= 2)
		{
			if ((p = strtok(NULL, ":")) != NULL)
				a->q_uid = atoi(p);
			if ((p = strtok(NULL, ":")) != NULL)
				a->q_gid = atoi(p);
			if ((p = strtok(NULL, ":")) != NULL)
				a->q_flags |= QGOODUID;
		}
		else if ((pw = sm_getpwnam(user)) != NULL)
		{
			if (strcmp(pw->pw_dir, "/") == 0)
				a->q_home = "";
			else
				a->q_home = newstr(pw->pw_dir);
			a->q_uid = pw->pw_uid;
			a->q_gid = pw->pw_gid;
			a->q_flags |= QGOODUID;
		}
	}

	a->q_flags |= QPRIMARY;		/* flag as a "ctladdr"  */
	a->q_mailer = LocalMailer;
	if (p == NULL)
		a->q_paddr = a->q_user;
	else
		a->q_paddr = newstr(p);
	return a;
}
/*
**  LOSEQFILE -- save the qf as Qf and try to let someone know
**
**	Parameters:
**		e -- the envelope (e->e_id will be used).
**		why -- reported to whomever can hear.
**
**	Returns:
**		none.
*/

void
loseqfile(e, why)
	register ENVELOPE *e;
	char *why;
{
	char *p;
	char buf[40];

	if (e == NULL || e->e_id == NULL)
		return;
	if (strlen(e->e_id) > (SIZE_T) sizeof buf - 4)
		return;
	strcpy(buf, queuename(e, 'q'));
	p = queuename(e, 'Q');
	if (rename(buf, p) < 0)
		syserr("cannot rename(%s, %s), uid=%d", buf, p, geteuid());
#ifdef LOG
	else if (LogLevel > 0)
		syslog(LOG_ALERT, "Losing %s: %s", buf, why);
#endif
}
