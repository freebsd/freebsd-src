/*
 * Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
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

#ifndef lint
static char id[] = "@(#)$Id: deliver.c,v 8.600.2.1.2.31 2000/07/18 02:24:43 gshapiro Exp $";
#endif /* ! lint */

#include <sendmail.h>


#if HASSETUSERCONTEXT
# include <login_cap.h>
#endif /* HASSETUSERCONTEXT */

#if STARTTLS || (SASL && SFIO)
# include "sfsasl.h"
#endif /* STARTTLS || (SASL && SFIO) */

static int	deliver __P((ENVELOPE *, ADDRESS *));
static void	dup_queue_file __P((ENVELOPE *, ENVELOPE *, int));
static void	mailfiletimeout __P((void));
static void	markfailure __P((ENVELOPE *, ADDRESS *, MCI *, int, bool));
static int	parse_hostsignature __P((char *, char **, MAILER *));
static void	sendenvelope __P((ENVELOPE *, int));
static char	*hostsignature __P((MAILER *, char *));

#if SMTP
# if STARTTLS
static int	starttls __P((MAILER *, MCI *, ENVELOPE *));
# endif /* STARTTLS */
#endif /* SMTP */

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
	bool somedeliveries = FALSE, expensive = FALSE;
	pid_t pid;

	/*
	**  If this message is to be discarded, don't bother sending
	**  the message at all.
	*/

	if (bitset(EF_DISCARD, e->e_flags))
	{
		if (tTd(13, 1))
			dprintf("sendall: discarding id %s\n", e->e_id);
		e->e_flags |= EF_CLRQUEUE;
		if (LogLevel > 4)
			sm_syslog(LOG_INFO, e->e_id, "discarded");
		markstats(e, NULL, TRUE);
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
		dprintf("\n===== SENDALL: mode %c, id %s, e_from ",
			mode, e->e_id);
		printaddr(&e->e_from, FALSE);
		dprintf("\te_flags = ");
		printenvflags(e);
		dprintf("sendqueue:\n");
		printaddr(e->e_sendqueue, TRUE);
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
		errno = 0;
#if QUEUE
		queueup(e, mode == SM_QUEUE || mode == SM_DEFER);
#endif /* QUEUE */
		e->e_flags |= EF_FATALERRS|EF_PM_NOTIFY|EF_CLRQUEUE;
		ExitStat = EX_UNAVAILABLE;
		syserr("554 5.0.0 Too many hops %d (%d max): from %s via %s, to %s",
			e->e_hopcount, MaxHopCount, e->e_from.q_paddr,
			RealHostName == NULL ? "localhost" : RealHostName,
			e->e_sendqueue->q_paddr);
		for (q = e->e_sendqueue; q != NULL; q = q->q_next)
		{
			if (QS_IS_DEAD(q->q_state))
				continue;
			q->q_state = QS_BADADDR;
			q->q_status = "5.4.6";
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
			dprintf("sendall: QS_SENDER ");
			printaddr(&e->e_from, FALSE);
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
		dprintf("\nAfter first owner pass, sendq =\n");
		printaddr(e->e_sendqueue, TRUE);
	}

	owner = "";
	otherowners = 1;
	while (owner != NULL && otherowners > 0)
	{
		if (tTd(13, 28))
			dprintf("owner = \"%s\", otherowners = %d\n",
				owner, otherowners);
		owner = NULL;
		otherowners = bitset(EF_SENDRECEIPT, e->e_flags) ? 1 : 0;

		for (q = e->e_sendqueue; q != NULL; q = q->q_next)
		{
			if (tTd(13, 30))
			{
				dprintf("Checking ");
				printaddr(q, FALSE);
			}
			if (QS_IS_DEAD(q->q_state))
			{
				if (tTd(13, 30))
					dprintf("    ... QS_IS_DEAD\n");
				continue;
			}
			if (tTd(13, 29) && !tTd(13, 30))
			{
				dprintf("Checking ");
				printaddr(q, FALSE);
			}

			if (q->q_owner != NULL)
			{
				if (owner == NULL)
				{
					if (tTd(13, 40))
						dprintf("    ... First owner = \"%s\"\n",
							q->q_owner);
					owner = q->q_owner;
				}
				else if (owner != q->q_owner)
				{
					if (strcmp(owner, q->q_owner) == 0)
					{
						if (tTd(13, 40))
							dprintf("    ... Same owner = \"%s\"\n",
								owner);

						/* make future comparisons cheap */
						q->q_owner = owner;
					}
					else
					{
						if (tTd(13, 40))
							dprintf("    ... Another owner \"%s\"\n",
								q->q_owner);
						otherowners++;
					}
					owner = q->q_owner;
				}
				else if (tTd(13, 40))
					dprintf("    ... Same owner = \"%s\"\n",
						owner);
			}
			else
			{
				if (tTd(13, 40))
					dprintf("    ... Null owner\n");
				otherowners++;
			}

			if (QS_IS_BADADDR(q->q_state))
			{
				if (tTd(13, 30))
					dprintf("    ... QS_IS_BADADDR\n");
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

				if (FallBackMX != NULL &&
				    !wordinclass(FallBackMX, 'w') &&
				    mode != SM_VERIFY &&
				    (strcmp(m->m_mailer, "[IPC]") == 0 ||
				     strcmp(m->m_mailer, "[TCP]") == 0) &&
				    m->m_argv[0] != NULL &&
				    (strcmp(m->m_argv[0], "TCP") == 0 ||
				     strcmp(m->m_argv[0], "IPC") == 0))
				{
					int len;
					char *p;

					if (tTd(13, 30))
						dprintf("    ... FallBackMX\n");

					len = strlen(FallBackMX) + 3;
					p = xalloc(len);
					snprintf(p, len, "[%s]", FallBackMX);
					q->q_state = QS_OK;
					q->q_host = p;
				}
				else
				{
					if (tTd(13, 30))
						dprintf("    ... QS_IS_QUEUEUP\n");
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
					dprintf("    ... expensive\n");
				q->q_state = QS_QUEUEUP;
				expensive = TRUE;
			}
			else if (bitnset(M_HOLD, q->q_mailer->m_flags) &&
				 QueueLimitId == NULL &&
				 QueueLimitSender == NULL &&
				 QueueLimitRecipient == NULL)
			{
				if (tTd(13, 30))
					dprintf("    ... hold\n");
				q->q_state = QS_QUEUEUP;
				expensive = TRUE;
			}
			else
			{
				if (tTd(13, 30))
					dprintf("    ... deliverable\n");
				somedeliveries = TRUE;
			}
		}

		if (owner != NULL && otherowners > 0)
		{
			/*
			**  Split this envelope into two.
			*/

			ee = (ENVELOPE *) xalloc(sizeof *ee);
			*ee = *e;
			ee->e_message = NULL;
			ee->e_id = NULL;
			assign_queueid(ee);

			if (tTd(13, 1))
				dprintf("sendall: split %s into %s, owner = \"%s\", otherowners = %d\n",
					e->e_id, ee->e_id, owner, otherowners);

			ee->e_header = copyheader(e->e_header);
			ee->e_sendqueue = copyqueue(e->e_sendqueue);
			ee->e_errorqueue = copyqueue(e->e_errorqueue);
			ee->e_flags = e->e_flags & ~(EF_INQUEUE|EF_CLRQUEUE|EF_FATALERRS|EF_SENDRECEIPT|EF_RET_PARAM);
			ee->e_flags |= EF_NORECEIPT;
			setsender(owner, ee, NULL, '\0', TRUE);
			if (tTd(13, 5))
			{
				dprintf("sendall(split): QS_SENDER ");
				printaddr(&ee->e_from, FALSE);
			}
			ee->e_from.q_state = QS_SENDER;
			ee->e_dfp = NULL;
			ee->e_lockfp = NULL;
			ee->e_xfp = NULL;
			ee->e_queuedir = e->e_queuedir;
			ee->e_errormode = EM_MAIL;
			ee->e_sibling = splitenv;
			ee->e_statmsg = NULL;
			splitenv = ee;

			for (q = e->e_sendqueue; q != NULL; q = q->q_next)
			{
				if (q->q_owner == owner)
				{
					q->q_state = QS_CLONED;
					if (tTd(13, 6))
						dprintf("\t... stripping %s from original envelope\n",
							q->q_paddr);
				}
			}
			for (q = ee->e_sendqueue; q != NULL; q = q->q_next)
			{
				if (q->q_owner != owner)
				{
					q->q_state = QS_CLONED;
					if (tTd(13, 6))
						dprintf("\t... dropping %s from cloned envelope\n",
							q->q_paddr);
				}
				else
				{
					/* clear DSN parameters */
					q->q_flags &= ~(QHASNOTIFY|Q_PINGFLAGS);
					q->q_flags |= DefaultNotify & ~QPINGONSUCCESS;
					if (tTd(13, 6))
						dprintf("\t... moving %s to cloned envelope\n",
							q->q_paddr);
				}
			}

			if (mode != SM_VERIFY && bitset(EF_HAS_DF, e->e_flags))
				dup_queue_file(e, ee, 'd');

			/*
			**  Give the split envelope access to the parent
			**  transcript file for errors obtained while
			**  processing the recipients (done before the
			**  envelope splitting).
			*/

			if (e->e_xfp != NULL)
				ee->e_xfp = bfdup(e->e_xfp);

			/* failed to dup e->e_xfp, start a new transcript */
			if (ee->e_xfp == NULL)
				openxscript(ee);

			if (mode != SM_VERIFY && LogLevel > 4)
				sm_syslog(LOG_INFO, ee->e_id,
					  "clone %s, owner=%s",
					  e->e_id, owner);
		}
	}

	if (owner != NULL)
	{
		setsender(owner, e, NULL, '\0', TRUE);
		if (tTd(13, 5))
		{
			dprintf("sendall(owner): QS_SENDER ");
			printaddr(&e->e_from, FALSE);
		}
		e->e_from.q_state = QS_SENDER;
		e->e_errormode = EM_MAIL;
		e->e_flags |= EF_NORECEIPT;
		e->e_flags &= ~EF_FATALERRS;
	}

	/* if nothing to be delivered, just queue up everything */
	if (!somedeliveries && mode != SM_QUEUE && mode != SM_DEFER &&
	    mode != SM_VERIFY)
	{
		if (tTd(13, 29))
			dprintf("No deliveries: auto-queuing\n");
		mode = SM_QUEUE;

		/* treat this as a delivery in terms of counting tries */
		e->e_dtime = curtime();
		if (!expensive)
			e->e_ntries++;
		for (ee = splitenv; ee != NULL; ee = ee->e_sibling)
		{
			ee->e_dtime = curtime();
			if (!expensive)
				ee->e_ntries++;
		}
	}

#if QUEUE
	if ((mode == SM_QUEUE || mode == SM_DEFER || mode == SM_FORK ||
	     (mode != SM_VERIFY && SuperSafe)) &&
	    (!bitset(EF_INQUEUE, e->e_flags) || splitenv != NULL))
	{
		/* be sure everything is instantiated in the queue */
		queueup(e, mode == SM_QUEUE || mode == SM_DEFER);
		for (ee = splitenv; ee != NULL; ee = ee->e_sibling)
			queueup(ee, mode == SM_QUEUE || mode == SM_DEFER);
	}
#endif /* QUEUE */

	if (tTd(62, 10))
		checkfds("after envelope splitting");

	/*
	**  If we belong in background, fork now.
	*/

	if (tTd(13, 20))
	{
		dprintf("sendall: final mode = %c\n", mode);
		if (tTd(13, 21))
		{
			dprintf("\n================ Final Send Queue(s) =====================\n");
			dprintf("\n  *** Envelope %s, e_from=%s ***\n",
				e->e_id, e->e_from.q_paddr);
			printaddr(e->e_sendqueue, TRUE);
			for (ee = splitenv; ee != NULL; ee = ee->e_sibling)
			{
				dprintf("\n  *** Envelope %s, e_from=%s ***\n",
					ee->e_id, ee->e_from.q_paddr);
				printaddr(ee->e_sendqueue, TRUE);
			}
			dprintf("==========================================================\n\n");
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
#endif /* HASFLOCK */
		if (e->e_nrcpts > 0)
			e->e_flags |= EF_INQUEUE;
		dropenvelope(e, splitenv != NULL);
		for (ee = splitenv; ee != NULL; ee = ee->e_sibling)
		{
			if (ee->e_nrcpts > 0)
				ee->e_flags |= EF_INQUEUE;
			dropenvelope(ee, FALSE);
		}
		return;

	  case SM_FORK:
		if (e->e_xfp != NULL)
			(void) fflush(e->e_xfp);

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
			dropenvelope(e, splitenv != NULL);

			/* arrange to reacquire lock after fork */
			e->e_id = qid;
		}

		for (ee = splitenv; ee != NULL; ee = ee->e_sibling)
		{
			/* save id for future use */
			char *qid = ee->e_id;

			/* drop envelope in parent */
			ee->e_flags |= EF_INQUEUE;
			dropenvelope(ee, FALSE);

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

		closemaps();

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
			if (e->e_dfp != NULL)
				(void) bfclose(e->e_dfp);
			e->e_dfp = NULL;
			e->e_flags &= ~EF_HAS_DF;

			/* can't call unlockqueue to avoid unlink of xfp */
			if (e->e_lockfp != NULL)
				(void) fclose(e->e_lockfp);
			else
				syserr("%s: sendall: null lockfp", e->e_id);
			e->e_lockfp = NULL;
#endif /* HASFLOCK */

			/* make sure the parent doesn't own the envelope */
			e->e_id = NULL;

			/* catch intermediate zombie */
			(void) waitfor(pid);
			return;
		}

		/* double fork to avoid zombies */
		pid = fork();
		if (pid > 0)
			exit(EX_OK);
		save_errno = errno;

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
#else /* HASFLOCK */
			e->e_id = NULL;
#endif /* HASFLOCK */
			finis(TRUE, ExitStat);
		}

		/* be sure to give error messages in child */
		QuickAbort = FALSE;

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

#if HASFLOCK
		break;
#else /* HASFLOCK */

		/*
		**  Now reacquire and run the various queue files.
		*/

		for (ee = splitenv; ee != NULL; ee = ee->e_sibling)
		{
			ENVELOPE *sibling = ee->e_sibling;

			(void) dowork(ee->e_queuedir, ee->e_id,
				      FALSE, FALSE, ee);
			ee->e_sibling = sibling;
		}
		(void) dowork(e->e_queuedir, e->e_id,
			      FALSE, FALSE, e);
		finis(TRUE, ExitStat);
#endif /* HASFLOCK */
	}

	sendenvelope(e, mode);
	dropenvelope(e, TRUE);
	for (ee = splitenv; ee != NULL; ee = ee->e_sibling)
	{
		CurEnv = ee;
		if (mode != SM_VERIFY)
			openxscript(ee);
		sendenvelope(ee, mode);
		dropenvelope(ee, TRUE);
	}
	CurEnv = e;

	Verbose = oldverbose;
	if (mode == SM_FORK)
		finis(TRUE, ExitStat);
}

