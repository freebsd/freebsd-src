/*
 * Copyright (c) 1998 Daniel M. Eischen <eischen@vigrid.com>
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
 *	This product includes software developed by Daniel M. Eischen.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DANIEL M. EISCHEN AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#include <stdlib.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <assert.h>
#include <errno.h>
#include "pthread.h"
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>

#if defined(_LIBC_R_)
#include <pthread_np.h>
#endif

#ifndef NELEMENTS
#define NELEMENTS(arr)	(sizeof (arr) / sizeof (arr[0]))
#endif

#ifndef NUM_THREADS
#define NUM_THREADS	10
#endif

#define MAX_THREAD_CMDS	10


/*------------------------------------------------------------
 * Types
 *----------------------------------------------------------*/

typedef enum {
	STAT_INITIAL,		/* initial state */
	STAT_WAITCONDVAR,	/* waiting for condition variable signal */
	STAT_WAITMUTEX		/* waiting for mutex lock */
} thread_status_t;

typedef enum {
	FLAGS_REPORT_WAITCONDMUTEX	= 0x01,
	FLAGS_REPORT_WAITCONDVAR	= 0x02,
	FLAGS_REPORT_WAITMUTEX		= 0x04,
	FLAGS_REPORT_BUSY_LOOP		= 0x08,
	FLAGS_IS_BUSY			= 0x10,
	FLAGS_WAS_BUSY			= 0x20
} thread_flags_t;

typedef enum {
	CMD_NONE,
	CMD_TAKE_MUTEX,
	CMD_RELEASE_MUTEX,
	CMD_WAIT_FOR_SIGNAL,
	CMD_BUSY_LOOP,
	CMD_PROTECTED_OP,
	CMD_RELEASE_ALL
} thread_cmd_id_t;

typedef struct {
	thread_cmd_id_t	cmd_id;
	pthread_mutex_t	*mutex;
	pthread_cond_t	*cond;
} thread_cmd_t;

typedef struct {
	pthread_cond_t	cond_var;
	thread_status_t	status;
	thread_cmd_t	cmd;
	int		flags;
	int		priority;
	int		ret;
	pthread_t	tid;
	u_int8_t	id;
} thread_state_t;

typedef enum {
	M_POSIX,
	M_SS2_DEFAULT,
	M_SS2_ERRORCHECK,
	M_SS2_NORMAL,
	M_SS2_RECURSIVE
} mutex_kind_t;


/*------------------------------------------------------------
 * Constants
 *----------------------------------------------------------*/

const char *protocol_strs[] = {
	"PTHREAD_PRIO_NONE",
	"PTHREAD_PRIO_INHERIT",
	"PTHREAD_PRIO_PROTECT"
};

const int protocols[] = {
	PTHREAD_PRIO_NONE,
	PTHREAD_PRIO_INHERIT,
	PTHREAD_PRIO_PROTECT
};

const char *mutextype_strs[] = {
	"POSIX (type not specified)",
	"SS2 PTHREAD_MUTEX_DEFAULT",
	"SS2 PTHREAD_MUTEX_ERRORCHECK",
	"SS2 PTHREAD_MUTEX_NORMAL",
	"SS2 PTHREAD_MUTEX_RECURSIVE"
};

const int mutex_types[] = {
	0,				/* M_POSIX		*/
	PTHREAD_MUTEX_DEFAULT,		/* M_SS2_DEFAULT	*/
	PTHREAD_MUTEX_ERRORCHECK,	/* M_SS2_ERRORCHECK	*/
	PTHREAD_MUTEX_NORMAL,		/* M_SS2_NORMAL		*/
	PTHREAD_MUTEX_RECURSIVE		/* M_SS2_RECURSIVE	*/
};


/*------------------------------------------------------------
 * Objects
 *----------------------------------------------------------*/

static int		done = 0;
static int		trace_enabled = 0;
static int		use_global_condvar = 0;
static thread_state_t	states[NUM_THREADS];
static int		pipefd[2];

static pthread_mutex_t	waiter_mutex;
static pthread_mutex_t	cond_mutex;
static pthread_cond_t	cond_var;

static FILE *logfile = stdout;
static int error_count = 0, pass_count = 0, total = 0;


/*------------------------------------------------------------
 * Prototypes
 *----------------------------------------------------------*/
extern char *strtok_r(char *str, const char *sep, char **last);


/*------------------------------------------------------------
 * Functions
 *----------------------------------------------------------*/

#ifdef DEBUG
static void
kern_switch (pthread_t pthread_out, pthread_t pthread_in)
{
	if (pthread_out != NULL)
		printf ("Swapping out thread 0x%x, ", (int) pthread_out);
	else
		printf ("Swapping out kernel thread, ");

	if (pthread_in != NULL)
		printf ("swapping in thread 0x%x\n", (int) pthread_in);
	else
		printf ("swapping in kernel thread.\n");
}
#endif


static void
log_error (const char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	fprintf (logfile, "FAIL: ");
	vfprintf (logfile, fmt, ap);
	error_count = error_count + 1;
	total = total + 1;
}


static void
log_pass (void)
{
	fprintf (logfile, "PASS\n");
	pass_count = pass_count + 1;
	total = total + 1;
}


static void
log_trace (const char *fmt, ...)
{
	va_list ap;

	if (trace_enabled) {
		va_start (ap, fmt);
		vfprintf (logfile, fmt, ap);
	}
}


static void
log (const char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	vfprintf (logfile, fmt, ap);
}


static void
check_result (int expected, int actual)
{
	if (expected != actual)
		log_error ("expected %d, returned %d\n", expected, actual);
	else
		log_pass ();
}


/*
 * Check to see that the threads ran in the specified order.
 */
