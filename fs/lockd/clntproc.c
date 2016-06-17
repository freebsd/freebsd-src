/*
 * linux/fs/lockd/clntproc.c
 *
 * RPC procedures for the client side NLM implementation
 *
 * Copyright (C) 1996, Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/nfs_fs.h>
#include <linux/utsname.h>
#include <linux/smp_lock.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/svc.h>
#include <linux/lockd/lockd.h>
#include <linux/lockd/sm_inter.h>

#define NLMDBG_FACILITY		NLMDBG_CLIENT
#define NLMCLNT_GRACE_WAIT	(5*HZ)

static int	nlmclnt_test(struct nlm_rqst *, struct file_lock *);
static int	nlmclnt_lock(struct nlm_rqst *, struct file_lock *);
static int	nlmclnt_unlock(struct nlm_rqst *, struct file_lock *);
static void	nlmclnt_unlock_callback(struct rpc_task *);
static void	nlmclnt_cancel_callback(struct rpc_task *);
static int	nlm_stat_to_errno(u32 stat);

/*
 * Cookie counter for NLM requests
 */
static u32	nlm_cookie = 0x1234;

static inline void nlmclnt_next_cookie(struct nlm_cookie *c)
{
	memcpy(c->data, &nlm_cookie, 4);
	memset(c->data+4, 0, 4);
	c->len=4;
	nlm_cookie++;
}

/*
 * Initialize arguments for TEST/LOCK/UNLOCK/CANCEL calls
 */
static inline void
nlmclnt_setlockargs(struct nlm_rqst *req, struct file_lock *fl)
{
	struct nlm_args	*argp = &req->a_args;
	struct nlm_lock	*lock = &argp->lock;

	nlmclnt_next_cookie(&argp->cookie);
	argp->state   = nsm_local_state;
	memcpy(&lock->fh, NFS_FH(fl->fl_file->f_dentry->d_inode), sizeof(struct nfs_fh));
	lock->caller  = system_utsname.nodename;
	lock->oh.data = req->a_owner;
	lock->oh.len  = sprintf(req->a_owner, "%d@%s",
				current->pid, system_utsname.nodename);
	locks_copy_lock(&lock->fl, fl);
}

/*
 * Initialize arguments for GRANTED call. The nlm_rqst structure
 * has been cleared already.
 */
int
nlmclnt_setgrantargs(struct nlm_rqst *call, struct nlm_lock *lock)
{
	locks_copy_lock(&call->a_args.lock.fl, &lock->fl);
	memcpy(&call->a_args.lock.fh, &lock->fh, sizeof(call->a_args.lock.fh));
	call->a_args.lock.caller = system_utsname.nodename;
	call->a_args.lock.oh.len = lock->oh.len;

	/* set default data area */
	call->a_args.lock.oh.data = call->a_owner;

	if (lock->oh.len > NLMCLNT_OHSIZE) {
		void *data = kmalloc(lock->oh.len, GFP_KERNEL);
		if (!data)
			return 0;
		call->a_args.lock.oh.data = (u8 *) data;
	}

	memcpy(call->a_args.lock.oh.data, lock->oh.data, lock->oh.len);
	return 1;
}

void
nlmclnt_freegrantargs(struct nlm_rqst *call)
{
	/*
	 * Check whether we allocated memory for the owner.
	 */
	if (call->a_args.lock.oh.data != (u8 *) call->a_owner) {
		kfree(call->a_args.lock.oh.data);
	}
}

/*
 * This is the main entry point for the NLM client.
 */
