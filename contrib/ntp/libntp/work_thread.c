/*
 * work_thread.c - threads implementation for blocking worker child.
 */
#include <config.h>
#include "ntp_workimpl.h"

#ifdef WORK_THREAD

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#ifndef SYS_WINNT
#include <pthread.h>
#endif

#include "ntp_stdlib.h"
#include "ntp_malloc.h"
#include "ntp_syslog.h"
#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_assert.h"
#include "ntp_unixtime.h"
#include "timespecops.h"
#include "ntp_worker.h"

#define CHILD_EXIT_REQ	((blocking_pipe_header *)(intptr_t)-1)
#define CHILD_GONE_RESP	CHILD_EXIT_REQ
#define WORKITEMS_ALLOC_INC	16
#define RESPONSES_ALLOC_INC	4

#ifndef THREAD_MINSTACKSIZE
#define THREAD_MINSTACKSIZE	(64U * 1024)
#endif

#ifndef DEVOLATILE
#define DEVOLATILE(type, var) ((type)(uintptr_t)(volatile void *)(var))
#endif

#ifdef SYS_WINNT
# define thread_exit(c)	_endthreadex(c)
# define tickle_sem	SetEvent
#else
# define thread_exit(c)	pthread_exit((void*)(size_t)(c))
# define tickle_sem	sem_post
#endif

#ifdef WORK_PIPE
addremove_io_fd_func		addremove_io_fd;
#else
addremove_io_semaphore_func	addremove_io_semaphore;
#endif

static	void	start_blocking_thread(blocking_child *);
static	void	start_blocking_thread_internal(blocking_child *);
static	void	prepare_child_sems(blocking_child *);
static	int	wait_for_sem(sem_ref, struct timespec *);
static	void	ensure_workitems_empty_slot(blocking_child *);
static	void	ensure_workresp_empty_slot(blocking_child *);
static	int	queue_req_pointer(blocking_child *, blocking_pipe_header *);
static	void	cleanup_after_child(blocking_child *);
#ifdef SYS_WINNT
u_int	WINAPI	blocking_thread(void *);
#else
void *		blocking_thread(void *);
#endif
#ifndef SYS_WINNT
static	void	block_thread_signals(sigset_t *);
#endif


void
exit_worker(
	int	exitcode
	)
{
	thread_exit(exitcode);	/* see #define thread_exit */
}


int
worker_sleep(
	blocking_child *	c,
	time_t			seconds
	)
{
	struct timespec	until;
	int		rc;

# ifdef HAVE_CLOCK_GETTIME
	if (0 != clock_gettime(CLOCK_REALTIME, &until)) {
		msyslog(LOG_ERR, "worker_sleep: clock_gettime() failed: %m");
		return -1;
	}
# else
	if (0 != getclock(TIMEOFDAY, &until)) {
		msyslog(LOG_ERR, "worker_sleep: getclock() failed: %m");
		return -1;
	}
# endif
	until.tv_sec += seconds;
	do {
		rc = wait_for_sem(c->wake_scheduled_sleep, &until);
	} while (-1 == rc && EINTR == errno);
	if (0 == rc)
		return -1;
	if (-1 == rc && ETIMEDOUT == errno)
		return 0;
	msyslog(LOG_ERR, "worker_sleep: sem_timedwait: %m");
	return -1;
}


void
interrupt_worker_sleep(void)
{
	u_int			idx;
	blocking_child *	c;

	for (idx = 0; idx < blocking_children_alloc; idx++) {
		c = blocking_children[idx];
		if (NULL == c || NULL == c->wake_scheduled_sleep)
			continue;
		tickle_sem(c->wake_scheduled_sleep);
	}
}


static void
ensure_workitems_empty_slot(
	blocking_child *c
	)
{
	const size_t	each = sizeof(blocking_children[0]->workitems[0]);
	size_t		new_alloc;
	size_t		old_octets;
	size_t		new_octets;
	void *		nonvol_workitems;


	if (c->workitems != NULL &&
	    NULL == c->workitems[c->next_workitem])
		return;

	new_alloc = c->workitems_alloc + WORKITEMS_ALLOC_INC;
	old_octets = c->workitems_alloc * each;
	new_octets = new_alloc * each;
	nonvol_workitems = DEVOLATILE(void *, c->workitems);
	c->workitems = erealloc_zero(nonvol_workitems, new_octets,
				     old_octets);
	if (0 == c->next_workitem)
		c->next_workitem = c->workitems_alloc;
	c->workitems_alloc = new_alloc;
}


