#ifndef _NFS_FS_SB
#define _NFS_FS_SB

#include <linux/list.h>

/*
 * NFS client parameters stored in the superblock.
 */
struct nfs_server {
	struct rpc_clnt *	client;		/* RPC client handle */
	struct nfs_rpc_ops *	rpc_ops;	/* NFS protocol vector */
	int			flags;		/* various flags */
	unsigned int		rsize;		/* read size */
	unsigned int		rpages;		/* read size (in pages) */
	unsigned int		wsize;		/* write size */
	unsigned int		wpages;		/* write size (in pages) */
	unsigned int		dtsize;		/* readdir size */
	unsigned int		bsize;		/* server block size */
	unsigned int		acregmin;	/* attr cache timeouts */
	unsigned int		acregmax;
	unsigned int		acdirmin;
	unsigned int		acdirmax;
	unsigned int		namelen;
	char *			hostname;	/* remote hostname */
	struct nfs_reqlist *	rw_requests;	/* async read/write requests */
	struct list_head	lru_read,
				lru_dirty,
				lru_commit,
				lru_busy;
};

/*
 * nfs super-block data in memory
 */
struct nfs_sb_info {
	struct nfs_server	s_server;
};

#endif