static void
check_run_order (char *order)
{
	const char *sep = ":,";
	char *tok, *last, *idstr, *endptr;
	int expected_id, bytes, count = 0, errors = 0;
	u_int8_t id;

	assert ((tok = (char *) malloc (strlen(order) + 1)) != NULL);
	strcpy (tok, order);	/* tok has to be larger than order */
	assert (ioctl (pipefd[0], FIONREAD, &bytes) == 0);
	log_trace ("%d bytes read from FIFO.\n", bytes);

	for (idstr = strtok_r (tok, sep, &last);
	     (idstr != NULL) && (count < bytes);
	     idstr = strtok_r (NULL, sep, &last)) {

		/* Get the expected id: */
		expected_id = (int) strtol (idstr, &endptr, 10);
		assert ((endptr != NULL) && (*endptr == '\0'));

		/* Read the actual id from the pipe: */
		assert (read (pipefd[0], &id, sizeof (id)) == sizeof (id));
		count = count + sizeof (id);

		if (id != expected_id) {
			log_trace ("Thread %d ran out of order.\n", id);
			errors = errors + 1;
		}
		else {
			log_trace ("Thread %d at priority %d reporting.\n",
			    (int) id, states[id].priority);
		}
	}

	if (count < bytes) {
		/* Clear the pipe: */
		while (count < bytes) {
			read (pipefd[0], &id, sizeof (id));
			count = count + 1;
			errors = errors + 1;
		}
	}
	else if (bytes < count)
		errors = errors + count - bytes;

	if (errors == 0)
		log_pass ();
	else
		log_error ("%d threads ran out of order", errors);
}


static void *
waiter (void *arg)
{
	thread_state_t	*statep = (thread_state_t *) arg;
	pthread_mutex_t	*held_mutex[MAX_THREAD_CMDS];
	int 		held_mutex_owned[MAX_THREAD_CMDS];
	sigset_t	mask;
	struct timeval	tv1, tv2;
	thread_cmd_t	cmd;
	int 		i, mutex_count = 0;

	statep->status = STAT_INITIAL;

	/* Block all signals except for interrupt.*/
	sigfillset (&mask);
	sigdelset (&mask, SIGINT);
	sigprocmask (SIG_BLOCK, &mask, NULL);

	while (done == 0) {
		/* Wait for signal from the main thread to continue. */
		statep->status = STAT_WAITMUTEX;
		log_trace ("Thread %d: locking cond_mutex.\n",
		    (int) statep->id);
		pthread_mutex_lock (&cond_mutex);

		/* Do we report our status. */
		if (statep->flags & FLAGS_REPORT_WAITCONDMUTEX)
			write (pipefd[1], &statep->id, sizeof (statep->id));
		log_trace ("Thread %d: waiting for cond_var.\n",
		    (int) statep->id);

		/* Wait for a command. */
		statep->status = STAT_WAITCONDVAR;

		/*
		 * The threads are allowed commanded to wait either on
		 * their own unique condition variable (so they may be
		 * separately signaled) or on one global condition variable
		 * (so they may be signaled together).
		 */
		if (use_global_condvar != 0)
			pthread_cond_wait (&cond_var, &cond_mutex);
		else
			pthread_cond_wait (&statep->cond_var, &cond_mutex);

		/* Do we report our status? */
		if (statep->flags & FLAGS_REPORT_WAITCONDVAR) {
			write (pipefd[1], &statep->id, sizeof (statep->id));
			log_trace ("Thread %d: wrote %d to pipe.\n",
			    (int) statep->id);
		}
		log_trace ("Thread %d: received cond_var signal.\n",
		    (int) statep->id);

		/* Get a copy of the command before releasing the mutex. */
		cmd = statep->cmd;

		/* Clear the command after copying it. */
		statep->cmd.cmd_id = CMD_NONE;

		/* Unlock the condition variable mutex. */
		assert (pthread_mutex_unlock (&cond_mutex) == 0);

		/* Peform the command.*/
		switch (cmd.cmd_id) {
		case CMD_TAKE_MUTEX:
			statep->ret = pthread_mutex_lock (cmd.mutex);
			if (statep->ret == 0) {
				assert (mutex_count < sizeof (held_mutex));
				held_mutex[mutex_count] = cmd.mutex;
				held_mutex_owned[mutex_count] = 1;
				mutex_count++;
			}
			else {
				held_mutex_owned[mutex_count] = 0;
				log_trace ("Thread id %d unable to lock mutex, "
				    "error = %d\n", (int) statep->id,
				    statep->ret);
			}
			break;

		case CMD_RELEASE_MUTEX:
			assert ((mutex_count <= sizeof (held_mutex)) &&
			    (mutex_count > 0));
			mutex_count--;
			if (held_mutex_owned[mutex_count] != 0)
				assert (pthread_mutex_unlock
				    (held_mutex[mutex_count]) == 0);
			break;

		case CMD_WAIT_FOR_SIGNAL:
			assert (pthread_mutex_lock (cmd.mutex) == 0);
			assert (pthread_cond_wait (cmd.cond, cmd.mutex) == 0);
			assert (pthread_mutex_unlock (cmd.mutex) == 0);
			break;

		case CMD_BUSY_LOOP:
			log_trace ("Thread %d: Entering busy loop.\n",
			    (int) statep->id);
			/* Spin for 15 seconds. */
			assert (gettimeofday (&tv2, NULL) == 0);
			tv1.tv_sec = tv2.tv_sec + 5;
			tv1.tv_usec = tv2.tv_usec;
			statep->flags |= FLAGS_IS_BUSY;
			while (timercmp (&tv2, &tv1,<)) {
				assert (gettimeofday (&tv2, NULL) == 0);
			}
			statep->flags &= ~FLAGS_IS_BUSY;
			statep->flags |= FLAGS_WAS_BUSY;

			/* Do we report our status? */
			if (statep->flags & FLAGS_REPORT_BUSY_LOOP)
				write (pipefd[1], &statep->id,
				    sizeof (statep->id));

			log_trace ("Thread %d: Leaving busy loop.\n",
			    (int) statep->id);
			break;

		case CMD_PROTECTED_OP:
			assert (pthread_mutex_lock (cmd.mutex) == 0);
			statep->flags |= FLAGS_WAS_BUSY;
			/* Do we report our status? */
			if (statep->flags & FLAGS_REPORT_BUSY_LOOP)
				write (pipefd[1], &statep->id,
				    sizeof (statep->id));

			assert (pthread_mutex_unlock (cmd.mutex) == 0);
			break;

		case CMD_RELEASE_ALL:
			assert ((mutex_count <= sizeof (held_mutex)) &&
			    (mutex_count > 0));
			for (i = mutex_count - 1; i >= 0; i--) {
				if (held_mutex_owned[i] != 0)
					assert (pthread_mutex_unlock
					    (held_mutex[i]) == 0);
			}
			mutex_count = 0;
			break;

		case CMD_NONE:
		default:
			break;
		}

		/* Wait for the big giant waiter lock. */
		statep->status = STAT_WAITMUTEX;
		log_trace ("Thread %d: waiting for big giant lock.\n",
		    (int) statep->id);
		pthread_mutex_lock (&waiter_mutex);
		if (statep->flags & FLAGS_REPORT_WAITMUTEX)
			write (pipefd[1], &statep->id, sizeof (statep->id));
		log_trace ("Thread %d: got big giant lock.\n",
		    (int) statep->id);
		statep->status = STAT_INITIAL;
		pthread_mutex_unlock (&waiter_mutex);
	}

	log_trace ("Thread %d: Exiting thread 0x%x\n", (int) statep->id,
	    (int) pthread_self());
	pthread_exit (arg);
	return (NULL);
}


