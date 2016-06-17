/*
 * linux/fs/lockd/svc.c
 *
 * This is the central lockd service.
 *
 * FIXME: Separate the lockd NFS server functionality from the lockd NFS
 * 	  client functionality. Oh why didn't Sun create two separate
 *	  services in the first place?
 *
 * Authors:	Olaf Kirch (okir@monad.swb.de)
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#define __KERNEL_SYSCALLS__
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/uio.h>
#include <linux/version.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>

#include <linux/sunrpc/types.h>
#include <linux/sunrpc/stats.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/svc.h>
#include <linux/sunrpc/svcsock.h>
#include <linux/lockd/lockd.h>
#include <linux/nfs.h>

#define NLMDBG_FACILITY		NLMDBG_SVC
#define LOCKD_BUFSIZE		(1024 + NLMSSVC_XDRSIZE)
#define ALLOWED_SIGS		(sigmask(SIGKILL))

extern struct svc_program	nlmsvc_program;
struct nlmsvc_binding *		nlmsvc_ops;
static DECLARE_MUTEX(nlmsvc_sema);
static unsigned int		nlmsvc_users;
static pid_t			nlmsvc_pid;
int				nlmsvc_grace_period;
unsigned long			nlmsvc_timeout;

static DECLARE_MUTEX_LOCKED(lockd_start);
static DECLARE_WAIT_QUEUE_HEAD(lockd_exit);

/*
 * Currently the following can be set only at insmod time.
 * Ideally, they would be accessible through the sysctl interface.
 */
unsigned long			nlm_grace_period;
unsigned long			nlm_timeout = LOCKD_DFLT_TIMEO;
unsigned long			nlm_udpport, nlm_tcpport;

static unsigned long set_grace_period(void)
{
	unsigned long grace_period;

	/* Note: nlm_timeout should always be nonzero */
	if (nlm_grace_period)
		grace_period = ((nlm_grace_period + nlm_timeout - 1)
				/ nlm_timeout) * nlm_timeout * HZ;
	else
		grace_period = nlm_timeout * 5 * HZ;
	nlmsvc_grace_period = 1;
	return grace_period + jiffies;
}

/*
 * This is the lockd kernel thread
 */