int
nlmclnt_proc(struct inode *inode, int cmd, struct file_lock *fl)
{
	struct nfs_server	*nfssrv = NFS_SERVER(inode);
	struct nlm_host		*host;
	struct nlm_rqst		reqst, *call = &reqst;
	sigset_t		oldset;
	unsigned long		flags;
	int			status, proto, vers;

	vers = (NFS_PROTO(inode)->version == 3) ? 4 : 1;
	if (NFS_PROTO(inode)->version > 3) {
		printk(KERN_NOTICE "NFSv4 file locking not implemented!\n");
		return -ENOLCK;
	}

	/* Retrieve transport protocol from NFS client */
	proto = NFS_CLIENT(inode)->cl_xprt->prot;

	if (!(host = nlmclnt_lookup_host(NFS_ADDR(inode), proto, vers)))
		return -ENOLCK;

	/* Create RPC client handle if not there, and copy soft
	 * and intr flags from NFS client. */
	if (host->h_rpcclnt == NULL) {
		struct rpc_clnt	*clnt;

		/* Bind an rpc client to this host handle (does not
		 * perform a portmapper lookup) */
		if (!(clnt = nlm_bind_host(host))) {
			status = -ENOLCK;
			goto done;
		}
		clnt->cl_softrtry = nfssrv->client->cl_softrtry;
		clnt->cl_intr     = nfssrv->client->cl_intr;
		clnt->cl_chatty   = nfssrv->client->cl_chatty;
	}

	/* Keep the old signal mask */
	spin_lock_irqsave(&current->sigmask_lock, flags);
	oldset = current->blocked;

	/* If we're cleaning up locks because the process is exiting,
	 * perform the RPC call asynchronously. */
	if ((IS_SETLK(cmd) || IS_SETLKW(cmd))
	    && fl->fl_type == F_UNLCK
	    && (current->flags & PF_EXITING)) {
		sigfillset(&current->blocked);	/* Mask all signals */
		recalc_sigpending(current);
		spin_unlock_irqrestore(&current->sigmask_lock, flags);

		call = nlmclnt_alloc_call();
		if (!call) {
			status = -ENOMEM;
			goto out_restore;
		}
		call->a_flags = RPC_TASK_ASYNC;
	} else {
		spin_unlock_irqrestore(&current->sigmask_lock, flags);
		memset(call, 0, sizeof(*call));
		locks_init_lock(&call->a_args.lock.fl);
		locks_init_lock(&call->a_res.lock.fl);
	}
	call->a_host = host;

	/* Set up the argument struct */
	nlmclnt_setlockargs(call, fl);

	if (IS_SETLK(cmd) || IS_SETLKW(cmd)) {
		if (fl->fl_type != F_UNLCK) {
			call->a_args.block = IS_SETLKW(cmd) ? 1 : 0;
			status = nlmclnt_lock(call, fl);
		} else
			status = nlmclnt_unlock(call, fl);
	} else if (IS_GETLK(cmd))
		status = nlmclnt_test(call, fl);
	else
		status = -EINVAL;

	if (status < 0 && (call->a_flags & RPC_TASK_ASYNC))
		kfree(call);

 out_restore:
	spin_lock_irqsave(&current->sigmask_lock, flags);
	current->blocked = oldset;
	recalc_sigpending(current);
	spin_unlock_irqrestore(&current->sigmask_lock, flags);

done:
	dprintk("lockd: clnt proc returns %d\n", status);
	nlm_release_host(host);
	return status;
}

/*
 * Wait while server is in grace period
 */
static inline int
nlmclnt_grace_wait(struct nlm_host *host)
{
	if (!host->h_reclaiming)
		interruptible_sleep_on_timeout(&host->h_gracewait, 10*HZ);
	else
		interruptible_sleep_on(&host->h_gracewait);
	return signalled()? -ERESTARTSYS : 0;
}

/*
 * Allocate an NLM RPC call struct
 */
struct nlm_rqst *
nlmclnt_alloc_call(void)
{
	struct nlm_rqst	*call;

	while (!signalled()) {
		call = (struct nlm_rqst *) kmalloc(sizeof(struct nlm_rqst), GFP_KERNEL);
		if (call) {
			memset(call, 0, sizeof(*call));
			locks_init_lock(&call->a_args.lock.fl);
			locks_init_lock(&call->a_res.lock.fl);
			return call;
		}
		printk("nlmclnt_alloc_call: failed, waiting for memory\n");
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(5*HZ);
	}
	return NULL;
}

/*
 * Generic NLM call
 */
