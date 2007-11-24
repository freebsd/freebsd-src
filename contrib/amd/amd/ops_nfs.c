/*
 * Copyright (c) 1997-2004 Erez Zadok
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
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
 *    must display the following acknowledgment:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *      %W% (Berkeley) %G%
 *
 * $Id: ops_nfs.c,v 1.6.2.6 2004/01/06 03:15:16 ezk Exp $
 *
 */

/*
 * Network file system
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

/*
 * Convert from nfsstat to UN*X error code
 */
#define unx_error(e)	((int)(e))

/*
 * FH_TTL is the time a file handle will remain in the cache since
 * last being used.  If the file handle becomes invalid, then it
 * will be flushed anyway.
 */
#define	FH_TTL			(5 * 60) /* five minutes */
#define	FH_TTL_ERROR		(30) /* 30 seconds */
#define	FHID_ALLOC(struct)	(++fh_id)

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
typedef struct fh_cache fh_cache;
struct fh_cache {
  qelem			fh_q;		/* List header */
  voidp			fh_wchan;	/* Wait channel */
  int			fh_error;	/* Valid data? */
  int			fh_id;		/* Unique id */
  int			fh_cid;		/* Callout id */
  u_long		fh_nfs_version;	/* highest NFS version on host */
  am_nfs_handle_t	fh_nfs_handle;	/* Handle on filesystem */
  struct sockaddr_in	fh_sin;		/* Address of mountd */
  fserver		*fh_fs;		/* Server holding filesystem */
  char			*fh_path;	/* Filesystem on host */
};

/* forward definitions */
static int call_mountd(fh_cache *fp, u_long proc, fwd_fun f, voidp wchan);
static int fh_id = 0;

/* globals */
AUTH *nfs_auth;
qelem fh_head = {&fh_head, &fh_head};

/*
 * Network file system operations
 */
am_ops nfs_ops =
{
  "nfs",
  nfs_match,
  nfs_init,
  amfs_auto_fmount,
  nfs_fmount,
  amfs_auto_fumount,
  nfs_fumount,
  amfs_error_lookuppn,
  amfs_error_readdir,
  0,				/* nfs_readlink */
  0,				/* nfs_mounted */
  nfs_umounted,
  find_nfs_srvr,
  FS_MKMNT | FS_BACKGROUND | FS_AMQINFO
};


