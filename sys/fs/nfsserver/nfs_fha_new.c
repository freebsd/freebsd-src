/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2008 Isilon Inc http://www.isilon.com/
 * Copyright (c) 2013 Spectra Logic Corporation
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
#include <sys/types.h>
#include <sys/mbuf.h>
#include <sys/sbuf.h>

#include <fs/nfs/nfsport.h>
#include <fs/nfsserver/nfs_fha_new.h>

#include <rpc/rpc.h>

static MALLOC_DEFINE(M_NFS_FHA, "NFS FHA", "NFS FHA");

static void		fhanew_init(void *foo);
static void		fhanew_uninit(void *foo);
static rpcproc_t	fhanew_get_procnum(rpcproc_t procnum);
static int		fhanew_get_fh(uint64_t *fh, int v3, struct mbuf **md,
			    caddr_t *dpos);
static int		fhanew_is_read(rpcproc_t procnum);
static int		fhanew_is_write(rpcproc_t procnum);
static int		fhanew_get_offset(struct mbuf **md, caddr_t *dpos,
			    int v3, struct fha_info *info);
static int		fhanew_no_offset(rpcproc_t procnum);
static void		fhanew_set_locktype(rpcproc_t procnum,
			    struct fha_info *info);
static int		fhenew_stats_sysctl(SYSCTL_HANDLER_ARGS);
static void		fha_extract_info(struct svc_req *req,
			    struct fha_info *i);

NFSD_VNET_DEFINE_STATIC(struct fha_params *, fhanew_softc);
NFSD_VNET_DEFINE_STATIC(struct fha_ctls, nfsfha_ctls);

SYSCTL_DECL(_vfs_nfsd);
SYSCTL_NODE(_vfs_nfsd, OID_AUTO, fha, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "NFS File Handle Affinity (FHA)");

SYSCTL_UINT(_vfs_nfsd_fha,
    OID_AUTO, enable, CTLFLAG_NFSD_VNET | CTLFLAG_RWTUN,
    &NFSD_VNET_NAME(nfsfha_ctls).enable, 0,
    "Enable NFS File Handle Affinity (FHA)");

SYSCTL_UINT(_vfs_nfsd_fha,
    OID_AUTO, read, CTLFLAG_NFSD_VNET | CTLFLAG_RWTUN,
    &NFSD_VNET_NAME(nfsfha_ctls).read, 0,
    "Enable NFS FHA read locality");

SYSCTL_UINT(_vfs_nfsd_fha,
    OID_AUTO, write, CTLFLAG_NFSD_VNET | CTLFLAG_RWTUN,
    &NFSD_VNET_NAME(nfsfha_ctls).write, 0,
    "Enable NFS FHA write locality");

SYSCTL_UINT(_vfs_nfsd_fha,
    OID_AUTO, bin_shift, CTLFLAG_NFSD_VNET | CTLFLAG_RWTUN,
    &NFSD_VNET_NAME(nfsfha_ctls).bin_shift, 0,
    "Maximum locality distance 2^(bin_shift) bytes");

SYSCTL_UINT(_vfs_nfsd_fha,
    OID_AUTO, max_nfsds_per_fh, CTLFLAG_NFSD_VNET | CTLFLAG_RWTUN,
    &NFSD_VNET_NAME(nfsfha_ctls).max_nfsds_per_fh, 0,
    "Maximum nfsd threads that "
    "should be working on requests for the same file handle");

SYSCTL_UINT(_vfs_nfsd_fha,
    OID_AUTO, max_reqs_per_nfsd, CTLFLAG_NFSD_VNET | CTLFLAG_RWTUN,
    &NFSD_VNET_NAME(nfsfha_ctls).max_reqs_per_nfsd, 0, "Maximum requests that "
    "single nfsd thread should be working on at any time");

SYSCTL_PROC(_vfs_nfsd_fha, OID_AUTO, fhe_stats,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, 0, 0,
    fhenew_stats_sysctl, "A", "");

extern int newnfs_nfsv3_procid[];

