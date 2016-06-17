/*
 * linux/net/sunrpc/svcauth.c
 *
 * The generic interface for RPC authentication on the server side.
 * 
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 *
 * CHANGES
 * 19-Apr-2000 Chris Evans      - Security fix
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/sunrpc/types.h>
#include <linux/sunrpc/xdr.h>
#include <linux/sunrpc/svcauth.h>
#include <linux/sunrpc/svcsock.h>

#define RPCDBG_FACILITY	RPCDBG_AUTH

/*
 * Type of authenticator function
 */
typedef void	(*auth_fn_t)(struct svc_rqst *rqstp, u32 *statp, u32 *authp);

/*
 * Builtin auth flavors
 */
static void	svcauth_null(struct svc_rqst *rqstp, u32 *statp, u32 *authp);
static void	svcauth_unix(struct svc_rqst *rqstp, u32 *statp, u32 *authp);

/*
 * Max number of authentication flavors we support
 */
#define RPC_SVCAUTH_MAX	8

/*
 * Table of authenticators
 */
static auth_fn_t	authtab[RPC_SVCAUTH_MAX] = {
	svcauth_null,
	svcauth_unix,
	NULL,
};

void
svc_authenticate(struct svc_rqst *rqstp, u32 *statp, u32 *authp)
{
	u32		flavor;
	auth_fn_t	func;

	*statp = rpc_success;
	*authp = rpc_auth_ok;

	svc_getlong(&rqstp->rq_argbuf, flavor);
	flavor = ntohl(flavor);

	dprintk("svc: svc_authenticate (%d)\n", flavor);
	if (flavor >= RPC_SVCAUTH_MAX || !(func = authtab[flavor])) {
		*authp = rpc_autherr_badcred;
		return;
	}

	rqstp->rq_cred.cr_flavor = flavor;
	func(rqstp, statp, authp);
}

int
svc_auth_register(u32 flavor, auth_fn_t func)
{
	if (flavor >= RPC_SVCAUTH_MAX || authtab[flavor])
		return -EINVAL;
	authtab[flavor] = func;
	return 0;
}

void
svc_auth_unregister(u32 flavor)
{
	if (flavor < RPC_SVCAUTH_MAX)
		authtab[flavor] = NULL;
}

static void
svcauth_null(struct svc_rqst *rqstp, u32 *statp, u32 *authp)
{
	struct svc_buf	*argp = &rqstp->rq_argbuf;
	struct svc_buf	*resp = &rqstp->rq_resbuf;

	if ((argp->len -= 3) < 0) {
		*statp = rpc_garbage_args;
		return;
	}
	if (*(argp->buf)++ != 0) {	/* we already skipped the flavor */
		dprintk("svc: bad null cred\n");
		*authp = rpc_autherr_badcred;
		return;
	}
	if (*(argp->buf)++ != RPC_AUTH_NULL || *(argp->buf)++ != 0) {
		dprintk("svc: bad null verf\n");
		*authp = rpc_autherr_badverf;
		return;
	}

	/* Signal that mapping to nobody uid/gid is required */
	rqstp->rq_cred.cr_uid = (uid_t) -1;
	rqstp->rq_cred.cr_gid = (gid_t) -1;
	rqstp->rq_cred.cr_groups[0] = NOGROUP;

	/* Put NULL verifier */
	rqstp->rq_verfed = 1;
	svc_putlong(resp, RPC_AUTH_NULL);
	svc_putlong(resp, 0);
}

static void
svcauth_unix(struct svc_rqst *rqstp, u32 *statp, u32 *authp)
{
	struct svc_buf	*argp = &rqstp->rq_argbuf;
	struct svc_buf	*resp = &rqstp->rq_resbuf;
	struct svc_cred	*cred = &rqstp->rq_cred;
	u32		*bufp = argp->buf, slen, i;
	int		len   = argp->len;

	if ((len -= 3) < 0) {
		*statp = rpc_garbage_args;
		return;
	}

	bufp++;					/* length */
	bufp++;					/* time stamp */
	slen = (ntohl(*bufp++) + 3) >> 2;	/* machname length */
	if (slen > 64 || (len -= slen + 3) < 0)
		goto badcred;
	bufp += slen;				/* skip machname */

	cred->cr_uid = ntohl(*bufp++);		/* uid */
	cred->cr_gid = ntohl(*bufp++);		/* gid */

	slen = ntohl(*bufp++);			/* gids length */
	if (slen > 16 || (len -= slen + 2) < 0)
		goto badcred;
	for (i = 0; i < NGROUPS && i < slen; i++)
		cred->cr_groups[i] = ntohl(*bufp++);
	if (i < NGROUPS)
		cred->cr_groups[i] = NOGROUP;
	bufp += (slen - i);

	if (*bufp++ != RPC_AUTH_NULL || *bufp++ != 0) {
		*authp = rpc_autherr_badverf;
		return;
	}

	argp->buf = bufp;
	argp->len = len;

	/* Put NULL verifier */
	rqstp->rq_verfed = 1;
	svc_putlong(resp, RPC_AUTH_NULL);
	svc_putlong(resp, 0);

	return;

badcred:
	*authp = rpc_autherr_badcred;
}
