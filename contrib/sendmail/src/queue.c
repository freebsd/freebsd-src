/*
 * Copyright (c) 1998-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */


#include <sendmail.h>

#ifndef lint
# if QUEUE
static char id[] = "@(#)$Id: queue.c,v 8.343.4.55 2001/05/03 23:37:11 gshapiro Exp $ (with queueing)";
# else /* QUEUE */
static char id[] = "@(#)$Id: queue.c,v 8.343.4.55 2001/05/03 23:37:11 gshapiro Exp $ (without queueing)";
# endif /* QUEUE */
#endif /* ! lint */

# include <dirent.h>

#if QUEUE

# if _FFR_QUEUEDELAY
#  define QF_VERSION	5	/* version number of this queue format */
static time_t	queuedelay __P((ENVELOPE *));
# else /* _FFR_QUEUEDELAY */
#  define QF_VERSION	4	/* version number of this queue format */
#  define queuedelay(e)	MinQueueAge
# endif /* _FFR_QUEUEDELAY */

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

static WORK	*WorkQ;			/* queue of things to be done */

static void	grow_wlist __P((int));
static int	orderq __P((int, bool));
static void	printctladdr __P((ADDRESS *, FILE *));
static int	print_single_queue __P((int));
static bool	readqf __P((ENVELOPE *));
static void	runqueueevent __P((void));
static int	run_single_queue __P((int, bool, bool));
static char	*strrev __P((char *));
static ADDRESS	*setctluser __P((char *, int));
static int	workcmpf0();
static int	workcmpf1();
static int	workcmpf2();
static int	workcmpf3();
static int	workcmpf4();

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

# define TEMPQF_LETTER 'T'
# define LOSEQF_LETTER 'Q'