static void
ensure_workresp_empty_slot(
	blocking_child *c
	)
{
	const size_t	each = sizeof(blocking_children[0]->responses[0]);
	size_t		new_alloc;
	size_t		old_octets;
	size_t		new_octets;
	void *		nonvol_responses;

	if (c->responses != NULL &&
	    NULL == c->responses[c->next_response])
		return;

	new_alloc = c->responses_alloc + RESPONSES_ALLOC_INC;
	old_octets = c->responses_alloc * each;
	new_octets = new_alloc * each;
	nonvol_responses = DEVOLATILE(void *, c->responses);
	c->responses = erealloc_zero(nonvol_responses, new_octets,
				     old_octets);
	if (0 == c->next_response)
		c->next_response = c->responses_alloc;
	c->responses_alloc = new_alloc;
}


/*
 * queue_req_pointer() - append a work item or idle exit request to
 *			 blocking_workitems[].
 */
static int
queue_req_pointer(
	blocking_child	*	c,
	blocking_pipe_header *	hdr
	)
{
	c->workitems[c->next_workitem] = hdr;
	c->next_workitem = (1 + c->next_workitem) % c->workitems_alloc;

	/*
	 * We only want to signal the wakeup event if the child is
	 * blocking on it, which is indicated by setting the blocking
	 * event.  Wait with zero timeout to test.
	 */
	/* !!!! if (WAIT_OBJECT_0 == WaitForSingleObject(c->child_is_blocking, 0)) */
		tickle_sem(c->blocking_req_ready);

	return 0;
}


int
send_blocking_req_internal(
	blocking_child *	c,
	blocking_pipe_header *	hdr,
	void *			data
	)
{
	blocking_pipe_header *	threadcopy;
	size_t			payload_octets;

	REQUIRE(hdr != NULL);
	REQUIRE(data != NULL);
	DEBUG_REQUIRE(BLOCKING_REQ_MAGIC == hdr->magic_sig);

	if (hdr->octets <= sizeof(*hdr))
		return 1;	/* failure */
	payload_octets = hdr->octets - sizeof(*hdr);

	ensure_workitems_empty_slot(c);
	if (NULL == c->thread_ref) {
		ensure_workresp_empty_slot(c);
		start_blocking_thread(c);
	}

	threadcopy = emalloc(hdr->octets);
	memcpy(threadcopy, hdr, sizeof(*hdr));
	memcpy((char *)threadcopy + sizeof(*hdr), data, payload_octets);

	return queue_req_pointer(c, threadcopy);
}


blocking_pipe_header *
receive_blocking_req_internal(
	blocking_child *	c
	)
{
	blocking_pipe_header *	req;
	int			rc;

	/*
	 * Child blocks here when idle.  SysV semaphores maintain a
	 * count and release from sem_wait() only when it reaches 0.
	 * Windows auto-reset events are simpler, and multiple SetEvent
	 * calls before any thread waits result in a single wakeup.
	 * On Windows, the child drains all workitems each wakeup, while
	 * with SysV semaphores wait_sem() is used before each item.
	 */
#ifdef SYS_WINNT
	while (NULL == c->workitems[c->next_workeritem]) {
		/* !!!! SetEvent(c->child_is_blocking); */
		rc = wait_for_sem(c->blocking_req_ready, NULL);
		INSIST(0 == rc);
		/* !!!! ResetEvent(c->child_is_blocking); */
	}
#else
	do {
		rc = wait_for_sem(c->blocking_req_ready, NULL);
	} while (-1 == rc && EINTR == errno);
	INSIST(0 == rc);
#endif

	req = c->workitems[c->next_workeritem];
	INSIST(NULL != req);
	c->workitems[c->next_workeritem] = NULL;
	c->next_workeritem = (1 + c->next_workeritem) %
				c->workitems_alloc;

	if (CHILD_EXIT_REQ == req) {	/* idled out */
		send_blocking_resp_internal(c, CHILD_GONE_RESP);
		req = NULL;
	}

	return req;
}


int
send_blocking_resp_internal(
	blocking_child *	c,
	blocking_pipe_header *	resp
	)
{
	ensure_workresp_empty_slot(c);

	c->responses[c->next_response] = resp;
	c->next_response = (1 + c->next_response) % c->responses_alloc;

#ifdef WORK_PIPE
	write(c->resp_write_pipe, "", 1);
#else
	tickle_sem(c->blocking_response_ready);
#endif

	return 0;
}


