/*-
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: nfs_ops.c,v 1.6 1997/02/22 16:01:37 peter Exp $
 */

#ifndef lint
static char sccsid[] = "@(#)nfs_ops.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include "am.h"
#include <sys/stat.h>

#ifdef HAS_NFS

#define NFS
#define NFSCLIENT
#ifdef NFS_3
typedef nfs_fh fhandle_t;
#endif /* NFS_3 */
#ifdef NFS_HDR
#include NFS_HDR
#endif /* NFS_HDR */
#include <sys/mount.h>
#include "mount.h"

/*
 * Network file system
 */

/*
 * Convert from nfsstat to UN*X error code
 */
#define unx_error(e)	((e) < 10000 ? (int)(e)			\
			 : ((e) == MNT3ERR_NOTSUPP ? EOPNOTSUPP	\
			    : ((e) == MNT3ERR_SERVERFAULT ? EIO	\
			       : EINVAL)))

/*
 * The NFS layer maintains a cache of file handles.
 * This is *fundamental* to the implementation and
 * also allows quick remounting when a filesystem
 * is accessed soon after timing out.
 *
 * The NFS server layer knows to flush this cache
 * when a server goes down so avoiding stale handles.
 *
 * Each cache entry keeps a hard reference to
 * the corresponding server.  This ensures that
 * the server keepalive information is maintained.
 *
 * The copy of the sockaddr_in here is taken so
 * that the port can be twiddled to talk to mountd
 * instead of portmap or the NFS server as used
 * elsewhere.
 * The port# is flushed if a server goes down.
 * The IP address is never flushed - we assume
 * that the address of a mounted machine never
 * changes.  If it does, then you have other
 * problems...
 */
typedef struct mountres {
	int	mr_version;		/* 1 or 3 */
	union {
		struct fhstatus mru_fhstatus; /* mount v1 result */
		struct mountres3 mru_mountres3;	/* mount v3 result */
	} mr_un;
} mountres;
#define mr_fhstatus	mr_un.mru_fhstatus
#define mr_mountres3	mr_un.mru_mountres3

typedef struct fh_cache fh_cache;
struct fh_cache {
	qelem	fh_q;			/* List header */
	voidp	fh_wchan;		/* Wait channel */
	int	fh_error;		/* Valid data? */
	int	fh_id;			/* Unique id */
	int	fh_cid;			/* Callout id */
	mountres fh_mountres;		/* Result of mount rpc */
	struct sockaddr_in fh_sin;	/* Address of mountd */
	fserver *fh_fs;			/* Server holding filesystem */
	char	*fh_path;		/* Filesystem on host */
};

/*
 * FH_TTL is the time a file handle will remain in the cache since
 * last being used.  If the file handle becomes invalid, then it
 * will be flushed anyway.
 */
#define	FH_TTL		(5 * 60)		/* five minutes */
#define	FH_TTL_ERROR	(30)			/* 30 seconds */

static int fh_id = 0;
#define	FHID_ALLOC()	(++fh_id)
extern qelem fh_head;
qelem fh_head = { &fh_head, &fh_head };

static int call_mountd P((fh_cache*, unsigned long, unsigned long, fwd_fun, voidp));

AUTH *nfs_auth;

static fh_cache *find_nfs_fhandle_cache P((voidp idv, int done));
static fh_cache *find_nfs_fhandle_cache(idv, done)
voidp idv;
int done;
{
	fh_cache *fp, *fp2 = 0;
	int id = (int) idv;

	ITER(fp, fh_cache, &fh_head) {
		if (fp->fh_id == id) {
			fp2 = fp;
			break;
		}
	}

#ifdef DEBUG
	if (fp2) {
		dlog("fh cache gives fp %#x, fs %s", fp2, fp2->fh_path);
	} else {
		dlog("fh cache search failed");
	}
#endif /* DEBUG */

	if (fp2 && !done) {
		fp2->fh_error = ETIMEDOUT;
		return 0;
	}

	return fp2;
}

/*
 * Called when a filehandle appears
 */
static void got_nfs_fh P((voidp pkt, int len, struct sockaddr_in *sa,
				struct sockaddr_in *ia, voidp idv, int done));