static void *
lock_twice (void *arg)
{
	thread_state_t	*statep = (thread_state_t *) arg;
	sigset_t	mask;

	statep->status = STAT_INITIAL;

	/* Block all signals except for interrupt.*/
	sigfillset (&mask);
	sigdelset (&mask, SIGINT);
	sigprocmask (SIG_BLOCK, &mask, NULL);

	/* Wait for a signal to continue. */
	log_trace ("Thread %d: locking cond_mutex.\n", (int) statep->id);
	pthread_mutex_lock (&cond_mutex);

	log_trace ("Thread %d: waiting for cond_var.\n", (int) statep->id);
	statep->status = STAT_WAITCONDVAR;
	pthread_cond_wait (&cond_var, &cond_mutex);

	log_trace ("Thread %d: received cond_var signal.\n", (int) statep->id);

	/* Unlock the condition variable mutex. */
	assert (pthread_mutex_unlock (&cond_mutex) == 0);

	statep->status = STAT_WAITMUTEX;
	/* Lock the mutex once. */
	assert (pthread_mutex_lock (statep->cmd.mutex) == 0);

	/* Lock it again and capture the error. */
	statep->ret = pthread_mutex_lock (statep->cmd.mutex);
	statep->status = 0;

	assert (pthread_mutex_unlock (statep->cmd.mutex) == 0);

	/* Unlock it again if it is locked recursively. */
	if (statep->ret == 0)
		pthread_mutex_unlock (statep->cmd.mutex);

	log_trace ("Thread %d: Exiting thread 0x%x\n", (int) statep->id,
	    (int) pthread_self());
	pthread_exit (arg);
	return (NULL);
}


static void
sighandler (int signo)
{
	log ("Signal handler caught signal %d, thread id 0x%x\n",
	    signo, (int) pthread_self());

	if (signo == SIGINT)
		done = 1;
}


static void
send_cmd (int id, thread_cmd_id_t cmd)
{
	assert (pthread_mutex_lock (&cond_mutex) == 0);
	assert (states[id].status == STAT_WAITCONDVAR);
	states[id].cmd.cmd_id = cmd;
	states[id].cmd.mutex = NULL;
	states[id].cmd.cond = NULL;
	/* Clear the busy flags. */
	states[id].flags &= ~(FLAGS_WAS_BUSY | FLAGS_IS_BUSY);
	assert (pthread_cond_signal (&states[id].cond_var) == 0);
	assert (pthread_mutex_unlock (&cond_mutex) == 0);
}


static void
send_mutex_cmd (int id, thread_cmd_id_t cmd, pthread_mutex_t *m)
{
	assert (pthread_mutex_lock (&cond_mutex) == 0);
	assert (states[id].status == STAT_WAITCONDVAR);
	states[id].cmd.cmd_id = cmd;
	states[id].cmd.mutex = m;
	states[id].cmd.cond = NULL;
	/* Clear the busy flags. */
	states[id].flags &= ~(FLAGS_WAS_BUSY | FLAGS_IS_BUSY);
	assert (pthread_cond_signal (&states[id].cond_var) == 0);
	assert (pthread_mutex_unlock (&cond_mutex) == 0);
}


static void
send_mutex_cv_cmd (int id, thread_cmd_id_t cmd, pthread_mutex_t *m,
    pthread_cond_t *cv)
{
	assert (pthread_mutex_lock (&cond_mutex) == 0);
	assert (states[id].status == STAT_WAITCONDVAR);
	states[id].cmd.cmd_id = cmd;
	states[id].cmd.mutex = m;
	states[id].cmd.cond = cv;
	/* Clear the busy flags. */
	states[id].flags &= ~(FLAGS_WAS_BUSY | FLAGS_IS_BUSY);
	assert (pthread_cond_signal (&states[id].cond_var) == 0);
	assert (pthread_mutex_unlock (&cond_mutex) == 0);
}


static void
mutex_init_test (void)
{
	pthread_mutexattr_t mattr;
	pthread_mutex_t	mutex;
	mutex_kind_t mkind;
	int mproto, ret;

	/*
	 * Initialize a mutex attribute.
	 *
	 * pthread_mutexattr_init not tested for: ENOMEM
	 */
	assert (pthread_mutexattr_init (&mattr) == 0);

	/*
	 * Initialize a mutex.
	 *
	 * pthread_mutex_init not tested for: EAGAIN ENOMEM EPERM EBUSY
	 */
	log ("Testing pthread_mutex_init\n");
	log ("--------------------------\n");

	for (mproto = 0; mproto < NELEMENTS(protocols); mproto++) {
		for (mkind = M_POSIX; mkind <= M_SS2_RECURSIVE; mkind++) {
			/* Initialize the mutex attribute. */
			assert (pthread_mutexattr_init (&mattr) == 0);
			assert (pthread_mutexattr_setprotocol (&mattr,
			    protocols[mproto]) == 0);

			/*
			 * Ensure that the first mutex type is a POSIX
			 * compliant mutex.
			 */
			if (mkind != M_POSIX) {
				assert (pthread_mutexattr_settype (&mattr,
				    mutex_types[mkind]) == 0);
			}

			log ("  Protocol %s, Type %s - ",
			    protocol_strs[mproto], mutextype_strs[mkind]);
			ret = pthread_mutex_init (&mutex, &mattr);
			check_result (/* expected */ 0, ret);
			assert (pthread_mutex_destroy (&mutex) == 0);

			/*
			 * Destroy a mutex attribute.
			 *
			 * XXX - There should probably be a magic number
			 *       associated with a mutex attribute so that
			 *       destroy can be reasonably sure the attribute
			 *       is valid.
			 *
			 * pthread_mutexattr_destroy not tested for: EINVAL
			 */
			assert (pthread_mutexattr_destroy (&mattr) == 0);
		}
	}
}


