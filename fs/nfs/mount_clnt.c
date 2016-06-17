/*
 * linux/fs/nfs/mount_clnt.c
 *
 * MOUNT client to support NFSroot.
 *
 * Copyright (C) 1997, Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/types.h>
#include <linux/socket.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/uio.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/xprt.h>
#include <linux/sunrpc/sched.h>
#include <linux/nfs_fs.h>

#ifdef RPC_DEBUG
# define NFSDBG_FACILITY	NFSDBG_ROOT
#endif

/*
#define MOUNT_PROGRAM		100005
#define MOUNT_VERSION		1
#define MOUNT_MNT		1
#define MOUNT_UMNT		3
 */

static struct rpc_clnt *	mnt_create(char *, struct sockaddr_in *,
								int, int);
struct rpc_program      mnt_program;

struct mnt_fhstatus {
	unsigned int		status;
	struct nfs_fh *		fh;
};

/*
 * Obtain an NFS file handle for the given host and path
 */
int
nfsroot_mount(struct sockaddr_in *addr, char *path, struct nfs_fh *fh,
		int version, int protocol)
{
	struct rpc_clnt		*mnt_clnt;
	struct mnt_fhstatus	result = { 0, fh };
	char			hostname[32];
	int			status;
	int			call;

	dprintk("NFS:      nfs_mount(%08x:%s)\n",
			(unsigned)ntohl(addr->sin_addr.s_addr), path);

	sprintf(hostname, "%u.%u.%u.%u", NIPQUAD(addr->sin_addr.s_addr));
	if (!(mnt_clnt = mnt_create(hostname, addr, version, protocol)))
		return -EACCES;

	call = (version == NFS_MNT3_VERSION)?  MOUNTPROC3_MNT : MNTPROC_MNT;
	status = rpc_call(mnt_clnt, call, path, &result, 0);
	return status < 0? status : (result.status? -EACCES : 0);
}

static struct rpc_clnt *
mnt_create(char *hostname, struct sockaddr_in *srvaddr, int version,
		int protocol)
{
	struct rpc_xprt	*xprt;
	struct rpc_clnt	*clnt;

	if (!(xprt = xprt_create_proto(protocol, srvaddr, NULL)))
		return NULL;

	clnt = rpc_create_client(xprt, hostname,
				&mnt_program, version,
				RPC_AUTH_NULL);
	if (!clnt) {
		xprt_destroy(xprt);
	} else {
		clnt->cl_softrtry = 1;
		clnt->cl_chatty   = 1;
		clnt->cl_oneshot  = 1;
		clnt->cl_intr = 1;
	}
	return clnt;
}

/*
 * XDR encode/decode functions for MOUNT
 */
static int
xdr_error(struct rpc_rqst *req, u32 *p, void *dummy)
{
	return -EIO;
}

static int
xdr_encode_dirpath(struct rpc_rqst *req, u32 *p, const char *path)
{
	p = xdr_encode_string(p, path);

	req->rq_slen = xdr_adjust_iovec(req->rq_svec, p);
	return 0;
}

static int
xdr_decode_fhstatus(struct rpc_rqst *req, u32 *p, struct mnt_fhstatus *res)
{
	struct nfs_fh *fh = res->fh;

	memset((void *)fh, 0, sizeof(*fh));
	if ((res->status = ntohl(*p++)) == 0) {
		fh->size = NFS2_FHSIZE;
		memcpy(fh->data, p, NFS2_FHSIZE);
	}
	return 0;
}

static int
xdr_decode_fhstatus3(struct rpc_rqst *req, u32 *p, struct mnt_fhstatus *res)
{
	struct nfs_fh *fh = res->fh;

	memset((void *)fh, 0, sizeof(*fh));
	if ((res->status = ntohl(*p++)) == 0) {
		int size = ntohl(*p++);
		if (size <= NFS3_FHSIZE) {
			fh->size = size;
			memcpy(fh->data, p, size);
		} else
			res->status = -EBADHANDLE;
	}
	return 0;
}

#define MNT_dirpath_sz		(1 + 256)
#define MNT_fhstatus_sz		(1 + 8)

static struct rpc_procinfo	mnt_procedures[2] = {
	{ "mnt_null",
		(kxdrproc_t) xdr_error,	
		(kxdrproc_t) xdr_error,	0, 0 },
	{ "mnt_mount",
		(kxdrproc_t) xdr_encode_dirpath,	
		(kxdrproc_t) xdr_decode_fhstatus,
		MNT_dirpath_sz << 2, 0 },
};

static struct rpc_procinfo mnt3_procedures[2] = {
	{ "mnt3_null",
		(kxdrproc_t) xdr_error,
		(kxdrproc_t) xdr_error, 0, 0 },
	{ "mnt3_mount",
		(kxdrproc_t) xdr_encode_dirpath,
		(kxdrproc_t) xdr_decode_fhstatus3,
		MNT_dirpath_sz << 2, 0 },
};


static struct rpc_version	mnt_version1 = {
	1, 2, mnt_procedures
};

static struct rpc_version       mnt_version3 = {
	3, 2, mnt3_procedures
};

static struct rpc_version *	mnt_version[] = {
	NULL,
	&mnt_version1,
	NULL,
	&mnt_version3,
};

static struct rpc_stat		mnt_stats;

struct rpc_program	mnt_program = {
	"mount",
	NFS_MNT_PROGRAM,
	sizeof(mnt_version)/sizeof(mnt_version[0]),
	mnt_version,
	&mnt_stats,
};