int
nlmclnt_call(struct nlm_rqst *req, u32 proc)
{
	struct nlm_host	*host = req->a_host;
	struct rpc_clnt	*clnt;
	struct nlm_args	*argp = &req->a_args;
	struct nlm_res	*resp = &req->a_res;
	struct file	*filp = argp->lock.fl.fl_file;
	struct rpc_message msg;
	int		status;

	dprintk("lockd: call procedure %s on %s\n",
			nlm_procname(proc), host->h_name);

	msg.rpc_proc = proc;
	msg.rpc_argp = argp;
	msg.rpc_resp = resp;
	if (filp)
		msg.rpc_cred = nfs_file_cred(filp);
	else
		msg.rpc_cred = NULL;

	do {
		if (host->h_reclaiming && !argp->reclaim) {
			interruptible_sleep_on(&host->h_gracewait);
			continue;
		}

		/* If we have no RPC client yet, create one. */
		if ((clnt = nlm_bind_host(host)) == NULL)
			return -ENOLCK;

		/* Perform the RPC call. If an error occurs, try again */
		if ((status = rpc_call_sync(clnt, &msg, 0)) < 0) {
			dprintk("lockd: rpc_call returned error %d\n", -status);
			switch (status) {
			case -EPROTONOSUPPORT:
				status = -EINVAL;
				break;
			case -ECONNREFUSED:
			case -ETIMEDOUT:
			case -ENOTCONN:
				nlm_rebind_host(host);
				status = -EAGAIN;
				break;
			case -ERESTARTSYS:
				return signalled () ? -EINTR : status;
			default:
				break;
			}
			break;
		} else
		if (resp->status == NLM_LCK_DENIED_GRACE_PERIOD) {
			dprintk("lockd: server in grace period\n");
			if (argp->reclaim) {
				printk(KERN_WARNING
				     "lockd: spurious grace period reject?!\n");
				return -ENOLCK;
			}
		} else {
			dprintk("lockd: server returns status %d\n", resp->status);
			return 0;	/* Okay, call complete */
		}

		/* Back off a little and try again */
		interruptible_sleep_on_timeout(&host->h_gracewait, 15*HZ);

		/* When the lock requested by F_SETLKW isn't available,
		   we will wait until the request can be satisfied. If
		   a signal is received during wait, we should return
		   -EINTR. */
		if (signalled ()) {
			status = -EINTR;
			break;
		}
	} while (1);

	return status;
}

/*
 * Generic NLM call, async version.
 */
int
nlmsvc_async_call(struct nlm_rqst *req, u32 proc, rpc_action callback)
{
	struct nlm_host	*host = req->a_host;
	struct rpc_clnt	*clnt;
	struct nlm_args	*argp = &req->a_args;
	struct nlm_res	*resp = &req->a_res;
	struct rpc_message msg;
	int		status;

	dprintk("lockd: call procedure %s on %s (async)\n",
			nlm_procname(proc), host->h_name);

	/* If we have no RPC client yet, create one. */
	if ((clnt = nlm_bind_host(host)) == NULL)
		return -ENOLCK;

        /* bootstrap and kick off the async RPC call */
	msg.rpc_proc = proc;
	msg.rpc_argp = argp;
	msg.rpc_resp =resp;
	msg.rpc_cred = NULL;	
        status = rpc_call_async(clnt, &msg, RPC_TASK_ASYNC, callback, req);

	return status;
}

int
nlmclnt_async_call(struct nlm_rqst *req, u32 proc, rpc_action callback)
{
	struct nlm_host	*host = req->a_host;
	struct rpc_clnt	*clnt;
	struct nlm_args	*argp = &req->a_args;
	struct nlm_res	*resp = &req->a_res;
	struct file	*file = argp->lock.fl.fl_file;
	struct rpc_message msg;
	int		status;

	dprintk("lockd: call procedure %s on %s (async)\n",
			nlm_procname(proc), host->h_name);

	/* If we have no RPC client yet, create one. */
	if ((clnt = nlm_bind_host(host)) == NULL)
		return -ENOLCK;

        /* bootstrap and kick off the async RPC call */
	msg.rpc_proc = proc;
	msg.rpc_argp = argp;
	msg.rpc_resp =resp;
	if (file)
		msg.rpc_cred = nfs_file_cred(file);
	else
		msg.rpc_cred = NULL;
	/* Increment host refcount */
	nlm_get_host(host);
        status = rpc_call_async(clnt, &msg, RPC_TASK_ASYNC, callback, req);
	if (status < 0)
		nlm_release_host(host);
	return status;
}

/*
 * TEST for the presence of a conflicting lock
 */
static int
nlmclnt_test(struct nlm_rqst *req, struct file_lock *fl)
{
	int	status;

	if ((status = nlmclnt_call(req, NLMPROC_TEST)) < 0)
		return status;

	status = req->a_res.status;
	if (status == NLM_LCK_GRANTED) {
		fl->fl_type = F_UNLCK;
	} if (status == NLM_LCK_DENIED) {
		/*
		 * Report the conflicting lock back to the application.
		 * FIXME: Is it OK to report the pid back as well?
		 */
		locks_copy_lock(fl, &req->a_res.lock.fl);
		/* fl->fl_pid = 0; */
	} else {
		return nlm_stat_to_errno(req->a_res.status);
	}

	return 0;
}

