/*
 * linux/fs/nfsd/nfssvc.c
 *
 * Central processing for nfsd.
 *
 * Authors:	Olaf Kirch (okir@monad.swb.de)
 *
 * Copyright (C) 1995, 1996, 1997 Olaf Kirch <okir@monad.swb.de>
 */

#define __NO_VERSION__
#include <linux/config.h>
#include <linux/module.h>

#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/nfs.h>
#include <linux/in.h>
#include <linux/uio.h>
#include <linux/version.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>

#include <linux/sunrpc/types.h>
#include <linux/sunrpc/stats.h>
#include <linux/sunrpc/svc.h>
#include <linux/sunrpc/svcsock.h>
#include <linux/nfsd/nfsd.h>
#include <linux/nfsd/stats.h>
#include <linux/nfsd/cache.h>
#include <linux/nfsd/xdr.h>
#include <linux/lockd/bind.h>

#define NFSDDBG_FACILITY	NFSDDBG_SVC
#define NFSD_BUFSIZE		(1024 + NFSSVC_MAXBLKSIZE)

/* these signals will be delivered to an nfsd thread 
 * when handling a request
 */
#define ALLOWED_SIGS	(sigmask(SIGKILL))
/* these signals will be delivered to an nfsd thread
 * when not handling a request. i.e. when waiting
 */
#define SHUTDOWN_SIGS	(sigmask(SIGKILL) | sigmask(SIGHUP) | sigmask(SIGINT) | sigmask(SIGQUIT))
/* if the last thread dies with SIGHUP, then the exports table is
 * left unchanged ( like 2.4-{0-9} ).  Any other signal will clear
 * the exports table (like 2.2).
 */
#define	SIG_NOCLEAN	SIGHUP

extern struct svc_program	nfsd_program;
static void			nfsd(struct svc_rqst *rqstp);
struct timeval			nfssvc_boot;
static struct svc_serv 		*nfsd_serv;
static int			nfsd_busy;
static unsigned long		nfsd_last_call;

struct nfsd_list {
	struct list_head 	list;
	struct task_struct	*task;
};
struct list_head nfsd_list = LIST_HEAD_INIT(nfsd_list);

/*
 * Maximum number of nfsd processes
 */
#define	NFSD_MAXSERVS		8192

int
nfsd_svc(unsigned short port, int nrservs)
{
	int	error;
	int	none_left;	
	struct list_head *victim;

	dprintk("nfsd: creating service\n");
	error = -EINVAL;
	if (nrservs <= 0)
		nrservs = 0;
	if (nrservs > NFSD_MAXSERVS)
		nrservs = NFSD_MAXSERVS;
	
	/* Readahead param cache - will no-op if it already exists */
	error =	nfsd_racache_init(2*nrservs);
	if (error<0)
		goto out;
	if (!nfsd_serv) {
		error = -ENOMEM;
		nfsd_serv = svc_create(&nfsd_program, NFSD_BUFSIZE, NFSSVC_XDRSIZE);
		if (nfsd_serv == NULL)
			goto out;
		error = svc_makesock(nfsd_serv, IPPROTO_UDP, port);
		if (error < 0)
			goto failure;

#if CONFIG_NFSD_TCP
		error = svc_makesock(nfsd_serv, IPPROTO_TCP, port);
		if (error < 0)
			goto failure;
#endif
		do_gettimeofday(&nfssvc_boot);		/* record boot time */
	} else
		nfsd_serv->sv_nrthreads++;
	nrservs -= (nfsd_serv->sv_nrthreads-1);
	while (nrservs > 0) {
		nrservs--;
		error = svc_create_thread(nfsd, nfsd_serv);
		if (error < 0)
			break;
	}
	victim = nfsd_list.next;
	while (nrservs < 0 && victim != &nfsd_list) {
		struct nfsd_list *nl =
			list_entry(victim,struct nfsd_list, list);
		victim = victim->next;
		send_sig(SIG_NOCLEAN, nl->task, 1);
		nrservs++;
	}
 failure:
	none_left = (nfsd_serv->sv_nrthreads == 1);
	svc_destroy(nfsd_serv);		/* Release server */
	if (none_left) {
		nfsd_serv = NULL;
		nfsd_racache_shutdown();
	}
 out:
	return error;
}

static inline void
update_thread_usage(int busy_threads)
{
	unsigned long prev_call;
	unsigned long diff;
	int decile;

	prev_call = nfsd_last_call;
	nfsd_last_call = jiffies;
	decile = busy_threads*10/nfsdstats.th_cnt;
	if (decile>0 && decile <= 10) {
		diff = nfsd_last_call - prev_call;
		if ( (nfsdstats.th_usage[decile-1] += diff) >= NFSD_USAGE_WRAP)
			nfsdstats.th_usage[decile-1] -= NFSD_USAGE_WRAP;
		if (decile == 10)
			nfsdstats.th_fullcnt++;
	}
}

/*
 * This is the NFS server kernel thread
 */