static void
mutex_destroy_test (void)
{
	pthread_mutexattr_t mattr;
	pthread_mutex_t	mutex;
	pthread_condattr_t cattr;
	pthread_cond_t	cv;
	pthread_attr_t pattr;
	int mproto, ret;
	mutex_kind_t mkind;
	thread_state_t state;

	/*
	 * Destroy a mutex.
	 *
	 * XXX - There should probably be a magic number associated
	 *       with a mutex so that destroy can be reasonably sure
	 *       the mutex is valid.
	 *
	 * pthread_mutex_destroy not tested for: 
	 */
	log ("Testing pthread_mutex_destroy\n");
	log ("-----------------------------\n");

	assert (pthread_attr_init (&pattr) == 0);
	assert (pthread_attr_setdetachstate (&pattr,
	    PTHREAD_CREATE_DETACHED) == 0);
	state.flags = 0;	/* No flags yet. */

	for (mproto = 0; mproto < NELEMENTS(protocols); mproto++) {
		for (mkind = M_POSIX; mkind <= M_SS2_RECURSIVE; mkind++) {
			/* Initialize the mutex attribute. */
			assert (pthread_mutexattr_init (&mattr) == 0);
			assert (pthread_mutexattr_setprotocol (&mattr,
			    protocols[mproto]) == 0);

			/*
			 * Ensure that the first mutex type is a POSIX
			 * compliant mutex.
			 */
			if (mkind != M_POSIX) {
				assert (pthread_mutexattr_settype (&mattr,
				    mutex_types[mkind]) == 0);
			}

			/* Create the mutex. */
			assert (pthread_mutex_init (&mutex, &mattr) == 0);

			log ("  Protocol %s, Type %s\n",
			    protocol_strs[mproto], mutextype_strs[mkind]);

			log ("    Destruction of unused mutex - ");
			assert (pthread_mutex_init (&mutex, &mattr) == 0);
			ret = pthread_mutex_destroy (&mutex);
			check_result (/* expected */ 0, ret);

			log ("    Destruction of mutex locked by self - ");
			assert (pthread_mutex_init (&mutex, &mattr) == 0);
			assert (pthread_mutex_lock (&mutex) == 0);
			ret = pthread_mutex_destroy (&mutex);
			check_result (/* expected */ EBUSY, ret);
			assert (pthread_mutex_unlock (&mutex) == 0);
			assert (pthread_mutex_destroy (&mutex) == 0);

			log ("    Destruction of mutex locked by another "
			    "thread - ");
			assert (pthread_mutex_init (&mutex, &mattr) == 0);
			send_mutex_cmd (0, CMD_TAKE_MUTEX, &mutex);
			sleep (1);
			ret = pthread_mutex_destroy (&mutex);
			check_result (/* expected */ EBUSY, ret);
			send_cmd (0, CMD_RELEASE_ALL);
			sleep (1);
			assert (pthread_mutex_destroy (&mutex) == 0);

			log ("    Destruction of mutex while being used in "
			    "cond_wait - ");
			assert (pthread_mutex_init (&mutex, &mattr) == 0);
			assert (pthread_condattr_init (&cattr) == 0);
			assert (pthread_cond_init (&cv, &cattr) == 0);
			send_mutex_cv_cmd (0, CMD_WAIT_FOR_SIGNAL, &mutex, &cv);
			sleep (1);
			ret = pthread_mutex_destroy (&mutex);
			check_result (/* expected */ EBUSY, ret);
			pthread_cond_signal (&cv);
			sleep (1);
			assert (pthread_mutex_destroy (&mutex) == 0);
		}
	}
}


static void
mutex_lock_test (void)
{
	pthread_mutexattr_t mattr;
	pthread_mutex_t	mutex;
	pthread_attr_t pattr;
	int mproto, ret;
	mutex_kind_t mkind;
	thread_state_t state;

	/*
	 * Lock a mutex.
	 *
	 * pthread_lock not tested for: 
	 */
	log ("Testing pthread_mutex_lock\n");
	log ("--------------------------\n");

	assert (pthread_attr_init (&pattr) == 0);
	assert (pthread_attr_setdetachstate (&pattr,
	    PTHREAD_CREATE_DETACHED) == 0);
	state.flags = 0;	/* No flags yet. */

	for (mproto = 0; mproto < NELEMENTS(protocols); mproto++) {
		for (mkind = M_POSIX; mkind <= M_SS2_RECURSIVE; mkind++) {
			/* Initialize the mutex attribute. */
			assert (pthread_mutexattr_init (&mattr) == 0);
			assert (pthread_mutexattr_setprotocol (&mattr,
			    protocols[mproto]) == 0);

			/*
			 * Ensure that the first mutex type is a POSIX
			 * compliant mutex.
			 */
			if (mkind != M_POSIX) {
				assert (pthread_mutexattr_settype (&mattr,
				    mutex_types[mkind]) == 0);
			}

			/* Create the mutex. */
			assert (pthread_mutex_init (&mutex, &mattr) == 0);

			log ("  Protocol %s, Type %s\n",
			    protocol_strs[mproto], mutextype_strs[mkind]);

			log ("    Lock on unlocked mutex - ");
			ret = pthread_mutex_lock (&mutex);
			check_result (/* expected */ 0, ret);
			pthread_mutex_unlock (&mutex);

			log ("    Lock on invalid mutex - ");
			ret = pthread_mutex_lock (NULL);
			check_result (/* expected */ EINVAL, ret);

			log ("    Lock on mutex held by self - ");
			assert (pthread_create (&state.tid, &pattr, lock_twice,
			    (void *) &state) == 0);
			/* Let the thread start. */
			sleep (1);
			state.cmd.mutex = &mutex;
			state.ret = 0xdeadbeef;
			assert (pthread_mutex_lock (&cond_mutex) == 0);
			assert (pthread_cond_signal (&cond_var) == 0);
			assert (pthread_mutex_unlock (&cond_mutex) == 0);
			/* Let the thread receive and process the command. */
			sleep (1);

			switch (mkind) {
			case M_POSIX:
				check_result (/* expected */ EDEADLK,
				    state.ret);
				break;
			case M_SS2_DEFAULT:
				check_result (/* expected */ EDEADLK,
				    state.ret);
				break;
			case M_SS2_ERRORCHECK:
				check_result (/* expected */ EDEADLK,
				    state.ret);
				break;
			case M_SS2_NORMAL:
				check_result (/* expected */ 0xdeadbeef,
				    state.ret);
				break;
			case M_SS2_RECURSIVE:
				check_result (/* expected */ 0, state.ret);
				break;
			}
			pthread_mutex_destroy (&mutex);
			pthread_mutexattr_destroy (&mattr);
		}
	}
}


