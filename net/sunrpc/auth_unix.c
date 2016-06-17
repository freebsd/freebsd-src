/*
 * linux/net/sunrpc/rpcauth_unix.c
 *
 * UNIX-style authentication; no AUTH_SHORT support
 *
 * Copyright (C) 1996, Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/auth.h>

#define NFS_NGROUPS	16
struct unx_cred {
	struct rpc_cred		uc_base;
	uid_t			uc_fsuid;
	gid_t			uc_gid, uc_fsgid;
	gid_t			uc_gids[NFS_NGROUPS];
};
#define uc_uid			uc_base.cr_uid
#define uc_count		uc_base.cr_count
#define uc_flags		uc_base.cr_flags
#define uc_expire		uc_base.cr_expire

#define UNX_CRED_EXPIRE		(60 * HZ)

#define UNX_WRITESLACK		(21 + (UNX_MAXNODENAME >> 2))

#ifdef RPC_DEBUG
# define RPCDBG_FACILITY	RPCDBG_AUTH
#endif

static struct rpc_credops	unix_credops;

static struct rpc_auth *
unx_create(struct rpc_clnt *clnt)
{
	struct rpc_auth	*auth;

	dprintk("RPC: creating UNIX authenticator for client %p\n", clnt);
	if (!(auth = (struct rpc_auth *) rpc_allocate(0, sizeof(*auth))))
		return NULL;
	auth->au_cslack = UNX_WRITESLACK;
	auth->au_rslack = 2;	/* assume AUTH_NULL verf */
	auth->au_expire = UNX_CRED_EXPIRE;
	auth->au_ops = &authunix_ops;

	rpcauth_init_credcache(auth);

	return auth;
}

static void
unx_destroy(struct rpc_auth *auth)
{
	dprintk("RPC: destroying UNIX authenticator %p\n", auth);
	rpcauth_free_credcache(auth);
	rpc_free(auth);
}

static struct rpc_cred *
unx_create_cred(int flags)
{
	struct unx_cred	*cred;
	int		i;

	dprintk("RPC:      allocating UNIX cred for uid %d gid %d\n",
				current->uid, current->gid);

	if (!(cred = (struct unx_cred *) rpc_allocate(flags, sizeof(*cred))))
		return NULL;

	atomic_set(&cred->uc_count, 0);
	cred->uc_flags = RPCAUTH_CRED_UPTODATE;
	if (flags & RPC_TASK_ROOTCREDS) {
		cred->uc_uid = cred->uc_fsuid = 0;
		cred->uc_gid = cred->uc_fsgid = 0;
		cred->uc_gids[0] = NOGROUP;
	} else {
		int groups = current->ngroups;
		if (groups > NFS_NGROUPS)
			groups = NFS_NGROUPS;

		cred->uc_uid = current->uid;
		cred->uc_gid = current->gid;
		cred->uc_fsuid = current->fsuid;
		cred->uc_fsgid = current->fsgid;
		for (i = 0; i < groups; i++)
			cred->uc_gids[i] = (gid_t) current->groups[i];
		if (i < NFS_NGROUPS)
		  cred->uc_gids[i] = NOGROUP;
	}
	cred->uc_base.cr_ops = &unix_credops;

	return (struct rpc_cred *) cred;
}

struct rpc_cred *
authunix_fake_cred(struct rpc_task *task, uid_t uid, gid_t gid)
{
	struct unx_cred	*cred;

	dprintk("RPC:      allocating fake UNIX cred for uid %d gid %d\n",
				uid, gid);

	if (!(cred = (struct unx_cred *) rpc_malloc(task, sizeof(*cred))))
		return NULL;

	atomic_set(&cred->uc_count, 1);
	cred->uc_flags = RPCAUTH_CRED_DEAD|RPCAUTH_CRED_UPTODATE;
	cred->uc_uid   = uid;
	cred->uc_gid   = gid;
	cred->uc_fsuid = uid;
	cred->uc_fsgid = gid;
	cred->uc_gids[0] = (gid_t) NOGROUP;

	return task->tk_msg.rpc_cred = (struct rpc_cred *) cred;
}

