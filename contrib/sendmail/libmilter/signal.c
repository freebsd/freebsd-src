/*
 *  Copyright (c) 1999-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: signal.c,v 8.10.4.4 2000/07/14 06:16:57 msk Exp $";
#endif /* ! lint */

#if _FFR_MILTER
#include "libmilter.h"

typedef pthread_mutex_t smutex_t;
#define smutex_init(mp)		(pthread_mutex_init(mp, NULL) == 0)
#define smutex_destroy(mp)	(pthread_mutex_destroy(mp) == 0)
#define smutex_lock(mp)		(pthread_mutex_lock(mp) == 0)
#define smutex_unlock(mp)	(pthread_mutex_unlock(mp) == 0)
#define smutex_trylock(mp)	(pthread_mutex_trylock(mp) == 0)

/*
** thread to handle signals
*/

static smutex_t M_Mutex;

static int MilterStop = MILTER_CONT;

/*
**  MI_STOP -- return value of MilterStop
**
**	Parameters:
**		none.
**
**	Returns:
**		value of MilterStop
*/

int
mi_stop()
{
	return MilterStop;
}
/*
**  MI_STOP_MILTERS -- set value of MilterStop
**
**	Parameters:
**		v -- new value for MilterStop.
**
**	Returns:
**		none.
*/

void
mi_stop_milters(v)
	int v;
{
	(void) smutex_lock(&M_Mutex);
	if (MilterStop < v)
		MilterStop = v;
	(void) smutex_unlock(&M_Mutex);
}
/*
**  MI_CLEAN_SIGNALS -- clean up signal handler thread
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
*/

void
mi_clean_signals()
{
	(void) smutex_destroy(&M_Mutex);
}
/*
**  MI_SIGNAL_THREAD -- thread to deal with signals
**
**	Parameters:
**		name -- name of milter
**
**	Returns:
**		NULL
*/

static void *
mi_signal_thread(name)
	void *name;
{
	int sig, errs;
	sigset_t set;

	sigemptyset(&set);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGTERM);

	/* Handle Ctrl-C gracefully for debugging */
	sigaddset(&set, SIGINT);
	errs = 0;

	while (TRUE)
	{
		sig = 0;
#ifdef SOLARIS
		if ((sig = sigwait(&set)) < 0)
#else /* SOLARIS */
		if (sigwait(&set, &sig) != 0)
#endif /* SOLARIS */
		{
			smi_log(SMI_LOG_ERR,
				"%s: sigwait returned error: %s",
				(char *)name, strerror(errno));
			if (++errs > MAX_FAILS_T)
			{
				mi_stop_milters(MILTER_ABRT);
				return NULL;
			}
			continue;
		}
		errs = 0;

		switch (sig)
		{
		  case SIGHUP:
		  case SIGTERM:
			mi_stop_milters(MILTER_STOP);
			return NULL;
		  case SIGINT:
			mi_stop_milters(MILTER_ABRT);
			return NULL;
		  default:
			smi_log(SMI_LOG_ERR,
				"%s: sigwait returned unmasked signal: %d",
				(char *)name, sig);
			break;
		}
	}
}
/*
**  MI_SPAWN_SIGNAL_THREAD -- spawn thread to handle signals
**
**	Parameters:
**		name -- name of milter
**
**	Returns:
**		MI_SUCCESS/MI_FAILURE
*/

static int
mi_spawn_signal_thread(name)
	char *name;
{
	sthread_t tid;
	sigset_t set;

	/* Mask HUP and KILL signals */
	sigemptyset(&set);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGTERM);
	sigaddset(&set, SIGINT);

	if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0)
	{
		smi_log(SMI_LOG_ERR,
			"%s: Couldn't mask HUP and KILL signals", name);
		return MI_FAILURE;
	}
	if (thread_create(&tid, mi_signal_thread,
			  (void *)name) != MI_SUCCESS)
	{
		smi_log(SMI_LOG_ERR,
			"%s: Couldn't start signal thread", name);
		return MI_FAILURE;
	}
	return MI_SUCCESS;
}
/*
**  MI_CONTROL_STARTUP -- startup for thread to handle signals
**
**	Parameters:
**		name -- name of milter
**
**	Returns:
**		MI_SUCCESS/MI_FAILURE
*/

int
mi_control_startup(name)
	char *name;
{

	if (!smutex_init(&M_Mutex))
	{
		smi_log(SMI_LOG_ERR,
			"%s: Couldn't initialize control pipe mutex", name);
		return MI_FAILURE;
	}

	/*
	**  spawn_signal_thread must happen before other threads are spawned
	**  off so that it can mask the right signals and other threads
	**  will inherit that mask.
	*/
	if (mi_spawn_signal_thread(name) == MI_FAILURE)
	{
		smi_log(SMI_LOG_ERR,
			"%s: Couldn't spawn signal thread", name);
		(void) smutex_destroy(&M_Mutex);
		return MI_FAILURE;
	}
	return MI_SUCCESS;
}
#endif /* _FFR_MILTER */