static void
mutex_unlock_test (void)
{
	const int test_thread_id = 0;	/* ID of test thread */
	pthread_mutexattr_t mattr;
	pthread_mutex_t	mutex;
	int mproto, ret;
	mutex_kind_t mkind;

	/*
	 * Unlock a mutex.
	 *
	 * pthread_unlock not tested for: 
	 */
	log ("Testing pthread_mutex_unlock\n");
	log ("----------------------------\n");

	for (mproto = 0; mproto < NELEMENTS(protocols); mproto++) {
		for (mkind = M_POSIX; mkind <= M_SS2_RECURSIVE; mkind++) {
			/* Initialize the mutex attribute. */
			assert (pthread_mutexattr_init (&mattr) == 0);
			assert (pthread_mutexattr_setprotocol (&mattr,
			    protocols[mproto]) == 0);

			/*
			 * Ensure that the first mutex type is a POSIX
			 * compliant mutex.
			 */
			if (mkind != M_POSIX) {
				assert (pthread_mutexattr_settype (&mattr,
				    mutex_types[mkind]) == 0);
			}

			/* Create the mutex. */
			assert (pthread_mutex_init (&mutex, &mattr) == 0);

			log ("  Protocol %s, Type %s\n",
			    protocol_strs[mproto], mutextype_strs[mkind]);

			log ("    Unlock on mutex held by self - ");
			assert (pthread_mutex_lock (&mutex) == 0);
			ret = pthread_mutex_unlock (&mutex);
			check_result (/* expected */ 0, ret);

			log ("    Unlock on invalid mutex - ");
			ret = pthread_mutex_unlock (NULL);
			check_result (/* expected */ EINVAL, ret);

			log ("    Unlock on mutex locked by another thread - ");
			send_mutex_cmd (test_thread_id, CMD_TAKE_MUTEX, &mutex);
			sleep (1);
			ret = pthread_mutex_unlock (&mutex);
			switch (mkind) {
			case M_POSIX:
				check_result (/* expected */ EPERM, ret);
				break;
			case M_SS2_DEFAULT:
				check_result (/* expected */ EPERM, ret);
				break;
			case M_SS2_ERRORCHECK:
				check_result (/* expected */ EPERM, ret);
				break;
			case M_SS2_NORMAL:
				check_result (/* expected */ EPERM, ret);
				break;
			case M_SS2_RECURSIVE:
				check_result (/* expected */ EPERM, ret);
				break;
			}
			if (ret == 0) {
				/*
				 * If for some reason we were able to unlock
				 * the mutex, relock it so that the test
				 * thread has no problems releasing the mutex.
				 */
				pthread_mutex_lock (&mutex);
			}
			send_cmd (test_thread_id, CMD_RELEASE_ALL);
			sleep (1);

			pthread_mutex_destroy (&mutex);
			pthread_mutexattr_destroy (&mattr);
		}
	}
}


static void
queueing_order_test (void)
{
	int i;

	log ("Testing queueing order\n");
	log ("----------------------\n");
	assert (pthread_mutex_lock (&waiter_mutex) == 0);
	/*
	 * Tell the threads to report when they take the waiters mutex.
	 */
	assert (pthread_mutex_lock (&cond_mutex) == 0);
	for (i = 0; i < NUM_THREADS; i++) {
		states[i].flags = FLAGS_REPORT_WAITMUTEX;
		assert (pthread_cond_signal (&states[i].cond_var) == 0);
	}
	assert (pthread_mutex_unlock (&cond_mutex) == 0);

	/* Signal the threads to continue. */
	sleep (1);

	/* Use the global condition variable next time. */
	use_global_condvar = 1;

	/* Release the waiting threads and allow them to run again. */
	assert (pthread_mutex_unlock (&waiter_mutex) == 0);
	sleep (1);

	log ("  Queueing order on a mutex - ");
	check_run_order ("9,8,7,6,5,4,3,2,1,0");
	for (i = 0; i < NUM_THREADS; i = i + 1) {
		/* Tell the threads to report when they've been signaled. */
		states[i].flags = FLAGS_REPORT_WAITCONDVAR;
	}

	/*
	 * Prevent the threads from continuing their loop after we
	 * signal them.
	 */
	assert (pthread_mutex_lock (&waiter_mutex) == 0);


	log ("  Queueing order on a condition variable - ");
	/*
	 * Signal one thread to run and see that the highest priority
	 * thread executes.
	 */
	assert (pthread_mutex_lock (&cond_mutex) == 0);
	assert (pthread_cond_signal (&cond_var) == 0);
	assert (pthread_mutex_unlock (&cond_mutex) == 0);
	sleep (1);
	if (states[NUM_THREADS - 1].status != STAT_WAITMUTEX)
		log_error ("highest priority thread does not run.\n");

	/* Signal the remaining threads. */
	assert (pthread_mutex_lock (&cond_mutex) == 0);
	assert (pthread_cond_broadcast (&cond_var) == 0);
	assert (pthread_mutex_unlock (&cond_mutex) == 0);
	sleep (1);

	check_run_order ("9,8,7,6,5,4,3,2,1,0");
	for (i = 0; i < NUM_THREADS; i = i + 1) {
		/* Tell the threads not to report anything. */
		states[i].flags = 0;
	}

	/* Use the thread unique condition variable next time. */
	use_global_condvar = 0;

	/* Allow the threads to continue their loop. */
	assert (pthread_mutex_unlock (&waiter_mutex) == 0);
	sleep (1);
}