static void
unx_destroy_cred(struct rpc_cred *cred)
{
	rpc_free(cred);
}

/*
 * Match credentials against current process creds.
 * The root_override argument takes care of cases where the caller may
 * request root creds (e.g. for NFS swapping).
 */
static int
unx_match(struct rpc_cred *rcred, int taskflags)
{
	struct unx_cred	*cred = (struct unx_cred *) rcred;
	int		i;

	if (!(taskflags & RPC_TASK_ROOTCREDS)) {
		int groups;

		if (cred->uc_uid != current->uid
		 || cred->uc_gid != current->gid
		 || cred->uc_fsuid != current->fsuid
		 || cred->uc_fsgid != current->fsgid)
			return 0;

		groups = current->ngroups;
		if (groups > NFS_NGROUPS)
			groups = NFS_NGROUPS;
		for (i = 0; i < groups ; i++)
			if (cred->uc_gids[i] != (gid_t) current->groups[i])
				return 0;
		return 1;
	}
	return (cred->uc_uid == 0 && cred->uc_fsuid == 0
	     && cred->uc_gid == 0 && cred->uc_fsgid == 0
	     && cred->uc_gids[0] == (gid_t) NOGROUP);
}

/*
 * Marshal credentials.
 * Maybe we should keep a cached credential for performance reasons.
 */
static u32 *
unx_marshal(struct rpc_task *task, u32 *p, int ruid)
{
	struct rpc_clnt	*clnt = task->tk_client;
	struct unx_cred	*cred = (struct unx_cred *) task->tk_msg.rpc_cred;
	u32		*base, *hold;
	int		i, n;

	*p++ = htonl(RPC_AUTH_UNIX);
	base = p++;
	*p++ = htonl(jiffies/HZ);

	/*
	 * Copy the UTS nodename captured when the client was created.
	 */
	n = clnt->cl_nodelen;
	*p++ = htonl(n);
	memcpy(p, clnt->cl_nodename, n);
	p += (n + 3) >> 2;

	/* Note: we don't use real uid if it involves raising priviledge */
	if (ruid && cred->uc_uid != 0 && cred->uc_gid != 0) {
		*p++ = htonl((u32) cred->uc_uid);
		*p++ = htonl((u32) cred->uc_gid);
	} else {
		*p++ = htonl((u32) cred->uc_fsuid);
		*p++ = htonl((u32) cred->uc_fsgid);
	}
	hold = p++;
	for (i = 0; i < 16 && cred->uc_gids[i] != (gid_t) NOGROUP; i++)
		*p++ = htonl((u32) cred->uc_gids[i]);
	*hold = htonl(p - hold - 1);		/* gid array length */
	*base = htonl((p - base - 1) << 2);	/* cred length */

	*p++ = htonl(RPC_AUTH_NULL);
	*p++ = htonl(0);

	return p;
}

/*
 * Refresh credentials. This is a no-op for AUTH_UNIX
 */
static int
unx_refresh(struct rpc_task *task)
{
	task->tk_msg.rpc_cred->cr_flags |= RPCAUTH_CRED_UPTODATE;
	return task->tk_status = -EACCES;
}

static u32 *
unx_validate(struct rpc_task *task, u32 *p)
{
	u32		n = ntohl(*p++);

	if (n != RPC_AUTH_NULL && n != RPC_AUTH_UNIX && n != RPC_AUTH_SHORT) {
		printk("RPC: bad verf flavor: %ld\n", (unsigned long) n);
		return NULL;
	}
	if ((n = ntohl(*p++)) > 400) {
		printk("RPC: giant verf size: %ld\n", (unsigned long) n);
		return NULL;
	}
	task->tk_auth->au_rslack = (n >> 2) + 2;
	p += (n >> 2);

	return p;
}

struct rpc_authops	authunix_ops = {
	RPC_AUTH_UNIX,
#ifdef RPC_DEBUG
	"UNIX",
#endif
	unx_create,
	unx_destroy,
	unx_create_cred
};

static
struct rpc_credops	unix_credops = {
	unx_destroy_cred,
	unx_match,
	unx_marshal,
	unx_refresh,
	unx_validate
};
