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
#include <nfs/nfs_fha.h>

static MALLOC_DEFINE(M_NFS_FHA, "NFS FHA", "NFS FHA");

/*
 * XXX need to commonize definitions between old and new NFS code.  Define
 * this here so we don't include one nfsproto.h over the other.
 */
#define	NFS_PROG		100003

void
fha_init(struct fha_params *softc)
{
	char tmpstr[128];

	/*
	 * A small hash table to map filehandles to fha_hash_entry
	 * structures.
	 */
	softc->g_fha.hashtable = hashinit(256, M_NFS_FHA,
	    &softc->g_fha.hashmask);

	/*
	 * Set the default tuning parameters.
	 */
	softc->ctls.enable = FHA_DEF_ENABLE;
	softc->ctls.bin_shift = FHA_DEF_BIN_SHIFT;
	softc->ctls.max_nfsds_per_fh = FHA_DEF_MAX_NFSDS_PER_FH;
	softc->ctls.max_reqs_per_nfsd = FHA_DEF_MAX_REQS_PER_NFSD;

	/*
	 * Allow the user to override the defaults at boot time with
	 * tunables.
	 */
	snprintf(tmpstr, sizeof(tmpstr), "vfs.%s.fha.enable",
	    softc->server_name);
	TUNABLE_INT_FETCH(tmpstr, &softc->ctls.enable);
	snprintf(tmpstr, sizeof(tmpstr), "vfs.%s.fha.bin_shift",
	    softc->server_name);
	TUNABLE_INT_FETCH(tmpstr, &softc->ctls.bin_shift);
	snprintf(tmpstr, sizeof(tmpstr), "vfs.%s.fha.max_nfsds_per_fh",
	    softc->server_name);
	TUNABLE_INT_FETCH(tmpstr, &softc->ctls.max_nfsds_per_fh);
	snprintf(tmpstr, sizeof(tmpstr), "vfs.%s.fha.max_reqs_per_nfsd",
	    softc->server_name);
	TUNABLE_INT_FETCH(tmpstr, &softc->ctls.max_reqs_per_nfsd);

	/*
	 * Add sysctls so the user can change the tuning parameters at
	 * runtime.
	 */
	SYSCTL_ADD_UINT(&softc->sysctl_ctx, SYSCTL_CHILDREN(softc->sysctl_tree),
	    OID_AUTO, "enable", CTLFLAG_RW,
	    &softc->ctls.enable, 0, "Enable NFS File Handle Affinity (FHA)");

	SYSCTL_ADD_UINT(&softc->sysctl_ctx, SYSCTL_CHILDREN(softc->sysctl_tree),
	    OID_AUTO, "bin_shift", CTLFLAG_RW,
	    &softc->ctls.bin_shift, 0, "For FHA reads, no two requests will "
	    "contend if they're 2^(bin_shift) bytes apart");

	SYSCTL_ADD_UINT(&softc->sysctl_ctx, SYSCTL_CHILDREN(softc->sysctl_tree),
	    OID_AUTO, "max_nfsds_per_fh", CTLFLAG_RW,
	    &softc->ctls.max_nfsds_per_fh, 0, "Maximum nfsd threads that "
	    "should be working on requests for the same file handle");

	SYSCTL_ADD_UINT(&softc->sysctl_ctx, SYSCTL_CHILDREN(softc->sysctl_tree),
	    OID_AUTO, "max_reqs_per_nfsd", CTLFLAG_RW,
	    &softc->ctls.max_reqs_per_nfsd, 0, "Maximum requests that "
	    "single nfsd thread should be working on at any time");

	SYSCTL_ADD_OID(&softc->sysctl_ctx, SYSCTL_CHILDREN(softc->sysctl_tree),
	    OID_AUTO, "fhe_stats", CTLTYPE_STRING | CTLFLAG_RD, 0, 0,
	    softc->callbacks.fhe_stats_sysctl, "A", "");

}

void
fha_uninit(struct fha_params *softc)
{
	sysctl_ctx_free(&softc->sysctl_ctx);
	hashdestroy(softc->g_fha.hashtable, M_NFS_FHA, softc->g_fha.hashmask);
}

/*
 * This just specifies that offsets should obey affinity when within
 * the same 1Mbyte (1<<20) chunk for the file (reads only for now).
 */
static void
fha_extract_info(struct svc_req *req, struct fha_info *i,
    struct fha_callbacks *cb)
{
	struct mbuf *md;
	fhandle_t fh;
	caddr_t dpos;
	static u_int64_t random_fh = 0;
	int error;
	int v3 = (req->rq_vers == 3);
	rpcproc_t procnum;

