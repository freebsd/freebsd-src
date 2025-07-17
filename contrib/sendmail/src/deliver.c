/*
 * Copyright (c) 1998-2010, 2012, 2020-2023 Proofpoint, Inc. and its suppliers.
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
#include <sm/time.h>

SM_RCSID("@(#)$Id: deliver.c,v 8.1030 2013-11-22 20:51:55 ca Exp $")

#include <sm/sendmail.h>

#if HASSETUSERCONTEXT
# include <login_cap.h>
#endif

#if NETINET || NETINET6
# include <arpa/inet.h>
#endif

#if STARTTLS || SASL
# include "sfsasl.h"
# include "tls.h"
#endif

#if !_FFR_DMTRIGGER
static int	deliver __P((ENVELOPE *, ADDRESS *));
#endif
static void	dup_queue_file __P((ENVELOPE *, ENVELOPE *, int));
static void	mailfiletimeout __P((int));
static void	endwaittimeout __P((int));
static int	parse_hostsignature __P((char *, char **, MAILER *
#if DANE
		, BITMAP256
#endif
		));
static void	sendenvelope __P((ENVELOPE *, int));
static int	coloncmp __P((const char *, const char *));


#if STARTTLS
# include <openssl/err.h>
# if DANE
static int	starttls __P((MAILER *, MCI *, ENVELOPE *, bool, dane_vrfy_ctx_P));

# define MXADS_ISSET(mxads, i)	(0 != bitnset(bitidx(i), mxads))
# define MXADS_SET(mxads, i)	setbitn(bitidx(i), mxads)

/* use "marks" in hostsignature for "had ad"? (WIP) */
#  ifndef HSMARKS
#   define HSMARKS 1
#  endif
#  if HSMARKS
#   define HSM_AD	'+'	/* mark for hostsignature: ad flag */
#   define DANE_SEC(dane)	(DANE_SECURE == DANEMODE((dane)))
#  endif
# else /* DANE */
static int	starttls __P((MAILER *, MCI *, ENVELOPE *, bool));
# define MXADS_ISSET(mxads, i)	0
# endif /* DANE */
static int	endtlsclt __P((MCI *));
#endif /* STARTTLS */
#if STARTTLS || SASL
static bool	iscltflgset __P((ENVELOPE *, int));
#endif

#define SEP_MXHOSTS(endp, sep)	\
		if (endp != NULL)	\
		{	\
			sep = *endp;	\
			*endp = '\0';	\
		}

#if NETINET6
# define FIX_MXHOSTS(hp, endp, sep)	\
	do {	\
		if (*hp == '[')	\
		{	\
			endp = strchr(hp + 1, ']'); \
			if (endp != NULL)	\
				endp = strpbrk(endp + 1, ":,");	\
		}	\
		else	\
			endp = strpbrk(hp, ":,"); \
		SEP_MXHOSTS(endp, sep);	\
	} while (0)
#else /* NETINET6 */
# define FIX_MXHOSTS(hp, endp, sep)	\
	do {	\
		endp = strpbrk(hp, ":,"); \
		SEP_MXHOSTS(endp, sep);	\
	} while (0)
#endif /* NETINET6 */

#if _FFR_OCC
# include <ratectrl.h>
#endif

#define ESCNULLMXRCPT "5.1.10"
#define ERRNULLMX "556 Host does not accept mail: MX 0 ."

#if _FFR_LOG_FAILOVER
/*
**  These are not very useful to show the protocol stage,
**  but it's better than nothing right now.
**  XXX the actual values must be 0..N, otherwise a lookup
**  table must be used!
*/

static char *mcis[] =
{
	"CLOSED",
	"GREET",
	"OPEN",
	"MAIL",
	"RCPT",
	"DATA",
	"QUITING",
	"SSD",
	"ERROR",
	NULL
};
#endif /* _FFR_LOG_FAILOVER */

#if _FFR_LOG_STAGE
static char *xs_states[] =
{
	"none",
	"STARTTLS",
	"AUTH",
	"GREET",
	"EHLO",
	"MAIL",
	"RCPT",
	"DATA",
	"EOM",
	"DATA2",
	"QUIT",
	NULL
};
#endif /* _FFR_LOG_STAGE */

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

void
sendall(e, mode)
	ENVELOPE *e;
	int mode;
{
	register ADDRESS *q;
	char *owner;
	int otherowners;
	int save_errno;
	register ENVELOPE *ee;
	ENVELOPE *splitenv = NULL;
	int oldverbose = Verbose;
	bool somedeliveries = false, expensive = false;
	pid_t pid;

	/*
	**  If this message is to be discarded, don't bother sending
	**  the message at all.
	*/

	if (bitset(EF_DISCARD, e->e_flags))
	{
		if (tTd(13, 1))
			sm_dprintf("sendall: discarding id %s\n", e->e_id);
		e->e_flags |= EF_CLRQUEUE;
		if (LogLevel > 9)
			logundelrcpts(e, "discarded", 9, true);
		else if (LogLevel > 4)
			sm_syslog(LOG_INFO, e->e_id, "discarded");
		markstats(e, NULL, STATS_REJECT);
		return;
	}

	/*
	**  If we have had global, fatal errors, don't bother sending
	**  the message at all if we are in SMTP mode.  Local errors
	**  (e.g., a single address failing) will still cause the other
	**  addresses to be sent.
	*/

	if (bitset(EF_FATALERRS, e->e_flags) &&
	    (OpMode == MD_SMTP || OpMode == MD_DAEMON))
	{
		e->e_flags |= EF_CLRQUEUE;
		return;
	}

	/* determine actual delivery mode */
	if (mode == SM_DEFAULT)
	{
		mode = e->e_sendmode;
		if (mode != SM_VERIFY && mode != SM_DEFER &&
		    shouldqueue(e->e_msgpriority, e->e_ctime))
			mode = SM_QUEUE;
	}

	if (tTd(13, 1))
	{
		sm_dprintf("\n===== SENDALL: mode %c, id %s, e_from ",
			mode, e->e_id);
		printaddr(sm_debug_file(), &e->e_from, false);
		sm_dprintf("\te_flags = ");
		printenvflags(e);
		sm_dprintf("sendqueue:\n");
		printaddr(sm_debug_file(), e->e_sendqueue, true);
	}

	/*
	**  Do any preprocessing necessary for the mode we are running.
	**	Check to make sure the hop count is reasonable.
	**	Delete sends to the sender in mailing lists.
	*/

	CurEnv = e;
	if (tTd(62, 1))
		checkfds(NULL);

	if (e->e_hopcount > MaxHopCount)
	{
		char *recip;

		if (e->e_sendqueue != NULL &&
		    e->e_sendqueue->q_paddr != NULL)
			recip = e->e_sendqueue->q_paddr;
		else
			recip = "(nobody)";

		errno = 0;
		queueup(e, WILL_BE_QUEUED(mode) ? QUP_FL_ANNOUNCE : QUP_FL_NONE);
		e->e_flags |= EF_FATALERRS|EF_PM_NOTIFY|EF_CLRQUEUE;
		ExitStat = EX_UNAVAILABLE;
		syserr("554 5.4.6 Too many hops %d (%d max): from %s via %s, to %s",
		       e->e_hopcount, MaxHopCount, e->e_from.q_paddr,
		       RealHostName == NULL ? "localhost" : RealHostName,
		       recip);
		for (q = e->e_sendqueue; q != NULL; q = q->q_next)
		{
			if (QS_IS_DEAD(q->q_state))
				continue;
			q->q_state = QS_BADADDR;
			q->q_status = "5.4.6";
			q->q_rstatus = "554 5.4.6 Too many hops";
		}
		return;
	}

	/*
	**  Do sender deletion.
	**
	**	If the sender should be queued up, skip this.
	**	This can happen if the name server is hosed when you
	**	are trying to send mail.  The result is that the sender
	**	is instantiated in the queue as a recipient.
	*/

	if (!bitset(EF_METOO, e->e_flags) &&
	    !QS_IS_QUEUEUP(e->e_from.q_state))
	{
		if (tTd(13, 5))
		{
			sm_dprintf("sendall: QS_SENDER ");
			printaddr(sm_debug_file(), &e->e_from, false);
		}
		e->e_from.q_state = QS_SENDER;
		(void) recipient(&e->e_from, &e->e_sendqueue, 0, e);
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
		    !QS_IS_DEAD(q->q_state) &&
		    strcmp(q->q_owner, e->e_from.q_paddr) == 0)
			q->q_owner = NULL;
	}

	if (tTd(13, 25))
	{
		sm_dprintf("\nAfter first owner pass, sendq =\n");
		printaddr(sm_debug_file(), e->e_sendqueue, true);
	}

	owner = "";
	otherowners = 1;
	while (owner != NULL && otherowners > 0)
	{
		if (tTd(13, 28))
			sm_dprintf("owner = \"%s\", otherowners = %d\n",
				   owner, otherowners);
		owner = NULL;
		otherowners = bitset(EF_SENDRECEIPT, e->e_flags) ? 1 : 0;

		for (q = e->e_sendqueue; q != NULL; q = q->q_next)
		{
			if (tTd(13, 30))
			{
				sm_dprintf("Checking ");
				printaddr(sm_debug_file(), q, false);
			}
			if (QS_IS_DEAD(q->q_state))
			{
				if (tTd(13, 30))
					sm_dprintf("    ... QS_IS_DEAD\n");
				continue;
			}
			if (tTd(13, 29) && !tTd(13, 30))
			{
				sm_dprintf("Checking ");
				printaddr(sm_debug_file(), q, false);
			}

			if (q->q_owner != NULL)
			{
				if (owner == NULL)
				{
					if (tTd(13, 40))
						sm_dprintf("    ... First owner = \"%s\"\n",
							   q->q_owner);
					owner = q->q_owner;
				}
				else if (owner != q->q_owner)
				{
					if (strcmp(owner, q->q_owner) == 0)
					{
						if (tTd(13, 40))
							sm_dprintf("    ... Same owner = \"%s\"\n",
								   owner);

						/* make future comparisons cheap */
						q->q_owner = owner;
					}
					else
					{
						if (tTd(13, 40))
							sm_dprintf("    ... Another owner \"%s\"\n",
								   q->q_owner);
						otherowners++;
					}
					owner = q->q_owner;
				}
				else if (tTd(13, 40))
					sm_dprintf("    ... Same owner = \"%s\"\n",
						   owner);
			}
			else
			{
				if (tTd(13, 40))
					sm_dprintf("    ... Null owner\n");
				otherowners++;
			}

			if (QS_IS_BADADDR(q->q_state))
			{
				if (tTd(13, 30))
					sm_dprintf("    ... QS_IS_BADADDR\n");
				continue;
			}

			if (QS_IS_QUEUEUP(q->q_state))
			{
				MAILER *m = q->q_mailer;

				/*
				**  If we have temporary address failures
				**  (e.g., dns failure) and a fallback MX is
				**  set, send directly to the fallback MX host.
				*/

				if (FallbackMX != NULL &&
				    !wordinclass(FallbackMX, 'w') &&
				    mode != SM_VERIFY &&
				    !bitnset(M_NOMX, m->m_flags) &&
				    strcmp(m->m_mailer, "[IPC]") == 0 &&
				    m->m_argv[0] != NULL &&
				    strcmp(m->m_argv[0], "TCP") == 0)
				{
					int len;
					char *p;

					if (tTd(13, 30))
						sm_dprintf("    ... FallbackMX\n");

					len = strlen(FallbackMX) + 1;
					p = sm_rpool_malloc_x(e->e_rpool, len);
					(void) sm_strlcpy(p, FallbackMX, len);
					q->q_state = QS_OK;
					q->q_host = p;
				}
				else
				{
					if (tTd(13, 30))
						sm_dprintf("    ... QS_IS_QUEUEUP\n");
					continue;
				}
			}

			/*
			**  If this mailer is expensive, and if we don't
			**  want to make connections now, just mark these
			**  addresses and return.  This is useful if we
			**  want to batch connections to reduce load.  This
			**  will cause the messages to be queued up, and a
			**  daemon will come along to send the messages later.
			*/

			if (NoConnect && !Verbose &&
			    bitnset(M_EXPENSIVE, q->q_mailer->m_flags))
			{
				if (tTd(13, 30))
					sm_dprintf("    ... expensive\n");
				q->q_state = QS_QUEUEUP;
				expensive = true;
			}
			else if (bitnset(M_HOLD, q->q_mailer->m_flags) &&
				 QueueLimitId == NULL &&
				 QueueLimitSender == NULL &&
				 QueueLimitRecipient == NULL)
			{
				if (tTd(13, 30))
					sm_dprintf("    ... hold\n");
				q->q_state = QS_QUEUEUP;
				expensive = true;
			}
			else if (QueueMode != QM_QUARANTINE &&
				 e->e_quarmsg != NULL)
			{
				if (tTd(13, 30))
					sm_dprintf("    ... quarantine: %s\n",
						   e->e_quarmsg);
				q->q_state = QS_QUEUEUP;
				expensive = true;
			}
			else
			{
				if (tTd(13, 30))
					sm_dprintf("    ... deliverable\n");
				somedeliveries = true;
			}
		}

		if (owner != NULL && otherowners > 0)
		{
			/*
			**  Split this envelope into two.
			*/

			ee = (ENVELOPE *) sm_rpool_malloc_x(e->e_rpool,
							    sizeof(*ee));
			STRUCTCOPY(*e, *ee);
			ee->e_message = NULL;
			ee->e_id = NULL;
			assign_queueid(ee);

			if (tTd(13, 1))
				sm_dprintf("sendall: split %s into %s, owner = \"%s\", otherowners = %d\n",
					   e->e_id, ee->e_id, owner,
					   otherowners);

			ee->e_header = copyheader(e->e_header, ee->e_rpool);
			ee->e_sendqueue = copyqueue(e->e_sendqueue,
						    ee->e_rpool);
			ee->e_errorqueue = copyqueue(e->e_errorqueue,
						     ee->e_rpool);
			ee->e_flags = e->e_flags & ~(EF_INQUEUE|EF_CLRQUEUE|EF_FATALERRS|EF_SENDRECEIPT|EF_RET_PARAM);
			ee->e_flags |= EF_NORECEIPT;
			setsender(owner, ee, NULL, '\0', true);
			if (tTd(13, 5))
			{
				sm_dprintf("sendall(split): QS_SENDER ");
				printaddr(sm_debug_file(), &ee->e_from, false);
			}
			ee->e_from.q_state = QS_SENDER;
			ee->e_dfp = NULL;
			ee->e_lockfp = NULL;
			ee->e_xfp = NULL;
			ee->e_qgrp = e->e_qgrp;
			ee->e_qdir = e->e_qdir;
			ee->e_errormode = EM_MAIL;
			ee->e_sibling = splitenv;
			ee->e_statmsg = NULL;
			if (e->e_quarmsg != NULL)
				ee->e_quarmsg = sm_rpool_strdup_x(ee->e_rpool,
								  e->e_quarmsg);
			splitenv = ee;

			for (q = e->e_sendqueue; q != NULL; q = q->q_next)
			{
				if (q->q_owner == owner)
				{
					q->q_state = QS_CLONED;
					if (tTd(13, 6))
						sm_dprintf("\t... stripping %s from original envelope\n",
							   q->q_paddr);
				}
			}
			for (q = ee->e_sendqueue; q != NULL; q = q->q_next)
			{
				if (q->q_owner != owner)
				{
					q->q_state = QS_CLONED;
					if (tTd(13, 6))
						sm_dprintf("\t... dropping %s from cloned envelope\n",
							   q->q_paddr);
				}
				else
				{
					/* clear DSN parameters */
					q->q_flags &= ~(QHASNOTIFY|Q_PINGFLAGS);
					q->q_flags |= DefaultNotify & ~QPINGONSUCCESS;
					if (tTd(13, 6))
						sm_dprintf("\t... moving %s to cloned envelope\n",
							   q->q_paddr);
				}
			}

			if (mode != SM_VERIFY && bitset(EF_HAS_DF, e->e_flags))
				dup_queue_file(e, ee, DATAFL_LETTER);

			/*
			**  Give the split envelope access to the parent
			**  transcript file for errors obtained while
			**  processing the recipients (done before the
			**  envelope splitting).
			*/

			if (e->e_xfp != NULL)
				ee->e_xfp = sm_io_dup(e->e_xfp);

			/* failed to dup e->e_xfp, start a new transcript */
			if (ee->e_xfp == NULL)
				openxscript(ee);

			if (mode != SM_VERIFY && LogLevel > 4)
				sm_syslog(LOG_INFO, e->e_id,
					  "%s: clone: owner=%s",
					  ee->e_id, owner);
		}
	}

	if (owner != NULL)
	{
		setsender(owner, e, NULL, '\0', true);
		if (tTd(13, 5))
		{
			sm_dprintf("sendall(owner): QS_SENDER ");
			printaddr(sm_debug_file(), &e->e_from, false);
		}
		e->e_from.q_state = QS_SENDER;
		e->e_errormode = EM_MAIL;
		e->e_flags |= EF_NORECEIPT;
		e->e_flags &= ~EF_FATALERRS;
	}

	/* if nothing to be delivered, just queue up everything */
	if (!somedeliveries && !WILL_BE_QUEUED(mode) &&
	    mode != SM_VERIFY)
	{
		time_t now;

		if (tTd(13, 29))
			sm_dprintf("No deliveries: auto-queueing\n");
		mode = SM_QUEUE;
		now = curtime();

		/* treat this as a delivery in terms of counting tries */
		e->e_dtime = now;
		if (!expensive)
			e->e_ntries++;
		for (ee = splitenv; ee != NULL; ee = ee->e_sibling)
		{
			ee->e_dtime = now;
			if (!expensive)
				ee->e_ntries++;
		}
	}

	if ((WILL_BE_QUEUED(mode) || mode == SM_FORK ||
	     (mode != SM_VERIFY &&
	      (SuperSafe == SAFE_REALLY ||
	       SuperSafe == SAFE_REALLY_POSTMILTER))) &&
	    (!bitset(EF_INQUEUE, e->e_flags) || splitenv != NULL))
	{
		unsigned int qup_flags;

		/*
		**  Be sure everything is instantiated in the queue.
		**  Split envelopes first in case the machine crashes.
		**  If the original were done first, we may lose
		**  recipients.
		*/

		if (WILL_BE_QUEUED(mode))
			qup_flags = QUP_FL_ANNOUNCE;
		else
			qup_flags = QUP_FL_NONE;
#if HASFLOCK
		if (mode == SM_FORK)
			qup_flags |= QUP_FL_MSYNC;
#endif

		for (ee = splitenv; ee != NULL; ee = ee->e_sibling)
			queueup(ee, qup_flags);
		queueup(e, qup_flags);
	}

	if (tTd(62, 10))
		checkfds("after envelope splitting");

	/*
	**  If we belong in background, fork now.
	*/

	if (tTd(13, 20))
	{
		sm_dprintf("sendall: final mode = %c\n", mode);
		if (tTd(13, 21))
		{
			sm_dprintf("\n================ Final Send Queue(s) =====================\n");
			sm_dprintf("\n  *** Envelope %s, e_from=%s ***\n",
				   e->e_id, e->e_from.q_paddr);
			printaddr(sm_debug_file(), e->e_sendqueue, true);
			for (ee = splitenv; ee != NULL; ee = ee->e_sibling)
			{
				sm_dprintf("\n  *** Envelope %s, e_from=%s ***\n",
					   ee->e_id, ee->e_from.q_paddr);
				printaddr(sm_debug_file(), ee->e_sendqueue, true);
			}
			sm_dprintf("==========================================================\n\n");
		}
	}
	switch (mode)
	{
	  case SM_VERIFY:
		Verbose = 2;
		break;

	  case SM_QUEUE:
	  case SM_DEFER:
#if HASFLOCK
  queueonly:
#endif
		if (e->e_nrcpts > 0)
			e->e_flags |= EF_INQUEUE;
		(void) dropenvelope(e, splitenv != NULL, true);
		for (ee = splitenv; ee != NULL; ee = ee->e_sibling)
		{
			if (ee->e_nrcpts > 0)
				ee->e_flags |= EF_INQUEUE;
			(void) dropenvelope(ee, false, true);
		}
		return;

	  case SM_FORK:
		if (e->e_xfp != NULL)
			(void) sm_io_flush(e->e_xfp, SM_TIME_DEFAULT);

#if !HASFLOCK
		/*
		**  Since fcntl locking has the interesting semantic that
		**  the lock is owned by a process, not by an open file
		**  descriptor, we have to flush this to the queue, and
		**  then restart from scratch in the child.
		*/

		{
			/* save id for future use */
			char *qid = e->e_id;

			/* now drop the envelope in the parent */
			e->e_flags |= EF_INQUEUE;
			(void) dropenvelope(e, splitenv != NULL, false);

			/* arrange to reacquire lock after fork */
			e->e_id = qid;
		}

		for (ee = splitenv; ee != NULL; ee = ee->e_sibling)
		{
			/* save id for future use */
			char *qid = ee->e_id;

			/* drop envelope in parent */
			ee->e_flags |= EF_INQUEUE;
			(void) dropenvelope(ee, false, false);

			/* and save qid for reacquisition */
			ee->e_id = qid;
		}

#endif /* !HASFLOCK */

		/*
		**  Since the delivery may happen in a child and the parent
		**  does not wait, the parent may close the maps thereby
		**  removing any shared memory used by the map.  Therefore,
		**  close the maps now so the child will dynamically open
		**  them if necessary.
		*/

		closemaps(false);

		pid = fork();
		if (pid < 0)
		{
			syserr("deliver: fork 1");
#if HASFLOCK
			goto queueonly;
#else /* HASFLOCK */
			e->e_id = NULL;
			for (ee = splitenv; ee != NULL; ee = ee->e_sibling)
				ee->e_id = NULL;
			return;
#endif /* HASFLOCK */
		}
		else if (pid > 0)
		{
#if HASFLOCK
			/* be sure we leave the temp files to our child */
			/* close any random open files in the envelope */
			closexscript(e);
			SM_CLOSE_FP(e->e_dfp);
			e->e_flags &= ~EF_HAS_DF;

			/* can't call unlockqueue to avoid unlink of xfp */
			if (e->e_lockfp != NULL)
				(void) sm_io_close(e->e_lockfp, SM_TIME_DEFAULT);
			else
				syserr("%s: sendall: null lockfp", e->e_id);
			e->e_lockfp = NULL;
#endif /* HASFLOCK */

			/* make sure the parent doesn't own the envelope */
			e->e_id = NULL;

#if USE_DOUBLE_FORK
			/* catch intermediate zombie */
			(void) waitfor(pid);
#endif
			return;
		}

		/* Reset global flags */
		RestartRequest = NULL;
		RestartWorkGroup = false;
		ShutdownRequest = NULL;
		PendingSignal = 0;

		/*
		**  Initialize exception stack and default exception
		**  handler for child process.
		*/

		sm_exc_newthread(fatal_error);

		/*
		**  Since we have accepted responsbility for the message,
		**  change the SIGTERM handler.  intsig() (the old handler)
		**  would remove the envelope if this was a command line
		**  message submission.
		*/

		(void) sm_signal(SIGTERM, SIG_DFL);

#if USE_DOUBLE_FORK
		/* double fork to avoid zombies */
		pid = fork();
		if (pid > 0)
			exit(EX_OK);
		save_errno = errno;
#endif /* USE_DOUBLE_FORK */

		CurrentPid = getpid();

		/* be sure we are immune from the terminal */
		disconnect(2, e);
		clearstats();

		/* prevent parent from waiting if there was an error */
		if (pid < 0)
		{
			errno = save_errno;
			syserr("deliver: fork 2");
#if HASFLOCK
			e->e_flags |= EF_INQUEUE;
#else
			e->e_id = NULL;
#endif
			finis(true, true, ExitStat);
		}

		/* be sure to give error messages in child */
		QuickAbort = false;

		/*
		**  Close any cached connections.
		**
		**	We don't send the QUIT protocol because the parent
		**	still knows about the connection.
		**
		**	This should only happen when delivering an error
		**	message.
		*/

		mci_flush(false, NULL);

#if HASFLOCK
		break;
#else /* HASFLOCK */

		/*
		**  Now reacquire and run the various queue files.
		*/

		for (ee = splitenv; ee != NULL; ee = ee->e_sibling)
		{
			ENVELOPE *sibling = ee->e_sibling;

			(void) dowork(ee->e_qgrp, ee->e_qdir, ee->e_id,
				      false, false, ee);
			ee->e_sibling = sibling;
		}
		(void) dowork(e->e_qgrp, e->e_qdir, e->e_id,
			      false, false, e);
		finis(true, true, ExitStat);
#endif /* HASFLOCK */
	}

	sendenvelope(e, mode);
	(void) dropenvelope(e, true, true);
	for (ee = splitenv; ee != NULL; ee = ee->e_sibling)
	{
		CurEnv = ee;
		if (mode != SM_VERIFY)
			openxscript(ee);
		sendenvelope(ee, mode);
		(void) dropenvelope(ee, true, true);
	}
	CurEnv = e;

	Verbose = oldverbose;
	if (mode == SM_FORK)
		finis(true, true, ExitStat);
}

static void
sendenvelope(e, mode)
	register ENVELOPE *e;
	int mode;
{
	register ADDRESS *q;
	bool didany;

	if (tTd(13, 10))
		sm_dprintf("sendenvelope(%s) e_flags=0x%lx\n",
			   e->e_id == NULL ? "[NOQUEUE]" : e->e_id,
			   e->e_flags);
	if (LogLevel > 80)
		sm_syslog(LOG_DEBUG, e->e_id,
			  "sendenvelope, flags=0x%lx",
			  e->e_flags);

	/*
	**  If we have had global, fatal errors, don't bother sending
	**  the message at all if we are in SMTP mode.  Local errors
	**  (e.g., a single address failing) will still cause the other
	**  addresses to be sent.
	*/

	if (bitset(EF_FATALERRS, e->e_flags) &&
	    (OpMode == MD_SMTP || OpMode == MD_DAEMON))
	{
		e->e_flags |= EF_CLRQUEUE;
		return;
	}

	/*
	**  Don't attempt deliveries if we want to bounce now
	**  or if deliver-by time is exceeded.
	*/

	if (!bitset(EF_RESPONSE, e->e_flags) &&
	    (TimeOuts.to_q_return[e->e_timeoutclass] == NOW ||
	     (IS_DLVR_RETURN(e) && e->e_deliver_by > 0 &&
	      curtime() > e->e_ctime + e->e_deliver_by)))
		return;

	/*
	**  Run through the list and send everything.
	**
	**	Set EF_GLOBALERRS so that error messages during delivery
	**	result in returned mail.
	*/

	e->e_nsent = 0;
	e->e_flags |= EF_GLOBALERRS;

	macdefine(&e->e_macro, A_PERM, macid("{envid}"), e->e_envid);
	macdefine(&e->e_macro, A_PERM, macid("{bodytype}"), e->e_bodytype);
	didany = false;

	if (!bitset(EF_SPLIT, e->e_flags))
	{
		ENVELOPE *oldsib;
		ENVELOPE *ee;

		/*
		**  Save old sibling and set it to NULL to avoid
		**  queueing up the same envelopes again.
		**  This requires that envelopes in that list have
		**  been take care of before (or at some other place).
		*/

		oldsib = e->e_sibling;
		e->e_sibling = NULL;
		if (!split_by_recipient(e) &&
		    bitset(EF_FATALERRS, e->e_flags))
		{
			if (OpMode == MD_SMTP || OpMode == MD_DAEMON)
				e->e_flags |= EF_CLRQUEUE;
			return;
		}
		for (ee = e->e_sibling; ee != NULL; ee = ee->e_sibling)
			queueup(ee, QUP_FL_MSYNC);

		/* clean up */
		for (ee = e->e_sibling; ee != NULL; ee = ee->e_sibling)
		{
			/* now unlock the job */
			closexscript(ee);
			unlockqueue(ee);

			/* this envelope is marked unused */
			SM_CLOSE_FP(ee->e_dfp);
			ee->e_id = NULL;
			ee->e_flags &= ~EF_HAS_DF;
		}
		e->e_sibling = oldsib;
	}

	/* now run through the queue */
	for (q = e->e_sendqueue; q != NULL; q = q->q_next)
	{
#if XDEBUG
		char wbuf[MAXNAME + 20]; /* EAI: might be too short, but that's ok for debugging */

		(void) sm_snprintf(wbuf, sizeof(wbuf), "sendall(%.*s)",
				   MAXNAME, q->q_paddr); /* EAI: see above */
		checkfd012(wbuf);
#endif /* XDEBUG */
		if (mode == SM_VERIFY)
		{
			e->e_to = q->q_paddr;
			if (QS_IS_SENDABLE(q->q_state))
			{
				if (q->q_host != NULL && q->q_host[0] != '\0')
					message("deliverable: mailer %s, host %s, user %s",
						q->q_mailer->m_name,
						q->q_host,
						q->q_user);
				else
					message("deliverable: mailer %s, user %s",
						q->q_mailer->m_name,
						q->q_user);
			}
		}
		else if (QS_IS_OK(q->q_state))
		{
			/*
			**  Checkpoint the send list every few addresses
			*/

			if (CheckpointInterval > 0 &&
			    e->e_nsent >= CheckpointInterval)
			{
				queueup(e, QUP_FL_NONE);
				e->e_nsent = 0;
			}
			(void) deliver(e, q);
			didany = true;
		}
	}
	if (didany)
	{
		e->e_dtime = curtime();
		e->e_ntries++;
	}

#if XDEBUG
	checkfd012("end of sendenvelope");
#endif
}

#if REQUIRES_DIR_FSYNC
/*
**  SYNC_DIR -- fsync a directory based on a filename
**
**	Parameters:
**		filename -- path of file
**		panic -- panic?
**
**	Returns:
**		none
*/