static void got_nfs_fh(pkt, len, sa, ia, idv, done)
voidp pkt;
int len;
struct sockaddr_in *sa, *ia;
voidp idv;
int done;
{
	fh_cache *fp = find_nfs_fhandle_cache(idv, done);
	if (fp) {
		if (fp->fh_mountres.mr_version == MOUNTVERS3) {
			fp->fh_error = pickup_rpc_reply(pkt, len, (voidp) &fp->fh_mountres.mr_mountres3, xdr_mountres3);
			if (fp->fh_error) {
				/*
				 * Fall back to version 1 protocol.
				 */
#ifdef DEBUG
				dlog("mount version 3 refused, retrying with version 1");
#endif
				fp->fh_id = FHID_ALLOC();
				fp->fh_mountres.mr_version = MOUNTVERS;
				call_mountd(fp, MOUNTPROC_MNT, MOUNTVERS, got_nfs_fh, fp->fh_wchan);
			}
		} else
			fp->fh_error = pickup_rpc_reply(pkt, len, (voidp) &fp->fh_mountres.mr_fhstatus, xdr_fhstatus);
		if (!fp->fh_error) {
#ifdef DEBUG
			dlog("got filehandle for %s:%s, version=%d", fp->fh_fs->fs_host, fp->fh_path, fp->fh_mountres.mr_version);
#endif /* DEBUG */
			/*
			 * Wakeup anything sleeping on this filehandle
			 */
			if (fp->fh_wchan) {
#ifdef DEBUG
				dlog("Calling wakeup on %#x", fp->fh_wchan);
#endif /* DEBUG */
				wakeup(fp->fh_wchan);
			}
		}
	}
}

void flush_nfs_fhandle_cache P((fserver *fs));
void flush_nfs_fhandle_cache(fs)
fserver *fs;
{
	fh_cache *fp;
	ITER(fp, fh_cache, &fh_head) {
		if (fp->fh_fs == fs || fs == 0) {
			fp->fh_sin.sin_port = (u_short) 0;
			fp->fh_error = -1;
		}
	}
}

static void discard_fh P((fh_cache *fp));
static void discard_fh(fp)
fh_cache *fp;
{
	rem_que(&fp->fh_q);
#ifdef DEBUG
	dlog("Discarding filehandle for %s:%s", fp->fh_fs->fs_host, fp->fh_path);
#endif /* DEBUG */
	free_srvr(fp->fh_fs);
	if (fp->fh_mountres.mr_version == MOUNTVERS3
	    && fp->fh_mountres.mr_mountres3.mountres3_u.mountinfo.fhandle.fhandle3_val)
		free(fp->fh_mountres.mr_mountres3.mountres3_u.mountinfo.fhandle.fhandle3_val);
	free((voidp) fp->fh_path);
	free((voidp) fp);
}

/*
 * Determine the file handle for a node
 */
