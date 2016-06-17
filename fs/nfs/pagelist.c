/*
 * linux/fs/nfs/pagelist.c
 *
 * A set of helper functions for managing NFS read and write requests.
 * The main purpose of these routines is to provide support for the
 * coalescing of several requests into a single RPC call.
 *
 * Copyright 2000, 2001 (c) Trond Myklebust <trond.myklebust@fys.uio.no>
 *
 */

#include <linux/config.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/sunrpc/clnt.h>
#include <linux/nfs3.h>
#include <linux/nfs_page.h>
#include <linux/nfs_fs.h>
#include <linux/nfs_flushd.h>
#include <linux/nfs_mount.h>

#define NFS_PARANOIA 1

/*
 * Spinlock
 */
spinlock_t nfs_wreq_lock = SPIN_LOCK_UNLOCKED;

static kmem_cache_t *nfs_page_cachep;

static inline struct nfs_page *
nfs_page_alloc(void)
{
	struct nfs_page	*p;
	p = kmem_cache_alloc(nfs_page_cachep, SLAB_NOFS);
	if (p) {
		memset(p, 0, sizeof(*p));
		INIT_LIST_HEAD(&p->wb_hash);
		INIT_LIST_HEAD(&p->wb_list);
		INIT_LIST_HEAD(&p->wb_lru);
		init_waitqueue_head(&p->wb_wait);
	}
	return p;
}

static inline void
nfs_page_free(struct nfs_page *p)
{
	kmem_cache_free(nfs_page_cachep, p);
}

static int nfs_try_to_free_pages(struct nfs_server *);

/**
 * nfs_create_request - Create an NFS read/write request.
 * @cred: RPC credential to use
 * @inode: inode to which the request is attached
 * @page: page to write
 * @offset: starting offset within the page for the write
 * @count: number of bytes to read/write
 *
 * The page must be locked by the caller. This makes sure we never
 * create two different requests for the same page, and avoids
 * a possible deadlock when we reach the hard limit on the number
 * of dirty pages.
 * User should ensure it is safe to sleep in this function.
 */
struct nfs_page *
nfs_create_request(struct rpc_cred *cred, struct inode *inode,
		   struct page *page,
		   unsigned int offset, unsigned int count)
{
	struct nfs_server *server = NFS_SERVER(inode);
	struct nfs_reqlist	*cache = NFS_REQUESTLIST(inode);
	struct nfs_page		*req;

	/* Deal with hard limits.  */
	for (;;) {
		/* Prevent races by incrementing *before* we test */
		atomic_inc(&cache->nr_requests);

		/* If we haven't reached the local hard limit yet,
		 * try to allocate the request struct */
		if (atomic_read(&cache->nr_requests) <= MAX_REQUEST_HARD) {
			req = nfs_page_alloc();
			if (req != NULL)
				break;
		}

		atomic_dec(&cache->nr_requests);

		/* Try to free up at least one request in order to stay
		 * below the hard limit
		 */
		if (nfs_try_to_free_pages(server))
			continue;
		if (signalled() && (server->flags & NFS_MOUNT_INTR))
			return ERR_PTR(-ERESTARTSYS);
		yield();
	}

	/* Initialize the request struct. Initially, we assume a
	 * long write-back delay. This will be adjusted in
	 * update_nfs_request below if the region is not locked. */
	req->wb_page    = page;
	page_cache_get(page);
	req->wb_offset  = offset;
	req->wb_bytes   = count;

	if (cred)
		req->wb_cred = get_rpccred(cred);
	req->wb_inode   = inode;
	req->wb_count   = 1;

	return req;
}

/**
 * nfs_clear_request - Free up all resources allocated to the request
 * @req:
 *
 * Release all resources associated with a write request after it
 * has completed.
 */
void nfs_clear_request(struct nfs_page *req)
{
	/* Release struct file or cached credential */
	if (req->wb_file) {
		fput(req->wb_file);
		req->wb_file = NULL;
	}
	if (req->wb_cred) {
		put_rpccred(req->wb_cred);
		req->wb_cred = NULL;
	}
	if (req->wb_page) {
		atomic_dec(&NFS_REQUESTLIST(req->wb_inode)->nr_requests);
#ifdef NFS_PARANOIA
		BUG_ON(atomic_read(&NFS_REQUESTLIST(req->wb_inode)->nr_requests) < 0);
#endif
		page_cache_release(req->wb_page);
		req->wb_page = NULL;
	}
}


/**
 * nfs_release_request - Release the count on an NFS read/write request
 * @req: request to release
 *
 * Note: Should never be called with the spinlock held!
 */
void
nfs_release_request(struct nfs_page *req)
{
	spin_lock(&nfs_wreq_lock);
	if (--req->wb_count) {
		spin_unlock(&nfs_wreq_lock);
		return;
	}
	__nfs_del_lru(req);
	spin_unlock(&nfs_wreq_lock);

#ifdef NFS_PARANOIA
	BUG_ON(!list_empty(&req->wb_list));
	BUG_ON(!list_empty(&req->wb_hash));
	BUG_ON(NFS_WBACK_BUSY(req));
#endif

	/* Release struct file or cached credential */
	nfs_clear_request(req);
	nfs_page_free(req);
}