static fh_cache *
find_nfs_fhandle_cache(voidp idv, int done)
{
  fh_cache *fp, *fp2 = 0;
  int id = (long) idv;		/* for 64-bit archs */

  ITER(fp, fh_cache, &fh_head) {
    if (fp->fh_id == id) {
      fp2 = fp;
      break;
    }
  }

#ifdef DEBUG
  if (fp2) {
    dlog("fh cache gives fp %#lx, fs %s", (unsigned long) fp2, fp2->fh_path);
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
static void
got_nfs_fh(voidp pkt, int len, struct sockaddr_in *sa, struct sockaddr_in *ia, voidp idv, int done)
{
  fh_cache *fp;

  fp = find_nfs_fhandle_cache(idv, done);
  if (!fp)
    return;

  /*
   * retrieve the correct RPC reply for the file handle, based on the
   * NFS protocol version.
   */
#ifdef HAVE_FS_NFS3
  if (fp->fh_nfs_version == NFS_VERSION3)
    fp->fh_error = pickup_rpc_reply(pkt, len, (voidp) &fp->fh_nfs_handle.v3,
				    (XDRPROC_T_TYPE) xdr_mountres3);
  else
#endif /* HAVE_FS_NFS3 */
    fp->fh_error = pickup_rpc_reply(pkt, len, (voidp) &fp->fh_nfs_handle.v2,
				    (XDRPROC_T_TYPE) xdr_fhstatus);

  if (!fp->fh_error) {
#ifdef DEBUG
    dlog("got filehandle for %s:%s", fp->fh_fs->fs_host, fp->fh_path);
#endif /* DEBUG */

    /*
     * Wakeup anything sleeping on this filehandle
     */
    if (fp->fh_wchan) {
#ifdef DEBUG
      dlog("Calling wakeup on %#lx", (unsigned long) fp->fh_wchan);
#endif /* DEBUG */
      wakeup(fp->fh_wchan);
    }
  }
}


void
flush_nfs_fhandle_cache(fserver *fs)
{
  fh_cache *fp;

  ITER(fp, fh_cache, &fh_head) {
    if (fp->fh_fs == fs || fs == 0) {
      fp->fh_sin.sin_port = (u_short) 0;
      fp->fh_error = -1;
    }
  }
}


static void
discard_fh(voidp v)
{
  fh_cache *fp = v;

  rem_que(&fp->fh_q);
  if (fp->fh_fs) {
#ifdef DEBUG
    dlog("Discarding filehandle for %s:%s", fp->fh_fs->fs_host, fp->fh_path);
#endif /* DEBUG */
    free_srvr(fp->fh_fs);
  }
  if (fp->fh_path)
    XFREE(fp->fh_path);
  XFREE(fp);
}


/*
 * Determine the file handle for a node
 */
static int
prime_nfs_fhandle_cache(char *path, fserver *fs, am_nfs_handle_t *fhbuf, voidp wchan)
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
    if (fs == fp->fh_fs && STREQ(path, fp->fh_path)) {
      switch (fp->fh_error) {
      case 0:
	plog(XLOG_INFO, "prime_nfs_fhandle_cache: NFS version %d", (int) fp->fh_nfs_version);
#ifdef HAVE_FS_NFS3
	if (fp->fh_nfs_version == NFS_VERSION3)
	  error = fp->fh_error = unx_error(fp->fh_nfs_handle.v3.fhs_status);
	else
#endif /* HAVE_FS_NFS3 */
	  error = fp->fh_error = unx_error(fp->fh_nfs_handle.v2.fhs_status);
	if (error == 0) {
	  if (fhbuf) {
#ifdef HAVE_FS_NFS3
	    if (fp->fh_nfs_version == NFS_VERSION3)
	      memmove((voidp) &(fhbuf->v3), (voidp) &(fp->fh_nfs_handle.v3),
		      sizeof(fp->fh_nfs_handle.v3));
	    else
#endif /* HAVE_FS_NFS3 */
	      memmove((voidp) &(fhbuf->v2), (voidp) &(fp->fh_nfs_handle.v2),
		      sizeof(fp->fh_nfs_handle.v2));
	  }
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
    XFREE(fp->fh_path);
  } else {
    fp = ALLOC(struct fh_cache);
    memset((voidp) fp, 0, sizeof(struct fh_cache));
    ins_que(&fp->fh_q, &fh_head);
  }
  if (!reuse_id)
    fp->fh_id = FHID_ALLOC(struct );
  fp->fh_wchan = wchan;
  fp->fh_error = -1;
  fp->fh_cid = timeout(FH_TTL, discard_fh, (voidp) fp);

  /*
   * if fs->fs_ip is null, remote server is probably down.
   */
  if (!fs->fs_ip) {
    /* Mark the fileserver down and invalid again */
    fs->fs_flags &= ~FSF_VALID;
    fs->fs_flags |= FSF_DOWN;
    error = AM_ERRNO_HOST_DOWN;
    return error;
  }

  /*
   * If the address has changed then don't try to re-use the
   * port information
   */
  if (fp->fh_sin.sin_addr.s_addr != fs->fs_ip->sin_addr.s_addr) {
    fp->fh_sin = *fs->fs_ip;
    fp->fh_sin.sin_port = 0;
    fp->fh_nfs_version = fs->fs_version;
  }
  fp->fh_fs = dup_srvr(fs);
  fp->fh_path = strdup(path);

  error = call_mountd(fp, MOUNTPROC_MNT, got_nfs_fh, wchan);
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


int
make_nfs_auth(void)
{
  AUTH_CREATE_GIDLIST_TYPE group_wheel = 0;

  /* Some NFS mounts (particularly cross-domain) require FQDNs to succeed */

#ifdef HAVE_TRANSPORT_TYPE_TLI
  if (gopt.flags & CFM_FULLY_QUALIFIED_HOSTS) {
    plog(XLOG_INFO, "Using NFS auth for FQHN \"%s\"", hostd);
    nfs_auth = authsys_create(hostd, 0, 0, 1, &group_wheel);
  } else {
    nfs_auth = authsys_create_default();
  }
#else /* not HAVE_TRANSPORT_TYPE_TLI */
  if (gopt.flags & CFM_FULLY_QUALIFIED_HOSTS) {
    plog(XLOG_INFO, "Using NFS auth for FQHN \"%s\"", hostd);
    nfs_auth = authunix_create(hostd, 0, 0, 1, &group_wheel);
  } else {
    nfs_auth = authunix_create_default();
  }
#endif /* not HAVE_TRANSPORT_TYPE_TLI */

  if (!nfs_auth)
    return ENOBUFS;

  return 0;
}


static int
call_mountd(fh_cache *fp, u_long proc, fwd_fun f, voidp wchan)
{
  struct rpc_msg mnt_msg;
  int len;
  char iobuf[8192];
  int error;
  u_long mnt_version;

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

  /* find the right version of the mount protocol */
#ifdef HAVE_FS_NFS3
  if (fp->fh_nfs_version == NFS_VERSION3)
    mnt_version = MOUNTVERS3;
  else
#endif /* HAVE_FS_NFS3 */
    mnt_version = MOUNTVERS;
  plog(XLOG_INFO, "call_mountd: NFS version %d, mount version %d",
       (int) fp->fh_nfs_version, (int) mnt_version);

  rpc_msg_init(&mnt_msg, MOUNTPROG, mnt_version, MOUNTPROC_NULL);
  len = make_rpc_packet(iobuf,
			sizeof(iobuf),
			proc,
			&mnt_msg,
			(voidp) &fp->fh_path,
			(XDRPROC_T_TYPE) xdr_nfspath,
			nfs_auth);

  if (len > 0) {
    error = fwd_packet(MK_RPC_XID(RPC_XID_MOUNTD, fp->fh_id),
		       (voidp) iobuf,
		       len,
		       &fp->fh_sin,
		       &fp->fh_sin,
		       (voidp) ((long) fp->fh_id), /* for 64-bit archs */
		       f);
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


/*
 * NFS needs the local filesystem, remote filesystem
 * remote hostname.
 * Local filesystem defaults to remote and vice-versa.
 */
char *
nfs_match(am_opts *fo)
{
  char *xmtab;

  if (fo->opt_fs && !fo->opt_rfs)
    fo->opt_rfs = fo->opt_fs;
  if (!fo->opt_rfs) {
    plog(XLOG_USER, "nfs: no remote filesystem specified");
    return NULL;
  }
  if (!fo->opt_rhost) {
    plog(XLOG_USER, "nfs: no remote host specified");
    return NULL;
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


/*
 * Initialize am structure for nfs
 */
int
nfs_init(mntfs *mf)
{
  int error;
  am_nfs_handle_t fhs;
  char *colon;

  if (mf->mf_private)
    return 0;

  colon = strchr(mf->mf_info, ':');
  if (colon == 0)
    return ENOENT;

  error = prime_nfs_fhandle_cache(colon + 1, mf->mf_server, &fhs, (voidp) mf);
  if (!error) {
    mf->mf_private = (voidp) ALLOC(am_nfs_handle_t);
    mf->mf_prfree = (void (*)(voidp)) free;
    memmove(mf->mf_private, (voidp) &fhs, sizeof(fhs));
  }
  return error;
}


int
mount_nfs_fh(am_nfs_handle_t *fhp, char *dir, char *fs_name, char *opts, mntfs *mf)
{
  MTYPE_TYPE type;
  char *colon;
  char *xopts;
  char host[MAXHOSTNAMELEN + MAXPATHLEN + 2];
  fserver *fs = mf->mf_server;
  u_long nfs_version = fs->fs_version;
  char *nfs_proto = fs->fs_proto; /* "tcp" or "udp" */
  int error;
  int genflags;
  int retry;
  mntent_t mnt;
  nfs_args_t nfs_args;

  /*
   * Extract HOST name to give to kernel.
   * Some systems like osf1/aix3/bsd44 variants may need old code
   * for NFS_ARGS_NEEDS_PATH.
   */
  if (!(colon = strchr(fs_name, ':')))
    return ENOENT;
#ifdef MOUNT_TABLE_ON_FILE
  *colon = '\0';
#endif /* MOUNT_TABLE_ON_FILE */
  strncpy(host, fs_name, sizeof(host));
#ifdef MOUNT_TABLE_ON_FILE
  *colon = ':';
#endif /* MOUNT_TABLE_ON_FILE */
#ifdef MAXHOSTNAMELEN
  /* most kernels have a name length restriction */
  if (strlen(host) >= MAXHOSTNAMELEN)
    strcpy(host + MAXHOSTNAMELEN - 3, "..");
#endif /* MAXHOSTNAMELEN */

  if (mf->mf_remopts && *mf->mf_remopts &&
      !islocalnet(fs->fs_ip->sin_addr.s_addr)) {
    plog(XLOG_INFO, "Using remopts=\"%s\"", mf->mf_remopts);
    xopts = strdup(mf->mf_remopts);
  } else {
    xopts = strdup(opts);
  }

  memset((voidp) &mnt, 0, sizeof(mnt));
  mnt.mnt_dir = dir;
  mnt.mnt_fsname = fs_name;
  mnt.mnt_opts = xopts;

  /*
   * Set mount types accordingly
   */
#ifndef HAVE_FS_NFS3
  type = MOUNT_TYPE_NFS;
  mnt.mnt_type = MNTTAB_TYPE_NFS;
#else /* HAVE_FS_NFS3 */
  if (nfs_version == NFS_VERSION3) {
    type = MOUNT_TYPE_NFS3;
    /*
     * Systems that include the mount table "vers" option generally do not
     * set the mnttab entry to "nfs3", but to "nfs" and then they set
     * "vers=3".  Setting it to "nfs3" works, but it may break some things
     * like "df -t nfs" and the "quota" program (esp. on Solaris and Irix).
     * So on those systems, set it to "nfs".
     * Note: MNTTAB_OPT_VERS is always set for NFS3 (see am_compat.h).
     */
# if defined(MNTTAB_OPT_VERS) && defined(MOUNT_TABLE_ON_FILE)
    mnt.mnt_type = MNTTAB_TYPE_NFS;
# else /* defined(MNTTAB_OPT_VERS) && defined(MOUNT_TABLE_ON_FILE) */
    mnt.mnt_type = MNTTAB_TYPE_NFS3;
# endif /* defined(MNTTAB_OPT_VERS) && defined(MOUNT_TABLE_ON_FILE) */
  } else {
    type = MOUNT_TYPE_NFS;
    mnt.mnt_type = MNTTAB_TYPE_NFS;
  }
#endif /* HAVE_FS_NFS3 */
  plog(XLOG_INFO, "mount_nfs_fh: NFS version %d", (int) nfs_version);
#if defined(HAVE_FS_NFS3) || defined(HAVE_TRANSPORT_TYPE_TLI)
  plog(XLOG_INFO, "mount_nfs_fh: using NFS transport %s", nfs_proto);
#endif /* defined(HAVE_FS_NFS3) || defined(HAVE_TRANSPORT_TYPE_TLI) */

  retry = hasmntval(&mnt, MNTTAB_OPT_RETRY);
  if (retry <= 0)
    retry = 1;			/* XXX */

  genflags = compute_mount_flags(&mnt);

  /* setup the many fields and flags within nfs_args */
#ifdef HAVE_TRANSPORT_TYPE_TLI
  compute_nfs_args(&nfs_args,
		   &mnt,
		   genflags,
		   NULL,	/* struct netconfig *nfsncp */
		   fs->fs_ip,
		   nfs_version,
		   nfs_proto,
		   fhp,
		   host,
		   fs_name);
#else /* not HAVE_TRANSPORT_TYPE_TLI */
  compute_nfs_args(&nfs_args,
		   &mnt,
		   genflags,
		   fs->fs_ip,
		   nfs_version,
		   nfs_proto,
		   fhp,
		   host,
		   fs_name);
#endif /* not HAVE_TRANSPORT_TYPE_TLI */

  /* finally call the mounting function */
#ifdef DEBUG
  amuDebug(D_TRACE) {
    print_nfs_args(&nfs_args, nfs_version);
    plog(XLOG_DEBUG, "Generic mount flags 0x%x used for NFS mount", genflags);
  }
#endif /* DEBUG */
  error = mount_fs(&mnt, genflags, (caddr_t) &nfs_args, retry, type,
		   nfs_version, nfs_proto, mnttab_file_name);
  XFREE(xopts);

#ifdef HAVE_TRANSPORT_TYPE_TLI
  free_knetconfig(nfs_args.knconf);
  if (nfs_args.addr)
    XFREE(nfs_args.addr);	/* allocated in compute_nfs_args() */
#endif /* HAVE_TRANSPORT_TYPE_TLI */

  return error;
}


static int
mount_nfs(char *dir, char *fs_name, char *opts, mntfs *mf)
{
  if (!mf->mf_private) {
    plog(XLOG_ERROR, "Missing filehandle for %s", fs_name);
    return EINVAL;
  }

  return mount_nfs_fh((am_nfs_handle_t *) mf->mf_private, dir, fs_name, opts, mf);
}


int
nfs_fmount(mntfs *mf)
{
  int error = 0;

  error = mount_nfs(mf->mf_mount, mf->mf_info, mf->mf_mopts, mf);

#ifdef DEBUG
  if (error) {
    errno = error;
    dlog("mount_nfs: %m");
  }
#endif /* DEBUG */

  return error;
}


int
nfs_fumount(mntfs *mf)
{
  int error = UMOUNT_FS(mf->mf_mount, mnttab_file_name);

  /*
   * Here is some code to unmount 'restarted' file systems.
   * The restarted file systems are marked as 'nfs', not
   * 'host', so we only have the map information for the
   * the top-level mount.  The unmount will fail (EBUSY)
   * if there are anything else from the NFS server mounted
   * below the mount-point.  This code checks to see if there
   * is anything mounted with the same prefix as the
   * file system to be unmounted ("/a/b/c" when unmounting "/a/b").
   * If there is, and it is a 'restarted' file system, we unmount
   * it.
   * Added by Mike Mitchell, mcm@unx.sas.com, 09/08/93
   */
  if (error == EBUSY) {
    mntfs *new_mf;
    int len = strlen(mf->mf_mount);
    int didsome = 0;

    ITER(new_mf, mntfs, &mfhead) {
      if (new_mf->mf_ops != mf->mf_ops ||
	  new_mf->mf_refc > 1 ||
	  mf == new_mf ||
	  ((new_mf->mf_flags & (MFF_MOUNTED | MFF_UNMOUNTING | MFF_RESTART)) == (MFF_MOUNTED | MFF_RESTART)))
	continue;

      if (NSTREQ(mf->mf_mount, new_mf->mf_mount, len) &&
	  new_mf->mf_mount[len] == '/') {
	UMOUNT_FS(new_mf->mf_mount, mnttab_file_name);
	didsome = 1;
      }
    }
    if (didsome)
      error = UMOUNT_FS(mf->mf_mount, mnttab_file_name);
  }
  if (error)
    return error;

  return 0;
}


void
nfs_umounted(am_node *mp)
{
  /*
   * Don't bother to inform remote mountd that we are finished.  Until a
   * full track of filehandles is maintained the mountd unmount callback
   * cannot be done correctly anyway...
   */
  mntfs *mf = mp->am_mnt;
  fserver *fs;
  char *colon, *path;

  if (mf->mf_error || mf->mf_refc > 1)
    return;

  fs = mf->mf_server;

  /*
   * Call the mount daemon on the server to announce that we are not using
   * the fs any more.
   *
   * This is *wrong*.  The mountd should be called when the fhandle is
   * flushed from the cache, and a reference held to the cached entry while
   * the fs is mounted...
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
    f.fh_nfs_version = fs->fs_version;
    f.fh_fs = fs;
    f.fh_id = 0;
    f.fh_error = 0;
    prime_nfs_fhandle_cache(colon + 1, mf->mf_server, (am_nfs_handle_t *) 0, (voidp) mf);
    call_mountd(&f, MOUNTPROC_UMNT, (fwd_fun) 0, (voidp) 0);
    *colon = ':';
  }
}
