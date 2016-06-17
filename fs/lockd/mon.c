/*
 * linux/fs/lockd/mon.c
 *
 * The kernel statd client.
 *
 * Copyright (C) 1996, Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/types.h>
#include <linux/utsname.h>
#include <linux/kernel.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/svc.h>
#include <linux/lockd/lockd.h>
#include <linux/lockd/sm_inter.h>


#define NLMDBG_FACILITY		NLMDBG_MONITOR

static struct rpc_clnt *	nsm_create(void);

extern struct rpc_program	nsm_program;

/*
 * Local NSM state
 */
u32				nsm_local_state;

/*
 * Common procedure for SM_MON/SM_UNMON calls
 */
static int
nsm_mon_unmon(struct nlm_host *host, u32 proc, struct nsm_res *res)
{
	struct rpc_clnt	*clnt;
	int		status;
	struct nsm_args	args;

	status = -EACCES;
	clnt = nsm_create();
	if (!clnt)
		goto out;

	args.addr = host->h_addr.sin_addr.s_addr;
	args.prog = NLM_PROGRAM;
	args.vers = host->h_version;
	args.proc = NLMPROC_NSM_NOTIFY;
	memset(res, 0, sizeof(*res));

	status = rpc_call(clnt, proc, &args, res, 0);
	if (status < 0)
		printk(KERN_DEBUG "nsm_mon_unmon: rpc failed, status=%d\n",
			status);
	else
		status = 0;
 out:
	return status;
}

/*
 * Set up monitoring of a remote host
 */
int
nsm_monitor(struct nlm_host *host)
{
	struct nsm_res	res;
	int		status;

	dprintk("lockd: nsm_monitor(%s)\n", host->h_name);

	status = nsm_mon_unmon(host, SM_MON, &res);

	if (status < 0 || res.status != 0)
		printk(KERN_NOTICE "lockd: cannot monitor %s\n", host->h_name);
	else
		host->h_monitored = 1;
	return status;
}

/*
 * Cease to monitor remote host
 */
int
nsm_unmonitor(struct nlm_host *host)
{
	struct nsm_res	res;
	int		status;

	dprintk("lockd: nsm_unmonitor(%s)\n", host->h_name);

	status = nsm_mon_unmon(host, SM_UNMON, &res);
	if (status < 0)
		printk(KERN_NOTICE "lockd: cannot unmonitor %s\n", host->h_name);
	else
		host->h_monitored = 0;
	return status;
}

/*
 * Create NSM client for the local host
 */
static struct rpc_clnt *
nsm_create(void)
{
	struct rpc_xprt		*xprt;
	struct rpc_clnt		*clnt = NULL;
	struct sockaddr_in	sin;

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sin.sin_port = 0;

	xprt = xprt_create_proto(IPPROTO_UDP, &sin, NULL);
	if (!xprt)
		goto out;

	clnt = rpc_create_client(xprt, "localhost",
				&nsm_program, SM_VERSION,
				RPC_AUTH_NULL);
	if (!clnt)
		goto out_destroy;
	clnt->cl_softrtry = 1;
	clnt->cl_chatty   = 1;
	clnt->cl_oneshot  = 1;
	xprt->resvport = 1;	/* NSM requires a reserved port */
out:
	return clnt;

out_destroy:
	xprt_destroy(xprt);
	goto out;
}

/*
 * XDR functions for NSM.
 */
static int
xdr_error(struct rpc_rqst *rqstp, u32 *p, void *dummy)
{
	return -EACCES;
}

static int
xdr_encode_mon(struct rpc_rqst *rqstp, u32 *p, struct nsm_args *argp)
{
	char	buffer[20];
	u32	addr = ntohl(argp->addr);

	dprintk("nsm: xdr_encode_mon(%08x, %d, %d, %d)\n",
			htonl(argp->addr), htonl(argp->prog),
			htonl(argp->vers), htonl(argp->proc));

	/*
	 * Use the dotted-quad IP address of the remote host as
	 * identifier. Linux statd always looks up the canonical
	 * hostname first for whatever remote hostname it receives,
	 * so this works alright.
	 */
	sprintf(buffer, "%d.%d.%d.%d", (addr>>24) & 0xff, (addr>>16) & 0xff,
				 	(addr>>8) & 0xff,  (addr) & 0xff);
	if (!(p = xdr_encode_string(p, buffer))
	 || !(p = xdr_encode_string(p, system_utsname.nodename)))
		return -EIO;
	*p++ = htonl(argp->prog);
	*p++ = htonl(argp->vers);
	*p++ = htonl(argp->proc);

	/* This is the private part. Needed only for SM_MON call */
	if (rqstp->rq_task->tk_msg.rpc_proc == SM_MON) {
		*p++ = argp->addr;
		*p++ = 0;
		*p++ = 0;
		*p++ = 0;
	}

	rqstp->rq_slen = xdr_adjust_iovec(rqstp->rq_svec, p);
	return 0;
}

static int
xdr_decode_stat_res(struct rpc_rqst *rqstp, u32 *p, struct nsm_res *resp)
{
	resp->status = ntohl(*p++);
	resp->state = ntohl(*p++);
	dprintk("nsm: xdr_decode_stat_res status %d state %d\n",
			resp->status, resp->state);
	return 0;
}

static int
xdr_decode_stat(struct rpc_rqst *rqstp, u32 *p, struct nsm_res *resp)
{
	resp->state = ntohl(*p++);
	return 0;
}

#define SM_my_name_sz	(1+XDR_QUADLEN(SM_MAXSTRLEN))
#define SM_my_id_sz	(3+1+SM_my_name_sz)
#define SM_mon_id_sz	(1+XDR_QUADLEN(20)+SM_my_id_sz)
#define SM_mon_sz	(SM_mon_id_sz+4)
#define SM_monres_sz	2
#define SM_unmonres_sz	1

#ifndef MAX
# define MAX(a, b)	(((a) > (b))? (a) : (b))
#endif

static struct rpc_procinfo	nsm_procedures[] = {
        { "sm_null",
		(kxdrproc_t) xdr_error,
		(kxdrproc_t) xdr_error, 0, 0 },
        { "sm_stat",
		(kxdrproc_t) xdr_error,
		(kxdrproc_t) xdr_error, 0, 0 },
        { "sm_mon",
		(kxdrproc_t) xdr_encode_mon,
		(kxdrproc_t) xdr_decode_stat_res, MAX(SM_mon_sz, SM_monres_sz) << 2, 0 },
        { "sm_unmon",
		(kxdrproc_t) xdr_encode_mon,
		(kxdrproc_t) xdr_decode_stat, MAX(SM_mon_id_sz, SM_unmonres_sz) << 2, 0 },
        { "sm_unmon_all",
		(kxdrproc_t) xdr_error,
		(kxdrproc_t) xdr_error, 0, 0 },
        { "sm_simu_crash",
		(kxdrproc_t) xdr_error,
		(kxdrproc_t) xdr_error, 0, 0 },
        { "sm_notify",
		(kxdrproc_t) xdr_error,
		(kxdrproc_t) xdr_error, 0, 0 },
};

static struct rpc_version	nsm_version1 = {
	1, 
	sizeof(nsm_procedures)/sizeof(nsm_procedures[0]),
	nsm_procedures
};

static struct rpc_version *	nsm_version[] = {
	NULL,
	&nsm_version1,
};

static struct rpc_stat		nsm_stats;

struct rpc_program		nsm_program = {
	"statd",
	SM_PROGRAM,
	sizeof(nsm_version)/sizeof(nsm_version[0]),
	nsm_version,
	&nsm_stats
};