/**
 * nfs_list_add_request - Insert a request into a sorted list
 * @req: request
 * @head: head of list into which to insert the request.
 *
 * Note that the wb_list is sorted by page index in order to facilitate
 * coalescing of requests.
 * We use an insertion sort that is optimized for the case of appended
 * writes.
 */
void
nfs_list_add_request(struct nfs_page *req, struct list_head *head)
{
	struct list_head *pos;
	unsigned long pg_idx = page_index(req->wb_page);

#ifdef NFS_PARANOIA
	if (!list_empty(&req->wb_list)) {
		printk(KERN_ERR "NFS: Add to list failed!\n");
		BUG();
	}
#endif
	list_for_each_prev(pos, head) {
		struct nfs_page	*p = nfs_list_entry(pos);
		if (page_index(p->wb_page) < pg_idx)
			break;
	}
	list_add(&req->wb_list, pos);
	req->wb_list_head = head;
}

/**
 * nfs_wait_on_request - Wait for a request to complete.
 * @req: request to wait upon.
 *
 * Interruptible by signals only if mounted with intr flag.
 * The user is responsible for holding a count on the request.
 */
int
nfs_wait_on_request(struct nfs_page *req)
{
	struct inode	*inode = req->wb_inode;
        struct rpc_clnt	*clnt = NFS_CLIENT(inode);

	if (!NFS_WBACK_BUSY(req))
		return 0;
	return nfs_wait_event(clnt, req->wb_wait, !NFS_WBACK_BUSY(req));
}

/**
 * nfs_coalesce_requests - Split coalesced requests out from a list.
 * @head: source list
 * @dst: destination list
 * @nmax: maximum number of requests to coalesce
 *
 * Moves a maximum of 'nmax' elements from one list to another.
 * The elements are checked to ensure that they form a contiguous set
 * of pages, and that they originated from the same file.
 */
int
nfs_coalesce_requests(struct list_head *head, struct list_head *dst,
		      unsigned int nmax)
{
	struct nfs_page		*req = NULL;
	unsigned int		npages = 0;

	while (!list_empty(head)) {
		struct nfs_page	*prev = req;

		req = nfs_list_entry(head->next);
		if (prev) {
			if (req->wb_cred != prev->wb_cred)
				break;
			if (page_index(req->wb_page) != page_index(prev->wb_page)+1)
				break;

			if (req->wb_offset != 0)
				break;
		}
		nfs_list_remove_request(req);
		nfs_list_add_request(req, dst);
		npages++;
		if (req->wb_offset + req->wb_bytes != PAGE_CACHE_SIZE)
			break;
		if (npages >= nmax)
			break;
	}
	return npages;
}

/*
 * nfs_scan_forward - Coalesce more requests
 * @req: First request to add
 * @dst: destination list
 * @nmax: maximum number of requests to coalesce
 *
 * Tries to coalesce more requests by traversing the request's wb_list.
 * Moves the resulting list into dst. Requests are guaranteed to be
 * contiguous, and to originate from the same file.
 */
static int
nfs_scan_forward(struct nfs_page *req, struct list_head *dst, int nmax)
{
	struct nfs_server *server = NFS_SERVER(req->wb_inode);
	struct list_head *pos, *head = req->wb_list_head;
	struct rpc_cred *cred = req->wb_cred;
	unsigned long idx = page_index(req->wb_page) + 1;
	int npages = 0;

	for (pos = req->wb_list.next; nfs_lock_request(req); pos = pos->next) {
		nfs_list_remove_request(req);
		nfs_list_add_request(req, dst);
		__nfs_del_lru(req);
		__nfs_add_lru(&server->lru_busy, req);
		npages++;
		if (npages == nmax)
			break;
		if (pos == head)
			break;
		if (req->wb_offset + req->wb_bytes != PAGE_CACHE_SIZE)
			break;
		req = nfs_list_entry(pos);
		if (page_index(req->wb_page) != idx++)
			break;
		if (req->wb_offset != 0)
			break;
		if (req->wb_cred != cred)
			break;
	}
	return npages;
}

/**
 * nfs_scan_lru - Scan one of the least recently used list
 * @head: One of the NFS superblock lru lists
 * @dst: Destination list
 * @nmax: maximum number of requests to coalesce
 *
 * Scans one of the NFS superblock lru lists for upto nmax requests
 * and returns them on a list. The requests are all guaranteed to be
 * contiguous, originating from the same inode and the same file.
 */
int
nfs_scan_lru(struct list_head *head, struct list_head *dst, int nmax)
{
	struct list_head *pos;
	struct nfs_page *req;
	int npages = 0;

	list_for_each(pos, head) {
		req = nfs_lru_entry(pos);
		npages = nfs_scan_forward(req, dst, nmax);
		if (npages)
			break;
	}
	return npages;
}

