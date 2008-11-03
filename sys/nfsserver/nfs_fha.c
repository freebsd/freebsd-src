/*-
 * Copyright (c) 2008 Isilon Inc http://www.isilon.com/
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mbuf.h>
#include <sys/sbuf.h>

#include <rpc/rpc.h>
#include <nfs/xdr_subs.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsserver/nfs.h>
#include <nfsserver/nfsm_subs.h>
#include <nfsserver/nfs_fha.h>

#ifndef NFS_LEGACYRPC

static MALLOC_DEFINE(M_NFS_FHA, "NFS FHA", "NFS FHA");

/* Sysctl defaults. */
#define DEF_BIN_SHIFT		18 /* 256k */
#define DEF_MAX_NFSDS_PER_FH	8
#define DEF_MAX_REQS_PER_NFSD	4

struct fha_ctls {
	u_int32_t bin_shift;
	u_int32_t max_nfsds_per_fh;
	u_int32_t max_reqs_per_nfsd;
} fha_ctls;

struct sysctl_ctx_list fha_clist;

SYSCTL_DECL(_vfs_nfsrv);
SYSCTL_DECL(_vfs_nfsrv_fha);

/* Static sysctl node for the fha from the top-level vfs_nfsrv node. */
SYSCTL_NODE(_vfs_nfsrv, OID_AUTO, fha, CTLFLAG_RD, 0, "fha node");

/* This is the global structure that represents the state of the fha system. */
static struct fha_global {
	struct fha_hash_entry_list *hashtable;
	u_long hashmask;
} g_fha;

/* 
 * These are the entries in the filehandle hash. They talk about a specific 
 * file, requests against which are being handled by one or more nfsds. We keep
 * a chain of nfsds against the file. We only have more than one if reads are 
 * ongoing, and then only if the reads affect disparate regions of the file.
 *
 * In general, we want to assign a new request to an existing nfsd if it is 
 * going to contend with work happening already on that nfsd, or if the 
 * operation is a read and the nfsd is already handling a proximate read. We 
 * do this to avoid jumping around in the read stream unnecessarily, and to 
 * avoid contention between threads over single files.
 */
struct fha_hash_entry {
	LIST_ENTRY(fha_hash_entry) link;
	u_int64_t fh;
	u_int16_t num_reads;
	u_int16_t num_writes;
	u_int8_t num_threads;
	struct svcthread_list threads;
};
LIST_HEAD(fha_hash_entry_list, fha_hash_entry);

/* A structure used for passing around data internally. */
struct fha_info {
	u_int64_t fh;
	off_t offset;
	int locktype;
};

static int fhe_stats_sysctl(SYSCTL_HANDLER_ARGS);
 
static void
nfs_fha_init(void *foo)
{

	/*
	 * A small hash table to map filehandles to fha_hash_entry
	 * structures.
	 */
	g_fha.hashtable = hashinit(256, M_NFS_FHA, &g_fha.hashmask);

	/*
	 * Initialize the sysctl context list for the fha module.
	 */
	sysctl_ctx_init(&fha_clist);

	fha_ctls.bin_shift = DEF_BIN_SHIFT;
	fha_ctls.max_nfsds_per_fh = DEF_MAX_NFSDS_PER_FH;
	fha_ctls.max_reqs_per_nfsd = DEF_MAX_REQS_PER_NFSD;

	SYSCTL_ADD_UINT(&fha_clist, SYSCTL_STATIC_CHILDREN(_vfs_nfsrv_fha),
	    OID_AUTO, "bin_shift", CTLFLAG_RW,
	    &fha_ctls.bin_shift, 0, "For FHA reads, no two requests will "
	    "contend if they're 2^(bin_shift) bytes apart");

	SYSCTL_ADD_UINT(&fha_clist, SYSCTL_STATIC_CHILDREN(_vfs_nfsrv_fha),
	    OID_AUTO, "max_nfsds_per_fh", CTLFLAG_RW,
	    &fha_ctls.max_nfsds_per_fh, 0, "Maximum nfsd threads that "
	    "should be working on requests for the same file handle");

	SYSCTL_ADD_UINT(&fha_clist, SYSCTL_STATIC_CHILDREN(_vfs_nfsrv_fha),
	    OID_AUTO, "max_reqs_per_nfsd", CTLFLAG_RW,
	    &fha_ctls.max_reqs_per_nfsd, 0, "Maximum requests that "
	    "single nfsd thread should be working on at any time");

	SYSCTL_ADD_OID(&fha_clist, SYSCTL_STATIC_CHILDREN(_vfs_nfsrv_fha), 
	    OID_AUTO, "fhe_stats", CTLTYPE_STRING | CTLFLAG_RD, 0, 0,
	    fhe_stats_sysctl, "A", "");
}