static void
sendenvelope(e, mode)
	register ENVELOPE *e;
	int mode;
{
	register ADDRESS *q;
	bool didany;

	if (tTd(13, 10))
		dprintf("sendenvelope(%s) e_flags=0x%lx\n",
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

	/* Don't attempt deliveries if we want to bounce now */
	if (!bitset(EF_RESPONSE, e->e_flags) &&
	    TimeOuts.to_q_return[e->e_timeoutclass] == NOW)
		return;

	/*
	**  Run through the list and send everything.
	**
	**	Set EF_GLOBALERRS so that error messages during delivery
	**	result in returned mail.
	*/

	e->e_nsent = 0;
	e->e_flags |= EF_GLOBALERRS;

	define(macid("{envid}", NULL), e->e_envid, e);
	define(macid("{bodytype}", NULL), e->e_bodytype, e);
	didany = FALSE;

	/* now run through the queue */
	for (q = e->e_sendqueue; q != NULL; q = q->q_next)
	{
#if XDEBUG
		char wbuf[MAXNAME + 20];

		(void) snprintf(wbuf, sizeof wbuf, "sendall(%.*s)",
			MAXNAME, q->q_paddr);
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
#if QUEUE
			/*
			**  Checkpoint the send list every few addresses
			*/

			if (e->e_nsent >= CheckpointInterval)
			{
				queueup(e, FALSE);
				e->e_nsent = 0;
			}
#endif /* QUEUE */
			(void) deliver(e, q);
			didany = TRUE;
		}
	}
	if (didany)
	{
		e->e_dtime = curtime();
		e->e_ntries++;
	}

#if XDEBUG
	checkfd012("end of sendenvelope");
#endif /* XDEBUG */
}
/*
**  DUP_QUEUE_FILE -- duplicate a queue file into a split queue
**
**	Parameters:
**		e -- the existing envelope
**		ee -- the new envelope
**		type -- the queue file type (e.g., 'd')
**
**	Returns:
**		none
*/