static int prime_nfs_fhandle_cache P((char *path, fserver *fs, int forcev2, struct mountres *mrbuf, voidp wchan));
static int prime_nfs_fhandle_cache(path, fs, forcev2, mrbuf, wchan)
char *path;
fserver *fs;
int forcev2;
struct mountres *mrbuf;
voidp wchan;
{
	fh_cache *fp, *fp_save = 0;
	int error;
	int reuse_id = FALSE;

#ifdef DEBUG
	dlog("Searching cache for %s:%s", fs->fs_host, path);
#endif /* DEBUG */

	/*
	 * First search the cache
	 */
	ITER(fp, fh_cache, &fh_head) {
		if (fs == fp->fh_fs && strcmp(path, fp->fh_path) == 0) {
			switch (fp->fh_error) {
			case 0:
				if (fp->fh_mountres.mr_version == MOUNTVERS)
					error = fp->fh_error = unx_error(fp->fh_mountres.mr_fhstatus.fhs_status);
				else
					error = fp->fh_error = unx_error(fp->fh_mountres.mr_mountres3.fhs_status);
				if (error == 0) {
					if (mrbuf)
						bcopy((voidp) &fp->fh_mountres, (voidp) mrbuf,
							sizeof(fp->fh_mountres));
					if (fp->fh_cid)
						untimeout(fp->fh_cid);
					fp->fh_cid = timeout(FH_TTL, discard_fh, (voidp) fp);
				} else if (error == EACCES) {
					/*
					 * Now decode the file handle return code.
					 */
					plog(XLOG_INFO, "Filehandle denied for \"%s:%s\"",
						fs->fs_host, path);
				} else {
					errno = error;	/* XXX */
					plog(XLOG_INFO, "Filehandle error for \"%s:%s\": %m",
						fs->fs_host, path);
				}

				/*
				 * The error was returned from the remote mount daemon.
				 * Policy: this error will be cached for now...
				 */
				return error;

			case -1:
				/*
				 * Still thinking about it, but we can re-use.
				 */
				fp_save = fp;
				reuse_id = TRUE;
				break;

			default:
				/*
				 * Return the error.
				 * Policy: make sure we recompute if required again
				 * in case this was caused by a network failure.
				 * This can thrash mountd's though...  If you find
				 * your mountd going slowly then:
				 * 1.  Add a fork() loop to main.
				 * 2.  Remove the call to innetgr() and don't use
				 *     netgroups, especially if you don't use YP.
				 */
				error = fp->fh_error;
				fp->fh_error = -1;
				return error;
			}
			break;
		}
	}

	/*
	 * Not in cache
	 */
	if (fp_save) {
		fp = fp_save;
		/*
		 * Re-use existing slot
		 */
		untimeout(fp->fh_cid);
		free_srvr(fp->fh_fs);
		free(fp->fh_path);
	} else {
		fp = ALLOC(fh_cache);
		bzero((voidp) fp, sizeof(*fp));
		ins_que(&fp->fh_q, &fh_head);
	}
	if (!reuse_id)
		fp->fh_id = FHID_ALLOC();
	fp->fh_wchan = wchan;
	fp->fh_error = -1;
	fp->fh_cid = timeout(FH_TTL, discard_fh, (voidp) fp);
	if (forcev2) {
		/*
		 * Force an NFSv2 mount.
		 */
#ifdef DEBUG
		dlog("forcing v2 mount");
#endif
		fp->fh_mountres.mr_version = MOUNTVERS;
	} else {
		/*
		 * Attempt v3 first.
		 */
		fp->fh_mountres.mr_version = MOUNTVERS3;
		fp->fh_mountres.mr_mountres3.mountres3_u.mountinfo.fhandle.fhandle3_val = NULL;
	}

	/*
	 * If the address has changed then don't try to re-use the
	 * port information
	 */
	if (fp->fh_sin.sin_addr.s_addr != fs->fs_ip->sin_addr.s_addr) {
		fp->fh_sin = *fs->fs_ip;
		fp->fh_sin.sin_port = 0;
	}
	fp->fh_fs = dup_srvr(fs);
	fp->fh_path = strdup(path);

	error = call_mountd(fp, MOUNTPROC_MNT, fp->fh_mountres.mr_version, got_nfs_fh, wchan);
	if (error) {
		/*
		 * Local error - cache for a short period
		 * just to prevent thrashing.
		 */
		untimeout(fp->fh_cid);
		fp->fh_cid = timeout(error < 0 ? 2 * ALLOWED_MOUNT_TIME : FH_TTL_ERROR,
						discard_fh, (voidp) fp);
		fp->fh_error = error;
	} else {
		error = fp->fh_error;
	}
	return error;
}

int make_nfs_auth P((void))
{
#ifdef HAS_NFS_QUALIFIED_NAMES
	/*
	 * From: Chris Metcalf <metcalf@masala.lcs.mit.edu>
	 * Use hostd, not just hostname.  Note that uids
	 * and gids and the gidlist are type *int* and not the
	 * system uid_t and gid_t types.
	 */
	static int group_wheel = 0;
	nfs_auth = authunix_create(hostd, 0, 0, 1, &group_wheel);
#else
	nfs_auth = authunix_create_default();
#endif
	if (!nfs_auth)
		return ENOBUFS;
	return 0;
}