static void
nfs_fha_uninit(void *foo)
{

	hashdestroy(g_fha.hashtable, M_NFS_FHA, g_fha.hashmask);
}

SYSINIT(nfs_fha, SI_SUB_ROOT_CONF, SI_ORDER_ANY, nfs_fha_init, NULL);
SYSUNINIT(nfs_fha, SI_SUB_ROOT_CONF, SI_ORDER_ANY, nfs_fha_uninit, NULL);

/* 
 * This just specifies that offsets should obey affinity when within
 * the same 1Mbyte (1<<20) chunk for the file (reads only for now).
 */
static void
fha_extract_info(struct svc_req *req, struct fha_info *i)
{
	struct mbuf *md = req->rq_args;
	fhandle_t fh;
	caddr_t dpos = mtod(md, caddr_t);
	static u_int64_t random_fh = 0;
	int error;
	int v3 = (req->rq_vers == 3);
	u_int32_t *tl;
	rpcproc_t procnum;

	/* 
	 * We start off with a random fh. If we get a reasonable
	 * procnum, we set the fh. If there's a concept of offset 
	 * that we're interested in, we set that.
	 */
	i->fh = ++random_fh;
	i->offset = 0;
	i->locktype = LK_EXCLUSIVE;
	
	/*
	 * Extract the procnum and convert to v3 form if necessary.
	 */
	procnum = req->rq_proc;
	if (!v3)
		procnum = nfsrv_nfsv3_procid[procnum];

	/* 
	 * We do affinity for most. However, we divide a realm of affinity 
	 * by file offset so as to allow for concurrent random access. We 
	 * only do this for reads today, but this may change when IFS supports 
	 * efficient concurrent writes.
	 */
	if (procnum == NFSPROC_FSSTAT ||
	    procnum == NFSPROC_FSINFO ||
	    procnum == NFSPROC_PATHCONF ||
	    procnum == NFSPROC_NOOP || 
	    procnum == NFSPROC_NULL)
		goto out;
	
	/* Grab the filehandle. */
	error = nfsm_srvmtofh_xx(&fh, v3, &md, &dpos);
	if (error)
		goto out;

	i->fh = *(const u_int64_t *)(fh.fh_fid.fid_data);

	/* Content ourselves with zero offset for all but reads. */
	if (procnum != NFSPROC_READ)
		goto out;

	if (v3) {
		tl = nfsm_dissect_xx_nonblock(2 * NFSX_UNSIGNED, &md, &dpos);
		if (tl == NULL)
			goto out;
		i->offset = fxdr_hyper(tl);
	} else {
		tl = nfsm_dissect_xx_nonblock(NFSX_UNSIGNED, &md, &dpos);
		if (tl == NULL)
			goto out;
		i->offset = fxdr_unsigned(u_int32_t, *tl);
	}
 out:
	switch (procnum) {
	case NFSPROC_NULL:
	case NFSPROC_GETATTR:
	case NFSPROC_LOOKUP:
	case NFSPROC_ACCESS:
	case NFSPROC_READLINK:
	case NFSPROC_READ:
	case NFSPROC_READDIR:
	case NFSPROC_READDIRPLUS:
		i->locktype = LK_SHARED;
		break;
	case NFSPROC_SETATTR:
	case NFSPROC_WRITE:
	case NFSPROC_CREATE:
	case NFSPROC_MKDIR:
	case NFSPROC_SYMLINK:
	case NFSPROC_MKNOD:
	case NFSPROC_REMOVE:
	case NFSPROC_RMDIR:
	case NFSPROC_RENAME:
	case NFSPROC_LINK:
	case NFSPROC_FSSTAT:
	case NFSPROC_FSINFO:
	case NFSPROC_PATHCONF:
	case NFSPROC_COMMIT:
	case NFSPROC_NOOP:
		i->locktype = LK_EXCLUSIVE;
		break;
	}
}

