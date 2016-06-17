/*
 * linux/net/sunrpc/svc.c
 *
 * High-level RPC service routines
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#define __KERNEL_SYSCALLS__
#include <linux/linkage.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/unistd.h>

#include <linux/sunrpc/types.h>
#include <linux/sunrpc/xdr.h>
#include <linux/sunrpc/stats.h>
#include <linux/sunrpc/svcsock.h>
#include <linux/sunrpc/clnt.h>

#define RPCDBG_FACILITY	RPCDBG_SVCDSP
#define RPC_PARANOIA 1

/*
 * Create an RPC service
 */
struct svc_serv *
svc_create(struct svc_program *prog, unsigned int bufsize, unsigned int xdrsize)
{
	struct svc_serv	*serv;

	if (!(serv = (struct svc_serv *) kmalloc(sizeof(*serv), GFP_KERNEL)))
		return NULL;

	memset(serv, 0, sizeof(*serv));
	serv->sv_program   = prog;
	serv->sv_nrthreads = 1;
	serv->sv_stats     = prog->pg_stats;
	serv->sv_bufsz	   = bufsize? bufsize : 4096;
	serv->sv_xdrsize   = xdrsize;
	INIT_LIST_HEAD(&serv->sv_threads);
	INIT_LIST_HEAD(&serv->sv_sockets);
	INIT_LIST_HEAD(&serv->sv_tempsocks);
	INIT_LIST_HEAD(&serv->sv_permsocks);
	spin_lock_init(&serv->sv_lock);

	serv->sv_name      = prog->pg_name;

	/* Remove any stale portmap registrations */
	svc_register(serv, 0, 0);

	return serv;
}

/*
 * Destroy an RPC service
 */
void
svc_destroy(struct svc_serv *serv)
{
	struct svc_sock	*svsk;

	dprintk("RPC: svc_destroy(%s, %d)\n",
				serv->sv_program->pg_name,
				serv->sv_nrthreads);

	if (serv->sv_nrthreads) {
		if (--(serv->sv_nrthreads) != 0) {
			svc_sock_update_bufs(serv);
			return;
		}
	} else
		printk("svc_destroy: no threads for serv=%p!\n", serv);

	while (!list_empty(&serv->sv_tempsocks)) {
		svsk = list_entry(serv->sv_tempsocks.next,
				  struct svc_sock,
				  sk_list);
		svc_delete_socket(svsk);
	}
	while (!list_empty(&serv->sv_permsocks)) {
		svsk = list_entry(serv->sv_permsocks.next,
				  struct svc_sock,
				  sk_list);
		svc_delete_socket(svsk);
	}

	/* Unregister service with the portmapper */
	svc_register(serv, 0, 0);
	kfree(serv);
}

/*
 * Allocate an RPC server buffer
 * Later versions may do nifty things by allocating multiple pages
 * of memory directly and putting them into the bufp->iov.
 */
int
svc_init_buffer(struct svc_buf *bufp, unsigned int size)
{
	if (!(bufp->area = (u32 *) kmalloc(size, GFP_KERNEL)))
		return 0;
	bufp->base   = bufp->area;
	bufp->buf    = bufp->area;
	bufp->len    = 0;
	bufp->buflen = size >> 2;

	bufp->iov[0].iov_base = bufp->area;
	bufp->iov[0].iov_len  = size;
	bufp->nriov = 1;

	return 1;
}

/*
 * Release an RPC server buffer
 */
void
svc_release_buffer(struct svc_buf *bufp)
{
	kfree(bufp->area);
	bufp->area = 0;
}

/*
 * Create a server thread
 */
int
svc_create_thread(svc_thread_fn func, struct svc_serv *serv)
{
	struct svc_rqst	*rqstp;
	int		error = -ENOMEM;

	rqstp = kmalloc(sizeof(*rqstp), GFP_KERNEL);
	if (!rqstp)
		goto out;

	memset(rqstp, 0, sizeof(*rqstp));
	init_waitqueue_head(&rqstp->rq_wait);

	if (!(rqstp->rq_argp = (u32 *) kmalloc(serv->sv_xdrsize, GFP_KERNEL))
	 || !(rqstp->rq_resp = (u32 *) kmalloc(serv->sv_xdrsize, GFP_KERNEL))
	 || !svc_init_buffer(&rqstp->rq_defbuf, serv->sv_bufsz))
		goto out_thread;

	serv->sv_nrthreads++;
	rqstp->rq_server = serv;
	error = kernel_thread((int (*)(void *)) func, rqstp, 0);
	if (error < 0)
		goto out_thread;
	svc_sock_update_bufs(serv);
	error = 0;
out:
	return error;

out_thread:
	svc_exit_thread(rqstp);
	goto out;
}