static void
lockd(struct svc_rqst *rqstp)
{
	struct svc_serv	*serv = rqstp->rq_server;
	int		err = 0;
	unsigned long grace_period_expire;

	/* Lock module and set up kernel thread */
	MOD_INC_USE_COUNT;
	lock_kernel();

	/*
	 * Let our maker know we're running.
	 */
	nlmsvc_pid = current->pid;
	up(&lockd_start);

	daemonize();
	reparent_to_init();
	sprintf(current->comm, "lockd");

	/* Process request with signals blocked.  */
	spin_lock_irq(&current->sigmask_lock);
	siginitsetinv(&current->blocked, sigmask(SIGKILL));
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	/* kick rpciod */
	rpciod_up();

	dprintk("NFS locking service started (ver " LOCKD_VERSION ").\n");

	if (!nlm_timeout)
		nlm_timeout = LOCKD_DFLT_TIMEO;
	nlmsvc_timeout = nlm_timeout * HZ;

	grace_period_expire = set_grace_period();

	/*
	 * The main request loop. We don't terminate until the last
	 * NFS mount or NFS daemon has gone away, and we've been sent a
	 * signal, or else another process has taken over our job.
	 */
	while ((nlmsvc_users || !signalled()) && nlmsvc_pid == current->pid)
	{
		long timeout = MAX_SCHEDULE_TIMEOUT;
		if (signalled()) {
			spin_lock_irq(&current->sigmask_lock);
			flush_signals(current);
			spin_unlock_irq(&current->sigmask_lock);
			if (nlmsvc_ops) {
				nlmsvc_ops->detach();
				grace_period_expire = set_grace_period();
			}
		}

		/*
		 * Retry any blocked locks that have been notified by
		 * the VFS. Don't do this during grace period.
		 * (Theoretically, there shouldn't even be blocked locks
		 * during grace period).
		 */
		if (!nlmsvc_grace_period)
			timeout = nlmsvc_retry_blocked();

		/*
		 * Find a socket with data available and call its
		 * recvfrom routine.
		 */
		err = svc_recv(serv, rqstp, timeout);
		if (err == -EAGAIN || err == -EINTR)
			continue;
		if (err < 0) {
			printk(KERN_WARNING
			       "lockd: terminating on error %d\n",
			       -err);
			break;
		}

		dprintk("lockd: request from %08x\n",
			(unsigned)ntohl(rqstp->rq_addr.sin_addr.s_addr));

		/*
		 * Look up the NFS client handle. The handle is needed for
		 * all but the GRANTED callback RPCs.
		 */
		rqstp->rq_client = NULL;
		if (nlmsvc_ops) {
			nlmsvc_ops->exp_readlock();
			rqstp->rq_client =
				nlmsvc_ops->exp_getclient(&rqstp->rq_addr);
		}

		if (nlmsvc_grace_period &&
		    time_before(grace_period_expire, jiffies))
			nlmsvc_grace_period = 0;
		svc_process(serv, rqstp);

		/* Unlock export hash tables */
		if (nlmsvc_ops)
			nlmsvc_ops->exp_unlock();
	}

	/*
	 * Check whether there's a new lockd process before
	 * shutting down the hosts and clearing the slot.
	 */
	if (!nlmsvc_pid || current->pid == nlmsvc_pid) {
		if (nlmsvc_ops)
			nlmsvc_ops->detach();
		nlm_shutdown_hosts();
		nlmsvc_pid = 0;
	} else
		printk(KERN_DEBUG
			"lockd: new process, skipping host shutdown\n");
	wake_up(&lockd_exit);
		
	/* Exit the RPC thread */
	svc_exit_thread(rqstp);

	/* release rpciod */
	rpciod_down();

	/* Release module */
	MOD_DEC_USE_COUNT;
}

/*
 * Bring up the lockd process if it's not already up.
 */
int
lockd_up(void)
{
	static int		warned = 0; 
	struct svc_serv *	serv;
	int			error = 0;

	down(&nlmsvc_sema);
	/*
	 * Unconditionally increment the user count ... this is
	 * the number of clients who _want_ a lockd process.
	 */
	nlmsvc_users++; 
	/*
	 * Check whether we're already up and running.
	 */
	if (nlmsvc_pid)
		goto out;

	/*
	 * Sanity check: if there's no pid,
	 * we should be the first user ...
	 */
	if (nlmsvc_users > 1)
		printk(KERN_WARNING
			"lockd_up: no pid, %d users??\n", nlmsvc_users);

	error = -ENOMEM;
	serv = svc_create(&nlmsvc_program, 0, NLMSVC_XDRSIZE);
	if (!serv) {
		printk(KERN_WARNING "lockd_up: create service failed\n");
		goto out;
	}

	if ((error = svc_makesock(serv, IPPROTO_UDP, nlm_udpport)) < 0 
#ifdef CONFIG_NFSD_TCP
	 || (error = svc_makesock(serv, IPPROTO_TCP, nlm_tcpport)) < 0
#endif
		) {
		if (warned++ == 0) 
			printk(KERN_WARNING
				"lockd_up: makesock failed, error=%d\n", error);
		goto destroy_and_out;
	} 
	warned = 0;

	/*
	 * Create the kernel thread and wait for it to start.
	 */
	error = svc_create_thread(lockd, serv);
	if (error) {
		printk(KERN_WARNING
			"lockd_up: create thread failed, error=%d\n", error);
		goto destroy_and_out;
	}
	down(&lockd_start);

	/*
	 * Note: svc_serv structures have an initial use count of 1,
	 * so we exit through here on both success and failure.
	 */
destroy_and_out:
	svc_destroy(serv);
out:
	up(&nlmsvc_sema);
	return error;
}

