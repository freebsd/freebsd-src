/*
 * linux/include/linux/nfsiod.h
 *
 * Declarations for asynchronous NFS RPC calls.
 *
 */

#ifndef _LINUX_NFSIOD_H
#define _LINUX_NFSIOD_H

#include <linux/rpcsock.h>
#include <linux/nfs_fs.h>

#ifdef __KERNEL__

/*
 * This is the callback handler for nfsiod requests.
 * Note that the callback procedure must NOT sleep.
 */
struct nfsiod_req;
typedef int	(*nfsiod_callback_t)(int result, struct nfsiod_req *);

/*
 * This is the nfsiod request struct.
 */
struct nfsiod_req {
	struct nfsiod_req *	rq_next;
	struct nfsiod_req *	rq_prev;
	wait_queue_head_t	rq_wait;
	struct rpc_ioreq	rq_rpcreq;
	nfsiod_callback_t	rq_callback;
	struct nfs_server *	rq_server;
	struct inode *		rq_inode;
	struct page *		rq_page;

	/* user creds */
	uid_t			rq_fsuid;
	gid_t			rq_fsgid;
	int			rq_groups[NGROUPS];

	/* retry handling */
	int			rq_retries;
};

struct nfsiod_req *	nfsiod_reserve(struct nfs_server *);
void			nfsiod_release(struct nfsiod_req *);
void			nfsiod_enqueue(struct nfsiod_req *);
int			nfsiod(void);


#endif /* __KERNEL__ */
#endif /* _LINUX_NFSIOD_H */
