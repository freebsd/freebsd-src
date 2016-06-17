/*
 * linux/net/sunrpc/pmap.c
 *
 * Portmapper client.
 *
 * FIXME: In a secure environment, we may want to use an authentication
 * flavor other than AUTH_NULL.
 *
 * Copyright (C) 1996, Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/uio.h>
#include <linux/in.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/xprt.h>
#include <linux/sunrpc/sched.h>

#ifdef RPC_DEBUG
# define RPCDBG_FACILITY	RPCDBG_PMAP
#endif

#define PMAP_SET		1
#define PMAP_UNSET		2
#define PMAP_GETPORT		3

static struct rpc_clnt *	pmap_create(char *, struct sockaddr_in *, int);
static void			pmap_getport_done(struct rpc_task *);
extern struct rpc_program	pmap_program;
static spinlock_t		pmap_lock = SPIN_LOCK_UNLOCKED;

/*
 * Obtain the port for a given RPC service on a given host. This one can
 * be called for an ongoing RPC request.
 */
void
rpc_getport(struct rpc_task *task, struct rpc_clnt *clnt)
{
	struct rpc_portmap *map = &clnt->cl_pmap;
	struct sockaddr_in *sap = &clnt->cl_xprt->addr;
	struct rpc_message msg = { PMAP_GETPORT, map, &clnt->cl_port, NULL };
	struct rpc_clnt	*pmap_clnt;
	struct rpc_task	*child;

	dprintk("RPC: %4d rpc_getport(%s, %d, %d, %d)\n",
			task->tk_pid, clnt->cl_server,
			map->pm_prog, map->pm_vers, map->pm_prot);

	spin_lock(&pmap_lock);
	if (clnt->cl_binding) {
		rpc_sleep_on(&clnt->cl_bindwait, task, NULL, 0);
		spin_unlock(&pmap_lock);
		return;
	}
	clnt->cl_binding = 1;
	spin_unlock(&pmap_lock);

	task->tk_status = -EACCES; /* why set this? returns -EIO below */
	if (!(pmap_clnt = pmap_create(clnt->cl_server, sap, map->pm_prot)))
		goto bailout;
	task->tk_status = 0;

	/*
	 * Note: rpc_new_child will release client after a failure.
	 */
	if (!(child = rpc_new_child(pmap_clnt, task)))
		goto bailout;

	/* Setup the call info struct */
	rpc_call_setup(child, &msg, 0);

	/* ... and run the child task */
	rpc_run_child(task, child, pmap_getport_done);
	return;

bailout:
	spin_lock(&pmap_lock);
	clnt->cl_binding = 0;
	rpc_wake_up(&clnt->cl_bindwait);
	spin_unlock(&pmap_lock);
	task->tk_status = -EIO;
	task->tk_action = NULL;
}

#ifdef CONFIG_ROOT_NFS
int
rpc_getport_external(struct sockaddr_in *sin, __u32 prog, __u32 vers, int prot)
{
	struct rpc_portmap map = { prog, vers, prot, 0 };
	struct rpc_clnt	*pmap_clnt;
	char		hostname[32];
	int		status;

	dprintk("RPC:      rpc_getport_external(%u.%u.%u.%u, %d, %d, %d)\n",
			NIPQUAD(sin->sin_addr.s_addr), prog, vers, prot);

	sprintf(hostname, "%u.%u.%u.%u", NIPQUAD(sin->sin_addr.s_addr));
	if (!(pmap_clnt = pmap_create(hostname, sin, prot)))
		return -EACCES;

	/* Setup the call info struct */
	status = rpc_call(pmap_clnt, PMAP_GETPORT, &map, &map.pm_port, 0);

	if (status >= 0) {
		if (map.pm_port != 0)
			return map.pm_port;
		status = -EACCES;
	}
	return status;
}
#endif

static void
pmap_getport_done(struct rpc_task *task)
{
	struct rpc_clnt	*clnt = task->tk_client;

	dprintk("RPC: %4d pmap_getport_done(status %d, port %d)\n",
			task->tk_pid, task->tk_status, clnt->cl_port);
	if (task->tk_status < 0) {
		/* Make the calling task exit with an error */
		task->tk_action = NULL;
	} else if (clnt->cl_port == 0) {
		/* Program not registered */
		task->tk_status = -EACCES;
		task->tk_action = NULL;
	} else {
		/* byte-swap port number first */
		clnt->cl_port = htons(clnt->cl_port);
		clnt->cl_xprt->addr.sin_port = clnt->cl_port;
	}
	spin_lock(&pmap_lock);
	clnt->cl_binding = 0;
	rpc_wake_up(&clnt->cl_bindwait);
	spin_unlock(&pmap_lock);
}