static void
mutex_prioceiling_test (void)
{
	const int test_thread_id = 0;	/* ID of test thread */
	pthread_mutexattr_t mattr;
	struct sched_param param;
	pthread_mutex_t	m[3];
	mutex_kind_t	mkind;
	int		i, ret, policy, my_prio, old_ceiling;

	log ("Testing priority ceilings\n");
	log ("-------------------------\n");
	for (mkind = M_POSIX; mkind <= M_SS2_RECURSIVE; mkind++) {

		log ("  Protype PTHREAD_PRIO_PROTECT, Type %s\n",
		    mutextype_strs[mkind]);

		/*
		 * Initialize and create a mutex.
		 */
		assert (pthread_mutexattr_init (&mattr) == 0);

		/* Get this threads current priority. */
		assert (pthread_getschedparam (pthread_self(), &policy,
		    &param) == 0);
		my_prio = param.sched_priority;	/* save for later use */
		log_trace ("Current scheduling policy %d, priority %d\n",
		    policy, my_prio);

		/*
		 * Initialize and create 3 priority protection mutexes with
		 * default (max priority) ceilings.
		 */
		assert (pthread_mutexattr_setprotocol(&mattr,
		    PTHREAD_PRIO_PROTECT) == 0);

		/*
		 * Ensure that the first mutex type is a POSIX
		 * compliant mutex.
		 */
		if (mkind != M_POSIX) {
			assert (pthread_mutexattr_settype (&mattr,
			    mutex_types[mkind]) == 0);
		}

		for (i = 0; i < 3; i++)
			assert (pthread_mutex_init (&m[i], &mattr) == 0);

		/*
		 * Set the ceiling priorities for the 3 priority protection
		 * mutexes to, 5 less than, equal to, and 5 greater than,
		 * this threads current priority.
		 */
		for (i = 0; i < 3; i++)
			assert (pthread_mutex_setprioceiling (&m[i],
			    my_prio - 5 + 5*i, &old_ceiling) == 0);

		/*
		 * Check that if we attempt to take a mutex whose priority
		 * ceiling is lower than our priority, we get an error.
		 */
		log ("    Lock with ceiling priority < thread priority - ");
		ret = pthread_mutex_lock (&m[0]);
		check_result (/* expected */ EINVAL, ret);
		if (ret == 0)
			pthread_mutex_unlock (&m[0]);

		/*
		 * Check that we can take a mutex whose priority ceiling
		 * is equal to our priority.
		 */
		log ("    Lock with ceiling priority = thread priority - ");
		ret = pthread_mutex_lock (&m[1]);
		check_result (/* expected */ 0, ret);
		if (ret == 0)
			pthread_mutex_unlock (&m[1]);

		/*
		 * Check that we can take a mutex whose priority ceiling
		 * is higher than our priority.
		 */
		log ("    Lock with ceiling priority > thread priority - ");
		ret = pthread_mutex_lock (&m[2]);
		check_result (/* expected */ 0, ret);
		if (ret == 0)
			pthread_mutex_unlock (&m[2]);

		/*
		 * Have the test thread go into a busy loop for 5 seconds
		 * and see that it doesn't block this thread (since the
		 * priority ceiling of mutex 0 and the priority of the test
		 * thread are both less than the priority of this thread).
		 */
		log ("    Preemption with ceiling priority < thread "
		    "priority - ");
		/* Have the test thread take mutex 0. */
		send_mutex_cmd (test_thread_id, CMD_TAKE_MUTEX, &m[0]);
		sleep (1);

		log_trace ("Sending busy command.\n");
		send_cmd (test_thread_id, CMD_BUSY_LOOP);
		log_trace ("Busy sent, yielding\n");
		pthread_yield ();
		log_trace ("Returned from yield.\n");
		if (states[test_thread_id].flags &
		    (FLAGS_IS_BUSY | FLAGS_WAS_BUSY))
			log_error ("test thread inproperly preempted us.\n");
		else {
			/* Let the thread finish its busy loop. */
			sleep (6);
			if ((states[test_thread_id].flags & FLAGS_WAS_BUSY) == 0)
				log_error ("test thread never finished.\n");
			else
				log_pass ();
		}
		states[test_thread_id].flags &= ~FLAGS_WAS_BUSY;

		/* Have the test thread release mutex 0. */
		send_cmd (test_thread_id, CMD_RELEASE_ALL);
		sleep (1);

		/*
		 * Have the test thread go into a busy loop for 5 seconds
		 * and see that it preempts this thread (since the priority
		 * ceiling of mutex 1 is the same as the priority of this
		 * thread).  The test thread should not run to completion
		 * as its time quantum should expire before the 5 seconds
		 * are up.
		 */
		log ("    Preemption with ceiling priority = thread "
		    "priority - ");

		/* Have the test thread take mutex 1. */
		send_mutex_cmd (test_thread_id, CMD_TAKE_MUTEX, &m[1]);
		sleep (1);

		log_trace ("Sending busy\n");
		send_cmd (test_thread_id, CMD_BUSY_LOOP);
		log_trace ("Busy sent, yielding\n");
		pthread_yield ();
		log_trace ("Returned from yield.\n");
		if ((states[test_thread_id].flags & FLAGS_IS_BUSY) == 0)
			log_error ("test thread did not switch in on yield.\n");
		else if (states[test_thread_id].flags & FLAGS_WAS_BUSY)
			log_error ("test thread ran to completion.\n");
		else {
			/* Let the thread finish its busy loop. */
			sleep (6);
			if ((states[test_thread_id].flags & FLAGS_WAS_BUSY) == 0)
				log_error ("test thread never finished.\n");
			else
				log_pass ();
		}
		states[test_thread_id].flags &= ~FLAGS_WAS_BUSY;

		/* Have the test thread release mutex 1. */
		send_cmd (test_thread_id, CMD_RELEASE_ALL);
		sleep (1);

		/*
		 * Set the scheduling policy of the test thread to SCHED_FIFO
		 * and have it go into a busy loop for 5 seconds.  This
		 * thread is SCHED_RR, and since the priority ceiling of
		 * mutex 1 is the same as the priority of this thread, the
		 * test thread should run to completion once it is switched
		 * in.
		 */
		log ("    SCHED_FIFO scheduling and ceiling priority = "
		    "thread priority - ");
		param.sched_priority = states[test_thread_id].priority;
		assert (pthread_setschedparam (states[test_thread_id].tid,
		    SCHED_FIFO, &param) == 0);

		/* Have the test thread take mutex 1. */
		send_mutex_cmd (test_thread_id, CMD_TAKE_MUTEX, &m[1]);
		sleep (1);

		log_trace ("Sending busy\n");
		send_cmd (test_thread_id, CMD_BUSY_LOOP);
		log_trace ("Busy sent, yielding\n");
		pthread_yield ();
		log_trace ("Returned from yield.\n");
		if ((states[test_thread_id].flags & FLAGS_WAS_BUSY) == 0) {
			log_error ("test thread did not run to completion.\n");
			/* Let the thread finish it's busy loop. */
			sleep (6);
		}
		else
			log_pass ();
		states[test_thread_id].flags &= ~FLAGS_WAS_BUSY;

		/* Restore the test thread scheduling parameters. */
		param.sched_priority = states[test_thread_id].priority;
		assert (pthread_setschedparam (states[test_thread_id].tid,
		    SCHED_RR, &param) == 0);

		/* Have the test thread release mutex 1. */
		send_cmd (test_thread_id, CMD_RELEASE_ALL);
		sleep (1);

		/*
		 * Have the test thread go into a busy loop for 5 seconds
		 * and see that it preempts this thread (since the priority
		 * ceiling of mutex 2 is the greater than the priority of
		 * this thread).  The test thread should run to completion
		 * and block this thread because its active priority is
		 * higher.
		 */
		log ("    SCHED_FIFO scheduling and ceiling priority > "
		    "thread priority - ");
		/* Have the test thread take mutex 2. */
		send_mutex_cmd (test_thread_id, CMD_TAKE_MUTEX, &m[2]);
		sleep (1);

		log_trace ("Sending busy\n");
		send_cmd (test_thread_id, CMD_BUSY_LOOP);
		log_trace ("Busy sent, yielding\n");
		pthread_yield ();
		log_trace ("Returned from yield.\n");
		if ((states[test_thread_id].flags & FLAGS_IS_BUSY) != 0) {
			log_error ("test thread did not run to completion.\n");
			/* Let the thread finish it's busy loop. */
			sleep (6);
		}
		else if ((states[test_thread_id].flags & FLAGS_WAS_BUSY) == 0)
			log_error ("test thread never finished.\n");
		else
			log_pass ();
		states[test_thread_id].flags &= ~FLAGS_WAS_BUSY;

		/* Have the test thread release mutex 2. */
		send_cmd (test_thread_id, CMD_RELEASE_ALL);
		sleep (1);

		/* Destroy the mutexes. */
		for (i = 0; i < 3; i++)
			assert (pthread_mutex_destroy (&m[i]) == 0);
	}
}