/*
 * Destroy an RPC server thread
 */
void
svc_exit_thread(struct svc_rqst *rqstp)
{
	struct svc_serv	*serv = rqstp->rq_server;

	svc_release_buffer(&rqstp->rq_defbuf);
	if (rqstp->rq_resp)
		kfree(rqstp->rq_resp);
	if (rqstp->rq_argp)
		kfree(rqstp->rq_argp);
	kfree(rqstp);

	/* Release the server */
	if (serv)
		svc_destroy(serv);
}

/*
 * Register an RPC service with the local portmapper.
 * To unregister a service, call this routine with 
 * proto and port == 0.
 */
int
svc_register(struct svc_serv *serv, int proto, unsigned short port)
{
	struct svc_program	*progp;
	unsigned long		flags;
	int			i, error = 0, dummy;

	progp = serv->sv_program;

	dprintk("RPC: svc_register(%s, %s, %d)\n",
		progp->pg_name, proto == IPPROTO_UDP? "udp" : "tcp", port);

	if (!port)
		current->sigpending = 0;

	for (i = 0; i < progp->pg_nvers; i++) {
		if (progp->pg_vers[i] == NULL)
			continue;
		error = rpc_register(progp->pg_prog, i, proto, port, &dummy);
		if (error < 0)
			break;
		if (port && !dummy) {
			error = -EACCES;
			break;
		}
	}

	if (!port) {
		spin_lock_irqsave(&current->sigmask_lock, flags);
		recalc_sigpending(current);
		spin_unlock_irqrestore(&current->sigmask_lock, flags);
	}

	return error;
}

/*
 * Process the RPC request.
 */