/*
 * Decrement the user count and bring down lockd if we're the last.
 */
void
lockd_down(void)
{
	static int warned = 0; 

	down(&nlmsvc_sema);
	if (nlmsvc_users) {
		if (--nlmsvc_users)
			goto out;
	} else
		printk(KERN_WARNING "lockd_down: no users! pid=%d\n", nlmsvc_pid);

	if (!nlmsvc_pid) {
		if (warned++ == 0)
			printk(KERN_WARNING "lockd_down: no lockd running.\n"); 
		goto out;
	}
	warned = 0;

	kill_proc(nlmsvc_pid, SIGKILL, 1);
	/*
	 * Wait for the lockd process to exit, but since we're holding
	 * the lockd semaphore, we can't wait around forever ...
	 */
	current->sigpending = 0;
	interruptible_sleep_on_timeout(&lockd_exit, HZ);
	if (nlmsvc_pid) {
		printk(KERN_WARNING 
			"lockd_down: lockd failed to exit, clearing pid\n");
		nlmsvc_pid = 0;
	}
	spin_lock_irq(&current->sigmask_lock);
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);
out:
	up(&nlmsvc_sema);
}

#ifdef MODULE
/* New module support in 2.1.18 */

MODULE_AUTHOR("Olaf Kirch <okir@monad.swb.de>");
MODULE_DESCRIPTION("NFS file locking service version " LOCKD_VERSION ".");
MODULE_LICENSE("GPL");
MODULE_PARM(nlm_grace_period, "10-240l");
MODULE_PARM(nlm_timeout, "3-20l");
MODULE_PARM(nlm_udpport, "0-65535l");
MODULE_PARM(nlm_tcpport, "0-65535l");

int
init_module(void)
{
	/* Init the static variables */
	init_MUTEX(&nlmsvc_sema);
	nlmsvc_users = 0;
	nlmsvc_pid = 0;
	return 0;
}

void
cleanup_module(void)
{
	/* FIXME: delete all NLM clients */
	nlm_shutdown_hosts();
}
#else
/* not a module, so process bootargs
 * lockd.udpport and lockd.tcpport
 */

static int __init udpport_set(char *str)
{
	nlm_udpport = simple_strtoul(str, NULL, 0);
	return 1;
}
static int __init tcpport_set(char *str)
{
	nlm_tcpport = simple_strtoul(str, NULL, 0);
	return 1;
}
__setup("lockd.udpport=", udpport_set);
__setup("lockd.tcpport=", tcpport_set);

#endif

/*
 * Define NLM program and procedures
 */
static struct svc_version	nlmsvc_version1 = {
	1, 17, nlmsvc_procedures, NULL
};
static struct svc_version	nlmsvc_version3 = {
	3, 24, nlmsvc_procedures, NULL
};
#ifdef CONFIG_LOCKD_V4
static struct svc_version	nlmsvc_version4 = {
	4, 24, nlmsvc_procedures4, NULL
};
#endif
static struct svc_version *	nlmsvc_version[] = {
	NULL,
	&nlmsvc_version1,
	NULL,
	&nlmsvc_version3,
#ifdef CONFIG_LOCKD_V4
	&nlmsvc_version4,
#endif
};

static struct svc_stat		nlmsvc_stats;

#define NLM_NRVERS	(sizeof(nlmsvc_version)/sizeof(nlmsvc_version[0]))
struct svc_program		nlmsvc_program = {
	NLM_PROGRAM,		/* program number */
	1, NLM_NRVERS-1,	/* version range */
	NLM_NRVERS,		/* number of entries in nlmsvc_version */
	nlmsvc_version,		/* version table */
	"lockd",		/* service name */
	&nlmsvc_stats,		/* stats table */
};
