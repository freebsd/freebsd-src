/*
 * Copyright (c) 2021 Proofpoint, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#include <sendmail.h>

#if _FFR_DMTRIGGER
#include <sm/sendmail.h>
#include <sm/notify.h>

static ENVELOPE	QmEnvelope;

/*
**  Macro for fork():
**  FORK_P1(): parent
**  FORK_C1(): child
**  Note: these are not "universal", e.g.,
**  proc_list_add() might be used in parent or child.
**  maybe check pname != NULL to invoke proc_list_add()?
*/

#define FORK_P1(emsg, pname, ptype)	\
	do {	\
		(void) sm_blocksignal(SIGCHLD);	\
		(void) sm_signal(SIGCHLD, reapchild);	\
		\
		pid = dofork();	\
		if (pid == -1)	\
		{	\
			const char *msg = emsg;	\
			const char *err = sm_errstring(errno);	\
		\
			if (LogLevel > 8)	\
				sm_syslog(LOG_INFO, NOQID, "%s: %s",	\
					  msg, err);	\
			(void) sm_releasesignal(SIGCHLD);	\
			return false;	\
		}	\
		if (pid != 0)	\
		{	\
			proc_list_add(pid, pname, ptype, 0, -1, NULL); \
			/* parent -- pick up intermediate zombie */	\
			(void) sm_releasesignal(SIGCHLD);	\
			return true;	\
		}	\
	} while (0)

#define FORK_C1()	\
	do {	\
		/* child -- clean up signals */	\
		\
		/* Reset global flags */	\
		RestartRequest = NULL;	\
		RestartWorkGroup = false;	\
		ShutdownRequest = NULL;	\
		PendingSignal = 0;	\
		CurrentPid = getpid();	\
		close_sendmail_pid();	\
		\
		/*	\
		**  Initialize exception stack and default exception	\
		**  handler for child process.	\
		*/	\
		\
		sm_exc_newthread(fatal_error);	\
		clrcontrol();	\
		proc_list_clear();	\
		\
		(void) sm_releasesignal(SIGCHLD);	\
		(void) sm_signal(SIGCHLD, SIG_DFL);	\
		(void) sm_signal(SIGHUP, SIG_DFL);	\
		(void) sm_signal(SIGTERM, intsig);	\
		\
		/* drop privileges */	\
		if (geteuid() == (uid_t) 0)	\
			(void) drop_privileges(false);	\
		disconnect(1, NULL);	\
		QuickAbort = false;	\
		\
	} while (0)

/*
**  QM -- queue "manager"
**
**	Parameters:
**		none.
**
**	Returns:
**		false on error
**
**	Side Effects:
**		fork()s and runs as process to deliver queue entries
*/

bool
qm()
{
	int r;
	pid_t pid;
	long tmo;

	sm_syslog(LOG_DEBUG, NOQID, "queue manager: start");

	FORK_P1("Queue manager -- fork() failed", "QM", PROC_QM);
	FORK_C1();

	r = sm_notify_start(true, 0);
	if (r != 0)
		syserr("sm_notify_start() failed=%d", r);

	/*
	**  Initially wait indefinitely, then only wait
	**  until something needs to get done (not yet implemented).
	*/

	tmo = -1;
	while (true)
	{
		char buf[64];
		ENVELOPE *e;
		SM_RPOOL_T *rpool;

/*
**  TODO: This should try to receive multiple ids:
**  after it got one, check for more with a very short timeout
**  and collect them in a list.
**  but them some other code should be used to run all of them.
*/

		sm_syslog(LOG_DEBUG, NOQID, "queue manager: rcv=start");
		r = sm_notify_rcv(buf, sizeof(buf), tmo);
		if (-ETIMEDOUT == r)
		{
			sm_syslog(LOG_DEBUG, NOQID, "queue manager: rcv=timed_out");
			continue;
		}
		if (r < 0)
		{
			sm_syslog(LOG_DEBUG, NOQID, "queue manager: rcv=%d", r);
			goto end;
		}
		if (r > 0 && r < sizeof(buf))
			buf[r] = '\0';
		buf[sizeof(buf) - 1] = '\0';
		sm_syslog(LOG_DEBUG, NOQID, "queue manager: got=%s", buf);
		CurEnv = &QmEnvelope;
		rpool = sm_rpool_new_x(NULL);
		e = newenvelope(&QmEnvelope, CurEnv, rpool);
		e->e_flags = BlankEnvelope.e_flags;
		e->e_parent = NULL;
		r = sm_io_sscanf(buf, "N:%d:%d:%s", &e->e_qgrp, &e->e_qdir, e->e_id);
		if (r != 3)
		{
			sm_syslog(LOG_DEBUG, NOQID, "queue manager: buf=%s, scan=%d", buf, r);
			goto end;
		}
		dowork(e->e_qgrp, e->e_qdir, e->e_id, true, false, e);
	}

  end:
	sm_syslog(LOG_DEBUG, NOQID, "queue manager: stop");
	finis(false, false, EX_OK);
	/* NOTREACHED */
	return false;
}
#endif /* _FFR_DMTRIGGER */
