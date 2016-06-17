#ifndef NFS_CLUSTER_H
#define NFS_CLUSTER_H



#ifdef __KERNEL__
#include <asm/atomic.h>
#include <linux/nfs_fs_sb.h>

/*
 * Counters of total number and pending number of requests.
 * When the total number of requests exceeds the hard limit, we stall
 * until it drops again.
 */
#define MAX_REQUEST_HARD        256

/*
 * Maximum number of requests per write cluster.
 * 32 requests per cluster account for 128K of data on an intel box.
 * Note: it's a good idea to make this number smaller than MAX_REQUEST_SOFT.
 *
 * For 100Mbps Ethernet, 128 pages (i.e. 256K) per cluster gives much
 * better performance.
 */
#define REQUEST_HASH_SIZE	16
#define REQUEST_NR(off)		((off) >> PAGE_CACHE_SHIFT)
#define REQUEST_HASH(ino, off)	(((ino) ^ REQUEST_NR(off)) & (REQUEST_HASH_SIZE - 1))


/*
 * Functions
 */
extern int		nfs_reqlist_alloc(struct nfs_server *);
extern void		nfs_reqlist_free(struct nfs_server *);
extern void		nfs_reqlist_exit(struct nfs_server *);
extern void		nfs_wake_flushd(void);

/*
 * This is the per-mount writeback cache.
 */
struct nfs_reqlist {
	atomic_t		nr_requests;
	unsigned long		runat;
	wait_queue_head_t	request_wait;

	/* The async RPC task that is responsible for scanning the
	 * requests.
	 */
	struct rpc_task		*task;		/* request flush task */

	/* Authentication flavor handle for this NFS client */
	struct rpc_auth		*auth;

	/* The list of all inodes with pending writebacks.  */
	struct inode		*inodes;
};

#endif

#endif