static int call_mountd P((fh_cache *fp, u_long proc, u_long version, fwd_fun f, voidp wchan));
static int call_mountd(fp, proc, version, f, wchan)
fh_cache *fp;
u_long proc;
u_long version;
fwd_fun f;
voidp wchan;
{
	struct rpc_msg mnt_msg;
	int len;
	char iobuf[8192];
	int error;

	if (!nfs_auth) {
		error = make_nfs_auth();
		if (error)
			return error;
	}

	if (fp->fh_sin.sin_port == 0) {
		u_short port;
		error = nfs_srvr_port(fp->fh_fs, &port, wchan);
		if (error)
			return error;
		fp->fh_sin.sin_port = port;
	}

	rpc_msg_init(&mnt_msg, MOUNTPROG, version, (unsigned long) 0);
	len = make_rpc_packet(iobuf, sizeof(iobuf), proc,
			&mnt_msg, (voidp) &fp->fh_path, xdr_nfspath,  nfs_auth);

	if (len > 0) {
		error = fwd_packet(MK_RPC_XID(RPC_XID_MOUNTD, fp->fh_id),
			(voidp) iobuf, len, &fp->fh_sin, &fp->fh_sin, (voidp) fp->fh_id, f);
	} else {
		error = -len;
	}
/*
 * It may be the case that we're sending to the wrong MOUNTD port.  This
 * occurs if mountd is restarted on the server after the port has been
 * looked up and stored in the filehandle cache somewhere.  The correct
 * solution, if we're going to cache port numbers is to catch the ICMP
 * port unreachable reply from the server and cause the portmap request
 * to be redone.  The quick solution here is to invalidate the MOUNTD
 * port.
 */
      fp->fh_sin.sin_port = 0;

	return error;
}

/*-------------------------------------------------------------------------*/

/*
 * NFS needs the local filesystem, remote filesystem
 * remote hostname.
 * Local filesystem defaults to remote and vice-versa.
 */
static char *nfs_match(fo)
am_opts *fo;
{
	char *xmtab;
	if (fo->opt_fs && !fo->opt_rfs)
		fo->opt_rfs = fo->opt_fs;
	if (!fo->opt_rfs) {
		plog(XLOG_USER, "nfs: no remote filesystem specified");
		return FALSE;
	}
	if (!fo->opt_rhost) {
		plog(XLOG_USER, "nfs: no remote host specified");
		return FALSE;
	}
	/*
	 * Determine magic cookie to put in mtab
	 */
	xmtab = (char *) xmalloc(strlen(fo->opt_rhost) + strlen(fo->opt_rfs) + 2);
	sprintf(xmtab, "%s:%s", fo->opt_rhost, fo->opt_rfs);
#ifdef DEBUG
	dlog("NFS: mounting remote server \"%s\", remote fs \"%s\" on \"%s\"",
		fo->opt_rhost, fo->opt_rfs, fo->opt_fs);
#endif /* DEBUG */

	return xmtab;
}

static int forcev2(mf)
mntfs *mf;
{
	struct mntent mnt;

	mnt.mnt_dir = mf->mf_mount;
	mnt.mnt_fsname = mf->mf_info;
	mnt.mnt_type = MTAB_TYPE_NFS;
	mnt.mnt_opts = mf->mf_mopts;
	mnt.mnt_freq = 0;
	mnt.mnt_passno = 0;

	if (hasmntopt(&mnt, "nfsv2") != NULL)
		return TRUE;
	else
		return FALSE;
}

/*
 * Initialise am structure for nfs
 */
static int nfs_init(mf)
mntfs *mf;
{
	if (!mf->mf_private) {
		int error;
		struct mountres mr;

		char *colon = strchr(mf->mf_info, ':');
		if (colon == 0)
			return ENOENT;

		error = prime_nfs_fhandle_cache(colon+1, mf->mf_server, forcev2(mf), &mr, (voidp) mf);
		if (!error) {
			mf->mf_private = (voidp) ALLOC(mountres);
			mf->mf_prfree = (void (*)()) free;
			bcopy((voidp) &mr, mf->mf_private, sizeof(mr));
		}
		return error;
	}

	return 0;
}

