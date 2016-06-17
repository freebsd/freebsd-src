/*
 * linux/include/linux/sunrpc/svcauth.h
 *
 * RPC server-side authentication stuff.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#ifndef _LINUX_SUNRPC_SVCAUTH_H_
#define _LINUX_SUNRPC_SVCAUTH_H_

#ifdef __KERNEL__

#include <linux/sunrpc/msg_prot.h>

struct svc_cred {
	u32			cr_flavor;
	uid_t			cr_uid;
	gid_t			cr_gid;
	gid_t			cr_groups[NGROUPS];
};

struct svc_rqst;		/* forward decl */

void	svc_authenticate(struct svc_rqst *rqstp, u32 *statp, u32 *authp);
int	svc_auth_register(u32 flavor, void (*)(struct svc_rqst *,u32 *,u32 *));
void	svc_auth_unregister(u32 flavor);

#if 0
/*
 * Decoded AUTH_UNIX data. This is different from what's in the RPC lib.
 */
#define NGRPS		16
struct authunix_parms {
	u32		aup_stamp;
	u32		aup_uid;
	u32		aup_gid;
	u32		aup_len;
	u32		aup_gids[NGRPS];
};

struct svc_authops *	auth_getops(u32 flavor);
#endif


#endif /* __KERNEL__ */

#endif /* _LINUX_SUNRPC_SVCAUTH_H_ */