void
queueup(e, announce)
	register ENVELOPE *e;
	bool announce;
{
	char *qf;
	register FILE *tfp;
	register HDR *h;
	register ADDRESS *q;
	int tfd = -1;
	int i;
	bool newid;
	register char *p;
	MAILER nullmailer;
	MCI mcibuf;
	char tf[MAXPATHLEN];
	char buf[MAXLINE];

	/*
	**  Create control file.
	*/

	newid = (e->e_id == NULL) || !bitset(EF_INQUEUE, e->e_flags);

	/* if newid, queuename will create a locked qf file in e->lockfp */
	(void) strlcpy(tf, queuename(e, 't'), sizeof tf);
	tfp = e->e_lockfp;
	if (tfp == NULL)
		newid = FALSE;

	/* if newid, just write the qf file directly (instead of tf file) */
	if (!newid)
	{
		int flags;

		flags = O_CREAT|O_WRONLY|O_EXCL;

		/* get a locked tf file */
		for (i = 0; i < 128; i++)
		{
			if (tfd < 0)
			{
#if _FFR_QUEUE_FILE_MODE
				MODE_T oldumask;

				if (bitset(S_IWGRP, QueueFileMode))
					oldumask = umask(002);
				tfd = open(tf, flags, QueueFileMode);
				if (bitset(S_IWGRP, QueueFileMode))
					(void) umask(oldumask);
#else /* _FFR_QUEUE_FILE_MODE */
				tfd = open(tf, flags, FileMode);
#endif /* _FFR_QUEUE_FILE_MODE */

				if (tfd < 0)
				{
					if (errno != EEXIST)
						break;
					if (LogLevel > 0 && (i % 32) == 0)
						sm_syslog(LOG_ALERT, e->e_id,
							  "queueup: cannot create %s, uid=%d: %s",
							  tf, geteuid(), errstring(errno));
				}
			}
			if (tfd >= 0)
			{
				if (lockfile(tfd, tf, NULL, LOCK_EX|LOCK_NB))
					break;
				else if (LogLevel > 0 && (i % 32) == 0)
					sm_syslog(LOG_ALERT, e->e_id,
						  "queueup: cannot lock %s: %s",
						  tf, errstring(errno));
				if ((i % 32) == 31)
				{
					(void) close(tfd);
					tfd = -1;
				}
			}

			if ((i % 32) == 31)
			{
				/* save the old temp file away */
				(void) rename(tf, queuename(e, TEMPQF_LETTER));
			}
			else
				(void) sleep(i % 32);
		}
		if (tfd < 0 || (tfp = fdopen(tfd, "w")) == NULL)
		{
			int save_errno = errno;

			printopenfds(TRUE);
			errno = save_errno;
			syserr("!queueup: cannot create queue temp file %s, uid=%d",
				tf, geteuid());
		}
	}

	if (tTd(40, 1))
		dprintf("\n>>>>> queueing %s/qf%s%s >>>>>\n",
			qid_printqueue(e->e_queuedir), e->e_id,
			newid ? " (new id)" : "");
	if (tTd(40, 3))
	{
		dprintf("  e_flags=");
		printenvflags(e);
	}
	if (tTd(40, 32))
	{
		dprintf("  sendq=");
		printaddr(e->e_sendqueue, TRUE);
	}
	if (tTd(40, 9))
	{
		dprintf("  tfp=");
		dumpfd(fileno(tfp), TRUE, FALSE);
		dprintf("  lockfp=");
		if (e->e_lockfp == NULL)
			dprintf("NULL\n");
		else
			dumpfd(fileno(e->e_lockfp), TRUE, FALSE);
	}

	/*
	**  If there is no data file yet, create one.
	*/

	if (bitset(EF_HAS_DF, e->e_flags))
	{
		if (e->e_dfp != NULL && bfcommit(e->e_dfp) < 0)
			syserr("!queueup: cannot commit data file %s, uid=%d",
				queuename(e, 'd'), geteuid());
	}
	else
	{
		int dfd;
		register FILE *dfp = NULL;
		char dfname[MAXPATHLEN];
		struct stat stbuf;

		if (e->e_dfp != NULL && bftest(e->e_dfp))
			syserr("committing over bf file");

		(void) strlcpy(dfname, queuename(e, 'd'), sizeof dfname);
#if _FFR_QUEUE_FILE_MODE
		{
			MODE_T oldumask;

			if (bitset(S_IWGRP, QueueFileMode))
				oldumask = umask(002);
			dfd = open(dfname, O_WRONLY|O_CREAT|O_TRUNC,
				   QueueFileMode);
			if (bitset(S_IWGRP, QueueFileMode))
				(void) umask(oldumask);
		}
#else /* _FFR_QUEUE_FILE_MODE */
		dfd = open(dfname, O_WRONLY|O_CREAT|O_TRUNC, FileMode);
#endif /* _FFR_QUEUE_FILE_MODE */
		if (dfd < 0 || (dfp = fdopen(dfd, "w")) == NULL)
			syserr("!queueup: cannot create data temp file %s, uid=%d",
				dfname, geteuid());
		if (fstat(dfd, &stbuf) < 0)
			e->e_dfino = -1;
		else
		{
			e->e_dfdev = stbuf.st_dev;
			e->e_dfino = stbuf.st_ino;
		}
		e->e_flags |= EF_HAS_DF;
		memset(&mcibuf, '\0', sizeof mcibuf);
		mcibuf.mci_out = dfp;
		mcibuf.mci_mailer = FileMailer;
		(*e->e_putbody)(&mcibuf, e, NULL);
		if (fclose(dfp) < 0)
			syserr("!queueup: cannot save data temp file %s, uid=%d",
				dfname, geteuid());
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
	fprintf(tfp, "T%ld\n", (long) e->e_ctime);

	/* output last delivery time */
# if _FFR_QUEUEDELAY
	fprintf(tfp, "K%ld\n", (long) e->e_dtime);
	fprintf(tfp, "G%d\n", e->e_queuealg);
	fprintf(tfp, "Y%ld\n", (long) e->e_queuedelay);
	if (tTd(40, 64))
		sm_syslog(LOG_INFO, e->e_id,
			"queue alg: %d delay %ld next: %ld (now: %ld)\n",
			e->e_queuealg, e->e_queuedelay, e->e_dtime, curtime());
# else /* _FFR_QUEUEDELAY */
	fprintf(tfp, "K%ld\n", (long) e->e_dtime);
# endif /* _FFR_QUEUEDELAY */

	/* output number of delivery attempts */
	fprintf(tfp, "N%d\n", e->e_ntries);

	/* output message priority */
	fprintf(tfp, "P%ld\n", e->e_msgpriority);

	/* output inode number of data file */
	/* XXX should probably include device major/minor too */
	if (e->e_dfino != -1)
	{
		/*CONSTCOND*/
		if (sizeof e->e_dfino > sizeof(long))
			fprintf(tfp, "I%ld/%ld/%s\n",
				(long) major(e->e_dfdev),
				(long) minor(e->e_dfdev),
				quad_to_string(e->e_dfino));
		else
			fprintf(tfp, "I%ld/%ld/%lu\n",
				(long) major(e->e_dfdev),
				(long) minor(e->e_dfdev),
				(unsigned long) e->e_dfino);
	}

	/* output body type */
	if (e->e_bodytype != NULL)
		fprintf(tfp, "B%s\n", denlstring(e->e_bodytype, TRUE, FALSE));

# if _FFR_SAVE_CHARSET
	if (e->e_charset != NULL)
		fprintf(tfp, "X%s\n", denlstring(e->e_charset, TRUE, FALSE));
# endif /* _FFR_SAVE_CHARSET */

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

	/* save $={persistentMacros} macro values */
	queueup_macros(macid("{persistentMacros}", NULL), tfp, e);

	/* output name of sender */
	if (bitnset(M_UDBENVELOPE, e->e_from.q_mailer->m_flags))
		p = e->e_sender;
	else
		p = e->e_from.q_paddr;
	fprintf(tfp, "S%s\n", denlstring(p, TRUE, FALSE));

	/* output ESMTP-supplied "original" information */
	if (e->e_envid != NULL)
		fprintf(tfp, "Z%s\n", denlstring(e->e_envid, TRUE, FALSE));

	/* output AUTH= parameter */
	if (e->e_auth_param != NULL)
		fprintf(tfp, "A%s\n", denlstring(e->e_auth_param,
						 TRUE, FALSE));

	/* output list of recipient addresses */
	printctladdr(NULL, NULL);
	for (q = e->e_sendqueue; q != NULL; q = q->q_next)
	{
		if (!QS_IS_UNDELIVERED(q->q_state))
			continue;

		printctladdr(q, tfp);
		if (q->q_orcpt != NULL)
			fprintf(tfp, "Q%s\n",
				denlstring(q->q_orcpt, TRUE, FALSE));
		(void) putc('R', tfp);
		if (bitset(QPRIMARY, q->q_flags))
			(void) putc('P', tfp);
		if (bitset(QHASNOTIFY, q->q_flags))
			(void) putc('N', tfp);
		if (bitset(QPINGONSUCCESS, q->q_flags))
			(void) putc('S', tfp);
		if (bitset(QPINGONFAILURE, q->q_flags))
			(void) putc('F', tfp);
		if (bitset(QPINGONDELAY, q->q_flags))
			(void) putc('D', tfp);
		if (q->q_alias != NULL &&
		    bitset(QALIAS, q->q_alias->q_flags))
			(void) putc('A', tfp);
		(void) putc(':', tfp);
		(void) fprintf(tfp, "%s\n", denlstring(q->q_paddr, TRUE, FALSE));
		if (announce)
		{
			e->e_to = q->q_paddr;
			message("queued");
			if (LogLevel > 8)
				logdelivery(q->q_mailer, NULL, q->q_status,
					    "queued", NULL, (time_t) 0, e);
			e->e_to = NULL;
		}
		if (tTd(40, 1))
		{
			dprintf("queueing ");
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

	memset((char *) &nullmailer, '\0', sizeof nullmailer);
	nullmailer.m_re_rwset = nullmailer.m_rh_rwset =
			nullmailer.m_se_rwset = nullmailer.m_sh_rwset = -1;
	nullmailer.m_eol = "\n";
	memset(&mcibuf, '\0', sizeof mcibuf);
	mcibuf.mci_mailer = &nullmailer;
	mcibuf.mci_out = tfp;

	define('g', "\201f", e);
	for (h = e->e_header; h != NULL; h = h->h_link)
	{
		if (h->h_value == NULL)
			continue;

		/* don't output resent headers on non-resent messages */
		if (bitset(H_RESENT, h->h_flags) &&
		    !bitset(EF_RESENT, e->e_flags))
			continue;

		/* expand macros; if null, don't output header at all */
		if (bitset(H_DEFAULT, h->h_flags))
		{
			(void) expand(h->h_value, buf, sizeof buf, e);
			if (buf[0] == '\0')
				continue;
		}

		/* output this header */
		fprintf(tfp, "H?");

		/* output conditional macro if present */
		if (h->h_macro != '\0')
		{
			if (bitset(0200, h->h_macro))
				fprintf(tfp, "${%s}",
					macname(bitidx(h->h_macro)));
			else
				fprintf(tfp, "$%c", h->h_macro);
		}
		else if (!bitzerop(h->h_mflags) &&
			 bitset(H_CHECK|H_ACHECK, h->h_flags))
		{
			int j;

			/* if conditional, output the set of conditions */
			for (j = '\0'; j <= '\177'; j++)
				if (bitnset(j, h->h_mflags))
					(void) putc(j, tfp);
		}
		(void) putc('?', tfp);

		/* output the header: expand macros, convert addresses */
		if (bitset(H_DEFAULT, h->h_flags) &&
		    !bitset(H_BINDLATE, h->h_flags))
		{
			fprintf(tfp, "%s: %s\n",
				h->h_field,
				denlstring(buf, FALSE, TRUE));
		}
		else if (bitset(H_FROM|H_RCPT, h->h_flags) &&
			 !bitset(H_BINDLATE, h->h_flags))
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

	if (fflush(tfp) != 0 ||
	    (SuperSafe && fsync(fileno(tfp)) < 0) ||
	    ferror(tfp))
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
		/*
		**  fsync() after renaming to make sure
		**  metadata is written to disk on
		**  filesystems in which renames are
		**  not guaranteed such as softupdates.
		*/

		if (tfd >= 0 && SuperSafe && fsync(tfd) < 0)
			syserr("!queueup: cannot fsync queue temp file %s", tf);

		/* close and unlock old (locked) qf */
		if (e->e_lockfp != NULL)
			(void) fclose(e->e_lockfp);
		e->e_lockfp = tfp;
	}
	else
		qf = tf;
	errno = 0;
	e->e_flags |= EF_INQUEUE;

	/* save log info */
	if (LogLevel > 79)
		sm_syslog(LOG_DEBUG, e->e_id, "queueup, qf=%s", qf);

	if (tTd(40, 1))
		dprintf("<<<<< done queueing %s <<<<<\n\n", e->e_id);
	return;
}

static void
printctladdr(a, tfp)
	register ADDRESS *a;
	FILE *tfp;
{
	char *user;
	register ADDRESS *q;
	uid_t uid;
	gid_t gid;
	static ADDRESS *lastctladdr = NULL;
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
		user = NULL;
		uid = 0;
		gid = 0;
	}
	else
	{
		user = q->q_ruser != NULL ? q->q_ruser : q->q_user;
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

	if (uid == 0 || user == NULL || user[0] == '\0')
		fprintf(tfp, "C");
	else
		fprintf(tfp, "C%s:%ld:%ld",
			denlstring(user, TRUE, FALSE), (long) uid, (long) gid);
	fprintf(tfp, ":%s\n", denlstring(a->q_paddr, TRUE, FALSE));
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
**			FALSE can be ignored if we have multiple queues.
**		verbose -- if TRUE, print out status information.
**
**	Returns:
**		TRUE if the queue run successfully began.
**
**	Side Effects:
**		runs things in the mail queue.
*/

static ENVELOPE	QueueEnvelope;		/* the queue run envelope */
int		NumQueues = 0;		/* number of queues */
static time_t	LastQueueTime = 0;	/* last time a queue ID assigned */
static pid_t	LastQueuePid = -1;	/* last PID which had a queue ID */

struct qpaths_s
{
	char	*qp_name;	/* name of queue dir */
	short	qp_subdirs;	/* use subdirs? */
};

typedef struct qpaths_s QPATHS;

/* values for qp_supdirs */
#define QP_NOSUB	0x0000	/* No subdirectories */
#define QP_SUBDF	0x0001	/* "df" subdirectory */
#define QP_SUBQF	0x0002	/* "qf" subdirectory */
#define QP_SUBXF	0x0004	/* "xf" subdirectory */

static QPATHS	*QPaths = NULL;		/* list of queue directories */

bool
runqueue(forkflag, verbose)
	bool forkflag;
	bool verbose;
{
	int i;
	bool ret = TRUE;
	static int curnum = 0;

	DoQueueRun = FALSE;


	if (!forkflag && NumQueues > 1 && !verbose)
		forkflag = TRUE;

	for (i = 0; i < NumQueues; i++)
	{
		/*
		**  Pick up where we left off, in case we
		**  used up all the children last time
		**  without finishing.
		*/

		ret = run_single_queue(curnum, forkflag, verbose);

		/*
		**  Failure means a message was printed for ETRN
		**  and subsequent queues are likely to fail as well.
		*/

		if (!ret)
			break;

		if (++curnum >= NumQueues)
			curnum = 0;
	}
	if (QueueIntvl != 0)
		(void) setevent(QueueIntvl, runqueueevent, 0);
	return ret;
}
/*
**  RUN_SINGLE_QUEUE -- run the jobs in a single queue.
**
**	Gets the stuff out of the queue in some presumably logical
**	order and processes them.
**
**	Parameters:
**		queuedir -- queue to process
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

static bool
run_single_queue(queuedir, forkflag, verbose)
	int queuedir;
	bool forkflag;
	bool verbose;
{
	register ENVELOPE *e;
	int njobs;
	int sequenceno = 0;
	time_t current_la_time, now;
	extern ENVELOPE BlankEnvelope;

	/*
	**  If no work will ever be selected, don't even bother reading
	**  the queue.
	*/

	CurrentLA = sm_getla(NULL);	/* get load average */
	current_la_time = curtime();

	if (shouldqueue(WkRecipFact, current_la_time))
	{
		char *msg = "Skipping queue run -- load average too high";

		if (verbose)
			message("458 %s\n", msg);
		if (LogLevel > 8)
			sm_syslog(LOG_INFO, NOQID,
				  "runqueue: %s",
				  msg);
		return FALSE;
	}

	/*
	**  See if we already have too many children.
	*/

	if (forkflag && QueueIntvl != 0 &&
	    MaxChildren > 0 && CurChildren >= MaxChildren)
	{
		char *msg = "Skipping queue run -- too many children";

		if (verbose)
			message("458 %s (%d)\n", msg, CurChildren);
		if (LogLevel > 8)
			sm_syslog(LOG_INFO, NOQID,
				  "runqueue: %s (%d)",
				  msg, CurChildren);
		return FALSE;
	}

	/*
	**  See if we want to go off and do other useful work.
	*/

	if (forkflag)
	{
		pid_t pid;

		(void) blocksignal(SIGCHLD);
		(void) setsignal(SIGCHLD, reapchild);

		pid = dofork();
		if (pid == -1)
		{
			const char *msg = "Skipping queue run -- fork() failed";
			const char *err = errstring(errno);

			if (verbose)
				message("458 %s: %s\n", msg, err);
			if (LogLevel > 8)
				sm_syslog(LOG_INFO, NOQID,
					  "runqueue: %s: %s",
					  msg, err);
			(void) releasesignal(SIGCHLD);
			return FALSE;
		}
		if (pid != 0)
		{
			/* parent -- pick up intermediate zombie */
			(void) blocksignal(SIGALRM);
			proc_list_add(pid, "Queue runner", PROC_QUEUE);
			(void) releasesignal(SIGALRM);
			(void) releasesignal(SIGCHLD);
			return TRUE;
		}
		/* child -- clean up signals */

		/* Reset global flags */
		RestartRequest = NULL;
		ShutdownRequest = NULL;
		PendingSignal = 0;

		clrcontrol();
		proc_list_clear();

		/* Add parent process as first child item */
		proc_list_add(getpid(), "Queue runner child process",
			      PROC_QUEUE_CHILD);
		(void) releasesignal(SIGCHLD);
		(void) setsignal(SIGCHLD, SIG_DFL);
		(void) setsignal(SIGHUP, SIG_DFL);
		(void) setsignal(SIGTERM, intsig);
	}

	sm_setproctitle(TRUE, CurEnv, "running queue: %s",
			qid_printqueue(queuedir));

	if (LogLevel > 69 || tTd(63, 99))
		sm_syslog(LOG_DEBUG, NOQID,
			  "runqueue %s, pid=%d, forkflag=%d",
			  qid_printqueue(queuedir), (int) getpid(), forkflag);

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
		(void) drop_privileges(FALSE);

	/*
	**  Create ourselves an envelope
	*/

	CurEnv = &QueueEnvelope;
	e = newenvelope(&QueueEnvelope, CurEnv);
	e->e_flags = BlankEnvelope.e_flags;
	e->e_parent = NULL;

	/* make sure we have disconnected from parent */
	if (forkflag)
	{
		disconnect(1, e);
		QuickAbort = FALSE;
	}

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
	njobs = orderq(queuedir, FALSE);


	/* process them once at a time */
	while (WorkQ != NULL)
	{
		WORK *w = WorkQ;

		WorkQ = WorkQ->w_next;
		e->e_to = NULL;

		/*
		**  Ignore jobs that are too expensive for the moment.
		**
		**	Get new load average every 30 seconds.
		*/

		now = curtime();
		if (current_la_time < now - 30)
		{
			CurrentLA = sm_getla(e);
			current_la_time = now;
		}
		if (shouldqueue(WkRecipFact, current_la_time))
		{
			char *msg = "Aborting queue run: load average too high";

			if (Verbose)
				message("%s", msg);
			if (LogLevel > 8)
				sm_syslog(LOG_INFO, NOQID,
					  "runqueue: %s",
					  msg);
			break;
		}
		sequenceno++;
		if (shouldqueue(w->w_pri, w->w_ctime))
		{
			if (Verbose)
				message("");
			if (QueueSortOrder == QSO_BYPRIORITY)
			{
				if (Verbose)
					message("Skipping %s/%s (sequence %d of %d) and flushing rest of queue",
						qid_printqueue(queuedir),
						w->w_name + 2,
						sequenceno,
						njobs);
				if (LogLevel > 8)
					sm_syslog(LOG_INFO, NOQID,
						  "runqueue: Flushing queue from %s/%s (pri %ld, LA %d, %d of %d)",
						  qid_printqueue(queuedir),
						  w->w_name + 2,
						  w->w_pri,
						  CurrentLA,
						  sequenceno,
						  njobs);
				break;
			}
			else if (Verbose)
				message("Skipping %s/%s (sequence %d of %d)",
					qid_printqueue(queuedir),
					w->w_name + 2,
					sequenceno, njobs);
		}
		else
		{
			pid_t pid;

			if (Verbose)
			{
				message("");
				message("Running %s/%s (sequence %d of %d)",
					qid_printqueue(queuedir),
					w->w_name + 2,
					sequenceno, njobs);
			}
			if (tTd(63, 100))
				sm_syslog(LOG_DEBUG, NOQID,
					  "runqueue %s dowork(%s)",
					  qid_printqueue(queuedir),
					  w->w_name + 2);

			pid = dowork(queuedir, w->w_name + 2,
				     ForkQueueRuns, FALSE, e);
			errno = 0;
			if (pid != 0)
				(void) waitfor(pid);
		}
		sm_free(w->w_name);
		if (w->w_host)
			sm_free(w->w_host);
		sm_free((char *) w);
	}

	/* exit without the usual cleanup */
	e->e_id = NULL;
	if (forkflag)
		finis(TRUE, ExitStat);
	/* NOTREACHED */
	return TRUE;
}

/*
**  RUNQUEUEEVENT -- stub for use in setevent
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	NOTE:	THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
**		ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
**		DOING.
*/

static void
runqueueevent()
{
	DoQueueRun = TRUE;
}
/*
**  ORDERQ -- order the work queue.
**
**	Parameters:
**		queuedir -- the index of the queue directory.
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
# define NEED_H		020

static WORK	*WorkList = NULL;
static int	WorkListSize = 0;

static int
orderq(queuedir, doall)
	int queuedir;
	bool doall;
{
	register struct dirent *d;
	register WORK *w;
	register char *p;
	DIR *f;
	register int i;
	int wn = -1;
	int wc;
	QUEUE_CHAR *check;
	char qd[MAXPATHLEN];
	char qf[MAXPATHLEN];

	if (queuedir == NOQDIR)
		(void) strlcpy(qd, ".", sizeof qd);
	else
		(void) snprintf(qd, sizeof qd, "%s%s",
				QPaths[queuedir].qp_name,
				(bitset(QP_SUBQF, QPaths[queuedir].qp_subdirs) ? "/qf" : ""));

	if (tTd(41, 1))
	{
		dprintf("orderq:\n");

		check = QueueLimitId;
		while (check != NULL)
		{
			dprintf("\tQueueLimitId = %s\n",
				check->queue_match);
			check = check->queue_next;
		}

		check = QueueLimitSender;
		while (check != NULL)
		{
			dprintf("\tQueueLimitSender = %s\n",
				check->queue_match);
			check = check->queue_next;
		}

		check = QueueLimitRecipient;
		while (check != NULL)
		{
			dprintf("\tQueueLimitRecipient = %s\n",
				check->queue_match);
			check = check->queue_next;
		}
	}

	/* clear out old WorkQ */
	for (w = WorkQ; w != NULL; )
	{
		register WORK *nw = w->w_next;

		WorkQ = nw;
		sm_free(w->w_name);
		if (w->w_host != NULL)
			sm_free(w->w_host);
		sm_free((char *) w);
		w = nw;
	}

	/* open the queue directory */
	f = opendir(qd);
	if (f == NULL)
	{
		syserr("orderq: cannot open \"%s\"", qid_printqueue(queuedir));
		return 0;
	}

	/*
	**  Read the work directory.
	*/

	while ((d = readdir(f)) != NULL)
	{
		FILE *cf;
		int qfver = 0;
		char lbuf[MAXNAME + 1];
		struct stat sbuf;

		if (tTd(41, 50))
			dprintf("orderq: checking %s\n", d->d_name);

		/* is this an interesting entry? */
		if (d->d_name[0] != 'q' || d->d_name[1] != 'f')
			continue;

		if (strlen(d->d_name) >= MAXQFNAME)
		{
			if (Verbose)
				printf("orderq: %s too long, %d max characters\n",
					d->d_name, MAXQFNAME);
			if (LogLevel > 0)
				sm_syslog(LOG_ALERT, NOQID,
					  "orderq: %s too long, %d max characters",
					  d->d_name, MAXQFNAME);
			continue;
		}

		check = QueueLimitId;
		while (check != NULL)
		{
			if (strcontainedin(check->queue_match, d->d_name))
				break;
			else
				check = check->queue_next;
		}
		if (QueueLimitId != NULL && check == NULL)
			continue;

		/* grow work list if necessary */
		if (++wn >= MaxQueueRun && MaxQueueRun > 0)
		{
			if (wn == MaxQueueRun && LogLevel > 0)
				sm_syslog(LOG_WARNING, NOQID,
					  "WorkList for %s maxed out at %d",
					  qid_printqueue(queuedir),
					  MaxQueueRun);
			continue;
		}
		if (wn >= WorkListSize)
		{
			grow_wlist(queuedir);
			if (wn >= WorkListSize)
				continue;
		}
		w = &WorkList[wn];

		(void) snprintf(qf, sizeof qf, "%s/%s", qd, d->d_name);
		if (stat(qf, &sbuf) < 0)
		{
			if (errno != ENOENT)
				sm_syslog(LOG_INFO, NOQID,
					  "orderq: can't stat %s/%s",
					  qid_printqueue(queuedir), d->d_name);
			wn--;
			continue;
		}
		if (!bitset(S_IFREG, sbuf.st_mode))
		{
			/* Yikes!  Skip it or we will hang on open! */
			syserr("orderq: %s/%s is not a regular file",
			       qid_printqueue(queuedir), d->d_name);
			wn--;
			continue;
		}

		/* avoid work if possible */
		if (QueueSortOrder == QSO_BYFILENAME &&
		    QueueLimitSender == NULL &&
		    QueueLimitRecipient == NULL)
		{
			w->w_name = newstr(d->d_name);
			w->w_host = NULL;
			w->w_lock = w->w_tooyoung = FALSE;
			w->w_pri = 0;
			w->w_ctime = 0;
			continue;
		}

		/* open control file */
		cf = fopen(qf, "r");

		if (cf == NULL)
		{
			/* this may be some random person sending hir msgs */
			/* syserr("orderq: cannot open %s", cbuf); */
			if (tTd(41, 2))
				dprintf("orderq: cannot open %s: %s\n",
					d->d_name, errstring(errno));
			errno = 0;
			wn--;
			continue;
		}
		w->w_name = newstr(d->d_name);
		w->w_host = NULL;
		w->w_lock = !lockfile(fileno(cf), w->w_name, NULL, LOCK_SH|LOCK_NB);
		w->w_tooyoung = FALSE;

		/* make sure jobs in creation don't clog queue */
		w->w_pri = 0x7fffffff;
		w->w_ctime = 0;

		/* extract useful information */
		i = NEED_P | NEED_T;
		if (QueueSortOrder == QSO_BYHOST)
		{
			/* need w_host set for host sort order */
			i |= NEED_H;
		}
		if (QueueLimitSender != NULL)
			i |= NEED_S;
		if (QueueLimitRecipient != NULL)
			i |= NEED_R;
		while (i != 0 && fgets(lbuf, sizeof lbuf, cf) != NULL)
		{
			int c;
			time_t age;

			p = strchr(lbuf, '\n');
			if (p != NULL)
				*p = '\0';
			else
			{
				/* flush rest of overly long line */
				while ((c = getc(cf)) != EOF && c != '\n')
					continue;
			}

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
				{
					w->w_host = strrev(&p[1]);
					makelower(w->w_host);
					i &= ~NEED_H;
				}
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
				check = QueueLimitRecipient;
				while (check != NULL)
				{
					if (strcontainedin(check->queue_match,
							   p))
						break;
					else
						check = check->queue_next;
				}
				if (check != NULL)
					i &= ~NEED_R;
				break;

			  case 'S':
				check = QueueLimitSender;
				while (check != NULL)
				{
					if (strcontainedin(check->queue_match,
							   &lbuf[1]))
						break;
					else
						check = check->queue_next;
				}
				if (check != NULL)
					i &= ~NEED_S;
				break;

			  case 'K':
				age = curtime() - (time_t) atol(&lbuf[1]);
				if (age >= 0 && MinQueueAge > 0 &&
				    age < MinQueueAge)
					w->w_tooyoung = TRUE;
				break;

			  case 'N':
				if (atol(&lbuf[1]) == 0)
					w->w_tooyoung = FALSE;
				break;

# if _FFR_QUEUEDELAY
/*
			  case 'G':
				queuealg = atoi(lbuf[1]);
				break;
			  case 'Y':
				queuedelay = (time_t) atol(&lbuf[1]);
				break;
*/
# endif /* _FFR_QUEUEDELAY */
			}
		}
		(void) fclose(cf);

		if ((!doall && shouldqueue(w->w_pri, w->w_ctime)) ||
		    bitset(NEED_R|NEED_S, i))
		{
			/* don't even bother sorting this job in */
			if (tTd(41, 49))
				dprintf("skipping %s (%x)\n", w->w_name, i);
			sm_free(w->w_name);
			if (w->w_host)
				sm_free(w->w_host);
			wn--;
		}
	}
	(void) closedir(f);
	wn++;

	WorkQ = NULL;
	if (WorkList == NULL)
		return 0;
	wc = min(wn, WorkListSize);
	if (wc > MaxQueueRun && MaxQueueRun > 0)
		wc = MaxQueueRun;

	if (QueueSortOrder == QSO_BYHOST)
	{
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
					 sm_strcasecmp(WorkList[i].w_host, w->w_host) == 0)
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
	else if (QueueSortOrder == QSO_BYTIME)
	{
		/*
		**  Simple sort based on submission time only.
		*/

		qsort((char *) WorkList, wc, sizeof *WorkList, workcmpf3);
	}
	else if (QueueSortOrder == QSO_BYFILENAME)
	{
		/*
		**  Sort based on qf filename.
		*/

		qsort((char *) WorkList, wc, sizeof *WorkList, workcmpf4);
	}
	else
	{
		/*
		**  Simple sort based on queue priority only.
		*/

		qsort((char *) WorkList, wc, sizeof *WorkList, workcmpf0);
	}

	/*
	**  Convert the work list into canonical form.
	**	Should be turning it into a list of envelopes here perhaps.
	*/

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
		sm_free(WorkList);
	WorkList = NULL;
	WorkListSize = 0;

	if (tTd(40, 1))
	{
		for (w = WorkQ; w != NULL; w = w->w_next)
		{
			if (w->w_host != NULL)
				dprintf("%22s: pri=%ld %s\n",
					w->w_name, w->w_pri, w->w_host);
			else
				dprintf("%32s: pri=%ld\n",
					w->w_name, w->w_pri);
		}
	}

	return wn;
}
/*
**  GROW_WLIST -- make the work list larger
**
**	Parameters:
**		queuedir -- the index for the queue directory.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Adds another QUEUESEGSIZE entries to WorkList if possible.
**		It can fail if there isn't enough memory, so WorkListSize
**		should be checked again upon return.
*/

static void
grow_wlist(queuedir)
	int queuedir;
{
	if (tTd(41, 1))
		dprintf("grow_wlist: WorkListSize=%d\n", WorkListSize);
	if (WorkList == NULL)
	{
		WorkList = (WORK *) xalloc((sizeof *WorkList) *
					   (QUEUESEGSIZE + 1));
		WorkListSize = QUEUESEGSIZE;
	}
	else
	{
		int newsize = WorkListSize + QUEUESEGSIZE;
		WORK *newlist = (WORK *) xrealloc((char *)WorkList,
						  (unsigned)sizeof(WORK) * (newsize + 1));

		if (newlist != NULL)
		{
			WorkListSize = newsize;
			WorkList = newlist;
			if (LogLevel > 1)
			{
				sm_syslog(LOG_INFO, NOQID,
					  "grew WorkList for %s to %d",
					  qid_printqueue(queuedir),
					  WorkListSize);
			}
		}
		else if (LogLevel > 0)
		{
			sm_syslog(LOG_ALERT, NOQID,
				  "FAILED to grow WorkList for %s to %d",
				  qid_printqueue(queuedir), newsize);
		}
	}
	if (tTd(41, 1))
		dprintf("grow_wlist: WorkListSize now %d\n", WorkListSize);
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

static int
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

static int
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
	    (i = sm_strcasecmp(a->w_host, b->w_host)) != 0)
		return i;

	/* lock status */
	if (a->w_lock != b->w_lock)
		return b->w_lock - a->w_lock;

	/* job priority */
	return workcmpf0(a, b);
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

static int
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
	    (i = sm_strcasecmp(a->w_host, b->w_host)) != 0)
		return i;

	/* job priority */
	return workcmpf0(a, b);
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