void
sync_dir(filename, panic)
	char *filename;
	bool panic;
{
	int dirfd;
	char *dirp;
	char dir[MAXPATHLEN];

	if (!RequiresDirfsync)
		return;

	/* filesystems which require the directory be synced */
	dirp = strrchr(filename, '/');
	if (dirp != NULL)
	{
		if (sm_strlcpy(dir, filename, sizeof(dir)) >= sizeof(dir))
			return;
		dir[dirp - filename] = '\0';
		dirp = dir;
	}
	else
		dirp = ".";
	dirfd = open(dirp, O_RDONLY, 0700);
	if (tTd(40,32))
		sm_syslog(LOG_INFO, NOQID, "sync_dir: %s: fsync(%d)",
			  dirp, dirfd);
	if (dirfd >= 0)
	{
		if (fsync(dirfd) < 0)
		{
			if (panic)
				syserr("!sync_dir: cannot fsync directory %s",
				       dirp);
			else if (LogLevel > 1)
				sm_syslog(LOG_ERR, NOQID,
					  "sync_dir: cannot fsync directory %s: %s",
					  dirp, sm_errstring(errno));
		}
		(void) close(dirfd);
	}
}
#endif /* REQUIRES_DIR_FSYNC */
/*
**  DUP_QUEUE_FILE -- duplicate a queue file into a split queue
**
**	Parameters:
**		e -- the existing envelope
**		ee -- the new envelope
**		type -- the queue file type (e.g., DATAFL_LETTER)
**
**	Returns:
**		none
*/

static void
dup_queue_file(e, ee, type)
	ENVELOPE *e, *ee;
	int type;
{
	char f1buf[MAXPATHLEN], f2buf[MAXPATHLEN];

	ee->e_dfp = NULL;
	ee->e_xfp = NULL;

	/*
	**  Make sure both are in the same directory.
	*/

	(void) sm_strlcpy(f1buf, queuename(e, type), sizeof(f1buf));
	(void) sm_strlcpy(f2buf, queuename(ee, type), sizeof(f2buf));

	/* Force the df to disk if it's not there yet */
	if (type == DATAFL_LETTER && e->e_dfp != NULL &&
	    sm_io_setinfo(e->e_dfp, SM_BF_COMMIT, NULL) < 0 &&
	    errno != EINVAL)
	{
		syserr("!dup_queue_file: can't commit %s", f1buf);
		/* NOTREACHED */
	}

	if (link(f1buf, f2buf) < 0)
	{
		int save_errno = errno;

		syserr("sendall: link(%s, %s)", f1buf, f2buf);
		if (save_errno == EEXIST)
		{
			if (unlink(f2buf) < 0)
			{
				syserr("!sendall: unlink(%s): permanent",
				       f2buf);
				/* NOTREACHED */
			}
			if (link(f1buf, f2buf) < 0)
			{
				syserr("!sendall: link(%s, %s): permanent",
				       f1buf, f2buf);
				/* NOTREACHED */
			}
		}
	}
	SYNC_DIR(f2buf, true);
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

#define NFORKTRIES	5

#ifndef FORK
# define FORK	fork
#endif

#define DOFORK(fORKfN) \
{\
	register int i;\
\
	for (i = NFORKTRIES; --i >= 0; )\
	{\
		pid = fORKfN();\
		if (pid >= 0)\
			break;\
		if (i > 0)\
			(void) sleep((unsigned) NFORKTRIES - i);\
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

pid_t
dofork()
{
	register pid_t pid = -1;

	DOFORK(fork);
	return pid;
}

/*
**  COLONCMP -- compare host-signatures up to first ':' or EOS
**
**	This takes two strings which happen to be host-signatures and
**	compares them. If the lowest preference portions of the MX-RR's
**	match (up to ':' or EOS, whichever is first), then we have
**	match. This is used for coattail-piggybacking messages during
**	message delivery.
**	If the signatures are the same up to the first ':' the remainder of
**	the signatures are then compared with a normal strcmp(). This saves
**	re-examining the first part of the signatures.
**
**	Parameters:
**		a - first host-signature
**		b - second host-signature
**
**	Returns:
**		HS_MATCH_NO -- no "match".
**		HS_MATCH_FIRST -- "match" for the first MX preference
**			(up to the first colon (':')).
**		HS_MATCH_FULL -- match for the entire MX record.
**		HS_MATCH_SKIP -- match but only one of the entries has a "mark"
**
**	Side Effects:
**		none.
*/

#define HS_MATCH_NO	0
#define HS_MATCH_FIRST	1
#define HS_MATCH_FULL	2
#define HS_MATCH_SKIP	4

static int
coloncmp(a, b)
	register const char *a;
	register const char *b;
{
	int ret = HS_MATCH_NO;
	int braclev = 0;
# if HSMARKS
	bool a_hsmark = false;
	bool b_hsmark = false;

	if (HSM_AD == *a)
	{
		a_hsmark = true;
		++a;
	}
	if (HSM_AD == *b)
	{
		b_hsmark = true;
		++b;
	}
# endif
	while (*a == *b++)
	{
		/* Need to account for IPv6 bracketed addresses */
		if (*a == '[')
			braclev++;
		else if (*a == ']' && braclev > 0)
			braclev--;
		else if (*a == ':' && braclev <= 0)
		{
			ret = HS_MATCH_FIRST;
			a++;
			break;
		}
		else if (*a == '\0')
		{
# if HSMARKS
			/* exactly one mark */
			if (a_hsmark != b_hsmark)
				return HS_MATCH_SKIP;
# endif
			return HS_MATCH_FULL; /* a full match */
		}
		a++;
	}
	if (ret == HS_MATCH_NO &&
	    braclev <= 0 &&
	    ((*a == '\0' && *(b - 1) == ':') ||
	     (*a == ':' && *(b - 1) == '\0')))
		return HS_MATCH_FIRST;
	if (ret == HS_MATCH_FIRST && strcmp(a, b) == 0)
	{
# if HSMARKS
		/* exactly one mark */
		if (a_hsmark != b_hsmark)
			return HS_MATCH_SKIP;
# endif
		return HS_MATCH_FULL;
	}

	return ret;
}

/*
**  SHOULD_TRY_FBSH -- Should try FallbackSmartHost?
**
**	Parameters:
**		e -- envelope
**		tried_fallbacksmarthost -- has been tried already? (in/out)
**		hostbuf -- buffer for hostname (expand FallbackSmartHost) (out)
**		hbsz -- size of hostbuf
**		status -- current delivery status
**
**	Returns:
**		true iff FallbackSmartHost should be tried.
*/

static bool should_try_fbsh __P((ENVELOPE *, bool *, char *, size_t, int));

static bool
should_try_fbsh(e, tried_fallbacksmarthost, hostbuf, hbsz, status)
	ENVELOPE *e;
	bool *tried_fallbacksmarthost;
	char *hostbuf;
	size_t hbsz;
	int status;
{
	/*
	**  If the host was not found or a temporary failure occurred
	**  and a FallbackSmartHost is defined (and we have not yet
	**  tried it), then make one last try with it as the host.
	*/

	if ((status == EX_NOHOST || status == EX_TEMPFAIL) &&
	    FallbackSmartHost != NULL && !*tried_fallbacksmarthost)
	{
		*tried_fallbacksmarthost = true;
		expand(FallbackSmartHost, hostbuf, hbsz, e);
		if (!wordinclass(hostbuf, 'w'))
		{
			if (tTd(11, 1))
				sm_dprintf("one last try with FallbackSmartHost %s\n",
					   hostbuf);
			return true;
		}
	}
	return false;
}

#if STARTTLS || SASL
/*
**  CLTFEATURES -- Get features for SMTP client
**
**	Parameters:
**		e -- envelope
**		servername -- name of server.
**
**	Returns:
**		EX_OK or EX_TEMPFAIL
*/

static int cltfeatures __P((ENVELOPE *, char *));
static int
cltfeatures(e, servername)
	ENVELOPE *e;
	char *servername;
{
	int r, i, idx;
	char **pvp, c;
	char pvpbuf[PSBUFSIZE];
	char flags[64];	/* XXX */

	SM_ASSERT(e != NULL);
	SM_ASSERT(e->e_mci != NULL);
	macdefine(&e->e_mci->mci_macro, A_PERM, macid("{client_flags}"), "");
	pvp = NULL;
	r = rscap("clt_features", servername, NULL, e, &pvp, pvpbuf,
		  sizeof(pvpbuf));
	if (r != EX_OK)
		return EX_OK;
	if (pvp == NULL || pvp[0] == NULL || (pvp[0][0] & 0377) != CANONNET)
		return EX_OK;
	if (pvp[1] != NULL && sm_strncasecmp(pvp[1], "temp", 4) == 0)
		return EX_TEMPFAIL;

	/* XXX Note: this does not inherit defaults! */
	for (idx = 0, i = 1; pvp[i] != NULL; i++)
	{
		c = pvp[i][0];
		if (!(isascii(c) && !isspace(c) && isprint(c)))
			continue;
		if (idx >= sizeof(flags) - 4)
			break;
		flags[idx++] = c;
		if (isupper(c))
			flags[idx++] = c;
		flags[idx++] = ' ';
	}
	flags[idx] = '\0';

	macdefine(&e->e_mci->mci_macro, A_TEMP, macid("{client_flags}"), flags);
	if (tTd(10, 30))
		sm_dprintf("cltfeatures: server=%s, mci=%p, flags=%s, {client_flags}=%s\n",
			servername, e->e_mci, flags,
			macvalue(macid("{client_flags}"), e));
	return EX_OK;
}
#endif /* STARTTLS || SASL */

#if _FFR_LOG_FAILOVER
/*
**  LOGFAILOVER -- log reason why trying another host
**
**	Parameters:
**		e -- envelope
**		m -- the mailer info for this mailer
**		mci -- mailer connection information
**		rcode -- the code signifying the particular failure
**		rcpt -- current RCPT
**
**	Returns:
**		none.
*/

static void logfailover __P((ENVELOPE *, MAILER *, MCI *, int, ADDRESS *));
static void
logfailover(e, m, mci, rcode, rcpt)
	ENVELOPE *e;
	MAILER *m;
	MCI *mci;
	int rcode;
	ADDRESS *rcpt;
{
	char buf[MAXNAME];
	char cbuf[SM_MAX(SYSLOG_BUFSIZE, MAXNAME)];

	buf[0] = '\0';
	cbuf[0] = '\0';
	sm_strlcat(cbuf, "deliver: ", sizeof(cbuf));
	if (m != NULL && m->m_name != NULL)
	{
		sm_snprintf(buf, sizeof(buf),
			"mailer=%s, ", m->m_name);
		sm_strlcat(cbuf, buf, sizeof(cbuf));
	}
	if (mci != NULL && mci->mci_host != NULL)
	{
		extern SOCKADDR CurHostAddr;

		sm_snprintf(buf, sizeof(buf),
			"relay=%.100s", mci->mci_host);
		sm_strlcat(cbuf, buf, sizeof(cbuf));
		if (CurHostAddr.sa.sa_family != 0)
		{
			sm_snprintf(buf, sizeof(buf),
				" [%.100s]",
				anynet_ntoa(&CurHostAddr));
			sm_strlcat(cbuf, buf, sizeof(cbuf));
		}
		sm_strlcat(cbuf, ", ", sizeof(cbuf));
	}
	if (mci != NULL)
	{
		if (mci->mci_state >= 0 && mci->mci_state < SM_ARRAY_SIZE(mcis))
			sm_snprintf(buf, sizeof(buf),
				"state=%s, ", mcis[mci->mci_state]);
		else
			sm_snprintf(buf, sizeof(buf),
				"state=%d, ", mci->mci_state);
		sm_strlcat(cbuf, buf, sizeof(cbuf));
	}
	if (tTd(11, 64))
	{
		sm_snprintf(buf, sizeof(buf),
			"rcode=%d, okrcpts=%d, retryrcpt=%d, e_rcode=%d, ",
			rcode, mci->mci_okrcpts, mci->mci_retryrcpt,
			e->e_rcode);
		sm_strlcat(cbuf, buf, sizeof(cbuf));
	}
	if (rcode != EX_OK && rcpt != NULL
	    && !SM_IS_EMPTY(rcpt->q_rstatus)
	    && !bitset(QINTREPLY, rcpt->q_flags))
	{
		sm_snprintf(buf, sizeof(buf),
			"q_rstatus=%s, ", rcpt->q_rstatus);
		sm_strlcat(cbuf, buf, sizeof(cbuf));
	}
	else if (e->e_text != NULL)
	{
		sm_snprintf(buf, sizeof(buf),
			"reply=%d %s%s%s, ",
			e->e_rcode,
			e->e_renhsc,
			(e->e_renhsc[0] != '\0') ? " " : "",
			e->e_text);
		sm_strlcat(cbuf, buf, sizeof(cbuf));
	}
	sm_strlcat(cbuf,
		"stat=tempfail: trying next host",
		sizeof(cbuf));
	sm_syslog(LOG_INFO, e->e_id, "%s", cbuf);
}
#else /* _FFR_LOG_FAILOVER */
# define logfailover(e, m, mci, rcode, rcpt)	((void) 0)
#endif /* _FFR_LOG_FAILOVER */

#if STARTTLS || SASL
# define RM_TRAIL_DOT(name)				\
	do {						\
		dotpos = strlen(name) - 1;		\
		if (dotpos >= 0)			\
		{					\
			if (name[dotpos] == '.')	\
				name[dotpos] = '\0';	\
			else				\
				dotpos = -1;		\
		}					\
	} while (0)

# define FIX_TRAIL_DOT(name)				\
	do {						\
		if (dotpos >= 0)			\
			name[dotpos] = '.';		\
	} while (0)


/*
**  SETSERVERMACROS -- set ${server_addr} and ${server_name}
**
**	Parameters:
**		mci -- mailer connection information
**		pdotpos -- return pointer to former dot position in hostname
**
**	Returns:
**		server name
*/

static char *setservermacros __P((MCI *, int *));

static char *
setservermacros(mci, pdotpos)
	MCI *mci;
	int *pdotpos;
{
	char *srvname;
	int dotpos;
	extern SOCKADDR CurHostAddr;

	/* don't use CurHostName, it is changed in many places */
	if (mci->mci_host != NULL)
	{
		srvname = mci->mci_host;
		RM_TRAIL_DOT(srvname);
	}
	else if (mci->mci_mailer != NULL)
	{
		srvname = mci->mci_mailer->m_name;
		dotpos = -1;
	}
	else
	{
		srvname = "local";
		dotpos = -1;
	}

	/* don't set {server_name} to NULL or "": see getauth() */
	macdefine(&mci->mci_macro, A_TEMP, macid("{server_name}"),
		  srvname);

	/* CurHostAddr is set by makeconnection() and mci_get() */
	if (CurHostAddr.sa.sa_family != 0)
	{
		macdefine(&mci->mci_macro, A_TEMP,
			  macid("{server_addr}"),
			  anynet_ntoa(&CurHostAddr));
	}
	else if (mci->mci_mailer != NULL)
	{
		/* mailer name is unique, use it as address */
		macdefine(&mci->mci_macro, A_PERM,
			  macid("{server_addr}"),
			  mci->mci_mailer->m_name);
	}
	else
	{
		/* don't set it to NULL or "": see getauth() */
		macdefine(&mci->mci_macro, A_PERM,
			  macid("{server_addr}"), "0");
	}

	if (pdotpos != NULL)
		*pdotpos = dotpos;
	else
		FIX_TRAIL_DOT(srvname);
	return srvname;
}
#endif /* STARTTLS || SASL */

/*
**  DELIVER -- Deliver a message to a list of addresses.
**
**	This routine delivers to everyone on the same host as the
**	user on the head of the list.  It is clever about mailers
**	that don't handle multiple users.  It is NOT guaranteed
**	that it will deliver to all these addresses however -- so
**	deliver should be called once for each address on the list.
**	Deliver tries to be as opportunistic as possible about piggybacking
**	messages. Some definitions to make understanding easier follow below.
**	Piggybacking occurs when an existing connection to a mail host can
**	be used to send the same message to more than one recipient at the
**	same time. So "no piggybacking" means one message for one recipient
**	per connection. "Intentional piggybacking" happens when the
**	recipients' host address (not the mail host address) is used to
**	attempt piggybacking. Recipients with the same host address
**	have the same mail host. "Coincidental piggybacking" relies on
**	piggybacking based on all the mail host addresses in the MX-RR. This
**	is "coincidental" in the fact it could not be predicted until the
**	MX Resource Records for the hosts were obtained and examined. For
**	example (preference order and equivalence is important, not values):
**		domain1 IN MX 10 mxhost-A
**			IN MX 20 mxhost-B
**		domain2 IN MX  4 mxhost-A
**			IN MX  8 mxhost-B
**	Domain1 and domain2 can piggyback the same message to mxhost-A or
**	mxhost-B (if mxhost-A cannot be reached).
**	"Coattail piggybacking" relaxes the strictness of "coincidental
**	piggybacking" in the hope that most significant (lowest value)
**	MX preference host(s) can create more piggybacking. For example
**	(again, preference order and equivalence is important, not values):
**		domain3 IN MX 100 mxhost-C
**			IN MX 100 mxhost-D
**			IN MX 200 mxhost-E
**		domain4 IN MX  50 mxhost-C
**			IN MX  50 mxhost-D
**			IN MX  80 mxhost-F
**	A message for domain3 and domain4 can piggyback to mxhost-C if mxhost-C
**	is available. Same with mxhost-D because in both RR's the preference
**	value is the same as mxhost-C, respectively.
**	So deliver attempts coattail piggybacking when possible. If the
**	first MX preference level hosts cannot be used then the piggybacking
**	reverts to coincidental piggybacking. Using the above example you
**	cannot deliver to mxhost-F for domain3 regardless of preference value.
**	("Coattail" from "riding on the coattails of your predecessor" meaning
**	gaining benefit from a predecessor effort with no or little addition
**	effort. The predecessor here being the preceding MX RR).
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

#if !_FFR_DMTRIGGER
static
#endif
int
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
	ADDRESS *volatile ctladdr;
#if HASSETUSERCONTEXT
	ADDRESS *volatile contextaddr = NULL;
#endif
	register MCI *volatile mci;
	register ADDRESS *SM_NONVOLATILE to = firstto;
	volatile bool clever = false;	/* running user smtp to this mailer */
	ADDRESS *volatile tochain = NULL; /* users chain in this mailer call */
	int rcode;			/* response code */
	SM_NONVOLATILE int lmtp_rcode = EX_OK;
	SM_NONVOLATILE int nummxhosts = 0; /* number of MX hosts available */
	SM_NONVOLATILE int hostnum = 0;	/* current MX host index */
	char *firstsig;			/* signature of firstto */
	volatile pid_t pid = -1;
	char *volatile curhost;
	SM_NONVOLATILE unsigned short port = 0;
	SM_NONVOLATILE time_t enough = 0;
#if NETUNIX
	char *SM_NONVOLATILE mux_path = NULL;	/* path to UNIX domain socket */
#endif
	time_t xstart;
	bool suidwarn;
	bool anyok;			/* at least one address was OK */
	SM_NONVOLATILE bool goodmxfound = false; /* at least one MX was OK */
	bool ovr;
	bool quarantine;
#if STARTTLS
	bool implicittls = false;
# if _FFR_SMTPS_CLIENT
	bool smtptls = false;
# endif
	/* 0: try TLS, 1: try without TLS again, >1: don't try again */
	int tlsstate;
# if DANE
	dane_vrfy_ctx_T	dane_vrfy_ctx;
	STAB *ste;
	char *vrfy;
	int dane_req;

/* should this allow DANE_ALWAYS == DANEMODE(dane)? */
#  define RCPT_MXSECURE(rcpt)	(0 != ((rcpt)->q_flags & QMXSECURE))

#  define STE_HAS_TLSA(ste) ((ste) != NULL && (ste)->s_tlsa != NULL)

/* NOTE: the following macros use some local variables directly! */
#  define RCPT_HAS_DANE(rcpt)	(RCPT_MXSECURE(rcpt) \
	&& !iscltflgset(e, D_NODANE)	\
	&& STE_HAS_TLSA(ste)	\
	&& (0 == (dane_vrfy_ctx.dane_vrfy_chk & TLSAFLTEMPVRFY))	\
	&& (0 != (dane_vrfy_ctx.dane_vrfy_chk & TLSAFLADIP))	\
	&& CHK_DANE(dane_vrfy_ctx.dane_vrfy_chk)	\
	)

#  define RCPT_REQ_DANE(rcpt)	(RCPT_HAS_DANE(rcpt) \
	&& TLSA_IS_FL(ste->s_tlsa, TLSAFLSUP))

#  define RCPT_REQ_TLS(rcpt)	(RCPT_HAS_DANE(rcpt) \
	&& TLSA_IS_FL(ste->s_tlsa, TLSAFLUNS))

#  define CHK_DANE_RCPT(dane, rcpt) (CHK_DANE(dane) && \
	 (RCPT_MXSECURE(rcpt) || DANE_ALWAYS == DANEMODE(dane)))

	BITMAP256 mxads;
# endif /* DANE */
#endif /* STARTTLS */
	int strsize;
	int rcptcount;
	int ret;
	static int tobufsize = 0;
	static char *tobuf = NULL;
	char *rpath;	/* translated return path */
	int mpvect[2];
	int rpvect[2];
	char *mxhosts[MAXMXHOSTS + 1];
	char *pv[MAXPV + 1];
	char buf[MAXNAME + 1];	/* EAI:ok */
	char cbuf[MAXPATHLEN];
#if _FFR_8BITENVADDR
	char xbuf[SM_MAX(SYSLOG_BUFSIZE, MAXNAME)];
#endif

	errno = 0;
	SM_REQUIRE(firstto != NULL);	/* same as to */
	if (!QS_IS_OK(to->q_state))
		return 0;

	suidwarn = geteuid() == 0;

	SM_REQUIRE(e != NULL);
	m = to->q_mailer;
	host = to->q_host;
	CurEnv = e;			/* just in case */
	e->e_statmsg = NULL;
	SmtpError[0] = '\0';
	xstart = curtime();
#if STARTTLS
	tlsstate = 0;
# if DANE
	memset(&dane_vrfy_ctx, '\0', sizeof(dane_vrfy_ctx));
	ste = NULL;