static void
dup_queue_file(e, ee, type)
	struct envelope *e, *ee;
	int type;
{
	char f1buf[MAXPATHLEN], f2buf[MAXPATHLEN];

	ee->e_dfp = NULL;
	ee->e_xfp = NULL;

	/*
	**  Make sure both are in the same directory.
	*/

	snprintf(f1buf, sizeof f1buf, "%s", queuename(e, type));
	snprintf(f2buf, sizeof f2buf, "%s", queuename(ee, type));
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
#endif /* ! FORK */

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

int
dofork()
{
	register pid_t pid = -1;

	DOFORK(fork);
	return pid;
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

#ifndef NO_UID
# define NO_UID		-1
#endif /* ! NO_UID */
#ifndef NO_GID
# define NO_GID		-1
#endif /* ! NO_GID */

static int
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
	ADDRESS *volatile contextaddr = NULL;
	register MCI *volatile mci;
	register ADDRESS *to = firstto;
	volatile bool clever = FALSE;	/* running user smtp to this mailer */
	ADDRESS *volatile tochain = NULL; /* users chain in this mailer call */
	int rcode;			/* response code */
	int lmtp_rcode = EX_OK;
	int nummxhosts = 0;		/* number of MX hosts available */
	int hostnum = 0;		/* current MX host index */
	char *firstsig;			/* signature of firstto */
	pid_t pid = -1;
	char *volatile curhost;
	register u_short port = 0;
#if NETUNIX
	char *mux_path = NULL;		/* path to UNIX domain socket */
#endif /* NETUNIX */
	time_t xstart;
	bool suidwarn;
	bool anyok;			/* at least one address was OK */
	bool goodmxfound = FALSE;	/* at least one MX was OK */
	bool ovr;
#if _FFR_DYNAMIC_TOBUF
	int strsize;
	int rcptcount;
	static int tobufsize = 0;
	static char *tobuf = NULL;
#else /* _FFR_DYNAMIC_TOBUF */
	char tobuf[TOBUFSIZE];		/* text line of to people */
#endif /* _FFR_DYNAMIC_TOBUF */
	int mpvect[2];
	int rpvect[2];
	char *mxhosts[MAXMXHOSTS + 1];
	char *pv[MAXPV + 1];
	char buf[MAXNAME + 1];
	char rpathbuf[MAXNAME + 1];	/* translated return path */

	errno = 0;
	if (!QS_IS_OK(to->q_state))
		return 0;

	suidwarn = geteuid() == 0;

	m = to->q_mailer;
	host = to->q_host;
	CurEnv = e;			/* just in case */
	e->e_statmsg = NULL;
#if SMTP
	SmtpError[0] = '\0';
#endif /* SMTP */
	xstart = curtime();

	if (tTd(10, 1))
		dprintf("\n--deliver, id=%s, mailer=%s, host=`%s', first user=`%s'\n",
			e->e_id, m->m_name, host, to->q_user);
	if (tTd(10, 100))
		printopenfds(FALSE);

	/*
	**  Clear $&{client_*} macros if this is a bounce message to
	**  prevent rejection by check_compat ruleset.
	*/

	if (bitset(EF_RESPONSE, e->e_flags))
	{
		define(macid("{client_name}", NULL), "", e);
		define(macid("{client_addr}", NULL), "", e);
		define(macid("{client_port}", NULL), "", e);
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
	if (bitnset(M_UDBENVELOPE, e->e_from.q_mailer->m_flags))
		p = e->e_sender;
	else
		p = e->e_from.q_paddr;
	p = remotename(p, m, RF_SENDERADDR|RF_CANONICAL, &rcode, e);
	if (strlen(p) >= (SIZE_T) sizeof rpathbuf)
	{
		p = shortenstring(p, MAXSHORTSTR);
		syserr("remotename: huge return %s", p);
	}
	snprintf(rpathbuf, sizeof rpathbuf, "%s", p);
	define('g', rpathbuf, e);		/* translated return path */
	define('h', host, e);			/* to host */
	Errors = 0;
	pvp = pv;
	*pvp++ = m->m_argv[0];

	/* insert -f or -r flag as appropriate */
	if (FromFlag &&
	    (bitnset(M_FOPT, m->m_flags) ||
	     bitnset(M_ROPT, m->m_flags)))
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
		expand(*mvp, buf, sizeof buf, e);
		*pvp++ = newstr(buf);
		if (pvp >= &pv[MAXPV - 3])
		{
			syserr("554 5.3.5 Too many parameters to %s before $u",
			       pv[0]);
			return -1;
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
#if SMTP
		clever = TRUE;
		*pvp = NULL;
#else /* SMTP */
		/* oops!  we don't implement SMTP */
		syserr("554 5.3.5 SMTP style mailer not implemented");
		return EX_SOFTWARE;
#endif /* SMTP */
	}

	/*
	**  At this point *mvp points to the argument with $u.  We
	**  run through our address list and append all the addresses
	**  we can.  If we run out of space, do not fret!  We can
	**  always send another copy later.
	*/

#if _FFR_DYNAMIC_TOBUF
	e->e_to = NULL;
	strsize = 2;
	rcptcount = 0;
#else /* _FFR_DYNAMIC_TOBUF */
	tobuf[0] = '\0';
	e->e_to = tobuf;
#endif /* _FFR_DYNAMIC_TOBUF */

	ctladdr = NULL;
	firstsig = hostsignature(firstto->q_mailer, firstto->q_host);
	for (; to != NULL; to = to->q_next)
	{
		/* avoid sending multiple recipients to dumb mailers */
#if _FFR_DYNAMIC_TOBUF
		if (tochain != NULL && !bitnset(M_MUSER, m->m_flags))
			break;
#else /* _FFR_DYNAMIC_TOBUF */
		if (tobuf[0] != '\0' && !bitnset(M_MUSER, m->m_flags))
			break;
#endif /* _FFR_DYNAMIC_TOBUF */

		/* if already sent or not for this host, don't send */
		if (!QS_IS_OK(to->q_state) ||
		    to->q_mailer != firstto->q_mailer ||
		    strcmp(hostsignature(to->q_mailer, to->q_host),
			   firstsig) != 0)
			continue;

		/* avoid overflowing tobuf */
#if _FFR_DYNAMIC_TOBUF
		strsize += strlen(to->q_paddr) + 1;
		if (!clever && strsize > TOBUFSIZE)
			break;

		if (++rcptcount > to->q_mailer->m_maxrcpt)
			break;
#else /* _FFR_DYNAMIC_TOBUF */
		if (sizeof tobuf < (strlen(to->q_paddr) + strlen(tobuf) + 2))
			break;
#endif /* _FFR_DYNAMIC_TOBUF */

		if (tTd(10, 1))
		{
			dprintf("\nsend to ");
			printaddr(to, FALSE);
		}

		/* compute effective uid/gid when sending */
		if (bitnset(M_RUNASRCPT, to->q_mailer->m_flags))
			contextaddr = ctladdr = getctladdr(to);

		if (tTd(10, 2))
		{
			dprintf("ctladdr=");
			printaddr(ctladdr, FALSE);
		}

		user = to->q_user;
		e->e_to = to->q_paddr;

		/*
		**  Check to see that these people are allowed to
		**  talk to each other.
		*/

		if (m->m_maxsize != 0 && e->e_msgsize > m->m_maxsize)
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
			markfailure(e, to, NULL, EX_UNAVAILABLE, FALSE);
			giveresponse(EX_UNAVAILABLE, to->q_status, m,
				     NULL, ctladdr, xstart, e);
			continue;
		}
#if NAMED_BIND
		h_errno = 0;
#endif /* NAMED_BIND */

		ovr = TRUE;
		/* do config file checking of compatibility */
		rcode = rscheck("check_compat", e->e_from.q_paddr, to->q_paddr,
				e, TRUE, TRUE, 4);
		if (rcode == EX_OK)
		{
			/* do in-code checking if not discarding */
			if (!bitset(EF_DISCARD, e->e_flags))
			{
				rcode = checkcompat(to, e);
				ovr = FALSE;
			}
		}
		if (rcode != EX_OK)
		{
			markfailure(e, to, NULL, rcode, ovr);
			giveresponse(rcode, to->q_status, m,
				     NULL, ctladdr, xstart, e);
			continue;
		}
		if (bitset(EF_DISCARD, e->e_flags))
		{
			if (tTd(10, 5))
			{
				dprintf("deliver: discarding recipient ");
				printaddr(to, FALSE);
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
			define('u', user, e);	/* to user */
			p = to->q_home;
			if (p == NULL && ctladdr != NULL)
				p = ctladdr->q_home;
			define('z', p, e);	/* user's home */
			expand(m->m_argv[1], buf, sizeof buf, e);
			if (strlen(buf) > 0)
				rcode = mailfile(buf, m, ctladdr, SFF_CREAT, e);
			else
			{
				syserr("empty filename specification for mailer %s",
				       m->m_name);
				rcode = EX_CONFIG;
			}
			giveresponse(rcode, to->q_status, m, NULL,
				     ctladdr, xstart, e);
			markfailure(e, to, NULL, rcode, TRUE);
			e->e_nsent++;
			if (rcode == EX_OK)
			{
				to->q_state = QS_SENT;
				if (bitnset(M_LOCALMAILER, m->m_flags) &&
				    bitset(QPINGONSUCCESS, to->q_flags))
				{
					to->q_flags |= QDELIVERED;
					to->q_status = "2.1.5";
					fprintf(e->e_xfp, "%s... Successfully delivered\n",
						to->q_paddr);
				}
			}
			to->q_statdate = curtime();
			markstats(e, to, FALSE);
			continue;
		}

		/*
		**  Address is verified -- add this user to mailer
		**  argv, and add it to the print list of recipients.
		*/

		/* link together the chain of recipients */
		to->q_tchain = tochain;
		tochain = to;

#if _FFR_DYNAMIC_TOBUF
		e->e_to = "[CHAIN]";
#else /* _FFR_DYNAMIC_TOBUF */
		/* create list of users for error messages */
		(void) strlcat(tobuf, ",", sizeof tobuf);
		(void) strlcat(tobuf, to->q_paddr, sizeof tobuf);
#endif /* _FFR_DYNAMIC_TOBUF */

		define('u', user, e);		/* to user */
		p = to->q_home;
		if (p == NULL && ctladdr != NULL)
			p = ctladdr->q_home;
		define('z', p, e);	/* user's home */

		/* set the ${dsn_notify} macro if applicable */
		if (bitset(QHASNOTIFY, to->q_flags))
		{
			char notify[MAXLINE];

			notify[0] = '\0';
			if (bitset(QPINGONSUCCESS, to->q_flags))
				(void) strlcat(notify, "SUCCESS,",
					       sizeof notify);
			if (bitset(QPINGONFAILURE, to->q_flags))
				(void) strlcat(notify, "FAILURE,",
					       sizeof notify);
			if (bitset(QPINGONDELAY, to->q_flags))
				(void) strlcat(notify, "DELAY,", sizeof notify);

			/* Set to NEVER or drop trailing comma */
			if (notify[0] == '\0')
				(void) strlcat(notify, "NEVER", sizeof notify);
			else
				notify[strlen(notify) - 1] = '\0';

			define(macid("{dsn_notify}", NULL), newstr(notify), e);
		}
		else
			define(macid("{dsn_notify}", NULL), NULL, e);

		/*
		**  Expand out this user into argument list.
		*/

		if (!clever)
		{
			expand(*mvp, buf, sizeof buf, e);
			*pvp++ = newstr(buf);
			if (pvp >= &pv[MAXPV - 2])
			{
				/* allow some space for trailing parms */
				break;
			}
		}
	}

	/* see if any addresses still exist */
#if _FFR_DYNAMIC_TOBUF
	if (tochain == NULL)
#else /* _FFR_DYNAMIC_TOBUF */
	if (tobuf[0] == '\0')
#endif /* _FFR_DYNAMIC_TOBUF */
	{
		define('g', (char *) NULL, e);
		e->e_to = NULL;
		return 0;
	}

	/* print out messages as full list */
#if _FFR_DYNAMIC_TOBUF
	{
		int l = 1;
		char *tobufptr;

		for (to = tochain; to != NULL; to = to->q_tchain)
			l += strlen(to->q_paddr) + 1;
		if (l < TOBUFSIZE)
			l = TOBUFSIZE;
		if (l > tobufsize)
		{
			if (tobuf != NULL)
				free(tobuf);
			tobufsize = l;
			tobuf = xalloc(tobufsize);
		}
		tobufptr = tobuf;
		*tobufptr = '\0';
		for (to = tochain; to != NULL; to = to->q_tchain)
		{
			snprintf(tobufptr, tobufsize - (tobufptr - tobuf),
				 ",%s", to->q_paddr);
			tobufptr += strlen(tobufptr);
		}
	}
#endif /* _FFR_DYNAMIC_TOBUF */
	e->e_to = tobuf + 1;

	/*
	**  Fill out any parameters after the $u parameter.
	*/

	while (!clever && *++mvp != NULL)
	{
		expand(*mvp, buf, sizeof buf, e);
		*pvp++ = newstr(buf);
		if (pvp >= &pv[MAXPV])
			syserr("554 5.3.0 deliver: pv overflow after $u for %s",
			       pv[0]);
	}
	*pvp++ = NULL;

	/*
	**  Call the mailer.
	**	The argument vector gets built, pipes
	**	are created as necessary, and we fork & exec as
	**	appropriate.
	**	If we are running SMTP, we just need to clean up.
	*/

	/* XXX this seems a bit wierd */
	if (ctladdr == NULL && m != ProgMailer && m != FileMailer &&
	    bitset(QGOODUID, e->e_from.q_flags))
		ctladdr = &e->e_from;

#if NAMED_BIND
	if (ConfigLevel < 2)
		_res.options &= ~(RES_DEFNAMES | RES_DNSRCH);	/* XXX */
#endif /* NAMED_BIND */

	if (tTd(11, 1))
	{
		dprintf("openmailer:");
		printav(pv);
	}
	errno = 0;
#if NAMED_BIND
	h_errno = 0;
#endif /* NAMED_BIND */

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
		snprintf(wbuf, sizeof wbuf, "%s... openmailer(%s)",
			shortenstring(e->e_to, MAXSHORTSTR), m->m_name);
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
		mci = (MCI *) xalloc(sizeof *mci);
		memset((char *) mci, '\0', sizeof *mci);
		mci->mci_in = stdin;
		mci->mci_out = stdout;
		mci->mci_state = clever ? MCIS_OPENING : MCIS_OPEN;
		mci->mci_mailer = m;
	}
	else if (strcmp(m->m_mailer, "[IPC]") == 0 ||
		 strcmp(m->m_mailer, "[TCP]") == 0)
	{
#if DAEMON
		register int i;

		if (pv[0] == NULL || pv[1] == NULL || pv[1][0] == '\0')
		{
			syserr("null destination for %s mailer", m->m_mailer);
			rcode = EX_CONFIG;
			goto give_up;
		}

# if NETUNIX
		if (strcmp(pv[0], "FILE") == 0)
		{
			curhost = CurHostName = "localhost";
			mux_path = pv[1];
		}
		else
# endif /* NETUNIX */
		{
			CurHostName = pv[1];
			curhost = hostsignature(m, pv[1]);
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
# if NETUNIX
		    && mux_path == NULL
# endif /* NETUNIX */
		    )
		{
			port = htons((u_short)atoi(pv[2]));
			if (port == 0)
			{
# ifdef NO_GETSERVBYNAME
				syserr("Invalid port number: %s", pv[2]);
# else /* NO_GETSERVBYNAME */
				struct servent *sp = getservbyname(pv[2], "tcp");

				if (sp == NULL)
					syserr("Service %s unknown", pv[2]);
				else
					port = sp->s_port;
# endif /* NO_GETSERVBYNAME */
			}
		}

		nummxhosts = parse_hostsignature(curhost, mxhosts, m);
tryhost:
		while (hostnum < nummxhosts)
		{
			char sep = ':';
			char *endp;
			static char hostbuf[MAXNAME + 1];

# if NETINET6
			if (*mxhosts[hostnum] == '[')
			{
				endp = strchr(mxhosts[hostnum] + 1, ']');
				if (endp != NULL)
					endp = strpbrk(endp + 1, ":,");
			}
			else
				endp = strpbrk(mxhosts[hostnum], ":,");
# else /* NETINET6 */
			endp = strpbrk(mxhosts[hostnum], ":,");
# endif /* NETINET6 */
			if (endp != NULL)
			{
				sep = *endp;
				*endp = '\0';
			}

			if (*mxhosts[hostnum] == '\0')
			{
				syserr("deliver: null host name in signature");
				hostnum++;
				if (endp != NULL)
					*endp = sep;
				continue;
			}
			(void) strlcpy(hostbuf, mxhosts[hostnum],
				       sizeof hostbuf);
			hostnum++;
			if (endp != NULL)
				*endp = sep;

			/* see if we already know that this host is fried */
			CurHostName = hostbuf;
			mci = mci_get(hostbuf, m);
			if (mci->mci_state != MCIS_CLOSED)
			{
				if (tTd(11, 1))
				{
					dprintf("openmailer: ");
					mci_dump(mci, FALSE);
				}
				CurHostName = mci->mci_host;
				message("Using cached %sSMTP connection to %s via %s...",
					bitset(MCIF_ESMTP, mci->mci_flags) ? "E" : "",
					hostbuf, m->m_name);
				mci->mci_deliveries++;
				break;
			}
			mci->mci_mailer = m;
			if (mci->mci_exitstat != EX_OK)
			{
				if (mci->mci_exitstat == EX_TEMPFAIL)
					goodmxfound = TRUE;
				continue;
			}

			if (mci_lock_host(mci) != EX_OK)
			{
				mci_setstat(mci, EX_TEMPFAIL, "4.4.5", NULL);
				goodmxfound = TRUE;
				continue;
			}

			/* try the connection */
			sm_setproctitle(TRUE, e, "%s %s: %s",
					qid_printname(e),
					hostbuf, "user open");
# if NETUNIX
			if (mux_path != NULL)
			{
				message("Connecting to %s via %s...",
					mux_path, m->m_name);
				i = makeconnection_ds(mux_path, mci);
			}
			else
# endif /* NETUNIX */
			{
				if (port == 0)
					message("Connecting to %s via %s...",
						hostbuf, m->m_name);
				else
					message("Connecting to %s port %d via %s...",
						hostbuf, ntohs(port),
						m->m_name);
				i = makeconnection(hostbuf, port, mci, e);
			}
			mci->mci_lastuse = curtime();
			mci->mci_deliveries = 0;
			mci->mci_exitstat = i;
			mci->mci_errno = errno;
# if NAMED_BIND
			mci->mci_herrno = h_errno;
# endif /* NAMED_BIND */
			if (i == EX_OK)
			{
				goodmxfound = TRUE;
				mci->mci_state = MCIS_OPENING;
				mci_cache(mci);
				if (TrafficLogFile != NULL)
					fprintf(TrafficLogFile, "%05d === CONNECT %s\n",
						(int) getpid(), hostbuf);
				break;
			}
			else
			{
				if (tTd(11, 1))
					dprintf("openmailer: makeconnection => stat=%d, errno=%d\n",
						i, errno);
				if (i == EX_TEMPFAIL)
					goodmxfound = TRUE;
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
#else /* DAEMON */
		syserr("554 5.3.5 openmailer: no IPC");
		if (tTd(11, 1))
			dprintf("openmailer: NULL\n");
		rcode = EX_UNAVAILABLE;
		goto give_up;
#endif /* DAEMON */
	}
	else
	{
		/* flush any expired connections */
		(void) mci_scan(NULL);
		mci = NULL;

#if SMTP
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
#endif /* SMTP */

		/* announce the connection to verbose listeners */
		if (host == NULL || host[0] == '\0')
			message("Connecting to %s...", m->m_name);
		else
			message("Connecting to %s via %s...", host, m->m_name);
		if (TrafficLogFile != NULL)
		{
			char **av;

			fprintf(TrafficLogFile, "%05d === EXEC", (int) getpid());
			for (av = pv; *av != NULL; av++)
				fprintf(TrafficLogFile, " %s", *av);
			fprintf(TrafficLogFile, "\n");
		}

#if XDEBUG
		checkfd012("before creating mail pipe");
#endif /* XDEBUG */

		/* create a pipe to shove the mail through */
		if (pipe(mpvect) < 0)
		{
			syserr("%s... openmailer(%s): pipe (to mailer)",
				shortenstring(e->e_to, MAXSHORTSTR), m->m_name);
			if (tTd(11, 1))
				dprintf("openmailer: NULL\n");
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
			printopenfds(TRUE);
			if (tTd(11, 1))
				dprintf("openmailer: NULL\n");
			rcode = EX_OSERR;
			goto give_up;
		}

		/* make sure system call isn't dead meat */
		checkfdopen(mpvect[0], "mpvect[0]");
		checkfdopen(mpvect[1], "mpvect[1]");
		if (mpvect[0] == mpvect[1] ||
		    (e->e_lockfp != NULL &&
		     (mpvect[0] == fileno(e->e_lockfp) ||
		      mpvect[1] == fileno(e->e_lockfp))))
		{
			if (e->e_lockfp == NULL)
				syserr("%s... openmailer(%s): overlapping mpvect %d %d",
					shortenstring(e->e_to, MAXSHORTSTR),
					m->m_name, mpvect[0], mpvect[1]);
			else
				syserr("%s... openmailer(%s): overlapping mpvect %d %d, lockfp = %d",
					shortenstring(e->e_to, MAXSHORTSTR),
					m->m_name, mpvect[0], mpvect[1],
					fileno(e->e_lockfp));
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
				dprintf("openmailer: NULL\n");
			rcode = EX_OSERR;
			goto give_up;
		}
#if XDEBUG
		checkfdopen(rpvect[0], "rpvect[0]");
		checkfdopen(rpvect[1], "rpvect[1]");
#endif /* XDEBUG */

		/*
		**  Actually fork the mailer process.
		**	DOFORK is clever about retrying.
		**
		**	Dispose of SIGCHLD signal catchers that may be laying
		**	around so that endmailer will get it.
		*/

		if (e->e_xfp != NULL)
			(void) fflush(e->e_xfp);	/* for debugging */
		(void) fflush(stdout);
		(void) setsignal(SIGCHLD, SIG_DFL);


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
				dprintf("openmailer: NULL\n");
			rcode = EX_OSERR;
			goto give_up;
		}
		else if (pid == 0)
		{
			int i;
			int save_errno;
			int new_euid = NO_UID;
			int new_ruid = NO_UID;
			int new_gid = NO_GID;
			struct stat stb;
			extern int DtableSize;

			if (e->e_lockfp != NULL)
				(void) close(fileno(e->e_lockfp));

			/* child -- set up input & exec mailer */
			(void) setsignal(SIGINT, SIG_IGN);
			(void) setsignal(SIGHUP, SIG_IGN);
			(void) setsignal(SIGTERM, SIG_DFL);

			if (m != FileMailer || stat(tochain->q_user, &stb) < 0)
				stb.st_mode = 0;

# if HASSETUSERCONTEXT
			/*
			**  Set user resources.
			*/

			if (contextaddr != NULL)
			{
				struct passwd *pwd;

				if (contextaddr->q_ruser != NULL)
					pwd = sm_getpwnam(contextaddr->q_ruser);
				else
					pwd = sm_getpwnam(contextaddr->q_user);
				if (pwd != NULL)
					(void) setusercontext(NULL,
						pwd, pwd->pw_uid,
						LOGIN_SETRESOURCES|LOGIN_SETPRIORITY);
			}
# endif /* HASSETUSERCONTEXT */

			/* tweak niceness */
			if (m->m_nice != 0)
				(void) nice(m->m_nice);

			/* reset group id */
			if (bitnset(M_SPECIFIC_UID, m->m_flags))
				new_gid = m->m_gid;
			else if (bitset(S_ISGID, stb.st_mode))
				new_gid = stb.st_gid;
			else if (ctladdr != NULL && ctladdr->q_gid != 0)
			{
				if (!DontInitGroups)
				{
					char *u = ctladdr->q_ruser;

					if (u == NULL)
						u = ctladdr->q_user;

					if (initgroups(u, ctladdr->q_gid) == -1 && suidwarn)
					{
						syserr("openmailer: initgroups(%s, %d) failed",
							u, ctladdr->q_gid);
						exit(EX_TEMPFAIL);
					}
				}
				else
				{
					GIDSET_T gidset[1];

					gidset[0] = ctladdr->q_gid;
					if (setgroups(1, gidset) == -1 && suidwarn)
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
					if (initgroups(DefUser, DefGid) == -1 && suidwarn)
					{
						syserr("openmailer: initgroups(%s, %d) failed",
							DefUser, DefGid);
						exit(EX_TEMPFAIL);
					}
				}
				else
				{
					GIDSET_T gidset[1];

					gidset[0] = DefGid;
					if (setgroups(1, gidset) == -1 && suidwarn)
					{
						syserr("openmailer: setgroups() failed");
						exit(EX_TEMPFAIL);
					}
				}
				if (m->m_gid == 0)
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
					syserr("openmailer: insufficient privileges to change gid");
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
				expand(m->m_rootdir, buf, sizeof buf, e);
				if (tTd(11, 20))
					dprintf("openmailer: chroot %s\n",
						buf);
				if (chroot(buf) < 0)
				{
					syserr("openmailer: Cannot chroot(%s)",
					       buf);
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
			if (bitnset(M_SPECIFIC_UID, m->m_flags))
				new_euid = m->m_uid;
			else if (bitset(S_ISUID, stb.st_mode))
				new_ruid = stb.st_uid;
			else if (ctladdr != NULL && ctladdr->q_uid != 0)
				new_ruid = ctladdr->q_uid;
			else if (m->m_uid != 0)
				new_ruid = m->m_uid;
			else
				new_ruid = DefUid;
			if (new_euid != NO_UID)
			{
				if (RunAsUid != 0 && new_euid != RunAsUid)
				{
					/* Only root can change the uid */
					syserr("openmailer: insufficient privileges to change uid");
					exit(EX_TEMPFAIL);
				}

				vendor_set_uid(new_euid);
# if MAILER_SETUID_METHOD == USE_SETEUID
				if (seteuid(new_euid) < 0 && suidwarn)
				{
					syserr("openmailer: seteuid(%ld) failed",
						(long) new_euid);
					exit(EX_TEMPFAIL);
				}
# endif /* MAILER_SETUID_METHOD == USE_SETEUID */
# if MAILER_SETUID_METHOD == USE_SETREUID
				if (setreuid(new_ruid, new_euid) < 0 && suidwarn)
				{
					syserr("openmailer: setreuid(%ld, %ld) failed",
						(long) new_ruid, (long) new_euid);
					exit(EX_TEMPFAIL);
				}
# endif /* MAILER_SETUID_METHOD == USE_SETREUID */
# if MAILER_SETUID_METHOD == USE_SETUID
				if (new_euid != geteuid() && setuid(new_euid) < 0 && suidwarn)
				{
					syserr("openmailer: setuid(%ld) failed",
						(long) new_euid);
					exit(EX_TEMPFAIL);
				}
# endif /* MAILER_SETUID_METHOD == USE_SETUID */
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
				dprintf("openmailer: running as r/euid=%d/%d, r/egid=%d/%d\n",
					(int) getuid(), (int) geteuid(),
					(int) getgid(), (int) getegid());

			/* move into some "safe" directory */
			if (m->m_execdir != NULL)
			{
				char *q;

				for (p = m->m_execdir; p != NULL; p = q)
				{
					q = strchr(p, ':');
					if (q != NULL)
						*q = '\0';
					expand(p, buf, sizeof buf, e);
					if (q != NULL)
						*q++ = ':';
					if (tTd(11, 20))
						dprintf("openmailer: trydir %s\n",
							buf);
					if (buf[0] != '\0' && chdir(buf) >= 0)
						break;
				}
			}

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
			for (i = 3; i < DtableSize; i++)
			{
				register int j;

				if ((j = fcntl(i, F_GETFD, 0)) != -1)
					(void) fcntl(i, F_SETFD,
						     j | FD_CLOEXEC);
			}

			/* run disconnected from terminal */
			(void) setsid();

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
			mci = (MCI *) xalloc(sizeof *mci);
			memset((char *) mci, '\0', sizeof *mci);
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
		mci->mci_out = fdopen(mpvect[1], "w");
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
		mci->mci_in = fdopen(rpvect[0], "r");
		if (mci->mci_in == NULL)
		{
			syserr("deliver: cannot create mailer input channel, fd=%d",
			       mpvect[1]);
			(void) close(rpvect[0]);
			(void) fclose(mci->mci_out);
			mci->mci_out = NULL;
			rcode = EX_OSERR;
			goto give_up;
		}

		/* Don't cache non-clever connections */
		if (!clever)
			mci->mci_flags |= MCIF_TEMP;
	}

	/*
	**  If we are in SMTP opening state, send initial protocol.
	*/

	if (bitnset(M_7BITS, m->m_flags) &&
	    (!clever || mci->mci_state == MCIS_OPENING))
		mci->mci_flags |= MCIF_7BIT;
#if SMTP
	if (clever && mci->mci_state != MCIS_CLOSED)
	{
		static u_short again;
# if SASL && SFIO
#  define DONE_TLS_B	0x01
#  define DONE_TLS	bitset(DONE_TLS_B, again)
# endif /* SASL && SFIO */
# if STARTTLS
#  define DONE_STARTTLS_B	0x02
#  define DONE_STARTTLS	bitset(DONE_STARTTLS_B, again)
# endif /* STARTTLS */
# define ONLY_HELO_B	0x04
# define ONLY_HELO	bitset(ONLY_HELO_B, again)
# define SET_HELO	again |= ONLY_HELO_B
# define CLR_HELO	again &= ~ONLY_HELO_B

		again = 0;
# if STARTTLS || (SASL && SFIO)
reconnect:	/* after switching to an authenticated connection */
# endif /* STARTTLS || (SASL && SFIO) */

# if SASL
		mci->mci_saslcap = NULL;
# endif /* SASL */
		smtpinit(m, mci, e, ONLY_HELO);
		CLR_HELO;

# if STARTTLS
		/* first TLS then AUTH to provide a security layer */
		if (mci->mci_state != MCIS_CLOSED && !DONE_STARTTLS)
		{
			int olderrors;
			bool hasdot;
			bool usetls;
			bool saveQuickAbort = QuickAbort;
			bool saveSuprErrs = SuprErrs;
			extern SOCKADDR CurHostAddr;

			rcode = EX_OK;
			usetls = bitset(MCIF_TLS, mci->mci_flags);
			hasdot = CurHostName[strlen(CurHostName) - 1] == '.';
			if (hasdot)
				CurHostName[strlen(CurHostName) - 1] = '\0';
			define(macid("{server_name}", NULL),
			       newstr(CurHostName), e);
			if (CurHostAddr.sa.sa_family != 0)
				define(macid("{server_addr}", NULL),
				       newstr(anynet_ntoa(&CurHostAddr)), e);
			else
				define(macid("{server_addr}", NULL), NULL, e);
#  if _FFR_TLS_O_T
			if (usetls)
			{
				olderrors = Errors;
				QuickAbort = FALSE;
				SuprErrs = TRUE;
				if (rscheck("try_tls", CurHostName, NULL,
					    e, TRUE, FALSE, 8) != EX_OK
				    || Errors > olderrors)
					usetls = FALSE;
				SuprErrs = saveSuprErrs;
				QuickAbort = saveQuickAbort;
			}
#  endif /* _FFR_TLS_O_T */

			/* undo change of CurHostName */
			if (hasdot)
				CurHostName[strlen(CurHostName)] = '.';
			if (usetls)
			{
				if ((rcode = starttls(m, mci, e)) == EX_OK)
				{
					/* start again without STARTTLS */
					again |= DONE_STARTTLS_B;
					mci->mci_flags |= MCIF_TLSACT;
				}
				else
				{
					char *s;

					/*
					**  TLS negotation failed, what to do?
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
					  case EX_USAGE:
						s = "USAGE";
						break;
					  case EX_PROTOCOL:
						s = "PROTOCOL";
						break;
					  case EX_SOFTWARE:
						s = "SOFTWARE";
						break;

					  /* everything else is a failure */
					  default:
						s = "FAILURE";
						rcode = EX_TEMPFAIL;
					}
					define(macid("{verify}", NULL),
					       newstr(s), e);
				}
			}
			else
				define(macid("{verify}", NULL), "NONE", e);
			olderrors = Errors;
			QuickAbort = FALSE;
			SuprErrs = TRUE;

			/*
			**  rcode == EX_SOFTWARE is special:
			**  the TLS negotation failed
			**  we have to drop the connection no matter what
			**  However, we call tls_server to give it the chance
			**  to log the problem and return an appropriate
			**  error code.
			*/
			if (rscheck("tls_server",
				     macvalue(macid("{verify}", NULL), e),
				     NULL, e, TRUE, TRUE, 6) != EX_OK ||
			    Errors > olderrors ||
			    rcode == EX_SOFTWARE)
			{
				char enhsc[ENHSCLEN];
				extern char MsgBuf[];

				if (ISSMTPCODE(MsgBuf) &&
				    extenhsc(MsgBuf + 4, ' ', enhsc) > 0)
				{
					p = newstr(MsgBuf);
				}
				else
				{
					p = "403 4.7.0 server not authenticated.";
					(void) strlcpy(enhsc, "4.7.0",
						       sizeof enhsc);
				}
				SuprErrs = saveSuprErrs;
				QuickAbort = saveQuickAbort;

				if (rcode == EX_SOFTWARE)
				{
					/* drop the connection */
					mci->mci_state = MCIS_QUITING;
					if (mci->mci_in != NULL)
					{
						(void) fclose(mci->mci_in);
						mci->mci_in = NULL;
					}
					mci->mci_flags &= ~MCIF_TLSACT;
					(void) endmailer(mci, e, pv);
				}
				else
				{
					/* abort transfer */
					smtpquit(m, mci, e);
				}

				/* temp or permanent failure? */
				rcode = (*p == '4') ? EX_TEMPFAIL
						    : EX_UNAVAILABLE;
				mci_setstat(mci, rcode, newstr(enhsc), p);

				/*
				**  hack to get the error message into
				**  the envelope (done in giveresponse())
				*/
				(void) strlcpy(SmtpError, p, sizeof SmtpError);
			}
			QuickAbort = saveQuickAbort;
			SuprErrs = saveSuprErrs;
			if (DONE_STARTTLS && mci->mci_state != MCIS_CLOSED)
			{
				SET_HELO;
				mci->mci_flags &= ~MCIF_EXTENS;
				goto reconnect;
			}
		}
# endif /* STARTTLS */
# if SASL
		/* if other server supports authentication let's authenticate */
		if (mci->mci_state != MCIS_CLOSED &&
		    mci->mci_saslcap != NULL &&
#  if SFIO
		    !DONE_TLS &&
#  endif /* SFIO */
		    SASLInfo != NULL)
		{
			/*
			**  should we require some minimum authentication?
			**  XXX ignore result?
			*/
			if (smtpauth(m, mci, e) == EX_OK)
			{
#  if SFIO
				int result;
				sasl_ssf_t *ssf;

				/* get security strength (features) */
				result = sasl_getprop(mci->mci_conn, SASL_SSF,
						      (void **) &ssf);
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, NOQID,
						  "SASL: outgoing connection to %.64s: mech=%.16s, bits=%d",
						  mci->mci_host,
						  macvalue(macid("{auth_type}",
								 NULL), e),
						  *ssf);
				/*
				**  only switch to encrypted connection
				**  if a security layer has been negotiated
				*/
				if (result == SASL_OK && *ssf > 0)
				{
					/*
					**  convert sfio stuff to use SASL
					**  check return values
					**  if the call fails,
					**  fall back to unencrypted version
					**  unless some cf option requires
					**  encryption then the connection must
					**  be aborted
					*/
					if (sfdcsasl(mci->mci_in, mci->mci_out,
						     mci->mci_conn) == 0)
					{
						again |= DONE_TLS_B;
						SET_HELO;
						mci->mci_flags &= ~MCIF_EXTENS;
						mci->mci_flags |= MCIF_AUTHACT;
						goto reconnect;
					}
					syserr("SASL TLS switch failed in client");
				}
				/* else? XXX */
#  endif /* SFIO */
				mci->mci_flags |= MCIF_AUTHACT;

			}
		}
# endif /* SASL */
	}

#endif /* SMTP */

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
	     (strcasecmp(p, "quoted-printable") == 0 ||
	      strcasecmp(p, "base64") == 0) &&
	    (p = hvalue("Content-Type", e->e_header)) != NULL)
	{
		/* may want to convert 7 -> 8 */
		/* XXX should really parse it here -- and use a class XXX */
		if (strncasecmp(p, "text/plain", 10) == 0 &&
		    (p[10] == '\0' || p[10] == ' ' || p[10] == ';'))
			mci->mci_flags |= MCIF_CVT7TO8;
	}
#endif /* MIME7TO8 */

	if (tTd(11, 1))
	{
		dprintf("openmailer: ");
		mci_dump(mci, FALSE);
	}

	if (mci->mci_state != MCIS_OPEN)
	{
		/* couldn't open the mailer */
		rcode = mci->mci_exitstat;
		errno = mci->mci_errno;
#if NAMED_BIND
		h_errno = mci->mci_herrno;
#endif /* NAMED_BIND */
		if (rcode == EX_OK)
		{
			/* shouldn't happen */
			syserr("554 5.3.5 deliver: mci=%lx rcode=%d errno=%d state=%d sig=%s",
				(u_long) mci, rcode, errno, mci->mci_state,
				firstsig);
			mci_dump_all(TRUE);
			rcode = EX_SOFTWARE;
		}
#if DAEMON
		else if (nummxhosts > hostnum)
		{
			/* try next MX site */
			goto tryhost;
		}
#endif /* DAEMON */
	}
	else if (!clever)
	{
		/*
		**  Format and send message.
		*/

		putfromline(mci, e);
		(*e->e_puthdr)(mci, e->e_header, e, M87F_OUTER);
		(*e->e_putbody)(mci, e, NULL);

		/* get the exit status */
		rcode = endmailer(mci, e, pv);
	}
	else
#if SMTP
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
#if !_FFR_DYNAMIC_TOBUF
				if (strlen(to->q_paddr) +
				    (t - tobuf) + 2 > sizeof tobuf)
				{
					/* not enough room */
					continue;
				}
#endif /* !_FFR_DYNAMIC_TOBUF */

# if STARTTLS
#  if _FFR_TLS_RCPT
				i = rscheck("tls_rcpt", to->q_user, NULL, e,
					    TRUE, TRUE, 4);
				if (i != EX_OK)
				{
					markfailure(e, to, mci, i, FALSE);
					giveresponse(i, to->q_status,  m,
						     mci, ctladdr, xstart, e);
					continue;
				}
#  endif /* _FFR_TLS_RCPT */
# endif /* STARTTLS */

				if ((i = smtprcpt(to, m, mci, e)) != EX_OK)
				{
					markfailure(e, to, mci, i, FALSE);
					giveresponse(i, to->q_status,  m,
						     mci, ctladdr, xstart, e);
				}
				else
				{
					*t++ = ',';
					for (p = to->q_paddr; *p; *t++ = *p++)
						continue;
					*t = '\0';
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
		}
# if DAEMON
		if (rcode == EX_TEMPFAIL && nummxhosts > hostnum)
		{
			/* try next MX site */
			goto tryhost;
		}
# endif /* DAEMON */
	}
#else /* SMTP */
	{
		syserr("554 5.3.5 deliver: need SMTP compiled to use clever mailer");
		rcode = EX_CONFIG;
		goto give_up;
	}
#endif /* SMTP */
#if NAMED_BIND
	if (ConfigLevel < 2)
		_res.options |= RES_DEFNAMES | RES_DNSRCH;	/* XXX */
#endif /* NAMED_BIND */

	if (tTd(62, 1))
		checkfds("after delivery");

	/*
	**  Do final status disposal.
	**	We check for something in tobuf for the SMTP case.
	**	If we got a temporary failure, arrange to queue the
	**		addressees.
	*/

  give_up:
#if SMTP
	if (bitnset(M_LMTP, m->m_flags))
	{
		lmtp_rcode = rcode;
		tobuf[0] = '\0';
		anyok = FALSE;
	}
	else
#endif /* SMTP */
		anyok = rcode == EX_OK;

	for (to = tochain; to != NULL; to = to->q_tchain)
	{
		/* see if address already marked */
		if (!QS_IS_OK(to->q_state))
			continue;

#if SMTP
		/* if running LMTP, get the status for each address */
		if (bitnset(M_LMTP, m->m_flags))
		{
			if (lmtp_rcode == EX_OK)
				rcode = smtpgetstat(m, mci, e);
			if (rcode == EX_OK)
			{
#if !_FFR_DYNAMIC_TOBUF
				if (strlen(to->q_paddr) +
				    strlen(tobuf) + 2 > sizeof tobuf)
				{
					syserr("LMTP tobuf overflow");
				}
				else
#endif /* !_FFR_DYNAMIC_TOBUF */
				{
					(void) strlcat(tobuf, ",",
						       sizeof tobuf);
					(void) strlcat(tobuf, to->q_paddr,
						       sizeof tobuf);
				}
				anyok = TRUE;
			}
			else
			{
				e->e_to = to->q_paddr;
				markfailure(e, to, mci, rcode, TRUE);
				giveresponse(rcode, to->q_status, m, mci,
					     ctladdr, xstart, e);
				e->e_to = tobuf + 1;
				continue;
			}
		}
		else
#endif /* SMTP */
		{
			/* mark bad addresses */
			if (rcode != EX_OK)
			{
				if (goodmxfound && rcode == EX_NOHOST)
					rcode = EX_TEMPFAIL;
				markfailure(e, to, mci, rcode, TRUE);
				continue;
			}
		}

		/* successful delivery */
		to->q_state = QS_SENT;
		to->q_statdate = curtime();
		e->e_nsent++;

#if QUEUE
		/*
		**  Checkpoint the send list every few addresses
		*/

		if (e->e_nsent >= CheckpointInterval)
		{
			queueup(e, FALSE);
			e->e_nsent = 0;
		}
#endif /* QUEUE */

		if (bitnset(M_LOCALMAILER, m->m_flags) &&
		    bitset(QPINGONSUCCESS, to->q_flags))
		{
			to->q_flags |= QDELIVERED;
			to->q_status = "2.1.5";
			fprintf(e->e_xfp, "%s... Successfully delivered\n",
				to->q_paddr);
		}
		else if (bitset(QPINGONSUCCESS, to->q_flags) &&
			 bitset(QPRIMARY, to->q_flags) &&
			 !bitset(MCIF_DSN, mci->mci_flags))
		{
			to->q_flags |= QRELAYED;
			fprintf(e->e_xfp, "%s... relayed; expect no further notifications\n",
				to->q_paddr);
		}
	}

#if SMTP
	if (bitnset(M_LMTP, m->m_flags))
	{
		/*
		**  Global information applies to the last recipient only;
		**  clear it out to avoid bogus errors.
		*/

		rcode = EX_OK;
		e->e_statmsg = NULL;

		/* reset the mci state for the next transaction */
		if (mci != NULL && mci->mci_state == MCIS_ACTIVE)
			mci->mci_state = MCIS_OPEN;
	}
#endif /* SMTP */

	if (tobuf[0] != '\0')
		giveresponse(rcode, NULL, m, mci, ctladdr, xstart, e);
	if (anyok)
		markstats(e, tochain, FALSE);
	mci_store_persistent(mci);

#if SMTP
	/* now close the connection */
	if (clever && mci != NULL && mci->mci_state != MCIS_CLOSED &&
	    !bitset(MCIF_CACHED, mci->mci_flags))
		smtpquit(m, mci, e);
#endif /* SMTP */

	/*
	**  Restore state and return.
	*/

#if XDEBUG
	{
		char wbuf[MAXLINE];

		/* make absolutely certain 0, 1, and 2 are in use */
		snprintf(wbuf, sizeof wbuf, "%s... end of deliver(%s)",
			e->e_to == NULL ? "NO-TO-LIST"
					: shortenstring(e->e_to, MAXSHORTSTR),
			m->m_name);
		checkfd012(wbuf);
	}
#endif /* XDEBUG */

	errno = 0;
	define('g', (char *) NULL, e);
	e->e_to = NULL;
	return rcode;
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

static void
markfailure(e, q, mci, rcode, ovr)
	register ENVELOPE *e;
	register ADDRESS *q;
	register MCI *mci;
	int rcode;
	bool ovr;
{
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
		status = mci->mci_status;
		if (mci->mci_rstatus != NULL)
			rstatus = newstr(mci->mci_rstatus);
		else
			rstatus = NULL;
	}
	else if (e->e_status != NULL)
	{
		status = e->e_status;
		rstatus = NULL;
	}
	else
	{
		switch (rcode)
		{
		  case EX_USAGE:
			status = "5.5.4";
			break;

		  case EX_DATAERR:
			status = "5.5.2";
			break;

		  case EX_NOUSER:
			status = "5.1.1";
			break;

		  case EX_NOHOST:
			status = "5.1.2";
			break;

		  case EX_NOINPUT:
		  case EX_CANTCREAT:
		  case EX_NOPERM:
			status = "5.3.0";
			break;

		  case EX_UNAVAILABLE:
		  case EX_SOFTWARE:
		  case EX_OSFILE:
		  case EX_PROTOCOL:
		  case EX_CONFIG:
			status = "5.5.0";
			break;

		  case EX_OSERR:
		  case EX_IOERR:
			status = "4.5.0";
			break;

		  case EX_TEMPFAIL:
			status = "4.2.0";
			break;
		}
	}

	/* new status? */
	if (status != NULL && *status != '\0' && (ovr || q->q_status == NULL ||
	    *q->q_status == '\0' || *q->q_status < *status))
	{
		q->q_status = status;
		q->q_rstatus = rstatus;
	}
	if (rcode != EX_OK && q->q_rstatus == NULL &&
	    q->q_mailer != NULL && q->q_mailer->m_diagtype != NULL &&
	    strcasecmp(q->q_mailer->m_diagtype, "X-UNIX") == 0)
	{
		char buf[16];

		(void) snprintf(buf, sizeof buf, "%d", rcode);
		q->q_rstatus = newstr(buf);
	}

	q->q_statdate = curtime();
	if (CurHostName != NULL && CurHostName[0] != '\0' &&
	    mci != NULL && !bitset(M_LOCALMAILER, mci->mci_flags))
		q->q_statmta = newstr(CurHostName);
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

static jmp_buf	EndWaitTimeout;

static void
endwaittimeout()
{
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
	EVENT *ev = NULL;


	mci_unlock_host(mci);

#if SASL
	/* shutdown SASL */
	if (bitset(MCIF_AUTHACT, mci->mci_flags))
	{
		sasl_dispose(&mci->mci_conn);
		mci->mci_flags &= ~MCIF_AUTHACT;
	}
#endif /* SASL */

#if STARTTLS
	/* shutdown TLS */
	(void) endtlsclt(mci);
#endif /* STARTTLS */

	/* close output to mailer */
	if (mci->mci_out != NULL)
		(void) fclose(mci->mci_out);

	/* copy any remaining input to transcript */
	if (mci->mci_in != NULL && mci->mci_state != MCIS_ERROR &&
	    e->e_xfp != NULL)
	{
		while (sfgets(buf, sizeof buf, mci->mci_in,
		       TimeOuts.to_quit, "Draining Input") != NULL)
			(void) fputs(buf, e->e_xfp);
	}

	/* now close the input */
	if (mci->mci_in != NULL)
		(void) fclose(mci->mci_in);
	mci->mci_in = mci->mci_out = NULL;
	mci->mci_state = MCIS_CLOSED;

	errno = save_errno;

	/* in the IPC case there is nothing to wait for */
	if (mci->mci_pid == 0)
		return EX_OK;

	/* put a timeout around the wait */
	if (mci->mci_mailer->m_wait > 0)
	{
		if (setjmp(EndWaitTimeout) == 0)
			ev = setevent(mci->mci_mailer->m_wait,
				      endwaittimeout, 0);
		else
		{
			syserr("endmailer %s: wait timeout (%d)",
			       mci->mci_mailer->m_name,
			       mci->mci_mailer->m_wait);
			return EX_TEMPFAIL;
		}
	}

	/* wait for the mailer process, collect status */
	st = waitfor(mci->mci_pid);
	save_errno = errno;
	if (ev != NULL)
		clrevent(ev);
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

		fprintf(e->e_xfp, "Arguments:");
		for (av = pv; *av != NULL; av++)
			fprintf(e->e_xfp, " %s", *av);
		fprintf(e->e_xfp, "\n");
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
**
**	Returns:
**		none.
**
**	Side Effects:
**		Errors may be incremented.
**		ExitStat may be set.
*/

void
giveresponse(status, dsn, m, mci, ctladdr, xstart, e)
	int status;
	char *dsn;
	register MAILER *m;
	register MCI *mci;
	ADDRESS *ctladdr;
	time_t xstart;
	ENVELOPE *e;
{
	register const char *statmsg;
	extern char *SysExMsg[];
	register int i;
	int errnum = errno;
	int off = 4;
	extern int N_SysEx;
	char dsnbuf[ENHSCLEN];
	char buf[MAXLINE];

	if (e == NULL)
		syserr("giveresponse: null envelope");

	/*
	**  Compute status message from code.
	*/

	i = status - EX__BASE;
	if (status == 0)
	{
		statmsg = "250 2.0.0 Sent";
		if (e->e_statmsg != NULL)
		{
			(void) snprintf(buf, sizeof buf, "%s (%s)",
					statmsg,
					shortenstring(e->e_statmsg, 403));
			statmsg = buf;
		}
	}
	else if (i < 0 || i >= N_SysEx)
	{
		(void) snprintf(buf, sizeof buf,
				"554 5.3.0 unknown mailer error %d",
				status);
		status = EX_UNAVAILABLE;
		statmsg = buf;
	}
	else if (status == EX_TEMPFAIL)
	{
		char *bp = buf;

		snprintf(bp, SPACELEFT(buf, bp), "%s", SysExMsg[i] + 1);
		bp += strlen(bp);
#if NAMED_BIND
		if (h_errno == TRY_AGAIN)
			statmsg = errstring(h_errno+E_DNSBASE);
		else
#endif /* NAMED_BIND */
		{
			if (errnum != 0)
				statmsg = errstring(errnum);
			else
			{
#if SMTP
				statmsg = SmtpError;
#else /* SMTP */
				statmsg = NULL;
#endif /* SMTP */
			}
		}
		if (statmsg != NULL && statmsg[0] != '\0')
		{
			switch (errnum)
			{
#ifdef ENETDOWN
			  case ENETDOWN:	/* Network is down */
#endif /* ENETDOWN */
#ifdef ENETUNREACH
			  case ENETUNREACH:	/* Network is unreachable */
#endif /* ENETUNREACH */
#ifdef ENETRESET
			  case ENETRESET:	/* Network dropped connection on reset */
#endif /* ENETRESET */
#ifdef ECONNABORTED
			  case ECONNABORTED:	/* Software caused connection abort */
#endif /* ECONNABORTED */
#ifdef EHOSTDOWN
			  case EHOSTDOWN:	/* Host is down */
#endif /* EHOSTDOWN */
#ifdef EHOSTUNREACH
			  case EHOSTUNREACH:	/* No route to host */
#endif /* EHOSTUNREACH */
				if (mci->mci_host != NULL)
				{
					snprintf(bp, SPACELEFT(buf, bp),
						 ": %s", mci->mci_host);
					bp += strlen(bp);
				}
				break;
			}
			snprintf(bp, SPACELEFT(buf, bp), ": %s", statmsg);
		}
		statmsg = buf;
	}
#if NAMED_BIND
	else if (status == EX_NOHOST && h_errno != 0)
	{
		statmsg = errstring(h_errno + E_DNSBASE);
		(void) snprintf(buf, sizeof buf, "%s (%s)",
			SysExMsg[i] + 1, statmsg);
		statmsg = buf;
	}
#endif /* NAMED_BIND */
	else
	{
		statmsg = SysExMsg[i];
		if (*statmsg++ == ':' && errnum != 0)
		{
			(void) snprintf(buf, sizeof buf, "%s: %s",
				statmsg, errstring(errnum));
			statmsg = buf;
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
				snprintf(dsnbuf, sizeof dsnbuf,
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
			fprintf(e->e_xfp, "%s\n", &MsgBuf[4]);
	}
	else
	{
		char mbuf[ENHSCLEN + 4];

		Errors++;
		if ((off = isenhsc(statmsg + 4, ' ')) > 0 &&
		    off < sizeof mbuf - 4)
		{
			if (dsn == NULL)
			{
				snprintf(dsnbuf, sizeof dsnbuf,
					 "%.*s", off, statmsg + 4);
				dsn = dsnbuf;
			}
			off += 5;
			(void) strlcpy(mbuf, statmsg, off);
			(void) strlcat(mbuf, " %s", sizeof mbuf);
		}
		else
		{
			dsnbuf[0] = '\0';
			(void) snprintf(mbuf, sizeof mbuf, "%.3s %%s", statmsg);
			off = 4;
		}
		usrerr(mbuf, &statmsg[off]);
	}

	/*
	**  Final cleanup.
	**	Log a record of the transaction.  Compute the new
	**	ExitStat -- if we already had an error, stick with
	**	that.
	*/

	if (OpMode != MD_VERIFY && !bitset(EF_VRFYONLY, e->e_flags) &&
	    LogLevel > ((status == EX_TEMPFAIL) ? 8 : (status == EX_OK) ? 7 : 6))
		logdelivery(m, mci, dsn, statmsg + off, ctladdr, xstart, e);

	if (tTd(11, 2))
		dprintf("giveresponse: status=%d, dsn=%s, e->e_message=%s\n",
			status,
			dsn == NULL ? "<NULL>" : dsn,
			e->e_message == NULL ? "<NULL>" : e->e_message);

	if (status != EX_TEMPFAIL)
		setstat(status);
	if (status != EX_OK && (status != EX_TEMPFAIL || e->e_message == NULL))
	{
		if (e->e_message != NULL)
			free(e->e_message);
		e->e_message = newstr(statmsg + off);
	}
	errno = 0;
#if NAMED_BIND
	h_errno = 0;
#endif /* NAMED_BIND */
}
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
**
**	Returns:
**		none
**
**	Side Effects:
**		none
*/

void
logdelivery(m, mci, dsn, status, ctladdr, xstart, e)
	MAILER *m;
	register MCI *mci;
	char *dsn;
	const char *status;
	ADDRESS *ctladdr;
	time_t xstart;
	register ENVELOPE *e;
{
	register char *bp;
	register char *p;
	int l;
	char buf[1024];

#if (SYSLOG_BUFSIZE) >= 256
	/* ctladdr: max 106 bytes */
	bp = buf;
	if (ctladdr != NULL)
	{
		snprintf(bp, SPACELEFT(buf, bp), ", ctladdr=%s",
			 shortenstring(ctladdr->q_paddr, 83));
		bp += strlen(bp);
		if (bitset(QGOODUID, ctladdr->q_flags))
		{
			(void) snprintf(bp, SPACELEFT(buf, bp), " (%d/%d)",
					(int) ctladdr->q_uid,
					(int) ctladdr->q_gid);
			bp += strlen(bp);
		}
	}

	/* delay & xdelay: max 41 bytes */
	snprintf(bp, SPACELEFT(buf, bp), ", delay=%s",
		 pintvl(curtime() - e->e_ctime, TRUE));
	bp += strlen(bp);

	if (xstart != (time_t) 0)
	{
		snprintf(bp, SPACELEFT(buf, bp), ", xdelay=%s",
			 pintvl(curtime() - xstart, TRUE));
		bp += strlen(bp);
	}

	/* mailer: assume about 19 bytes (max 10 byte mailer name) */
	if (m != NULL)
	{
		snprintf(bp, SPACELEFT(buf, bp), ", mailer=%s", m->m_name);
		bp += strlen(bp);
	}

	/* pri: changes with each delivery attempt */
	snprintf(bp, SPACELEFT(buf, bp), ", pri=%ld", e->e_msgpriority);
	bp += strlen(bp);

	/* relay: max 66 bytes for IPv4 addresses */
	if (mci != NULL && mci->mci_host != NULL)
	{
# if DAEMON
		extern SOCKADDR CurHostAddr;
# endif /* DAEMON */

		snprintf(bp, SPACELEFT(buf, bp), ", relay=%s",
			 shortenstring(mci->mci_host, 40));
		bp += strlen(bp);

# if DAEMON
		if (CurHostAddr.sa.sa_family != 0)
		{
			snprintf(bp, SPACELEFT(buf, bp), " [%s]",
				 anynet_ntoa(&CurHostAddr));
		}
# endif /* DAEMON */
	}
	else if (strcmp(status, "queued") != 0)
	{
		p = macvalue('h', e);
		if (p != NULL && p[0] != '\0')
		{
			snprintf(bp, SPACELEFT(buf, bp), ", relay=%s",
				 shortenstring(p, 40));
		}
	}
	bp += strlen(bp);

	/* dsn */
	if (dsn != NULL && *dsn != '\0')
	{
		snprintf(bp, SPACELEFT(buf, bp), ", dsn=%s",
			 shortenstring(dsn, ENHSCLEN));
		bp += strlen(bp);
	}

# define STATLEN		(((SYSLOG_BUFSIZE) - 100) / 4)
# if (STATLEN) < 63
#  undef STATLEN
#  define STATLEN	63
# endif /* (STATLEN) < 63 */
# if (STATLEN) > 203
#  undef STATLEN
#  define STATLEN	203
# endif /* (STATLEN) > 203 */

	/* stat: max 210 bytes */
	if ((bp - buf) > (sizeof buf - ((STATLEN) + 20)))
	{
		/* desperation move -- truncate data */
		bp = buf + sizeof buf - ((STATLEN) + 17);
		(void) strlcpy(bp, "...", SPACELEFT(buf, bp));
		bp += 3;
	}

	(void) strlcpy(bp, ", stat=", SPACELEFT(buf, bp));
	bp += strlen(bp);

	(void) strlcpy(bp, shortenstring(status, STATLEN), SPACELEFT(buf, bp));

	/* id, to: max 13 + TOBUFSIZE bytes */
	l = SYSLOG_BUFSIZE - 100 - strlen(buf);
	p = e->e_to == NULL ? "NO-TO-LIST" : e->e_to;
	while (strlen(p) >= (SIZE_T) l)
	{
		register char *q;

#if _FFR_DYNAMIC_TOBUF
		for (q = p + l; q > p; q--)
		{
			if (*q == ',')
				break;
		}
		if (p == q)
			break;
#else /* _FFR_DYNAMIC_TOBUF */
		q = strchr(p + l, ',');
		if (q == NULL)
			break;
#endif /* _FFR_DYNAMIC_TOBUF */

		sm_syslog(LOG_INFO, e->e_id,
			  "to=%.*s [more]%s",
			  ++q - p, p, buf);
		p = q;
	}
#if _FFR_DYNAMIC_TOBUF
	sm_syslog(LOG_INFO, e->e_id, "to=%.*s%s", l, p, buf);
#else /* _FFR_DYNAMIC_TOBUF */
	sm_syslog(LOG_INFO, e->e_id, "to=%s%s", p, buf);
#endif /* _FFR_DYNAMIC_TOBUF */

#else /* (SYSLOG_BUFSIZE) >= 256 */

	l = SYSLOG_BUFSIZE - 85;
	p = e->e_to == NULL ? "NO-TO-LIST" : e->e_to;
	while (strlen(p) >= (SIZE_T) l)
	{
		register char *q;

#if _FFR_DYNAMIC_TOBUF
		for (q = p + l; q > p; q--)
		{
			if (*q == ',')
				break;
		}
		if (p == q)
			break;
#else /* _FFR_DYNAMIC_TOBUF */
		q = strchr(p + l, ',');
		if (q == NULL)
			break;
#endif /* _FFR_DYNAMIC_TOBUF */

		sm_syslog(LOG_INFO, e->e_id,
			  "to=%.*s [more]",
			  ++q - p, p);
		p = q;
	}
#if _FFR_DYNAMIC_TOBUF
	sm_syslog(LOG_INFO, e->e_id, "to=%.*s", l, p);
#else /* _FFR_DYNAMIC_TOBUF */
	sm_syslog(LOG_INFO, e->e_id, "to=%s", p);
#endif /* _FFR_DYNAMIC_TOBUF */

	if (ctladdr != NULL)
	{
		bp = buf;
		snprintf(bp, SPACELEFT(buf, bp), "ctladdr=%s",
			 shortenstring(ctladdr->q_paddr, 83));
		bp += strlen(bp);
		if (bitset(QGOODUID, ctladdr->q_flags))
		{
			(void) snprintf(bp, SPACELEFT(buf, bp), " (%d/%d)",
					ctladdr->q_uid, ctladdr->q_gid);
			bp += strlen(bp);
		}
		sm_syslog(LOG_INFO, e->e_id, "%s", buf);
	}
	bp = buf;
	snprintf(bp, SPACELEFT(buf, bp), "delay=%s",
		 pintvl(curtime() - e->e_ctime, TRUE));
	bp += strlen(bp);
	if (xstart != (time_t) 0)
	{
		snprintf(bp, SPACELEFT(buf, bp), ", xdelay=%s",
			 pintvl(curtime() - xstart, TRUE));
		bp += strlen(bp);
	}

	if (m != NULL)
	{
		snprintf(bp, SPACELEFT(buf, bp), ", mailer=%s", m->m_name);
		bp += strlen(bp);
	}
	sm_syslog(LOG_INFO, e->e_id, "%.1000s", buf);

	buf[0] = '\0';
	bp = buf;
	if (mci != NULL && mci->mci_host != NULL)
	{
# if DAEMON
		extern SOCKADDR CurHostAddr;
# endif /* DAEMON */

		snprintf(bp, SPACELEFT(buf, bp), "relay=%.100s", mci->mci_host);
		bp += strlen(bp);

# if DAEMON
		if (CurHostAddr.sa.sa_family != 0)
			snprintf(bp, SPACELEFT(buf, bp), " [%.100s]",
				anynet_ntoa(&CurHostAddr));
# endif /* DAEMON */
	}
	else if (strcmp(status, "queued") != 0)
	{
		p = macvalue('h', e);
		if (p != NULL && p[0] != '\0')
			snprintf(buf, sizeof buf, "relay=%.100s", p);
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
**		none
**
**	Side Effects:
**		outputs some text to fp.
*/

void
putfromline(mci, e)
	register MCI *mci;
	ENVELOPE *e;
{
	char *template = UnixFromLine;
	char buf[MAXLINE];
	char xbuf[MAXLINE];

	if (bitnset(M_NHDR, mci->mci_mailer->m_flags))
		return;

	mci->mci_flags |= MCIF_INHEADER;

	if (bitnset(M_UGLYUUCP, mci->mci_mailer->m_flags))
	{
		char *bang;

		expand("\201g", buf, sizeof buf, e);
		bang = strchr(buf, '!');
		if (bang == NULL)
		{
			char *at;
			char hname[MAXNAME];

			/*
			**  If we can construct a UUCP path, do so
			*/

			at = strrchr(buf, '@');
			if (at == NULL)
			{
				expand("\201k", hname, sizeof hname, e);
				at = hname;
			}
			else
				*at++ = '\0';
			(void) snprintf(xbuf, sizeof xbuf,
				"From %.800s  \201d remote from %.100s\n",
				buf, at);
		}
		else
		{
			*bang++ = '\0';
			(void) snprintf(xbuf, sizeof xbuf,
				"From %.800s  \201d remote from %.100s\n",
				bang, buf);
			template = xbuf;
		}
	}
	expand(template, buf, sizeof buf, e);
	putxline(buf, strlen(buf), mci, PXLF_HEADER);
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
**		none.
**
**	Side Effects:
**		The message is written onto fp.
*/

/* values for output state variable */
#define OS_HEAD		0	/* at beginning of line */
#define OS_CR		1	/* read a carriage return */
#define OS_INLINE	2	/* putting rest of line */

void
putbody(mci, e, separator)
	register MCI *mci;
	register ENVELOPE *e;
	char *separator;
{
	bool dead = FALSE;
	char buf[MAXLINE];
	char *boundaries[MAXMIMENESTING + 1];

	/*
	**  Output the body of the message
	*/

	if (e->e_dfp == NULL && bitset(EF_HAS_DF, e->e_flags))
	{
		char *df = queuename(e, 'd');

		e->e_dfp = fopen(df, "r");
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
			putline("", mci);
			mci->mci_flags &= ~MCIF_INHEADER;
		}
		putline("<<< No Message Collected >>>", mci);
		goto endofmessage;
	}

	if (e->e_dfino == (ino_t) 0)
	{
		struct stat stbuf;

		if (fstat(fileno(e->e_dfp), &stbuf) < 0)
			e->e_dfino = -1;
		else
		{
			e->e_dfdev = stbuf.st_dev;
			e->e_dfino = stbuf.st_ino;
		}
	}

	/* paranoia: the df file should always be in a rewound state */
	(void) bfrewind(e->e_dfp);

#if MIME8TO7
	if (bitset(MCIF_CVT8TO7, mci->mci_flags))
	{
		/*
		**  Do 8 to 7 bit MIME conversion.
		*/

		/* make sure it looks like a MIME message */
		if (hvalue("MIME-Version", e->e_header) == NULL)
			putline("MIME-Version: 1.0", mci);

		if (hvalue("Content-Type", e->e_header) == NULL)
		{
			snprintf(buf, sizeof buf,
				"Content-Type: text/plain; charset=%s",
				defcharset(e));
			putline(buf, mci);
		}

		/* now do the hard work */
		boundaries[0] = NULL;
		mci->mci_flags |= MCIF_INHEADER;
		(void) mime8to7(mci, e->e_header, e, boundaries, M87F_OUTER);
	}
# if MIME7TO8
	else if (bitset(MCIF_CVT7TO8, mci->mci_flags))
	{
		(void) mime7to8(mci, e->e_header, e);
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
			SuprErrs = TRUE;

		(void) mime8to7(mci, e->e_header, e, boundaries,
				M87F_OUTER|M87F_NO8TO7);

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
			putline("", mci);
			mci->mci_flags &= ~MCIF_INHEADER;
		}

		/* determine end of buffer; allow for short mailer lines */
		buflim = &buf[sizeof buf - 1];
		if (mci->mci_mailer->m_linelimit > 0 &&
		    mci->mci_mailer->m_linelimit < sizeof buf - 1)
			buflim = &buf[mci->mci_mailer->m_linelimit - 1];

		/* copy temp file to output with mapping */
		ostate = OS_HEAD;
		bp = buf;
		pbp = peekbuf;
		while (!ferror(mci->mci_out) && !dead)
		{
			if (pbp > peekbuf)
				c = *--pbp;
			else if ((c = getc(e->e_dfp)) == EOF)
				break;
			if (bitset(MCIF_7BIT, mci->mci_flags))
				c &= 0x7f;
			switch (ostate)
			{
			  case OS_HEAD:
#if _FFR_NONULLS
				if (c == '\0' &&
				    bitnset(M_NONULLS, mci->mci_mailer->m_flags))
					break;
#endif /* _FFR_NONULLS */
				if (c != '\r' && c != '\n' && bp < buflim)
				{
					*bp++ = c;
					break;
				}

				/* check beginning of line for special cases */
				*bp = '\0';
				pos = 0;
				padc = EOF;
				if (buf[0] == 'F' &&
				    bitnset(M_ESCFROM, mci->mci_mailer->m_flags) &&
				    strncmp(buf, "From ", 5) == 0)
				{
					padc = '>';
				}
				if (buf[0] == '-' && buf[1] == '-' &&
				    separator != NULL)
				{
					/* possible separator */
					int sl = strlen(separator);

					if (strncmp(&buf[2], separator, sl) == 0)
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
					fprintf(TrafficLogFile, "%05d >>> ",
						(int) getpid());
					if (padc != EOF)
						(void) putc(padc,
							    TrafficLogFile);
					for (xp = buf; xp < bp; xp++)
						(void) putc((unsigned char) *xp,
							    TrafficLogFile);
					if (c == '\n')
						(void) fputs(mci->mci_mailer->m_eol,
						      TrafficLogFile);
				}
				if (padc != EOF)
				{
					if (putc(padc, mci->mci_out) == EOF)
					{
						dead = TRUE;
						continue;
					}
					pos++;
				}
				for (xp = buf; xp < bp; xp++)
				{
					if (putc((unsigned char) *xp,
						 mci->mci_out) == EOF)
					{
						dead = TRUE;
						break;
					}

					/* record progress for DATA timeout */
					DataProgress = TRUE;
				}
				if (dead)
					continue;
				if (c == '\n')
				{
					if (fputs(mci->mci_mailer->m_eol,
						  mci->mci_out) == EOF)
						break;
					pos = 0;
				}
				else
				{
					pos += bp - buf;
					if (c != '\r')
						*pbp++ = c;
				}

				/* record progress for DATA timeout */
				DataProgress = TRUE;
				bp = buf;

				/* determine next state */
				if (c == '\n')
					ostate = OS_HEAD;
				else if (c == '\r')
					ostate = OS_CR;
				else
					ostate = OS_INLINE;
				continue;

			  case OS_CR:
				if (c == '\n')
				{
					/* got CRLF */
					if (fputs(mci->mci_mailer->m_eol,
						  mci->mci_out) == EOF)
						continue;

					/* record progress for DATA timeout */
					DataProgress = TRUE;

					if (TrafficLogFile != NULL)
					{
						(void) fputs(mci->mci_mailer->m_eol,
							     TrafficLogFile);
					}
					ostate = OS_HEAD;
					continue;
				}

				/* had a naked carriage return */
				*pbp++ = c;
				c = '\r';
				ostate = OS_INLINE;
				goto putch;

			  case OS_INLINE:
				if (c == '\r')
				{
					ostate = OS_CR;
					continue;
				}
#if _FFR_NONULLS
				if (c == '\0' &&
				    bitnset(M_NONULLS, mci->mci_mailer->m_flags))
					break;
#endif /* _FFR_NONULLS */
putch:
				if (mci->mci_mailer->m_linelimit > 0 &&
				    pos >= mci->mci_mailer->m_linelimit - 1 &&
				    c != '\n')
				{
					int d;

					/* check next character for EOL */
					if (pbp > peekbuf)
						d = *(pbp - 1);
					else if ((d = getc(e->e_dfp)) != EOF)
						*pbp++ = d;

					if (d == '\n' || d == EOF)
					{
						if (TrafficLogFile != NULL)
							(void) putc((unsigned char) c,
							    TrafficLogFile);
						if (putc((unsigned char) c,
							 mci->mci_out) == EOF)
						{
							dead = TRUE;
							continue;
						}
						pos++;
						continue;
					}

					if (putc('!', mci->mci_out) == EOF ||
					    fputs(mci->mci_mailer->m_eol,
						  mci->mci_out) == EOF)
					{
						dead = TRUE;
						continue;
					}

					/* record progress for DATA timeout */
					DataProgress = TRUE;

					if (TrafficLogFile != NULL)
					{
						fprintf(TrafficLogFile, "!%s",
							mci->mci_mailer->m_eol);
					}
					ostate = OS_HEAD;
					*pbp++ = c;
					continue;
				}
				if (c == '\n')
				{
					if (TrafficLogFile != NULL)
						(void) fputs(mci->mci_mailer->m_eol,
						      TrafficLogFile);
					if (fputs(mci->mci_mailer->m_eol,
						  mci->mci_out) == EOF)
						continue;
					pos = 0;
					ostate = OS_HEAD;
				}
				else
				{
					if (TrafficLogFile != NULL)
						(void) putc((unsigned char) c,
							    TrafficLogFile);
					if (putc((unsigned char) c,
						 mci->mci_out) == EOF)
					{
						dead = TRUE;
						continue;
					}
					pos++;
					ostate = OS_INLINE;
				}

				/* record progress for DATA timeout */
				DataProgress = TRUE;
				break;
			}
		}

		/* make sure we are at the beginning of a line */
		if (bp > buf)
		{
			if (TrafficLogFile != NULL)
			{
				for (xp = buf; xp < bp; xp++)
					(void) putc((unsigned char) *xp,
						    TrafficLogFile);
			}
			for (xp = buf; xp < bp; xp++)
			{
				if (putc((unsigned char) *xp, mci->mci_out) ==
				    EOF)
				{
					dead = TRUE;
					break;
				}

				/* record progress for DATA timeout */
				DataProgress = TRUE;
			}
			pos += bp - buf;
		}
		if (!dead && pos > 0)
		{
			if (TrafficLogFile != NULL)
				(void) fputs(mci->mci_mailer->m_eol,
					     TrafficLogFile);
			(void) fputs(mci->mci_mailer->m_eol, mci->mci_out);

			/* record progress for DATA timeout */
			DataProgress = TRUE;
		}
	}

	if (ferror(e->e_dfp))
	{
		syserr("putbody: %s/df%s: read error",
		       qid_printqueue(e->e_queuedir), e->e_id);
		ExitStat = EX_IOERR;
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

	if (e->e_dfp != NULL)
		(void) bfrewind(e->e_dfp);

	/* some mailers want extra blank line at end of message */
	if (!dead && bitnset(M_BLANKEND, mci->mci_mailer->m_flags) &&
	    buf[0] != '\0' && buf[0] != '\n')
		putline("", mci);

	(void) fflush(mci->mci_out);
	if (ferror(mci->mci_out) && errno != EPIPE)
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

static jmp_buf	CtxMailfileTimeout;

int
mailfile(filename, mailer, ctladdr, sfflags, e)
	char *volatile filename;
	MAILER *volatile mailer;
	ADDRESS *ctladdr;
	volatile long sfflags;
	register ENVELOPE *e;
{
	register FILE *f;
	register pid_t pid = -1;
	volatile int mode;
	int len;
	off_t curoff;
	bool suidwarn = geteuid() == 0;
	char *p;
	char *volatile realfile;
	EVENT *ev;
	char buf[MAXLINE + 1];
	char targetfile[MAXPATHLEN + 1];

	if (tTd(11, 1))
	{
		dprintf("mailfile %s\n  ctladdr=", filename);
		printaddr(ctladdr, FALSE);
	}

	if (mailer == NULL)
		mailer = FileMailer;

	if (e->e_xfp != NULL)
		(void) fflush(e->e_xfp);

	/*
	**  Special case /dev/null.  This allows us to restrict file
	**  delivery to regular files only.
	*/

	if (strcmp(filename, "/dev/null") == 0)
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
		return EX_DATAERR;
	}

	/* Find the actual file */
	if (SafeFileEnv != NULL && SafeFileEnv[0] != '\0')
	{
		len = strlen(SafeFileEnv);

		if (strncmp(SafeFileEnv, filename, len) == 0)
			filename += len;

		if (len + strlen(filename) + 1 > MAXPATHLEN)
		{
			syserr("mailfile: filename too long (%s/%s)",
			       SafeFileEnv, filename);
			return EX_CANTCREAT;
		}
		(void) strlcpy(targetfile, SafeFileEnv, sizeof targetfile);
		realfile = targetfile + len;
		if (targetfile[len - 1] != '/')
			(void) strlcat(targetfile, "/", sizeof targetfile);
		if (*filename == '/')
			filename++;
		(void) strlcat(targetfile, filename, sizeof targetfile);
	}
	else if (mailer->m_rootdir != NULL)
	{
		expand(mailer->m_rootdir, targetfile, sizeof targetfile, e);
		len = strlen(targetfile);

		if (strncmp(targetfile, filename, len) == 0)
			filename += len;

		if (len + strlen(filename) + 1 > MAXPATHLEN)
		{
			syserr("mailfile: filename too long (%s/%s)",
			       targetfile, filename);
			return EX_CANTCREAT;
		}
		realfile = targetfile + len;
		if (targetfile[len - 1] != '/')
			(void) strlcat(targetfile, "/", sizeof targetfile);
		if (*filename == '/')
			(void) strlcat(targetfile, filename + 1,
				       sizeof targetfile);
		else
			(void) strlcat(targetfile, filename, sizeof targetfile);
	}
	else
	{
		if (strlen(filename) > MAXPATHLEN)
		{
			syserr("mailfile: filename too long (%s)", filename);
			return EX_CANTCREAT;
		}
		(void) strlcpy(targetfile, filename, sizeof targetfile);
		realfile = targetfile;
	}

	/*
	**  Fork so we can change permissions here.
	**	Note that we MUST use fork, not vfork, because of
	**	the complications of calling subroutines, etc.
	*/

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

		if (e->e_lockfp != NULL)
			(void) close(fileno(e->e_lockfp));

		(void) setsignal(SIGINT, SIG_DFL);
		(void) setsignal(SIGHUP, SIG_DFL);
		(void) setsignal(SIGTERM, SIG_DFL);
		(void) umask(OldUmask);
		e->e_to = filename;
		ExitStat = EX_OK;

		if (setjmp(CtxMailfileTimeout) != 0)
		{
			exit(EX_TEMPFAIL);
		}

		if (TimeOuts.to_fileopen > 0)
			ev = setevent(TimeOuts.to_fileopen, mailfiletimeout, 0);
		else
			ev = NULL;

		/* check file mode to see if setuid */
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
			/* ignore setuid and setgid bits */
			mode &= ~(S_ISGID|S_ISUID);
			if (tTd(11, 20))
				dprintf("mailfile: ignoring setuid/setgid bits\n");
		}

		/* we have to open the dfile BEFORE setuid */
		if (e->e_dfp == NULL && bitset(EF_HAS_DF, e->e_flags))
		{
			char *df = queuename(e, 'd');

			e->e_dfp = fopen(df, "r");
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
				RealUid = mailer->m_uid;
				if (RunAsUid != 0 && RealUid != RunAsUid)
				{
					/* Only root can change the uid */
					syserr("mailfile: insufficient privileges to change uid");
					exit(EX_TEMPFAIL);
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
			else if (mailer != NULL && mailer->m_uid != 0)
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
				RealGid = mailer->m_gid;
				if (RunAsUid != 0 &&
				    (RealGid != getgid() ||
				     RealGid != getegid()))
				{
					/* Only root can change the gid */
					syserr("mailfile: insufficient privileges to change gid");
					exit(EX_TEMPFAIL);
				}
			}
			else if (bitset(S_ISGID, mode))
				RealGid = stb.st_gid;
			else if (ctladdr != NULL && ctladdr->q_uid != 0)
				RealGid = ctladdr->q_gid;
			else if (ctladdr != NULL &&
				 ctladdr->q_uid == DefUid &&
				 ctladdr->q_gid == 0)
				RealGid = DefGid;
			else if (mailer != NULL && mailer->m_gid != 0)
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
				syserr("mailfile: initgroups(%s, %d) failed",
					RealUserName, RealGid);
				exit(EX_TEMPFAIL);
			}
		}
		else
		{
			GIDSET_T gidset[1];

			gidset[0] = RealGid;
			if (setgroups(1, gidset) == -1 && suidwarn)
			{
				syserr("mailfile: setgroups() failed");
				exit(EX_TEMPFAIL);
			}
		}

		/*
		**  If you have a safe environment, go into it.
		*/

		if (realfile != targetfile)
		{
			*realfile = '\0';
			if (tTd(11, 20))
				dprintf("mailfile: chroot %s\n", targetfile);
			if (chroot(targetfile) < 0)
			{
				syserr("mailfile: Cannot chroot(%s)",
				       targetfile);
				exit(EX_CANTCREAT);
			}
			*realfile = '/';
		}

		if (tTd(11, 40))
			dprintf("mailfile: deliver to %s\n", realfile);

		if (chdir("/") < 0)
		{
			syserr("mailfile: cannot chdir(/)");
			exit(EX_CANTCREAT);
		}

		/* now reset the group and user ids */
		endpwent();
		if (setgid(RealGid) < 0 && suidwarn)
		{
			syserr("mailfile: setgid(%ld) failed", (long) RealGid);
			exit(EX_TEMPFAIL);
		}
		vendor_set_uid(RealUid);
		if (setuid(RealUid) < 0 && suidwarn)
		{
			syserr("mailfile: setuid(%ld) failed", (long) RealUid);
			exit(EX_TEMPFAIL);
		}

		if (tTd(11, 2))
			dprintf("mailfile: running as r/euid=%d/%d, r/egid=%d/%d\n",
				(int) getuid(), (int) geteuid(),
				(int) getgid(), (int) getegid());


		/* move into some "safe" directory */
		if (mailer->m_execdir != NULL)
		{
			char *q;

			for (p = mailer->m_execdir; p != NULL; p = q)
			{
				q = strchr(p, ':');
				if (q != NULL)
					*q = '\0';
				expand(p, buf, sizeof buf, e);
				if (q != NULL)
					*q++ = ':';
				if (tTd(11, 20))
					dprintf("mailfile: trydir %s\n", buf);
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
				       errstring(errno));
				exit(EX_TEMPFAIL);
			}
			else
			{
				usrerr("554 5.3.0 cannot open %s: %s",
				       shortenstring(realfile, MAXSHORTSTR),
				       errstring(errno));
				exit(EX_CANTCREAT);
			}
		}
		if (filechanged(realfile, fileno(f), &stb))
		{
			syserr("554 5.3.0 file changed after open");
			exit(EX_CANTCREAT);
		}
		if (fstat(fileno(f), &stb) < 0)
		{
			syserr("554 5.3.0 cannot fstat %s", errstring(errno));
			exit(EX_CANTCREAT);
		}

		curoff = stb.st_size;

		if (ev != NULL)
			clrevent(ev);

		memset(&mcibuf, '\0', sizeof mcibuf);
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
		    (strcasecmp(p, "quoted-printable") == 0 ||
		     strcasecmp(p, "base64") == 0) &&
		    (p = hvalue("Content-Type", e->e_header)) != NULL)
		{
			/* may want to convert 7 -> 8 */
			/* XXX should really parse it here -- and use a class XXX */
			if (strncasecmp(p, "text/plain", 10) == 0 &&
			    (p[10] == '\0' || p[10] == ' ' || p[10] == ';'))
				mcibuf.mci_flags |= MCIF_CVT7TO8;
		}
#endif /* MIME7TO8 */

		putfromline(&mcibuf, e);
		(*e->e_puthdr)(&mcibuf, e->e_header, e, M87F_OUTER);
		(*e->e_putbody)(&mcibuf, e, NULL);
		putline("\n", &mcibuf);
		if (fflush(f) < 0 ||
		    (SuperSafe && fsync(fileno(f)) < 0) ||
		    ferror(f))
		{
			setstat(EX_IOERR);
#if !NOFTRUNCATE
			(void) ftruncate(fileno(f), curoff);
#endif /* !NOFTRUNCATE */
		}

		/* reset ISUID & ISGID bits for paranoid systems */
#if HASFCHMOD
		(void) fchmod(fileno(f), (MODE_T) mode);
#else /* HASFCHMOD */
		(void) chmod(filename, (MODE_T) mode);
#endif /* HASFCHMOD */
		if (fclose(f) < 0)
			setstat(EX_IOERR);
		(void) fflush(stdout);
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
			return (WEXITSTATUS(st));
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
mailfiletimeout()
{
	longjmp(CtxMailfileTimeout, 1);
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
**
**	Returns:
**		The signature for this host.
**
**	Side Effects:
**		Can tweak the symbol table.
*/
#define MAXHOSTSIGNATURE	8192	/* max len of hostsignature */

static char *
hostsignature(m, host)
	register MAILER *m;
	char *host;
{
	register char *p;
	register STAB *s;
#if NAMED_BIND
	char sep = ':';
	char prevsep = ':';
	int i;
	int len;
	int nmx;
	int hl;
	char *hp;
	char *endp;
	int oldoptions = _res.options;
	char *mxhosts[MAXMXHOSTS + 1];
	u_short mxprefs[MAXMXHOSTS + 1];
#endif /* NAMED_BIND */

	if (tTd(17, 3))
		dprintf("hostsignature(%s)\n", host);

	/*
	**  If local delivery, just return a constant.
	*/

	if (bitnset(M_LOCALMAILER, m->m_flags))
		return "localhost";

	/*
	**  Check to see if this uses IPC -- if not, it can't have MX records.
	*/

	p = m->m_mailer;
	if (strcmp(p, "[IPC]") != 0 &&
	    strcmp(p, "[TCP]") != 0)
	{
		/* just an ordinary mailer */
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

	s = stab(host, ST_HOSTSIG, ST_ENTER);
	if (s->s_hostsig != NULL)
	{
		if (tTd(17, 3))
			dprintf("hostsignature(): stab(%s) found %s\n", host,
				s->s_hostsig);
		return s->s_hostsig;
	}

	/*
	**  Not already there -- create a signature.
	*/

#if NAMED_BIND
	if (ConfigLevel < 2)
		_res.options &= ~(RES_DEFNAMES | RES_DNSRCH);	/* XXX */

	for (hp = host; hp != NULL; hp = endp)
	{
#if NETINET6
		if (*hp == '[')
		{
			endp = strchr(hp + 1, ']');
			if (endp != NULL)
				endp = strpbrk(endp + 1, ":,");
		}
		else
			endp = strpbrk(hp, ":,");
#else /* NETINET6 */
		endp = strpbrk(hp, ":,");
#endif /* NETINET6 */
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

			nmx = getmxrr(hp, mxhosts, mxprefs, TRUE, &rcode);
			if (nmx <= 0)
			{
				register MCI *mci;

				/* update the connection info for this host */
				mci = mci_get(hp, m);
				mci->mci_errno = errno;
				mci->mci_herrno = h_errno;
				mci->mci_lastuse = curtime();
				if (rcode == EX_NOHOST)
					mci_setstat(mci, rcode, "5.1.2",
						"550 Host unknown");
				else
					mci_setstat(mci, rcode, NULL, NULL);

				/* use the original host name as signature */
				nmx = 1;
				mxhosts[0] = hp;
			}
			if (tTd(17, 3))
				dprintf("hostsignature(): getmxrr() returned %d, mxhosts[0]=%s\n",
					nmx, mxhosts[0]);
		}

		len = 0;
		for (i = 0; i < nmx; i++)
			len += strlen(mxhosts[i]) + 1;
		if (s->s_hostsig != NULL)
			len += strlen(s->s_hostsig) + 1;
		if (len >= MAXHOSTSIGNATURE)
		{
			sm_syslog(LOG_WARNING, NOQID, "hostsignature for host '%s' exceeds maxlen (%d): %d",
				  host, MAXHOSTSIGNATURE, len);
			len = MAXHOSTSIGNATURE;
		}
		p = xalloc(len);
		if (s->s_hostsig != NULL)
		{
			(void) strlcpy(p, s->s_hostsig, len);
			free(s->s_hostsig);
			s->s_hostsig = p;
			hl = strlen(p);
			p += hl;
			*p++ = prevsep;
			len -= hl + 1;
		}
		else
			s->s_hostsig = p;
		for (i = 0; i < nmx; i++)
		{
			hl = strlen(mxhosts[i]);
			if (len - 1 < hl || len <= 1)
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
			(void) strlcpy(p, mxhosts[i], len);
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
	makelower(s->s_hostsig);
	if (ConfigLevel < 2)
		_res.options = oldoptions;
#else /* NAMED_BIND */
	/* not using BIND -- the signature is just the host name */
	s->s_hostsig = host;
#endif /* NAMED_BIND */
	if (tTd(17, 1))
		dprintf("hostsignature(%s) = %s\n", host, s->s_hostsig);
	return s->s_hostsig;
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
**
**	Returns:
**		The number of hosts inserted into mxhosts array.
**
**	Side Effects:
**		Randomizes equal MX preference hosts in mxhosts.
*/

static int
parse_hostsignature(sig, mxhosts, mailer)
	char *sig;
	char **mxhosts;
	MAILER *mailer;
{
	int nmx = 0;
	int curpref = 0;
	int i, j;
	char *hp, *endp;
	u_short prefer[MAXMXHOSTS];
	long rndm[MAXMXHOSTS];

	for (hp = sig; hp != NULL; hp = endp)
	{
		char sep = ':';

#if NETINET6
		if (*hp == '[')
		{
			endp = strchr(hp + 1, ']');
			if (endp != NULL)
				endp = strpbrk(endp + 1, ":,");
		}
		else
			endp = strpbrk(hp, ":,");
#else /* NETINET6 */
		endp = strpbrk(hp, ":,");
#endif /* NETINET6 */
		if (endp != NULL)
		{
			sep = *endp;
			*endp = '\0';
		}

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
				register u_short tempp;
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

#if SMTP
# if STARTTLS
static SSL_CTX	*clt_ctx = NULL;

/*
**  INITCLTTLS -- initialize client side TLS
**
**	Parameters:
**		none.
**
**	Returns:
**		succeeded?
*/

bool
initclttls()
{
	if (clt_ctx != NULL)
		return TRUE;	/* already done */
	return inittls(&clt_ctx, TLS_I_CLT, FALSE, CltCERTfile, Cltkeyfile,
		       CACERTpath, CACERTfile, DHParams);
}

/*
**  STARTTLS -- try to start secure connection (client side)
**
**	Parameters:
**		m -- the mailer.
**		mci -- the mailer connection info.
**		e -- the envelope.
**
**	Returns:
**		success?
**		(maybe this should be some other code than EX_
**		that denotes which stage failed.)
*/

static int
starttls(m, mci, e)
	MAILER *m;
	MCI *mci;
	ENVELOPE *e;
{
	int smtpresult;
	int result;
	SSL *clt_ssl = NULL;

	smtpmessage("STARTTLS", m, mci);

	/* get the reply */
	smtpresult = reply(m, mci, e, TimeOuts.to_datafinal, NULL, NULL);
	/* which timeout? XXX */

	/* check return code from server */
	if (smtpresult == 454)
		return EX_TEMPFAIL;
	if (smtpresult == 501)
		return EX_USAGE;
	if (smtpresult == -1)
		return smtpresult;
	if (smtpresult != 220)
		return EX_PROTOCOL;

	if (LogLevel > 13)
		sm_syslog(LOG_INFO, e->e_id, "TLS: start client");
	if (clt_ctx == NULL && !initclttls())
		return EX_SOFTWARE;

	/* start connection */
	if ((clt_ssl = SSL_new(clt_ctx)) == NULL)
	{
		if (LogLevel > 5)
		{
			sm_syslog(LOG_ERR, e->e_id,
				  "TLS: error: client: SSL_new failed");
			if (LogLevel > 9)
				tlslogerr();
		}
		return EX_SOFTWARE;
	}

	/* SSL_clear(clt_ssl); ? */
	if ((result = SSL_set_rfd(clt_ssl, fileno(mci->mci_in))) != 1 ||
	    (result = SSL_set_wfd(clt_ssl, fileno(mci->mci_out))) != 1)
	{
		if (LogLevel > 5)
		{
			sm_syslog(LOG_ERR, e->e_id,
				  "TLS: error: SSL_set_xfd failed=%d", result);
			if (LogLevel > 9)
				tlslogerr();
		}
		return EX_SOFTWARE;
	}
	SSL_set_connect_state(clt_ssl);
	if ((result = SSL_connect(clt_ssl)) <= 0)
	{
		int i;

		/* what to do in this case? */
		i = SSL_get_error(clt_ssl, result);
		if (LogLevel > 5)
		{
			sm_syslog(LOG_ERR, e->e_id,
				  "TLS: error: SSL_connect failed=%d (%d)",
				  result, i);
			if (LogLevel > 9)
				tlslogerr();
		}
		SSL_free(clt_ssl);
		clt_ssl = NULL;
		return EX_SOFTWARE;
	}
	mci->mci_ssl = clt_ssl;
	result = tls_get_info(clt_ssl, e, FALSE, mci->mci_host);

	/* switch to use SSL... */
#if SFIO
	if (sfdctls(mci->mci_in, mci->mci_out, mci->mci_ssl) == 0)
		return EX_OK;
#else /* SFIO */
# if _FFR_TLS_TOREK
	if (sfdctls(&mci->mci_in, &mci->mci_out, mci->mci_ssl) == 0)
		return EX_OK;
# endif /* _FFR_TLS_TOREK */
#endif /* SFIO */

	/* failure */
	SSL_free(clt_ssl);
	clt_ssl = NULL;
	return EX_SOFTWARE;
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
int
endtlsclt(mci)
	MCI *mci;
{
	int r;

	if (!bitset(MCIF_TLSACT, mci->mci_flags))
		return EX_OK;
	r = endtls(mci->mci_ssl, "client");
	mci->mci_flags &= ~MCIF_TLSACT;
	return r;
}
/*
**  ENDTLS -- shutdown secure connection
**
**	Parameters:
**		ssl -- SSL connection information.
**		side -- srv/clt (for logging).
**
**	Returns:
**		success?
*/

int
endtls(ssl, side)
	SSL *ssl;
	char *side;
{
	if (ssl != NULL)
	{
		int r;

		if ((r = SSL_shutdown(ssl)) < 0)
		{
			if (LogLevel > 11)
				sm_syslog(LOG_WARNING, NOQID,
					  "SSL_shutdown %s failed: %d",
					  side, r);
			return EX_SOFTWARE;
		}
		else if (r == 0)
		{
			if (LogLevel > 13)
				sm_syslog(LOG_WARNING, NOQID,
					  "SSL_shutdown %s not done",
					  side);
			return EX_SOFTWARE;
		}
		SSL_free(ssl);
		ssl = NULL;
	}
	return EX_OK;
}
# endif /* STARTTLS */
#endif /* SMTP */
