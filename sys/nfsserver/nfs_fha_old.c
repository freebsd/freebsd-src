/*-
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>

#include <rpc/rpc.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs_fha.h>
#include <nfsserver/nfs.h>
#include <nfsserver/nfsm_subs.h>
#include <nfsserver/nfs_fha_old.h>

static void fhaold_init(void *foo);
static void fhaold_uninit(void *foo);
rpcproc_t fhaold_get_procnum(rpcproc_t procnum);
int fhaold_realign(struct mbuf **mb, int malloc_flags);
int fhaold_get_fh(fhandle_t *fh, int v3, struct mbuf **md, caddr_t *dpos);
int fhaold_is_read(rpcproc_t procnum);
int fhaold_is_write(rpcproc_t procnum);
int fhaold_get_offset(struct mbuf **md, caddr_t *dpos, int v3,
		      struct fha_info *info);
int fhaold_no_offset(rpcproc_t procnum);
void fhaold_set_locktype(rpcproc_t procnum, struct fha_info *info);
static int fheold_stats_sysctl(SYSCTL_HANDLER_ARGS);

static struct fha_params fhaold_softc;

SYSCTL_DECL(_vfs_nfsrv);

extern SVCPOOL *nfsrv_pool;

SYSINIT(nfs_fhaold, SI_SUB_ROOT_CONF, SI_ORDER_ANY, fhaold_init, NULL);
SYSUNINIT(nfs_fhaold, SI_SUB_ROOT_CONF, SI_ORDER_ANY, fhaold_uninit, NULL);

static void
fhaold_init(void *foo)
{
	struct fha_params *softc;

	softc = &fhaold_softc;

	bzero(softc, sizeof(*softc));

	/*
	 * Setup the callbacks for this FHA personality.
	 */
	softc->callbacks.get_procnum = fhaold_get_procnum;
	softc->callbacks.realign = fhaold_realign;
	softc->callbacks.get_fh = fhaold_get_fh;
	softc->callbacks.is_read = fhaold_is_read;
	softc->callbacks.is_write = fhaold_is_write;
	softc->callbacks.get_offset = fhaold_get_offset;
	softc->callbacks.no_offset = fhaold_no_offset;
	softc->callbacks.set_locktype = fhaold_set_locktype;
	softc->callbacks.fhe_stats_sysctl = fheold_stats_sysctl;

	snprintf(softc->server_name, sizeof(softc->server_name),
	    FHAOLD_SERVER_NAME);

	softc->pool = &nfsrv_pool;

	/*
	 * Initialize the sysctl context list for the fha module.
	 */
	sysctl_ctx_init(&softc->sysctl_ctx);
	softc->sysctl_tree = SYSCTL_ADD_NODE(&softc->sysctl_ctx,
	    SYSCTL_STATIC_CHILDREN(_vfs_nfsrv), OID_AUTO, "fha", CTLFLAG_RD,
	    0, "fha node");
	if (softc->sysctl_tree == NULL) {
		printf("%s: unable to allocate sysctl tree\n", __func__);
		return;
	}
	fha_init(softc);
}

static void
fhaold_uninit(void *foo)
{
	struct fha_params *softc;

	softc = &fhaold_softc;

	fha_uninit(softc);
}


rpcproc_t
fhaold_get_procnum(rpcproc_t procnum)
{
	if (procnum > NFSV2PROC_STATFS)
		return (-1);

	return (nfsrv_nfsv3_procid[procnum]);
}

int
fhaold_realign(struct mbuf **mb, int malloc_flags)
{
	return (nfs_realign(mb, malloc_flags));
}

int
fhaold_get_fh(fhandle_t *fh, int v3, struct mbuf **md, caddr_t *dpos)
{
	return (nfsm_srvmtofh_xx(fh, v3, md, dpos));
}

int
fhaold_is_read(rpcproc_t procnum)
{
	if (procnum == NFSPROC_READ)
		return (1);
	else
		return (0);
}

int
fhaold_is_write(rpcproc_t procnum)
{
	if (procnum == NFSPROC_WRITE)
		return (1);
	else
		return (0);
}

int
fhaold_get_offset(struct mbuf **md, caddr_t *dpos, int v3,
		  struct fha_info *info)
{
	uint32_t *tl;

	if (v3) {
		tl = nfsm_dissect_xx_nonblock(2 * NFSX_UNSIGNED, md, dpos);
		if (tl == NULL)
			goto out;
		info->offset = fxdr_hyper(tl);
	} else {
		tl = nfsm_dissect_xx_nonblock(NFSX_UNSIGNED, md, dpos);
		if (tl == NULL)
			goto out;
		info->offset = fxdr_unsigned(uint32_t, *tl);
	}

	return (0);
out:
	return (-1);
}

int
fhaold_no_offset(rpcproc_t procnum)
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

void
fhaold_set_locktype(rpcproc_t procnum, struct fha_info *info)
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

static int
fheold_stats_sysctl(SYSCTL_HANDLER_ARGS)
{
	return (fhe_stats_sysctl(oidp, arg1, arg2, req, &fhaold_softc));
}

SVCTHREAD *
fhaold_assign(SVCTHREAD *this_thread, struct svc_req *req)
{
	return (fha_assign(this_thread, req, &fhaold_softc));
}
