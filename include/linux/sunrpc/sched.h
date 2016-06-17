/*
 * linux/include/linux/sunrpc/sched.h
 *
 * Scheduling primitives for kernel Sun RPC.
 *
 * Copyright (C) 1996, Olaf Kirch <okir@monad.swb.de>
 */

#ifndef _LINUX_SUNRPC_SCHED_H_
#define _LINUX_SUNRPC_SCHED_H_

#include <linux/timer.h>
#include <linux/tqueue.h>
#include <linux/sunrpc/types.h>
#include <linux/wait.h>

/*
 * Define this if you want to test the fast scheduler for async calls.
 * This is still experimental and may not work.
 */
#undef  CONFIG_RPC_FASTSCHED

/*
 * This is the actual RPC procedure call info.
 */
struct rpc_message {
	__u32			rpc_proc;	/* Procedure number */
	void *			rpc_argp;	/* Arguments */
	void *			rpc_resp;	/* Result */
	struct rpc_cred *	rpc_cred;	/* Credentials */
};

/*
 * This is the RPC task struct
 */
struct rpc_task {
	struct list_head	tk_list;	/* wait queue links */
#ifdef RPC_DEBUG
	unsigned long		tk_magic;	/* 0xf00baa */
#endif
	struct list_head	tk_task;	/* global list of tasks */
	struct rpc_clnt *	tk_client;	/* RPC client */
	struct rpc_rqst *	tk_rqstp;	/* RPC request */
	int			tk_status;	/* result of last operation */
	struct rpc_wait_queue *	tk_rpcwait;	/* RPC wait queue we're on */

	/*
	 * RPC call state
	 */
	struct rpc_message	tk_msg;		/* RPC call info */
	__u32 *			tk_buffer;	/* XDR buffer */
	__u8			tk_garb_retry,
				tk_cred_retry,
				tk_suid_retry;

	/*
	 * timeout_fn   to be executed by timer bottom half
	 * callback	to be executed after waking up
	 * action	next procedure for async tasks
	 * exit		exit async task and report to caller
	 */
	void			(*tk_timeout_fn)(struct rpc_task *);
	void			(*tk_callback)(struct rpc_task *);
	void			(*tk_action)(struct rpc_task *);
	void			(*tk_exit)(struct rpc_task *);
	void			(*tk_release)(struct rpc_task *);
	void *			tk_calldata;

	/*
	 * tk_timer is used for async processing by the RPC scheduling
	 * primitives. You should not access this directly unless
	 * you have a pathological interest in kernel oopses.
	 */
	struct timer_list	tk_timer;	/* kernel timer */
	wait_queue_head_t	tk_wait;	/* sync: sleep on this q */
	unsigned long		tk_timeout;	/* timeout for rpc_sleep() */
	unsigned short		tk_flags;	/* misc flags */
	unsigned char		tk_active   : 1;/* Task has been activated */
	unsigned long		tk_runstate;	/* Task run status */
#ifdef RPC_DEBUG
	unsigned short		tk_pid;		/* debugging aid */
#endif
};
#define tk_auth			tk_client->cl_auth
#define tk_xprt			tk_client->cl_xprt

/* support walking a list of tasks on a wait queue */
#define	task_for_each(task, pos, head) \
	list_for_each(pos, head) \
		if ((task=list_entry(pos, struct rpc_task, tk_list)),1)

#define	task_for_first(task, head) \
	if (!list_empty(head) &&  \
	    ((task=list_entry((head)->next, struct rpc_task, tk_list)),1))

/* .. and walking list of all tasks */
#define	alltask_for_each(task, pos, head) \
	list_for_each(pos, head) \
		if ((task=list_entry(pos, struct rpc_task, tk_task)),1)

typedef void			(*rpc_action)(struct rpc_task *);

/*
 * RPC task flags
 */
#define RPC_TASK_ASYNC		0x0001		/* is an async task */
#define RPC_TASK_SWAPPER	0x0002		/* is swapping in/out */
#define RPC_TASK_SETUID		0x0004		/* is setuid process */
#define RPC_TASK_CHILD		0x0008		/* is child of other task */
#define RPC_CALL_REALUID	0x0010		/* try using real uid */
#define RPC_CALL_MAJORSEEN	0x0020		/* major timeout seen */
#define RPC_TASK_ROOTCREDS	0x0040		/* force root creds */
#define RPC_TASK_DYNAMIC	0x0080		/* task was kmalloc'ed */
#define RPC_TASK_KILLED		0x0100		/* task was killed */