static
void nlmclnt_insert_lock_callback(struct file_lock *fl)
{
	nlm_get_host(fl->fl_u.nfs_fl.host);
}
static
void nlmclnt_remove_lock_callback(struct file_lock *fl)
{
	if (fl->fl_u.nfs_fl.host) {
		nlm_release_host(fl->fl_u.nfs_fl.host);
		fl->fl_u.nfs_fl.host = NULL;
	}
}

/*
 * LOCK: Try to create a lock
 *
 *			Programmer Harassment Alert
 *
 * When given a blocking lock request in a sync RPC call, the HPUX lockd
 * will faithfully return LCK_BLOCKED but never cares to notify us when
 * the lock could be granted. This way, our local process could hang
 * around forever waiting for the callback.
 *
 *  Solution A:	Implement busy-waiting
 *  Solution B: Use the async version of the call (NLM_LOCK_{MSG,RES})
 *
 * For now I am implementing solution A, because I hate the idea of
 * re-implementing lockd for a third time in two months. The async
 * calls shouldn't be too hard to do, however.
 *
 * This is one of the lovely things about standards in the NFS area:
 * they're so soft and squishy you can't really blame HP for doing this.
 */
static int
nlmclnt_lock(struct nlm_rqst *req, struct file_lock *fl)
{
	struct nlm_host	*host = req->a_host;
	struct nlm_res	*resp = &req->a_res;
	int		status;

	if (!host->h_monitored && nsm_monitor(host) < 0) {
		printk(KERN_NOTICE "lockd: failed to monitor %s\n",
					host->h_name);
		return -ENOLCK;
	}

	do {
		if ((status = nlmclnt_call(req, NLMPROC_LOCK)) >= 0) {
			if (resp->status != NLM_LCK_BLOCKED)
				break;
			status = nlmclnt_block(host, fl, &resp->status);
		}
		if (status < 0)
			return status;
	} while (resp->status == NLM_LCK_BLOCKED && req->a_args.block);

	if (resp->status == NLM_LCK_GRANTED) {
		fl->fl_u.nfs_fl.state = host->h_state;
		fl->fl_u.nfs_fl.flags |= NFS_LCK_GRANTED;
		fl->fl_u.nfs_fl.host = host;
		fl->fl_insert = nlmclnt_insert_lock_callback;
		fl->fl_remove = nlmclnt_remove_lock_callback;
	}

	return nlm_stat_to_errno(resp->status);
}

/*
 * RECLAIM: Try to reclaim a lock
 */
int
nlmclnt_reclaim(struct nlm_host *host, struct file_lock *fl)
{
	struct nlm_rqst reqst, *req;
	int		status;

	req = &reqst;
	memset(req, 0, sizeof(*req));
	locks_init_lock(&req->a_args.lock.fl);
	locks_init_lock(&req->a_res.lock.fl);
	req->a_host  = host;
	req->a_flags = 0;

	/* Set up the argument struct */
	nlmclnt_setlockargs(req, fl);
	req->a_args.reclaim = 1;

	if ((status = nlmclnt_call(req, NLMPROC_LOCK)) >= 0
	 && req->a_res.status == NLM_LCK_GRANTED)
		return 0;

	printk(KERN_WARNING "lockd: failed to reclaim lock for pid %d "
				"(errno %d, status %d)\n", fl->fl_pid,
				status, req->a_res.status);

	/*
	 * FIXME: This is a serious failure. We can
	 *
	 *  a.	Ignore the problem
	 *  b.	Send the owning process some signal (Linux doesn't have
	 *	SIGLOST, though...)
	 *  c.	Retry the operation
	 *
	 * Until someone comes up with a simple implementation
	 * for b or c, I'll choose option a.
	 */

	return -ENOLCK;
}

/*
 * UNLOCK: remove an existing lock
 */
static int
nlmclnt_unlock(struct nlm_rqst *req, struct file_lock *fl)
{
	struct nlm_res	*resp = &req->a_res;
	int		status;

	/* Clean the GRANTED flag now so the lock doesn't get
	 * reclaimed while we're stuck in the unlock call. */
	fl->fl_u.nfs_fl.flags &= ~NFS_LCK_GRANTED;

	if (req->a_flags & RPC_TASK_ASYNC) {
		return nlmclnt_async_call(req, NLMPROC_UNLOCK,
					nlmclnt_unlock_callback);
	}

	if ((status = nlmclnt_call(req, NLMPROC_UNLOCK)) < 0)
		return status;

	if (resp->status == NLM_LCK_GRANTED)
		return 0;

	if (resp->status != NLM_LCK_DENIED_NOLOCKS)
		printk("lockd: unexpected unlock status: %d\n", resp->status);

	/* What to do now? I'm out of my depth... */

	return -ENOLCK;
}