#ifndef WORK_PIPE
void
handle_blocking_resp_sem(
	void *	context
	)
{
	HANDLE			ready;
	blocking_child *	c;
	u_int			idx;

	ready = (HANDLE)context;
	c = NULL;
	for (idx = 0; idx < blocking_children_alloc; idx++) {
		c = blocking_children[idx];
		if (c != NULL && c->thread_ref != NULL &&
		    ready == c->blocking_response_ready)
			break;
	}
	if (idx < blocking_children_alloc)
		process_blocking_resp(c);
}
#endif	/* !WORK_PIPE */


blocking_pipe_header *
receive_blocking_resp_internal(
	blocking_child *	c
	)
{
	blocking_pipe_header *	removed;
#ifdef WORK_PIPE
	int			rc;
	char			scratch[32];

	do {
		rc = read(c->resp_read_pipe, scratch, sizeof(scratch));
	} while (-1 == rc && EINTR == errno);
#endif
	removed = c->responses[c->next_workresp];
	if (NULL != removed) {
		c->responses[c->next_workresp] = NULL;
		c->next_workresp = (1 + c->next_workresp) %
				   c->responses_alloc;
		DEBUG_ENSURE(CHILD_GONE_RESP == removed ||
			     BLOCKING_RESP_MAGIC == removed->magic_sig);
	}
	if (CHILD_GONE_RESP == removed) {
		cleanup_after_child(c);
		removed = NULL;
	}

	return removed;
}


static void
start_blocking_thread(
	blocking_child *	c
	)
{

	DEBUG_INSIST(!c->reusable);

	prepare_child_sems(c);
	start_blocking_thread_internal(c);
}


static void
start_blocking_thread_internal(
	blocking_child *	c
	)
#ifdef SYS_WINNT
{
	thr_ref	blocking_child_thread;
	u_int	blocking_thread_id;
	BOOL	resumed;

	(*addremove_io_semaphore)(c->blocking_response_ready, FALSE);
	blocking_child_thread =
		(HANDLE)_beginthreadex(
			NULL,
			0,
			&blocking_thread,
			c,
			CREATE_SUSPENDED,
			&blocking_thread_id);

	if (NULL == blocking_child_thread) {
		msyslog(LOG_ERR, "start blocking thread failed: %m");
		exit(-1);
	}
	c->thread_id = blocking_thread_id;
	c->thread_ref = blocking_child_thread;
	/* remember the thread priority is only within the process class */
	if (!SetThreadPriority(blocking_child_thread,
			       THREAD_PRIORITY_BELOW_NORMAL))
		msyslog(LOG_ERR, "Error lowering blocking thread priority: %m");

	resumed = ResumeThread(blocking_child_thread);
	DEBUG_INSIST(resumed);
}
#else	/* pthreads start_blocking_thread_internal() follows */
{
# ifdef NEED_PTHREAD_INIT
	static int	pthread_init_called;
# endif
	pthread_attr_t	thr_attr;
	int		rc;
	int		saved_errno;
	int		pipe_ends[2];	/* read then write */
	int		is_pipe;
	int		flags;
	size_t		stacksize;
	sigset_t	saved_sig_mask;

# ifdef NEED_PTHREAD_INIT
	/*
	 * from lib/isc/unix/app.c:
	 * BSDI 3.1 seg faults in pthread_sigmask() if we don't do this.
	 */
	if (!pthread_init_called) {
		pthread_init();
		pthread_init_called = TRUE;
	}
# endif

	rc = pipe_socketpair(&pipe_ends[0], &is_pipe);
	if (0 != rc) {
		msyslog(LOG_ERR, "start_blocking_thread: pipe_socketpair() %m");
		exit(1);
	}
	c->resp_read_pipe = move_fd(pipe_ends[0]);
	c->resp_write_pipe = move_fd(pipe_ends[1]);
	c->ispipe = is_pipe;
	flags = fcntl(c->resp_read_pipe, F_GETFL, 0);
	if (-1 == flags) {
		msyslog(LOG_ERR, "start_blocking_thread: fcntl(F_GETFL) %m");
		exit(1);
	}
	rc = fcntl(c->resp_read_pipe, F_SETFL, O_NONBLOCK | flags);
	if (-1 == rc) {
		msyslog(LOG_ERR,
			"start_blocking_thread: fcntl(F_SETFL, O_NONBLOCK) %m");
		exit(1);
	}
	(*addremove_io_fd)(c->resp_read_pipe, c->ispipe, FALSE);
	pthread_attr_init(&thr_attr);
	pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_DETACHED);