	/*
	 * We start off with a random fh.  If we get a reasonable
	 * procnum, we set the fh.  If there's a concept of offset
	 * that we're interested in, we set that.
	 */
	i->fh = ++random_fh;
	i->offset = 0;
	i->locktype = LK_EXCLUSIVE;

	/*
	 * Extract the procnum and convert to v3 form if necessary,
	 * taking care to deal with out-of-range procnums.  Caller will
	 * ensure that rq_vers is either 2 or 3.
	 */
	procnum = req->rq_proc;
	if (!v3) {
		rpcproc_t tmp_procnum;

		tmp_procnum = cb->get_procnum(procnum);
		if (tmp_procnum == -1)
			goto out;
		procnum = tmp_procnum;
	}

	/*
	 * We do affinity for most.  However, we divide a realm of affinity
	 * by file offset so as to allow for concurrent random access.  We
	 * only do this for reads today, but this may change when IFS supports
	 * efficient concurrent writes.
	 */
	if (cb->no_offset(procnum))
		goto out;

	error = cb->realign(&req->rq_args, M_NOWAIT);
	if (error)
		goto out;
	md = req->rq_args;
	dpos = mtod(md, caddr_t);

	/* Grab the filehandle. */
	error = cb->get_fh(&fh, v3, &md, &dpos);
	if (error)
		goto out;

	bcopy(fh.fh_fid.fid_data, &i->fh, sizeof(i->fh));

	/* Content ourselves with zero offset for all but reads. */
	if (cb->is_read(procnum) || cb->is_write(procnum))
		cb->get_offset(&md, &dpos, v3, i);

out:
	cb->set_locktype(procnum, i);
}

static struct fha_hash_entry *
fha_hash_entry_new(u_int64_t fh)
{
	struct fha_hash_entry *e;

	e = malloc(sizeof(*e), M_NFS_FHA, M_WAITOK);
	e->fh = fh;
	e->num_rw = 0;
	e->num_exclusive = 0;
	e->num_threads = 0;
	LIST_INIT(&e->threads);

	return (e);
}