static void
nlmclnt_unlock_callback(struct rpc_task *task)
{
	struct nlm_rqst	*req = (struct nlm_rqst *) task->tk_calldata;
	int		status = req->a_res.status;

	if (RPC_ASSASSINATED(task))
		goto die;

	if (task->tk_status < 0) {
		dprintk("lockd: unlock failed (err = %d)\n", -task->tk_status);
		goto retry_rebind;
	}
	if (status == NLM_LCK_DENIED_GRACE_PERIOD) {
		rpc_delay(task, NLMCLNT_GRACE_WAIT);
		goto retry_unlock;
	}
	if (status != NLM_LCK_GRANTED)
		printk(KERN_WARNING "lockd: unexpected unlock status: %d\n", status);

die:
	nlm_release_host(req->a_host);
	kfree(req);
	return;
 retry_rebind:
	nlm_rebind_host(req->a_host);
 retry_unlock:
	rpc_restart_call(task);
}

/*
 * Cancel a blocked lock request.
 * We always use an async RPC call for this in order not to hang a
 * process that has been Ctrl-C'ed.
 */
int
nlmclnt_cancel(struct nlm_host *host, struct file_lock *fl)
{
	struct nlm_rqst	*req;
	unsigned long	flags;
	sigset_t	oldset;
	int		status;

	/* Block all signals while setting up call */
	spin_lock_irqsave(&current->sigmask_lock, flags);
	oldset = current->blocked;
	sigfillset(&current->blocked);
	recalc_sigpending(current);
	spin_unlock_irqrestore(&current->sigmask_lock, flags);

	req = nlmclnt_alloc_call();
	if (!req)
		return -ENOMEM;
	req->a_host  = host;
	req->a_flags = RPC_TASK_ASYNC;

	nlmclnt_setlockargs(req, fl);

	status = nlmclnt_async_call(req, NLMPROC_CANCEL,
					nlmclnt_cancel_callback);
	if (status < 0)
		kfree(req);

	spin_lock_irqsave(&current->sigmask_lock, flags);
	current->blocked = oldset;
	recalc_sigpending(current);
	spin_unlock_irqrestore(&current->sigmask_lock, flags);

	return status;
}

static void
nlmclnt_cancel_callback(struct rpc_task *task)
{
	struct nlm_rqst	*req = (struct nlm_rqst *) task->tk_calldata;

	if (RPC_ASSASSINATED(task))
		goto die;

	if (task->tk_status < 0) {
		dprintk("lockd: CANCEL call error %d, retrying.\n",
					task->tk_status);
		goto retry_cancel;
	}

	dprintk("lockd: cancel status %d (task %d)\n",
			req->a_res.status, task->tk_pid);

	switch (req->a_res.status) {
	case NLM_LCK_GRANTED:
	case NLM_LCK_DENIED_GRACE_PERIOD:
		/* Everything's good */
		break;
	case NLM_LCK_DENIED_NOLOCKS:
		dprintk("lockd: CANCEL failed (server has no locks)\n");
		goto retry_cancel;
	default:
		printk(KERN_NOTICE "lockd: weird return %d for CANCEL call\n",
			req->a_res.status);
	}

die:
	nlm_release_host(req->a_host);
	kfree(req);
	return;

retry_cancel:
	nlm_rebind_host(req->a_host);
	rpc_restart_call(task);
	rpc_delay(task, 30 * HZ);
}

/*
 * Convert an NLM status code to a generic kernel errno
 */
static int
nlm_stat_to_errno(u32 status)
{
	switch(status) {
	case NLM_LCK_GRANTED:
		return 0;
	case NLM_LCK_DENIED:
		return -EAGAIN;
	case NLM_LCK_DENIED_NOLOCKS:
	case NLM_LCK_DENIED_GRACE_PERIOD:
		return -ENOLCK;
	case NLM_LCK_BLOCKED:
		printk(KERN_NOTICE "lockd: unexpected status NLM_BLOCKED\n");
		return -ENOLCK;
#ifdef CONFIG_LOCKD_V4
	case NLM_DEADLCK:
		return -EDEADLK;
	case NLM_ROFS:
		return -EROFS;
	case NLM_STALE_FH:
		return -ESTALE;
	case NLM_FBIG:
		return -EOVERFLOW;
	case NLM_FAILED:
		return -ENOLCK;
#endif
	}
	printk(KERN_NOTICE "lockd: unexpected server status %d\n", status);
	return -ENOLCK;
}