#if defined(HAVE_PTHREAD_ATTR_GETSTACKSIZE) && \
    defined(HAVE_PTHREAD_ATTR_SETSTACKSIZE)
	rc = pthread_attr_getstacksize(&thr_attr, &stacksize);
	if (-1 == rc) {
		msyslog(LOG_ERR,
			"start_blocking_thread: pthread_attr_getstacksize %m");
	} else if (stacksize < THREAD_MINSTACKSIZE) {
		rc = pthread_attr_setstacksize(&thr_attr,
					       THREAD_MINSTACKSIZE);
		if (-1 == rc)
			msyslog(LOG_ERR,
				"start_blocking_thread: pthread_attr_setstacksize(0x%lx -> 0x%lx) %m",
				(u_long)stacksize,
				(u_long)THREAD_MINSTACKSIZE);
	}
#else
	UNUSED_ARG(stacksize);
#endif
#if defined(PTHREAD_SCOPE_SYSTEM) && defined(NEED_PTHREAD_SCOPE_SYSTEM)
	pthread_attr_setscope(&thr_attr, PTHREAD_SCOPE_SYSTEM);
#endif
	c->thread_ref = emalloc_zero(sizeof(*c->thread_ref));
	block_thread_signals(&saved_sig_mask);
	rc = pthread_create(c->thread_ref, &thr_attr,
			    &blocking_thread, c);
	saved_errno = errno;
	pthread_sigmask(SIG_SETMASK, &saved_sig_mask, NULL);
	pthread_attr_destroy(&thr_attr);
	if (0 != rc) {
		errno = saved_errno;
		msyslog(LOG_ERR, "pthread_create() blocking child: %m");
		exit(1);
	}
}
#endif


/*
 * block_thread_signals()
 *
 * Temporarily block signals used by ntpd main thread, so that signal
 * mask inherited by child threads leaves them blocked.  Returns prior
 * active signal mask via pmask, to be restored by the main thread
 * after pthread_create().
 */
#ifndef SYS_WINNT
void
block_thread_signals(
	sigset_t *	pmask
	)
{
	sigset_t	block;

	sigemptyset(&block);
# ifdef HAVE_SIGNALED_IO
#  ifdef SIGIO
	sigaddset(&block, SIGIO);
#  endif
#  ifdef SIGPOLL
	sigaddset(&block, SIGPOLL);
#  endif
# endif	/* HAVE_SIGNALED_IO */
	sigaddset(&block, SIGALRM);
	sigaddset(&block, MOREDEBUGSIG);
	sigaddset(&block, LESSDEBUGSIG);
# ifdef SIGDIE1
	sigaddset(&block, SIGDIE1);
# endif
# ifdef SIGDIE2
	sigaddset(&block, SIGDIE2);
# endif
# ifdef SIGDIE3
	sigaddset(&block, SIGDIE3);
# endif
# ifdef SIGDIE4
	sigaddset(&block, SIGDIE4);
# endif
# ifdef SIGBUS
	sigaddset(&block, SIGBUS);
# endif
	sigemptyset(pmask);
	pthread_sigmask(SIG_BLOCK, &block, pmask);
}
#endif	/* !SYS_WINNT */


/*
 * prepare_child_sems()
 *
 * create sync events (semaphores)
 * child_is_blocking initially unset
 * blocking_req_ready initially unset
 *
 * Child waits for blocking_req_ready to be set after
 * setting child_is_blocking.  blocking_req_ready and
 * blocking_response_ready are auto-reset, so wake one
 * waiter and become unset (unsignalled) in one operation.
 */
static void
prepare_child_sems(
	blocking_child *c
	)
