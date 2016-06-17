/*
 * linux/fs/nfs/flushd.c
 *
 * For each NFS mount, there is a separate cache object that contains
 * a hash table of all clusters. With this cache, an async RPC task
 * (`flushd') is associated, which wakes up occasionally to inspect
 * its list of dirty buffers.
 * (Note that RPC tasks aren't kernel threads. Take a look at the
 * rpciod code to understand what they are).
 *
 * Inside the cache object, we also maintain a count of the current number
 * of dirty pages, which may not exceed a certain threshold.
 * (FIXME: This threshold should be configurable).
 *
 * The code is streamlined for what I think is the prevalent case for
 * NFS traffic, which is sequential write access without concurrent
 * access by different processes.
 *
 * Copyright (C) 1996, 1997, Olaf Kirch <okir@monad.swb.de>
 *
 * Rewritten 6/3/2000 by Trond Myklebust
 * Copyright (C) 1999, 2000, Trond Myklebust <trond.myklebust@fys.uio.no>
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/file.h>

#include <linux/sched.h>

#include <linux/sunrpc/auth.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/sched.h>

#include <linux/smp_lock.h>

#include <linux/nfs.h>
#include <linux/nfs_fs.h>
#include <linux/nfs_page.h>
#include <linux/nfs_fs_sb.h>
#include <linux/nfs_flushd.h>

/*
 * Various constants
 */
#define NFSDBG_FACILITY         NFSDBG_PAGECACHE

/*
 * This is the wait queue all cluster daemons sleep on
 */
static RPC_WAITQ(flushd_queue, "nfs_flushd");

/*
 * Local function declarations.
 */
static void	nfs_flushd(struct rpc_task *);
static void	nfs_flushd_exit(struct rpc_task *);


static int nfs_reqlist_init(struct nfs_server *server)
{
	struct nfs_reqlist	*cache;
	struct rpc_task		*task;
	int			status;

	dprintk("NFS: writecache_init\n");

	lock_kernel();
	status = -ENOMEM;
	/* Create the RPC task */
	if (!(task = rpc_new_task(server->client, NULL, RPC_TASK_ASYNC)))
		goto out_unlock;

	cache = server->rw_requests;

	status = 0;
	if (cache->task)
		goto out_unlock;

	task->tk_calldata = server;

	cache->task = task;

	/* Run the task */
	cache->runat = jiffies;

	cache->auth = server->client->cl_auth;
	task->tk_action   = nfs_flushd;
	task->tk_exit   = nfs_flushd_exit;

	rpc_execute(task);
	unlock_kernel();
	return 0;
 out_unlock:
	if (task)
		rpc_release_task(task);
	unlock_kernel();
	return status;
}

void nfs_reqlist_exit(struct nfs_server *server)
{
	struct nfs_reqlist      *cache;

	lock_kernel();
	cache = server->rw_requests;
	if (!cache)
		goto out;

	dprintk("NFS: reqlist_exit (ptr %p rpc %p)\n", cache, cache->task);

	while (cache->task) {
		rpc_exit(cache->task, 0);
		rpc_wake_up_task(cache->task);

		interruptible_sleep_on_timeout(&cache->request_wait, 1 * HZ);
	}
 out:
	unlock_kernel();
}

int nfs_reqlist_alloc(struct nfs_server *server)
{
	struct nfs_reqlist	*cache;
	if (server->rw_requests)
		return 0;

	cache = (struct nfs_reqlist *)kmalloc(sizeof(*cache), GFP_KERNEL);
	if (!cache)
		return -ENOMEM;

	memset(cache, 0, sizeof(*cache));
	atomic_set(&cache->nr_requests, 0);
	init_waitqueue_head(&cache->request_wait);
	server->rw_requests = cache;

	return nfs_reqlist_init(server);
}

void nfs_reqlist_free(struct nfs_server *server)
{
	if (server->rw_requests) {
		kfree(server->rw_requests);
		server->rw_requests = NULL;
	}
}

#define NFS_FLUSHD_TIMEOUT	(30*HZ)
static void
nfs_flushd(struct rpc_task *task)
{
	struct nfs_server	*server;
	struct nfs_reqlist	*cache;
	LIST_HEAD(head);

        dprintk("NFS: %4d flushd starting\n", task->tk_pid);
	server = (struct nfs_server *) task->tk_calldata;
        cache = server->rw_requests;

	for(;;) {
		spin_lock(&nfs_wreq_lock);
		if (nfs_scan_lru_dirty_timeout(server, &head)) {
			spin_unlock(&nfs_wreq_lock);
			nfs_flush_list(&head, server->wpages, FLUSH_AGING);
			continue;
		}
		if (nfs_scan_lru_read_timeout(server, &head)) {
			spin_unlock(&nfs_wreq_lock);
			nfs_pagein_list(&head, server->rpages);
			continue;
		}
#ifdef CONFIG_NFS_V3
		if (nfs_scan_lru_commit_timeout(server, &head)) {
			spin_unlock(&nfs_wreq_lock);
			nfs_commit_list(&head, FLUSH_AGING);
			continue;
		}
#endif
		spin_unlock(&nfs_wreq_lock);
		break;
	}

	dprintk("NFS: %4d flushd back to sleep\n", task->tk_pid);
	if (task->tk_action) {
		task->tk_timeout = NFS_FLUSHD_TIMEOUT;
		cache->runat = jiffies + task->tk_timeout;
		rpc_sleep_on(&flushd_queue, task, NULL, NULL);
	}
}

static void
nfs_flushd_exit(struct rpc_task *task)
{
	struct nfs_server	*server;
	struct nfs_reqlist	*cache;
	server = (struct nfs_server *) task->tk_calldata;
	cache = server->rw_requests;

	if (cache->task == task)
		cache->task = NULL;
	wake_up(&cache->request_wait);
}