# endif
#endif

	if (tTd(10, 1))
		sm_dprintf("\n--deliver, id=%s, mailer=%s, host=`%s', first user=`%s'\n",
			e->e_id, m->m_name, host, to->q_user);
	if (tTd(10, 100))
		printopenfds(false);
	maps_reset_chged("deliver");

	/*
	**  Clear {client_*} macros if this is a bounce message to
	**  prevent rejection by check_compat ruleset.
	*/

	if (bitset(EF_RESPONSE, e->e_flags))
	{
		macdefine(&e->e_macro, A_PERM, macid("{client_name}"), "");
		macdefine(&e->e_macro, A_PERM, macid("{client_ptr}"), "");
		macdefine(&e->e_macro, A_PERM, macid("{client_addr}"), "");
		macdefine(&e->e_macro, A_PERM, macid("{client_port}"), "");
		macdefine(&e->e_macro, A_PERM, macid("{client_resolve}"), "");
	}

	SM_TRY
	{
	ADDRESS *skip_back = NULL;

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
	SM_ASSERT(e->e_from.q_mailer != NULL);
	if (bitnset(M_UDBENVELOPE, e->e_from.q_mailer->m_flags))
		p = e->e_sender;
	else
		p = e->e_from.q_paddr;
	rpath = remotename(p, m, RF_SENDERADDR|RF_CANONICAL, &rcode, e);
	if (rcode != EX_OK && bitnset(M_xSMTP, m->m_flags))
		goto cleanup;

	/* need to check external format, not internal! */
	if (strlen(rpath) > MAXNAME_I)
	{
		rpath = shortenstring(rpath, MAXSHORTSTR);

		/* avoid bogus errno */
		errno = 0;
		syserr("remotename: huge return path %s", rpath);
	}
	rpath = sm_rpool_strdup_x(e->e_rpool, rpath);
	macdefine(&e->e_macro, A_PERM, 'g', rpath);
#if _FFR_8BITENVADDR
	host = quote_internal_chars(host, NULL, &strsize, NULL);
#endif
	macdefine(&e->e_macro, A_PERM, 'h', host);
	Errors = 0;
	pvp = pv;
	*pvp++ = m->m_argv[0];

	/* ignore long term host status information if mailer flag W is set */
	if (bitnset(M_NOHOSTSTAT, m->m_flags))
		IgnoreHostStatus = true;

	/* insert -f or -r flag as appropriate */
	if (FromFlag &&
	    (bitnset(M_FOPT, m->m_flags) ||
	     bitnset(M_ROPT, m->m_flags)))
	{
		if (bitnset(M_FOPT, m->m_flags))
			*pvp++ = "-f";
		else
			*pvp++ = "-r";
		*pvp++ = rpath;
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
		expand(*mvp, buf, sizeof(buf), e);
		p = buf;
#if _FFR_8BITENVADDR
		/* apply to all args? */
		if (strcmp(m->m_mailer, "[IPC]") == 0
		    && ((*mvp)[0] & 0377) == MACROEXPAND
/* for now only apply [i] -> [x] conversion to $h by default */
# ifndef _FFR_H2X_ONLY
#  define _FFR_H2X_ONLY 1
# endif
# if _FFR_H2X_ONLY
		    && 'h' == (*mvp)[1] && '\0' == (*mvp)[2]
# endif
		   )
		{
			(void) dequote_internal_chars(buf, xbuf, sizeof(xbuf));
			p = xbuf;
			if (tTd(10, 33))
				sm_dprintf("expand(%s), dequoted=%s\n", *mvp, p);
		}
#endif /* _FFR_8BITENVADDR */
		*pvp++ = sm_rpool_strdup_x(e->e_rpool, p);
		if (pvp >= &pv[MAXPV - 3])
		{
			syserr("554 5.3.5 Too many parameters to %s before $u",
			       pv[0]);
			rcode = -1;
			goto cleanup;
		}
	}

	/*
	**  If we have no substitution for the user name in the argument
	**  list, we know that we must supply the names otherwise -- and
	**  SMTP is the answer!!
	*/

	if (*mvp == NULL)
	{
		/* running LMTP or SMTP */
		clever = true;
		*pvp = NULL;
		setbitn(M_xSMTP, m->m_flags);
	}
	else if (bitnset(M_LMTP, m->m_flags))
	{
		/* not running LMTP */
		sm_syslog(LOG_ERR, NULL,
			  "Warning: mailer %s: LMTP flag (F=z) turned off",
			  m->m_name);
		clrbitn(M_LMTP, m->m_flags);
	}

	/*
	**  At this point *mvp points to the argument with $u.  We
	**  run through our address list and append all the addresses
	**  we can.  If we run out of space, do not fret!  We can
	**  always send another copy later.
	*/

	e->e_to = NULL;
	strsize = 2;
	rcptcount = 0;
	ctladdr = NULL;
	if (firstto->q_signature == NULL)
		firstto->q_signature = hostsignature(firstto->q_mailer,
						     firstto->q_host,
						     QISSECURE(firstto),
						     &firstto->q_flags);
	firstsig = firstto->q_signature;
#if DANE
# define NODANEREQYET	(-1)
	dane_req = NODANEREQYET;
#endif

	for (; to != NULL; to = to->q_next)
	{
		/* avoid sending multiple recipients to dumb mailers */
		if (tochain != NULL && !bitnset(M_MUSER, m->m_flags))
			break;

		/* if already sent or not for this host, don't send */
		if (!QS_IS_OK(to->q_state)) /* already sent; look at next */
			continue;

		/*
		**  Must be same mailer to keep grouping rcpts.
		**  If mailers don't match: continue; sendqueue is not
		**  sorted by mailers, so don't break;
		*/

		if (to->q_mailer != firstto->q_mailer)
			continue;

		if (to->q_signature == NULL) /* for safety */
			to->q_signature = hostsignature(to->q_mailer,
							to->q_host,
							QISSECURE(to),
							&to->q_flags);

		/*
		**  This is for coincidental and tailcoat piggybacking messages
		**  to the same mail host. While the signatures are identical
		**  (that's the MX-RR's are identical) we can do coincidental
		**  piggybacking. We try hard for coattail piggybacking
		**  with the same mail host when the next recipient has the
		**  same host at lowest preference. It may be that this
		**  won't work out, so 'skip_back' is maintained if a backup
		**  to coincidental piggybacking or full signature must happen.
		*/

		ret = firstto == to ? HS_MATCH_FULL :
				      coloncmp(to->q_signature, firstsig);
		if (ret == HS_MATCH_FULL)
			skip_back = to;
		else if (ret == HS_MATCH_NO)
			break;
# if HSMARKS
		else if (ret == HS_MATCH_SKIP)
			continue;
# endif

		if (!clever)
		{
			/* avoid overflowing tobuf */
			strsize += strlen(to->q_paddr) + 1;
			if (strsize > TOBUFSIZE)
				break;
		}

		if (++rcptcount > to->q_mailer->m_maxrcpt)
			break;

#if DANE
		if (TTD(10, 30))
		{
			char sep = ':';

			parse_hostsignature(to->q_signature, mxhosts, m, mxads);
			FIX_MXHOSTS(mxhosts[0], p, sep);
# if HSMARKS
			if (MXADS_ISSET(mxads, 0))
				to->q_flags |= QMXSECURE;
			else
				to->q_flags &= ~QMXSECURE;
# endif

			gettlsa(mxhosts[0], NULL, &ste, RCPT_MXSECURE(to) ? TLSAFLADMX : 0, 0, m->m_port);
			sm_dprintf("tochain: to=%s, rcptcount=%d, QSECURE=%d, QMXSECURE=%d, MXADS[0]=%d, ste=%p\n",
				to->q_user, rcptcount, QISSECURE(to),
				RCPT_MXSECURE(to), MXADS_ISSET(mxads, 0), ste);
			sm_dprintf("tochain: hostsig=%s, mx=%s, tlsa_n=%d, tlsa_flags=%#lx, chk_dane=%d, dane_req=%d\n"
				, to->q_signature, mxhosts[0]
				, STE_HAS_TLSA(ste) ? ste->s_tlsa->dane_tlsa_n : -1
				, STE_HAS_TLSA(ste) ? ste->s_tlsa->dane_tlsa_flags : -1
				, CHK_DANE_RCPT(Dane, to)
				, dane_req
				);
			if (p != NULL)
				*p = sep;
		}
		if (NODANEREQYET == dane_req)
			dane_req = CHK_DANE_RCPT(Dane, to);
		else if (dane_req != CHK_DANE_RCPT(Dane, to))
		{
			if (tTd(10, 30))
				sm_dprintf("tochain: to=%s, rcptcount=%d, status=skip\n",
					to->q_user, rcptcount);
			continue;
		}
#endif /* DANE */

		/*
		**  prepare envelope for new session to avoid leakage
		**  between delivery attempts.
		*/

		smtpclrse(e);

		if (tTd(10, 1))
		{
			sm_dprintf("\nsend to ");
			printaddr(sm_debug_file(), to, false);
		}

		/* compute effective uid/gid when sending */
		if (bitnset(M_RUNASRCPT, to->q_mailer->m_flags))
#if HASSETUSERCONTEXT
			contextaddr = ctladdr = getctladdr(to);
#else
			ctladdr = getctladdr(to);
#endif

		if (tTd(10, 2))
		{
			sm_dprintf("ctladdr=");
			printaddr(sm_debug_file(), ctladdr, false);
		}

		user = to->q_user;
		e->e_to = to->q_paddr;

		/*
		**  Check to see that these people are allowed to
		**  talk to each other.
		**  Check also for overflow of e_msgsize.
		*/

		if (m->m_maxsize != 0 &&
		    (e->e_msgsize > m->m_maxsize || e->e_msgsize < 0))
		{
			e->e_flags |= EF_NO_BODY_RETN;
			if (bitnset(M_LOCALMAILER, to->q_mailer->m_flags))
				to->q_status = "5.2.3";
			else
				to->q_status = "5.3.4";

			/* set to->q_rstatus = NULL; or to the following? */
			usrerrenh(to->q_status,
				  "552 Message is too large; %ld bytes max",
				  m->m_maxsize);
			markfailure(e, to, NULL, EX_UNAVAILABLE, false);
			giveresponse(EX_UNAVAILABLE, to->q_status, m,
				     NULL, ctladdr, xstart, e, to);
			continue;
		}
		SM_SET_H_ERRNO(0);
		ovr = true;

		/* do config file checking of compatibility */
		quarantine = (e->e_quarmsg != NULL);
		rcode = rscheck("check_compat", e->e_from.q_paddr, to->q_paddr,
				e, RSF_RMCOMM|RSF_COUNT, 3, NULL,
				e->e_id, NULL, NULL);
		if (rcode == EX_OK)
		{
			/* do in-code checking if not discarding */
			if (!bitset(EF_DISCARD, e->e_flags))
			{
				rcode = checkcompat(to, e);
				ovr = false;
			}
		}
		if (rcode != EX_OK)
		{
			markfailure(e, to, NULL, rcode, ovr);
			giveresponse(rcode, to->q_status, m,
				     NULL, ctladdr, xstart, e, to);
			continue;
		}
		if (!quarantine && e->e_quarmsg != NULL)
		{
			/*
			**  check_compat or checkcompat() has tried
			**  to quarantine but that isn't supported.
			**  Revert the attempt.
			*/

			e->e_quarmsg = NULL;
			macdefine(&e->e_macro, A_PERM,
				  macid("{quarantine}"), "");
		}
		if (bitset(EF_DISCARD, e->e_flags))
		{
			if (tTd(10, 5))
			{
				sm_dprintf("deliver: discarding recipient ");
				printaddr(sm_debug_file(), to, false);
			}

			/* pretend the message was sent */
			/* XXX should we log something here? */
			to->q_state = QS_DISCARDED;

			/*
			**  Remove discard bit to prevent discard of
			**  future recipients.  This is safe because the
			**  true "global discard" has been handled before
			**  we get here.
			*/

			e->e_flags &= ~EF_DISCARD;
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

		/*
		**  Strip all leading backslashes if requested and the
		**  next character is alphanumerical (the latter can
		**  probably relaxed a bit, see RFC2821).
		*/

		if (bitnset(M_STRIPBACKSL, m->m_flags) && user[0] == '\\')
			stripbackslash(user);

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

		if (!QS_IS_OK(to->q_state))
			continue;

		/*
		**  See if this user name is "special".
		**	If the user name has a slash in it, assume that this
		**	is a file -- send it off without further ado.  Note
		**	that this type of addresses is not processed along
		**	with the others, so we fudge on the To person.
		*/

		if (strcmp(m->m_mailer, "[FILE]") == 0)
		{
			macdefine(&e->e_macro, A_PERM, 'u', user);
			p = to->q_home;
			if (p == NULL && ctladdr != NULL)
				p = ctladdr->q_home;
			macdefine(&e->e_macro, A_PERM, 'z', p);
			expand(m->m_argv[1], buf, sizeof(buf), e);
			if (strlen(buf) > 0)
				rcode = mailfile(buf, m, ctladdr, SFF_CREAT, e);
			else
			{
				syserr("empty filename specification for mailer %s",
				       m->m_name);
				rcode = EX_CONFIG;
			}
			giveresponse(rcode, to->q_status, m, NULL,
				     ctladdr, xstart, e, to);
			markfailure(e, to, NULL, rcode, true);
			e->e_nsent++;
			if (rcode == EX_OK)
			{
				to->q_state = QS_SENT;
				if (bitnset(M_LOCALMAILER, m->m_flags) &&
				    bitset(QPINGONSUCCESS, to->q_flags))
				{
					to->q_flags |= QDELIVERED;
					to->q_status = "2.1.5";
					(void) sm_io_fprintf(e->e_xfp,
							     SM_TIME_DEFAULT,
							     "%s... Successfully delivered\n",
							     to->q_paddr);
				}
			}
			to->q_statdate = curtime();
			markstats(e, to, STATS_NORMAL);
			continue;
		}

		/*
		**  Address is verified -- add this user to mailer
		**  argv, and add it to the print list of recipients.
		*/

		/* link together the chain of recipients */
		to->q_tchain = tochain;
		tochain = to;
		e->e_to = "[CHAIN]";

		macdefine(&e->e_macro, A_PERM, 'u', user);  /* to user */
		p = to->q_home;
		if (p == NULL && ctladdr != NULL)
			p = ctladdr->q_home;
		macdefine(&e->e_macro, A_PERM, 'z', p);  /* user's home */

		/* set the ${dsn_notify} macro if applicable */
		if (bitset(QHASNOTIFY, to->q_flags))
		{
			char notify[MAXLINE];

			notify[0] = '\0';
			if (bitset(QPINGONSUCCESS, to->q_flags))
				(void) sm_strlcat(notify, "SUCCESS,",
						  sizeof(notify));
			if (bitset(QPINGONFAILURE, to->q_flags))
				(void) sm_strlcat(notify, "FAILURE,",
						  sizeof(notify));
			if (bitset(QPINGONDELAY, to->q_flags))
				(void) sm_strlcat(notify, "DELAY,",
						  sizeof(notify));

			/* Set to NEVER or drop trailing comma */
			if (notify[0] == '\0')
				(void) sm_strlcat(notify, "NEVER",
						  sizeof(notify));
			else
				notify[strlen(notify) - 1] = '\0';

			macdefine(&e->e_macro, A_TEMP,
				macid("{dsn_notify}"), notify);
		}
		else
			macdefine(&e->e_macro, A_PERM,
				macid("{dsn_notify}"), NULL);

		/*
		**  Expand out this user into argument list.
		*/

		if (!clever)
		{
			expand(*mvp, buf, sizeof(buf), e);
			p = buf;
#if _FFR_8BITENVADDR
			if (((*mvp)[0] & 0377) == MACROEXPAND)
			{
				(void) dequote_internal_chars(buf, xbuf, sizeof(xbuf));
				p = xbuf;
				if (tTd(10, 33))
					sm_dprintf("expand(%s), dequoted=%s\n", *mvp, p);
			}
#endif
			*pvp++ = sm_rpool_strdup_x(e->e_rpool, p);
			if (pvp >= &pv[MAXPV - 2])
			{
				/* allow some space for trailing parms */
				break;
			}
		}
	}

	/* see if any addresses still exist */
	if (tochain == NULL)
	{
		rcode = 0;
		goto cleanup;
	}

	/* print out messages as full list */
	strsize = 1;
	for (to = tochain; to != NULL; to = to->q_tchain)
		strsize += strlen(to->q_paddr) + 1;
	if (strsize < TOBUFSIZE)
		strsize = TOBUFSIZE;
	if (strsize > tobufsize)
	{
		SM_FREE(tobuf);
		tobuf = sm_pmalloc_x(strsize);
		tobufsize = strsize;
	}
	p = tobuf;
	*p = '\0';
	for (to = tochain; to != NULL; to = to->q_tchain)
	{
		(void) sm_strlcpyn(p, tobufsize - (p - tobuf), 2,
				   ",", to->q_paddr);
		p += strlen(p);
	}
	e->e_to = tobuf + 1;

	/*
	**  Fill out any parameters after the $u parameter.
	*/

	if (!clever)
	{
		while (*++mvp != NULL)
		{
			expand(*mvp, buf, sizeof(buf), e);
			p = buf;
#if _FFR_8BITENVADDR && 0
			/* disabled for now - is there a use case for this? */
			if (((*mvp)[0] & 0377) == MACROEXPAND)
			{
				(void) dequote_internal_chars(buf, xbuf, sizeof(xbuf));
				p = xbuf;
				if (tTd(10, 33))
					sm_dprintf("expand(%s), dequoted=%s\n", *mvp, p);
			}
#endif
			*pvp++ = sm_rpool_strdup_x(e->e_rpool, p);
			if (pvp >= &pv[MAXPV])
				syserr("554 5.3.0 deliver: pv overflow after $u for %s",
				       pv[0]);
		}
	}
	*pvp++ = NULL;

	/*
	**  Call the mailer.
	**	The argument vector gets built, pipes
	**	are created as necessary, and we fork & exec as
	**	appropriate.
	**	If we are running SMTP, we just need to clean up.
	*/

	/* XXX this seems a bit weird */
	if (ctladdr == NULL && m != ProgMailer && m != FileMailer &&
	    bitset(QGOODUID, e->e_from.q_flags))
		ctladdr = &e->e_from;

#if NAMED_BIND
	if (ConfigLevel < 2)
		_res.options &= ~(RES_DEFNAMES | RES_DNSRCH);	/* XXX */
#endif

	if (tTd(11, 1))
	{
		sm_dprintf("openmailer:");
		printav(sm_debug_file(), pv);
	}
	errno = 0;
	SM_SET_H_ERRNO(0);
	CurHostName = NULL;

	/*
	**  Deal with the special case of mail handled through an IPC
	**  connection.
	**	In this case we don't actually fork.  We must be
	**	running SMTP for this to work.  We will return a
	**	zero pid to indicate that we are running IPC.
	**  We also handle a debug version that just talks to stdin/out.
	*/

	curhost = NULL;
	SmtpPhase = NULL;
	mci = NULL;

#if XDEBUG
	{
		char wbuf[MAXLINE];

		/* make absolutely certain 0, 1, and 2 are in use */
		(void) sm_snprintf(wbuf, sizeof(wbuf), "%s... openmailer(%s)",
				   shortenstring(e->e_to, MAXSHORTSTR),
				   m->m_name);
		checkfd012(wbuf);
	}
#endif /* XDEBUG */

	/* check for 8-bit available */
	if (bitset(EF_HAS8BIT, e->e_flags) &&
	    bitnset(M_7BITS, m->m_flags) &&
	    (bitset(EF_DONT_MIME, e->e_flags) ||
	     !(bitset(MM_MIME8BIT, MimeMode) ||
	       (bitset(EF_IS_MIME, e->e_flags) &&
		bitset(MM_CVTMIME, MimeMode)))))
	{
		e->e_status = "5.6.3";
		usrerrenh(e->e_status,
			  "554 Cannot send 8-bit data to 7-bit destination");
		rcode = EX_DATAERR;
		goto give_up;
	}

	if (tTd(62, 8))
		checkfds("before delivery");

	/* check for Local Person Communication -- not for mortals!!! */
	if (strcmp(m->m_mailer, "[LPC]") == 0)
	{
		if (clever)
		{
			/* flush any expired connections */
			(void) mci_scan(NULL);

			/* try to get a cached connection or just a slot */
			mci = mci_get(m->m_name, m);
			if (mci->mci_host == NULL)
				mci->mci_host = m->m_name;
			CurHostName = mci->mci_host;
			if (mci->mci_state != MCIS_CLOSED)
			{
				message("Using cached SMTP/LPC connection for %s...",
					m->m_name);
				mci->mci_deliveries++;
				goto do_transfer;
			}
		}
		else
		{
			mci = mci_new(e->e_rpool);
		}
		mci->mci_in = smioin;
		mci->mci_out = smioout;
		mci->mci_mailer = m;
		mci->mci_host = m->m_name;
		if (clever)
		{
			mci->mci_state = MCIS_OPENING;
			mci_cache(mci);
		}
		else
			mci->mci_state = MCIS_OPEN;
	}
	else if (strcmp(m->m_mailer, "[IPC]") == 0)
	{
		register int i;

		if (pv[0] == NULL || pv[1] == NULL || pv[1][0] == '\0')
		{
			syserr("null destination for %s mailer", m->m_mailer);
			rcode = EX_CONFIG;
			goto give_up;
		}

#if NETUNIX
		if (strcmp(pv[0], "FILE") == 0)
		{
			curhost = CurHostName = "localhost";
			mux_path = pv[1];
		}
		else
#endif /* NETUNIX */
		/* "else" in #if code above */
		{
			CurHostName = pv[1];
							/* XXX ??? */
			curhost = hostsignature(m, pv[1],
					QISSECURE(firstto),
					&firstto->q_flags);
		}

		if (curhost == NULL || curhost[0] == '\0')
		{
			syserr("null host signature for %s", pv[1]);
			rcode = EX_CONFIG;
			goto give_up;
		}

		if (!clever)
		{
			syserr("554 5.3.5 non-clever IPC");
			rcode = EX_CONFIG;
			goto give_up;
		}
		if (pv[2] != NULL
#if NETUNIX
		    && mux_path == NULL
#endif
		    )
		{
			port = htons((unsigned short) atoi(pv[2]));
			if (port == 0)
			{
#ifdef NO_GETSERVBYNAME
				syserr("Invalid port number: %s", pv[2]);
#else /* NO_GETSERVBYNAME */
				struct servent *sp = getservbyname(pv[2], "tcp");

				if (sp == NULL)
					syserr("Service %s unknown", pv[2]);
				else
					port = sp->s_port;
#endif /* NO_GETSERVBYNAME */
			}
		}

		nummxhosts = parse_hostsignature(curhost, mxhosts, m
#if DANE
				, mxads
#endif
				);
		if (TimeOuts.to_aconnect > 0)
			enough = curtime() + TimeOuts.to_aconnect;
tryhost:
		while (hostnum < nummxhosts)
		{
			char sep = ':';
			char *endp;
			static char hostbuf[MAXNAME_I + 1];
			bool tried_fallbacksmarthost = false;
#if DANE
			unsigned long tlsa_flags;
# if HSMARKS
			bool mxsecure;
# endif

			ste = NULL;
			tlsa_flags = 0;
# if HSMARKS
			mxsecure = MXADS_ISSET(mxads, hostnum);
# endif
#endif /* DANE */
			FIX_MXHOSTS(mxhosts[hostnum], endp, sep);

			if (hostnum == 1 && skip_back != NULL)
			{
				/*
				**  Coattail piggybacking is no longer an
				**  option with the mail host next to be tried
				**  no longer the lowest MX preference
				**  (hostnum == 1 meaning we're on the second
				**  preference). We do not try to coattail
				**  piggyback more than the first MX preference.
				**  Revert 'tochain' to last location for
				**  coincidental piggybacking. This works this
				**  easily because the q_tchain kept getting
				**  added to the top of the linked list.
				*/

				tochain = skip_back;
			}

			if (*mxhosts[hostnum] == '\0')
			{
				syserr("deliver: null host name in signature");
				hostnum++;
				if (endp != NULL)
					*endp = sep;
				continue;
			}
			(void) sm_strlcpy(hostbuf, mxhosts[hostnum],
					  sizeof(hostbuf));
			hostnum++;
			if (endp != NULL)
				*endp = sep;
#if STARTTLS
			tlsstate = 0;
#endif

  one_last_try:
			/* see if we already know that this host is fried */
			CurHostName = hostbuf;
			mci = mci_get(hostbuf, m);

#if DANE
			tlsa_flags = 0;
# if HSMARKS
			if (mxsecure)
				firstto->q_flags |= QMXSECURE;
			else
				firstto->q_flags &= ~QMXSECURE;
# endif
			if (TTD(90, 30))
				sm_dprintf("deliver: mci_get: 1: mci=%p, host=%s, mx=%s, ste=%p, dane=%#x, mci_state=%d, QMXSECURE=%d, reqdane=%d, chk_dane_rcpt=%d, firstto=%s, to=%s\n",
					mci, host, hostbuf, ste, Dane,
					mci->mci_state, RCPT_MXSECURE(firstto),
					RCPT_REQ_DANE(firstto),
					CHK_DANE_RCPT(Dane, firstto),
					firstto->q_user, e->e_to);

			if (CHK_DANE_RCPT(Dane, firstto))
			{
				(void) gettlsa(hostbuf, NULL, &ste,
					RCPT_MXSECURE(firstto) ? TLSAFLADMX : 0,
					0, m->m_port);
			}
			if (TTD(90, 30))
				sm_dprintf("deliver: host=%s, mx=%s, ste=%p, chk_dane=%d\n",
					host, hostbuf, ste,
					CHK_DANE_RCPT(Dane, firstto));

			/* XXX: check expiration! */
			if (ste != NULL && TLSA_RR_TEMPFAIL(ste->s_tlsa))
			{
				if (tTd(11, 1))
					sm_dprintf("skip: host=%s, TLSA_RR_lookup=%d\n"
						, hostbuf
						, ste->s_tlsa->dane_tlsa_dnsrc);

				tlsa_flags |= TLSAFLTEMP;
			}
			else if (ste != NULL && TTD(90, 30))
			{
				if (ste->s_tlsa != NULL)
					sm_dprintf("deliver: host=%s, mx=%s, tlsa_n=%d, tlsa_flags=%#lx, ssl=%p, chk=%#x, res=%d\n"
					, host, hostbuf
					, ste->s_tlsa->dane_tlsa_n
					, ste->s_tlsa->dane_tlsa_flags
					, mci->mci_ssl
					, mci->mci_tlsi.tlsi_dvc.dane_vrfy_chk
					, mci->mci_tlsi.tlsi_dvc.dane_vrfy_res
					);
				else
					sm_dprintf("deliver: host=%s, mx=%s, notlsa\n", host, hostbuf);
			}

			if (mci->mci_state != MCIS_CLOSED)
			{
				bool dane_old, dane_new, new_session;

				/* CHK_DANE(Dane): implicit via ste != NULL */
				dane_new = !iscltflgset(e, D_NODANE) &&
					ste != NULL && ste->s_tlsa != NULL &&
					TLSA_IS_FL(ste->s_tlsa, TLSAFLSUP);
				dane_old = CHK_DANE(mci->mci_tlsi.tlsi_dvc.dane_vrfy_chk);
				new_session = (dane_old != dane_new);
				vrfy = "";
				if (dane_old && new_session)
				{
					vrfy = macget(&mci->mci_macro, macid("{verify}"));
					new_session = NULL == vrfy || strcmp("TRUSTED", vrfy) != 0;
				}
				if (TTD(11, 32))
					sm_dprintf("deliver: host=%s, mx=%s, dane_old=%d, dane_new=%d, new_session=%d, vrfy=%s\n",
						host, hostbuf, dane_old,
						dane_new, new_session, vrfy);
				if (new_session)
				{
					if (TTD(11, 34))
						sm_dprintf("deliver: host=%s, mx=%s, old_mci=%p, state=%d\n",
							host, hostbuf,
							mci, mci->mci_state);
					smtpquit(mci->mci_mailer, mci, e);
					if (TTD(11, 34))
						sm_dprintf("deliver: host=%s, mx=%s, new_mci=%p, state=%d\n",
							host, hostbuf,
							mci, mci->mci_state);
				}
				else
				{
					/* are tlsa_flags the same as dane_vrfy_chk? */
					tlsa_flags = mci->mci_tlsi.tlsi_dvc.dane_vrfy_chk;
					memcpy(&dane_vrfy_ctx,
						&mci->mci_tlsi.tlsi_dvc.dane_vrfy_chk,
						sizeof(dane_vrfy_ctx));
					dane_vrfy_ctx.dane_vrfy_host = NULL;
					dane_vrfy_ctx.dane_vrfy_sni = NULL;
					if (TTD(90, 40))
						sm_dprintf("deliver: host=%s, mx=%s, state=reuse, chk=%#x\n",
							host, hostbuf, mci->mci_tlsi.tlsi_dvc.dane_vrfy_chk);
				}
			}
#endif /* DANE */
			if (mci->mci_state != MCIS_CLOSED)
			{
				char *type;

				if (tTd(11, 1))
				{
					sm_dprintf("openmailer: ");
					mci_dump(sm_debug_file(), mci, false);
				}
				CurHostName = mci->mci_host;
				if (bitnset(M_LMTP, m->m_flags))
					type = "L";
				else if (bitset(MCIF_ESMTP, mci->mci_flags))
					type = "ES";
				else
					type = "S";
				message("Using cached %sMTP connection to %s via %s...",
					type, hostbuf, m->m_name);
				mci->mci_deliveries++;
				break;
			}
			mci->mci_mailer = m;

			if (mci->mci_exitstat != EX_OK)
			{
				if (mci->mci_exitstat == EX_TEMPFAIL)
					goodmxfound = true;

				/* Try FallbackSmartHost? */
				if (should_try_fbsh(e, &tried_fallbacksmarthost,
						    hostbuf, sizeof(hostbuf),
						    mci->mci_exitstat))
					goto one_last_try;

				continue;
			}

			if (mci_lock_host(mci) != EX_OK)
			{
				mci_setstat(mci, EX_TEMPFAIL, "4.4.5", NULL);
				goodmxfound = true;
				continue;
			}

			/* try the connection */
			sm_setproctitle(true, e, "%s %s: %s",
					qid_printname(e),
					hostbuf, "user open");

			i = EX_OK;
			e->e_mci = mci;
#if STARTTLS || SASL
			if ((i = cltfeatures(e, hostbuf)) != EX_OK)
			{
				if (LogLevel > 8)
					sm_syslog(LOG_WARNING, e->e_id,
					  "clt_features=TEMPFAIL, host=%s, status=skipped"
					  , hostbuf);
				/* XXX handle error! */
				(void) sm_strlcpy(SmtpError,
					"clt_features=TEMPFAIL",
					sizeof(SmtpError));
# if DANE
				tlsa_flags &= ~TLSAFLTEMP;
# endif
			}
# if DANE
			/* hack: disable DANE if requested */
			if (iscltflgset(e, D_NODANE))
				ste = NULL;
			tlsa_flags |= ste != NULL ? Dane : DANE_NEVER;
			dane_vrfy_ctx.dane_vrfy_chk = tlsa_flags;
			dane_vrfy_ctx.dane_vrfy_port = m->m_port;
			if (TTD(11, 11))
				sm_dprintf("deliver: makeconnection=before, chk=%#x, tlsa_flags=%#lx, {client_flags}=%s, stat=%d, dane_enabled=%d\n",
					dane_vrfy_ctx.dane_vrfy_chk,
					tlsa_flags,
					macvalue(macid("{client_flags}"), e),
					i, dane_vrfy_ctx.dane_vrfy_dane_enabled);
# endif /* DANE */
#endif /* STARTTLS || SASL */
#if NETUNIX
			if (mux_path != NULL)
			{
				message("Connecting to %s via %s...",
					mux_path, m->m_name);
				if (EX_OK == i)
				{
					i = makeconnection_ds((char *) mux_path, mci);
#if DANE
					/* fake it: "IP is secure" */
					tlsa_flags |= TLSAFLADIP;
#endif
				}
			}
			else
#endif /* NETUNIX */
			/* "else" in #if code above */
			{
				if (port == 0)
					message("Connecting to %s via %s...",
						hostbuf, m->m_name);
				else
					message("Connecting to %s port %d via %s...",
						hostbuf, ntohs(port),
						m->m_name);

				/*
				**  set the current connection information,
				**  required to set {client_flags} in e->e_mci
				*/

				if (EX_OK == i)
					i = makeconnection(hostbuf, port, mci,
						e, enough
#if DANE
						, &tlsa_flags
#endif
						);
			}
#if DANE
			if (TTD(11, 11))
				sm_dprintf("deliver: makeconnection=after, chk=%#x, tlsa_flags=%#lx, stat=%d\n",
					dane_vrfy_ctx.dane_vrfy_chk,
					tlsa_flags, i);
#if OLD_WAY_TLSA_FLAGS
			if (dane_vrfy_ctx.dane_vrfy_chk != DANE_ALWAYS)
				dane_vrfy_ctx.dane_vrfy_chk = DANEMODE(tlsa_flags);
#else
			dane_vrfy_ctx.dane_vrfy_chk = tlsa_flags;
#endif
			if (EX_TEMPFAIL == i &&
			    ((tlsa_flags & (TLSAFLTEMP|DANE_SECURE)) ==
			     (TLSAFLTEMP|DANE_SECURE)))
			{
				(void) sm_strlcpy(SmtpError,
					" for TLSA RR",
					sizeof(SmtpError));
# if NAMED_BIND
				SM_SET_H_ERRNO(TRY_AGAIN);
# endif
			}
#endif /* DANE */
			mci->mci_errno = errno;
			mci->mci_lastuse = curtime();
			mci->mci_deliveries = 0;
			mci->mci_exitstat = i;
			mci_clr_extensions(mci);
#if NAMED_BIND
			mci->mci_herrno = h_errno;
#endif

			/*
			**  Have we tried long enough to get a connection?
			**	If yes, skip to the fallback MX hosts
			**	(if existent).
			*/

			if (enough > 0 && mci->mci_lastuse >= enough)
			{
				int h;
#if NAMED_BIND
				extern int NumFallbackMXHosts;
#else
				const int NumFallbackMXHosts = 0;
#endif

				if (hostnum < nummxhosts && LogLevel > 9)
					sm_syslog(LOG_INFO, e->e_id,
						  "Timeout.to_aconnect occurred before exhausting all addresses");

				/* turn off timeout if fallback available */
				if (NumFallbackMXHosts > 0)
					enough = 0;

				/* skip to a fallback MX host */
				h = nummxhosts - NumFallbackMXHosts;
				if (hostnum < h)
					hostnum = h;
			}
			if (i == EX_OK)
			{
				goodmxfound = true;
				markstats(e, firstto, STATS_CONNECT);
				mci->mci_state = MCIS_OPENING;
				mci_cache(mci);
				if (TrafficLogFile != NULL)
					(void) sm_io_fprintf(TrafficLogFile,
							     SM_TIME_DEFAULT,
							     "%05d === CONNECT %s\n",
							     (int) CurrentPid,
							     hostbuf);
				break;
			}
			else
			{
				/* Try FallbackSmartHost? */
				if (should_try_fbsh(e, &tried_fallbacksmarthost,
						    hostbuf, sizeof(hostbuf), i))
					goto one_last_try;

				if (tTd(11, 1))
					sm_dprintf("openmailer: makeconnection(%s) => stat=%d, errno=%d\n",
						   hostbuf, i, errno);
				if (i == EX_TEMPFAIL)
					goodmxfound = true;
				mci_unlock_host(mci);
			}

			/* enter status of this host */
			setstat(i);

			/* should print some message here for -v mode */
		}
		if (mci == NULL)
		{
			syserr("deliver: no host name");
			rcode = EX_SOFTWARE;
			goto give_up;
		}
		mci->mci_pid = 0;
	}
	else
	{
		/* flush any expired connections */
		(void) mci_scan(NULL);
		mci = NULL;

		if (bitnset(M_LMTP, m->m_flags))
		{
			/* try to get a cached connection */
			mci = mci_get(m->m_name, m);
			if (mci->mci_host == NULL)
				mci->mci_host = m->m_name;
			CurHostName = mci->mci_host;
			if (mci->mci_state != MCIS_CLOSED)
			{
				message("Using cached LMTP connection for %s...",
					m->m_name);
				mci->mci_deliveries++;
				goto do_transfer;
			}
		}

		/* announce the connection to verbose listeners */
		if (host == NULL || host[0] == '\0')
			message("Connecting to %s...", m->m_name);
		else
			message("Connecting to %s via %s...", host, m->m_name);
		if (TrafficLogFile != NULL)
		{
			char **av;

			(void) sm_io_fprintf(TrafficLogFile, SM_TIME_DEFAULT,
					     "%05d === EXEC", (int) CurrentPid);
			for (av = pv; *av != NULL; av++)
				(void) sm_io_fprintf(TrafficLogFile,
						     SM_TIME_DEFAULT, " %s",
						     *av);
			(void) sm_io_fprintf(TrafficLogFile, SM_TIME_DEFAULT,
					     "\n");
		}

		checkfd012("before creating mail pipe");

		/* create a pipe to shove the mail through */
		if (pipe(mpvect) < 0)
		{
			syserr("%s... openmailer(%s): pipe (to mailer)",
			       shortenstring(e->e_to, MAXSHORTSTR), m->m_name);
			if (tTd(11, 1))
				sm_dprintf("openmailer: NULL\n");
			rcode = EX_OSERR;
			goto give_up;
		}

#if XDEBUG
		/* make sure we didn't get one of the standard I/O files */
		if (mpvect[0] < 3 || mpvect[1] < 3)
		{
			syserr("%s... openmailer(%s): bogus mpvect %d %d",
			       shortenstring(e->e_to, MAXSHORTSTR), m->m_name,
			       mpvect[0], mpvect[1]);
			printopenfds(true);
			if (tTd(11, 1))
				sm_dprintf("openmailer: NULL\n");
			rcode = EX_OSERR;
			goto give_up;
		}

		/* make sure system call isn't dead meat */
		checkfdopen(mpvect[0], "mpvect[0]");
		checkfdopen(mpvect[1], "mpvect[1]");
		if (mpvect[0] == mpvect[1] ||
		    (e->e_lockfp != NULL &&
		     (mpvect[0] == sm_io_getinfo(e->e_lockfp, SM_IO_WHAT_FD,
						 NULL) ||
		      mpvect[1] == sm_io_getinfo(e->e_lockfp, SM_IO_WHAT_FD,
						 NULL))))
		{
			if (e->e_lockfp == NULL)
				syserr("%s... openmailer(%s): overlapping mpvect %d %d",
				       shortenstring(e->e_to, MAXSHORTSTR),
				       m->m_name, mpvect[0], mpvect[1]);
			else
				syserr("%s... openmailer(%s): overlapping mpvect %d %d, lockfp = %d",
				       shortenstring(e->e_to, MAXSHORTSTR),
				       m->m_name, mpvect[0], mpvect[1],
				       sm_io_getinfo(e->e_lockfp,
						     SM_IO_WHAT_FD, NULL));
		}
#endif /* XDEBUG */

		/* create a return pipe */
		if (pipe(rpvect) < 0)
		{
			syserr("%s... openmailer(%s): pipe (from mailer)",
			       shortenstring(e->e_to, MAXSHORTSTR),
			       m->m_name);
			(void) close(mpvect[0]);
			(void) close(mpvect[1]);
			if (tTd(11, 1))
				sm_dprintf("openmailer: NULL\n");
			rcode = EX_OSERR;
			goto give_up;
		}
		checkfdopen(rpvect[0], "rpvect[0]");
		checkfdopen(rpvect[1], "rpvect[1]");

		/*
		**  Actually fork the mailer process.
		**	DOFORK is clever about retrying.
		**
		**	Dispose of SIGCHLD signal catchers that may be laying
		**	around so that endmailer will get it.
		*/

		if (e->e_xfp != NULL)	/* for debugging */
			(void) sm_io_flush(e->e_xfp, SM_TIME_DEFAULT);
		(void) sm_io_flush(smioout, SM_TIME_DEFAULT);
		(void) sm_signal(SIGCHLD, SIG_DFL);


		DOFORK(FORK);
		/* pid is set by DOFORK */

		if (pid < 0)
		{
			/* failure */
			syserr("%s... openmailer(%s): cannot fork",
			       shortenstring(e->e_to, MAXSHORTSTR), m->m_name);
			(void) close(mpvect[0]);
			(void) close(mpvect[1]);
			(void) close(rpvect[0]);
			(void) close(rpvect[1]);
			if (tTd(11, 1))
				sm_dprintf("openmailer: NULL\n");
			rcode = EX_OSERR;
			goto give_up;
		}
		else if (pid == 0)
		{
			int save_errno;
			int sff;
			int new_euid = NO_UID;
			int new_ruid = NO_UID;
			int new_gid = NO_GID;
			char *user = NULL;
			struct stat stb;
			extern int DtableSize;

			CurrentPid = getpid();

			/* clear the events to turn off SIGALRMs */
			sm_clear_events();

			/* Reset global flags */
			RestartRequest = NULL;
			RestartWorkGroup = false;
			ShutdownRequest = NULL;
			PendingSignal = 0;

			if (e->e_lockfp != NULL)
				(void) close(sm_io_getinfo(e->e_lockfp,
							   SM_IO_WHAT_FD,
							   NULL));

			/* child -- set up input & exec mailer */
			(void) sm_signal(SIGALRM, sm_signal_noop);
			(void) sm_signal(SIGCHLD, SIG_DFL);
			(void) sm_signal(SIGHUP, SIG_IGN);
			(void) sm_signal(SIGINT, SIG_IGN);
			(void) sm_signal(SIGTERM, SIG_DFL);
#ifdef SIGUSR1
			(void) sm_signal(SIGUSR1, sm_signal_noop);
#endif

			if (m != FileMailer || stat(tochain->q_user, &stb) < 0)
				stb.st_mode = 0;

#if HASSETUSERCONTEXT
			/*
			**  Set user resources.
			*/

			if (contextaddr != NULL)
			{
				int sucflags;
				struct passwd *pwd;

				if (contextaddr->q_ruser != NULL)
					pwd = sm_getpwnam(contextaddr->q_ruser);
				else
					pwd = sm_getpwnam(contextaddr->q_user);
				sucflags = LOGIN_SETRESOURCES|LOGIN_SETPRIORITY;
# ifdef LOGIN_SETCPUMASK
				sucflags |= LOGIN_SETCPUMASK;
# endif
# ifdef LOGIN_SETLOGINCLASS
				sucflags |= LOGIN_SETLOGINCLASS;
# endif
# ifdef LOGIN_SETMAC
				sucflags |= LOGIN_SETMAC;
# endif
				if (pwd != NULL &&
				    setusercontext(NULL, pwd, pwd->pw_uid,
						   sucflags) == -1 &&
				    suidwarn)
				{
					syserr("openmailer: setusercontext() failed");
					exit(EX_TEMPFAIL);
				}
			}
#endif /* HASSETUSERCONTEXT */

#if HASNICE
			/* tweak niceness */
			if (m->m_nice != 0)
				(void) nice(m->m_nice);
#endif

			/* reset group id */
			if (bitnset(M_SPECIFIC_UID, m->m_flags))
			{
				if (m->m_gid == NO_GID)
					new_gid = RunAsGid;
				else
					new_gid = m->m_gid;
			}
			else if (bitset(S_ISGID, stb.st_mode))
				new_gid = stb.st_gid;
			else if (ctladdr != NULL && ctladdr->q_gid != 0)
			{
				if (!DontInitGroups)
				{
					user = ctladdr->q_ruser;
					if (user == NULL)
						user = ctladdr->q_user;

					if (initgroups(user,
						       ctladdr->q_gid) == -1
					    && suidwarn)
					{
						syserr("openmailer: initgroups(%s, %ld) failed",
							user, (long) ctladdr->q_gid);
						exit(EX_TEMPFAIL);
					}
				}
				else
				{
					GIDSET_T gidset[1];

					gidset[0] = ctladdr->q_gid;
					if (setgroups(1, gidset) == -1
					    && suidwarn)
					{
						syserr("openmailer: setgroups() failed");
						exit(EX_TEMPFAIL);
					}
				}
				new_gid = ctladdr->q_gid;
			}
			else
			{
				if (!DontInitGroups)
				{
					user = DefUser;
					if (initgroups(DefUser, DefGid) == -1 &&
					    suidwarn)
					{
						syserr("openmailer: initgroups(%s, %ld) failed",
						       DefUser, (long) DefGid);
						exit(EX_TEMPFAIL);
					}
				}
				else
				{
					GIDSET_T gidset[1];

					gidset[0] = DefGid;
					if (setgroups(1, gidset) == -1
					    && suidwarn)
					{
						syserr("openmailer: setgroups() failed");
						exit(EX_TEMPFAIL);
					}
				}
				if (m->m_gid == NO_GID)
					new_gid = DefGid;
				else
					new_gid = m->m_gid;
			}
			if (new_gid != NO_GID)
			{
				if (RunAsUid != 0 &&
				    bitnset(M_SPECIFIC_UID, m->m_flags) &&
				    new_gid != getgid() &&
				    new_gid != getegid())
				{
					/* Only root can change the gid */
					syserr("openmailer: insufficient privileges to change gid, RunAsUid=%ld, new_gid=%ld, gid=%ld, egid=%ld",
					       (long) RunAsUid, (long) new_gid,
					       (long) getgid(), (long) getegid());
					exit(EX_TEMPFAIL);
				}

				if (setgid(new_gid) < 0 && suidwarn)
				{
					syserr("openmailer: setgid(%ld) failed",
					       (long) new_gid);
					exit(EX_TEMPFAIL);
				}
			}

			/* change root to some "safe" directory */
			if (m->m_rootdir != NULL)
			{
				expand(m->m_rootdir, cbuf, sizeof(cbuf), e);
				if (tTd(11, 20))
					sm_dprintf("openmailer: chroot %s\n",
						   cbuf);
				if (chroot(cbuf) < 0)
				{
					syserr("openmailer: Cannot chroot(%s)",
					       cbuf);
					exit(EX_TEMPFAIL);
				}
				if (chdir("/") < 0)
				{
					syserr("openmailer: cannot chdir(/)");
					exit(EX_TEMPFAIL);
				}
			}

			/* reset user id */
			endpwent();
			sm_mbdb_terminate();
			if (bitnset(M_SPECIFIC_UID, m->m_flags))
			{
				if (m->m_uid == NO_UID)
					new_euid = RunAsUid;
				else
					new_euid = m->m_uid;

				/*
				**  Undo the effects of the uid change in main
				**  for signal handling.  The real uid may
				**  be used by mailer in adding a "From "
				**  line.
				*/

				if (RealUid != 0 && RealUid != getuid())
				{
#if MAILER_SETUID_METHOD == USE_SETEUID
# if HASSETREUID
					if (setreuid(RealUid, geteuid()) < 0)
					{
						syserr("openmailer: setreuid(%d, %d) failed",
						       (int) RealUid, (int) geteuid());
						exit(EX_OSERR);
					}
# endif /* HASSETREUID */
#endif /* MAILER_SETUID_METHOD == USE_SETEUID */
#if MAILER_SETUID_METHOD == USE_SETREUID
					new_ruid = RealUid;
#endif
				}
			}
			else if (bitset(S_ISUID, stb.st_mode))
				new_ruid = stb.st_uid;
			else if (ctladdr != NULL && ctladdr->q_uid != 0)
				new_ruid = ctladdr->q_uid;
			else if (m->m_uid != NO_UID)
				new_ruid = m->m_uid;
			else
				new_ruid = DefUid;

#if _FFR_USE_SETLOGIN
			/* run disconnected from terminal and set login name */
			if (setsid() >= 0 &&
			    ctladdr != NULL && ctladdr->q_uid != 0 &&
			    new_euid == ctladdr->q_uid)
			{
				struct passwd *pwd;

				pwd = sm_getpwuid(ctladdr->q_uid);
				if (pwd != NULL && suidwarn)
					(void) setlogin(pwd->pw_name);
				endpwent();
			}
#endif /* _FFR_USE_SETLOGIN */

			if (new_euid != NO_UID)
			{
				if (RunAsUid != 0 && new_euid != RunAsUid)
				{
					/* Only root can change the uid */
					syserr("openmailer: insufficient privileges to change uid, new_euid=%ld, RunAsUid=%ld",
					       (long) new_euid, (long) RunAsUid);
					exit(EX_TEMPFAIL);
				}

				vendor_set_uid(new_euid);
#if MAILER_SETUID_METHOD == USE_SETEUID
				if (seteuid(new_euid) < 0 && suidwarn)
				{
					syserr("openmailer: seteuid(%ld) failed",
					       (long) new_euid);
					exit(EX_TEMPFAIL);
				}
#endif /* MAILER_SETUID_METHOD == USE_SETEUID */
#if MAILER_SETUID_METHOD == USE_SETREUID
				if (setreuid(new_ruid, new_euid) < 0 && suidwarn)
				{
					syserr("openmailer: setreuid(%ld, %ld) failed",
					       (long) new_ruid, (long) new_euid);
					exit(EX_TEMPFAIL);
				}
#endif /* MAILER_SETUID_METHOD == USE_SETREUID */
#if MAILER_SETUID_METHOD == USE_SETUID
				if (new_euid != geteuid() && setuid(new_euid) < 0 && suidwarn)
				{
					syserr("openmailer: setuid(%ld) failed",
					       (long) new_euid);
					exit(EX_TEMPFAIL);
				}
#endif /* MAILER_SETUID_METHOD == USE_SETUID */
			}
			else if (new_ruid != NO_UID)
			{
				vendor_set_uid(new_ruid);
				if (setuid(new_ruid) < 0 && suidwarn)
				{
					syserr("openmailer: setuid(%ld) failed",
					       (long) new_ruid);
					exit(EX_TEMPFAIL);
				}
			}

			if (tTd(11, 2))
				sm_dprintf("openmailer: running as r/euid=%ld/%ld, r/egid=%ld/%ld\n",
					   (long) getuid(), (long) geteuid(),
					   (long) getgid(), (long) getegid());

			/* move into some "safe" directory */
			if (m->m_execdir != NULL)
			{
				char *q;

				for (p = m->m_execdir; p != NULL; p = q)
				{
					q = strchr(p, ':');
					if (q != NULL)
						*q = '\0';
					expand(p, cbuf, sizeof(cbuf), e);
					if (q != NULL)
						*q++ = ':';
					if (tTd(11, 20))
						sm_dprintf("openmailer: trydir %s\n",
							   cbuf);
					if (cbuf[0] != '\0' &&
					    chdir(cbuf) >= 0)
						break;
				}
			}

			/* Check safety of program to be run */
			sff = SFF_ROOTOK|SFF_EXECOK;
			if (!bitnset(DBS_RUNWRITABLEPROGRAM,
				     DontBlameSendmail))
				sff |= SFF_NOGWFILES|SFF_NOWWFILES;
			if (bitnset(DBS_RUNPROGRAMINUNSAFEDIRPATH,
				    DontBlameSendmail))
				sff |= SFF_NOPATHCHECK;
			else
				sff |= SFF_SAFEDIRPATH;
			ret = safefile(m->m_mailer, getuid(), getgid(),
				       user, sff, 0, NULL);
			if (ret != 0)
				sm_syslog(LOG_INFO, e->e_id,
					  "Warning: program %s unsafe: %s",
					  m->m_mailer, sm_errstring(ret));

			/* arrange to filter std & diag output of command */
			(void) close(rpvect[0]);
			if (dup2(rpvect[1], STDOUT_FILENO) < 0)
			{
				syserr("%s... openmailer(%s): cannot dup pipe %d for stdout",
				       shortenstring(e->e_to, MAXSHORTSTR),
				       m->m_name, rpvect[1]);
				_exit(EX_OSERR);
			}
			(void) close(rpvect[1]);

			if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0)
			{
				syserr("%s... openmailer(%s): cannot dup stdout for stderr",
				       shortenstring(e->e_to, MAXSHORTSTR),
				       m->m_name);
				_exit(EX_OSERR);
			}

			/* arrange to get standard input */
			(void) close(mpvect[1]);
			if (dup2(mpvect[0], STDIN_FILENO) < 0)
			{
				syserr("%s... openmailer(%s): cannot dup pipe %d for stdin",
				       shortenstring(e->e_to, MAXSHORTSTR),
				       m->m_name, mpvect[0]);
				_exit(EX_OSERR);
			}
			(void) close(mpvect[0]);

			/* arrange for all the files to be closed */
			sm_close_on_exec(STDERR_FILENO + 1, DtableSize);

#if !_FFR_USE_SETLOGIN
			/* run disconnected from terminal */
			(void) setsid();
#endif

			/* try to execute the mailer */
			(void) execve(m->m_mailer, (ARGV_T) pv,
				      (ARGV_T) UserEnviron);
			save_errno = errno;
			syserr("Cannot exec %s", m->m_mailer);
			if (bitnset(M_LOCALMAILER, m->m_flags) ||
			    transienterror(save_errno))
				_exit(EX_OSERR);
			_exit(EX_UNAVAILABLE);
		}

		/*
		**  Set up return value.
		*/

		if (mci == NULL)
		{
			if (clever)
			{
				/*
				**  Allocate from general heap, not
				**  envelope rpool, because this mci
				**  is going to be cached.
				*/

				mci = mci_new(NULL);
			}
			else
			{
				/*
				**  Prevent a storage leak by allocating
				**  this from the envelope rpool.
				*/

				mci = mci_new(e->e_rpool);
			}
		}
		mci->mci_mailer = m;
		if (clever)
		{
			mci->mci_state = MCIS_OPENING;
			mci_cache(mci);
		}
		else
		{
			mci->mci_state = MCIS_OPEN;
		}
		mci->mci_pid = pid;
		(void) close(mpvect[0]);
		mci->mci_out = sm_io_open(SmFtStdiofd, SM_TIME_DEFAULT,
					  (void *) &(mpvect[1]), SM_IO_WRONLY_B,
					  NULL);
		if (mci->mci_out == NULL)
		{
			syserr("deliver: cannot create mailer output channel, fd=%d",
			       mpvect[1]);
			(void) close(mpvect[1]);
			(void) close(rpvect[0]);
			(void) close(rpvect[1]);
			rcode = EX_OSERR;
			goto give_up;
		}

		(void) close(rpvect[1]);
		mci->mci_in = sm_io_open(SmFtStdiofd, SM_TIME_DEFAULT,
					 (void *) &(rpvect[0]), SM_IO_RDONLY_B,
					 NULL);
		if (mci->mci_in == NULL)
		{
			syserr("deliver: cannot create mailer input channel, fd=%d",
			       mpvect[1]);
			(void) close(rpvect[0]);
			SM_CLOSE_FP(mci->mci_out);
			rcode = EX_OSERR;
			goto give_up;
		}
	}

	/*
	**  If we are in SMTP opening state, send initial protocol.
	*/

	if (bitnset(M_7BITS, m->m_flags) &&
	    (!clever || mci->mci_state == MCIS_OPENING))
		mci->mci_flags |= MCIF_7BIT;
	if (clever && mci->mci_state != MCIS_CLOSED)
	{
#if STARTTLS || SASL
		char *srvname;
		int dotpos;
# if SASL
#  define DONE_AUTH(f)		bitset(MCIF_AUTHACT, f)
# endif
# if STARTTLS
#  define DONE_STARTTLS(f)	bitset(MCIF_TLSACT, f)
# endif

		srvname = setservermacros(mci, &dotpos);
# if DANE
		SM_FREE(dane_vrfy_ctx.dane_vrfy_host);
		SM_FREE(dane_vrfy_ctx.dane_vrfy_sni);
		dane_vrfy_ctx.dane_vrfy_fp[0] = '\0';
		if (STE_HAS_TLSA(ste) && ste->s_tlsa->dane_tlsa_sni != NULL)
			dane_vrfy_ctx.dane_vrfy_sni = sm_strdup(ste->s_tlsa->dane_tlsa_sni);
		dane_vrfy_ctx.dane_vrfy_host = sm_strdup(srvname);
# endif /* DANE */
		/* undo change of srvname (== mci->mci_host) */
		FIX_TRAIL_DOT(srvname);

reconnect:	/* after switching to an encrypted connection */
# if DANE
		if (DONE_STARTTLS(mci->mci_flags))
		{
			/* use a "reset" function? */
			/* when is it required to "reset" this data? */
			SM_FREE(dane_vrfy_ctx.dane_vrfy_host);
			SM_FREE(dane_vrfy_ctx.dane_vrfy_sni);
			dane_vrfy_ctx.dane_vrfy_fp[0] = '\0';
			dane_vrfy_ctx.dane_vrfy_res = DANE_VRFY_NONE;
			dane_vrfy_ctx.dane_vrfy_dane_enabled = false;
			if (TTD(90, 40))
				sm_dprintf("deliver: reset: chk=%#x, dane_enabled=%d\n",
					dane_vrfy_ctx.dane_vrfy_chk,
					dane_vrfy_ctx.dane_vrfy_dane_enabled);
		}
# endif /* DANE */

#endif /* STARTTLS || SASL */

		/* set the current connection information */
		e->e_mci = mci;
#if SASL
		mci->mci_saslcap = NULL;
#endif
#if _FFR_MTA_STS
# define USEMTASTS (MTASTS && !SM_TLSI_IS(&(mci->mci_tlsi), TLSI_FL_NOSTS) && !iscltflgset(e, D_NOSTS))
# if DANE
#  define CHKMTASTS (USEMTASTS && (ste == NULL || ste->s_tlsa == NULL || SM_TLSI_IS(&(mci->mci_tlsi), TLSI_FL_NODANE)))
# else
#  define CHKMTASTS USEMTASTS
# endif
#endif /* _FFR_MTA_STS */
#if _FFR_MTA_STS
		if (!DONE_STARTTLS(mci->mci_flags))
		{
		/*
		**  HACK: use the domain of the first valid RCPT for STS.
		**  It seems whoever wrote the specs did not consider
		**  SMTP sessions versus transactions.
		**  (but what would you expect from people who try
		**  to use https for "security" after relying on DNS?)
		*/

		macdefine(&e->e_macro, A_PERM, macid("{rcpt_addr}"), "");
# if DANE
		if (MTASTS && STE_HAS_TLSA(ste))
			macdefine(&e->e_macro, A_PERM, macid("{sts_sni}"), "DANE");
		else
# endif
			macdefine(&e->e_macro, A_PERM, macid("{sts_sni}"), "");
		if (USEMTASTS && firstto->q_user != NULL)
		{
			if (tTd(10, 64))
			{
				sm_dprintf("firstto ");
				printaddr(sm_debug_file(), firstto, false);
			}
			macdefine(&e->e_macro, A_TEMP,
				  macid("{rcpt_addr}"), firstto->q_user);
		}
		else if (USEMTASTS)
		{
			if (tTd(10, 64))
			{
				sm_dprintf("tochain ");
				printaddr(sm_debug_file(), tochain, false);
			}
			for (to = tochain; to != NULL; to = to->q_tchain)
			{
				if (!QS_IS_UNMARKED(to->q_state))
					continue;
				if (to->q_user == NULL)
					continue;
				macdefine(&e->e_macro, A_TEMP,
					  macid("{rcpt_addr}"), to->q_user);
				break;
			}
		}
		}
#endif /* _FFR_MTA_STS */
#if USE_EAI
		if (!addr_is_ascii(e->e_from.q_paddr) && !e->e_smtputf8)
			e->e_smtputf8 = true;
		for (to = tochain; to != NULL && !e->e_smtputf8; to = to->q_tchain)
		{
			if (!QS_IS_UNMARKED(to->q_state))
				continue;
			if (!addr_is_ascii(to->q_user))
				e->e_smtputf8 = true;
		}
		/* XXX reset e_smtputf8 to original state at the end? */
#endif /* USE_EAI */

#define ONLY_HELO(f)		bitset(MCIF_ONLY_EHLO, f)
#define SET_HELO(f)		f |= MCIF_ONLY_EHLO
#define CLR_HELO(f)		f &= ~MCIF_ONLY_EHLO

#if _FFR_SMTPS_CLIENT && STARTTLS
		/*
		**  For M_SMTPS_CLIENT, we do the STARTTLS code first,
		**  then jump back and start the SMTP conversation.
		*/

		implicittls = bitnset(M_SMTPS_CLIENT, mci->mci_mailer->m_flags);
		if (implicittls)
			goto dotls;
backtosmtp:
#endif /* _FFR_SMTPS_CLIENT && STARTTLS */

		smtpinit(m, mci, e, ONLY_HELO(mci->mci_flags));
		CLR_HELO(mci->mci_flags);

		if (IS_DLVR_RETURN(e))
		{
			/*
			**  Check whether other side can deliver e-mail
			**  fast enough
			*/

			if (!bitset(MCIF_DLVR_BY, mci->mci_flags))
			{
				e->e_status = "5.4.7";
				usrerrenh(e->e_status,
					  "554 Server does not support Deliver By");
				rcode = EX_UNAVAILABLE;
				goto give_up;
			}
			if (e->e_deliver_by > 0 &&
			    e->e_deliver_by - (curtime() - e->e_ctime) <
			    mci->mci_min_by)
			{
				e->e_status = "5.4.7";
				usrerrenh(e->e_status,
					  "554 Message can't be delivered in time; %ld < %ld",
					  e->e_deliver_by - (long) (curtime() -
								e->e_ctime),
					  mci->mci_min_by);
				rcode = EX_UNAVAILABLE;
				goto give_up;
			}
		}

#if STARTTLS
# if _FFR_SMTPS_CLIENT
dotls:
# endif
		/* first TLS then AUTH to provide a security layer */
		if (mci->mci_state != MCIS_CLOSED &&
		    !DONE_STARTTLS(mci->mci_flags))
		{
			int olderrors;
			bool usetls;
			bool saveQuickAbort = QuickAbort;
			bool saveSuprErrs = SuprErrs;
			char *srvname = NULL;

			rcode = EX_OK;
			usetls = bitset(MCIF_TLS, mci->mci_flags) || implicittls;
			if (usetls)
				usetls = !iscltflgset(e, D_NOTLS);
			if (usetls)
				usetls = tlsstate == 0;

			srvname = macvalue(macid("{server_name}"), e);
			if (usetls)
			{
				olderrors = Errors;
				QuickAbort = false;
				SuprErrs = true;
				if (rscheck("try_tls", srvname, NULL, e,
					    RSF_RMCOMM|RSF_STATUS, 7, srvname,
					    NOQID, NULL, NULL) != EX_OK
				    || Errors > olderrors)
				{
					usetls = false;
				}
				SuprErrs = saveSuprErrs;
				QuickAbort = saveQuickAbort;
			}

			if (usetls)
			{
				if ((rcode = starttls(m, mci, e, implicittls
# if DANE
							, &dane_vrfy_ctx
# endif
					)) == EX_OK)
				{
					/* start again without STARTTLS */
					mci->mci_flags |= MCIF_TLSACT;
# if DANE && _FFR_MTA_STS
/* if DANE is used (and STS should be used): disable STS */
/* also check MTASTS and NOSTS flag? */
					if (STE_HAS_TLSA(ste) &&
					    !SM_TLSI_IS(&(mci->mci_tlsi), TLSI_FL_NODANE))
						macdefine(&e->e_macro, A_PERM, macid("{rcpt_addr}"), "");
# endif
				}
				else
				{
					char *s;

					/*
					**  TLS negotiation failed, what to do?
					**  fall back to unencrypted connection
					**  or abort? How to decide?
					**  set a macro and call a ruleset.
					*/

					mci->mci_flags &= ~MCIF_TLS;
					switch (rcode)
					{
					  case EX_TEMPFAIL:
						s = "TEMP";
						break;
#if 0
					/* see starttls() */
					  case EX_USAGE:
						s = "USAGE";
						break;
#endif
					  case EX_PROTOCOL:
						s = "PROTOCOL";
						break;
					  case EX_SOFTWARE:
						s = "SOFTWARE";
						break;
					  case EX_UNAVAILABLE:
						s = "NONE";
						break;

					/*
					**  Possible return from ruleset
					**  tls_clt_features via
					**  get_tls_se_features().
					*/

					  case EX_CONFIG:
						s = "CONFIG";
						break;

					  /* everything else is a failure */
					  default:
						s = "FAILURE";
						rcode = EX_TEMPFAIL;
					}
# if DANE
					/*
					**  TLSA found but STARTTLS "failed"?
					**  What is the best way to "fail"?
					**  XXX: check expiration!
					*/

					if (!iscltflgset(e, D_NODANE) &&
					    STE_HAS_TLSA(ste) &&
					    TLSA_HAS_RRs(ste->s_tlsa))
					{
						if (LogLevel > 8)
							sm_syslog(LOG_NOTICE, NOQID,
								"STARTTLS=client, relay=%.100s, warning=DANE configured in DNS but STARTTLS failed",
								srvname);
						/* XXX include TLSA RR from DNS? */

						/*
						**  Only override codes which
						**  do not cause a failure
						**  in the default rules.
						*/

						if (EX_PROTOCOL != rcode &&
						    EX_SOFTWARE != rcode &&
						    EX_CONFIG != rcode)
						{
							/* s = "DANE_TEMP"; */
							dane_vrfy_ctx.dane_vrfy_chk |= TLSAFLNOTLS;
						}
					}
# endif /* DANE */
					macdefine(&e->e_macro, A_PERM,
						  macid("{verify}"), s);
				}
			}
			else
			{
				p = tlsstate == 0 ? "NONE": "CLEAR";
# if DANE
				/*
				**  TLSA found but STARTTLS not offered?
				**  What is the best way to "fail"?
				**  XXX: check expiration!
				*/

				if (!bitset(MCIF_TLS, mci->mci_flags) &&
				    !iscltflgset(e, D_NODANE) &&
				    STE_HAS_TLSA(ste) &&
				    TLSA_HAS_RRs(ste->s_tlsa))
				{
					if (LogLevel > 8)
						sm_syslog(LOG_NOTICE, NOQID,
							"STARTTLS=client, relay=%.100s, warning=DANE configured in DNS but STARTTLS not offered",
							srvname);
					/* XXX include TLSA RR from DNS? */
				}
# endif /* DANE */
				macdefine(&e->e_macro, A_PERM,
					  macid("{verify}"), p);
			}
			olderrors = Errors;
			QuickAbort = false;
			SuprErrs = true;

			/*
			**  rcode == EX_SOFTWARE is special:
			**  the TLS negotiation failed
			**  we have to drop the connection no matter what.
			**  However, we call tls_server to give it the chance
			**  to log the problem and return an appropriate
			**  error code.
			*/

			if (rscheck("tls_server",
				    macvalue(macid("{verify}"), e),
				    NULL, e, RSF_RMCOMM|RSF_COUNT, 5,
				    srvname, NOQID, NULL, NULL) != EX_OK ||
			    Errors > olderrors ||
			    rcode == EX_SOFTWARE)
			{
				char enhsc[ENHSCLEN];
				extern char MsgBuf[];

				if (ISSMTPCODE(MsgBuf) &&
				    extenhsc(MsgBuf + 4, ' ', enhsc) > 0)
				{
					p = sm_rpool_strdup_x(e->e_rpool,
							      MsgBuf);
				}
				else
				{
					p = "403 4.7.0 server not authenticated.";
					(void) sm_strlcpy(enhsc, "4.7.0",
							  sizeof(enhsc));
				}
				SuprErrs = saveSuprErrs;
				QuickAbort = saveQuickAbort;

				if (rcode == EX_SOFTWARE)
				{
					/* drop the connection */
					mci->mci_state = MCIS_ERROR;
					SM_CLOSE_FP(mci->mci_out);
					mci->mci_flags &= ~MCIF_TLSACT;
					(void) endmailer(mci, e, pv);

					if ((TLSFallbacktoClear ||
					     SM_TLSI_IS(&(mci->mci_tlsi),
							TLSI_FL_FB2CLR)) &&
					    !SM_TLSI_IS(&(mci->mci_tlsi),
							TLSI_FL_NOFB2CLR)
# if DANE
					     && dane_vrfy_ctx.dane_vrfy_chk !=
						DANE_SECURE
# endif
# if _FFR_MTA_STS
					     && !SM_TLSI_IS(&(mci->mci_tlsi),
							TLSI_FL_STS_NOFB2CLR)
# endif
					    )
					{
						++tlsstate;
					}
				}
				else
				{
					/* abort transfer */
					smtpquit(m, mci, e);
				}

				/* avoid bogus error msg */
				mci->mci_errno = 0;

				/* temp or permanent failure? */
				rcode = (*p == '4') ? EX_TEMPFAIL
						    : EX_UNAVAILABLE;
				mci_setstat(mci, rcode, enhsc, p);

				/*
				**  hack to get the error message into
				**  the envelope (done in giveresponse())
				*/

				(void) sm_strlcpy(SmtpError, p,
						  sizeof(SmtpError));
			}
			else if (mci->mci_state == MCIS_CLOSED)
			{
				/* connection close caused by 421 */
				mci->mci_errno = 0;
				rcode = EX_TEMPFAIL;
				mci_setstat(mci, rcode, NULL, "421");
			}
			else
				rcode = 0;

			QuickAbort = saveQuickAbort;
			SuprErrs = saveSuprErrs;
			if (DONE_STARTTLS(mci->mci_flags) &&
			    mci->mci_state != MCIS_CLOSED
# if _FFR_SMTPS_CLIENT
			    && !implicittls && !smtptls
# endif
			   )
			{
				SET_HELO(mci->mci_flags);
				mci_clr_extensions(mci);
				goto reconnect;
			}
			if (tlsstate == 1)
			{
				if (tTd(11, 1))
				{
					sm_syslog(LOG_DEBUG, NOQID,
						"STARTTLS=client, relay=%.100s, tlsstate=%d, status=trying_again",
						mci->mci_host, tlsstate);
					mci_dump(NULL, mci, true);
				}
				++tlsstate;

				/*
				**  Fake the status so a new connection is
				**  tried, otherwise the TLS error will
				**  "persist" during this delivery attempt.
				*/

				mci->mci_errno = 0;
				rcode = EX_OK;
				mci_setstat(mci, rcode, NULL, NULL);
				goto one_last_try;
}
		}

# if _FFR_SMTPS_CLIENT
		/*
		**  For M_SMTPS_CLIENT, we do the STARTTLS code first,
		**  then jump back and start the SMTP conversation.
		*/

		if (implicittls && !smtptls)
		{
			smtptls = true;
			if (!DONE_STARTTLS(mci->mci_flags))
			{
				if (rcode == EX_TEMPFAIL)
				{
					e->e_status = "4.3.3";
					usrerrenh(e->e_status, "454 TLS session initiation failed");
				}
				else
				{
					e->e_status = "5.3.3";
					usrerrenh(e->e_status, "554 TLS session initiation failed");
				}
				goto give_up;
			}
			goto backtosmtp;
		}
# endif /* _FFR_SMTPS_CLIENT */
#endif /* STARTTLS */
#if SASL
		/* if other server supports authentication let's authenticate */
		if (mci->mci_state != MCIS_CLOSED &&
		    mci->mci_saslcap != NULL &&
		    !DONE_AUTH(mci->mci_flags) && !iscltflgset(e, D_NOAUTH))
		{
			/* Should we require some minimum authentication? */
			if ((ret = smtpauth(m, mci, e)) == EX_OK)
			{
				int result;
				sasl_ssf_t *ssf = NULL;

				/* Get security strength (features) */
				result = sasl_getprop(mci->mci_conn, SASL_SSF,
# if SASL >= 20000
						      (const void **) &ssf);
# else
						      (void **) &ssf);
# endif

				/* XXX authid? */
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, NOQID,
						  "AUTH=client, relay=%.100s, mech=%.16s, bits=%d",
						  mci->mci_host,
						  macvalue(macid("{auth_type}"), e),
						  result == SASL_OK ? *ssf : 0);

				/*
				**  Only switch to encrypted connection
				**  if a security layer has been negotiated
				*/

				if (result == SASL_OK && *ssf > 0)
				{
					int tmo;

					/*
					**  Convert I/O layer to use SASL.
					**  If the call fails, the connection
					**  is aborted.
					*/

					tmo = DATA_PROGRESS_TIMEOUT * 1000;
					if (sfdcsasl(&mci->mci_in,
						     &mci->mci_out,
						     mci->mci_conn, tmo) == 0)
					{
						mci_clr_extensions(mci);
						mci->mci_flags |= MCIF_AUTHACT|
								  MCIF_ONLY_EHLO;
						goto reconnect;
					}
					syserr("AUTH TLS switch failed in client");
				}
				/* else? XXX */
				mci->mci_flags |= MCIF_AUTHACT;

			}
			else if (ret == EX_TEMPFAIL)
			{
				if (LogLevel > 8)
					sm_syslog(LOG_ERR, NOQID,
						  "AUTH=client, relay=%.100s, temporary failure, connection abort",
						  mci->mci_host);
				smtpquit(m, mci, e);

				/* avoid bogus error msg */
				mci->mci_errno = 0;
				rcode = EX_TEMPFAIL;
				mci_setstat(mci, rcode, "4.3.0", p);

				/*
				**  hack to get the error message into
				**  the envelope (done in giveresponse())
				*/

				(void) sm_strlcpy(SmtpError,
						  "Temporary AUTH failure",
						  sizeof(SmtpError));
			}
		}