VNET_SYSINIT(nfs_fhanew, SI_SUB_VNET_DONE, SI_ORDER_ANY, fhanew_init, NULL);
VNET_SYSUNINIT(nfs_fhanew, SI_SUB_VNET_DONE, SI_ORDER_ANY, fhanew_uninit, NULL);

static void
fhanew_init(void *foo)
{
	struct fha_params *softc;
	int i;

	NFSD_VNET(fhanew_softc) = malloc(sizeof(struct fha_params), M_TEMP,
	    M_WAITOK | M_ZERO);
	softc = NFSD_VNET(fhanew_softc);

	snprintf(softc->server_name, sizeof(softc->server_name),
	    FHANEW_SERVER_NAME);

	for (i = 0; i < FHA_HASH_SIZE; i++)
		mtx_init(&softc->fha_hash[i].mtx, "fhalock", NULL, MTX_DEF);

	/*
	 * Set the default tuning parameters.
	 */
	NFSD_VNET(nfsfha_ctls).enable = FHA_DEF_ENABLE;
	NFSD_VNET(nfsfha_ctls).read = FHA_DEF_READ;
	NFSD_VNET(nfsfha_ctls).write = FHA_DEF_WRITE;
	NFSD_VNET(nfsfha_ctls).bin_shift = FHA_DEF_BIN_SHIFT;
	NFSD_VNET(nfsfha_ctls).max_nfsds_per_fh = FHA_DEF_MAX_NFSDS_PER_FH;
	NFSD_VNET(nfsfha_ctls).max_reqs_per_nfsd = FHA_DEF_MAX_REQS_PER_NFSD;

}

static void
fhanew_uninit(void *foo)
{
	struct fha_params *softc;
	int i;

	softc = NFSD_VNET(fhanew_softc);

	for (i = 0; i < FHA_HASH_SIZE; i++)
		mtx_destroy(&softc->fha_hash[i].mtx);
	free(softc, M_TEMP);
}

static rpcproc_t
fhanew_get_procnum(rpcproc_t procnum)
{
	if (procnum > NFSV2PROC_STATFS)
		return (-1);

	return (newnfs_nfsv3_procid[procnum]);
}

static int
fhanew_get_fh(uint64_t *fh, int v3, struct mbuf **md, caddr_t *dpos)
{
	struct nfsrv_descript lnd, *nd;
	uint32_t *tl;
	uint8_t *buf;
	uint64_t t;
	int error, len, i;

	error = 0;
	len = 0;
	nd = &lnd;

	nd->nd_md = *md;
	nd->nd_dpos = *dpos;

	if (v3) {
		NFSM_DISSECT_NONBLOCK(tl, uint32_t *, NFSX_UNSIGNED);
		if ((len = fxdr_unsigned(int, *tl)) <= 0 || len > NFSX_FHMAX) {
			error = EBADRPC;
			goto nfsmout;
		}
	} else {
		len = NFSX_V2FH;
	}

	t = 0;
	if (len != 0) {
		NFSM_DISSECT_NONBLOCK(buf, uint8_t *, len);
		for (i = 0; i < len; i++)
			t ^= ((uint64_t)buf[i] << (i & 7) * 8);
	}
	*fh = t;

nfsmout:
	*md = nd->nd_md;
	*dpos = nd->nd_dpos;

	return (error);
}

static int
fhanew_is_read(rpcproc_t procnum)
{
	if (procnum == NFSPROC_READ)
		return (1);
	else
		return (0);
}

static int
fhanew_is_write(rpcproc_t procnum)
{
	if (procnum == NFSPROC_WRITE)
		return (1);
	else
		return (0);
}

static int
fhanew_get_offset(struct mbuf **md, caddr_t *dpos, int v3,
    struct fha_info *info)
{
	struct nfsrv_descript lnd, *nd;
	uint32_t *tl;
	int error;

	error = 0;

	nd = &lnd;
	nd->nd_md = *md;
	nd->nd_dpos = *dpos;

	if (v3) {
		NFSM_DISSECT_NONBLOCK(tl, uint32_t *, 2 * NFSX_UNSIGNED);
		info->offset = fxdr_hyper(tl);
	} else {
		NFSM_DISSECT_NONBLOCK(tl, uint32_t *, NFSX_UNSIGNED);
		info->offset = fxdr_unsigned(uint32_t, *tl);
	}

nfsmout:
	*md = nd->nd_md;
	*dpos = nd->nd_dpos;

