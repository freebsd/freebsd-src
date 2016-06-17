/*
 * linux/net/sunrpc/rpcauth_null.c
 *
 * AUTH_NULL authentication. Really :-)
 *
 * Copyright (C) 1996, Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/utsname.h>
#include <linux/sunrpc/clnt.h>

#ifdef RPC_DEBUG
# define RPCDBG_FACILITY	RPCDBG_AUTH
#endif

static struct rpc_credops	null_credops;

static struct rpc_auth *
nul_create(struct rpc_clnt *clnt)
{
	struct rpc_auth	*auth;

	dprintk("RPC: creating NULL authenticator for client %p\n", clnt);
	if (!(auth = (struct rpc_auth *) rpc_allocate(0, sizeof(*auth))))
		return NULL;
	auth->au_cslack = 4;
	auth->au_rslack = 2;
	auth->au_ops = &authnull_ops;
	auth->au_expire = 1800 * HZ;
	rpcauth_init_credcache(auth);

	return (struct rpc_auth *) auth;
}

static void
nul_destroy(struct rpc_auth *auth)
{
	dprintk("RPC: destroying NULL authenticator %p\n", auth);
	rpcauth_free_credcache(auth);
	rpc_free(auth);
}

/*
 * Create NULL creds for current process
 */
static struct rpc_cred *
nul_create_cred(int flags)
{
	struct rpc_cred	*cred;

	if (!(cred = (struct rpc_cred *) rpc_allocate(flags, sizeof(*cred))))
		return NULL;
	atomic_set(&cred->cr_count, 0);
	cred->cr_flags = RPCAUTH_CRED_UPTODATE;
	cred->cr_uid = current->uid;
	cred->cr_ops = &null_credops;

	return cred;
}

/*
 * Destroy cred handle.
 */
static void
nul_destroy_cred(struct rpc_cred *cred)
{
	rpc_free(cred);
}

/*
 * Match cred handle against current process
 */
static int
nul_match(struct rpc_cred *cred, int taskflags)
{
	return 1;
}

/*
 * Marshal credential.
 */
static u32 *
nul_marshal(struct rpc_task *task, u32 *p, int ruid)
{
	*p++ = htonl(RPC_AUTH_NULL);
	*p++ = 0;
	*p++ = htonl(RPC_AUTH_NULL);
	*p++ = 0;

	return p;
}

/*
 * Refresh credential. This is a no-op for AUTH_NULL
 */
static int
nul_refresh(struct rpc_task *task)
{
	return task->tk_status = -EACCES;
}

static u32 *
nul_validate(struct rpc_task *task, u32 *p)
{
	u32		n = ntohl(*p++);

	if (n != RPC_AUTH_NULL) {
		printk("RPC: bad verf flavor: %ld\n", (unsigned long) n);
		return NULL;
	}
	if ((n = ntohl(*p++)) != 0) {
		printk("RPC: bad verf size: %ld\n", (unsigned long) n);
		return NULL;
	}

	return p;
}

struct rpc_authops	authnull_ops = {
	RPC_AUTH_NULL,
#ifdef RPC_DEBUG
	"NULL",
#endif
	nul_create,
	nul_destroy,
	nul_create_cred
};

static
struct rpc_credops	null_credops = {
	nul_destroy_cred,
	nul_match,
	nul_marshal,
	nul_refresh,
	nul_validate
};