static struct fha_hash_entry *
fha_hash_entry_new(u_int64_t fh)
{
	struct fha_hash_entry *e;

	e = malloc(sizeof(*e), M_NFS_FHA, M_WAITOK);
	e->fh = fh;
	e->num_reads = 0;
	e->num_writes = 0;
	e->num_threads = 0;
	LIST_INIT(&e->threads);
	
	return e;
}

static void
fha_hash_entry_destroy(struct fha_hash_entry *e)
{

	if (e->num_reads + e->num_writes)
		panic("nonempty fhe");
	free(e, M_NFS_FHA);
}

static void
fha_hash_entry_remove(struct fha_hash_entry *e)
{

	LIST_REMOVE(e, link);
	fha_hash_entry_destroy(e);
}

static struct fha_hash_entry *
fha_hash_entry_lookup(SVCPOOL *pool, u_int64_t fh)
{
	struct fha_hash_entry *fhe, *new_fhe;

	LIST_FOREACH(fhe, &g_fha.hashtable[fh % g_fha.hashmask], link) {
		if (fhe->fh == fh)
			break;
	}

	if (!fhe) {
		/* Allocate a new entry. */
		mtx_unlock(&pool->sp_lock);
		new_fhe = fha_hash_entry_new(fh);
		mtx_lock(&pool->sp_lock);

		/* Double-check to make sure we still need the new entry. */
		LIST_FOREACH(fhe, &g_fha.hashtable[fh % g_fha.hashmask], link) {
			if (fhe->fh == fh)
				break;
		}
		if (!fhe) {
			fhe = new_fhe;
			LIST_INSERT_HEAD(&g_fha.hashtable[fh % g_fha.hashmask],
			    fhe, link);
		} else {
			fha_hash_entry_destroy(new_fhe);
		}
	}

	return fhe;
}

static void
fha_hash_entry_add_thread(struct fha_hash_entry *fhe, SVCTHREAD *thread)
{
	LIST_INSERT_HEAD(&fhe->threads, thread, st_alink);
	fhe->num_threads++;
}

static void
fha_hash_entry_remove_thread(struct fha_hash_entry *fhe, SVCTHREAD *thread)
{

	LIST_REMOVE(thread, st_alink);
	fhe->num_threads--;
}

/* 
 * Account for an ongoing operation associated with this file.
 */
static void
fha_hash_entry_add_op(struct fha_hash_entry *fhe, int locktype, int count)
{

	if (LK_EXCLUSIVE == locktype)
		fhe->num_writes += count;
	else
		fhe->num_reads += count;
}

static SVCTHREAD *
get_idle_thread(SVCPOOL *pool)
{
	SVCTHREAD *st;

	LIST_FOREACH(st, &pool->sp_idlethreads, st_ilink) {
		if (st->st_xprt == NULL && STAILQ_EMPTY(&st->st_reqs))
			return (st);
	}
	return (NULL);
}


/* 
 * Get the service thread currently associated with the fhe that is
 * appropriate to handle this operation.
 */