	return (error);
}

static int
fhanew_no_offset(rpcproc_t procnum)
{
	if (procnum == NFSPROC_FSSTAT ||
	    procnum == NFSPROC_FSINFO ||
	    procnum == NFSPROC_PATHCONF ||
	    procnum == NFSPROC_NOOP ||
	    procnum == NFSPROC_NULL)
		return (1);
	else
		return (0);
}

static void
fhanew_set_locktype(rpcproc_t procnum, struct fha_info *info)
{
	switch (procnum) {
	case NFSPROC_NULL:
	case NFSPROC_GETATTR:
	case NFSPROC_LOOKUP:
	case NFSPROC_ACCESS:
	case NFSPROC_READLINK:
	case NFSPROC_READ:
	case NFSPROC_READDIR:
	case NFSPROC_READDIRPLUS:
	case NFSPROC_WRITE:
		info->locktype = LK_SHARED;
		break;
	case NFSPROC_SETATTR:
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
		info->locktype = LK_EXCLUSIVE;
		break;
	}
}

/*
 * This just specifies that offsets should obey affinity when within
 * the same 1Mbyte (1<<20) chunk for the file (reads only for now).
 */
static void
fha_extract_info(struct svc_req *req, struct fha_info *i)
{
	struct mbuf *md;
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
	i->read = i->write = 0;

	/*
	 * Extract the procnum and convert to v3 form if necessary,
	 * taking care to deal with out-of-range procnums.  Caller will
	 * ensure that rq_vers is either 2 or 3.
	 */
	procnum = req->rq_proc;
	if (!v3) {
		rpcproc_t tmp_procnum;

		tmp_procnum = fhanew_get_procnum(procnum);
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
	if (fhanew_no_offset(procnum))
		goto out;

	i->read = fhanew_is_read(procnum);
	i->write = fhanew_is_write(procnum);

	error = newnfs_realign(&req->rq_args, M_NOWAIT);
	if (error)
		goto out;
	md = req->rq_args;
	dpos = mtod(md, caddr_t);

	/* Grab the filehandle. */
	error = fhanew_get_fh(&i->fh, v3, &md, &dpos);
	if (error)
		goto out;

	/* Content ourselves with zero offset for all but reads. */
	if (i->read || i->write)
		fhanew_get_offset(&md, &dpos, v3, i);

out:
	fhanew_set_locktype(procnum, i);
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

	mtx_assert(e->mtx, MA_OWNED);
	KASSERT(e->num_rw == 0,
	    ("%d reqs on destroyed fhe %p", e->num_rw, e));
	KASSERT(e->num_exclusive == 0,
	    ("%d exclusive reqs on destroyed fhe %p", e->num_exclusive, e));
	KASSERT(e->num_threads == 0,
	    ("%d threads on destroyed fhe %p", e->num_threads, e));
	free(e, M_NFS_FHA);
}

static void
fha_hash_entry_remove(struct fha_hash_entry *e)
{

	mtx_assert(e->mtx, MA_OWNED);
	LIST_REMOVE(e, link);
	fha_hash_entry_destroy(e);
}

static struct fha_hash_entry *
fha_hash_entry_lookup(struct fha_params *softc, u_int64_t fh)
{
	struct fha_hash_slot *fhs;
	struct fha_hash_entry *fhe, *new_fhe;

	fhs = &softc->fha_hash[fh % FHA_HASH_SIZE];
	new_fhe = fha_hash_entry_new(fh);
	new_fhe->mtx = &fhs->mtx;
	mtx_lock(&fhs->mtx);
	LIST_FOREACH(fhe, &fhs->list, link)
		if (fhe->fh == fh)
			break;
	if (!fhe) {
		fhe = new_fhe;
		LIST_INSERT_HEAD(&fhs->list, fhe, link);
	} else
		fha_hash_entry_destroy(new_fhe);
	return (fhe);
}

static void
fha_hash_entry_add_thread(struct fha_hash_entry *fhe, SVCTHREAD *thread)
{

	mtx_assert(fhe->mtx, MA_OWNED);
	thread->st_p2 = 0;
	LIST_INSERT_HEAD(&fhe->threads, thread, st_alink);
	fhe->num_threads++;
}

static void
fha_hash_entry_remove_thread(struct fha_hash_entry *fhe, SVCTHREAD *thread)
{

	mtx_assert(fhe->mtx, MA_OWNED);
	KASSERT(thread->st_p2 == 0,
	    ("%d reqs on removed thread %p", thread->st_p2, thread));
	LIST_REMOVE(thread, st_alink);
	fhe->num_threads--;
}

/*
 * Account for an ongoing operation associated with this file.
 */
static void
fha_hash_entry_add_op(struct fha_hash_entry *fhe, int locktype, int count)
{

	mtx_assert(fhe->mtx, MA_OWNED);
	if (LK_EXCLUSIVE == locktype)
		fhe->num_exclusive += count;
	else
		fhe->num_rw += count;
}

/*
 * Get the service thread currently associated with the fhe that is
 * appropriate to handle this operation.
 */
static SVCTHREAD *
fha_hash_entry_choose_thread(struct fha_params *softc,
    struct fha_hash_entry *fhe, struct fha_info *i, SVCTHREAD *this_thread)
{
	SVCTHREAD *thread, *min_thread = NULL;
	int req_count, min_count = 0;
	off_t offset1, offset2;

	LIST_FOREACH(thread, &fhe->threads, st_alink) {
		req_count = thread->st_p2;

		/* If there are any writes in progress, use the first thread. */
		if (fhe->num_exclusive) {
#if 0
			ITRACE_CURPROC(ITRACE_NFS, ITRACE_INFO,
			    "fha: %p(%d)w", thread, req_count);
#endif
			return (thread);
		}

		/* Check whether we should consider locality. */
		if ((i->read && !NFSD_VNET(nfsfha_ctls).read) ||
		    (i->write && !NFSD_VNET(nfsfha_ctls).write))
			goto noloc;

		/*
		 * Check for locality, making sure that we won't
		 * exceed our per-thread load limit in the process.
		 */
		offset1 = i->offset;
		offset2 = thread->st_p3;

		if (((offset1 >= offset2)
		  && ((offset1 - offset2) < (1 << NFSD_VNET(nfsfha_ctls).bin_shift)))
		 || ((offset2 > offset1)
		  && ((offset2 - offset1) < (1 << NFSD_VNET(nfsfha_ctls).bin_shift)))) {
			if ((NFSD_VNET(nfsfha_ctls).max_reqs_per_nfsd == 0) ||
			    (req_count < NFSD_VNET(nfsfha_ctls).max_reqs_per_nfsd)) {
#if 0
				ITRACE_CURPROC(ITRACE_NFS, ITRACE_INFO,
				    "fha: %p(%d)r", thread, req_count);
#endif
				return (thread);
			}
		}

noloc:
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
	if ((NFSD_VNET(nfsfha_ctls).max_nfsds_per_fh == 0) ||
	    (fhe->num_threads < NFSD_VNET(nfsfha_ctls).max_nfsds_per_fh)) {
		thread = this_thread;
#if 0
		ITRACE_CURPROC(ITRACE_NFS, ITRACE_INFO,
		    "fha: %p(%d)t", thread, thread->st_p2);
#endif
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
fhanew_assign(SVCTHREAD *this_thread, struct svc_req *req)
{
	struct fha_params *softc;
	SVCTHREAD *thread;
	struct fha_info i;
	struct fha_hash_entry *fhe;

	NFSD_CURVNET_SET(NFSD_TD_TO_VNET(curthread));
	softc = NFSD_VNET(fhanew_softc);
	/* Check to see whether we're enabled. */
	if (NFSD_VNET(nfsfha_ctls).enable == 0)
		goto thist;

	/*
	 * Only do placement if this is an NFS request.
	 */
	if (req->rq_prog != NFS_PROG)
		goto thist;

	if (req->rq_vers != 2 && req->rq_vers != 3)
		goto thist;

	fha_extract_info(req, &i);

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
	thread->st_p2++;
	thread->st_p3 = i.offset;

	/*
	 * Grab the pool lock here to not let chosen thread go away before
	 * the new request inserted to its queue while we drop fhe lock.
	 */
	mtx_lock(&thread->st_lock);
	mtx_unlock(fhe->mtx);

	NFSD_CURVNET_RESTORE();
	return (thread);
thist:
	req->rq_p1 = NULL;
	NFSD_CURVNET_RESTORE();
	mtx_lock(&this_thread->st_lock);
	return (this_thread);
}

/*
 * Called when we're done with an operation.  The request has already
 * been de-queued.
 */
void
fhanew_nd_complete(SVCTHREAD *thread, struct svc_req *req)
{
	struct fha_hash_entry *fhe = req->rq_p1;
	struct mtx *mtx;

	NFSD_CURVNET_SET(NFSD_TD_TO_VNET(curthread));
	/*
	 * This may be called for reqs that didn't go through
	 * fha_assign (e.g. extra NULL ops used for RPCSEC_GSS.
	 */
	if (!fhe) {
		NFSD_CURVNET_RESTORE();
		return;
	}

	mtx = fhe->mtx;
	mtx_lock(mtx);
	fha_hash_entry_add_op(fhe, req->rq_p2, -1);
	thread->st_p2--;
	KASSERT(thread->st_p2 >= 0, ("Negative request count %d on %p",
	    thread->st_p2, thread));
	if (thread->st_p2 == 0) {
		fha_hash_entry_remove_thread(fhe, thread);
		if (0 == fhe->num_rw + fhe->num_exclusive)
			fha_hash_entry_remove(fhe);
	}
	mtx_unlock(mtx);
	NFSD_CURVNET_RESTORE();
}

static int
fhenew_stats_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct fha_params *softc;
	int error, i;
	struct sbuf sb;
	struct fha_hash_entry *fhe;
	bool_t first, hfirst;
	SVCTHREAD *thread;

	sbuf_new(&sb, NULL, 65536, SBUF_FIXEDLEN);

	NFSD_CURVNET_SET(NFSD_TD_TO_VNET(curthread));
	softc = NFSD_VNET(fhanew_softc);
	for (i = 0; i < FHA_HASH_SIZE; i++)
		if (!LIST_EMPTY(&softc->fha_hash[i].list))
			break;

	if (i == FHA_HASH_SIZE) {
		sbuf_printf(&sb, "No file handle entries.\n");
		goto out;
	}

	hfirst = TRUE;
	for (; i < FHA_HASH_SIZE; i++) {
		mtx_lock(&softc->fha_hash[i].mtx);
		if (LIST_EMPTY(&softc->fha_hash[i].list)) {
			mtx_unlock(&softc->fha_hash[i].mtx);
			continue;
		}
		sbuf_printf(&sb, "%shash %d: {\n", hfirst ? "" : ", ", i);
		first = TRUE;
		LIST_FOREACH(fhe, &softc->fha_hash[i].list, link) {
			sbuf_printf(&sb, "%sfhe %p: {\n", first ? "  " : ", ",
			    fhe);
			sbuf_printf(&sb, "    fh: %ju\n", (uintmax_t) fhe->fh);
			sbuf_printf(&sb, "    num_rw/exclusive: %d/%d\n",
			    fhe->num_rw, fhe->num_exclusive);
			sbuf_printf(&sb, "    num_threads: %d\n",
			    fhe->num_threads);

			LIST_FOREACH(thread, &fhe->threads, st_alink) {
				sbuf_printf(&sb, "      thread %p offset %ju "
				    "reqs %d\n", thread,
				    thread->st_p3, thread->st_p2);
			}

			sbuf_printf(&sb, "  }");
			first = FALSE;
		}
		sbuf_printf(&sb, "\n}");
		mtx_unlock(&softc->fha_hash[i].mtx);
		hfirst = FALSE;
	}

 out:
	NFSD_CURVNET_RESTORE();
	sbuf_trim(&sb);
	sbuf_finish(&sb);
	error = sysctl_handle_string(oidp, sbuf_data(&sb), sbuf_len(&sb), req);
	sbuf_delete(&sb);
	return (error);
}
