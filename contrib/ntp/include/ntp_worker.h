/*
 * ntp_worker.h
 */

#ifndef NTP_WORKER_H
#define NTP_WORKER_H

#include "ntp_workimpl.h"

#ifdef WORKER
# if defined(WORK_THREAD) && defined(WORK_PIPE)
#  ifdef HAVE_SEMAPHORE_H
#   include <semaphore.h>
#  endif
# endif
#include "ntp_stdlib.h"

/* #define TEST_BLOCKING_WORKER */	/* ntp_config.c ntp_intres.c */

typedef enum blocking_work_req_tag {
	BLOCKING_GETNAMEINFO,
	BLOCKING_GETADDRINFO,
} blocking_work_req;

typedef void (*blocking_work_callback)(blocking_work_req, void *, size_t, void *);

typedef enum blocking_magic_sig_e {
	BLOCKING_REQ_MAGIC  = 0x510c7ecf,
	BLOCKING_RESP_MAGIC = 0x510c7e54,
} blocking_magic_sig;

/*
 * The same header is used for both requests to and responses from
 * the child.  In the child, done_func and context are opaque.
 */
typedef struct blocking_pipe_header_tag {
	size_t			octets;
	blocking_magic_sig	magic_sig;
	blocking_work_req	rtype;
	u_int			child_idx;
	blocking_work_callback	done_func;
	void *			context;
} blocking_pipe_header;

# ifdef WORK_THREAD
#  ifdef WORK_PIPE
typedef pthread_t *	thr_ref;
typedef sem_t *		sem_ref;
#  else
typedef HANDLE		thr_ref;
typedef HANDLE		sem_ref;
#  endif
# endif

/*
 *
 */
#ifdef WORK_FORK
typedef struct blocking_child_tag {
	int	reusable;
	int	pid;
	int	req_write_pipe;		/* parent */
	int	resp_read_pipe;
	void *	resp_read_ctx;
	int	req_read_pipe;		/* child */
	int	resp_write_pipe;
	int	ispipe;
} blocking_child;
#elif defined(WORK_THREAD)
typedef struct blocking_child_tag {
/*
 * blocking workitems and blocking_responses are dynamically-sized
 * one-dimensional arrays of pointers to blocking worker requests and
 * responses.
 */
	int			reusable;
	thr_ref			thread_ref;
	u_int			thread_id;
	blocking_pipe_header * volatile * volatile 
				workitems;
	volatile size_t		workitems_alloc;
	size_t			next_workitem;	 /* parent */
	size_t			next_workeritem; /* child */
	blocking_pipe_header * volatile * volatile 
				responses;
	volatile size_t		responses_alloc;
	size_t			next_response;	/* child */
	size_t			next_workresp;	/* parent */
	/* event handles / sem_t pointers */
	/* sem_ref		child_is_blocking; */
	sem_ref			blocking_req_ready;
	sem_ref			wake_scheduled_sleep;
#ifdef WORK_PIPE
	int			resp_read_pipe;	/* parent */
	int			resp_write_pipe;/* child */
	int			ispipe;
	void *			resp_read_ctx;	/* child */
#else
	sem_ref			blocking_response_ready;
#endif
} blocking_child;

#endif	/* WORK_THREAD */

extern	blocking_child **	blocking_children;
extern	size_t			blocking_children_alloc;
extern	int			worker_per_query;	/* boolean */
extern	int			intres_req_pending;

extern	u_int	available_blocking_child_slot(void);
extern	int	queue_blocking_request(blocking_work_req, void *,
				       size_t, blocking_work_callback,
				       void *);
extern	int	queue_blocking_response(blocking_child *, 
					blocking_pipe_header *, size_t,
					const blocking_pipe_header *);
extern	void	process_blocking_resp(blocking_child *);
extern	int	send_blocking_req_internal(blocking_child *,
					   blocking_pipe_header *,
					   void *);
extern	int	send_blocking_resp_internal(blocking_child *,
					    blocking_pipe_header *);
extern	blocking_pipe_header *
		receive_blocking_req_internal(blocking_child *);
extern	blocking_pipe_header *
		receive_blocking_resp_internal(blocking_child *);
extern	int	blocking_child_common(blocking_child *);
extern	void	exit_worker(int)
			__attribute__ ((__noreturn__));
extern	int	worker_sleep(blocking_child *, time_t);
extern	void	worker_idle_timer_fired(void);
extern	void	interrupt_worker_sleep(void);
extern	int	req_child_exit(blocking_child *);
#ifndef HAVE_IO_COMPLETION_PORT
extern	int	pipe_socketpair(int fds[2], int *is_pipe);
extern	void	close_all_beyond(int);
extern	void	close_all_except(int);
extern	void	kill_asyncio	(int);
#endif

# ifdef WORK_PIPE
typedef	void	(*addremove_io_fd_func)(int, int, int);
extern	addremove_io_fd_func		addremove_io_fd;
# else
extern	void	handle_blocking_resp_sem(void *);
typedef	void	(*addremove_io_semaphore_func)(sem_ref, int);
extern	addremove_io_semaphore_func	addremove_io_semaphore;
# endif

# ifdef WORK_FORK
extern	int				worker_process;
# endif

#endif	/* WORKER */

#if defined(HAVE_DROPROOT) && defined(WORK_FORK)
extern void	fork_deferred_worker(void);
#else
# define	fork_deferred_worker()	do {} while (0)
#endif

#endif	/* !NTP_WORKER_H */
