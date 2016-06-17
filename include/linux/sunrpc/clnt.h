/*
 *  linux/include/linux/sunrpc/clnt.h
 *
 *  Declarations for the high-level RPC client interface
 *
 *  Copyright (C) 1995, 1996, Olaf Kirch <okir@monad.swb.de>
 */

#ifndef _LINUX_SUNRPC_CLNT_H
#define _LINUX_SUNRPC_CLNT_H

#include <linux/sunrpc/msg_prot.h>
#include <linux/sunrpc/sched.h>
#include <linux/sunrpc/xprt.h>
#include <linux/sunrpc/auth.h>
#include <linux/sunrpc/stats.h>
#include <linux/sunrpc/xdr.h>
#include <linux/sunrpc/timer.h>

/*
 * This defines an RPC port mapping
 */
struct rpc_portmap {
	__u32			pm_prog;
	__u32			pm_vers;
	__u32			pm_prot;
	__u16			pm_port;
};

/*
 * The high-level client handle
 */
struct rpc_clnt {
	atomic_t		cl_users;	/* number of references */
	struct rpc_xprt *	cl_xprt;	/* transport */
	struct rpc_procinfo *	cl_procinfo;	/* procedure info */
	u32			cl_maxproc;	/* max procedure number */

	char *			cl_server;	/* server machine name */
	char *			cl_protname;	/* protocol name */
	struct rpc_auth *	cl_auth;	/* authenticator */
	struct rpc_stat *	cl_stats;	/* statistics */

	unsigned int		cl_softrtry : 1,/* soft timeouts */
				cl_intr     : 1,/* interruptible */
				cl_chatty   : 1,/* be verbose */
				cl_autobind : 1,/* use getport() */
				cl_binding  : 1,/* doing a getport() */
				cl_droppriv : 1,/* enable NFS suid hack */
				cl_oneshot  : 1,/* dispose after use */
				cl_dead     : 1;/* abandoned */
	unsigned int		cl_flags;	/* misc client flags */
	unsigned long		cl_hardmax;	/* max hard timeout */

	struct rpc_rtt		cl_rtt;		/* RTO estimator data */

	struct rpc_portmap	cl_pmap;	/* port mapping */
	struct rpc_wait_queue	cl_bindwait;	/* waiting on getport() */

	int			cl_nodelen;	/* nodename length */
	char 			cl_nodename[UNX_MAXNODENAME];
};
#define cl_timeout		cl_xprt->timeout
#define cl_prog			cl_pmap.pm_prog
#define cl_vers			cl_pmap.pm_vers
#define cl_port			cl_pmap.pm_port
#define cl_prot			cl_pmap.pm_prot

/*
 * General RPC program info
 */
#define RPC_MAXVERSION		4
struct rpc_program {
	char *			name;		/* protocol name */
	u32			number;		/* program number */
	unsigned int		nrvers;		/* number of versions */
	struct rpc_version **	version;	/* version array */
	struct rpc_stat *	stats;		/* statistics */
};

struct rpc_version {
	u32			number;		/* version number */
	unsigned int		nrprocs;	/* number of procs */
	struct rpc_procinfo *	procs;		/* procedure array */
};

/*
 * Procedure information
 */
struct rpc_procinfo {
	char *			p_procname;	/* procedure name */
	kxdrproc_t		p_encode;	/* XDR encode function */
	kxdrproc_t		p_decode;	/* XDR decode function */
	unsigned int		p_bufsiz;	/* req. buffer size */
	unsigned int		p_count;	/* call count */
	unsigned int		p_timer;	/* Which RTT timer to use */
};

#define rpcproc_bufsiz(clnt, proc)	((clnt)->cl_procinfo[proc].p_bufsiz)
#define rpcproc_encode(clnt, proc)	((clnt)->cl_procinfo[proc].p_encode)
#define rpcproc_decode(clnt, proc)	((clnt)->cl_procinfo[proc].p_decode)
#define rpcproc_name(clnt, proc)	((clnt)->cl_procinfo[proc].p_procname)
#define rpcproc_count(clnt, proc)	((clnt)->cl_procinfo[proc].p_count)
#define rpcproc_timer(clnt, proc)	((clnt)->cl_procinfo[proc].p_timer)

#define RPC_CONGESTED(clnt)	(RPCXPRT_CONGESTED((clnt)->cl_xprt))
#define RPC_PEERADDR(clnt)	(&(clnt)->cl_xprt->addr)

#ifdef __KERNEL__

struct rpc_clnt *rpc_create_client(struct rpc_xprt *xprt, char *servname,
				struct rpc_program *info,
				u32 version, int authflavor);
int		rpc_shutdown_client(struct rpc_clnt *);
int		rpc_destroy_client(struct rpc_clnt *);
void		rpc_release_client(struct rpc_clnt *);
void		rpc_getport(struct rpc_task *, struct rpc_clnt *);
int		rpc_register(u32, u32, int, unsigned short, int *);

void		rpc_call_setup(struct rpc_task *, struct rpc_message *, int);

int		rpc_call_async(struct rpc_clnt *clnt, struct rpc_message *msg,
			       int flags, rpc_action callback, void *clntdata);
int		rpc_call_sync(struct rpc_clnt *clnt, struct rpc_message *msg,
			      int flags);
void		rpc_restart_call(struct rpc_task *);
void		rpc_clnt_sigmask(struct rpc_clnt *clnt, sigset_t *oldset);
void		rpc_clnt_sigunmask(struct rpc_clnt *clnt, sigset_t *oldset);
void		rpc_setbufsize(struct rpc_clnt *, unsigned int, unsigned int);

static __inline__
int rpc_call(struct rpc_clnt *clnt, u32 proc, void *argp, void *resp, int flags)
{
	struct rpc_message msg = { proc, argp, resp, NULL };
	return rpc_call_sync(clnt, &msg, flags);
}
		

static __inline__ void
rpc_set_timeout(struct rpc_clnt *clnt, unsigned int retr, unsigned long incr)
{
	xprt_set_timeout(&clnt->cl_timeout, retr, incr);
}

extern void rpciod_wake_up(void);

/*
 * Helper function for NFSroot support
 */
int		rpc_getport_external(struct sockaddr_in *, __u32, __u32, int);

#endif /* __KERNEL__ */
#endif /* _LINUX_SUNRPC_CLNT_H */