/*
 * Set or unset a port registration with the local portmapper.
 * port == 0 means unregister, port != 0 means register.
 */
int
rpc_register(u32 prog, u32 vers, int prot, unsigned short port, int *okay)
{
	struct sockaddr_in	sin;
	struct rpc_portmap	map;
	struct rpc_clnt		*pmap_clnt;
	unsigned int		error = 0;

	dprintk("RPC: registering (%d, %d, %d, %d) with portmapper.\n",
			prog, vers, prot, port);

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (!(pmap_clnt = pmap_create("localhost", &sin, IPPROTO_UDP))) {
		dprintk("RPC: couldn't create pmap client\n");
		return -EACCES;
	}

	map.pm_prog = prog;
	map.pm_vers = vers;
	map.pm_prot = prot;
	map.pm_port = port;

	error = rpc_call(pmap_clnt, port? PMAP_SET : PMAP_UNSET,
					&map, okay, 0);

	if (error < 0) {
		printk(KERN_WARNING
			"RPC: failed to contact portmap (errno %d).\n",
			error);
	}
	dprintk("RPC: registration status %d/%d\n", error, *okay);

	/* Client deleted automatically because cl_oneshot == 1 */
	return error;
}

static struct rpc_clnt *
pmap_create(char *hostname, struct sockaddr_in *srvaddr, int proto)
{
	struct rpc_xprt	*xprt;
	struct rpc_clnt	*clnt;

	/* printk("pmap: create xprt\n"); */
	if (!(xprt = xprt_create_proto(proto, srvaddr, NULL)))
		return NULL;
	xprt->addr.sin_port = htons(RPC_PMAP_PORT);

	/* printk("pmap: create clnt\n"); */
	clnt = rpc_create_client(xprt, hostname,
				&pmap_program, RPC_PMAP_VERSION,
				RPC_AUTH_NULL);
	if (!clnt) {
		xprt_destroy(xprt);
	} else {
		clnt->cl_softrtry = 1;
		clnt->cl_chatty   = 1;
		clnt->cl_oneshot  = 1;
	}
	return clnt;
}

/*
 * XDR encode/decode functions for PMAP
 */
static int
xdr_error(struct rpc_rqst *req, u32 *p, void *dummy)
{
	return -EIO;
}

static int
xdr_encode_mapping(struct rpc_rqst *req, u32 *p, struct rpc_portmap *map)
{
	dprintk("RPC: xdr_encode_mapping(%d, %d, %d, %d)\n",
		map->pm_prog, map->pm_vers, map->pm_prot, map->pm_port);
	*p++ = htonl(map->pm_prog);
	*p++ = htonl(map->pm_vers);
	*p++ = htonl(map->pm_prot);
	*p++ = htonl(map->pm_port);

	req->rq_slen = xdr_adjust_iovec(req->rq_svec, p);
	return 0;
}

static int
xdr_decode_port(struct rpc_rqst *req, u32 *p, unsigned short *portp)
{
	*portp = (unsigned short) ntohl(*p++);
	return 0;
}

static int
xdr_decode_bool(struct rpc_rqst *req, u32 *p, unsigned int *boolp)
{
	*boolp = (unsigned int) ntohl(*p++);
	return 0;
}

static struct rpc_procinfo	pmap_procedures[4] = {
	{ "pmap_null",
		(kxdrproc_t) xdr_error,	
		(kxdrproc_t) xdr_error,	0, 0 },
	{ "pmap_set",
		(kxdrproc_t) xdr_encode_mapping,	
		(kxdrproc_t) xdr_decode_bool, 4, 1 },
	{ "pmap_unset",
		(kxdrproc_t) xdr_encode_mapping,	
		(kxdrproc_t) xdr_decode_bool, 4, 1 },
	{ "pmap_get",
		(kxdrproc_t) xdr_encode_mapping,
		(kxdrproc_t) xdr_decode_port, 4, 1 },
};

static struct rpc_version	pmap_version2 = {
	2, 4, pmap_procedures
};

static struct rpc_version *	pmap_version[] = {
	NULL,
	NULL,
	&pmap_version2,
};

static struct rpc_stat		pmap_stats;

struct rpc_program	pmap_program = {
	"portmap",
	RPC_PMAP_PROGRAM,
	sizeof(pmap_version)/sizeof(pmap_version[0]),
	pmap_version,
	&pmap_stats,
};