/**
 * nfs_scan_lru_timeout - Scan one of the superblock lru lists for timed out requests
 * @head: One of the NFS superblock lru lists
 * @dst: Destination list
 * @nmax: maximum number of requests to coalesce
 *
 * Scans one of the NFS superblock lru lists for upto nmax requests
 * and returns them on a list. The requests are all guaranteed to be
 * contiguous, originating from the same inode and the same file.
 * The first request on the destination list will be timed out, the
 * others are not guaranteed to be so.
 */
int
nfs_scan_lru_timeout(struct list_head *head, struct list_head *dst, int nmax)
{
	struct list_head *pos;
	struct nfs_page *req;
	int npages = 0;

	list_for_each(pos, head) {
		req = nfs_lru_entry(pos);
		if (time_after(req->wb_timeout, jiffies))
			break;
		npages = nfs_scan_forward(req, dst, nmax);
		if (npages)
			break;
	}
	return npages;
}

/**
 * nfs_scan_list - Scan a list for matching requests
 * @head: One of the NFS inode request lists
 * @dst: Destination list
 * @idx_start: lower bound of page->index to scan
 * @npages: idx_start + npages sets the upper bound to scan.
 *
 * Moves elements from one of the inode request lists.
 * If the number of requests is set to 0, the entire address_space
 * starting at index idx_start, is scanned.
 * The requests are *not* checked to ensure that they form a contiguous set.
 * You must be holding the nfs_wreq_lock when calling this function
 */
int
nfs_scan_list(struct list_head *head, struct list_head *dst,
	      unsigned long idx_start, unsigned int npages)
{
	struct list_head	*pos, *tmp;
	struct nfs_page		*req;
	unsigned long		idx_end;
	int			res;

	res = 0;
	if (npages == 0)
		idx_end = ~0;
	else
		idx_end = idx_start + npages - 1;

	list_for_each_safe(pos, tmp, head) {
		unsigned long pg_idx;

		req = nfs_list_entry(pos);

		pg_idx = page_index(req->wb_page);
		if (pg_idx < idx_start)
			continue;
		if (pg_idx > idx_end)
			break;

		if (!nfs_lock_request(req))
			continue;
		nfs_list_remove_request(req);
		nfs_list_add_request(req, dst);
		__nfs_del_lru(req);
		__nfs_add_lru(&NFS_SERVER(req->wb_inode)->lru_busy, req);
		res++;
	}
	return res;
}

/*
 * nfs_try_to_free_pages - Free up NFS read/write requests
 * @server: The NFS superblock
 *
 * This function attempts to flush out NFS reads and writes in order
 * to keep the hard limit on the total number of pending requests
 * on a given NFS partition.
 * Note: we first try to commit unstable writes, then flush out pending
 *       reads, then finally the dirty pages.
 *       The assumption is that this reflects the ordering from the fastest
 *       to the slowest method for reclaiming requests.
 */
static int
nfs_try_to_free_pages(struct nfs_server *server)
{
	LIST_HEAD(head);
	struct nfs_page *req = NULL;
	int nreq;

	for (;;) {
		if (req) {
			int status = nfs_wait_on_request(req);
			nfs_release_request(req);
			if (status)
				break;
			req = NULL;
		}
		nreq = atomic_read(&server->rw_requests->nr_requests);
		if (nreq < MAX_REQUEST_HARD)
			return 1;
		spin_lock(&nfs_wreq_lock);
		/* Are there any busy RPC calls that might free up requests? */
		if (!list_empty(&server->lru_busy)) {
			req = nfs_lru_entry(server->lru_busy.next);
			req->wb_count++;
			__nfs_del_lru(req);
			spin_unlock(&nfs_wreq_lock);
			continue;
		}

#ifdef CONFIG_NFS_V3
		/* Let's try to free up some completed NFSv3 unstable writes */
		nfs_scan_lru_commit(server, &head);
		if (!list_empty(&head)) {
			spin_unlock(&nfs_wreq_lock);
			nfs_commit_list(&head, 0);
			continue;
		}
#endif
		/* OK, so we try to free up some pending readaheads */
		nfs_scan_lru_read(server, &head);
		if (!list_empty(&head)) {
			spin_unlock(&nfs_wreq_lock);
			nfs_pagein_list(&head, server->rpages);
			continue;
		}
		/* Last resort: we try to flush out single requests */
		nfs_scan_lru_dirty(server, &head);
		if (!list_empty(&head)) {
			spin_unlock(&nfs_wreq_lock);
			nfs_flush_list(&head, server->wpages, FLUSH_STABLE);
			continue;
		}
		spin_unlock(&nfs_wreq_lock);
		break;
	}
	/* We failed to free up requests */
	return 0;
}

int nfs_init_nfspagecache(void)
{
	nfs_page_cachep = kmem_cache_create("nfs_page",
					    sizeof(struct nfs_page),
					    0, SLAB_HWCACHE_ALIGN,
					    NULL, NULL);
	if (nfs_page_cachep == NULL)
		return -ENOMEM;

	return 0;
}

void nfs_destroy_nfspagecache(void)
{
	if (kmem_cache_destroy(nfs_page_cachep))
		printk(KERN_INFO "nfs_page: not all structures were freed\n");
}