#endif /* SASL */
	}

do_transfer:
	/* clear out per-message flags from connection structure */
	mci->mci_flags &= ~(MCIF_CVT7TO8|MCIF_CVT8TO7);

	if (bitset(EF_HAS8BIT, e->e_flags) &&
	    !bitset(EF_DONT_MIME, e->e_flags) &&
	    bitnset(M_7BITS, m->m_flags))
		mci->mci_flags |= MCIF_CVT8TO7;

#if MIME7TO8
	if (bitnset(M_MAKE8BIT, m->m_flags) &&
	    !bitset(MCIF_7BIT, mci->mci_flags) &&
	    (p = hvalue("Content-Transfer-Encoding", e->e_header)) != NULL &&
	     (SM_STRCASEEQ(p, "quoted-printable") ||
	      SM_STRCASEEQ(p, "base64")) &&
	    (p = hvalue("Content-Type", e->e_header)) != NULL)
	{
		/* may want to convert 7 -> 8 */
		/* XXX should really parse it here -- and use a class XXX */
		if (sm_strncasecmp(p, "text/plain", 10) == 0 &&
		    (p[10] == '\0' || p[10] == ' ' || p[10] == ';'))
			mci->mci_flags |= MCIF_CVT7TO8;
	}
#endif /* MIME7TO8 */

	if (tTd(11, 1))
	{
		sm_dprintf("openmailer: ");
		mci_dump(sm_debug_file(), mci, false);
	}