#ifdef SYS_WINNT
{
	if (NULL == c->blocking_req_ready) {
		/* manual reset using ResetEvent() */
		/* !!!! c->child_is_blocking = CreateEvent(NULL, TRUE, FALSE, NULL); */
		/* auto reset - one thread released from wait each set */
		c->blocking_req_ready = CreateEvent(NULL, FALSE, FALSE, NULL);
		c->blocking_response_ready = CreateEvent(NULL, FALSE, FALSE, NULL);
		c->wake_scheduled_sleep = CreateEvent(NULL, FALSE, FALSE, NULL);
	} else {
		/* !!!! ResetEvent(c->child_is_blocking); */
		/* ResetEvent(c->blocking_req_ready); */
		/* ResetEvent(c->blocking_response_ready); */
		/* ResetEvent(c->wake_scheduled_sleep); */
	}
}
#else	/* pthreads prepare_child_sems() follows */
{
	size_t	octets;

	if (NULL == c->blocking_req_ready) {
		octets = sizeof(*c->blocking_req_ready);
		octets += sizeof(*c->wake_scheduled_sleep);
		/* !!!! octets += sizeof(*c->child_is_blocking); */
		c->blocking_req_ready = emalloc_zero(octets);;
		c->wake_scheduled_sleep = 1 + c->blocking_req_ready;
		/* !!!! c->child_is_blocking = 1 + c->wake_scheduled_sleep; */
	} else {
		sem_destroy(c->blocking_req_ready);
		sem_destroy(c->wake_scheduled_sleep);
		/* !!!! sem_destroy(c->child_is_blocking); */
	}
	sem_init(c->blocking_req_ready, FALSE, 0);
	sem_init(c->wake_scheduled_sleep, FALSE, 0);
	/* !!!! sem_init(c->child_is_blocking, FALSE, 0); */
}
#endif


static int
wait_for_sem(
	sem_ref			sem,
	struct timespec *	timeout		/* wall-clock */
	)
#ifdef SYS_WINNT
{
	struct timespec now;
	struct timespec delta;
	DWORD		msec;
	DWORD		rc;

	if (NULL == timeout) {
		msec = INFINITE;
	} else {
		getclock(TIMEOFDAY, &now);
		delta = sub_tspec(*timeout, now);
		if (delta.tv_sec < 0) {
			msec = 0;
		} else if ((delta.tv_sec + 1) >= (MAXDWORD / 1000)) {
			msec = INFINITE;
		} else {
			msec = 1000 * (DWORD)delta.tv_sec;
			msec += delta.tv_nsec / (1000 * 1000);
		}
	}
	rc = WaitForSingleObject(sem, msec);
	if (WAIT_OBJECT_0 == rc)
		return 0;
	if (WAIT_TIMEOUT == rc) {
		errno = ETIMEDOUT;
		return -1;
	}
	msyslog(LOG_ERR, "WaitForSingleObject unexpected 0x%x", rc);
	errno = EFAULT;
	return -1;
}
#else	/* pthreads wait_for_sem() follows */
{
	int rc;

	if (NULL == timeout)
		rc = sem_wait(sem);
	else
		rc = sem_timedwait(sem, timeout);

	return rc;
}
#endif


/*
 * blocking_thread - thread functions have WINAPI calling convention
 */
#ifdef SYS_WINNT
u_int
WINAPI
#else
void *
#endif
blocking_thread(
	void *	ThreadArg
	)
{
	blocking_child *c;

	c = ThreadArg;
	exit_worker(blocking_child_common(c));

	/* NOTREACHED */
	return 0;
}


/*
 * req_child_exit() runs in the parent.
 */
int
req_child_exit(
	blocking_child *c
	)
{
	return queue_req_pointer(c, CHILD_EXIT_REQ);
}


/*
 * cleanup_after_child() runs in parent.
 */
static void
cleanup_after_child(
	blocking_child *	c
	)
{
	u_int	idx;

	DEBUG_INSIST(!c->reusable);
#ifdef SYS_WINNT
	INSIST(CloseHandle(c->thread_ref));
#else
	free(c->thread_ref);
#endif
	c->thread_ref = NULL;
	c->thread_id = 0;
#ifdef WORK_PIPE
	DEBUG_INSIST(-1 != c->resp_read_pipe);
	DEBUG_INSIST(-1 != c->resp_write_pipe);
	(*addremove_io_fd)(c->resp_read_pipe, c->ispipe, TRUE);
	close(c->resp_write_pipe);
	close(c->resp_read_pipe);
	c->resp_write_pipe = -1;
	c->resp_read_pipe = -1;
#else
	DEBUG_INSIST(NULL != c->blocking_response_ready);
	(*addremove_io_semaphore)(c->blocking_response_ready, TRUE);
#endif
	for (idx = 0; idx < c->workitems_alloc; idx++)
		c->workitems[idx] = NULL;
	c->next_workitem = 0;
	c->next_workeritem = 0;
	for (idx = 0; idx < c->responses_alloc; idx++)
		c->responses[idx] = NULL;
	c->next_response = 0;
	c->next_workresp = 0;
	c->reusable = TRUE;
}


#else	/* !WORK_THREAD follows */
char work_thread_nonempty_compilation_unit;
#endif