static void
mutex_prioinherit_test (void)
{
	pthread_mutexattr_t mattr;
	struct sched_param param;
	pthread_mutex_t	m[3];
	mutex_kind_t	mkind;
	int		i, policy, my_prio;

	/* Get this threads current priority. */
	assert (pthread_getschedparam (pthread_self(), &policy,
	    &param) == 0);
	my_prio = param.sched_priority;	/* save for later use */
	log_trace ("Current scheduling policy %d, priority %d\n",
	    policy, my_prio);

	log ("Testing priority inheritence\n");
	log ("----------------------------\n");
	for (mkind = M_POSIX; mkind <= M_SS2_RECURSIVE; mkind++) {

		log ("  Protype PTHREAD_PRIO_INHERIT, Type %s\n",
		    mutextype_strs[mkind]);

		/*
		 * Initialize and create a mutex.
		 */
		assert (pthread_mutexattr_init (&mattr) == 0);

		/*
		 * Initialize and create 3 priority inheritence mutexes with
		 * default (max priority) ceilings.
		 */
		assert (pthread_mutexattr_setprotocol(&mattr,
		    PTHREAD_PRIO_INHERIT) == 0);

		/*
		 * Ensure that the first mutex type is a POSIX
		 * compliant mutex.
		 */
		if (mkind != M_POSIX) {
			assert (pthread_mutexattr_settype (&mattr,
			    mutex_types[mkind]) == 0);
		}

		for (i = 0; i < 3; i++)
			assert (pthread_mutex_init (&m[i], &mattr) == 0);

		/*
		 * Test setup:
		 *   Thread 4 - take mutex 0, 1
		 *   Thread 2 - enter protected busy loop with mutex 0
		 *   Thread 3 - enter protected busy loop with mutex 1
		 *   Thread 4 - enter protected busy loop with mutex 2
		 *   Thread 5 - enter busy loop
		 *   Thread 6 - enter protected busy loop with mutex 0
		 *   Thread 4 - releases mutexes 1 and 0.
		 *
		 * Expected results:
		 *   Threads complete in order 4, 6, 5, 3, 2
		 */
		log ("    Simple inheritence test - ");

		/*
		 * Command thread 4 to take mutexes 0 and 1.
		 */
		send_mutex_cmd (4, CMD_TAKE_MUTEX, &m[0]);
		sleep (1);	/* Allow command to be received. */
		send_mutex_cmd (4, CMD_TAKE_MUTEX, &m[1]);
		sleep (1);

		/*
		 * Tell the threads to report themselves when they are
		 * at the bottom of their loop (waiting on wait_mutex).
		 */
		for (i = 0; i < NUM_THREADS; i++)
			states[i].flags |= FLAGS_REPORT_WAITMUTEX;

		/*
		 * Command thread 2 to take mutex 0 and thread 3 to take
		 * mutex 1, both via a protected operation command.  Since
		 * thread 4 owns mutexes 0 and 1, both threads 2 and 3
		 * will block until the mutexes are released by thread 4.
		 */
		log_trace ("Commanding protected operation to thread 2.\n");
		send_mutex_cmd (2, CMD_PROTECTED_OP, &m[0]);
		log_trace ("Commanding protected operation to thread 3.\n");
		send_mutex_cmd (3, CMD_PROTECTED_OP, &m[1]);
		sleep (1);

		/*
		 * Command thread 4 to take mutex 2 via a protected operation
		 * and thread 5 to enter a busy loop for 5 seconds.  Since
		 * thread 5 has higher priority than thread 4, thread 5 will
		 * enter the busy loop before thread 4 is activated.
		 */
		log_trace ("Commanding protected operation to thread 4.\n");
		send_mutex_cmd (4, CMD_PROTECTED_OP, &m[2]);
		log_trace ("Commanding busy loop to thread 5.\n");
		send_cmd (5, CMD_BUSY_LOOP);
		sleep (1);
		if ((states[5].flags & FLAGS_IS_BUSY) == 0)
			log_error ("thread 5 is not running.\n");
		log_trace ("Commanding protected operation thread 6.\n");
		send_mutex_cmd (6, CMD_PROTECTED_OP, &m[0]);
		sleep (1);
		if ((states[4].flags & FLAGS_WAS_BUSY) == 0)
			log_error ("thread 4 failed to inherit priority.\n");
		states[4].flags = 0;
		send_cmd (4, CMD_RELEASE_ALL);
		sleep (5);
		check_run_order ("4,6,5,3,2");

		/*
		 * Clear the flags.
		 */
		for (i = 0; i < NUM_THREADS; i++)
			states[i].flags = 0;

		/*
		 * Test setup:
		 *   Thread 2 - enter busy loop (SCHED_FIFO)
		 *   Thread 4 - take mutex 0
		 *   Thread 4 - priority change to same priority as thread 2
		 *   Thread 4 - release mutex 0
		 *
		 * Expected results:
		 *   Since thread 4 owns a priority mutex, it should be
		 *   placed at the front of the run queue (for its new
		 *   priority slot) when its priority is lowered to the
		 *   same priority as thread 2.  If thread 4 did not own
		 *   a priority mutex, then it would have been added to
		 *   the end of the run queue and thread 2 would have
		 *   executed until it blocked (because it's scheduling
		 *   policy is SCHED_FIFO).
		 *   
		 */
		log ("    Inheritence test with change of priority - ");

		/*
		 * Change threads 2 and 4 scheduling policies to be
		 * SCHED_FIFO.
		 */
		param.sched_priority = states[2].priority;
		assert (pthread_setschedparam (states[2].tid, SCHED_FIFO,
		    &param) == 0);
		param.sched_priority = states[4].priority;
		assert (pthread_setschedparam (states[4].tid, SCHED_FIFO,
		    &param) == 0);

		/*
		 * Command thread 4 to take mutex 0.
		 */
		send_mutex_cmd (4, CMD_TAKE_MUTEX, &m[0]);
		sleep (1);

		/*
		 * Command thread 2 to enter busy loop.
		 */
		send_cmd (2, CMD_BUSY_LOOP);
		sleep (1);	/* Allow command to be received. */

		/*
		 * Command thread 4 to enter busy loop.
		 */
		send_cmd (4, CMD_BUSY_LOOP);
		sleep (1);	/* Allow command to be received. */

		/* Have threads 2 and 4 report themselves. */
		states[2].flags = FLAGS_REPORT_WAITMUTEX;
		states[4].flags = FLAGS_REPORT_WAITMUTEX;

		/* Change the priority of thread 4. */
		param.sched_priority = states[2].priority;
		assert (pthread_setschedparam (states[4].tid, SCHED_FIFO,
		    &param) == 0);
		sleep (5);
		check_run_order ("4,2");

		/* Clear the flags */
		states[2].flags = 0;
		states[4].flags = 0;

		/* Reset the policies. */
		param.sched_priority = states[2].priority;
		assert (pthread_setschedparam (states[2].tid, SCHED_RR,
		    &param) == 0);
		param.sched_priority = states[4].priority;
		assert (pthread_setschedparam (states[4].tid, SCHED_RR,
		    &param) == 0);

		send_cmd (4, CMD_RELEASE_MUTEX);
		sleep (1);

		/* Destroy the mutexes. */
		for (i = 0; i < 3; i++)
			assert (pthread_mutex_destroy (&m[i]) == 0);
	}
}