static void
nfsd(struct svc_rqst *rqstp)
{
	struct svc_serv	*serv = rqstp->rq_server;
	int		err;
	struct nfsd_list me;

	/* Lock module and set up kernel thread */
	MOD_INC_USE_COUNT;
	lock_kernel();
	daemonize();
	sprintf(current->comm, "nfsd");
	current->rlim[RLIMIT_FSIZE].rlim_cur = RLIM_INFINITY;

	nfsdstats.th_cnt++;
	/* Let svc_process check client's authentication. */
	rqstp->rq_auth = 1;

	lockd_up();				/* start lockd */

	me.task = current;
	list_add(&me.list, &nfsd_list);

	/*
	 * The main request loop
	 */
	for (;;) {
		/* Block all but the shutdown signals */
		spin_lock_irq(&current->sigmask_lock);
		siginitsetinv(&current->blocked, SHUTDOWN_SIGS);
		recalc_sigpending(current);
		spin_unlock_irq(&current->sigmask_lock);

		/*
		 * Find a socket with data available and call its
		 * recvfrom routine.
		 */
		while ((err = svc_recv(serv, rqstp,
				       5*60*HZ)) == -EAGAIN)
		    ;
		if (err < 0)
			break;
		update_thread_usage(nfsd_busy);
		nfsd_busy++;

		/* Lock the export hash tables for reading. */
		exp_readlock();

		/* Validate the client's address. This will also defeat
		 * port probes on port 2049 by unauthorized clients.
		 */
		rqstp->rq_client = exp_getclient(&rqstp->rq_addr);
		/* Process request with signals blocked.  */
		spin_lock_irq(&current->sigmask_lock);
		siginitsetinv(&current->blocked, ALLOWED_SIGS);
		recalc_sigpending(current);
		spin_unlock_irq(&current->sigmask_lock);

		svc_process(serv, rqstp);

		/* Unlock export hash tables */
		exp_unlock();
		update_thread_usage(nfsd_busy);
		nfsd_busy--;
	}

	if (err != -EINTR) {
		printk(KERN_WARNING "nfsd: terminating on error %d\n", -err);
	} else {
		unsigned int	signo;

		for (signo = 1; signo <= _NSIG; signo++)
			if (sigismember(&current->pending.signal, signo) &&
			    !sigismember(&current->blocked, signo))
				break;
		err = signo;
	}

	/* Release lockd */
	lockd_down();

	/* Check if this is last thread */
	if (serv->sv_nrthreads==1) {
		
		printk(KERN_WARNING "nfsd: last server has exited\n");
		if (err != SIG_NOCLEAN) {
			printk(KERN_WARNING "nfsd: unexporting all filesystems\n");
			nfsd_export_shutdown();
		}
		nfsd_serv = NULL;
	        nfsd_racache_shutdown();	/* release read-ahead cache */
	}
	list_del(&me.list);
	nfsdstats.th_cnt --;

	/* Release the thread */
	svc_exit_thread(rqstp);

	/* Release module */
	MOD_DEC_USE_COUNT;
}

static int
nfsd_dispatch(struct svc_rqst *rqstp, u32 *statp)
{
	struct svc_procedure	*proc;
	kxdrproc_t		xdr;
	u32			nfserr;

	dprintk("nfsd_dispatch: vers %d proc %d\n",
				rqstp->rq_vers, rqstp->rq_proc);
	proc = rqstp->rq_procinfo;

	/* Check whether we have this call in the cache. */
	switch (nfsd_cache_lookup(rqstp, proc->pc_cachetype)) {
	case RC_INTR:
	case RC_DROPIT:
		return 0;
	case RC_REPLY:
		return 1;
	case RC_DOIT:;
		/* do it */
	}

	/* Decode arguments */
	xdr = proc->pc_decode;
	if (xdr && !xdr(rqstp, rqstp->rq_argbuf.buf, rqstp->rq_argp)) {
		dprintk("nfsd: failed to decode arguments!\n");
		nfsd_cache_update(rqstp, RC_NOCACHE, NULL);
		*statp = rpc_garbage_args;
		return 1;
	}

	/* Now call the procedure handler, and encode NFS status. */
	nfserr = proc->pc_func(rqstp, rqstp->rq_argp, rqstp->rq_resp);
	if (nfserr == nfserr_dropit) {
		dprintk("nfsd: Dropping request due to malloc failure!\n");
		nfsd_cache_update(rqstp, RC_NOCACHE, NULL);
		return 0;
	}
		
	if (rqstp->rq_proc != 0)
		svc_putlong(&rqstp->rq_resbuf, nfserr);

	/* Encode result.
	 * For NFSv2, additional info is never returned in case of an error.
	 */
	if (!(nfserr && rqstp->rq_vers == 2)) {
		xdr = proc->pc_encode;
		if (xdr && !xdr(rqstp, rqstp->rq_resbuf.buf, rqstp->rq_resp)) {
			/* Failed to encode result. Release cache entry */
			dprintk("nfsd: failed to encode result!\n");
			nfsd_cache_update(rqstp, RC_NOCACHE, NULL);
			*statp = rpc_system_err;
			return 1;
		}
	}

	/* Store reply in cache. */
	nfsd_cache_update(rqstp, proc->pc_cachetype, statp + 1);
	return 1;
}

static struct svc_version	nfsd_version2 = {
	2, 18, nfsd_procedures2, nfsd_dispatch
};
#ifdef CONFIG_NFSD_V3
static struct svc_version	nfsd_version3 = {
	3, 22, nfsd_procedures3, nfsd_dispatch
};
#endif
static struct svc_version *	nfsd_version[] = {
	NULL,
	NULL,
	&nfsd_version2,
#ifdef CONFIG_NFSD_V3
	&nfsd_version3,
#endif
};

#define NFSD_NRVERS		(sizeof(nfsd_version)/sizeof(nfsd_version[0]))
struct svc_program		nfsd_program = {
	NFS_PROGRAM,		/* program number */
	2, NFSD_NRVERS-1,	/* version range */
	NFSD_NRVERS,		/* nr of entries in nfsd_version */
	nfsd_version,		/* version table */
	"nfsd",			/* program name */
	&nfsd_svcstats,		/* version table */
};