int mount_nfs_fh P((struct mountres *mrp, char *dir, char *fs_name, char *opts, mntfs *mf));
int mount_nfs_fh(mrp, dir, fs_name, opts, mf)
struct mountres *mrp;
char *dir;
char *fs_name;
char *opts;
mntfs *mf;
{
	struct nfs_args nfs_args;
	struct mntent mnt;
	int retry;
	char *colon;
	/*char *path;*/
	char host[MAXHOSTNAMELEN + MAXPATHLEN + 2];
	fserver *fs = mf->mf_server;
	int flags;
	char *xopts;
	int error;
#ifdef notdef
	unsigned short port;
#endif /* notdef */

	MTYPE_TYPE type = MOUNT_TYPE_NFS;

	bzero((voidp) &nfs_args, sizeof(nfs_args));	/* Paranoid */

	/*
	 * Extract host name to give to kernel
	 */
	if (!(colon = strchr(fs_name, ':')))
		return ENOENT;
#ifndef NFS_ARGS_NEEDS_PATH
	*colon = '\0';
#endif
	strncpy(host, fs_name, sizeof(host));
#ifndef NFS_ARGS_NEEDS_PATH
	*colon = ':';
#endif /* NFS_ARGS_NEEDS_PATH */
	/*path = colon + 1;*/

	if (mf->mf_remopts && *mf->mf_remopts && !islocalnet(fs->fs_ip->sin_addr.s_addr))
		xopts = strdup(mf->mf_remopts);
	else
		xopts = strdup(opts);

	bzero((voidp) &nfs_args, sizeof(nfs_args));

	mnt.mnt_dir = dir;
	mnt.mnt_fsname = fs_name;
	mnt.mnt_type = MTAB_TYPE_NFS;
	mnt.mnt_opts = xopts;
	mnt.mnt_freq = 0;
	mnt.mnt_passno = 0;

	retry = hasmntval(&mnt, "retry");
	if (retry <= 0)
		retry = 1;	/* XXX */

/*again:*/

	/*
	 * set mount args
	 */
	if (mrp->mr_version == MOUNTVERS) {
	    NFS_FH_DREF(nfs_args.fh, (NFS_FH_TYPE) mrp->mr_fhstatus.fhstatus_u.fhs_fhandle);
	    nfs_args.fhsize = FHSIZE;
	} else {
	    NFS_FH_DREF(nfs_args.fh, (NFS_FH_TYPE) mrp->mr_mountres3.mountres3_u.mountinfo.fhandle.fhandle3_val);
	    nfs_args.fhsize = mrp->mr_mountres3.mountres3_u.mountinfo.fhandle.fhandle3_len;
	    nfs_args.flags |= NFSMNT_NFSV3;
	}

#ifdef ULTRIX_HACK
	nfs_args.optstr = mnt.mnt_opts;
#endif /* ULTRIX_HACK */

	nfs_args.hostname = host;
	nfs_args.flags |= NFSMNT_HOSTNAME;
#ifdef HOSTNAMESZ
	/*
	 * Most kernels have a name length restriction.
	 */
	if (strlen(host) >= HOSTNAMESZ)
		strcpy(host + HOSTNAMESZ - 3, "..");
#endif /* HOSTNAMESZ */

	if (nfs_args.rsize = hasmntval(&mnt, "rsize"))
		nfs_args.flags |= NFSMNT_RSIZE;

	if (nfs_args.wsize = hasmntval(&mnt, "wsize"))
		nfs_args.flags |= NFSMNT_WSIZE;

	if (nfs_args.timeo = hasmntval(&mnt, "timeo"))
		nfs_args.flags |= NFSMNT_TIMEO;

	if (nfs_args.retrans = hasmntval(&mnt, "retrans"))
		nfs_args.flags |= NFSMNT_RETRANS;

/*
 * How is it possible that 4.4BSD has NFSMNT_RESVPORT but amd
 * doesn't know about it?!!
 */
#ifdef NFSMNT_RESVPORT
	if (hasmntopt(&mnt, "resvport") != NULL)
		nfs_args.flags |= NFSMNT_RESVPORT;
#endif

#ifdef NFSMNT_BIODS
	if (nfs_args.biods = hasmntval(&mnt, "biods"))
		nfs_args.flags |= NFSMNT_BIODS;

#endif /* NFSMNT_BIODS */

#ifdef NFSMNT_MAXGRPS
	if (nfs_args.maxgrouplist = hasmntval(&mnt, "maxgroups"))
		nfs_args.flags |= NFSMNT_MAXGRPS;
#endif /* NFSMNT_MAXGRPS */

#ifdef notdef
/*
 * This isn't supported by the ping algorithm yet.
 * In any case, it is all done in nfs_init().
 */
 	if (port = hasmntval(&mnt, "port"))
		sin.sin_port = htons(port);
	else
		sin.sin_port = htons(NFS_PORT);	/* XXX should use portmapper */
#endif /* notdef */

	if (hasmntopt(&mnt, MNTOPT_SOFT) != NULL)
		nfs_args.flags |= NFSMNT_SOFT;

#ifdef NFSMNT_SPONGY
	if (hasmntopt(&mnt, "spongy") != NULL) {
		nfs_args.flags |= NFSMNT_SPONGY;
		if (nfs_args.flags & NFSMNT_SOFT) {
			plog(XLOG_USER, "Mount opts soft and spongy are incompatible - soft ignored");
			nfs_args.flags &= ~NFSMNT_SOFT;
		}
	}
#endif /* MNTOPT_SPONGY */

#ifdef MNTOPT_INTR
	if (hasmntopt(&mnt, MNTOPT_INTR) != NULL)
		nfs_args.flags |= NFSMNT_INT;
#endif /* MNTOPT_INTR */

#ifdef MNTOPT_NODEVS
	if (hasmntopt(&mnt, MNTOPT_NODEVS) != NULL)
		nfs_args.flags |= NFSMNT_NODEVS;
#endif /* MNTOPT_NODEVS */

#ifdef MNTOPT_COMPRESS
	if (hasmntopt(&mnt, "compress") != NULL)
		nfs_args.flags |= NFSMNT_COMPRESS;
#endif /* MNTOPT_COMPRESS */

#ifdef MNTOPT_NOCONN
	if (hasmntopt(&mnt, "noconn") != NULL)
		nfs_args.flags |= NFSMNT_NOCONN;
#endif /* MNTOPT_NOCONN */

#ifdef NFSMNT_PGTHRESH
	if (nfs_args.pg_thresh = hasmntval(&mnt, "pgthresh"))
		nfs_args.flags |= NFSMNT_PGTHRESH;
#endif /* NFSMNT_PGTHRESH */

	NFS_SA_DREF(nfs_args, fs->fs_ip);

	flags = compute_mount_flags(&mnt);

#ifdef NFSMNT_NOCTO
	if (hasmntopt(&mnt, "nocto") != NULL)
		nfs_args.flags |= NFSMNT_NOCTO;
#endif /* NFSMNT_NOCTO */

#ifdef HAS_TCP_NFS
	if (hasmntopt(&mnt, "tcp") != NULL)
		nfs_args.sotype = SOCK_STREAM;
#endif /* HAS_TCP_NFS */


#ifdef ULTRIX_HACK
	/*
	 * Ultrix passes the flags argument as part of the
	 * mount data structure, rather than using the
	 * flags argument to the system call.  This is
	 * confusing...
	 */
	if (!(nfs_args.flags & NFSMNT_PGTHRESH)) {
		nfs_args.pg_thresh = 64; /* 64k - XXX */
		nfs_args.flags |= NFSMNT_PGTHRESH;
	}
	nfs_args.gfs_flags = flags;
	flags &= M_RDONLY;
	if (flags & M_RDONLY)
		nfs_args.flags |= NFSMNT_RONLY;
#endif /* ULTRIX_HACK */

	error = mount_fs(&mnt, flags, (caddr_t) &nfs_args, retry, type);
	free(xopts);
	return error;
}