#if _FFR_CLIENT_SIZE
	/*
	**  See if we know the maximum size and
	**  abort if the message is too big.
	**
	**  NOTE: _FFR_CLIENT_SIZE is untested.
	*/

	if (bitset(MCIF_SIZE, mci->mci_flags) &&
	    mci->mci_maxsize > 0 &&
	    e->e_msgsize > mci->mci_maxsize)
	{
		e->e_flags |= EF_NO_BODY_RETN;
		if (bitnset(M_LOCALMAILER, m->m_flags))
			e->e_status = "5.2.3";
		else
			e->e_status = "5.3.4";

		usrerrenh(e->e_status,
			  "552 Message is too large; %ld bytes max",
			  mci->mci_maxsize);
		rcode = EX_DATAERR;

		/* Need an e_message for error */
		(void) sm_snprintf(SmtpError, sizeof(SmtpError),
				   "Message is too large; %ld bytes max",
				   mci->mci_maxsize);
		goto give_up;
	}
#endif /* _FFR_CLIENT_SIZE */

	if (mci->mci_state != MCIS_OPEN)
	{
		/* couldn't open the mailer */
		rcode = mci->mci_exitstat;
		errno = mci->mci_errno;
		SM_SET_H_ERRNO(mci->mci_herrno);
		if (rcode == EX_OK)
		{
			/* shouldn't happen */
			syserr("554 5.3.5 deliver: mci=%lx rcode=%d errno=%d state=%d sig=%s",
			       (unsigned long) mci, rcode, errno,
			       mci->mci_state, firstsig);
			mci_dump_all(smioout, true);
			rcode = EX_SOFTWARE;
		}
		else if (nummxhosts > hostnum)
		{
			logfailover(e, m, mci, rcode, NULL);
			/* try next MX site */
			goto tryhost;
		}
	}
	else if (!clever)
	{
		bool ok;

		/*
		**  Format and send message.
		*/

		rcode = EX_OK;
		errno = 0;
		ok = putfromline(mci, e);
		if (ok)
			ok = (*e->e_puthdr)(mci, e->e_header, e, M87F_OUTER);
		if (ok)
			ok = (*e->e_putbody)(mci, e, NULL);
		if (ok && bitset(MCIF_INLONGLINE, mci->mci_flags))
			ok = putline("", mci);

		/*
		**  Ignore an I/O error that was caused by EPIPE.
		**  Some broken mailers don't read the entire body
		**  but just exit() thus causing an I/O error.
		*/

		if (!ok && (sm_io_error(mci->mci_out) && errno == EPIPE))
			ok = true;

		/* (always) get the exit status */
		rcode = endmailer(mci, e, pv);
		if (!ok)
			rcode = EX_TEMPFAIL;
		if (rcode == EX_TEMPFAIL && SmtpError[0] == '\0')
		{
			/*
			**  Need an e_message for mailq display.
			**  We set SmtpError as
			*/

			(void) sm_snprintf(SmtpError, sizeof(SmtpError),
					   "%s mailer (%s) exited with EX_TEMPFAIL",
					   m->m_name, m->m_mailer);
		}
	}
	else
	{
		/*
		**  Send the MAIL FROM: protocol
		*/

		/* XXX this isn't pipelined... */
		rcode = smtpmailfrom(m, mci, e);
		mci->mci_okrcpts = 0;
		mci->mci_retryrcpt = rcode == EX_TEMPFAIL;
		if (rcode == EX_OK)
		{
			register int rc;
#if PIPELINING
			ADDRESS *volatile pchain;
#endif
#if STARTTLS
			ADDRESS addr;
#endif

			/* send the recipient list */
			rc = EX_OK;
			tobuf[0] = '\0';
			mci->mci_retryrcpt = false;
			mci->mci_tolist = tobuf;
#if PIPELINING
			pchain = NULL;
			mci->mci_nextaddr = NULL;
#endif

			for (to = tochain; to != NULL; to = to->q_tchain)
			{
				if (!QS_IS_UNMARKED(to->q_state))
					continue;

				/* mark recipient state as "ok so far" */
				to->q_state = QS_OK;
				e->e_to = to->q_paddr;
#if _FFR_MTA_STS
				if (CHKMTASTS && to->q_user != NULL)
					macdefine(&e->e_macro, A_TEMP,
						macid("{rcpt_addr}"), to->q_user);
				else
					macdefine(&e->e_macro, A_PERM,
						macid("{rcpt_addr}"), "");
#endif /* _FFR_MTA_STS */
#if STARTTLS
# if DANE
				vrfy = macvalue(macid("{verify}"), e);
				if (NULL == vrfy)
					vrfy = "NONE";
				vrfy = sm_strdup(vrfy);
				if (TTD(10, 32))
					sm_dprintf("deliver: 0: vrfy=%s, to=%s, mx=%s, QMXSECURE=%d, secure=%d, ste=%p, dane=%#x\n",
						vrfy, to->q_user, CurHostName, RCPT_MXSECURE(to),
						RCPT_REQ_DANE(to), ste, Dane);
				if (NULL == ste && CHK_DANE_RCPT(Dane, to))
				{
					(void) gettlsa(CurHostName, NULL, &ste,
						RCPT_MXSECURE(to) ? TLSAFLADMX : 0,
						0, m->m_port);
				}
				if (TTD(10, 32))
					sm_dprintf("deliver: 2: vrfy=%s, to=%s, QMXSECURE=%d, secure=%d, ste=%p, dane=%#x, SUP=%#x, !TEMP=%d, ADIP=%d, chk_dane=%d, vrfy_chk=%#x, mcif=%#lx\n",
						vrfy, to->q_user,
						RCPT_MXSECURE(to), RCPT_REQ_DANE(to), ste, Dane,
						STE_HAS_TLSA(ste) ? TLSA_IS_FL(ste->s_tlsa, TLSAFLSUP) : -1,
						(0 == (dane_vrfy_ctx.dane_vrfy_chk & TLSAFLTEMPVRFY)),
						(0 != (dane_vrfy_ctx.dane_vrfy_chk & TLSAFLADIP)),
						CHK_DANE(dane_vrfy_ctx.dane_vrfy_chk),
						dane_vrfy_ctx.dane_vrfy_chk,
						mci->mci_flags
						);

				if (strcmp("DANE_FAIL", vrfy) == 0)
				{
					if (!RCPT_REQ_DANE(to))
						macdefine(&mci->mci_macro, A_PERM, macid("{verify}"), "FAIL");
					else
						SM_FREE(vrfy);
				}

				/*
				**  Note: MCIF_TLS should be reset when
				**  when starttls was successful because
				**  the server should not offer it anymore.
				*/

				else if (strcmp("TRUSTED", vrfy) != 0 &&
					 RCPT_REQ_DANE(to))
				{
					macdefine(&mci->mci_macro, A_PERM, macid("{verify}"),
						(0 != (dane_vrfy_ctx.dane_vrfy_chk & TLSAFLNOTLS)) ?
						"DANE_TEMP" :
						(bitset(MCIF_TLS|MCIF_TLSACT, mci->mci_flags) ?
						 "DANE_FAIL" : "DANE_NOTLS"));
				}
				/* DANE: unsupported types: require TLS but not available? */
				else if (strcmp("TRUSTED", vrfy) != 0 &&
					RCPT_REQ_TLS(to)
					&& (!bitset(MCIF_TLS|MCIF_TLSACT, mci->mci_flags)
					     || (0 != (dane_vrfy_ctx.dane_vrfy_chk & TLSAFLNOTLS))))
				{
					macdefine(&mci->mci_macro, A_PERM, macid("{verify}"),
						(0 != (dane_vrfy_ctx.dane_vrfy_chk & TLSAFLNOTLS)) ?
						"DANE_TEMP" : "DANE_NOTLS");
				}
				else
					SM_FREE(vrfy);
				if (TTD(10, 32))
					sm_dprintf("deliver: 7: verify=%s, secure=%d\n",
						macvalue(macid("{verify}"), e),
						RCPT_REQ_DANE(to));
# endif /* DANE */

				rc = rscheck("tls_rcpt", to->q_user, NULL, e,
					    RSF_RMCOMM|RSF_COUNT, 3,
					    mci->mci_host, e->e_id, &addr, NULL);

# if DANE
				if (vrfy != NULL)
				{
					macdefine(&mci->mci_macro, A_PERM, macid("{verify}"), vrfy);
					SM_FREE(vrfy);
				}
# endif

				if (TTD(10, 32))
					sm_dprintf("deliver: 9: verify=%s, to=%s, tls_rcpt=%d\n",
						macvalue(macid("{verify}"), e),
						to->q_user, rc);

				if (rc != EX_OK)
				{
					char *dsn;

					to->q_flags |= QINTREPLY;
					markfailure(e, to, mci, rc, false);
					if (addr.q_host != NULL &&
					    isenhsc(addr.q_host, ' ') > 0)
						dsn = addr.q_host;
					else
						dsn = NULL;
					giveresponse(rc, dsn, m, mci,
						     ctladdr, xstart, e, to);
					if (rc == EX_TEMPFAIL)
					{
						mci->mci_retryrcpt = true;
						to->q_state = QS_RETRY;
					}
					continue;
				}
#endif /* STARTTLS */

				rc = smtprcpt(to, m, mci, e, ctladdr, xstart);
#if PIPELINING
				if (rc == EX_OK &&
				    bitset(MCIF_PIPELINED, mci->mci_flags))
				{
					/*
					**  Add new element to list of
					**  recipients for pipelining.
					*/

					to->q_pchain = NULL;
					if (mci->mci_nextaddr == NULL)
						mci->mci_nextaddr = to;
					if (pchain == NULL)
						pchain = to;
					else
					{
						pchain->q_pchain = to;
						pchain = pchain->q_pchain;
					}
				}
#endif /* PIPELINING */
				if (rc != EX_OK)
				{
					markfailure(e, to, mci, rc, false);
					giveresponse(rc, to->q_status, m, mci,
						     ctladdr, xstart, e, to);
					if (rc == EX_TEMPFAIL)
						to->q_state = QS_RETRY;
				}
			}

			/* No recipients in list and no missing responses? */
			if (tobuf[0] == '\0'
#if PIPELINING
			    /* && bitset(MCIF_PIPELINED, mci->mci_flags) */
			    && mci->mci_nextaddr == NULL
#endif
			   )
			{
				rcode = rc;
				e->e_to = NULL;
				if (bitset(MCIF_CACHED, mci->mci_flags))
					smtprset(m, mci, e);
			}
			else
			{
				e->e_to = tobuf + 1;
				rcode = smtpdata(m, mci, e, ctladdr, xstart);
			}
		}

		if (rcode == EX_TEMPFAIL && nummxhosts > hostnum
		    && (mci->mci_retryrcpt || mci->mci_okrcpts > 0)
		   )
		{
			logfailover(e, m, mci, rcode, to);
			/* try next MX site */
			goto tryhost;
		}
	}
#if NAMED_BIND
	if (ConfigLevel < 2)
		_res.options |= RES_DEFNAMES | RES_DNSRCH;	/* XXX */
#endif

	if (tTd(62, 1))
		checkfds("after delivery");

	/*
	**  Do final status disposal.
	**	We check for something in tobuf for the SMTP case.
	**	If we got a temporary failure, arrange to queue the
	**		addressees.
	*/

  give_up:
	if (bitnset(M_LMTP, m->m_flags))
	{
		lmtp_rcode = rcode;
		tobuf[0] = '\0';
		anyok = false;
		strsize = 0;
	}
	else
		anyok = rcode == EX_OK;

	for (to = tochain; to != NULL; to = to->q_tchain)
	{
		/* see if address already marked */
		if (!QS_IS_OK(to->q_state))
			continue;

		/* if running LMTP, get the status for each address */
		if (bitnset(M_LMTP, m->m_flags))
		{
			if (lmtp_rcode == EX_OK)
				rcode = smtpgetstat(m, mci, e);
			if (rcode == EX_OK)
			{
				strsize += sm_strlcat2(tobuf + strsize, ",",
						to->q_paddr,
						tobufsize - strsize);
				SM_ASSERT(strsize < tobufsize);
				anyok = true;
			}
			else
			{
				e->e_to = to->q_paddr;
				markfailure(e, to, mci, rcode, true);
				giveresponse(rcode, to->q_status, m, mci,
					     ctladdr, xstart, e, to);
				e->e_to = tobuf + 1;
				continue;
			}
		}
		else
		{
			/* mark bad addresses */
			if (rcode != EX_OK)
			{
				if (goodmxfound && rcode == EX_NOHOST)
					rcode = EX_TEMPFAIL;
				markfailure(e, to, mci, rcode, true);
				continue;
			}
		}

		/* successful delivery */
		to->q_state = QS_SENT;
		to->q_statdate = curtime();
		e->e_nsent++;

		/*
		**  Checkpoint the send list every few addresses
		*/

		if (CheckpointInterval > 0 && e->e_nsent >= CheckpointInterval)
		{
			queueup(e, QUP_FL_NONE);
			e->e_nsent = 0;
		}

		if (bitnset(M_LOCALMAILER, m->m_flags) &&
		    bitset(QPINGONSUCCESS, to->q_flags))
		{
			to->q_flags |= QDELIVERED;
			to->q_status = "2.1.5";
			(void) sm_io_fprintf(e->e_xfp, SM_TIME_DEFAULT,
					     "%s... Successfully delivered\n",
					     to->q_paddr);
		}
		else if (bitset(QPINGONSUCCESS, to->q_flags) &&
			 bitset(QPRIMARY, to->q_flags) &&
			 !bitset(MCIF_DSN, mci->mci_flags))
		{
			to->q_flags |= QRELAYED;
			(void) sm_io_fprintf(e->e_xfp, SM_TIME_DEFAULT,
					     "%s... relayed; expect no further notifications\n",
					     to->q_paddr);
		}
		else if (IS_DLVR_NOTIFY(e) &&
			 !bitset(MCIF_DLVR_BY, mci->mci_flags) &&
			 bitset(QPRIMARY, to->q_flags) &&
			 (!bitset(QHASNOTIFY, to->q_flags) ||
			  bitset(QPINGONSUCCESS, to->q_flags) ||
			  bitset(QPINGONFAILURE, to->q_flags) ||
			  bitset(QPINGONDELAY, to->q_flags)))
		{
			/* RFC 2852, 4.1.4.2: no NOTIFY, or not NEVER */
			to->q_flags |= QBYNRELAY;
			(void) sm_io_fprintf(e->e_xfp, SM_TIME_DEFAULT,
					     "%s... Deliver-by notify: relayed\n",
					     to->q_paddr);
		}
		else if (IS_DLVR_TRACE(e) &&
			 (!bitset(QHASNOTIFY, to->q_flags) ||
			  bitset(QPINGONSUCCESS, to->q_flags) ||
			  bitset(QPINGONFAILURE, to->q_flags) ||
			  bitset(QPINGONDELAY, to->q_flags)) &&
			 bitset(QPRIMARY, to->q_flags))
		{
			/* RFC 2852, 4.1.4: no NOTIFY, or not NEVER */
			to->q_flags |= QBYTRACE;
			(void) sm_io_fprintf(e->e_xfp, SM_TIME_DEFAULT,
					     "%s... Deliver-By trace: relayed\n",
					     to->q_paddr);
		}
	}

	if (bitnset(M_LMTP, m->m_flags))
	{
		/*
		**  Global information applies to the last recipient only;
		**  clear it out to avoid bogus errors.
		*/

		rcode = EX_OK;
		e->e_statmsg = NULL;

		/* reset the mci state for the next transaction */
		if (mci != NULL &&
		    (mci->mci_state == MCIS_MAIL ||
		     mci->mci_state == MCIS_RCPT ||
		     mci->mci_state == MCIS_DATA))
		{
			mci->mci_state = MCIS_OPEN;
			SmtpPhase = mci->mci_phase = "idle";
			sm_setproctitle(true, e, "%s: %s", CurHostName,
					mci->mci_phase);
		}
	}

	if (tobuf[0] != '\0')
	{
		giveresponse(rcode,
#if _FFR_NULLMX_STATUS
			(NULL == mci || SM_IS_EMPTY(mci->mci_status))
				? NULL :
#endif
				mci->mci_status,
			m, mci, ctladdr, xstart, e, NULL);
#if 0
		/*
		**  This code is disabled for now because I am not
		**  sure that copying status from the first recipient
		**  to all non-status'ed recipients is a good idea.
		*/

		if (tochain->q_message != NULL &&
		    !bitnset(M_LMTP, m->m_flags) && rcode != EX_OK)
		{
			for (to = tochain->q_tchain; to != NULL;
			     to = to->q_tchain)
			{
				/* see if address already marked */
				if (QS_IS_QUEUEUP(to->q_state) &&
				    to->q_message == NULL)
					to->q_message = sm_rpool_strdup_x(e->e_rpool,
							tochain->q_message);
			}
		}
#endif /* 0 */
	}
	if (anyok)
		markstats(e, tochain, STATS_NORMAL);
	mci_store_persistent(mci);

#if _FFR_OCC
	/*
	**  HACK: this is NOT the right place to "close" a connection!
	**  use smtpquit?
	**  add a flag to mci to indicate that rate/conc. was increased?
	*/

	if (clever)
	{
		extern SOCKADDR CurHostAddr;

		/* check family... {} */
		/* r = anynet_pton(AF_INET, p, dst); */
		occ_close(e, mci, host, &CurHostAddr);
	}
#endif /* _FFR_OCC */

	/* Some recipients were tempfailed, try them on the next host */
	if (mci != NULL && mci->mci_retryrcpt && nummxhosts > hostnum)
	{
		logfailover(e, m, mci, rcode, to);
		/* try next MX site */
		goto tryhost;
	}

	/* now close the connection */
	if (clever && mci != NULL && mci->mci_state != MCIS_CLOSED &&
	    !bitset(MCIF_CACHED, mci->mci_flags))
		smtpquit(m, mci, e);

cleanup: ;
	}
	SM_FINALLY
	{
		/*
		**  Restore state and return.
		*/
#if XDEBUG
		char wbuf[MAXLINE];

		/* make absolutely certain 0, 1, and 2 are in use */
		(void) sm_snprintf(wbuf, sizeof(wbuf),
				   "%s... end of deliver(%s)",
				   e->e_to == NULL ? "NO-TO-LIST"
						   : shortenstring(e->e_to,
								   MAXSHORTSTR),
				  m->m_name);
		checkfd012(wbuf);
#endif /* XDEBUG */

		errno = 0;

		/*
		**  It was originally necessary to set macro 'g' to NULL
		**  because it previously pointed to an auto buffer.
		**  We don't do this any more, so this may be unnecessary.
		*/

		macdefine(&e->e_macro, A_PERM, 'g', (char *) NULL);
		e->e_to = NULL;
	}
	SM_END_TRY
	return rcode;
}

/*
**  EX2ENHSC -- return proper enhanced status code for an EX_ code
**
**	Parameters:
**		xcode -- EX_* code
**
**	Returns:
**		enhanced status code if appropriate
**		NULL otherwise
*/

static char *ex2enhsc __P((int));

static char *
ex2enhsc(xcode)
	int xcode;
{
	switch (xcode)
	{
	  case EX_USAGE:
		return "5.5.4";
		break;

	  case EX_DATAERR:
		return "5.5.2";
		break;

	  case EX_NOUSER:
		return "5.1.1";
		break;

	  case EX_NOHOST:
		return "5.1.2";
		break;

	  case EX_NOINPUT:
	  case EX_CANTCREAT:
	  case EX_NOPERM:
		return "5.3.0";
		break;

	  case EX_UNAVAILABLE:
	  case EX_SOFTWARE:
	  case EX_OSFILE:
	  case EX_PROTOCOL:
	  case EX_CONFIG:
		return "5.5.0";
		break;

	  case EX_OSERR:
	  case EX_IOERR:
		return "4.5.0";
		break;

	  case EX_TEMPFAIL:
		return "4.2.0";
		break;
	}
	return NULL;
}

/*
**  MARKFAILURE -- mark a failure on a specific address.
**
**	Parameters:
**		e -- the envelope we are sending.
**		q -- the address to mark.
**		mci -- mailer connection information.
**		rcode -- the code signifying the particular failure.
**		ovr -- override an existing code?
**
**	Returns:
**		none.
**
**	Side Effects:
**		marks the address (and possibly the envelope) with the
**			failure so that an error will be returned or
**			the message will be queued, as appropriate.
*/