SVCTHREAD *
fha_hash_entry_choose_thread(SVCPOOL *pool, struct fha_hash_entry *fhe,
    struct fha_info *i, SVCTHREAD *this_thread);

SVCTHREAD *
fha_hash_entry_choose_thread(SVCPOOL *pool, struct fha_hash_entry *fhe,
    struct fha_info *i, SVCTHREAD *this_thread)
{
	SVCTHREAD *thread, *min_thread = NULL;
	int req_count, min_count = 0;
	off_t offset1, offset2;

	LIST_FOREACH(thread, &fhe->threads, st_alink) {
		req_count = thread->st_reqcount;

		/* If there are any writes in progress, use the first thread. */
		if (fhe->num_writes) {
#if 0
			ITRACE_CURPROC(ITRACE_NFS, ITRACE_INFO, 
			    "fha: %p(%d)w", thread, req_count);
#endif
			return (thread);
		}

		/* 
		 * Check for read locality, making sure that we won't 
		 * exceed our per-thread load limit in the process. 
		 */
		offset1 = i->offset >> fha_ctls.bin_shift;
		offset2 = STAILQ_FIRST(&thread->st_reqs)->rq_p3
			>> fha_ctls.bin_shift;
		if (offset1 == offset2) {
			if ((fha_ctls.max_reqs_per_nfsd == 0) ||
			    (req_count < fha_ctls.max_reqs_per_nfsd)) {
#if 0
				ITRACE_CURPROC(ITRACE_NFS, ITRACE_INFO, 
				    "fha: %p(%d)r", thread, req_count);
#endif
				return (thread);
			}
		}

		/* 
		 * We don't have a locality match, so skip this thread,
		 * but keep track of the most attractive thread in case 
		 * we need to come back to it later.
		 */
#if 0
		ITRACE_CURPROC(ITRACE_NFS, ITRACE_INFO, 
		    "fha: %p(%d)s off1 %llu off2 %llu", thread, 
		    req_count, offset1, offset2);
#endif
		if ((min_thread == NULL) || (req_count < min_count)) {
			min_count = req_count;
			min_thread = thread;
		}
	}

	/* 
	 * We didn't find a good match yet. See if we can add 
	 * a new thread to this file handle entry's thread list.
	 */
	if ((fha_ctls.max_nfsds_per_fh == 0) || 
	    (fhe->num_threads < fha_ctls.max_nfsds_per_fh)) {
		/* 
		 * We can add a new thread, so try for an idle thread 
		 * first, and fall back to this_thread if none are idle. 
		 */
		if (STAILQ_EMPTY(&this_thread->st_reqs)) {
			thread = this_thread;
#if 0
			ITRACE_CURPROC(ITRACE_NFS, ITRACE_INFO, 
			    "fha: %p(%d)t", thread, thread->st_reqcount);
#endif
		} else if ((thread = get_idle_thread(pool))) {
#if 0
			ITRACE_CURPROC(ITRACE_NFS, ITRACE_INFO, 
			    "fha: %p(%d)i", thread, thread->st_reqcount);
#endif
		} else { 
			thread = this_thread;
#if 0
			ITRACE_CURPROC(ITRACE_NFS, ITRACE_INFO, 
			    "fha: %p(%d)b", thread, thread->st_reqcount);
#endif
		}
		fha_hash_entry_add_thread(fhe, thread);
	} else {
		/* 
		 * We don't want to use any more threads for this file, so 
		 * go back to the most attractive nfsd we're already using.
		 */
		thread = min_thread;
	}

	return (thread);
}

/* 
 * After getting a request, try to assign it to some thread. Usually we
 * handle it ourselves.
 */