static int mount_nfs(dir, fs_name, opts, mf)
char *dir;
char *fs_name;
char *opts;
mntfs *mf;
{
#ifdef notdef
	int error;
	struct fhstatus fhs;
	char *colon;

	if (!(colon = strchr(fs_name, ':')))
		return ENOENT;

#ifdef DEBUG
	dlog("locating fhandle for %s", fs_name);
#endif /* DEBUG */
	error = prime_nfs_fhandle_cache(colon+1, mf->mf_server, forcev2(mf), &fhs, (voidp) 0);

	if (error)
		return error;

	return mount_nfs_fh(&fhs, dir, fs_name, opts, mf);
#endif
	if (!mf->mf_private) {
		plog(XLOG_ERROR, "Missing filehandle for %s", fs_name);
		return EINVAL;
	}

	return mount_nfs_fh((struct mountres *) mf->mf_private, dir, fs_name, opts, mf);
}

static int nfs_fmount(mf)
mntfs *mf;
{
	int error;

	error = mount_nfs(mf->mf_mount, mf->mf_info, mf->mf_mopts, mf);

#ifdef DEBUG
	if (error) {
		errno = error;
		dlog("mount_nfs: %m");
	}
#endif /* DEBUG */
	return error;
}

static int nfs_fumount(mf)
mntfs *mf;
{
	int error = UMOUNT_FS(mf->mf_mount);
	if (error)
		return error;

	return 0;
}