#define RPC_IS_ASYNC(t)		((t)->tk_flags & RPC_TASK_ASYNC)
#define RPC_IS_SETUID(t)	((t)->tk_flags & RPC_TASK_SETUID)
#define RPC_IS_CHILD(t)		((t)->tk_flags & RPC_TASK_CHILD)
#define RPC_IS_SWAPPER(t)	((t)->tk_flags & RPC_TASK_SWAPPER)
#define RPC_DO_ROOTOVERRIDE(t)	((t)->tk_flags & RPC_TASK_ROOTCREDS)
#define RPC_ASSASSINATED(t)	((t)->tk_flags & RPC_TASK_KILLED)
#define RPC_IS_ACTIVATED(t)	((t)->tk_active)
#define RPC_DO_CALLBACK(t)	((t)->tk_callback != NULL)

#define RPC_TASK_SLEEPING	0
#define RPC_TASK_RUNNING	1
#define RPC_IS_SLEEPING(t)	(test_bit(RPC_TASK_SLEEPING, &(t)->tk_runstate))
#define RPC_IS_RUNNING(t)	(test_bit(RPC_TASK_RUNNING, &(t)->tk_runstate))

#define rpc_set_running(t)	(set_bit(RPC_TASK_RUNNING, &(t)->tk_runstate))
#define rpc_clear_running(t)	(clear_bit(RPC_TASK_RUNNING, &(t)->tk_runstate))

#define rpc_set_sleeping(t)	(set_bit(RPC_TASK_SLEEPING, &(t)->tk_runstate))

#define rpc_clear_sleeping(t) \
	do { \
		smp_mb__before_clear_bit(); \
		clear_bit(RPC_TASK_SLEEPING, &(t)->tk_runstate); \
		smp_mb__after_clear_bit(); \
	} while(0)

/*
 * RPC synchronization objects
 */
struct rpc_wait_queue {
	struct list_head	tasks;
#ifdef RPC_DEBUG
	char *			name;
#endif
};

#ifndef RPC_DEBUG
# define RPC_WAITQ_INIT(var,qname) ((struct rpc_wait_queue) {LIST_HEAD_INIT(var)})
# define RPC_WAITQ(var,qname)      struct rpc_wait_queue var = RPC_WAITQ_INIT(var.tasks,qname)
# define INIT_RPC_WAITQ(ptr,qname) do { \
	INIT_LIST_HEAD(&(ptr)->tasks); \
	} while(0)
#else
# define RPC_WAITQ_INIT(var,qname) ((struct rpc_wait_queue) {LIST_HEAD_INIT(var.tasks), qname})
# define RPC_WAITQ(var,qname)      struct rpc_wait_queue var = RPC_WAITQ_INIT(var,qname)
# define INIT_RPC_WAITQ(ptr,qname) do { \
	INIT_LIST_HEAD(&(ptr)->tasks); (ptr)->name = qname; \
	} while(0)
#endif

/*
 * Function prototypes
 */
struct rpc_task *rpc_new_task(struct rpc_clnt *, rpc_action, int flags);
struct rpc_task *rpc_new_child(struct rpc_clnt *, struct rpc_task *parent);
void		rpc_init_task(struct rpc_task *, struct rpc_clnt *,
					rpc_action exitfunc, int flags);
void		rpc_release_task(struct rpc_task *);
void		rpc_killall_tasks(struct rpc_clnt *);
int		rpc_execute(struct rpc_task *);
void		rpc_run_child(struct rpc_task *parent, struct rpc_task *child,
					rpc_action action);
int		rpc_add_wait_queue(struct rpc_wait_queue *, struct rpc_task *);
void		rpc_remove_wait_queue(struct rpc_task *);
void		rpc_sleep_on(struct rpc_wait_queue *, struct rpc_task *,
					rpc_action action, rpc_action timer);
void		rpc_add_timer(struct rpc_task *, rpc_action);
void		rpc_wake_up_task(struct rpc_task *);
void		rpc_wake_up(struct rpc_wait_queue *);
struct rpc_task *rpc_wake_up_next(struct rpc_wait_queue *);
void		rpc_wake_up_status(struct rpc_wait_queue *, int);
void		rpc_delay(struct rpc_task *, unsigned long);
void *		rpc_allocate(unsigned int flags, unsigned int);
void		rpc_free(void *);
int		rpciod_up(void);
void		rpciod_down(void);
void		rpciod_wake_up(void);
#ifdef RPC_DEBUG
void		rpc_show_tasks(void);
#endif

static __inline__ void *
rpc_malloc(struct rpc_task *task, unsigned int size)
{
	return rpc_allocate(task->tk_flags, size);
}

static __inline__ void
rpc_exit(struct rpc_task *task, int status)
{
	task->tk_status = status;
	task->tk_action = NULL;
}

#ifdef RPC_DEBUG
static __inline__ char *
rpc_qname(struct rpc_wait_queue *q)
{
	return q->name? q->name : "unknown";
}
#endif

#endif /* _LINUX_SUNRPC_SCHED_H_ */