static void
fha_hash_entry_destroy(struct fha_hash_entry *e)
{

	if (e->num_rw + e->num_exclusive)
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
fha_hash_entry_lookup(struct fha_params *softc, u_int64_t fh)
{
	SVCPOOL *pool;

	pool = *softc->pool;

	struct fha_hash_entry *fhe, *new_fhe;

	LIST_FOREACH(fhe, &softc->g_fha.hashtable[fh % softc->g_fha.hashmask],
	    link)
		if (fhe->fh == fh)
			break;

	if (!fhe) {
		/* Allocate a new entry. */
		mtx_unlock(&pool->sp_lock);
		new_fhe = fha_hash_entry_new(fh);
		mtx_lock(&pool->sp_lock);

		/* Double-check to make sure we still need the new entry. */
		LIST_FOREACH(fhe,
		    &softc->g_fha.hashtable[fh % softc->g_fha.hashmask], link)
			if (fhe->fh == fh)
				break;
		if (!fhe) {
			fhe = new_fhe;
			LIST_INSERT_HEAD(
			    &softc->g_fha.hashtable[fh % softc->g_fha.hashmask],
			    fhe, link);
		} else
			fha_hash_entry_destroy(new_fhe);
	}

	return (fhe);
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
		fhe->num_exclusive += count;
	else
		fhe->num_rw += count;
}

/*
 * Get the service thread currently associated with the fhe that is
 * appropriate to handle this operation.
 */
SVCTHREAD *
fha_hash_entry_choose_thread(struct fha_params *softc,
    struct fha_hash_entry *fhe, struct fha_info *i, SVCTHREAD *this_thread);

SVCTHREAD *
fha_hash_entry_choose_thread(struct fha_params *softc,
    struct fha_hash_entry *fhe, struct fha_info *i, SVCTHREAD *this_thread)
{
	SVCTHREAD *thread, *min_thread = NULL;
	SVCPOOL *pool;
	int req_count, min_count = 0;
	off_t offset1, offset2;

	pool = *softc->pool;

	LIST_FOREACH(thread, &fhe->threads, st_alink) {
		req_count = thread->st_reqcount;

		/* If there are any writes in progress, use the first thread. */
		if (fhe->num_exclusive) {
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
		offset1 = i->offset;
		offset2 = STAILQ_FIRST(&thread->st_reqs)->rq_p3;

		if (((offset1 >= offset2)
		  && ((offset1 - offset2) < (1 << softc->ctls.bin_shift)))
		 || ((offset2 > offset1)
		  && ((offset2 - offset1) < (1 << softc->ctls.bin_shift)))) {
			if ((softc->ctls.max_reqs_per_nfsd == 0) ||
			    (req_count < softc->ctls.max_reqs_per_nfsd)) {
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
	 * We didn't find a good match yet.  See if we can add
	 * a new thread to this file handle entry's thread list.
	 */
	if ((softc->ctls.max_nfsds_per_fh == 0) ||
	    (fhe->num_threads < softc->ctls.max_nfsds_per_fh)) {
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
		} else if ((thread = LIST_FIRST(&pool->sp_idlethreads))) {
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
 * After getting a request, try to assign it to some thread.  Usually we
 * handle it ourselves.
 */
SVCTHREAD *
fha_assign(SVCTHREAD *this_thread, struct svc_req *req,
    struct fha_params *softc)
{
	SVCTHREAD *thread;
	struct fha_info i;
	struct fha_hash_entry *fhe;
	struct fha_callbacks *cb;

	cb = &softc->callbacks;

	/* Check to see whether we're enabled. */
	if (softc->ctls.enable == 0)
		return (this_thread);

	/*
	 * Only do placement if this is an NFS request.
	 */
	if (req->rq_prog != NFS_PROG)
		return (this_thread);

	if (req->rq_vers != 2 && req->rq_vers != 3)
		return (this_thread);

	fha_extract_info(req, &i, cb);

	/*
	 * We save the offset associated with this request for later
	 * nfsd matching.
	 */
	fhe = fha_hash_entry_lookup(softc, i.fh);
	req->rq_p1 = fhe;
	req->rq_p2 = i.locktype;
	req->rq_p3 = i.offset;

	/*
	 * Choose a thread, taking into consideration locality, thread load,
	 * and the number of threads already working on this file.
	 */
	thread = fha_hash_entry_choose_thread(softc, fhe, &i, this_thread);
	KASSERT(thread, ("fha_assign: NULL thread!"));
	fha_hash_entry_add_op(fhe, i.locktype, 1);

	return (thread);
}

/*
 * Called when we're done with an operation.  The request has already
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
		if (0 == fhe->num_rw + fhe->num_exclusive)
			fha_hash_entry_remove(fhe);
	}
}

int
fhe_stats_sysctl(SYSCTL_HANDLER_ARGS, struct fha_params *softc)
{
	int error, count, i;
	struct sbuf sb;
	struct fha_hash_entry *fhe;
	bool_t first = TRUE;
	SVCTHREAD *thread;
	SVCPOOL *pool;

	sbuf_new(&sb, NULL, 4096, SBUF_FIXEDLEN);

	pool = NULL;

	if (!*softc->pool) {
		sbuf_printf(&sb, "NFSD not running\n");
		goto out;
	}
	pool = *softc->pool;

	mtx_lock(&pool->sp_lock);
	count = 0;
	for (i = 0; i <= softc->g_fha.hashmask; i++)
		if (!LIST_EMPTY(&softc->g_fha.hashtable[i]))
			count++;

	if (count == 0) {
		sbuf_printf(&sb, "No file handle entries.\n");
		goto out;
	}

	for (i = 0; i <= softc->g_fha.hashmask; i++) {
		LIST_FOREACH(fhe, &softc->g_fha.hashtable[i], link) {
			sbuf_printf(&sb, "%sfhe %p: {\n", first ? "" : ", ", fhe);

			sbuf_printf(&sb, "    fh: %ju\n", (uintmax_t) fhe->fh);
			sbuf_printf(&sb, "    num_rw: %d\n", fhe->num_rw);
			sbuf_printf(&sb, "    num_exclusive: %d\n", fhe->num_exclusive);
			sbuf_printf(&sb, "    num_threads: %d\n", fhe->num_threads);

			LIST_FOREACH(thread, &fhe->threads, st_alink) {
				sbuf_printf(&sb, "    thread %p offset %ju "
				    "(count %d)\n", thread,
				    STAILQ_FIRST(&thread->st_reqs)->rq_p3,
				    thread->st_reqcount);
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
	if (pool)
		mtx_unlock(&pool->sp_lock);
	sbuf_trim(&sb);
	sbuf_finish(&sb);
	error = sysctl_handle_string(oidp, sbuf_data(&sb), sbuf_len(&sb), req);
	sbuf_delete(&sb);
	return (error);
}