SVCTHREAD *
fha_assign(SVCTHREAD *this_thread, struct svc_req *req)
{
	SVCPOOL *pool;
	SVCTHREAD *thread;
	struct fha_info i;
	struct fha_hash_entry *fhe;

	/*
	 * Only do placement if this is an NFS request.
	 */
	if (req->rq_prog != NFS_PROG)
		return (this_thread);

	if (req->rq_vers != 2 && req->rq_vers != 3)
		return (this_thread);

	pool = req->rq_xprt->xp_pool;
	fha_extract_info(req, &i);

	/* 
	 * We save the offset associated with this request for later 
	 * nfsd matching.
	 */
	fhe = fha_hash_entry_lookup(pool, i.fh);
	req->rq_p1 = fhe;
	req->rq_p2 = i.locktype;
	req->rq_p3 = i.offset;
	
	/* 
	 * Choose a thread, taking into consideration locality, thread load,
	 * and the number of threads already working on this file.
	 */
	thread = fha_hash_entry_choose_thread(pool, fhe, &i, this_thread);
	KASSERT(thread, ("fha_assign: NULL thread!"));
	fha_hash_entry_add_op(fhe, i.locktype, 1);

	return (thread);
}

/* 
 * Called when we're done with an operation. The request has already
 * been de-queued.
 */
void
fha_nd_complete(SVCTHREAD *thread, struct svc_req *req)
{
	struct fha_hash_entry *fhe = req->rq_p1;

	/*
	 * This may be called for reqs that didn't go through
	 * fha_assign (e.g. extra NULL ops used for RPCSEC_GSS.
	 */
	if (!fhe)
		return;

	fha_hash_entry_add_op(fhe, req->rq_p2, -1);

	if (thread->st_reqcount == 0) {
		fha_hash_entry_remove_thread(fhe, thread);
		if (0 == fhe->num_reads + fhe->num_writes)
			fha_hash_entry_remove(fhe);
	}
}

extern SVCPOOL *nfsrv_pool;

static int
fhe_stats_sysctl(SYSCTL_HANDLER_ARGS)
{
	int error, count, i;
	struct sbuf sb;
	struct fha_hash_entry *fhe;
	bool_t first = TRUE;
	SVCTHREAD *thread;

	sbuf_new(&sb, NULL, 4096, SBUF_FIXEDLEN);

	if (!nfsrv_pool) {
		sbuf_printf(&sb, "NFSD not running\n");
		goto out;
	}

	mtx_lock(&nfsrv_pool->sp_lock);
	count = 0;
	for (i = 0; i <= g_fha.hashmask; i++)
		if (!LIST_EMPTY(&g_fha.hashtable[i]))
			count++;

	if (count == 0) {
		sbuf_printf(&sb, "No file handle entries.\n");
		goto out;
	}

	for (i = 0; i <= g_fha.hashmask; i++) {
		LIST_FOREACH(fhe, &g_fha.hashtable[i], link) {
			sbuf_printf(&sb, "%sfhe %p: {\n", first ? "" : ", ", fhe);

			sbuf_printf(&sb, "    fh: %ju\n", (uintmax_t) fhe->fh);
			sbuf_printf(&sb, "    num_reads: %d\n", fhe->num_reads);
			sbuf_printf(&sb, "    num_writes: %d\n", fhe->num_writes);
			sbuf_printf(&sb, "    num_threads: %d\n", fhe->num_threads);

			LIST_FOREACH(thread, &fhe->threads, st_alink) {
				sbuf_printf(&sb, "    thread %p (count %d)\n",
				    thread, thread->st_reqcount);
			}

			sbuf_printf(&sb, "}");
			first = FALSE;

			/* Limit the output. */
			if (++count > 128) {
				sbuf_printf(&sb, "...");
				break;
			}
		}
	}

 out:
	if (nfsrv_pool)
		mtx_unlock(&nfsrv_pool->sp_lock);
	sbuf_trim(&sb);
	sbuf_finish(&sb);
	error = sysctl_handle_string(oidp, sbuf_data(&sb), sbuf_len(&sb), req);
	sbuf_delete(&sb);
	return (error);
}

#endif /* !NFS_LEGACYRPC */