static void nfs_umounted(mp)
am_node *mp;
{
#ifdef INFORM_MOUNTD
	/*
	 * Don't bother to inform remote mountd
	 * that we are finished.  Until a full
	 * track of filehandles is maintained
	 * the mountd unmount callback cannot
	 * be done correctly anyway...
	 */

	mntfs *mf = mp->am_mnt;
	fserver *fs;
	char *colon, *path;

	if (mf->mf_error || mf->mf_refc > 1)
		return;

	fs = mf->mf_server;

	/*
	 * Call the mount daemon on the server to
	 * announce that we are not using the fs any more.
	 *
	 * This is *wrong*.  The mountd should be called
	 * when the fhandle is flushed from the cache, and
	 * a reference held to the cached entry while the
	 * fs is mounted...
	 */
	colon = path = strchr(mf->mf_info, ':');
	if (fs && colon) {
		fh_cache f;
#ifdef DEBUG
		dlog("calling mountd for %s", mf->mf_info);
#endif /* DEBUG */
		*path++ = '\0';
		f.fh_path = path;
		f.fh_sin = *fs->fs_ip;
		f.fh_sin.sin_port = (u_short) 0;
		f.fh_fs = fs;
		f.fh_id = 0;
		f.fh_error = 0;
		(void) prime_nfs_fhandle_cache(colon+1, mf->mf_server, (struct fhstatus *) 0, (voidp) mf);
		(void) call_mountd(&f, MOUNTPROC_UMNT, MOUNTVERS, (fwd_fun) 0, (voidp) 0);
		*colon = ':';
	}
#endif /* INFORM_MOUNTD */

#ifdef KICK_KERNEL
	/* This should go into the mainline code, not in nfs_ops... */

	/*
	 * Run lstat over the underlying directory in
	 * case this was a direct mount.  This will
	 * get the kernel back in sync with reality.
	 */
	if (mp->am_parent && mp->am_parent->am_path &&
	    STREQ(mp->am_parent->am_mnt->mf_ops->fs_type, "direct")) {
		struct stat stb;
		int pid;
		if ((pid = background()) == 0) {
			if (lstat(mp->am_parent->am_path, &stb) < 0) {
				plog(XLOG_ERROR, "lstat(%s) after unmount: %m", mp->am_parent->am_path);
#ifdef DEBUG
			} else {
				dlog("hack lstat(%s): ok", mp->am_parent->am_path);
#endif /* DEBUG */
			}
			_exit(0);
		}
	}
#endif /* KICK_KERNEL */
}

/*
 * Network file system
 */
am_ops nfs_ops = {
	"nfs",
	nfs_match,
	nfs_init,
	auto_fmount,
	nfs_fmount,
	auto_fumount,
	nfs_fumount,
	efs_lookuppn,
	efs_readdir,
	0, /* nfs_readlink */
	0, /* nfs_mounted */
	nfs_umounted,
	find_nfs_srvr,
	FS_MKMNT|FS_BACKGROUND|FS_AMQINFO
};

#endif /* HAS_NFS */