int
svc_process(struct svc_serv *serv, struct svc_rqst *rqstp)
{
	struct svc_program	*progp;
	struct svc_version	*versp = NULL;	/* compiler food */
	struct svc_procedure	*procp = NULL;
	struct svc_buf *	argp = &rqstp->rq_argbuf;
	struct svc_buf *	resp = &rqstp->rq_resbuf;
	kxdrproc_t		xdr;
	u32			*bufp, *statp;
	u32			dir, prog, vers, proc,
				auth_stat, rpc_stat;

	rpc_stat = rpc_success;
	bufp = argp->buf;

	if (argp->len < 5)
		goto err_short_len;

	dir  = ntohl(*bufp++);
	vers = ntohl(*bufp++);

	/* First words of reply: */
	svc_putlong(resp, xdr_one);		/* REPLY */
	svc_putlong(resp, xdr_zero);		/* ACCEPT */

	if (dir != 0)		/* direction != CALL */
		goto err_bad_dir;
	if (vers != 2)		/* RPC version number */
		goto err_bad_rpc;

	rqstp->rq_prog = prog = ntohl(*bufp++);	/* program number */
	rqstp->rq_vers = vers = ntohl(*bufp++);	/* version number */
	rqstp->rq_proc = proc = ntohl(*bufp++);	/* procedure number */

	argp->buf += 5;
	argp->len -= 5;

	/* Used by nfsd to only allow the NULL procedure for amd. */
	if (rqstp->rq_auth && !rqstp->rq_client && proc) {
		auth_stat = rpc_autherr_badcred;
		goto err_bad_auth;
	}

	/*
	 * Decode auth data, and add verifier to reply buffer.
	 * We do this before anything else in order to get a decent
	 * auth verifier.
	 */
	svc_authenticate(rqstp, &rpc_stat, &auth_stat);

	if (rpc_stat != rpc_success)
		goto err_garbage;

	if (auth_stat != rpc_auth_ok)
		goto err_bad_auth;

	progp = serv->sv_program;
	if (prog != progp->pg_prog)
		goto err_bad_prog;

	if (vers >= progp->pg_nvers ||
	  !(versp = progp->pg_vers[vers]))
		goto err_bad_vers;

	procp = versp->vs_proc + proc;
	if (proc >= versp->vs_nproc || !procp->pc_func)
		goto err_bad_proc;
	rqstp->rq_server   = serv;
	rqstp->rq_procinfo = procp;

	/* Syntactic check complete */
	serv->sv_stats->rpccnt++;

	/* Build the reply header. */
	statp = resp->buf;
	svc_putlong(resp, rpc_success);		/* RPC_SUCCESS */

	/* Bump per-procedure stats counter */
	procp->pc_count++;

	/* Initialize storage for argp and resp */
	memset(rqstp->rq_argp, 0, procp->pc_argsize);
	memset(rqstp->rq_resp, 0, procp->pc_ressize);

	/* un-reserve some of the out-queue now that we have a 
	 * better idea of reply size
	 */
	if (procp->pc_xdrressize)
		svc_reserve(rqstp, procp->pc_xdrressize<<2);

	/* Call the function that processes the request. */
	if (!versp->vs_dispatch) {
		/* Decode arguments */
		xdr = procp->pc_decode;
		if (xdr && !xdr(rqstp, rqstp->rq_argbuf.buf, rqstp->rq_argp))
			goto err_garbage;

		*statp = procp->pc_func(rqstp, rqstp->rq_argp, rqstp->rq_resp);

		/* Encode reply */
		if (*statp == rpc_success && (xdr = procp->pc_encode)
		 && !xdr(rqstp, rqstp->rq_resbuf.buf, rqstp->rq_resp)) {
			dprintk("svc: failed to encode reply\n");
			/* serv->sv_stats->rpcsystemerr++; */
			*statp = rpc_system_err;
		}
	} else {
		dprintk("svc: calling dispatcher\n");
		if (!versp->vs_dispatch(rqstp, statp)) {
			/* Release reply info */
			if (procp->pc_release)
				procp->pc_release(rqstp, NULL, rqstp->rq_resp);
			goto dropit;
		}
	}

	/* Check RPC status result */
	if (*statp != rpc_success)
		resp->len = statp + 1 - resp->base;

	/* Release reply info */
	if (procp->pc_release)
		procp->pc_release(rqstp, NULL, rqstp->rq_resp);

	if (procp->pc_encode == NULL)
		goto dropit;
sendit:
	return svc_send(rqstp);

dropit:
	dprintk("svc: svc_process dropit\n");
	svc_drop(rqstp);
	return 0;

err_short_len:
#ifdef RPC_PARANOIA
	printk("svc: short len %d, dropping request\n", argp->len);
#endif
	goto dropit;			/* drop request */

err_bad_dir:
#ifdef RPC_PARANOIA
	printk("svc: bad direction %d, dropping request\n", dir);
#endif
	serv->sv_stats->rpcbadfmt++;
	goto dropit;			/* drop request */

err_bad_rpc:
	serv->sv_stats->rpcbadfmt++;
	resp->buf[-1] = xdr_one;	/* REJECT */
	svc_putlong(resp, xdr_zero);	/* RPC_MISMATCH */
	svc_putlong(resp, xdr_two);	/* Only RPCv2 supported */
	svc_putlong(resp, xdr_two);
	goto sendit;

err_bad_auth:
	dprintk("svc: authentication failed (%d)\n", ntohl(auth_stat));
	serv->sv_stats->rpcbadauth++;
	resp->buf[-1] = xdr_one;	/* REJECT */
	svc_putlong(resp, xdr_one);	/* AUTH_ERROR */
	svc_putlong(resp, auth_stat);	/* status */
	goto sendit;

err_bad_prog:
#ifdef RPC_PARANOIA
	if (prog != 100227 || progp->pg_prog != 100003)
		printk("svc: unknown program %d (me %d)\n", prog, progp->pg_prog);
	/* else it is just a Solaris client seeing if ACLs are supported */
#endif
	serv->sv_stats->rpcbadfmt++;
	svc_putlong(resp, rpc_prog_unavail);
	goto sendit;

err_bad_vers:
#ifdef RPC_PARANOIA
	if (vers)
		printk("svc: unknown version (%d)\n", vers);
#endif
	serv->sv_stats->rpcbadfmt++;
	svc_putlong(resp, rpc_prog_mismatch);
	svc_putlong(resp, htonl(progp->pg_lovers));
	svc_putlong(resp, htonl(progp->pg_hivers));
	goto sendit;

err_bad_proc:
#ifdef RPC_PARANOIA
	printk("svc: unknown procedure (%d)\n", proc);
#endif
	serv->sv_stats->rpcbadfmt++;
	svc_putlong(resp, rpc_proc_unavail);
	goto sendit;

err_garbage:
#ifdef RPC_PARANOIA
	printk("svc: failed to decode args\n");
#endif
	serv->sv_stats->rpcbadfmt++;
	svc_putlong(resp, rpc_garbage_args);
	goto sendit;
}