int main (int argc, char *argv[])
{
	pthread_mutexattr_t mattr;
	pthread_condattr_t cattr;
	pthread_attr_t	pattr;
	int		i, policy, main_prio;
	void *		exit_status;
	sigset_t	mask;
	struct sigaction act;
	struct sched_param param;

	assert (pthread_getschedparam (pthread_self (), &policy, &param) == 0);
	main_prio = param.sched_priority;

	/* Setupt our signal mask. */
	sigfillset (&mask);
	sigdelset (&mask, SIGINT);
	sigprocmask (SIG_SETMASK, &mask, NULL);

	/* Install a signal handler for SIGINT */
	sigemptyset (&act.sa_mask);
	sigaddset (&act.sa_mask, SIGINT);
	act.sa_handler = sighandler;
	act.sa_flags = SA_RESTART;
	sigaction (SIGINT, &act, NULL);

	/*
	 * Initialize the thread attribute.
	 */
	assert (pthread_attr_init (&pattr) == 0);
	assert (pthread_attr_setdetachstate (&pattr,
	    PTHREAD_CREATE_JOINABLE) == 0);

	/*
	 * Initialize and create the waiter and condvar mutexes.
	 */
	assert (pthread_mutexattr_init (&mattr) == 0);
	assert (pthread_mutex_init (&waiter_mutex, &mattr) == 0);
	assert (pthread_mutex_init (&cond_mutex, &mattr) == 0);

	/*
	 * Initialize and create a condition variable.
	 */
	assert (pthread_condattr_init (&cattr) == 0);
	assert (pthread_cond_init (&cond_var, &cattr) == 0);

	/* Create a pipe to catch the results of thread wakeups. */
	assert (pipe (pipefd) == 0);

#ifdef DEBUG
	assert (pthread_switch_add_np (kern_switch) == 0);
#endif

	/*
	 * Create the waiting threads.
	 */
	for (i = 0; i < NUM_THREADS; i++) {
		assert (pthread_cond_init (&states[i].cond_var, &cattr) == 0);
		states[i].id = (u_int8_t) i;  /* NUM_THREADS must be <= 256 */
		states[i].status = 0;
		states[i].cmd.cmd_id = CMD_NONE;
		states[i].flags = 0;	/* No flags yet. */
		assert (pthread_create (&states[i].tid, &pattr, waiter,
		    (void *) &states[i]) == 0);
		param.sched_priority = main_prio - 10 + i;
		states[i].priority = param.sched_priority;
		assert (pthread_setschedparam (states[i].tid, SCHED_OTHER,
		    &param) == 0);
#if defined(_LIBC_R_)
		{
			char buf[30];

			snprintf (buf, sizeof(buf), "waiter_%d", i);
			pthread_set_name_np (states[i].tid, buf);
		}
#endif
	}

	/* Allow the threads to start. */
	sleep (1);
	log_trace ("Done creating threads.\n");

	log ("\n");
	mutex_init_test ();
	log ("\n");
	mutex_destroy_test ();
	log ("\n");
	mutex_lock_test ();
	log ("\n");
	mutex_unlock_test ();
	log ("\n");
	queueing_order_test ();
	log ("\n");
	mutex_prioinherit_test ();
	log ("\n");
	mutex_prioceiling_test ();
	log ("\n");

	log ("Total tests %d, passed %d, failed %d\n",
	    total, pass_count, error_count);

	/* Set the done flag and signal the threads to exit. */
	log_trace ("Setting done flag.\n");
	done = 1;

	/*
	 * Wait for the threads to finish.
	 */
	log_trace ("Trying to join threads.\n");
	for (i = 0; i < NUM_THREADS; i++) {
		send_cmd (i, CMD_NONE);
		assert (pthread_join (states[i].tid, &exit_status) == 0);
	}

	/* Clean up after ourselves. */
	close (pipefd[0]);
	close (pipefd[1]);

	if (error_count != 0)
		exit (EX_OSERR);	/* any better ideas??? */
	else
		exit (EX_OK);
}
