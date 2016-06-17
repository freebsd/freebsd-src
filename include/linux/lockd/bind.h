/*
 * linux/include/linux/lockd/bind.h
 *
 * This is the part of lockd visible to nfsd and the nfs client.
 *
 * Copyright (C) 1996, Olaf Kirch <okir@monad.swb.de>
 */

#ifndef LINUX_LOCKD_BIND_H
#define LINUX_LOCKD_BIND_H

#include <linux/lockd/nlm.h>

/* Dummy declarations */
struct svc_rqst;
struct svc_client;		/* opaque type */

/*
 * This is the set of functions for lockd->nfsd communication
 */
struct nlmsvc_binding {
	void			(*exp_readlock)(void);
	void			(*exp_unlock)(void);
	struct svc_client *	(*exp_getclient)(struct sockaddr_in *);
	u32			(*fopen)(struct svc_rqst *,
						struct nfs_fh *,
						struct file *);
	void			(*fclose)(struct file *);
	void			(*detach)(void);
};

extern struct nlmsvc_binding *	nlmsvc_ops;

/*
 * Functions exported by the lockd module
 */
extern void	nlmsvc_invalidate_client(struct svc_client *clnt);
extern int	nlmclnt_proc(struct inode *, int, struct file_lock *);
extern int	lockd_up(void);
extern void	lockd_down(void);

#endif /* LINUX_LOCKD_BIND_H */