void
markfailure(e, q, mci, rcode, ovr)
	register ENVELOPE *e;
	register ADDRESS *q;
	register MCI *mci;
	int rcode;
	bool ovr;
{
	int save_errno = errno;
	char *status = NULL;
	char *rstatus = NULL;

	switch (rcode)
	{
	  case EX_OK:
		break;

	  case EX_TEMPFAIL:
	  case EX_IOERR:
	  case EX_OSERR:
		q->q_state = QS_QUEUEUP;
		break;

	  default:
		q->q_state = QS_BADADDR;
		break;
	}

	/* find most specific error code possible */
	if (mci != NULL && mci->mci_status != NULL)
	{
		status = sm_rpool_strdup_x(e->e_rpool, mci->mci_status);
		if (mci->mci_rstatus != NULL)
			rstatus = sm_rpool_strdup_x(e->e_rpool,
						    mci->mci_rstatus);
	}
	else if (e->e_status != NULL)
		status = e->e_status;
	else
		status = ex2enhsc(rcode);

	/* new status? */
	if (status != NULL && *status != '\0' && (ovr || q->q_status == NULL ||
	    *q->q_status == '\0' || *q->q_status < *status))
	{
		q->q_status = status;
		q->q_rstatus = rstatus;
	}
	if (rcode != EX_OK && q->q_rstatus == NULL &&
	    q->q_mailer != NULL && q->q_mailer->m_diagtype != NULL &&
	    SM_STRCASEEQ(q->q_mailer->m_diagtype, "X-UNIX"))
	{
		char buf[16];

		(void) sm_snprintf(buf, sizeof(buf), "%d", rcode);
		q->q_rstatus = sm_rpool_strdup_x(e->e_rpool, buf);
	}

	q->q_statdate = curtime();
	if (CurHostName != NULL && CurHostName[0] != '\0' &&
	    mci != NULL && !bitset(M_LOCALMAILER, mci->mci_flags))
		q->q_statmta = sm_rpool_strdup_x(e->e_rpool, CurHostName);

	/* restore errno */
	errno = save_errno;
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
**		mci -- the mailer connection info.
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

static jmp_buf	EndWaitTimeout;

static void
endwaittimeout(ignore)
	int ignore;
{
	/*
	**  NOTE: THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
	**	ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
	**	DOING.
	*/

	errno = ETIMEDOUT;
	longjmp(EndWaitTimeout, 1);
}

int
endmailer(mci, e, pv)
	register MCI *mci;
	register ENVELOPE *e;
	char **pv;
{
	int st;
	int save_errno = errno;
	char buf[MAXLINE];
	SM_EVENT *ev = NULL;


	mci_unlock_host(mci);

	/* close output to mailer */
	SM_CLOSE_FP(mci->mci_out);

	/* copy any remaining input to transcript */
	if (mci->mci_in != NULL && mci->mci_state != MCIS_ERROR &&
	    e->e_xfp != NULL)
	{
		while (sfgets(buf, sizeof(buf), mci->mci_in,
			      TimeOuts.to_quit, "Draining Input") != NULL)
			(void) sm_io_fputs(e->e_xfp, SM_TIME_DEFAULT, buf);
	}

#if SASL
	/* close SASL connection */
	if (bitset(MCIF_AUTHACT, mci->mci_flags))
	{
		sasl_dispose(&mci->mci_conn);
		mci->mci_flags &= ~MCIF_AUTHACT;
	}
#endif /* SASL */

#if STARTTLS
	/* shutdown TLS */
	(void) endtlsclt(mci);
#endif

	/* now close the input */
	SM_CLOSE_FP(mci->mci_in);
	mci->mci_state = MCIS_CLOSED;

	errno = save_errno;

	/* in the IPC case there is nothing to wait for */
	if (mci->mci_pid == 0)
		return EX_OK;

	/* put a timeout around the wait */
	if (mci->mci_mailer->m_wait > 0)
	{
		if (setjmp(EndWaitTimeout) == 0)
			ev = sm_setevent(mci->mci_mailer->m_wait,
					 endwaittimeout, 0);
		else
		{
			syserr("endmailer %s: wait timeout (%ld)",
			       mci->mci_mailer->m_name,
			       (long) mci->mci_mailer->m_wait);
			return EX_TEMPFAIL;
		}
	}

	/* wait for the mailer process, collect status */
	st = waitfor(mci->mci_pid);
	save_errno = errno;
	if (ev != NULL)
		sm_clrevent(ev);
	errno = save_errno;

	if (st == -1)
	{
		syserr("endmailer %s: wait", mci->mci_mailer->m_name);
		return EX_SOFTWARE;
	}

	if (WIFEXITED(st))
	{
		/* normal death -- return status */
		return (WEXITSTATUS(st));
	}

	/* it died a horrid death */
	syserr("451 4.3.0 mailer %s died with signal %d%s",
		mci->mci_mailer->m_name, WTERMSIG(st),
		WCOREDUMP(st) ? " (core dumped)" :
		(WIFSTOPPED(st) ? " (stopped)" : ""));

	/* log the arguments */
	if (pv != NULL && e->e_xfp != NULL)
	{
		register char **av;

		(void) sm_io_fprintf(e->e_xfp, SM_TIME_DEFAULT, "Arguments:");
		for (av = pv; *av != NULL; av++)
			(void) sm_io_fprintf(e->e_xfp, SM_TIME_DEFAULT, " %s",
					     *av);
		(void) sm_io_fprintf(e->e_xfp, SM_TIME_DEFAULT, "\n");
	}

	ExitStat = EX_TEMPFAIL;
	return EX_TEMPFAIL;
}
/*
**  GIVERESPONSE -- Interpret an error response from a mailer
**
**	Parameters:
**		status -- the status code from the mailer (high byte
**			only; core dumps must have been taken care of
**			already).
**		dsn -- the DSN associated with the address, if any.
**		m -- the mailer info for this mailer.
**		mci -- the mailer connection info -- can be NULL if the
**			response is given before the connection is made.
**		ctladdr -- the controlling address for the recipient
**			address(es).
**		xstart -- the transaction start time, for computing
**			transaction delays.
**		e -- the current envelope.
**		to -- the current recipient (NULL if none).
**
**	Returns:
**		none.
**
**	Side Effects:
**		Errors may be incremented.
**		ExitStat may be set.
*/

void
giveresponse(status, dsn, m, mci, ctladdr, xstart, e, to)
	int status;
	char *dsn;
	register MAILER *m;
	register MCI *mci;
	ADDRESS *ctladdr;
	time_t xstart;
	ENVELOPE *e;
	ADDRESS *to;
{
	register const char *statmsg;
	int errnum = errno;
	int off = 4;
	bool usestat = false;
	char dsnbuf[ENHSCLEN];
	char buf[MAXLINE];
	char *exmsg;

	if (e == NULL)
	{
		syserr("giveresponse: null envelope");
		/* NOTREACHED */
		SM_ASSERT(0);
	}

	if (tTd(11, 4))
		sm_dprintf("giveresponse: status=%d, e->e_message=%s, dsn=%s, SmtpError=%s\n",
			status, e->e_message, dsn, SmtpError);

	/*
	**  Compute status message from code.
	*/

	exmsg = sm_sysexmsg(status);
	if (status == 0)
	{
		statmsg = "250 2.0.0 Sent";
		if (e->e_statmsg != NULL)
		{
			(void) sm_snprintf(buf, sizeof(buf), "%s (%s)",
					   statmsg,
					   shortenstring(e->e_statmsg, 403));
			statmsg = buf;
		}
	}
	else if (exmsg == NULL)
	{
		(void) sm_snprintf(buf, sizeof(buf),
				   "554 5.3.0 unknown mailer error %d",
				   status);
		status = EX_UNAVAILABLE;
		statmsg = buf;
		usestat = true;
	}
	else if (status == EX_TEMPFAIL)
	{
		char *bp = buf;

		(void) sm_strlcpy(bp, exmsg + 1, SPACELEFT(buf, bp));
		bp += strlen(bp);
#if NAMED_BIND
		if (h_errno == TRY_AGAIN)
			statmsg = sm_errstring(h_errno + E_DNSBASE);
		else
#endif
		{
			if (errnum != 0)
				statmsg = sm_errstring(errnum);
			else
				statmsg = SmtpError;
		}
		if (statmsg != NULL && statmsg[0] != '\0')
		{
			switch (errnum)
			{
#ifdef ENETDOWN
			  case ENETDOWN:	/* Network is down */
#endif
#ifdef ENETUNREACH
			  case ENETUNREACH:	/* Network is unreachable */
#endif
#ifdef ENETRESET
			  case ENETRESET:	/* Network dropped connection on reset */
#endif
#ifdef ECONNABORTED
			  case ECONNABORTED:	/* Software caused connection abort */
#endif
#ifdef EHOSTDOWN
			  case EHOSTDOWN:	/* Host is down */
#endif
#ifdef EHOSTUNREACH
			  case EHOSTUNREACH:	/* No route to host */
#endif
				if (mci != NULL && mci->mci_host != NULL)
				{
					(void) sm_strlcpyn(bp,
							   SPACELEFT(buf, bp),
							   2, ": ",
							   mci->mci_host);
					bp += strlen(bp);
				}
				break;
			}
			(void) sm_strlcpyn(bp, SPACELEFT(buf, bp), 2, ": ",
					   statmsg);
#if DANE
			if (errnum == 0 && SmtpError[0] != '\0' &&
			    h_errno == TRY_AGAIN &&
			    mci->mci_exitstat == EX_TEMPFAIL)
			{
				(void) sm_strlcat(bp, SmtpError,
					SPACELEFT(buf, bp));
				bp += strlen(bp);
			}
#endif /* DANE */
			usestat = true;
		}
		statmsg = buf;
	}
#if NAMED_BIND
	else if (status == EX_NOHOST && h_errno != 0)
	{
		statmsg = sm_errstring(h_errno + E_DNSBASE);
		(void) sm_snprintf(buf, sizeof(buf), "%s (%s)", exmsg + 1,
				   statmsg);
		statmsg = buf;
		usestat = true;
	}
#endif /* NAMED_BIND */
#if USE_EAI
	else if (errnum == 0 && status == EX_DATAERR
		&& e->e_message != NULL && e->e_message[0] != '\0')
	{
		int m;

		/* XREF: 2nd arg must be coordinated with smtpmailfrom() */
		m = skipaddrhost(e->e_message, false);

		/*
		**  XXX Why is the SMTP reply code needed here?
		**  How to avoid a hard-coded value?
		*/

		(void) sm_snprintf(buf, sizeof(buf), "550 %s", e->e_message + m);
		statmsg = buf;
		usestat = true;
	}
#endif /* USE_EAI */
	else
	{
		statmsg = exmsg;
		if (*statmsg++ == ':' && errnum != 0)
		{
			(void) sm_snprintf(buf, sizeof(buf), "%s: %s", statmsg,
					   sm_errstring(errnum));
			statmsg = buf;
			usestat = true;
		}
		else if (bitnset(M_LMTP, m->m_flags) && e->e_statmsg != NULL)
		{
			(void) sm_snprintf(buf, sizeof(buf), "%s (%s)", statmsg,
					   shortenstring(e->e_statmsg, 403));
			statmsg = buf;
			usestat = true;
		}
	}

	/*
	**  Print the message as appropriate
	*/

	if (status == EX_OK || status == EX_TEMPFAIL)
	{
		extern char MsgBuf[];

		if ((off = isenhsc(statmsg + 4, ' ')) > 0)
		{
			if (dsn == NULL)
			{
				(void) sm_snprintf(dsnbuf, sizeof(dsnbuf),
						   "%.*s", off, statmsg + 4);
				dsn = dsnbuf;
			}
			off += 5;
		}
		else
		{
			off = 4;
		}
		message("%s", statmsg + off);
		if (status == EX_TEMPFAIL && e->e_xfp != NULL)
			(void) sm_io_fprintf(e->e_xfp, SM_TIME_DEFAULT, "%s\n",
					     &MsgBuf[4]);
	}
	else
	{
		char mbuf[ENHSCLEN + 4];

		Errors++;
		if ((off = isenhsc(statmsg + 4, ' ')) > 0 &&
		    off < sizeof(mbuf) - 4)
		{
			if (dsn == NULL)
			{
				(void) sm_snprintf(dsnbuf, sizeof(dsnbuf),
						   "%.*s", off, statmsg + 4);
				dsn = dsnbuf;
			}
			off += 5;

			/* copy only part of statmsg to mbuf */
			(void) sm_strlcpy(mbuf, statmsg, off);
			(void) sm_strlcat(mbuf, " %s", sizeof(mbuf));
		}
		else
		{
			dsnbuf[0] = '\0';
			(void) sm_snprintf(mbuf, sizeof(mbuf), "%.3s %%s",
					   statmsg);
			off = 4;
		}
		usrerr(mbuf, &statmsg[off]);
	}

	/*
	**  Final cleanup.
	**	Log a record of the transaction.  Compute the new ExitStat
	**	-- if we already had an error, stick with that.
	*/

	if (OpMode != MD_VERIFY && !bitset(EF_VRFYONLY, e->e_flags) &&
	    LogLevel > ((status == EX_TEMPFAIL) ? 8 : (status == EX_OK) ? 7 : 6))
		logdelivery(m, mci, dsn, statmsg + off, ctladdr, xstart, e, to, status);

	if (tTd(11, 2))
		sm_dprintf("giveresponse: status=%d, dsn=%s, e->e_message=%s, errnum=%d, statmsg=%s\n",
			   status,
			   dsn == NULL ? "<NULL>" : dsn,
			   e->e_message == NULL ? "<NULL>" : e->e_message,
			   errnum, statmsg);

	if (status != EX_TEMPFAIL)
		setstat(status);
	if (status != EX_OK && (status != EX_TEMPFAIL || e->e_message == NULL))
		e->e_message = sm_rpool_strdup_x(e->e_rpool, statmsg + off);
	if (status != EX_OK && to != NULL && to->q_message == NULL)
	{
		if (!usestat && e->e_message != NULL)
			to->q_message = sm_rpool_strdup_x(e->e_rpool,
							  e->e_message);
		else
			to->q_message = sm_rpool_strdup_x(e->e_rpool,
							  statmsg + off);
	}
	errno = 0;
	SM_SET_H_ERRNO(0);
}

#if _FFR_8BITENVADDR
# define GET_HOST_VALUE	\
	(void) dequote_internal_chars(p, xbuf, sizeof(xbuf));	\
	p = xbuf;
#else
# define GET_HOST_VALUE
#endif

/*
**  LOGDELIVERY -- log the delivery in the system log
**
**	Care is taken to avoid logging lines that are too long, because
**	some versions of syslog have an unfortunate proclivity for core
**	dumping.  This is a hack, to be sure, that is at best empirical.
**
**	Parameters:
**		m -- the mailer info.  Can be NULL for initial queue.
**		mci -- the mailer connection info -- can be NULL if the
**			log is occurring when no connection is active.
**		dsn -- the DSN attached to the status.
**		status -- the message to print for the status.
**		ctladdr -- the controlling address for the to list.
**		xstart -- the transaction start time, used for
**			computing transaction delay.
**		e -- the current envelope.
**		to -- the current recipient (NULL if none).
**		rcode -- status code.
**
**	Returns:
**		none
**
**	Side Effects:
**		none
*/

void
logdelivery(m, mci, dsn, status, ctladdr, xstart, e, to, rcode)
	MAILER *m;
	register MCI *mci;
	char *dsn;
	const char *status;
	ADDRESS *ctladdr;
	time_t xstart;
	register ENVELOPE *e;
	ADDRESS *to;
	int rcode;
{
	register char *bp;
	register char *p;
	int l;
	time_t now = curtime();
	char buf[1024];
#if _FFR_8BITENVADDR
	char xbuf[SM_MAX(SYSLOG_BUFSIZE, MAXNAME)];	/* EAI:ok */
#endif
	char *xstr;

#if (SYSLOG_BUFSIZE) >= 256
	int xtype, rtype;

	/* ctladdr: max 106 bytes */
	bp = buf;
	if (ctladdr != NULL)
	{
		(void) sm_strlcpyn(bp, SPACELEFT(buf, bp), 2, ", ctladdr=",
				   shortenstring(ctladdr->q_paddr, 83));
		bp += strlen(bp);
		if (bitset(QGOODUID, ctladdr->q_flags))
		{
			(void) sm_snprintf(bp, SPACELEFT(buf, bp), " (%d/%d)",
					   (int) ctladdr->q_uid,
					   (int) ctladdr->q_gid);
			bp += strlen(bp);
		}
	}

	/* delay & xdelay: max 41 bytes */
	(void) sm_strlcpyn(bp, SPACELEFT(buf, bp), 2, ", delay=",
			   pintvl(now - e->e_ctime, true));
	bp += strlen(bp);

	if (xstart != (time_t) 0)
	{
		(void) sm_strlcpyn(bp, SPACELEFT(buf, bp), 2, ", xdelay=",
				   pintvl(now - xstart, true));
		bp += strlen(bp);
	}

	/* mailer: assume about 19 bytes (max 10 byte mailer name) */
	if (m != NULL)
	{
		(void) sm_strlcpyn(bp, SPACELEFT(buf, bp), 2, ", mailer=",
				   m->m_name);
		bp += strlen(bp);
	}

# if _FFR_LOG_MORE2
	LOG_MORE(buf, bp);
# endif

	/* pri: changes with each delivery attempt */
	(void) sm_snprintf(bp, SPACELEFT(buf, bp), ", pri=%ld",
		PRT_NONNEGL(e->e_msgpriority));
	bp += strlen(bp);

	/* relay: max 66 bytes for IPv4 addresses */
	if (mci != NULL && mci->mci_host != NULL)
	{
		extern SOCKADDR CurHostAddr;

		(void) sm_strlcpyn(bp, SPACELEFT(buf, bp), 2, ", relay=",
				   shortenstring(mci->mci_host, 40));
		bp += strlen(bp);

		if (CurHostAddr.sa.sa_family != 0)
		{
			(void) sm_snprintf(bp, SPACELEFT(buf, bp), " [%s]",
					   anynet_ntoa(&CurHostAddr));
		}
	}
	else if (strcmp(status, "quarantined") == 0)
	{
		if (e->e_quarmsg != NULL)
			(void) sm_snprintf(bp, SPACELEFT(buf, bp),
					   ", quarantine=%s",
					   shortenstring(e->e_quarmsg, 40));
	}
	else if (strcmp(status, "queued") != 0)
	{
		p = macvalue('h', e);
		if (p != NULL && p[0] != '\0')
		{
			GET_HOST_VALUE;
			(void) sm_snprintf(bp, SPACELEFT(buf, bp),
					   ", relay=%s", shortenstring(p, 40));
		}
	}
	bp += strlen(bp);

	p = ex2enhsc(rcode);
	if (p != NULL)
		xtype = p[0] - '0';
	else
		xtype = 0;

#ifndef WHERE2REPORT
# define WHERE2REPORT "please see <https://sendmail.org/Report>, "
#endif

	/* dsn */
	if (dsn != NULL && *dsn != '\0')
	{
		(void) sm_strlcpyn(bp, SPACELEFT(buf, bp), 2, ", dsn=",
				   shortenstring(dsn, ENHSCLEN));
		bp += strlen(bp);
		if (xtype > 0 && ISSMTPCODE(dsn) &&
		    (rtype = dsn[0] - '0') > 0 && rtype != xtype)
			sm_syslog(LOG_ERR, e->e_id,
				"ERROR: inconsistent dsn, %srcode=%d, dsn=%s",
				WHERE2REPORT, rcode, dsn);
	}

# if _FFR_LOG_NTRIES
	/* ntries */
	if (e->e_ntries >= 0)
	{
		(void) sm_snprintf(bp, SPACELEFT(buf, bp),
				   ", ntries=%d", e->e_ntries + 1);
		bp += strlen(bp);
	}
# endif /* _FFR_LOG_NTRIES */

# define STATLEN		(((SYSLOG_BUFSIZE) - 100) / 4)
# if (STATLEN) < 63
#  undef STATLEN
#  define STATLEN	63
# endif
# if (STATLEN) > 203
#  undef STATLEN
#  define STATLEN	203
# endif

# if _FFR_LOG_STAGE
	/* only do this when reply= is logged? */
	if (rcode != EX_OK && e->e_estate >= 0)
	{
		if (e->e_estate >= 0 && e->e_estate < SM_ARRAY_SIZE(xs_states))
			(void) sm_snprintf(bp, SPACELEFT(buf, bp),
				", stage=%s", xs_states[e->e_estate]);
		else
			(void) sm_snprintf(bp, SPACELEFT(buf, bp),
				", stage=%d", e->e_estate);
		bp += strlen(bp);
	}
# endif /* _FFR_LOG_STAGE */

	/*
	**  Notes:
	**  per-rcpt status: to->q_rstatus
	**  global status: e->e_text
	**
	**  We (re)use STATLEN here, is that a good choice?
	**
	**  stat=Deferred: ...
	**  has sometimes the same text?
	**
	**  Note: in some case the normal logging might show the same server
	**  reply - how to avoid that?
	*/

	/* only show errors from server */
	if (rcode != EX_OK && (NULL == to || !bitset(QINTREPLY, to->q_flags)))
	{
		if (to != NULL && !SM_IS_EMPTY(to->q_rstatus))
		{
			(void) sm_snprintf(bp, SPACELEFT(buf, bp),
				", reply=%s",
				shortenstring(to->q_rstatus, STATLEN));
			bp += strlen(bp);
			if (ISSMTPCODE(to->q_rstatus) &&
			    (rtype = to->q_rstatus[0] - '0') > 0 &&
			    ((xtype > 0 && rtype != xtype) || rtype < 4))
				sm_syslog(LOG_ERR, e->e_id,
					"ERROR: inconsistent reply, %srcode=%d, q_rstatus=%s",
					WHERE2REPORT, rcode, to->q_rstatus);
		}
		else if (e->e_text != NULL)
		{
			(void) sm_snprintf(bp, SPACELEFT(buf, bp),
				", reply=%d %s%s%s",
				e->e_rcode,
				e->e_renhsc,
				(e->e_renhsc[0] != '\0') ? " " : "",
				shortenstring(e->e_text, STATLEN));
			bp += strlen(bp);
			rtype = REPLYTYPE(e->e_rcode);
			if (rtype > 0 &&
			    ((xtype > 0 && rtype != xtype) || rtype < 4))
				sm_syslog(LOG_ERR, e->e_id,
					"ERROR: inconsistent reply, %srcode=%d, e_rcode=%d, e_text=%s",
					WHERE2REPORT, rcode, e->e_rcode, e->e_text);
		}
#if _FFR_NULLMX_STATUS
		/* Hack for MX 0 . : how to make this general? */
		else if (NULL == to && dsn != NULL &&
			 strcmp(dsn, ESCNULLMXRCPT) == 0)
		{
			(void) sm_snprintf(bp, SPACELEFT(buf, bp),
				", status=%s", ERRNULLMX);
			bp += strlen(bp);
		}
#endif
	}

	/* stat: max 210 bytes */
	if ((bp - buf) > (sizeof(buf) - ((STATLEN) + 20)))
	{
		/* desperation move -- truncate data */
		bp = buf + sizeof(buf) - ((STATLEN) + 17);
		(void) sm_strlcpy(bp, "...", SPACELEFT(buf, bp));
		bp += 3;
	}

	(void) sm_strlcpy(bp, ", stat=", SPACELEFT(buf, bp));
	bp += strlen(bp);

	(void) sm_strlcpy(bp, shortenstring(status, STATLEN),
			  SPACELEFT(buf, bp));

	/* id, to: max 13 + TOBUFSIZE bytes */
	l = SYSLOG_BUFSIZE - 100 - strlen(buf);
	if (l < 0)
		l = 0;
	p = e->e_to == NULL ? "NO-TO-LIST" : e->e_to;
	while (strlen(p) >= l)
	{
		register char *q;

		for (q = p + l; q > p; q--)
		{
			/* XXX a comma in an address will break this! */
			if (*q == ',')
				break;
		}
		if (p == q)
			break;
# if _FFR_8BITENVADDR
		/* XXX is this correct? dequote all of p? */
		(void) dequote_internal_chars(p, xbuf, sizeof(xbuf));
		xstr = xbuf;
# else
		xstr = p;
# endif
		sm_syslog(LOG_INFO, e->e_id, "to=%.*s [more]%s",
			  (int) (++q - p), xstr, buf);
		p = q;
	}
# if _FFR_8BITENVADDR
	(void) dequote_internal_chars(p, xbuf, sizeof(xbuf));
	xstr = xbuf;
# else
	xstr = p;
# endif
	sm_syslog(LOG_INFO, e->e_id, "to=%.*s%s", l, xstr, buf);

#else /* (SYSLOG_BUFSIZE) >= 256 */

	l = SYSLOG_BUFSIZE - 85;
	if (l < 0)
		l = 0;
	p = e->e_to == NULL ? "NO-TO-LIST" : e->e_to;
	while (strlen(p) >= l)
	{
		register char *q;

		for (q = p + l; q > p; q--)
		{
			if (*q == ',')
				break;
		}
		if (p == q)
			break;

		sm_syslog(LOG_INFO, e->e_id, "to=%.*s [more]",
			  (int) (++q - p), p);
		p = q;
	}
	sm_syslog(LOG_INFO, e->e_id, "to=%.*s", l, p);

	if (ctladdr != NULL)
	{
		bp = buf;
		(void) sm_strlcpyn(bp, SPACELEFT(buf, bp), 2, "ctladdr=",
				   shortenstring(ctladdr->q_paddr, 83));
		bp += strlen(bp);
		if (bitset(QGOODUID, ctladdr->q_flags))
		{
			(void) sm_snprintf(bp, SPACELEFT(buf, bp), " (%d/%d)",
					   ctladdr->q_uid, ctladdr->q_gid);
			bp += strlen(bp);
		}
		sm_syslog(LOG_INFO, e->e_id, "%s", buf);
	}
	bp = buf;
	(void) sm_strlcpyn(bp, SPACELEFT(buf, bp), 2, "delay=",
			   pintvl(now - e->e_ctime, true));
	bp += strlen(bp);
	if (xstart != (time_t) 0)
	{
		(void) sm_strlcpyn(bp, SPACELEFT(buf, bp), 2, ", xdelay=",
				   pintvl(now - xstart, true));
		bp += strlen(bp);
	}

	if (m != NULL)
	{
		(void) sm_strlcpyn(bp, SPACELEFT(buf, bp), 2, ", mailer=",
				   m->m_name);
		bp += strlen(bp);
	}
	sm_syslog(LOG_INFO, e->e_id, "%.1000s", buf);

	buf[0] = '\0';
	bp = buf;
	if (mci != NULL && mci->mci_host != NULL)
	{
		extern SOCKADDR CurHostAddr;

		(void) sm_snprintf(bp, SPACELEFT(buf, bp), "relay=%.100s",
				   mci->mci_host);
		bp += strlen(bp);

		if (CurHostAddr.sa.sa_family != 0)
			(void) sm_snprintf(bp, SPACELEFT(buf, bp),
					   " [%.100s]",
					   anynet_ntoa(&CurHostAddr));
	}
	else if (strcmp(status, "quarantined") == 0)
	{
		if (e->e_quarmsg != NULL)
			(void) sm_snprintf(bp, SPACELEFT(buf, bp),
					   ", quarantine=%.100s",
					   e->e_quarmsg);
	}
	else if (strcmp(status, "queued") != 0)
	{
		p = macvalue('h', e);
		if (p != NULL && p[0] != '\0')
		{
			GET_HOST_VALUE;
			(void) sm_snprintf(buf, sizeof(buf), "relay=%.100s", p);
		}
	}
	if (buf[0] != '\0')
		sm_syslog(LOG_INFO, e->e_id, "%.1000s", buf);

	sm_syslog(LOG_INFO, e->e_id, "stat=%s", shortenstring(status, 63));
#endif /* (SYSLOG_BUFSIZE) >= 256 */
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
**		mci -- the connection information.
**		e -- the envelope.
**
**	Returns:
**		true iff line was written successfully
**
**	Side Effects:
**		outputs some text to fp.
*/

bool
putfromline(mci, e)
	register MCI *mci;
	ENVELOPE *e;
{
	char *template = UnixFromLine;
	char buf[MAXLINE];
	char xbuf[MAXLINE];

	if (bitnset(M_NHDR, mci->mci_mailer->m_flags))
		return true;

	mci->mci_flags |= MCIF_INHEADER;

	if (bitnset(M_UGLYUUCP, mci->mci_mailer->m_flags))
	{
		char *bang;

		expand("\201g", buf, sizeof(buf), e);
		bang = strchr(buf, '!');
		if (bang == NULL)
		{
			char *at;
			char hname[MAXNAME];	/* EAI:ok */

			/*
			**  If we can construct a UUCP path, do so
			*/

			at = strrchr(buf, '@');
			if (at == NULL)
			{
				expand("\201k", hname, sizeof(hname), e);
				at = hname;
			}
			else
				*at++ = '\0';
			(void) sm_snprintf(xbuf, sizeof(xbuf),
					   "From %.800s  \201d remote from %.100s\n",
					   buf, at);
		}
		else
		{
			*bang++ = '\0';
			(void) sm_snprintf(xbuf, sizeof(xbuf),
					   "From %.800s  \201d remote from %.100s\n",
					   bang, buf);
			template = xbuf;
		}
	}
	expand(template, buf, sizeof(buf), e);
	return putxline(buf, strlen(buf), mci, PXLF_HEADER);
}

/*
**  PUTBODY -- put the body of a message.
**
**	Parameters:
**		mci -- the connection information.
**		e -- the envelope to put out.
**		separator -- if non-NULL, a message separator that must
**			not be permitted in the resulting message.
**
**	Returns:
**		true iff message was written successfully
**
**	Side Effects:
**		The message is written onto fp.
*/

/* values for output state variable */
#define OSTATE_HEAD	0	/* at beginning of line */
#define OSTATE_CR	1	/* read a carriage return */
#define OSTATE_INLINE	2	/* putting rest of line */

bool
putbody(mci, e, separator)
	register MCI *mci;
	register ENVELOPE *e;
	char *separator;
{
	bool dead = false;
	bool ioerr = false;
	int save_errno;
	char buf[MAXLINE];
#if MIME8TO7
	char *boundaries[MAXMIMENESTING + 1];
#endif

	/*
	**  Output the body of the message
	*/

	if (e->e_dfp == NULL && bitset(EF_HAS_DF, e->e_flags))
	{
		char *df = queuename(e, DATAFL_LETTER);

		e->e_dfp = sm_io_open(SmFtStdio, SM_TIME_DEFAULT, df,
				      SM_IO_RDONLY_B, NULL);
		if (e->e_dfp == NULL)
		{
			char *msg = "!putbody: Cannot open %s for %s from %s";

			if (errno == ENOENT)
				msg++;
			syserr(msg, df, e->e_to, e->e_from.q_paddr);
		}

	}
	if (e->e_dfp == NULL)
	{
		if (bitset(MCIF_INHEADER, mci->mci_flags))
		{
			if (!putline("", mci))
				goto writeerr;
			mci->mci_flags &= ~MCIF_INHEADER;
		}
		if (!putline("<<< No Message Collected >>>", mci))
			goto writeerr;
		goto endofmessage;
	}

	if (e->e_dfino == (ino_t) 0)
	{
		struct stat stbuf;

		if (fstat(sm_io_getinfo(e->e_dfp, SM_IO_WHAT_FD, NULL), &stbuf)
		    < 0)
			e->e_dfino = -1;
		else
		{
			e->e_dfdev = stbuf.st_dev;
			e->e_dfino = stbuf.st_ino;
		}
	}

	/* paranoia: the data file should always be in a rewound state */
	(void) bfrewind(e->e_dfp);

	/* simulate an I/O timeout when used as source */
	if (tTd(84, 101))
		sleep(319);

#if MIME8TO7
	if (bitset(MCIF_CVT8TO7, mci->mci_flags))
	{
		/*
		**  Do 8 to 7 bit MIME conversion.
		*/

		/* make sure it looks like a MIME message */
		if (hvalue("MIME-Version", e->e_header) == NULL &&
		    !putline("MIME-Version: 1.0", mci))
			goto writeerr;

		if (hvalue("Content-Type", e->e_header) == NULL)
		{
			(void) sm_snprintf(buf, sizeof(buf),
					   "Content-Type: text/plain; charset=%s",
					   defcharset(e));
			if (!putline(buf, mci))
				goto writeerr;
		}

		/* now do the hard work */
		boundaries[0] = NULL;
		mci->mci_flags |= MCIF_INHEADER;
		if (mime8to7(mci, e->e_header, e, boundaries, M87F_OUTER, 0) ==
								SM_IO_EOF)
			goto writeerr;
	}
# if MIME7TO8
	else if (bitset(MCIF_CVT7TO8, mci->mci_flags))
	{
		if (!mime7to8(mci, e->e_header, e))
			goto writeerr;
	}
# endif /* MIME7TO8 */
	else if (MaxMimeHeaderLength > 0 || MaxMimeFieldLength > 0)
	{
		bool oldsuprerrs = SuprErrs;

		/* Use mime8to7 to check multipart for MIME header overflows */
		boundaries[0] = NULL;
		mci->mci_flags |= MCIF_INHEADER;

		/*
		**  If EF_DONT_MIME is set, we have a broken MIME message
		**  and don't want to generate a new bounce message whose
		**  body propagates the broken MIME.  We can't just not call
		**  mime8to7() as is done above since we need the security
		**  checks.  The best we can do is suppress the errors.
		*/

		if (bitset(EF_DONT_MIME, e->e_flags))
			SuprErrs = true;

		if (mime8to7(mci, e->e_header, e, boundaries,
				M87F_OUTER|M87F_NO8TO7, 0) == SM_IO_EOF)
			goto writeerr;

		/* restore SuprErrs */
		SuprErrs = oldsuprerrs;
	}
	else
#endif /* MIME8TO7 */
	{
		int ostate;
		register char *bp;
		register char *pbp;
		register int c;
		register char *xp;
		int padc;
		char *buflim;
		int pos = 0;
		char peekbuf[12];

		if (bitset(MCIF_INHEADER, mci->mci_flags))
		{
			if (!putline("", mci))
				goto writeerr;
			mci->mci_flags &= ~MCIF_INHEADER;
		}

		/* determine end of buffer; allow for short mailer lines */
		buflim = &buf[sizeof(buf) - 1];
		if (mci->mci_mailer->m_linelimit > 0 &&
		    mci->mci_mailer->m_linelimit < sizeof(buf) - 1)
			buflim = &buf[mci->mci_mailer->m_linelimit - 1];

		/* copy temp file to output with mapping */
		ostate = OSTATE_HEAD;
		bp = buf;
		pbp = peekbuf;
		while (!sm_io_error(mci->mci_out) && !dead)
		{
			if (pbp > peekbuf)
				c = *--pbp;
			else if ((c = sm_io_getc(e->e_dfp, SM_TIME_DEFAULT))
				 == SM_IO_EOF)
				break;
			if (bitset(MCIF_7BIT, mci->mci_flags))
				c &= 0x7f;
			switch (ostate)
			{
			  case OSTATE_HEAD:
				if (c == '\0' &&
				    bitnset(M_NONULLS,
					    mci->mci_mailer->m_flags))
					break;
				if (c != '\r' && c != '\n' && bp < buflim)
				{
					*bp++ = c;
					break;
				}

				/* check beginning of line for special cases */
				*bp = '\0';
				pos = 0;
				padc = SM_IO_EOF;
				if (buf[0] == 'F' &&
				    bitnset(M_ESCFROM, mci->mci_mailer->m_flags)
				    && strncmp(buf, "From ", 5) == 0)
				{
					padc = '>';
				}
				if (buf[0] == '-' && buf[1] == '-' &&
				    separator != NULL)
				{
					/* possible separator */
					int sl = strlen(separator);

					if (strncmp(&buf[2], separator, sl)
					    == 0)
						padc = ' ';
				}
				if (buf[0] == '.' &&
				    bitnset(M_XDOT, mci->mci_mailer->m_flags))
				{
					padc = '.';
				}

				/* now copy out saved line */
				if (TrafficLogFile != NULL)
				{
					(void) sm_io_fprintf(TrafficLogFile,
							     SM_TIME_DEFAULT,
							     "%05d >>> ",
							     (int) CurrentPid);
					if (padc != SM_IO_EOF)
						(void) sm_io_putc(TrafficLogFile,
								  SM_TIME_DEFAULT,
								  padc);
					for (xp = buf; xp < bp; xp++)
						(void) sm_io_putc(TrafficLogFile,
								  SM_TIME_DEFAULT,
								  (unsigned char) *xp);
					if (c == '\n')
						(void) sm_io_fputs(TrafficLogFile,
								   SM_TIME_DEFAULT,
								   mci->mci_mailer->m_eol);
				}
				if (padc != SM_IO_EOF)
				{
					if (sm_io_putc(mci->mci_out,
						       SM_TIME_DEFAULT, padc)
					    == SM_IO_EOF)
					{
						dead = true;
						continue;
					}
					pos++;
				}
				for (xp = buf; xp < bp; xp++)
				{
					if (sm_io_putc(mci->mci_out,
						       SM_TIME_DEFAULT,
						       (unsigned char) *xp)
					    == SM_IO_EOF)
					{
						dead = true;
						break;
					}
				}
				if (dead)
					continue;
				if (c == '\n')
				{
					if (sm_io_fputs(mci->mci_out,
							SM_TIME_DEFAULT,
							mci->mci_mailer->m_eol)
							== SM_IO_EOF)
						break;
					pos = 0;
				}
				else
				{
					pos += bp - buf;
					if (c != '\r')
					{
						SM_ASSERT(pbp < peekbuf +
								sizeof(peekbuf));
						*pbp++ = c;
					}
				}

				bp = buf;

				/* determine next state */
				if (c == '\n')
					ostate = OSTATE_HEAD;
				else if (c == '\r')
					ostate = OSTATE_CR;
				else
					ostate = OSTATE_INLINE;
				continue;

			  case OSTATE_CR:
				if (c == '\n')
				{
					/* got CRLF */
					if (sm_io_fputs(mci->mci_out,
							SM_TIME_DEFAULT,
							mci->mci_mailer->m_eol)
							== SM_IO_EOF)
						continue;

					if (TrafficLogFile != NULL)
					{
						(void) sm_io_fputs(TrafficLogFile,
								   SM_TIME_DEFAULT,
								   mci->mci_mailer->m_eol);
					}
					pos = 0;
					ostate = OSTATE_HEAD;
					continue;
				}

				/* had a naked carriage return */
				SM_ASSERT(pbp < peekbuf + sizeof(peekbuf));
				*pbp++ = c;
				c = '\r';
				ostate = OSTATE_INLINE;
				goto putch;

			  case OSTATE_INLINE:
				if (c == '\r')
				{
					ostate = OSTATE_CR;
					continue;
				}
				if (c == '\0' &&
				    bitnset(M_NONULLS,
					    mci->mci_mailer->m_flags))
					break;
putch:
				if (mci->mci_mailer->m_linelimit > 0 &&
				    pos >= mci->mci_mailer->m_linelimit - 1 &&
				    c != '\n')
				{
					int d;

					/* check next character for EOL */
					if (pbp > peekbuf)
						d = *(pbp - 1);
					else if ((d = sm_io_getc(e->e_dfp,
								 SM_TIME_DEFAULT))
						 != SM_IO_EOF)
					{
						SM_ASSERT(pbp < peekbuf +
								sizeof(peekbuf));
						*pbp++ = d;
					}

					if (d == '\n' || d == SM_IO_EOF)
					{
						if (TrafficLogFile != NULL)
							(void) sm_io_putc(TrafficLogFile,
									  SM_TIME_DEFAULT,
									  (unsigned char) c);
						if (sm_io_putc(mci->mci_out,
							       SM_TIME_DEFAULT,
							       (unsigned char) c)
							       == SM_IO_EOF)
						{
							dead = true;
							continue;
						}
						pos++;
						continue;
					}

					if (sm_io_putc(mci->mci_out,
						       SM_TIME_DEFAULT, '!')
					    == SM_IO_EOF ||
					    sm_io_fputs(mci->mci_out,
							SM_TIME_DEFAULT,
							mci->mci_mailer->m_eol)
					    == SM_IO_EOF)
					{
						dead = true;
						continue;
					}

					if (TrafficLogFile != NULL)
					{
						(void) sm_io_fprintf(TrafficLogFile,
								     SM_TIME_DEFAULT,
								     "!%s",
								     mci->mci_mailer->m_eol);
					}
					ostate = OSTATE_HEAD;
					SM_ASSERT(pbp < peekbuf +
							sizeof(peekbuf));
					*pbp++ = c;
					continue;
				}
				if (c == '\n')
				{
					if (TrafficLogFile != NULL)
						(void) sm_io_fputs(TrafficLogFile,
								   SM_TIME_DEFAULT,
								   mci->mci_mailer->m_eol);
					if (sm_io_fputs(mci->mci_out,
							SM_TIME_DEFAULT,
							mci->mci_mailer->m_eol)
							== SM_IO_EOF)
						continue;
					pos = 0;
					ostate = OSTATE_HEAD;
				}
				else
				{
					if (TrafficLogFile != NULL)
						(void) sm_io_putc(TrafficLogFile,
								  SM_TIME_DEFAULT,
								  (unsigned char) c);
					if (sm_io_putc(mci->mci_out,
						       SM_TIME_DEFAULT,
						       (unsigned char) c)
					    == SM_IO_EOF)
					{
						dead = true;
						continue;
					}
					pos++;
					ostate = OSTATE_INLINE;
				}
				break;
			}
		}

		/* make sure we are at the beginning of a line */
		if (bp > buf)
		{
			if (TrafficLogFile != NULL)
			{
				for (xp = buf; xp < bp; xp++)
					(void) sm_io_putc(TrafficLogFile,
							  SM_TIME_DEFAULT,
							  (unsigned char) *xp);
			}
			for (xp = buf; xp < bp; xp++)
			{
				if (sm_io_putc(mci->mci_out, SM_TIME_DEFAULT,
					       (unsigned char) *xp)
				    == SM_IO_EOF)
				{
					dead = true;
					break;
				}
			}
			pos += bp - buf;
		}
		if (!dead && pos > 0)
		{
			if (TrafficLogFile != NULL)
				(void) sm_io_fputs(TrafficLogFile,
						   SM_TIME_DEFAULT,
						   mci->mci_mailer->m_eol);
			if (sm_io_fputs(mci->mci_out, SM_TIME_DEFAULT,
					   mci->mci_mailer->m_eol) == SM_IO_EOF)
				goto writeerr;
		}
	}

	if (sm_io_error(e->e_dfp))
	{
		syserr("putbody: %s/%cf%s: read error",
		       qid_printqueue(e->e_dfqgrp, e->e_dfqdir),
		       DATAFL_LETTER, e->e_id);
		ExitStat = EX_IOERR;
		ioerr = true;
	}

endofmessage:
	/*
	**  Since mailfile() uses e_dfp in a child process,
	**  the file offset in the stdio library for the
	**  parent process will not agree with the in-kernel
	**  file offset since the file descriptor is shared
	**  between the processes.  Therefore, it is vital
	**  that the file always be rewound.  This forces the
	**  kernel offset (lseek) and stdio library (ftell)
	**  offset to match.
	*/

	save_errno = errno;
	if (e->e_dfp != NULL)
		(void) bfrewind(e->e_dfp);

	/* some mailers want extra blank line at end of message */
	if (!dead && bitnset(M_BLANKEND, mci->mci_mailer->m_flags) &&
	    buf[0] != '\0' && buf[0] != '\n')
	{
		if (!putline("", mci))
			goto writeerr;
	}

	if (!dead &&
	    (sm_io_flush(mci->mci_out, SM_TIME_DEFAULT) == SM_IO_EOF ||
	     (sm_io_error(mci->mci_out) && errno != EPIPE)))
	{
		save_errno = errno;
		syserr("putbody: write error");
		ExitStat = EX_IOERR;
		ioerr = true;
	}

	errno = save_errno;
	return !dead && !ioerr;

  writeerr:
	return false;
}

/*
**  MAILFILE -- Send a message to a file.
**
**	If the file has the set-user-ID/set-group-ID bits set, but NO
**	execute bits, sendmail will try to become the owner of that file
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
**		mailer -- mailer definition for recipient -- if NULL,
**			use FileMailer.
**		ctladdr -- the controlling address header -- includes
**			the userid/groupid to be when sending.
**		sfflags -- flags for opening.
**		e -- the current envelope.
**
**	Returns:
**		The exit code associated with the operation.
**
**	Side Effects:
**		none.
*/

# define RETURN(st)			exit(st);

static jmp_buf	CtxMailfileTimeout;

int
mailfile(filename, mailer, ctladdr, sfflags, e)
	char *volatile filename;
	MAILER *volatile mailer;
	ADDRESS *ctladdr;
	volatile long sfflags;
	register ENVELOPE *e;
{
	register SM_FILE_T *f;
	register pid_t pid = -1;
	volatile int mode;
	int len;
	off_t curoff;
	bool suidwarn = geteuid() == 0;
	char *p;
	char *volatile realfile;
	SM_EVENT *ev;
	char buf[MAXPATHLEN];
	char targetfile[MAXPATHLEN];

	if (tTd(11, 1))
	{
		sm_dprintf("mailfile %s\n  ctladdr=", filename);
		printaddr(sm_debug_file(), ctladdr, false);
	}

	if (mailer == NULL)
		mailer = FileMailer;

	if (e->e_xfp != NULL)
		(void) sm_io_flush(e->e_xfp, SM_TIME_DEFAULT);

	/*
	**  Special case /dev/null.  This allows us to restrict file
	**  delivery to regular files only.
	*/

	if (sm_path_isdevnull(filename))
		return EX_OK;

	/* check for 8-bit available */
	if (bitset(EF_HAS8BIT, e->e_flags) &&
	    bitnset(M_7BITS, mailer->m_flags) &&
	    (bitset(EF_DONT_MIME, e->e_flags) ||
	     !(bitset(MM_MIME8BIT, MimeMode) ||
	       (bitset(EF_IS_MIME, e->e_flags) &&
		bitset(MM_CVTMIME, MimeMode)))))
	{
		e->e_status = "5.6.3";
		usrerrenh(e->e_status,
			  "554 Cannot send 8-bit data to 7-bit destination");
		errno = 0;
		return EX_DATAERR;
	}

	/* Find the actual file */
	if (SafeFileEnv != NULL && SafeFileEnv[0] != '\0')
	{
		len = strlen(SafeFileEnv);

		if (strncmp(SafeFileEnv, filename, len) == 0)
			filename += len;

		if (len + strlen(filename) + 1 >= sizeof(targetfile))
		{
			syserr("mailfile: filename too long (%s/%s)",
			       SafeFileEnv, filename);
			return EX_CANTCREAT;
		}
		(void) sm_strlcpy(targetfile, SafeFileEnv, sizeof(targetfile));
		realfile = targetfile + len;
		if (*filename == '/')
			filename++;
		if (*filename != '\0')
		{
			/* paranoia: trailing / should be removed in readcf */
			if (targetfile[len - 1] != '/')
				(void) sm_strlcat(targetfile,
						  "/", sizeof(targetfile));
			(void) sm_strlcat(targetfile, filename,
					  sizeof(targetfile));
		}
	}
	else if (mailer->m_rootdir != NULL)
	{
		expand(mailer->m_rootdir, targetfile, sizeof(targetfile), e);
		len = strlen(targetfile);

		if (strncmp(targetfile, filename, len) == 0)
			filename += len;

		if (len + strlen(filename) + 1 >= sizeof(targetfile))
		{
			syserr("mailfile: filename too long (%s/%s)",
			       targetfile, filename);
			return EX_CANTCREAT;
		}
		realfile = targetfile + len;
		if (targetfile[len - 1] != '/')
			(void) sm_strlcat(targetfile, "/", sizeof(targetfile));
		if (*filename == '/')
			(void) sm_strlcat(targetfile, filename + 1,
					  sizeof(targetfile));
		else
			(void) sm_strlcat(targetfile, filename,
					  sizeof(targetfile));
	}
	else
	{
		if (sm_strlcpy(targetfile, filename, sizeof(targetfile)) >=
		    sizeof(targetfile))
		{
			syserr("mailfile: filename too long (%s)", filename);
			return EX_CANTCREAT;
		}
		realfile = targetfile;
	}

	/*
	**  Fork so we can change permissions here.
	**	Note that we MUST use fork, not vfork, because of
	**	the complications of calling subroutines, etc.
	*/


	/*
	**  Dispose of SIGCHLD signal catchers that may be laying
	**  around so that the waitfor() below will get it.
	*/

	(void) sm_signal(SIGCHLD, SIG_DFL);

	DOFORK(fork);

	if (pid < 0)
		return EX_OSERR;
	else if (pid == 0)
	{
		/* child -- actually write to file */
		struct stat stb;
		MCI mcibuf;
		int err;
		volatile int oflags = O_WRONLY|O_APPEND;

		/* Reset global flags */
		RestartRequest = NULL;
		RestartWorkGroup = false;
		ShutdownRequest = NULL;
		PendingSignal = 0;
		CurrentPid = getpid();

		if (e->e_lockfp != NULL)
		{
			int fd;

			fd = sm_io_getinfo(e->e_lockfp, SM_IO_WHAT_FD, NULL);
			/* SM_ASSERT(fd >= 0); */
			if (fd >= 0)
				(void) close(fd);
		}

		(void) sm_signal(SIGINT, SIG_DFL);
		(void) sm_signal(SIGHUP, SIG_DFL);
		(void) sm_signal(SIGTERM, SIG_DFL);
		(void) umask(OldUmask);
		e->e_to = filename;
		ExitStat = EX_OK;

		if (setjmp(CtxMailfileTimeout) != 0)
		{
			RETURN(EX_TEMPFAIL);
		}

		if (TimeOuts.to_fileopen > 0)
			ev = sm_setevent(TimeOuts.to_fileopen, mailfiletimeout,
					 0);
		else
			ev = NULL;

		/* check file mode to see if set-user-ID */
		if (stat(targetfile, &stb) < 0)
			mode = FileMode;
		else
			mode = stb.st_mode;

		/* limit the errors to those actually caused in the child */
		errno = 0;
		ExitStat = EX_OK;

		/* Allow alias expansions to use the S_IS{U,G}ID bits */
		if ((ctladdr != NULL && !bitset(QALIAS, ctladdr->q_flags)) ||
		    bitset(SFF_RUNASREALUID, sfflags))
		{
			/* ignore set-user-ID and set-group-ID bits */
			mode &= ~(S_ISGID|S_ISUID);
			if (tTd(11, 20))
				sm_dprintf("mailfile: ignoring set-user-ID/set-group-ID bits\n");
		}

		/* we have to open the data file BEFORE setuid() */
		if (e->e_dfp == NULL && bitset(EF_HAS_DF, e->e_flags))
		{
			char *df = queuename(e, DATAFL_LETTER);

			e->e_dfp = sm_io_open(SmFtStdio, SM_TIME_DEFAULT, df,
					      SM_IO_RDONLY_B, NULL);
			if (e->e_dfp == NULL)
			{
				syserr("mailfile: Cannot open %s for %s from %s",
					df, e->e_to, e->e_from.q_paddr);
			}
		}

		/* select a new user to run as */
		if (!bitset(SFF_RUNASREALUID, sfflags))
		{
			if (bitnset(M_SPECIFIC_UID, mailer->m_flags))
			{
				RealUserName = NULL;
				if (mailer->m_uid == NO_UID)
					RealUid = RunAsUid;
				else
					RealUid = mailer->m_uid;
				if (RunAsUid != 0 && RealUid != RunAsUid)
				{
					/* Only root can change the uid */
					syserr("mailfile: insufficient privileges to change uid, RunAsUid=%ld, RealUid=%ld",
						(long) RunAsUid, (long) RealUid);
					RETURN(EX_TEMPFAIL);
				}
			}
			else if (bitset(S_ISUID, mode))
			{
				RealUserName = NULL;
				RealUid = stb.st_uid;
			}
			else if (ctladdr != NULL && ctladdr->q_uid != 0)
			{
				if (ctladdr->q_ruser != NULL)
					RealUserName = ctladdr->q_ruser;
				else
					RealUserName = ctladdr->q_user;
				RealUid = ctladdr->q_uid;
			}
			else if (mailer != NULL && mailer->m_uid != NO_UID)
			{
				RealUserName = DefUser;
				RealUid = mailer->m_uid;
			}
			else
			{
				RealUserName = DefUser;
				RealUid = DefUid;
			}

			/* select a new group to run as */
			if (bitnset(M_SPECIFIC_UID, mailer->m_flags))
			{
				if (mailer->m_gid == NO_GID)
					RealGid = RunAsGid;
				else
					RealGid = mailer->m_gid;
				if (RunAsUid != 0 &&
				    (RealGid != getgid() ||
				     RealGid != getegid()))
				{
					/* Only root can change the gid */
					syserr("mailfile: insufficient privileges to change gid, RealGid=%ld, RunAsUid=%ld, gid=%ld, egid=%ld",
					       (long) RealGid, (long) RunAsUid,
					       (long) getgid(), (long) getegid());
					RETURN(EX_TEMPFAIL);
				}
			}
			else if (bitset(S_ISGID, mode))
				RealGid = stb.st_gid;
			else if (ctladdr != NULL &&
				 ctladdr->q_uid == DefUid &&
				 ctladdr->q_gid == 0)
			{
				/*
				**  Special case:  This means it is an
				**  alias and we should act as DefaultUser.
				**  See alias()'s comments.
				*/

				RealGid = DefGid;
				RealUserName = DefUser;
			}
			else if (ctladdr != NULL && ctladdr->q_uid != 0)
				RealGid = ctladdr->q_gid;
			else if (mailer != NULL && mailer->m_gid != NO_GID)
				RealGid = mailer->m_gid;
			else
				RealGid = DefGid;
		}

		/* last ditch */
		if (!bitset(SFF_ROOTOK, sfflags))
		{
			if (RealUid == 0)
				RealUid = DefUid;
			if (RealGid == 0)
				RealGid = DefGid;
		}

		/* set group id list (needs /etc/group access) */
		if (RealUserName != NULL && !DontInitGroups)
		{
			if (initgroups(RealUserName, RealGid) == -1 && suidwarn)
			{
				syserr("mailfile: initgroups(%s, %ld) failed",
					RealUserName, (long) RealGid);
				RETURN(EX_TEMPFAIL);
			}
		}
		else
		{
			GIDSET_T gidset[1];

			gidset[0] = RealGid;
			if (setgroups(1, gidset) == -1 && suidwarn)
			{
				syserr("mailfile: setgroups() failed");
				RETURN(EX_TEMPFAIL);
			}
		}

		/*
		**  If you have a safe environment, go into it.
		*/

		if (realfile != targetfile)
		{
			char save;

			save = *realfile;
			*realfile = '\0';
			if (tTd(11, 20))
				sm_dprintf("mailfile: chroot %s\n", targetfile);
			if (chroot(targetfile) < 0)
			{
				syserr("mailfile: Cannot chroot(%s)",
				       targetfile);
				RETURN(EX_CANTCREAT);
			}
			*realfile = save;
		}

		if (tTd(11, 40))
			sm_dprintf("mailfile: deliver to %s\n", realfile);

		if (chdir("/") < 0)
		{
			syserr("mailfile: cannot chdir(/)");
			RETURN(EX_CANTCREAT);
		}

		/* now reset the group and user ids */
		endpwent();
		sm_mbdb_terminate();
		if (setgid(RealGid) < 0 && suidwarn)
		{
			syserr("mailfile: setgid(%ld) failed", (long) RealGid);
			RETURN(EX_TEMPFAIL);
		}
		vendor_set_uid(RealUid);
		if (setuid(RealUid) < 0 && suidwarn)
		{
			syserr("mailfile: setuid(%ld) failed", (long) RealUid);
			RETURN(EX_TEMPFAIL);
		}

		if (tTd(11, 2))
			sm_dprintf("mailfile: running as r/euid=%ld/%ld, r/egid=%ld/%ld\n",
				(long) getuid(), (long) geteuid(),
				(long) getgid(), (long) getegid());


		/* move into some "safe" directory */
		if (mailer->m_execdir != NULL)
		{
			char *q;

			for (p = mailer->m_execdir; p != NULL; p = q)
			{
				q = strchr(p, ':');
				if (q != NULL)
					*q = '\0';
				expand(p, buf, sizeof(buf), e);
				if (q != NULL)
					*q++ = ':';
				if (tTd(11, 20))
					sm_dprintf("mailfile: trydir %s\n",
						   buf);
				if (buf[0] != '\0' && chdir(buf) >= 0)
					break;
			}
		}

		/*
		**  Recheck the file after we have assumed the ID of the
		**  delivery user to make sure we can deliver to it as
		**  that user.  This is necessary if sendmail is running
		**  as root and the file is on an NFS mount which treats
		**  root as nobody.
		*/

#if HASLSTAT
		if (bitnset(DBS_FILEDELIVERYTOSYMLINK, DontBlameSendmail))
			err = stat(realfile, &stb);
		else
			err = lstat(realfile, &stb);
#else /* HASLSTAT */
		err = stat(realfile, &stb);
#endif /* HASLSTAT */

		if (err < 0)
		{
			stb.st_mode = ST_MODE_NOFILE;
			mode = FileMode;
			oflags |= O_CREAT|O_EXCL;
		}
		else if (bitset(S_IXUSR|S_IXGRP|S_IXOTH, mode) ||
			 (!bitnset(DBS_FILEDELIVERYTOHARDLINK,
				   DontBlameSendmail) &&
			  stb.st_nlink != 1) ||
			 (realfile != targetfile && !S_ISREG(mode)))
			exit(EX_CANTCREAT);
		else
			mode = stb.st_mode;

		if (!bitnset(DBS_FILEDELIVERYTOSYMLINK, DontBlameSendmail))
			sfflags |= SFF_NOSLINK;
		if (!bitnset(DBS_FILEDELIVERYTOHARDLINK, DontBlameSendmail))
			sfflags |= SFF_NOHLINK;
		sfflags &= ~SFF_OPENASROOT;
		f = safefopen(realfile, oflags, mode, sfflags);
		if (f == NULL)
		{
			if (transienterror(errno))
			{
				usrerr("454 4.3.0 cannot open %s: %s",
				       shortenstring(realfile, MAXSHORTSTR),
				       sm_errstring(errno));
				RETURN(EX_TEMPFAIL);
			}
			else
			{
				usrerr("554 5.3.0 cannot open %s: %s",
				       shortenstring(realfile, MAXSHORTSTR),
				       sm_errstring(errno));
				RETURN(EX_CANTCREAT);
			}
		}
		if (filechanged(realfile, sm_io_getinfo(f, SM_IO_WHAT_FD, NULL),
		    &stb))
		{
			syserr("554 5.3.0 file changed after open");
			RETURN(EX_CANTCREAT);
		}
		if (fstat(sm_io_getinfo(f, SM_IO_WHAT_FD, NULL), &stb) < 0)
		{
			syserr("554 5.3.0 cannot fstat %s",
				sm_errstring(errno));
			RETURN(EX_CANTCREAT);
		}

		curoff = stb.st_size;

		if (ev != NULL)
			sm_clrevent(ev);

		memset(&mcibuf, '\0', sizeof(mcibuf));
		mcibuf.mci_mailer = mailer;
		mcibuf.mci_out = f;
		if (bitnset(M_7BITS, mailer->m_flags))
			mcibuf.mci_flags |= MCIF_7BIT;

		/* clear out per-message flags from connection structure */
		mcibuf.mci_flags &= ~(MCIF_CVT7TO8|MCIF_CVT8TO7);

		if (bitset(EF_HAS8BIT, e->e_flags) &&
		    !bitset(EF_DONT_MIME, e->e_flags) &&
		    bitnset(M_7BITS, mailer->m_flags))
			mcibuf.mci_flags |= MCIF_CVT8TO7;

#if MIME7TO8
		if (bitnset(M_MAKE8BIT, mailer->m_flags) &&
		    !bitset(MCIF_7BIT, mcibuf.mci_flags) &&
		    (p = hvalue("Content-Transfer-Encoding", e->e_header)) != NULL &&
		    (SM_STRCASEEQ(p, "quoted-printable") ||
		     SM_STRCASEEQ(p, "base64")) &&
		    (p = hvalue("Content-Type", e->e_header)) != NULL)
		{
			/* may want to convert 7 -> 8 */
			/* XXX should really parse it here -- and use a class XXX */
			if (sm_strncasecmp(p, "text/plain", 10) == 0 &&
			    (p[10] == '\0' || p[10] == ' ' || p[10] == ';'))
				mcibuf.mci_flags |= MCIF_CVT7TO8;
		}
#endif /* MIME7TO8 */

		if (!putfromline(&mcibuf, e) ||
		    !(*e->e_puthdr)(&mcibuf, e->e_header, e, M87F_OUTER) ||
		    !(*e->e_putbody)(&mcibuf, e, NULL) ||
		    !putline("\n", &mcibuf) ||
		    (sm_io_flush(f, SM_TIME_DEFAULT) != 0 ||
		    (SuperSafe != SAFE_NO &&
		     fsync(sm_io_getinfo(f, SM_IO_WHAT_FD, NULL)) < 0) ||
		    sm_io_error(f)))
		{
			setstat(EX_IOERR);
#if !NOFTRUNCATE
			(void) ftruncate(sm_io_getinfo(f, SM_IO_WHAT_FD, NULL),
					 curoff);
#endif
		}

		/* reset ISUID & ISGID bits for paranoid systems */
#if HASFCHMOD
		(void) fchmod(sm_io_getinfo(f, SM_IO_WHAT_FD, NULL),
			      (MODE_T) mode);
#else
		(void) chmod(filename, (MODE_T) mode);
#endif
		if (sm_io_close(f, SM_TIME_DEFAULT) < 0)
			setstat(EX_IOERR);
		(void) sm_io_flush(smioout, SM_TIME_DEFAULT);
		(void) setuid(RealUid);
		exit(ExitStat);
		/* NOTREACHED */
	}
	else
	{
		/* parent -- wait for exit status */
		int st;

		st = waitfor(pid);
		if (st == -1)
		{
			syserr("mailfile: %s: wait", mailer->m_name);
			return EX_SOFTWARE;
		}
		if (WIFEXITED(st))
		{
			errno = 0;
			return (WEXITSTATUS(st));
		}
		else
		{
			syserr("mailfile: %s: child died on signal %d",
			       mailer->m_name, st);
			return EX_UNAVAILABLE;
		}
		/* NOTREACHED */
	}
	return EX_UNAVAILABLE;	/* avoid compiler warning on IRIX */
}

static void
mailfiletimeout(ignore)
	int ignore;
{
	/*
	**  NOTE: THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
	**	ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
	**	DOING.
	*/

	errno = ETIMEDOUT;
	longjmp(CtxMailfileTimeout, 1);
}

#if DANE

/*
**  GETMPORT -- return the port of a mailer
**
**	Parameters:
**		m -- the mailer describing this host.
**
**	Returns:
**		the port of the mailer if defined.
**		0 otherwise
**		<0 error
*/

static int getmport __P((MAILER *));

static int
getmport(m)
	MAILER *m;
{
	unsigned long ulval;
	char *buf, *ep;

	if (m->m_port > 0)
		return m->m_port;

	if (NULL == m->m_argv[0] ||NULL == m->m_argv[1])
		return -1;
	buf = m->m_argv[2];
	if (NULL == buf)
		return 0;

	errno = 0;
	ulval = strtoul(buf, &ep, 0);
	if (buf[0] == '\0' || *ep != '\0')
		return -1;
	if (errno == ERANGE && ulval == ULONG_MAX)
		return -1;
	if (ulval > USHRT_MAX)
		return -1;
	m->m_port = (unsigned short) ulval;
	if (tTd(17, 30))
		sm_dprintf("getmport: mailer=%s, port=%d\n", m->m_name,
			m->m_port);
	return m->m_port;
}
# define GETMPORT(m) getmport(m)
#else /* DANE */
# define GETMPORT(m)	25
#endif /* DANE */

/*
**  HOSTSIGNATURE -- return the "signature" for a host (list).
**
**	The signature describes how we are going to send this -- it
**	can be just the hostname (for non-Internet hosts) or can be
**	an ordered list of MX hosts.
**
**	Parameters:
**		m -- the mailer describing this host.
**		host -- the host name (can be a list).
**		ad -- DNSSEC: ad flag for lookup of host.
**		pqflags -- (pointer to) q_flags (can be NULL)
**
**	Returns:
**		The signature for this host.
**
**	Side Effects:
**		Can tweak the symbol table.
*/

#define MAXHOSTSIGNATURE	8192	/* max len of hostsignature */

char *
hostsignature(m, host, ad, pqflags)
	register MAILER *m;
	char *host;
	bool ad;
	unsigned long *pqflags;
{
	register char *p;
	register STAB *s;
	time_t now;
#if NAMED_BIND
	char sep = ':';
	char prevsep = ':';
	int i;
	int len;
	int nmx;
	int hl;
	char *hp;
	char *endp;
	char *lstr;
	int oldoptions = _res.options;
	char *mxhosts[MAXMXHOSTS + 1];
	unsigned short mxprefs[MAXMXHOSTS + 1];
#endif /* NAMED_BIND */
	int admx;

	if (tTd(17, 3))
		sm_dprintf("hostsignature: host=%s, ad=%d\n", host, ad);
	if (pqflags != NULL && !ad)
		*pqflags &= ~QMXSECURE;

	/*
	**  If local delivery (and not remote), just return a constant.
	*/

	if (bitnset(M_LOCALMAILER, m->m_flags) &&
	    strcmp(m->m_mailer, "[IPC]") != 0 &&
	    !(m->m_argv[0] != NULL && strcmp(m->m_argv[0], "TCP") == 0))
		return "localhost";

	/* an empty host does not have MX records */
	if (*host == '\0')
		return "_empty_";

	/*
	**  Check to see if this uses IPC -- if not, it can't have MX records.
	*/

	if (strcmp(m->m_mailer, "[IPC]") != 0 ||
	    CurEnv->e_sendmode == SM_DEFER)
	{
		/* just an ordinary mailer or deferred mode */
		return host;
	}
#if NETUNIX
	else if (m->m_argv[0] != NULL &&
		 strcmp(m->m_argv[0], "FILE") == 0)
	{
		/* rendezvous in the file system, no MX records */
		return host;
	}
#endif /* NETUNIX */

	/*
	**  Look it up in the symbol table.
	*/

	now = curtime();
	s = stab(host, ST_HOSTSIG, ST_ENTER);
	if (s->s_hostsig.hs_sig != NULL)
	{
		if (s->s_hostsig.hs_exp >= now)
		{
			if (tTd(17, 3))
				sm_dprintf("hostsignature: stab(%s) found %s\n", host,
					   s->s_hostsig.hs_sig);
			return s->s_hostsig.hs_sig;
		}

		/* signature is expired: clear it */
		sm_free(s->s_hostsig.hs_sig);
		s->s_hostsig.hs_sig = NULL;
	}

	/* set default TTL */
	s->s_hostsig.hs_exp = now + SM_DEFAULT_TTL;

	/*
	**  Not already there or expired -- create a signature.
	*/

#if NAMED_BIND
	if (ConfigLevel < 2)
		_res.options &= ~(RES_DEFNAMES | RES_DNSRCH);	/* XXX */

	for (hp = host; hp != NULL; hp = endp)
	{
# if NETINET6
		if (*hp == '[')
		{
			endp = strchr(hp + 1, ']');
			if (endp != NULL)
				endp = strpbrk(endp + 1, ":,");
		}
		else
			endp = strpbrk(hp, ":,");
# else /* NETINET6 */
		endp = strpbrk(hp, ":,");
# endif /* NETINET6 */
		if (endp != NULL)
		{
			sep = *endp;
			*endp = '\0';
		}

		if (bitnset(M_NOMX, m->m_flags))
		{
			/* skip MX lookups */
			nmx = 1;
			mxhosts[0] = hp;
		}
		else
		{
			auto int rcode;
			int ttl;

			admx = 0;
			nmx = getmxrr(hp, mxhosts, mxprefs,
				      DROPLOCALHOST|(ad ? ISAD :0)|
				      ((NULL == endp) ? TRYFALLBACK : 0),
				      &rcode, &ttl, GETMPORT(m), &admx);
			if (nmx <= 0)
			{
				int save_errno;
				register MCI *mci;

				/* update the connection info for this host */
				save_errno = errno;
				mci = mci_get(hp, m);
				mci->mci_errno = save_errno;
				mci->mci_herrno = h_errno;
				mci->mci_lastuse = now;
				if (nmx == NULLMX)
					mci_setstat(mci, rcode, ESCNULLMXRCPT,
						    ERRNULLMX);
				else if (rcode == EX_NOHOST)
					mci_setstat(mci, rcode, "5.1.2",
						    "550 Host unknown");
				else
					mci_setstat(mci, rcode, NULL, NULL);

				/* use the original host name as signature */
				nmx = 1;
				mxhosts[0] = hp;
			}

			/*
			**  NOTE: this sets QMXSECURE if the ad flags is set
			**  for at least one host in the host list. XXX
			*/

			if (pqflags != NULL && admx)
				*pqflags |= QMXSECURE;
			if (tTd(17, 3))
				sm_dprintf("hostsignature: host=%s, getmxrr=%d, mxhosts[0]=%s, admx=%d\n",
					   hp, nmx, mxhosts[0], admx);

			/*
			**  Set new TTL: we use only one!
			**	We could try to use the minimum instead.
			*/

			s->s_hostsig.hs_exp = now + SM_MIN(ttl, SM_DEFAULT_TTL);
		}

		len = 0;
		for (i = 0; i < nmx; i++)
			len += strlen(mxhosts[i]) + 1;
		if (s->s_hostsig.hs_sig != NULL)
			len += strlen(s->s_hostsig.hs_sig) + 1;
# if DANE && HSMARKS
		if (admx && DANE_SEC(Dane))
			len += nmx;
# endif
		if (len < 0 || len >= MAXHOSTSIGNATURE)
		{
			sm_syslog(LOG_WARNING, NOQID, "hostsignature for host '%s' exceeds maxlen (%d): %d",
				  host, MAXHOSTSIGNATURE, len);
			len = MAXHOSTSIGNATURE;
		}
		p = sm_pmalloc_x(len);
		if (s->s_hostsig.hs_sig != NULL)
		{
			(void) sm_strlcpy(p, s->s_hostsig.hs_sig, len);
			sm_free(s->s_hostsig.hs_sig); /* XXX */
			s->s_hostsig.hs_sig = p;
			hl = strlen(p);
			p += hl;
			*p++ = prevsep;
			len -= hl + 1;
		}
		else
			s->s_hostsig.hs_sig = p;
		for (i = 0; i < nmx; i++)
		{
			hl = strlen(mxhosts[i]);
			if (len <= 1 ||
# if DANE && HSMARKS
			    len - 1 < (hl + ((admx && DANE_SEC(Dane)) ? 1 : 0))
# else
			    len - 1 < hl
# endif
				)
			{
				/* force to drop out of outer loop */
				len = -1;
				break;
			}
			if (i != 0)
			{
				if (mxprefs[i] == mxprefs[i - 1])
					*p++ = ',';
				else
					*p++ = ':';
				len--;
			}
# if DANE && HSMARKS
			if (admx && DANE_SEC(Dane))
			{
				*p++ = HSM_AD;
				len--;
			}
# endif
			(void) sm_strlcpy(p, mxhosts[i], len);
			p += hl;
			len -= hl;
		}

		/*
		**  break out of loop if len exceeded MAXHOSTSIGNATURE
		**  because we won't have more space for further hosts
		**  anyway (separated by : in the .cf file).
		*/

		if (len < 0)
			break;
		if (endp != NULL)
			*endp++ = sep;
		prevsep = sep;
	}
	lstr = makelower_a(&s->s_hostsig.hs_sig, NULL);
	ASSIGN_IFDIFF(s->s_hostsig.hs_sig, lstr);
	if (ConfigLevel < 2)
		_res.options = oldoptions;
#else /* NAMED_BIND */
	/* not using BIND -- the signature is just the host name */
	/*
	**  'host' points to storage that will be freed after we are
	**  done processing the current envelope, so we copy it.
	*/
	s->s_hostsig.hs_sig = sm_pstrdup_x(host);
#endif /* NAMED_BIND */
	if (tTd(17, 1))
		sm_dprintf("hostsignature: host=%s, result=%s\n", host, s->s_hostsig.hs_sig);
	return s->s_hostsig.hs_sig;
}

/*
**  PARSE_HOSTSIGNATURE -- parse the "signature" and return MX host array.
**
**	The signature describes how we are going to send this -- it
**	can be just the hostname (for non-Internet hosts) or can be
**	an ordered list of MX hosts which must be randomized for equal
**	MX preference values.
**
**	Parameters:
**		sig -- the host signature.
**		mxhosts -- array to populate.
**		mailer -- mailer.
**
**	Returns:
**		The number of hosts inserted into mxhosts array.
**
**	NOTES:
**		mxhosts must have at least MAXMXHOSTS entries
**		mxhosts[] will point to elements in sig --
**		hence any changes to mxhosts[] will modify sig!
**
**	Side Effects:
**		Randomizes equal MX preference hosts in mxhosts.
*/

static int
parse_hostsignature(sig, mxhosts, mailer
#if DANE
	, mxads
#endif
	)
	char *sig;
	char **mxhosts;
	MAILER *mailer;
#if DANE
	BITMAP256 mxads;
#endif
{
	unsigned short curpref = 0;
	int nmx = 0, i, j;	/* NOTE: i, j, and nmx must have same type */
	char *hp, *endp;
	unsigned short prefer[MAXMXHOSTS];
	long rndm[MAXMXHOSTS];

#if DANE
	clrbitmap(mxads);
#endif
	for (hp = sig; hp != NULL; hp = endp)
	{
		char sep = ':';

		FIX_MXHOSTS(hp, endp, sep);
#if HSMARKS
		if (HSM_AD == *hp)
		{
			MXADS_SET(mxads, nmx);
			mxhosts[nmx] = hp + 1;
		}
		else
#endif
		mxhosts[nmx] = hp;
		prefer[nmx] = curpref;
		if (mci_match(hp, mailer))
			rndm[nmx] = 0;
		else
			rndm[nmx] = get_random();

		if (endp != NULL)
		{
			/*
			**  Since we don't have the original MX prefs,
			**  make our own.  If the separator is a ':', that
			**  means the preference for the next host will be
			**  higher than this one, so simply increment curpref.
			*/

			if (sep == ':')
				curpref++;

			*endp++ = sep;
		}
		if (++nmx >= MAXMXHOSTS)
			break;
	}

	/* sort the records using the random factor for equal preferences */
	for (i = 0; i < nmx; i++)
	{
		for (j = i + 1; j < nmx; j++)
		{
			/*
			**  List is already sorted by MX preference, only
			**  need to look for equal preference MX records
			*/

			if (prefer[i] < prefer[j])
				break;

			if (prefer[i] > prefer[j] ||
			    (prefer[i] == prefer[j] && rndm[i] > rndm[j]))
			{
				register unsigned short tempp;
				register long tempr;
				register char *temp1;

				tempp = prefer[i];
				prefer[i] = prefer[j];
				prefer[j] = tempp;
				temp1 = mxhosts[i];
				mxhosts[i] = mxhosts[j];
				mxhosts[j] = temp1;
				tempr = rndm[i];
				rndm[i] = rndm[j];
				rndm[j] = tempr;
			}
		}
	}
	return nmx;
}

#if STARTTLS
static SSL_CTX	*clt_ctx = NULL;
static bool	tls_ok_clt = true;
# if DANE
static bool	ctx_dane_enabled = false;
# endif

/*
**  SETCLTTLS -- client side TLS: allow/disallow.
**
**	Parameters:
**		tls_ok -- should tls be done?
**
**	Returns:
**		none.
**
**	Side Effects:
**		sets tls_ok_clt (static variable in this module)
*/

void
setclttls(tls_ok)
	bool tls_ok;
{
	tls_ok_clt = tls_ok;
	return;
}

/*
**  INITCLTTLS -- initialize client side TLS
**
**	Parameters:
**		tls_ok -- should TLS initialization be done?
**
**	Returns:
**		succeeded?
**
**	Side Effects:
**		sets tls_ok_clt, ctx_dane_enabled (static variables
**		in this module)
*/

bool
initclttls(tls_ok)
	bool tls_ok;
{
	if (!tls_ok_clt)
		return false;
	tls_ok_clt = tls_ok;
	if (!tls_ok_clt)
		return false;
	if (clt_ctx != NULL)
		return true;	/* already done */
	tls_ok_clt = inittls(&clt_ctx, TLS_I_CLT, Clt_SSL_Options, false,
			CltCertFile, CltKeyFile,
# if _FFR_CLIENTCA
			(CltCACertPath != NULL) ? CltCACertPath :
# endif
				CACertPath,
# if _FFR_CLIENTCA
			(CltCACertFile != NULL) ? CltCACertFile :
# endif
				CACertFile,
			DHParams);
# if _FFR_TESTS
	if (tls_ok_clt && tTd(90, 104))
	{
		sm_dprintf("test=simulate initclttls error\n");
		tls_ok_clt = false;
	}
# endif /* _FFR_TESTS */
# if DANE
	if (tls_ok_clt && CHK_DANE(Dane))
	{
#  if HAVE_SSL_CTX_dane_enable
		int r;

		r = SSL_CTX_dane_enable(clt_ctx);
#   if _FFR_TESTS
		if (tTd(90, 103))
		{
			sm_dprintf("test=simulate SSL_CTX_dane_enable error\n");
#    if defined(SSL_F_DANE_CTX_ENABLE)
			SSLerr(SSL_F_DANE_CTX_ENABLE, ERR_R_MALLOC_FAILURE);
#    endif
			r = -1;
		}
#   endif /* _FFR_TESTS */
		ctx_dane_enabled = (r > 0);
		if (r <= 0)
		{
			if (LogLevel > 1)
				sm_syslog(LOG_ERR, NOQID,
					"SSL_CTX_dane_enable=%d", r);
			tlslogerr(LOG_ERR, 7, "init_client");
		}
		else if (LogLevel > 13)
			sm_syslog(LOG_DEBUG, NOQID,
				"SSL_CTX_dane_enable=%d", r);
#  else
		ctx_dane_enabled = false;
#  endif /* HAVE_SSL_CTX_dane_enable */
	}
	if (tTd(90, 90))
		sm_dprintf("func=initclttls, ctx_dane_enabled=%d\n", ctx_dane_enabled);
# endif /* DANE */

	return tls_ok_clt;
}

/*
**  STARTTLS -- try to start secure connection (client side)
**
**	Parameters:
**		m -- the mailer.
**		mci -- the mailer connection info.
**		e -- the envelope.
**		implicit -- implicit TLS (SMTP over TLS, no STARTTLS command)
**
**	Returns:
**		success?
**		(maybe this should be some other code than EX_
**		that denotes which stage failed.)
*/

static int
starttls(m, mci, e, implicit
# if DANE
	, dane_vrfy_ctx
# endif
	)
	MAILER *m;
	MCI *mci;
	ENVELOPE *e;
	bool implicit;
# if DANE
	dane_vrfy_ctx_P	dane_vrfy_ctx;
# endif
{
	int smtpresult;
	int result = 0;
	int ret = EX_OK;
	int rfd, wfd;
	SSL *clt_ssl = NULL;
	time_t tlsstart;
	extern int TLSsslidx;

# if DANE
	if (TTD(90, 60))
		sm_dprintf("starttls=client: Dane=%d, dane_vrfy_chk=%#x\n",
			Dane,dane_vrfy_ctx->dane_vrfy_chk);
# endif
	if (clt_ctx == NULL && !initclttls(true))
		return EX_TEMPFAIL;

	if (!TLS_set_engine(SSLEngine, false))
	{
		sm_syslog(LOG_ERR, NOQID,
			  "STARTTLS=client, engine=%s, TLS_set_engine=failed",
			  SSLEngine);
		return EX_TEMPFAIL;
	}

	/* clt_ssl needed for get_tls_se_features() hence create here */
	if ((clt_ssl = SSL_new(clt_ctx)) == NULL)
	{
		if (LogLevel > 5)
		{
			sm_syslog(LOG_ERR, NOQID,
				  "STARTTLS=client, error: SSL_new failed");
			tlslogerr(LOG_WARNING, 9, "client");
		}
		return EX_TEMPFAIL;
	}

	ret = get_tls_se_features(e, clt_ssl, &mci->mci_tlsi, false);
	if (EX_OK != ret)
	{
		sm_syslog(LOG_ERR, NOQID,
			  "STARTTLS=client, get_tls_se_features=failed, ret=%d",
			  ret);
		goto fail;
	}

	if (!implicit)
	{
		smtpmessage("STARTTLS", m, mci);

		/* get the reply */
		smtpresult = reply(m, mci, e, TimeOuts.to_starttls, NULL, NULL,
				XS_STARTTLS, NULL);

		/* check return code from server */
		if (REPLYTYPE(smtpresult) == 4)
		{
			ret = EX_TEMPFAIL;
			goto fail;
		}
#if 0
		/*
		**  RFC 3207 says
		**  501       Syntax error (no parameters allowed)
		**  since sendmail does not use arguments, that's basically
		**  a "cannot happen", hence treat it as any other 5xy,
		**  which means it is also properly handled by the rules.
		*/

		if (smtpresult == 501)
		{
			ret = EX_USAGE;
			goto fail;
		}
#endif /* 0 */
		if (smtpresult == -1)
		{
			ret = smtpresult;
			goto fail;
		}

		/* not an expected reply but we have to deal with it */
		if (REPLYTYPE(smtpresult) == 5)
		{
			ret = EX_UNAVAILABLE;
			goto fail;
		}
		if (smtpresult != 220)
		{
			ret = EX_PROTOCOL;
			goto fail;
		}
	}

	if (LogLevel > 13)
		sm_syslog(LOG_INFO, NOQID, "STARTTLS=client, start=ok");

	/* SSL_clear(clt_ssl); ? */
	result = SSL_set_ex_data(clt_ssl, TLSsslidx, &mci->mci_tlsi);
	if (0 == result)
	{
		if (LogLevel > 5)
		{
			sm_syslog(LOG_ERR, NOQID,
				  "STARTTLS=client, error: SSL_set_ex_data failed=%d, idx=%d",
				  result, TLSsslidx);
			tlslogerr(LOG_WARNING, 9, "client");
		}
		goto fail;
	}
# if DANE
	if (SM_TLSI_IS(&(mci->mci_tlsi), TLSI_FL_NODANE))
		dane_vrfy_ctx->dane_vrfy_chk = DANE_NEVER;
	if (TTD(90, 60))
		sm_dprintf("starttls=client: 2: dane_vrfy_chk=%#x CHK_DANE=%d\n",
			dane_vrfy_ctx->dane_vrfy_chk,
			CHK_DANE(dane_vrfy_ctx->dane_vrfy_chk));
	if (CHK_DANE(dane_vrfy_ctx->dane_vrfy_chk))
	{
		int r;

		/* set SNI only if there is a TLSA RR */
		if (tTd(90, 40))
			sm_dprintf("dane_get_tlsa=%p, dane_vrfy_host=%s, dane_vrfy_sni=%s, ctx_dane_enabled=%d, dane_enabled=%d\n",
				dane_get_tlsa(dane_vrfy_ctx),
				dane_vrfy_ctx->dane_vrfy_host,
				dane_vrfy_ctx->dane_vrfy_sni,
				ctx_dane_enabled,
				dane_vrfy_ctx->dane_vrfy_dane_enabled);
		if (dane_get_tlsa(dane_vrfy_ctx) != NULL &&
		    !(SM_IS_EMPTY(dane_vrfy_ctx->dane_vrfy_host) &&
		      SM_IS_EMPTY(dane_vrfy_ctx->dane_vrfy_sni)))
		{
#  if _FFR_MTA_STS
			SM_FREE(STS_SNI);
#  endif
			dane_vrfy_ctx->dane_vrfy_dane_enabled = ctx_dane_enabled;
			if ((r = ssl_dane_enable(dane_vrfy_ctx, clt_ssl)) < 0)
			{
				dane_vrfy_ctx->dane_vrfy_dane_enabled = false;
				if (LogLevel > 5)
				{
					sm_syslog(LOG_ERR, NOQID,
						  "STARTTLS=client, host=%s, ssl_dane_enable=%d",
						  dane_vrfy_ctx->dane_vrfy_host, r);
				}
			}
			if (SM_NOTDONE == r)
				dane_vrfy_ctx->dane_vrfy_dane_enabled = false;
			if (tTd(90, 40))
				sm_dprintf("ssl_dane_enable=%d, chk=%#x, dane_enabled=%d\n",
					r, dane_vrfy_ctx->dane_vrfy_chk,
					dane_vrfy_ctx->dane_vrfy_dane_enabled);
			if ((r = SSL_set_tlsext_host_name(clt_ssl,
				(!SM_IS_EMPTY(dane_vrfy_ctx->dane_vrfy_sni)
				? dane_vrfy_ctx->dane_vrfy_sni
				: dane_vrfy_ctx->dane_vrfy_host))) <= 0)
			{
				if (LogLevel > 5)
				{
					sm_syslog(LOG_ERR, NOQID,
						  "STARTTLS=client, host=%s, SSL_set_tlsext_host_name=%d",
						  dane_vrfy_ctx->dane_vrfy_host, r);
				}
				tlslogerr(LOG_ERR, 5, "client");
				/* return EX_SOFTWARE; */
			}
		}
	}
	memcpy(&mci->mci_tlsi.tlsi_dvc, dane_vrfy_ctx, sizeof(*dane_vrfy_ctx));
# endif /* DANE */
# if _FFR_MTA_STS
	if (STS_SNI != NULL)
	{
		int r;

		if ((r = SSL_set_tlsext_host_name(clt_ssl, STS_SNI)) <= 0)
		{
			if (LogLevel > 5)
			{
				sm_syslog(LOG_ERR, NOQID,
					  "STARTTLS=client, host=%s, SSL_set_tlsext_host_name=%d",
					  STS_SNI, r);
			}
			tlslogerr(LOG_ERR, 5, "client");
			/* return EX_SOFTWARE; */
		}
	}
# endif /* _FFR_MTA_STS */

	rfd = sm_io_getinfo(mci->mci_in, SM_IO_WHAT_FD, NULL);
	wfd = sm_io_getinfo(mci->mci_out, SM_IO_WHAT_FD, NULL);

	if (rfd < 0 || wfd < 0 ||
	    (result = SSL_set_rfd(clt_ssl, rfd)) != 1 ||
	    (result = SSL_set_wfd(clt_ssl, wfd)) != 1)
	{
		if (LogLevel > 5)
		{
			sm_syslog(LOG_ERR, NOQID,
				  "STARTTLS=client, error: SSL_set_xfd failed=%d",
				  result);
			tlslogerr(LOG_WARNING, 9, "client");
		}
		goto fail;
	}
	SSL_set_connect_state(clt_ssl);
	tlsstart = curtime();

ssl_retry:
	if ((result = SSL_connect(clt_ssl)) <= 0)
	{
		int i, ssl_err;
		int save_errno = errno;

		ssl_err = SSL_get_error(clt_ssl, result);
		i = tls_retry(clt_ssl, rfd, wfd, tlsstart,
			TimeOuts.to_starttls, ssl_err, "client");
		if (i > 0)
			goto ssl_retry;

		if (LogLevel > 5)
		{
			unsigned long l;
			const char *sr;

			l = ERR_peek_error();
			sr = ERR_reason_error_string(l);
			sm_syslog(LOG_WARNING, NOQID,
				  "STARTTLS=client, error: connect failed=%d, reason=%s, SSL_error=%d, errno=%d, retry=%d",
				  result, sr == NULL ? "unknown" : sr, ssl_err,
				  save_errno, i);
			tlslogerr(LOG_WARNING, 9, "client");
		}

		goto fail;
	}
	mci->mci_ssl = clt_ssl;
	result = tls_get_info(mci->mci_ssl, false, mci->mci_host,
			      &mci->mci_macro, true);

	/* switch to use TLS... */
	if (sfdctls(&mci->mci_in, &mci->mci_out, mci->mci_ssl) == 0)
		return EX_OK;

  fail:
	/* failure */
	SM_SSL_FREE(clt_ssl);
	return (EX_OK == ret) ? EX_SOFTWARE : ret;
}

/*
**  ENDTLSCLT -- shutdown secure connection (client side)
**
**	Parameters:
**		mci -- the mailer connection info.
**
**	Returns:
**		success?
*/

static int
endtlsclt(mci)
	MCI *mci;
{
	int r;

	if (!bitset(MCIF_TLSACT, mci->mci_flags))
		return EX_OK;
	r = endtls(&mci->mci_ssl, "client");
	mci->mci_flags &= ~MCIF_TLSACT;
	return r;
}
#endif /* STARTTLS */
#if STARTTLS || SASL
/*
**  ISCLTFLGSET -- check whether client flag is set.
**
**	Parameters:
**		e -- envelope.
**		flag -- flag to check in {client_flags}
**
**	Returns:
**		true iff flag is set.
*/

static bool
iscltflgset(e, flag)
	ENVELOPE *e;
	int flag;
{
	char *p;

	p = macvalue(macid("{client_flags}"), e);
	if (p == NULL)
		return false;
	for (; *p != '\0'; p++)
	{
		/* look for just this one flag */
		if (*p == (char) flag)
			return true;
	}
	return false;
}
#endif /* STARTTLS || SASL */

#if _FFR_TESTS
void
t_parsehostsig(hs, mailer)
	char *hs;
	MAILER *mailer;
{
	int nummxhosts, i;
	char *mxhosts[MAXMXHOSTS + 1];
#if DANE
	BITMAP256 mxads;
#endif

	if (NULL == mailer)
		mailer = LocalMailer;
	nummxhosts = parse_hostsignature(hs, mxhosts, mailer
#if DANE
			, mxads
#endif
			);
	(void) sm_io_fprintf(smioout, SM_TIME_DEFAULT,
		     "nummxhosts=%d\n", nummxhosts);
	for (i = 0; i < nummxhosts; i++)
		(void) sm_io_fprintf(smioout, SM_TIME_DEFAULT,
		     "mx[%d]=%s, ad=%d\n", i, mxhosts[i], MXADS_ISSET(mxads, 0));
}

void
t_hostsig(a, hs, mailer)
	ADDRESS *a;
	char *hs;
	MAILER *mailer;
{
	char *q;

	if (NULL != a)
		q = hostsignature(a->q_mailer, a->q_host, true, &a->q_flags);
	else if (NULL != hs)
	{
		SM_REQUIRE(NULL != mailer);
		q = hostsignature(mailer, hs, true, NULL);
	}
	else
		SM_REQUIRE(NULL != hs);
	(void) sm_io_fprintf(smioout, SM_TIME_DEFAULT, "hostsig %s\n", q);
	t_parsehostsig(q, (NULL != a) ? a->q_mailer : mailer);
}
#endif /* _FFR_TESTS */