static int
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
**  WORKCMPF4 -- compare based on file name
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

static int
workcmpf4(a, b)
	register WORK *a;
	register WORK *b;
{
	return strcmp(a->w_name, b->w_name);
}
/*
**  STRREV -- reverse string
**
**	Returns a pointer to a new string that is the reverse of
**	the string pointed to by fwd.  The space for the new
**	string is obtained using xalloc().
**
**	Parameters:
**		fwd -- the string to reverse.
**
**	Returns:
**		the reversed string.
*/

static char *
strrev(fwd)
	char *fwd;
{
	char *rev = NULL;
	int len, cnt;

	len = strlen(fwd);
	rev = xalloc(len + 1);
	for (cnt = 0; cnt < len; ++cnt)
		rev[cnt] = fwd[len - cnt - 1];
	rev[len] = '\0';
	return rev;
}
/*
**  DOWORK -- do a work request.
**
**	Parameters:
**		queuedir -- the index of the queue directory for the job.
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
dowork(queuedir, id, forkflag, requeueflag, e)
	int queuedir;
	char *id;
	bool forkflag;
	bool requeueflag;
	register ENVELOPE *e;
{
	register pid_t pid;

	if (tTd(40, 1))
		dprintf("dowork(%s/%s)\n", qid_printqueue(queuedir), id);

	/*
	**  Fork for work.
	*/

	if (forkflag)
	{
		/*
		**  Since the delivery may happen in a child and the
		**  parent does not wait, the parent may close the
		**  maps thereby removing any shared memory used by
		**  the map.  Therefore, close the maps now so the
		**  child will dynamically open them if necessary.
		*/

		closemaps();

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
		else
		{
			/* child -- error messages to the transcript */
			QuickAbort = OnlyOneError = FALSE;
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

		/* Reset global flags */
		RestartRequest = NULL;
		ShutdownRequest = NULL;
		PendingSignal = 0;

		/* set basic modes, etc. */
		(void) alarm(0);
		clearstats();
		clearenvelope(e, FALSE);
		e->e_flags |= EF_QUEUERUN|EF_GLOBALERRS;
		set_delivery_mode(SM_DELIVER, e);
		e->e_errormode = EM_MAIL;
		e->e_id = id;
		e->e_queuedir = queuedir;
		GrabTo = UseErrorsTo = FALSE;
		ExitStat = EX_OK;
		if (forkflag)
		{
			disconnect(1, e);
			OpMode = MD_QUEUERUN;
		}
		sm_setproctitle(TRUE, e, "%s: from queue", qid_printname(e));
		if (LogLevel > 76)
			sm_syslog(LOG_DEBUG, e->e_id,
				  "dowork, pid=%d",
				  (int) getpid());

		/* don't use the headers from sendmail.cf... */
		e->e_header = NULL;

		/* read the queue control file -- return if locked */
		if (!readqf(e))
		{
			if (tTd(40, 4) && e->e_id != NULL)
				dprintf("readqf(%s) failed\n",
					qid_printname(e));
			e->e_id = NULL;
			if (forkflag)
				finis(FALSE, EX_OK);
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
			finis(TRUE, ExitStat);
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

static bool
readqf(e)
	register ENVELOPE *e;
{
	register FILE *qfp;
	ADDRESS *ctladdr;
	struct stat st, stf;
	char *bp;
	int qfver = 0;
	long hdrsize = 0;
	register char *p;
	char *orcpt = NULL;
	bool nomore = FALSE;
	MODE_T qsafe;
	char qf[MAXPATHLEN];
	char buf[MAXLINE];

	/*
	**  Read and process the file.
	*/

	(void) strlcpy(qf, queuename(e, 'q'), sizeof qf);
	qfp = fopen(qf, "r+");
	if (qfp == NULL)
	{
		int save_errno = errno;

		if (tTd(40, 8))
			dprintf("readqf(%s): fopen failure (%s)\n",
				qf, errstring(errno));
		errno = save_errno;
		if (errno != ENOENT
		    )
			syserr("readqf: no control file %s", qf);
		return FALSE;
	}

	if (!lockfile(fileno(qfp), qf, NULL, LOCK_EX|LOCK_NB))
	{
		/* being processed by another queuer */
		if (Verbose)
			printf("%s: locked\n", e->e_id);
		if (tTd(40, 8))
			dprintf("%s: locked\n", e->e_id);
		if (LogLevel > 19)
			sm_syslog(LOG_DEBUG, e->e_id, "locked");
		(void) fclose(qfp);
		return FALSE;
	}

	/*
	**  Prevent locking race condition.
	**
	**  Process A: readqf(): qfp = fopen(qffile)
	**  Process B: queueup(): rename(tf, qf)
	**  Process B: unlocks(tf)
	**  Process A: lockfile(qf);
	**
	**  Process A (us) has the old qf file (before the rename deleted
	**  the directory entry) and will be delivering based on old data.
	**  This can lead to multiple deliveries of the same recipients.
	**
	**  Catch this by checking if the underlying qf file has changed
	**  *after* acquiring our lock and if so, act as though the file
	**  was still locked (i.e., just return like the lockfile() case
	**  above.
	*/

	if (stat(qf, &stf) < 0 ||
	    fstat(fileno(qfp), &st) < 0)
	{
		/* must have been being processed by someone else */
		if (tTd(40, 8))
			dprintf("readqf(%s): [f]stat failure (%s)\n",
				qf, errstring(errno));
		(void) fclose(qfp);
		return FALSE;
	}

	if (st.st_nlink != stf.st_nlink ||
	    st.st_dev != stf.st_dev ||
	    st.st_ino != stf.st_ino ||
# if HAS_ST_GEN && 0		/* AFS returns garbage in st_gen */
	    st.st_gen != stf.st_gen ||
# endif /* HAS_ST_GEN && 0 */
	    st.st_uid != stf.st_uid ||
	    st.st_gid != stf.st_gid ||
	    st.st_size != stf.st_size)
	{
		/* changed after opened */
		if (Verbose)
			printf("%s: changed\n", e->e_id);
		if (tTd(40, 8))
			dprintf("%s: changed\n", e->e_id);
		if (LogLevel > 19)
			sm_syslog(LOG_DEBUG, e->e_id, "changed");
		(void) fclose(qfp);
		return FALSE;
	}

	/*
	**  Check the queue file for plausibility to avoid attacks.
	*/

	qsafe = S_IWOTH|S_IWGRP;
#if _FFR_QUEUE_FILE_MODE
	if (bitset(S_IWGRP, QueueFileMode))
		qsafe &= ~S_IWGRP;
#endif /* _FFR_QUEUE_FILE_MODE */

	if ((st.st_uid != geteuid() &&
	     st.st_uid != TrustedUid &&
	     geteuid() != RealUid) ||
	    bitset(qsafe, st.st_mode))
	{
		if (LogLevel > 0)
		{
			sm_syslog(LOG_ALERT, e->e_id,
				  "bogus queue file, uid=%d, mode=%o",
				  st.st_uid, st.st_mode);
		}
		if (tTd(40, 8))
			dprintf("readqf(%s): bogus file\n", qf);
		loseqfile(e, "bogus file uid in mqueue");
		(void) fclose(qfp);
		return FALSE;
	}

	if (st.st_size == 0)
	{
		/* must be a bogus file -- if also old, just remove it */
		if (st.st_ctime + 10 * 60 < curtime())
		{
			(void) xunlink(queuename(e, 'd'));
			(void) xunlink(queuename(e, 'q'));
		}
		(void) fclose(qfp);
		return FALSE;
	}

	if (st.st_nlink == 0)
	{
		/*
		**  Race condition -- we got a file just as it was being
		**  unlinked.  Just assume it is zero length.
		*/

		(void) fclose(qfp);
		return FALSE;
	}

	/* good file -- save this lock */
	e->e_lockfp = qfp;

	/* do basic system initialization */
	initsys(e);
	define('i', e->e_id, e);

	LineNumber = 0;
	e->e_flags |= EF_GLOBALERRS;
	OpMode = MD_QUEUERUN;
	ctladdr = NULL;
	e->e_dfino = -1;
	e->e_msgsize = -1;
# if _FFR_QUEUEDELAY
	e->e_queuealg = QD_LINEAR;
	e->e_queuedelay = (time_t) 0;
# endif /* _FFR_QUEUEDELAY */
	while ((bp = fgetfolded(buf, sizeof buf, qfp)) != NULL)
	{
		u_long qflags;
		ADDRESS *q;
		int mid;
		time_t now;
		auto char *ep;

		if (tTd(40, 4))
			dprintf("+++++ %s\n", bp);
		if (nomore)
		{
			/* hack attack */
			syserr("SECURITY ALERT: extra data in qf: %s", bp);
			(void) fclose(qfp);
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
			(void) fclose(qfp);
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

					  case 'A':
						if (ctladdr != NULL)
							ctladdr->q_flags |= QALIAS;
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
			(void) chompheader(&bp[1], CHHDR_QUEUE, NULL, e);
			hdrsize += strlen(&bp[1]);
			break;

		  case 'L':		/* Solaris Content-Length: */
		  case 'M':		/* message */
			/* ignore this; we want a new message next time */
			break;

		  case 'S':		/* sender */
			setsender(newstr(&bp[1]), e, NULL, '\0', TRUE);
			break;

		  case 'B':		/* body type */
			e->e_bodytype = newstr(&bp[1]);
			break;

# if _FFR_SAVE_CHARSET
		  case 'X':		/* character set */
			e->e_charset = newstr(&bp[1]);
			break;
# endif /* _FFR_SAVE_CHARSET */

		  case 'D':		/* data file name */
			/* obsolete -- ignore */
			break;

		  case 'T':		/* init time */
			e->e_ctime = atol(&bp[1]);
			break;

		  case 'I':		/* data file's inode number */
			/* regenerated below */
			break;

		  case 'K':	/* time of last delivery attempt */
			e->e_dtime = atol(&buf[1]);
			break;

# if _FFR_QUEUEDELAY
		  case 'G':	/* queue delay algorithm */
			e->e_queuealg = atoi(&buf[1]);
			break;
		  case 'Y':	/* current delay */
			e->e_queuedelay = (time_t) atol(&buf[1]);
			break;
# endif /* _FFR_QUEUEDELAY */

		  case 'N':		/* number of delivery attempts */
			e->e_ntries = atoi(&buf[1]);

			/* if this has been tried recently, let it be */
			now = curtime();
			if (e->e_ntries > 0 && e->e_dtime <= now &&
			    now < e->e_dtime + queuedelay(e))
			{
				char *howlong;

				howlong = pintvl(now - e->e_dtime, TRUE);
				if (Verbose)
					printf("%s: too young (%s)\n",
					       e->e_id, howlong);
				if (tTd(40, 8))
					dprintf("%s: too young (%s)\n",
						e->e_id, howlong);
				if (LogLevel > 19)
					sm_syslog(LOG_DEBUG, e->e_id,
						  "too young (%s)",
						  howlong);
				e->e_id = NULL;
				unlockqueue(e);
				return FALSE;
			}
			define(macid("{ntries}", NULL), newstr(&buf[1]), e);

# if NAMED_BIND
			/* adjust BIND parameters immediately */
			if (e->e_ntries == 0)
			{
				_res.retry = TimeOuts.res_retry[RES_TO_FIRST];
				_res.retrans = TimeOuts.res_retrans[RES_TO_FIRST];
			}
			else
			{
				_res.retry = TimeOuts.res_retry[RES_TO_NORMAL];
				_res.retrans = TimeOuts.res_retrans[RES_TO_NORMAL];
			}
# endif /* NAMED_BIND */
			break;

		  case 'P':		/* message priority */
			e->e_msgpriority = atol(&bp[1]) + WkTimeFact;
			break;

		  case 'F':		/* flag bits */
			if (strncmp(bp, "From ", 5) == 0)
			{
				/* we are being spoofed! */
				syserr("SECURITY ALERT: bogus qf line %s", bp);
				(void) fclose(qfp);
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
			define(macid("{dsn_envid}", NULL), newstr(&bp[1]), e);
			break;

		  case 'A':		/* AUTH= parameter */
			e->e_auth_param = newstr(&bp[1]);
			break;

		  case '$':		/* define macro */
			{
				char *p;

				mid = macid(&bp[1], &ep);
				if (mid == 0)
					break;

				p = newstr(ep);
				define(mid, p, e);

				/*
				**  HACK ALERT: Unfortunately, 8.10 and
				**  8.11 reused the ${if_addr} and
				**  ${if_family} macros for both the incoming
				**  interface address/family (getrequests())
				**  and the outgoing interface address/family
				**  (makeconnection()).  In order for D_BINDIF
				**  to work properly, have to preserve the
				**  incoming information in the queue file for
				**  later delivery attempts.  The original
				**  information is stored in the envelope
				**  in readqf() so it can be stored in
				**  queueup_macros().  This should be fixed
				**  in 8.12.
				*/

				if (strcmp(macname(mid), "if_addr") == 0)
					e->e_if_macros[EIF_ADDR] = p;
			}
			break;

		  case '.':		/* terminate file */
			nomore = TRUE;
			break;

		  default:
			syserr("readqf: %s: line %d: bad line \"%s\"",
				qf, LineNumber, shortenstring(bp, MAXSHORTSTR));
			(void) fclose(qfp);
			loseqfile(e, "unrecognized line");
			return FALSE;
		}

		if (bp != buf)
			sm_free(bp);
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

	/* possibly set ${dsn_ret} macro */
	if (bitset(EF_RET_PARAM, e->e_flags))
	{
		if (bitset(EF_NO_BODY_RETN, e->e_flags))
			define(macid("{dsn_ret}", NULL), "hdrs", e);
		else
			define(macid("{dsn_ret}", NULL), "full", e);
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
**  PRTSTR -- print a string, "unprintable" characters are shown as \oct
**
**	Parameters:
**		s -- string to print
**		ml -- maximum length of output
**
**	Returns:
**		none.
**
**	Side Effects:
**		Prints a string on stdout.
*/

static void
prtstr(s, ml)
	char *s;
	int ml;
{
	char c;

	if (s == NULL)
		return;
	while (ml-- > 0 && ((c = *s++) != '\0'))
	{
		if (c == '\\')
		{
			if (ml-- > 0)
			{
				putchar(c);
				putchar(c);
			}
		}
		else if (isascii(c) && isprint(c))
			putchar(c);
		else
		{
			if ((ml -= 3) > 0)
				printf("\\%03o", c);
		}
	}
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
	int i, nrequests = 0;

	for (i = 0; i < NumQueues; i++)
	{
		if (StopRequest)
			stop_sendmail();
		nrequests += print_single_queue(i);
	}
	if (NumQueues > 1)
		printf("\t\tTotal Requests: %d\n", nrequests);
}
/*
**  PRINT_SINGLE_QUEUE -- print out a representation of a single mail queue
**
**	Parameters:
**		queuedir -- queue directory
**
**	Returns:
**		number of entries
**
**	Side Effects:
**		Prints a listing of the mail queue on the standard output.
*/

static int
print_single_queue(queuedir)
	int queuedir;
{
	register WORK *w;
	FILE *f;
	int nrequests;
	char qd[MAXPATHLEN];
	char qddf[MAXPATHLEN];
	char buf[MAXLINE];

	if (queuedir == NOQDIR)
	{
		(void) strlcpy(qd, ".", sizeof qd);
		(void) strlcpy(qddf, ".", sizeof qddf);
	}
	else
	{
		(void) snprintf(qd, sizeof qd, "%s%s",
				QPaths[queuedir].qp_name,
				(bitset(QP_SUBQF, QPaths[queuedir].qp_subdirs) ? "/qf" : ""));
		(void) snprintf(qddf, sizeof qddf, "%s%s",
				QPaths[queuedir].qp_name,
				(bitset(QP_SUBDF, QPaths[queuedir].qp_subdirs) ? "/df" : ""));
	}

	/*
	**  Check for permission to print the queue
	*/

	if (bitset(PRIV_RESTRICTMAILQ, PrivacyFlags) && RealUid != 0)
	{
		struct stat st;
# ifdef NGROUPS_MAX
		int n;
		extern GIDSET_T InitialGidSet[NGROUPS_MAX];
# endif /* NGROUPS_MAX */

		if (stat(qd, &st) < 0)
		{
			syserr("Cannot stat %s", qid_printqueue(queuedir));
			return 0;
		}
# ifdef NGROUPS_MAX
		n = NGROUPS_MAX;
		while (--n >= 0)
		{
			if (InitialGidSet[n] == st.st_gid)
				break;
		}
		if (n < 0 && RealGid != st.st_gid)
# else /* NGROUPS_MAX */
		if (RealGid != st.st_gid)
# endif /* NGROUPS_MAX */
		{
			usrerr("510 You are not permitted to see the queue");
			setstat(EX_NOPERM);
			return 0;
		}
	}

	/*
	**  Read and order the queue.
	*/

	nrequests = orderq(queuedir, TRUE);

	/*
	**  Print the work list that we have read.
	*/

	/* first see if there is anything */
	if (nrequests <= 0)
	{
		printf("%s is empty\n", qid_printqueue(queuedir));
		return 0;
	}

	CurrentLA = sm_getla(NULL);	/* get load average */

	printf("\t\t%s (%d request%s", qid_printqueue(queuedir), nrequests,
	       nrequests == 1 ? "" : "s");
	if (MaxQueueRun > 0 && nrequests > MaxQueueRun)
		printf(", only %d printed", MaxQueueRun);
	if (Verbose)
		printf(")\n----Q-ID---- --Size-- -Priority- ---Q-Time--- ---------Sender/Recipient--------\n");
	else
		printf(")\n----Q-ID---- --Size-- -----Q-Time----- ------------Sender/Recipient------------\n");
	for (w = WorkQ; w != NULL; w = w->w_next)
	{
		struct stat st;
		auto time_t submittime = 0;
		long dfsize;
		int flags = 0;
		int qfver;
		char statmsg[MAXLINE];
		char bodytype[MAXNAME + 1];
		char qf[MAXPATHLEN];

		if (StopRequest)
			stop_sendmail();

		printf("%12s", w->w_name + 2);
		(void) snprintf(qf, sizeof qf, "%s/%s", qd, w->w_name);
		f = fopen(qf, "r");
		if (f == NULL)
		{
			printf(" (job completed)\n");
			errno = 0;
			continue;
		}
		w->w_name[0] = 'd';
		(void) snprintf(qf, sizeof qf, "%s/%s", qddf, w->w_name);
		if (stat(qf, &st) >= 0)
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

			if (StopRequest)
				stop_sendmail();

			fixcrlf(buf, TRUE);
			switch (buf[0])
			{
			  case 'V':	/* queue file version */
				qfver = atoi(&buf[1]);
				break;

			  case 'M':	/* error message */
				if ((i = strlen(&buf[1])) >= sizeof statmsg)
					i = sizeof statmsg - 1;
				memmove(statmsg, &buf[1], i);
				statmsg[i] = '\0';
				break;

			  case 'B':	/* body type */
				if ((i = strlen(&buf[1])) >= sizeof bodytype)
					i = sizeof bodytype - 1;
				memmove(bodytype, &buf[1], i);
				bodytype[i] = '\0';
				break;

			  case 'S':	/* sender name */
				if (Verbose)
				{
					printf("%8ld %10ld%c%.12s ",
					       dfsize,
					       w->w_pri,
					       bitset(EF_WARNING, flags) ? '+' : ' ',
					       ctime(&submittime) + 4);
					prtstr(&buf[1], 78);
				}
				else
				{
					printf("%8ld %.16s ", dfsize,
					    ctime(&submittime));
					prtstr(&buf[1], 40);
				}
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
				{
					printf("\n\t\t\t\t\t      ");
					prtstr(p, 73);
				}
				else
				{
					printf("\n\t\t\t\t       ");
					prtstr(p, 40);
				}
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
	return nrequests;
}
/*
**  QUEUENAME -- build a file name in the queue directory for this envelope.
**
**	Parameters:
**		e -- envelope to build it in/from.
**		type -- the file type, used as the first character
**			of the file name.
**
**	Returns:
**		a pointer to the queue name (in a static buffer).
**
**	Side Effects:
**		If no id code is already assigned, queuename() will
**		assign an id code with assign_queueid().  If no queue
**		directory is assigned, one will be set with setnewqueue().
*/

char *
queuename(e, type)
	register ENVELOPE *e;
	int type;
{
	char *sub = "";
	static char buf[MAXPATHLEN];

	/* Assign an ID if needed */
	if (e->e_id == NULL)
		assign_queueid(e);

	/* Assign a queue directory if needed */
	if (e->e_queuedir == NOQDIR)
		setnewqueue(e);

	if (e->e_queuedir == NOQDIR)
		(void) snprintf(buf, sizeof buf, "%cf%s",
				type, e->e_id);
	else
	{
		switch (type)
		{
		  case 'd':
			if (bitset(QP_SUBDF, QPaths[e->e_queuedir].qp_subdirs))
				sub = "/df";
			break;

		  case TEMPQF_LETTER:
		  case 't':
		  case LOSEQF_LETTER:
		  case 'q':
			if (bitset(QP_SUBQF, QPaths[e->e_queuedir].qp_subdirs))
				sub = "/qf";
			break;

		  case 'x':
			if (bitset(QP_SUBXF, QPaths[e->e_queuedir].qp_subdirs))
				sub = "/xf";
			break;
		}

		(void) snprintf(buf, sizeof buf, "%s%s/%cf%s",
				QPaths[e->e_queuedir].qp_name,
				sub, type, e->e_id);
	}

	if (tTd(7, 2))
		dprintf("queuename: %s\n", buf);
	return buf;
}
/*
**  ASSIGN_QUEUEID -- assign a queue ID for this envelope.
**
**	Assigns an id code if one does not already exist.
**	This code assumes that nothing will remain in the queue for
**	longer than 60 years.  It is critical that files with the given
**	name not already exist in the queue.
**	Also initializes e_queuedir to NOQDIR.
**
**	Parameters:
**		e -- envelope to set it in.
**
**	Returns:
**		none.
*/

static const char QueueIdChars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwx";
# define QIC_LEN	60

void
assign_queueid(e)
	register ENVELOPE *e;
{
	pid_t pid = getpid();
	static char cX = 0;
	static long random_offset;
	struct tm *tm;
	char idbuf[MAXQFNAME - 2];

	if (e->e_id != NULL)
		return;

	/* see if we need to get a new base time/pid */
	if (cX >= QIC_LEN || LastQueueTime == 0 || LastQueuePid != pid)
	{
		time_t then = LastQueueTime;

		/* if the first time through, pick a random offset */
		if (LastQueueTime == 0)
			random_offset = get_random();

		while ((LastQueueTime = curtime()) == then &&
		       LastQueuePid == pid)
		{
			(void) sleep(1);
		}
		LastQueuePid = getpid();
		cX = 0;
	}
	if (tTd(7, 50))
		dprintf("assign_queueid: random_offset = %ld (%d)\n",
			random_offset, (int)(cX + random_offset) % QIC_LEN);

	tm = gmtime(&LastQueueTime);
	idbuf[0] = QueueIdChars[tm->tm_year % QIC_LEN];
	idbuf[1] = QueueIdChars[tm->tm_mon];
	idbuf[2] = QueueIdChars[tm->tm_mday];
	idbuf[3] = QueueIdChars[tm->tm_hour];
	idbuf[4] = QueueIdChars[tm->tm_min];
	idbuf[5] = QueueIdChars[tm->tm_sec];
	idbuf[6] = QueueIdChars[((int)cX++ + random_offset) % QIC_LEN];
	(void) snprintf(&idbuf[7], sizeof idbuf - 7, "%05d",
			(int) LastQueuePid);
	e->e_id = newstr(idbuf);
	define('i', e->e_id, e);
	e->e_queuedir = NOQDIR;
	if (tTd(7, 1))
		dprintf("assign_queueid: assigned id %s, e=%lx\n",
			e->e_id, (u_long) e);
	if (LogLevel > 93)
		sm_syslog(LOG_DEBUG, e->e_id, "assigned id");
}
/*
**  SYNC_QUEUE_TIME -- Assure exclusive PID in any given second
**
**	Make sure one PID can't be used by two processes in any one second.
**
**		If the system rotates PIDs fast enough, may get the
**		same pid in the same second for two distinct processes.
**		This will interfere with the queue file naming system.
**
**	Parameters:
**		none
**
**	Returns:
**		none
*/
void
sync_queue_time()
{
# if FAST_PID_RECYCLE
	if (OpMode != MD_TEST &&
	    OpMode != MD_VERIFY &&
	    LastQueueTime > 0 &&
	    LastQueuePid == getpid() &&
	    curtime() == LastQueueTime)
		(void) sleep(1);
# endif /* FAST_PID_RECYCLE */
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
		dprintf("unlockqueue(%s)\n",
			e->e_id == NULL ? "NOQUEUE" : e->e_id);


	/* if there is a lock file in the envelope, close it */
	if (e->e_lockfp != NULL)
		(void) fclose(e->e_lockfp);
	e->e_lockfp = NULL;

	/* don't create a queue id if we don't already have one */
	if (e->e_id == NULL)
		return;

	/* remove the transcript */
	if (LogLevel > 87)
		sm_syslog(LOG_DEBUG, e->e_id, "unlock");
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

static ADDRESS *
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
	memset((char *) a, '\0', sizeof *a);

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
			if (*pw->pw_dir == '\0')
				a->q_home = NULL;
			else if (strcmp(pw->pw_dir, "/") == 0)
				a->q_home = "";
			else
				a->q_home = newstr(pw->pw_dir);
			a->q_uid = pw->pw_uid;
			a->q_gid = pw->pw_gid;
			a->q_flags |= QGOODUID;
		}
	}

	a->q_flags |= QPRIMARY;		/* flag as a "ctladdr" */
	a->q_mailer = LocalMailer;
	if (p == NULL)
		a->q_paddr = newstr(a->q_user);
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
	char buf[MAXPATHLEN];

	if (e == NULL || e->e_id == NULL)
		return;
	p = queuename(e, 'q');
	if (strlen(p) >= (SIZE_T) sizeof buf)
		return;
	(void) strlcpy(buf, p, sizeof buf);
	p = queuename(e, LOSEQF_LETTER);
	if (rename(buf, p) < 0)
		syserr("cannot rename(%s, %s), uid=%d", buf, p, geteuid());
	else if (LogLevel > 0)
		sm_syslog(LOG_ALERT, e->e_id,
			  "Losing %s: %s", buf, why);
}
/*
**  QID_PRINTNAME -- create externally printable version of queue id
**
**	Parameters:
**		e -- the envelope.
**
**	Returns:
**		a printable version
*/

char *
qid_printname(e)
	ENVELOPE *e;
{
	char *id;
	static char idbuf[MAXQFNAME + 34];

	if (e == NULL)
		return "";

	if (e->e_id == NULL)
		id = "";
	else
		id = e->e_id;

	if (e->e_queuedir == NOQDIR)
		return id;

	(void) snprintf(idbuf, sizeof idbuf, "%.32s/%s",
			QPaths[e->e_queuedir].qp_name, id);
	return idbuf;
}
/*
**  QID_PRINTQUEUE -- create full version of queue directory for df files
**
**	Parameters:
**		queuedir -- the short version of the queue directory
**
**	Returns:
**		the full pathname to the queue (static)
*/

char *
qid_printqueue(queuedir)
	int queuedir;
{
	char *subdir;
	static char dir[MAXPATHLEN];

	if (queuedir == NOQDIR)
		return QueueDir;

	if (strcmp(QPaths[queuedir].qp_name, ".") == 0)
		subdir = NULL;
	else
		subdir = QPaths[queuedir].qp_name;

	(void) snprintf(dir, sizeof dir, "%s%s%s%s", QueueDir,
			subdir == NULL ? "" : "/",
			subdir == NULL ? "" : subdir,
			(bitset(QP_SUBDF, QPaths[queuedir].qp_subdirs) ? "/df" : ""));
	return dir;
}
/*
**  SETNEWQUEUE -- Sets a new queue directory
**
**	Assign a queue directory to an envelope and store the directory
**	in e->e_queuedir.  The queue is chosen at random.
**
**	This routine may be improved in the future to allow for more
**	elaborate queueing schemes.  Suggestions and code contributions
**	are welcome.
**
**	Parameters:
**		e -- envelope to assign a queue for.
**
**	Returns:
**		none.
*/

void
setnewqueue(e)
	ENVELOPE *e;
{
	int idx;

	if (tTd(41, 20))
		dprintf("setnewqueue: called\n");

	if (e->e_queuedir != NOQDIR)
	{
		if (tTd(41, 20))
			dprintf("setnewqueue: e_queuedir already assigned (%s)\n",
				qid_printqueue(e->e_queuedir));
		return;
	}

	if (NumQueues == 1)
		idx = 0;
	else
	{
#if RANDOMSHIFT
		/* lower bits are not random "enough", select others */
		idx = (get_random() >> RANDOMSHIFT) % NumQueues;
#else /* RANDOMSHIFT */
		idx = get_random() % NumQueues;
#endif /* RANDOMSHIFT */
		if (tTd(41, 15))
			dprintf("setnewqueue: get_random() %% %d = %d\n",
				NumQueues, idx);
	}

	e->e_queuedir = idx;
	if (tTd(41, 3))
		dprintf("setnewqueue: Assigned queue directory %s\n",
			qid_printqueue(e->e_queuedir));
}

/*
**  CHKQDIR -- check a queue directory
**
**	Parameters:
**		name -- name of queue directory
**		sff -- flags for safefile()
**
**	Returns:
**		is it a queue directory?
*/

static bool
chkqdir(name, sff)
	char *name;
	long sff;
{
	struct stat statb;
	int i;

	/* skip over . and .. directories */
	if (name[0] == '.' &&
	    (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
		return FALSE;
# if HASLSTAT
	if (lstat(name, &statb) < 0)
# else /* HASLSTAT */
	if (stat(name, &statb) < 0)
# endif /* HASLSTAT */
	{
		if (tTd(41, 2))
			dprintf("multiqueue_cache: stat(\"%s\"): %s\n",
				name, errstring(errno));
		return FALSE;
	}
# if HASLSTAT
	if (S_ISLNK(statb.st_mode))
	{
		/*
		**  For a symlink we need to make sure the
		**  target is a directory
		*/
		if (stat(name, &statb) < 0)
		{
			if (tTd(41, 2))
				dprintf("multiqueue_cache: stat(\"%s\"): %s\n",
					name, errstring(errno));
			return FALSE;
		}
	}
# endif /* HASLSTAT */

	if (!S_ISDIR(statb.st_mode))
	{
		if (tTd(41, 2))
			dprintf("multiqueue_cache: \"%s\": Not a directory\n",
				name);
		return FALSE;
	}

	/* Print a warning if unsafe (but still use it) */
	i = safedirpath(name, RunAsUid, RunAsGid, NULL, sff, 0, 0);
	if (i != 0 && tTd(41, 2))
		dprintf("multiqueue_cache: \"%s\": Not safe: %s\n",
			name, errstring(i));
	return TRUE;
}

/*
**  MULTIQUEUE_CACHE -- cache a list of paths to queues.
**
**	Each potential queue is checked as the cache is built.
**	Thereafter, each is blindly trusted.
**	Note that we can be called again after a timeout to rebuild
**	(although code for that is not ready yet).
**
**	Parameters:
**		none
**
**	Returns:
**		none
*/

void
multiqueue_cache()
{
	register DIR *dp;
	register struct dirent *d;
	char *cp;
	int i, len;
	int slotsleft = 0;
	long sff = SFF_ANYFILE;
	char qpath[MAXPATHLEN];
	char subdir[MAXPATHLEN];

	if (tTd(41, 20))
		dprintf("multiqueue_cache: called\n");

	if (NumQueues != 0 && QPaths != NULL)
	{
		for (i = 0; i < NumQueues; i++)
		{
			if (QPaths[i].qp_name != NULL)
				sm_free(QPaths[i].qp_name);
		}
		sm_free((char *)QPaths);
		QPaths = NULL;
		NumQueues = 0;
	}

	/* If running as root, allow safedirpath() checks to use privs */
	if (RunAsUid == 0)
		sff |= SFF_ROOTOK;

	(void) snprintf(qpath, sizeof qpath, "%s", QueueDir);
	len = strlen(qpath) - 1;
	cp = &qpath[len];
	if (*cp == '*')
	{
		*cp = '\0';
		if ((cp = strrchr(qpath, '/')) == NULL)
		{
			syserr("QueueDirectory: can not wildcard relative path");
			if (tTd(41, 2))
				dprintf("multiqueue_cache: \"%s\": Can not wildcard relative path.\n",
					qpath);
			ExitStat = EX_CONFIG;
			return;
		}
		if (cp == qpath)
		{
			/*
			**  Special case of top level wildcard, like /foo*
			*/

			(void) snprintf(qpath + 1, sizeof qpath - 1,
					"%s", qpath);
			++cp;
		}
		*(cp++) = '\0';
		len = strlen(cp);

		if (tTd(41, 2))
			dprintf("multiqueue_cache: prefix=\"%s\"\n", cp);

		QueueDir = newstr(qpath);

		/*
		**  XXX Should probably wrap this whole loop in a timeout
		**  in case some wag decides to NFS mount the queues.
		*/

		/* test path to get warning messages */
		i= safedirpath(QueueDir, RunAsUid, RunAsGid, NULL, sff, 0, 0);
		if (i != 0 && tTd(41, 2))
			dprintf("multiqueue_cache: \"%s\": Not safe: %s\n",
				QueueDir, errstring(i));

		if (chdir(QueueDir) < 0)
		{
			syserr("can not chdir(%s)", QueueDir);
			if (tTd(41, 2))
				dprintf("multiqueue_cache: \"%s\": %s\n",
					qpath, errstring(errno));
			ExitStat = EX_CONFIG;
			return;
		}

		if ((dp = opendir(".")) == NULL)
		{
			syserr("can not opendir(%s)", QueueDir);
			if (tTd(41, 2))
				dprintf("multiqueue_cache: opendir(\"%s\"): %s\n",
					QueueDir, errstring(errno));
			ExitStat = EX_CONFIG;
			return;
		}
		while ((d = readdir(dp)) != NULL)
		{
			if (strncmp(d->d_name, cp, len) != 0)
			{
				if (tTd(41, 5))
					dprintf("multiqueue_cache: \"%s\", skipped\n",
						d->d_name);
				continue;
			}
			if (!chkqdir(d->d_name, sff))
				continue;

			if (QPaths == NULL)
			{
				slotsleft = 20;
				QPaths = (QPATHS *)xalloc((sizeof *QPaths) *
							  slotsleft);
				NumQueues = 0;
			}
			else if (slotsleft < 1)
			{
				QPaths = (QPATHS *)xrealloc((char *)QPaths,
							    (sizeof *QPaths) *
							    (NumQueues + 10));
				if (QPaths == NULL)
				{
					(void) closedir(dp);
					return;
				}
				slotsleft += 10;
			}

			/* check subdirs */
			QPaths[NumQueues].qp_subdirs = QP_NOSUB;
			(void) snprintf(subdir, sizeof subdir, "%s/%s/%s",
					qpath, d->d_name, "qf");
			if (chkqdir(subdir, sff))
				QPaths[NumQueues].qp_subdirs |= QP_SUBQF;

			(void) snprintf(subdir, sizeof subdir, "%s/%s/%s",
					qpath, d->d_name, "df");
			if (chkqdir(subdir, sff))
				QPaths[NumQueues].qp_subdirs |= QP_SUBDF;

			(void) snprintf(subdir, sizeof subdir, "%s/%s/%s",
					qpath, d->d_name, "xf");
			if (chkqdir(subdir, sff))
				QPaths[NumQueues].qp_subdirs |= QP_SUBXF;

			/* assert(strlen(d->d_name) < MAXPATHLEN - 14) */
			/* maybe even - 17 (subdirs) */
			QPaths[NumQueues].qp_name = newstr(d->d_name);
			if (tTd(41, 2))
				dprintf("multiqueue_cache: %d: \"%s\" cached (%x).\n",
					NumQueues, d->d_name,
					QPaths[NumQueues].qp_subdirs);
			NumQueues++;
			slotsleft--;
		}
		(void) closedir(dp);
	}
	if (NumQueues == 0)
	{
		if (*cp != '*' && tTd(41, 2))
			dprintf("multiqueue_cache: \"%s\": No wildcard suffix character\n",
				QueueDir);
		QPaths = (QPATHS *)xalloc(sizeof *QPaths);
		QPaths[0].qp_name = newstr(".");
		QPaths[0].qp_subdirs = QP_NOSUB;
		NumQueues = 1;

		/* test path to get warning messages */
		(void) safedirpath(QueueDir, RunAsUid, RunAsGid,
				   NULL, sff, 0, 0);
		if (chdir(QueueDir) < 0)
		{
			syserr("can not chdir(%s)", QueueDir);
			if (tTd(41, 2))
				dprintf("multiqueue_cache: \"%s\": %s\n",
					QueueDir, errstring(errno));
			ExitStat = EX_CONFIG;
		}

		/* check subdirs */
		(void) snprintf(subdir, sizeof subdir, "%s/qf", QueueDir);
		if (chkqdir(subdir, sff))
			QPaths[0].qp_subdirs |= QP_SUBQF;

		(void) snprintf(subdir, sizeof subdir, "%s/df",	QueueDir);
		if (chkqdir(subdir, sff))
			QPaths[0].qp_subdirs |= QP_SUBDF;

		(void) snprintf(subdir, sizeof subdir, "%s/xf", QueueDir);
		if (chkqdir(subdir, sff))
			QPaths[0].qp_subdirs |= QP_SUBXF;
	}
}

# if 0
/*
**  HASHFQN -- calculate a hash value for a fully qualified host name
**
**	Arguments:
**		fqn -- an all lower-case host.domain string
**		buckets -- the number of buckets (queue directories)
**
**	Returns:
**		a bucket number (signed integer)
**		-1 on error
**
**	Contributed by Exactis.com, Inc.
*/

int
hashfqn(fqn, buckets)
	register char *fqn;
	int buckets;
{
	register char *p;
	register int h = 0, hash, cnt;
#  define WATERINC (1000)

	if (fqn == NULL)
		return -1;

	/*
	**  A variation on the gdb hash
	**  This is the best as of Feb 19, 1996 --bcx
	*/

	p = fqn;
	h = 0x238F13AF * strlen(p);
	for (cnt = 0; *p != 0; ++p, cnt++)
	{
		h = (h + (*p << (cnt * 5 % 24))) & 0x7FFFFFFF;
	}
	h = (1103515243 * h + 12345) & 0x7FFFFFFF;
	if (buckets < 2)
		hash = 0;
	else
		hash = (h % buckets);

	return hash;
}
# endif /* 0 */

# if _FFR_QUEUEDELAY
/*
**  QUEUEDELAY -- compute queue delay time
**
**	Parameters:
**		e -- the envelope to queue up.
**
**	Returns:
**		queue delay time
**
**	Side Effects:
**		may change e_queuedelay
*/

static time_t
queuedelay(e)
	ENVELOPE *e;
{
	time_t qd;

	if (e->e_queuealg == QD_EXP)
	{
		if (e->e_queuedelay == 0)
			e->e_queuedelay = QueueInitDelay;
		else
		{
			e->e_queuedelay *= 2;
			if (e->e_queuedelay > QueueMaxDelay)
				e->e_queuedelay = QueueMaxDelay;
		}
		qd = e->e_queuedelay;
	}
	else
		qd = MinQueueAge;
	return qd;
}
# endif /* _FFR_QUEUEDELAY */
#endif /* QUEUE */
